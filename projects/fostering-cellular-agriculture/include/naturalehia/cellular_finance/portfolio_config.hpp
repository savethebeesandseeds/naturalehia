// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Strict key=value loader for a complete finite set of explicit weighted joint
// scenarios. Unknown, missing, and duplicate keys are errors.
[[nodiscard]] PortfolioConfig load_portfolio_config(
    const std::filesystem::path& path);

// Parses one immutable, already bounded byte snapshot. Hash-bound analytical
// packages use this overload so the verified and parsed bytes are identical.
[[nodiscard]] PortfolioConfig load_portfolio_config_bytes(
    std::string_view bytes);

void print_normalized_portfolio_config(
    std::ostream& output, const PortfolioConfig& config);

} // namespace naturalehia::cellular_finance
