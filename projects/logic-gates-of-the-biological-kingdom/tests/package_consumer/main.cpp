// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/equilibrium_acceptance.hpp>
#include <naturalehia/protein_logic/model.hpp>

int main() {
    const naturalehia::protein_logic::ModelParameters parameters{-4.0, 8.0, 8.0, -16.0};
    const auto endpoint_assessment = naturalehia::protein_logic::assess_xor(parameters);

    const naturalehia::protein_logic::EquilibriumParameters equilibrium_parameters{
        -4.0,
        {1.0, 1.0, 10.0},
        {0.01, 0.01, 0.001},
    };
    const naturalehia::protein_logic::OperatingWindows windows{
        {{0.0, 0.01}, {3.0, 10.0}},
        {{0.0, 0.01}, {3.0, 10.0}},
    };
    const auto region_assessment =
        naturalehia::protein_logic::assess_nominal(equilibrium_parameters, windows);
    const naturalehia::protein_logic::SteadyStateXorCriteria criteria{
        0.5,
        0.3,
        0.15,
        0.05,
    };
    const auto acceptance = naturalehia::protein_logic::assess_steady_state_xor(
        region_assessment.global, criteria);

    return endpoint_assessment.passes_threshold && region_assessment.global.passes_threshold &&
           acceptance.overall_outcome == naturalehia::protein_logic::CriterionOutcome::Pass
        ? 0
        : 1;
}
