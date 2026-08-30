// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/provider_price_ladder.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Parses the closed v0.1 key=value schema from an existing stream. Blank
// lines and full-line comments beginning with '#' are ignored. Unknown,
// missing, and duplicate keys are errors.
[[nodiscard]] ProviderPriceLadderConfig parse_provider_price_ladder_config(
    std::istream& input);

// Loads and parses one bounded configuration file using the same strict
// schema as parse_provider_price_ladder_config.
[[nodiscard]] ProviderPriceLadderConfig load_provider_price_ladder_config(
    const std::filesystem::path& path);

// Emits every v0.1 field in deterministic, reloadable order. Invalid in-memory
// configurations are rejected before any output is written.
void print_normalized_provider_price_ladder_config(
    std::ostream& output, const ProviderPriceLadderConfig& config);

} // namespace naturalehia::cellular_finance
