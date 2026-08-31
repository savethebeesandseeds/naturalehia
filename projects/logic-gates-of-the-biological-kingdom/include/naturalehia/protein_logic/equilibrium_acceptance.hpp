// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/protein_logic/equilibrium_model.hpp>

#include <optional>
#include <string_view>

namespace naturalehia::protein_logic {

// NotAssessed is distinct from Fail: it records that no criterion was
// supplied. An overall result can pass only when every criterion is present
// and passes.
enum class CriterionOutcome : unsigned char {
    NotAssessed,
    Pass,
    Fail,
};

[[nodiscard]] constexpr std::string_view criterion_outcome_name(
    CriterionOutcome outcome) noexcept {
    switch (outcome) {
    case CriterionOutcome::NotAssessed:
        return "not_assessed";
    case CriterionOutcome::Pass:
        return "pass";
    case CriterionOutcome::Fail:
        return "fail";
    }
    return "unknown";
}

// All quantities are model-qualified probability bounds. In particular,
// intended_off_activity_ceiling is not an off-target or cross-talk metric, and
// single_high_floor_imbalance is not full response-surface symmetry.
struct SteadyStateXorMetrics {
    double basal_off_activity_ceiling{0.0};
    double input_a_only_on_floor{0.0};
    double input_a_only_on_ceiling{0.0};
    double input_b_only_on_floor{0.0};
    double input_b_only_on_ceiling{0.0};
    double joint_high_off_activity_ceiling{0.0};
    double on_floor{0.0};
    double intended_off_activity_ceiling{0.0};
    double separation{0.0};
    double single_high_floor_imbalance{0.0};
    // Real-arithmetic bound on the absolute difference across the two complete
    // single-high response envelopes. The stored binary64 value is not an
    // outward-rounded certified bound. It also includes within-region variation.
    double single_high_response_gap_upper_bound{0.0};
};

// No field has a scientific default. A missing value remains NotAssessed.
// Criterion provenance belongs in the calling protocol or result record.
struct SteadyStateXorCriteria {
    std::optional<double> threshold{};
    std::optional<double> minimum_separation{};
    std::optional<double> maximum_intended_off_activity{};
    std::optional<double> maximum_single_high_floor_imbalance{};
};

struct SteadyStateXorAcceptance {
    SteadyStateXorMetrics metrics{};
    SteadyStateXorCriteria criteria{};
    double decision_tolerance{kAssessmentDecisionTolerance};
    std::optional<double> on_threshold_margin{};
    std::optional<double> off_threshold_margin{};
    std::optional<double> threshold_margin{};
    std::optional<double> separation_clearance{};
    std::optional<double> intended_off_activity_clearance{};
    std::optional<double> single_high_floor_balance_clearance{};
    CriterionOutcome threshold_outcome{CriterionOutcome::NotAssessed};
    CriterionOutcome separation_outcome{CriterionOutcome::NotAssessed};
    CriterionOutcome intended_off_activity_outcome{CriterionOutcome::NotAssessed};
    CriterionOutcome single_high_floor_balance_outcome{CriterionOutcome::NotAssessed};
    CriterionOutcome overall_outcome{CriterionOutcome::NotAssessed};
};

// Recomputes all aggregates from canonical regional bounds rather than trusting
// cached GlobalAssessment aggregates supplied by a caller.
[[nodiscard]] SteadyStateXorMetrics compute_steady_state_xor_metrics(
    const GlobalAssessment& assessment);

// Present criteria are strict and use kAssessmentDecisionTolerance as an
// operational binary64 deadband, not a certified error bound. Missing criteria
// remain NotAssessed. This is model acceptance only; it cannot assess kinetics,
// reset, cross-talk, foldability, experimental function, or biosafety.
[[nodiscard]] SteadyStateXorAcceptance assess_steady_state_xor(
    const GlobalAssessment& assessment, const SteadyStateXorCriteria& criteria = {});

} // namespace naturalehia::protein_logic
