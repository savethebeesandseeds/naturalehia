// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/funding_bridge.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Parses the closed v0.1 key=value schema from an existing stream. Blank
// lines and full-line comments beginning with '#' are ignored. Unknown,
// missing, and duplicate keys are errors. Validation here is intrinsic to the
// bridge object; Portfolio-dependent scenario and horizon checks remain in
// validate_funding_bridge_config.
[[nodiscard]] FundingBridgeConfig parse_funding_bridge_config(
    std::istream& input);

// Loads one bounded configuration file using the same strict schema as the
// stream parser.
[[nodiscard]] FundingBridgeConfig load_funding_bridge_config(
    const std::filesystem::path& path);

// Emits every v0.1 input in a deterministic, reloadable representation.
// Invalid in-memory bridge objects are rejected before output begins.
void print_normalized_funding_bridge_config(
    std::ostream& output, const FundingBridgeConfig& config);

} // namespace naturalehia::cellular_finance
