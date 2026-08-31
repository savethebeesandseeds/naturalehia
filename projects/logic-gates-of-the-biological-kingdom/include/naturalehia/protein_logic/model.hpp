// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>

namespace naturalehia::protein_logic {

enum class LogicalLevel : unsigned char {
    Off,
    On,
};

struct InputState {
    bool input_a{false};
    bool input_b{false};

    friend constexpr bool operator==(const InputState&, const InputState&) = default;
};

// Dimensionless effects on log(P(ON) / P(OFF)). This phenomenological model is
// a logic specification tool, not a molecular free-energy or folding model.
struct ModelParameters {
    double baseline_log_odds{0.0};
    double input_a_log_odds{0.0};
    double input_b_log_odds{0.0};
    double joint_log_odds{0.0};
};

struct StateEvaluation {
    InputState inputs{};
    double log_odds{0.0};
    double active_probability{0.0};
    LogicalLevel expected_xor_level{LogicalLevel::Off};
    LogicalLevel observed_level{LogicalLevel::Off};
};

struct XorAssessment {
    std::array<StateEvaluation, 4> states{};
    double on_floor{0.0};
    double off_ceiling{0.0};
    double separation_margin{0.0};
    double threshold{0.5};
    bool passes_threshold{false};
};

inline constexpr std::array<InputState, 4> kCanonicalInputOrder{{
    {false, false},
    {true, false},
    {false, true},
    {true, true},
}};

[[nodiscard]] constexpr LogicalLevel expected_xor_level(InputState inputs) noexcept {
    return inputs.input_a != inputs.input_b ? LogicalLevel::On : LogicalLevel::Off;
}

// Throws std::invalid_argument when a parameter is not finite and
// std::overflow_error when their sum cannot be represented.
[[nodiscard]] double log_odds(const ModelParameters& parameters, InputState inputs);

// Uses a numerically stable logistic transform. Throws the same exceptions as
// log_odds().
[[nodiscard]] double active_probability(const ModelParameters& parameters, InputState inputs);

// Threshold must be finite and strictly between zero and one.
[[nodiscard]] LogicalLevel classify_probability(double probability, double threshold = 0.5);

// Evaluates states in kCanonicalInputOrder. A positive separation margin means
// some threshold can separate the endpoint ON and OFF values; passes_threshold
// additionally evaluates the caller's declared threshold.
[[nodiscard]] XorAssessment assess_xor(const ModelParameters& parameters, double threshold = 0.5);

} // namespace naturalehia::protein_logic
