// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>
#include <naturalehia/cellular_finance/success_participation.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kCapitalStackLegacyModelVersion{"0.1.0"};
inline constexpr std::string_view kCapitalStackModelVersion{"0.2.0"};

// Tranches are declared from first-loss to most senior. Attachment and
// detachment partition issued principal. Principal cash is paid in reverse
// order (most senior first). In v0.1 only, resolved loss and continuing
// exposure occupy the stack from attachment zero upward. V0.2 instead layers
// the exact horizon issued-principal cash shortfall from attachment zero and
// never assigns pooled asset-loss causality to a tranche.
struct CapitalStackTrancheConfig {
    std::string id{};
    double attachment_million{0.0};
    double detachment_million{0.0};
    // A ceiling on this non-residual tranche's priority allocation of actual
    // non-principal pool receipts. It is neither accrued interest nor a
    // promised payment. The first-loss residual must set this field to zero
    // and receives only cash left after every priority cap.
    double priority_nonprincipal_cap_million{0.0};
    // Physical-measure NPV sensitivity for this tranche. It is not a market
    // discount curve, fair value input, or risk-neutral pricing parameter.
    double annual_physical_hurdle_rate{0.0};
    bool is_first_loss_residual{false};
};

// Version 0.1 is deliberately fully funded. Version 0.2 preserves that legacy
// mode and adds an explicit asset-to-liability bridge: acquisition or primary-
// funding cash, contractual asset principal, and issued tranche principal are
// three separate ledgers. The default stays at v0.1 so existing programmatic
// callers do not silently opt into the new accounting contract.
struct CapitalStackConfig {
    std::string model_version{kCapitalStackLegacyModelVersion};
    std::string scenario_label{"unnamed synthetic capital-stack analysis"};
    std::string source_note{
        "Unvalidated synthetic capital-stack terms for mechanics testing"};
    bool synthetic_inputs{true};

    bool aggregate_commitment_is_fully_funded_at_par_at_month_zero{false};
    bool subscription_reserve_is_zero_yield_and_lossless{false};
    bool undrawn_commitment_cancels_and_returns_only_at_horizon{false};
    bool pool_costs_are_additional_pro_rata_calls{false};
    bool principal_cash_is_paid_most_senior_first{false};
    bool nonprincipal_cash_is_paid_to_caps_then_residual{false};
    bool tranching_does_not_change_project_cash_or_gross_loss{false};
    bool premium_discount_or_fair_value_is_claimed{false};

    // Required only in v0.2. The reserve limit is the sum, by project, of the
    // greatest declared ClaimPurchasePrice plus PrimaryProjectFunding cash in
    // any supplied state. It excludes BuyerDirectCost and is partitioned by
    // the issued tranches at par.
    bool asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero{
        false};
    // Required only in v0.2. BuyerDirectCost cash remains an additional dated
    // pro-rata call and never becomes asset or tranche principal.
    bool buyer_direct_costs_are_additional_pro_rata_calls{false};
    // Required only in v0.2. Contractual-principal and unused-reserve cash
    // above unpaid issued principal remains actual cash but enters the stated
    // non-principal waterfall; it never manufactures more issued principal.
    bool principal_base_cash_above_issued_principal_is_nonprincipal{false};
    // Required only in v0.2. Arithmetic differences between contractual asset
    // principal and acquisition/funding basis are disclosed without asserting
    // fair value, market price, or an accounting valuation conclusion.
    bool principal_limit_capacity_difference_is_reported_without_valuation_claim{
        false};

    // Fixes the underlying participation right at one declared q. The stack
    // reports whether that q meets the participation term's robust NPV target,
    // but never re-solves q to improve either the pool or a tranche result.
    double underlying_success_participation_fraction{0.0};
    std::vector<CapitalStackTrancheConfig> tranches{};
};

struct CapitalStackMonthlyTrancheCashFlow {
    std::size_t month{0U};
    double par_subscription_million{0.0};
    double pro_rata_buyer_direct_cost_call_million{0.0};
    double pro_rata_pool_cost_call_million{0.0};
    double underlying_principal_cash_distribution_million{0.0};
    double unused_reserve_principal_return_million{0.0};
    double principal_cash_distribution_million{0.0};
    double contractual_principal_surplus_cash_distribution_million{0.0};
    double unused_reserve_surplus_cash_distribution_million{0.0};
    double underlying_nonprincipal_cash_distribution_million{0.0};
    double nonprincipal_cash_distribution_million{0.0};
    double net_cash_flow_million{0.0};
};

struct CapitalStackTrancheScenarioResult {
    std::string tranche_id{};
    // False in v0.2. The two legacy scalar fields below then remain zero
    // compatibility placeholders and must not be interpreted as zero loss,
    // impairment, recovery, or exposure.
    bool legacy_v01_loss_layering_metrics_are_applicable{false};
    double notional_million{0.0};
    double par_subscription_million{0.0};
    double pro_rata_buyer_direct_cost_calls_million{0.0};
    double pro_rata_pool_cost_calls_million{0.0};
    double total_contributions_million{0.0};
    double underlying_principal_cash_distribution_million{0.0};
    double unused_reserve_principal_return_million{0.0};
    double principal_cash_distribution_million{0.0};
    double contractual_principal_surplus_cash_distribution_million{0.0};
    double unused_reserve_surplus_cash_distribution_million{0.0};
    double underlying_nonprincipal_cash_distribution_million{0.0};
    double nonprincipal_cash_distribution_million{0.0};
    double total_distributions_million{0.0};

    // V0.1-only asset-resolution classification. V0.2 does not attribute a
    // pooled liability cash shortfall to resolved versus continuing assets;
    // it reports principal_cash_shortfall_million instead.
    double realized_principal_loss_million{0.0};
    double unresolved_principal_exposure_million{0.0};
    double principal_cash_shortfall_million{0.0};
    double unused_priority_nonprincipal_capacity_million{0.0};

    double nominal_net_cash_million{0.0};
    double npv_at_tranche_hurdle_million{0.0};
    double all_in_cash_shortfall_million{0.0};
    double cash_multiple{0.0};
    double net_return_fraction{0.0};
    double principal_cash_time_million_years{0.0};
    std::optional<double> weighted_average_principal_cash_month{};
    std::vector<CapitalStackMonthlyTrancheCashFlow> monthly_cash_flows{};
};

struct CapitalStackScenarioResult {
    std::string scenario_id{};
    double central_weight{0.0};
    std::string model_version{};
    bool uses_explicit_asset_liability_accounting{false};
    // Sum of Portfolio project cash-outlay limits. It is not necessarily the
    // v0.2 issued principal because BuyerDirectCost is excluded from reserve.
    double aggregate_project_outlay_limit_million{0.0};
    double aggregate_contractual_asset_principal_limit_million{0.0};
    double aggregate_commitment_million{0.0};
    double total_project_draws_million{0.0};
    double total_asset_acquisition_and_primary_funding_uses_million{0.0};
    double total_claim_purchase_price_million{0.0};
    double total_primary_project_funding_million{0.0};
    double total_buyer_direct_costs_million{0.0};
    // Contractual-principal limit less the scenario's acquisition and primary-
    // funding uses. This is a limit-capacity diagnostic only: it is not the
    // scenario principal created, a purchase discount or premium, fair value,
    // or an accounting valuation conclusion.
    double contractual_principal_limit_minus_funding_uses_million{0.0};
    double unused_commitment_returned_at_horizon_million{0.0};
    double underlying_principal_cash_million{0.0};
    double distributable_principal_cash_million{0.0};
    double contractual_principal_surplus_cash_million{0.0};
    double unused_reserve_surplus_cash_million{0.0};
    double underlying_nonprincipal_cash_million{0.0};
    double distributable_nonprincipal_cash_million{0.0};
    double total_pool_costs_million{0.0};
    // Exact asset-ledger observables. They are preserved separately from the
    // issued-principal cash shortfall and are never causally assigned to a
    // v0.2 liability layer.
    double contractual_asset_principal_loss_million{0.0};
    double contractual_asset_outstanding_principal_million{0.0};
    // Exact cash shortfall against issued principal at the analysis horizon.
    // It is not attributed to resolved or continuing assets and is not an
    // accounting impairment or assumed post-horizon recovery value.
    double issued_principal_cash_shortfall_million{0.0};
    double underlying_on_demand_npv_million{0.0};
    double fully_funded_stack_npv_at_pool_hurdle_million{0.0};
    // Underlying on-demand NPV minus fully-funded stack NPV. Non-negative at
    // a non-negative hurdle when reserve cash is returned no earlier than the
    // project draws it replaces.
    double prefunding_drag_npv_million{0.0};
    double underlying_nominal_net_cash_million{0.0};
    double stack_nominal_net_cash_million{0.0};
    std::vector<CapitalStackTrancheScenarioResult> tranches{};
};

struct CapitalStackTrancheSummary {
    std::string tranche_id{};
    double attachment_million{0.0};
    double detachment_million{0.0};
    double notional_million{0.0};
    double priority_nonprincipal_cap_million{0.0};
    double annual_physical_hurdle_rate{0.0};
    bool is_first_loss_residual{false};
    // When false, every legacy loss/exposure/impairment/exhaustion and legacy
    // principal-loss ES field below is an inapplicable zero placeholder.
    // V0.2 controlling risk fields are the cash-shortfall fields.
    bool legacy_v01_loss_layering_metrics_are_applicable{false};

    AmbiguityMetricRange expected_contributions_million{};
    AmbiguityMetricRange expected_buyer_direct_cost_calls_million{};
    AmbiguityMetricRange expected_underlying_principal_cash_distribution_million{};
    AmbiguityMetricRange expected_unused_reserve_principal_return_million{};
    AmbiguityMetricRange expected_principal_cash_distribution_million{};
    AmbiguityMetricRange expected_nonprincipal_cash_distribution_million{};
    AmbiguityMetricRange expected_total_distributions_million{};
    // V0.1-only fields; consult the applicability flag above.
    AmbiguityMetricRange expected_realized_principal_loss_million{};
    AmbiguityMetricRange expected_realized_principal_loss_fraction{};
    AmbiguityMetricRange expected_unresolved_principal_exposure_million{};
    AmbiguityMetricRange expected_principal_cash_shortfall_million{};
    AmbiguityMetricRange expected_npv_at_tranche_hurdle_million{};
    AmbiguityMetricRange expected_all_in_cash_shortfall_million{};
    // E[scenario cash multiple] and E[scenario net-return fraction]. These are
    // expectations of pathwise ratios, not ratios made from independently
    // optimized distribution and contribution endpoints.
    AmbiguityMetricRange expected_scenario_cash_multiple{};
    AmbiguityMetricRange expected_scenario_net_return_fraction{};
    // V0.1-only event fields; consult the applicability flag above.
    AmbiguityMetricRange principal_impairment_probability{};
    AmbiguityMetricRange principal_exhaustion_probability{};
    AmbiguityMetricRange principal_cash_shortfall_probability{};
    AmbiguityMetricRange full_principal_cash_shortfall_probability{};
    AmbiguityMetricRange negative_npv_probability{};
    // V0.1-only tail fields; consult the applicability flag above.
    AmbiguityMetricRange principal_loss_expected_shortfall_95_million{};
    AmbiguityMetricRange principal_loss_expected_shortfall_99_million{};
    AmbiguityMetricRange
        principal_cash_shortfall_expected_shortfall_95_million{};
    AmbiguityMetricRange
        principal_cash_shortfall_expected_shortfall_99_million{};
    AmbiguityMetricRange npv_shortfall_expected_shortfall_95_million{};
    AmbiguityMetricRange npv_shortfall_expected_shortfall_99_million{};
    // Common-witness ratio E[principal cash time]/E[principal cash], in
    // years. Absent if some feasible probability measure has expected
    // principal cash at or below the 1e-10-million numerical tolerance.
    // Numerator and denominator endpoints are never divided.
    std::optional<AmbiguityMetricRange>
        principal_cash_weighted_average_life_years{};
    bool central_expected_npv_meets_hurdle{false};
    bool robust_expected_npv_meets_hurdle{false};
};

struct CapitalStackSummary {
    std::string model_version{};
    bool uses_explicit_asset_liability_accounting{false};
    bool legacy_v01_loss_layering_metrics_are_applicable{false};
    double underlying_success_participation_fraction{0.0};
    double underlying_target_worst_expected_npv_million{0.0};
    double selected_underlying_target_gap_million{0.0};
    bool selected_underlying_success_participation_meets_target{false};
    double aggregate_project_outlay_limit_million{0.0};
    double aggregate_contractual_asset_principal_limit_million{0.0};
    double aggregate_commitment_million{0.0};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    std::vector<CapitalStackScenarioResult> scenarios{};
    std::vector<CapitalStackTrancheSummary> tranches{};

    AmbiguityMetricRange expected_underlying_on_demand_npv_million{};
    AmbiguityMetricRange expected_fully_funded_stack_npv_at_pool_hurdle_million{};
    AmbiguityMetricRange expected_prefunding_drag_npv_million{};
    AmbiguityMetricRange expected_contractual_asset_principal_loss_million{};
    AmbiguityMetricRange
        expected_contractual_asset_outstanding_principal_million{};
    AmbiguityMetricRange expected_issued_principal_cash_shortfall_million{};

    bool gross_project_principal_loss_is_changed{false};
    bool project_cash_is_changed_by_tranching{false};
    bool fair_value_or_market_price_is_estimated{false};
    bool legal_enforceability_is_validated{false};
    bool ratings_or_regulatory_capital_are_validated{false};
    std::string model_limitation{};

    double maximum_commitment_identity_error_million{0.0};
    double maximum_reserve_roll_forward_error_million{0.0};
    double maximum_reserve_shortfall_million{0.0};
    double maximum_subscription_reconciliation_error_million{0.0};
    double maximum_pool_cost_call_reconciliation_error_million{0.0};
    double maximum_buyer_direct_cost_call_reconciliation_error_million{0.0};
    double maximum_principal_distribution_reconciliation_error_million{0.0};
    double maximum_contractual_principal_surplus_reconciliation_error_million{
        0.0};
    double maximum_unused_reserve_surplus_reconciliation_error_million{0.0};
    double maximum_nonprincipal_distribution_reconciliation_error_million{0.0};
    double maximum_priority_nonprincipal_cap_violation_million{0.0};
    double maximum_realized_loss_reconciliation_error_million{0.0};
    double maximum_contractual_asset_loss_preservation_error_million{0.0};
    double maximum_contractual_asset_outstanding_preservation_error_million{
        0.0};
    double maximum_unresolved_exposure_reconciliation_error_million{0.0};
    double maximum_nominal_net_cash_reconciliation_error_million{0.0};
    double maximum_stack_npv_reconciliation_error_million{0.0};
    double maximum_wal_ratio_objective_residual_million_years{0.0};
    double maximum_endpoint_probability_error{0.0};
};

void validate_capital_stack_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack);

// Applies the fixed participation fraction, then allocates only its actual
// portfolio cash. Principal and non-principal waterfalls are separate. No
// project path, gross loss, probability, or external protection leg is added.
[[nodiscard]] CapitalStackSummary evaluate_capital_stack(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack);

} // namespace naturalehia::cellular_finance
