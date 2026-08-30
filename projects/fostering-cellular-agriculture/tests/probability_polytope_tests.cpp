// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/probability_polytope.hpp>

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
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
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
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

void expect_runtime_error(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::runtime_error&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] cf::ProjectJointPath empty_path() {
    cf::ProjectJointPath path;
    path.project_id = "project";
    path.resolution = cf::ProjectPathResolution::Resolved;
    return path;
}

[[nodiscard]] cf::PortfolioConfig make_portfolio(
    const std::vector<std::pair<std::string, double>>& scenarios) {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "probability polytope unit-test atoms";
    portfolio.source_note = "synthetic unit-test probabilities only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "dimensionless objective test basis";
    portfolio.horizon_months = 1U;
    portfolio.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 1.0}};
    portfolio.joint_scenarios.reserve(scenarios.size());
    for (const auto& [id, weight] : scenarios) {
        cf::JointScenario scenario;
        scenario.id = id;
        scenario.weight = weight;
        scenario.project_paths = {empty_path()};
        portfolio.joint_scenarios.push_back(std::move(scenario));
    }
    return portfolio;
}

[[nodiscard]] cf::PortfolioConfig four_atom_portfolio() {
    // Deliberately noncanonical caller order.
    return make_portfolio({
        {"s10", 0.20}, {"s00", 0.40},
        {"s11", 0.20}, {"s01", 0.20},
    });
}

[[nodiscard]] cf::ProbabilityPolytopeConfig four_atom_polytope() {
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "overlapping binary margins and common shock";
    polytope.source_note = "synthetic hand-solved event constraints";
    // Deliberately noncanonical caller order.
    polytope.scenario_probabilities = {
        cf::ProbabilityPolytopeScenario{"s11", 0.0, 0.20, 0.80},
        cf::ProbabilityPolytopeScenario{"s00", 0.0, 0.40, 0.80},
        cf::ProbabilityPolytopeScenario{"s01", 0.0, 0.20, 0.80},
        cf::ProbabilityPolytopeScenario{"s10", 0.0, 0.20, 0.80},
    };
    polytope.events = {
        cf::ProbabilityEventConstraint{"common-shock",
            "both binary events occur", 0.10, 0.25, {"s11"}},
        cf::ProbabilityEventConstraint{"margin-b",
            "second binary event occurs", 0.35, 0.45,
            {"s11", "s01"}},
        cf::ProbabilityEventConstraint{"margin-a",
            "first binary event occurs", 0.35, 0.45,
            {"s11", "s10"}},
    };
    return polytope;
}

[[nodiscard]] std::vector<cf::ProbabilityPolytopeScenarioValue>
hand_objective() {
    return {
        {"s11", 4.0}, {"s01", 1.0},
        {"s00", 0.0}, {"s10", 2.0},
    };
}

[[nodiscard]] double value_for(
    const std::vector<cf::ProbabilityPolytopeScenarioValue>& values,
    const std::string& scenario_id) {
    const auto matching = std::find_if(values.begin(), values.end(),
        [&scenario_id](const auto& value) {
            return value.scenario_id == scenario_id;
        });
    return matching == values.end()
        ? std::numeric_limits<double>::quiet_NaN()
        : matching->value;
}

void check_endpoint(const cf::ProbabilityPolytopeMetricProjection& projection,
    const cf::ProbabilityPolytopeEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenarioValue>& values,
    std::string_view message) {
    if (projection.scenario_probabilities.size() !=
            endpoint.scenario_weights.size()) {
        check(false, message);
        return;
    }
    std::unordered_map<std::string, double> weights;
    double total = 0.0;
    double objective = 0.0;
    bool feasible = true;
    for (std::size_t index = 0U;
         index < projection.scenario_probabilities.size(); ++index) {
        const cf::ProbabilityPolytopeScenario& scenario =
            projection.scenario_probabilities[index];
        const double weight = endpoint.scenario_weights[index];
        weights.emplace(scenario.scenario_id, weight);
        total += weight;
        objective += weight * value_for(values, scenario.scenario_id);
        feasible = feasible &&
            weight >= scenario.lower_weight - 1.0e-10 &&
            weight <= scenario.upper_weight + 1.0e-10;
    }
    for (const cf::ProbabilityEventConstraint& event : projection.events) {
        double event_probability = 0.0;
        for (const std::string& member : event.scenario_ids) {
            event_probability += weights.at(member);
        }
        feasible = feasible &&
            event_probability >= event.lower_probability - 1.0e-10 &&
            event_probability <= event.upper_probability + 1.0e-10;
    }
    check(feasible && near(total, 1.0) && near(objective, endpoint.value) &&
            endpoint.maximum_constraint_violation <= 1.0e-10 &&
            endpoint.objective_reconciliation_error <= 1.0e-9 &&
            endpoint.optimality_residual <= 1.0e-9,
        message);
}

void check_tail_endpoint(
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    const cf::ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenarioValue>& values,
    std::string_view message) {
    if (projection.scenario_probabilities.size() !=
            endpoint.scenario_weights.size() ||
        endpoint.scenario_weights.size() != endpoint.tail_mass_weights.size()) {
        check(false, message);
        return;
    }
    std::unordered_map<std::string, double> weights;
    double probability_sum = 0.0;
    double tail_sum = 0.0;
    double tail_total = 0.0;
    bool feasible = true;
    for (std::size_t index = 0U;
         index < projection.scenario_probabilities.size(); ++index) {
        const auto& scenario = projection.scenario_probabilities[index];
        const double weight = endpoint.scenario_weights[index];
        const double tail = endpoint.tail_mass_weights[index];
        weights.emplace(scenario.scenario_id, weight);
        probability_sum += weight;
        tail_sum += tail;
        tail_total += tail * value_for(values, scenario.scenario_id);
        feasible = feasible &&
            weight >= scenario.lower_weight - 1.0e-10 &&
            weight <= scenario.upper_weight + 1.0e-10 &&
            tail >= -1.0e-10 && tail <= weight + 1.0e-10;
    }
    for (const auto& event : projection.events) {
        double probability = 0.0;
        for (const std::string& member : event.scenario_ids) {
            probability += weights.at(member);
        }
        feasible = feasible &&
            probability >= event.lower_probability - 1.0e-10 &&
            probability <= event.upper_probability + 1.0e-10;
    }
    const double reconstructed = tail_total / projection.tail_probability;
    check(feasible && near(probability_sum, 1.0) &&
            near(tail_sum, projection.tail_probability) &&
            near(reconstructed, endpoint.value) &&
            endpoint.maximum_constraint_violation <= 1.0e-10 &&
            endpoint.maximum_tail_mass_violation <= 1.0e-10 &&
            endpoint.objective_reconciliation_error <= 1.0e-8 &&
            endpoint.threshold_formula_reconciliation_error <= 1.0e-8 &&
            endpoint.optimality_residual <= 1.0e-8,
        message);
}

[[nodiscard]] std::vector<cf::ProbabilityPolytopeScenarioValue>
hand_loss_objective() {
    return {
        {"s11", 100.0}, {"s01", 10.0},
        {"s00", 0.0}, {"s10", 20.0},
    };
}

void test_hand_solved_overlapping_events() {
    const cf::ProbabilityPolytopeProjector projector(
        four_atom_portfolio(), four_atom_polytope());
    const auto values = hand_objective();
    const cf::ProbabilityPolytopeMetricProjection projection =
        projector.project_expectation(values);

    check(projection.scenario_probabilities.size() == 4U &&
            projection.scenario_probabilities[0].scenario_id == "s00" &&
            projection.scenario_probabilities[1].scenario_id == "s01" &&
            projection.scenario_probabilities[2].scenario_id == "s10" &&
            projection.scenario_probabilities[3].scenario_id == "s11" &&
            projection.events.size() == 3U &&
            projection.events[0].event_id == "common-shock" &&
            projection.events[1].event_id == "margin-a" &&
            projection.events[2].event_id == "margin-b" &&
            projection.events[1].scenario_ids ==
                std::vector<std::string>({"s10", "s11"}),
        "scenario, event, and membership taxonomies are canonicalized by id");

    // With A=P(s10+s11), B=P(s01+s11), and Z=P(s11),
    // E[X]=2A+B+Z. Thus the three independent extrema are simultaneous.
    check(near(projection.expectation.minimum.value, 1.15) &&
            near(projection.expectation.central, 1.40) &&
            near(projection.expectation.maximum.value, 1.60),
        "overlapping marginal/common-shock expectation has hand-solved endpoints");
    check_endpoint(projection, projection.expectation.minimum, values,
        "minimum publishes a complete feasible audited witness");
    check_endpoint(projection, projection.expectation.maximum, values,
        "maximum publishes a complete feasible audited witness");

    const std::vector<double> expected_minimum{0.40, 0.25, 0.25, 0.10};
    const std::vector<double> expected_maximum{0.35, 0.20, 0.20, 0.25};
    check(projection.expectation.minimum.scenario_weights.size() == 4U &&
            std::equal(expected_minimum.begin(), expected_minimum.end(),
                projection.expectation.minimum.scenario_weights.begin(),
                [](double expected, double actual) {
                    return near(expected, actual);
                }) &&
            std::equal(expected_maximum.begin(), expected_maximum.end(),
                projection.expectation.maximum.scenario_weights.begin(),
                [](double expected, double actual) {
                    return near(expected, actual);
                }),
        "hand-solved endpoint witnesses expose all four atom weights");
    check(projection.maximum_endpoint_constraint_violation <= 1.0e-10 &&
            projection.maximum_endpoint_objective_reconciliation_error <=
                1.0e-9 &&
            projection.maximum_endpoint_optimality_residual <= 1.0e-9,
        "projection-level audits aggregate both endpoints");
}

void test_input_order_invariance() {
    const auto values = hand_objective();
    const cf::ProbabilityPolytopeMetricProjection first =
        cf::ProbabilityPolytopeProjector(
            four_atom_portfolio(), four_atom_polytope())
            .project_expectation(values);

    cf::PortfolioConfig reordered_portfolio = four_atom_portfolio();
    std::reverse(reordered_portfolio.joint_scenarios.begin(),
        reordered_portfolio.joint_scenarios.end());
    cf::ProbabilityPolytopeConfig reordered_polytope = four_atom_polytope();
    std::reverse(reordered_polytope.scenario_probabilities.begin(),
        reordered_polytope.scenario_probabilities.end());
    std::reverse(reordered_polytope.events.begin(),
        reordered_polytope.events.end());
    for (cf::ProbabilityEventConstraint& event : reordered_polytope.events) {
        std::reverse(event.scenario_ids.begin(), event.scenario_ids.end());
    }
    auto reordered_values = values;
    std::reverse(reordered_values.begin(), reordered_values.end());
    const cf::ProbabilityPolytopeMetricProjection second =
        cf::ProbabilityPolytopeProjector(
            reordered_portfolio, reordered_polytope)
            .project_expectation(reordered_values);

    check(first.expectation.minimum.value ==
                second.expectation.minimum.value &&
            first.expectation.central == second.expectation.central &&
            first.expectation.maximum.value ==
                second.expectation.maximum.value,
        "endpoint values are invariant to every caller input order");
}

void test_degeneracy_constant_and_signed_objectives() {
    const cf::ProbabilityPolytopeProjector projector(
        four_atom_portfolio(), four_atom_polytope());
    const std::vector<cf::ProbabilityPolytopeScenarioValue> constant{
        {"s00", 7.0}, {"s01", 7.0},
        {"s10", 7.0}, {"s11", 7.0},
    };
    const auto constant_projection = projector.project_expectation(constant);
    check(near(constant_projection.expectation.minimum.value, 7.0) &&
            near(constant_projection.expectation.central, 7.0) &&
            near(constant_projection.expectation.maximum.value, 7.0),
        "constant objective remains constant over a non-singleton feasible set");
    check_endpoint(constant_projection,
        constant_projection.expectation.minimum, constant,
        "degenerate minimum publishes one feasible non-unique witness");
    check_endpoint(constant_projection,
        constant_projection.expectation.maximum, constant,
        "degenerate maximum publishes one feasible non-unique witness");

    const std::vector<cf::ProbabilityPolytopeScenarioValue> signed_values{
        {"s00", -4.0}, {"s01", -1.0},
        {"s10", 2.0}, {"s11", 5.0},
    };
    const auto signed_projection =
        projector.project_expectation(signed_values);
    check(near(signed_projection.expectation.minimum.value, -0.85) &&
            near(signed_projection.expectation.central, -0.40) &&
            near(signed_projection.expectation.maximum.value, 0.05),
        "signed objective is scaled and solved without a non-negativity assumption");
    check_endpoint(signed_projection,
        signed_projection.expectation.minimum, signed_values,
        "signed minimum witness reconciles directly");
    check_endpoint(signed_projection,
        signed_projection.expectation.maximum, signed_values,
        "signed maximum witness reconciles directly");
}

void test_v01_box_and_singleton_event_equivalence() {
    const cf::PortfolioConfig portfolio = four_atom_portfolio();
    cf::ProbabilityPolytopeConfig box = four_atom_polytope();
    box.events.clear();
    box.scenario_probabilities = {
        {"s00", 0.10, 0.40, 0.60},
        {"s01", 0.05, 0.20, 0.45},
        {"s10", 0.05, 0.20, 0.40},
        {"s11", 0.10, 0.20, 0.50},
    };
    const auto values = hand_objective();

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = box.scenario_label;
    ambiguity.source_note = box.source_note;
    for (const auto& scenario : box.scenario_probabilities) {
        ambiguity.scenario_probabilities.push_back(
            cf::ScenarioProbabilityBounds{scenario.scenario_id,
                scenario.lower_weight, scenario.central_weight,
                scenario.upper_weight});
    }
    std::vector<cf::AmbiguityScenarioMetricValue> old_values;
    for (const auto& value : values) {
        old_values.push_back(
            cf::AmbiguityScenarioMetricValue{value.scenario_id, value.value});
    }
    const cf::AmbiguityMetricProjection old_projection =
        cf::PortfolioAmbiguityProjector(portfolio, ambiguity)
            .project_expectation(old_values);
    const cf::ProbabilityPolytopeMetricProjection delegated =
        cf::ProbabilityPolytopeProjector(portfolio, box)
            .project_expectation(values);
    check(delegated.expectation.minimum.value ==
                old_projection.expectation.minimum.value &&
            delegated.expectation.central ==
                old_projection.expectation.central &&
            delegated.expectation.maximum.value ==
                old_projection.expectation.maximum.value &&
            delegated.expectation.minimum.scenario_weights ==
                old_projection.expectation.minimum.scenario_weights &&
            delegated.expectation.maximum.scenario_weights ==
                old_projection.expectation.maximum.scenario_weights,
        "event-free v0.2 delegates exact v0.1 endpoint values and tied witnesses");

    cf::ProbabilityPolytopeConfig singleton_events = box;
    for (cf::ProbabilityPolytopeScenario& scenario :
         singleton_events.scenario_probabilities) {
        singleton_events.events.push_back(cf::ProbabilityEventConstraint{
            "bound-" + scenario.scenario_id,
            "singleton event reproducing the component probability bound",
            scenario.lower_weight, scenario.upper_weight,
            {scenario.scenario_id}});
        scenario.lower_weight = 0.0;
        scenario.upper_weight = 1.0;
    }
    const auto singleton_projection =
        cf::ProbabilityPolytopeProjector(portfolio, singleton_events)
            .project_expectation(values);
    check(near(singleton_projection.expectation.minimum.value,
              old_projection.expectation.minimum.value) &&
            near(singleton_projection.expectation.central,
              old_projection.expectation.central) &&
            near(singleton_projection.expectation.maximum.value,
              old_projection.expectation.maximum.value),
        "singleton event slabs reproduce the v0.1 component-bound feasible set");
}

void test_hand_solved_event_expected_shortfall() {
    const cf::ProbabilityPolytopeProjector projector(
        four_atom_portfolio(), four_atom_polytope());
    const auto losses = hand_loss_objective();
    const auto projection =
        projector.project_upper_expected_shortfall(losses, 0.20);

    check(near(projection.minimum.value, 60.0) &&
            near(projection.central, 100.0) &&
            near(projection.maximum.value, 100.0),
        "event-polytope upper tail has hand-solved 60/100/100 endpoints");
    check(projection.distinct_thresholds_examined == 4U &&
            projection.minimum_selected_threshold == 20.0,
        "minimum enumerates every exact loss threshold and selects loss 20");
    check_tail_endpoint(projection, projection.minimum, losses,
        "minimum tail endpoint publishes audited full and tail measures");
    check_tail_endpoint(projection, projection.maximum, losses,
        "lifted maximum publishes audited full and canonical tail measures");

    const std::vector<double> expected_minimum_tail{0.0, 0.0, 0.10, 0.10};
    const std::vector<double> expected_top_tail{0.0, 0.0, 0.0, 0.20};
    check(projection.minimum.tail_mass_weights.size() == 4U &&
            std::equal(expected_minimum_tail.begin(),
                expected_minimum_tail.end(),
                projection.minimum.tail_mass_weights.begin(),
                [](double expected, double actual) {
                    return near(expected, actual);
                }) &&
            std::equal(expected_top_tail.begin(), expected_top_tail.end(),
                projection.central_tail_mass_weights.begin(),
                [](double expected, double actual) {
                    return near(expected, actual);
                }) &&
            std::equal(expected_top_tail.begin(), expected_top_tail.end(),
                projection.maximum.tail_mass_weights.begin(),
                [](double expected, double actual) {
                    return near(expected, actual);
                }),
        "canonical tails contain .10 common shock plus .10 next loss at the minimum");
    check(projection.maximum_endpoint_constraint_violation <= 1.0e-10 &&
            projection.maximum_endpoint_tail_mass_violation <= 1.0e-10 &&
            projection.maximum_endpoint_objective_reconciliation_error <=
                1.0e-8 &&
            projection.maximum_endpoint_threshold_formula_reconciliation_error <=
                1.0e-8 &&
            projection.maximum_endpoint_optimality_residual <= 1.0e-8 &&
            projection.maximum_threshold_enumeration_optimality_residual <=
                1.0e-8,
        "expected-shortfall projection publishes bounded aggregate audits");
}

void test_expected_shortfall_order_constants_signed_ties_and_tau_one() {
    const cf::PortfolioConfig portfolio = four_atom_portfolio();
    const cf::ProbabilityPolytopeConfig polytope = four_atom_polytope();
    const cf::ProbabilityPolytopeProjector projector(portfolio, polytope);
    const auto losses = hand_loss_objective();
    const auto baseline =
        projector.project_upper_expected_shortfall(losses, 0.20);

    cf::PortfolioConfig reordered_portfolio = portfolio;
    std::reverse(reordered_portfolio.joint_scenarios.begin(),
        reordered_portfolio.joint_scenarios.end());
    cf::ProbabilityPolytopeConfig reordered_polytope = polytope;
    std::reverse(reordered_polytope.scenario_probabilities.begin(),
        reordered_polytope.scenario_probabilities.end());
    std::reverse(reordered_polytope.events.begin(),
        reordered_polytope.events.end());
    for (auto& event : reordered_polytope.events) {
        std::reverse(event.scenario_ids.begin(), event.scenario_ids.end());
    }
    auto reordered_losses = losses;
    std::reverse(reordered_losses.begin(), reordered_losses.end());
    const auto reordered = cf::ProbabilityPolytopeProjector(
        reordered_portfolio, reordered_polytope)
        .project_upper_expected_shortfall(reordered_losses, 0.20);
    check(baseline.minimum.value == reordered.minimum.value &&
            baseline.central == reordered.central &&
            baseline.maximum.value == reordered.maximum.value &&
            baseline.minimum.tail_mass_weights ==
                reordered.minimum.tail_mass_weights &&
            baseline.maximum.tail_mass_weights ==
                reordered.maximum.tail_mass_weights,
        "tail values and canonical masses are invariant to all caller orders");

    const std::vector<cf::ProbabilityPolytopeScenarioValue> constant{
        {"s00", 7.0}, {"s01", 7.0},
        {"s10", 7.0}, {"s11", 7.0},
    };
    const auto constant_projection =
        projector.project_upper_expected_shortfall(constant, 0.20);
    const std::vector<double> expected_constant_tail{0.08, 0.04, 0.04, 0.04};
    check(near(constant_projection.minimum.value, 7.0) &&
            near(constant_projection.central, 7.0) &&
            near(constant_projection.maximum.value, 7.0) &&
            constant_projection.distinct_thresholds_examined == 1U &&
            std::equal(expected_constant_tail.begin(),
                expected_constant_tail.end(),
                constant_projection.minimum.tail_mass_weights.begin(),
                [](double expected, double actual) {
                    return near(expected, actual);
                }),
        "constant loss uses the central feasible measure and pro-rata exact tie tail");
    check_tail_endpoint(constant_projection, constant_projection.minimum,
        constant, "constant minimum tail remains feasible");
    check_tail_endpoint(constant_projection, constant_projection.maximum,
        constant, "constant maximum tail remains feasible");

    auto signed_losses = losses;
    for (auto& loss : signed_losses) {
        loss.value -= 50.0;
    }
    const auto signed_projection =
        projector.project_upper_expected_shortfall(signed_losses, 0.20);
    check(near(signed_projection.minimum.value, 10.0) &&
            near(signed_projection.central, 50.0) &&
            near(signed_projection.maximum.value, 50.0),
        "finite signed losses preserve upper-tail translation equivariance");

    const std::vector<cf::ProbabilityPolytopeScenarioValue> tied_losses{
        {"s00", 0.0}, {"s01", 10.0},
        {"s10", 10.0}, {"s11", 100.0},
    };
    const auto tied_projection =
        projector.project_upper_expected_shortfall(tied_losses, 0.20);
    const double first_ratio =
        tied_projection.minimum.tail_mass_weights[1] /
        tied_projection.minimum.scenario_weights[1];
    const double second_ratio =
        tied_projection.minimum.tail_mass_weights[2] /
        tied_projection.minimum.scenario_weights[2];
    check(near(tied_projection.minimum.tail_mass_weights[1] +
                  tied_projection.minimum.tail_mass_weights[2],
              0.10) &&
            near(first_ratio, second_ratio),
        "exact equal-loss boundary mass is allocated pro rata within the block");

    const auto expectation = projector.project_expectation(losses);
    const auto entire_distribution =
        projector.project_upper_expected_shortfall(losses, 1.0);
    check(entire_distribution.minimum.value ==
                expectation.expectation.minimum.value &&
            entire_distribution.central == expectation.expectation.central &&
            entire_distribution.maximum.value ==
                expectation.expectation.maximum.value &&
            entire_distribution.minimum.scenario_weights ==
                expectation.expectation.minimum.scenario_weights &&
            entire_distribution.minimum.tail_mass_weights ==
                expectation.expectation.minimum.scenario_weights &&
            entire_distribution.maximum.tail_mass_weights ==
                expectation.expectation.maximum.scenario_weights,
        "tail probability one reuses ordinary expectation values and witnesses");
}

void test_event_free_expected_shortfall_equivalence_and_invalid_tail() {
    const cf::PortfolioConfig portfolio = four_atom_portfolio();
    cf::ProbabilityPolytopeConfig box = four_atom_polytope();
    box.events.clear();
    box.scenario_probabilities = {
        {"s00", 0.10, 0.40, 0.60},
        {"s01", 0.05, 0.20, 0.45},
        {"s10", 0.05, 0.20, 0.40},
        {"s11", 0.10, 0.20, 0.50},
    };
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = box.scenario_label;
    ambiguity.source_note = box.source_note;
    for (const auto& scenario : box.scenario_probabilities) {
        ambiguity.scenario_probabilities.push_back(
            cf::ScenarioProbabilityBounds{scenario.scenario_id,
                scenario.lower_weight, scenario.central_weight,
                scenario.upper_weight});
    }
    const auto losses = hand_loss_objective();
    std::vector<cf::AmbiguityScenarioMetricValue> old_losses;
    for (const auto& loss : losses) {
        old_losses.push_back(
            cf::AmbiguityScenarioMetricValue{loss.scenario_id, loss.value});
    }
    const auto old_projection =
        cf::PortfolioAmbiguityProjector(portfolio, ambiguity)
            .project_upper_expected_shortfall(old_losses, 0.20);
    const cf::ProbabilityPolytopeProjector projector(portfolio, box);
    const auto delegated =
        projector.project_upper_expected_shortfall(losses, 0.20);
    check(delegated.minimum.value ==
                old_projection.upper_expected_shortfall.minimum.value &&
            delegated.central ==
                old_projection.upper_expected_shortfall.central &&
            delegated.maximum.value ==
                old_projection.upper_expected_shortfall.maximum.value &&
            delegated.minimum.scenario_weights ==
                old_projection.upper_expected_shortfall.minimum.scenario_weights &&
            delegated.maximum.scenario_weights ==
                old_projection.upper_expected_shortfall.maximum.scenario_weights,
        "event-free v0.2 preserves exact v0.1 ES values and full witnesses");
    check_tail_endpoint(delegated, delegated.minimum, losses,
        "event-free minimum adds a canonical audited tail witness");
    check_tail_endpoint(delegated, delegated.maximum, losses,
        "event-free maximum adds a canonical audited tail witness");

    const auto tiny_delegated =
        projector.project_upper_expected_shortfall(losses, 1.0e-9);
    check(std::isfinite(tiny_delegated.minimum.value) &&
            std::isfinite(tiny_delegated.maximum.value),
        "event-free v0.1 delegation retains tails below the event solver floor");

    expect_invalid_argument(
        [&] { (void)projector.project_upper_expected_shortfall(losses, 0.0); },
        "zero tail probability is rejected");
    expect_invalid_argument(
        [&] { (void)projector.project_upper_expected_shortfall(losses, 1.01); },
        "tail probability above one is rejected");
    expect_invalid_argument(
        [&] {
            (void)projector.project_upper_expected_shortfall(losses,
                std::numeric_limits<double>::quiet_NaN());
        },
        "non-finite tail probability is rejected");
    const cf::ProbabilityPolytopeProjector event_projector(
        portfolio, four_atom_polytope());
    expect_invalid_argument(
        [&] {
            (void)event_projector.project_upper_expected_shortfall(
                losses, 5.0e-7);
        },
        "event solver rejects tails below its declared 1e-6 floor");
}

void test_invalid_central_taxonomy_and_events() {
    const cf::PortfolioConfig portfolio = four_atom_portfolio();
    cf::ProbabilityPolytopeConfig invalid = four_atom_polytope();

    invalid.synthetic_inputs = false;
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "non-synthetic probability claims are rejected");

    invalid = four_atom_polytope();
    invalid.scenario_probabilities.pop_back();
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "incomplete scenario taxonomy is rejected");

    invalid = four_atom_polytope();
    invalid.scenario_probabilities[1].scenario_id =
        invalid.scenario_probabilities[0].scenario_id;
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "duplicate scenario ids are rejected");

    invalid = four_atom_polytope();
    invalid.scenario_probabilities[0].central_weight += 0.01;
    invalid.scenario_probabilities[1].central_weight -= 0.01;
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "central weights must match portfolio scenarios by id");

    invalid = four_atom_polytope();
    invalid.scenario_probabilities[0].upper_weight =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "non-finite scenario bounds are rejected");

    invalid = four_atom_polytope();
    invalid.events[0].definition.clear();
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "empty event definitions are rejected");

    invalid = four_atom_polytope();
    invalid.events[1].event_id = invalid.events[0].event_id;
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "duplicate event ids are rejected");

    invalid = four_atom_polytope();
    invalid.events[0].scenario_ids.clear();
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "empty event membership is rejected");

    invalid = four_atom_polytope();
    invalid.events[0].scenario_ids.push_back("s11");
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "duplicate membership within one event is rejected");

    invalid = four_atom_polytope();
    invalid.events[0].scenario_ids = {"unknown"};
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "unknown event members are rejected");

    invalid = four_atom_polytope();
    invalid.events[0].scenario_ids = {"s00", "s01", "s10", "s11"};
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "full-taxonomy events are rejected as normalization duplicates");

    invalid = four_atom_polytope();
    invalid.events.push_back(cf::ProbabilityEventConstraint{
        "same-common-shock", "duplicate mathematical event", 0.0, 0.5,
        {"s11"}});
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "duplicate event membership sets are rejected");

    invalid = four_atom_polytope();
    invalid.events[1].lower_probability = 0.90;
    invalid.events[1].upper_probability = 0.95;
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "event slabs must contain the normalized central measure");

    invalid = four_atom_polytope();
    invalid.events[0].lower_probability = 0.6;
    invalid.events[0].upper_probability = 0.5;
    expect_invalid_argument(
        [&] { cf::validate_probability_polytope_config(portfolio, invalid); },
        "reversed event bounds are rejected");

    const cf::ProbabilityPolytopeProjector projector(
        portfolio, four_atom_polytope());
    auto invalid_values = hand_objective();
    invalid_values[0].value =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&] { (void)projector.project_expectation(invalid_values); },
        "non-finite objective values are rejected");
    invalid_values = hand_objective();
    invalid_values[0].scenario_id = invalid_values[1].scenario_id;
    expect_invalid_argument(
        [&] { (void)projector.project_expectation(invalid_values); },
        "duplicate objective scenario ids are rejected");
}

void test_normalized_central_bound_tolerance_alignment() {
    const cf::PortfolioConfig portfolio = make_portfolio({
        {"s0", 0.10}, {"s1", 0.20}, {"s2", 0.30},
        {"s3", 0.40000000000001},
    });
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "normalization tolerance alignment";
    polytope.source_note =
        "synthetic near-unit central-sum regression only";
    polytope.scenario_probabilities = {
        {"s0", 0.10, 0.10, 0.10},
        {"s1", 0.0, 0.20, 1.0},
        {"s2", 0.0, 0.30, 1.0},
        {"s3", 0.0, 0.40000000000001, 1.0},
    };
    polytope.events = {cf::ProbabilityEventConstraint{
        "middle-atoms", "two middle atoms force event-solver validation",
        0.0, 1.0, {"s1", "s2"}}};
    const std::vector<cf::ProbabilityPolytopeScenarioValue> values{
        {"s0", 1.0}, {"s1", 0.0}, {"s2", 0.0}, {"s3", 0.0}};

    const cf::ProbabilityPolytopeMetricProjection projection =
        cf::ProbabilityPolytopeProjector(portfolio, polytope)
            .project_expectation(values);
    check(near(projection.expectation.minimum.value, 0.10) &&
            near(projection.expectation.central, 0.10) &&
            near(projection.expectation.maximum.value, 0.10),
        "core and strict config accept the same normalized-central bound tolerance");

    polytope.events.clear();
    const cf::ProbabilityPolytopeMetricProjection delegated =
        cf::ProbabilityPolytopeProjector(portfolio, polytope)
            .project_expectation(values);
    check(near(delegated.expectation.minimum.value, 0.10) &&
            near(delegated.expectation.central, 0.10) &&
            near(delegated.expectation.maximum.value, 0.10),
        "event-free v0.1 delegation uses the same normalized-central tolerance");
}

void test_event_solver_resource_guard() {
    constexpr std::size_t scenario_count = 513U;
    std::vector<std::pair<std::string, double>> atoms;
    atoms.reserve(scenario_count);
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "resource guard candidate";
    polytope.source_note = "synthetic resource-bound test";
    polytope.scenario_probabilities.reserve(scenario_count);
    const double weight = 1.0 / static_cast<double>(scenario_count);
    for (std::size_t index = 0U; index < scenario_count; ++index) {
        const std::string id = "s" + std::to_string(index);
        atoms.emplace_back(id, weight);
        polytope.scenario_probabilities.push_back(
            cf::ProbabilityPolytopeScenario{id, 0.0, weight, 1.0});
    }
    polytope.events.push_back(cf::ProbabilityEventConstraint{
        "one-atom", "one atom forces the event-constrained solver path",
        0.0, 1.0, {"s0"}});
    const cf::PortfolioConfig portfolio = make_portfolio(atoms);
    expect_runtime_error(
        [&] {
            const cf::ProbabilityPolytopeProjector projector(
                portfolio, polytope);
            (void)projector;
        },
        "event solver rejects scenario dimensions beyond its guarded tableau policy");
}

void test_event_free_ten_thousand_scenario_delegation() {
    constexpr std::size_t scenario_count = 10'000U;
    std::vector<std::pair<std::string, double>> atoms;
    atoms.reserve(scenario_count);
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "ten-thousand-atom box delegation";
    polytope.source_note = "synthetic v0.1 capacity regression";
    polytope.scenario_probabilities.reserve(scenario_count);
    std::vector<cf::ProbabilityPolytopeScenarioValue> values;
    values.reserve(scenario_count);
    const double weight = 1.0 / static_cast<double>(scenario_count);
    for (std::size_t index = 0U; index < scenario_count; ++index) {
        const std::string id = "atom" + std::to_string(index);
        atoms.emplace_back(id, weight);
        polytope.scenario_probabilities.push_back(
            cf::ProbabilityPolytopeScenario{id, 0.0, weight, 1.0});
        values.push_back(cf::ProbabilityPolytopeScenarioValue{id,
            static_cast<double>(index % 7U) - 3.0});
    }
    const cf::PortfolioConfig portfolio = make_portfolio(atoms);
    const auto projection =
        cf::ProbabilityPolytopeProjector(portfolio, polytope)
            .project_expectation(values);
    check(projection.events.empty() &&
            projection.scenario_probabilities.size() == scenario_count &&
            projection.expectation.minimum.scenario_weights.size() ==
                scenario_count &&
            projection.expectation.maximum.scenario_weights.size() ==
                scenario_count &&
            near(projection.expectation.minimum.value, -3.0) &&
            near(projection.expectation.maximum.value, 3.0),
        "event-free v0.2 preserves the v0.1 ten-thousand-scenario path");
}

} // namespace

int main() {
    test_hand_solved_overlapping_events();
    test_input_order_invariance();
    test_degeneracy_constant_and_signed_objectives();
    test_v01_box_and_singleton_event_equivalence();
    test_hand_solved_event_expected_shortfall();
    test_expected_shortfall_order_constants_signed_ties_and_tau_one();
    test_event_free_expected_shortfall_equivalence_and_invalid_tail();
    test_invalid_central_taxonomy_and_events();
    test_normalized_central_bound_tolerance_alignment();
    test_event_solver_resource_guard();
    test_event_free_ten_thousand_scenario_delegation();

    if (failures != 0) {
        std::cerr << failures << " probability polytope test(s) failed\n";
        return 1;
    }
    std::cout << "all probability polytope tests passed\n";
    return 0;
}
