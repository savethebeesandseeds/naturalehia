// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/pooled_loss_protection.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Parses the closed v0.1 key=value schema from an existing stream. Blank
// lines and full-line comments beginning with '#' are ignored. Unknown,
// missing, and duplicate keys are errors.
[[nodiscard]] PooledLossProtectionConfig parse_pooled_loss_protection_config(
    std::istream& input);

// Loads and parses one bounded configuration file using the same strict
// schema as parse_pooled_loss_protection_config.
[[nodiscard]] PooledLossProtectionConfig load_pooled_loss_protection_config(
    const std::filesystem::path& path);

// Emits every v0.1 field in deterministic, reloadable order. Invalid in-memory
// configurations are rejected before any output is written.
void print_normalized_pooled_loss_protection_config(
    std::ostream& output, const PooledLossProtectionConfig& config);

} // namespace naturalehia::cellular_finance
