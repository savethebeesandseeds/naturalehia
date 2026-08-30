// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/model.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Loads a strict UTF-8 key=value file. Blank lines and lines beginning with '#'
// are ignored. Unknown and duplicate keys are errors rather than silent no-ops.
[[nodiscard]] SimulationConfig load_config(
    const std::filesystem::path& path);

// Emits the complete normalized input set used by the model. This is intended
// for audit logs and reproducible review, not as a substitute for source data.
void print_normalized_config(
    std::ostream& output, const SimulationConfig& config);

} // namespace naturalehia::cellular_finance

