// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_config.hpp>
#include <naturalehia/cellular_finance/joint_cohort_capital_stack.hpp>
#include <naturalehia/cellular_finance/joint_cohort_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(
    double first, double second, double tolerance = 1.0e-11) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] bool same_weights(
    const std::vector<double>& first,
    const std::vector<double>& second) {
    return first.size() == second.size() &&
        std::equal(first.begin(), first.end(), second.begin(),
            [](double left, double right) { return near(left, right); });
}

[[nodiscard]] bool same_range(
    const cf::AmbiguityMetricRange& first,
    const cf::AmbiguityMetricRange& second) {
    return near(first.minimum.value, second.minimum.value) &&
        near(first.central, second.central) &&
        near(first.maximum.value, second.maximum.value) &&
        same_weights(first.minimum.scenario_weights,
            second.minimum.scenario_weights) &&
        same_weights(first.maximum.scenario_weights,
            second.maximum.scenario_weights);
}

void expect_invalid_argument(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] const cf::CapitalStackTrancheSummary& find_tranche(
    const cf::CapitalStackSummary& summary, std::string_view id) {
    const auto matching = std::find_if(summary.tranches.begin(),
        summary.tranches.end(), [id](const auto& tranche) {
            return tranche.tranche_id == id;
        });
    if (matching == summary.tranches.end()) {
        throw std::logic_error("joint-cohort bridge test tranche not found");
    }
    return *matching;
}

void check_probability_bounds_identity(
    const cf::PortfolioAmbiguityConfig& generated,
    const cf::CapitalStackSummary& stack) {
    check(generated.scenario_probabilities.size() ==
            stack.scenario_probability_bounds.size(),
        "capital stack retains every generated cohort probability bound");
    if (generated.scenario_probabilities.size() !=
        stack.scenario_probability_bounds.size()) {
        return;
    }
    for (std::size_t index = 0U;
         index < generated.scenario_probabilities.size(); ++index) {
        const cf::ScenarioProbabilityBounds& source =
            generated.scenario_probabilities[index];
        const cf::ScenarioProbabilityBounds& applied =
            stack.scenario_probability_bounds[index];
        check(source.scenario_id == applied.scenario_id &&
                source.lower_weight == applied.lower_weight &&
                near(source.central_weight, applied.central_weight) &&
                source.upper_weight == applied.upper_weight,
            "capital stack applies the primary generated bounds without substitution");
    }
}

void test_success_and_direct_equivalence(
    const cf::JointCohortPackage& package,
    const cf::SuccessParticipationConfig& loaded_participation,
    const cf::CapitalStackConfig& loaded_stack) {
    cf::SuccessParticipationConfig participation = loaded_participation;
    participation.target_worst_expected_npv_million = 1'000'000.0;
    cf::CapitalStackConfig terms = loaded_stack;
    terms.underlying_success_participation_fraction = 0.37;

    const cf::JointCohortCapitalStackResult bridged =
        cf::evaluate_joint_cohort_capital_stack(package.config.analysis,
            package.portfolio, package.observations, participation, terms);
    check(bridged.capital_stack.has_value() &&
            bridged.selected_underlying_financial_ranges.has_value() &&
            bridged.cohort.generated_probability_envelope.has_value() &&
            bridged.cohort.financial_ranges_available &&
            bridged.block_reason.empty(),
        "exportable cohort produces a capital-stack result");
    check(!bridged.calibrated_execution_authorized &&
            !bridged.cohort.calibrated_execution_authorized,
        "the bridge never authorizes calibrated execution");
    if (!bridged.capital_stack.has_value() ||
        !bridged.selected_underlying_financial_ranges.has_value() ||
        !bridged.cohort.generated_probability_envelope.has_value()) {
        return;
    }

    const cf::PortfolioConfig selected_portfolio =
        cf::apply_success_participation_fraction(
            package.portfolio, participation, 0.37);
    const cf::PortfolioAmbiguitySummary direct_selected_financial =
        cf::evaluate_portfolio_ambiguity(selected_portfolio,
            *bridged.cohort.generated_probability_envelope);
    const cf::PortfolioAmbiguitySummary& selected_financial =
        *bridged.selected_underlying_financial_ranges;
    check(selected_financial.projects.size() ==
            direct_selected_financial.projects.size(),
        "bridge publishes every selected-q project financial summary");
    if (selected_financial.projects.size() ==
        direct_selected_financial.projects.size()) {
        for (std::size_t index = 0U;
             index < selected_financial.projects.size(); ++index) {
            const cf::ProjectAmbiguitySummary& bridged_project =
                selected_financial.projects[index];
            const cf::ProjectAmbiguitySummary& direct_project =
                direct_selected_financial.projects[index];
            check(bridged_project.project_id == direct_project.project_id &&
                    same_range(bridged_project.expected_total_draws_million,
                        direct_project.expected_total_draws_million) &&
                    same_range(bridged_project.expected_total_receipts_million,
                        direct_project.expected_total_receipts_million) &&
                    same_range(
                        bridged_project.expected_outstanding_principal_million,
                        direct_project.expected_outstanding_principal_million) &&
                    same_range(
                        bridged_project.expected_realized_principal_loss_million,
                        direct_project.expected_realized_principal_loss_million) &&
                    same_range(bridged_project.expected_npv_before_pool_costs_million,
                        direct_project.expected_npv_before_pool_costs_million) &&
                    same_range(bridged_project.principal_impairment_probability,
                        direct_project.principal_impairment_probability) &&
                    same_range(bridged_project.negative_npv_probability,
                        direct_project.negative_npv_probability),
                "bridge selected-q project ranges equal direct ambiguity evaluation");
        }
    }
    check(same_range(selected_financial.expected_total_receipts_million,
                     direct_selected_financial.expected_total_receipts_million) &&
            same_range(selected_financial.expected_npv_million,
                       direct_selected_financial.expected_npv_million) &&
            same_range(selected_financial.principal_loss_expected_shortfall_95_million,
                       direct_selected_financial.principal_loss_expected_shortfall_95_million) &&
            same_range(selected_financial.principal_loss_expected_shortfall_99_million,
                       direct_selected_financial.principal_loss_expected_shortfall_99_million),
        "bridge selected-q pool ranges and loss tails equal direct ambiguity evaluation");
    check(bridged.cohort.financial_ranges.has_value(),
        "cohort retains its bound-portfolio financial input view");
    if (bridged.cohort.financial_ranges.has_value()) {
        const cf::PortfolioAmbiguitySummary& evidence_financial =
            *bridged.cohort.financial_ranges;
        check(!near(selected_financial.expected_total_receipts_million.central,
                    evidence_financial.expected_total_receipts_million.central) &&
                !near(selected_financial.expected_npv_million.central,
                    evidence_financial.expected_npv_million.central),
            "non-unit q changes receipts and NPV from the bound-portfolio view");
        check(same_range(selected_financial.expected_total_draws_million,
                         evidence_financial.expected_total_draws_million) &&
                same_range(selected_financial.expected_outstanding_principal_million,
                           evidence_financial.expected_outstanding_principal_million) &&
                same_range(selected_financial.expected_principal_loss_million,
                           evidence_financial.expected_principal_loss_million) &&
                same_range(
                    selected_financial.principal_loss_expected_shortfall_95_million,
                    evidence_financial.principal_loss_expected_shortfall_95_million) &&
                same_range(
                    selected_financial.principal_loss_expected_shortfall_99_million,
                    evidence_financial.principal_loss_expected_shortfall_99_million),
            "fixed q leaves draws, exposure, realized loss, and loss tails unchanged");
    }

    const cf::CapitalStackSummary direct = cf::evaluate_capital_stack(
        package.portfolio,
        *bridged.cohort.generated_probability_envelope,
        participation, terms);
    const cf::CapitalStackSummary& stack = *bridged.capital_stack;
    check(stack.underlying_success_participation_fraction == 0.37 &&
            direct.underlying_success_participation_fraction == 0.37,
        "the bridge preserves the explicitly fixed participation fraction");
    check(same_range(stack.expected_underlying_on_demand_npv_million,
                     direct.expected_underlying_on_demand_npv_million) &&
            same_range(stack.expected_fully_funded_stack_npv_at_pool_hurdle_million,
                       direct.expected_fully_funded_stack_npv_at_pool_hurdle_million) &&
            same_range(stack.expected_prefunding_drag_npv_million,
                       direct.expected_prefunding_drag_npv_million),
        "bridge pool projections are identical to the direct capital-stack engine");
    check(stack.tranches.size() == direct.tranches.size(),
        "bridge and direct stack retain the same tranche count");
    if (stack.tranches.size() == direct.tranches.size()) {
        for (std::size_t index = 0U; index < stack.tranches.size(); ++index) {
            const auto& bridged_tranche = stack.tranches[index];
            const auto& direct_tranche = direct.tranches[index];
            check(bridged_tranche.tranche_id == direct_tranche.tranche_id &&
                    same_range(bridged_tranche.expected_total_distributions_million,
                        direct_tranche.expected_total_distributions_million) &&
                    same_range(bridged_tranche.expected_realized_principal_loss_million,
                        direct_tranche.expected_realized_principal_loss_million) &&
                    same_range(bridged_tranche.expected_npv_at_tranche_hurdle_million,
                        direct_tranche.expected_npv_at_tranche_hurdle_million) &&
                    same_range(bridged_tranche.principal_impairment_probability,
                        direct_tranche.principal_impairment_probability),
                "bridge tranche projections are identical to the direct waterfall");
        }
    }
    check_probability_bounds_identity(
        *bridged.cohort.generated_probability_envelope, stack);

    const double epsilon = bridged.cohort.hoeffding_bonferroni_epsilon;
    const cf::CapitalStackTrancheSummary& first_loss =
        find_tranche(stack, "first-loss-residual");
    const cf::CapitalStackTrancheSummary& senior =
        find_tranche(stack, "senior");
    check(near(first_loss.principal_impairment_probability.maximum.value,
                   0.45 + epsilon) &&
            near(senior.principal_impairment_probability.minimum.value, 0.0) &&
            near(senior.principal_impairment_probability.central, 0.02) &&
            near(senior.principal_impairment_probability.maximum.value,
                   0.15 + epsilon) &&
            near(senior.expected_realized_principal_loss_million.maximum.value,
                   10.0 * (0.15 + epsilon)),
        "cohort uncertainty reaches first-loss and senior claim-level risk ranges");
}

void test_blocked_paths(const cf::JointCohortPackage& package,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& terms) {
    std::vector<cf::JointCohortObservation> repeated = package.observations;
    repeated[1U].cluster_id = repeated[0U].cluster_id;
    const cf::JointCohortCapitalStackResult cluster_blocked =
        cf::evaluate_joint_cohort_capital_stack(package.config.analysis,
            package.portfolio, repeated, participation, terms);
    check(!cluster_blocked.capital_stack.has_value() &&
            !cluster_blocked.selected_underlying_financial_ranges.has_value() &&
            !cluster_blocked.cohort.primary_outer_set_available &&
            cluster_blocked.block_reason.find("repeated non-excluded cluster_id") !=
                std::string::npos &&
            !cluster_blocked.calibrated_execution_authorized,
        "statistically blocked cohort cannot reach the capital stack");

    cf::PortfolioConfig outside_portfolio = package.portfolio;
    for (cf::JointScenario& scenario : outside_portfolio.joint_scenarios) {
        if (scenario.id == "common-success") {
            scenario.weight = 0.01;
        } else if (scenario.id == "common-loss") {
            scenario.weight = 0.97;
        } else {
            scenario.weight = 0.01;
        }
    }
    const cf::JointCohortCapitalStackResult reference_blocked =
        cf::evaluate_joint_cohort_capital_stack(package.config.analysis,
            outside_portfolio, package.observations, participation, terms);
    check(!reference_blocked.capital_stack.has_value() &&
            !reference_blocked.selected_underlying_financial_ranges.has_value() &&
            reference_blocked.cohort.primary_outer_set_available &&
            !reference_blocked.cohort.portfolio_reference_within_primary_bounds &&
            reference_blocked.block_reason.find("reference weights") !=
                std::string::npos,
        "reference weights outside the primary cohort set cannot reach the stack");
}

void test_incompatible_terms_throw(const cf::JointCohortPackage& package,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& terms) {
    cf::CapitalStackConfig incompatible = terms;
    incompatible.tranches.back().detachment_million = 19.0;
    expect_invalid_argument(
        [&] {
            static_cast<void>(cf::evaluate_joint_cohort_capital_stack(
                package.config.analysis, package.portfolio,
                package.observations, participation, incompatible));
        },
        "an exportable cohort does not bypass incompatible capital-stack terms");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "usage: joint_cohort_capital_stack_tests "
                     "<joint-cohort-fixture-directory> "
                     "<success-participation.cfg> <capital-stack.cfg>\n";
        return 2;
    }
    try {
        const std::filesystem::path fixture(argv[1]);
        const cf::JointCohortPackage package =
            cf::load_joint_cohort_package(fixture / "cohort.cfg");
        const cf::SuccessParticipationConfig participation =
            cf::load_success_participation_config(
                std::filesystem::path(argv[2]));
        const cf::CapitalStackConfig terms =
            cf::load_capital_stack_config(std::filesystem::path(argv[3]));

        test_success_and_direct_equivalence(package, participation, terms);
        test_blocked_paths(package, participation, terms);
        test_incompatible_terms_throw(package, participation, terms);
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " joint-cohort capital-stack test(s) failed\n";
        return 1;
    }
    std::cout << "joint-cohort capital-stack bridge tests passed\n";
    return 0;
}
