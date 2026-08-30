// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/probability_polytope_tail_attribution.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
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
    double first, double second, double tolerance = 1.0e-9) {
    return std::abs(first - second) <= tolerance *
        std::max({1.0, std::abs(first), std::abs(second)});
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

[[nodiscard]] cf::MonthlyAmount amount(double value) {
    return cf::MonthlyAmount{0U, value};
}

[[nodiscard]] cf::ProjectJointPath project_path(
    std::string id, bool loss) {
    cf::ProjectJointPath path;
    path.project_id = std::move(id);
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.capital_draws = {amount(10.0)};
    if (!loss) {
        path.investor_receipts = {cf::InvestorReceipt{
            1U, "principal", 10.0, 10.0}};
    }
    return path;
}

[[nodiscard]] cf::JointScenario joint_scenario(std::string id,
    double weight, bool project_a_loss, bool project_b_loss,
    bool reverse_project_paths) {
    cf::JointScenario scenario;
    scenario.id = std::move(id);
    scenario.weight = weight;
    scenario.project_paths = {
        project_path("project-a", project_a_loss),
        project_path("project-b", project_b_loss)};
    if (reverse_project_paths) {
        std::reverse(scenario.project_paths.begin(),
            scenario.project_paths.end());
    }
    const double available =
        (project_a_loss ? 0.0 : 10.0) +
        (project_b_loss ? 0.0 : 10.0);
    if (available > 0.0) {
        cf::ScenarioCashSource source;
        source.id = "principal";
        source.kind = cf::PortfolioCashSource::Commercial;
        source.cash_available = {cf::MonthlyAmount{1U, available}};
        scenario.cash_sources = {std::move(source)};
    }
    return scenario;
}

[[nodiscard]] cf::PortfolioConfig two_project_portfolio(
    bool reverse_every_input_order = false) {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label =
        "two-project common-tail attribution unit test";
    portfolio.source_note = "synthetic hand-solved loss paths only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 1U;
    portfolio.projects = {
        cf::PortfolioProject{
            "project-a", cf::ProjectStage::Pilot, 10.0},
        cf::PortfolioProject{
            "project-b", cf::ProjectStage::Demonstration, 10.0}};
    portfolio.joint_scenarios = {
        joint_scenario("s10", 0.15, true, false,
            reverse_every_input_order),
        joint_scenario("s00", 0.50, false, false,
            reverse_every_input_order),
        joint_scenario("s11", 0.20, true, true,
            reverse_every_input_order),
        joint_scenario("s01", 0.15, false, true,
            reverse_every_input_order)};
    if (reverse_every_input_order) {
        std::reverse(portfolio.projects.begin(), portfolio.projects.end());
        std::reverse(portfolio.joint_scenarios.begin(),
            portfolio.joint_scenarios.end());
    }
    return portfolio;
}

[[nodiscard]] cf::ProbabilityPolytopeConfig two_project_polytope(
    bool reverse_every_input_order = false) {
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label =
        "two-project exact margins and bounded common shock";
    polytope.source_note = "synthetic hand-solved event constraints only";
    polytope.scenario_probabilities = {
        {"s11", 0.0, 0.20, 1.0},
        {"s00", 0.0, 0.50, 1.0},
        {"s01", 0.0, 0.15, 1.0},
        {"s10", 0.0, 0.15, 1.0}};
    polytope.events = {
        cf::ProbabilityEventConstraint{"common-loss",
            "both projects lose principal", 0.10, 0.25, {"s11"}},
        cf::ProbabilityEventConstraint{"project-b-loss",
            "project b loses principal", 0.35, 0.35,
            {"s01", "s11"}},
        cf::ProbabilityEventConstraint{"project-a-loss",
            "project a loses principal", 0.35, 0.35,
            {"s10", "s11"}}};
    if (reverse_every_input_order) {
        std::reverse(polytope.scenario_probabilities.begin(),
            polytope.scenario_probabilities.end());
        std::reverse(polytope.events.begin(), polytope.events.end());
        for (cf::ProbabilityEventConstraint& event : polytope.events) {
            std::reverse(event.scenario_ids.begin(),
                event.scenario_ids.end());
        }
    }
    return polytope;
}

[[nodiscard]] std::vector<cf::ProbabilityPolytopeScenarioValue>
pool_loss_values(const cf::PortfolioConfig& portfolio) {
    const cf::PortfolioSummary summary = cf::evaluate_portfolio(portfolio);
    std::vector<cf::ProbabilityPolytopeScenarioValue> values;
    values.reserve(summary.scenarios.size());
    for (const cf::JointScenarioResult& scenario : summary.scenarios) {
        values.push_back({
            scenario.scenario_id, scenario.principal_loss_million});
    }
    return values;
}

[[nodiscard]] std::vector<cf::ProbabilityPolytopeScenarioValue>
project_a_loss_values(const cf::PortfolioConfig& portfolio) {
    const cf::PortfolioSummary summary = cf::evaluate_portfolio(portfolio);
    std::vector<cf::ProbabilityPolytopeScenarioValue> values;
    values.reserve(summary.scenarios.size());
    for (const cf::JointScenarioResult& scenario : summary.scenarios) {
        const auto matching = std::find_if(scenario.projects.begin(),
            scenario.projects.end(), [](const cf::ProjectPathResult& project) {
                return project.project_id == "project-a";
            });
        values.push_back({scenario.scenario_id,
            matching->principal_loss_million});
    }
    return values;
}

[[nodiscard]] cf::ProbabilityPolytopeUpperExpectedShortfallProjection
pool_loss_projection(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    double tail_probability = 0.25) {
    return cf::ProbabilityPolytopeProjector(portfolio, polytope)
        .project_upper_expected_shortfall(
            pool_loss_values(portfolio), tail_probability);
}

void test_hand_solved_common_tail_attribution() {
    const cf::PortfolioConfig portfolio = two_project_portfolio();
    const auto projection = pool_loss_projection(
        portfolio, two_project_polytope());
    const cf::ProbabilityPolytopePoolLossTailAttribution attribution =
        cf::attribute_probability_polytope_pool_loss_tail(
            portfolio, projection);

    // With exact project impairment margins of 35%, p11 in [10%, 25%],
    // and a 25% tail, pool ES = 10 + 40*p11.
    check(near(projection.minimum.value, 14.0) &&
            near(projection.central, 18.0) &&
            near(projection.maximum.value, 20.0) &&
            near(attribution.minimum_pool_es_million, 14.0) &&
            near(attribution.central_pool_es_million, 18.0) &&
            near(attribution.maximum_pool_es_million, 20.0),
        "hand-solved aggregate pool ES endpoints are retained exactly");
    check(attribution.projects.size() == 2U &&
            attribution.projects[0].project_id == "project-a" &&
            attribution.projects[1].project_id == "project-b" &&
            near(attribution.projects[0]
                    .at_minimum_pool_es_witness_million,
                7.0) &&
            near(attribution.projects[1]
                    .at_minimum_pool_es_witness_million,
                7.0) &&
            near(attribution.projects[0].at_central_measure_million, 9.0) &&
            near(attribution.projects[1].at_central_measure_million, 9.0) &&
            near(attribution.projects[0]
                    .at_maximum_pool_es_witness_million,
                10.0) &&
            near(attribution.projects[1]
                    .at_maximum_pool_es_witness_million,
                10.0),
        "common aggregate-loss witnesses give additive hand-solved project contributions");
    check(attribution.scenario_ids ==
                std::vector<std::string>({"s00", "s01", "s10", "s11"}) &&
            attribution.minimum_pool_es_tail_mass_weights ==
                projection.minimum.tail_mass_weights &&
            attribution.central_tail_mass_weights ==
                projection.central_tail_mass_weights &&
            attribution.maximum_pool_es_tail_mass_weights ==
                projection.maximum.tail_mass_weights,
        "attribution copies the projector's canonical tail masses without changing them");
    check(near(attribution.central_tail_mass_weights[0], 0.0) &&
            near(attribution.central_tail_mass_weights[1], 0.025) &&
            near(attribution.central_tail_mass_weights[2], 0.025) &&
            near(attribution.central_tail_mass_weights[3], 0.20),
        "tied single-loss atoms share the fractional central boundary pro rata");
    check(attribution.maximum_pathwise_pool_loss_reconciliation_error_million <=
                1.0e-12 &&
            attribution
                    .maximum_contribution_to_pool_es_reconciliation_error_million <=
                1.0e-9 &&
            attribution.witness_disclosure ==
                cf::kProbabilityPolytopeTailAttributionWitnessDisclosure,
        "path losses, contribution sums, and the no-uniqueness disclosure are published");
}

void test_permutation_invariance() {
    const cf::PortfolioConfig first_portfolio = two_project_portfolio();
    const cf::PortfolioConfig second_portfolio = two_project_portfolio(true);
    const auto first =
        cf::attribute_probability_polytope_pool_loss_tail(first_portfolio,
            pool_loss_projection(first_portfolio, two_project_polytope()));
    const auto second =
        cf::attribute_probability_polytope_pool_loss_tail(second_portfolio,
            pool_loss_projection(second_portfolio,
                two_project_polytope(true)));

    check(first.scenario_ids == second.scenario_ids &&
            first.minimum_pool_es_tail_mass_weights ==
                second.minimum_pool_es_tail_mass_weights &&
            first.central_tail_mass_weights ==
                second.central_tail_mass_weights &&
            first.maximum_pool_es_tail_mass_weights ==
                second.maximum_pool_es_tail_mass_weights &&
            first.projects.size() == second.projects.size() &&
            first.projects[0].project_id == second.projects[0].project_id &&
            first.projects[1].project_id == second.projects[1].project_id &&
            near(first.projects[0]
                    .at_minimum_pool_es_witness_million,
                second.projects[0]
                    .at_minimum_pool_es_witness_million) &&
            near(first.projects[1].at_central_measure_million,
                second.projects[1].at_central_measure_million),
        "scenario, event, project, and path input permutations leave canonical attribution unchanged");
}

void test_wrong_taxonomy_and_wrong_objective_rejection() {
    const cf::PortfolioConfig portfolio = two_project_portfolio();
    const cf::ProbabilityPolytopeConfig polytope = two_project_polytope();
    cf::ProbabilityPolytopeUpperExpectedShortfallProjection wrong_taxonomy =
        pool_loss_projection(portfolio, polytope);
    wrong_taxonomy.scenario_probabilities[0].scenario_id = "not-s00";
    expect_invalid_argument(
        [&] {
            (void)cf::attribute_probability_polytope_pool_loss_tail(
                portfolio, wrong_taxonomy);
        },
        "a projection with a different scenario taxonomy is rejected");

    const auto wrong_objective =
        cf::ProbabilityPolytopeProjector(portfolio, polytope)
            .project_upper_expected_shortfall(
                project_a_loss_values(portfolio), 0.25);
    expect_invalid_argument(
        [&] {
            (void)cf::attribute_probability_polytope_pool_loss_tail(
                portfolio, wrong_objective);
        },
        "a valid ES projection of a project objective is rejected as a pool-loss projection");

    auto noncanonical_tie = pool_loss_projection(portfolio, polytope);
    noncanonical_tie.central_tail_mass_weights[1] += 0.01;
    noncanonical_tie.central_tail_mass_weights[2] -= 0.01;
    expect_invalid_argument(
        [&] {
            (void)cf::attribute_probability_polytope_pool_loss_tail(
                portfolio, noncanonical_tie);
        },
        "a biased allocation inside an equal aggregate-loss boundary is rejected rather than normalized");
}

void test_event_free_delegation_and_full_distribution_tail() {
    const cf::PortfolioConfig portfolio = two_project_portfolio();
    cf::ProbabilityPolytopeConfig box = two_project_polytope();
    box.events.clear();
    const auto delegated_projection =
        pool_loss_projection(portfolio, box);
    const auto delegated_attribution =
        cf::attribute_probability_polytope_pool_loss_tail(
            portfolio, delegated_projection);
    check(delegated_projection.distinct_thresholds_examined == 0U &&
            near(delegated_attribution.minimum_pool_es_million, 0.0) &&
            near(delegated_attribution.central_pool_es_million, 18.0) &&
            near(delegated_attribution.maximum_pool_es_million, 20.0),
        "event-free v0.1 ES delegation remains a valid common-tail attribution input");

    const auto full_distribution_projection = pool_loss_projection(
        portfolio, two_project_polytope(), 1.0);
    const auto full_distribution_attribution =
        cf::attribute_probability_polytope_pool_loss_tail(
            portfolio, full_distribution_projection);
    check(full_distribution_projection.distinct_thresholds_examined == 0U &&
            near(full_distribution_attribution.minimum_pool_es_million,
                7.0) &&
            near(full_distribution_attribution.central_pool_es_million,
                7.0) &&
            near(full_distribution_attribution.maximum_pool_es_million,
                7.0) &&
            near(full_distribution_attribution.projects[0]
                    .at_central_measure_million,
                3.5) &&
            near(full_distribution_attribution.projects[1]
                    .at_central_measure_million,
                3.5),
        "tau one reuses the complete common probability measure without requiring threshold enumeration");
}

} // namespace

int main() {
    test_hand_solved_common_tail_attribution();
    test_permutation_invariance();
    test_wrong_taxonomy_and_wrong_objective_rejection();
    test_event_free_delegation_and_full_distribution_tail();

    if (failures != 0) {
        std::cerr << failures <<
            " probability polytope tail attribution test(s) failed\n";
        return 1;
    }
    std::cout <<
        "all probability polytope tail attribution tests passed\n";
    return 0;
}
