// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/equilibrium_model.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace naturalehia::protein_logic {
namespace {

void validate_concentrations(Concentrations concentrations) {
    if (!std::isfinite(concentrations.input_a) || !std::isfinite(concentrations.input_b) ||
        concentrations.input_a < 0.0 || concentrations.input_b < 0.0) {
        throw std::invalid_argument("concentrations must be finite and non-negative");
    }
}

void validate_macrostate(const MacrostateParameters& macrostate) {
    if (!std::isfinite(macrostate.dissociation_a) || macrostate.dissociation_a <= 0.0 ||
        !std::isfinite(macrostate.dissociation_b) || macrostate.dissociation_b <= 0.0 ||
        !std::isfinite(macrostate.omega) || macrostate.omega <= 0.0) {
        throw std::invalid_argument(
                  "dissociation constants and omega must be positive");
    }
}

void validate_parameters(const EquilibriumParameters& parameters) {
    if (!std::isfinite(parameters.apo_log_on_off)) {
        throw std::invalid_argument("apo log ON/OFF ratio must be finite");
    }
    validate_macrostate(parameters.off);
    validate_macrostate(parameters.on);
}

void validate_interval(ClosedInterval interval) {
    if (!std::isfinite(interval.lower) || !std::isfinite(interval.upper) ||
        interval.lower > interval.upper) {
        throw std::invalid_argument("closed interval endpoints must be finite and ordered");
    }
}

void validate_concentration_interval(ClosedInterval interval) {
    validate_interval(interval);
    if (interval.lower < 0.0) {
        throw std::invalid_argument("concentration intervals must be non-negative");
    }
}

void validate_rectangle(const ConcentrationRectangle& rectangle) {
    validate_concentration_interval(rectangle.input_a);
    validate_concentration_interval(rectangle.input_b);
}

void validate_windows(const OperatingWindows& windows) {
    validate_concentration_interval(windows.input_a.low);
    validate_concentration_interval(windows.input_a.high);
    validate_concentration_interval(windows.input_b.low);
    validate_concentration_interval(windows.input_b.high);

    if (windows.input_a.low.upper >= windows.input_a.high.lower ||
        windows.input_b.low.upper >= windows.input_b.high.lower) {
        throw std::invalid_argument(
                  "each low operating window must lie strictly below its high window");
    }
}

void validate_macrostate_box(const MacrostateParameterBox& box) {
    validate_interval(box.dissociation_a);
    validate_interval(box.dissociation_b);
    validate_interval(box.omega);
    if (box.dissociation_a.lower <= 0.0 || box.dissociation_b.lower <= 0.0 ||
        box.omega.lower <= 0.0) {
        throw std::invalid_argument(
                  "boxed dissociation constants and omega must be positive");
    }
}

void validate_parameter_box(const EquilibriumParameterBox& box) {
    validate_interval(box.apo_log_on_off);
    validate_macrostate_box(box.off);
    validate_macrostate_box(box.on);
}

void validate_assessment_controls(double threshold, double required_margin) {
    if (!std::isfinite(threshold) || threshold <= 0.0 || threshold >= 1.0) {
        throw std::invalid_argument("threshold must be finite and strictly between 0 and 1");
    }
    if (!std::isfinite(required_margin) || required_margin < 0.0 || required_margin > 1.0) {
        throw std::invalid_argument("required margin must be finite and within [0, 1]");
    }
}

void validate_region(XorRegion region) {
    switch (region) {
    case XorRegion::LowLow:
    case XorRegion::HighLow:
    case XorRegion::LowHigh:
    case XorRegion::HighHigh:
        return;
    }
    throw std::invalid_argument("unknown XOR operating region");
}

double probability_from_log_odds(double value) {
    if (!std::isfinite(value)) {
        throw std::overflow_error("equilibrium log odds cannot be represented");
    }

    // Set log(Z_OFF)=0 and log(Z_ON)=value after dividing out Z_OFF, then
    // normalize the two log weights without exponentiating either partition.
    const double maximum = std::max(0.0, value);
    const double log_normalizer = maximum +
        std::log(std::exp(-maximum) + std::exp(value - maximum));
    return std::exp(value - log_normalizer);
}

std::array<Concentrations, 4> rectangle_corners(
    const ConcentrationRectangle& rectangle) {
    return {{
        {rectangle.input_a.lower, rectangle.input_b.lower},
        {rectangle.input_a.upper, rectangle.input_b.lower},
        {rectangle.input_a.lower, rectangle.input_b.upper},
        {rectangle.input_a.upper, rectangle.input_b.upper},
    }};
}

ProbabilityWitness minimum_witness(const EquilibriumParameters& parameters,
    const ConcentrationRectangle& rectangle) {
    const auto corners = rectangle_corners(rectangle);
    ProbabilityWitness result{
        corners.front(), parameters, equilibrium_active_probability(parameters, corners.front())};
    for (std::size_t index = 1; index < corners.size(); ++index) {
        const double probability = equilibrium_active_probability(parameters, corners[index]);
        if (probability < result.active_probability) {
            result = {corners[index], parameters, probability};
        }
    }
    return result;
}

ProbabilityWitness maximum_witness(const EquilibriumParameters& parameters,
    const ConcentrationRectangle& rectangle) {
    const auto corners = rectangle_corners(rectangle);
    ProbabilityWitness result{
        corners.front(), parameters, equilibrium_active_probability(parameters, corners.front())};
    for (std::size_t index = 1; index < corners.size(); ++index) {
        const double probability = equilibrium_active_probability(parameters, corners[index]);
        if (probability > result.active_probability) {
            result = {corners[index], parameters, probability};
        }
    }
    return result;
}

EquilibriumParameters lower_probability_corner(const EquilibriumParameterBox& box) {
    return {
        box.apo_log_on_off.lower,
        {box.off.dissociation_a.lower, box.off.dissociation_b.lower, box.off.omega.upper},
        {box.on.dissociation_a.upper, box.on.dissociation_b.upper, box.on.omega.lower},
    };
}

EquilibriumParameters upper_probability_corner(const EquilibriumParameterBox& box) {
    return {
        box.apo_log_on_off.upper,
        {box.off.dissociation_a.upper, box.off.dissociation_b.upper, box.off.omega.lower},
        {box.on.dissociation_a.lower, box.on.dissociation_b.lower, box.on.omega.upper},
    };
}

template<typename RegionBounder>
GlobalAssessment assess_global(const OperatingWindows& windows, double threshold,
    double required_margin, RegionBounder&& bounder) {
    GlobalAssessment result{};
    result.threshold = threshold;
    result.required_margin = required_margin;

    for (const XorRegion region : kCanonicalXorRegionOrder) {
        const std::size_t index = static_cast<std::size_t>(region);
        result.regions[index] = bounder(concentration_rectangle(windows, region), region);
    }

    result.on_floor = std::min(result.regions[1].minimum.active_probability,
            result.regions[2].minimum.active_probability);
    result.off_ceiling = std::max(result.regions[0].maximum.active_probability,
            result.regions[3].maximum.active_probability);
    result.separation_margin = result.on_floor - result.off_ceiling;
    result.threshold_margin =
        std::min(result.on_floor - threshold, threshold - result.off_ceiling);

    // The research convention is deliberately strict on both sides. The fixed
    // binary64 deadband is operational and is not a certified error bound.
    result.passes_threshold = result.threshold_margin > result.decision_tolerance;
    result.passes_required_margin =
        result.separation_margin - required_margin > result.decision_tolerance;
    return result;
}

void validate_surface_axis(std::span<const double> axis) {
    if (axis.empty()) {
        throw std::invalid_argument("surface axes must not be empty");
    }
    for (std::size_t index = 0; index < axis.size(); ++index) {
        if (!std::isfinite(axis[index]) || axis[index] < 0.0) {
            throw std::invalid_argument("surface axes must contain finite concentrations");
        }
        if (index != 0 && axis[index] < axis[index - 1]) {
            throw std::invalid_argument("surface axes must be in non-decreasing order");
        }
    }
}

} // namespace

double log_binding_polynomial(
    const MacrostateParameters& macrostate, Concentrations concentrations) {
    validate_macrostate(macrostate);
    validate_concentrations(concentrations);

    const double negative_infinity = -std::numeric_limits<double>::infinity();
    const double log_a = concentrations.input_a == 0.0
        ? negative_infinity
        : std::log(concentrations.input_a);
    const double log_b = concentrations.input_b == 0.0
        ? negative_infinity
        : std::log(concentrations.input_b);
    const double log_omega = std::log(macrostate.omega);
    const double log_ka = std::log(macrostate.dissociation_a);
    const double log_kb = std::log(macrostate.dissociation_b);

    const std::array<double, 4> log_terms{{
        0.0,
        log_a - log_ka,
        log_b - log_kb,
        log_omega + (log_a - log_ka) + (log_b - log_kb),
    }};
    const double maximum = *std::max_element(log_terms.begin(), log_terms.end());
    double scaled_sum = 0.0;
    for (const double term : log_terms) {
        if (term != negative_infinity) {
            scaled_sum += std::exp(term - maximum);
        }
    }
    return maximum + std::log(scaled_sum);
}

double equilibrium_log_odds(
    const EquilibriumParameters& parameters, Concentrations concentrations) {
    validate_parameters(parameters);
    validate_concentrations(concentrations);

    const double result = parameters.apo_log_on_off +
        log_binding_polynomial(parameters.on, concentrations) -
        log_binding_polynomial(parameters.off, concentrations);
    if (!std::isfinite(result)) {
        throw std::overflow_error("equilibrium log odds cannot be represented");
    }
    return result;
}

double equilibrium_active_probability(
    const EquilibriumParameters& parameters, Concentrations concentrations) {
    return probability_from_log_odds(equilibrium_log_odds(parameters, concentrations));
}

LogicalLevel expected_xor_level(XorRegion region) {
    validate_region(region);
    return region == XorRegion::HighLow || region == XorRegion::LowHigh ? LogicalLevel::On
                                                                        : LogicalLevel::Off;
}

ConcentrationRectangle concentration_rectangle(
    const OperatingWindows& windows, XorRegion region) {
    validate_windows(windows);
    switch (region) {
    case XorRegion::LowLow:
        return {windows.input_a.low, windows.input_b.low};
    case XorRegion::HighLow:
        return {windows.input_a.high, windows.input_b.low};
    case XorRegion::LowHigh:
        return {windows.input_a.low, windows.input_b.high};
    case XorRegion::HighHigh:
        return {windows.input_a.high, windows.input_b.high};
    }
    throw std::invalid_argument("unknown XOR operating region");
}

RegionBounds bound_region(const EquilibriumParameters& parameters,
    const ConcentrationRectangle& concentrations, XorRegion region) {
    validate_parameters(parameters);
    validate_rectangle(concentrations);
    validate_region(region);
    return {
        region,
        expected_xor_level(region),
        concentrations,
        minimum_witness(parameters, concentrations),
        maximum_witness(parameters, concentrations),
    };
}

RegionBounds bound_region(const EquilibriumParameterBox& parameter_box,
    const ConcentrationRectangle& concentrations, XorRegion region) {
    validate_parameter_box(parameter_box);
    validate_rectangle(concentrations);
    validate_region(region);

    const EquilibriumParameters lower = lower_probability_corner(parameter_box);
    const EquilibriumParameters upper = upper_probability_corner(parameter_box);
    return {
        region,
        expected_xor_level(region),
        concentrations,
        minimum_witness(lower, concentrations),
        maximum_witness(upper, concentrations),
    };
}

NominalAssessment assess_nominal(const EquilibriumParameters& parameters,
    const OperatingWindows& windows, double threshold, double required_margin) {
    validate_parameters(parameters);
    validate_windows(windows);
    validate_assessment_controls(threshold, required_margin);

    NominalAssessment result{};
    result.parameters = parameters;
    result.windows = windows;
    result.global = assess_global(windows, threshold, required_margin,
            [&parameters](const ConcentrationRectangle& rectangle, XorRegion region) {
            return bound_region(parameters, rectangle, region);
        });
    return result;
}

ParameterBoxAssessment assess_parameter_box(const EquilibriumParameterBox& parameter_box,
    const OperatingWindows& windows, double threshold, double required_margin) {
    validate_parameter_box(parameter_box);
    validate_windows(windows);
    validate_assessment_controls(threshold, required_margin);

    ParameterBoxAssessment result{};
    result.parameter_box = parameter_box;
    result.windows = windows;
    result.lower_probability_corner = lower_probability_corner(parameter_box);
    result.upper_probability_corner = upper_probability_corner(parameter_box);
    result.global = assess_global(windows, threshold, required_margin,
            [&parameter_box](const ConcentrationRectangle& rectangle, XorRegion region) {
            return bound_region(parameter_box, rectangle, region);
        });
    return result;
}

std::vector<double> make_concentration_axis(
    ClosedInterval bounds, std::size_t count, AxisSpacing spacing) {
    validate_concentration_interval(bounds);
    if (count == 0) {
        throw std::invalid_argument("axis count must be nonzero");
    }
    if (spacing != AxisSpacing::Linear && spacing != AxisSpacing::Logarithmic) {
        throw std::invalid_argument("unknown axis spacing");
    }
    if (spacing == AxisSpacing::Logarithmic && bounds.lower <= 0.0) {
        throw std::invalid_argument("logarithmic concentration axes must be strictly positive");
    }

    std::vector<double> axis;
    if (count > axis.max_size()) {
        throw std::overflow_error("axis count exceeds vector capacity");
    }
    axis.resize(count);
    if (count == 1) {
        axis.front() = bounds.lower;
        return axis;
    }

    const double denominator = static_cast<double>(count - 1);
    const double log_lower = spacing == AxisSpacing::Logarithmic ? std::log(bounds.lower) : 0.0;
    const double log_upper = spacing == AxisSpacing::Logarithmic ? std::log(bounds.upper) : 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const double fraction = static_cast<double>(index) / denominator;
        if (spacing == AxisSpacing::Linear) {
            axis[index] = (1.0 - fraction) * bounds.lower + fraction * bounds.upper;
        } else {
            axis[index] = std::exp((1.0 - fraction) * log_lower + fraction * log_upper);
        }
    }
    axis.front() = bounds.lower;
    axis.back() = bounds.upper;
    return axis;
}

std::size_t checked_surface_size(
    std::size_t input_a_count, std::size_t input_b_count) {
    if (input_a_count == 0 || input_b_count == 0) {
        throw std::invalid_argument("surface dimensions must be nonzero");
    }
    if (input_a_count > std::numeric_limits<std::size_t>::max() / input_b_count) {
        throw std::overflow_error("surface dimensions overflow size_t");
    }
    return input_a_count * input_b_count;
}

std::size_t surface_index(std::size_t input_a_index, std::size_t input_b_index,
    std::size_t input_a_count, std::size_t input_b_count) {
    (void)checked_surface_size(input_a_count, input_b_count);
    if (input_a_index >= input_a_count || input_b_index >= input_b_count) {
        throw std::out_of_range("surface index is outside the declared dimensions");
    }
    return input_b_index * input_a_count + input_a_index;
}

double ProbabilitySurface::at(
    std::size_t input_a_index, std::size_t input_b_index) const {
    const std::size_t expected_size =
        checked_surface_size(input_a_axis.size(), input_b_axis.size());
    if (active_probability.size() != expected_size) {
        throw std::logic_error("probability surface storage is inconsistent with its axes");
    }
    return active_probability[surface_index(input_a_index, input_b_index,
               input_a_axis.size(), input_b_axis.size())];
}

ProbabilitySurface sample_surface(const EquilibriumParameters& parameters,
    std::span<const double> input_a_axis, std::span<const double> input_b_axis) {
    validate_parameters(parameters);
    validate_surface_axis(input_a_axis);
    validate_surface_axis(input_b_axis);
    const std::size_t sample_count =
        checked_surface_size(input_a_axis.size(), input_b_axis.size());

    ProbabilitySurface result{};
    if (sample_count > result.active_probability.max_size()) {
        throw std::overflow_error("surface sample count exceeds vector capacity");
    }
    result.input_a_axis.assign(input_a_axis.begin(), input_a_axis.end());
    result.input_b_axis.assign(input_b_axis.begin(), input_b_axis.end());
    result.active_probability.reserve(sample_count);

    for (const double input_b : input_b_axis) {
        for (const double input_a : input_a_axis) {
            result.active_probability.push_back(
                equilibrium_active_probability(parameters, {input_a, input_b}));
        }
    }
    return result;
}

ProbabilitySurface sample_surface(const EquilibriumParameters& parameters,
    ClosedInterval input_a_bounds, std::size_t input_a_count, AxisSpacing input_a_spacing,
    ClosedInterval input_b_bounds, std::size_t input_b_count, AxisSpacing input_b_spacing) {
    const std::vector<double> input_a_axis =
        make_concentration_axis(input_a_bounds, input_a_count, input_a_spacing);
    const std::vector<double> input_b_axis =
        make_concentration_axis(input_b_bounds, input_b_count, input_b_spacing);
    return sample_surface(parameters, input_a_axis, input_b_axis);
}

} // namespace naturalehia::protein_logic
