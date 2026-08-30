// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp>

#include "robust_two_claim_grid_work.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kComparisonAbsoluteTolerance = 1.0e-10;
// Matches the validated capital-stack engine's one-base-currency-unit floor
// when all monetary inputs are expressed in millions.
constexpr double kMinimumGeneratedClaimNotionalMillion = 1.0e-6;

constexpr std::string_view kStructuralWorkLimitMessage{
    "frontier combined probability-projection and cash-path structural work "
    "exceeds the 4,000,000-unit resource bound"};

[[nodiscard]] detail::RobustTwoClaimGridWorkCounts
checked_structural_work_units(
    const PortfolioConfig& portfolio, std::size_t candidate_count,
    std::size_t scenario_count, std::size_t event_count,
    std::size_t horizon_months) {
    return detail::checked_robust_two_claim_grid_work(portfolio,
        candidate_count, scenario_count, event_count, horizon_months,
        kRobustCapitalMobilizationFrontierMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);
}

[[nodiscard]] double aggregate_commitment(
    const PortfolioConfig& portfolio) {
    long double total = 0.0L;
    for (const PortfolioProject& project : portfolio.projects) {
        total += static_cast<long double>(project.commitment_million);
    }
    const double result = static_cast<double>(total);
    if (!std::isfinite(result)) {
        throw std::overflow_error(
            "frontier aggregate commitment is outside finite double range");
    }
    return result;
}

void require_finite(double value, std::string_view description) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be finite");
    }
}

void require_non_negative(double value, std::string_view description) {
    require_finite(value, description);
    if (value < 0.0) {
        throw std::invalid_argument(
            std::string(description) + " must be non-negative");
    }
}

void require_unit_interval(double value, std::string_view description) {
    require_finite(value, description);
    if (value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(description) + " must lie in [0,1]");
    }
}

template <typename Validator>
void validate_optional(const std::optional<double>& value,
    std::string_view description, Validator validator) {
    if (value.has_value()) {
        validator(*value, description);
    }
}

[[nodiscard]] std::vector<double> sorted_unique_grid(
    const std::vector<double>& input, std::string_view description) {
    if (input.empty()) {
        throw std::invalid_argument(
            std::string(description) + " must not be empty");
    }
    std::vector<double> result = input;
    for (const double value : result) {
        require_finite(value, description);
    }
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end()) {
        throw std::invalid_argument(
            std::string(description) + " values must be unique");
    }
    return result;
}

[[nodiscard]] CapitalStackConfig make_two_claim_stack(
    const RobustCapitalMobilizationFrontierConfig& frontier,
    double participation_fraction, double first_loss_million,
    double commitment_million) {
    CapitalStackConfig stack;
    stack.scenario_label = frontier.scenario_label;
    stack.source_note = frontier.source_note;
    stack.synthetic_inputs = frontier.synthetic_inputs;
    stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero = true;
    stack.subscription_reserve_is_zero_yield_and_lossless = true;
    stack.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    stack.pool_costs_are_additional_pro_rata_calls = true;
    stack.principal_cash_is_paid_most_senior_first = true;
    stack.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    stack.tranching_does_not_change_project_cash_or_gross_loss = true;
    stack.premium_discount_or_fair_value_is_claimed = false;
    stack.underlying_success_participation_fraction =
        participation_fraction;
    stack.tranches = {
        CapitalStackTrancheConfig{frontier.catalytic_claim_id, 0.0,
            first_loss_million, 0.0,
            frontier.catalytic_annual_physical_hurdle_rate, true},
        CapitalStackTrancheConfig{frontier.market_claim_id,
            first_loss_million, commitment_million,
            frontier.market_priority_nonprincipal_cap_million,
            frontier.market_annual_physical_hurdle_rate, false},
    };
    return stack;
}

[[nodiscard]] double comparison_tolerance(
    double first, double second) noexcept {
    return kComparisonAbsoluteTolerance +
        256.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] bool meets_minimum(double actual, double minimum) noexcept {
    return actual + comparison_tolerance(actual, minimum) >= minimum;
}

[[nodiscard]] bool meets_maximum(double actual, double maximum) noexcept {
    return actual <= maximum + comparison_tolerance(actual, maximum);
}

[[nodiscard]] std::optional<bool> minimum_pass(
    const std::optional<double>& threshold, double actual) noexcept {
    if (!threshold.has_value()) {
        return std::nullopt;
    }
    return meets_minimum(actual, *threshold);
}

[[nodiscard]] std::optional<bool> maximum_pass(
    const std::optional<double>& threshold, double actual) noexcept {
    if (!threshold.has_value()) {
        return std::nullopt;
    }
    return meets_maximum(actual, *threshold);
}

[[nodiscard]] bool passes_every_declared_constraint(
    const RobustCapitalMobilizationConstraintPasses& passes) noexcept {
    const std::optional<bool> values[] = {
        passes.robust_aggregate_npv,
        passes.market_robust_npv_margin,
        passes.market_expected_loss_fraction,
        passes.market_principal_loss_es95_fraction,
        passes.market_principal_loss_es99_fraction,
        passes.market_principal_impairment_probability,
        passes.market_negative_npv_probability,
        passes.market_npv_shortfall_es95_fraction,
        passes.market_npv_shortfall_es99_fraction,
        passes.market_wal,
        passes.catalytic_first_loss,
        passes.catalytic_npv_concession,
    };
    return std::all_of(std::begin(values), std::end(values),
        [](const std::optional<bool>& value) {
            return !value.has_value() || *value;
        });
}

[[nodiscard]] std::size_t declared_constraint_count(
    const RobustCapitalMobilizationFrontierConstraints& constraints) noexcept {
    const bool values[] = {
        constraints.minimum_robust_aggregate_npv_million.has_value(),
        constraints.minimum_market_robust_npv_margin_fraction.has_value(),
        constraints.maximum_market_expected_loss_fraction.has_value(),
        constraints.maximum_market_principal_loss_es95_fraction.has_value(),
        constraints.maximum_market_principal_loss_es99_fraction.has_value(),
        constraints.maximum_market_principal_impairment_probability.has_value(),
        constraints.maximum_market_negative_npv_probability.has_value(),
        constraints.maximum_market_npv_shortfall_es95_fraction.has_value(),
        constraints.maximum_market_npv_shortfall_es99_fraction.has_value(),
        constraints.maximum_market_wal_years.has_value(),
        constraints.maximum_catalytic_first_loss_million.has_value(),
        constraints.maximum_catalytic_npv_concession_million.has_value(),
    };
    return static_cast<std::size_t>(std::count(
        std::begin(values), std::end(values), true));
}

[[nodiscard]] double maximum_stack_accounting_error(
    const CapitalStackProbabilityPolytopeSummary& stack) noexcept {
    return std::max({
        stack.maximum_commitment_identity_error_million,
        stack.maximum_reserve_roll_forward_error_million,
        stack.maximum_reserve_shortfall_million,
        stack.maximum_subscription_reconciliation_error_million,
        stack.maximum_pool_cost_call_reconciliation_error_million,
        stack.maximum_principal_distribution_reconciliation_error_million,
        stack.maximum_nonprincipal_distribution_reconciliation_error_million,
        stack.maximum_priority_nonprincipal_cap_violation_million,
        stack.maximum_realized_loss_reconciliation_error_million,
        stack.maximum_unresolved_exposure_reconciliation_error_million,
        stack.maximum_nominal_net_cash_reconciliation_error_million,
        stack.maximum_stack_npv_reconciliation_error_million,
    });
}

[[nodiscard]] RobustCapitalMobilizationFrontierCandidate evaluate_candidate(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const RobustCapitalMobilizationFrontierConfig& frontier,
    double participation_fraction, double first_loss_million,
    double commitment_million) {
    const CapitalStackConfig stack_terms = make_two_claim_stack(frontier,
        participation_fraction, first_loss_million, commitment_million);
    const CapitalStackProbabilityPolytopeSummary stack =
        evaluate_capital_stack_probability_polytope(
            portfolio, probability_polytope, participation, stack_terms);
    if (stack.tranches.size() != 2U ||
        stack.tranches[0].tranche_id != frontier.catalytic_claim_id ||
        stack.tranches[1].tranche_id != frontier.market_claim_id) {
        throw std::logic_error(
            "frontier two-claim stack lost its configured claim ordering");
    }

    const CapitalStackProbabilityPolytopeTrancheSummary& catalytic =
        stack.tranches[0];
    const CapitalStackProbabilityPolytopeTrancheSummary& market =
        stack.tranches[1];

    RobustCapitalMobilizationFrontierCandidate candidate;
    candidate.participation_fraction = participation_fraction;
    candidate.catalytic_first_loss_million = first_loss_million;
    candidate.market_notional_million = commitment_million - first_loss_million;
    candidate.aggregate_fully_funded_npv_million =
        stack.expected_fully_funded_stack_npv_at_pool_hurdle_million;
    candidate.catalytic_npv_million =
        catalytic.expected_npv_at_tranche_hurdle_million;
    candidate.market_npv_million =
        market.expected_npv_at_tranche_hurdle_million;
    candidate.market_expected_contributions_million =
        market.expected_contributions_million;
    candidate.market_expected_total_distributions_million =
        market.expected_total_distributions_million;
    candidate.market_expected_loss_fraction =
        market.expected_realized_principal_loss_fraction;
    candidate.market_expected_principal_cash_distribution_million =
        market.expected_principal_cash_distribution_million;
    candidate.market_principal_loss_es95_million =
        market.principal_loss_expected_shortfall_95_million;
    candidate.market_principal_loss_es99_million =
        market.principal_loss_expected_shortfall_99_million;
    candidate.market_principal_impairment_probability =
        market.principal_impairment_probability;
    candidate.market_negative_npv_probability =
        market.negative_npv_probability;
    candidate.market_npv_shortfall_es95_million =
        market.npv_shortfall_expected_shortfall_95_million;
    candidate.market_npv_shortfall_es99_million =
        market.npv_shortfall_expected_shortfall_99_million;
    candidate.market_principal_cash_wal_years =
        market.principal_cash_weighted_average_life_years;

    candidate.robust_aggregate_npv_million =
        candidate.aggregate_fully_funded_npv_million.minimum.value;
    candidate.robust_catalytic_npv_million =
        candidate.catalytic_npv_million.minimum.value;
    candidate.catalytic_npv_concession_million = std::max(0.0,
        frontier.catalytic_target_npv_million -
            candidate.robust_catalytic_npv_million);
    candidate.robust_market_npv_million =
        candidate.market_npv_million.minimum.value;
    candidate.robust_market_npv_margin_fraction =
        candidate.robust_market_npv_million /
        candidate.market_notional_million;
    candidate.worst_market_expected_loss_fraction =
        candidate.market_expected_loss_fraction.maximum.value;
    candidate.worst_market_principal_loss_es95_fraction =
        candidate.market_principal_loss_es95_million.maximum.value /
        candidate.market_notional_million;
    candidate.worst_market_principal_loss_es99_fraction =
        candidate.market_principal_loss_es99_million.maximum.value /
        candidate.market_notional_million;
    candidate.worst_market_principal_impairment_probability =
        candidate.market_principal_impairment_probability.maximum.value;
    candidate.worst_market_negative_npv_probability =
        candidate.market_negative_npv_probability.maximum.value;
    candidate.worst_market_npv_shortfall_es95_fraction =
        candidate.market_npv_shortfall_es95_million.maximum.value /
        candidate.market_notional_million;
    candidate.worst_market_npv_shortfall_es99_fraction =
        candidate.market_npv_shortfall_es99_million.maximum.value /
        candidate.market_notional_million;
    if (candidate.market_principal_cash_wal_years.has_value()) {
        candidate.worst_market_wal_years =
            candidate.market_principal_cash_wal_years->maximum.value_years;
    }

    const RobustCapitalMobilizationFrontierConstraints& limits =
        frontier.constraints;
    RobustCapitalMobilizationConstraintPasses& passes =
        candidate.constraint_passes;
    passes.robust_aggregate_npv = minimum_pass(
        limits.minimum_robust_aggregate_npv_million,
        candidate.robust_aggregate_npv_million);
    passes.market_robust_npv_margin = minimum_pass(
        limits.minimum_market_robust_npv_margin_fraction,
        candidate.robust_market_npv_margin_fraction);
    passes.market_expected_loss_fraction = maximum_pass(
        limits.maximum_market_expected_loss_fraction,
        candidate.worst_market_expected_loss_fraction);
    passes.market_principal_loss_es95_fraction = maximum_pass(
        limits.maximum_market_principal_loss_es95_fraction,
        candidate.worst_market_principal_loss_es95_fraction);
    passes.market_principal_loss_es99_fraction = maximum_pass(
        limits.maximum_market_principal_loss_es99_fraction,
        candidate.worst_market_principal_loss_es99_fraction);
    passes.market_principal_impairment_probability = maximum_pass(
        limits.maximum_market_principal_impairment_probability,
        candidate.worst_market_principal_impairment_probability);
    passes.market_negative_npv_probability = maximum_pass(
        limits.maximum_market_negative_npv_probability,
        candidate.worst_market_negative_npv_probability);
    passes.market_npv_shortfall_es95_fraction = maximum_pass(
        limits.maximum_market_npv_shortfall_es95_fraction,
        candidate.worst_market_npv_shortfall_es95_fraction);
    passes.market_npv_shortfall_es99_fraction = maximum_pass(
        limits.maximum_market_npv_shortfall_es99_fraction,
        candidate.worst_market_npv_shortfall_es99_fraction);
    if (limits.maximum_market_wal_years.has_value()) {
        passes.market_wal = candidate.worst_market_wal_years.has_value() &&
            meets_maximum(*candidate.worst_market_wal_years,
                *limits.maximum_market_wal_years);
    }
    passes.catalytic_first_loss = maximum_pass(
        limits.maximum_catalytic_first_loss_million,
        candidate.catalytic_first_loss_million);
    passes.catalytic_npv_concession = maximum_pass(
        limits.maximum_catalytic_npv_concession_million,
        candidate.catalytic_npv_concession_million);
    candidate.all_declared_constraints_pass =
        passes_every_declared_constraint(passes);

    candidate.audit.maximum_stack_accounting_error_million =
        maximum_stack_accounting_error(stack);
    candidate.audit.maximum_probability_constraint_violation =
        stack.maximum_probability_constraint_violation;
    candidate.audit.maximum_objective_reconciliation_error =
        stack.maximum_objective_reconciliation_error;
    candidate.audit.maximum_reduced_cost_optimality_residual =
        stack.maximum_reduced_cost_optimality_residual;
    candidate.audit.maximum_tail_mass_violation =
        stack.maximum_tail_mass_violation;
    candidate.audit.maximum_tail_objective_reconciliation_error =
        stack.maximum_tail_objective_reconciliation_error;
    candidate.audit.maximum_wal_ratio_reconciliation_error_years =
        stack.maximum_wal_ratio_reconciliation_error_years;
    candidate.audit
        .maximum_wal_root_objective_absolute_residual_million_years =
        stack.maximum_wal_root_objective_absolute_residual_million_years;
    return candidate;
}

[[nodiscard]] bool no_worse_minimum(double first, double second) noexcept {
    return first <= second + comparison_tolerance(first, second);
}

[[nodiscard]] bool strictly_better_minimum(
    double first, double second) noexcept {
    return first + comparison_tolerance(first, second) < second;
}

[[nodiscard]] bool no_worse_optional_wal(
    const std::optional<double>& first,
    const std::optional<double>& second) noexcept {
    if (first.has_value() && second.has_value()) {
        return no_worse_minimum(*first, *second);
    }
    return first.has_value() || !second.has_value();
}

[[nodiscard]] bool strictly_better_optional_wal(
    const std::optional<double>& first,
    const std::optional<double>& second) noexcept {
    if (first.has_value() && second.has_value()) {
        return strictly_better_minimum(*first, *second);
    }
    return first.has_value() && !second.has_value();
}

[[nodiscard]] bool dominates(
    const RobustCapitalMobilizationFrontierCandidate& first,
    const RobustCapitalMobilizationFrontierCandidate& second) noexcept {
    const double first_values[] = {
        first.participation_fraction,
        first.catalytic_first_loss_million,
        first.catalytic_npv_concession_million,
        first.worst_market_expected_loss_fraction,
        first.worst_market_principal_loss_es95_fraction,
        first.worst_market_principal_loss_es99_fraction,
        first.worst_market_principal_impairment_probability,
        first.worst_market_negative_npv_probability,
        first.worst_market_npv_shortfall_es95_fraction,
        first.worst_market_npv_shortfall_es99_fraction,
    };
    const double second_values[] = {
        second.participation_fraction,
        second.catalytic_first_loss_million,
        second.catalytic_npv_concession_million,
        second.worst_market_expected_loss_fraction,
        second.worst_market_principal_loss_es95_fraction,
        second.worst_market_principal_loss_es99_fraction,
        second.worst_market_principal_impairment_probability,
        second.worst_market_negative_npv_probability,
        second.worst_market_npv_shortfall_es95_fraction,
        second.worst_market_npv_shortfall_es99_fraction,
    };
    for (std::size_t index = 0U; index < std::size(first_values); ++index) {
        if (!no_worse_minimum(first_values[index], second_values[index])) {
            return false;
        }
    }
    if (!no_worse_optional_wal(
            first.worst_market_wal_years, second.worst_market_wal_years)) {
        return false;
    }
    for (std::size_t index = 0U; index < std::size(first_values); ++index) {
        if (strictly_better_minimum(
                first_values[index], second_values[index])) {
            return true;
        }
    }
    return strictly_better_optional_wal(
        first.worst_market_wal_years, second.worst_market_wal_years);
}

} // namespace

void validate_robust_capital_mobilization_frontier_config(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const RobustCapitalMobilizationFrontierConfig& frontier) {
    if (frontier.model_version !=
        kRobustCapitalMobilizationFrontierModelVersion) {
        throw std::invalid_argument(
            "unsupported robust capital-mobilization frontier model version");
    }
    if (!frontier.synthetic_inputs || !portfolio.synthetic_inputs ||
        !probability_polytope.synthetic_inputs ||
        !participation.synthetic_inputs) {
        throw std::invalid_argument(
            "robust capital-mobilization frontier v0.1 accepts synthetic inputs only");
    }
    if (frontier.participation_fraction_grid.empty() ||
        frontier.catalytic_first_loss_million_grid.empty()) {
        throw std::invalid_argument(
            "frontier participation and first-loss grids must not be empty");
    }
    if (frontier.participation_fraction_grid.size() >
            kRobustCapitalMobilizationFrontierMaximumCandidates /
                frontier.catalytic_first_loss_million_grid.size()) {
        throw std::invalid_argument(
            "frontier grid exceeds the 1,024-candidate resource bound");
    }

    validate_probability_polytope_config(portfolio, probability_polytope);
    const std::size_t candidate_count =
        frontier.participation_fraction_grid.size() *
        frontier.catalytic_first_loss_million_grid.size();
    (void)checked_structural_work_units(portfolio, candidate_count,
        probability_polytope.scenario_probabilities.size(),
        probability_polytope.events.size(), portfolio.horizon_months);
    const double commitment_million = aggregate_commitment(portfolio);
    if (!(commitment_million > 0.0)) {
        throw std::invalid_argument(
            "frontier aggregate commitment must be positive");
    }

    const std::vector<double> participation_grid = sorted_unique_grid(
        frontier.participation_fraction_grid,
        "frontier participation grid");
    for (const double fraction : participation_grid) {
        require_unit_interval(fraction,
            "frontier participation fraction");
    }
    const std::vector<double> first_loss_grid = sorted_unique_grid(
        frontier.catalytic_first_loss_million_grid,
        "frontier catalytic first-loss grid");
    for (const double first_loss : first_loss_grid) {
        require_finite(first_loss, "frontier catalytic first loss");
        if (first_loss < kMinimumGeneratedClaimNotionalMillion ||
            commitment_million - first_loss <
                kMinimumGeneratedClaimNotionalMillion) {
            throw std::invalid_argument(
                "frontier catalytic and market claim notionals must each be at least one base currency unit");
        }
    }

    require_non_negative(frontier.market_priority_nonprincipal_cap_million,
        "frontier market priority non-principal cap");
    require_non_negative(frontier.catalytic_annual_physical_hurdle_rate,
        "frontier catalytic physical hurdle");
    require_non_negative(frontier.market_annual_physical_hurdle_rate,
        "frontier market physical hurdle");
    require_finite(frontier.catalytic_target_npv_million,
        "frontier catalytic target NPV");
    if (frontier.catalytic_claim_id == frontier.market_claim_id) {
        throw std::invalid_argument(
            "frontier catalytic and market claim ids must be different");
    }

    const RobustCapitalMobilizationFrontierConstraints& limits =
        frontier.constraints;
    validate_optional(limits.minimum_robust_aggregate_npv_million,
        "minimum robust aggregate NPV", require_finite);
    validate_optional(limits.minimum_market_robust_npv_margin_fraction,
        "minimum market robust NPV margin", require_finite);
    validate_optional(limits.maximum_market_expected_loss_fraction,
        "maximum market expected-loss fraction", require_unit_interval);
    validate_optional(limits.maximum_market_principal_loss_es95_fraction,
        "maximum market principal-loss ES95 fraction", require_unit_interval);
    validate_optional(limits.maximum_market_principal_loss_es99_fraction,
        "maximum market principal-loss ES99 fraction", require_unit_interval);
    validate_optional(limits.maximum_market_principal_impairment_probability,
        "maximum market principal-impairment probability",
        require_unit_interval);
    validate_optional(limits.maximum_market_negative_npv_probability,
        "maximum market negative-NPV probability", require_unit_interval);
    validate_optional(limits.maximum_market_npv_shortfall_es95_fraction,
        "maximum market NPV-shortfall ES95 fraction", require_non_negative);
    validate_optional(limits.maximum_market_npv_shortfall_es99_fraction,
        "maximum market NPV-shortfall ES99 fraction", require_non_negative);
    validate_optional(limits.maximum_market_wal_years,
        "maximum market WAL", require_non_negative);
    validate_optional(limits.maximum_catalytic_first_loss_million,
        "maximum catalytic first loss", require_non_negative);
    validate_optional(limits.maximum_catalytic_npv_concession_million,
        "maximum catalytic NPV concession", require_non_negative);

    // This representative generated stack makes the fixed claim terms and
    // all structural assertions pass through the existing stack validator.
    // Every other pair changes only q and the already range-checked boundary A.
    validate_capital_stack_probability_polytope(portfolio,
        probability_polytope, participation,
        make_two_claim_stack(frontier, participation_grid.front(),
            first_loss_grid.front(), commitment_million));
}

RobustCapitalMobilizationFrontierSummary
evaluate_robust_capital_mobilization_frontier(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const RobustCapitalMobilizationFrontierConfig& frontier) {
    validate_robust_capital_mobilization_frontier_config(
        portfolio, probability_polytope, participation, frontier);

    RobustCapitalMobilizationFrontierSummary summary;
    summary.aggregate_commitment_and_stack_detachment_million =
        aggregate_commitment(portfolio);
    summary.evaluated_participation_fraction_grid = sorted_unique_grid(
        frontier.participation_fraction_grid,
        "frontier participation grid");
    summary.evaluated_catalytic_first_loss_million_grid = sorted_unique_grid(
        frontier.catalytic_first_loss_million_grid,
        "frontier catalytic first-loss grid");
    const detail::RobustTwoClaimGridWorkCounts work =
        checked_structural_work_units(portfolio,
            summary.evaluated_participation_fraction_grid.size() *
                summary.evaluated_catalytic_first_loss_million_grid.size(),
            probability_polytope.scenario_probabilities.size(),
            probability_polytope.events.size(), portfolio.horizon_months);
    summary.portfolio_cash_record_count = work.portfolio_cash_records;
    summary.portfolio_auxiliary_record_count =
        work.portfolio_auxiliary_records;
    summary.portfolio_record_count = work.portfolio_records;
    summary.probability_projection_work_units =
        work.probability_projection;
    summary.cash_path_work_units = work.cash_path;
    summary.structural_work_units = work.total;
    summary.declared_constraint_count =
        declared_constraint_count(frontier.constraints);
    summary.model_limitation =
        "Finite synthetic physical-measure grid only. It estimates no fair "
        "value, market spread, rating, legal enforceability, reserve custody "
        "risk, tax, regulatory capital, empirical calibration, or continuous "
        "optimum. Different risk endpoints may have different adverse "
        "probability witnesses and must not be combined as one scenario. "
        "Modeled feasibility does not establish capital mobilization or any "
        "actual third-party commitment.";

    const std::size_t first_loss_count =
        summary.evaluated_catalytic_first_loss_million_grid.size();
    summary.candidates.reserve(
        summary.evaluated_participation_fraction_grid.size() *
        first_loss_count);
    for (const double participation_fraction :
         summary.evaluated_participation_fraction_grid) {
        for (const double first_loss_million :
             summary.evaluated_catalytic_first_loss_million_grid) {
            summary.candidates.push_back(evaluate_candidate(portfolio,
                probability_polytope, participation, frontier,
                participation_fraction, first_loss_million,
                summary.aggregate_commitment_and_stack_detachment_million));
            if (summary.candidates.back().all_declared_constraints_pass) {
                summary.feasible_candidate_indices.push_back(
                    summary.candidates.size() - 1U);
            }
        }
    }

    for (const std::size_t candidate_index :
         summary.feasible_candidate_indices) {
        bool is_dominated = false;
        for (const std::size_t comparison_index :
             summary.feasible_candidate_indices) {
            if (candidate_index != comparison_index &&
                dominates(summary.candidates[comparison_index],
                    summary.candidates[candidate_index])) {
                is_dominated = true;
                break;
            }
        }
        if (!is_dominated) {
            summary.nondominated_feasible_candidate_indices.push_back(
                candidate_index);
        }
    }

    if (!summary.feasible_candidate_indices.empty()) {
        summary.minimum_tested_feasible_participation_fraction =
            summary.candidates[summary.feasible_candidate_indices.front()]
                .participation_fraction;
    }

    for (std::size_t participation_index = 0U;
         participation_index <
            summary.evaluated_participation_fraction_grid.size();
         ++participation_index) {
        const std::size_t first_candidate =
            participation_index * first_loss_count;
        for (std::size_t first_loss_index = 0U;
             first_loss_index < first_loss_count; ++first_loss_index) {
            const std::size_t candidate_index =
                first_candidate + first_loss_index;
            if (summary.candidates[candidate_index]
                    .all_declared_constraints_pass) {
                summary.least_first_loss_feasible_by_participation.push_back(
                    RobustCapitalMobilizationLeastFirstLossPoint{
                        summary.evaluated_participation_fraction_grid[
                            participation_index],
                        candidate_index});
                break;
            }
        }
    }
    return summary;
}

} // namespace naturalehia::cellular_finance
