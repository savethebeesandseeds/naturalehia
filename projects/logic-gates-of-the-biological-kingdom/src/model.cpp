// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/model.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace naturalehia::protein_logic {
namespace {

void validate_parameters(const ModelParameters& parameters) {
    if (!std::isfinite(parameters.baseline_log_odds) ||
        !std::isfinite(parameters.input_a_log_odds) ||
        !std::isfinite(parameters.input_b_log_odds) || !std::isfinite(parameters.joint_log_odds)) {
        throw std::invalid_argument("model parameters must be finite");
    }
}

void validate_threshold(double threshold) {
    if (!std::isfinite(threshold) || threshold <= 0.0 || threshold >= 1.0) {
        throw std::invalid_argument("threshold must be finite and strictly between 0 and 1");
    }
}

double probability_from_log_odds(double value) noexcept {
    if (value >= 0.0) {
        const double exponential = std::exp(-value);
        return 1.0 / (1.0 + exponential);
    }

    const double exponential = std::exp(value);
    return exponential / (1.0 + exponential);
}

} // namespace

double log_odds(const ModelParameters& parameters, InputState inputs) {
    validate_parameters(parameters);

    double result = parameters.baseline_log_odds;
    if (inputs.input_a) {
        result += parameters.input_a_log_odds;
    }
    if (inputs.input_b) {
        result += parameters.input_b_log_odds;
    }
    if (inputs.input_a && inputs.input_b) {
        result += parameters.joint_log_odds;
    }

    if (!std::isfinite(result)) {
        throw std::overflow_error("combined log odds cannot be represented");
    }
    return result;
}

double active_probability(const ModelParameters& parameters, InputState inputs) {
    return probability_from_log_odds(log_odds(parameters, inputs));
}

LogicalLevel classify_probability(double probability, double threshold) {
    if (!std::isfinite(probability) || probability < 0.0 || probability > 1.0) {
        throw std::invalid_argument("probability must be finite and within [0, 1]");
    }
    validate_threshold(threshold);
    return probability >= threshold ? LogicalLevel::On : LogicalLevel::Off;
}

XorAssessment assess_xor(const ModelParameters& parameters, double threshold) {
    validate_parameters(parameters);
    validate_threshold(threshold);

    XorAssessment assessment{};
    assessment.threshold = threshold;

    bool passes_threshold = true;
    for (std::size_t index = 0; index < kCanonicalInputOrder.size(); ++index) {
        const InputState inputs = kCanonicalInputOrder[index];
        const double state_log_odds = log_odds(parameters, inputs);
        const double probability = probability_from_log_odds(state_log_odds);
        const LogicalLevel expected = expected_xor_level(inputs);
        const LogicalLevel observed = classify_probability(probability, threshold);

        assessment.states[index] = StateEvaluation{
            inputs, state_log_odds, probability, expected, observed,
        };
        passes_threshold = passes_threshold && expected == observed;
    }

    assessment.on_floor =
        std::min(assessment.states[1].active_probability, assessment.states[2].active_probability);
    assessment.off_ceiling =
        std::max(assessment.states[0].active_probability, assessment.states[3].active_probability);
    assessment.separation_margin = assessment.on_floor - assessment.off_ceiling;
    assessment.passes_threshold = passes_threshold;
    return assessment;
}

} // namespace naturalehia::protein_logic
