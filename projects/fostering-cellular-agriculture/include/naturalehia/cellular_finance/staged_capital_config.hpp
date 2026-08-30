// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/staged_capital.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Strict key=value loader for a complete finite set of explicit, weighted
// synthetic cases. Unknown, missing, and duplicate keys are errors.
[[nodiscard]] StagedCapitalConfig load_staged_capital_config(
    const std::filesystem::path& path);

void print_normalized_staged_capital_config(
    std::ostream& output, const StagedCapitalConfig& config);

} // namespace naturalehia::cellular_finance
