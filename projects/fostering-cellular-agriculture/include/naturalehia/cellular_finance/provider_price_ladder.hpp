// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/pooled_loss_protection.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kProviderPriceLadderModelVersion{"0.1.0"};

enum class ProviderPriceCoverageSelection : unsigned char {
    ExplicitCoverageFraction,
    ReportedInvestorTargetPassingFraction,
};

// A deliberately narrow provider cost-recovery sensitivity. All rates are
// annual effective rates and all amount fields are in the portfolio's stated
// million-currency unit. The assertions are declarations supplied by the
// analyst; this engine does not prove them.
struct ProviderPriceLadderConfig {
    std::string model_version{kProviderPriceLadderModelVersion};
    std::string scenario_label{
        "unnamed synthetic provider price-ladder analysis"};
    std::string source_note{
        "Unvalidated synthetic provider cost terms for mechanics testing"};
    bool synthetic_inputs{true};

    ProviderPriceCoverageSelection coverage_selection{
        ProviderPriceCoverageSelection::
            ReportedInvestorTargetPassingFraction};
    std::optional<double> explicit_coverage_fraction{};

    bool cost_bases_use_contractual_maximum_exposure{false};
    bool collateral_and_capital_are_held_until_settlement{false};
    bool variable_claim_expense_is_paid_at_claim_settlement{false};
    bool fixed_expense_and_target_profit_are_month_zero_values{false};
    // Prevents claim payment, collateral carry, economic-capital charge,
    // operating expense, or profit from being knowingly entered twice.
    bool incremental_cost_terms_are_separate_and_nonduplicative{false};

    double collateral_fraction_of_contractual_maximum_exposure{0.0};
    double collateral_annual_effective_funding_rate{0.0};
    double collateral_annual_effective_yield_rate{0.0};

    double risk_capital_fraction_of_contractual_maximum_exposure{0.0};
    double risk_capital_annual_effective_charge_rate{0.0};

    double fixed_expense_upfront_million{0.0};
    double variable_claim_expense_fraction{0.0};
    double target_profit_upfront_million{0.0};

    // Version 0.1 has no state in which the external provider fails and does
    // not estimate a risk-neutral or market fair value.
    bool provider_default_risk_is_modeled{false};
    bool fair_value_is_claimed{false};
};

enum class ProviderPriceLadderStatus : unsigned char {
    RobustAllInPremiumIntervalExists,
    InvestorCannotPayNonnegativePremium,
    ProviderAllInFloorExceedsInvestorCeiling,
};

struct ProviderPriceLadderCostBreakdown {
    double contractual_maximum_exposure_million{0.0};
    double modeled_maximum_claim_million{0.0};
    double settlement_years{0.0};
    double provider_hurdle_accumulation_factor{1.0};

    double claim_only_robust_floor_million{0.0};
    double variable_claim_expense_at_robust_endpoint_million{0.0};

    double collateral_base_million{0.0};
    double collateral_funding_carry_present_value_million{0.0};

    double risk_capital_base_million{0.0};
    double risk_capital_charge_present_value_million{0.0};

    double fixed_expense_upfront_million{0.0};
    double provider_cost_recovery_floor_million{0.0};
    double target_profit_upfront_million{0.0};
    double provider_all_in_floor_million{0.0};
};

struct ProviderPriceLadderSummary {
    ProviderPriceLadderStatus status{
        ProviderPriceLadderStatus::InvestorCannotPayNonnegativePremium};
    PooledLossProtectionSolveStatus protection_solve_status{
        PooledLossProtectionSolveStatus::NoGrossReferenceLoss};
    ProviderPriceCoverageSelection coverage_selection{
        ProviderPriceCoverageSelection::
            ReportedInvestorTargetPassingFraction};

    // The complete selected point is retained so every underlying investor,
    // claim, tail, and probability-witness disclosure remains available.
    PooledLossProtectionRobustPoint selected_protection_point{};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    bool selected_point_meets_investor_target_before_premium{false};

    ProviderPriceLadderCostBreakdown costs{};

    // A positive affine transformation of the selected point's provider
    // claim-PV range. Endpoint scenario weights are copied unchanged.
    AmbiguityMetricRange provider_cost_recovery_requirement_million{};
    AmbiguityMetricRange provider_all_in_revenue_requirement_million{};

    double investor_signed_premium_headroom_million{0.0};
    std::optional<double> investor_maximum_nonnegative_premium_million{};
    std::optional<double> robust_price_interval_lower_bound_million{};
    std::optional<double> robust_price_interval_upper_bound_million{};
    double signed_all_in_bilateral_margin_million{0.0};
    double cost_recovery_support_gap_million{0.0};
    // These two economically distinct amounts sum to all_in_support_gap.
    // The first compensates the provider after using any non-negative
    // investor premium capacity; the second repairs a pre-premium investor
    // target deficit.
    double provider_premium_support_required_million{0.0};
    double investor_target_restoration_required_million{0.0};
    double all_in_support_gap_million{0.0};
    bool robust_all_in_nonnegative_premium_interval_exists{false};

    bool provider_default_risk_is_modeled{false};
    bool fair_value_is_estimated{false};
    bool legal_enforceability_is_validated{false};
    bool regulatory_capital_sufficiency_is_validated{false};
    std::string provider_price_model_limitation{};

    double selected_coverage_reconciliation_error{0.0};
    double maximum_cost_ladder_reconciliation_error_million{0.0};
    double support_gap_decomposition_reconciliation_error_million{0.0};
    double maximum_transformed_range_reconciliation_error_million{0.0};
    double maximum_endpoint_probability_error{0.0};
};

[[nodiscard]] std::string_view to_string(
    ProviderPriceCoverageSelection selection) noexcept;

[[nodiscard]] std::string_view to_string(
    ProviderPriceLadderStatus status) noexcept;

void validate_provider_price_ladder_config(
    const ProviderPriceLadderConfig& config);

// Prices one validated protection point. Reported-point selection requires
// the exact certified investor-target-passing point from protection_summary;
// explicit selection requires the point evaluated at the exact configured
// fraction. There is no fallback to a maximum supported but failing point.
[[nodiscard]] ProviderPriceLadderSummary evaluate_provider_price_ladder(
    const PooledLossProtectionConfig& protection,
    const PooledLossProtectionSummary& protection_summary,
    const PooledLossProtectionRobustPoint& selected_point,
    const ProviderPriceLadderConfig& pricing);

// Provenance-safe convenience entry point. It solves the protection analysis,
// selects the certified reported point or freshly projects the exact explicit
// fraction, and then evaluates the provider ladder.
[[nodiscard]] ProviderPriceLadderSummary solve_provider_price_ladder(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection,
    const ProviderPriceLadderConfig& pricing);

} // namespace naturalehia::cellular_finance
