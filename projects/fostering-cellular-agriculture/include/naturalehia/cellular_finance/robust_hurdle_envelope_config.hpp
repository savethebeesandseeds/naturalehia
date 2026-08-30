// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_hurdle_envelope.hpp>

#include <filesystem>
#include <iosfwd>

namespace naturalehia::cellular_finance {

// Closed v0.1 key=value schema. Every envelope, observation, comparability,
// and adjustment field is mandatory. Observations and their adjustments are
// returned in canonical id order. Unknown, duplicate, missing, unsafe,
// non-finite, incoherent, and resource-excessive inputs are rejected.
[[nodiscard]] RobustHurdleEnvelopeConfig
parse_robust_hurdle_envelope_config(std::istream& input);

[[nodiscard]] RobustHurdleEnvelopeConfig
load_robust_hurdle_envelope_config(const std::filesystem::path& path);

// Emits every field in a deterministic reloadable representation. Invalid
// in-memory records are rejected before any output is written.
void print_normalized_robust_hurdle_envelope_config(
    std::ostream& output, const RobustHurdleEnvelopeConfig& config);

} // namespace naturalehia::cellular_finance
