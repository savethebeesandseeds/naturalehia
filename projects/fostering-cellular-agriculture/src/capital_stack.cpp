// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumTranches = 128U;
constexpr std::size_t kMaximumScenarioTrancheMonths = 2'000'000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMaximumAnnualHurdle = 10.0;
constexpr double kMinimumTrancheNotionalMillion = 1.0e-6;
constexpr long double kInputMoneyAbsoluteTolerance = 1.0e-10L;
constexpr double kReconciliationAbsoluteTolerance = 1.0e-9;
constexpr double kReconciliationRelativeTolerance =
    16.0 * std::numeric_limits<double>::epsilon();
constexpr double kWalMinimumExpectedPrincipalCashMillion = 1.0e-10;

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

[[nodiscard]] double checked_double(
    long double value, std::string_view description) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            std::string(description) + " is outside the finite double range");
    }
    return converted;
}

[[nodiscard]] double absolute_difference(double left, double right) noexcept {
    return std::abs(left - right);
}

[[nodiscard]] double reconciliation_tolerance(double scale) noexcept {
    return kReconciliationAbsoluteTolerance +
        kReconciliationRelativeTolerance * std::max(1.0, std::abs(scale));
}

[[nodiscard]] double canonical_nonnegative_residual(
    double value, double scale) noexcept {
    const double nonnegative = std::max(0.0, value);
    return nonnegative <= reconciliation_tolerance(scale) ? 0.0 : nonnegative;
}

[[nodiscard]] bool is_material_positive(double value, double scale) noexcept {
    return value > reconciliation_tolerance(scale);
}

[[nodiscard]] bool is_material_negative(double value, double scale) noexcept {
    return value < -reconciliation_tolerance(scale);
}

void enforce_reconciliation(
    double error, double scale, std::string_view description) {
    const double tolerance = reconciliation_tolerance(scale);
    if (!std::isfinite(error) || error > tolerance) {
        throw std::logic_error(std::string(description));
    }
}

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

[[nodiscard]] bool near_input_money(
    long double left, long double right) noexcept {
    const long double scale =
        std::max({1.0L, std::abs(left), std::abs(right)});
    const long double tolerance = kInputMoneyAbsoluteTolerance + 8.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        scale;
    return std::abs(left - right) <= tolerance;
}

[[nodiscard]] bool exceeds_with_input_tolerance(
    long double value, long double limit) noexcept {
    const long double scale =
        std::max({1.0L, std::abs(value), std::abs(limit)});
    const long double tolerance = kInputMoneyAbsoluteTolerance + 8.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        scale;
    return value > limit + tolerance;
}

[[nodiscard]] double aggregate_commitment(const PortfolioConfig& portfolio) {
    CompensatedSum sum;
    for (const PortfolioProject& project : portfolio.projects) {
        sum.add(static_cast<long double>(project.commitment_million));
    }
    return checked_double(sum.value(), "capital-stack aggregate commitment");
}

[[nodiscard]] bool is_v02(const CapitalStackConfig& stack) noexcept {
    return stack.model_version == kCapitalStackModelVersion;
}

[[nodiscard]] long double path_acquisition_and_primary_funding(
    const ProjectJointPath& path,
    PrincipalAccountingMode accounting_mode) noexcept {
    CompensatedSum sum;
    if (accounting_mode == PrincipalAccountingMode::DrawEqualsPrincipalLegacy) {
        for (const MonthlyAmount& draw : path.capital_draws) {
            sum.add(static_cast<long double>(draw.amount_million));
        }
        return sum.value();
    }
    for (const InvestorOutlay& outlay : path.investor_outlays) {
        if (outlay.purpose == InvestorOutlayPurpose::PrimaryProjectFunding ||
            outlay.purpose == InvestorOutlayPurpose::ClaimPurchasePrice) {
            sum.add(static_cast<long double>(outlay.amount_million));
        }
    }
    return sum.value();
}

struct CapitalStackFundingBasis {
    double project_outlay_limit_million{0.0};
    double contractual_asset_principal_limit_million{0.0};
    double funded_reserve_limit_million{0.0};
    bool uses_explicit_asset_liability_accounting{false};
};

[[nodiscard]] CapitalStackFundingBasis capital_stack_funding_basis(
    const PortfolioConfig& portfolio, const CapitalStackConfig& stack) {
    CapitalStackFundingBasis result;
    result.project_outlay_limit_million = aggregate_commitment(portfolio);
    result.contractual_asset_principal_limit_million =
        portfolio_aggregate_reference_principal(portfolio);

    if (!is_v02(stack)) {
        result.funded_reserve_limit_million =
            result.project_outlay_limit_million;
        return result;
    }

    std::unordered_map<std::string, std::size_t> project_indices;
    project_indices.reserve(portfolio.projects.size());
    std::vector<long double> project_funding_limits(
        portfolio.projects.size(), 0.0L);
    for (std::size_t index = 0U; index < portfolio.projects.size(); ++index) {
        const PortfolioProject& project = portfolio.projects[index];
        project_indices.emplace(project.id, index);
        if (project.principal_accounting_mode ==
            PrincipalAccountingMode::DrawEqualsPrincipalLegacy) {
            project_funding_limits[index] =
                static_cast<long double>(project.commitment_million);
        } else {
            result.uses_explicit_asset_liability_accounting = true;
        }
    }
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        for (const ProjectJointPath& path : scenario.project_paths) {
            const auto found = project_indices.find(path.project_id);
            if (found == project_indices.end()) {
                throw std::logic_error(
                    "capital-stack funding basis lost a project");
            }
            const PortfolioProject& project = portfolio.projects[found->second];
            if (project.principal_accounting_mode ==
                PrincipalAccountingMode::ExplicitContractualLedger) {
                project_funding_limits[found->second] = std::max(
                    project_funding_limits[found->second],
                    path_acquisition_and_primary_funding(path,
                        project.principal_accounting_mode));
            }
        }
    }
    CompensatedSum funded_reserve;
    for (const long double limit : project_funding_limits) {
        funded_reserve.add(limit);
    }
    result.funded_reserve_limit_million = checked_double(
        funded_reserve.value(), "capital-stack acquisition funding limit");
    return result;
}

[[nodiscard]] double tranche_loss(
    double pool_amount, const CapitalStackTrancheConfig& tranche) noexcept {
    return std::min(std::max(pool_amount - tranche.attachment_million, 0.0),
        tranche.detachment_million - tranche.attachment_million);
}

[[nodiscard]] double discount_factor(double annual_rate, std::size_t month) {
    const double factor = std::pow(
        1.0 + annual_rate, static_cast<double>(month) / 12.0);
    if (!std::isfinite(factor) || factor <= 0.0) {
        throw std::overflow_error(
            "capital-stack discount factor is outside the supported range");
    }
    return factor;
}

template <typename Selector>
[[nodiscard]] AmbiguityMetricProjection project_metric(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<CapitalStackScenarioResult>& scenarios,
    Selector selector) {
    std::vector<AmbiguityScenarioMetricValue> values;
    values.reserve(scenarios.size());
    for (const CapitalStackScenarioResult& scenario : scenarios) {
        const double value = selector(scenario);
        if (!std::isfinite(value)) {
            throw std::logic_error(
                "capital-stack projection contains a non-finite value");
        }
        values.push_back(AmbiguityScenarioMetricValue{scenario.scenario_id, value});
    }
    return projector.project_expectation(values);
}

template <typename Selector>
[[nodiscard]] AmbiguityUpperExpectedShortfallProjection project_es(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<CapitalStackScenarioResult>& scenarios,
    Selector selector, double tail_probability) {
    std::vector<AmbiguityScenarioMetricValue> values;
    values.reserve(scenarios.size());
    for (const CapitalStackScenarioResult& scenario : scenarios) {
        const double value = selector(scenario);
        if (!std::isfinite(value) || value < 0.0) {
            throw std::logic_error(
                "capital-stack tail projection requires finite non-negative values");
        }
        values.push_back(AmbiguityScenarioMetricValue{scenario.scenario_id, value});
    }
    return projector.project_upper_expected_shortfall(values, tail_probability);
}

void update_projection_error(double candidate, CapitalStackSummary& summary) {
    summary.maximum_endpoint_probability_error =
        std::max(summary.maximum_endpoint_probability_error, candidate);
}

[[nodiscard]] const JointScenario& find_joint_scenario(
    const std::unordered_map<std::string, const JointScenario*>& scenarios,
    const std::string& id) {
    const auto iterator = scenarios.find(id);
    if (iterator == scenarios.end()) {
        throw std::logic_error(
            "capital-stack result cannot find its source joint scenario");
    }
    return *iterator->second;
}

void add_monthly_amounts(const std::vector<MonthlyAmount>& amounts,
    std::vector<CompensatedSum>& monthly) {
    for (const MonthlyAmount& amount : amounts) {
        monthly[amount.month].add(
            static_cast<long double>(amount.amount_million));
    }
}

struct MonthlyPoolCash {
    std::vector<CompensatedSum> acquisition_and_primary_funding_uses{};
    std::vector<CompensatedSum> claim_purchase_price_uses{};
    std::vector<CompensatedSum> primary_project_funding_uses{};
    std::vector<CompensatedSum> buyer_direct_costs{};
    std::vector<CompensatedSum> pool_costs{};
    std::vector<CompensatedSum> principal{};
    std::vector<CompensatedSum> nonprincipal{};
};

[[nodiscard]] MonthlyPoolCash build_monthly_pool_cash(
    const PortfolioConfig& portfolio, const JointScenario& scenario,
    std::size_t month_count, const CapitalStackConfig& stack) {
    MonthlyPoolCash cash;
    cash.acquisition_and_primary_funding_uses.resize(month_count);
    cash.claim_purchase_price_uses.resize(month_count);
    cash.primary_project_funding_uses.resize(month_count);
    cash.buyer_direct_costs.resize(month_count);
    cash.pool_costs.resize(month_count);
    cash.principal.resize(month_count);
    cash.nonprincipal.resize(month_count);
    add_monthly_amounts(scenario.pool_costs, cash.pool_costs);

    std::unordered_map<std::string, PrincipalAccountingMode> accounting_modes;
    accounting_modes.reserve(portfolio.projects.size());
    for (const PortfolioProject& project : portfolio.projects) {
        accounting_modes.emplace(project.id, project.principal_accounting_mode);
    }
    for (const ProjectJointPath& path : scenario.project_paths) {
        const auto found = accounting_modes.find(path.project_id);
        if (found == accounting_modes.end()) {
            throw std::logic_error(
                "capital-stack monthly cash lost a configured project");
        }
        if (!is_v02(stack) || found->second ==
                PrincipalAccountingMode::DrawEqualsPrincipalLegacy) {
            add_monthly_amounts(path.capital_draws,
                cash.acquisition_and_primary_funding_uses);
            add_monthly_amounts(path.capital_draws,
                cash.primary_project_funding_uses);
        } else {
            for (const InvestorOutlay& outlay : path.investor_outlays) {
                std::vector<CompensatedSum>* destination = nullptr;
                switch (outlay.purpose) {
                case InvestorOutlayPurpose::PrimaryProjectFunding:
                    destination = &cash.primary_project_funding_uses;
                    break;
                case InvestorOutlayPurpose::ClaimPurchasePrice:
                    destination = &cash.claim_purchase_price_uses;
                    break;
                case InvestorOutlayPurpose::BuyerDirectCost:
                    destination = &cash.buyer_direct_costs;
                    break;
                }
                (*destination)[outlay.month].add(
                    static_cast<long double>(outlay.amount_million));
                if (outlay.purpose != InvestorOutlayPurpose::BuyerDirectCost) {
                    cash.acquisition_and_primary_funding_uses[outlay.month].add(
                        static_cast<long double>(outlay.amount_million));
                }
            }
        }
        for (const InvestorReceipt& receipt : path.investor_receipts) {
            cash.principal[receipt.month].add(
                static_cast<long double>(receipt.principal_component_million));
            const long double nonprincipal =
                static_cast<long double>(receipt.amount_million) -
                static_cast<long double>(receipt.principal_component_million);
            if (nonprincipal < 0.0L) {
                throw std::logic_error(
                    "capital-stack receipt has negative non-principal cash");
            }
            cash.nonprincipal[receipt.month].add(nonprincipal);
        }
    }
    return cash;
}

struct RunningTranche {
    CapitalStackTrancheScenarioResult result{};
    CompensatedSum pro_rata_buyer_direct_cost_calls{};
    CompensatedSum pro_rata_pool_cost_calls{};
    CompensatedSum underlying_principal_cash{};
    CompensatedSum reserve_principal_cash{};
    CompensatedSum principal_cash{};
    CompensatedSum contractual_principal_surplus_cash{};
    CompensatedSum reserve_surplus_cash{};
    CompensatedSum underlying_nonprincipal_cash{};
    CompensatedSum nonprincipal_cash{};
    CompensatedSum npv_at_tranche_hurdle{};
    CompensatedSum principal_month_numerator{};
};

[[nodiscard]] long double allocate_principal(long double available,
    const CapitalStackConfig& config, std::vector<RunningTranche>& tranches,
    std::vector<long double>& allocation) {
    CompensatedSum paid_total;
    for (std::size_t reverse = config.tranches.size(); reverse > 0U; --reverse) {
        const std::size_t index = reverse - 1U;
        const double notional = config.tranches[index].detachment_million -
            config.tranches[index].attachment_million;
        const long double capacity = std::max(0.0L,
            static_cast<long double>(notional) -
                tranches[index].principal_cash.value());
        const long double remaining =
            std::max(0.0L, available - paid_total.value());
        const long double paid = std::min(remaining, capacity);
        allocation[index] = paid;
        paid_total.add(paid);
    }
    return paid_total.value();
}

void allocate_nonprincipal(long double available,
    const CapitalStackConfig& config, std::vector<RunningTranche>& tranches,
    std::vector<long double>& allocation) {
    CompensatedSum priority_paid_total;
    for (std::size_t reverse = config.tranches.size(); reverse > 1U; --reverse) {
        const std::size_t index = reverse - 1U;
        const long double capacity = std::max(0.0L,
            static_cast<long double>(
                config.tranches[index].priority_nonprincipal_cap_million) -
                tranches[index].nonprincipal_cash.value());
        const long double remaining =
            std::max(0.0L, available - priority_paid_total.value());
        const long double paid = std::min(remaining, capacity);
        allocation[index] = paid;
        priority_paid_total.add(paid);
    }
    allocation[0U] =
        std::max(0.0L, available - priority_paid_total.value());
}

[[nodiscard]] CapitalStackScenarioResult evaluate_scenario(
    const PortfolioConfig& selected_portfolio,
    const JointScenarioResult& underlying,
    const JointScenario& source_scenario, const CapitalStackConfig& config,
    double commitment, CapitalStackSummary& summary) {
    const std::size_t month_count = selected_portfolio.horizon_months + 1U;
    const CapitalStackFundingBasis funding =
        capital_stack_funding_basis(selected_portfolio, config);
    MonthlyPoolCash pool = build_monthly_pool_cash(
        selected_portfolio, source_scenario, month_count, config);

    CompensatedSum acquisition_uses;
    CompensatedSum claim_purchase_price_uses;
    CompensatedSum primary_project_funding_uses;
    CompensatedSum buyer_direct_costs;
    for (std::size_t month = 0U; month < month_count; ++month) {
        acquisition_uses.add(
            pool.acquisition_and_primary_funding_uses[month].value());
        claim_purchase_price_uses.add(
            pool.claim_purchase_price_uses[month].value());
        primary_project_funding_uses.add(
            pool.primary_project_funding_uses[month].value());
        buyer_direct_costs.add(pool.buyer_direct_costs[month].value());
    }
    const double total_acquisition_uses = checked_double(
        acquisition_uses.value(), "capital-stack acquisition uses");
    const double total_buyer_direct_costs = checked_double(
        buyer_direct_costs.value(), "capital-stack buyer direct costs");
    const double unused_commitment = std::max(
        0.0, commitment - total_acquisition_uses);
    const double principal_base_cash =
        underlying.principal_returned_million + unused_commitment;
    const double distributable_principal =
        std::min(commitment, principal_base_cash);
    const double principal_base_surplus =
        std::max(0.0, principal_base_cash - commitment);
    const double underlying_nonprincipal =
        underlying.total_receipts_million -
        underlying.principal_returned_million;

    CapitalStackScenarioResult scenario;
    scenario.scenario_id = underlying.scenario_id;
    scenario.central_weight = underlying.normalized_weight;
    scenario.model_version = config.model_version;
    scenario.uses_explicit_asset_liability_accounting =
        funding.uses_explicit_asset_liability_accounting;
    scenario.aggregate_project_outlay_limit_million =
        funding.project_outlay_limit_million;
    scenario.aggregate_contractual_asset_principal_limit_million =
        funding.contractual_asset_principal_limit_million;
    scenario.aggregate_commitment_million = commitment;
    scenario.total_project_draws_million = underlying.total_draws_million;
    scenario.total_asset_acquisition_and_primary_funding_uses_million =
        total_acquisition_uses;
    scenario.total_claim_purchase_price_million = checked_double(
        claim_purchase_price_uses.value(),
        "capital-stack claim purchase-price uses");
    scenario.total_primary_project_funding_million = checked_double(
        primary_project_funding_uses.value(),
        "capital-stack primary-project-funding uses");
    scenario.total_buyer_direct_costs_million = total_buyer_direct_costs;
    scenario.contractual_principal_limit_minus_funding_uses_million =
        funding.contractual_asset_principal_limit_million -
        total_acquisition_uses;
    scenario.unused_commitment_returned_at_horizon_million = unused_commitment;
    scenario.underlying_principal_cash_million =
        underlying.principal_returned_million;
    scenario.distributable_principal_cash_million = distributable_principal;
    scenario.underlying_nonprincipal_cash_million = underlying_nonprincipal;
    scenario.distributable_nonprincipal_cash_million =
        underlying_nonprincipal + principal_base_surplus;
    scenario.total_pool_costs_million = underlying.total_pool_costs_million;
    scenario.contractual_asset_principal_loss_million =
        underlying.principal_loss_million;
    scenario.contractual_asset_outstanding_principal_million =
        underlying.outstanding_principal_million;
    scenario.underlying_on_demand_npv_million = underlying.npv_million;
    scenario.underlying_nominal_net_cash_million =
        underlying.total_receipts_million -
        underlying.total_investor_outlays_million -
        underlying.total_pool_costs_million;

    std::vector<RunningTranche> running(config.tranches.size());
    for (std::size_t index = 0U; index < config.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& term = config.tranches[index];
        RunningTranche& tranche = running[index];
        tranche.result.tranche_id = term.id;
        tranche.result.legacy_v01_loss_layering_metrics_are_applicable =
            !is_v02(config);
        tranche.result.notional_million =
            term.detachment_million - term.attachment_million;
        tranche.result.par_subscription_million = tranche.result.notional_million;
    }

    CompensatedSum direct_stack_npv_at_pool_hurdle;
    direct_stack_npv_at_pool_hurdle.add(-static_cast<long double>(commitment));
    CompensatedSum allocated_stack_npv_at_pool_hurdle;
    std::vector<long double> principal_allocation(config.tranches.size());
    std::vector<long double> underlying_principal_allocation(
        config.tranches.size());
    std::vector<long double> reserve_principal_allocation(
        config.tranches.size());
    std::vector<long double> nonprincipal_allocation(config.tranches.size());

    CompensatedSum reserve_balance;
    reserve_balance.add(static_cast<long double>(commitment));
    double scenario_maximum_reserve_roll_forward_error_million = 0.0;
    double scenario_maximum_reserve_shortfall_million = 0.0;
    for (std::size_t month = 0U; month < month_count; ++month) {
        const long double monthly_acquisition =
            pool.acquisition_and_primary_funding_uses[month].value();
        const long double monthly_buyer_direct_cost =
            pool.buyer_direct_costs[month].value();
        const long double monthly_cost = pool.pool_costs[month].value();
        const long double monthly_principal = pool.principal[month].value();
        const long double monthly_underlying_nonprincipal =
            pool.nonprincipal[month].value();
        std::fill(principal_allocation.begin(), principal_allocation.end(), 0.0L);
        std::fill(underlying_principal_allocation.begin(),
            underlying_principal_allocation.end(), 0.0L);
        std::fill(reserve_principal_allocation.begin(),
            reserve_principal_allocation.end(), 0.0L);
        std::fill(
            nonprincipal_allocation.begin(), nonprincipal_allocation.end(), 0.0L);
        const long double reserve_before_draw = reserve_balance.value();
        reserve_balance.add(-monthly_acquisition);
        const long double reserve_after_draw = reserve_balance.value();
        const long double reserve_roll_forward_error = std::abs(
            reserve_before_draw - monthly_acquisition - reserve_after_draw);
        scenario_maximum_reserve_roll_forward_error_million = std::max(
            scenario_maximum_reserve_roll_forward_error_million,
            checked_double(reserve_roll_forward_error,
                "capital-stack reserve roll-forward error"));
        scenario_maximum_reserve_shortfall_million = std::max(
            scenario_maximum_reserve_shortfall_million,
            checked_double(std::max(0.0L, -reserve_after_draw),
                "capital-stack reserve shortfall"));
        const long double reserve_return =
            month == selected_portfolio.horizon_months
            ? static_cast<long double>(unused_commitment)
            : 0.0L;
        const long double combined_principal =
            monthly_principal + reserve_return;
        const long double principal_paid_total = allocate_principal(
            combined_principal, config, running, principal_allocation);
        const long double principal_surplus =
            std::max(0.0L, combined_principal - principal_paid_total);
        if (!is_v02(config)) {
            enforce_reconciliation(checked_double(principal_surplus,
                                       "capital-stack v0.1 principal residual"),
                checked_double(combined_principal,
                    "capital-stack v0.1 principal scale"),
                "capital-stack v0.1 principal waterfall left cash unallocated");
        }
        // Project principal and the reserve return have equal seniority. When
        // both arrive in one month, attribute each tranche's principal cash
        // pro rata to the two sources rather than letting processing order
        // manufacture a source preference.
        if (combined_principal > 0.0L) {
            const long double underlying_fraction =
                monthly_principal / combined_principal;
            for (std::size_t index = 0U; index < config.tranches.size(); ++index) {
                underlying_principal_allocation[index] =
                    principal_allocation[index] * underlying_fraction;
                reserve_principal_allocation[index] =
                    principal_allocation[index] -
                    underlying_principal_allocation[index];
                running[index].underlying_principal_cash.add(
                    underlying_principal_allocation[index]);
                running[index].reserve_principal_cash.add(
                    reserve_principal_allocation[index]);
                running[index].principal_cash.add(principal_allocation[index]);
            }
        }
        const long double contractual_principal_surplus =
            combined_principal > 0.0L
            ? principal_surplus * monthly_principal / combined_principal
            : 0.0L;
        const long double reserve_surplus =
            principal_surplus - contractual_principal_surplus;
        if (month == selected_portfolio.horizon_months) {
            const double reserve_error = absolute_difference(
                checked_double(reserve_balance.value(),
                    "capital-stack ending reserve balance"),
                unused_commitment);
            scenario_maximum_reserve_roll_forward_error_million = std::max(
                scenario_maximum_reserve_roll_forward_error_million,
                reserve_error);
            reserve_balance.add(-static_cast<long double>(unused_commitment));
            scenario_maximum_reserve_roll_forward_error_million = std::max(
                scenario_maximum_reserve_roll_forward_error_million,
                checked_double(std::abs(reserve_balance.value()),
                    "capital-stack reserve close-out error"));
        }
        const long double monthly_nonprincipal =
            monthly_underlying_nonprincipal +
            contractual_principal_surplus + reserve_surplus;
        allocate_nonprincipal(monthly_nonprincipal, config, running,
            nonprincipal_allocation);

        const double pool_discount = discount_factor(
            selected_portfolio.annual_physical_hurdle_rate, month);
        const long double pool_net = monthly_principal + reserve_return +
            monthly_underlying_nonprincipal - monthly_cost -
            monthly_buyer_direct_cost;
        direct_stack_npv_at_pool_hurdle.add(
            pool_net / static_cast<long double>(pool_discount));

        for (std::size_t index = 0U; index < config.tranches.size(); ++index) {
            RunningTranche& tranche = running[index];
            const CapitalStackTrancheConfig& term = config.tranches[index];
            const long double share =
                static_cast<long double>(tranche.result.notional_million) /
                static_cast<long double>(commitment);
            const long double buyer_direct_cost_call =
                monthly_buyer_direct_cost * share;
            const long double cost_call = monthly_cost * share;
            const long double subscription = month == 0U
                ? static_cast<long double>(tranche.result.notional_million)
                : 0.0L;
            const long double underlying_principal_paid =
                underlying_principal_allocation[index];
            const long double reserve_principal_paid =
                reserve_principal_allocation[index];
            const long double principal_paid = principal_allocation[index];
            const long double nonprincipal_paid = nonprincipal_allocation[index];
            const long double underlying_nonprincipal_paid =
                monthly_nonprincipal > 0.0L
                ? nonprincipal_paid * monthly_underlying_nonprincipal /
                    monthly_nonprincipal
                : 0.0L;
            const long double contractual_principal_surplus_paid =
                monthly_nonprincipal > 0.0L
                ? nonprincipal_paid * contractual_principal_surplus /
                    monthly_nonprincipal
                : 0.0L;
            const long double reserve_surplus_paid = nonprincipal_paid -
                underlying_nonprincipal_paid -
                contractual_principal_surplus_paid;
            const long double net = principal_paid + nonprincipal_paid -
                subscription - buyer_direct_cost_call - cost_call;

            tranche.pro_rata_buyer_direct_cost_calls.add(
                buyer_direct_cost_call);
            tranche.pro_rata_pool_cost_calls.add(cost_call);
            tranche.contractual_principal_surplus_cash.add(
                contractual_principal_surplus_paid);
            tranche.reserve_surplus_cash.add(reserve_surplus_paid);
            tranche.underlying_nonprincipal_cash.add(
                underlying_nonprincipal_paid);
            tranche.nonprincipal_cash.add(nonprincipal_paid);
            tranche.principal_month_numerator.add(
                static_cast<long double>(month) * principal_paid);

            const double tranche_discount =
                discount_factor(term.annual_physical_hurdle_rate, month);
            tranche.npv_at_tranche_hurdle.add(
                net / static_cast<long double>(tranche_discount));
            allocated_stack_npv_at_pool_hurdle.add(
                net / static_cast<long double>(pool_discount));

            if (subscription != 0.0L || buyer_direct_cost_call != 0.0L ||
                cost_call != 0.0L ||
                principal_paid != 0.0L || nonprincipal_paid != 0.0L) {
                tranche.result.monthly_cash_flows.push_back(
                    CapitalStackMonthlyTrancheCashFlow{month,
                        checked_double(subscription,
                            "capital-stack monthly subscription"),
                        checked_double(buyer_direct_cost_call,
                            "capital-stack monthly buyer direct-cost call"),
                        checked_double(cost_call,
                            "capital-stack monthly cost call"),
                        checked_double(underlying_principal_paid,
                            "capital-stack monthly underlying principal"),
                        checked_double(reserve_principal_paid,
                            "capital-stack monthly reserve principal"),
                        checked_double(principal_paid,
                            "capital-stack monthly principal distribution"),
                        checked_double(contractual_principal_surplus_paid,
                            "capital-stack monthly contractual-principal surplus"),
                        checked_double(reserve_surplus_paid,
                            "capital-stack monthly reserve surplus"),
                        checked_double(underlying_nonprincipal_paid,
                            "capital-stack monthly underlying non-principal"),
                        checked_double(nonprincipal_paid,
                            "capital-stack monthly non-principal distribution"),
                        checked_double(net, "capital-stack monthly net cash")});
            }
        }
    }

    CompensatedSum subscriptions;
    CompensatedSum buyer_direct_cost_calls;
    CompensatedSum cost_calls;
    CompensatedSum underlying_principal_distributions;
    CompensatedSum reserve_principal_distributions;
    CompensatedSum principal_distributions;
    CompensatedSum contractual_principal_surplus_distributions;
    CompensatedSum reserve_surplus_distributions;
    CompensatedSum underlying_nonprincipal_distributions;
    CompensatedSum nonprincipal_distributions;
    CompensatedSum layer_v01_losses;
    CompensatedSum layer_v01_outstanding;
    CompensatedSum layer_principal_cash_shortfalls;
    CompensatedSum stack_net_cash;
    scenario.tranches.reserve(config.tranches.size());
    const double total_principal_shortfall = canonical_nonnegative_residual(
        commitment - scenario.distributable_principal_cash_million,
        commitment);
    scenario.issued_principal_cash_shortfall_million =
        total_principal_shortfall;
    for (std::size_t index = 0U; index < config.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& term = config.tranches[index];
        CapitalStackTrancheScenarioResult& tranche = running[index].result;
        tranche.pro_rata_buyer_direct_cost_calls_million = checked_double(
            running[index].pro_rata_buyer_direct_cost_calls.value(),
            "capital-stack tranche buyer direct-cost calls");
        tranche.pro_rata_pool_cost_calls_million = checked_double(
            running[index].pro_rata_pool_cost_calls.value(),
            "capital-stack tranche pool-cost calls");
        tranche.underlying_principal_cash_distribution_million = checked_double(
            running[index].underlying_principal_cash.value(),
            "capital-stack tranche underlying principal cash");
        tranche.unused_reserve_principal_return_million = checked_double(
            running[index].reserve_principal_cash.value(),
            "capital-stack tranche reserve principal cash");
        tranche.principal_cash_distribution_million = checked_double(
            running[index].principal_cash.value(),
            "capital-stack tranche principal cash");
        tranche.contractual_principal_surplus_cash_distribution_million =
            checked_double(
                running[index].contractual_principal_surplus_cash.value(),
                "capital-stack tranche contractual-principal surplus");
        tranche.unused_reserve_surplus_cash_distribution_million =
            checked_double(running[index].reserve_surplus_cash.value(),
                "capital-stack tranche unused-reserve surplus");
        tranche.underlying_nonprincipal_cash_distribution_million =
            checked_double(running[index].underlying_nonprincipal_cash.value(),
                "capital-stack tranche underlying non-principal cash");
        tranche.nonprincipal_cash_distribution_million = checked_double(
            running[index].nonprincipal_cash.value(),
            "capital-stack tranche non-principal cash");
        tranche.npv_at_tranche_hurdle_million = checked_double(
            running[index].npv_at_tranche_hurdle.value(),
            "capital-stack tranche NPV");
        tranche.total_contributions_million = tranche.par_subscription_million +
            tranche.pro_rata_buyer_direct_cost_calls_million +
            tranche.pro_rata_pool_cost_calls_million;
        tranche.total_distributions_million =
            tranche.principal_cash_distribution_million +
            tranche.nonprincipal_cash_distribution_million;
        tranche.principal_cash_shortfall_million =
            tranche_loss(total_principal_shortfall, term);
        enforce_reconciliation(absolute_difference(
                                   tranche.principal_cash_shortfall_million,
                                   std::max(0.0,
                                       tranche.notional_million -
                                           tranche
                                               .principal_cash_distribution_million)),
            commitment,
            "capital-stack tranche cash shortfall disagrees with principal allocation");
        if (!is_v02(config)) {
            tranche.realized_principal_loss_million =
                tranche_loss(underlying.principal_loss_million, term);
            const double loss_and_outstanding = tranche_loss(
                underlying.principal_loss_million +
                    underlying.outstanding_principal_million,
                term);
            tranche.unresolved_principal_exposure_million = std::max(
                0.0, loss_and_outstanding -
                    tranche.realized_principal_loss_million);
        }
        tranche.unused_priority_nonprincipal_capacity_million =
            term.is_first_loss_residual
            ? 0.0
            : std::max(0.0,
                  term.priority_nonprincipal_cap_million -
                      tranche.nonprincipal_cash_distribution_million);
        tranche.nominal_net_cash_million = tranche.total_distributions_million -
            tranche.total_contributions_million;
        tranche.all_in_cash_shortfall_million = std::max(
            0.0, -tranche.nominal_net_cash_million);
        tranche.cash_multiple = tranche.total_distributions_million /
            tranche.total_contributions_million;
        tranche.net_return_fraction = tranche.cash_multiple - 1.0;
        tranche.principal_cash_time_million_years = checked_double(
            running[index].principal_month_numerator.value() / 12.0L,
            "capital-stack principal cash time");
        if (tranche.principal_cash_distribution_million > 0.0) {
            tranche.weighted_average_principal_cash_month = checked_double(
                running[index].principal_month_numerator.value() /
                    static_cast<long double>(
                        tranche.principal_cash_distribution_million),
                "capital-stack weighted-average principal cash month");
        }

        if (!is_v02(config)) {
            const double shortfall_error = absolute_difference(
                tranche.principal_cash_shortfall_million,
                tranche.realized_principal_loss_million +
                    tranche.unresolved_principal_exposure_million);
            enforce_reconciliation(shortfall_error, tranche.notional_million,
                "capital-stack v0.1 tranche principal shortfall does not reconcile");
        }

        subscriptions.add(
            static_cast<long double>(tranche.par_subscription_million));
        buyer_direct_cost_calls.add(static_cast<long double>(
            tranche.pro_rata_buyer_direct_cost_calls_million));
        cost_calls.add(static_cast<long double>(
            tranche.pro_rata_pool_cost_calls_million));
        underlying_principal_distributions.add(static_cast<long double>(
            tranche.underlying_principal_cash_distribution_million));
        reserve_principal_distributions.add(static_cast<long double>(
            tranche.unused_reserve_principal_return_million));
        principal_distributions.add(static_cast<long double>(
            tranche.principal_cash_distribution_million));
        contractual_principal_surplus_distributions.add(
            static_cast<long double>(
                tranche.contractual_principal_surplus_cash_distribution_million));
        reserve_surplus_distributions.add(static_cast<long double>(
            tranche.unused_reserve_surplus_cash_distribution_million));
        underlying_nonprincipal_distributions.add(static_cast<long double>(
            tranche.underlying_nonprincipal_cash_distribution_million));
        nonprincipal_distributions.add(static_cast<long double>(
            tranche.nonprincipal_cash_distribution_million));
        layer_v01_losses.add(
            static_cast<long double>(tranche.realized_principal_loss_million));
        layer_v01_outstanding.add(static_cast<long double>(
            tranche.unresolved_principal_exposure_million));
        layer_principal_cash_shortfalls.add(static_cast<long double>(
            tranche.principal_cash_shortfall_million));
        stack_net_cash.add(
            static_cast<long double>(tranche.nominal_net_cash_million));
        scenario.tranches.push_back(std::move(tranche));
    }

    scenario.stack_nominal_net_cash_million =
        checked_double(stack_net_cash.value(), "capital-stack nominal net cash");
    scenario.fully_funded_stack_npv_at_pool_hurdle_million = checked_double(
        direct_stack_npv_at_pool_hurdle.value(),
        "capital-stack fully-funded NPV");
    scenario.prefunding_drag_npv_million =
        scenario.underlying_on_demand_npv_million -
        scenario.fully_funded_stack_npv_at_pool_hurdle_million;

    scenario.contractual_principal_surplus_cash_million = checked_double(
        contractual_principal_surplus_distributions.value(),
        "capital-stack scenario contractual-principal surplus");
    scenario.unused_reserve_surplus_cash_million = checked_double(
        reserve_surplus_distributions.value(),
        "capital-stack scenario unused-reserve surplus");

    const double commitment_identity_error = absolute_difference(commitment,
        total_acquisition_uses + unused_commitment);
    const double principal_identity_error = absolute_difference(commitment,
        scenario.distributable_principal_cash_million +
            total_principal_shortfall);
    const double layer_shortfall_reconciliation_error = absolute_difference(
        checked_double(layer_principal_cash_shortfalls.value(),
            "capital-stack layer principal cash shortfalls"),
        total_principal_shortfall);
    const double investor_outlay_separation_error = absolute_difference(
        underlying.total_investor_outlays_million,
        total_acquisition_uses + total_buyer_direct_costs);
    const double maximum_commitment_identity_error =
        std::max({commitment_identity_error, principal_identity_error,
            investor_outlay_separation_error,
            layer_shortfall_reconciliation_error});
    const double subscription_reconciliation_error = absolute_difference(
        checked_double(subscriptions.value(), "capital-stack subscriptions"),
        commitment);
    const double pool_cost_call_reconciliation_error = absolute_difference(
        checked_double(cost_calls.value(), "capital-stack cost calls"),
        underlying.total_pool_costs_million);
    const double buyer_direct_cost_call_reconciliation_error =
        absolute_difference(checked_double(buyer_direct_cost_calls.value(),
                                "capital-stack buyer direct-cost calls"),
            total_buyer_direct_costs);
    const double principal_distribution_reconciliation_error = std::max(
        {absolute_difference(
             checked_double(underlying_principal_distributions.value() +
                     contractual_principal_surplus_distributions.value(),
                 "capital-stack underlying principal cash allocation"),
             underlying.principal_returned_million),
            absolute_difference(
                checked_double(reserve_principal_distributions.value() +
                        reserve_surplus_distributions.value(),
                    "capital-stack reserve cash allocation"),
                unused_commitment),
            absolute_difference(
                checked_double(principal_distributions.value(),
                    "capital-stack principal distributions"),
                scenario.distributable_principal_cash_million)});
    const double contractual_principal_surplus_reconciliation_error =
        absolute_difference(checked_double(
                                contractual_principal_surplus_distributions.value(),
                                "capital-stack contractual-principal surplus distributions"),
            scenario.contractual_principal_surplus_cash_million);
    const double unused_reserve_surplus_reconciliation_error =
        absolute_difference(checked_double(
                                reserve_surplus_distributions.value(),
                                "capital-stack reserve-surplus distributions"),
            scenario.unused_reserve_surplus_cash_million);
    const double nonprincipal_distribution_reconciliation_error =
        std::max({absolute_difference(
                      checked_double(
                          underlying_nonprincipal_distributions.value(),
                          "capital-stack underlying non-principal distributions"),
                      underlying_nonprincipal),
            absolute_difference(
            checked_double(nonprincipal_distributions.value(),
                "capital-stack non-principal distributions"),
            scenario.distributable_nonprincipal_cash_million),
            absolute_difference(
                scenario.contractual_principal_surplus_cash_million +
                    scenario.unused_reserve_surplus_cash_million,
                principal_base_surplus)});
    double priority_nonprincipal_cap_violation = 0.0;
    for (std::size_t index = 1U; index < config.tranches.size(); ++index) {
        priority_nonprincipal_cap_violation = std::max(
            priority_nonprincipal_cap_violation,
            std::max(0.0,
                scenario.tranches[index]
                        .nonprincipal_cash_distribution_million -
                    config.tranches[index]
                        .priority_nonprincipal_cap_million));
    }
    const double realized_loss_reconciliation_error = is_v02(config)
        ? 0.0
        : absolute_difference(
              checked_double(layer_v01_losses.value(),
                  "capital-stack v0.1 layer losses"),
              underlying.principal_loss_million);
    const double contractual_asset_loss_preservation_error =
        absolute_difference(scenario.contractual_asset_principal_loss_million,
            underlying.principal_loss_million);
    const double contractual_asset_outstanding_preservation_error =
        absolute_difference(
            scenario.contractual_asset_outstanding_principal_million,
            underlying.outstanding_principal_million);
    const double unresolved_exposure_reconciliation_error = is_v02(config)
        ? 0.0
        : absolute_difference(
              checked_double(layer_v01_outstanding.value(),
                  "capital-stack v0.1 layer outstanding exposure"),
              underlying.outstanding_principal_million);
    const double nominal_net_cash_reconciliation_error = absolute_difference(
        scenario.stack_nominal_net_cash_million,
        scenario.underlying_nominal_net_cash_million);
    const double stack_npv_reconciliation_error = absolute_difference(
        scenario.fully_funded_stack_npv_at_pool_hurdle_million,
        checked_double(allocated_stack_npv_at_pool_hurdle.value(),
            "capital-stack allocated NPV"));

    const double scale = std::max({commitment, underlying.total_receipts_million,
        underlying.total_pool_costs_million});
    enforce_reconciliation(maximum_commitment_identity_error,
        scale, "capital-stack commitment identity failed");
    enforce_reconciliation(
        subscription_reconciliation_error, scale,
        "capital-stack subscription reconciliation failed");
    enforce_reconciliation(
        pool_cost_call_reconciliation_error, scale,
        "capital-stack pool-cost reconciliation failed");
    enforce_reconciliation(buyer_direct_cost_call_reconciliation_error, scale,
        "capital-stack buyer direct-cost reconciliation failed");
    enforce_reconciliation(
        principal_distribution_reconciliation_error,
        scale, "capital-stack principal distribution reconciliation failed");
    enforce_reconciliation(
        nonprincipal_distribution_reconciliation_error,
        scale, "capital-stack non-principal distribution reconciliation failed");
    enforce_reconciliation(contractual_principal_surplus_reconciliation_error,
        scale,
        "capital-stack contractual-principal surplus reconciliation failed");
    enforce_reconciliation(unused_reserve_surplus_reconciliation_error, scale,
        "capital-stack unused-reserve surplus reconciliation failed");
    enforce_reconciliation(
        priority_nonprincipal_cap_violation, scale,
        "capital-stack priority non-principal cap was exceeded");
    enforce_reconciliation(
        realized_loss_reconciliation_error, scale,
        "capital-stack realized loss reconciliation failed");
    enforce_reconciliation(contractual_asset_loss_preservation_error, scale,
        "capital-stack changed contractual asset principal loss");
    enforce_reconciliation(contractual_asset_outstanding_preservation_error,
        scale, "capital-stack changed contractual asset outstanding principal");
    enforce_reconciliation(
        unresolved_exposure_reconciliation_error, scale,
        "capital-stack unresolved exposure reconciliation failed");
    enforce_reconciliation(
        nominal_net_cash_reconciliation_error, scale,
        "capital-stack nominal cash reconciliation failed");
    enforce_reconciliation(stack_npv_reconciliation_error,
        scale, "capital-stack NPV reconciliation failed");
    enforce_reconciliation(scenario_maximum_reserve_roll_forward_error_million,
        scale, "capital-stack reserve roll-forward failed");
    enforce_reconciliation(scenario_maximum_reserve_shortfall_million, scale,
        "capital-stack reserve became negative");
    if (scenario.prefunding_drag_npv_million <
        -(kReconciliationAbsoluteTolerance +
            kReconciliationRelativeTolerance * std::max(1.0, scale))) {
        throw std::logic_error(
            "capital-stack zero-yield prefunding unexpectedly improves NPV");
    }

    summary.maximum_commitment_identity_error_million = std::max(
        summary.maximum_commitment_identity_error_million,
        maximum_commitment_identity_error);
    summary.maximum_reserve_roll_forward_error_million = std::max(
        summary.maximum_reserve_roll_forward_error_million,
        scenario_maximum_reserve_roll_forward_error_million);
    summary.maximum_reserve_shortfall_million = std::max(
        summary.maximum_reserve_shortfall_million,
        scenario_maximum_reserve_shortfall_million);
    summary.maximum_subscription_reconciliation_error_million = std::max(
        summary.maximum_subscription_reconciliation_error_million,
        subscription_reconciliation_error);
    summary.maximum_pool_cost_call_reconciliation_error_million = std::max(
        summary.maximum_pool_cost_call_reconciliation_error_million,
        pool_cost_call_reconciliation_error);
    summary.maximum_buyer_direct_cost_call_reconciliation_error_million =
        std::max(summary
                     .maximum_buyer_direct_cost_call_reconciliation_error_million,
            buyer_direct_cost_call_reconciliation_error);
    summary.maximum_principal_distribution_reconciliation_error_million =
        std::max(
            summary.maximum_principal_distribution_reconciliation_error_million,
            principal_distribution_reconciliation_error);
    summary.maximum_contractual_principal_surplus_reconciliation_error_million =
        std::max(summary
                     .maximum_contractual_principal_surplus_reconciliation_error_million,
            contractual_principal_surplus_reconciliation_error);
    summary.maximum_unused_reserve_surplus_reconciliation_error_million =
        std::max(summary
                     .maximum_unused_reserve_surplus_reconciliation_error_million,
            unused_reserve_surplus_reconciliation_error);
    summary.maximum_nonprincipal_distribution_reconciliation_error_million =
        std::max(summary
                     .maximum_nonprincipal_distribution_reconciliation_error_million,
            nonprincipal_distribution_reconciliation_error);
    summary.maximum_priority_nonprincipal_cap_violation_million = std::max(
        summary.maximum_priority_nonprincipal_cap_violation_million,
        priority_nonprincipal_cap_violation);
    summary.maximum_realized_loss_reconciliation_error_million = std::max(
        summary.maximum_realized_loss_reconciliation_error_million,
        realized_loss_reconciliation_error);
    summary.maximum_contractual_asset_loss_preservation_error_million =
        std::max(summary
                     .maximum_contractual_asset_loss_preservation_error_million,
            contractual_asset_loss_preservation_error);
    summary.maximum_contractual_asset_outstanding_preservation_error_million =
        std::max(summary
                     .maximum_contractual_asset_outstanding_preservation_error_million,
            contractual_asset_outstanding_preservation_error);
    summary.maximum_unresolved_exposure_reconciliation_error_million = std::max(
        summary.maximum_unresolved_exposure_reconciliation_error_million,
        unresolved_exposure_reconciliation_error);
    summary.maximum_nominal_net_cash_reconciliation_error_million = std::max(
        summary.maximum_nominal_net_cash_reconciliation_error_million,
        nominal_net_cash_reconciliation_error);
    summary.maximum_stack_npv_reconciliation_error_million = std::max(
        summary.maximum_stack_npv_reconciliation_error_million,
        stack_npv_reconciliation_error);

    return scenario;
}

[[nodiscard]] double weighted_endpoint_component(
    const AmbiguityEndpoint& endpoint,
    const std::vector<ScenarioProbabilityBounds>& bounds,
    const std::vector<CapitalStackScenarioResult>& scenarios,
    std::size_t tranche_index, bool numerator) {
    if (endpoint.scenario_weights.size() != bounds.size()) {
        throw std::logic_error(
            "capital-stack WAL endpoint has the wrong probability count");
    }
    std::unordered_map<std::string, const CapitalStackScenarioResult*> by_id;
    by_id.reserve(scenarios.size());
    for (const CapitalStackScenarioResult& scenario : scenarios) {
        by_id.emplace(scenario.scenario_id, &scenario);
    }
    CompensatedSum sum;
    for (std::size_t index = 0U; index < bounds.size(); ++index) {
        const auto matching = by_id.find(bounds[index].scenario_id);
        if (matching == by_id.end() ||
            tranche_index >= matching->second->tranches.size()) {
            throw std::logic_error(
                "capital-stack WAL endpoint lost a scenario or tranche");
        }
        const CapitalStackTrancheScenarioResult& tranche =
            matching->second->tranches[tranche_index];
        const double value = numerator
            ? tranche.principal_cash_time_million_years
            : tranche.principal_cash_distribution_million;
        sum.add(static_cast<long double>(endpoint.scenario_weights[index]) *
            static_cast<long double>(value));
    }
    return checked_double(sum.value(), "capital-stack WAL witness component");
}

struct WalSolveResult {
    std::optional<AmbiguityMetricRange> range{};
    double maximum_objective_residual_million_years{0.0};
};

[[nodiscard]] WalSolveResult solve_principal_wal(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<CapitalStackScenarioResult>& scenarios,
    const std::vector<ScenarioProbabilityBounds>& bounds,
    std::size_t tranche_index, double maximum_years,
    CapitalStackSummary& summary) {
    const auto denominator = project_metric(projector, scenarios,
        [tranche_index](const CapitalStackScenarioResult& scenario) {
            return scenario.tranches[tranche_index]
                .principal_cash_distribution_million;
        });
    update_projection_error(
        denominator.maximum_endpoint_probability_error, summary);
    if (denominator.expectation.minimum.value <=
        kWalMinimumExpectedPrincipalCashMillion) {
        return {};
    }
    const auto numerator = project_metric(projector, scenarios,
        [tranche_index](const CapitalStackScenarioResult& scenario) {
            return scenario.tranches[tranche_index]
                .principal_cash_time_million_years;
        });
    update_projection_error(numerator.maximum_endpoint_probability_error,
        summary);

    const auto objective = [&](double ratio) {
        const auto projected = project_metric(projector, scenarios,
            [tranche_index, ratio](
                const CapitalStackScenarioResult& scenario) {
                const CapitalStackTrancheScenarioResult& tranche =
                    scenario.tranches[tranche_index];
                return checked_double(
                    static_cast<long double>(
                        tranche.principal_cash_time_million_years) -
                        static_cast<long double>(ratio) *
                            static_cast<long double>(
                                tranche.principal_cash_distribution_million),
                    "capital-stack WAL ratio objective");
            });
        update_projection_error(
            projected.maximum_endpoint_probability_error, summary);
        return projected;
    };

    const auto solve_endpoint = [&](bool maximize) {
        double lower = 0.0;
        double upper = maximum_years;
        std::optional<AmbiguityEndpoint> binding;
        for (std::size_t iteration = 0U; iteration < 128U; ++iteration) {
            const double midpoint = std::midpoint(lower, upper);
            if (midpoint == lower || midpoint == upper) {
                break;
            }
            const AmbiguityMetricProjection projected = objective(midpoint);
            const double endpoint_value = maximize
                ? projected.expectation.maximum.value
                : projected.expectation.minimum.value;
            const bool ratio_above_midpoint = maximize
                ? endpoint_value > 0.0
                : endpoint_value >= 0.0;
            if (ratio_above_midpoint) {
                lower = midpoint;
                if (maximize) {
                    binding = projected.expectation.maximum;
                }
            } else {
                upper = midpoint;
                if (!maximize) {
                    binding = projected.expectation.minimum;
                }
            }
        }
        if (!binding.has_value()) {
            const AmbiguityMetricProjection boundary =
                objective(maximize ? lower : upper);
            binding = maximize ? boundary.expectation.maximum
                               : boundary.expectation.minimum;
        }
        const double witness_numerator = weighted_endpoint_component(*binding,
            bounds, scenarios, tranche_index, true);
        const double witness_denominator = weighted_endpoint_component(*binding,
            bounds, scenarios, tranche_index, false);
        if (witness_denominator <= 0.0) {
            throw std::logic_error(
                "capital-stack WAL witness has no principal cash");
        }
        const double ratio = witness_numerator / witness_denominator;
        if (!std::isfinite(ratio) || ratio < -1.0e-12 ||
            ratio > maximum_years + 1.0e-12) {
            throw std::logic_error(
                "capital-stack WAL is outside the analysis horizon");
        }
        binding->value = std::clamp(ratio, 0.0, maximum_years);
        return *binding;
    };

    AmbiguityMetricRange wal;
    wal.minimum = solve_endpoint(false);
    wal.central = numerator.expectation.central /
        denominator.expectation.central;
    wal.maximum = solve_endpoint(true);
    if (wal.minimum.value > wal.central + 1.0e-10 ||
        wal.central > wal.maximum.value + 1.0e-10) {
        throw std::logic_error(
            "capital-stack WAL central value lies outside robust bounds");
    }

    WalSolveResult result;
    result.range = wal;
    const auto certify = [&](const AmbiguityEndpoint& endpoint,
                             bool maximize) {
        const AmbiguityMetricProjection certification =
            objective(endpoint.value);
        const double optimum = maximize
            ? certification.expectation.maximum.value
            : certification.expectation.minimum.value;
        const double witness_residual =
            weighted_endpoint_component(endpoint, bounds, scenarios,
                tranche_index, true) -
            endpoint.value * weighted_endpoint_component(endpoint, bounds,
                                 scenarios, tranche_index, false);
        result.maximum_objective_residual_million_years = std::max(
            result.maximum_objective_residual_million_years,
            std::max(std::abs(optimum), std::abs(witness_residual)));
    };
    certify(wal.minimum, false);
    certify(wal.maximum, true);
    const double scale = std::max(1.0,
        denominator.expectation.maximum.value * maximum_years);
    enforce_reconciliation(
        result.maximum_objective_residual_million_years, scale,
        "capital-stack WAL common-witness certification failed");
    return result;
}

} // namespace

void validate_capital_stack_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack) {
    validate_success_participation_config(portfolio, ambiguity, participation);
    const bool legacy_v01 =
        stack.model_version == kCapitalStackLegacyModelVersion;
    const bool explicit_v02 = is_v02(stack);
    if (!legacy_v01 && !explicit_v02) {
        throw std::invalid_argument(
            "unsupported capital-stack model version");
    }
    require_safe_text(stack.scenario_label, "capital-stack label");
    require_safe_text(stack.source_note, "capital-stack source note");
    if (!stack.synthetic_inputs || !portfolio.synthetic_inputs ||
        !ambiguity.synthetic_inputs || !participation.synthetic_inputs) {
        throw std::invalid_argument(
            "capital-stack v0.1 and v0.2 accept synthetic inputs only");
    }
    const bool portfolio_has_explicit_ledger = std::any_of(
        portfolio.projects.begin(), portfolio.projects.end(),
            [](const PortfolioProject& project) {
                return project.principal_accounting_mode ==
                    PrincipalAccountingMode::ExplicitContractualLedger;
            });
    if (legacy_v01 && portfolio_has_explicit_ledger) {
        throw std::invalid_argument(
            "capital-stack v0.1 cannot consume explicit contractual principal ledgers because subscription cash, reserve cash, purchase price and asset principal are not yet separated");
    }
    if (legacy_v01) {
        require_true(
            stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero,
            "aggregate commitment fully funded at par at month zero assertion");
        if (stack
                .asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero ||
            stack.buyer_direct_costs_are_additional_pro_rata_calls ||
            stack.principal_base_cash_above_issued_principal_is_nonprincipal ||
            stack
                .principal_limit_capacity_difference_is_reported_without_valuation_claim) {
            throw std::invalid_argument(
                "capital-stack v0.1 cannot enable v0.2 asset-liability assertions");
        }
    } else {
        if (stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero) {
            throw std::invalid_argument(
                "capital-stack v0.2 must not conflate aggregate project outlay limits with the acquisition funding reserve");
        }
        require_true(stack
                         .asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero,
            "asset acquisition and primary-funding limit fully funded at par at month zero assertion");
        require_true(stack.buyer_direct_costs_are_additional_pro_rata_calls,
            "buyer direct costs additional pro-rata calls assertion");
        require_true(
            stack.principal_base_cash_above_issued_principal_is_nonprincipal,
            "principal-base cash above issued principal enters the non-principal waterfall assertion");
        require_true(stack
                         .principal_limit_capacity_difference_is_reported_without_valuation_claim,
            "principal-limit capacity difference reported without valuation claim assertion");
    }
    require_true(stack.subscription_reserve_is_zero_yield_and_lossless,
        "zero-yield lossless subscription reserve assertion");
    require_true(stack.undrawn_commitment_cancels_and_returns_only_at_horizon,
        "undrawn commitment horizon return assertion");
    require_true(stack.pool_costs_are_additional_pro_rata_calls,
        "pool costs additional pro-rata calls assertion");
    require_true(stack.principal_cash_is_paid_most_senior_first,
        "senior-first principal waterfall assertion");
    require_true(stack.nonprincipal_cash_is_paid_to_caps_then_residual,
        "priority-cap then residual non-principal waterfall assertion");
    require_true(stack.tranching_does_not_change_project_cash_or_gross_loss,
        "unchanged project cash and gross loss assertion");
    if (stack.premium_discount_or_fair_value_is_claimed) {
        throw std::invalid_argument(
            "capital-stack cannot claim premium, discount, or fair value");
    }
    if (!std::isfinite(stack.underlying_success_participation_fraction) ||
        stack.underlying_success_participation_fraction < 0.0 ||
        stack.underlying_success_participation_fraction > 1.0) {
        throw std::invalid_argument(
            "capital-stack participation fraction must lie in [0,1]");
    }
    if (!portfolio.loss_layers.empty()) {
        throw std::invalid_argument(
            "capital-stack requires an untranched underlying portfolio");
    }
    if (stack.tranches.size() < 2U ||
        stack.tranches.size() > kMaximumTranches) {
        throw std::invalid_argument(
            "capital-stack tranche count must be between two and 128");
    }
    const std::size_t scenario_count = portfolio.joint_scenarios.size();
    const std::size_t month_count = portfolio.horizon_months + 1U;
    if (scenario_count >
            kMaximumScenarioTrancheMonths / stack.tranches.size() ||
        scenario_count * stack.tranches.size() >
            kMaximumScenarioTrancheMonths / month_count) {
        throw std::invalid_argument(
            "capital-stack scenario-tranche-month work exceeds the resource bound");
    }

    const CapitalStackFundingBasis funding =
        capital_stack_funding_basis(portfolio, stack);
    const long double commitment =
        static_cast<long double>(funding.funded_reserve_limit_million);
    if (!(commitment > 0.0L)) {
        throw std::invalid_argument(
            "capital-stack funded reserve limit must be positive");
    }
    std::unordered_set<std::string> ids;
    ids.reserve(stack.tranches.size());
    long double expected_attachment = 0.0L;
    std::size_t residual_count = 0U;
    for (std::size_t index = 0U; index < stack.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& tranche = stack.tranches[index];
        require_safe_identifier(tranche.id, "capital-stack tranche id");
        if (!ids.emplace(tranche.id).second) {
            throw std::invalid_argument(
                "capital-stack tranche ids must be unique");
        }
        if (!std::isfinite(tranche.attachment_million) ||
            !std::isfinite(tranche.detachment_million) ||
            tranche.attachment_million < 0.0 ||
            tranche.detachment_million - tranche.attachment_million <
                kMinimumTrancheNotionalMillion ||
            exceeds_with_input_tolerance(
                static_cast<long double>(tranche.detachment_million),
                commitment)) {
            throw std::invalid_argument(
                "capital-stack tranche bounds must lie within the funded reserve limit and each notional must be at least one base currency unit");
        }
        if (static_cast<long double>(tranche.attachment_million) !=
            expected_attachment) {
            throw std::invalid_argument(
                "capital-stack tranches must be contiguous from zero");
        }
        expected_attachment =
            static_cast<long double>(tranche.detachment_million);
        if (!std::isfinite(tranche.priority_nonprincipal_cap_million) ||
            tranche.priority_nonprincipal_cap_million < 0.0) {
            throw std::invalid_argument(
                "capital-stack non-principal cap must be finite and non-negative");
        }
        if (!std::isfinite(tranche.annual_physical_hurdle_rate) ||
            tranche.annual_physical_hurdle_rate < 0.0 ||
            tranche.annual_physical_hurdle_rate > kMaximumAnnualHurdle) {
            throw std::invalid_argument(
                "capital-stack tranche hurdle is outside the supported range");
        }
        if (tranche.is_first_loss_residual) {
            ++residual_count;
            if (index != 0U || tranche.priority_nonprincipal_cap_million != 0.0) {
                throw std::invalid_argument(
                    "only the attachment-zero tranche may be the uncapped residual");
            }
        } else if (index == 0U) {
            throw std::invalid_argument(
                "the attachment-zero tranche must be the first-loss residual");
        }
    }
    if (residual_count != 1U) {
        throw std::invalid_argument(
            "capital-stack requires exactly one first-loss residual");
    }
    if (!near_input_money(expected_attachment, commitment)) {
        throw std::invalid_argument(
            "capital-stack tranches must end at the funded reserve limit");
    }

    std::unordered_map<std::string, PrincipalAccountingMode> accounting_modes;
    accounting_modes.reserve(portfolio.projects.size());
    for (const PortfolioProject& project : portfolio.projects) {
        accounting_modes.emplace(project.id, project.principal_accounting_mode);
    }
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        CompensatedSum scenario_funding_uses;
        CompensatedSum scenario_raw_principal;
        CompensatedSum scenario_effective_principal;
        for (const ProjectJointPath& path : scenario.project_paths) {
            const auto found = accounting_modes.find(path.project_id);
            if (found == accounting_modes.end()) {
                throw std::logic_error(
                    "capital-stack validation lost a configured project");
            }
            CompensatedSum path_funding_uses;
            CompensatedSum path_principal;
            path_funding_uses.add(path_acquisition_and_primary_funding(
                path, found->second));
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                path_principal.add(static_cast<long double>(
                    receipt.principal_component_million));
            }
            scenario_funding_uses.add(path_funding_uses.value());
            scenario_raw_principal.add(path_principal.value());
            scenario_effective_principal.add(std::min(
                path_principal.value(), path_funding_uses.value()));
        }
        if (exceeds_with_input_tolerance(
                scenario_funding_uses.value(), commitment)) {
            throw std::invalid_argument(
                "capital-stack aggregate scenario acquisition and primary-funding uses exceed the funded reserve");
        }
        if (legacy_v01 && !near_input_money(scenario_raw_principal.value(),
                scenario_effective_principal.value())) {
            throw std::invalid_argument(
                "capital-stack aggregate raw principal classification does not reconcile to effective returned principal");
        }
    }

    (void)apply_success_participation_fraction(portfolio, participation,
        stack.underlying_success_participation_fraction);
}

CapitalStackSummary evaluate_capital_stack(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack) {
    validate_capital_stack_config(portfolio, ambiguity, participation, stack);
    const PortfolioConfig selected = apply_success_participation_fraction(
        portfolio, participation,
        stack.underlying_success_participation_fraction);
    const PortfolioSummary underlying = evaluate_portfolio(selected);
    const CapitalStackFundingBasis funding =
        capital_stack_funding_basis(selected, stack);
    const double commitment = funding.funded_reserve_limit_million;

    CapitalStackSummary summary;
    summary.model_version = stack.model_version;
    summary.uses_explicit_asset_liability_accounting =
        funding.uses_explicit_asset_liability_accounting;
    summary.legacy_v01_loss_layering_metrics_are_applicable =
        !is_v02(stack);
    summary.underlying_success_participation_fraction =
        stack.underlying_success_participation_fraction;
    summary.aggregate_project_outlay_limit_million =
        funding.project_outlay_limit_million;
    summary.aggregate_contractual_asset_principal_limit_million =
        funding.contractual_asset_principal_limit_million;
    summary.aggregate_commitment_million = commitment;
    summary.model_limitation = is_v02(stack)
        ? "Synthetic physical-scenario asset-to-liability cash allocation only. Purchase/funding basis, contractual asset principal and issued tranche principal are separated, but no fair value, market spread, rating, legal enforceability, reserve custody risk, tax, regulatory capital, or empirical calibration is established."
        : "Synthetic physical-scenario cash allocation only. No fair value, market spread, rating, legal enforceability, reserve custody risk, tax, regulatory capital, or empirical calibration is established.";

    std::unordered_map<std::string, const JointScenario*> source_scenarios;
    source_scenarios.reserve(selected.joint_scenarios.size());
    for (const JointScenario& scenario : selected.joint_scenarios) {
        source_scenarios.emplace(scenario.id, &scenario);
    }
    summary.scenarios.reserve(underlying.scenarios.size());
    for (const JointScenarioResult& scenario : underlying.scenarios) {
        summary.scenarios.push_back(evaluate_scenario(selected, scenario,
            find_joint_scenario(source_scenarios, scenario.scenario_id),
            stack, commitment, summary));
    }

    const PortfolioAmbiguityProjector projector(selected, ambiguity);
    const auto underlying_npv = project_metric(projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.underlying_on_demand_npv_million;
        });
    summary.expected_underlying_on_demand_npv_million =
        underlying_npv.expectation;
    summary.underlying_target_worst_expected_npv_million =
        participation.target_worst_expected_npv_million;
    summary.selected_underlying_success_participation_meets_target =
        summary.expected_underlying_on_demand_npv_million.minimum.value >=
        summary.underlying_target_worst_expected_npv_million;
    summary.selected_underlying_target_gap_million = std::max(0.0,
        summary.underlying_target_worst_expected_npv_million -
            summary.expected_underlying_on_demand_npv_million.minimum.value);
    summary.scenario_probability_bounds =
        underlying_npv.scenario_probability_bounds;
    update_projection_error(
        underlying_npv.maximum_endpoint_probability_error, summary);

    const auto full_stack_npv = project_metric(projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.fully_funded_stack_npv_at_pool_hurdle_million;
        });
    summary.expected_fully_funded_stack_npv_at_pool_hurdle_million =
        full_stack_npv.expectation;
    update_projection_error(
        full_stack_npv.maximum_endpoint_probability_error, summary);
    const auto prefunding_drag = project_metric(projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.prefunding_drag_npv_million;
        });
    summary.expected_prefunding_drag_npv_million = prefunding_drag.expectation;
    update_projection_error(
        prefunding_drag.maximum_endpoint_probability_error, summary);
    const auto contractual_asset_loss = project_metric(
        projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.contractual_asset_principal_loss_million;
        });
    summary.expected_contractual_asset_principal_loss_million =
        contractual_asset_loss.expectation;
    update_projection_error(
        contractual_asset_loss.maximum_endpoint_probability_error, summary);
    const auto contractual_asset_outstanding = project_metric(
        projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.contractual_asset_outstanding_principal_million;
        });
    summary.expected_contractual_asset_outstanding_principal_million =
        contractual_asset_outstanding.expectation;
    update_projection_error(
        contractual_asset_outstanding.maximum_endpoint_probability_error,
        summary);
    const auto issued_principal_shortfall = project_metric(
        projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.issued_principal_cash_shortfall_million;
        });
    summary.expected_issued_principal_cash_shortfall_million =
        issued_principal_shortfall.expectation;
    update_projection_error(
        issued_principal_shortfall.maximum_endpoint_probability_error,
        summary);

    summary.tranches.reserve(stack.tranches.size());
    for (std::size_t index = 0U; index < stack.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& term = stack.tranches[index];
        CapitalStackTrancheSummary tranche;
        tranche.tranche_id = term.id;
        tranche.attachment_million = term.attachment_million;
        tranche.detachment_million = term.detachment_million;
        tranche.notional_million =
            term.detachment_million - term.attachment_million;
        tranche.priority_nonprincipal_cap_million =
            term.priority_nonprincipal_cap_million;
        tranche.annual_physical_hurdle_rate =
            term.annual_physical_hurdle_rate;
        tranche.is_first_loss_residual = term.is_first_loss_residual;
        tranche.legacy_v01_loss_layering_metrics_are_applicable =
            summary.legacy_v01_loss_layering_metrics_are_applicable;

        const auto project = [&](auto selector) {
            return project_metric(projector, summary.scenarios,
                [index, selector](const CapitalStackScenarioResult& scenario) {
                    return selector(scenario.tranches[index]);
                });
        };
        const auto assign = [&](AmbiguityMetricRange& destination,
                                const auto& projection) {
            destination = projection.expectation;
            update_projection_error(
                projection.maximum_endpoint_probability_error, summary);
        };

        assign(tranche.expected_contributions_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.total_contributions_million;
            }));
        assign(tranche.expected_buyer_direct_cost_calls_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.pro_rata_buyer_direct_cost_calls_million;
            }));
        assign(tranche.expected_underlying_principal_cash_distribution_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.underlying_principal_cash_distribution_million;
            }));
        assign(tranche.expected_unused_reserve_principal_return_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.unused_reserve_principal_return_million;
            }));
        assign(tranche.expected_principal_cash_distribution_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.principal_cash_distribution_million;
            }));
        assign(tranche.expected_nonprincipal_cash_distribution_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.nonprincipal_cash_distribution_million;
            }));
        assign(tranche.expected_total_distributions_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.total_distributions_million;
            }));
        if (tranche.legacy_v01_loss_layering_metrics_are_applicable) {
            assign(tranche.expected_realized_principal_loss_million,
                project([](const CapitalStackTrancheScenarioResult& value) {
                    return value.realized_principal_loss_million;
                }));
            assign(tranche.expected_realized_principal_loss_fraction,
                project([](const CapitalStackTrancheScenarioResult& value) {
                    return value.realized_principal_loss_million /
                        value.notional_million;
                }));
            assign(tranche.expected_unresolved_principal_exposure_million,
                project([](const CapitalStackTrancheScenarioResult& value) {
                    return value.unresolved_principal_exposure_million;
                }));
        }
        assign(tranche.expected_principal_cash_shortfall_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.principal_cash_shortfall_million;
            }));
        assign(tranche.expected_npv_at_tranche_hurdle_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.npv_at_tranche_hurdle_million;
            }));
        assign(tranche.expected_all_in_cash_shortfall_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.all_in_cash_shortfall_million;
            }));
        assign(tranche.expected_scenario_cash_multiple,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.cash_multiple;
            }));
        assign(tranche.expected_scenario_net_return_fraction,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.net_return_fraction;
            }));
        if (tranche.legacy_v01_loss_layering_metrics_are_applicable) {
            assign(tranche.principal_impairment_probability,
                project([](const CapitalStackTrancheScenarioResult& value) {
                    return is_material_positive(
                               value.realized_principal_loss_million,
                               value.notional_million)
                        ? 1.0
                        : 0.0;
                }));
            assign(tranche.principal_exhaustion_probability,
                project([](const CapitalStackTrancheScenarioResult& value) {
                    return value.realized_principal_loss_million >=
                            value.notional_million -
                                reconciliation_tolerance(
                                    value.notional_million)
                        ? 1.0
                        : 0.0;
                }));
        }
        assign(tranche.principal_cash_shortfall_probability,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return is_material_positive(
                           value.principal_cash_shortfall_million,
                           value.notional_million)
                    ? 1.0
                    : 0.0;
            }));
        assign(tranche.full_principal_cash_shortfall_probability,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.principal_cash_shortfall_million >=
                        value.notional_million -
                            reconciliation_tolerance(value.notional_million)
                    ? 1.0
                    : 0.0;
            }));
        assign(tranche.negative_npv_probability,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return is_material_negative(value.npv_at_tranche_hurdle_million,
                           value.total_contributions_million)
                    ? 1.0
                    : 0.0;
            }));

        if (tranche.legacy_v01_loss_layering_metrics_are_applicable) {
            const auto loss_es95 = project_es(projector, summary.scenarios,
                [index](const CapitalStackScenarioResult& scenario) {
                    return scenario.tranches[index]
                        .realized_principal_loss_million;
                },
                0.05);
            tranche.principal_loss_expected_shortfall_95_million =
                loss_es95.upper_expected_shortfall;
            update_projection_error(
                loss_es95.maximum_endpoint_probability_error, summary);
            const auto loss_es99 = project_es(projector, summary.scenarios,
                [index](const CapitalStackScenarioResult& scenario) {
                    return scenario.tranches[index]
                        .realized_principal_loss_million;
                },
                0.01);
            tranche.principal_loss_expected_shortfall_99_million =
                loss_es99.upper_expected_shortfall;
            update_projection_error(
                loss_es99.maximum_endpoint_probability_error, summary);
        }
        const auto principal_cash_shortfall_es95 = project_es(
            projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return scenario.tranches[index]
                    .principal_cash_shortfall_million;
            },
            0.05);
        tranche.principal_cash_shortfall_expected_shortfall_95_million =
            principal_cash_shortfall_es95.upper_expected_shortfall;
        update_projection_error(
            principal_cash_shortfall_es95.maximum_endpoint_probability_error,
            summary);
        const auto principal_cash_shortfall_es99 = project_es(
            projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return scenario.tranches[index]
                    .principal_cash_shortfall_million;
            },
            0.01);
        tranche.principal_cash_shortfall_expected_shortfall_99_million =
            principal_cash_shortfall_es99.upper_expected_shortfall;
        update_projection_error(
            principal_cash_shortfall_es99.maximum_endpoint_probability_error,
            summary);
        const auto npv_es95 = project_es(projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return std::max(0.0,
                    -scenario.tranches[index].npv_at_tranche_hurdle_million);
            },
            0.05);
        tranche.npv_shortfall_expected_shortfall_95_million =
            npv_es95.upper_expected_shortfall;
        update_projection_error(
            npv_es95.maximum_endpoint_probability_error, summary);
        const auto npv_es99 = project_es(projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return std::max(0.0,
                    -scenario.tranches[index].npv_at_tranche_hurdle_million);
            },
            0.01);
        tranche.npv_shortfall_expected_shortfall_99_million =
            npv_es99.upper_expected_shortfall;
        update_projection_error(
            npv_es99.maximum_endpoint_probability_error, summary);

        const WalSolveResult wal = solve_principal_wal(projector,
            summary.scenarios, summary.scenario_probability_bounds, index,
            static_cast<double>(selected.horizon_months) / 12.0, summary);
        tranche.principal_cash_weighted_average_life_years = wal.range;
        summary.maximum_wal_ratio_objective_residual_million_years =
            std::max(
                summary.maximum_wal_ratio_objective_residual_million_years,
                wal.maximum_objective_residual_million_years);

        tranche.central_expected_npv_meets_hurdle =
            tranche.expected_npv_at_tranche_hurdle_million.central >= 0.0;
        tranche.robust_expected_npv_meets_hurdle =
            tranche.expected_npv_at_tranche_hurdle_million.minimum.value >= 0.0;
        summary.tranches.push_back(std::move(tranche));
    }

    return summary;
}

} // namespace naturalehia::cellular_finance
