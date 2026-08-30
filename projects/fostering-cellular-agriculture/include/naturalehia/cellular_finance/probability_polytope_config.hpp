// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/probability_polytope.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Strict key=value loader for a finite physical-probability polytope. Unknown,
// missing, and duplicate keys are errors. Event membership is an explicit set
// of scenario identifiers; it is never inferred from portfolio labels or
// factor tags. Matching the scenario taxonomy and central weights to a
// PortfolioConfig remains the responsibility of
// validate_probability_polytope_config.
[[nodiscard]] ProbabilityPolytopeConfig load_probability_polytope_config(
    const std::filesystem::path& path);

// Emits one canonical, reloadable semantic representation: scenarios, events,
// and event members are sorted independently by identifier. Caller stream
// formatting and locale are restored before this function returns.
void print_normalized_probability_polytope_config(
    std::ostream& output, const ProbabilityPolytopeConfig& config);

} // namespace naturalehia::cellular_finance
