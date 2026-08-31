// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/protein_logic/model.hpp>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace naturalehia::protein_logic {

// Fixed operational deadband applied to already-rounded binary64 results. It
// is not experimental uncertainty or a bound on accumulated floating-point
// error. A pass under this convention is not a certified real-arithmetic
// inequality.
inline constexpr double kAssessmentDecisionTolerance =
    64.0 * std::numeric_limits<double>::epsilon();

// This module assumes thermodynamic equilibrium, two output-relevant
// macrostates, one effective site per input, and undepleted free inputs. It is
// a mechanism-level mathematical hypothesis, not an inverse-folding model or
// evidence that a sequence, fold, kinetic gate, or safe biological system exists.

// Concentrations and dissociation constants must use the same units. The model
// only uses their ratios and therefore does not prescribe a laboratory unit.
struct Concentrations {
    double input_a{0.0};
    double input_b{0.0};
};

// Parameters for one OFF or ON conformational macrostate. All three fields are
// strictly positive. omega is dimensionless; omega=1 is independent binding.
struct MacrostateParameters {
    double dissociation_a{1.0};
    double dissociation_b{1.0};
    double omega{1.0};
};

// apo_log_on_off is log(Z_ON / Z_OFF) in the absence of either input.
struct EquilibriumParameters {
    double apo_log_on_off{0.0};
    MacrostateParameters off{};
    MacrostateParameters on{};
};

struct ClosedInterval {
    double lower{0.0};
    double upper{0.0};
};

struct MacrostateParameterBox {
    ClosedInterval dissociation_a{1.0, 1.0};
    ClosedInterval dissociation_b{1.0, 1.0};
    ClosedInterval omega{1.0, 1.0};
};

// Every interval in this box is varied independently. Correlated uncertainty
// requires a different representation and must not be inferred from this API.
struct EquilibriumParameterBox {
    ClosedInterval apo_log_on_off{0.0, 0.0};
    MacrostateParameterBox off{};
    MacrostateParameterBox on{};
};

struct ConcentrationRectangle {
    ClosedInterval input_a{};
    ClosedInterval input_b{};
};

struct InputOperatingWindows {
    ClosedInterval low{};
    ClosedInterval high{};
};

struct OperatingWindows {
    InputOperatingWindows input_a{};
    InputOperatingWindows input_b{};
};

// The numeric values preserve the existing A-fastest canonical XOR ordering:
// low/low, high/low, low/high, high/high.
enum class XorRegion : unsigned char {
    LowLow = 0,
    HighLow = 1,
    LowHigh = 2,
    HighHigh = 3,
};

inline constexpr std::array<XorRegion, 4> kCanonicalXorRegionOrder{{
    XorRegion::LowLow,
    XorRegion::HighLow,
    XorRegion::LowHigh,
    XorRegion::HighHigh,
}};

[[nodiscard]] LogicalLevel expected_xor_level(XorRegion region);

struct ProbabilityWitness {
    Concentrations concentrations{};
    EquilibriumParameters parameters{};
    double active_probability{0.0};
};

// The real-arithmetic extrema are exact for this two-state bilinear
// binding-polynomial model: each witness is selected from the four
// concentration corners, not a grid. Stored values are binary64 approximations.
struct RegionBounds {
    XorRegion region{XorRegion::LowLow};
    LogicalLevel expected_level{LogicalLevel::Off};
    ConcentrationRectangle concentrations{};
    ProbabilityWitness minimum{};
    ProbabilityWitness maximum{};
};

struct GlobalAssessment {
    std::array<RegionBounds, 4> regions{};
    double on_floor{0.0};
    double off_ceiling{0.0};
    double separation_margin{0.0};
    double threshold{0.5};
    double threshold_margin{0.0};
    double required_margin{0.0};
    double decision_tolerance{kAssessmentDecisionTolerance};
    // A pass requires its computed clearance to exceed decision_tolerance.
    // Values inside the buffer, including small positive values, are ties.
    bool passes_threshold{false};
    bool passes_required_margin{false};
};

struct NominalAssessment {
    EquilibriumParameters parameters{};
    OperatingWindows windows{};
    GlobalAssessment global{};
};

struct ParameterBoxAssessment {
    EquilibriumParameterBox parameter_box{};
    OperatingWindows windows{};
    // These analytic corners minimize/maximize P(ON) pointwise for every
    // non-negative concentration pair under independent interval uncertainty.
    EquilibriumParameters lower_probability_corner{};
    EquilibriumParameters upper_probability_corner{};
    GlobalAssessment global{};
};

enum class AxisSpacing : unsigned char {
    Linear,
    Logarithmic,
};

// active_probability uses index = b_index * input_a_axis.size() + a_index, so
// input A varies fastest. Surfaces are inspection aids, never proof of bounds.
struct ProbabilitySurface {
    std::vector<double> input_a_axis{};
    std::vector<double> input_b_axis{};
    std::vector<double> active_probability{};

    [[nodiscard]] double at(std::size_t input_a_index, std::size_t input_b_index) const;
};

// Computes log(Q_s), where
// Q_s = 1 + a/Ka_s + b/Kb_s + omega_s*a*b/(Ka_s*Kb_s).
[[nodiscard]] double log_binding_polynomial(
    const MacrostateParameters& macrostate, Concentrations concentrations);

// Computes log(Z_ON/Z_OFF) = apo_log_on_off + log(Q_ON) - log(Q_OFF).
[[nodiscard]] double equilibrium_log_odds(
    const EquilibriumParameters& parameters, Concentrations concentrations);

// Uses two-term log-sum-exp normalization without exponentiating either raw
// partition. An unrepresentable intermediate log odds is reported as overflow.
[[nodiscard]] double equilibrium_active_probability(
    const EquilibriumParameters& parameters, Concentrations concentrations);

[[nodiscard]] ConcentrationRectangle concentration_rectangle(
    const OperatingWindows& windows, XorRegion region);

[[nodiscard]] RegionBounds bound_region(const EquilibriumParameters& parameters,
    const ConcentrationRectangle& concentrations, XorRegion region);

[[nodiscard]] RegionBounds bound_region(const EquilibriumParameterBox& parameter_box,
    const ConcentrationRectangle& concentrations, XorRegion region);

// Threshold is in (0,1). required_margin is a probability separation in [0,1].
// Both pass flags are strict: equality at the threshold or required margin
// fails, matching the project's strict evidence convention.
// These assessments characterize this explicit equilibrium model only; they do
// not establish foldability, kinetics, experimental function, or biosafety.
[[nodiscard]] NominalAssessment assess_nominal(const EquilibriumParameters& parameters,
    const OperatingWindows& windows, double threshold = 0.5, double required_margin = 0.0);

[[nodiscard]] ParameterBoxAssessment assess_parameter_box(
    const EquilibriumParameterBox& parameter_box, const OperatingWindows& windows,
    double threshold = 0.5, double required_margin = 0.0);

// count must be nonzero. With count==1 the returned point is bounds.lower.
// Logarithmic spacing additionally requires a strictly positive lower bound.
[[nodiscard]] std::vector<double> make_concentration_axis(
    ClosedInterval bounds, std::size_t count, AxisSpacing spacing = AxisSpacing::Linear);

// Public checked helpers make the A-fastest layout and size overflow contract
// available to callers that store compatible auxiliary arrays.
[[nodiscard]] std::size_t checked_surface_size(
    std::size_t input_a_count, std::size_t input_b_count);

[[nodiscard]] std::size_t surface_index(std::size_t input_a_index,
    std::size_t input_b_index, std::size_t input_a_count, std::size_t input_b_count);

[[nodiscard]] ProbabilitySurface sample_surface(const EquilibriumParameters& parameters,
    std::span<const double> input_a_axis, std::span<const double> input_b_axis);

[[nodiscard]] ProbabilitySurface sample_surface(const EquilibriumParameters& parameters,
    ClosedInterval input_a_bounds, std::size_t input_a_count, AxisSpacing input_a_spacing,
    ClosedInterval input_b_bounds, std::size_t input_b_count, AxisSpacing input_b_spacing);

} // namespace naturalehia::protein_logic
