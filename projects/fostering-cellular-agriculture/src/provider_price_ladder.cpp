// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_price_ladder.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMaximumAnnualRate = 10.0;
constexpr double kMaximumVariableClaimExpenseFraction = 10.0;
constexpr double kValidationTolerance = 1.0e-10;

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    if (std::isspace(static_cast<unsigned char>(value.front())) != 0 ||
        std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        throw std::invalid_argument(
            std::string(description) +
            " must not have leading or trailing whitespace");
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

void require_unit_fraction(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(description) + " must be finite and within [0, 1]");
    }
}

void require_rate(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > kMaximumAnnualRate) {
        throw std::invalid_argument(
            std::string(description) + " is outside the supported range");
    }
}

void require_nonnegative(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            std::string(description) + " must be finite and non-negative");
    }
}

[[nodiscard]] double checked_double(
    long double value, std::string_view description) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            std::string(description) + " exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] bool close_enough(double first, double second) noexcept {
    if (!std::isfinite(first) || !std::isfinite(second)) {
        return false;
    }
    const double scale = std::max({1.0, std::abs(first), std::abs(second)});
    return std::abs(first - second) <= kValidationTolerance * scale;
}

void validate_endpoint(const AmbiguityEndpoint& endpoint,
    const std::vector<ScenarioProbabilityBounds>& bounds,
    std::string_view description) {
    if (!std::isfinite(endpoint.value) ||
        endpoint.scenario_weights.size() != bounds.size()) {
        throw std::invalid_argument(
            std::string(description) + " has an invalid value or witness size");
    }
    long double sum = 0.0L;
    for (std::size_t index = 0U; index < bounds.size(); ++index) {
        const double weight = endpoint.scenario_weights[index];
        if (!std::isfinite(weight) ||
            weight < bounds[index].lower_weight - kValidationTolerance ||
            weight > bounds[index].upper_weight + kValidationTolerance) {
            throw std::invalid_argument(
                std::string(description) + " has an infeasible witness weight");
        }
        sum += static_cast<long double>(weight);
    }
    if (std::abs(sum - 1.0L) >
        static_cast<long double>(kValidationTolerance) * 4.0L) {
        throw std::invalid_argument(
            std::string(description) + " witness weights do not sum to one");
    }
}

void validate_range(const AmbiguityMetricRange& range,
    const std::vector<ScenarioProbabilityBounds>& bounds,
    std::string_view description) {
    validate_endpoint(range.minimum, bounds,
        std::string(description) + " minimum");
    validate_endpoint(range.maximum, bounds,
        std::string(description) + " maximum");
    if (!std::isfinite(range.central) ||
        range.minimum.value > range.maximum.value + kValidationTolerance) {
        throw std::invalid_argument(
            std::string(description) + " has invalid central or endpoint values");
    }
}

[[nodiscard]] bool same_endpoint(
    const AmbiguityEndpoint& first, const AmbiguityEndpoint& second) {
    return first.value == second.value &&
        first.scenario_weights == second.scenario_weights;
}

[[nodiscard]] bool same_range(
    const AmbiguityMetricRange& first, const AmbiguityMetricRange& second) {
    return same_endpoint(first.minimum, second.minimum) &&
        first.central == second.central &&
        same_endpoint(first.maximum, second.maximum);
}

[[nodiscard]] AmbiguityEndpoint affine_endpoint(
    const AmbiguityEndpoint& endpoint, double scale, double shift) {
    AmbiguityEndpoint result = endpoint;
    result.value = checked_double(
        static_cast<long double>(scale) *
                static_cast<long double>(endpoint.value) +
            static_cast<long double>(shift),
        "provider price-ladder endpoint transformation");
    return result;
}

[[nodiscard]] AmbiguityMetricRange affine_range(
    const AmbiguityMetricRange& range, double scale, double shift) {
    if (!std::isfinite(scale) || scale <= 0.0 || !std::isfinite(shift)) {
        throw std::logic_error(
            "provider price-ladder affine transformation is invalid");
    }
    AmbiguityMetricRange result;
    result.minimum = affine_endpoint(range.minimum, scale, shift);
    result.central = checked_double(
        static_cast<long double>(scale) *
                static_cast<long double>(range.central) +
            static_cast<long double>(shift),
        "provider price-ladder central transformation");
    result.maximum = affine_endpoint(range.maximum, scale, shift);
    return result;
}

[[nodiscard]] double range_reconciliation_error(
    const AmbiguityMetricRange& original,
    const AmbiguityMetricRange& transformed,
    double scale, double shift) {
    const auto expected = [scale, shift](double value) {
        return checked_double(
            static_cast<long double>(scale) *
                    static_cast<long double>(value) +
                static_cast<long double>(shift),
            "provider price-ladder range reconciliation");
    };
    return std::max({
        std::abs(transformed.minimum.value -
            expected(original.minimum.value)),
        std::abs(transformed.central - expected(original.central)),
        std::abs(transformed.maximum.value -
            expected(original.maximum.value)),
    });
}

[[nodiscard]] double safe_growth(double annual_rate, double years,
    std::string_view description) {
    const double result = std::pow(1.0 + annual_rate, years);
    if (!std::isfinite(result) || result <= 0.0) {
        throw std::overflow_error(
            std::string(description) + " growth factor is invalid");
    }
    return result;
}

// Computes (1 + higher_rate)^years - (1 + lower_rate)^years without
// subtracting two nearly equal rounded powers. Rates are validated as
// non-negative and higher_rate >= lower_rate by the public validator.
[[nodiscard]] double stable_growth_difference(double higher_rate,
    double lower_rate, double years, std::string_view description) {
    const long double duration = static_cast<long double>(years);
    const long double higher_log = duration *
        std::log1p(static_cast<long double>(higher_rate));
    const long double lower_log = duration *
        std::log1p(static_cast<long double>(lower_rate));
    const long double difference = std::exp(lower_log) *
        std::expm1(higher_log - lower_log);
    if (!std::isfinite(difference) || difference < 0.0L) {
        throw std::overflow_error(
            std::string(description) + " growth difference is invalid");
    }
    return checked_double(difference, description);
}

// Computes (1 + annual_rate)^years - 1 stably for a small charge rate.
[[nodiscard]] double stable_growth_above_one(double annual_rate,
    double years, std::string_view description) {
    const long double exponent = static_cast<long double>(years) *
        std::log1p(static_cast<long double>(annual_rate));
    const long double difference = std::expm1(exponent);
    if (!std::isfinite(difference) || difference < 0.0L) {
        throw std::overflow_error(
            std::string(description) + " growth increment is invalid");
    }
    return checked_double(difference, description);
}

void validate_protection_point(const PooledLossProtectionConfig& protection,
    const PooledLossProtectionSummary& summary,
    const PooledLossProtectionRobustPoint& point,
    const ProviderPriceLadderConfig& pricing) {
    if (protection.model_version != kPooledLossProtectionModelVersion ||
        !protection.synthetic_inputs) {
        throw std::invalid_argument(
            "provider price ladder requires a validated synthetic pooled-loss-protection configuration");
    }
    require_rate(protection.provider_annual_physical_hurdle_rate,
        "provider price ladder inherited provider hurdle");
    if (summary.scenario_probability_bounds.empty()) {
        throw std::invalid_argument(
            "provider price ladder requires non-empty scenario probability bounds");
    }
    for (const ScenarioProbabilityBounds& bounds :
         summary.scenario_probability_bounds) {
        if (bounds.scenario_id.empty() ||
            !std::isfinite(bounds.lower_weight) ||
            !std::isfinite(bounds.central_weight) ||
            !std::isfinite(bounds.upper_weight) ||
            bounds.lower_weight < 0.0 ||
            bounds.lower_weight > bounds.central_weight ||
            bounds.central_weight > bounds.upper_weight ||
            bounds.upper_weight > 1.0) {
            throw std::invalid_argument(
                "provider price ladder received invalid scenario probability bounds");
        }
    }
    require_unit_fraction(point.coverage_fraction,
        "provider price ladder selected coverage fraction");
    require_unit_fraction(summary.maximum_supported_coverage_fraction,
        "provider price ladder maximum supported coverage fraction");
    if (point.coverage_fraction >
        summary.maximum_supported_coverage_fraction) {
        throw std::invalid_argument(
            "provider price ladder selected point exceeds maximum supported coverage");
    }
    require_nonnegative(summary.aggregate_reference_principal_million,
        "provider price ladder aggregate contractual reference principal");
    require_nonnegative(summary.aggregate_covered_commitment_million,
        "provider price ladder reference-principal compatibility alias");
    if (!close_enough(summary.aggregate_reference_principal_million,
            summary.aggregate_covered_commitment_million)) {
        throw std::invalid_argument(
            "provider price ladder received inconsistent reference-principal fields");
    }
    require_nonnegative(summary.legal_support_cap_million,
        "provider price ladder configured monetary support cap");
    require_nonnegative(
        point.provider_risk.contractual_maximum_exposure_million,
        "provider price ladder contractual maximum exposure");
    require_nonnegative(point.provider_risk.modeled_maximum_claim_million,
        "provider price ladder modeled maximum claim");
    if (!std::isfinite(point.investor_target_worst_expected_npv_million) ||
        !std::isfinite(point.investor_signed_premium_headroom_million) ||
        point.investor_target_worst_expected_npv_million !=
            summary.investor_target_worst_expected_npv_million) {
        throw std::invalid_argument(
            "provider price ladder selected point has an inconsistent investor target");
    }
    validate_range(point.investor_expected_npv_before_premium_million,
        summary.scenario_probability_bounds,
        "provider price ladder investor NPV range");
    validate_range(point.provider_risk.expected_claim_present_value_million,
        summary.scenario_probability_bounds,
        "provider price ladder provider claim-PV range");
    if (!close_enough(
            point.provider_minimum_robust_break_even_premium_million,
            point.provider_risk.expected_claim_present_value_million
                .maximum.value)) {
        throw std::invalid_argument(
            "provider price ladder selected point has an inconsistent claim-only floor");
    }
    const double reconstructed_headroom =
        point.investor_expected_npv_before_premium_million.minimum.value -
        point.investor_target_worst_expected_npv_million;
    if (!close_enough(point.investor_signed_premium_headroom_million,
            reconstructed_headroom)) {
        throw std::invalid_argument(
            "provider price ladder selected point has inconsistent investor premium headroom");
    }
    if (point.investor_maximum_nonnegative_premium_million.has_value()) {
        require_nonnegative(
            *point.investor_maximum_nonnegative_premium_million,
            "provider price ladder investor premium ceiling");
        if (point.investor_signed_premium_headroom_million < 0.0 ||
            *point.investor_maximum_nonnegative_premium_million >
                point.investor_signed_premium_headroom_million) {
            throw std::invalid_argument(
                "provider price ladder selected point has an invalid investor premium ceiling");
        }
    } else if (point.investor_signed_premium_headroom_million >= 0.0) {
        throw std::invalid_argument(
            "provider price ladder selected point omitted a non-negative investor premium ceiling");
    }

    const double expected_contractual_exposure = checked_double(
        static_cast<long double>(point.coverage_fraction) *
            static_cast<long double>(
                summary.aggregate_reference_principal_million),
        "provider price-ladder contractual exposure reconstruction");
    if (!close_enough(expected_contractual_exposure,
            point.provider_risk.contractual_maximum_exposure_million) ||
        point.provider_risk.contractual_maximum_exposure_million >
            summary.legal_support_cap_million + kValidationTolerance) {
        throw std::invalid_argument(
            "provider price ladder selected point has inconsistent contractual exposure");
    }

    if (pricing.coverage_selection ==
        ProviderPriceCoverageSelection::
            ReportedInvestorTargetPassingFraction) {
        if (!summary
                 .investor_target_passing_coverage_fraction_upper_bound
                 .has_value()) {
            throw std::invalid_argument(
                "provider price ladder requested a reported investor-target-passing point, but none exists");
        }
        if (point.coverage_fraction != summary.reported_coverage_fraction ||
            point.coverage_fraction !=
                *summary
                     .investor_target_passing_coverage_fraction_upper_bound ||
            !same_range(point.investor_expected_npv_before_premium_million,
                summary.reported
                    .investor_expected_npv_before_premium_million) ||
            !same_range(
                point.provider_risk.expected_claim_present_value_million,
                summary.reported.provider_risk
                    .expected_claim_present_value_million)) {
            throw std::invalid_argument(
                "provider price ladder did not receive the exact reported investor-target-passing point");
        }
    } else {
        if (!pricing.explicit_coverage_fraction.has_value() ||
            point.coverage_fraction !=
                *pricing.explicit_coverage_fraction) {
            throw std::invalid_argument(
                "provider price ladder did not receive the exact explicit coverage point");
        }
    }
}

} // namespace

std::string_view to_string(
    ProviderPriceCoverageSelection selection) noexcept {
    switch (selection) {
    case ProviderPriceCoverageSelection::ExplicitCoverageFraction:
        return "explicit";
    case ProviderPriceCoverageSelection::
        ReportedInvestorTargetPassingFraction:
        return "reported-investor-target-passing";
    }
    return "unknown";
}

std::string_view to_string(ProviderPriceLadderStatus status) noexcept {
    switch (status) {
    case ProviderPriceLadderStatus::RobustAllInPremiumIntervalExists:
        return "robust-all-in-premium-interval-exists";
    case ProviderPriceLadderStatus::InvestorCannotPayNonnegativePremium:
        return "investor-cannot-pay-nonnegative-premium";
    case ProviderPriceLadderStatus::ProviderAllInFloorExceedsInvestorCeiling:
        return "provider-all-in-floor-exceeds-investor-ceiling";
    }
    return "unknown";
}

void validate_provider_price_ladder_config(
    const ProviderPriceLadderConfig& config) {
    if (config.model_version != kProviderPriceLadderModelVersion) {
        throw std::invalid_argument(
            "provider price ladder model_version does not match this engine");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "provider price ladder v0.1 accepts synthetic inputs only");
    }
    require_safe_text(
        config.scenario_label, "provider price ladder scenario_label");
    require_safe_text(
        config.source_note, "provider price ladder source_note");

    switch (config.coverage_selection) {
    case ProviderPriceCoverageSelection::ExplicitCoverageFraction:
        if (!config.explicit_coverage_fraction.has_value()) {
            throw std::invalid_argument(
                "provider price ladder explicit selection requires a coverage fraction");
        }
        require_unit_fraction(*config.explicit_coverage_fraction,
            "provider price ladder explicit coverage fraction");
        break;
    case ProviderPriceCoverageSelection::
        ReportedInvestorTargetPassingFraction:
        if (config.explicit_coverage_fraction.has_value()) {
            throw std::invalid_argument(
                "provider price ladder reported selection requires explicit coverage to be none");
        }
        break;
    default:
        throw std::invalid_argument(
            "provider price ladder coverage selection is invalid");
    }

    if (!config.cost_bases_use_contractual_maximum_exposure ||
        !config.collateral_and_capital_are_held_until_settlement ||
        !config.variable_claim_expense_is_paid_at_claim_settlement ||
        !config.fixed_expense_and_target_profit_are_month_zero_values ||
        !config.incremental_cost_terms_are_separate_and_nonduplicative) {
        throw std::invalid_argument(
            "provider price ladder requires every cost convention and nonduplication assertion");
    }
    require_unit_fraction(
        config.collateral_fraction_of_contractual_maximum_exposure,
        "provider price ladder collateral fraction");
    require_rate(config.collateral_annual_effective_funding_rate,
        "provider price ladder collateral funding rate");
    require_rate(config.collateral_annual_effective_yield_rate,
        "provider price ladder collateral yield rate");
    if (config.collateral_annual_effective_funding_rate <
        config.collateral_annual_effective_yield_rate) {
        throw std::invalid_argument(
            "provider price ladder collateral funding rate must not be below collateral yield");
    }
    require_unit_fraction(
        config.risk_capital_fraction_of_contractual_maximum_exposure,
        "provider price ladder risk-capital fraction");
    require_rate(config.risk_capital_annual_effective_charge_rate,
        "provider price ladder risk-capital charge rate");
    require_nonnegative(config.fixed_expense_upfront_million,
        "provider price ladder fixed expense");
    if (!std::isfinite(config.variable_claim_expense_fraction) ||
        config.variable_claim_expense_fraction < 0.0 ||
        config.variable_claim_expense_fraction >
            kMaximumVariableClaimExpenseFraction) {
        throw std::invalid_argument(
            "provider price ladder variable claim-expense fraction is outside the supported range");
    }
    require_nonnegative(config.target_profit_upfront_million,
        "provider price ladder target profit");
    if (config.provider_default_risk_is_modeled) {
        throw std::invalid_argument(
            "provider price ladder v0.1 does not model provider default risk");
    }
    if (config.fair_value_is_claimed) {
        throw std::invalid_argument(
            "provider price ladder v0.1 must not claim fair value");
    }
}

ProviderPriceLadderSummary evaluate_provider_price_ladder(
    const PooledLossProtectionConfig& protection,
    const PooledLossProtectionSummary& protection_summary,
    const PooledLossProtectionRobustPoint& selected_point,
    const ProviderPriceLadderConfig& pricing) {
    validate_provider_price_ladder_config(pricing);
    validate_protection_point(
        protection, protection_summary, selected_point, pricing);

    ProviderPriceLadderSummary result;
    result.protection_solve_status = protection_summary.status;
    result.coverage_selection = pricing.coverage_selection;
    result.selected_protection_point = selected_point;
    result.scenario_probability_bounds =
        protection_summary.scenario_probability_bounds;
    result.selected_point_meets_investor_target_before_premium =
        selected_point.investor_signed_premium_headroom_million >= 0.0;

    ProviderPriceLadderCostBreakdown& costs = result.costs;
    costs.contractual_maximum_exposure_million =
        selected_point.provider_risk.contractual_maximum_exposure_million;
    costs.modeled_maximum_claim_million =
        selected_point.provider_risk.modeled_maximum_claim_million;
    costs.settlement_years =
        static_cast<double>(protection.settlement_month) / 12.0;
    costs.provider_hurdle_accumulation_factor = safe_growth(
        protection.provider_annual_physical_hurdle_rate,
        costs.settlement_years, "provider price ladder provider hurdle");
    costs.claim_only_robust_floor_million =
        selected_point.provider_risk.expected_claim_present_value_million
            .maximum.value;
    costs.variable_claim_expense_at_robust_endpoint_million =
        checked_double(
            static_cast<long double>(
                pricing.variable_claim_expense_fraction) *
                static_cast<long double>(
                    costs.claim_only_robust_floor_million),
            "provider price-ladder variable claim expense");

    costs.collateral_base_million = checked_double(
        static_cast<long double>(pricing
                .collateral_fraction_of_contractual_maximum_exposure) *
            static_cast<long double>(
                costs.contractual_maximum_exposure_million),
        "provider price-ladder collateral base");
    const double collateral_growth_difference = stable_growth_difference(
        pricing.collateral_annual_effective_funding_rate,
        pricing.collateral_annual_effective_yield_rate,
        costs.settlement_years,
        "provider price ladder collateral funding carry");
    costs.collateral_funding_carry_present_value_million = checked_double(
        static_cast<long double>(costs.collateral_base_million) *
            static_cast<long double>(collateral_growth_difference) /
            static_cast<long double>(
                costs.provider_hurdle_accumulation_factor),
        "provider price-ladder collateral funding carry");

    costs.risk_capital_base_million = checked_double(
        static_cast<long double>(pricing
                .risk_capital_fraction_of_contractual_maximum_exposure) *
            static_cast<long double>(
                costs.contractual_maximum_exposure_million),
        "provider price-ladder risk-capital base");
    const double capital_growth_increment = stable_growth_above_one(
        pricing.risk_capital_annual_effective_charge_rate,
        costs.settlement_years,
        "provider price ladder economic-capital charge");
    costs.risk_capital_charge_present_value_million = checked_double(
        static_cast<long double>(costs.risk_capital_base_million) *
            static_cast<long double>(capital_growth_increment) /
            static_cast<long double>(
                costs.provider_hurdle_accumulation_factor),
        "provider price-ladder risk-capital charge");
    costs.fixed_expense_upfront_million =
        pricing.fixed_expense_upfront_million;
    costs.target_profit_upfront_million =
        pricing.target_profit_upfront_million;

    const double claim_scale = checked_double(
        1.0L + static_cast<long double>(
                   pricing.variable_claim_expense_fraction),
        "provider price-ladder claim cost scale");
    const double deterministic_costs = checked_double(
        static_cast<long double>(
            costs.collateral_funding_carry_present_value_million) +
            static_cast<long double>(
                costs.risk_capital_charge_present_value_million) +
            static_cast<long double>(costs.fixed_expense_upfront_million),
        "provider price-ladder deterministic costs");
    const AmbiguityMetricRange& claim_range =
        selected_point.provider_risk.expected_claim_present_value_million;
    result.provider_cost_recovery_requirement_million =
        affine_range(claim_range, claim_scale, deterministic_costs);
    result.provider_all_in_revenue_requirement_million = affine_range(
        claim_range, claim_scale,
        checked_double(
            static_cast<long double>(deterministic_costs) +
                static_cast<long double>(
                    costs.target_profit_upfront_million),
            "provider price-ladder all-in deterministic requirement"));
    costs.provider_cost_recovery_floor_million =
        result.provider_cost_recovery_requirement_million.maximum.value;
    costs.provider_all_in_floor_million =
        result.provider_all_in_revenue_requirement_million.maximum.value;

    result.investor_signed_premium_headroom_million =
        selected_point.investor_signed_premium_headroom_million;
    result.investor_maximum_nonnegative_premium_million =
        selected_point.investor_maximum_nonnegative_premium_million;
    // The optional ceiling is the protection engine's conservatively
    // representable payable amount and can be slightly below raw signed
    // headroom. All interval and support calculations must use that certified
    // amount when it exists or their disclosures could contradict status.
    const double effective_signed_investor_capacity =
        result.investor_maximum_nonnegative_premium_million.value_or(
            result.investor_signed_premium_headroom_million);
    result.signed_all_in_bilateral_margin_million = checked_double(
        static_cast<long double>(effective_signed_investor_capacity) -
            static_cast<long double>(costs.provider_all_in_floor_million),
        "provider price-ladder bilateral margin");
    result.cost_recovery_support_gap_million = std::max(0.0,
        checked_double(
            static_cast<long double>(
                costs.provider_cost_recovery_floor_million) -
                static_cast<long double>(effective_signed_investor_capacity),
            "provider price-ladder cost-recovery gap"));
    result.investor_target_restoration_required_million = std::max(0.0,
        -result.investor_signed_premium_headroom_million);
    const double nonnegative_investor_capacity =
        result.investor_maximum_nonnegative_premium_million.value_or(0.0);
    result.provider_premium_support_required_million = std::max(0.0,
        checked_double(
            static_cast<long double>(costs.provider_all_in_floor_million) -
                static_cast<long double>(nonnegative_investor_capacity),
            "provider price-ladder provider premium support"));
    result.all_in_support_gap_million = std::max(0.0,
        checked_double(
            static_cast<long double>(costs.provider_all_in_floor_million) -
                static_cast<long double>(effective_signed_investor_capacity),
            "provider price-ladder all-in support gap"));

    if (!result.investor_maximum_nonnegative_premium_million.has_value()) {
        result.status = ProviderPriceLadderStatus::
            InvestorCannotPayNonnegativePremium;
    } else if (costs.provider_all_in_floor_million <=
        *result.investor_maximum_nonnegative_premium_million) {
        result.status = ProviderPriceLadderStatus::
            RobustAllInPremiumIntervalExists;
        result.robust_all_in_nonnegative_premium_interval_exists = true;
        result.robust_price_interval_lower_bound_million =
            costs.provider_all_in_floor_million;
        result.robust_price_interval_upper_bound_million =
            *result.investor_maximum_nonnegative_premium_million;
    } else {
        result.status = ProviderPriceLadderStatus::
            ProviderAllInFloorExceedsInvestorCeiling;
    }

    result.provider_price_model_limitation =
        "This physical-probability price ladder is not fair value or a market "
        "quote. Provider default, wrong-way risk, legal enforceability, tax, "
        "payment delay, disputes, dynamic collateral, and regulatory capital "
        "sufficiency are not modeled. The economic-capital charge is an "
        "independently supplied incremental allowance; it is not capital "
        "principal, proof of regulatory sufficiency, or a balance-sheet "
        "funding split.";

    if (pricing.coverage_selection ==
        ProviderPriceCoverageSelection::ExplicitCoverageFraction) {
        result.selected_coverage_reconciliation_error = std::abs(
            selected_point.coverage_fraction -
            *pricing.explicit_coverage_fraction);
    } else {
        result.selected_coverage_reconciliation_error = std::abs(
            selected_point.coverage_fraction -
            protection_summary.reported_coverage_fraction);
    }
    const double reconstructed_cost_recovery = checked_double(
        static_cast<long double>(costs.claim_only_robust_floor_million) +
            static_cast<long double>(
                costs.variable_claim_expense_at_robust_endpoint_million) +
            static_cast<long double>(
                costs.collateral_funding_carry_present_value_million) +
            static_cast<long double>(
                costs.risk_capital_charge_present_value_million) +
            static_cast<long double>(costs.fixed_expense_upfront_million),
        "provider price-ladder cost-recovery reconciliation");
    const double reconstructed_all_in = checked_double(
        static_cast<long double>(
            costs.provider_cost_recovery_floor_million) +
            static_cast<long double>(costs.target_profit_upfront_million),
        "provider price-ladder all-in reconciliation");
    const double reconstructed_support_gap = checked_double(
        static_cast<long double>(
            result.provider_premium_support_required_million) +
            static_cast<long double>(
                result.investor_target_restoration_required_million),
        "provider price-ladder support-gap reconciliation");
    result.support_gap_decomposition_reconciliation_error_million =
        std::abs(result.all_in_support_gap_million -
            reconstructed_support_gap);
    result.maximum_cost_ladder_reconciliation_error_million = std::max({
        std::abs(costs.provider_cost_recovery_floor_million -
            reconstructed_cost_recovery),
        std::abs(costs.provider_all_in_floor_million -
            reconstructed_all_in),
    });
    result.maximum_transformed_range_reconciliation_error_million = std::max(
        range_reconciliation_error(claim_range,
            result.provider_cost_recovery_requirement_million,
            claim_scale, deterministic_costs),
        range_reconciliation_error(claim_range,
            result.provider_all_in_revenue_requirement_million,
            claim_scale,
            checked_double(
                static_cast<long double>(deterministic_costs) +
                    static_cast<long double>(
                        costs.target_profit_upfront_million),
                "provider price-ladder all-in range reconciliation")));
    result.maximum_endpoint_probability_error = std::max(
        protection_summary.maximum_endpoint_probability_error,
        selected_point.maximum_endpoint_probability_error);
    return result;
}

ProviderPriceLadderSummary solve_provider_price_ladder(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection,
    const ProviderPriceLadderConfig& pricing) {
    validate_provider_price_ladder_config(pricing);
    const PooledLossProtectionSummary protection_summary =
        solve_pooled_loss_protection(
            portfolio, ambiguity, participation, protection);
    if (pricing.coverage_selection ==
        ProviderPriceCoverageSelection::
            ReportedInvestorTargetPassingFraction) {
        if (!protection_summary
                 .investor_target_passing_coverage_fraction_upper_bound
                 .has_value()) {
            throw std::invalid_argument(
                "provider price ladder requested a reported investor-target-passing point, but the protection solver found none");
        }
        return evaluate_provider_price_ladder(protection,
            protection_summary, protection_summary.reported, pricing);
    }

    const PooledLossProtectionRobustPoint selected_point =
        evaluate_pooled_loss_protection_coverage(portfolio, ambiguity,
            participation, protection,
            *pricing.explicit_coverage_fraction);
    return evaluate_provider_price_ladder(
        protection, protection_summary, selected_point, pricing);
}

} // namespace naturalehia::cellular_finance
