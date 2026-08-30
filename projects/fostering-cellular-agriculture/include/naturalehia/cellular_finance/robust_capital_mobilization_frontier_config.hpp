// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Closed v0.1 key=value schema. Optional numeric mandates use the literal
// "none" when absent. Unknown, duplicate, missing, unsafe, and non-finite
// inputs are rejected. Parsed grids are returned in canonical ascending order.
[[nodiscard]] RobustCapitalMobilizationFrontierConfig
parse_robust_capital_mobilization_frontier_config(std::istream& input);

[[nodiscard]] RobustCapitalMobilizationFrontierConfig
load_robust_capital_mobilization_frontier_config(
    const std::filesystem::path& path);

// Emits every field in a deterministic reloadable representation. Grid order
// is canonicalized, and invalid in-memory terms are rejected before output.
void print_normalized_robust_capital_mobilization_frontier_config(
    std::ostream& output,
    const RobustCapitalMobilizationFrontierConfig& config);

} // namespace naturalehia::cellular_finance
