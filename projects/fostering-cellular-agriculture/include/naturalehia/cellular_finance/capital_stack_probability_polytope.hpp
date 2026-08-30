// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/capital_stack.hpp>
#include <naturalehia/cellular_finance/probability_polytope.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view
    kCapitalStackProbabilityPolytopeModelVersion{"0.2.0"};

// A common-measure ratio endpoint for expected principal-cash weighted
// average life. The full probability witness is retained. Numerator and
// denominator are recomputed from that witness and are never optimized or
// divided independently. The root fields audit the last linear projection of
// N-rD used by the floating-point ratio solver; they are not a symbolic
// certificate or an independent dual proof.
struct CapitalStackProbabilityPolytopeWalEndpoint {
    double value_years{0.0};
    std::vector<double> scenario_weights{};

    double numerator_million_years{0.0};
    double denominator_million{0.0};
    double root_ratio_input_years{0.0};
    double root_objective_value_million_years{0.0};

    double numerator_reconciliation_error_million_years{0.0};
    double denominator_reconciliation_error_million{0.0};
    double ratio_reconciliation_error_years{0.0};
    double root_objective_reconciliation_error_million_years{0.0};
    double root_objective_absolute_residual_million_years{0.0};

    double maximum_constraint_violation{0.0};
    double objective_reconciliation_error{0.0};
    double optimality_residual{0.0};
};

struct CapitalStackProbabilityPolytopeWalRange {
    CapitalStackProbabilityPolytopeWalEndpoint minimum{};
    double central_years{0.0};
    double central_numerator_million_years{0.0};
    double central_denominator_million{0.0};
    double central_ratio_reconciliation_error_years{0.0};
    CapitalStackProbabilityPolytopeWalEndpoint maximum{};
};

struct CapitalStackProbabilityPolytopeTrancheSummary {
    std::string tranche_id{};
    double attachment_million{0.0};
    double detachment_million{0.0};
    double notional_million{0.0};
    double priority_nonprincipal_cap_million{0.0};
    double annual_physical_hurdle_rate{0.0};
    bool is_first_loss_residual{false};

    ProbabilityPolytopeMetricRange expected_contributions_million{};
    ProbabilityPolytopeMetricRange
        expected_underlying_principal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange
        expected_unused_reserve_principal_return_million{};
    ProbabilityPolytopeMetricRange
        expected_principal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange
        expected_nonprincipal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange expected_total_distributions_million{};
    ProbabilityPolytopeMetricRange
        expected_realized_principal_loss_million{};
    ProbabilityPolytopeMetricRange
        expected_realized_principal_loss_fraction{};
    ProbabilityPolytopeMetricRange
        expected_unresolved_principal_exposure_million{};
    ProbabilityPolytopeMetricRange
        expected_principal_cash_shortfall_million{};
    ProbabilityPolytopeMetricRange
        expected_npv_at_tranche_hurdle_million{};
    ProbabilityPolytopeMetricRange
        expected_all_in_cash_shortfall_million{};
    // These remain expectations of pathwise ratios, not ratios of separately
    // optimized aggregate cash endpoints.
    ProbabilityPolytopeMetricRange expected_scenario_cash_multiple{};
    ProbabilityPolytopeMetricRange expected_scenario_net_return_fraction{};
    ProbabilityPolytopeMetricRange principal_impairment_probability{};
    ProbabilityPolytopeMetricRange principal_exhaustion_probability{};
    ProbabilityPolytopeMetricRange negative_npv_probability{};

    ProbabilityPolytopeUpperExpectedShortfallProjection
        principal_loss_expected_shortfall_95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        principal_loss_expected_shortfall_99_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        npv_shortfall_expected_shortfall_95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        npv_shortfall_expected_shortfall_99_million{};

    // Absent if the minimum feasible expected principal cash is at or below
    // 1e-10 million. No ratio is then reported, even if the central measure
    // has positive principal cash.
    std::optional<CapitalStackProbabilityPolytopeWalRange>
        principal_cash_weighted_average_life_years{};
    bool central_expected_npv_meets_hurdle{false};
    bool robust_expected_npv_meets_hurdle{false};
};

// Event-constrained risk results over the fixed deterministic cash paths
// produced by the v0.1 stack waterfall. All probability endpoints below are
// new v0.2 projections. No v0.1 ambiguity range or target conclusion is
// coerced into this type.
struct CapitalStackProbabilityPolytopeSummary {
    double underlying_success_participation_fraction{0.0};
    double underlying_target_worst_expected_npv_million{0.0};
    double selected_underlying_target_gap_million{0.0};
    bool selected_underlying_success_participation_meets_target{false};
    double aggregate_commitment_million{0.0};

    std::vector<ProbabilityPolytopeScenario> scenario_probabilities{};
    std::vector<ProbabilityEventConstraint> events{};
    std::vector<CapitalStackScenarioResult> scenarios{};
    std::vector<CapitalStackProbabilityPolytopeTrancheSummary> tranches{};

    ProbabilityPolytopeMetricRange
        expected_underlying_on_demand_npv_million{};
    ProbabilityPolytopeMetricRange
        expected_fully_funded_stack_npv_at_pool_hurdle_million{};
    ProbabilityPolytopeMetricRange expected_prefunding_drag_npv_million{};

    bool gross_project_principal_loss_is_changed{false};
    bool project_cash_is_changed_by_tranching{false};
    bool fair_value_or_market_price_is_estimated{false};
    bool legal_enforceability_is_validated{false};
    bool ratings_or_regulatory_capital_are_validated{false};
    std::string model_limitation{};

    // Path and accounting controls copied from the deterministic waterfall.
    // They do not validate the probability constraints or market value.
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

    // Aggregate audits for every published v0.2 linear, tail, and WAL solve.
    // Reduced-cost fields describe the final simplex tableau and are not an
    // independent dual-gap certificate.
    double maximum_probability_constraint_violation{0.0};
    double maximum_objective_reconciliation_error{0.0};
    double maximum_reduced_cost_optimality_residual{0.0};
    double maximum_tail_mass_violation{0.0};
    double maximum_tail_objective_reconciliation_error{0.0};
    double maximum_tail_threshold_formula_reconciliation_error{0.0};
    double maximum_tail_threshold_enumeration_optimality_residual{0.0};
    double maximum_wal_numerator_reconciliation_error_million_years{0.0};
    double maximum_wal_denominator_reconciliation_error_million{0.0};
    double maximum_wal_ratio_reconciliation_error_years{0.0};
    double maximum_wal_root_objective_reconciliation_error_million_years{0.0};
    double maximum_wal_root_objective_absolute_residual_million_years{0.0};
};

void validate_capital_stack_probability_polytope(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack);

// Applies the stack's one declared participation fraction, evaluates the
// deterministic v0.1 waterfall once under private ledger-only [0,1] bounds,
// then independently projects every reported financial metric over the
// supplied scenario/event probability polytope. The private bounds have no
// financial interpretation and none of their v0.1 risk outputs are consumed.
[[nodiscard]] CapitalStackProbabilityPolytopeSummary
evaluate_capital_stack_probability_polytope(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack);

} // namespace naturalehia::cellular_finance
