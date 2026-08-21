// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/model.hpp>

int main() {
    const naturalehia::protein_logic::ModelParameters parameters{-4.0, 8.0, 8.0, -16.0};
    const auto assessment = naturalehia::protein_logic::assess_xor(parameters);
    return assessment.passes_threshold ? 0 : 1;
}