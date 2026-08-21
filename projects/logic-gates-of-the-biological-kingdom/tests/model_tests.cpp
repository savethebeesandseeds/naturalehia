// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/model.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {

using naturalehia::protein_logic::InputState;
using naturalehia::protein_logic::LogicalLevel;
using naturalehia::protein_logic::ModelParameters;

class TestContext {
public:
    void expect(bool condition, std::string_view message) {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void expect_near(double actual, double expected, double tolerance, std::string_view message) {
        expect(std::abs(actual - expected) <= tolerance, message);
    }

    [[nodiscard]] int failures() const noexcept {
        return failures_;
    }

private:
    int failures_{0};
};

void test_canonical_truth_table(TestContext& context) {
    using naturalehia::protein_logic::expected_xor_level;
    using naturalehia::protein_logic::kCanonicalInputOrder;

    context.expect(kCanonicalInputOrder[0] == InputState{false, false}, "state 0 is 00");
    context.expect(kCanonicalInputOrder[1] == InputState{true, false}, "state 1 is 10");
    context.expect(kCanonicalInputOrder[2] == InputState{false, true}, "state 2 is 01");
    context.expect(kCanonicalInputOrder[3] == InputState{true, true}, "state 3 is 11");
    context.expect(expected_xor_level(kCanonicalInputOrder[0]) == LogicalLevel::Off,
        "00 expects OFF");
    context.expect(expected_xor_level(kCanonicalInputOrder[1]) == LogicalLevel::On,
        "10 expects ON");
    context.expect(expected_xor_level(kCanonicalInputOrder[2]) == LogicalLevel::On,
        "01 expects ON");
    context.expect(expected_xor_level(kCanonicalInputOrder[3]) == LogicalLevel::Off,
        "11 expects OFF");
}

void test_negative_joint_effect_can_separate_xor(TestContext& context) {
    const ModelParameters parameters{-4.0, 8.0, 8.0, -16.0};
    const auto assessment = naturalehia::protein_logic::assess_xor(parameters);

    context.expect(assessment.passes_threshold, "illustrative parameters pass XOR at 0.5");
    context.expect(assessment.separation_margin > 0.96,
        "illustrative XOR has broad endpoint margin");
    context.expect_near(assessment.states[0].log_odds, -4.0, 1e-12, "00 log odds");
    context.expect_near(assessment.states[1].log_odds, 4.0, 1e-12, "10 log odds");
    context.expect_near(assessment.states[2].log_odds, 4.0, 1e-12, "01 log odds");
    context.expect_near(assessment.states[3].log_odds, -4.0, 1e-12, "11 log odds");
    context.expect_near(assessment.on_floor - assessment.off_ceiling, assessment.separation_margin,
        1e-12, "reported margin matches definition");
}

void test_additive_activation_does_not_form_xor(TestContext& context) {
    const ModelParameters parameters{-4.0, 8.0, 8.0, 0.0};
    const auto assessment = naturalehia::protein_logic::assess_xor(parameters);

    context.expect(!assessment.passes_threshold, "additive example fails XOR");
    context.expect(assessment.states[3].observed_level == LogicalLevel::On,
        "additive joint-input state remains ON");
    context.expect(assessment.separation_margin < 0.0, "additive example has negative XOR margin");
}

void test_probability_transform_is_stable(TestContext& context) {
    ModelParameters parameters{1000.0, 0.0, 0.0, 0.0};
    const double high = naturalehia::protein_logic::active_probability(parameters, {false, false});
    context.expect(std::isfinite(high) && high == 1.0, "large positive log odds are stable");

    parameters.baseline_log_odds = -1000.0;
    const double low = naturalehia::protein_logic::active_probability(parameters, {false, false});
    context.expect(std::isfinite(low) && low == 0.0, "large negative log odds are stable");
}

void test_invalid_domains_are_rejected(TestContext& context) {
    bool invalid_threshold_threw = false;
    try {
        (void)naturalehia::protein_logic::assess_xor({-4.0, 8.0, 8.0, -16.0}, 1.0);
    } catch (const std::invalid_argument&) {
        invalid_threshold_threw = true;
    }
    context.expect(invalid_threshold_threw, "threshold 1 is rejected");

    bool invalid_probability_threw = false;
    try {
        (void)naturalehia::protein_logic::classify_probability(-0.1);
    } catch (const std::invalid_argument&) {
        invalid_probability_threw = true;
    }
    context.expect(invalid_probability_threw, "negative probability is rejected");

    ModelParameters invalid_parameters{-4.0, 8.0, 8.0, -16.0};
    invalid_parameters.joint_log_odds = std::numeric_limits<double>::quiet_NaN();
    bool invalid_parameter_threw = false;
    try {
        (void)naturalehia::protein_logic::active_probability(invalid_parameters, {true, true});
    } catch (const std::invalid_argument&) {
        invalid_parameter_threw = true;
    }
    context.expect(invalid_parameter_threw, "non-finite parameter is rejected");
}

} // namespace

int main() {
    TestContext context;
    test_canonical_truth_table(context);
    test_negative_joint_effect_can_separate_xor(context);
    test_additive_activation_does_not_form_xor(context);
    test_probability_transform_is_stable(context);
    test_invalid_domains_are_rejected(context);

    if (context.failures() != 0) {
        std::cerr << context.failures() << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All protein logic model tests passed\n";
    return 0;
}