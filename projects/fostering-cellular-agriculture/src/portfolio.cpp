// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kCashSourceCount = 8U;
constexpr std::size_t kMaximumProjects = 128U;
constexpr std::size_t kMaximumScenarios = 10'000U;
constexpr std::size_t kMaximumScenarioCashSources = 256U;
// Covers the staged-capital engine's maximum 1,200-month scheduled path plus
// its maximum 1,200-month delayed recovery. Aggregate month-work guards below
// still bound accepted portfolio dimensions.
constexpr std::size_t kMaximumHorizonMonths = 2'400U;
constexpr std::size_t kMaximumFactorTags = 64U;
constexpr std::size_t kMaximumLayers = 128U;
constexpr std::size_t kMaximumProjectScenarioPairs = 500'000U;
constexpr std::size_t kMaximumScenarioMonths = 2'000'000U;
constexpr std::size_t kMaximumProjectScenarioMonths = 2'000'000U;
constexpr std::size_t kMaximumCashRecords = 2'000'000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMaximumAmountMillion = 1.0e6;
constexpr double kMaximumAnnualHurdle = 10.0;
constexpr double kWeightTolerance = 1.0e-12;
constexpr long double kInputMoneyAbsoluteTolerance = 1.0e-10L;

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

struct WeightedValue {
    double value{0.0};
    double weight{0.0};
};

[[nodiscard]] bool is_ascii_alpha_numeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alpha_numeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alpha_numeric(character) || character == '-' ||
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
    if (value.empty() || value.size() > kMaximumTextLength) {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    for (const unsigned char character : value) {
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

void require_nonnegative_amount(
    double amount, std::string_view description) {
    if (!std::isfinite(amount) || amount < 0.0 ||
        amount > kMaximumAmountMillion) {
        throw std::invalid_argument(
            std::string(description) +
            " must be finite, non-negative, and no greater than 1e6 million");
    }
}

[[nodiscard]] bool valid_project_stage(ProjectStage stage) noexcept {
    switch (stage) {
    case ProjectStage::Research:
    case ProjectStage::Pilot:
    case ProjectStage::Demonstration:
    case ProjectStage::FirstIndustrial:
    case ProjectStage::RepeatProduction:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_resolution(ProjectPathResolution resolution) noexcept {
    switch (resolution) {
    case ProjectPathResolution::Resolved:
    case ProjectPathResolution::Continuing:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_principal_accounting_mode(
    PrincipalAccountingMode mode) noexcept {
    switch (mode) {
    case PrincipalAccountingMode::DrawEqualsPrincipalLegacy:
    case PrincipalAccountingMode::ExplicitContractualLedger:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_principal_movement_kind(
    PrincipalMovementKind kind) noexcept {
    switch (kind) {
    case PrincipalMovementKind::FundedPrincipalAddition:
    case PrincipalMovementKind::CapitalizedFeeAddition:
    case PrincipalMovementKind::CapitalizedInterestAddition:
    case PrincipalMovementKind::ConversionExtinguishment:
    case PrincipalMovementKind::Writeoff:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_investor_outlay_purpose(
    InvestorOutlayPurpose purpose) noexcept {
    switch (purpose) {
    case InvestorOutlayPurpose::PrimaryProjectFunding:
    case InvestorOutlayPurpose::ClaimPurchasePrice:
    case InvestorOutlayPurpose::BuyerDirectCost:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_cash_source(PortfolioCashSource source) noexcept {
    switch (source) {
    case PortfolioCashSource::Commercial:
    case PortfolioCashSource::LicensingRoyalty:
    case PortfolioCashSource::ExitSale:
    case PortfolioCashSource::Recovery:
    case PortfolioCashSource::Refinancing:
    case PortfolioCashSource::ExplicitSupport:
    case PortfolioCashSource::SponsorFee:
    case PortfolioCashSource::FinancingFee:
        return true;
    }
    return false;
}

[[nodiscard]] std::size_t source_index(PortfolioCashSource source) {
    if (!valid_cash_source(source)) {
        throw std::invalid_argument("cash source is outside the defined taxonomy");
    }
    return static_cast<std::size_t>(source);
}

[[nodiscard]] long double input_money_tolerance(
    long double first, long double second) noexcept {
    const long double scale =
        std::max({1.0L, std::abs(first), std::abs(second)});
    return kInputMoneyAbsoluteTolerance + 8.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        scale;
}

[[nodiscard]] bool near_input_money(
    long double first, long double second) noexcept {
    return std::abs(first - second) <= input_money_tolerance(first, second);
}

[[nodiscard]] bool exceeds_with_input_tolerance(
    long double value, long double limit) noexcept {
    return value > limit + input_money_tolerance(value, limit);
}

[[nodiscard]] double to_double(long double value) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error("portfolio aggregation exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] double reconciliation_tolerance(double scale) noexcept {
    return 1.0e-9 + 16.0 * std::numeric_limits<double>::epsilon() *
        std::max(1.0, std::abs(scale));
}

[[nodiscard]] double checked_diversification_benefit(
    double standalone_sum, double pooled_tail, std::string_view label) {
    const double benefit = standalone_sum - pooled_tail;
    const double tolerance = reconciliation_tolerance(
        std::max(standalone_sum, pooled_tail));
    if (benefit < -tolerance) {
        throw std::logic_error(
            std::string(label) +
            " diversification benefit is materially negative");
    }
    return std::abs(benefit) <= tolerance ? 0.0 : benefit;
}

void enforce_reconciliation(
    double error, double scale, std::string_view message) {
    if (!std::isfinite(error) || error > reconciliation_tolerance(scale)) {
        throw std::logic_error(std::string(message));
    }
}

void add_record_count(std::size_t& current, std::size_t additional) {
    if (additional > kMaximumCashRecords - current) {
        throw std::invalid_argument(
            "aggregate cash record count exceeds the resource bound");
    }
    current += additional;
}

[[nodiscard]] long double stable_weight_sum(
    const std::vector<WeightedValue>& values) noexcept {
    CompensatedSum sum;
    for (const WeightedValue& item : values) {
        sum.add(static_cast<long double>(item.weight));
    }
    return sum.value();
}

[[nodiscard]] long double stable_weight_sum(
    const std::vector<double>& weights) noexcept {
    CompensatedSum sum;
    for (const double weight : weights) {
        sum.add(static_cast<long double>(weight));
    }
    return sum.value();
}

[[nodiscard]] double weighted_quantile(
    const std::vector<WeightedValue>& values, double probability) {
    std::vector<WeightedValue> ordered = values;
    std::sort(ordered.begin(), ordered.end(),
        [](const WeightedValue& left, const WeightedValue& right) {
            if (left.value != right.value) {
                return left.value < right.value;
            }
            return left.weight < right.weight;
        });
    const long double total_weight = stable_weight_sum(ordered);
    const long double target =
        static_cast<long double>(probability) * total_weight;
    CompensatedSum cumulative;
    for (const WeightedValue& item : ordered) {
        cumulative.add(static_cast<long double>(item.weight));
        if (cumulative.value() >= target) {
            return item.value;
        }
    }
    return ordered.back().value;
}

[[nodiscard]] std::vector<double> upper_tail_fractions(
    const std::vector<WeightedValue>& values, double tail_probability) {
    const long double total_weight = stable_weight_sum(values);
    const long double requested =
        static_cast<long double>(tail_probability) * total_weight;
    const double threshold =
        weighted_quantile(values, 1.0 - tail_probability);
    CompensatedSum weight_above;
    CompensatedSum weight_at;
    for (const WeightedValue& item : values) {
        if (item.value > threshold) {
            weight_above.add(static_cast<long double>(item.weight));
        } else if (item.value == threshold) {
            weight_at.add(static_cast<long double>(item.weight));
        }
    }
    const long double at_weight = weight_at.value();
    const long double remaining = std::clamp(
        requested - weight_above.value(), 0.0L, at_weight);
    const double boundary_fraction = at_weight > 0.0L
        ? static_cast<double>(remaining / at_weight)
        : 0.0;

    std::vector<double> fractions;
    fractions.reserve(values.size());
    for (const WeightedValue& item : values) {
        fractions.push_back(item.value > threshold
                ? 1.0
                : (item.value == threshold ? boundary_fraction : 0.0));
    }
    return fractions;
}

[[nodiscard]] double upper_expected_shortfall(
    const std::vector<WeightedValue>& values, double tail_probability) {
    const long double requested =
        static_cast<long double>(tail_probability) * stable_weight_sum(values);
    const std::vector<double> fractions =
        upper_tail_fractions(values, tail_probability);
    CompensatedSum weighted_tail;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        weighted_tail.add(static_cast<long double>(values[index].weight) *
            static_cast<long double>(fractions[index]) *
            static_cast<long double>(values[index].value));
    }
    return to_double(weighted_tail.value() / requested);
}

[[nodiscard]] PortfolioDistributionSummary summarize_distribution(
    const std::vector<double>& values, const std::vector<double>& weights) {
    if (values.empty() || values.size() != weights.size()) {
        throw std::logic_error("invalid internal weighted distribution");
    }
    std::vector<WeightedValue> weighted_values;
    weighted_values.reserve(values.size());
    for (std::size_t index = 0U; index < values.size(); ++index) {
        weighted_values.push_back(WeightedValue{values[index], weights[index]});
    }
    std::sort(weighted_values.begin(), weighted_values.end(),
        [](const WeightedValue& left, const WeightedValue& right) {
            if (left.value != right.value) {
                return left.value < right.value;
            }
            return left.weight < right.weight;
        });
    const long double total_weight = stable_weight_sum(weighted_values);
    CompensatedSum weighted_sum;
    for (const WeightedValue& item : weighted_values) {
        weighted_sum.add(static_cast<long double>(item.weight) *
            static_cast<long double>(item.value));
    }
    const long double mean = weighted_sum.value() / total_weight;
    CompensatedSum weighted_variance;
    for (const WeightedValue& item : weighted_values) {
        const long double difference =
            static_cast<long double>(item.value) - mean;
        weighted_variance.add(static_cast<long double>(item.weight) *
            difference * difference);
    }

    PortfolioDistributionSummary summary;
    summary.mean = to_double(mean);
    summary.standard_deviation = std::sqrt(std::max(
        0.0, to_double(weighted_variance.value() / total_weight)));
    summary.p50 = weighted_quantile(weighted_values, 0.50);
    summary.p95 = weighted_quantile(weighted_values, 0.95);
    summary.p99 = weighted_quantile(weighted_values, 0.99);
    const auto maximum_supported = std::find_if(
        weighted_values.rbegin(), weighted_values.rend(),
        [](const WeightedValue& item) { return item.weight > 0.0; });
    if (maximum_supported == weighted_values.rend()) {
        throw std::logic_error(
            "weighted distribution has no positive-probability atom");
    }
    summary.maximum = maximum_supported->value;
    summary.expected_shortfall_95 =
        upper_expected_shortfall(weighted_values, 0.05);
    summary.expected_shortfall_99 =
        upper_expected_shortfall(weighted_values, 0.01);
    return summary;
}

[[nodiscard]] std::vector<ReturnSourceTotal> empty_source_totals() {
    std::vector<ReturnSourceTotal> totals;
    totals.reserve(kCashSourceCount);
    for (std::size_t index = 0U; index < kCashSourceCount; ++index) {
        totals.push_back(ReturnSourceTotal{
            static_cast<PortfolioCashSource>(index), 0.0, 0.0});
    }
    return totals;
}

[[nodiscard]] double sum_nominal_sources(
    const std::vector<ReturnSourceTotal>& totals) {
    CompensatedSum result;
    for (const ReturnSourceTotal& total : totals) {
        result.add(static_cast<long double>(total.nominal_million));
    }
    return to_double(result.value());
}

[[nodiscard]] double absolute_difference(double left, double right) noexcept {
    return std::abs(left - right);
}

[[nodiscard]] double layer_loss(
    double pool_loss, const LossLayer& layer) noexcept {
    const double width = layer.detachment_million - layer.attachment_million;
    return std::clamp(
        pool_loss - layer.attachment_million, 0.0, width);
}

[[nodiscard]] std::vector<const MonthlyAmount*> sorted_monthly_amounts(
    const std::vector<MonthlyAmount>& amounts) {
    std::vector<const MonthlyAmount*> ordered;
    ordered.reserve(amounts.size());
    for (const MonthlyAmount& amount : amounts) {
        ordered.push_back(&amount);
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const MonthlyAmount* left, const MonthlyAmount* right) {
            if (left->month != right->month) {
                return left->month < right->month;
            }
            return left->amount_million < right->amount_million;
        });
    return ordered;
}

[[nodiscard]] std::vector<const InvestorReceipt*> sorted_receipts(
    const std::vector<InvestorReceipt>& receipts) {
    std::vector<const InvestorReceipt*> ordered;
    ordered.reserve(receipts.size());
    for (const InvestorReceipt& receipt : receipts) {
        ordered.push_back(&receipt);
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const InvestorReceipt* left, const InvestorReceipt* right) {
            if (left->month != right->month) {
                return left->month < right->month;
            }
            if (left->cash_source_id != right->cash_source_id) {
                return left->cash_source_id < right->cash_source_id;
            }
            if (left->amount_million != right->amount_million) {
                return left->amount_million < right->amount_million;
            }
            return left->principal_component_million <
                right->principal_component_million;
        });
    return ordered;
}

} // namespace

std::string_view to_string(ProjectStage stage) noexcept {
    switch (stage) {
    case ProjectStage::Research:
        return "research";
    case ProjectStage::Pilot:
        return "pilot";
    case ProjectStage::Demonstration:
        return "demonstration";
    case ProjectStage::FirstIndustrial:
        return "first-industrial";
    case ProjectStage::RepeatProduction:
        return "repeat-production";
    }
    return "unknown";
}

std::string_view to_string(ProjectPathResolution resolution) noexcept {
    switch (resolution) {
    case ProjectPathResolution::Resolved:
        return "resolved";
    case ProjectPathResolution::Continuing:
        return "continuing";
    }
    return "unknown";
}

std::string_view to_string(PrincipalAccountingMode mode) noexcept {
    switch (mode) {
    case PrincipalAccountingMode::DrawEqualsPrincipalLegacy:
        return "draw-equals-principal-legacy";
    case PrincipalAccountingMode::ExplicitContractualLedger:
        return "explicit-contractual-ledger";
    }
    return "unknown";
}

std::string_view to_string(PrincipalMovementKind kind) noexcept {
    switch (kind) {
    case PrincipalMovementKind::FundedPrincipalAddition:
        return "funded-principal-addition";
    case PrincipalMovementKind::CapitalizedFeeAddition:
        return "capitalized-fee-addition";
    case PrincipalMovementKind::CapitalizedInterestAddition:
        return "capitalized-interest-addition";
    case PrincipalMovementKind::ConversionExtinguishment:
        return "conversion-extinguishment";
    case PrincipalMovementKind::Writeoff:
        return "writeoff";
    }
    return "unknown";
}

std::string_view to_string(InvestorOutlayPurpose purpose) noexcept {
    switch (purpose) {
    case InvestorOutlayPurpose::PrimaryProjectFunding:
        return "primary-project-funding";
    case InvestorOutlayPurpose::ClaimPurchasePrice:
        return "claim-purchase-price";
    case InvestorOutlayPurpose::BuyerDirectCost:
        return "buyer-direct-cost";
    }
    return "unknown";
}

std::string_view to_string(PortfolioCashSource source) noexcept {
    switch (source) {
    case PortfolioCashSource::Commercial:
        return "commercial";
    case PortfolioCashSource::LicensingRoyalty:
        return "licensing-royalty";
    case PortfolioCashSource::ExitSale:
        return "exit-sale";
    case PortfolioCashSource::Recovery:
        return "recovery";
    case PortfolioCashSource::Refinancing:
        return "refinancing";
    case PortfolioCashSource::ExplicitSupport:
        return "explicit-support";
    case PortfolioCashSource::SponsorFee:
        return "sponsor-fee";
    case PortfolioCashSource::FinancingFee:
        return "financing-fee";
    }
    return "unknown";
}

double portfolio_reference_principal_limit(
    const PortfolioProject& project) {
    switch (project.principal_accounting_mode) {
    case PrincipalAccountingMode::DrawEqualsPrincipalLegacy:
        return project.commitment_million;
    case PrincipalAccountingMode::ExplicitContractualLedger:
        return project.principal_limit_million;
    }
    throw std::invalid_argument(
        "project principal accounting mode is outside the taxonomy");
}

double portfolio_aggregate_reference_principal(
    const PortfolioConfig& config) {
    CompensatedSum total;
    for (const PortfolioProject& project : config.projects) {
        total.add(static_cast<long double>(
            portfolio_reference_principal_limit(project)));
    }
    return to_double(total.value());
}

void validate_portfolio_config(const PortfolioConfig& config) {
    const bool legacy_model =
        config.model_version == kPortfolioLegacyModelVersion;
    if (!legacy_model && config.model_version != kPortfolioModelVersion) {
        throw std::invalid_argument(
            "portfolio model_version does not match this engine");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "portfolio accepts synthetic inputs only at this implementation boundary");
    }
    require_safe_text(config.scenario_label, "scenario_label");
    require_safe_text(config.source_note, "source_note");
    require_safe_text(config.currency_label, "currency_label");
    require_safe_text(config.monetary_basis, "monetary_basis");
    if (config.horizon_months == 0U ||
        config.horizon_months > kMaximumHorizonMonths) {
        throw std::invalid_argument("horizon_months is outside the resource bound");
    }
    if (!std::isfinite(config.annual_physical_hurdle_rate) ||
        config.annual_physical_hurdle_rate < 0.0 ||
        config.annual_physical_hurdle_rate > kMaximumAnnualHurdle) {
        throw std::invalid_argument(
            "annual_physical_hurdle_rate is outside the supported range");
    }
    if (config.projects.empty() ||
        config.projects.size() > kMaximumProjects) {
        throw std::invalid_argument("projects must be non-empty and bounded");
    }
    if (config.joint_scenarios.empty() ||
        config.joint_scenarios.size() > kMaximumScenarios) {
        throw std::invalid_argument(
            "joint_scenarios must be non-empty and bounded");
    }
    if (config.projects.size() >
        kMaximumProjectScenarioPairs / config.joint_scenarios.size()) {
        throw std::invalid_argument(
            "aggregate project-scenario count exceeds the resource bound");
    }
    const std::size_t month_count = config.horizon_months + 1U;
    if (month_count >
        kMaximumScenarioMonths / config.joint_scenarios.size()) {
        throw std::invalid_argument(
            "aggregate scenario-month count exceeds the resource bound");
    }
    const std::size_t project_scenario_pairs =
        config.projects.size() * config.joint_scenarios.size();
    if (month_count >
        kMaximumProjectScenarioMonths / project_scenario_pairs) {
        throw std::invalid_argument(
            "aggregate project-scenario-month work exceeds the resource bound");
    }

    std::unordered_map<std::string, std::size_t> project_indices;
    project_indices.reserve(config.projects.size());
    CompensatedSum aggregate_reference_principal_sum;
    for (std::size_t index = 0U; index < config.projects.size(); ++index) {
        const PortfolioProject& project = config.projects[index];
        require_safe_identifier(project.id, "project id");
        if (!valid_project_stage(project.stage)) {
            throw std::invalid_argument("project stage is outside the taxonomy");
        }
        require_nonnegative_amount(
            project.commitment_million, "project commitment");
        if (project.commitment_million == 0.0) {
            throw std::invalid_argument("project commitment must be positive");
        }
        if (!valid_principal_accounting_mode(
                project.principal_accounting_mode)) {
            throw std::invalid_argument(
                "project principal accounting mode is outside the taxonomy");
        }
        require_nonnegative_amount(
            project.principal_limit_million, "project principal limit");
        require_nonnegative_amount(
            project.opening_principal_million, "project opening principal");
        if (legacy_model && project.principal_accounting_mode !=
                PrincipalAccountingMode::DrawEqualsPrincipalLegacy) {
            throw std::invalid_argument(
                "portfolio v0.1 cannot encode explicit contractual principal accounting");
        }
        if (project.principal_accounting_mode ==
            PrincipalAccountingMode::DrawEqualsPrincipalLegacy) {
            if (project.principal_limit_million != 0.0 ||
                project.opening_principal_million != 0.0) {
                throw std::invalid_argument(
                    "legacy principal accounting requires zero explicit principal fields");
            }
        } else {
            if (project.principal_limit_million == 0.0) {
                throw std::invalid_argument(
                    "explicit contractual principal accounting requires a positive principal limit");
            }
            if (exceeds_with_input_tolerance(
                    static_cast<long double>(
                        project.opening_principal_million),
                    static_cast<long double>(
                        project.principal_limit_million))) {
                throw std::invalid_argument(
                    "project opening principal exceeds its principal limit");
            }
        }
        if (!project_indices.emplace(project.id, index).second) {
            throw std::invalid_argument("project ids must be unique");
        }
        aggregate_reference_principal_sum.add(static_cast<long double>(
            portfolio_reference_principal_limit(project)));
    }
    const long double aggregate_reference_principal =
        aggregate_reference_principal_sum.value();

    std::unordered_set<std::string> scenario_ids;
    scenario_ids.reserve(config.joint_scenarios.size());
    std::vector<const JointScenario*> ordered_scenarios;
    ordered_scenarios.reserve(config.joint_scenarios.size());
    for (const JointScenario& scenario : config.joint_scenarios) {
        require_safe_identifier(scenario.id, "joint scenario id");
        if (!scenario_ids.emplace(scenario.id).second) {
            throw std::invalid_argument("joint scenario ids must be unique");
        }
        ordered_scenarios.push_back(&scenario);
    }
    std::sort(ordered_scenarios.begin(), ordered_scenarios.end(),
        [](const JointScenario* left, const JointScenario* right) {
            return left->id < right->id;
        });

    CompensatedSum weight_sum;
    std::size_t cash_record_count = 0U;
    for (const JointScenario* scenario_pointer : ordered_scenarios) {
        const JointScenario& scenario = *scenario_pointer;
        if (!std::isfinite(scenario.weight) || scenario.weight < 0.0) {
            throw std::invalid_argument(
                "joint scenario weights must be finite and non-negative");
        }
        weight_sum.add(static_cast<long double>(scenario.weight));
        if (scenario.project_paths.size() != config.projects.size()) {
            throw std::invalid_argument(
                "each joint scenario must contain every project exactly once");
        }
        if (scenario.cash_sources.size() > kMaximumScenarioCashSources) {
            throw std::invalid_argument(
                "scenario cash source count exceeds the resource bound");
        }
        if (scenario.factor_tags.size() > kMaximumFactorTags) {
            throw std::invalid_argument("factor tag count exceeds the resource bound");
        }

        std::unordered_set<std::string> factor_tags;
        factor_tags.reserve(scenario.factor_tags.size());
        for (const std::string& tag : scenario.factor_tags) {
            require_safe_identifier(tag, "factor tag");
            if (!factor_tags.emplace(tag).second) {
                throw std::invalid_argument(
                    "factor tags must be unique within a scenario");
            }
        }

        std::unordered_map<std::string, std::size_t> cash_source_indices;
        cash_source_indices.reserve(scenario.cash_sources.size());
        for (std::size_t source_position = 0U;
             source_position < scenario.cash_sources.size(); ++source_position) {
            const ScenarioCashSource& source =
                scenario.cash_sources[source_position];
            require_safe_identifier(source.id, "scenario cash source id");
            if (!valid_cash_source(source.kind)) {
                throw std::invalid_argument(
                    "scenario cash source kind is outside the taxonomy");
            }
            if (!cash_source_indices.emplace(source.id, source_position).second) {
                throw std::invalid_argument(
                    "scenario cash source ids must be unique");
            }
            add_record_count(cash_record_count, source.cash_available.size());
            for (const MonthlyAmount& available : source.cash_available) {
                if (available.month > config.horizon_months) {
                    throw std::invalid_argument(
                        "cash availability falls outside the analysis horizon");
                }
                require_nonnegative_amount(
                    available.amount_million, "cash availability");
            }
        }

        std::vector<std::vector<const InvestorReceipt*>> receipts_by_source(
            scenario.cash_sources.size());
        std::vector<bool> project_seen(config.projects.size(), false);
        for (const ProjectJointPath& path : scenario.project_paths) {
            require_safe_identifier(path.project_id, "path project id");
            if (!valid_resolution(path.resolution)) {
                throw std::invalid_argument(
                    "project path resolution is outside the taxonomy");
            }
            const auto project_position = project_indices.find(path.project_id);
            if (project_position == project_indices.end()) {
                throw std::invalid_argument(
                    "joint scenario path names an unknown project");
            }
            const std::size_t project_index = project_position->second;
            if (project_seen[project_index]) {
                throw std::invalid_argument(
                    "joint scenario contains a duplicate project path");
            }
            project_seen[project_index] = true;
            add_record_count(cash_record_count, path.capital_draws.size());
            add_record_count(cash_record_count, path.investor_outlays.size());
            add_record_count(cash_record_count, path.investor_receipts.size());
            add_record_count(
                cash_record_count, path.principal_movements.size());

            std::vector<CompensatedSum> monthly_draws(month_count);
            std::vector<CompensatedSum> monthly_principal_cash(month_count);
            std::vector<CompensatedSum> monthly_principal_additions(
                month_count);
            std::vector<CompensatedSum> monthly_principal_conversions(
                month_count);
            std::vector<CompensatedSum> monthly_principal_writeoffs(
                month_count);
            for (const MonthlyAmount& draw : path.capital_draws) {
                if (draw.month > config.horizon_months) {
                    throw std::invalid_argument(
                        "capital draw falls outside the analysis horizon");
                }
                require_nonnegative_amount(draw.amount_million, "capital draw");
                monthly_draws[draw.month].add(
                    static_cast<long double>(draw.amount_million));
            }
            const PortfolioProject& project = config.projects[project_index];
            if (project.principal_accounting_mode ==
                    PrincipalAccountingMode::DrawEqualsPrincipalLegacy &&
                !path.investor_outlays.empty()) {
                throw std::invalid_argument(
                    "legacy principal accounting prohibits classified investor outlays");
            }
            if (project.principal_accounting_mode ==
                    PrincipalAccountingMode::ExplicitContractualLedger &&
                !path.capital_draws.empty()) {
                throw std::invalid_argument(
                    "explicit contractual principal accounting requires classified investor outlays rather than legacy draws");
            }
            for (const InvestorOutlay& outlay : path.investor_outlays) {
                if (outlay.month > config.horizon_months) {
                    throw std::invalid_argument(
                        "investor outlay falls outside the analysis horizon");
                }
                if (!valid_investor_outlay_purpose(outlay.purpose)) {
                    throw std::invalid_argument(
                        "investor outlay purpose is outside the taxonomy");
                }
                require_nonnegative_amount(
                    outlay.amount_million, "investor outlay");
                monthly_draws[outlay.month].add(
                    static_cast<long double>(outlay.amount_million));
            }
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                if (receipt.month > config.horizon_months) {
                    throw std::invalid_argument(
                        "investor receipt falls outside the analysis horizon");
                }
                require_safe_identifier(
                    receipt.cash_source_id, "receipt cash_source_id");
                const auto source_position =
                    cash_source_indices.find(receipt.cash_source_id);
                if (source_position == cash_source_indices.end()) {
                    throw std::invalid_argument(
                        "investor receipt references an unknown scenario cash source");
                }
                require_nonnegative_amount(
                    receipt.amount_million, "investor receipt");
                require_nonnegative_amount(receipt.principal_component_million,
                    "receipt principal component");
                if (receipt.principal_component_million >
                    receipt.amount_million) {
                    throw std::invalid_argument(
                        "receipt principal component exceeds receipt cash");
                }
                monthly_principal_cash[receipt.month].add(
                    static_cast<long double>(
                        receipt.principal_component_million));
                receipts_by_source[source_position->second].push_back(&receipt);
            }

            if (project.principal_accounting_mode ==
                    PrincipalAccountingMode::DrawEqualsPrincipalLegacy &&
                !path.principal_movements.empty()) {
                throw std::invalid_argument(
                    "legacy principal accounting prohibits explicit principal movements");
            }
            for (const PrincipalMovement& movement :
                 path.principal_movements) {
                if (movement.month > config.horizon_months) {
                    throw std::invalid_argument(
                        "principal movement falls outside the analysis horizon");
                }
                if (!valid_principal_movement_kind(movement.kind)) {
                    throw std::invalid_argument(
                        "principal movement kind is outside the taxonomy");
                }
                require_nonnegative_amount(
                    movement.amount_million, "principal movement");
                CompensatedSum* destination = nullptr;
                switch (movement.kind) {
                case PrincipalMovementKind::FundedPrincipalAddition:
                case PrincipalMovementKind::CapitalizedFeeAddition:
                case PrincipalMovementKind::CapitalizedInterestAddition:
                    destination =
                        &monthly_principal_additions[movement.month];
                    break;
                case PrincipalMovementKind::ConversionExtinguishment:
                    destination =
                        &monthly_principal_conversions[movement.month];
                    break;
                case PrincipalMovementKind::Writeoff:
                    destination =
                        &monthly_principal_writeoffs[movement.month];
                    break;
                }
                destination->add(
                    static_cast<long double>(movement.amount_million));
            }

            CompensatedSum cumulative_draws;
            CompensatedSum cumulative_principal_cash;
            for (std::size_t month = 0U; month < month_count; ++month) {
                cumulative_draws.add(monthly_draws[month].value());
                cumulative_principal_cash.add(
                    monthly_principal_cash[month].value());
                if (project.principal_accounting_mode ==
                        PrincipalAccountingMode::DrawEqualsPrincipalLegacy &&
                    exceeds_with_input_tolerance(
                        cumulative_principal_cash.value(),
                        cumulative_draws.value())) {
                    throw std::invalid_argument(
                        "principal cannot be returned before or beyond funded draws in legacy mode");
                }
            }
            if (exceeds_with_input_tolerance(cumulative_draws.value(),
                    static_cast<long double>(
                        project.commitment_million))) {
                throw std::invalid_argument(
                    "project draws exceed the declared commitment");
            }
            if (project.principal_accounting_mode ==
                PrincipalAccountingMode::ExplicitContractualLedger) {
                long double balance = static_cast<long double>(
                    project.opening_principal_million);
                for (std::size_t month = 0U; month < month_count; ++month) {
                    balance += monthly_principal_additions[month].value();
                    if (exceeds_with_input_tolerance(balance,
                            static_cast<long double>(
                                project.principal_limit_million))) {
                        throw std::invalid_argument(
                            "explicit contractual principal exceeds its project limit");
                    }
                    const auto reduce_principal =
                        [&](long double reduction,
                            std::string_view description) {
                            if (exceeds_with_input_tolerance(
                                    reduction, balance)) {
                                throw std::invalid_argument(
                                    std::string(description) +
                                    " exceeds outstanding contractual principal");
                            }
                            balance = std::max(0.0L, balance - reduction);
                        };
                    reduce_principal(monthly_principal_cash[month].value(),
                        "principal cash");
                    reduce_principal(
                        monthly_principal_conversions[month].value(),
                        "principal conversion");
                    reduce_principal(
                        monthly_principal_writeoffs[month].value(),
                        "principal writeoff");
                }
                if (path.resolution == ProjectPathResolution::Resolved &&
                    !near_input_money(balance, 0.0L)) {
                    throw std::invalid_argument(
                        "resolved explicit-contractual path must close principal at zero");
                }
            }
        }
        if (std::find(project_seen.begin(), project_seen.end(), false) !=
            project_seen.end()) {
            throw std::invalid_argument(
                "joint scenario is missing a configured project path");
        }

        for (std::size_t source_position = 0U;
             source_position < scenario.cash_sources.size(); ++source_position) {
            const ScenarioCashSource& source =
                scenario.cash_sources[source_position];
            const std::vector<const MonthlyAmount*> available =
                sorted_monthly_amounts(source.cash_available);
            std::vector<const InvestorReceipt*>& receipts =
                receipts_by_source[source_position];
            std::sort(receipts.begin(), receipts.end(),
                [](const InvestorReceipt* left, const InvestorReceipt* right) {
                    if (left->month != right->month) {
                        return left->month < right->month;
                    }
                    if (left->amount_million != right->amount_million) {
                        return left->amount_million < right->amount_million;
                    }
                    return left->principal_component_million <
                        right->principal_component_million;
                });
            CompensatedSum cumulative_available;
            CompensatedSum cumulative_receipts;
            std::size_t available_position = 0U;
            std::size_t receipt_position = 0U;
            while (receipt_position < receipts.size()) {
                const std::size_t month = receipts[receipt_position]->month;
                while (available_position < available.size() &&
                    available[available_position]->month <= month) {
                    cumulative_available.add(static_cast<long double>(
                        available[available_position]->amount_million));
                    ++available_position;
                }
                while (receipt_position < receipts.size() &&
                    receipts[receipt_position]->month == month) {
                    cumulative_receipts.add(static_cast<long double>(
                        receipts[receipt_position]->amount_million));
                    ++receipt_position;
                }
                if (exceeds_with_input_tolerance(
                        cumulative_receipts.value(),
                        cumulative_available.value())) {
                    throw std::invalid_argument(
                        "aggregate investor receipts exceed their shared scenario cash budget");
                }
            }
        }

        add_record_count(cash_record_count, scenario.pool_costs.size());
        for (const MonthlyAmount& cost : scenario.pool_costs) {
            if (cost.month > config.horizon_months) {
                throw std::invalid_argument(
                    "pool cost falls outside the analysis horizon");
            }
            require_nonnegative_amount(cost.amount_million, "pool cost");
        }
    }
    const double configured_weight_sum = to_double(weight_sum.value());
    if (std::abs(configured_weight_sum - 1.0) > kWeightTolerance) {
        throw std::invalid_argument(
            "joint scenario weights must sum to one within tolerance");
    }

    if (config.loss_layers.size() > kMaximumLayers) {
        throw std::invalid_argument("loss layer count exceeds the resource bound");
    }
    if (!config.loss_layers.empty()) {
        std::unordered_set<std::string> layer_ids;
        layer_ids.reserve(config.loss_layers.size());
        long double expected_attachment = 0.0L;
        for (const LossLayer& layer : config.loss_layers) {
            require_safe_identifier(layer.id, "loss layer id");
            if (!layer_ids.emplace(layer.id).second) {
                throw std::invalid_argument("loss layer ids must be unique");
            }
            if (!std::isfinite(layer.attachment_million) ||
                !std::isfinite(layer.detachment_million) ||
                layer.attachment_million < 0.0 ||
                layer.detachment_million < 0.0 ||
                exceeds_with_input_tolerance(
                    static_cast<long double>(layer.attachment_million),
                    aggregate_reference_principal) ||
                exceeds_with_input_tolerance(
                    static_cast<long double>(layer.detachment_million),
                    aggregate_reference_principal)) {
                throw std::invalid_argument(
                    "loss layer bounds must lie within aggregate reference principal");
            }
            if (layer.detachment_million <= layer.attachment_million) {
                throw std::invalid_argument(
                    "loss layer detachment must exceed attachment");
            }
            if (!near_input_money(
                    static_cast<long double>(layer.attachment_million),
                    expected_attachment)) {
                throw std::invalid_argument(
                    "loss layers must be contiguous and start at zero");
            }
            expected_attachment =
                static_cast<long double>(layer.detachment_million);
        }
        if (!near_input_money(
                expected_attachment, aggregate_reference_principal)) {
            throw std::invalid_argument(
                "loss layers must end at aggregate reference principal");
        }
    }
}

PortfolioSummary evaluate_portfolio(const PortfolioConfig& config) {
    validate_portfolio_config(config);

    const std::size_t month_count = config.horizon_months + 1U;
    std::unordered_map<std::string, std::size_t> project_indices;
    project_indices.reserve(config.projects.size());
    for (std::size_t index = 0U; index < config.projects.size(); ++index) {
        project_indices.emplace(config.projects[index].id, index);
    }

    std::vector<const JointScenario*> ordered_scenarios;
    ordered_scenarios.reserve(config.joint_scenarios.size());
    for (const JointScenario& scenario : config.joint_scenarios) {
        ordered_scenarios.push_back(&scenario);
    }
    std::sort(ordered_scenarios.begin(), ordered_scenarios.end(),
        [](const JointScenario* left, const JointScenario* right) {
            return left->id < right->id;
        });
    CompensatedSum raw_weight_sum_accumulator;
    for (const JointScenario* scenario : ordered_scenarios) {
        raw_weight_sum_accumulator.add(
            static_cast<long double>(scenario->weight));
    }
    const long double raw_weight_sum = raw_weight_sum_accumulator.value();

    std::vector<double> discount_factors(month_count, 1.0);
    for (std::size_t month = 0U; month < month_count; ++month) {
        discount_factors[month] = std::pow(
            1.0 + config.annual_physical_hurdle_rate,
            static_cast<double>(month) / 12.0);
    }

    PortfolioSummary summary;
    summary.configured_scenario_weight_sum = to_double(raw_weight_sum);
    summary.scenarios.reserve(ordered_scenarios.size());
    double maximum_cash_error = 0.0;
    double maximum_layer_error = 0.0;

    for (const JointScenario* scenario_pointer : ordered_scenarios) {
        const JointScenario& scenario = *scenario_pointer;
        JointScenarioResult result;
        result.scenario_id = scenario.id;
        result.declared_weight = scenario.weight;
        result.normalized_weight = to_double(
            static_cast<long double>(scenario.weight) / raw_weight_sum);
        result.factor_tags = scenario.factor_tags;
        std::sort(result.factor_tags.begin(), result.factor_tags.end());
        result.return_sources = empty_source_totals();
        result.projects.resize(config.projects.size());
        result.monthly_cash_flows.resize(month_count);

        std::unordered_map<std::string, PortfolioCashSource> source_kinds;
        source_kinds.reserve(scenario.cash_sources.size());
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            source_kinds.emplace(source.id, source.kind);
        }
        std::vector<const ProjectJointPath*> paths_by_project(
            config.projects.size(), nullptr);
        for (const ProjectJointPath& path : scenario.project_paths) {
            paths_by_project[project_indices.at(path.project_id)] = &path;
        }

        std::vector<CompensatedSum> monthly_draws(month_count);
        std::vector<CompensatedSum> monthly_receipts(month_count);
        std::vector<CompensatedSum> monthly_costs(month_count);
        CompensatedSum direct_project_npv;
        CompensatedSum project_loss_sum;
        CompensatedSum project_outstanding_sum;
        CompensatedSum project_opening_principal_sum;
        CompensatedSum project_principal_added_sum;
        CompensatedSum project_principal_converted_sum;
        double maximum_principal_reconciliation_error = 0.0;
        std::array<CompensatedSum, kCashSourceCount> scenario_source_nominal{};
        std::array<CompensatedSum, kCashSourceCount> scenario_source_pv{};

        for (std::size_t project_index = 0U;
             project_index < config.projects.size(); ++project_index) {
            const ProjectJointPath& path = *paths_by_project[project_index];
            const PortfolioProject& project = config.projects[project_index];
            ProjectPathResult& project_result = result.projects[project_index];
            project_result.project_id = path.project_id;
            project_result.resolution = path.resolution;
            project_result.return_sources = empty_source_totals();

            CompensatedSum project_draws;
            CompensatedSum project_receipts;
            CompensatedSum principal_returned;
            CompensatedSum principal_added;
            CompensatedSum principal_converted;
            CompensatedSum principal_written_off;
            CompensatedSum project_npv;
            std::array<CompensatedSum, kCashSourceCount> source_nominal{};
            std::array<CompensatedSum, kCashSourceCount> source_pv{};

            for (const MonthlyAmount* draw :
                 sorted_monthly_amounts(path.capital_draws)) {
                const long double amount =
                    static_cast<long double>(draw->amount_million);
                monthly_draws[draw->month].add(amount);
                project_draws.add(amount);
                project_npv.add(-amount /
                    static_cast<long double>(discount_factors[draw->month]));
            }
            for (const InvestorOutlay& outlay : path.investor_outlays) {
                const long double amount =
                    static_cast<long double>(outlay.amount_million);
                monthly_draws[outlay.month].add(amount);
                project_draws.add(amount);
                project_npv.add(-amount / static_cast<long double>(
                    discount_factors[outlay.month]));
            }
            for (const InvestorReceipt* receipt :
                 sorted_receipts(path.investor_receipts)) {
                const long double amount =
                    static_cast<long double>(receipt->amount_million);
                const long double present_value = amount /
                    static_cast<long double>(discount_factors[receipt->month]);
                const PortfolioCashSource kind =
                    source_kinds.at(receipt->cash_source_id);
                const std::size_t kind_index = source_index(kind);
                monthly_receipts[receipt->month].add(amount);
                project_receipts.add(amount);
                principal_returned.add(static_cast<long double>(
                    receipt->principal_component_million));
                project_npv.add(present_value);
                source_nominal[kind_index].add(amount);
                source_pv[kind_index].add(present_value);
                scenario_source_nominal[kind_index].add(amount);
                scenario_source_pv[kind_index].add(present_value);
            }
            for (const PrincipalMovement& movement :
                 path.principal_movements) {
                switch (movement.kind) {
                case PrincipalMovementKind::FundedPrincipalAddition:
                case PrincipalMovementKind::CapitalizedFeeAddition:
                case PrincipalMovementKind::CapitalizedInterestAddition:
                    principal_added.add(static_cast<long double>(
                        movement.amount_million));
                    break;
                case PrincipalMovementKind::ConversionExtinguishment:
                    principal_converted.add(static_cast<long double>(
                        movement.amount_million));
                    break;
                case PrincipalMovementKind::Writeoff:
                    principal_written_off.add(static_cast<long double>(
                        movement.amount_million));
                    break;
                }
            }

            const long double draw_total = project_draws.value();
            const bool explicit_principal =
                project.principal_accounting_mode ==
                PrincipalAccountingMode::ExplicitContractualLedger;
            const long double opening_principal = explicit_principal
                ? static_cast<long double>(project.opening_principal_million)
                : 0.0L;
            const long double added_principal = explicit_principal
                ? principal_added.value()
                : draw_total;
            const long double effective_principal = explicit_principal
                ? principal_returned.value()
                : std::min(principal_returned.value(), draw_total);
            const long double converted_principal = explicit_principal
                ? principal_converted.value()
                : 0.0L;
            const long double written_off_principal = explicit_principal
                ? principal_written_off.value()
                : 0.0L;
            const long double closing_principal = std::max(0.0L,
                opening_principal + added_principal - effective_principal -
                    converted_principal - written_off_principal);
            const long double outstanding_principal =
                path.resolution == ProjectPathResolution::Continuing
                ? closing_principal
                : 0.0L;
            const long double principal_loss = explicit_principal
                ? written_off_principal
                : (path.resolution == ProjectPathResolution::Resolved
                        ? closing_principal
                        : 0.0L);
            project_result.total_draws_million = to_double(draw_total);
            project_result.total_investor_outlays_million =
                to_double(draw_total);
            project_result.total_receipts_million =
                to_double(project_receipts.value());
            project_result.opening_principal_million =
                to_double(opening_principal);
            project_result.principal_added_million =
                to_double(added_principal);
            project_result.principal_returned_million =
                to_double(effective_principal);
            project_result.principal_converted_million =
                to_double(converted_principal);
            project_result.outstanding_principal_million =
                to_double(outstanding_principal);
            project_result.principal_loss_million =
                to_double(principal_loss);
            project_result.npv_before_pool_costs_million =
                to_double(project_npv.value());
            for (std::size_t source = 0U; source < kCashSourceCount; ++source) {
                project_result.return_sources[source].nominal_million =
                    to_double(source_nominal[source].value());
                project_result.return_sources[source].present_value_million =
                    to_double(source_pv[source].value());
            }
            direct_project_npv.add(project_npv.value());
            project_loss_sum.add(static_cast<long double>(
                project_result.principal_loss_million));
            project_outstanding_sum.add(static_cast<long double>(
                project_result.outstanding_principal_million));
            project_opening_principal_sum.add(opening_principal);
            project_principal_added_sum.add(added_principal);
            project_principal_converted_sum.add(converted_principal);
            const double principal_reconciliation_error =
                absolute_difference(to_double(
                    opening_principal + added_principal),
                    to_double(effective_principal + converted_principal +
                        outstanding_principal + principal_loss));
            maximum_principal_reconciliation_error = std::max(
                maximum_principal_reconciliation_error,
                principal_reconciliation_error);
        }
        for (std::size_t source = 0U; source < kCashSourceCount; ++source) {
            result.return_sources[source].nominal_million =
                to_double(scenario_source_nominal[source].value());
            result.return_sources[source].present_value_million =
                to_double(scenario_source_pv[source].value());
        }

        for (const MonthlyAmount* cost :
             sorted_monthly_amounts(scenario.pool_costs)) {
            monthly_costs[cost->month].add(
                static_cast<long double>(cost->amount_million));
        }

        CompensatedSum cumulative_net_cash;
        CompensatedSum total_draws;
        CompensatedSum total_receipts;
        CompensatedSum total_costs;
        CompensatedSum pool_cost_npv;
        CompensatedSum scenario_npv_from_months;
        double peak_draw = 0.0;
        double peak_funding_need = 0.0;
        double peak_net_outlay = 0.0;
        for (std::size_t month = 0U; month < month_count; ++month) {
            MonthlyInvestorCashFlow& cash_flow =
                result.monthly_cash_flows[month];
            cash_flow.month = month;
            cash_flow.capital_draws_million =
                to_double(monthly_draws[month].value());
            cash_flow.investor_outlays_million =
                cash_flow.capital_draws_million;
            cash_flow.investor_receipts_million =
                to_double(monthly_receipts[month].value());
            cash_flow.pool_costs_million =
                to_double(monthly_costs[month].value());
            cash_flow.funding_need_million =
                cash_flow.capital_draws_million +
                cash_flow.pool_costs_million;
            cash_flow.net_cash_flow_million =
                cash_flow.investor_receipts_million -
                cash_flow.funding_need_million;

            // Conservative within-month convention: fund draws and pool costs
            // first, measure liquidity, then release investor receipts.
            cumulative_net_cash.add(-static_cast<long double>(
                cash_flow.funding_need_million));
            peak_net_outlay = std::max(peak_net_outlay,
                std::max(0.0, -to_double(cumulative_net_cash.value())));
            cumulative_net_cash.add(static_cast<long double>(
                cash_flow.investor_receipts_million));
            cash_flow.cumulative_net_cash_flow_million =
                to_double(cumulative_net_cash.value());

            total_draws.add(monthly_draws[month].value());
            total_receipts.add(monthly_receipts[month].value());
            total_costs.add(monthly_costs[month].value());
            pool_cost_npv.add(monthly_costs[month].value() /
                static_cast<long double>(discount_factors[month]));
            scenario_npv_from_months.add(
                static_cast<long double>(cash_flow.net_cash_flow_million) /
                static_cast<long double>(discount_factors[month]));
            peak_draw = std::max(
                peak_draw, cash_flow.capital_draws_million);
            peak_funding_need = std::max(
                peak_funding_need, cash_flow.funding_need_million);
        }

        result.total_draws_million = to_double(total_draws.value());
        result.total_investor_outlays_million =
            result.total_draws_million;
        result.total_receipts_million = to_double(total_receipts.value());
        result.total_pool_costs_million = to_double(total_costs.value());
        result.peak_same_month_draw_million = peak_draw;
        result.peak_same_month_funding_need_million = peak_funding_need;
        result.peak_cumulative_net_outlay_million = peak_net_outlay;
        CompensatedSum principal_returned;
        for (const ProjectPathResult& project : result.projects) {
            principal_returned.add(static_cast<long double>(
                project.principal_returned_million));
        }
        result.principal_returned_million =
            to_double(principal_returned.value());
        result.opening_principal_million =
            to_double(project_opening_principal_sum.value());
        result.principal_added_million =
            to_double(project_principal_added_sum.value());
        result.principal_converted_million =
            to_double(project_principal_converted_sum.value());
        result.outstanding_principal_million =
            to_double(project_outstanding_sum.value());
        result.principal_loss_million =
            to_double(project_loss_sum.value());
        result.npv_million = to_double(scenario_npv_from_months.value());

        const double direct_npv_after_pool_costs =
            to_double(direct_project_npv.value() - pool_cost_npv.value());
        const double source_receipts = sum_nominal_sources(result.return_sources);
        const double cash_error = std::max({
            absolute_difference(result.total_receipts_million, source_receipts),
            maximum_principal_reconciliation_error,
            absolute_difference(result.npv_million,
                direct_npv_after_pool_costs)});
        maximum_cash_error = std::max(maximum_cash_error, cash_error);
        const double cash_scale = std::max({result.total_draws_million,
            result.total_receipts_million, result.total_pool_costs_million,
            result.opening_principal_million,
            result.principal_added_million,
            std::abs(result.npv_million)});
        enforce_reconciliation(cash_error, cash_scale,
            "scenario cash-flow or principal-ledger reconciliation failed");

        CompensatedSum allocated_layer_loss;
        result.layers.reserve(config.loss_layers.size());
        for (const LossLayer& layer : config.loss_layers) {
            const double allocated =
                layer_loss(result.principal_loss_million, layer);
            result.layers.push_back(LayerPathResult{layer.id, allocated});
            allocated_layer_loss.add(static_cast<long double>(allocated));
        }
        if (!config.loss_layers.empty()) {
            const double layer_error = absolute_difference(
                to_double(allocated_layer_loss.value()),
                result.principal_loss_million);
            maximum_layer_error = std::max(maximum_layer_error, layer_error);
            enforce_reconciliation(layer_error, result.principal_loss_million,
                "scenario layer-loss reconciliation failed");
        }
        summary.scenarios.push_back(std::move(result));
    }

    summary.maximum_cash_reconciliation_error_million = maximum_cash_error;
    summary.maximum_layer_reconciliation_error_million = maximum_layer_error;

    std::vector<double> weights;
    std::vector<double> total_draws;
    std::vector<double> peak_draws;
    std::vector<double> peak_funding_needs;
    std::vector<double> peak_outlays;
    std::vector<double> pool_outstanding;
    std::vector<double> pool_losses;
    std::vector<double> npvs;
    std::vector<double> npv_shortfalls;
    weights.reserve(summary.scenarios.size());
    total_draws.reserve(summary.scenarios.size());
    peak_draws.reserve(summary.scenarios.size());
    peak_funding_needs.reserve(summary.scenarios.size());
    peak_outlays.reserve(summary.scenarios.size());
    pool_outstanding.reserve(summary.scenarios.size());
    pool_losses.reserve(summary.scenarios.size());
    npvs.reserve(summary.scenarios.size());
    npv_shortfalls.reserve(summary.scenarios.size());
    CompensatedSum pool_impairment_weight;
    CompensatedSum pool_negative_npv_weight;
    for (const JointScenarioResult& scenario : summary.scenarios) {
        weights.push_back(scenario.normalized_weight);
        total_draws.push_back(scenario.total_draws_million);
        peak_draws.push_back(scenario.peak_same_month_draw_million);
        peak_funding_needs.push_back(
            scenario.peak_same_month_funding_need_million);
        peak_outlays.push_back(scenario.peak_cumulative_net_outlay_million);
        pool_outstanding.push_back(scenario.outstanding_principal_million);
        pool_losses.push_back(scenario.principal_loss_million);
        npvs.push_back(scenario.npv_million);
        npv_shortfalls.push_back(std::max(0.0, -scenario.npv_million));
        if (scenario.principal_loss_million > 0.0) {
            pool_impairment_weight.add(
                static_cast<long double>(scenario.normalized_weight));
        }
        if (scenario.npv_million < 0.0) {
            pool_negative_npv_weight.add(
                static_cast<long double>(scenario.normalized_weight));
        }
    }
    const long double aggregate_weight = stable_weight_sum(weights);
    summary.total_draws_million =
        summarize_distribution(total_draws, weights);
    summary.peak_same_month_draw_million =
        summarize_distribution(peak_draws, weights);
    summary.peak_same_month_funding_need_million =
        summarize_distribution(peak_funding_needs, weights);
    summary.peak_cumulative_net_outlay_million =
        summarize_distribution(peak_outlays, weights);
    summary.outstanding_principal_million =
        summarize_distribution(pool_outstanding, weights);
    summary.principal_loss_million =
        summarize_distribution(pool_losses, weights);
    summary.npv_million = summarize_distribution(npvs, weights);
    summary.npv_shortfall_million =
        summarize_distribution(npv_shortfalls, weights);
    summary.principal_impairment_probability = to_double(
        pool_impairment_weight.value() / aggregate_weight);
    summary.negative_npv_probability = to_double(
        pool_negative_npv_weight.value() / aggregate_weight);

    summary.expected_return_sources = empty_source_totals();
    for (std::size_t source = 0U; source < kCashSourceCount; ++source) {
        CompensatedSum nominal;
        CompensatedSum present_value;
        for (const JointScenarioResult& scenario : summary.scenarios) {
            nominal.add(static_cast<long double>(scenario.normalized_weight) *
                static_cast<long double>(
                    scenario.return_sources[source].nominal_million));
            present_value.add(
                static_cast<long double>(scenario.normalized_weight) *
                static_cast<long double>(
                    scenario.return_sources[source].present_value_million));
        }
        summary.expected_return_sources[source].nominal_million =
            to_double(nominal.value() / aggregate_weight);
        summary.expected_return_sources[source].present_value_million =
            to_double(present_value.value() / aggregate_weight);
    }

    std::vector<WeightedValue> weighted_pool_losses;
    weighted_pool_losses.reserve(summary.scenarios.size());
    for (std::size_t scenario_index = 0U;
         scenario_index < summary.scenarios.size(); ++scenario_index) {
        weighted_pool_losses.push_back(WeightedValue{
            pool_losses[scenario_index], weights[scenario_index]});
    }
    const std::vector<double> pool_tail_95 =
        upper_tail_fractions(weighted_pool_losses, 0.05);
    const std::vector<double> pool_tail_99 =
        upper_tail_fractions(weighted_pool_losses, 0.01);
    const long double requested_tail_95 = 0.05L * aggregate_weight;
    const long double requested_tail_99 = 0.01L * aggregate_weight;

    summary.projects.reserve(config.projects.size());
    std::vector<std::vector<double>> project_losses(config.projects.size());
    CompensatedSum sum_standalone_es95;
    CompensatedSum sum_standalone_es99;
    for (std::size_t project_index = 0U;
         project_index < config.projects.size(); ++project_index) {
        std::vector<double> project_draws;
        std::vector<double> project_receipts;
        std::vector<double> project_outstanding;
        std::vector<double> project_npvs;
        std::vector<double> project_npv_shortfalls;
        project_draws.reserve(summary.scenarios.size());
        project_receipts.reserve(summary.scenarios.size());
        project_outstanding.reserve(summary.scenarios.size());
        project_losses[project_index].reserve(summary.scenarios.size());
        project_npvs.reserve(summary.scenarios.size());
        project_npv_shortfalls.reserve(summary.scenarios.size());
        CompensatedSum tail_contribution_95;
        CompensatedSum tail_contribution_99;
        CompensatedSum impairment_weight;
        CompensatedSum negative_npv_weight;
        for (std::size_t scenario_index = 0U;
             scenario_index < summary.scenarios.size(); ++scenario_index) {
            const ProjectPathResult& project =
                summary.scenarios[scenario_index].projects[project_index];
            project_draws.push_back(project.total_draws_million);
            project_receipts.push_back(project.total_receipts_million);
            project_outstanding.push_back(project.outstanding_principal_million);
            project_losses[project_index].push_back(
                project.principal_loss_million);
            project_npvs.push_back(project.npv_before_pool_costs_million);
            project_npv_shortfalls.push_back(
                std::max(0.0, -project.npv_before_pool_costs_million));
            if (project.principal_loss_million > 0.0) {
                impairment_weight.add(
                    static_cast<long double>(weights[scenario_index]));
            }
            if (project.npv_before_pool_costs_million < 0.0) {
                negative_npv_weight.add(
                    static_cast<long double>(weights[scenario_index]));
            }
            tail_contribution_95.add(
                static_cast<long double>(weights[scenario_index]) *
                static_cast<long double>(pool_tail_95[scenario_index]) *
                static_cast<long double>(project.principal_loss_million));
            tail_contribution_99.add(
                static_cast<long double>(weights[scenario_index]) *
                static_cast<long double>(pool_tail_99[scenario_index]) *
                static_cast<long double>(project.principal_loss_million));
        }

        ProjectPortfolioSummary project_summary;
        project_summary.project_id = config.projects[project_index].id;
        project_summary.total_draws_million =
            summarize_distribution(project_draws, weights);
        project_summary.total_receipts_million =
            summarize_distribution(project_receipts, weights);
        project_summary.outstanding_principal_million =
            summarize_distribution(project_outstanding, weights);
        project_summary.principal_loss_million =
            summarize_distribution(project_losses[project_index], weights);
        project_summary.npv_before_pool_costs_million =
            summarize_distribution(project_npvs, weights);
        project_summary.npv_shortfall_before_pool_costs_million =
            summarize_distribution(project_npv_shortfalls, weights);
        project_summary.expected_draws_million =
            project_summary.total_draws_million.mean;
        project_summary.expected_receipts_million =
            project_summary.total_receipts_million.mean;
        project_summary.expected_outstanding_principal_million =
            project_summary.outstanding_principal_million.mean;
        project_summary.expected_principal_loss_million =
            project_summary.principal_loss_million.mean;
        project_summary.expected_npv_before_pool_costs_million =
            project_summary.npv_before_pool_costs_million.mean;
        project_summary.principal_impairment_probability = to_double(
            impairment_weight.value() / aggregate_weight);
        project_summary.negative_npv_probability = to_double(
            negative_npv_weight.value() / aggregate_weight);
        project_summary.pool_loss_tail_contribution_es95_million = to_double(
            tail_contribution_95.value() / requested_tail_95);
        project_summary.pool_loss_tail_contribution_es99_million = to_double(
            tail_contribution_99.value() / requested_tail_99);
        sum_standalone_es95.add(static_cast<long double>(
            project_summary.principal_loss_million.expected_shortfall_95));
        sum_standalone_es99.add(static_cast<long double>(
            project_summary.principal_loss_million.expected_shortfall_99));
        summary.projects.push_back(std::move(project_summary));
    }
    summary.sum_standalone_es95_million =
        to_double(sum_standalone_es95.value());
    summary.sum_standalone_es99_million =
        to_double(sum_standalone_es99.value());

    const double pooled_es95 =
        summary.principal_loss_million.expected_shortfall_95;
    const double pooled_es99 =
        summary.principal_loss_million.expected_shortfall_99;
    summary.diversification_benefit_es95_million =
        checked_diversification_benefit(
            summary.sum_standalone_es95_million, pooled_es95, "ES95");
    summary.diversification_benefit_es99_million =
        checked_diversification_benefit(
            summary.sum_standalone_es99_million, pooled_es99, "ES99");
    if (summary.sum_standalone_es95_million > 0.0) {
        summary.diversification_ratio_es95 =
            summary.diversification_benefit_es95_million /
            summary.sum_standalone_es95_million;
    }
    if (summary.sum_standalone_es99_million > 0.0) {
        summary.diversification_ratio_es99 =
            summary.diversification_benefit_es99_million /
            summary.sum_standalone_es99_million;
    }

    summary.pairwise_loss_correlations.reserve(
        config.projects.size() * (config.projects.size() - 1U) / 2U);
    for (std::size_t first = 0U; first < config.projects.size(); ++first) {
        for (std::size_t second = first + 1U;
             second < config.projects.size(); ++second) {
            CompensatedSum first_weighted_sum;
            CompensatedSum second_weighted_sum;
            for (std::size_t scenario_index = 0U;
                 scenario_index < weights.size(); ++scenario_index) {
                first_weighted_sum.add(
                    static_cast<long double>(weights[scenario_index]) *
                    static_cast<long double>(
                        project_losses[first][scenario_index]));
                second_weighted_sum.add(
                    static_cast<long double>(weights[scenario_index]) *
                    static_cast<long double>(
                        project_losses[second][scenario_index]));
            }
            const long double first_mean =
                first_weighted_sum.value() / aggregate_weight;
            const long double second_mean =
                second_weighted_sum.value() / aggregate_weight;
            CompensatedSum covariance;
            CompensatedSum first_variance;
            CompensatedSum second_variance;
            for (std::size_t scenario_index = 0U;
                 scenario_index < weights.size(); ++scenario_index) {
                const long double first_difference =
                    static_cast<long double>(
                        project_losses[first][scenario_index]) - first_mean;
                const long double second_difference =
                    static_cast<long double>(
                        project_losses[second][scenario_index]) - second_mean;
                const long double weight =
                    static_cast<long double>(weights[scenario_index]);
                covariance.add(weight * first_difference * second_difference);
                first_variance.add(
                    weight * first_difference * first_difference);
                second_variance.add(
                    weight * second_difference * second_difference);
            }
            PairwiseLossCorrelation correlation;
            correlation.first_project_id = config.projects[first].id;
            correlation.second_project_id = config.projects[second].id;
            if (first_variance.value() > 0.0L &&
                second_variance.value() > 0.0L) {
                const long double denominator = std::sqrt(
                    first_variance.value() * second_variance.value());
                correlation.correlation = std::clamp(
                    to_double(covariance.value() / denominator), -1.0, 1.0);
            }
            summary.pairwise_loss_correlations.push_back(
                std::move(correlation));
        }
    }

    summary.layers.reserve(config.loss_layers.size());
    for (std::size_t layer_index = 0U;
         layer_index < config.loss_layers.size(); ++layer_index) {
        const LossLayer& layer = config.loss_layers[layer_index];
        std::vector<double> losses;
        losses.reserve(summary.scenarios.size());
        CompensatedSum impairment_weight;
        CompensatedSum exhaustion_weight;
        const double width =
            layer.detachment_million - layer.attachment_million;
        for (std::size_t scenario_index = 0U;
             scenario_index < summary.scenarios.size(); ++scenario_index) {
            const double loss = summary.scenarios[scenario_index]
                                    .layers[layer_index]
                                    .principal_loss_million;
            losses.push_back(loss);
            if (loss > 0.0) {
                impairment_weight.add(
                    static_cast<long double>(weights[scenario_index]));
            }
            if (loss >= width) {
                exhaustion_weight.add(
                    static_cast<long double>(weights[scenario_index]));
            }
        }
        LayerPortfolioSummary layer_summary;
        layer_summary.layer_id = layer.id;
        layer_summary.attachment_million = layer.attachment_million;
        layer_summary.detachment_million = layer.detachment_million;
        layer_summary.principal_loss_million =
            summarize_distribution(losses, weights);
        layer_summary.expected_loss_million =
            layer_summary.principal_loss_million.mean;
        layer_summary.impairment_probability = to_double(
            impairment_weight.value() / aggregate_weight);
        layer_summary.exhaustion_probability = to_double(
            exhaustion_weight.value() / aggregate_weight);
        summary.layers.push_back(std::move(layer_summary));
    }

    CompensatedSum tail_contribution_sum_95;
    CompensatedSum tail_contribution_sum_99;
    for (const ProjectPortfolioSummary& project : summary.projects) {
        tail_contribution_sum_95.add(static_cast<long double>(
            project.pool_loss_tail_contribution_es95_million));
        tail_contribution_sum_99.add(static_cast<long double>(
            project.pool_loss_tail_contribution_es99_million));
    }
    const double tail_error_95 = absolute_difference(
        to_double(tail_contribution_sum_95.value()), pooled_es95);
    const double tail_error_99 = absolute_difference(
        to_double(tail_contribution_sum_99.value()), pooled_es99);
    enforce_reconciliation(tail_error_95, pooled_es95,
        "ES95 project contribution reconciliation failed");
    enforce_reconciliation(tail_error_99, pooled_es99,
        "ES99 project contribution reconciliation failed");

    return summary;
}

} // namespace naturalehia::cellular_finance
