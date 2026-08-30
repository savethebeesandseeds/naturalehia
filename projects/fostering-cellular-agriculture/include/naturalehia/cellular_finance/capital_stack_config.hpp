// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/capital_stack.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Closed v0.1 key=value parser. Unknown, duplicate, and missing keys are
// errors; normalized output contains every field and is reloadable.
[[nodiscard]] CapitalStackConfig parse_capital_stack_config(
    std::istream& input);

[[nodiscard]] CapitalStackConfig load_capital_stack_config(
    const std::filesystem::path& path);

void print_normalized_capital_stack_config(
    std::ostream& output, const CapitalStackConfig& config);

} // namespace naturalehia::cellular_finance
