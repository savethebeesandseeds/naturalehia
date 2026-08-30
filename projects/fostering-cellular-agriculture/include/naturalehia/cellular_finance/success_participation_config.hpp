// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/success_participation.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Parses the closed v0.1 key=value schema from an existing stream. Blank
// lines and full-line comments beginning with '#' are ignored. Unknown,
// missing, and duplicate keys are errors.
[[nodiscard]] SuccessParticipationConfig parse_success_participation_config(
    std::istream& input);

// Loads and parses one bounded configuration file using the same strict
// schema as parse_success_participation_config.
[[nodiscard]] SuccessParticipationConfig load_success_participation_config(
    const std::filesystem::path& path);

// Emits a deterministic, reloadable representation of every v0.1 input.
// Invalid in-memory configurations are rejected before any output is written.
void print_normalized_success_participation_config(
    std::ostream& output, const SuccessParticipationConfig& config);

} // namespace naturalehia::cellular_finance
