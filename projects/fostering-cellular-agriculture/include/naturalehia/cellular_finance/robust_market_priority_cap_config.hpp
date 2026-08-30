// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_market_priority_cap.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Closed v0.1 key=value schema. Optional numeric mandates use the exact
// literal "none" when absent. Unknown, duplicate, missing, unsafe, and
// non-finite inputs are rejected. The finite cap grid is returned in
// canonical ascending order.
[[nodiscard]] RobustMarketPriorityCapConfig
parse_robust_market_priority_cap_config(std::istream& input);

[[nodiscard]] RobustMarketPriorityCapConfig
load_robust_market_priority_cap_config(const std::filesystem::path& path);

// Emits every field in a deterministic reloadable representation. Invalid
// in-memory terms are rejected before any output is written.
void print_normalized_robust_market_priority_cap_config(
    std::ostream& output, const RobustMarketPriorityCapConfig& config);

} // namespace naturalehia::cellular_finance
