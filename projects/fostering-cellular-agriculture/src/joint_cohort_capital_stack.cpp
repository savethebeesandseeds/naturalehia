// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/joint_cohort_capital_stack.hpp>

#include <stdexcept>

namespace naturalehia::cellular_finance {

JointCohortCapitalStackResult evaluate_joint_cohort_capital_stack(
    const JointCohortAnalysisConfig& cohort_config,
    const PortfolioConfig& portfolio,
    const std::vector<JointCohortObservation>& observations,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack) {
    JointCohortCapitalStackResult result;
    result.cohort = evaluate_joint_cohort(
        cohort_config, portfolio, observations);

    if (result.cohort.calibrated_execution_authorized) {
        throw std::logic_error(
            "joint-cohort capital-stack bridge cannot authorize calibrated execution");
    }
    if (result.cohort.financial_ranges_available !=
            result.cohort.financial_ranges.has_value() ||
        result.cohort.financial_ranges_available !=
            result.cohort.generated_probability_envelope.has_value()) {
        throw std::logic_error(
            "joint-cohort primary financial export state is inconsistent");
    }

    const bool primary_export_available =
        result.cohort.candidate_only &&
        result.cohort.generated_envelope_synthetic &&
        result.cohort.included_cluster_ids_unique &&
        result.cohort.primary_outer_set_available &&
        result.cohort.portfolio_reference_within_primary_bounds &&
        result.cohort.financial_ranges_available &&
        result.cohort.generated_probability_envelope.has_value();
    if (!primary_export_available) {
        result.block_reason = result.cohort.block_reason.empty()
            ? "joint-cohort primary probability-envelope export is unavailable"
            : result.cohort.block_reason;
        return result;
    }

    const PortfolioConfig selected_underlying =
        apply_success_participation_fraction(portfolio, participation,
            stack.underlying_success_participation_fraction);
    result.selected_underlying_financial_ranges =
        evaluate_portfolio_ambiguity(selected_underlying,
            *result.cohort.generated_probability_envelope);
    result.capital_stack = evaluate_capital_stack(portfolio,
        *result.cohort.generated_probability_envelope,
        participation, stack);
    return result;
}

} // namespace naturalehia::cellular_finance
