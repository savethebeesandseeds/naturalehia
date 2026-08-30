// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/capital_stack_probability_polytope.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view
    kRobustCapitalMobilizationFrontierModelVersion{"0.1.0"};
inline constexpr std::size_t
    kRobustCapitalMobilizationFrontierMaximumCandidates{1'024U};
inline constexpr std::size_t
    kRobustCapitalMobilizationFrontierMaximumStructuralWorkUnits{4'000'000U};

// Every field is an independently declared mandate. An absent field is not
// silently assigned a market convention and does not affect feasibility.
// Aggregate NPV is the fully-funded two-claim stack's worst-case modelled
// expected NPV surplus to the portfolio hurdle. Claim NPV uses each claim's
// separately declared hurdle. These are physical-measure surplus tests, not
// expected returns, fair value, or prices.
struct RobustCapitalMobilizationFrontierConstraints {
    std::optional<double> minimum_robust_aggregate_npv_million{};
    std::optional<double> minimum_market_robust_npv_margin_fraction{};
    std::optional<double> maximum_market_expected_loss_fraction{};
    std::optional<double> maximum_market_principal_loss_es95_fraction{};
    std::optional<double> maximum_market_principal_loss_es99_fraction{};
    std::optional<double> maximum_market_principal_impairment_probability{};
    std::optional<double> maximum_market_negative_npv_probability{};
    std::optional<double> maximum_market_npv_shortfall_es95_fraction{};
    std::optional<double> maximum_market_npv_shortfall_es99_fraction{};
    std::optional<double> maximum_market_wal_years{};
    std::optional<double> maximum_catalytic_first_loss_million{};
    std::optional<double> maximum_catalytic_npv_concession_million{};
};

// Version 0.1 evaluates only this declared finite grid. It creates exactly two
// fully funded claims: funded junior loss-absorbing capital / first-loss layer
// [0,A] and a market-facing claim [A,K], senior in the modelled waterfall,
// where K is aggregate project commitment and top stack detachment. The two
// fully funded claims cover [0,K], with no hidden third claim. The market cap
// and both physical hurdle rates are fixed across the grid rather than
// optimized.
struct RobustCapitalMobilizationFrontierConfig {
    std::string model_version{
        kRobustCapitalMobilizationFrontierModelVersion};
    std::string scenario_label{
        "unnamed synthetic robust capital-mobilization frontier"};
    std::string source_note{
        "Unvalidated synthetic mandate and instrument terms"};
    bool synthetic_inputs{true};

    std::vector<double> participation_fraction_grid{};
    std::vector<double> catalytic_first_loss_million_grid{};

    std::string catalytic_claim_id{"catalytic-first-loss"};
    std::string market_claim_id{"market-priority"};
    double market_priority_nonprincipal_cap_million{0.0};
    double catalytic_annual_physical_hurdle_rate{0.0};
    double market_annual_physical_hurdle_rate{0.0};

    // Catalytic NPV concession is max(0, this target minus the minimum expected
    // catalytic NPV over the same event probability polytope). It is a modeled
    // benchmark gap, not automatically a grant, subsidy, or fiscal cost.
    double catalytic_target_npv_million{0.0};
    RobustCapitalMobilizationFrontierConstraints constraints{};
};

// A declared constraint has a bool result. An absent result means the
// corresponding constraint was not declared and did not affect feasibility.
struct RobustCapitalMobilizationConstraintPasses {
    std::optional<bool> robust_aggregate_npv{};
    std::optional<bool> market_robust_npv_margin{};
    std::optional<bool> market_expected_loss_fraction{};
    std::optional<bool> market_principal_loss_es95_fraction{};
    std::optional<bool> market_principal_loss_es99_fraction{};
    std::optional<bool> market_principal_impairment_probability{};
    std::optional<bool> market_negative_npv_probability{};
    std::optional<bool> market_npv_shortfall_es95_fraction{};
    std::optional<bool> market_npv_shortfall_es99_fraction{};
    std::optional<bool> market_wal{};
    std::optional<bool> catalytic_first_loss{};
    std::optional<bool> catalytic_npv_concession{};
};

struct RobustCapitalMobilizationCandidateAudit {
    double maximum_stack_accounting_error_million{0.0};
    double maximum_probability_constraint_violation{0.0};
    double maximum_objective_reconciliation_error{0.0};
    double maximum_reduced_cost_optimality_residual{0.0};
    double maximum_tail_mass_violation{0.0};
    double maximum_tail_objective_reconciliation_error{0.0};
    double maximum_wal_ratio_reconciliation_error_years{0.0};
    double maximum_wal_root_objective_absolute_residual_million_years{0.0};
};

struct RobustCapitalMobilizationFrontierCandidate {
    double participation_fraction{0.0};
    double catalytic_first_loss_million{0.0};
    // M=K-A is the market claim's funded principal notional. It excludes the
    // additional pro-rata pool-cost calls captured in expected contributions.
    double market_notional_million{0.0};

    // Full projections are retained so each reported endpoint remains tied to
    // its own probability witness. Ratios below divide only by fixed notional;
    // no endpoints from different projections are combined.
    ProbabilityPolytopeMetricRange aggregate_fully_funded_npv_million{};
    ProbabilityPolytopeMetricRange catalytic_npv_million{};
    ProbabilityPolytopeMetricRange market_npv_million{};
    // Contributions include funded principal and the market claim's allocated
    // pro-rata pool-cost calls; distributions are gross modeled claim cash.
    ProbabilityPolytopeMetricRange market_expected_contributions_million{};
    ProbabilityPolytopeMetricRange
        market_expected_total_distributions_million{};
    ProbabilityPolytopeMetricRange market_expected_loss_fraction{};
    // Shown beside WAL so a shorter life caused by principal write-down is not
    // mistaken for faster repayment. Its endpoints are not divided into the
    // separately optimized WAL endpoints.
    ProbabilityPolytopeMetricRange
        market_expected_principal_cash_distribution_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_principal_loss_es95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_principal_loss_es99_million{};
    // Probability of any modelled principal write-down; this is not an
    // accounting classification or a legal default determination.
    ProbabilityPolytopeMetricRange market_principal_impairment_probability{};
    ProbabilityPolytopeMetricRange market_negative_npv_probability{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_npv_shortfall_es95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        market_npv_shortfall_es99_million{};
    std::optional<CapitalStackProbabilityPolytopeWalRange>
        market_principal_cash_wal_years{};

    double robust_aggregate_npv_million{0.0};
    double robust_catalytic_npv_million{0.0};
    double catalytic_npv_concession_million{0.0};
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

    RobustCapitalMobilizationConstraintPasses constraint_passes{};
    bool all_declared_constraints_pass{false};
    RobustCapitalMobilizationCandidateAudit audit{};
};

struct RobustCapitalMobilizationLeastFirstLossPoint {
    double participation_fraction{0.0};
    std::size_t candidate_index{0U};
};

struct RobustCapitalMobilizationFrontierSummary {
    double aggregate_commitment_and_stack_detachment_million{0.0};
    // Deterministic resource proxies. Probability work is candidates *
    // scenarios * (scenarios + events + 1). Cash-path work is candidates *
    // (portfolio records + projects * scenarios * (horizon + 1) +
    //  2 * scenarios * (horizon + 1)); two is the fixed claim count. Their
    // sum is structural_work_units. These are not economic metrics or runtime
    // estimates; they bound the principal repeated work of the finite grid.
    // Portfolio records comprise cash-availability, draw, receipt, pool-cost,
    // cash-source and factor-tag entries, plus each configured portfolio loss
    // layer applied to each scenario.
    std::size_t portfolio_cash_record_count{0U};
    std::size_t portfolio_auxiliary_record_count{0U};
    std::size_t portfolio_record_count{0U};
    std::size_t probability_projection_work_units{0U};
    std::size_t cash_path_work_units{0U};
    std::size_t structural_work_units{0U};
    std::size_t structural_work_unit_limit{
        kRobustCapitalMobilizationFrontierMaximumStructuralWorkUnits};
    std::vector<double> evaluated_participation_fraction_grid{};
    std::vector<double> evaluated_catalytic_first_loss_million_grid{};
    std::size_t declared_constraint_count{0U};
    // The minimum tested contingent success-cash-flow share required for
    // robust feasibility. Absent when no declared grid candidate is feasible;
    // it is not a continuous optimum or a contractual recommendation.
    std::optional<double> minimum_tested_feasible_participation_fraction{};

    // Candidates are in ascending (q,A) order. The index lists refer to this
    // vector, avoiding copies that could separate metrics from their witnesses.
    std::vector<RobustCapitalMobilizationFrontierCandidate> candidates{};
    std::vector<std::size_t> feasible_candidate_indices{};
    std::vector<std::size_t> nondominated_feasible_candidate_indices{};
    std::vector<RobustCapitalMobilizationLeastFirstLossPoint>
        least_first_loss_feasible_by_participation{};

    // Pareto dominance minimizes q, A, catalytic NPV concession, market
    // expected principal loss, principal-loss ES95/99, impairment, negative
    // NPV probability, NPV-shortfall ES95/99, and market WAL. An available WAL
    // is preferred to an unavailable WAL. NPV-surplus mandates remain explicit
    // feasibility constraints; no weighted score silently rewards extracting
    // additional project participation after a mandate is met.
    bool weighted_score_or_continuous_optimum_is_claimed{false};
    bool fair_value_or_market_price_is_estimated{false};
    bool capital_mobilization_is_established{false};
    std::string model_limitation{};
};

void validate_robust_capital_mobilization_frontier_config(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const RobustCapitalMobilizationFrontierConfig& frontier);

// Re-evaluates selected participation cash paths, the two-claim waterfall, and
// every linear, tail, impairment, and common-measure WAL projection separately
// for every declared (q,A) pair. At most 1,024 pairs are accepted, subject also
// to the published combined probability-projection and cash-path work bound.
// The result is a finite-grid decision aid, not a continuous optimum, price,
// or rating.
[[nodiscard]] RobustCapitalMobilizationFrontierSummary
evaluate_robust_capital_mobilization_frontier(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const RobustCapitalMobilizationFrontierConfig& frontier);

} // namespace naturalehia::cellular_finance
