// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/equilibrium_acceptance.hpp>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using naturalehia::protein_logic::CriterionOutcome;
using naturalehia::protein_logic::EquilibriumParameters;
using naturalehia::protein_logic::GlobalAssessment;
using naturalehia::protein_logic::OperatingWindows;
using naturalehia::protein_logic::SteadyStateXorCriteria;
using naturalehia::protein_logic::XorRegion;

class TestContext {
public:
    void expect(bool condition, std::string_view message) {
        if (!condition) {
            ++failures_;
            std::cerr << "FAIL: " << message << '\n';
        }
    }

    void expect_near(
        double actual, double expected, double tolerance, std::string_view message) {
        expect(std::abs(actual - expected) <= tolerance, message);
    }

    template<typename Exception, typename Function>
    void expect_throws(Function&& function, std::string_view message) {
        bool caught = false;
        try {
            function();
        } catch (const Exception&) {
            caught = true;
        } catch (...) {
        }
        expect(caught, message);
    }

    [[nodiscard]] int failures() const noexcept {
        return failures_;
    }

private:
    int failures_{0};
};

EquilibriumParameters nominal_parameters() {
    return {
        -4.0,
        {1.0, 1.0, 10.0},
        {0.01, 0.01, 0.001},
    };
}

OperatingWindows nominal_windows() {
    return {
        {{0.0, 0.01}, {3.0, 10.0}},
        {{0.0, 0.01}, {3.0, 10.0}},
    };
}

GlobalAssessment nominal_assessment() {
    return naturalehia::protein_logic::assess_nominal(
        nominal_parameters(), nominal_windows()).global;
}

GlobalAssessment assessment_from_intervals(const std::array<double, 4>& lower,
    const std::array<double, 4>& upper) {
    GlobalAssessment assessment{};
    for (std::size_t index = 0; index < assessment.regions.size(); ++index) {
        const XorRegion region = naturalehia::protein_logic::kCanonicalXorRegionOrder[index];
        assessment.regions[index].region = region;
        assessment.regions[index].expected_level =
            naturalehia::protein_logic::expected_xor_level(region);
        assessment.regions[index].minimum.active_probability = lower[index];
        assessment.regions[index].maximum.active_probability = upper[index];
    }
    return assessment;
}

SteadyStateXorCriteria illustrative_criteria() {
    return {0.5, 0.3, 0.15, 0.05};
}

void test_nominal_metrics(TestContext& context) {
    const GlobalAssessment global = nominal_assessment();
    const auto metrics = naturalehia::protein_logic::compute_steady_state_xor_metrics(global);

    context.expect_near(metrics.basal_off_activity_ceiling,
        global.regions[0].maximum.active_probability, 0.0,
        "basal intended-OFF activity is reported separately");
    context.expect_near(metrics.joint_high_off_activity_ceiling,
        global.regions[3].maximum.active_probability, 0.0,
        "joint-high intended-OFF activity is reported separately");
    context.expect_near(metrics.input_a_only_on_floor,
        global.regions[1].minimum.active_probability, 0.0,
        "input-A-only floor is retained");
    context.expect_near(metrics.input_b_only_on_floor,
        global.regions[2].minimum.active_probability, 0.0,
        "input-B-only floor is retained");
    context.expect_near(metrics.on_floor, global.on_floor, 0.0,
        "acceptance metrics recompute the existing ON floor");
    context.expect_near(metrics.intended_off_activity_ceiling, global.off_ceiling, 0.0,
        "acceptance metrics recompute intended OFF activity");
    context.expect_near(metrics.separation, global.separation_margin, 0.0,
        "acceptance metrics recompute separation");
    context.expect_near(metrics.single_high_floor_imbalance, 0.0, 0.0,
        "symmetric regression fixture has equal single-high floors");
    context.expect_near(metrics.single_high_response_gap_upper_bound,
        global.regions[1].maximum.active_probability -
        global.regions[2].minimum.active_probability,
        1.0e-15, "envelope-gap upper bound includes within-region variation");
}

void test_missing_and_partial_criteria(TestContext& context) {
    const GlobalAssessment global = nominal_assessment();
    const auto absent = naturalehia::protein_logic::assess_steady_state_xor(global);
    context.expect(absent.overall_outcome == CriterionOutcome::NotAssessed,
        "absent criteria are not assessed rather than passed");
    context.expect(!absent.threshold_margin.has_value() &&
        !absent.separation_clearance.has_value() &&
        !absent.intended_off_activity_clearance.has_value() &&
        !absent.single_high_floor_balance_clearance.has_value(),
        "absent criteria do not fabricate numeric clearances");

    SteadyStateXorCriteria partial{};
    partial.threshold = 0.5;
    const auto partial_pass =
        naturalehia::protein_logic::assess_steady_state_xor(global, partial);
    context.expect(partial_pass.threshold_outcome == CriterionOutcome::Pass,
        "a present threshold is assessed");
    context.expect(partial_pass.overall_outcome == CriterionOutcome::NotAssessed,
        "a passing partial protocol cannot produce overall acceptance");

    partial.threshold = global.on_floor;
    const auto partial_fail =
        naturalehia::protein_logic::assess_steady_state_xor(global, partial);
    context.expect(partial_fail.overall_outcome == CriterionOutcome::Fail,
        "an observed failure remains failure even when other criteria are missing");
}

void test_complete_acceptance_and_null(TestContext& context) {
    const auto coupled = naturalehia::protein_logic::assess_steady_state_xor(
        nominal_assessment(), illustrative_criteria());
    context.expect(coupled.threshold_outcome == CriterionOutcome::Pass &&
        coupled.separation_outcome == CriterionOutcome::Pass &&
        coupled.intended_off_activity_outcome == CriterionOutcome::Pass &&
        coupled.single_high_floor_balance_outcome == CriterionOutcome::Pass &&
        coupled.overall_outcome == CriterionOutcome::Pass,
        "illustrative coupled fixture clears every explicit test-only criterion");

    EquilibriumParameters independent = nominal_parameters();
    independent.off.omega = 1.0;
    independent.on.omega = 1.0;
    const GlobalAssessment null_global = naturalehia::protein_logic::assess_nominal(
        independent, nominal_windows()).global;
    const auto null_assessment = naturalehia::protein_logic::assess_steady_state_xor(
        null_global, illustrative_criteria());
    context.expect(null_assessment.overall_outcome == CriterionOutcome::Fail,
        "nested independent-binding null fails the same explicit criteria");
}

void test_metrics_do_not_substitute_for_each_other(TestContext& context) {
    const SteadyStateXorCriteria criteria{0.5, 0.1, 0.2, 0.1};

    const auto weak_symmetric = naturalehia::protein_logic::assess_steady_state_xor(
        assessment_from_intervals({{0.1, 0.4, 0.4, 0.1}}, {{0.1, 0.4, 0.4, 0.1}}),
        criteria);
    context.expect(weak_symmetric.single_high_floor_balance_outcome ==
        CriterionOutcome::Pass,
        "equal weak responses can pass floor balance");
    context.expect(weak_symmetric.intended_off_activity_outcome == CriterionOutcome::Pass,
        "low intended-OFF activity can pass independently");
    context.expect(weak_symmetric.threshold_outcome == CriterionOutcome::Fail &&
        weak_symmetric.overall_outcome == CriterionOutcome::Fail,
        "balance and low OFF activity cannot hide absent ON activation");

    const auto poor_threshold = naturalehia::protein_logic::assess_steady_state_xor(
        assessment_from_intervals({{0.1, 0.4, 0.4, 0.1}}, {{0.1, 0.4, 0.4, 0.1}}),
        {0.8, 0.2, 0.2, 0.1});
    context.expect(poor_threshold.metrics.separation > 0.0 &&
        poor_threshold.separation_outcome == CriterionOutcome::Pass,
        "positive separation can clear its criterion");
    context.expect(poor_threshold.threshold_outcome == CriterionOutcome::Fail,
        "a badly placed fixed threshold still fails");

    const auto insufficient_separation = naturalehia::protein_logic::assess_steady_state_xor(
        assessment_from_intervals({{0.2, 0.8, 0.8, 0.2}}, {{0.2, 0.8, 0.8, 0.2}}),
        {0.5, 0.7, 0.3, 0.1});
    context.expect(insufficient_separation.threshold_outcome == CriterionOutcome::Pass,
        "threshold can pass independently");
    context.expect(insufficient_separation.separation_outcome == CriterionOutcome::Fail,
        "required separation remains an independent test");
}

void test_strict_decision_buffer(TestContext& context) {
    const GlobalAssessment global = nominal_assessment();
    const auto metrics = naturalehia::protein_logic::compute_steady_state_xor_metrics(global);
    const SteadyStateXorCriteria ties{
        metrics.on_floor,
        metrics.separation,
        metrics.intended_off_activity_ceiling,
        metrics.single_high_floor_imbalance,
    };
    const auto equality = naturalehia::protein_logic::assess_steady_state_xor(global, ties);
    context.expect(equality.threshold_outcome == CriterionOutcome::Fail &&
        equality.separation_outcome == CriterionOutcome::Fail &&
        equality.intended_off_activity_outcome == CriterionOutcome::Fail &&
        equality.single_high_floor_balance_outcome == CriterionOutcome::Fail,
        "equality fails every strict criterion");

    const SteadyStateXorCriteria adjacent{
        std::nextafter(metrics.on_floor, 0.0),
        std::nextafter(metrics.separation, 0.0),
        std::nextafter(metrics.intended_off_activity_ceiling, 1.0),
        std::nextafter(metrics.single_high_floor_imbalance, 1.0),
    };
    const auto one_ulp =
        naturalehia::protein_logic::assess_steady_state_xor(global, adjacent);
    context.expect(one_ulp.threshold_outcome == CriterionOutcome::Fail &&
        one_ulp.separation_outcome == CriterionOutcome::Fail &&
        one_ulp.intended_off_activity_outcome == CriterionOutcome::Fail &&
        one_ulp.single_high_floor_balance_outcome == CriterionOutcome::Fail,
        "one-ULP apparent clearances remain inside the operational deadband");

    constexpr double tau = naturalehia::protein_logic::kAssessmentDecisionTolerance;
    const GlobalAssessment dyadic = assessment_from_intervals(
        {{0.25, 0.75, 0.75, 0.25}}, {{0.25, 0.75, 0.75, 0.25}});
    const SteadyStateXorCriteria exact_tau{
        0.75 - tau,
        0.5 - tau,
        0.25 + tau,
        tau,
    };
    const auto at_deadband =
        naturalehia::protein_logic::assess_steady_state_xor(dyadic, exact_tau);
    context.expect(at_deadband.threshold_outcome == CriterionOutcome::Fail &&
        at_deadband.separation_outcome == CriterionOutcome::Fail &&
        at_deadband.intended_off_activity_outcome == CriterionOutcome::Fail &&
        at_deadband.single_high_floor_balance_outcome == CriterionOutcome::Fail,
        "an exact dyadic clearance of tau fails every operational criterion");

    const SteadyStateXorCriteria beyond_tau{
        std::nextafter(*exact_tau.threshold, 0.0),
        std::nextafter(*exact_tau.minimum_separation, 0.0),
        std::nextafter(*exact_tau.maximum_intended_off_activity, 1.0),
        std::nextafter(*exact_tau.maximum_single_high_floor_imbalance, 1.0),
    };
    const auto beyond_deadband =
        naturalehia::protein_logic::assess_steady_state_xor(dyadic, beyond_tau);
    context.expect(beyond_deadband.threshold_outcome == CriterionOutcome::Pass &&
        beyond_deadband.separation_outcome == CriterionOutcome::Pass &&
        beyond_deadband.intended_off_activity_outcome == CriterionOutcome::Pass &&
        beyond_deadband.single_high_floor_balance_outcome == CriterionOutcome::Pass,
        "one representable dyadic step beyond tau passes every operational criterion");
}

void test_balance_scope_and_envelope(TestContext& context) {
    const GlobalAssessment original = assessment_from_intervals(
        {{0.1, 0.61, 0.73, 0.2}}, {{0.2, 0.82, 0.88, 0.3}});
    GlobalAssessment swapped = original;
    std::swap(swapped.regions[1], swapped.regions[2]);
    swapped.regions[1].region = XorRegion::HighLow;
    swapped.regions[1].expected_level = naturalehia::protein_logic::LogicalLevel::On;
    swapped.regions[2].region = XorRegion::LowHigh;
    swapped.regions[2].expected_level = naturalehia::protein_logic::LogicalLevel::On;

    const auto original_metrics =
        naturalehia::protein_logic::compute_steady_state_xor_metrics(original);
    const auto swapped_metrics =
        naturalehia::protein_logic::compute_steady_state_xor_metrics(swapped);
    context.expect_near(original_metrics.single_high_floor_imbalance,
        swapped_metrics.single_high_floor_imbalance, 0.0,
        "A/B exchange preserves absolute floor imbalance");
    context.expect_near(original_metrics.single_high_response_gap_upper_bound,
        swapped_metrics.single_high_response_gap_upper_bound, 0.0,
        "A/B exchange preserves the envelope-gap bound");

    const auto narrow = naturalehia::protein_logic::compute_steady_state_xor_metrics(
        assessment_from_intervals({{0.1, 0.60, 0.62, 0.1}},
        {{0.1, 0.70, 0.72, 0.1}}));
    const auto wide = naturalehia::protein_logic::compute_steady_state_xor_metrics(
        assessment_from_intervals({{0.1, 0.50, 0.52, 0.1}},
        {{0.1, 0.80, 0.82, 0.1}}));
    context.expect(wide.single_high_response_gap_upper_bound >=
        narrow.single_high_response_gap_upper_bound,
        "widening response envelopes cannot improve their mathematical gap bound");
}

void test_invalid_records_and_criteria(TestContext& context) {
    GlobalAssessment invalid = nominal_assessment();
    invalid.regions[0].region = XorRegion::HighLow;
    context.expect_throws<std::invalid_argument>(
        [&invalid] {
            (void)naturalehia::protein_logic::compute_steady_state_xor_metrics(invalid);
        },
        "noncanonical region order is rejected");

    invalid = nominal_assessment();
    invalid.regions[1].minimum.active_probability = 1.1;
    context.expect_throws<std::invalid_argument>(
        [&invalid] {
            (void)naturalehia::protein_logic::compute_steady_state_xor_metrics(invalid);
        },
        "out-of-range probability records are rejected");

    SteadyStateXorCriteria invalid_criteria{};
    invalid_criteria.threshold = 0.0;
    context.expect_throws<std::invalid_argument>(
        [&invalid_criteria] {
            (void)naturalehia::protein_logic::assess_steady_state_xor(
                nominal_assessment(), invalid_criteria);
        },
        "closed-interval threshold is rejected");

    invalid_criteria = {};
    invalid_criteria.maximum_intended_off_activity =
        std::numeric_limits<double>::quiet_NaN();
    context.expect_throws<std::invalid_argument>(
        [&invalid_criteria] {
            (void)naturalehia::protein_logic::assess_steady_state_xor(
                nominal_assessment(), invalid_criteria);
        },
        "non-finite optional criterion is rejected");
}

} // namespace

int main() {
    TestContext context;
    test_nominal_metrics(context);
    test_missing_and_partial_criteria(context);
    test_complete_acceptance_and_null(context);
    test_metrics_do_not_substitute_for_each_other(context);
    test_strict_decision_buffer(context);
    test_balance_scope_and_envelope(context);
    test_invalid_records_and_criteria(context);

    if (context.failures() != 0) {
        std::cerr << context.failures() << " acceptance-protocol assertion(s) failed\n";
        return 1;
    }

    std::cout << "All steady-state XOR acceptance protocol tests passed\n";
    return 0;
}
