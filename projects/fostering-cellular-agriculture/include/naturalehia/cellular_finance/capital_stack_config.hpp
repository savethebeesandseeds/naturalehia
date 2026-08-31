// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/capital_stack.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Closed, version-aware key=value parser. Unknown, duplicate, and missing
// keys are errors within each supported version; normalized output contains
// every field defined by that version and is reloadable.
[[nodiscard]] CapitalStackConfig parse_capital_stack_config(
    std::istream& input);

[[nodiscard]] CapitalStackConfig load_capital_stack_config(
    const std::filesystem::path& path);

void print_normalized_capital_stack_config(
    std::ostream& output, const CapitalStackConfig& config);

} // namespace naturalehia::cellular_finance
