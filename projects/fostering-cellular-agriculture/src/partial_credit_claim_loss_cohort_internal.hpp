// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/partial_credit_claim_loss_cohort.hpp>

#include <filesystem>
#include <string_view>

namespace naturalehia::cellular_finance::detail {

// Private cross-translation-unit seam used only by the sealed evaluator. It
// reloads the binder without invoking the public evaluator, preventing a
// loader/evaluator recursion while keeping the ordinary in-memory evaluator
// available for unsealed mechanical inputs.
struct PartialCreditClaimLossCohortLoadAccess {
    [[nodiscard]] static PartialCreditClaimLossCohortPackage
    reload_for_evaluation(
        const std::filesystem::path& canonical_directory,
        std::string_view expected_cohort_config_sha256);
};

} // namespace naturalehia::cellular_finance::detail
