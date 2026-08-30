// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_issue_price_support.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Closed v0.1 key=value schema. The single reference-price and support
// records are mandatory. Hurdle cases are finite, provenance-bearing records
// and are returned in canonical rate/id order. Unknown, duplicate, missing,
// unsafe, non-finite, incoherent, and resource-excessive inputs are rejected.
[[nodiscard]] RobustIssuePriceSupportConfig
parse_robust_issue_price_support_config(std::istream& input);

[[nodiscard]] RobustIssuePriceSupportConfig
load_robust_issue_price_support_config(const std::filesystem::path& path);

// Emits every field in a deterministic reloadable representation. Invalid
// in-memory terms are rejected before any output is written.
void print_normalized_robust_issue_price_support_config(
    std::ostream& output, const RobustIssuePriceSupportConfig& config);

} // namespace naturalehia::cellular_finance
