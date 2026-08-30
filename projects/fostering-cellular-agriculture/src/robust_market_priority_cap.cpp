// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_market_priority_cap.hpp>

#include "robust_two_claim_grid_work.hpp"

#include <algorithm>
#include <array>
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
constexpr double kMinimumPositiveCapMillion = 1.0e-6;
constexpr std::string_view kStructuralWorkLimitMessage{
    "market priority-cap combined probability-projection and cash-path "
    "structural work exceeds the 4,000,000-unit resource bound"};

struct ValidatedInputs {
    std::vector<double> cap_grid{};
    detail::RobustTwoClaimGridWorkCounts work{};
    double commitment_million{0.0};
    double junior_first_loss_million{0.0};
    double market_notional_million{0.0};
    double reference_cap_million{0.0};
    std::size_t reference_cap_index{0U};
    std::size_t ceiling_cap_index{0U};
};

struct EvaluatedCandidate {
    RobustMarketPriorityCapCandidate report{};
    CapitalStackProbabilityPolytopeSummary stack{};
};

[[nodiscard]] double comparison_tolerance(
    double first, double second) noexcept {
    return kComparisonAbsoluteTolerance +
        256.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] bool nearly_equal(double first, double second) noexcept {
    return std::abs(first - second) <=
        comparison_tolerance(first, second);
}

[[nodiscard]] bool meets_minimum(double actual, double minimum) noexcept {
    return actual + comparison_tolerance(actual, minimum) >= minimum;
}

[[nodiscard]] bool meets_maximum(double actual, double maximum) noexcept {
    return actual <= maximum + comparison_tolerance(actual, maximum);
}

[[nodiscard]] bool is_materially_negative(double value) noexcept {
    return value < -comparison_tolerance(value, 0.0);
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

[[nodiscard]] bool every_declared_passes(
    std::initializer_list<std::optional<bool>> values) noexcept {
    return std::all_of(values.begin(), values.end(),
        [](const std::optional<bool>& value) {
            return !value.has_value() || *value;
        });
}

[[nodiscard]] std::vector<double> canonical_cap_grid(
    const RobustMarketPriorityCapConfig& cap_term) {
    const std::vector<double>& input =
        cap_term.market_priority_nonprincipal_cap_million_grid;
    if (input.empty()) {
        throw std::invalid_argument(
            "market priority-cap grid must not be empty");
    }
    if (input.size() > kRobustMarketPriorityCapMaximumCandidates) {
        throw std::invalid_argument(
            "market priority-cap grid exceeds the 1,024-candidate resource bound");
    }
    std::vector<double> result = input;
    for (const double cap : result) {
        require_non_negative(cap, "market priority-cap grid value");
        if (std::signbit(cap)) {
            throw std::invalid_argument(
                "market priority-cap grid values must not use negative zero");
        }
        if (cap > 0.0 && cap < kMinimumPositiveCapMillion) {
            throw std::invalid_argument(
                "positive market priority-cap grid values must be at least one base currency unit");
        }
    }
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end()) {
        throw std::invalid_argument(
            "market priority-cap grid values must be unique");
    }
    if (result.front() != 0.0) {
        throw std::invalid_argument(
            "market priority-cap grid must contain literal zero");
    }
    return result;
}

[[nodiscard]] std::size_t exact_grid_index(
    const std::vector<double>& grid, double value,
    std::string_view missing_message) {
    const auto found = std::lower_bound(grid.begin(), grid.end(), value);
    if (found == grid.end() || *found != value) {
        throw std::invalid_argument(std::string(missing_message));
    }
    return static_cast<std::size_t>(std::distance(grid.begin(), found));
}

void validate_constraints(
    const RobustMarketPriorityCapConstraints& limits) {
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
        "maximum junior first loss", require_non_negative);
    validate_optional(limits.maximum_catalytic_npv_concession_million,
        "maximum junior NPV concession", require_non_negative);

    const bool has_cap_sensitive_mandate =
        limits.minimum_market_robust_npv_margin_fraction.has_value() ||
        limits.maximum_market_negative_npv_probability.has_value() ||
        limits.maximum_market_npv_shortfall_es95_fraction.has_value() ||
        limits.maximum_market_npv_shortfall_es99_fraction.has_value();
    if (!has_cap_sensitive_mandate) {
        throw std::invalid_argument(
            "market priority-cap term requires at least one cap-sensitive market mandate");
    }
}

[[nodiscard]] ValidatedInputs validate_inputs(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& cap_term) {
    if (cap_term.model_version != kRobustMarketPriorityCapModelVersion) {
        throw std::invalid_argument(
            "unsupported robust market priority-cap model version");
    }
    if (!cap_term.synthetic_inputs || !portfolio.synthetic_inputs ||
        !probability_polytope.synthetic_inputs ||
        !participation.synthetic_inputs || !base_stack.synthetic_inputs) {
        throw std::invalid_argument(
            "robust market priority-cap v0.1 accepts synthetic inputs only");
    }

    ValidatedInputs result;
    result.cap_grid = canonical_cap_grid(cap_term);
    result.work = detail::checked_robust_two_claim_grid_work(portfolio,
        result.cap_grid.size(),
        probability_polytope.scenario_probabilities.size(),
        probability_polytope.events.size(), portfolio.horizon_months,
        kRobustMarketPriorityCapMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);

    validate_capital_stack_probability_polytope(
        portfolio, probability_polytope, participation, base_stack);
    if (base_stack.tranches.size() != 2U) {
        throw std::invalid_argument(
            "market priority-cap base stack must contain exactly two tranches");
    }
    const CapitalStackTrancheConfig& junior = base_stack.tranches[0];
    const CapitalStackTrancheConfig& market = base_stack.tranches[1];
    if (junior.id != cap_term.junior_claim_id ||
        market.id != cap_term.market_claim_id) {
        throw std::invalid_argument(
            "market priority-cap claim ids must match the ordered base-stack tranches");
    }
    if (!junior.is_first_loss_residual || market.is_first_loss_residual) {
        throw std::invalid_argument(
            "market priority-cap base stack must order the junior residual before the market claim");
    }

    result.commitment_million = market.detachment_million;
    result.junior_first_loss_million = junior.detachment_million;
    result.market_notional_million =
        market.detachment_million - market.attachment_million;
    result.reference_cap_million = market.priority_nonprincipal_cap_million;

    require_non_negative(cap_term.contractual_ceiling_million,
        "market priority-cap contractual ceiling");
    if (std::signbit(cap_term.contractual_ceiling_million)) {
        throw std::invalid_argument(
            "market priority-cap contractual ceiling must not use negative zero");
    }
    if (std::signbit(result.reference_cap_million)) {
        throw std::invalid_argument(
            "market priority-cap base-stack reference cap must not use negative zero");
    }
    if (cap_term.contractual_ceiling_million > 0.0 &&
        cap_term.contractual_ceiling_million < kMinimumPositiveCapMillion) {
        throw std::invalid_argument(
            "positive market priority-cap contractual ceiling must be at least one base currency unit");
    }
    for (const double cap : result.cap_grid) {
        if (cap > cap_term.contractual_ceiling_million) {
            throw std::invalid_argument(
                "market priority-cap grid value exceeds the contractual ceiling");
        }
    }
    result.reference_cap_index = exact_grid_index(result.cap_grid,
        result.reference_cap_million,
        "market priority-cap grid must contain the base-stack reference cap");
    result.ceiling_cap_index = exact_grid_index(result.cap_grid,
        cap_term.contractual_ceiling_million,
        "market priority-cap grid must contain the contractual ceiling");

    require_finite(cap_term.junior_target_npv_million,
        "market priority-cap junior target NPV");
    validate_constraints(cap_term.constraints);
    return result;
}

[[nodiscard]] std::vector<ProbabilityPolytopeScenarioValue> scenario_values(
    const CapitalStackProbabilityPolytopeSummary& stack,
    const auto& selector) {
    std::vector<ProbabilityPolytopeScenarioValue> values;
    values.reserve(stack.scenarios.size());
    for (const CapitalStackScenarioResult& scenario : stack.scenarios) {
        values.push_back(
            ProbabilityPolytopeScenarioValue{scenario.scenario_id,
                selector(scenario)});
    }
    return values;
}

void copy_candidate_audits(
    const CapitalStackProbabilityPolytopeSummary& stack,
    RobustMarketPriorityCapCandidateAudit& audit) {
    audit.maximum_commitment_identity_error_million =
        stack.maximum_commitment_identity_error_million;
    audit.maximum_reserve_roll_forward_error_million =
        stack.maximum_reserve_roll_forward_error_million;
    audit.maximum_reserve_shortfall_million =
        stack.maximum_reserve_shortfall_million;
    audit.maximum_subscription_reconciliation_error_million =
        stack.maximum_subscription_reconciliation_error_million;
    audit.maximum_pool_cost_call_reconciliation_error_million =
        stack.maximum_pool_cost_call_reconciliation_error_million;
    audit.maximum_principal_distribution_reconciliation_error_million =
        stack.maximum_principal_distribution_reconciliation_error_million;
    audit.maximum_nonprincipal_distribution_reconciliation_error_million =
        stack.maximum_nonprincipal_distribution_reconciliation_error_million;
    audit.maximum_priority_nonprincipal_cap_violation_million =
        stack.maximum_priority_nonprincipal_cap_violation_million;
    audit.maximum_realized_loss_reconciliation_error_million =
        stack.maximum_realized_loss_reconciliation_error_million;
    audit.maximum_unresolved_exposure_reconciliation_error_million =
        stack.maximum_unresolved_exposure_reconciliation_error_million;
    audit.maximum_nominal_net_cash_reconciliation_error_million =
        stack.maximum_nominal_net_cash_reconciliation_error_million;
    audit.maximum_stack_npv_reconciliation_error_million =
        stack.maximum_stack_npv_reconciliation_error_million;
    audit.maximum_probability_constraint_violation =
        stack.maximum_probability_constraint_violation;
    audit.maximum_objective_reconciliation_error =
        stack.maximum_objective_reconciliation_error;
    audit.maximum_reduced_cost_optimality_residual =
        stack.maximum_reduced_cost_optimality_residual;
    audit.maximum_tail_mass_violation =
        stack.maximum_tail_mass_violation;
    audit.maximum_tail_objective_reconciliation_error =
        stack.maximum_tail_objective_reconciliation_error;
    audit.maximum_tail_threshold_formula_reconciliation_error =
        stack.maximum_tail_threshold_formula_reconciliation_error;
    audit.maximum_tail_threshold_enumeration_optimality_residual =
        stack.maximum_tail_threshold_enumeration_optimality_residual;
    audit.maximum_wal_numerator_reconciliation_error_million_years =
        stack.maximum_wal_numerator_reconciliation_error_million_years;
    audit.maximum_wal_denominator_reconciliation_error_million =
        stack.maximum_wal_denominator_reconciliation_error_million;
    audit.maximum_wal_ratio_reconciliation_error_years =
        stack.maximum_wal_ratio_reconciliation_error_years;
    audit.maximum_wal_root_objective_reconciliation_error_million_years =
        stack.maximum_wal_root_objective_reconciliation_error_million_years;
    audit.maximum_wal_root_objective_absolute_residual_million_years =
        stack.maximum_wal_root_objective_absolute_residual_million_years;
}

void update_linear_audits(const ProbabilityPolytopeMetricProjection& value,
    RobustMarketPriorityCapCandidateAudit& audit) {
    audit.maximum_probability_constraint_violation = std::max(
        audit.maximum_probability_constraint_violation,
        value.maximum_endpoint_constraint_violation);
    audit.maximum_objective_reconciliation_error = std::max(
        audit.maximum_objective_reconciliation_error,
        value.maximum_endpoint_objective_reconciliation_error);
    audit.maximum_reduced_cost_optimality_residual = std::max(
        audit.maximum_reduced_cost_optimality_residual,
        value.maximum_endpoint_optimality_residual);
}

[[nodiscard]] EvaluatedCandidate evaluate_candidate(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& cap_term,
    const ProbabilityPolytopeProjector& projector, double cap_million) {
    CapitalStackConfig tested_stack = base_stack;
    tested_stack.tranches[1].priority_nonprincipal_cap_million = cap_million;

    EvaluatedCandidate result;
    result.stack = evaluate_capital_stack_probability_polytope(
        portfolio, probability_polytope, participation, tested_stack);
    if (result.stack.tranches.size() != 2U ||
        result.stack.tranches[0].tranche_id != cap_term.junior_claim_id ||
        result.stack.tranches[1].tranche_id != cap_term.market_claim_id) {
        throw std::logic_error(
            "market priority-cap evaluation lost the base-stack claim ordering");
    }

    const CapitalStackProbabilityPolytopeTrancheSummary& junior =
        result.stack.tranches[0];
    const CapitalStackProbabilityPolytopeTrancheSummary& market =
        result.stack.tranches[1];
    RobustMarketPriorityCapCandidate& candidate = result.report;
    candidate.market_priority_nonprincipal_cap_million = cap_million;
    candidate.market_notional_million = market.notional_million;
    candidate.aggregate_fully_funded_npv_million =
        result.stack.expected_fully_funded_stack_npv_at_pool_hurdle_million;

    candidate.junior_expected_contributions_million =
        junior.expected_contributions_million;
    candidate.junior_expected_nonprincipal_cash_distribution_million =
        junior.expected_nonprincipal_cash_distribution_million;
    candidate.junior_expected_total_distributions_million =
        junior.expected_total_distributions_million;
    candidate.junior_expected_scenario_cash_multiple =
        junior.expected_scenario_cash_multiple;
    candidate.junior_expected_scenario_net_return_fraction =
        junior.expected_scenario_net_return_fraction;
    candidate.junior_npv_million =
        junior.expected_npv_at_tranche_hurdle_million;

    candidate.market_expected_contributions_million =
        market.expected_contributions_million;
    candidate.market_expected_principal_cash_distribution_million =
        market.expected_principal_cash_distribution_million;
    candidate.market_expected_nonprincipal_cash_distribution_million =
        market.expected_nonprincipal_cash_distribution_million;
    candidate.market_expected_total_distributions_million =
        market.expected_total_distributions_million;
    candidate.market_expected_scenario_cash_multiple =
        market.expected_scenario_cash_multiple;
    candidate.market_expected_scenario_net_return_fraction =
        market.expected_scenario_net_return_fraction;
    candidate.market_npv_million =
        market.expected_npv_at_tranche_hurdle_million;
    candidate.market_expected_loss_fraction =
        market.expected_realized_principal_loss_fraction;
    candidate.market_principal_loss_es95_million =
        market.principal_loss_expected_shortfall_95_million;
    candidate.market_principal_loss_es99_million =
        market.principal_loss_expected_shortfall_99_million;
    candidate.market_principal_impairment_probability =
        market.principal_impairment_probability;
    candidate.market_npv_shortfall_es95_million =
        market.npv_shortfall_expected_shortfall_95_million;
    candidate.market_npv_shortfall_es99_million =
        market.npv_shortfall_expected_shortfall_99_million;
    candidate.market_principal_cash_wal_years =
        market.principal_cash_weighted_average_life_years;

    copy_candidate_audits(result.stack, candidate.audit);
    const ProbabilityPolytopeMetricProjection expired_cap =
        projector.project_expectation(scenario_values(result.stack,
            [](const CapitalStackScenarioResult& scenario) {
                return scenario.tranches[1]
                    .unused_priority_nonprincipal_capacity_million;
            }));
    candidate.market_expired_priority_cap_capacity_million =
        expired_cap.expectation;
    update_linear_audits(expired_cap, candidate.audit);

    // The stack engine's raw sign indicator uses an exact comparison. This
    // term deliberately applies its disclosed money tolerance so a binary
    // representation at the B=0.08 hand boundary cannot choose the result.
    const ProbabilityPolytopeMetricProjection negative_npv =
        projector.project_expectation(scenario_values(result.stack,
            [](const CapitalStackScenarioResult& scenario) {
                return is_materially_negative(
                           scenario.tranches[1]
                               .npv_at_tranche_hurdle_million)
                    ? 1.0
                    : 0.0;
            }));
    candidate.market_negative_npv_probability = negative_npv.expectation;
    update_linear_audits(negative_npv, candidate.audit);

    candidate.robust_aggregate_npv_million =
        candidate.aggregate_fully_funded_npv_million.minimum.value;
    candidate.robust_junior_npv_million =
        candidate.junior_npv_million.minimum.value;
    candidate.junior_npv_concession_million = std::max(0.0,
        cap_term.junior_target_npv_million -
            candidate.robust_junior_npv_million);
    candidate.robust_market_npv_million =
        candidate.market_npv_million.minimum.value;
    candidate.robust_market_npv_margin_fraction =
        candidate.robust_market_npv_million / candidate.market_notional_million;
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

    const RobustMarketPriorityCapConstraints& limits = cap_term.constraints;
    RobustMarketPriorityCapConstraintPasses& passes =
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
        junior.notional_million);
    passes.catalytic_npv_concession = maximum_pass(
        limits.maximum_catalytic_npv_concession_million,
        candidate.junior_npv_concession_million);

    candidate.fixed_structure_eligible = every_declared_passes({
        passes.robust_aggregate_npv,
        passes.market_expected_loss_fraction,
        passes.market_principal_loss_es95_fraction,
        passes.market_principal_loss_es99_fraction,
        passes.market_principal_impairment_probability,
        passes.market_wal,
        passes.catalytic_first_loss,
    });
    candidate.cap_sensitive_market_mandates_pass = every_declared_passes({
        passes.market_robust_npv_margin,
        passes.market_negative_npv_probability,
        passes.market_npv_shortfall_es95_fraction,
        passes.market_npv_shortfall_es99_fraction,
    });
    candidate.market_adequate = candidate.fixed_structure_eligible &&
        candidate.cap_sensitive_market_mandates_pass;
    candidate.junior_concession_limit_passes =
        !passes.catalytic_npv_concession.has_value() ||
        *passes.catalytic_npv_concession;
    candidate.balanced = candidate.market_adequate &&
        candidate.junior_concession_limit_passes;
    return result;
}

[[nodiscard]] bool stack_config_equal(const CapitalStackConfig& first,
    const CapitalStackConfig& second) noexcept {
    if (first.model_version != second.model_version ||
        first.scenario_label != second.scenario_label ||
        first.source_note != second.source_note ||
        first.synthetic_inputs != second.synthetic_inputs ||
        first.aggregate_commitment_is_fully_funded_at_par_at_month_zero !=
            second.aggregate_commitment_is_fully_funded_at_par_at_month_zero ||
        first.subscription_reserve_is_zero_yield_and_lossless !=
            second.subscription_reserve_is_zero_yield_and_lossless ||
        first.undrawn_commitment_cancels_and_returns_only_at_horizon !=
            second.undrawn_commitment_cancels_and_returns_only_at_horizon ||
        first.pool_costs_are_additional_pro_rata_calls !=
            second.pool_costs_are_additional_pro_rata_calls ||
        first.principal_cash_is_paid_most_senior_first !=
            second.principal_cash_is_paid_most_senior_first ||
        first.nonprincipal_cash_is_paid_to_caps_then_residual !=
            second.nonprincipal_cash_is_paid_to_caps_then_residual ||
        first.tranching_does_not_change_project_cash_or_gross_loss !=
            second.tranching_does_not_change_project_cash_or_gross_loss ||
        first.premium_discount_or_fair_value_is_claimed !=
            second.premium_discount_or_fair_value_is_claimed ||
        first.underlying_success_participation_fraction !=
            second.underlying_success_participation_fraction ||
        first.tranches.size() != second.tranches.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& a = first.tranches[index];
        const CapitalStackTrancheConfig& b = second.tranches[index];
        if (a.id != b.id || a.attachment_million != b.attachment_million ||
            a.detachment_million != b.detachment_million ||
            a.priority_nonprincipal_cap_million !=
                b.priority_nonprincipal_cap_million ||
            a.annual_physical_hurdle_rate !=
                b.annual_physical_hurdle_rate ||
            a.is_first_loss_residual != b.is_first_loss_residual) {
            return false;
        }
    }
    return true;
}

void update_change(double first, double second, double& maximum,
    bool& invariant) noexcept {
    maximum = std::max(maximum, std::abs(first - second));
    invariant = invariant && nearly_equal(first, second);
}

void update_nondecreasing(double previous, double current, double& maximum,
    bool& monotone) noexcept {
    maximum = std::max(maximum, std::max(0.0, previous - current));
    monotone = monotone &&
        current + comparison_tolerance(previous, current) >= previous;
}

void update_nonincreasing(double previous, double current, double& maximum,
    bool& monotone) noexcept {
    maximum = std::max(maximum, std::max(0.0, current - previous));
    monotone = monotone &&
        current <= previous + comparison_tolerance(previous, current);
}

template <typename Update>
void update_metric_range(const ProbabilityPolytopeMetricRange& previous,
    const ProbabilityPolytopeMetricRange& current, double& maximum,
    bool& result, Update update) noexcept {
    update(previous.minimum.value, current.minimum.value, maximum, result);
    update(previous.central, current.central, maximum, result);
    update(previous.maximum.value, current.maximum.value, maximum, result);
}

template <typename Update>
void update_tail_range(
    const ProbabilityPolytopeUpperExpectedShortfallProjection& previous,
    const ProbabilityPolytopeUpperExpectedShortfallProjection& current,
    double& maximum, bool& result, Update update) noexcept {
    update(previous.minimum.value, current.minimum.value, maximum, result);
    update(previous.central, current.central, maximum, result);
    update(previous.maximum.value, current.maximum.value, maximum, result);
}

[[nodiscard]] const CapitalStackMonthlyTrancheCashFlow* cash_flow_at_month(
    const CapitalStackTrancheScenarioResult& tranche,
    std::size_t month) noexcept {
    const auto found = std::lower_bound(tranche.monthly_cash_flows.begin(),
        tranche.monthly_cash_flows.end(), month,
        [](const CapitalStackMonthlyTrancheCashFlow& cash_flow,
            std::size_t key) { return cash_flow.month < key; });
    if (found == tranche.monthly_cash_flows.end() || found->month != month) {
        return nullptr;
    }
    return &*found;
}

[[nodiscard]] double nonprincipal_cash_at_month(
    const CapitalStackTrancheScenarioResult& tranche,
    std::size_t month) noexcept {
    const CapitalStackMonthlyTrancheCashFlow* const cash_flow =
        cash_flow_at_month(tranche, month);
    return cash_flow == nullptr
        ? 0.0
        : cash_flow->nonprincipal_cash_distribution_million;
}

[[nodiscard]] double monthly_aggregate_net_cash(
    const CapitalStackScenarioResult& scenario, std::size_t month) {
    long double total = 0.0L;
    for (const CapitalStackTrancheScenarioResult& tranche :
         scenario.tranches) {
        const CapitalStackMonthlyTrancheCashFlow* const cash_flow =
            cash_flow_at_month(tranche, month);
        if (cash_flow != nullptr) {
            total += static_cast<long double>(cash_flow->net_cash_flow_million);
        }
    }
    const double result = static_cast<double>(total);
    if (!std::isfinite(result)) {
        throw std::overflow_error(
            "market priority-cap aggregate monthly cash is outside finite double range");
    }
    return result;
}

[[nodiscard]] RobustMarketPriorityCapGridAudit audit_grid(
    const std::vector<EvaluatedCandidate>& evaluated,
    bool base_stack_was_not_mutated) {
    RobustMarketPriorityCapGridAudit audit;
    audit.base_stack_was_not_mutated = base_stack_was_not_mutated;
    audit.market_contributions_are_invariant = true;
    audit.market_principal_cash_is_invariant = true;
    audit.market_principal_risk_is_invariant = true;
    audit.market_principal_wal_is_invariant = true;
    audit.market_nonprincipal_cash_is_nondecreasing = true;
    audit.market_path_npv_is_nondecreasing = true;
    audit.junior_nonprincipal_cash_is_nonincreasing = true;
    audit.junior_path_npv_is_nonincreasing = true;
    audit.market_negative_npv_probability_is_nonincreasing = true;
    audit.market_npv_shortfall_tails_are_nonincreasing = true;
    audit.market_cash_gained_equals_junior_cash_surrendered = true;
    audit.aggregate_cash_is_invariant = true;
    audit.pool_hurdle_npv_is_invariant = true;

    for (std::size_t candidate_index = 1U;
         candidate_index < evaluated.size(); ++candidate_index) {
        const EvaluatedCandidate& previous =
            evaluated[candidate_index - 1U];
        const EvaluatedCandidate& current = evaluated[candidate_index];
        if (previous.stack.scenarios.size() != current.stack.scenarios.size()) {
            throw std::logic_error(
                "market priority-cap scenario count changed across the cap grid");
        }

        update_metric_range(
            previous.report.market_negative_npv_probability,
            current.report.market_negative_npv_probability,
            audit.maximum_negative_npv_probability_monotonicity_violation,
            audit.market_negative_npv_probability_is_nonincreasing,
            update_nonincreasing);
        update_tail_range(previous.report.market_npv_shortfall_es95_million,
            current.report.market_npv_shortfall_es95_million,
            audit.maximum_npv_shortfall_tail_monotonicity_violation_million,
            audit.market_npv_shortfall_tails_are_nonincreasing,
            update_nonincreasing);
        update_tail_range(previous.report.market_npv_shortfall_es99_million,
            current.report.market_npv_shortfall_es99_million,
            audit.maximum_npv_shortfall_tail_monotonicity_violation_million,
            audit.market_npv_shortfall_tails_are_nonincreasing,
            update_nonincreasing);

        const auto& previous_wal =
            previous.report.market_principal_cash_wal_years;
        const auto& current_wal = current.report.market_principal_cash_wal_years;
        if (previous_wal.has_value() != current_wal.has_value()) {
            audit.market_principal_wal_is_invariant = false;
            audit.maximum_market_wal_change_years =
                std::numeric_limits<double>::infinity();
        } else if (previous_wal.has_value()) {
            update_change(previous_wal->minimum.value_years,
                current_wal->minimum.value_years,
                audit.maximum_market_wal_change_years,
                audit.market_principal_wal_is_invariant);
            update_change(previous_wal->central_years,
                current_wal->central_years,
                audit.maximum_market_wal_change_years,
                audit.market_principal_wal_is_invariant);
            update_change(previous_wal->maximum.value_years,
                current_wal->maximum.value_years,
                audit.maximum_market_wal_change_years,
                audit.market_principal_wal_is_invariant);
        }

        for (std::size_t scenario_index = 0U;
             scenario_index < previous.stack.scenarios.size();
             ++scenario_index) {
            const CapitalStackScenarioResult& a =
                previous.stack.scenarios[scenario_index];
            const CapitalStackScenarioResult& b =
                current.stack.scenarios[scenario_index];
            if (a.scenario_id != b.scenario_id || a.tranches.size() != 2U ||
                b.tranches.size() != 2U) {
                throw std::logic_error(
                    "market priority-cap scenario or tranche ordering changed across the cap grid");
            }
            const CapitalStackTrancheScenarioResult& junior_a = a.tranches[0];
            const CapitalStackTrancheScenarioResult& market_a = a.tranches[1];
            const CapitalStackTrancheScenarioResult& junior_b = b.tranches[0];
            const CapitalStackTrancheScenarioResult& market_b = b.tranches[1];

            update_change(market_a.total_contributions_million,
                market_b.total_contributions_million,
                audit.maximum_market_contribution_change_million,
                audit.market_contributions_are_invariant);
            update_change(market_a.principal_cash_distribution_million,
                market_b.principal_cash_distribution_million,
                audit.maximum_market_principal_cash_change_million,
                audit.market_principal_cash_is_invariant);
            update_change(market_a.realized_principal_loss_million,
                market_b.realized_principal_loss_million,
                audit.maximum_market_principal_risk_change,
                audit.market_principal_risk_is_invariant);
            update_change(market_a.unresolved_principal_exposure_million,
                market_b.unresolved_principal_exposure_million,
                audit.maximum_market_principal_risk_change,
                audit.market_principal_risk_is_invariant);
            update_change(market_a.principal_cash_shortfall_million,
                market_b.principal_cash_shortfall_million,
                audit.maximum_market_principal_risk_change,
                audit.market_principal_risk_is_invariant);
            update_nondecreasing(market_a.nonprincipal_cash_distribution_million,
                market_b.nonprincipal_cash_distribution_million,
                audit.maximum_market_nonprincipal_monotonicity_violation_million,
                audit.market_nonprincipal_cash_is_nondecreasing);
            update_nondecreasing(market_a.npv_at_tranche_hurdle_million,
                market_b.npv_at_tranche_hurdle_million,
                audit.maximum_market_path_npv_monotonicity_violation_million,
                audit.market_path_npv_is_nondecreasing);
            update_nonincreasing(junior_a.nonprincipal_cash_distribution_million,
                junior_b.nonprincipal_cash_distribution_million,
                audit.maximum_junior_nonprincipal_monotonicity_violation_million,
                audit.junior_nonprincipal_cash_is_nonincreasing);
            update_nonincreasing(junior_a.npv_at_tranche_hurdle_million,
                junior_b.npv_at_tranche_hurdle_million,
                audit.maximum_junior_path_npv_monotonicity_violation_million,
                audit.junior_path_npv_is_nonincreasing);
            update_change(a.stack_nominal_net_cash_million,
                b.stack_nominal_net_cash_million,
                audit.maximum_aggregate_cash_change_million,
                audit.aggregate_cash_is_invariant);
            update_change(a.fully_funded_stack_npv_at_pool_hurdle_million,
                b.fully_funded_stack_npv_at_pool_hurdle_million,
                audit.maximum_pool_hurdle_npv_change_million,
                audit.pool_hurdle_npv_is_invariant);

            std::vector<std::size_t> cash_months;
            const auto append_months = [&cash_months](const auto& tranche) {
                for (const auto& cash_flow : tranche.monthly_cash_flows) {
                    cash_months.push_back(cash_flow.month);
                }
            };
            append_months(junior_a);
            append_months(market_a);
            append_months(junior_b);
            append_months(market_b);
            std::sort(cash_months.begin(), cash_months.end());
            cash_months.erase(
                std::unique(cash_months.begin(), cash_months.end()),
                cash_months.end());
            for (const std::size_t month : cash_months) {
                const CapitalStackMonthlyTrancheCashFlow* const market_flow_a =
                    cash_flow_at_month(market_a, month);
                const CapitalStackMonthlyTrancheCashFlow* const market_flow_b =
                    cash_flow_at_month(market_b, month);
                const double market_nonprincipal_a =
                    nonprincipal_cash_at_month(market_a, month);
                const double market_nonprincipal_b =
                    nonprincipal_cash_at_month(market_b, month);
                const double junior_nonprincipal_a =
                    nonprincipal_cash_at_month(junior_a, month);
                const double junior_nonprincipal_b =
                    nonprincipal_cash_at_month(junior_b, month);
                update_nondecreasing(market_nonprincipal_a,
                    market_nonprincipal_b,
                    audit
                        .maximum_market_nonprincipal_monotonicity_violation_million,
                    audit.market_nonprincipal_cash_is_nondecreasing);
                update_nonincreasing(junior_nonprincipal_a,
                    junior_nonprincipal_b,
                    audit
                        .maximum_junior_nonprincipal_monotonicity_violation_million,
                    audit.junior_nonprincipal_cash_is_nonincreasing);

                const double market_principal_a = market_flow_a == nullptr
                    ? 0.0
                    : market_flow_a->principal_cash_distribution_million;
                const double market_principal_b = market_flow_b == nullptr
                    ? 0.0
                    : market_flow_b->principal_cash_distribution_million;
                update_change(market_principal_a, market_principal_b,
                    audit.maximum_market_principal_cash_change_million,
                    audit.market_principal_cash_is_invariant);
                const double market_contribution_a = market_flow_a == nullptr
                    ? 0.0
                    : market_flow_a->par_subscription_million +
                        market_flow_a->pro_rata_pool_cost_call_million;
                const double market_contribution_b = market_flow_b == nullptr
                    ? 0.0
                    : market_flow_b->par_subscription_million +
                        market_flow_b->pro_rata_pool_cost_call_million;
                update_change(market_contribution_a, market_contribution_b,
                    audit.maximum_market_contribution_change_million,
                    audit.market_contributions_are_invariant);

                const double market_gain =
                    market_nonprincipal_b - market_nonprincipal_a;
                const double junior_surrender =
                    junior_nonprincipal_a - junior_nonprincipal_b;
                const double transfer_error =
                    std::abs(market_gain - junior_surrender);
                audit.maximum_cash_transfer_reconciliation_error_million =
                    std::max(
                        audit.maximum_cash_transfer_reconciliation_error_million,
                        transfer_error);
                audit.market_cash_gained_equals_junior_cash_surrendered =
                    audit.market_cash_gained_equals_junior_cash_surrendered &&
                    transfer_error <= comparison_tolerance(
                        market_gain, junior_surrender);
                update_change(monthly_aggregate_net_cash(a, month),
                    monthly_aggregate_net_cash(b, month),
                    audit.maximum_aggregate_cash_change_million,
                    audit.aggregate_cash_is_invariant);
            }
        }
    }
    return audit;
}

[[nodiscard]] std::size_t fixed_constraint_count(
    const RobustMarketPriorityCapConstraints& value) noexcept {
    const bool present[] = {
        value.minimum_robust_aggregate_npv_million.has_value(),
        value.maximum_market_expected_loss_fraction.has_value(),
        value.maximum_market_principal_loss_es95_fraction.has_value(),
        value.maximum_market_principal_loss_es99_fraction.has_value(),
        value.maximum_market_principal_impairment_probability.has_value(),
        value.maximum_market_wal_years.has_value(),
        value.maximum_catalytic_first_loss_million.has_value(),
    };
    return static_cast<std::size_t>(
        std::count(std::begin(present), std::end(present), true));
}

[[nodiscard]] std::size_t cap_sensitive_constraint_count(
    const RobustMarketPriorityCapConstraints& value) noexcept {
    const bool present[] = {
        value.minimum_market_robust_npv_margin_fraction.has_value(),
        value.maximum_market_negative_npv_probability.has_value(),
        value.maximum_market_npv_shortfall_es95_fraction.has_value(),
        value.maximum_market_npv_shortfall_es99_fraction.has_value(),
    };
    return static_cast<std::size_t>(
        std::count(std::begin(present), std::end(present), true));
}

} // namespace

std::string_view to_string(RobustMarketPriorityCapStatus status) noexcept {
    switch (status) {
    case RobustMarketPriorityCapStatus::FixedStructureIneligible:
        return "fixed-structure-ineligible";
    case RobustMarketPriorityCapStatus::NoTestedMarketAdequateCap:
        return "no-tested-market-adequate-cap";
    case RobustMarketPriorityCapStatus::
        MarketAndJuniorRequirementsDoNotOverlap:
        return "market-and-junior-requirements-do-not-overlap";
    case RobustMarketPriorityCapStatus::MinimumTestedMarketAdequateCapFound:
        return "minimum-tested-market-adequate-cap-found";
    case RobustMarketPriorityCapStatus::MinimumTestedBalancedCapFound:
        return "minimum-tested-balanced-cap-found";
    }
    return "unknown";
}

void validate_robust_market_priority_cap_config(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& cap_term) {
    (void)validate_inputs(portfolio, probability_polytope, participation,
        base_stack, cap_term);
}

RobustMarketPriorityCapSummary evaluate_robust_market_priority_cap(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& cap_term) {
    const CapitalStackConfig base_stack_snapshot = base_stack;
    const ValidatedInputs inputs = validate_inputs(portfolio,
        probability_polytope, participation, base_stack, cap_term);
    const ProbabilityPolytopeProjector projector(
        portfolio, probability_polytope);

    RobustMarketPriorityCapSummary summary;
    summary.fixed_underlying_success_participation_fraction =
        base_stack.underlying_success_participation_fraction;
    summary.fixed_junior_first_loss_million =
        inputs.junior_first_loss_million;
    summary.aggregate_commitment_and_stack_detachment_million =
        inputs.commitment_million;
    summary.fixed_market_notional_million = inputs.market_notional_million;
    summary.fixed_junior_annual_physical_hurdle_rate =
        base_stack.tranches[0].annual_physical_hurdle_rate;
    summary.fixed_market_annual_physical_hurdle_rate =
        base_stack.tranches[1].annual_physical_hurdle_rate;
    summary.base_reference_market_priority_cap_million =
        inputs.reference_cap_million;
    summary.contractual_ceiling_million =
        cap_term.contractual_ceiling_million;
    summary.portfolio_cash_record_count = inputs.work.portfolio_cash_records;
    summary.portfolio_auxiliary_record_count =
        inputs.work.portfolio_auxiliary_records;
    summary.portfolio_record_count = inputs.work.portfolio_records;
    summary.probability_projection_work_units =
        inputs.work.probability_projection;
    summary.cash_path_work_units = inputs.work.cash_path;
    summary.structural_work_units = inputs.work.total;
    summary.evaluated_market_priority_cap_million_grid = inputs.cap_grid;
    summary.declared_fixed_structure_constraint_count =
        fixed_constraint_count(cap_term.constraints);
    summary.declared_cap_sensitive_market_constraint_count =
        cap_sensitive_constraint_count(cap_term.constraints);
    summary.junior_concession_constraint_is_declared =
        cap_term.constraints.maximum_catalytic_npv_concession_million
            .has_value();
    summary.base_reference_candidate_index = inputs.reference_cap_index;
    summary.contractual_ceiling_candidate_index = inputs.ceiling_cap_index;
    summary.model_limitation =
        "Finite synthetic physical-measure cap grid only. B is a lifetime "
        "priority ceiling on actual non-principal cash, not a return cap, "
        "coupon, promise, price, spread, expected return, annualized yield, "
        "rating, offer, or investor commitment. The evaluator does not solve "
        "or calibrate a market hurdle, interpolate a continuous minimum, or "
        "establish demand, suitability, legal enforceability, tax treatment, "
        "regulatory capital, empirical calibration, capital mobilization, or "
        "crowding-in. Different robust and tail endpoints can have different "
        "probability witnesses and must not be combined as one state; market "
        "and junior own-hurdle NPVs must not be added when their hurdles differ.";

    std::vector<EvaluatedCandidate> evaluated;
    evaluated.reserve(inputs.cap_grid.size());
    summary.candidates.reserve(inputs.cap_grid.size());
    for (const double cap_million : inputs.cap_grid) {
        evaluated.push_back(evaluate_candidate(portfolio,
            probability_polytope, participation, base_stack, cap_term,
            projector, cap_million));
        summary.candidates.push_back(evaluated.back().report);
        const std::size_t candidate_index = summary.candidates.size() - 1U;
        if (summary.candidates.back().market_adequate) {
            summary.market_adequate_candidate_indices.push_back(
                candidate_index);
        }
        if (summary.candidates.back().balanced) {
            summary.balanced_candidate_indices.push_back(candidate_index);
        }
    }

    if (!summary.market_adequate_candidate_indices.empty()) {
        const std::size_t first =
            summary.market_adequate_candidate_indices.front();
        summary.minimum_tested_market_adequate_candidate_index = first;
        if (first > 0U) {
            summary.previous_tested_candidate_before_market_adequate_index =
                first - 1U;
        }
    }
    if (!summary.balanced_candidate_indices.empty()) {
        const std::size_t first = summary.balanced_candidate_indices.front();
        summary.minimum_tested_balanced_candidate_index = first;
        if (first > 0U) {
            summary.previous_tested_candidate_before_balanced_index =
                first - 1U;
        }
    }

    const bool fixed_eligible = std::all_of(summary.candidates.begin(),
        summary.candidates.end(),
        [](const RobustMarketPriorityCapCandidate& candidate) {
            return candidate.fixed_structure_eligible;
        });
    if (!fixed_eligible) {
        summary.status =
            RobustMarketPriorityCapStatus::FixedStructureIneligible;
    } else if (!summary.minimum_tested_market_adequate_candidate_index
                    .has_value()) {
        summary.status =
            RobustMarketPriorityCapStatus::NoTestedMarketAdequateCap;
    } else if (summary.junior_concession_constraint_is_declared &&
        !summary.candidates[
             *summary.minimum_tested_market_adequate_candidate_index]
             .junior_concession_limit_passes) {
        summary.status = RobustMarketPriorityCapStatus::
            MarketAndJuniorRequirementsDoNotOverlap;
    } else if (summary.junior_concession_constraint_is_declared) {
        summary.status =
            RobustMarketPriorityCapStatus::MinimumTestedBalancedCapFound;
    } else {
        summary.status = RobustMarketPriorityCapStatus::
            MinimumTestedMarketAdequateCapFound;
    }

    summary.grid_audit = audit_grid(evaluated,
        stack_config_equal(base_stack, base_stack_snapshot));
    return summary;
}

} // namespace naturalehia::cellular_finance
