// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/funding_bridge.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::size_t kMaximumRecordsPerScenario = 250'000U;
constexpr std::size_t kMaximumAggregateRecords = 2'000'000U;
constexpr double kMaximumAmountMillion = 1.0e6;
constexpr double kMaximumAnnualRate = 10.0;
constexpr long double kInputMoneyAbsoluteTolerance = 1.0e-10L;
constexpr double kReconciliationAbsoluteTolerance = 1.0e-9;
constexpr double kReconciliationRelativeTolerance =
    16.0 * std::numeric_limits<double>::epsilon();

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
};

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!is_safe_identifier(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a safe identifier");
    }
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength ||
        value.front() == ' ' || value.back() == ' ') {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

void require_true(bool value, std::string_view description) {
    if (!value) {
        throw std::invalid_argument(
            std::string(description) + " must be explicitly true");
    }
}

void require_nonnegative_amount(
    double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > kMaximumAmountMillion) {
        throw std::invalid_argument(std::string(description) +
            " must be finite, non-negative, and no greater than 1e6 million");
    }
}

void require_positive_amount(double value, std::string_view description) {
    require_nonnegative_amount(value, description);
    if (value <= 0.0) {
        throw std::invalid_argument(
            std::string(description) + " must be positive");
    }
}

void require_fraction(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(description) + " must be finite and in [0, 1]");
    }
}

void require_annual_rate(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > kMaximumAnnualRate) {
        throw std::invalid_argument(std::string(description) +
            " must be finite, non-negative, and no greater than 10");
    }
}

[[nodiscard]] long double input_money_tolerance(
    long double left, long double right) noexcept {
    const long double scale =
        std::max({1.0L, std::abs(left), std::abs(right)});
    return kInputMoneyAbsoluteTolerance + 8.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        scale;
}

[[nodiscard]] bool near_input_money(
    long double left, long double right) noexcept {
    return std::abs(left - right) <= input_money_tolerance(left, right);
}

[[nodiscard]] bool materially_exceeds(
    long double value, long double limit) noexcept {
    return value > limit + input_money_tolerance(value, limit);
}

[[nodiscard]] double checked_double(
    long double value, std::string_view description) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            std::string(description) + " is outside the finite double range");
    }
    return converted;
}

[[nodiscard]] double canonical_nonnegative(
    long double value, long double scale) noexcept {
    const long double nonnegative = std::max(0.0L, value);
    return nonnegative <= input_money_tolerance(nonnegative, scale)
        ? 0.0
        : static_cast<double>(nonnegative);
}

[[nodiscard]] double canonical_sum(
    std::vector<double> values, std::string_view description) {
    std::sort(values.begin(), values.end(), [](double left, double right) {
        if (std::abs(left) != std::abs(right)) {
            return std::abs(left) < std::abs(right);
        }
        return left < right;
    });
    CompensatedSum sum;
    for (const double value : values) {
        sum.add(static_cast<long double>(value));
    }
    return checked_double(sum.value(), description);
}

void enforce_reconciliation(
    double error, double scale, std::string_view description) {
    const double tolerance = kReconciliationAbsoluteTolerance +
        kReconciliationRelativeTolerance * std::max(1.0, std::abs(scale));
    if (!std::isfinite(error) || error > tolerance) {
        throw std::logic_error(std::string(description));
    }
}

[[nodiscard]] double discount_factor(double annual_rate, std::size_t month) {
    const double factor = std::pow(
        1.0 + annual_rate, static_cast<double>(month) / 12.0);
    if (!std::isfinite(factor) || factor <= 0.0) {
        throw std::overflow_error(
            "funding-bridge discount factor is outside the finite range");
    }
    return factor;
}

[[nodiscard]] std::string amount_token(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
           << value;
    return output.str();
}

[[nodiscard]] std::string information_set_id(std::string_view canonical) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char raw_byte : canonical) {
        const auto byte = static_cast<unsigned char>(raw_byte);
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "fnv1a64-" << std::hex << std::setw(16) << std::setfill('0')
           << hash;
    return output.str();
}

struct MonthlyDeclaredCash {
    std::vector<double> acquisition_and_primary{};
    std::vector<std::string> acquisition_and_primary_lineage{};
    std::vector<double> buyer_direct_cost{};
    std::vector<std::string> buyer_direct_cost_lineage{};
    std::vector<double> pool_cost{};
    std::vector<std::string> pool_cost_lineage{};
    std::vector<double> project_principal_receipt{};
    std::vector<double> project_nonprincipal_receipt{};
};

struct PreparedBridge {
    PortfolioConfig participated_portfolio{};
    PortfolioSummary portfolio_summary{};
    CapitalStackSummary stack_summary{};
    double first_loss_notional_million{0.0};
    double callable_notional_million{0.0};
    std::vector<const FundingBridgeScenarioPerformance*> performances{};
    std::unordered_map<std::string, const FundingProvider*> providers{};
};

[[nodiscard]] const JointScenario& find_joint_scenario(
    const PortfolioConfig& portfolio, std::string_view id) {
    const auto found = std::find_if(portfolio.joint_scenarios.begin(),
        portfolio.joint_scenarios.end(), [id](const JointScenario& scenario) {
            return scenario.id == id;
        });
    if (found == portfolio.joint_scenarios.end()) {
        throw std::logic_error("prepared funding scenario is missing");
    }
    return *found;
}

[[nodiscard]] const JointScenarioResult& find_portfolio_scenario(
    const PortfolioSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const JointScenarioResult& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error("prepared portfolio result is missing");
    }
    return *found;
}

[[nodiscard]] const CapitalStackScenarioResult& find_stack_scenario(
    const CapitalStackSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(),
        [id](const CapitalStackScenarioResult& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error("prepared capital-stack result is missing");
    }
    return *found;
}

[[nodiscard]] std::vector<MonthlyDeclaredCash> collect_declared_cash(
    const PortfolioConfig& portfolio, const JointScenario& scenario) {
    std::vector<MonthlyDeclaredCash> months(portfolio.horizon_months + 1U);
    std::unordered_map<std::string, PrincipalAccountingMode> modes;
    modes.reserve(portfolio.projects.size());
    for (const PortfolioProject& project : portfolio.projects) {
        modes.emplace(project.id, project.principal_accounting_mode);
    }
    for (const ProjectJointPath& path : scenario.project_paths) {
        const auto mode = modes.find(path.project_id);
        if (mode == modes.end()) {
            throw std::logic_error("validated project path has no project");
        }
        if (mode->second == PrincipalAccountingMode::DrawEqualsPrincipalLegacy) {
            for (std::size_t index = 0U; index < path.capital_draws.size();
                 ++index) {
                const MonthlyAmount& draw = path.capital_draws[index];
                months[draw.month].acquisition_and_primary.push_back(
                    draw.amount_million);
                months[draw.month].acquisition_and_primary_lineage.push_back(
                    "portfolio:" + scenario.id + ":project:" + path.project_id +
                    ":capital-draw:" + std::to_string(index));
            }
        } else {
            for (std::size_t index = 0U;
                 index < path.investor_outlays.size(); ++index) {
                const InvestorOutlay& outlay = path.investor_outlays[index];
                if (outlay.purpose ==
                        InvestorOutlayPurpose::PrimaryProjectFunding ||
                    outlay.purpose ==
                        InvestorOutlayPurpose::ClaimPurchasePrice) {
                    months[outlay.month].acquisition_and_primary.push_back(
                        outlay.amount_million);
                    months[outlay.month]
                        .acquisition_and_primary_lineage.push_back(
                            "portfolio:" + scenario.id + ":project:" +
                            path.project_id + ":investor-outlay:" +
                            std::to_string(index));
                } else {
                    months[outlay.month].buyer_direct_cost.push_back(
                        outlay.amount_million);
                    months[outlay.month].buyer_direct_cost_lineage.push_back(
                        "portfolio:" + scenario.id + ":project:" +
                        path.project_id + ":investor-outlay:" +
                        std::to_string(index));
                }
            }
        }
        for (const InvestorReceipt& receipt : path.investor_receipts) {
            months[receipt.month].project_principal_receipt.push_back(
                receipt.principal_component_million);
            months[receipt.month].project_nonprincipal_receipt.push_back(
                receipt.amount_million - receipt.principal_component_million);
        }
    }
    for (std::size_t index = 0U; index < scenario.pool_costs.size(); ++index) {
        const MonthlyAmount& cost = scenario.pool_costs[index];
        months[cost.month].pool_cost.push_back(cost.amount_million);
        months[cost.month].pool_cost_lineage.push_back(
            "portfolio:" + scenario.id + ":pool-cost:" +
            std::to_string(index));
    }
    return months;
}

[[nodiscard]] std::string canonical_observed_history(
    const PortfolioConfig& portfolio,
    const FundingBridgeScenarioPerformance& performance,
    std::size_t decision_month) {
    const JointScenario& scenario =
        find_joint_scenario(portfolio, performance.scenario_id);
    std::vector<std::string> facts;
    auto add_amount_fact = [&facts](std::string prefix, std::size_t month,
                               double amount) {
        facts.push_back(std::move(prefix) + ":" + std::to_string(month) +
            ":" + amount_token(amount));
    };

    for (const ProjectJointPath& path : scenario.project_paths) {
        for (const MonthlyAmount& draw : path.capital_draws) {
            if (draw.month < decision_month) {
                add_amount_fact("draw:" + path.project_id, draw.month,
                    draw.amount_million);
            }
        }
        for (const InvestorOutlay& outlay : path.investor_outlays) {
            if (outlay.month < decision_month) {
                add_amount_fact("outlay:" + path.project_id + ":" +
                        std::to_string(static_cast<unsigned>(outlay.purpose)),
                    outlay.month, outlay.amount_million);
            }
        }
        for (const InvestorReceipt& receipt : path.investor_receipts) {
            if (receipt.month < decision_month) {
                facts.push_back("receipt:" + path.project_id + ":" +
                    receipt.cash_source_id + ":" +
                    std::to_string(receipt.month) + ":" +
                    amount_token(receipt.amount_million) + ":" +
                    amount_token(receipt.principal_component_million));
            }
        }
        for (const PrincipalMovement& movement : path.principal_movements) {
            if (movement.month < decision_month) {
                add_amount_fact("principal-movement:" + path.project_id +
                        ":" + std::to_string(
                            static_cast<unsigned>(movement.kind)),
                    movement.month, movement.amount_million);
            }
        }
    }
    for (const ScenarioCashSource& source : scenario.cash_sources) {
        for (const MonthlyAmount& cash : source.cash_available) {
            if (cash.month < decision_month) {
                add_amount_fact("cash-source:" + source.id + ":" +
                        std::to_string(static_cast<unsigned>(source.kind)),
                    cash.month, cash.amount_million);
            }
        }
    }
    for (const MonthlyAmount& cost : scenario.pool_costs) {
        if (cost.month < decision_month) {
            add_amount_fact("pool-cost", cost.month, cost.amount_million);
        }
    }

    std::unordered_map<std::string, const CapitalCallRequest*> call_requests;
    for (const CapitalCallRequest& request :
        performance.capital_call_requests) {
        call_requests.emplace(request.id, &request);
        if (request.notice_month < decision_month) {
            add_amount_fact("call-request:" + request.facility_id,
                request.notice_month, request.requested_million);
        }
    }
    for (const FundingSettlementOutcome& outcome :
        performance.capital_call_outcomes) {
        if (outcome.settlement_month < decision_month) {
            const CapitalCallRequest& request =
                *call_requests.at(outcome.request_id);
            facts.push_back("call-outcome:" + request.facility_id + ":" +
                std::to_string(outcome.settlement_month) + ":" +
                std::to_string(static_cast<unsigned>(outcome.status)) + ":" +
                amount_token(outcome.actual_cash_million));
        }
    }

    std::unordered_map<std::string, const WarehouseDrawRequest*>
        warehouse_requests;
    for (const WarehouseDrawRequest& request :
        performance.warehouse_draw_requests) {
        warehouse_requests.emplace(request.id, &request);
        if (request.request_month < decision_month) {
            add_amount_fact("warehouse-request:" + request.facility_id,
                request.request_month, request.requested_million);
        }
    }
    for (const FundingSettlementOutcome& outcome :
        performance.warehouse_draw_outcomes) {
        if (outcome.settlement_month < decision_month) {
            const WarehouseDrawRequest& request =
                *warehouse_requests.at(outcome.request_id);
            facts.push_back("warehouse-outcome:" + request.facility_id +
                ":" + std::to_string(outcome.settlement_month) + ":" +
                std::to_string(static_cast<unsigned>(outcome.status)) + ":" +
                amount_token(outcome.actual_cash_million));
        }
    }
    for (const SupplementalFundingReceipt& receipt :
        performance.supplemental_receipts) {
        if (receipt.month < decision_month) {
            add_amount_fact("supplemental:" + receipt.provider_id + ":" +
                    std::to_string(static_cast<unsigned>(receipt.purpose)),
                receipt.month, receipt.actual_cash_million);
        }
    }
    for (const EligibleBasisMovement& movement :
        performance.eligible_basis_movements) {
        if (movement.month < decision_month) {
            add_amount_fact("eligible-basis:" + std::to_string(
                    static_cast<unsigned>(movement.kind)),
                movement.month, movement.amount_million);
        }
    }
    for (const ProtectionAbsorption& absorption :
        performance.protection_absorptions) {
        if (absorption.month < decision_month) {
            add_amount_fact("protection-absorption", absorption.month,
                absorption.amount_million);
        }
    }
    for (const ProtectionRelease& release :
        performance.protection_releases) {
        if (release.month < decision_month) {
            add_amount_fact("protection-release", release.month,
                release.amount_million);
        }
    }

    std::sort(facts.begin(), facts.end());
    std::string canonical;
    for (const std::string& fact : facts) {
        canonical += fact;
        canonical.push_back('\n');
    }
    return canonical;
}

[[nodiscard]] std::string canonical_policy_action(
    const FundingBridgeScenarioPerformance& performance,
    std::size_t decision_month) {
    std::vector<std::string> calls;
    std::vector<std::string> draws;
    std::vector<std::string> releases;
    for (const CapitalCallRequest& request :
        performance.capital_call_requests) {
        if (request.notice_month == decision_month) {
            calls.push_back(request.facility_id + ":" +
                amount_token(request.requested_million));
        }
    }
    for (const WarehouseDrawRequest& request :
        performance.warehouse_draw_requests) {
        if (request.request_month == decision_month) {
            draws.push_back(request.facility_id + ":" +
                amount_token(request.requested_million));
        }
    }
    for (const ProtectionRelease& release :
        performance.protection_releases) {
        if (release.month == decision_month) {
            releases.push_back(amount_token(release.amount_million));
        }
    }
    const auto encode_multiset = [](std::vector<std::string> values) {
        std::sort(values.begin(), values.end());
        std::string encoded;
        for (const std::string& value : values) {
            encoded += std::to_string(value.size());
            encoded.push_back(':');
            encoded += value;
            encoded.push_back(';');
        }
        return encoded;
    };
    return "calls=" + encode_multiset(std::move(calls)) +
        "|draws=" + encode_multiset(std::move(draws)) +
        "|releases=" + encode_multiset(std::move(releases));
}

void validate_nonanticipativity(const PortfolioConfig& portfolio,
    const std::vector<const FundingBridgeScenarioPerformance*>& performances) {
    for (std::size_t month = 0U; month <= portfolio.horizon_months; ++month) {
        std::unordered_map<std::string, std::string> action_by_history;
        for (const FundingBridgeScenarioPerformance* performance :
            performances) {
            const std::string history = canonical_observed_history(
                portfolio, *performance, month);
            const std::string action =
                canonical_policy_action(*performance, month);
            const auto [position, inserted] =
                action_by_history.emplace(history, action);
            if (!inserted && position->second != action) {
                throw std::invalid_argument(
                    "funding policy uses future information: identical observed histories have different actions");
            }
        }
    }
}

void validate_outcome(const FundingSettlementOutcome& outcome,
    double requested_million, std::size_t required_settlement_month,
    std::string_view description) {
    require_safe_identifier(outcome.request_id,
        std::string(description) + " request_id");
    require_safe_identifier(outcome.source_record_id,
        std::string(description) + " source_record_id");
    if (outcome.settlement_month != required_settlement_month) {
        throw std::invalid_argument(std::string(description) +
            " settlement month does not equal request month plus lag");
    }
    require_nonnegative_amount(
        outcome.actual_cash_million,
        std::string(description) + " actual cash");
    if (materially_exceeds(outcome.actual_cash_million, requested_million)) {
        throw std::invalid_argument(std::string(description) +
            " actual cash exceeds the requested amount");
    }
    switch (outcome.status) {
    case FundingSettlementStatus::SettledInFull:
        if (!near_input_money(outcome.actual_cash_million,
                requested_million)) {
            throw std::invalid_argument(std::string(description) +
                " full settlement must equal the request");
        }
        break;
    case FundingSettlementStatus::FinalPartialSettlement:
        if (outcome.actual_cash_million <= 0.0 ||
            !materially_exceeds(requested_million,
                outcome.actual_cash_million)) {
            throw std::invalid_argument(std::string(description) +
                " final partial settlement must be positive and below the request");
        }
        break;
    case FundingSettlementStatus::Failed:
        if (!near_input_money(outcome.actual_cash_million, 0.0L)) {
            throw std::invalid_argument(std::string(description) +
                " failed settlement must deliver zero cash");
        }
        break;
    default:
        throw std::invalid_argument(std::string(description) +
            " has an unsupported settlement status");
    }
}

[[nodiscard]] PreparedBridge prepare_bridge(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack, const FundingBridgeConfig& bridge) {
    if (bridge.model_version != kFundingBridgeModelVersion) {
        throw std::invalid_argument(
            "unsupported funding-bridge model_version");
    }
    require_safe_text(bridge.scenario_label,
        "funding-bridge scenario_label");
    require_safe_text(bridge.source_note, "funding-bridge source_note");
    if (!bridge.synthetic_inputs) {
        throw std::invalid_argument(
            "funding bridge v0.1 is synthetic-only: live known-at facts, facility evidence, and legal enforceability are not validated");
    }
    require_nonnegative_amount(bridge.funded_at_close_cash_million,
        "funded_at_close_cash_million");
    require_safe_identifier(bridge.funded_at_close_provider_id,
        "funded_at_close_provider_id");
    require_safe_identifier(bridge.funded_at_close_source_record_id,
        "funded_at_close_source_record_id");
    require_true(bridge.uncalled_commitment_is_not_cash_or_loss_absorption,
        "uncalled_commitment_is_not_cash_or_loss_absorption");
    require_true(
        bridge.acquisition_and_primary_funding_uses_precede_same_month_receipts,
        "acquisition_and_primary_funding_uses_precede_same_month_receipts");
    require_true(bridge.warehouse_is_external_temporary_debt,
        "warehouse_is_external_temporary_debt");
    require_true(
        bridge.warehouse_proceeds_cannot_fund_interest_fees_or_costs,
        "warehouse_proceeds_cannot_fund_interest_fees_or_costs");
    require_true(
        bridge.project_receipts_sweep_warehouse_principal_before_investor_cash,
        "project_receipts_sweep_warehouse_principal_before_investor_cash");
    require_true(bridge.policy_uses_observed_history_only,
        "policy_uses_observed_history_only");
    require_true(bridge.no_dynamic_tranche_allocation_or_pricing_is_claimed,
        "no_dynamic_tranche_allocation_or_pricing_is_claimed");
    if (stack.model_version != kCapitalStackModelVersion) {
        throw std::invalid_argument(
            "funding bridge requires CapitalStackConfig v0.2");
    }

    PreparedBridge prepared;
    if (bridge.providers.empty()) {
        throw std::invalid_argument(
            "funding bridge requires at least one named provider");
    }
    prepared.providers.reserve(bridge.providers.size());
    for (const FundingProvider& provider : bridge.providers) {
        require_safe_identifier(provider.id, "provider id");
        require_safe_identifier(
            provider.economic_group_id, "provider economic_group_id");
        require_positive_amount(
            provider.declared_capacity_million, "provider capacity");
        require_safe_identifier(
            provider.source_record_id, "provider source_record_id");
        if (!prepared.providers.emplace(provider.id, &provider).second) {
            throw std::invalid_argument("provider ids must be unique");
        }
    }
    if (!prepared.providers.contains(bridge.funded_at_close_provider_id)) {
        throw std::invalid_argument(
            "funded-at-close provider is not declared");
    }

    const CallableCapitalFacility& callable = bridge.callable_facility;
    require_safe_identifier(callable.id, "callable facility id");
    require_safe_identifier(
        callable.provider_id, "callable facility provider_id");
    require_safe_identifier(
        callable.source_record_id, "callable facility source_record_id");
    if (!prepared.providers.contains(callable.provider_id)) {
        throw std::invalid_argument(
            "callable facility provider is not declared");
    }
    require_nonnegative_amount(
        callable.commitment_million, "callable facility commitment");
    require_safe_text(
        callable.permitted_purpose, "callable facility permitted_purpose");
    require_annual_rate(callable.annual_commitment_fee_rate,
        "callable annual commitment fee rate");
    require_fraction(callable.liquidity_reserve_fraction,
        "callable liquidity reserve fraction");
    require_annual_rate(callable.annual_liquidity_hurdle_rate,
        "callable liquidity hurdle rate");
    require_annual_rate(callable.annual_reserve_yield_rate,
        "callable reserve yield rate");
    if (callable.annual_reserve_yield_rate >
        callable.annual_liquidity_hurdle_rate) {
        throw std::invalid_argument(
            "callable reserve yield cannot exceed the liquidity hurdle in v0.1");
    }
    if (callable.availability_start_month >
            callable.contractual_expiry_month ||
        callable.contractual_expiry_month > portfolio.horizon_months ||
        callable.settlement_lag_months > portfolio.horizon_months) {
        throw std::invalid_argument(
            "callable facility dates lie outside the supported horizon");
    }

    const WarehouseFacility& warehouse = bridge.warehouse_facility;
    require_safe_identifier(warehouse.id, "warehouse facility id");
    require_safe_identifier(
        warehouse.provider_id, "warehouse facility provider_id");
    require_safe_identifier(
        warehouse.source_record_id, "warehouse facility source_record_id");
    if (!prepared.providers.contains(warehouse.provider_id)) {
        throw std::invalid_argument(
            "warehouse facility provider is not declared");
    }
    require_nonnegative_amount(
        warehouse.committed_limit_million, "warehouse committed limit");
    require_safe_text(
        warehouse.permitted_purpose, "warehouse permitted_purpose");
    require_fraction(
        warehouse.collateral_advance_rate, "warehouse advance rate");
    require_annual_rate(
        warehouse.annual_interest_rate, "warehouse annual interest rate");
    require_annual_rate(warehouse.annual_undrawn_fee_rate,
        "warehouse annual undrawn fee rate");
    require_fraction(
        warehouse.advance_fee_rate, "warehouse advance fee rate");
    require_fraction(
        warehouse.upfront_fee_rate, "warehouse upfront fee rate");
    if (warehouse.availability_start_month >
            warehouse.availability_end_month ||
        warehouse.availability_end_month > warehouse.legal_maturity_month ||
        warehouse.legal_maturity_month > portfolio.horizon_months ||
        warehouse.settlement_lag_months > portfolio.horizon_months) {
        throw std::invalid_argument(
            "warehouse facility dates lie outside the supported horizon");
    }
    if (warehouse.committed_limit_million > 0.0 &&
        warehouse.collateral_advance_rate <= 0.0) {
        throw std::invalid_argument(
            "positive warehouse limit requires a positive advance rate");
    }
    if (warehouse.committed_limit_million == 0.0 &&
        (warehouse.annual_interest_rate != 0.0 ||
            warehouse.annual_undrawn_fee_rate != 0.0 ||
            warehouse.advance_fee_rate != 0.0 ||
            warehouse.upfront_fee_rate != 0.0)) {
        throw std::invalid_argument(
            "zero warehouse limit requires zero rates and fees");
    }

    prepared.participated_portfolio = apply_success_participation_fraction(
        portfolio, participation,
        stack.underlying_success_participation_fraction);
    prepared.portfolio_summary =
        evaluate_portfolio(prepared.participated_portfolio);
    prepared.stack_summary =
        evaluate_capital_stack(portfolio, ambiguity, participation, stack);

    std::size_t first_loss_count = 0U;
    CompensatedSum first_loss_sum;
    for (const CapitalStackTrancheSummary& tranche :
        prepared.stack_summary.tranches) {
        if (tranche.is_first_loss_residual) {
            ++first_loss_count;
            first_loss_sum.add(tranche.notional_million);
        }
    }
    if (first_loss_count != 1U) {
        throw std::logic_error(
            "validated v0.2 stack did not expose one first-loss residual");
    }
    prepared.first_loss_notional_million = checked_double(
        first_loss_sum.value(), "first-loss residual notional");
    prepared.callable_notional_million = canonical_nonnegative(
        static_cast<long double>(
            prepared.stack_summary.aggregate_commitment_million) -
            first_loss_sum.value(),
        prepared.stack_summary.aggregate_commitment_million);
    if (!near_input_money(bridge.funded_at_close_cash_million,
            prepared.first_loss_notional_million)) {
        throw std::invalid_argument(
            "funded-at-close cash must equal the v0.2 first-loss notional");
    }
    if (!near_input_money(callable.commitment_million,
            prepared.callable_notional_million)) {
        throw std::invalid_argument(
            "callable commitment must equal the remaining v0.2 stack notional");
    }

    if (bridge.scenario_performance.size() !=
        portfolio.joint_scenarios.size()) {
        throw std::invalid_argument(
            "funding performance must match portfolio scenarios exactly");
    }
    std::unordered_set<std::string> portfolio_ids;
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        portfolio_ids.insert(scenario.id);
    }
    std::unordered_set<std::string> performance_ids;
    std::size_t aggregate_records = 0U;
    prepared.performances.reserve(bridge.scenario_performance.size());
    for (const FundingBridgeScenarioPerformance& performance :
        bridge.scenario_performance) {
        require_safe_identifier(
            performance.scenario_id, "funding-performance scenario_id");
        if (!performance_ids.insert(performance.scenario_id).second) {
            throw std::invalid_argument(
                "funding-performance scenario ids must be unique");
        }
        if (!portfolio_ids.contains(performance.scenario_id)) {
            throw std::invalid_argument(
                "funding performance contains an unknown scenario id");
        }
        const std::size_t record_count =
            performance.capital_call_requests.size() +
            performance.capital_call_outcomes.size() +
            performance.warehouse_draw_requests.size() +
            performance.warehouse_draw_outcomes.size() +
            performance.supplemental_receipts.size() +
            performance.eligible_basis_movements.size() +
            performance.protection_absorptions.size() +
            performance.protection_releases.size();
        if (record_count > kMaximumRecordsPerScenario ||
            record_count > kMaximumAggregateRecords - aggregate_records) {
            throw std::invalid_argument(
                "funding-performance records exceed a resource bound");
        }
        aggregate_records += record_count;

        std::unordered_set<std::string> all_ids;
        std::unordered_map<std::string, const CapitalCallRequest*>
            call_requests;
        CompensatedSum total_call_requests;
        for (const CapitalCallRequest& request :
            performance.capital_call_requests) {
            require_safe_identifier(request.id, "capital-call request id");
            require_safe_identifier(
                request.facility_id, "capital-call facility_id");
            if (!all_ids.insert(request.id).second) {
                throw std::invalid_argument(
                    "funding record ids must be unique within a scenario");
            }
            if (request.facility_id != callable.id) {
                throw std::invalid_argument(
                    "capital-call request references the wrong facility");
            }
            if (request.notice_month > portfolio.horizon_months -
                    callable.settlement_lag_months) {
                throw std::invalid_argument(
                    "capital-call request cannot settle within the horizon");
            }
            require_positive_amount(
                request.requested_million, "capital-call request amount");
            call_requests.emplace(request.id, &request);
            total_call_requests.add(request.requested_million);
        }
        if (materially_exceeds(
                total_call_requests.value(), callable.commitment_million)) {
            throw std::invalid_argument(
                "capital-call requests exceed the callable commitment");
        }
        std::unordered_set<std::string> call_outcome_ids;
        for (const FundingSettlementOutcome& outcome :
            performance.capital_call_outcomes) {
            if (!call_outcome_ids.insert(outcome.request_id).second) {
                throw std::invalid_argument(
                    "capital-call outcomes must be unique by request");
            }
            const auto found = call_requests.find(outcome.request_id);
            if (found == call_requests.end()) {
                throw std::invalid_argument(
                    "capital-call outcome has no matching request");
            }
            validate_outcome(outcome, found->second->requested_million,
                found->second->notice_month +
                    callable.settlement_lag_months,
                "capital-call outcome");
        }
        if (call_outcome_ids.size() != call_requests.size()) {
            throw std::invalid_argument(
                "every capital-call request requires exactly one final outcome");
        }
        std::unordered_map<std::string, const WarehouseDrawRequest*>
            warehouse_requests;
        for (const WarehouseDrawRequest& request :
            performance.warehouse_draw_requests) {
            require_safe_identifier(
                request.id, "warehouse-draw request id");
            require_safe_identifier(
                request.facility_id, "warehouse-draw facility_id");
            if (!all_ids.insert(request.id).second) {
                throw std::invalid_argument(
                    "funding record ids must be unique within a scenario");
            }
            if (request.facility_id != warehouse.id) {
                throw std::invalid_argument(
                    "warehouse-draw request references the wrong facility");
            }
            if (request.request_month > portfolio.horizon_months -
                    warehouse.settlement_lag_months) {
                throw std::invalid_argument(
                    "warehouse-draw request cannot settle within the horizon");
            }
            if (request.request_month + warehouse.settlement_lag_months >
                warehouse.legal_maturity_month) {
                throw std::invalid_argument(
                    "warehouse-draw request cannot settle after legal maturity");
            }
            require_positive_amount(
                request.requested_million, "warehouse-draw request amount");
            if (warehouse.committed_limit_million == 0.0) {
                throw std::invalid_argument(
                    "warehouse requests require a positive facility limit");
            }
            warehouse_requests.emplace(request.id, &request);
        }
        std::unordered_set<std::string> warehouse_outcome_ids;
        for (const FundingSettlementOutcome& outcome :
            performance.warehouse_draw_outcomes) {
            if (!warehouse_outcome_ids.insert(outcome.request_id).second) {
                throw std::invalid_argument(
                    "warehouse outcomes must be unique by request");
            }
            const auto found = warehouse_requests.find(outcome.request_id);
            if (found == warehouse_requests.end()) {
                throw std::invalid_argument(
                    "warehouse outcome has no matching request");
            }
            validate_outcome(outcome, found->second->requested_million,
                found->second->request_month +
                    warehouse.settlement_lag_months,
                "warehouse-draw outcome");
        }
        if (warehouse_outcome_ids.size() != warehouse_requests.size()) {
            throw std::invalid_argument(
                "every warehouse-draw request requires exactly one final outcome");
        }
        for (const SupplementalFundingReceipt& receipt :
            performance.supplemental_receipts) {
            switch (receipt.purpose) {
            case SupplementalFundingPurpose::CostSupport:
            case SupplementalFundingPurpose::ProtectionReplenishment:
            case SupplementalFundingPurpose::SettledTakeout:
                break;
            default:
                throw std::invalid_argument(
                    "supplemental receipt has an unsupported purpose");
            }
            require_safe_identifier(receipt.id, "supplemental receipt id");
            require_safe_identifier(
                receipt.provider_id, "supplemental receipt provider_id");
            require_safe_identifier(receipt.source_record_id,
                "supplemental receipt source_record_id");
            if (!all_ids.insert(receipt.id).second) {
                throw std::invalid_argument(
                    "funding record ids must be unique within a scenario");
            }
            if (!prepared.providers.contains(receipt.provider_id)) {
                throw std::invalid_argument(
                    "supplemental receipt provider is not declared");
            }
            if (receipt.month > portfolio.horizon_months) {
                throw std::invalid_argument(
                    "supplemental receipt lies outside the horizon");
            }
            require_positive_amount(
                receipt.actual_cash_million, "supplemental receipt cash");
        }

        struct DatedWriteoff {
            double amount_million{0.0};
            std::size_t month{0U};
        };
        std::unordered_map<std::string, DatedWriteoff> writeoffs;
        std::unordered_map<std::string, double> absorption_by_writeoff;
        std::vector<long double> additions(portfolio.horizon_months + 1U, 0.0L);
        std::vector<long double> reductions(portfolio.horizon_months + 1U, 0.0L);
        std::vector<long double> principal_returns(
            portfolio.horizon_months + 1U, 0.0L);
        std::vector<long double> dispositions(
            portfolio.horizon_months + 1U, 0.0L);
        for (const EligibleBasisMovement& movement :
            performance.eligible_basis_movements) {
            switch (movement.kind) {
            case EligibleBasisMovementKind::EligibleAddition:
            case EligibleBasisMovementKind::PrincipalBasisReturn:
            case EligibleBasisMovementKind::Disposition:
            case EligibleBasisMovementKind::Writeoff:
            case EligibleBasisMovementKind::EligibilityRemoval:
                break;
            default:
                throw std::invalid_argument(
                    "eligible-basis movement has an unsupported kind");
            }
            require_safe_identifier(movement.id, "eligible-basis movement id");
            require_safe_identifier(
                movement.reference_id, "eligible-basis reference_id");
            require_safe_identifier(movement.source_record_id,
                "eligible-basis source_record_id");
            if (!all_ids.insert(movement.id).second) {
                throw std::invalid_argument(
                    "funding record ids must be unique within a scenario");
            }
            if (movement.month > portfolio.horizon_months) {
                throw std::invalid_argument(
                    "eligible-basis movement lies outside the horizon");
            }
            require_positive_amount(
                movement.amount_million, "eligible-basis movement amount");
            if (movement.kind == EligibleBasisMovementKind::EligibleAddition) {
                additions[movement.month] += movement.amount_million;
            } else {
                reductions[movement.month] += movement.amount_million;
            }
            if (movement.kind ==
                EligibleBasisMovementKind::PrincipalBasisReturn) {
                principal_returns[movement.month] += movement.amount_million;
            }
            if (movement.kind == EligibleBasisMovementKind::Disposition) {
                dispositions[movement.month] += movement.amount_million;
            }
            if (movement.kind == EligibleBasisMovementKind::Writeoff) {
                writeoffs.emplace(movement.id,
                    DatedWriteoff{movement.amount_million, movement.month});
            }
        }
        const JointScenario& participated_scenario = find_joint_scenario(
            prepared.participated_portfolio, performance.scenario_id);
        const std::vector<MonthlyDeclaredCash> declared =
            collect_declared_cash(
                prepared.participated_portfolio, participated_scenario);
        long double eligible_basis = 0.0L;
        for (std::size_t month = 0U; month <= portfolio.horizon_months;
             ++month) {
            const double acquisition_due = canonical_sum(
                declared[month].acquisition_and_primary,
                "declared acquisition use");
            if (materially_exceeds(additions[month], acquisition_due)) {
                throw std::invalid_argument(
                    "eligible-basis additions exceed same-month acquisition uses");
            }
            eligible_basis += additions[month];
            if (materially_exceeds(reductions[month], eligible_basis)) {
                throw std::invalid_argument(
                    "eligible-basis reductions exceed retained eligible basis");
            }
            const long double principal_cash = canonical_sum(
                declared[month].project_principal_receipt,
                "declared project principal receipts");
            const long double all_project_cash = principal_cash +
                canonical_sum(declared[month].project_nonprincipal_receipt,
                    "declared project non-principal receipts");
            if (materially_exceeds(
                    principal_returns[month], principal_cash)) {
                throw std::invalid_argument(
                    "principal-basis returns exceed same-month project principal cash");
            }
            if (materially_exceeds(
                    principal_returns[month] + dispositions[month],
                    all_project_cash)) {
                throw std::invalid_argument(
                    "cash-returning basis reductions exceed same-month project cash");
            }
            eligible_basis -= reductions[month];
        }

        for (const ProtectionAbsorption& absorption :
            performance.protection_absorptions) {
            require_safe_identifier(
                absorption.id, "protection-absorption id");
            require_safe_identifier(absorption.reference_id,
                "protection-absorption reference_id");
            require_safe_identifier(absorption.source_record_id,
                "protection-absorption source_record_id");
            if (!all_ids.insert(absorption.id).second) {
                throw std::invalid_argument(
                    "funding record ids must be unique within a scenario");
            }
            if (absorption.month > portfolio.horizon_months) {
                throw std::invalid_argument(
                    "protection absorption lies outside the horizon");
            }
            require_positive_amount(
                absorption.amount_million, "protection absorption amount");
            const auto writeoff = writeoffs.find(
                absorption.reference_id);
            if (writeoff == writeoffs.end()) {
                throw std::invalid_argument(
                    "protection absorption must reference an eligible-basis writeoff");
            }
            if (absorption.month != writeoff->second.month) {
                throw std::invalid_argument(
                    "protection absorption must occur in the same month as its referenced writeoff");
            }
            absorption_by_writeoff[absorption.reference_id] +=
                absorption.amount_million;
            if (materially_exceeds(
                    absorption_by_writeoff[absorption.reference_id],
                    writeoff->second.amount_million)) {
                throw std::invalid_argument(
                    "protection absorption exceeds its referenced writeoff");
            }
        }
        for (const ProtectionRelease& release :
            performance.protection_releases) {
            require_safe_identifier(release.id, "protection-release id");
            require_safe_identifier(release.source_record_id,
                "protection-release source_record_id");
            if (!all_ids.insert(release.id).second) {
                throw std::invalid_argument(
                    "funding record ids must be unique within a scenario");
            }
            if (release.month > portfolio.horizon_months) {
                throw std::invalid_argument(
                    "protection release lies outside the horizon");
            }
            require_positive_amount(
                release.amount_million, "protection release amount");
        }
        prepared.performances.push_back(&performance);
    }
    if (performance_ids != portfolio_ids) {
        throw std::invalid_argument(
            "funding performance omits a portfolio scenario id");
    }
    std::sort(prepared.performances.begin(), prepared.performances.end(),
        [](const auto* left, const auto* right) {
            return left->scenario_id < right->scenario_id;
        });
    validate_nonanticipativity(
        prepared.participated_portfolio, prepared.performances);
    return prepared;
}

struct ProviderRuntime {
    long double settled_initial_and_supplemental{0.0L};
    long double settled_callable{0.0L};
    long double settled_takeout{0.0L};
    long double cumulative_settled_warehouse_advances{0.0L};
    long double callable_defaulted{0.0L};
    long double warehouse_defaulted{0.0L};
    bool nonperformance{false};
};

struct ScenarioPlan {
    std::vector<std::vector<const CapitalCallRequest*>> call_requests{};
    std::vector<std::vector<std::pair<const CapitalCallRequest*,
        const FundingSettlementOutcome*>>> call_outcomes{};
    std::vector<std::vector<const WarehouseDrawRequest*>> warehouse_requests{};
    std::vector<std::vector<std::pair<const WarehouseDrawRequest*,
        const FundingSettlementOutcome*>>> warehouse_outcomes{};
    std::vector<std::vector<const SupplementalFundingReceipt*>> supplemental{};
    std::vector<std::vector<const EligibleBasisMovement*>> basis_movements{};
    std::vector<std::vector<const ProtectionAbsorption*>> absorptions{};
    std::vector<std::vector<const ProtectionRelease*>> releases{};
};

[[nodiscard]] ScenarioPlan prepare_scenario_plan(
    const FundingBridgeScenarioPerformance& performance,
    std::size_t month_count) {
    ScenarioPlan plan;
    plan.call_requests.resize(month_count);
    plan.call_outcomes.resize(month_count);
    plan.warehouse_requests.resize(month_count);
    plan.warehouse_outcomes.resize(month_count);
    plan.supplemental.resize(month_count);
    plan.basis_movements.resize(month_count);
    plan.absorptions.resize(month_count);
    plan.releases.resize(month_count);

    std::unordered_map<std::string, const CapitalCallRequest*> calls;
    for (const CapitalCallRequest& request :
        performance.capital_call_requests) {
        calls.emplace(request.id, &request);
        plan.call_requests[request.notice_month].push_back(&request);
    }
    for (const FundingSettlementOutcome& outcome :
        performance.capital_call_outcomes) {
        plan.call_outcomes[outcome.settlement_month].push_back(
            {calls.at(outcome.request_id), &outcome});
    }
    std::unordered_map<std::string, const WarehouseDrawRequest*> draws;
    for (const WarehouseDrawRequest& request :
        performance.warehouse_draw_requests) {
        draws.emplace(request.id, &request);
        plan.warehouse_requests[request.request_month].push_back(&request);
    }
    for (const FundingSettlementOutcome& outcome :
        performance.warehouse_draw_outcomes) {
        plan.warehouse_outcomes[outcome.settlement_month].push_back(
            {draws.at(outcome.request_id), &outcome});
    }
    for (const SupplementalFundingReceipt& receipt :
        performance.supplemental_receipts) {
        plan.supplemental[receipt.month].push_back(&receipt);
    }
    for (const EligibleBasisMovement& movement :
        performance.eligible_basis_movements) {
        plan.basis_movements[movement.month].push_back(&movement);
    }
    for (const ProtectionAbsorption& absorption :
        performance.protection_absorptions) {
        plan.absorptions[absorption.month].push_back(&absorption);
    }
    for (const ProtectionRelease& release :
        performance.protection_releases) {
        plan.releases[release.month].push_back(&release);
    }
    const auto sort_pointer_ids = [](auto& month_rows) {
        for (auto& rows : month_rows) {
            std::sort(rows.begin(), rows.end(), [](const auto* left,
                                                   const auto* right) {
                return left->id < right->id;
            });
        }
    };
    sort_pointer_ids(plan.call_requests);
    sort_pointer_ids(plan.warehouse_requests);
    sort_pointer_ids(plan.supplemental);
    sort_pointer_ids(plan.basis_movements);
    sort_pointer_ids(plan.absorptions);
    sort_pointer_ids(plan.releases);
    const auto sort_outcomes = [](auto& month_rows) {
        for (auto& rows : month_rows) {
            std::sort(rows.begin(), rows.end(), [](const auto& left,
                                                   const auto& right) {
                return left.first->id < right.first->id;
            });
        }
    };
    sort_outcomes(plan.call_outcomes);
    sort_outcomes(plan.warehouse_outcomes);
    return plan;
}

[[nodiscard]] long double borrowing_base(const WarehouseFacility& facility,
    long double eligible_basis, long double funded_protection) noexcept {
    return std::min({
        static_cast<long double>(facility.committed_limit_million),
        static_cast<long double>(facility.collateral_advance_rate) *
            eligible_basis,
        std::max(0.0L, eligible_basis - funded_protection)});
}

[[nodiscard]] FundingBridgeScenarioResult evaluate_scenario(
    const PreparedBridge& prepared, const FundingBridgeConfig& bridge,
    const FundingBridgeScenarioPerformance& performance,
    const JointScenarioResult& portfolio_result,
    const CapitalStackScenarioResult& stack_result) {
    const PortfolioConfig& portfolio = prepared.participated_portfolio;
    const JointScenario& scenario =
        find_joint_scenario(portfolio, performance.scenario_id);
    const std::vector<MonthlyDeclaredCash> declared =
        collect_declared_cash(portfolio, scenario);
    const std::size_t month_count = portfolio.horizon_months + 1U;
    const ScenarioPlan plan = prepare_scenario_plan(performance, month_count);
    const CallableCapitalFacility& callable = bridge.callable_facility;
    const WarehouseFacility& warehouse = bridge.warehouse_facility;

    FundingBridgeScenarioResult result;
    result.scenario_id = performance.scenario_id;
    result.months.reserve(month_count);
    result.minimum_funded_protection_headroom_million =
        std::numeric_limits<double>::infinity();
    result.minimum_warehouse_borrowing_base_headroom_million =
        std::numeric_limits<double>::infinity();

    std::unordered_map<std::string, ProviderRuntime> provider_runtime;
    for (const FundingProvider& provider : bridge.providers) {
        provider_runtime.emplace(provider.id, ProviderRuntime{});
    }
    provider_runtime.at(bridge.funded_at_close_provider_id)
        .settled_initial_and_supplemental =
        bridge.funded_at_close_cash_million;

    long double qualifying_protection_cash = 0.0L;
    long double callable_cash = 0.0L;
    long double cost_support_cash = 0.0L;
    long double warehouse_proceeds_cash = 0.0L;
    long double takeout_cash = 0.0L;
    long double project_principal_cash = 0.0L;
    long double project_nonprincipal_cash = 0.0L;
    long double eligible_basis = 0.0L;
    long double retained_protection_supporting_asset_basis = 0.0L;
    long double protection_invested_basis = 0.0L;
    long double funded_protection = 0.0L;
    long double warehouse_principal = 0.0L;
    long double unpaid_warehouse_charges = 0.0L;
    long double callable_available = callable.commitment_million;
    long double callable_called_unsettled = 0.0L;
    long double callable_defaulted = 0.0L;
    long double callable_expired = 0.0L;
    long double warehouse_called_unsettled = 0.0L;
    long double warehouse_defaulted = 0.0L;
    bool failed = false;
    bool takeout_used = false;
    std::vector<double> investor_cash_flows(month_count, 0.0);

    CompensatedSum total_call_requests;
    CompensatedSum total_call_receipts;
    CompensatedSum total_warehouse_requests;
    CompensatedSum total_warehouse_advances;
    CompensatedSum total_warehouse_repayments;
    CompensatedSum total_interest;
    CompensatedSum total_warehouse_fees;
    CompensatedSum total_callable_commitment_fees;
    CompensatedSum total_cost_support;
    CompensatedSum total_protection_replenishment;
    CompensatedSum total_takeout;
    CompensatedSum total_acquisition_uses;
    CompensatedSum total_buyer_costs;
    CompensatedSum total_pool_costs;
    CompensatedSum total_project_receipts;
    CompensatedSum total_shortfall;
    CompensatedSum liquidity_opportunity_cost_pv;

    for (std::size_t month = 0U; month < month_count; ++month) {
        FundingBridgeMonthlyResult monthly;
        monthly.month = month;
        monthly.opening_qualifying_protection_cash_million = checked_double(
            qualifying_protection_cash, "opening protection cash");
        monthly.opening_callable_cash_million =
            checked_double(callable_cash, "opening callable cash");
        monthly.opening_controlled_cash_million = checked_double(
            qualifying_protection_cash + callable_cash,
            "opening controlled cash");
        monthly.opening_cost_support_cash_million = checked_double(
            cost_support_cash, "opening cost-support cash");
        monthly.opening_warehouse_proceeds_cash_million = checked_double(
            warehouse_proceeds_cash, "opening warehouse-proceeds cash");
        monthly.opening_takeout_cash_million =
            checked_double(takeout_cash, "opening takeout cash");
        monthly.opening_project_principal_cash_million = checked_double(
            project_principal_cash, "opening project-principal cash");
        monthly.opening_project_nonprincipal_cash_million = checked_double(
            project_nonprincipal_cash, "opening project-nonprincipal cash");
        monthly.warehouse_opening_principal_million = checked_double(
            warehouse_principal, "opening warehouse principal");
        monthly.warehouse_opening_unpaid_charges_million = checked_double(
            unpaid_warehouse_charges, "opening unpaid warehouse charges");
        monthly.eligible_basis_opening_million =
            checked_double(eligible_basis, "opening eligible basis");
        monthly.funded_protection_opening_million = checked_double(
            funded_protection, "opening funded protection");
        monthly.qualifying_protection_invested_basis_opening_million =
            checked_double(protection_invested_basis,
                "opening invested protection basis");
        monthly.retained_protection_supporting_asset_basis_opening_million =
            checked_double(retained_protection_supporting_asset_basis,
                "opening retained protection-supporting asset basis");
        monthly.computed_information_set_id = information_set_id(
            canonical_observed_history(portfolio, performance, month));
        monthly.declared_counterfactual_project_principal_receipts_million =
            canonical_sum(declared[month].project_principal_receipt,
                "declared project principal receipts");
        const double declared_nonprincipal = canonical_sum(
            declared[month].project_nonprincipal_receipt,
            "declared project nonprincipal receipts");
        monthly.declared_counterfactual_project_receipts_million =
            monthly.declared_counterfactual_project_principal_receipts_million +
            declared_nonprincipal;

        if (failed) {
            monthly.callable_available_undrawn_million =
                checked_double(callable_available,
                    "available callable commitment");
            monthly.callable_called_unsettled_million = checked_double(
                callable_called_unsettled, "called-unsettled callable");
            monthly.callable_defaulted_million = checked_double(
                callable_defaulted, "defaulted callable");
            monthly.callable_expired_uncalled_million = checked_double(
                callable_expired, "expired callable");
            monthly.warehouse_draw_called_unsettled_million = checked_double(
                warehouse_called_unsettled,
                "called-unsettled warehouse draws");
            monthly.warehouse_draw_defaulted_million = checked_double(
                warehouse_defaulted, "defaulted warehouse draws");
            monthly.warehouse_closing_principal_million = checked_double(
                warehouse_principal, "warehouse closing principal");
            monthly.warehouse_closing_unpaid_charges_million = checked_double(
                unpaid_warehouse_charges,
                "warehouse closing unpaid charges");
            monthly.eligible_basis_closing_million = checked_double(
                eligible_basis, "eligible basis closing");
            monthly.funded_protection_closing_million = checked_double(
                funded_protection, "funded protection closing");
            monthly.qualifying_protection_invested_basis_closing_million =
                checked_double(protection_invested_basis,
                    "closing invested protection basis");
            monthly.retained_protection_supporting_asset_basis_closing_million =
                checked_double(retained_protection_supporting_asset_basis,
                    "closing retained protection-supporting asset basis");
            monthly.closing_qualifying_protection_cash_million =
                checked_double(qualifying_protection_cash,
                    "closing protection cash");
            monthly.closing_callable_cash_million = checked_double(
                callable_cash, "closing callable cash");
            monthly.closing_controlled_cash_million = checked_double(
                qualifying_protection_cash + callable_cash,
                "closing controlled cash");
            monthly.closing_cost_support_cash_million = checked_double(
                cost_support_cash, "closing cost-support cash");
            monthly.closing_warehouse_proceeds_cash_million = checked_double(
                warehouse_proceeds_cash, "closing warehouse-proceeds cash");
            monthly.closing_takeout_cash_million = checked_double(
                takeout_cash, "closing takeout cash");
            monthly.closing_project_principal_cash_million = checked_double(
                project_principal_cash, "closing project-principal cash");
            monthly.closing_project_nonprincipal_cash_million = checked_double(
                project_nonprincipal_cash,
                "closing project-nonprincipal cash");
            result.months.push_back(std::move(monthly));
            continue;
        }
        monthly.processed = true;

        const long double opening_total_cash =
            qualifying_protection_cash + callable_cash + cost_support_cash +
            warehouse_proceeds_cash + takeout_cash + project_principal_cash +
            project_nonprincipal_cash;
        long double current_cash_sources = opening_total_cash;
        long double current_cash_uses = 0.0L;
        long double warehouse_repayment = 0.0L;
        bool settlement_batch_completed = false;
        std::vector<std::string> pending_failure_source_record_ids;
        std::vector<FundingBridgeFailureRecord::ProviderCapacityBreach>
            pending_provider_capacity_breaches;
        long double pending_rejected_transaction = 0.0L;
        long double pending_eligible_purpose_cash = 0.0L;

        const auto fail = [&](FundingBridgeFailureKind kind,
                              FundingBridgeFailurePhase phase,
                              std::string provider_id,
                              std::string facility_id,
                              std::string request_id,
                              std::string source_record_id,
                              long double due,
                              long double available,
                              std::string explanation) {
            if (failed) {
                return;
            }
            failed = true;
            result.failure_kind = kind;
            result.failure_phase = phase;
            result.first_infeasible_month = month;
            FundingBridgeFailureRecord record;
            record.kind = kind;
            record.phase = phase;
            record.month = month;
            record.provider_id = std::move(provider_id);
            record.facility_id = std::move(facility_id);
            record.request_id = std::move(request_id);
            record.source_record_id = std::move(source_record_id);
            record.amount_due_million = checked_double(due, "failure due");
            record.amount_available_million =
                checked_double(available, "failure available");
            record.shortfall_million = canonical_nonnegative(
                due - available, std::max(due, available));
            record.rejected_transaction_million = checked_double(
                pending_rejected_transaction,
                "failure rejected transaction");
            record.eligible_purpose_cash_available_million = checked_double(
                pending_eligible_purpose_cash,
                "failure eligible-purpose cash available");
            if (!record.source_record_id.empty()) {
                pending_failure_source_record_ids.push_back(
                    record.source_record_id);
            }
            std::sort(pending_failure_source_record_ids.begin(),
                pending_failure_source_record_ids.end());
            pending_failure_source_record_ids.erase(std::unique(
                pending_failure_source_record_ids.begin(),
                pending_failure_source_record_ids.end()),
                pending_failure_source_record_ids.end());
            record.causal_source_record_ids =
                pending_failure_source_record_ids;
            record.settlement_shortfalls =
                result.provider_settlement_shortfalls;
            record.provider_capacity_breaches =
                pending_provider_capacity_breaches;
            record.explanation = std::move(explanation);
            result.failure = std::move(record);
        };

        monthly.funded_at_close_receipt_million =
            month == 0U ? bridge.funded_at_close_cash_million : 0.0;
        if (month == 0U) {
            qualifying_protection_cash +=
                bridge.funded_at_close_cash_million;
            funded_protection += bridge.funded_at_close_cash_million;
            monthly.funded_protection_paid_million +=
                bridge.funded_at_close_cash_million;
            current_cash_sources += bridge.funded_at_close_cash_million;
        }

        std::vector<double> call_request_values;
        for (const CapitalCallRequest* request : plan.call_requests[month]) {
            monthly.capital_call_requested_million +=
                request->requested_million;
            total_call_requests.add(request->requested_million);
            call_request_values.push_back(request->requested_million);
        }
        const long double call_request_batch = canonical_sum(
            std::move(call_request_values), "capital-call request batch");
        const bool call_requests_present =
            !plan.call_requests[month].empty();
        const bool call_request_window_open =
            month >= callable.availability_start_month &&
            month <= callable.contractual_expiry_month;
        const bool call_request_batch_ok = !call_requests_present ||
            (call_request_window_open &&
                !materially_exceeds(
                    call_request_batch, callable_available));

        std::vector<double> eligible_addition_values;
        for (const EligibleBasisMovement* movement :
            plan.basis_movements[month]) {
            if (movement->kind ==
                EligibleBasisMovementKind::EligibleAddition) {
                eligible_addition_values.push_back(movement->amount_million);
            }
        }
        const long double planned_eligible_additions = canonical_sum(
            std::move(eligible_addition_values),
            "planned eligible-basis additions");
        const long double pro_forma_eligible_basis =
            eligible_basis + planned_eligible_additions;
        long double pre_use_required_protection = std::min(
            static_cast<long double>(prepared.first_loss_notional_million),
            pro_forma_eligible_basis);
        long double pre_use_borrowing_base = borrowing_base(
            warehouse, pro_forma_eligible_basis, funded_protection);

        const bool warehouse_requests_present =
            !plan.warehouse_requests[month].empty();
        long double warehouse_request_batch = 0.0L;
        long double prospective_warehouse_dependency =
            warehouse_principal + warehouse_called_unsettled +
            warehouse_defaulted;
        bool warehouse_request_window_open = true;
        bool warehouse_request_batch_ok = true;
        if (warehouse_requests_present) {
            std::vector<double> warehouse_request_values;
            for (const WarehouseDrawRequest* request :
                plan.warehouse_requests[month]) {
                monthly.warehouse_draw_requested_million +=
                    request->requested_million;
                total_warehouse_requests.add(request->requested_million);
                warehouse_request_values.push_back(
                    request->requested_million);
            }
            warehouse_request_batch = canonical_sum(
                std::move(warehouse_request_values),
                "warehouse request batch");
            warehouse_request_window_open =
                month >= warehouse.availability_start_month &&
                month <= warehouse.availability_end_month;
            prospective_warehouse_dependency =
                warehouse_principal + warehouse_called_unsettled +
                warehouse_defaulted + warehouse_request_batch;
            warehouse_request_batch_ok = warehouse_request_window_open &&
                !materially_exceeds(prospective_warehouse_dependency,
                    warehouse.committed_limit_million);
            for (const WarehouseDrawRequest* request :
                plan.warehouse_requests[month]) {
                WarehouseAdvanceTestResult test;
                test.request_id = request->id;
                test.phase = WarehouseAdvanceTestPhase::Request;
                test.requested_million = request->requested_million;
                test.prospective_funded_and_contingent_dependency_million =
                    checked_double(prospective_warehouse_dependency,
                        "prospective warehouse dependency");
                test.principal_after_settlement_million =
                    checked_double(warehouse_principal,
                        "warehouse principal before request settlement");
                test.eligible_basis_million = checked_double(
                    pro_forma_eligible_basis, "pro-forma eligible basis");
                test.funded_protection_million = checked_double(
                    funded_protection, "funded protection");
                test.required_funded_protection_million = checked_double(
                    pre_use_required_protection, "required protection");
                test.borrowing_base_million = checked_double(
                    pre_use_borrowing_base, "warehouse borrowing base");
                test.borrowing_base_headroom_million = checked_double(
                    pre_use_borrowing_base - warehouse_principal,
                    "warehouse borrowing-base headroom");
                test.protection_headroom_million = checked_double(
                    funded_protection - pre_use_required_protection,
                    "funded-protection headroom");
                test.facility_limit_passed = warehouse_request_batch_ok;
                test.funded_protection_test_applicable = false;
                test.borrowing_base_test_applicable = false;
                test.passed = warehouse_request_batch_ok;
                monthly.warehouse_advance_tests.push_back(test);
            }
        }

        // Call and warehouse notices are one start-of-month policy action.
        // Both facility batches are tested before either contingent state is
        // mutated; a failed side cannot leave the other half committed merely
        // because one facility type happened to be processed first.
        const bool request_phase_accepted =
            call_request_batch_ok && warehouse_request_batch_ok;
        for (const CapitalCallRequest* request : plan.call_requests[month]) {
            result.provider_requests.push_back({month,
                FundingRequestKind::CallableCapital, callable.provider_id,
                callable.id, request->id, callable.source_record_id,
                request->requested_million, request_phase_accepted});
        }
        for (const WarehouseDrawRequest* request :
            plan.warehouse_requests[month]) {
            result.provider_requests.push_back({month,
                FundingRequestKind::WarehouseDraw, warehouse.provider_id,
                warehouse.id, request->id, warehouse.source_record_id,
                request->requested_million, request_phase_accepted});
        }
        if (request_phase_accepted) {
            callable_available -= call_request_batch;
            callable_called_unsettled += call_request_batch;
            warehouse_called_unsettled += warehouse_request_batch;
        } else if (!call_request_batch_ok &&
            !warehouse_request_batch_ok) {
            const long double callable_incremental_available =
                call_request_window_open ? callable_available : 0.0L;
            const long double warehouse_incremental_available =
                warehouse_request_window_open
                ? std::max(0.0L,
                    static_cast<long double>(
                        warehouse.committed_limit_million) -
                        warehouse_principal - warehouse_called_unsettled -
                        warehouse_defaulted)
                : 0.0L;
            pending_failure_source_record_ids = {
                callable.source_record_id, warehouse.source_record_id};
            fail(FundingBridgeFailureKind::SimultaneousFacilityRequestFailure,
                FundingBridgeFailurePhase::ContractAndProviderTest,
                {}, {}, {}, {},
                call_request_batch + warehouse_request_batch,
                std::min(call_request_batch,
                    callable_incremental_available) +
                    std::min(warehouse_request_batch,
                        warehouse_incremental_available),
                "atomic same-month call and warehouse request batches both fail their facility tests");
        } else if (!call_request_batch_ok) {
            const std::string request_id =
                plan.call_requests[month].size() == 1U
                ? plan.call_requests[month].front()->id
                : std::string{};
            fail(FundingBridgeFailureKind::CallableFacilityUnavailable,
                FundingBridgeFailurePhase::ContractAndProviderTest,
                callable.provider_id, callable.id, request_id,
                callable.source_record_id, call_request_batch,
                call_request_window_open ? callable_available : 0.0L,
                call_request_window_open
                    ? "same-month capital-call request batch exceeds available uncalled commitment"
                    : "same-month capital-call request batch lies outside contractual availability");
        } else {
            const std::string request_id =
                plan.warehouse_requests[month].size() == 1U
                ? plan.warehouse_requests[month].front()->id
                : std::string{};
            fail(warehouse_request_window_open
                    ? FundingBridgeFailureKind::WarehouseLimitExceeded
                    : FundingBridgeFailureKind::WarehouseFacilityUnavailable,
                FundingBridgeFailurePhase::ContractAndProviderTest,
                warehouse.provider_id, warehouse.id, request_id,
                warehouse.source_record_id,
                warehouse_request_window_open
                    ? prospective_warehouse_dependency
                    : warehouse_request_batch,
                warehouse_request_window_open
                    ? static_cast<long double>(
                        warehouse.committed_limit_million)
                    : 0.0L,
                warehouse_request_window_open
                    ? "same-month requested plus funded warehouse dependency exceeds the facility limit"
                    : "same-month warehouse request batch lies outside contractual availability");
        }

        const bool callable_notice_window_open =
            month >= callable.availability_start_month &&
            month <= callable.contractual_expiry_month;
        const long double callable_fee_base = callable_notice_window_open
            ? callable_available + callable_called_unsettled
            : 0.0L;
        const long double callable_liquidity_base =
            month >= callable.availability_start_month
            ? (callable_notice_window_open ? callable_available : 0.0L) +
                callable_called_unsettled
            : 0.0L;
        if (!failed && callable_notice_window_open) {
            monthly.callable_commitment_fee_due_million = checked_double(
                callable_fee_base *
                    static_cast<long double>(
                        callable.annual_commitment_fee_rate / 12.0),
                "callable commitment fee");
        }
        if (!failed && callable_liquidity_base > 0.0L) {
            monthly.callable_liquidity_opportunity_cost_million = checked_double(
                callable_liquidity_base *
                    static_cast<long double>(
                        callable.liquidity_reserve_fraction) *
                    static_cast<long double>(
                        (callable.annual_liquidity_hurdle_rate -
                            callable.annual_reserve_yield_rate) /
                        12.0),
                "callable liquidity opportunity cost");
            liquidity_opportunity_cost_pv.add(
                static_cast<long double>(
                    monthly.callable_liquidity_opportunity_cost_million) /
                discount_factor(portfolio.annual_physical_hurdle_rate, month));
        }

        if (!failed) {
            for (const auto& [request, outcome] : plan.call_outcomes[month]) {
                callable_called_unsettled -= request->requested_million;
                const long double default_amount =
                    static_cast<long double>(request->requested_million) -
                    outcome->actual_cash_million;
                result.provider_settlements.push_back(
                    {month, outcome->status, callable.provider_id,
                        callable.id, request->id, outcome->source_record_id,
                        request->requested_million,
                        outcome->actual_cash_million,
                        checked_double(default_amount,
                            "callable settlement missing cash")});
                callable_defaulted += default_amount;
                ProviderRuntime& provider =
                    provider_runtime.at(callable.provider_id);
                provider.callable_defaulted += default_amount;
                provider.nonperformance =
                    provider.nonperformance || default_amount > 0.0L;
                provider.settled_callable += outcome->actual_cash_million;
                if (default_amount > 0.0L) {
                    FundingBridgeFailureRecord::SettlementShortfall shortfall{
                        month, callable.provider_id, callable.id, request->id,
                        outcome->source_record_id,
                        checked_double(default_amount,
                            "callable settlement shortfall")};
                    result.provider_settlement_shortfalls.push_back(
                        std::move(shortfall));
                }
                monthly.capital_call_settled_million +=
                    outcome->actual_cash_million;
                total_call_receipts.add(outcome->actual_cash_million);
                current_cash_sources += outcome->actual_cash_million;
                const long double repayment = std::min(warehouse_principal,
                    static_cast<long double>(outcome->actual_cash_million));
                warehouse_principal -= repayment;
                warehouse_repayment += repayment;
                monthly.warehouse_principal_repaid_from_call_settlement_million +=
                    checked_double(repayment, "call-settlement debt repayment");
                callable_cash +=
                    static_cast<long double>(outcome->actual_cash_million) -
                    repayment;
            }
        }

        if (!failed) {
            for (const auto& [request, outcome] :
                plan.warehouse_outcomes[month]) {
                warehouse_called_unsettled -= request->requested_million;
                const long double default_amount =
                    static_cast<long double>(request->requested_million) -
                    outcome->actual_cash_million;
                result.provider_settlements.push_back(
                    {month, outcome->status, warehouse.provider_id,
                        warehouse.id, request->id, outcome->source_record_id,
                        request->requested_million,
                        outcome->actual_cash_million,
                        checked_double(default_amount,
                            "warehouse settlement missing cash")});
                warehouse_defaulted += default_amount;
                ProviderRuntime& provider =
                    provider_runtime.at(warehouse.provider_id);
                provider.warehouse_defaulted += default_amount;
                provider.cumulative_settled_warehouse_advances +=
                    outcome->actual_cash_million;
                provider.nonperformance =
                    provider.nonperformance || default_amount > 0.0L;
                if (default_amount > 0.0L) {
                    FundingBridgeFailureRecord::SettlementShortfall shortfall{
                        month, warehouse.provider_id, warehouse.id,
                        request->id, outcome->source_record_id,
                        checked_double(default_amount,
                            "warehouse settlement shortfall")};
                    result.provider_settlement_shortfalls.push_back(
                        std::move(shortfall));
                }
                warehouse_principal += outcome->actual_cash_million;
                warehouse_proceeds_cash += outcome->actual_cash_million;
                monthly.warehouse_advance_settled_million +=
                    outcome->actual_cash_million;
                total_warehouse_advances.add(outcome->actual_cash_million);
                current_cash_sources += outcome->actual_cash_million;
            }
        }

        if (!failed) {
            for (const SupplementalFundingReceipt* receipt :
                plan.supplemental[month]) {
                ProviderRuntime& provider =
                    provider_runtime.at(receipt->provider_id);
                current_cash_sources += receipt->actual_cash_million;
                switch (receipt->purpose) {
                case SupplementalFundingPurpose::CostSupport:
                    cost_support_cash += receipt->actual_cash_million;
                    monthly.cost_support_receipts_million +=
                        receipt->actual_cash_million;
                    total_cost_support.add(receipt->actual_cash_million);
                    provider.settled_initial_and_supplemental +=
                        receipt->actual_cash_million;
                    break;
                case SupplementalFundingPurpose::ProtectionReplenishment:
                    qualifying_protection_cash += receipt->actual_cash_million;
                    funded_protection += receipt->actual_cash_million;
                    monthly.protection_replenishment_receipts_million +=
                        receipt->actual_cash_million;
                    monthly.funded_protection_paid_million +=
                        receipt->actual_cash_million;
                    total_protection_replenishment.add(
                        receipt->actual_cash_million);
                    provider.settled_initial_and_supplemental +=
                        receipt->actual_cash_million;
                    break;
                case SupplementalFundingPurpose::SettledTakeout:
                    takeout_cash += receipt->actual_cash_million;
                    monthly.settled_takeout_receipts_million +=
                        receipt->actual_cash_million;
                    total_takeout.add(receipt->actual_cash_million);
                    provider.settled_takeout += receipt->actual_cash_million;
                    takeout_used = true;
                    break;
                }
            }
            const long double takeout_repayment =
                std::min(warehouse_principal, takeout_cash);
            takeout_cash -= takeout_repayment;
            warehouse_principal -= takeout_repayment;
            warehouse_repayment += takeout_repayment;
            monthly.warehouse_principal_repaid_from_takeout_million =
                checked_double(takeout_repayment, "takeout debt repayment");
            pre_use_required_protection = std::min(
                static_cast<long double>(
                    prepared.first_loss_notional_million),
                pro_forma_eligible_basis);
            pre_use_borrowing_base = borrowing_base(
                warehouse, pro_forma_eligible_basis, funded_protection);
            settlement_batch_completed = true;
        }

        // All same-month final outcomes and supplemental cash are booked as
        // one deterministic batch before testing the funded warehouse state.
        // This prevents request-id ordering from suppressing later cash,
        // defaults, protection replenishment, or contractual charges.
        if (!failed && !plan.warehouse_outcomes[month].empty()) {
            const long double settlement_dependency =
                warehouse_principal + warehouse_called_unsettled +
                warehouse_defaulted;
            const bool limit_ok = !materially_exceeds(
                settlement_dependency, warehouse.committed_limit_million);
            const bool any_settled_warehouse =
                !near_input_money(
                    monthly.warehouse_advance_settled_million, 0.0L);
            const bool protection_ok = !any_settled_warehouse ||
                !materially_exceeds(
                    pre_use_required_protection, funded_protection);
            const bool base_ok = !any_settled_warehouse ||
                !materially_exceeds(
                    warehouse_principal, pre_use_borrowing_base);

            for (const auto& [request, outcome] :
                plan.warehouse_outcomes[month]) {
                WarehouseAdvanceTestResult test;
                test.request_id = request->id;
                test.phase = WarehouseAdvanceTestPhase::Settlement;
                test.requested_million = request->requested_million;
                test.settled_million = outcome->actual_cash_million;
                test.prospective_funded_and_contingent_dependency_million =
                    checked_double(settlement_dependency,
                        "warehouse dependency after settlement batch");
                test.principal_after_settlement_million = checked_double(
                    warehouse_principal,
                    "warehouse principal after settlement batch");
                test.eligible_basis_million = checked_double(
                    pro_forma_eligible_basis, "pro-forma eligible basis");
                test.funded_protection_million = checked_double(
                    funded_protection, "funded protection");
                test.required_funded_protection_million = checked_double(
                    pre_use_required_protection, "required protection");
                test.borrowing_base_million = checked_double(
                    pre_use_borrowing_base, "warehouse borrowing base");
                test.borrowing_base_headroom_million = checked_double(
                    pre_use_borrowing_base - warehouse_principal,
                    "warehouse borrowing-base headroom");
                test.protection_headroom_million = checked_double(
                    funded_protection - pre_use_required_protection,
                    "funded-protection headroom");
                const bool funded_test_applicable = !near_input_money(
                    outcome->actual_cash_million, 0.0L);
                test.facility_limit_passed = limit_ok;
                test.funded_protection_test_applicable =
                    funded_test_applicable;
                test.funded_protection_test_passed =
                    !funded_test_applicable || protection_ok;
                test.borrowing_base_test_applicable =
                    funded_test_applicable;
                test.borrowing_base_test_passed =
                    !funded_test_applicable || base_ok;
                test.passed = limit_ok &&
                    test.funded_protection_test_passed &&
                    test.borrowing_base_test_passed;
                monthly.warehouse_advance_tests.push_back(test);
            }

            const WarehouseDrawRequest* headline_request = nullptr;
            const FundingSettlementOutcome* headline_outcome = nullptr;
            std::size_t positive_settlement_count = 0U;
            std::vector<std::string> settlement_batch_lineage = {
                warehouse.source_record_id};
            for (const auto& [request, outcome] :
                plan.warehouse_outcomes[month]) {
                if (!near_input_money(
                        outcome->actual_cash_million, 0.0L)) {
                    ++positive_settlement_count;
                    headline_request = request;
                    headline_outcome = outcome;
                    settlement_batch_lineage.push_back(
                        outcome->source_record_id);
                }
            }
            if (positive_settlement_count != 1U) {
                headline_request = nullptr;
                headline_outcome = nullptr;
            }
            const std::string headline_request_id = headline_request != nullptr
                ? headline_request->id
                : std::string{};
            const std::string headline_source_id = headline_outcome != nullptr
                ? headline_outcome->source_record_id
                : std::string{};
            if (!limit_ok || !protection_ok || !base_ok) {
                pending_failure_source_record_ids =
                    std::move(settlement_batch_lineage);
            }
            if (!limit_ok) {
                fail(FundingBridgeFailureKind::WarehouseLimitExceeded,
                    FundingBridgeFailurePhase::ContractAndProviderTest,
                    warehouse.provider_id, warehouse.id,
                    headline_request_id, headline_source_id,
                    settlement_dependency,
                    warehouse.committed_limit_million,
                    "same-month warehouse settlement batch exceeds the facility limit");
            } else if (!protection_ok) {
                fail(FundingBridgeFailureKind::FundedProtectionDeficit,
                    FundingBridgeFailurePhase::ContractAndProviderTest,
                    warehouse.provider_id, warehouse.id,
                    headline_request_id, headline_source_id,
                    pre_use_required_protection, funded_protection,
                    "same-month settled warehouse advances lack funded protection after all supplemental cash");
            } else if (!base_ok) {
                fail(FundingBridgeFailureKind::WarehouseBorrowingBaseDeficit,
                    FundingBridgeFailurePhase::ContractAndProviderTest,
                    warehouse.provider_id, warehouse.id,
                    headline_request_id, headline_source_id,
                    warehouse_principal, pre_use_borrowing_base,
                    "same-month settled warehouse advances exceed the borrowing base after all supplemental cash");
            }
        }

        if (!failed) {
            long double breached_capacity_used = 0.0L;
            long double breached_capacity_available = 0.0L;
            for (const FundingProvider& provider : bridge.providers) {
                const ProviderRuntime& runtime =
                    provider_runtime.at(provider.id);
                long double used = runtime.settled_initial_and_supplemental +
                    runtime.settled_callable + runtime.settled_takeout;
                if (provider.id == callable.provider_id) {
                    used += callable_available + callable_called_unsettled;
                }
                if (provider.id == warehouse.provider_id) {
                    const long double contingent =
                        month <= warehouse.availability_end_month
                        ? std::max(0.0L,
                            static_cast<long double>(
                                warehouse.committed_limit_million) -
                                warehouse_principal - warehouse_defaulted)
                        : warehouse_called_unsettled;
                    used += warehouse_principal + contingent;
                }
                if (materially_exceeds(
                        used, provider.declared_capacity_million)) {
                    const long double excess =
                        used - provider.declared_capacity_million;
                    pending_provider_capacity_breaches.push_back(
                        {provider.id, provider.source_record_id,
                            checked_double(used,
                                "breached provider capacity used"),
                            provider.declared_capacity_million,
                            checked_double(excess,
                                "provider capacity excess")});
                    pending_failure_source_record_ids.push_back(
                        provider.source_record_id);
                    breached_capacity_used += used;
                    breached_capacity_available +=
                        provider.declared_capacity_million;
                }
            }
            if (!pending_provider_capacity_breaches.empty()) {
                std::sort(pending_provider_capacity_breaches.begin(),
                    pending_provider_capacity_breaches.end(),
                    [](const auto& left, const auto& right) {
                        return left.provider_id < right.provider_id;
                    });
                const bool one_breach =
                    pending_provider_capacity_breaches.size() == 1U;
                fail(FundingBridgeFailureKind::ProviderCapacityExceeded,
                    FundingBridgeFailurePhase::ContractAndProviderTest,
                    one_breach
                        ? pending_provider_capacity_breaches.front().provider_id
                        : std::string{},
                    {}, {},
                    one_breach
                        ? pending_provider_capacity_breaches.front()
                              .source_record_id
                        : std::string{},
                    breached_capacity_used, breached_capacity_available,
                    one_breach
                        ? "settled funding and contingent obligations exceed declared provider capacity"
                        : "multiple legal providers simultaneously exceed declared capacity");
            }
        }

        if (settlement_batch_completed) {
            monthly.warehouse_interest_due_million = checked_double(
                warehouse_principal *
                    static_cast<long double>(
                        warehouse.annual_interest_rate / 12.0),
                "warehouse monthly interest");
            if (month >= warehouse.availability_start_month &&
                month <= warehouse.availability_end_month) {
                monthly.warehouse_undrawn_fee_due_million = checked_double(
                    std::max(0.0L,
                        static_cast<long double>(
                            warehouse.committed_limit_million) -
                            warehouse_principal -
                            warehouse_called_unsettled -
                            warehouse_defaulted) *
                        static_cast<long double>(
                            warehouse.annual_undrawn_fee_rate / 12.0),
                    "warehouse monthly undrawn fee");
            }
            monthly.warehouse_advance_fee_due_million =
                monthly.warehouse_advance_settled_million *
                warehouse.advance_fee_rate;
            monthly.warehouse_upfront_fee_due_million =
                month == warehouse.availability_start_month
                ? warehouse.committed_limit_million *
                    warehouse.upfront_fee_rate
                : 0.0;
        }

        if (settlement_batch_completed) {
            monthly.buyer_direct_cost_due_million = canonical_sum(
                declared[month].buyer_direct_cost, "buyer-direct costs");
            monthly.pool_cost_due_million = canonical_sum(
                declared[month].pool_cost, "pool costs");
            total_buyer_costs.add(monthly.buyer_direct_cost_due_million);
            total_pool_costs.add(monthly.pool_cost_due_million);
        }
        const long double warehouse_charge_due =
            static_cast<long double>(monthly.warehouse_interest_due_million) +
            monthly.warehouse_undrawn_fee_due_million +
            monthly.warehouse_advance_fee_due_million +
            monthly.warehouse_upfront_fee_due_million;
        monthly.total_nonasset_cost_due_million = checked_double(
            static_cast<long double>(monthly.buyer_direct_cost_due_million) +
                monthly.pool_cost_due_million +
                monthly.callable_commitment_fee_due_million +
                warehouse_charge_due,
            "total nonasset cost due");
        total_interest.add(monthly.warehouse_interest_due_million);
        total_warehouse_fees.add(
            static_cast<long double>(monthly.warehouse_undrawn_fee_due_million) +
            monthly.warehouse_advance_fee_due_million +
            monthly.warehouse_upfront_fee_due_million);
        total_callable_commitment_fees.add(
            monthly.callable_commitment_fee_due_million);

        // A settlement or provider-capacity failure does not erase interest
        // and fees already crystallized on cash booked earlier in the same
        // monthly batch. They remain unpaid warehouse EAD when step 6 is not
        // reached.
        if (failed && settlement_batch_completed &&
            monthly.total_nonasset_cost_due_million > 0.0) {
            monthly.total_nonasset_cost_unpaid_million =
                monthly.total_nonasset_cost_due_million;
            if (warehouse_charge_due > 0.0L) {
                monthly.warehouse_charges_unpaid_million = checked_double(
                    warehouse_charge_due,
                    "warehouse charges crystallized before failure");
                unpaid_warehouse_charges += warehouse_charge_due;
            }
        }

        if (!failed) {
            const long double cost_due =
                monthly.total_nonasset_cost_due_million;
            if (materially_exceeds(cost_due, cost_support_cash)) {
                monthly.total_nonasset_cost_unpaid_million =
                    checked_double(cost_due,
                        "atomic nonasset-cost amount rejected");
                monthly.warehouse_charges_unpaid_million = checked_double(
                    warehouse_charge_due,
                    "atomic unpaid warehouse charges");
                unpaid_warehouse_charges +=
                    monthly.warehouse_charges_unpaid_million;
                const long double shortfall = cost_due - cost_support_cash;
                monthly.nonasset_cost_liquidity_gap_million =
                    checked_double(shortfall,
                        "nonasset-cost liquidity gap");
                if (result.first_funding_shortfall_million == 0.0) {
                    result.first_funding_shortfall_million =
                        checked_double(shortfall, "first funding shortfall");
                }
                total_shortfall.add(shortfall);
                pending_rejected_transaction = cost_due;
                pending_failure_source_record_ids =
                    declared[month].buyer_direct_cost_lineage;
                pending_failure_source_record_ids.insert(
                    pending_failure_source_record_ids.end(),
                    declared[month].pool_cost_lineage.begin(),
                    declared[month].pool_cost_lineage.end());
                if (monthly.callable_commitment_fee_due_million > 0.0) {
                    pending_failure_source_record_ids.push_back(
                        callable.source_record_id);
                }
                if (warehouse_charge_due > 0.0L) {
                    pending_failure_source_record_ids.push_back(
                        warehouse.source_record_id);
                }
                fail(FundingBridgeFailureKind::NonAssetCostShortfall,
                    FundingBridgeFailurePhase::NonAssetCosts, {}, {}, {}, {},
                    cost_due, cost_support_cash,
                    "named settled cost-support cash is insufficient; no automatic sponsor cure is created");
            } else {
                cost_support_cash -= cost_due;
                current_cash_uses += cost_due;
                monthly.total_nonasset_cost_paid_million =
                    checked_double(cost_due, "nonasset costs paid");
                monthly.warehouse_charges_paid_million = checked_double(
                    warehouse_charge_due, "warehouse charges paid");
            }
        }

        if (!failed) {
            monthly.acquisition_and_primary_funding_due_million = canonical_sum(
                declared[month].acquisition_and_primary,
                "acquisition and primary-funding uses");
            const long double total_due =
                monthly.acquisition_and_primary_funding_due_million;
            const long double eligible_due = planned_eligible_additions;
            const long double ineligible_due = total_due - eligible_due;
            long double callable_after_ineligible = callable_cash;
            if (materially_exceeds(ineligible_due, callable_after_ineligible)) {
                monthly.acquisition_and_primary_funding_unpaid_million =
                    checked_double(total_due,
                        "atomic asset-funding transaction rejected");
                const long double shortfall =
                    ineligible_due - callable_cash;
                monthly.acquisition_and_primary_funding_liquidity_gap_million =
                    checked_double(shortfall,
                        "restricted ineligible-use liquidity gap");
                if (result.first_funding_shortfall_million == 0.0) {
                    result.first_funding_shortfall_million =
                        checked_double(shortfall, "first funding shortfall");
                }
                total_shortfall.add(shortfall);
                pending_rejected_transaction = total_due;
                pending_eligible_purpose_cash =
                    qualifying_protection_cash + warehouse_proceeds_cash;
                pending_failure_source_record_ids =
                    declared[month].acquisition_and_primary_lineage;
                fail(FundingBridgeFailureKind::FundingUseShortfall,
                    FundingBridgeFailurePhase::AssetFundingUse,
                    {}, {}, {}, {}, ineligible_due, callable_cash,
                    "atomic asset use is rejected because callable cash cannot cover its ineligible-purpose component");
            } else {
                callable_after_ineligible -= ineligible_due;
                const long double eligible_available =
                    qualifying_protection_cash + warehouse_proceeds_cash +
                    callable_after_ineligible;
                if (materially_exceeds(eligible_due, eligible_available)) {
                    monthly.acquisition_and_primary_funding_unpaid_million =
                        checked_double(total_due,
                            "atomic asset-funding transaction rejected");
                    const long double shortfall =
                        eligible_due - eligible_available;
                    monthly.acquisition_and_primary_funding_liquidity_gap_million =
                        checked_double(shortfall,
                            "eligible-use liquidity gap");
                    if (result.first_funding_shortfall_million == 0.0) {
                        result.first_funding_shortfall_million =
                            checked_double(
                                shortfall, "first funding shortfall");
                    }
                    total_shortfall.add(shortfall);
                    pending_rejected_transaction = total_due;
                    pending_eligible_purpose_cash = eligible_available;
                    pending_failure_source_record_ids =
                        declared[month].acquisition_and_primary_lineage;
                    fail(FundingBridgeFailureKind::FundingUseShortfall,
                        FundingBridgeFailurePhase::AssetFundingUse,
                        {}, {}, {}, {}, eligible_due, eligible_available,
                        "atomic asset use is rejected because eligible-purpose settled funding is insufficient");
                } else {
                    callable_cash -= ineligible_due;
                    monthly.asset_uses_paid_from_callable_cash_million =
                        checked_double(ineligible_due,
                            "ineligible use paid from callable cash");
                    long double remaining_eligible = eligible_due;
                    const long double protection_use = std::min(
                        remaining_eligible, qualifying_protection_cash);
                    qualifying_protection_cash -= protection_use;
                    protection_invested_basis += protection_use;
                    remaining_eligible -= protection_use;
                    const long double warehouse_use = std::min(
                        remaining_eligible, warehouse_proceeds_cash);
                    warehouse_proceeds_cash -= warehouse_use;
                    remaining_eligible -= warehouse_use;
                    callable_cash -= remaining_eligible;
                    monthly.asset_uses_paid_from_qualifying_protection_cash_million =
                        checked_double(protection_use,
                            "asset use paid from protection cash");
                    monthly.asset_uses_paid_from_warehouse_proceeds_million =
                        checked_double(warehouse_use,
                            "asset use paid from warehouse proceeds");
                    monthly.asset_uses_paid_from_callable_cash_million +=
                        checked_double(remaining_eligible,
                            "asset use paid from callable cash");
                    monthly.acquisition_and_primary_funding_paid_million =
                        checked_double(total_due, "asset use paid");
                    current_cash_uses += total_due;
                    eligible_basis += eligible_due;
                    retained_protection_supporting_asset_basis += eligible_due;
                    monthly.eligible_basis_additions_million =
                        checked_double(eligible_due,
                            "eligible-basis additions");
                    total_acquisition_uses.add(total_due);
                }
            }
        }

        if (!failed) {
            project_principal_cash +=
                monthly.declared_counterfactual_project_principal_receipts_million;
            project_nonprincipal_cash += declared_nonprincipal;
            monthly.actual_project_principal_receipts_million =
                monthly.declared_counterfactual_project_principal_receipts_million;
            monthly.actual_project_nonprincipal_receipts_million =
                declared_nonprincipal;
            monthly.actual_project_receipts_million =
                monthly.actual_project_principal_receipts_million +
                monthly.actual_project_nonprincipal_receipts_million;
            current_cash_sources += monthly.actual_project_receipts_million;
            total_project_receipts.add(monthly.actual_project_receipts_million);

            const long double principal_sweep = std::min(
                warehouse_principal, project_principal_cash);
            project_principal_cash -= principal_sweep;
            warehouse_principal -= principal_sweep;
            warehouse_repayment += principal_sweep;
            monthly.warehouse_principal_repaid_from_project_principal_million =
                checked_double(
                    principal_sweep, "project-principal warehouse sweep");
            const long double nonprincipal_sweep = std::min(
                warehouse_principal, project_nonprincipal_cash);
            project_nonprincipal_cash -= nonprincipal_sweep;
            warehouse_principal -= nonprincipal_sweep;
            warehouse_repayment += nonprincipal_sweep;
            monthly.warehouse_principal_repaid_from_project_nonprincipal_million =
                checked_double(nonprincipal_sweep,
                    "project-nonprincipal warehouse sweep");
        }

        if (!failed) {
            long double cash_returning_basis_reductions = 0.0L;
            long double writeoffs = 0.0L;
            std::vector<std::string> reduction_source_record_ids;
            for (const EligibleBasisMovement* movement :
                plan.basis_movements[month]) {
                if (movement->kind ==
                    EligibleBasisMovementKind::EligibleAddition) {
                    continue;
                }
                if (materially_exceeds(
                        movement->amount_million, eligible_basis)) {
                    throw std::logic_error(
                        "validated eligible-basis reduction exceeds funded basis");
                }
                eligible_basis -= movement->amount_million;
                reduction_source_record_ids.push_back(
                    movement->source_record_id);
                switch (movement->kind) {
                case EligibleBasisMovementKind::EligibleAddition:
                    break;
                case EligibleBasisMovementKind::PrincipalBasisReturn:
                    monthly.eligible_basis_principal_returns_million +=
                        movement->amount_million;
                    cash_returning_basis_reductions +=
                        movement->amount_million;
                    break;
                case EligibleBasisMovementKind::Disposition:
                    monthly.eligible_basis_dispositions_million +=
                        movement->amount_million;
                    cash_returning_basis_reductions +=
                        movement->amount_million;
                    break;
                case EligibleBasisMovementKind::Writeoff:
                    monthly.eligible_basis_writeoffs_million +=
                        movement->amount_million;
                    writeoffs += movement->amount_million;
                    break;
                case EligibleBasisMovementKind::EligibilityRemoval:
                    monthly.eligible_basis_removals_million +=
                        movement->amount_million;
                    break;
                }
            }
            if (materially_exceeds(cash_returning_basis_reductions,
                    retained_protection_supporting_asset_basis)) {
                throw std::logic_error(
                    "cash-returning basis reduction exceeds retained supporting assets");
            }
            retained_protection_supporting_asset_basis -=
                cash_returning_basis_reductions;
            const long double protection_cash_to_reclassify =
                canonical_nonnegative(
                    protection_invested_basis -
                        retained_protection_supporting_asset_basis,
                    protection_invested_basis);
            const long double project_cash_available =
                project_principal_cash + project_nonprincipal_cash;
            if (materially_exceeds(
                    protection_cash_to_reclassify, project_cash_available)) {
                pending_failure_source_record_ids =
                    reduction_source_record_ids;
                pending_failure_source_record_ids.push_back(
                    warehouse.source_record_id);
                fail(FundingBridgeFailureKind::FundedProtectionDeficit,
                    FundingBridgeFailurePhase::EligibilityAndProtectionTest,
                    warehouse.provider_id, warehouse.id, {}, {},
                    protection_cash_to_reclassify, project_cash_available,
                    "returned or disposed supporting assets lack enough retained project cash to re-segregate funded protection");
            } else {
                long double remaining_reclassification =
                    protection_cash_to_reclassify;
                const long double from_principal = std::min(
                    remaining_reclassification, project_principal_cash);
                project_principal_cash -= from_principal;
                remaining_reclassification -= from_principal;
                const long double from_nonprincipal = std::min(
                    remaining_reclassification, project_nonprincipal_cash);
                project_nonprincipal_cash -= from_nonprincipal;
                remaining_reclassification -= from_nonprincipal;
                qualifying_protection_cash += protection_cash_to_reclassify;
                protection_invested_basis -= protection_cash_to_reclassify;
                monthly.protection_cash_reclassified_from_project_receipts_million =
                    checked_double(protection_cash_to_reclassify,
                        "project cash reclassified as funded protection");
                if (!near_input_money(remaining_reclassification, 0.0L)) {
                    throw std::logic_error(
                        "protection-cash reclassification did not exhaust");
                }
            }

            if (!failed) {
                if (materially_exceeds(writeoffs,
                        retained_protection_supporting_asset_basis)) {
                    throw std::logic_error(
                        "writeoff exceeds retained supporting assets");
                }
                retained_protection_supporting_asset_basis -= writeoffs;
                long double declared_absorption = 0.0L;
                for (const ProtectionAbsorption* absorption :
                    plan.absorptions[month]) {
                    declared_absorption += absorption->amount_million;
                    reduction_source_record_ids.push_back(
                        absorption->source_record_id);
                }
                const long double required_absorption = std::min(
                    writeoffs, protection_invested_basis);
                if (!near_input_money(
                        declared_absorption, required_absorption)) {
                    throw std::invalid_argument(
                        "same-month protection absorption must equal the first-loss amount of eligible-basis writeoffs");
                }
                if (materially_exceeds(
                        declared_absorption, funded_protection)) {
                    throw std::invalid_argument(
                        "protection absorption exceeds funded protection");
                }
                protection_invested_basis -= declared_absorption;
                funded_protection -= declared_absorption;
                monthly.funded_protection_absorbed_million =
                    checked_double(declared_absorption,
                        "funded protection absorbed");
                monthly.eligible_basis_writeoff_not_absorbed_by_protection_million =
                    checked_double(writeoffs - declared_absorption,
                        "writeoff not absorbed by funded protection");
                if (materially_exceeds(protection_invested_basis,
                        retained_protection_supporting_asset_basis)) {
                    throw std::logic_error(
                        "invested protection exceeds retained supporting assets");
                }
            }
        }

        long double required_protection = std::min(
            static_cast<long double>(prepared.first_loss_notional_million),
            eligible_basis);
        long double post_reduction_base = borrowing_base(
            warehouse, eligible_basis, funded_protection);
        if (!failed && warehouse_principal > 0.0L &&
            materially_exceeds(required_protection, funded_protection)) {
            pending_failure_source_record_ids.clear();
            for (const EligibleBasisMovement* movement :
                plan.basis_movements[month]) {
                if (movement->kind !=
                    EligibleBasisMovementKind::EligibleAddition) {
                    pending_failure_source_record_ids.push_back(
                        movement->source_record_id);
                }
            }
            for (const ProtectionAbsorption* absorption :
                plan.absorptions[month]) {
                pending_failure_source_record_ids.push_back(
                    absorption->source_record_id);
            }
            pending_failure_source_record_ids.push_back(
                warehouse.source_record_id);
            fail(FundingBridgeFailureKind::FundedProtectionDeficit,
                FundingBridgeFailurePhase::EligibilityAndProtectionTest,
                warehouse.provider_id, warehouse.id, {},
                warehouse.source_record_id,
                required_protection, funded_protection,
                "post-event funded protection is below the required amount");
        }
        if (!failed && materially_exceeds(
                warehouse_principal, post_reduction_base)) {
            pending_failure_source_record_ids.clear();
            for (const EligibleBasisMovement* movement :
                plan.basis_movements[month]) {
                if (movement->kind !=
                    EligibleBasisMovementKind::EligibleAddition) {
                    pending_failure_source_record_ids.push_back(
                        movement->source_record_id);
                }
            }
            pending_failure_source_record_ids.push_back(
                warehouse.source_record_id);
            fail(FundingBridgeFailureKind::WarehouseBorrowingBaseDeficit,
                FundingBridgeFailurePhase::EligibilityAndProtectionTest,
                warehouse.provider_id, warehouse.id, {},
                warehouse.source_record_id,
                warehouse_principal, post_reduction_base,
                "post-event warehouse principal exceeds the borrowing base");
        }

        if (!failed) {
            std::vector<double> release_values;
            std::vector<std::string> release_lineage;
            for (const ProtectionRelease* release : plan.releases[month]) {
                release_values.push_back(release->amount_million);
                release_lineage.push_back(release->source_record_id);
            }
            const long double requested_release = canonical_sum(
                std::move(release_values), "protection-release batch");
            if (requested_release > 0.0L) {
                const long double pro_forma_protection =
                    funded_protection - requested_release;
                const long double cash_available =
                    qualifying_protection_cash + project_principal_cash +
                    project_nonprincipal_cash;
                const std::string release_id =
                    plan.releases[month].size() == 1U
                    ? plan.releases[month].front()->id
                    : std::string{};
                const std::string release_source_id =
                    plan.releases[month].size() == 1U
                    ? plan.releases[month].front()->source_record_id
                    : std::string{};
                if (materially_exceeds(
                        requested_release, funded_protection) ||
                    materially_exceeds(requested_release, cash_available)) {
                    pending_failure_source_record_ids = release_lineage;
                    fail(FundingBridgeFailureKind::ProtectionReleaseShortfall,
                        FundingBridgeFailurePhase::EligibilityAndProtectionTest,
                        bridge.funded_at_close_provider_id, {}, release_id,
                        release_source_id, requested_release,
                        std::min(funded_protection, cash_available),
                        "same-month protection-release batch lacks funded protection or settled cash");
                } else if (warehouse_principal > 0.0L &&
                    materially_exceeds(required_protection,
                        pro_forma_protection)) {
                    pending_failure_source_record_ids = release_lineage;
                    pending_failure_source_record_ids.push_back(
                        warehouse.source_record_id);
                    fail(FundingBridgeFailureKind::FundedProtectionDeficit,
                        FundingBridgeFailurePhase::EligibilityAndProtectionTest,
                        warehouse.provider_id, warehouse.id, release_id,
                        release_source_id, required_protection,
                        pro_forma_protection,
                        "same-month protection-release batch would breach the funded-protection covenant");
                } else {
                    long double remaining_release = requested_release;
                    const long double from_protection_cash = std::min(
                        remaining_release, qualifying_protection_cash);
                    qualifying_protection_cash -= from_protection_cash;
                    remaining_release -= from_protection_cash;
                    const long double from_principal = std::min(
                        remaining_release, project_principal_cash);
                    project_principal_cash -= from_principal;
                    remaining_release -= from_principal;
                    const long double from_nonprincipal = std::min(
                        remaining_release, project_nonprincipal_cash);
                    project_nonprincipal_cash -= from_nonprincipal;
                    remaining_release -= from_nonprincipal;
                    const long double invested_release =
                        requested_release - from_protection_cash;
                    if (materially_exceeds(
                            invested_release, protection_invested_basis)) {
                        throw std::invalid_argument(
                            "protection release funded by asset receipts exceeds invested protection");
                    }
                    protection_invested_basis -= invested_release;
                    funded_protection -= requested_release;
                    monthly.funded_protection_released_million +=
                        checked_double(requested_release,
                            "protection release batch");
                    monthly.protection_release_million += checked_double(
                        requested_release, "protection release batch");
                    current_cash_uses += requested_release;
                    if (!near_input_money(remaining_release, 0.0L)) {
                        throw std::logic_error(
                            "protection-release allocation did not exhaust");
                    }
                }
            }
        }

        required_protection = std::min(
            static_cast<long double>(prepared.first_loss_notional_million),
            eligible_basis);
        post_reduction_base = borrowing_base(
            warehouse, eligible_basis, funded_protection);

        if (!failed && month == warehouse.legal_maturity_month) {
            const long double maturity_obligation =
                warehouse_principal + unpaid_warehouse_charges;
            const long double from_unused_warehouse = std::min(
                warehouse_principal, warehouse_proceeds_cash);
            warehouse_proceeds_cash -= from_unused_warehouse;
            warehouse_principal -= from_unused_warehouse;
            warehouse_repayment += from_unused_warehouse;
            monthly.warehouse_principal_repaid_from_unused_warehouse_cash_million =
                checked_double(from_unused_warehouse,
                    "unused warehouse cash repayment");
            const long double from_callable =
                std::min(warehouse_principal, callable_cash);
            callable_cash -= from_callable;
            warehouse_principal -= from_callable;
            warehouse_repayment += from_callable;
            monthly.warehouse_principal_repaid_from_callable_cash_million =
                checked_double(from_callable,
                    "callable-cash maturity repayment");
            const long double from_protection_cash =
                std::min(warehouse_principal, qualifying_protection_cash);
            qualifying_protection_cash -= from_protection_cash;
            protection_invested_basis += from_protection_cash;
            warehouse_principal -= from_protection_cash;
            warehouse_repayment += from_protection_cash;
            monthly.warehouse_principal_repaid_from_protection_cash_million =
                checked_double(from_protection_cash,
                    "protection-cash maturity repayment");
            if (materially_exceeds(protection_invested_basis,
                    retained_protection_supporting_asset_basis)) {
                throw std::logic_error(
                    "maturity repayment created unsupported invested protection");
            }
            if (warehouse_principal > input_money_tolerance(
                    warehouse_principal, 0.0L) ||
                unpaid_warehouse_charges > input_money_tolerance(
                    unpaid_warehouse_charges, 0.0L)) {
                monthly.warehouse_past_due_principal_million = checked_double(
                    warehouse_principal, "past-due warehouse principal");
                const long double residual_due =
                    warehouse_principal + unpaid_warehouse_charges;
                const long double maturity_cash_applied =
                    maturity_obligation - residual_due;
                if (result.first_funding_shortfall_million == 0.0) {
                    result.first_funding_shortfall_million =
                        checked_double(
                            residual_due, "first funding shortfall");
                }
                total_shortfall.add(residual_due);
                pending_failure_source_record_ids = {
                    warehouse.source_record_id};
                fail(FundingBridgeFailureKind::WarehouseMaturityUnpaid,
                    FundingBridgeFailurePhase::LegalMaturity,
                    warehouse.provider_id, warehouse.id, {},
                    warehouse.source_record_id, maturity_obligation,
                    maturity_cash_applied,
                    "contractual warehouse maturity is unpaid; exposure remains outstanding and no loss is inferred");
            }
        }

        if (!failed) {
            const long double normal_distribution =
                project_principal_cash + project_nonprincipal_cash;
            project_principal_cash = 0.0L;
            project_nonprincipal_cash = 0.0L;
            monthly.investor_distribution_million = checked_double(
                normal_distribution, "project investor distribution");
            current_cash_uses += normal_distribution;

            if (month == portfolio.horizon_months) {
                const long double terminal_return =
                    callable_cash + cost_support_cash +
                    warehouse_proceeds_cash;
                callable_cash = 0.0L;
                cost_support_cash = 0.0L;
                warehouse_proceeds_cash = 0.0L;
                monthly.investor_distribution_million += checked_double(
                    terminal_return, "terminal unencumbered cash return");
                result.ending_controlled_cash_return_million = checked_double(
                    terminal_return, "ending controlled cash return");
                current_cash_uses += terminal_return;
            }
        }

        // The expiry month is the final notice month. Uncalled capacity
        // expires at that month's close; valid notices remain pending until
        // their fixed due outcome and keep their noncash reserve cost.
        if (!failed && month == callable.contractual_expiry_month &&
            callable_available > 0.0L) {
            callable_expired += callable_available;
            callable_available = 0.0L;
        }

        monthly.callable_available_undrawn_million = checked_double(
            callable_available, "available callable commitment");
        monthly.callable_called_unsettled_million = checked_double(
            callable_called_unsettled, "called-unsettled callable");
        monthly.callable_defaulted_million = checked_double(
            callable_defaulted, "defaulted callable");
        monthly.callable_expired_uncalled_million = checked_double(
            callable_expired, "expired callable");
        monthly.warehouse_draw_called_unsettled_million = checked_double(
            warehouse_called_unsettled,
            "called-unsettled warehouse draws");
        monthly.warehouse_draw_defaulted_million = checked_double(
            warehouse_defaulted, "defaulted warehouse draws");
        monthly.warehouse_principal_repayment_million = checked_double(
            warehouse_repayment, "warehouse principal repayment");
        total_warehouse_repayments.add(warehouse_repayment);
        current_cash_uses += warehouse_repayment;
        monthly.warehouse_closing_principal_million = checked_double(
            warehouse_principal, "warehouse closing principal");
        monthly.warehouse_closing_unpaid_charges_million = checked_double(
            unpaid_warehouse_charges,
            "warehouse closing unpaid charges");
        monthly.eligible_basis_closing_million =
            checked_double(eligible_basis, "eligible basis closing");
        monthly.funded_protection_closing_million = checked_double(
            funded_protection, "funded protection closing");
        monthly.qualifying_protection_invested_basis_closing_million =
            checked_double(protection_invested_basis,
                "closing invested protection basis");
        monthly.retained_protection_supporting_asset_basis_closing_million =
            checked_double(retained_protection_supporting_asset_basis,
                "closing retained protection-supporting asset basis");
        monthly.required_funded_protection_million = checked_double(
            required_protection, "required funded protection");
        monthly.funded_protection_headroom_million = checked_double(
            funded_protection - required_protection,
            "funded-protection headroom");
        monthly.warehouse_borrowing_base_before_reductions_million =
            checked_double(pre_use_borrowing_base,
                "borrowing base before reductions");
        monthly.warehouse_borrowing_base_after_reductions_million =
            checked_double(post_reduction_base,
                "borrowing base after reductions");
        monthly.warehouse_borrowing_base_headroom_million = checked_double(
            post_reduction_base - warehouse_principal,
            "warehouse borrowing-base headroom");
        result.minimum_funded_protection_headroom_million = std::min(
            result.minimum_funded_protection_headroom_million,
            monthly.funded_protection_headroom_million);
        result.minimum_warehouse_borrowing_base_headroom_million = std::min(
            result.minimum_warehouse_borrowing_base_headroom_million,
            monthly.warehouse_borrowing_base_headroom_million);

        monthly.closing_qualifying_protection_cash_million = checked_double(
            qualifying_protection_cash, "closing protection cash");
        monthly.closing_callable_cash_million =
            checked_double(callable_cash, "closing callable cash");
        monthly.closing_controlled_cash_million = checked_double(
            qualifying_protection_cash + callable_cash,
            "closing controlled cash");
        monthly.closing_cost_support_cash_million = checked_double(
            cost_support_cash, "closing cost-support cash");
        monthly.closing_warehouse_proceeds_cash_million = checked_double(
            warehouse_proceeds_cash, "closing warehouse-proceeds cash");
        monthly.closing_takeout_cash_million =
            checked_double(takeout_cash, "closing takeout cash");
        monthly.closing_project_principal_cash_million = checked_double(
            project_principal_cash, "closing project-principal cash");
        monthly.closing_project_nonprincipal_cash_million = checked_double(
            project_nonprincipal_cash, "closing project-nonprincipal cash");
        const long double closing_total_cash =
            qualifying_protection_cash + callable_cash + cost_support_cash +
            warehouse_proceeds_cash + takeout_cash + project_principal_cash +
            project_nonprincipal_cash;
        monthly.total_actual_cash_sources_million = checked_double(
            current_cash_sources, "monthly actual cash sources");
        monthly.total_actual_cash_uses_million = checked_double(
            current_cash_uses + closing_total_cash,
            "monthly actual cash uses and closing cash");
        monthly.cash_reconciliation_error_million = checked_double(
            std::abs(current_cash_sources - current_cash_uses -
                closing_total_cash),
            "cash reconciliation error");
        const long double warehouse_left =
            static_cast<long double>(
                monthly.warehouse_opening_principal_million) +
            monthly.warehouse_advance_settled_million;
        const long double warehouse_right =
            warehouse_repayment + warehouse_principal;
        monthly.warehouse_reconciliation_error_million = checked_double(
            std::abs(warehouse_left - warehouse_right),
            "warehouse reconciliation error");
        const long double warehouse_charge_left =
            static_cast<long double>(
                monthly.warehouse_opening_unpaid_charges_million) +
            warehouse_charge_due;
        const long double warehouse_charge_right =
            static_cast<long double>(monthly.warehouse_charges_paid_million) +
            unpaid_warehouse_charges;
        monthly.warehouse_charge_reconciliation_error_million =
            checked_double(std::abs(
                warehouse_charge_left - warehouse_charge_right),
                "warehouse charge reconciliation error");
        const long double basis_reductions =
            static_cast<long double>(
                monthly.eligible_basis_principal_returns_million) +
            monthly.eligible_basis_dispositions_million +
            monthly.eligible_basis_writeoffs_million +
            monthly.eligible_basis_removals_million;
        const long double basis_left =
            static_cast<long double>(monthly.eligible_basis_opening_million) +
            monthly.eligible_basis_additions_million;
        const long double basis_right = basis_reductions + eligible_basis;
        monthly.eligible_basis_reconciliation_error_million = checked_double(
            std::abs(basis_left - basis_right),
            "eligible-basis reconciliation error");
        const long double protection_left =
            static_cast<long double>(monthly.funded_protection_opening_million) +
            monthly.funded_protection_paid_million;
        const long double protection_right =
            static_cast<long double>(
                monthly.funded_protection_released_million) +
            monthly.funded_protection_absorbed_million + funded_protection;
        const long double protection_component_error = std::abs(
            funded_protection - qualifying_protection_cash -
            protection_invested_basis);
        const long double supporting_asset_error = canonical_nonnegative(
            protection_invested_basis -
                retained_protection_supporting_asset_basis,
            std::max(protection_invested_basis,
                retained_protection_supporting_asset_basis));
        monthly.funded_protection_reconciliation_error_million = checked_double(
            std::max({std::abs(protection_left - protection_right),
                protection_component_error, supporting_asset_error}),
            "funded-protection reconciliation error");
        enforce_reconciliation(monthly.cash_reconciliation_error_million,
            monthly.total_actual_cash_sources_million,
            "funding bridge failed its full cash identity");
        enforce_reconciliation(
            monthly.warehouse_reconciliation_error_million,
            checked_double(warehouse_left, "warehouse identity scale"),
            "funding bridge failed its warehouse identity");
        enforce_reconciliation(
            monthly.warehouse_charge_reconciliation_error_million,
            checked_double(
                warehouse_charge_left, "warehouse charge identity scale"),
            "funding bridge failed its warehouse-charge identity");
        enforce_reconciliation(
            monthly.eligible_basis_reconciliation_error_million,
            checked_double(basis_left, "eligible-basis identity scale"),
            "funding bridge failed its eligible-basis identity");
        enforce_reconciliation(
            monthly.funded_protection_reconciliation_error_million,
            checked_double(protection_left, "protection identity scale"),
            "funding bridge failed its funded-protection identity");
        result.maximum_cash_reconciliation_error_million = std::max(
            result.maximum_cash_reconciliation_error_million,
            monthly.cash_reconciliation_error_million);
        result.maximum_warehouse_reconciliation_error_million = std::max(
            result.maximum_warehouse_reconciliation_error_million,
            monthly.warehouse_reconciliation_error_million);
        result.maximum_warehouse_charge_reconciliation_error_million =
            std::max(
                result.maximum_warehouse_charge_reconciliation_error_million,
                monthly.warehouse_charge_reconciliation_error_million);
        result.maximum_eligible_basis_reconciliation_error_million = std::max(
            result.maximum_eligible_basis_reconciliation_error_million,
            monthly.eligible_basis_reconciliation_error_million);
        result.maximum_funded_protection_reconciliation_error_million =
            std::max(
                result.maximum_funded_protection_reconciliation_error_million,
                monthly.funded_protection_reconciliation_error_million);

        const long double permanent_capital_contribution =
            static_cast<long double>(monthly.funded_at_close_receipt_million) +
            monthly.capital_call_settled_million +
            monthly.cost_support_receipts_million +
            monthly.protection_replenishment_receipts_million;
        if (!failed) {
            investor_cash_flows[month] = checked_double(
                static_cast<long double>(
                    monthly.investor_distribution_million) +
                    monthly.protection_release_million -
                    permanent_capital_contribution,
                "aggregate permanent-capital net cash flow");
            monthly.investor_net_cash_flow_million =
                investor_cash_flows[month];
        }
        if (warehouse_principal + unpaid_warehouse_charges >
            input_money_tolerance(
                warehouse_principal + unpaid_warehouse_charges, 0.0L)) {
            ++result.warehouse_exposure_months;
        }
        result.peak_warehouse_funded_ead_million = std::max(
            result.peak_warehouse_funded_ead_million,
            checked_double(warehouse_principal + unpaid_warehouse_charges,
                "warehouse funded EAD"));
        result.months.push_back(std::move(monthly));
    }

    result.feasible = !failed;
    result.total_call_requested_million = checked_double(
        total_call_requests.value(), "total capital-call requests");
    result.total_actual_call_receipts_million = checked_double(
        total_call_receipts.value(), "total capital-call receipts");
    result.ending_available_callable_commitment_million = checked_double(
        callable_available, "ending available callable commitment");
    result.ending_called_unsettled_callable_million = checked_double(
        callable_called_unsettled, "ending called-unsettled callable");
    result.callable_defaulted_million = checked_double(
        callable_defaulted, "called defaulted");
    result.expired_uncalled_callable_million = checked_double(
        callable_expired, "expired uncalled commitment");
    result.total_warehouse_draw_requested_million = checked_double(
        total_warehouse_requests.value(), "total warehouse requests");
    result.total_warehouse_advances_million = checked_double(
        total_warehouse_advances.value(), "total warehouse advances");
    result.warehouse_draw_defaulted_million = checked_double(
        warehouse_defaulted, "warehouse draw defaulted");
    result.ending_called_unsettled_warehouse_million = checked_double(
        warehouse_called_unsettled,
        "ending called-unsettled warehouse draws");
    result.total_warehouse_principal_repayments_million = checked_double(
        total_warehouse_repayments.value(), "total warehouse repayments");
    result.total_warehouse_interest_million = checked_double(
        total_interest.value(), "total warehouse interest");
    result.total_warehouse_fees_million = checked_double(
        total_warehouse_fees.value(), "total warehouse fees");
    result.total_callable_commitment_fees_million = checked_double(
        total_callable_commitment_fees.value(),
        "total callable commitment fees");
    result.ending_warehouse_principal_million = checked_double(
        warehouse_principal, "ending warehouse principal");
    result.ending_warehouse_unpaid_charges_million = checked_double(
        unpaid_warehouse_charges, "ending unpaid warehouse charges");
    result.ending_warehouse_funded_ead_million = checked_double(
        warehouse_principal + unpaid_warehouse_charges,
        "ending warehouse funded EAD");
    result.total_cost_support_receipts_million = checked_double(
        total_cost_support.value(), "total cost-support receipts");
    result.total_protection_replenishment_receipts_million = checked_double(
        total_protection_replenishment.value(),
        "total protection-replenishment receipts");
    result.total_settled_takeout_receipts_million = checked_double(
        total_takeout.value(), "total takeout receipts");
    result.total_acquisition_and_primary_funding_uses_million = checked_double(
        total_acquisition_uses.value(), "total acquisition uses");
    result.total_buyer_direct_cost_uses_million = checked_double(
        total_buyer_costs.value(), "total buyer costs");
    result.total_pool_cost_uses_million = checked_double(
        total_pool_costs.value(), "total pool costs");
    result.total_project_receipts_million = checked_double(
        total_project_receipts.value(), "total actual project receipts");
    result.total_funding_shortfall_million = checked_double(
        total_shortfall.value(), "total funding shortfall");
    result.ending_eligible_basis_million = checked_double(
        eligible_basis, "ending eligible basis");
    result.ending_funded_protection_million = checked_double(
        funded_protection, "ending funded protection");
    result.callable_liquidity_opportunity_cost_pv_million = checked_double(
        liquidity_opportunity_cost_pv.value(),
        "callable liquidity opportunity-cost PV");
    if (!std::isfinite(result.minimum_funded_protection_headroom_million)) {
        result.minimum_funded_protection_headroom_million = 0.0;
    }
    if (!std::isfinite(
            result.minimum_warehouse_borrowing_base_headroom_million)) {
        result.minimum_warehouse_borrowing_base_headroom_million = 0.0;
    }

    result.providers.reserve(bridge.providers.size());
    const std::size_t effective_measurement_month =
        result.first_infeasible_month.value_or(portfolio.horizon_months);
    const bool warehouse_notice_window_open_at_measurement =
        result.first_infeasible_month.has_value()
        ? effective_measurement_month <= warehouse.availability_end_month
        : effective_measurement_month < warehouse.availability_end_month;
    for (const FundingProvider& provider : bridge.providers) {
        const ProviderRuntime& runtime = provider_runtime.at(provider.id);
        FundingProviderScenarioResult provider_result;
        provider_result.provider_id = provider.id;
        provider_result.economic_group_id = provider.economic_group_id;
        provider_result.declared_capacity_million =
            provider.declared_capacity_million;
        provider_result.settled_initial_and_supplemental_capital_million =
            checked_double(runtime.settled_initial_and_supplemental,
                "settled initial and supplemental capital");
        provider_result.settled_callable_capital_million = checked_double(
            runtime.settled_callable, "settled callable capital");
        provider_result.settled_takeout_cash_million = checked_double(
            runtime.settled_takeout, "settled takeout cash");
        provider_result.cumulative_settled_warehouse_advances_million =
            checked_double(runtime.cumulative_settled_warehouse_advances,
                "cumulative settled warehouse advances");
        provider_result.callable_defaulted_million = checked_double(
            runtime.callable_defaulted, "provider callable default");
        provider_result.warehouse_draw_defaulted_million = checked_double(
            runtime.warehouse_defaulted, "provider warehouse default");
        provider_result.provider_nonperformance_observed =
            runtime.nonperformance;
        if (provider.id == callable.provider_id) {
            provider_result.ending_available_callable_commitment_million =
                result.ending_available_callable_commitment_million;
            provider_result.ending_called_unsettled_callable_million =
                result.ending_called_unsettled_callable_million;
            provider_result.expired_uncalled_callable_million =
                result.expired_uncalled_callable_million;
            provider_result.callable_default_share_of_request =
                result.total_call_requested_million > 0.0
                ? result.callable_defaulted_million /
                    result.total_call_requested_million
                : 0.0;
        }
        if (provider.id == warehouse.provider_id) {
            provider_result.peak_warehouse_funded_ead_million =
                result.peak_warehouse_funded_ead_million;
            provider_result.ending_warehouse_funded_ead_million =
                result.ending_warehouse_funded_ead_million;
            provider_result.ending_called_unsettled_warehouse_million =
                result.ending_called_unsettled_warehouse_million;
            provider_result.ending_warehouse_contingent_commitment_million =
                warehouse_notice_window_open_at_measurement
                ? canonical_nonnegative(
                    static_cast<long double>(
                        warehouse.committed_limit_million) -
                        warehouse_principal - warehouse_defaulted,
                    warehouse.committed_limit_million)
                : result.ending_called_unsettled_warehouse_million;
            provider_result.warehouse_default_share_of_request =
                result.total_warehouse_draw_requested_million > 0.0
                ? result.warehouse_draw_defaulted_million /
                    result.total_warehouse_draw_requested_million
                : 0.0;
        }
        const long double used =
            static_cast<long double>(
                provider_result.settled_initial_and_supplemental_capital_million) +
            provider_result.settled_callable_capital_million +
            provider_result.settled_takeout_cash_million +
            provider_result.ending_available_callable_commitment_million +
            provider_result.ending_called_unsettled_callable_million +
            provider_result.ending_warehouse_funded_ead_million +
            provider_result.ending_warehouse_contingent_commitment_million;
        provider_result.capacity_used_million =
            checked_double(used, "provider capacity used");
        provider_result.capacity_headroom_million = checked_double(
            static_cast<long double>(provider.declared_capacity_million) - used,
            "provider capacity headroom");
        result.providers.push_back(std::move(provider_result));
    }
    std::sort(result.providers.begin(), result.providers.end(),
        [](const auto& left, const auto& right) {
            return left.provider_id < right.provider_id;
    });

    std::unordered_map<std::string, FundingEconomicGroupScenarioResult> groups;
    long double total_gross_settled_funding = 0.0L;
    long double total_contingent_dependency = 0.0L;
    for (const FundingProviderScenarioResult& provider : result.providers) {
        FundingEconomicGroupScenarioResult& group =
            groups[provider.economic_group_id];
        group.economic_group_id = provider.economic_group_id;
        const long double gross_settled =
            static_cast<long double>(
                provider.settled_initial_and_supplemental_capital_million) +
            provider.settled_callable_capital_million +
            provider.settled_takeout_cash_million +
            provider.cumulative_settled_warehouse_advances_million;
        const long double contingent =
            static_cast<long double>(
                provider.ending_available_callable_commitment_million) +
            provider.ending_called_unsettled_callable_million +
            provider.ending_warehouse_contingent_commitment_million;
        group.cumulative_gross_settled_funding_million += checked_double(
            gross_settled,
            "economic-group cumulative gross settled funding");
        group.ending_contingent_funding_dependency_million += checked_double(
            contingent,
            "economic-group ending contingent funding dependency");
        total_gross_settled_funding += gross_settled;
        total_contingent_dependency += contingent;
    }
    for (auto& [group_id, group] : groups) {
        static_cast<void>(group_id);
        group.cumulative_gross_settled_funding_share =
            total_gross_settled_funding > 0.0L
            ? checked_double(group.cumulative_gross_settled_funding_million /
                    total_gross_settled_funding,
                "economic-group cumulative gross settled funding share")
            : 0.0;
        group.ending_contingent_funding_dependency_share =
            total_contingent_dependency > 0.0L
            ? checked_double(
                group.ending_contingent_funding_dependency_million /
                    total_contingent_dependency,
                "economic-group ending contingent dependency share")
            : 0.0;
        result.cumulative_gross_settled_funding_source_hhi +=
            group.cumulative_gross_settled_funding_share *
            group.cumulative_gross_settled_funding_share;
        result.ending_economic_group_contingent_funding_dependency_hhi +=
            group.ending_contingent_funding_dependency_share *
            group.ending_contingent_funding_dependency_share;
        result.economic_groups.push_back(group);
    }
    std::sort(result.economic_groups.begin(), result.economic_groups.end(),
        [](const auto& left, const auto& right) {
            return left.economic_group_id < right.economic_group_id;
        });

    if (result.feasible &&
        (!near_input_money(callable_called_unsettled, 0.0L) ||
            !near_input_money(warehouse_called_unsettled, 0.0L))) {
        throw std::logic_error(
            "feasible funding path ended with an unresolved provider settlement");
    }
    if (result.feasible &&
        !near_input_money(
            warehouse_principal + unpaid_warehouse_charges, 0.0L)) {
        throw std::logic_error(
            "feasible funding path ended with past-due warehouse exposure");
    }
    const bool callable_fee_recipient_boundary_is_complete =
        near_input_money(total_callable_commitment_fees.value(), 0.0L);
    if (result.feasible && !takeout_used &&
        callable_fee_recipient_boundary_is_complete) {
        CompensatedSum npv;
        for (std::size_t month = 0U; month < month_count; ++month) {
            npv.add(static_cast<long double>(investor_cash_flows[month]) /
                discount_factor(portfolio.annual_physical_hurdle_rate, month));
        }
        result.investor_cash_npv_million = checked_double(
            npv.value(), "aggregate permanent-capital cash NPV");
        result.fully_prefunded_baseline_npv_million =
            stack_result.fully_funded_stack_npv_at_pool_hurdle_million;
        result.cash_npv_change_vs_fully_prefunded_million = checked_double(
            static_cast<long double>(*result.investor_cash_npv_million) -
                *result.fully_prefunded_baseline_npv_million,
            "cash NPV change versus fully prefunded baseline");
        result.economic_npv_change_after_callable_liquidity_cost_million =
            checked_double(
                static_cast<long double>(
                    *result.cash_npv_change_vs_fully_prefunded_million) -
                    result.callable_liquidity_opportunity_cost_pv_million,
                "economic NPV change after callable liquidity cost");
        result.fully_prefunded_prefunding_drag_million =
            stack_result.prefunding_drag_npv_million;
        result.preserved_project_receipts_million =
            portfolio_result.total_receipts_million;
        result.preserved_project_principal_loss_million =
            portfolio_result.principal_loss_million;
        result.project_cash_is_preserved = near_input_money(
            result.total_project_receipts_million,
            portfolio_result.total_receipts_million);
        result.gross_project_principal_loss_is_preserved = near_input_money(
            portfolio_result.principal_loss_million,
            stack_result.contractual_asset_principal_loss_million);
        if (!*result.project_cash_is_preserved ||
            !*result.gross_project_principal_loss_is_preserved) {
            throw std::logic_error(
                "feasible funding bridge changed frozen project cash or loss");
        }
    }
    return result;
}

} // namespace

std::string_view to_string(FundingSettlementStatus status) noexcept {
    switch (status) {
    case FundingSettlementStatus::SettledInFull:
        return "settled-in-full";
    case FundingSettlementStatus::FinalPartialSettlement:
        return "final-partial-settlement";
    case FundingSettlementStatus::Failed:
        return "failed";
    }
    return "unknown";
}

std::string_view to_string(SupplementalFundingPurpose purpose) noexcept {
    switch (purpose) {
    case SupplementalFundingPurpose::CostSupport:
        return "cost-support";
    case SupplementalFundingPurpose::ProtectionReplenishment:
        return "protection-replenishment";
    case SupplementalFundingPurpose::SettledTakeout:
        return "settled-takeout";
    }
    return "unknown";
}

std::string_view to_string(EligibleBasisMovementKind kind) noexcept {
    switch (kind) {
    case EligibleBasisMovementKind::EligibleAddition:
        return "eligible-addition";
    case EligibleBasisMovementKind::PrincipalBasisReturn:
        return "principal-basis-return";
    case EligibleBasisMovementKind::Disposition:
        return "disposition";
    case EligibleBasisMovementKind::Writeoff:
        return "writeoff";
    case EligibleBasisMovementKind::EligibilityRemoval:
        return "eligibility-removal";
    }
    return "unknown";
}

std::string_view to_string(FundingBridgeFailureKind kind) noexcept {
    switch (kind) {
    case FundingBridgeFailureKind::None:
        return "none";
    case FundingBridgeFailureKind::CallableFacilityUnavailable:
        return "callable-facility-unavailable";
    case FundingBridgeFailureKind::WarehouseFacilityUnavailable:
        return "warehouse-facility-unavailable";
    case FundingBridgeFailureKind::SimultaneousFacilityRequestFailure:
        return "simultaneous-facility-request-failure";
    case FundingBridgeFailureKind::ProviderCapacityExceeded:
        return "provider-capacity-exceeded";
    case FundingBridgeFailureKind::WarehouseLimitExceeded:
        return "warehouse-limit-exceeded";
    case FundingBridgeFailureKind::WarehouseBorrowingBaseDeficit:
        return "warehouse-borrowing-base-deficit";
    case FundingBridgeFailureKind::FundedProtectionDeficit:
        return "funded-protection-deficit";
    case FundingBridgeFailureKind::NonAssetCostShortfall:
        return "nonasset-cost-shortfall";
    case FundingBridgeFailureKind::FundingUseShortfall:
        return "funding-use-shortfall";
    case FundingBridgeFailureKind::ProtectionReleaseShortfall:
        return "protection-release-shortfall";
    case FundingBridgeFailureKind::WarehouseMaturityUnpaid:
        return "warehouse-maturity-unpaid";
    }
    return "unknown";
}

std::string_view to_string(FundingBridgeFailurePhase phase) noexcept {
    switch (phase) {
    case FundingBridgeFailurePhase::None:
        return "none";
    case FundingBridgeFailurePhase::ContractAndProviderTest:
        return "contract-and-provider-test";
    case FundingBridgeFailurePhase::NonAssetCosts:
        return "nonasset-costs";
    case FundingBridgeFailurePhase::AssetFundingUse:
        return "asset-funding-use";
    case FundingBridgeFailurePhase::EligibilityAndProtectionTest:
        return "eligibility-and-protection-test";
    case FundingBridgeFailurePhase::LegalMaturity:
        return "legal-maturity";
    }
    return "unknown";
}

void validate_funding_bridge_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack, const FundingBridgeConfig& bridge) {
    static_cast<void>(
        prepare_bridge(portfolio, ambiguity, participation, stack, bridge));
}

FundingBridgeSummary evaluate_funding_bridge(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack, const FundingBridgeConfig& bridge) {
    const PreparedBridge prepared =
        prepare_bridge(portfolio, ambiguity, participation, stack, bridge);
    FundingBridgeSummary summary;
    summary.model_version = std::string(kFundingBridgeModelVersion);
    summary.scenario_label = bridge.scenario_label;
    summary.source_note = bridge.source_note;
    summary.synthetic_inputs = bridge.synthetic_inputs;
    summary.funded_protection_target_million =
        prepared.first_loss_notional_million;
    summary.callable_facility = bridge.callable_facility;
    summary.warehouse_facility = bridge.warehouse_facility;
    summary.scenario_probability_bounds =
        prepared.stack_summary.scenario_probability_bounds;
    summary.monthly_path_nonanticipativity_validated = true;
    summary.model_limitation =
        "Synthetic-only exact declared-path funding feasibility under a "
        "start-of-month convention: all prior-month path facts are treated "
        "as observed and current-month facts are not. Cost basis and the "
        "completeness of declared reductions are supplied, not appraised or "
        "independently evidenced; positive callable commitment fees suppress NPV until "
        "their recipient boundary is modeled; takeout liabilities remain "
        "outside the bridge; no warehouse loss or recovery, fair value, issue "
        "price, probability calibration, legal enforceability, or tranche "
        "allocation is inferred.";

    summary.scenarios.reserve(prepared.performances.size());
    bool all_cash_npvs_available = true;
    bool all_economic_npvs_available = true;
    for (const FundingBridgeScenarioPerformance* performance :
        prepared.performances) {
        const JointScenarioResult& portfolio_result = find_portfolio_scenario(
            prepared.portfolio_summary, performance->scenario_id);
        const CapitalStackScenarioResult& stack_result = find_stack_scenario(
            prepared.stack_summary, performance->scenario_id);
        FundingBridgeScenarioResult scenario = evaluate_scenario(prepared,
            bridge, *performance, portfolio_result, stack_result);
        all_cash_npvs_available = all_cash_npvs_available &&
            scenario.investor_cash_npv_million.has_value();
        all_economic_npvs_available = all_economic_npvs_available &&
            scenario.economic_npv_change_after_callable_liquidity_cost_million
                .has_value();
        summary.maximum_cash_reconciliation_error_million = std::max(
            summary.maximum_cash_reconciliation_error_million,
            scenario.maximum_cash_reconciliation_error_million);
        summary.maximum_warehouse_reconciliation_error_million = std::max(
            summary.maximum_warehouse_reconciliation_error_million,
            scenario.maximum_warehouse_reconciliation_error_million);
        summary.maximum_warehouse_charge_reconciliation_error_million =
            std::max(
                summary.maximum_warehouse_charge_reconciliation_error_million,
                scenario.maximum_warehouse_charge_reconciliation_error_million);
        summary.maximum_eligible_basis_reconciliation_error_million = std::max(
            summary.maximum_eligible_basis_reconciliation_error_million,
            scenario.maximum_eligible_basis_reconciliation_error_million);
        summary.maximum_funded_protection_reconciliation_error_million =
            std::max(
                summary.maximum_funded_protection_reconciliation_error_million,
                scenario.maximum_funded_protection_reconciliation_error_million);
        summary.scenarios.push_back(std::move(scenario));
    }

    const PortfolioAmbiguityProjector projector(
        prepared.participated_portfolio, ambiguity);
    std::vector<AmbiguityScenarioMetricValue> failures;
    std::vector<AmbiguityScenarioMetricValue> shortfalls;
    std::vector<AmbiguityScenarioMetricValue> ending_ead;
    std::vector<AmbiguityScenarioMetricValue> peak_ead;
    std::vector<AmbiguityScenarioMetricValue> callable_defaults;
    for (const FundingBridgeScenarioResult& scenario : summary.scenarios) {
        failures.push_back(
            {scenario.scenario_id, scenario.feasible ? 0.0 : 1.0});
        shortfalls.push_back(
            {scenario.scenario_id, scenario.total_funding_shortfall_million});
        ending_ead.push_back(
            {scenario.scenario_id,
                scenario.ending_warehouse_funded_ead_million});
        peak_ead.push_back(
            {scenario.scenario_id,
                scenario.peak_warehouse_funded_ead_million});
        callable_defaults.push_back(
            {scenario.scenario_id, scenario.callable_defaulted_million});
    }
    summary.funding_failure_probability =
        projector.project_expectation(failures).expectation;
    summary.expected_funding_shortfall_million =
        projector.project_expectation(shortfalls).expectation;
    summary.funding_shortfall_expected_shortfall_95_million =
        projector.project_upper_expected_shortfall(shortfalls, 0.05)
            .upper_expected_shortfall;
    summary.funding_shortfall_expected_shortfall_99_million =
        projector.project_upper_expected_shortfall(shortfalls, 0.01)
            .upper_expected_shortfall;
    summary.expected_ending_warehouse_funded_ead_million =
        projector.project_expectation(ending_ead).expectation;
    summary.ending_warehouse_funded_ead_expected_shortfall_95_million =
        projector.project_upper_expected_shortfall(ending_ead, 0.05)
            .upper_expected_shortfall;
    summary.ending_warehouse_funded_ead_expected_shortfall_99_million =
        projector.project_upper_expected_shortfall(ending_ead, 0.01)
            .upper_expected_shortfall;
    summary.expected_peak_warehouse_funded_ead_million =
        projector.project_expectation(peak_ead).expectation;
    summary.expected_callable_provider_default_million =
        projector.project_expectation(callable_defaults).expectation;

    if (all_cash_npvs_available) {
        std::vector<AmbiguityScenarioMetricValue> npvs;
        std::vector<AmbiguityScenarioMetricValue> changes;
        for (const FundingBridgeScenarioResult& scenario : summary.scenarios) {
            npvs.push_back(
                {scenario.scenario_id, *scenario.investor_cash_npv_million});
            changes.push_back({scenario.scenario_id,
                *scenario.cash_npv_change_vs_fully_prefunded_million});
        }
        summary.expected_investor_cash_npv_million =
            projector.project_expectation(npvs).expectation;
        summary.expected_cash_npv_change_vs_fully_prefunded_million =
            projector.project_expectation(changes).expectation;
    }
    if (all_economic_npvs_available) {
        std::vector<AmbiguityScenarioMetricValue> economic_changes;
        for (const FundingBridgeScenarioResult& scenario : summary.scenarios) {
            economic_changes.push_back({scenario.scenario_id,
                *scenario
                     .economic_npv_change_after_callable_liquidity_cost_million});
        }
        summary
            .expected_economic_npv_change_after_callable_liquidity_cost_million =
            projector.project_expectation(economic_changes).expectation;
    }
    return summary;
}

} // namespace naturalehia::cellular_finance
