// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/probability_polytope.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view
    kProbabilityPolytopeTailAttributionWitnessDisclosure{
        "Each column attributes one selected common aggregate-loss tail "
        "witness; neither the optimizing full measure nor the resulting "
        "project attribution is claimed to be unique."};

// These are additive shares under the three aggregate pool-loss ES witnesses.
// They are not independently optimized project-tail bounds. In particular,
// "minimum" means the project contribution at the selected measure minimizing
// pool ES, not the smallest feasible contribution for that project.
struct ProbabilityPolytopeProjectPoolLossTailContribution {
    std::string project_id{};
    double at_minimum_pool_es_witness_million{0.0};
    double at_central_measure_million{0.0};
    double at_maximum_pool_es_witness_million{0.0};
};

// A self-describing copy of the exact common tail masses used for attribution.
// scenario_ids and all three mass vectors use ascending scenario-id order. The
// vectors are copied from the supplied ES projection without re-optimization,
// reallocation, or normalization. Project rows use ascending project-id order.
struct ProbabilityPolytopePoolLossTailAttribution {
    double tail_probability{0.0};
    std::vector<std::string> scenario_ids{};
    std::vector<double> minimum_pool_es_tail_mass_weights{};
    std::vector<double> central_tail_mass_weights{};
    std::vector<double> maximum_pool_es_tail_mass_weights{};

    double minimum_pool_es_million{0.0};
    double central_pool_es_million{0.0};
    double maximum_pool_es_million{0.0};
    std::vector<ProbabilityPolytopeProjectPoolLossTailContribution>
        projects{};

    double maximum_pathwise_pool_loss_reconciliation_error_million{0.0};
    double maximum_contribution_to_pool_es_reconciliation_error_million{0.0};
    std::string witness_disclosure{
        kProbabilityPolytopeTailAttributionWitnessDisclosure};
};

// Re-evaluates the validated fixed cash paths, verifies that the projection is
// an upper-ES projection of their aggregate resolved principal loss, and then
// attributes that same selected min/central/max pool tail to projects. It does
// not optimize a project objective or alter any probability or tail mass. The
// adapter independently checks pathwise loss additivity, taxonomy, probability
// feasibility, upper-tail ordering, tied-boundary pro-rata allocation, and ES
// reconciliation. It does not re-solve the global ES extrema or construct an
// independent dual certificate; that optimality boundary remains the supplied
// projection's explicitly reported simplex audit. Neither module calibrates or
// authenticates the economic evidence behind an event bound.
[[nodiscard]] ProbabilityPolytopePoolLossTailAttribution
attribute_probability_polytope_pool_loss_tail(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeUpperExpectedShortfallProjection&
        pool_loss_projection);

} // namespace naturalehia::cellular_finance
