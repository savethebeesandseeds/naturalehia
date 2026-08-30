// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Strict key=value loader for a componentwise probability envelope over a
// finite set of explicitly named joint scenarios. Unknown, missing, and
// duplicate keys are errors. Matching the scenario set and central weights to
// a PortfolioConfig remains the responsibility of
// validate_portfolio_ambiguity_config.
[[nodiscard]] PortfolioAmbiguityConfig load_portfolio_ambiguity_config(
    const std::filesystem::path& path);

void print_normalized_portfolio_ambiguity_config(
    std::ostream& output, const PortfolioAmbiguityConfig& config);

} // namespace naturalehia::cellular_finance
