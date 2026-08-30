// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kRobustMarketPriorityCapModelVersion{
    "0.1.0"};
inline constexpr std::size_t kRobustMarketPriorityCapMaximumCandidates{
    1'024U};
inline constexpr std::size_t
    kRobustMarketPriorityCapMaximumStructuralWorkUnits{4'000'000U};

// The twelve mandate fields and their optional pass results deliberately use
// the same definitions as the robust capital-mobilization frontier. The cap
// term classifies them as fixed-structure, cap-sensitive market, and optional
// junior-concession tests rather than treating all failures as curable by B.
using RobustMarketPriorityCapConstraints =
    RobustCapitalMobilizationFrontierConstraints;
using RobustMarketPriorityCapConstraintPasses =
    RobustCapitalMobilizationConstraintPasses;

struct RobustMarketPriorityCapConfig {
    std::string model_version{kRobustMarketPriorityCapModelVersion};
    std::string scenario_label{
        "unnamed synthetic robust market priority-cap term"};
    std::string source_note{
        "Unvalidated synthetic market priority-cap mandate"};
    bool synthetic_inputs{true};

    // The base stack's first tranche must be this funded junior residual
    // claim; its second tranche must be this market-facing priority claim.
    std::string junior_claim_id{"catalytic-first-loss"};
    std::string market_claim_id{"market-priority"};

    // The finite grid must contain literal zero, this contractual ceiling,
    // and the market cap already present in the supplied base stack. No
    // interpolation or continuous optimization is performed.
    std::vector<double> market_priority_nonprincipal_cap_million_grid{};
    double contractual_ceiling_million{0.0};

    // Junior concession is max(0, target - robust junior NPV), using the
    // junior claim's already fixed physical hurdle from the base stack.
    double junior_target_npv_million{0.0};
    RobustMarketPriorityCapConstraints constraints{};
};

enum class RobustMarketPriorityCapStatus {
    FixedStructureIneligible,
    NoTestedMarketAdequateCap,
    MarketAndJuniorRequirementsDoNotOverlap,
    MinimumTestedMarketAdequateCapFound,
    MinimumTestedBalancedCapFound,
};

[[nodiscard]] std::string_view to_string(
    RobustMarketPriorityCapStatus status) noexcept;

// All residuals are absolute. Probability and tail fields include the custom
// expired-cap projection as well as the public stack-polytope evaluator.
struct RobustMarketPriorityCapCandidateAudit {
    double maximum_commitment_identity_error_million{0.0};
    double maximum_reserve_roll_forward_error_million{0.0};
    double maximum_reserve_shortfall_million{0.0};
    double maximum_subscription_reconciliation_error_million{0.0};
    double maximum_pool_cost_call_reconciliation_error_million{0.0};
    double maximum_principal_distribution_reconciliation_error_million{0.0};
    double maximum_nonprincipal_distribution_reconciliation_error_million{
        0.0};
    double maximum_priority_nonprincipal_cap_violation_million{0.0};
    double maximum_realized_loss_reconciliation_error_million{0.0};
    double maximum_unresolved_exposure_reconciliation_error_million{0.0};
    double maximum_nominal_net_cash_reconciliation_error_million{0.0};
    double maximum_stack_npv_reconciliation_error_million{0.0};
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
    double maximum_wal_root_objective_reconciliation_error_million_years{
        0.0};
    double maximum_wal_root_objective_absolute_residual_million_years{0.0};
};

struct RobustMarketPriorityCapCandidate {
    double market_priority_nonprincipal_cap_million{0.0};
    double market_notional_million{0.0};

    ProbabilityPolytopeMetricRange aggregate_fully_funded_npv_million{};

    ProbabilityPolytopeMetricRange junior_expected_contributions_million{};
    ProbabilityPolytopeMetricRange
        junior_expected_nonprincipal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange
        junior_expected_total_distributions_million{};
    ProbabilityPolytopeMetricRange junior_expected_scenario_cash_multiple{};
    ProbabilityPolytopeMetricRange
        junior_expected_scenario_net_return_fraction{};
    ProbabilityPolytopeMetricRange junior_npv_million{};

    ProbabilityPolytopeMetricRange market_expected_contributions_million{};
    ProbabilityPolytopeMetricRange
        market_expected_principal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange
        market_expected_nonprincipal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange
        market_expected_total_distributions_million{};
    ProbabilityPolytopeMetricRange market_expected_scenario_cash_multiple{};
    ProbabilityPolytopeMetricRange
        market_expected_scenario_net_return_fraction{};
    ProbabilityPolytopeMetricRange market_npv_million{};
    ProbabilityPolytopeMetricRange
        market_expired_priority_cap_capacity_million{};

    ProbabilityPolytopeMetricRange market_expected_loss_fraction{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_principal_loss_es95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_principal_loss_es99_million{};
    ProbabilityPolytopeMetricRange market_principal_impairment_probability{};
    ProbabilityPolytopeMetricRange market_negative_npv_probability{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_npv_shortfall_es95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_npv_shortfall_es99_million{};
    std::optional<CapitalStackProbabilityPolytopeWalRange>
        market_principal_cash_wal_years{};

    double robust_aggregate_npv_million{0.0};
    double robust_junior_npv_million{0.0};
    double junior_npv_concession_million{0.0};
    double robust_market_npv_million{0.0};
    double robust_market_npv_margin_fraction{0.0};
    double worst_market_expected_loss_fraction{0.0};
    double worst_market_principal_loss_es95_fraction{0.0};
    double worst_market_principal_loss_es99_fraction{0.0};
    double worst_market_principal_impairment_probability{0.0};
    double worst_market_negative_npv_probability{0.0};
    double worst_market_npv_shortfall_es95_fraction{0.0};
    double worst_market_npv_shortfall_es99_fraction{0.0};
    std::optional<double> worst_market_wal_years{};

    RobustMarketPriorityCapConstraintPasses constraint_passes{};
    bool fixed_structure_eligible{false};
    bool cap_sensitive_market_mandates_pass{false};
    bool market_adequate{false};
    bool junior_concession_limit_passes{false};
    bool balanced{false};
    RobustMarketPriorityCapCandidateAudit audit{};
};

struct RobustMarketPriorityCapGridAudit {
    bool base_stack_was_not_mutated{false};
    bool market_contributions_are_invariant{false};
    bool market_principal_cash_is_invariant{false};
    bool market_principal_risk_is_invariant{false};
    bool market_principal_wal_is_invariant{false};
    bool market_nonprincipal_cash_is_nondecreasing{false};
    bool market_path_npv_is_nondecreasing{false};
    bool junior_nonprincipal_cash_is_nonincreasing{false};
    bool junior_path_npv_is_nonincreasing{false};
    bool market_negative_npv_probability_is_nonincreasing{false};
    bool market_npv_shortfall_tails_are_nonincreasing{false};
    bool market_cash_gained_equals_junior_cash_surrendered{false};
    bool aggregate_cash_is_invariant{false};
    bool pool_hurdle_npv_is_invariant{false};

    double maximum_market_contribution_change_million{0.0};
    double maximum_market_principal_cash_change_million{0.0};
    double maximum_market_principal_risk_change{0.0};
    double maximum_market_wal_change_years{0.0};
    double maximum_market_nonprincipal_monotonicity_violation_million{0.0};
    double maximum_market_path_npv_monotonicity_violation_million{0.0};
    double maximum_junior_nonprincipal_monotonicity_violation_million{0.0};
    double maximum_junior_path_npv_monotonicity_violation_million{0.0};
    double maximum_negative_npv_probability_monotonicity_violation{0.0};
    double maximum_npv_shortfall_tail_monotonicity_violation_million{0.0};
    double maximum_cash_transfer_reconciliation_error_million{0.0};
    double maximum_aggregate_cash_change_million{0.0};
    double maximum_pool_hurdle_npv_change_million{0.0};
};

struct RobustMarketPriorityCapSummary {
    double fixed_underlying_success_participation_fraction{0.0};
    double fixed_junior_first_loss_million{0.0};
    double aggregate_commitment_and_stack_detachment_million{0.0};
    double fixed_market_notional_million{0.0};
    double fixed_junior_annual_physical_hurdle_rate{0.0};
    double fixed_market_annual_physical_hurdle_rate{0.0};
    double base_reference_market_priority_cap_million{0.0};
    double contractual_ceiling_million{0.0};

    std::size_t portfolio_cash_record_count{0U};
    std::size_t portfolio_auxiliary_record_count{0U};
    std::size_t portfolio_record_count{0U};
    std::size_t probability_projection_work_units{0U};
    std::size_t cash_path_work_units{0U};
    std::size_t structural_work_units{0U};
    std::size_t structural_work_unit_limit{
        kRobustMarketPriorityCapMaximumStructuralWorkUnits};

    std::vector<double> evaluated_market_priority_cap_million_grid{};
    std::size_t declared_fixed_structure_constraint_count{0U};
    std::size_t declared_cap_sensitive_market_constraint_count{0U};
    bool junior_concession_constraint_is_declared{false};

    std::vector<RobustMarketPriorityCapCandidate> candidates{};
    std::vector<std::size_t> market_adequate_candidate_indices{};
    std::vector<std::size_t> balanced_candidate_indices{};
    std::optional<std::size_t>
        minimum_tested_market_adequate_candidate_index{};
    std::optional<std::size_t>
        previous_tested_candidate_before_market_adequate_index{};
    std::optional<std::size_t> minimum_tested_balanced_candidate_index{};
    std::optional<std::size_t>
        previous_tested_candidate_before_balanced_index{};
    std::size_t base_reference_candidate_index{0U};
    std::size_t contractual_ceiling_candidate_index{0U};

    RobustMarketPriorityCapStatus status{
        RobustMarketPriorityCapStatus::NoTestedMarketAdequateCap};
    RobustMarketPriorityCapGridAudit grid_audit{};

    bool continuous_minimum_or_optimized_contract_is_claimed{false};
    bool market_hurdle_is_solved_or_empirically_calibrated{false};
    bool expected_investor_return_or_annualized_yield_is_estimated{false};
    bool fair_value_issue_price_or_market_spread_is_estimated{false};
    bool investor_demand_or_suitability_is_established{false};
    bool legal_form_enforceability_or_regulatory_treatment_is_validated{false};
    bool capital_mobilization_or_crowding_in_is_established{false};
    std::string model_limitation{};
};

void validate_robust_market_priority_cap_config(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& cap_term);

// Evaluates the public event-polytope stack exactly once for each tested cap.
// It copies the supplied stack and changes only the market claim's lifetime
// priority cap. The result is a finite physical-measure adequacy sensitivity,
// not a price, expected return, coupon, yield, or continuous optimum.
[[nodiscard]] RobustMarketPriorityCapSummary
evaluate_robust_market_priority_cap(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& cap_term);

} // namespace naturalehia::cellular_finance
