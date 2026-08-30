// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/provider_credit_stress_config.hpp>
#include <naturalehia/cellular_finance/provider_price_ladder.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace naturalehia::cellular_finance {

// One provider-performance atom conditional on one original portfolio
// scenario. expanded_central_weight is a reporting weight only: ambiguity is
// never applied independently to these atoms.
struct ProviderCreditOutcomeResult {
    std::string outcome_id{};
    double conditional_weight{0.0};
    double expanded_central_weight{0.0};
    bool provider_performs{true};

    std::size_t settlement_month{0U};
    std::size_t unsecured_recovery_month{0U};
    double gross_contractual_claim_million{0.0};
    double full_claim_present_value_million{0.0};

    // The grown pledged account is a memo amount. Only collateral_applied is
    // transferred, and it is capped at the contractual claim.
    double pledged_collateral_available_at_settlement_million{0.0};
    double collateral_realization_fraction{0.0};
    double unsecured_recovery_fraction{0.0};
    double direct_provider_payment_at_settlement_million{0.0};
    double collateral_applied_at_settlement_million{0.0};
    double unsecured_exposure_at_default_million{0.0};
    double delayed_unsecured_recovery_million{0.0};
    // Nominal claim amount still unpaid after the modeled recovery. Together,
    // direct payment, collateral, delayed recovery, and this amount reconcile
    // exactly to the unchanged contractual claim.
    double ultimate_unpaid_claim_million{0.0};
    double actual_support_received_nominal_million{0.0};

    double collateral_received_present_value_million{0.0};
    double unsecured_recovery_present_value_million{0.0};
    double actual_support_received_present_value_million{0.0};
    double investor_credit_loss_present_value_million{0.0};
    double investor_npv_before_premium_million{0.0};
    double investor_npv_after_unchanged_full_performance_price_million{0.0};
};

// Conditional outcome values are collapsed inside each original portfolio
// scenario before any ambiguity projection. This preserves the declared
// portfolio ambiguity set and prevents provider states from becoming new,
// independently movable scenario probabilities.
struct ProviderCreditScenarioResult {
    std::string scenario_id{};
    double central_portfolio_weight{0.0};
    double underlying_investor_npv_million{0.0};
    double gross_project_principal_loss_million{0.0};
    double gross_contractual_claim_million{0.0};
    double full_claim_present_value_million{0.0};
    double conditional_provider_default_probability{0.0};

    double conditional_expected_direct_provider_payment_million{0.0};
    double conditional_expected_collateral_applied_million{0.0};
    double conditional_expected_delayed_unsecured_recovery_million{0.0};
    double conditional_expected_ultimate_unpaid_claim_million{0.0};
    double conditional_expected_actual_support_present_value_million{0.0};
    double conditional_expected_credit_loss_present_value_million{0.0};
    double conditional_expected_unsecured_exposure_at_default_million{0.0};
    double conditional_expected_investor_npv_before_premium_million{0.0};
    std::vector<ProviderCreditOutcomeResult> outcomes{};
};

// Central physical-measure distributions use the expanded p_s * theta_sj
// atoms. They are descriptive stress statistics, not ambiguity endpoints,
// market prices, or regulatory exposure measures.
struct ProviderCreditCentralRisk {
    PortfolioDistributionSummary gross_contractual_claim_million{};
    PortfolioDistributionSummary full_claim_present_value_million{};
    PortfolioDistributionSummary actual_support_received_present_value_million{};
    PortfolioDistributionSummary unsecured_exposure_at_default_million{};
    PortfolioDistributionSummary ultimate_unpaid_claim_million{};
    PortfolioDistributionSummary investor_credit_loss_present_value_million{};

    double provider_default_probability{0.0};
    double positive_claim_probability{0.0};
    double positive_claim_and_provider_default_probability{0.0};
    std::optional<double> provider_default_probability_given_positive_claim{};
    std::optional<double> positive_claim_probability_given_provider_default{};
    std::optional<double> expected_contractual_claim_given_provider_default_million{};
    std::optional<double> expected_unsecured_exposure_given_provider_default_million{};
    std::optional<double> claim_weighted_provider_default_rate{};
    // E[claim | default] / E[claim]. Values above one are a direct severity
    // concentration diagnostic under the central physical measure.
    std::optional<double> claim_at_default_severity_multiplier{};

    // A positive value indicates that provider default is concentrated in
    // larger contractual-claim states under the central physical measure.
    double contractual_claim_provider_default_covariance_million{0.0};
    std::optional<double> contractual_claim_provider_default_correlation{};
};

// Every range is projected over the original portfolio scenario bounds after
// conditional provider outcomes have been averaged at their fixed theta.
struct ProviderCreditRobustRisk {
    AmbiguityMetricRange provider_default_probability{};
    AmbiguityMetricRange positive_claim_and_provider_default_probability{};
    // These are unconditional joint amounts E[X * 1_default] and
    // E[U * 1_default], not conditional E[X | default] or E[U | default].
    // The central conditional severities are the separately named optional
    // fields in ProviderCreditCentralRisk.
    AmbiguityMetricRange expected_contractual_claim_at_default_million{};
    AmbiguityMetricRange expected_direct_provider_payment_million{};
    AmbiguityMetricRange expected_collateral_applied_million{};
    AmbiguityMetricRange expected_delayed_unsecured_recovery_million{};
    AmbiguityMetricRange expected_ultimate_unpaid_claim_million{};
    AmbiguityMetricRange expected_unsecured_exposure_at_default_million{};
    AmbiguityMetricRange expected_full_claim_present_value_million{};
    AmbiguityMetricRange expected_actual_support_received_present_value_million{};
    AmbiguityMetricRange expected_investor_credit_loss_present_value_million{};
    AmbiguityMetricRange investor_expected_npv_before_premium_million{};
    AmbiguityMetricRange
        investor_expected_npv_after_unchanged_full_performance_price_million{};

    // Both ratios use one common probability measure in numerator and
    // denominator. The robust endpoint carries the feasible worst witness;
    // neither ratio is formed from independently optimized range endpoints.
    std::optional<double> central_claim_present_value_delivery_ratio{};
    std::optional<AmbiguityEndpoint>
        robust_minimum_claim_present_value_delivery_ratio{};
    double robust_minimum_delivery_ratio_objective_residual_million{0.0};
};

struct ProviderCreditExposureBoundary {
    double contractual_maximum_exposure_million{0.0};
    double modeled_maximum_claim_million{0.0};
    double pledged_collateral_base_million{0.0};
    double pledged_collateral_at_settlement_million{0.0};
    double contractual_maximum_unsecured_exposure_million{0.0};
    double modeled_maximum_unsecured_exposure_million{0.0};
};

struct ProviderCreditSupportEconomics {
    // This is copied from the full-performance price ladder and must not fall
    // when the provider defaults in a stress atom.
    double unchanged_full_performance_provider_price_million{0.0};
    double full_performance_provider_price_change_million{0.0};

    // The NPV range in ProviderCreditRobustRisk with this price deducted is a
    // hypothetical in which the investor alone pays the entire provider
    // floor. It is distinct from the support-funded gap decomposition below.
    double stressed_investor_signed_premium_headroom_million{0.0};
    std::optional<double>
        stressed_investor_maximum_nonnegative_premium_million{};
    double stressed_provider_premium_support_required_million{0.0};
    double stressed_investor_target_restoration_required_million{0.0};
    double stressed_all_in_support_gap_million{0.0};

    double base_full_performance_all_in_support_gap_million{0.0};
    double incremental_counterparty_credit_support_gap_million{0.0};
};

struct ProviderCreditStressSummary {
    ProviderPriceLadderSummary full_performance_price_ladder{};
    std::string provider_id{};
    double selected_coverage_fraction{0.0};
    std::size_t settlement_month{0U};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    std::vector<ProviderCreditScenarioResult> scenarios{};

    ProviderCreditExposureBoundary exposure{};
    ProviderCreditCentralRisk central{};
    ProviderCreditRobustRisk robust{};
    ProviderCreditSupportEconomics support{};

    bool gross_contractual_claim_is_changed{false};
    bool provider_price_is_reduced_for_default{false};
    // True when the configured conditional outcomes contain at least one
    // provider-default atom. Validation requires every configured outcome to
    // have strictly positive conditional weight, so a default atom always
    // activates this flag and zero-weight placeholder atoms are invalid. An
    // all-performance reconstruction therefore reports false even though this
    // engine is capable of stress.
    bool provider_default_risk_is_modeled{false};
    bool market_cva_or_fair_value_is_estimated{false};
    bool legal_enforceability_is_validated{false};
    bool regulatory_capital_sufficiency_is_validated{false};
    std::string provider_credit_model_limitation{};

    double maximum_gross_project_loss_change_million{0.0};
    double maximum_conditional_weight_sum_error{0.0};
    double expanded_central_probability_sum_error{0.0};
    double maximum_default_waterfall_reconciliation_error_million{0.0};
    double maximum_credit_loss_reconciliation_error_million{0.0};
    double maximum_conditional_collapse_reconciliation_error_million{0.0};
    double maximum_central_probability_projection_reconciliation_error{0.0};
    double maximum_central_monetary_projection_reconciliation_error_million{
        0.0};
    double support_gap_decomposition_reconciliation_error_million{0.0};
    double maximum_probability_witness_reconciliation_error{0.0};
    double maximum_monetary_witness_reconciliation_error_million{0.0};
    double maximum_endpoint_probability_error{0.0};
};

// Provenance-safe v0.1 entry point. The full-performance protection and price
// ladder are solved first. Provider-credit states then change only delivery of
// the selected contractual claim; they do not mutate project cash, gross loss,
// contractual coverage, the ambiguity bounds, or the provider's required
// full-performance price.
[[nodiscard]] ProviderCreditStressSummary solve_provider_credit_stress(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection,
    const ProviderPriceLadderConfig& pricing,
    const ProviderCreditStressConfig& credit);

} // namespace naturalehia::cellular_finance
