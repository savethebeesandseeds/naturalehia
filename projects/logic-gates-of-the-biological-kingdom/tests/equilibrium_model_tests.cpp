// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/equilibrium_model.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

using naturalehia::protein_logic::AxisSpacing;
using naturalehia::protein_logic::ClosedInterval;
using naturalehia::protein_logic::ConcentrationRectangle;
using naturalehia::protein_logic::Concentrations;
using naturalehia::protein_logic::EquilibriumParameterBox;
using naturalehia::protein_logic::EquilibriumParameters;
using naturalehia::protein_logic::MacrostateParameterBox;
using naturalehia::protein_logic::MacrostateParameters;
using naturalehia::protein_logic::OperatingWindows;
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
            // A different exception does not satisfy the public error contract.
        }
        expect(caught, message);
    }

    [[nodiscard]] int failures() const noexcept {
        return failures_;
    }

private:
    int failures_{0};
};

EquilibriumParameters nominal_fixture() {
    return {
        -4.0,
        {1.0, 1.0, 10.0},
        {0.01, 0.01, 0.001},
    };
}

OperatingWindows operating_windows_fixture() {
    return {
        {{0.0, 0.01}, {3.0, 10.0}},
        {{0.0, 0.01}, {3.0, 10.0}},
    };
}

MacrostateParameterBox relative_box(const MacrostateParameters& parameters, double fraction) {
    return {
        {parameters.dissociation_a * (1.0 - fraction),
         parameters.dissociation_a * (1.0 + fraction)},
        {parameters.dissociation_b * (1.0 - fraction),
         parameters.dissociation_b * (1.0 + fraction)},
        {parameters.omega * (1.0 - fraction), parameters.omega * (1.0 + fraction)},
    };
}

EquilibriumParameterBox stress_box_fixture(double positive_fraction = 0.05,
    double apo_radius = 0.05) {
    const EquilibriumParameters nominal = nominal_fixture();
    return {
        {nominal.apo_log_on_off - apo_radius, nominal.apo_log_on_off + apo_radius},
        relative_box(nominal.off, positive_fraction),
        relative_box(nominal.on, positive_fraction),
    };
}

double select_endpoint(const ClosedInterval& interval, std::size_t mask, unsigned bit) {
    return (mask & (std::size_t{1} << bit)) == 0U ? interval.lower : interval.upper;
}

EquilibriumParameters parameter_vertex(
    const EquilibriumParameterBox& parameter_box, std::size_t mask) {
    return {
        select_endpoint(parameter_box.apo_log_on_off, mask, 0),
        {
            select_endpoint(parameter_box.off.dissociation_a, mask, 1),
            select_endpoint(parameter_box.off.dissociation_b, mask, 2),
            select_endpoint(parameter_box.off.omega, mask, 3),
        },
        {
            select_endpoint(parameter_box.on.dissociation_a, mask, 4),
            select_endpoint(parameter_box.on.dissociation_b, mask, 5),
            select_endpoint(parameter_box.on.omega, mask, 6),
        },
    };
}

void test_nominal_regression_fixture(TestContext& context) {
    const auto assessment = naturalehia::protein_logic::assess_nominal(
        nominal_fixture(), operating_windows_fixture(), 0.5, 0.0);

    context.expect_near(assessment.global.on_floor, 0.562295, 1.0e-6,
        "nominal ON floor matches the published illustrative fixture");
    context.expect_near(assessment.global.off_ceiling, 0.115416, 1.0e-6,
        "nominal OFF ceiling matches the published illustrative fixture");
    context.expect_near(assessment.global.separation_margin, 0.446879, 1.0e-6,
        "nominal separation matches the published illustrative fixture");
    context.expect_near(assessment.global.threshold_margin, 0.062295, 1.0e-6,
        "nominal threshold margin matches the published illustrative fixture");
    context.expect(assessment.global.passes_threshold,
        "nominal fixture strictly clears its declared threshold");
    context.expect(assessment.global.passes_required_margin,
        "nominal fixture has a positive separation when required margin is zero");

    const std::array<XorRegion, 4> expected_order{{
        XorRegion::LowLow,
        XorRegion::HighLow,
        XorRegion::LowHigh,
        XorRegion::HighHigh,
    }};
    for (std::size_t index = 0; index < expected_order.size(); ++index) {
        context.expect(assessment.global.regions[index].region == expected_order[index],
            "regions retain deterministic A-fastest XOR ordering");
    }
}

void test_independent_binding_null_fails(TestContext& context) {
    EquilibriumParameters independent = nominal_fixture();
    independent.off.omega = 1.0;
    independent.on.omega = 1.0;
    const auto assessment = naturalehia::protein_logic::assess_nominal(
        independent, operating_windows_fixture(), 0.5, 0.0);

    context.expect(!assessment.global.passes_threshold,
        "independent binding cannot preserve this XOR operating region");
    context.expect(assessment.global.separation_margin < 0.0,
        "independent null has overlapping ON and OFF probability ranges");
}

void test_parameter_box_regression_fixture(TestContext& context) {
    const auto assessment = naturalehia::protein_logic::assess_parameter_box(
        stress_box_fixture(), operating_windows_fixture(), 0.5, 0.0);

    context.expect_near(assessment.global.on_floor, 0.526164, 1.0e-6,
        "robust ON floor matches the independent five-percent stress box");
    context.expect_near(assessment.global.off_ceiling, 0.144181, 1.0e-6,
        "robust OFF ceiling matches the independent five-percent stress box");
    context.expect_near(assessment.global.separation_margin, 0.381983, 1.0e-6,
        "robust separation matches the independent five-percent stress box");
    context.expect_near(assessment.global.threshold_margin, 0.026164, 1.0e-6,
        "robust threshold margin matches the independent five-percent stress box");
    context.expect(assessment.global.passes_threshold,
        "the full illustrative stress box strictly clears threshold");
    context.expect(assessment.global.passes_required_margin,
        "the full illustrative stress box has positive separation");

    context.expect_near(assessment.lower_probability_corner.apo_log_on_off, -4.05, 0.0,
        "lower-probability corner uses lower apo log ratio");
    context.expect_near(assessment.lower_probability_corner.off.dissociation_a, 0.95, 1.0e-15,
        "lower-probability corner uses stronger OFF binding");
    context.expect_near(assessment.lower_probability_corner.on.dissociation_a, 0.0105, 1.0e-15,
        "lower-probability corner uses weaker ON binding");
    context.expect_near(assessment.upper_probability_corner.off.omega, 9.5, 1.0e-14,
        "upper-probability corner uses lower OFF joint coupling");
    context.expect_near(assessment.upper_probability_corner.on.omega, 0.00105, 1.0e-15,
        "upper-probability corner uses higher ON joint coupling");
}

EquilibriumParameters swap_inputs(EquilibriumParameters parameters) {
    std::swap(parameters.off.dissociation_a, parameters.off.dissociation_b);
    std::swap(parameters.on.dissociation_a, parameters.on.dissociation_b);
    return parameters;
}

void test_symmetry_scaling_and_zero_input(TestContext& context) {
    EquilibriumParameters asymmetric = nominal_fixture();
    asymmetric.off.dissociation_a = 0.7;
    asymmetric.off.dissociation_b = 1.3;
    asymmetric.on.dissociation_a = 0.008;
    asymmetric.on.dissociation_b = 0.014;
    const Concentrations inputs{0.17, 4.2};
    const double original =
        naturalehia::protein_logic::equilibrium_active_probability(asymmetric, inputs);
    const double swapped = naturalehia::protein_logic::equilibrium_active_probability(
        swap_inputs(asymmetric), {inputs.input_b, inputs.input_a});
    context.expect_near(swapped, original, 2.0e-15,
        "simultaneously swapping A and B leaves probability unchanged");

    constexpr double scale = 37.0;
    EquilibriumParameters scaled = asymmetric;
    scaled.off.dissociation_a *= scale;
    scaled.off.dissociation_b *= scale;
    scaled.on.dissociation_a *= scale;
    scaled.on.dissociation_b *= scale;
    const double scaled_probability = naturalehia::protein_logic::equilibrium_active_probability(
        scaled, {inputs.input_a * scale, inputs.input_b * scale});
    context.expect_near(scaled_probability, original, 2.0e-15,
        "common concentration and dissociation scaling is invariant");

    const double zero = naturalehia::protein_logic::equilibrium_active_probability(
        asymmetric, {0.0, 0.0});
    const double expected_zero = 1.0 / (1.0 + std::exp(4.0));
    context.expect_near(zero, expected_zero, 1.0e-15,
        "zero-input probability is the apo logistic probability");
}

void test_corner_bounds_contain_dense_samples(TestContext& context) {
    const EquilibriumParameters parameters = nominal_fixture();
    const OperatingWindows windows = operating_windows_fixture();
    const auto assessment = naturalehia::protein_logic::assess_nominal(parameters, windows);

    for (const auto& region : assessment.global.regions) {
        constexpr std::size_t samples = 41;
        for (std::size_t b_index = 0; b_index < samples; ++b_index) {
            const double b_fraction =
                static_cast<double>(b_index) / static_cast<double>(samples - 1);
            const double input_b = (1.0 - b_fraction) * region.concentrations.input_b.lower +
                b_fraction * region.concentrations.input_b.upper;
            for (std::size_t a_index = 0; a_index < samples; ++a_index) {
                const double a_fraction =
                    static_cast<double>(a_index) / static_cast<double>(samples - 1);
                const double input_a =
                    (1.0 - a_fraction) * region.concentrations.input_a.lower +
                    a_fraction * region.concentrations.input_a.upper;
                const double probability =
                    naturalehia::protein_logic::equilibrium_active_probability(
                    parameters, {input_a, input_b});
                context.expect(probability >= region.minimum.active_probability - 2.0e-15,
                    "analytic corner lower bound contains dense interior samples");
                context.expect(probability <= region.maximum.active_probability + 2.0e-15,
                    "analytic corner upper bound contains dense interior samples");
            }
        }
    }
}

void test_asymmetric_parameter_box_bounds_contain_vertices(TestContext& context) {
    const EquilibriumParameterBox parameter_box{
        {-3.2, -3.0},
        {{0.7, 0.9}, {1.3, 1.6}, {3.0, 4.0}},
        {{0.02, 0.025}, {0.04, 0.05}, {0.1, 0.2}},
    };
    const OperatingWindows windows{
        {{0.01, 0.12}, {1.7, 6.2}},
        {{0.0, 0.07}, {2.4, 9.1}},
    };
    const auto assessment =
        naturalehia::protein_logic::assess_parameter_box(parameter_box, windows);

    context.expect(std::abs(assessment.global.regions[1].minimum.active_probability -
        assessment.global.regions[2].minimum.active_probability) >
        1.0e-4,
        "asymmetric inputs exercise distinct 10 and 01 regional bounds");

    constexpr std::size_t parameter_vertex_count = std::size_t{1} << 7U;
    constexpr std::size_t concentration_samples = 9;
    for (std::size_t mask = 0; mask < parameter_vertex_count; ++mask) {
        const EquilibriumParameters parameters = parameter_vertex(parameter_box, mask);
        for (const auto& region : assessment.global.regions) {
            for (std::size_t b_index = 0; b_index < concentration_samples; ++b_index) {
                const double b_fraction = static_cast<double>(b_index) /
                    static_cast<double>(concentration_samples - 1);
                const double input_b =
                    (1.0 - b_fraction) * region.concentrations.input_b.lower +
                    b_fraction * region.concentrations.input_b.upper;
                for (std::size_t a_index = 0; a_index < concentration_samples; ++a_index) {
                    const double a_fraction = static_cast<double>(a_index) /
                        static_cast<double>(concentration_samples - 1);
                    const double input_a =
                        (1.0 - a_fraction) * region.concentrations.input_a.lower +
                        a_fraction * region.concentrations.input_a.upper;
                    const double probability =
                        naturalehia::protein_logic::equilibrium_active_probability(
                        parameters, {input_a, input_b});
                    context.expect(
                        probability >= region.minimum.active_probability - 4.0e-15,
                        "robust lower bound contains asymmetric parameter vertices");
                    context.expect(
                        probability <= region.maximum.active_probability + 4.0e-15,
                        "robust upper bound contains asymmetric parameter vertices");
                }
            }
        }
    }
}

void test_widening_stress_box_cannot_improve_robust_margin(TestContext& context) {
    const auto narrow = naturalehia::protein_logic::assess_parameter_box(
        stress_box_fixture(0.02, 0.02), operating_windows_fixture());
    const auto wide = naturalehia::protein_logic::assess_parameter_box(
        stress_box_fixture(0.05, 0.05), operating_windows_fixture());

    context.expect(wide.global.on_floor <= narrow.global.on_floor,
        "widening intervals cannot raise the robust ON floor");
    context.expect(wide.global.off_ceiling >= narrow.global.off_ceiling,
        "widening intervals cannot lower the robust OFF ceiling");
    context.expect(wide.global.separation_margin <= narrow.global.separation_margin,
        "widening intervals cannot improve robust separation");
    context.expect(wide.global.threshold_margin <= narrow.global.threshold_margin,
        "widening intervals cannot improve robust threshold margin");
}

void test_invalid_and_extreme_domains(TestContext& context) {
    const EquilibriumParameters nominal = nominal_fixture();
    context.expect_throws<std::invalid_argument>(
        [&nominal] {
            (void)naturalehia::protein_logic::equilibrium_active_probability(
                nominal, {-0.01, 1.0});
        },
        "negative concentration is rejected");

    EquilibriumParameters invalid = nominal;
    invalid.on.dissociation_a = 0.0;
    context.expect_throws<std::invalid_argument>(
        [&invalid] {
            (void)naturalehia::protein_logic::equilibrium_active_probability(
                invalid, {1.0, 1.0});
        },
        "zero dissociation constant is rejected");

    invalid = nominal;
    invalid.off.omega = 0.0;
    context.expect_throws<std::invalid_argument>(
        [&invalid] {
            (void)naturalehia::protein_logic::equilibrium_active_probability(
                invalid, {1.0, 1.0});
        },
        "zero joint-binding factor is rejected");

    invalid = nominal;
    invalid.apo_log_on_off = std::numeric_limits<double>::quiet_NaN();
    context.expect_throws<std::invalid_argument>(
        [&invalid] {
            (void)naturalehia::protein_logic::equilibrium_active_probability(
                invalid, {1.0, 1.0});
        },
        "non-finite apo ratio is rejected");

    OperatingWindows overlapping = operating_windows_fixture();
    overlapping.input_a.low.upper = overlapping.input_a.high.lower;
    context.expect_throws<std::invalid_argument>(
        [&nominal, &overlapping] {
            (void)naturalehia::protein_logic::assess_nominal(nominal, overlapping);
        },
        "touching low and high windows are rejected");

    context.expect_throws<std::invalid_argument>(
        [&nominal] {
            (void)naturalehia::protein_logic::assess_nominal(
                nominal, operating_windows_fixture(), 1.0);
        },
        "threshold one is rejected");
    context.expect_throws<std::invalid_argument>(
        [&nominal] {
            (void)naturalehia::protein_logic::assess_nominal(
                nominal, operating_windows_fixture(), 0.5, -0.01);
        },
        "negative required margin is rejected");
    context.expect_throws<std::invalid_argument>(
        [&nominal] {
            (void)naturalehia::protein_logic::bound_region(nominal,
            ConcentrationRectangle{{0.0, 1.0}, {0.0, 1.0}},
            static_cast<XorRegion>(255));
        },
        "unknown operating region is rejected");

    EquilibriumParameterBox invalid_box = stress_box_fixture();
    invalid_box.on.omega.lower = 0.0;
    context.expect_throws<std::invalid_argument>(
        [&invalid_box] {
            (void)naturalehia::protein_logic::assess_parameter_box(
                invalid_box, operating_windows_fixture());
        },
        "a parameter box containing zero joint weight is rejected");

    const double smallest = std::numeric_limits<double>::denorm_min();
    const double largest = std::numeric_limits<double>::max();
    const EquilibriumParameters extreme{
        -700.0,
        {smallest, largest, largest},
        {largest, smallest, smallest},
    };
    const double extreme_probability =
        naturalehia::protein_logic::equilibrium_active_probability(
        extreme, {largest, smallest});
    context.expect(std::isfinite(extreme_probability) && extreme_probability >= 0.0 &&
        extreme_probability <= 1.0,
        "log-sum-exp remains finite across representable positive extremes");
}

void test_axes_surface_ordering_and_overflow(TestContext& context) {
    const std::vector<double> linear = naturalehia::protein_logic::make_concentration_axis(
        {0.0, 1.0}, 5, AxisSpacing::Linear);
    context.expect(linear.size() == 5 && linear.front() == 0.0 && linear.back() == 1.0,
        "linear axis contains its exact endpoints");
    context.expect(std::is_sorted(linear.begin(), linear.end()), "linear axis is ordered");
    context.expect_near(linear[2], 0.5, 0.0, "linear axis has deterministic midpoint");

    const std::vector<double> logarithmic =
        naturalehia::protein_logic::make_concentration_axis(
        {0.01, 100.0}, 5, AxisSpacing::Logarithmic);
    context.expect(logarithmic.front() == 0.01 && logarithmic.back() == 100.0,
        "logarithmic axis contains its exact endpoints");
    context.expect(std::is_sorted(logarithmic.begin(), logarithmic.end()),
        "logarithmic axis is ordered");
    context.expect_near(logarithmic[2], 1.0, 1.0e-15,
        "logarithmic axis has the expected geometric midpoint");

    context.expect_throws<std::invalid_argument>(
        [] {
            (void)naturalehia::protein_logic::make_concentration_axis(
                {0.0, 1.0}, 5, AxisSpacing::Logarithmic);
        },
        "zero is rejected for logarithmic spacing");
    context.expect_throws<std::invalid_argument>(
        [] {
            (void)naturalehia::protein_logic::make_concentration_axis(
                {2.0, 1.0}, 5, AxisSpacing::Linear);
        },
        "reversed concentration bounds are rejected");
    context.expect_throws<std::invalid_argument>(
        [] {
            (void)naturalehia::protein_logic::make_concentration_axis(
                {1.0, 1.0}, 1, static_cast<AxisSpacing>(255));
        },
        "unknown spacing is rejected even for a singleton axis");

    const std::array<double, 3> input_a{{0.0, 0.1, 2.0}};
    const std::array<double, 2> input_b{{0.0, 4.0}};
    const auto surface =
        naturalehia::protein_logic::sample_surface(nominal_fixture(), input_a, input_b);
    context.expect(surface.active_probability.size() == 6,
        "surface has the checked Cartesian-product size");
    context.expect_near(surface.active_probability[3],
        naturalehia::protein_logic::equilibrium_active_probability(
            nominal_fixture(), {input_a[0], input_b[1]}),
        0.0, "surface layout advances B only after all A values");
    context.expect_near(surface.at(2, 1),
        naturalehia::protein_logic::equilibrium_active_probability(
            nominal_fixture(), {input_a[2], input_b[1]}),
        0.0, "surface lookup follows the documented A-fastest layout");

    context.expect_throws<std::overflow_error>(
        [] {
            (void)naturalehia::protein_logic::checked_surface_size(
                std::numeric_limits<std::size_t>::max(), 2);
        },
        "surface dimension multiplication overflow is rejected");
    context.expect_throws<std::out_of_range>(
        [] {
            (void)naturalehia::protein_logic::surface_index(3, 0, 3, 2);
        },
        "surface indices are range checked");
}

void test_strict_pass_conventions(TestContext& context) {
    const auto baseline = naturalehia::protein_logic::assess_nominal(
        nominal_fixture(), operating_windows_fixture());
    const auto threshold_tie = naturalehia::protein_logic::assess_nominal(nominal_fixture(),
            operating_windows_fixture(), baseline.global.on_floor, 0.0);
    context.expect(!threshold_tie.global.passes_threshold,
        "equality at the ON threshold fails the strict evidence convention");

    const auto margin_tie = naturalehia::protein_logic::assess_nominal(nominal_fixture(),
            operating_windows_fixture(), 0.5, baseline.global.separation_margin);
    context.expect(!margin_tie.global.passes_required_margin,
        "equality at the requested separation fails the strict evidence convention");

    const double threshold_one_step_below =
        std::nextafter(baseline.global.on_floor, 0.0);
    const auto threshold_near_tie = naturalehia::protein_logic::assess_nominal(
        nominal_fixture(), operating_windows_fixture(), threshold_one_step_below, 0.0);
    context.expect(!threshold_near_tie.global.passes_threshold,
        "a one-ULP apparent threshold clearance remains inside the operational deadband");

    const double separation_one_step_below =
        std::nextafter(baseline.global.separation_margin, 0.0);
    const auto separation_near_tie = naturalehia::protein_logic::assess_nominal(
        nominal_fixture(), operating_windows_fixture(), 0.5, separation_one_step_below);
    context.expect(!separation_near_tie.global.passes_required_margin,
        "a one-ULP apparent separation clearance remains inside the operational deadband");

    const auto resolved_clearance = naturalehia::protein_logic::assess_nominal(
        nominal_fixture(), operating_windows_fixture(),
        baseline.global.on_floor - 1.0e-10,
        baseline.global.separation_margin - 1.0e-10);
    context.expect(resolved_clearance.global.passes_threshold,
        "a threshold clearance larger than the decision buffer can pass");
    context.expect(resolved_clearance.global.passes_required_margin,
        "a separation clearance larger than the decision buffer can pass");
}

} // namespace

int main() {
    TestContext context;
    test_nominal_regression_fixture(context);
    test_independent_binding_null_fails(context);
    test_parameter_box_regression_fixture(context);
    test_symmetry_scaling_and_zero_input(context);
    test_corner_bounds_contain_dense_samples(context);
    test_asymmetric_parameter_box_bounds_contain_vertices(context);
    test_widening_stress_box_cannot_improve_robust_margin(context);
    test_invalid_and_extreme_domains(context);
    test_axes_surface_ordering_and_overflow(context);
    test_strict_pass_conventions(context);

    if (context.failures() != 0) {
        std::cerr << context.failures() << " equilibrium-model assertion(s) failed\n";
        return 1;
    }

    // Passing these tests verifies the documented mathematics and numerical
    // contracts only. It is not evidence for a foldable or functional protein.
    std::cout << "All equilibrium protein logic model tests passed\n";
    return 0;
}
