// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
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

[[nodiscard]] cf::ProjectJointPath make_path(std::string project_id,
    cf::ProjectPathResolution resolution, double draw, double receipt,
    double principal, std::string source_id = "unused") {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = resolution;
    if (draw > 0.0) {
        path.capital_draws.push_back(cf::MonthlyAmount{0U, draw});
    }
    if (receipt > 0.0) {
        path.investor_receipts.push_back(cf::InvestorReceipt{
            12U, std::move(source_id), receipt, principal});
    }
    return path;
}

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, double amount) {
    cf::ScenarioCashSource source;
    source.id = std::move(id);
    source.kind = kind;
    if (amount > 0.0) {
        source.cash_available.push_back(cf::MonthlyAmount{12U, amount});
    }
    return source;
}

[[nodiscard]] cf::PortfolioConfig three_state_config() {
    cf::PortfolioConfig config;
    config.scenario_label = "three-state ambiguity hand table";
    config.source_note = "synthetic ambiguity unit-test values only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 10.0}};

    cf::JointScenario good;
    good.id = "good";
    good.weight = 0.50;
    good.project_paths = {make_path("project",
        cf::ProjectPathResolution::Resolved, 10.0, 12.0, 10.0,
        "commercial-budget")};
    good.cash_sources = {make_source("commercial-budget",
        cf::PortfolioCashSource::Commercial, 12.0)};
    good.pool_costs = {cf::MonthlyAmount{0U, 1.0}};

    cf::JointScenario bad;
    bad.id = "bad";
    bad.weight = 0.30;
    bad.project_paths = {make_path("project",
        cf::ProjectPathResolution::Resolved, 10.0, 2.0, 2.0,
        "recovery-budget")};
    bad.cash_sources = {make_source("recovery-budget",
        cf::PortfolioCashSource::Recovery, 2.0)};
    bad.pool_costs = {cf::MonthlyAmount{0U, 2.0}};

    cf::JointScenario continuing;
    continuing.id = "continuing";
    continuing.weight = 0.20;
    continuing.project_paths = {make_path("project",
        cf::ProjectPathResolution::Continuing, 5.0, 0.0, 0.0)};

    // Deliberately not identifier-sorted: result ordering must not depend on it.
    config.joint_scenarios = {good, bad, continuing};
    return config;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig three_state_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "three-state interval probability set";
    ambiguity.source_note = "synthetic ambiguity unit-test bounds only";
    ambiguity.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{"continuing", 0.10, 0.20, 0.40},
        cf::ScenarioProbabilityBounds{"good", 0.20, 0.50, 0.70},
        cf::ScenarioProbabilityBounds{"bad", 0.10, 0.30, 0.60},
    };
    return ambiguity;
}

[[nodiscard]] const cf::AmbiguityReturnSourceTotal& source_range(
    const cf::PortfolioAmbiguitySummary& summary,
    cf::PortfolioCashSource source) {
    return summary.expected_return_sources.at(
        static_cast<std::size_t>(source));
}

void check_witness(const cf::AmbiguityEndpoint& endpoint,
    const std::vector<double>& scenario_values,
    const cf::PortfolioAmbiguitySummary& summary,
    std::string_view message) {
    check(endpoint.scenario_weights.size() == scenario_values.size() &&
            scenario_values.size() ==
                summary.scenario_probability_bounds.size(),
        message);
    if (endpoint.scenario_weights.size() != scenario_values.size() ||
        scenario_values.size() !=
            summary.scenario_probability_bounds.size()) {
        return;
    }
    double weight_sum = 0.0;
    double objective = 0.0;
    bool inside = true;
    for (std::size_t index = 0U; index < scenario_values.size(); ++index) {
        const double weight = endpoint.scenario_weights[index];
        const cf::ScenarioProbabilityBounds& bounds =
            summary.scenario_probability_bounds[index];
        weight_sum += weight;
        objective += weight * scenario_values[index];
        inside = inside && weight >= bounds.lower_weight - 1.0e-12 &&
            weight <= bounds.upper_weight + 1.0e-12;
    }
    check(near(weight_sum, 1.0) && inside &&
            near(objective, endpoint.value),
        message);
}

[[nodiscard]] double reconstruct_upper_expected_shortfall(
    const cf::AmbiguityUpperExpectedShortfallProjection& projection,
    const cf::AmbiguityEndpoint& endpoint,
    const std::vector<cf::AmbiguityScenarioMetricValue>& keyed_values) {
    if (projection.scenario_probability_bounds.size() !=
            endpoint.scenario_weights.size() ||
        endpoint.scenario_weights.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    struct TailAtom {
        std::string scenario_id{};
        double value{0.0};
        double weight{0.0};
    };
    std::vector<TailAtom> atoms;
    atoms.reserve(endpoint.scenario_weights.size());
    double total_probability = 0.0;
    for (std::size_t index = 0U;
         index < projection.scenario_probability_bounds.size(); ++index) {
        const std::string& scenario_id =
            projection.scenario_probability_bounds[index].scenario_id;
        const auto matching = std::find_if(keyed_values.begin(),
            keyed_values.end(), [&scenario_id](const auto& keyed) {
                return keyed.scenario_id == scenario_id;
            });
        if (matching == keyed_values.end()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        const double weight = endpoint.scenario_weights[index];
        atoms.push_back(TailAtom{scenario_id, matching->value, weight});
        total_probability += weight;
    }
    std::sort(atoms.begin(), atoms.end(),
        [](const TailAtom& first, const TailAtom& second) {
            if (first.value != second.value) {
                return first.value > second.value;
            }
            return first.scenario_id < second.scenario_id;
        });

    const double requested =
        projection.tail_probability * total_probability;
    double remaining = requested;
    double total = 0.0;
    for (const TailAtom& atom : atoms) {
        const double included = std::min(remaining, atom.weight);
        total += included * atom.value;
        remaining -= included;
        if (remaining <= 0.0) {
            break;
        }
    }
    if (!(requested > 0.0) || remaining > 1.0e-12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return total / requested;
}

void test_exact_linear_envelopes_and_witnesses() {
    const cf::PortfolioConfig config = three_state_config();
    const cf::PortfolioAmbiguitySummary summary =
        cf::evaluate_portfolio_ambiguity(config, three_state_ambiguity());

    check(summary.scenario_probability_bounds.size() == 3U &&
            summary.scenario_probability_bounds[0].scenario_id == "bad" &&
            summary.scenario_probability_bounds[1].scenario_id ==
                "continuing" &&
            summary.scenario_probability_bounds[2].scenario_id == "good",
        "ambiguity scenarios and witnesses use deterministic identifier order");
    check(near(summary.lower_bound_sum, 0.40) &&
            near(summary.configured_central_weight_sum, 1.0) &&
            near(summary.upper_bound_sum, 1.70),
        "published lower, central, and upper sums retain their distinct meanings");

    check(near(summary.expected_total_draws_million.minimum.value, 8.0) &&
            near(summary.expected_total_draws_million.central, 9.0) &&
            near(summary.expected_total_draws_million.maximum.value, 9.5),
        "expected funding has exact capped-simplex endpoints");
    check(near(summary.expected_total_receipts_million.minimum.value, 3.2) &&
            near(summary.expected_total_receipts_million.central, 6.6) &&
            near(summary.expected_total_receipts_million.maximum.value, 8.8),
        "expected total receipts are bounded under one coherent probability witness");
    check(near(summary.expected_total_pool_costs_million.minimum.value, 0.7) &&
            near(summary.expected_total_pool_costs_million.central, 1.1) &&
            near(summary.expected_total_pool_costs_million.maximum.value, 1.5),
        "expected pool costs have exact capped-simplex endpoints");
    check(near(summary.expected_outstanding_principal_million.minimum.value,
              0.5) &&
            near(summary.expected_outstanding_principal_million.central, 1.0) &&
            near(summary.expected_outstanding_principal_million.maximum.value,
              2.0),
        "continuing exposure is bounded without relabeling it as loss");
    check(near(summary.expected_principal_loss_million.minimum.value, 0.8) &&
            near(summary.expected_principal_loss_million.central, 2.4) &&
            near(summary.expected_principal_loss_million.maximum.value, 4.8),
        "expected principal loss has exact capped-simplex endpoints");
    check(near(summary.expected_npv_million.minimum.value, -6.8) &&
            near(summary.expected_npv_million.central, -3.5) &&
            near(summary.expected_npv_million.maximum.value, -1.3),
        "NPV minimum and maximum correctly reverse economic worst and best");
    check(near(summary.principal_impairment_probability.minimum.value, 0.10) &&
            near(summary.principal_impairment_probability.central, 0.30) &&
            near(summary.principal_impairment_probability.maximum.value, 0.60) &&
            near(summary.negative_npv_probability.minimum.value, 0.30) &&
            near(summary.negative_npv_probability.central, 0.50) &&
            near(summary.negative_npv_probability.maximum.value, 0.80),
        "event-probability envelopes match exact indicator optimization");

    check(summary.projects.size() == 1U &&
            summary.projects[0].project_id == "project",
        "ambiguity output retains the complete project taxonomy");
    const cf::ProjectAmbiguitySummary& project = summary.projects[0];
    check(near(project.expected_total_draws_million.minimum.value, 8.0) &&
            near(project.expected_total_draws_million.central, 9.0) &&
            near(project.expected_total_draws_million.maximum.value, 9.5) &&
            near(project.expected_total_receipts_million.minimum.value, 3.2) &&
            near(project.expected_total_receipts_million.central, 6.6) &&
            near(project.expected_total_receipts_million.maximum.value, 8.8),
        "project draw and receipt ranges are exact linear projections");
    check(near(project.expected_outstanding_principal_million.minimum.value,
              0.5) &&
            near(project.expected_outstanding_principal_million.central, 1.0) &&
            near(project.expected_outstanding_principal_million.maximum.value,
              2.0) &&
            near(project.expected_realized_principal_loss_million.minimum.value,
              0.8) &&
            near(project.expected_realized_principal_loss_million.central,
              2.4) &&
            near(project.expected_realized_principal_loss_million.maximum.value,
              4.8),
        "project continuing exposure and realized loss remain distinct exact ranges");
    check(near(project.expected_npv_before_pool_costs_million.minimum.value,
              -5.4) &&
            near(project.expected_npv_before_pool_costs_million.central,
              -2.4) &&
            near(project.expected_npv_before_pool_costs_million.maximum.value,
              -0.4) &&
            near(project.principal_impairment_probability.minimum.value, 0.10) &&
            near(project.principal_impairment_probability.central, 0.30) &&
            near(project.principal_impairment_probability.maximum.value, 0.60) &&
            near(project.negative_npv_probability.minimum.value, 0.30) &&
            near(project.negative_npv_probability.central, 0.50) &&
            near(project.negative_npv_probability.maximum.value, 0.80),
        "project pre-cost NPV and event-probability ranges retain their exact meanings");
    check(project.expected_realized_principal_loss_million.maximum
              .scenario_weights !=
            project.expected_total_receipts_million.maximum.scenario_weights,
        "project extrema retain metric-specific witnesses rather than a fictitious common endpoint");
    std::vector<double> project_receipts;
    std::vector<double> project_losses;
    std::vector<double> project_npvs;
    for (const cf::JointScenarioResult& scenario :
         summary.central_portfolio.scenarios) {
        project_receipts.push_back(
            scenario.projects[0].total_receipts_million);
        project_losses.push_back(
            scenario.projects[0].principal_loss_million);
        project_npvs.push_back(
            scenario.projects[0].npv_before_pool_costs_million);
    }
    check_witness(project.expected_total_receipts_million.maximum,
        project_receipts, summary,
        "project maximum receipt witness is feasible and reconstructs");
    check_witness(project.expected_realized_principal_loss_million.maximum,
        project_losses, summary,
        "project maximum realized-loss witness is feasible and reconstructs");
    check_witness(project.expected_npv_before_pool_costs_million.minimum,
        project_npvs, summary,
        "project minimum pre-cost NPV witness is feasible and reconstructs");
    check(near(summary.expected_peak_same_month_draw_million.minimum.value,
              8.0) &&
            near(summary.expected_peak_same_month_funding_need_million
                     .maximum.value,
              11.0) &&
            near(summary.expected_peak_cumulative_net_outlay_million.central,
              10.1),
        "all three existing pathwise peak-liquidity definitions are bounded");

    const cf::AmbiguityReturnSourceTotal& commercial = source_range(
        summary, cf::PortfolioCashSource::Commercial);
    const cf::AmbiguityReturnSourceTotal& recovery = source_range(
        summary, cf::PortfolioCashSource::Recovery);
    check(near(commercial.nominal_million.minimum.value, 2.4) &&
            near(commercial.nominal_million.central, 6.0) &&
            near(commercial.nominal_million.maximum.value, 8.4) &&
            near(recovery.nominal_million.minimum.value, 0.2) &&
            near(recovery.nominal_million.central, 0.6) &&
            near(recovery.nominal_million.maximum.value, 1.2),
        "cash-source totals have exact componentwise nominal envelopes");
    check(commercial.nominal_million.maximum.scenario_weights !=
            recovery.nominal_million.maximum.scenario_weights,
        "source maxima disclose different witnesses rather than a fictitious simultaneous source mix");
    check(commercial.nominal_million.maximum.value +
                recovery.nominal_million.maximum.value >
            summary.expected_total_receipts_million.maximum.value,
        "componentwise source maxima are not misreported as a feasible total-receipts maximum");

    std::vector<double> losses;
    std::vector<double> npvs;
    std::vector<double> receipts;
    for (const cf::JointScenarioResult& scenario :
         summary.central_portfolio.scenarios) {
        losses.push_back(scenario.principal_loss_million);
        npvs.push_back(scenario.npv_million);
        receipts.push_back(scenario.total_receipts_million);
    }
    check_witness(summary.expected_principal_loss_million.minimum,
        losses, summary, "minimum loss witness is feasible and reconstructs");
    check_witness(summary.expected_principal_loss_million.maximum,
        losses, summary, "maximum loss witness is feasible and reconstructs");
    check_witness(summary.expected_npv_million.minimum,
        npvs, summary, "minimum NPV witness is feasible and reconstructs");
    check_witness(summary.expected_npv_million.maximum,
        npvs, summary, "maximum NPV witness is feasible and reconstructs");
    check_witness(summary.expected_total_receipts_million.maximum,
        receipts, summary,
        "maximum total-receipts witness is feasible and reconstructs");
    check(summary.maximum_endpoint_probability_error <= 1.0e-12 &&
            summary.maximum_central_metric_reconciliation_error <= 1.0e-9,
        "published ambiguity witnesses and central metrics reconcile tightly");

    const cf::PortfolioSummary ordinary = cf::evaluate_portfolio(config);
    check(ordinary.scenarios.size() == summary.central_portfolio.scenarios.size() &&
            ordinary.scenarios[0].monthly_cash_flows[0].net_cash_flow_million ==
                summary.central_portfolio.scenarios[0]
                    .monthly_cash_flows[0].net_cash_flow_million &&
            ordinary.scenarios[2].total_receipts_million ==
                summary.central_portfolio.scenarios[2]
                    .total_receipts_million,
        "ambiguity evaluation preserves the ordinary project cash paths exactly");
}

void test_prepared_keyed_expectation_projection() {
    const cf::PortfolioConfig config = three_state_config();
    const cf::PortfolioAmbiguityConfig ambiguity = three_state_ambiguity();
    const cf::PortfolioAmbiguitySummary summary =
        cf::evaluate_portfolio_ambiguity(config, ambiguity);
    const cf::PortfolioAmbiguityProjector projector(config, ambiguity);

    std::vector<cf::AmbiguityScenarioMetricValue> npvs;
    std::vector<cf::AmbiguityScenarioMetricValue> receipts;
    for (auto scenario = summary.central_portfolio.scenarios.rbegin();
         scenario != summary.central_portfolio.scenarios.rend(); ++scenario) {
        npvs.push_back(cf::AmbiguityScenarioMetricValue{
            scenario->scenario_id, scenario->npv_million});
        receipts.push_back(cf::AmbiguityScenarioMetricValue{
            scenario->scenario_id, scenario->total_receipts_million});
    }

    const cf::AmbiguityMetricProjection npv_projection =
        projector.project_expectation(npvs);
    check(npv_projection.scenario_probability_bounds.size() == 3U &&
            npv_projection.scenario_probability_bounds[0].scenario_id ==
                "bad" &&
            npv_projection.scenario_probability_bounds[1].scenario_id ==
                "continuing" &&
            npv_projection.scenario_probability_bounds[2].scenario_id ==
                "good",
        "keyed projection returns its canonical scenario and witness order");
    check(near(npv_projection.expectation.minimum.value,
              summary.expected_npv_million.minimum.value) &&
            near(npv_projection.expectation.central,
              summary.expected_npv_million.central) &&
            near(npv_projection.expectation.maximum.value,
              summary.expected_npv_million.maximum.value) &&
            npv_projection.expectation.minimum.scenario_weights ==
                summary.expected_npv_million.minimum.scenario_weights &&
            npv_projection.expectation.maximum.scenario_weights ==
                summary.expected_npv_million.maximum.scenario_weights,
        "generic keyed NPV projection reuses the ordinary ambiguity optimizer exactly");

    std::vector<double> ordered_npvs;
    for (const cf::ScenarioProbabilityBounds& bounds :
         npv_projection.scenario_probability_bounds) {
        const auto matching = std::find_if(npvs.begin(), npvs.end(),
            [&bounds](const cf::AmbiguityScenarioMetricValue& value) {
                return value.scenario_id == bounds.scenario_id;
            });
        ordered_npvs.push_back(matching->value);
    }
    double minimum_mass = 0.0;
    double minimum_value = 0.0;
    bool minimum_inside = true;
    for (std::size_t index = 0U; index < ordered_npvs.size(); ++index) {
        const double weight =
            npv_projection.expectation.minimum.scenario_weights[index];
        const cf::ScenarioProbabilityBounds& bounds =
            npv_projection.scenario_probability_bounds[index];
        minimum_mass += weight;
        minimum_value += weight * ordered_npvs[index];
        minimum_inside = minimum_inside &&
            weight >= bounds.lower_weight - 1.0e-12 &&
            weight <= bounds.upper_weight + 1.0e-12;
    }
    check(near(minimum_mass, 1.0) && minimum_inside &&
            near(minimum_value,
              npv_projection.expectation.minimum.value) &&
            npv_projection.maximum_endpoint_probability_error <= 1.0e-12,
        "generic projection minimum publishes a reconstructible feasible witness");

    const cf::AmbiguityMetricProjection receipt_projection =
        projector.project_expectation(receipts);
    check(near(receipt_projection.expectation.minimum.value,
              summary.expected_total_receipts_million.minimum.value) &&
            near(receipt_projection.expectation.central,
              summary.expected_total_receipts_million.central) &&
            near(receipt_projection.expectation.maximum.value,
              summary.expected_total_receipts_million.maximum.value),
        "one prepared projector safely supports multiple keyed objectives");
}

void test_keyed_projection_validation() {
    const cf::PortfolioConfig config = three_state_config();
    const cf::PortfolioAmbiguityProjector projector(
        config, three_state_ambiguity());
    const std::vector<cf::AmbiguityScenarioMetricValue> valid = {
        {"good", 2.0}, {"bad", -8.0}, {"continuing", -5.0}};
    (void)projector.project_expectation(valid);

    std::vector<cf::AmbiguityScenarioMetricValue> invalid = valid;
    invalid.pop_back();
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_expectation(invalid);
        },
        "keyed projection rejects a missing scenario value");

    invalid = valid;
    invalid[1].scenario_id = "good";
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_expectation(invalid);
        },
        "keyed projection rejects duplicate scenario ids");

    invalid = valid;
    invalid[1].scenario_id = "unknown";
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_expectation(invalid);
        },
        "keyed projection rejects a missing-plus-unknown id substitution");

    invalid = valid;
    invalid[1].scenario_id = "bad id";
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_expectation(invalid);
        },
        "keyed projection rejects unsafe scenario ids");

    invalid = valid;
    invalid[1].value = std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_expectation(invalid);
        },
        "keyed projection rejects non-finite scenario values");

    invalid = valid;
    invalid[1].value = std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_expectation(invalid);
        },
        "keyed projection rejects infinite scenario values");
}

void test_keyed_projection_preserves_ten_thousand_scenario_bound() {
    constexpr std::size_t scenario_count = 10'000U;
    const double central_weight =
        1.0 / static_cast<double>(scenario_count);

    cf::PortfolioConfig config;
    config.scenario_label = "ten-thousand-state projection resource boundary";
    config.source_note = "synthetic maximum-row projection test only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 1U;
    config.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Research, 1.0}};
    config.joint_scenarios.reserve(scenario_count);

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label =
        "ten-thousand-state interval probability resource boundary";
    ambiguity.source_note = "synthetic maximum-row bounds only";
    ambiguity.scenario_probabilities.reserve(scenario_count);

    std::vector<cf::AmbiguityScenarioMetricValue> values;
    values.reserve(scenario_count);
    for (std::size_t index = 0U; index < scenario_count; ++index) {
        const std::string id = "scenario-" + std::to_string(index);
        cf::JointScenario scenario;
        scenario.id = id;
        scenario.weight = central_weight;
        scenario.project_paths = {make_path("project",
            cf::ProjectPathResolution::Continuing, 0.0, 0.0, 0.0)};
        config.joint_scenarios.push_back(std::move(scenario));
        ambiguity.scenario_probabilities.push_back(
            cf::ScenarioProbabilityBounds{
                id, 0.0, central_weight, 1.0});
        values.push_back(cf::AmbiguityScenarioMetricValue{
            id, static_cast<double>(index)});
    }

    const cf::PortfolioAmbiguityProjector projector(config, ambiguity);
    const cf::AmbiguityMetricProjection projection =
        projector.project_expectation(values);
    check(projection.scenario_probability_bounds.size() == scenario_count &&
            projection.expectation.minimum.scenario_weights.size() ==
                scenario_count &&
            projection.expectation.maximum.scenario_weights.size() ==
                scenario_count &&
            near(projection.expectation.minimum.value, 0.0) &&
            near(projection.expectation.central, 4'999.5) &&
            near(projection.expectation.maximum.value, 9'999.0),
        "prepared keyed projection preserves the validated 10,000-scenario resource boundary");
}

[[nodiscard]] cf::PortfolioConfig binary_tail_config(double loss_weight) {
    cf::PortfolioConfig config;
    config.scenario_label = "binary expected-shortfall boundary";
    config.source_note = "synthetic fractional-tail test values only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.projects = {cf::PortfolioProject{
        "note", cf::ProjectStage::Pilot, 10.0}};

    cf::JointScenario no_loss;
    no_loss.id = "no-loss";
    no_loss.weight = 1.0 - loss_weight;
    no_loss.project_paths = {make_path("note",
        cf::ProjectPathResolution::Resolved, 10.0, 10.0, 10.0,
        "commercial-budget")};
    no_loss.cash_sources = {make_source("commercial-budget",
        cf::PortfolioCashSource::Commercial, 10.0)};

    cf::JointScenario loss;
    loss.id = "loss";
    loss.weight = loss_weight;
    loss.project_paths = {make_path("note",
        cf::ProjectPathResolution::Resolved, 10.0, 0.0, 0.0)};
    config.joint_scenarios = {no_loss, loss};
    return config;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig binary_tail_ambiguity(
    double lower_loss, double central_loss, double upper_loss) {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "binary expected-shortfall ambiguity set";
    ambiguity.source_note = "synthetic fractional-tail bounds only";
    ambiguity.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{
            "no-loss", 1.0 - upper_loss, 1.0 - central_loss,
            1.0 - lower_loss},
        cf::ScenarioProbabilityBounds{
            "loss", lower_loss, central_loss, upper_loss},
    };
    return ambiguity;
}

[[nodiscard]] cf::PortfolioConfig tied_project_tail_config(
    std::string first_loss_id, std::string second_loss_id,
    bool reverse_input_order) {
    cf::PortfolioConfig config;
    config.scenario_label = "two-project tied pool-loss tail";
    config.source_note = "synthetic common-witness attribution test only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.projects = {
        cf::PortfolioProject{"project-a", cf::ProjectStage::Research, 10.0},
        cf::PortfolioProject{"project-b", cf::ProjectStage::Pilot, 10.0},
    };

    const auto make_loss_state = [](std::string id, double weight,
                                     bool first_loses) {
        cf::JointScenario scenario;
        scenario.id = std::move(id);
        scenario.weight = weight;
        scenario.project_paths = first_loses
            ? std::vector<cf::ProjectJointPath>{
                  make_path("project-a", cf::ProjectPathResolution::Resolved,
                      10.0, 0.0, 0.0),
                  make_path("project-b", cf::ProjectPathResolution::Resolved,
                      10.0, 10.0, 10.0, "performing-principal")}
            : std::vector<cf::ProjectJointPath>{
                  make_path("project-a", cf::ProjectPathResolution::Resolved,
                      10.0, 10.0, 10.0, "performing-principal"),
                  make_path("project-b", cf::ProjectPathResolution::Resolved,
                      10.0, 0.0, 0.0)};
        scenario.cash_sources = {make_source("performing-principal",
            cf::PortfolioCashSource::Commercial, 10.0)};
        return scenario;
    };

    cf::JointScenario first_loss =
        make_loss_state(std::move(first_loss_id), 0.02, true);
    cf::JointScenario second_loss =
        make_loss_state(std::move(second_loss_id), 0.18, false);
    cf::JointScenario safe;
    safe.id = "safe";
    safe.weight = 0.80;
    safe.project_paths = {
        make_path("project-a", cf::ProjectPathResolution::Resolved,
            10.0, 10.0, 10.0, "safe-principal"),
        make_path("project-b", cf::ProjectPathResolution::Resolved,
            10.0, 10.0, 10.0, "safe-principal"),
    };
    safe.cash_sources = {make_source("safe-principal",
        cf::PortfolioCashSource::Commercial, 20.0)};
    config.joint_scenarios = reverse_input_order
        ? std::vector<cf::JointScenario>{safe, second_loss, first_loss}
        : std::vector<cf::JointScenario>{first_loss, second_loss, safe};
    return config;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig tied_project_tail_ambiguity(
    std::string first_loss_id, std::string second_loss_id) {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "two-project tied pool-loss ambiguity";
    ambiguity.source_note = "synthetic common-witness attribution bounds only";
    ambiguity.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{
            std::move(first_loss_id), 0.01, 0.02, 0.10},
        cf::ScenarioProbabilityBounds{
            std::move(second_loss_id), 0.10, 0.18, 0.25},
        cf::ScenarioProbabilityBounds{"safe", 0.50, 0.80, 0.89},
    };
    return ambiguity;
}

void test_common_witness_project_tail_attribution_and_tie_invariance() {
    cf::PortfolioConfig symmetric =
        tied_project_tail_config("first-loss", "second-loss", true);
    for (cf::JointScenario& scenario : symmetric.joint_scenarios) {
        scenario.weight = scenario.id == "safe" ? 0.90 : 0.05;
    }
    cf::PortfolioAmbiguityConfig symmetric_measure;
    symmetric_measure.scenario_label = "fixed symmetric tied-tail measure";
    symmetric_measure.source_note = "synthetic symmetric attribution only";
    symmetric_measure.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{"first-loss", 0.05, 0.05, 0.05},
        cf::ScenarioProbabilityBounds{"second-loss", 0.05, 0.05, 0.05},
        cf::ScenarioProbabilityBounds{"safe", 0.90, 0.90, 0.90},
    };
    const cf::PortfolioAmbiguitySummary symmetric_summary =
        cf::evaluate_portfolio_ambiguity(symmetric, symmetric_measure);
    const cf::PoolLossTailAttribution& symmetric_tail =
        symmetric_summary.principal_loss_tail_attribution_95;
    check(symmetric_tail.projects.size() == 2U &&
            near(symmetric_tail.projects[0].at_central_measure_million, 5.0) &&
            near(symmetric_tail.projects[1].at_central_measure_million, 5.0) &&
            near(symmetric_tail.projects[0]
                     .at_minimum_pool_es_measure_million,
              5.0) &&
            near(symmetric_tail.projects[1]
                     .at_maximum_pool_es_measure_million,
              5.0),
        "a symmetric tied five-percent pool tail attributes five million to each project");

    const cf::PortfolioAmbiguitySummary ordinary_names =
        cf::evaluate_portfolio_ambiguity(
            tied_project_tail_config("first-loss", "second-loss", false),
            tied_project_tail_ambiguity("first-loss", "second-loss"));
    const cf::PoolLossTailAttribution& tail =
        ordinary_names.principal_loss_tail_attribution_95;
    check(tail.projects.size() == 2U &&
            near(ordinary_names.principal_loss_expected_shortfall_95_million
                     .minimum.value,
              10.0) &&
            near(ordinary_names.principal_loss_expected_shortfall_95_million
                     .central,
              10.0) &&
            near(ordinary_names.principal_loss_expected_shortfall_95_million
                     .maximum.value,
              10.0),
        "tied aggregate-loss states preserve the exact pool ES endpoint");
    check(near(tail.projects[0].at_minimum_pool_es_measure_million,
              10.0 / 11.0) &&
            near(tail.projects[1].at_minimum_pool_es_measure_million,
              100.0 / 11.0) &&
            near(tail.projects[0].at_central_measure_million, 1.0) &&
            near(tail.projects[1].at_central_measure_million, 9.0) &&
            near(tail.projects[0].at_maximum_pool_es_measure_million,
              20.0 / 7.0) &&
            near(tail.projects[1].at_maximum_pool_es_measure_million,
              50.0 / 7.0),
        "fractional tied tails allocate each common measure pro rata and add to pool ES");
    check(near(tail.projects[0].at_central_measure_million,
              ordinary_names.central_portfolio.projects[0]
                  .pool_loss_tail_contribution_es95_million) &&
            near(tail.projects[1].at_central_measure_million,
              ordinary_names.central_portfolio.projects[1]
                  .pool_loss_tail_contribution_es95_million),
        "ambiguity central tail attribution reproduces the ordinary engine exactly");

    const cf::PortfolioAmbiguitySummary renamed =
        cf::evaluate_portfolio_ambiguity(
            tied_project_tail_config("z-first-shock", "a-second-shock", true),
            tied_project_tail_ambiguity(
                "z-first-shock", "a-second-shock"));
    const cf::PoolLossTailAttribution& renamed_tail =
        renamed.principal_loss_tail_attribution_95;
    check(renamed_tail.projects.size() == tail.projects.size() &&
            near(renamed_tail.projects[0]
                     .at_minimum_pool_es_measure_million,
              tail.projects[0].at_minimum_pool_es_measure_million) &&
            near(renamed_tail.projects[0]
                     .at_maximum_pool_es_measure_million,
              tail.projects[0].at_maximum_pool_es_measure_million) &&
            near(renamed_tail.projects[1]
                     .at_minimum_pool_es_measure_million,
              tail.projects[1].at_minimum_pool_es_measure_million) &&
            near(renamed_tail.projects[1]
                     .at_maximum_pool_es_measure_million,
              tail.projects[1].at_maximum_pool_es_measure_million),
        "equal-loss capacity blocks and fractional tail ties are invariant to scenario names and input order");
    check(tail.maximum_tail_mass_reconciliation_error <= 1.0e-12 &&
            tail.maximum_project_contribution_reconciliation_error_million <=
                1.0e-9 &&
            renamed_tail.maximum_tail_mass_reconciliation_error <= 1.0e-12 &&
            renamed_tail
                    .maximum_project_contribution_reconciliation_error_million <=
                1.0e-9,
        "all common-witness tail masses and project allocations reconcile tightly");
}

void test_exact_expected_shortfall_bounds() {
    const cf::PortfolioAmbiguitySummary es95 =
        cf::evaluate_portfolio_ambiguity(binary_tail_config(0.05),
            binary_tail_ambiguity(0.02, 0.05, 0.08));
    check(near(es95.principal_loss_expected_shortfall_95_million
                   .minimum.value,
              4.0) &&
            near(es95.principal_loss_expected_shortfall_95_million.central,
              10.0) &&
            near(es95.principal_loss_expected_shortfall_95_million
                   .maximum.value,
              10.0) &&
            near(es95.principal_loss_expected_shortfall_99_million
                   .minimum.value,
              10.0) &&
            near(es95.principal_loss_expected_shortfall_99_million
                   .maximum.value,
              10.0),
        "FOSD witnesses give exact fractional-atom ES95 and ES99 bounds");
    check(near(es95.npv_shortfall_expected_shortfall_95_million
                   .minimum.value,
              4.0) &&
            near(es95.npv_shortfall_expected_shortfall_95_million
                   .maximum.value,
              10.0),
        "NPV downside ES is bounded on non-negative shortfall");
    check(es95.expected_total_draws_million.minimum.scenario_weights ==
            es95.expected_total_draws_million.maximum.scenario_weights,
        "a nondegenerate ambiguity set uses one canonical pro-rata witness for a tied objective");
    check(near(es95.principal_loss_expected_shortfall_95_million
                   .minimum.scenario_weights[0],
              0.02) &&
            near(es95.principal_loss_expected_shortfall_95_million
                   .maximum.scenario_weights[0],
              0.08),
        "ES endpoints publish their loss-probability witnesses in sorted ID order");
    const cf::PoolLossTailAttribution& attribution95 =
        es95.principal_loss_tail_attribution_95;
    check(near(attribution95.tail_probability, 0.05) &&
            attribution95.projects.size() == 1U &&
            near(attribution95.projects[0]
                     .at_minimum_pool_es_measure_million,
              4.0) &&
            near(attribution95.projects[0].at_central_measure_million, 10.0) &&
            near(attribution95.projects[0]
                     .at_maximum_pool_es_measure_million,
              10.0),
        "one-project common-witness tail attribution equals pool ES at every measure");
    double minimum_tail_mass = 0.0;
    double central_tail_mass = 0.0;
    double maximum_tail_mass = 0.0;
    for (const double value :
         attribution95.minimum_pool_es_tail_mass_weights) {
        minimum_tail_mass += value;
    }
    for (const double value : attribution95.central_tail_mass_weights) {
        central_tail_mass += value;
    }
    for (const double value :
         attribution95.maximum_pool_es_tail_mass_weights) {
        maximum_tail_mass += value;
    }
    check(near(minimum_tail_mass, 0.05) &&
            near(central_tail_mass, 0.05) &&
            near(maximum_tail_mass, 0.05) &&
            attribution95.maximum_tail_mass_reconciliation_error <= 1.0e-12 &&
            attribution95
                    .maximum_project_contribution_reconciliation_error_million <=
                1.0e-9,
        "published common-witness tail masses and project contributions reconcile");

    const cf::PortfolioAmbiguitySummary es99 =
        cf::evaluate_portfolio_ambiguity(binary_tail_config(0.01),
            binary_tail_ambiguity(0.002, 0.01, 0.02));
    check(near(es99.principal_loss_expected_shortfall_99_million
                   .minimum.value,
              2.0) &&
            near(es99.principal_loss_expected_shortfall_99_million.central,
              10.0) &&
            near(es99.principal_loss_expected_shortfall_99_million
                   .maximum.value,
              10.0) &&
            near(es99.principal_loss_expected_shortfall_95_million
                   .minimum.value,
              0.4) &&
            near(es99.principal_loss_expected_shortfall_95_million
                   .maximum.value,
              4.0),
        "one-percent tails consume exact fractional scenario mass");
}

[[nodiscard]] cf::PortfolioConfig three_level_tail_config() {
    cf::PortfolioConfig config;
    config.scenario_label = "three-level expected-shortfall boundary";
    config.source_note = "synthetic multi-atom tail values only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.projects = {cf::PortfolioProject{
        "note", cf::ProjectStage::Pilot, 100.0}};

    cf::JointScenario zero;
    zero.id = "zero-loss";
    zero.weight = 0.92;
    zero.project_paths = {make_path("note",
        cf::ProjectPathResolution::Resolved, 100.0, 100.0, 100.0,
        "commercial-budget")};
    zero.cash_sources = {make_source("commercial-budget",
        cf::PortfolioCashSource::Commercial, 100.0)};

    cf::JointScenario ten;
    ten.id = "ten-loss";
    ten.weight = 0.03;
    ten.project_paths = {make_path("note",
        cf::ProjectPathResolution::Resolved, 100.0, 90.0, 90.0,
        "recovery-budget")};
    ten.cash_sources = {make_source("recovery-budget",
        cf::PortfolioCashSource::Recovery, 90.0)};

    cf::JointScenario hundred;
    hundred.id = "hundred-loss";
    hundred.weight = 0.05;
    hundred.project_paths = {make_path("note",
        cf::ProjectPathResolution::Resolved, 100.0, 0.0, 0.0)};
    config.joint_scenarios = {ten, hundred, zero};
    return config;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig three_level_tail_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "three-level expected-shortfall ambiguity set";
    ambiguity.source_note = "synthetic multi-atom tail bounds only";
    ambiguity.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{"zero-loss", 0.0, 0.92, 0.94},
        cf::ScenarioProbabilityBounds{"ten-loss", 0.0, 0.03, 0.04},
        cf::ScenarioProbabilityBounds{"hundred-loss", 0.0, 0.05, 0.10},
    };
    return ambiguity;
}

void test_prepared_keyed_upper_expected_shortfall_projection() {
    const cf::PortfolioAmbiguityProjector binary_projector(
        binary_tail_config(0.05),
        binary_tail_ambiguity(0.02, 0.05, 0.08));
    std::vector<cf::AmbiguityScenarioMetricValue> binary_values = {
        {"no-loss", 0.0}, {"loss", 10.0}};

    const cf::AmbiguityUpperExpectedShortfallProjection es05 =
        binary_projector.project_upper_expected_shortfall(
            binary_values, 0.05);
    check(near(es05.tail_probability, 0.05) &&
            es05.scenario_probability_bounds.size() == 2U &&
            es05.scenario_probability_bounds[0].scenario_id == "loss" &&
            es05.scenario_probability_bounds[1].scenario_id == "no-loss",
        "upper-ES projection publishes its tail mass and canonical scenario order");
    check(near(es05.upper_expected_shortfall.minimum.value, 4.0) &&
            near(es05.upper_expected_shortfall.central, 10.0) &&
            near(es05.upper_expected_shortfall.maximum.value, 10.0) &&
            near(es05.upper_expected_shortfall.minimum.scenario_weights[0],
              0.02) &&
            near(es05.upper_expected_shortfall.maximum.scenario_weights[0],
              0.08),
        "generic upper-ES projection preserves exact fractional binary-tail endpoints and witnesses");
    check(near(reconstruct_upper_expected_shortfall(es05,
                   es05.upper_expected_shortfall.minimum, binary_values),
              es05.upper_expected_shortfall.minimum.value) &&
            near(reconstruct_upper_expected_shortfall(es05,
                   es05.upper_expected_shortfall.maximum, binary_values),
              es05.upper_expected_shortfall.maximum.value) &&
            es05.maximum_endpoint_probability_error <= 1.0e-12,
        "upper-ES endpoint measures are feasible and reconstruct their fractional tails");

    std::reverse(binary_values.begin(), binary_values.end());
    const cf::AmbiguityUpperExpectedShortfallProjection reordered =
        binary_projector.project_upper_expected_shortfall(
            binary_values, 0.05);
    check(reordered.scenario_probability_bounds[0].scenario_id == "loss" &&
            reordered.upper_expected_shortfall.minimum.scenario_weights ==
                es05.upper_expected_shortfall.minimum.scenario_weights &&
            reordered.upper_expected_shortfall.maximum.scenario_weights ==
                es05.upper_expected_shortfall.maximum.scenario_weights &&
            near(reordered.upper_expected_shortfall.minimum.value, 4.0) &&
            near(reordered.upper_expected_shortfall.central, 10.0) &&
            near(reordered.upper_expected_shortfall.maximum.value, 10.0),
        "upper-ES keyed input order cannot change canonical values or witnesses");

    const cf::AmbiguityUpperExpectedShortfallProjection es10 =
        binary_projector.project_upper_expected_shortfall(
            binary_values, 0.10);
    const cf::AmbiguityUpperExpectedShortfallProjection es100 =
        binary_projector.project_upper_expected_shortfall(
            binary_values, 1.0);
    check(near(es10.upper_expected_shortfall.minimum.value, 2.0) &&
            near(es10.upper_expected_shortfall.central, 5.0) &&
            near(es10.upper_expected_shortfall.maximum.value, 8.0) &&
            near(es100.upper_expected_shortfall.minimum.value, 0.2) &&
            near(es100.upper_expected_shortfall.central, 0.5) &&
            near(es100.upper_expected_shortfall.maximum.value, 0.8),
        "arbitrary valid tail masses include exact fractional atoms and tail one equals the mean");

    const std::vector<cf::AmbiguityScenarioMetricValue> tied_values = {
        {"no-loss", 7.0}, {"loss", 7.0}};
    const cf::AmbiguityUpperExpectedShortfallProjection tied =
        binary_projector.project_upper_expected_shortfall(
            tied_values, 0.037);
    check(near(tied.upper_expected_shortfall.minimum.value, 7.0) &&
            near(tied.upper_expected_shortfall.central, 7.0) &&
            near(tied.upper_expected_shortfall.maximum.value, 7.0) &&
            tied.upper_expected_shortfall.minimum.scenario_weights ==
                tied.upper_expected_shortfall.maximum.scenario_weights,
        "tied upper-ES objectives use one deterministic pro-rata capacity endpoint witness");

    const cf::PortfolioAmbiguityProjector multilevel_projector(
        three_level_tail_config(), three_level_tail_ambiguity());
    const std::vector<cf::AmbiguityScenarioMetricValue> multilevel_values = {
        {"zero-loss", 0.0}, {"hundred-loss", 100.0},
        {"ten-loss", 10.0}};
    const cf::AmbiguityUpperExpectedShortfallProjection multilevel =
        multilevel_projector.project_upper_expected_shortfall(
            multilevel_values, 0.05);
    check(near(multilevel.upper_expected_shortfall.minimum.value, 46.0) &&
            near(multilevel.upper_expected_shortfall.central, 100.0) &&
            near(multilevel.upper_expected_shortfall.maximum.value, 100.0) &&
            near(multilevel.upper_expected_shortfall.minimum
                     .scenario_weights[0],
              0.02) &&
            near(multilevel.upper_expected_shortfall.minimum
                     .scenario_weights[1],
              0.04) &&
            near(multilevel.upper_expected_shortfall.minimum
                     .scenario_weights[2],
              0.94),
        "generic upper-ES projection handles a fractional boundary atom in a reordered multilevel metric");
}

void test_keyed_upper_expected_shortfall_validation() {
    const cf::PortfolioAmbiguityProjector projector(
        binary_tail_config(0.05),
        binary_tail_ambiguity(0.02, 0.05, 0.08));
    const std::vector<cf::AmbiguityScenarioMetricValue> valid = {
        {"loss", 10.0}, {"no-loss", 0.0}};
    (void)projector.project_upper_expected_shortfall(valid, 0.05);

    for (const double invalid_tail : {0.0, -0.01, 1.01,
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity()}) {
        expect_invalid_argument(
            [&projector, &valid, invalid_tail] {
                (void)projector.project_upper_expected_shortfall(
                    valid, invalid_tail);
            },
            "upper-ES projection rejects a non-finite or out-of-domain tail probability");
    }

    std::vector<cf::AmbiguityScenarioMetricValue> invalid = valid;
    invalid.pop_back();
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_upper_expected_shortfall(invalid, 0.05);
        },
        "upper-ES keyed projection rejects a missing scenario value");

    invalid = valid;
    invalid[1].scenario_id = "loss";
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_upper_expected_shortfall(invalid, 0.05);
        },
        "upper-ES keyed projection rejects duplicate scenario ids");

    invalid = valid;
    invalid[1].scenario_id = "unknown";
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_upper_expected_shortfall(invalid, 0.05);
        },
        "upper-ES keyed projection rejects a missing-plus-unknown id substitution");

    invalid = valid;
    invalid[1].scenario_id = "bad id";
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_upper_expected_shortfall(invalid, 0.05);
        },
        "upper-ES keyed projection rejects unsafe scenario ids");

    invalid = valid;
    invalid[1].value = std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_upper_expected_shortfall(invalid, 0.05);
        },
        "upper-ES keyed projection rejects non-finite scenario values");

    invalid = valid;
    invalid[1].value = std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&projector, &invalid] {
            (void)projector.project_upper_expected_shortfall(invalid, 0.05);
        },
        "upper-ES keyed projection rejects infinite scenario values");
}

void test_multilevel_expected_shortfall_tail() {
    const cf::PortfolioAmbiguitySummary summary =
        cf::evaluate_portfolio_ambiguity(
            three_level_tail_config(), three_level_tail_ambiguity());
    check(near(summary.principal_loss_expected_shortfall_95_million
                   .minimum.value,
              46.0) &&
            near(summary.principal_loss_expected_shortfall_95_million.central,
              100.0) &&
            near(summary.principal_loss_expected_shortfall_95_million
                   .maximum.value,
              100.0),
        "exact ES optimization mixes fractional boundary atoms rather than only event mass");
}

void test_degenerate_set_and_deterministic_ties() {
    const cf::PortfolioConfig config = binary_tail_config(0.05);
    cf::PortfolioAmbiguityConfig ambiguity =
        binary_tail_ambiguity(0.05, 0.05, 0.05);
    std::reverse(ambiguity.scenario_probabilities.begin(),
        ambiguity.scenario_probabilities.end());
    const cf::PortfolioAmbiguitySummary summary =
        cf::evaluate_portfolio_ambiguity(config, ambiguity);

    check(near(summary.expected_principal_loss_million.minimum.value,
              summary.expected_principal_loss_million.central) &&
            near(summary.expected_principal_loss_million.maximum.value,
              summary.expected_principal_loss_million.central) &&
            summary.expected_principal_loss_million.minimum.scenario_weights ==
                summary.expected_principal_loss_million.maximum.scenario_weights,
        "a degenerate ambiguity set collapses every endpoint to the central measure");
    check(summary.expected_total_draws_million.minimum.scenario_weights ==
            summary.expected_total_draws_million.maximum.scenario_weights,
        "equal-valued objectives use the same pro-rata capacity allocation in both directions");
    check(summary.scenario_probability_bounds[0].scenario_id == "loss" &&
            summary.scenario_probability_bounds[1].scenario_id == "no-loss",
        "ambiguity input permutations normalize to identifier order");
}

void test_zero_central_atom_validates_and_projects() {
    cf::PortfolioConfig config = three_state_config();
    config.scenario_label = "zero-central-weight ambiguity projection";
    config.joint_scenarios[0].weight = 0.70;
    config.joint_scenarios[1].weight = 0.30;
    config.joint_scenarios[2].weight = 0.0;

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "zero-central-weight interval set";
    ambiguity.source_note =
        "synthetic zero-central-probability projection regression";
    ambiguity.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{"continuing", 0.0, 0.0, 0.20},
        cf::ScenarioProbabilityBounds{"good", 0.50, 0.70, 0.80},
        cf::ScenarioProbabilityBounds{"bad", 0.10, 0.30, 0.50},
    };

    cf::validate_portfolio_ambiguity_config(config, ambiguity);
    const cf::PortfolioAmbiguityProjector projector(config, ambiguity);
    const std::vector<cf::AmbiguityScenarioMetricValue> values = {
        {"good", 10.0}, {"bad", 0.0}, {"continuing", 1'000.0}};

    const cf::AmbiguityMetricProjection expectation =
        projector.project_expectation(values);
    check(near(expectation.expectation.minimum.value, 5.0) &&
            near(expectation.expectation.central, 7.0) &&
            near(expectation.expectation.maximum.value, 207.0) &&
            expectation.maximum_endpoint_probability_error <= 1.0e-12,
        "a zero-central atom validates, contributes nothing centrally, and remains available to ambiguity endpoints");

    const cf::AmbiguityUpperExpectedShortfallProjection tail =
        projector.project_upper_expected_shortfall(values, 0.05);
    check(near(tail.upper_expected_shortfall.minimum.value, 10.0) &&
            near(tail.upper_expected_shortfall.central, 10.0) &&
            near(tail.upper_expected_shortfall.maximum.value, 1'000.0) &&
            tail.maximum_endpoint_probability_error <= 1.0e-12,
        "zero central mass does not contaminate central expected shortfall while positive ambiguity mass can expose the extreme atom");
}

void test_validation_boundaries() {
    const cf::PortfolioConfig config = three_state_config();
    cf::PortfolioAmbiguityConfig ambiguity = three_state_ambiguity();
    cf::validate_portfolio_ambiguity_config(config, ambiguity);

    ambiguity.model_version = "9.9.9";
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "unknown ambiguity model versions are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.synthetic_inputs = false;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "ambiguity v0.1 rejects empirical-input claims");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[1].scenario_id = "continuing";
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "duplicate ambiguity scenario ids are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[1].scenario_id = "unknown";
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "missing and extra ambiguity scenario ids are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].lower_weight =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "non-finite ambiguity weights are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].upper_weight =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "infinite ambiguity weights are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].lower_weight = -0.01;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "negative lower weights are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].lower_weight = 0.41;
    ambiguity.scenario_probabilities[0].upper_weight = 0.40;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "reversed probability intervals are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].central_weight = -0.01;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "negative central weights are rejected");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].central_weight = 0.21;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "central weights must sum to one within tolerance");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].central_weight = 0.21;
    ambiguity.scenario_probabilities[1].central_weight = 0.49;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "normalized ambiguity center must match the portfolio center by id");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[1].lower_weight = 0.5001;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "normalized central probabilities must lie inside their intervals");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].lower_weight = 0.40;
    ambiguity.scenario_probabilities[1].lower_weight = 0.40;
    ambiguity.scenario_probabilities[2].lower_weight = 0.30;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "lower bounds whose sum exceeds one are infeasible");

    ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].upper_weight = 0.25;
    ambiguity.scenario_probabilities[1].upper_weight = 0.35;
    ambiguity.scenario_probabilities[2].upper_weight = 0.35;
    expect_invalid_argument(
        [&config, &ambiguity] {
            cf::validate_portfolio_ambiguity_config(config, ambiguity);
        },
        "upper bounds whose sum is below one are infeasible");

    cf::PortfolioAmbiguityConfig lower_locked = three_state_ambiguity();
    for (cf::ScenarioProbabilityBounds& bounds :
         lower_locked.scenario_probabilities) {
        bounds.lower_weight = bounds.central_weight;
    }
    const cf::PortfolioAmbiguitySummary lower_locked_summary =
        cf::evaluate_portfolio_ambiguity(config, lower_locked);
    check(near(lower_locked_summary.expected_npv_million.minimum.value,
              lower_locked_summary.expected_npv_million.central) &&
            near(lower_locked_summary.expected_npv_million.maximum.value,
              lower_locked_summary.expected_npv_million.central),
        "a lower-bound sum of one produces the unique central measure");

    cf::PortfolioAmbiguityConfig upper_locked = three_state_ambiguity();
    for (cf::ScenarioProbabilityBounds& bounds :
         upper_locked.scenario_probabilities) {
        bounds.upper_weight = bounds.central_weight;
    }
    const cf::PortfolioAmbiguitySummary upper_locked_summary =
        cf::evaluate_portfolio_ambiguity(config, upper_locked);
    check(near(upper_locked_summary.expected_npv_million.minimum.value,
              upper_locked_summary.expected_npv_million.central) &&
            near(upper_locked_summary.expected_npv_million.maximum.value,
              upper_locked_summary.expected_npv_million.central),
        "an upper-bound sum of one produces the unique central measure");

    cf::PortfolioConfig near_one = binary_tail_config(0.5);
    near_one.joint_scenarios[0].weight = 0.5000000000004;
    cf::PortfolioAmbiguityConfig normalization_crossing;
    normalization_crossing.scenario_label = "normalization crossing bounds";
    normalization_crossing.source_note =
        "synthetic near-one normalization rejection test";
    normalization_crossing.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{
            "no-loss", 0.5000000000004, 0.5000000000004, 0.90},
        cf::ScenarioProbabilityBounds{"loss", 0.0, 0.50, 0.50},
    };
    expect_invalid_argument(
        [&near_one, &normalization_crossing] {
            cf::validate_portfolio_ambiguity_config(
                near_one, normalization_crossing);
        },
        "near-one central normalization may not cross a declared interval bound");
}

void test_normalized_central_bound_tolerance_alignment() {
    cf::PortfolioConfig portfolio = three_state_config();
    portfolio.joint_scenarios[2].weight = 0.20000000000001;
    cf::PortfolioAmbiguityConfig ambiguity = three_state_ambiguity();
    ambiguity.scenario_probabilities[0].central_weight =
        0.20000000000001;
    ambiguity.scenario_probabilities[1].lower_weight = 0.50;
    ambiguity.scenario_probabilities[1].upper_weight = 0.50;
    const std::vector<cf::AmbiguityScenarioMetricValue> values{
        {"good", 1.0}, {"bad", 0.0}, {"continuing", 0.0}};

    const cf::AmbiguityMetricProjection projection =
        cf::PortfolioAmbiguityProjector(portfolio, ambiguity)
            .project_expectation(values);
    check(near(projection.expectation.minimum.value, 0.50) &&
            near(projection.expectation.central, 0.50) &&
            near(projection.expectation.maximum.value, 0.50),
        "v0.1 core and strict config share the normalized-central bound tolerance");
}

} // namespace

int main() {
    test_exact_linear_envelopes_and_witnesses();
    test_prepared_keyed_expectation_projection();
    test_keyed_projection_validation();
    test_keyed_projection_preserves_ten_thousand_scenario_bound();
    test_exact_expected_shortfall_bounds();
    test_common_witness_project_tail_attribution_and_tie_invariance();
    test_prepared_keyed_upper_expected_shortfall_projection();
    test_keyed_upper_expected_shortfall_validation();
    test_multilevel_expected_shortfall_tail();
    test_degenerate_set_and_deterministic_ties();
    test_zero_central_atom_validates_and_projects();
    test_validation_boundaries();
    test_normalized_central_bound_tolerance_alignment();

    if (failures != 0) {
        std::cerr << failures << " portfolio ambiguity test(s) failed\n";
        return 1;
    }
    std::cout << "all portfolio ambiguity tests passed\n";
    return 0;
}
