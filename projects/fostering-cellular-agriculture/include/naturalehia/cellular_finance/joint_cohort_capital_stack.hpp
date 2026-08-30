// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/capital_stack.hpp>
#include <naturalehia/cellular_finance/joint_cohort.hpp>

#include <optional>
#include <string>
#include <vector>

namespace naturalehia::cellular_finance {

// A deliberately thin evidence-to-claim bridge. The cohort result remains
// visible even when its statistical or financial export boundary blocks the
// capital stack. No probability fallback, participation solve, or second
// portfolio is admitted.
struct JointCohortCapitalStackResult {
    JointCohortResult cohort{};
    // Financial ranges for the fixed success-participation fraction that
    // actually enters the waterfall. The cohort's own financial_ranges remain
    // the unmodified, hash-bound portfolio view used as calibration evidence.
    std::optional<PortfolioAmbiguitySummary>
        selected_underlying_financial_ranges{};
    std::optional<CapitalStackSummary> capital_stack{};
    bool calibrated_execution_authorized{false};
    std::string block_reason{};
};

// Evaluates the authoritative cohort rows against the supplied portfolio and,
// only when every primary-envelope export condition is satisfied, applies that
// generated physical-P envelope to the existing fixed-q capital-stack engine.
// The programmatic surface does not establish file-hash provenance; callers
// that need it must load the inputs through load_joint_cohort_package.
[[nodiscard]] JointCohortCapitalStackResult
evaluate_joint_cohort_capital_stack(
    const JointCohortAnalysisConfig& cohort_config,
    const PortfolioConfig& portfolio,
    const std::vector<JointCohortObservation>& observations,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack);

} // namespace naturalehia::cellular_finance
