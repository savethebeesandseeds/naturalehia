// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kProbabilityPolytopeModelVersion{"0.2.0"};

// One atom in the candidate probability set. The central weight is an audit
// checksum: after normalization it must match the corresponding weight in the
// PortfolioConfig supplied to ProbabilityPolytopeProjector.
struct ProbabilityPolytopeScenario {
    std::string scenario_id{};
    double lower_weight{0.0};
    double central_weight{0.0};
    double upper_weight{0.0};
};

// An authoritative, explicitly enumerated event. Membership is never inferred
// from factor tags or project paths. Definition is a bounded human-readable
// description; scenario_ids is the mathematical event taxonomy.
struct ProbabilityEventConstraint {
    std::string event_id{};
    std::string definition{};
    double lower_probability{0.0};
    double upper_probability{1.0};
    std::vector<std::string> scenario_ids{};
};

// Version 0.2 accepts synthetic candidate constraints only. It does not turn
// the declared central measure or bounds into empirical calibration evidence.
struct ProbabilityPolytopeConfig {
    std::string model_version{kProbabilityPolytopeModelVersion};
    std::string scenario_label{
        "unnamed synthetic candidate probability polytope"};
    std::string source_note{
        "Unvalidated synthetic probability constraints for robustness testing"};
    bool synthetic_inputs{true};
    std::vector<ProbabilityPolytopeScenario> scenario_probabilities{};
    std::vector<ProbabilityEventConstraint> events{};
};

// One finite scalar objective coefficient keyed to a declared scenario. Keying
// prevents a caller from silently using a different scenario order.
struct ProbabilityPolytopeScenarioValue {
    std::string scenario_id{};
    double value{0.0};
};

// scenario_weights is a complete probability measure in the associated
// projection's ascending scenario-id order. Residuals audit the published
// double witness, its directly recomputed objective, and the final simplex
// tableau's reduced-cost optimality residual. The latter is not an independent
// dual certificate.
struct ProbabilityPolytopeEndpoint {
    double value{0.0};
    std::vector<double> scenario_weights{};
    double maximum_constraint_violation{0.0};
    double objective_reconciliation_error{0.0};
    double optimality_residual{0.0};
};

struct ProbabilityPolytopeMetricRange {
    ProbabilityPolytopeEndpoint minimum{};
    double central{0.0};
    ProbabilityPolytopeEndpoint maximum{};
};

// Scenarios, events, event members, and endpoint weights use canonical
// ascending identifier order. Endpoint values are order-invariant. A chosen
// optimal witness can be non-unique; deterministic pivoting is only an audit
// convention and does not assert economic or mathematical uniqueness.
struct ProbabilityPolytopeMetricProjection {
    std::vector<ProbabilityPolytopeScenario> scenario_probabilities{};
    std::vector<ProbabilityEventConstraint> events{};
    ProbabilityPolytopeMetricRange expectation{};
    double maximum_endpoint_constraint_violation{0.0};
    double maximum_endpoint_objective_reconciliation_error{0.0};
    double maximum_endpoint_optimality_residual{0.0};
};

// A full probability witness p and its canonical fractional upper-tail mass y.
// tail_mass_weights is selected pro rata inside an exact equal-value boundary
// block and satisfies 0 <= y <= p and sum(y)=tail_probability up to the
// published floating-point residual. optimality_residual is the final simplex
// tableau's reduced-cost residual in expected-shortfall units, not a dual-gap
// certificate. threshold_formula_reconciliation_error is populated for the
// minimum endpoint selected by threshold enumeration and is zero otherwise.
struct ProbabilityPolytopeUpperExpectedShortfallEndpoint {
    double value{0.0};
    std::vector<double> scenario_weights{};
    std::vector<double> tail_mass_weights{};
    double maximum_constraint_violation{0.0};
    double maximum_tail_mass_violation{0.0};
    double objective_reconciliation_error{0.0};
    double threshold_formula_reconciliation_error{0.0};
    double optimality_residual{0.0};
};

// An audited floating-point projection of the upper tail_probability fraction
// of a finite signed scenario objective. It is not a symbolic optimum, an
// independent dual certificate, empirical calibration, or fair value. The
// minimum enumerates distinct exact scenario-value thresholds; the maximum
// uses a lifted full-measure/tail-mass LP. A deterministic full witness can be
// non-unique even though equal-value boundary tail mass is canonicalized.
struct ProbabilityPolytopeUpperExpectedShortfallProjection {
    std::vector<ProbabilityPolytopeScenario> scenario_probabilities{};
    std::vector<ProbabilityEventConstraint> events{};
    double tail_probability{0.0};
    ProbabilityPolytopeUpperExpectedShortfallEndpoint minimum{};
    double central{0.0};
    std::vector<double> central_tail_mass_weights{};
    double central_objective_reconciliation_error{0.0};
    ProbabilityPolytopeUpperExpectedShortfallEndpoint maximum{};
    double minimum_selected_threshold{0.0};
    std::size_t distinct_thresholds_examined{0U};
    double maximum_endpoint_constraint_violation{0.0};
    double maximum_endpoint_tail_mass_violation{0.0};
    double maximum_endpoint_objective_reconciliation_error{0.0};
    double maximum_endpoint_threshold_formula_reconciliation_error{0.0};
    double maximum_endpoint_optimality_residual{0.0};
    // Covers every inner linear solve in minimum threshold enumeration, not
    // only the solve associated with the selected threshold.
    double maximum_threshold_enumeration_optimality_residual{0.0};
};

// Prepared immutable projector over the explicitly enumerated scenarios in a
// PortfolioConfig. Event constraints reduce probability-set dimensionality;
// they do not generate payoff states, infer dependence, calibrate probabilities,
// or produce a market quote or fair value.
class ProbabilityPolytopeProjector {
public:
    ProbabilityPolytopeProjector(const PortfolioConfig& portfolio,
        const ProbabilityPolytopeConfig& probability_polytope);

    [[nodiscard]] ProbabilityPolytopeMetricProjection project_expectation(
        const std::vector<ProbabilityPolytopeScenarioValue>& scenario_values)
        const;

    // tail_probability is tail mass (0.20 means the largest twenty percent),
    // not a confidence level. Event-constrained solves require at least 1e-6;
    // the event-free v0.1 delegation continues to accept every value in (0,1].
    [[nodiscard]] ProbabilityPolytopeUpperExpectedShortfallProjection
    project_upper_expected_shortfall(
        const std::vector<ProbabilityPolytopeScenarioValue>& scenario_values,
        double tail_probability) const;

private:
    struct State;
    std::shared_ptr<const State> state_{};
};

void validate_probability_polytope_config(const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope);

} // namespace naturalehia::cellular_finance
