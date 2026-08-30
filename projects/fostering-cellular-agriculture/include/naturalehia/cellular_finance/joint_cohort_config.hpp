// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/joint_cohort.hpp>
#include <naturalehia/cellular_finance/portfolio.hpp>

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace naturalehia::cellular_finance {

struct JointCohortBoundFile {
    std::string relative_path{};
    std::string sha256{};
};

struct JointCohortPackageConfig {
    JointCohortAnalysisConfig analysis{};
    JointCohortBoundFile portfolio_file{};
    JointCohortBoundFile ledger_file{};
};

struct JointCohortPackage {
    std::filesystem::path directory{};
    JointCohortPackageConfig config{};
    PortfolioConfig portfolio{};
    std::vector<JointCohortObservation> observations{};
};

// Strict key=value parser. Unknown, duplicate, missing, unsafe, non-finite,
// and non-candidate fields are rejected.
[[nodiscard]] JointCohortPackageConfig load_joint_cohort_config(
    const std::filesystem::path& path);

// Strict raw TSV parser. The returned rows, rather than aggregate counts, are
// the authoritative analytical input.
[[nodiscard]] std::vector<JointCohortObservation>
load_joint_cohort_ledger(const std::filesystem::path& path);

// Confines both bound paths to the configuration package, reads bounded
// immutable byte snapshots, hashes and parses those exact bytes, then
// re-resolves and byte-compares fresh verified snapshots before return.
[[nodiscard]] JointCohortPackage load_joint_cohort_package(
    const std::filesystem::path& config_path);

void print_normalized_joint_cohort_config(
    std::ostream& output,
    const JointCohortPackageConfig& config);

void print_normalized_joint_cohort_ledger(
    std::ostream& output,
    const std::vector<JointCohortObservation>& observations);

} // namespace naturalehia::cellular_finance
