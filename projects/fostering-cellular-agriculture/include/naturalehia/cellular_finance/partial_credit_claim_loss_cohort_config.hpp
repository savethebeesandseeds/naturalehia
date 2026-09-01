// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/partial_credit_claim_loss_cohort.hpp>

#include <filesystem>

namespace naturalehia::cellular_finance {

// Strictly parses the closed cohort.cfg schema. This validates the declared
// paths and digests but does not open any of the four hash-bound files.
[[nodiscard]] PartialCreditClaimLossCohortConfig
load_partial_credit_claim_loss_cohort_config(
    const std::filesystem::path& config_path);

// Loads the exact five-file v0.1 binder rooted at root/cohort.cfg. Every bound
// file and included Claim Ledger root is confined, snapshotted, hash checked,
// and parsed before return. A successful load establishes package structure
// and the compiled population-frame gate only; it grants no empirical,
// calibration, pricing, probability, or Portfolio authority.
[[nodiscard]] PartialCreditClaimLossCohortPackage
load_partial_credit_claim_loss_cohort_package(
    const std::filesystem::path& root);

} // namespace naturalehia::cellular_finance
