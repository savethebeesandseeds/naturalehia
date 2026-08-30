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

inline constexpr std::string_view kCapitalStackModelVersion{"0.1.0"};

// Tranches are declared from first-loss to most senior. Attachment and
// detachment partition aggregate project commitment. Principal cash is paid
// in reverse order (most senior first), while realized principal loss and
// unresolved exposure occupy the stack from attachment zero upward.
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

// Version 0.1 is deliberately fully funded. The aggregate project commitment
// is subscribed at par at month zero and held in a zero-yield, lossless reserve
// until drawn. Pool costs are separate pro-rata investor calls. This makes the
// loss support real in every path and exposes the liquidity cost of prefunding.
struct CapitalStackConfig {
    std::string model_version{kCapitalStackModelVersion};
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

    // Fixes the underlying participation right at one declared q. The stack
    // reports whether that q meets the participation term's robust NPV target,
    // but never re-solves q to improve either the pool or a tranche result.
    double underlying_success_participation_fraction{0.0};
    std::vector<CapitalStackTrancheConfig> tranches{};
};

struct CapitalStackMonthlyTrancheCashFlow {
    std::size_t month{0U};
    double par_subscription_million{0.0};
    double pro_rata_pool_cost_call_million{0.0};
    double underlying_principal_cash_distribution_million{0.0};
    double unused_reserve_principal_return_million{0.0};
    double principal_cash_distribution_million{0.0};
    double nonprincipal_cash_distribution_million{0.0};
    double net_cash_flow_million{0.0};
};

struct CapitalStackTrancheScenarioResult {
    std::string tranche_id{};
    double notional_million{0.0};
    double par_subscription_million{0.0};
    double pro_rata_pool_cost_calls_million{0.0};
    double total_contributions_million{0.0};
    double underlying_principal_cash_distribution_million{0.0};
    double unused_reserve_principal_return_million{0.0};
    double principal_cash_distribution_million{0.0};
    double nonprincipal_cash_distribution_million{0.0};
    double total_distributions_million{0.0};

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
    double aggregate_commitment_million{0.0};
    double total_project_draws_million{0.0};
    double unused_commitment_returned_at_horizon_million{0.0};
    double underlying_principal_cash_million{0.0};
    double distributable_principal_cash_million{0.0};
    double distributable_nonprincipal_cash_million{0.0};
    double total_pool_costs_million{0.0};
    double gross_realized_principal_loss_million{0.0};
    double unresolved_principal_exposure_million{0.0};
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

    AmbiguityMetricRange expected_contributions_million{};
    AmbiguityMetricRange expected_underlying_principal_cash_distribution_million{};
    AmbiguityMetricRange expected_unused_reserve_principal_return_million{};
    AmbiguityMetricRange expected_principal_cash_distribution_million{};
    AmbiguityMetricRange expected_nonprincipal_cash_distribution_million{};
    AmbiguityMetricRange expected_total_distributions_million{};
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
    AmbiguityMetricRange principal_impairment_probability{};
    AmbiguityMetricRange principal_exhaustion_probability{};
    AmbiguityMetricRange negative_npv_probability{};
    AmbiguityMetricRange principal_loss_expected_shortfall_95_million{};
    AmbiguityMetricRange principal_loss_expected_shortfall_99_million{};
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
    double underlying_success_participation_fraction{0.0};
    double underlying_target_worst_expected_npv_million{0.0};
    double selected_underlying_target_gap_million{0.0};
    bool selected_underlying_success_participation_meets_target{false};
    double aggregate_commitment_million{0.0};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    std::vector<CapitalStackScenarioResult> scenarios{};
    std::vector<CapitalStackTrancheSummary> tranches{};

    AmbiguityMetricRange expected_underlying_on_demand_npv_million{};
    AmbiguityMetricRange expected_fully_funded_stack_npv_at_pool_hurdle_million{};
    AmbiguityMetricRange expected_prefunding_drag_npv_million{};

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
    double maximum_principal_distribution_reconciliation_error_million{0.0};
    double maximum_nonprincipal_distribution_reconciliation_error_million{0.0};
    double maximum_priority_nonprincipal_cap_violation_million{0.0};
    double maximum_realized_loss_reconciliation_error_million{0.0};
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
