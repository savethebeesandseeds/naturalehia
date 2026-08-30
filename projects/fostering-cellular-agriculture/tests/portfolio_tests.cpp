// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio.hpp>

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
    double draw, double receipt, double principal,
    std::string cash_source_id = "commercial-budget",
    cf::ProjectPathResolution resolution =
        cf::ProjectPathResolution::Resolved,
    std::size_t draw_month = 0U, std::size_t receipt_month = 12U) {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = resolution;
    if (draw > 0.0) {
        path.capital_draws.push_back(cf::MonthlyAmount{draw_month, draw});
    }
    if (receipt > 0.0) {
        path.investor_receipts.push_back(cf::InvestorReceipt{
            receipt_month, std::move(cash_source_id), receipt, principal});
    }
    return path;
}

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, double amount,
    std::size_t month = 12U) {
    cf::ScenarioCashSource source;
    source.id = std::move(id);
    source.kind = kind;
    if (amount > 0.0) {
        source.cash_available.push_back(cf::MonthlyAmount{month, amount});
    }
    return source;
}

[[nodiscard]] cf::PortfolioConfig hand_table_config() {
    cf::PortfolioConfig config;
    config.scenario_label = "two-project deterministic hand table";
    config.source_note = "synthetic unit-test values only";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.horizon_months = 12U;
    config.projects = {
        cf::PortfolioProject{"research-a", cf::ProjectStage::Research, 10.0},
        cf::PortfolioProject{"pilot-b", cf::ProjectStage::Pilot, 10.0},
    };

    cf::JointScenario completion;
    completion.id = "completion";
    completion.weight = 0.60;
    cf::ProjectJointPath research = make_path(
        "research-a", 0.0, 0.0, 0.0, "commercial-budget");
    research.capital_draws = {
        cf::MonthlyAmount{0U, 5.0},
        cf::MonthlyAmount{0U, 2.0},
        cf::MonthlyAmount{0U, 3.0},
    };
    research.investor_receipts = {
        cf::InvestorReceipt{12U, "commercial-budget", 4.0, 4.0},
        cf::InvestorReceipt{12U, "commercial-budget", 5.0, 5.0},
        cf::InvestorReceipt{12U, "commercial-budget", 3.0, 1.0},
    };
    completion.project_paths = {
        research,
        make_path("pilot-b", 5.0, 6.0, 5.0, "licensing-budget"),
    };
    completion.cash_sources = {
        make_source("commercial-budget",
            cf::PortfolioCashSource::Commercial, 0.0),
        make_source("licensing-budget",
            cf::PortfolioCashSource::LicensingRoyalty, 6.0),
    };
    completion.cash_sources[0].cash_available = {
        cf::MonthlyAmount{12U, 7.0},
        cf::MonthlyAmount{12U, 5.0},
    };
    completion.pool_costs = {cf::MonthlyAmount{0U, 1.0}};
    completion.factor_tags = {"demand-normal", "biology-stable"};

    cf::JointScenario failure;
    failure.id = "joint-failure";
    failure.weight = 0.40;
    failure.project_paths = {
        make_path("research-a", 10.0, 2.0, 2.0, "recovery-budget"),
        make_path("pilot-b", 5.0, 0.0, 0.0),
    };
    failure.cash_sources = {make_source("recovery-budget",
        cf::PortfolioCashSource::Recovery, 2.0)};
    failure.pool_costs = {cf::MonthlyAmount{0U, 1.0}};
    failure.factor_tags = {"shared-scale-shock"};

    config.joint_scenarios = {completion, failure};
    config.loss_layers = {
        cf::LossLayer{"first-loss", 0.0, 8.0},
        cf::LossLayer{"upper-loss", 8.0, 20.0},
    };
    return config;
}

[[nodiscard]] const cf::ReturnSourceTotal& source_total(
    const std::vector<cf::ReturnSourceTotal>& totals,
    cf::PortfolioCashSource source) {
    return totals.at(static_cast<std::size_t>(source));
}

void test_hand_table_and_reconciliations() {
    const cf::PortfolioSummary summary =
        cf::evaluate_portfolio(hand_table_config());
    check(summary.scenarios.size() == 2U && summary.projects.size() == 2U,
        "the engine retains both explicit scenarios and projects");
    const cf::JointScenarioResult& completion = summary.scenarios[0];
    const cf::JointScenarioResult& failure = summary.scenarios[1];
    check(completion.scenario_id == "completion" &&
            failure.scenario_id == "joint-failure",
        "scenario output has deterministic identifier order");
    check(near(completion.total_draws_million, 15.0) &&
            near(completion.total_receipts_million, 18.0) &&
            near(completion.total_pool_costs_million, 1.0) &&
            near(completion.npv_million, 2.0),
        "completion scenario matches the dated cash-flow hand table");
    check(near(completion.peak_same_month_draw_million, 15.0) &&
            near(completion.peak_same_month_funding_need_million, 16.0) &&
            near(completion.peak_cumulative_net_outlay_million, 16.0) &&
            near(completion.outstanding_principal_million, 0.0) &&
            near(completion.principal_loss_million, 0.0),
        "completion exposure, liquidity, and loss match the hand table");
    check(near(failure.total_draws_million, 15.0) &&
            near(failure.total_receipts_million, 2.0) &&
            near(failure.npv_million, -14.0) &&
            near(failure.outstanding_principal_million, 0.0) &&
            near(failure.principal_loss_million, 13.0),
        "resolved failure recognizes unreturned principal as loss");
    check(completion.monthly_cash_flows.size() == 13U &&
            near(completion.monthly_cash_flows.front()
                     .cumulative_net_cash_flow_million,
                -16.0) &&
            near(completion.monthly_cash_flows.back()
                     .cumulative_net_cash_flow_million,
                2.0),
        "monthly investor cash flows expose the complete horizon");

    check(near(summary.principal_loss_million.mean, 5.2) &&
            near(summary.outstanding_principal_million.mean, 0.0) &&
            near(summary.npv_million.mean, -4.4) &&
            near(summary.principal_impairment_probability, 0.4) &&
            near(summary.negative_npv_probability, 0.4),
        "weighted pool loss, exposure, NPV, and event metrics are correct");
    check(near(summary.npv_shortfall_million.mean, 5.6) &&
            near(summary.npv_shortfall_million.expected_shortfall_95, 14.0),
        "NPV downside is an explicit non-negative shortfall tail");
    check(near(source_total(summary.expected_return_sources,
                   cf::PortfolioCashSource::Commercial)
                   .nominal_million,
              7.2) &&
            near(source_total(summary.expected_return_sources,
                   cf::PortfolioCashSource::LicensingRoyalty)
                   .nominal_million,
              3.6) &&
            near(source_total(summary.expected_return_sources,
                   cf::PortfolioCashSource::Recovery)
                   .nominal_million,
              0.8),
        "expected receipts remain attributable to external source kinds");
    check(near(summary.projects[0].expected_draws_million, 10.0) &&
            near(summary.projects[0].expected_receipts_million, 8.0) &&
            near(summary.projects[0].total_receipts_million.mean, 8.0) &&
            near(summary.projects[0].expected_outstanding_principal_million,
                0.0) &&
            near(summary.projects[0].expected_principal_loss_million, 3.2) &&
            near(summary.projects[0].expected_npv_before_pool_costs_million,
                -2.0) &&
            near(summary.projects[0].principal_impairment_probability, 0.4),
        "project-level draw, receipt, exposure, loss, NPV, and impairment are explicit");

    check(summary.layers.size() == 2U &&
            near(failure.layers[0].principal_loss_million, 8.0) &&
            near(failure.layers[1].principal_loss_million, 5.0) &&
            near(summary.layers[0].expected_loss_million, 3.2) &&
            near(summary.layers[1].expected_loss_million, 2.0),
        "contiguous layers allocate the same realized pool loss");
    for (const cf::JointScenarioResult& scenario : summary.scenarios) {
        double layer_sum = 0.0;
        double project_loss_sum = 0.0;
        for (const cf::LayerPathResult& layer : scenario.layers) {
            layer_sum += layer.principal_loss_million;
        }
        for (const cf::ProjectPathResult& project : scenario.projects) {
            project_loss_sum += project.principal_loss_million;
        }
        check(near(layer_sum, scenario.principal_loss_million) &&
                near(project_loss_sum, scenario.principal_loss_million),
            "project, pool, and layer realized losses reconcile pathwise");
    }
    check(summary.maximum_cash_reconciliation_error_million <= 1.0e-9 &&
            summary.maximum_layer_reconciliation_error_million <= 1.0e-9,
        "published reconciliation residuals remain immaterial");
}

void test_continuing_balloon_is_exposure_not_realized_loss() {
    cf::PortfolioConfig config = hand_table_config();
    config.scenario_label = "performing continuing balloon";
    config.projects = {cf::PortfolioProject{
        "performing-note", cf::ProjectStage::RepeatProduction, 10.0}};
    cf::JointScenario scenario;
    scenario.id = "performing-at-horizon";
    scenario.weight = 1.0;
    scenario.project_paths = {make_path("performing-note", 10.0, 0.0,
        0.0, "unused", cf::ProjectPathResolution::Continuing)};
    config.joint_scenarios = {scenario};
    config.loss_layers = {cf::LossLayer{"whole-pool", 0.0, 10.0}};

    const cf::PortfolioSummary summary = cf::evaluate_portfolio(config);
    const cf::JointScenarioResult& result = summary.scenarios.front();
    check(near(result.total_draws_million, 10.0) &&
            near(result.principal_returned_million, 0.0) &&
            near(result.outstanding_principal_million, 10.0) &&
            near(result.principal_loss_million, 0.0),
        "unreturned continuing principal remains outstanding rather than impaired");
    check(near(summary.outstanding_principal_million.mean, 10.0) &&
            near(summary.projects.front()
                     .expected_outstanding_principal_million,
                10.0) &&
            near(summary.principal_loss_million.mean, 0.0) &&
            near(summary.principal_impairment_probability, 0.0) &&
            near(summary.layers.front().expected_loss_million, 0.0),
        "pool, project, probability, and layer outputs preserve the exposure/loss boundary");
}

void test_shared_cash_budget_prevents_double_use() {
    cf::PortfolioConfig config = hand_table_config();
    config.scenario_label = "shared guarantee budget";
    cf::JointScenario& scenario = config.joint_scenarios.front();
    scenario.project_paths = {
        make_path("research-a", 5.0, 6.0, 5.0, "shared-guarantee"),
        make_path("pilot-b", 5.0, 6.0, 5.0, "shared-guarantee"),
    };
    scenario.cash_sources = {make_source("shared-guarantee",
        cf::PortfolioCashSource::ExplicitSupport, 10.0)};
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "two projects cannot each consume the same finite guarantee budget");

    scenario.project_paths[0].investor_receipts.front().amount_million = 5.0;
    scenario.project_paths[1].investor_receipts.front().amount_million = 5.0;
    cf::validate_portfolio_config(config);
    const cf::PortfolioSummary accepted = cf::evaluate_portfolio(config);
    check(near(source_total(accepted.scenarios.front().return_sources,
                   cf::PortfolioCashSource::ExplicitSupport)
                   .nominal_million,
              10.0),
        "two projects may share a budget when aggregate receipts exactly fit it");
}

void test_same_month_liquidity_is_gross_not_netted() {
    cf::PortfolioConfig config;
    config.scenario_label = "gross same-month liquidity";
    config.source_note = "synthetic liquidity ordering test";
    config.currency_label = "TEST";
    config.monetary_basis = "test units";
    config.horizon_months = 1U;
    config.projects = {cf::PortfolioProject{
        "same-month-note", cf::ProjectStage::Pilot, 10.0}};
    cf::JointScenario scenario;
    scenario.id = "same-month-settlement";
    scenario.weight = 1.0;
    scenario.project_paths = {make_path("same-month-note", 10.0, 10.0,
        10.0, "sale-budget", cf::ProjectPathResolution::Resolved, 0U, 0U)};
    scenario.cash_sources = {make_source("sale-budget",
        cf::PortfolioCashSource::ExitSale, 10.0, 0U)};
    scenario.pool_costs = {cf::MonthlyAmount{0U, 2.0}};
    config.joint_scenarios = {scenario};

    const cf::PortfolioSummary summary = cf::evaluate_portfolio(config);
    const cf::JointScenarioResult& result = summary.scenarios.front();
    check(near(result.peak_same_month_draw_million, 10.0) &&
            near(result.peak_same_month_funding_need_million, 12.0) &&
            near(result.peak_cumulative_net_outlay_million, 12.0) &&
            near(result.monthly_cash_flows.front().funding_need_million, 12.0) &&
            near(result.monthly_cash_flows.front()
                     .cumulative_net_cash_flow_million,
                -2.0),
        "draws and costs settle before same-month receipts for liquidity measurement");
}

[[nodiscard]] cf::PortfolioConfig binary_loss_config(bool mutually_exclusive) {
    cf::PortfolioConfig config = hand_table_config();
    config.scenario_label = mutually_exclusive
        ? "mutually exclusive project losses"
        : "perfectly correlated project losses";
    config.loss_layers.clear();
    config.joint_scenarios.clear();

    cf::JointScenario first;
    first.id = "state-one";
    first.weight = 0.50;
    cf::JointScenario second;
    second.id = "state-two";
    second.weight = 0.50;
    if (mutually_exclusive) {
        first.project_paths = {
            make_path("research-a", 10.0, 0.0, 0.0),
            make_path("pilot-b", 10.0, 10.0, 10.0, "commercial-budget"),
        };
        first.cash_sources = {make_source("commercial-budget",
            cf::PortfolioCashSource::Commercial, 10.0)};
        second.project_paths = {
            make_path("research-a", 10.0, 10.0, 10.0,
                "commercial-budget"),
            make_path("pilot-b", 10.0, 0.0, 0.0),
        };
        second.cash_sources = first.cash_sources;
    } else {
        first.project_paths = {
            make_path("research-a", 10.0, 0.0, 0.0),
            make_path("pilot-b", 10.0, 0.0, 0.0),
        };
        second.project_paths = {
            make_path("research-a", 10.0, 10.0, 10.0,
                "commercial-budget"),
            make_path("pilot-b", 10.0, 10.0, 10.0,
                "commercial-budget"),
        };
        second.cash_sources = {make_source("commercial-budget",
            cf::PortfolioCashSource::Commercial, 20.0)};
    }
    config.joint_scenarios = {first, second};
    return config;
}

void test_dependence_and_diversification_metrics() {
    const cf::PortfolioSummary correlated =
        cf::evaluate_portfolio(binary_loss_config(false));
    check(correlated.pairwise_loss_correlations.size() == 1U &&
            correlated.pairwise_loss_correlations.front()
                .correlation.has_value() &&
            near(*correlated.pairwise_loss_correlations.front().correlation,
                1.0) &&
            near(correlated.diversification_benefit_es95_million, 0.0) &&
            correlated.diversification_ratio_es95.has_value() &&
            near(*correlated.diversification_ratio_es95, 0.0),
        "perfectly aligned losses have correlation one and no diversification benefit");

    const cf::PortfolioSummary exclusive =
        cf::evaluate_portfolio(binary_loss_config(true));
    check(exclusive.pairwise_loss_correlations.front()
              .correlation.has_value() &&
            near(*exclusive.pairwise_loss_correlations.front().correlation,
                -1.0) &&
            near(exclusive.sum_standalone_es95_million, 20.0) &&
            near(exclusive.principal_loss_million.expected_shortfall_95, 10.0) &&
            near(exclusive.diversification_benefit_es95_million, 10.0) &&
            exclusive.diversification_ratio_es95.has_value() &&
            near(*exclusive.diversification_ratio_es95, 0.5),
        "mutually exclusive losses reduce pool ES without creating cash");
    check(near(exclusive.projects[0]
                   .pool_loss_tail_contribution_es95_million,
              5.0) &&
            near(exclusive.projects[1]
                    .pool_loss_tail_contribution_es95_million,
              5.0),
        "tie-neutral project contributions reconcile to constant pool-tail loss");

    cf::PortfolioConfig incomplete = hand_table_config();
    incomplete.scenario_label = "incomplete tail diversification";
    incomplete.loss_layers.clear();
    incomplete.joint_scenarios.clear();
    const auto add_state = [&incomplete](std::string id, double weight,
                               bool first_loses, bool second_loses) {
        cf::JointScenario state;
        state.id = std::move(id);
        state.weight = weight;
        const double first_receipt = first_loses ? 2.0 : 10.0;
        const double second_receipt = second_loses ? 2.0 : 10.0;
        state.project_paths = {
            make_path("research-a", 10.0, first_receipt, first_receipt,
                "first-source"),
            make_path("pilot-b", 10.0, second_receipt, second_receipt,
                "second-source"),
        };
        state.cash_sources = {
            make_source("first-source", first_loses
                    ? cf::PortfolioCashSource::Recovery
                    : cf::PortfolioCashSource::Commercial,
                first_receipt),
            make_source("second-source", second_loses
                    ? cf::PortfolioCashSource::Recovery
                    : cf::PortfolioCashSource::Commercial,
                second_receipt),
        };
        incomplete.joint_scenarios.push_back(std::move(state));
    };
    add_state("common-success", 0.62, false, false);
    add_state("first-only-loss", 0.18, true, false);
    add_state("second-only-loss", 0.18, false, true);
    add_state("common-loss", 0.02, true, true);
    const cf::PortfolioSummary incomplete_summary =
        cf::evaluate_portfolio(incomplete);
    check(near(incomplete_summary.diversification_benefit_es95_million,
              4.8) &&
            incomplete_summary.diversification_ratio_es95.has_value() &&
            near(*incomplete_summary.diversification_ratio_es95, 0.3) &&
            incomplete_summary.diversification_benefit_es99_million == 0.0 &&
            !std::signbit(
                incomplete_summary.diversification_benefit_es99_million) &&
            incomplete_summary.diversification_ratio_es99.has_value() &&
            *incomplete_summary.diversification_ratio_es99 == 0.0 &&
            !std::signbit(*incomplete_summary.diversification_ratio_es99),
        "roundoff at an exact zero tail benefit is canonicalized without hiding material negative diversification");
}

void test_exact_weighted_tail_boundary() {
    cf::PortfolioConfig config;
    config.scenario_label = "exact weighted tail boundary";
    config.source_note = "synthetic exact boundary test";
    config.currency_label = "TEST";
    config.monetary_basis = "test units";
    config.horizon_months = 12U;
    config.projects = {cf::PortfolioProject{
        "boundary-note", cf::ProjectStage::Pilot, 10.0}};
    cf::JointScenario no_loss;
    no_loss.id = "no-loss";
    no_loss.weight = 0.95;
    no_loss.project_paths = {make_path("boundary-note", 10.0, 10.0, 10.0,
        "commercial-budget")};
    no_loss.cash_sources = {make_source("commercial-budget",
        cf::PortfolioCashSource::Commercial, 10.0)};
    cf::JointScenario loss;
    loss.id = "total-loss";
    loss.weight = 0.05;
    loss.project_paths = {make_path("boundary-note", 10.0, 0.0, 0.0)};
    config.joint_scenarios = {no_loss, loss};

    const cf::PortfolioSummary summary = cf::evaluate_portfolio(config);
    check(near(summary.principal_loss_million.p95, 0.0) &&
            near(summary.principal_loss_million.expected_shortfall_95, 10.0) &&
            near(summary.principal_impairment_probability, 0.05),
        "exact 95 percent CDF boundary keeps VaR at zero and allocates full ES tail");
}

void test_zero_weight_extreme_atom_is_outside_distribution_support() {
    cf::PortfolioConfig config;
    config.scenario_label = "zero-weight extreme loss atom";
    config.source_note = "synthetic zero-probability support regression";
    config.currency_label = "TEST";
    config.monetary_basis = "test units";
    config.horizon_months = 12U;
    config.projects = {cf::PortfolioProject{
        "support-note", cf::ProjectStage::Pilot, 100.0}};

    cf::JointScenario no_loss;
    no_loss.id = "no-loss";
    no_loss.weight = 0.95;
    no_loss.project_paths = {make_path("support-note", 10.0, 10.0, 10.0,
        "commercial-budget")};
    no_loss.cash_sources = {make_source("commercial-budget",
        cf::PortfolioCashSource::Commercial, 10.0)};

    cf::JointScenario supported_loss;
    supported_loss.id = "supported-loss";
    supported_loss.weight = 0.05;
    supported_loss.project_paths = {
        make_path("support-note", 10.0, 0.0, 0.0)};

    cf::JointScenario impossible_extreme;
    impossible_extreme.id = "zero-weight-extreme-loss";
    impossible_extreme.weight = 0.0;
    impossible_extreme.project_paths = {
        make_path("support-note", 100.0, 0.0, 0.0)};

    config.joint_scenarios = {
        no_loss, supported_loss, impossible_extreme};
    cf::validate_portfolio_config(config);
    const cf::PortfolioSummary summary = cf::evaluate_portfolio(config);

    check(summary.scenarios.size() == 3U &&
            near(summary.principal_loss_million.p95, 0.0) &&
            near(summary.principal_loss_million.p99, 10.0) &&
            near(summary.principal_loss_million.maximum, 10.0) &&
            near(summary.principal_loss_million.expected_shortfall_95, 10.0) &&
            near(summary.principal_loss_million.expected_shortfall_99, 10.0),
        "a retained zero-probability extreme atom does not affect quantiles, supported maximum, or expected shortfall");
}

void test_input_permutations_do_not_change_published_financials() {
    const cf::PortfolioSummary baseline =
        cf::evaluate_portfolio(hand_table_config());
    cf::PortfolioConfig permuted_config = hand_table_config();
    std::reverse(permuted_config.joint_scenarios.begin(),
        permuted_config.joint_scenarios.end());
    for (cf::JointScenario& scenario : permuted_config.joint_scenarios) {
        std::reverse(scenario.project_paths.begin(), scenario.project_paths.end());
        std::reverse(scenario.cash_sources.begin(), scenario.cash_sources.end());
        std::reverse(scenario.pool_costs.begin(), scenario.pool_costs.end());
        for (cf::ProjectJointPath& path : scenario.project_paths) {
            std::reverse(path.capital_draws.begin(), path.capital_draws.end());
            std::reverse(
                path.investor_receipts.begin(), path.investor_receipts.end());
        }
        for (cf::ScenarioCashSource& source : scenario.cash_sources) {
            std::reverse(
                source.cash_available.begin(), source.cash_available.end());
        }
    }
    const cf::PortfolioSummary permuted =
        cf::evaluate_portfolio(permuted_config);

    check(permuted.scenarios.size() == baseline.scenarios.size(),
        "scenario permutation preserves result cardinality");
    for (std::size_t index = 0U; index < baseline.scenarios.size(); ++index) {
        const cf::JointScenarioResult& first = baseline.scenarios[index];
        const cf::JointScenarioResult& second = permuted.scenarios[index];
        check(first.scenario_id == second.scenario_id &&
                first.total_receipts_million == second.total_receipts_million &&
                first.npv_million == second.npv_million &&
                first.principal_loss_million == second.principal_loss_million &&
                first.outstanding_principal_million ==
                    second.outstanding_principal_million &&
                first.peak_same_month_funding_need_million ==
                    second.peak_same_month_funding_need_million &&
                first.peak_cumulative_net_outlay_million ==
                    second.peak_cumulative_net_outlay_million,
            "scenario, path, and record permutations preserve main published cash and risk metrics exactly");
        for (std::size_t source = 0U;
             source < first.return_sources.size(); ++source) {
            check(first.return_sources[source].nominal_million ==
                        second.return_sources[source].nominal_million &&
                    first.return_sources[source].present_value_million ==
                        second.return_sources[source].present_value_million,
                "record permutations preserve published source totals exactly");
        }
    }
    check(baseline.total_draws_million.mean ==
                permuted.total_draws_million.mean &&
            baseline.principal_loss_million.mean ==
                permuted.principal_loss_million.mean &&
            baseline.npv_million.mean == permuted.npv_million.mean &&
            baseline.peak_cumulative_net_outlay_million.mean ==
                permuted.peak_cumulative_net_outlay_million.mean,
        "scenario permutations preserve aggregate published metrics exactly");
}

void test_project_source_weight_and_principal_validation() {
    cf::PortfolioConfig config = hand_table_config();
    config.joint_scenarios.front().project_paths[1].project_id = "research-a";
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "duplicate project paths are rejected");

    config = hand_table_config();
    config.joint_scenarios.front().project_paths.pop_back();
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "missing project paths are rejected");

    config = hand_table_config();
    config.joint_scenarios.front().weight = 0.50;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "joint scenario weights must sum to one within tolerance");

    config = hand_table_config();
    config.joint_scenarios.front().project_paths.front()
        .investor_receipts.front().cash_source_id = "unknown-budget";
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "receipts must reference a declared scenario cash budget");

    config = hand_table_config();
    config.joint_scenarios.front().cash_sources.push_back(
        config.joint_scenarios.front().cash_sources.front());
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "scenario cash budget identifiers must be unique");

    config = hand_table_config();
    config.joint_scenarios.front().project_paths.front()
        .investor_receipts.front().principal_component_million = 4.5;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "a principal component cannot exceed its receipt cash");

    config = hand_table_config();
    cf::InvestorReceipt& near_overage = config.joint_scenarios.front()
        .project_paths.front()
        .investor_receipts.front();
    near_overage.principal_component_million =
        near_overage.amount_million + 1.0e-12;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "principal classification cannot invent cash inside a numeric tolerance");

    config = hand_table_config();
    config.joint_scenarios.front().project_paths.front()
        .investor_receipts.back().principal_component_million = 2.0;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "total returned principal cannot exceed funded project draws");

    config = hand_table_config();
    config.joint_scenarios.front().project_paths.front()
        .capital_draws.front().amount_million = 10.01;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "project draws cannot exceed the declared commitment");
}

void test_resource_numeric_and_aggregate_layer_bounds() {
    cf::PortfolioConfig staged_horizon = hand_table_config();
    staged_horizon.horizon_months = 2'400U;
    cf::validate_portfolio_config(staged_horizon);
    staged_horizon.horizon_months = 2'401U;
    expect_invalid_argument(
        [&staged_horizon] {
            cf::validate_portfolio_config(staged_horizon);
        },
        "portfolio horizon covers staged construction plus delayed recovery but remains bounded");

    cf::PortfolioConfig large_layers;
    large_layers.scenario_label = "aggregate-aware layer bounds";
    large_layers.source_note = "synthetic aggregate layer boundary test";
    large_layers.currency_label = "TEST";
    large_layers.monetary_basis = "test units";
    large_layers.horizon_months = 1U;
    large_layers.projects = {
        cf::PortfolioProject{"large-a", cf::ProjectStage::FirstIndustrial,
            750'000.0},
        cf::PortfolioProject{"large-b", cf::ProjectStage::RepeatProduction,
            750'000.0},
    };
    cf::JointScenario scenario;
    scenario.id = "no-draw";
    scenario.weight = 1.0;
    scenario.project_paths = {
        make_path("large-a", 0.0, 0.0, 0.0),
        make_path("large-b", 0.0, 0.0, 0.0),
    };
    large_layers.joint_scenarios = {scenario};
    large_layers.loss_layers = {
        cf::LossLayer{"lower", 0.0, 750'000.0},
        cf::LossLayer{"upper", 750'000.0, 1'500'000.0},
    };
    cf::validate_portfolio_config(large_layers);

    cf::PortfolioConfig oversized = hand_table_config();
    oversized.projects.front().commitment_million = 1'000'000.01;
    expect_invalid_argument(
        [&oversized] { cf::validate_portfolio_config(oversized); },
        "per-field money inputs above 1e6 million are rejected");

    cf::PortfolioConfig excessive_work;
    excessive_work.scenario_label = "project month work guard";
    excessive_work.source_note = "synthetic resource-bound test";
    excessive_work.currency_label = "TEST";
    excessive_work.monetary_basis = "test units";
    excessive_work.horizon_months = 1'000U;
    constexpr std::size_t item_count = 50U;
    for (std::size_t project = 0U; project < item_count; ++project) {
        excessive_work.projects.push_back(cf::PortfolioProject{
            "project-" + std::to_string(project),
            cf::ProjectStage::Research, 1.0});
    }
    for (std::size_t state = 0U; state < item_count; ++state) {
        cf::JointScenario work_scenario;
        work_scenario.id = "scenario-" + std::to_string(state);
        work_scenario.weight = 1.0 / static_cast<double>(item_count);
        for (std::size_t project = 0U; project < item_count; ++project) {
            work_scenario.project_paths.push_back(make_path(
                "project-" + std::to_string(project), 0.0, 0.0, 0.0));
        }
        excessive_work.joint_scenarios.push_back(std::move(work_scenario));
    }
    expect_invalid_argument(
        [&excessive_work] {
            cf::validate_portfolio_config(excessive_work);
        },
        "aggregate project-scenario-month work is bounded before allocation");
}

void test_layer_synthetic_and_finite_validation() {
    cf::PortfolioConfig config = hand_table_config();
    config.loss_layers[1].attachment_million = 9.0;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "loss-layer gaps are rejected");

    config = hand_table_config();
    config.loss_layers.back().detachment_million = 19.0;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "loss layers must end at aggregate commitment");

    config = hand_table_config();
    config.synthetic_inputs = false;
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "portfolio v0.1 rejects claims of validated empirical inputs");

    config = hand_table_config();
    config.joint_scenarios.front().project_paths.front()
        .capital_draws.front().amount_million =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&config] { cf::validate_portfolio_config(config); },
        "non-finite cash amounts are rejected");
}

} // namespace

int main() {
    test_hand_table_and_reconciliations();
    test_continuing_balloon_is_exposure_not_realized_loss();
    test_shared_cash_budget_prevents_double_use();
    test_same_month_liquidity_is_gross_not_netted();
    test_dependence_and_diversification_metrics();
    test_exact_weighted_tail_boundary();
    test_zero_weight_extreme_atom_is_outside_distribution_support();
    test_input_permutations_do_not_change_published_financials();
    test_project_source_weight_and_principal_validation();
    test_resource_numeric_and_aggregate_layer_bounds();
    test_layer_synthetic_and_finite_validation();

    if (failures != 0) {
        std::cerr << failures << " portfolio test(s) failed\n";
        return 1;
    }
    std::cout << "all portfolio tests passed\n";
    return 0;
}
