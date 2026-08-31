// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

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

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, std::size_t month, double amount) {
    cf::ScenarioCashSource source;
    source.id = std::move(id);
    source.kind = kind;
    source.cash_available.push_back(cf::MonthlyAmount{month, amount});
    return source;
}

[[nodiscard]] cf::ProjectJointPath make_path(std::string project_id,
    std::size_t receipt_month, std::string source_id, double receipt,
    double principal) {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.capital_draws.push_back(cf::MonthlyAmount{0U, 10.0});
    path.investor_receipts.push_back(cf::InvestorReceipt{receipt_month,
        std::move(source_id), receipt, principal});
    return path;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_terms() {
    cf::SuccessParticipationConfig terms;
    terms.scenario_label = "synthetic selected success-payoff terms";
    terms.source_note =
        "unit-test assertion that selected excess cash is scalable";
    terms.selected_nonprincipal_cash_is_contractually_scalable = true;
    terms.scalable_source_kinds = {cf::PortfolioCashSource::Commercial};
    return terms;
}

[[nodiscard]] cf::PortfolioConfig four_state_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "four-state capital-stack hand table";
    portfolio.source_note = "synthetic unit-test cash paths only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 24U;
    portfolio.projects = {
        {"culture-platform", cf::ProjectStage::Research, 10.0},
        {"bioprocess-scaleup", cf::ProjectStage::Pilot, 10.0},
    };

    cf::JointScenario success;
    success.id = "common-success";
    success.weight = 0.62;
    success.pool_costs = {{0U, 0.2}};
    success.cash_sources = {
        make_source("culture-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
        make_source("scaleup-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
    };
    success.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 13.0, 10.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 13.0,
            10.0),
    };

    cf::JointScenario culture_loss;
    culture_loss.id = "culture-loss-scaleup-success";
    culture_loss.weight = 0.18;
    culture_loss.pool_costs = {{0U, 0.2}};
    culture_loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
    };
    culture_loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 13.0,
            10.0),
    };

    cf::JointScenario scaleup_loss;
    scaleup_loss.id = "culture-success-scaleup-loss";
    scaleup_loss.weight = 0.18;
    scaleup_loss.pool_costs = {{0U, 0.2}};
    scaleup_loss.cash_sources = {
        make_source("culture-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    scaleup_loss.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 13.0, 10.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0, 2.0),
    };

    cf::JointScenario loss;
    loss.id = "common-loss";
    loss.weight = 0.02;
    loss.pool_costs = {{0U, 0.2}};
    loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0, 2.0),
    };
    portfolio.joint_scenarios = {scaleup_loss, loss, success, culture_loss};
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig four_state_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "four-state synthetic probability envelope";
    ambiguity.source_note = "invented probability bounds for unit tests";
    ambiguity.scenario_probabilities = {
        {"common-success", 0.50, 0.62, 0.70},
        {"culture-loss-scaleup-success", 0.10, 0.18, 0.25},
        {"culture-success-scaleup-loss", 0.10, 0.18, 0.25},
        {"common-loss", 0.01, 0.02, 0.10},
    };
    return ambiguity;
}

[[nodiscard]] cf::CapitalStackConfig stack_terms() {
    cf::CapitalStackConfig terms;
    terms.scenario_label = "three-tranche synthetic hand terms";
    terms.source_note = "invented priority caps for unit tests";
    terms.aggregate_commitment_is_fully_funded_at_par_at_month_zero = true;
    terms.subscription_reserve_is_zero_yield_and_lossless = true;
    terms.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    terms.pool_costs_are_additional_pro_rata_calls = true;
    terms.principal_cash_is_paid_most_senior_first = true;
    terms.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    terms.tranching_does_not_change_project_cash_or_gross_loss = true;
    terms.underlying_success_participation_fraction = 1.0;
    terms.tranches = {
        {"first-loss-residual", 0.0, 4.0, 0.0, 0.0, true},
        {"intermediate", 4.0, 10.0, 2.0, 0.0, false},
        {"senior", 10.0, 20.0, 1.0, 0.0, false},
    };
    return terms;
}

[[nodiscard]] const cf::CapitalStackScenarioResult& find_scenario(
    const cf::CapitalStackSummary& summary, std::string_view id) {
    const auto iterator = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const auto& scenario) {
            return scenario.scenario_id == id;
        });
    if (iterator == summary.scenarios.end()) {
        throw std::logic_error("test scenario not found");
    }
    return *iterator;
}

[[nodiscard]] const cf::CapitalStackTrancheScenarioResult& find_tranche(
    const cf::CapitalStackScenarioResult& scenario, std::string_view id) {
    const auto iterator = std::find_if(scenario.tranches.begin(),
        scenario.tranches.end(), [id](const auto& tranche) {
            return tranche.tranche_id == id;
        });
    if (iterator == scenario.tranches.end()) {
        throw std::logic_error("test tranche not found");
    }
    return *iterator;
}

void test_hand_waterfalls_and_cash_conservation() {
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        four_state_portfolio(), four_state_ambiguity(),
        participation_terms(), stack_terms());
    check(summary.legacy_v01_loss_layering_metrics_are_applicable &&
            std::all_of(summary.tranches.begin(), summary.tranches.end(),
                [](const auto& tranche) {
                    return tranche
                        .legacy_v01_loss_layering_metrics_are_applicable;
                }),
        "v0.1 marks its tranche loss-layering metrics applicable");

    const auto& success = find_scenario(summary, "common-success");
    const auto& success_junior =
        find_tranche(success, "first-loss-residual");
    const auto& success_mezz = find_tranche(success, "intermediate");
    const auto& success_senior = find_tranche(success, "senior");
    check(near(success_junior.principal_cash_distribution_million, 4.0) &&
            near(success_junior.nonprincipal_cash_distribution_million, 3.0) &&
            near(success_junior.total_distributions_million, 7.0),
        "success cash reaches the residual only after both priority caps");
    check(near(success_mezz.principal_cash_distribution_million, 6.0) &&
            near(success_mezz.nonprincipal_cash_distribution_million, 2.0) &&
            near(success_mezz.total_distributions_million, 8.0),
        "success intermediate cash matches the hand waterfall");
    check(near(success_senior.principal_cash_distribution_million, 10.0) &&
            near(success_senior.nonprincipal_cash_distribution_million, 1.0) &&
            near(success_senior.total_distributions_million, 11.0),
        "success senior cash matches the hand waterfall");

    const auto& one_loss =
        find_scenario(summary, "culture-loss-scaleup-success");
    const auto& loss_junior = find_tranche(one_loss, "first-loss-residual");
    const auto& loss_mezz = find_tranche(one_loss, "intermediate");
    const auto& loss_senior = find_tranche(one_loss, "senior");
    check(near(loss_junior.realized_principal_loss_million, 4.0) &&
            near(loss_junior.total_distributions_million, 0.0),
        "first-loss tranche absorbs the first four and receives no one-loss cash");
    check(near(loss_mezz.realized_principal_loss_million, 4.0) &&
            near(loss_mezz.principal_cash_distribution_million, 2.0) &&
            near(loss_mezz.nonprincipal_cash_distribution_million, 2.0),
        "intermediate tranche absorbs the next four and retains its cap");
    check(near(loss_senior.realized_principal_loss_million, 0.0) &&
            near(loss_senior.total_distributions_million, 11.0),
        "senior is unimpaired in a single-project loss state");

    const auto& common_loss = find_scenario(summary, "common-loss");
    check(near(find_tranche(common_loss, "first-loss-residual")
                   .realized_principal_loss_million,
              4.0) &&
            near(find_tranche(common_loss, "intermediate")
                   .realized_principal_loss_million,
              6.0) &&
            near(find_tranche(common_loss, "senior")
                   .realized_principal_loss_million,
              6.0),
        "common-loss state exhausts junior and intermediate before senior");
    check(near(find_tranche(common_loss, "senior")
                   .principal_cash_distribution_million,
              4.0),
        "all common-loss recovery principal pays the senior tranche");

    check(near(summary.maximum_commitment_identity_error_million, 0.0) &&
            near(summary.maximum_subscription_reconciliation_error_million,
                0.0) &&
            near(summary.maximum_pool_cost_call_reconciliation_error_million,
                0.0) &&
            near(summary.maximum_principal_distribution_reconciliation_error_million,
                0.0) &&
            near(summary.maximum_nonprincipal_distribution_reconciliation_error_million,
                0.0) &&
            near(summary.maximum_realized_loss_reconciliation_error_million,
                0.0) &&
            near(summary.maximum_nominal_net_cash_reconciliation_error_million,
                0.0),
        "every pathwise accounting control reconciles");
}

void test_robust_tranche_risk_and_return_ranges() {
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        four_state_portfolio(), four_state_ambiguity(),
        participation_terms(), stack_terms());
    const auto& junior = summary.tranches[0];
    const auto& mezz = summary.tranches[1];
    const auto& senior = summary.tranches[2];

    check(near(junior.expected_total_distributions_million.minimum.value, 3.5) &&
            near(junior.expected_total_distributions_million.central, 4.34) &&
            near(junior.expected_total_distributions_million.maximum.value, 4.9),
        "residual robust distributions match the bounded-probability hand table");
    check(near(mezz.expected_total_distributions_million.minimum.value, 5.6) &&
            near(mezz.expected_total_distributions_million.central, 6.4) &&
            near(mezz.expected_total_distributions_million.maximum.value, 6.76),
        "intermediate robust distributions match the hand table");
    check(near(senior.expected_total_distributions_million.minimum.value, 10.3) &&
            near(senior.expected_total_distributions_million.central, 10.86) &&
            near(senior.expected_total_distributions_million.maximum.value,
                10.93),
        "senior robust distributions match the hand table");

    check(near(junior.expected_npv_at_tranche_hurdle_million.minimum.value,
              -0.54) &&
            near(junior.expected_npv_at_tranche_hurdle_million.central, 0.30) &&
            near(mezz.expected_npv_at_tranche_hurdle_million.minimum.value,
                -0.46) &&
            near(mezz.expected_npv_at_tranche_hurdle_million.central, 0.34) &&
            near(senior.expected_npv_at_tranche_hurdle_million.minimum.value,
                0.20) &&
            near(senior.expected_npv_at_tranche_hurdle_million.central, 0.76),
        "robust NPV retains each tranche's complete probability witness");
    check(junior.central_expected_npv_meets_hurdle &&
            !junior.robust_expected_npv_meets_hurdle &&
            mezz.central_expected_npv_meets_hurdle &&
            !mezz.robust_expected_npv_meets_hurdle &&
            senior.central_expected_npv_meets_hurdle &&
            senior.robust_expected_npv_meets_hurdle,
        "the engine distinguishes central attraction from robust attraction");

    check(near(junior.principal_impairment_probability.minimum.value, 0.30) &&
            near(junior.principal_impairment_probability.central, 0.38) &&
            near(junior.principal_impairment_probability.maximum.value, 0.50) &&
            near(senior.principal_impairment_probability.minimum.value, 0.01) &&
            near(senior.principal_impairment_probability.central, 0.02) &&
            near(senior.principal_impairment_probability.maximum.value, 0.10),
        "attachment redistributes impairment probability without erasing loss");
    check(near(senior.expected_realized_principal_loss_million.minimum.value,
              0.06) &&
            near(senior.expected_realized_principal_loss_million.central, 0.12) &&
            near(senior.expected_realized_principal_loss_million.maximum.value,
                0.60),
        "senior expected loss range keeps common-loss exposure visible");
    check(near(junior.principal_exhaustion_probability.minimum.value, 0.30) &&
            near(junior.principal_exhaustion_probability.central, 0.38) &&
            near(junior.principal_exhaustion_probability.maximum.value, 0.50) &&
            near(mezz.principal_exhaustion_probability.minimum.value, 0.01) &&
            near(mezz.principal_exhaustion_probability.central, 0.02) &&
            near(mezz.principal_exhaustion_probability.maximum.value, 0.10) &&
            near(senior.principal_exhaustion_probability.minimum.value, 0.0) &&
            near(senior.principal_exhaustion_probability.central, 0.0) &&
            near(senior.principal_exhaustion_probability.maximum.value, 0.0),
        "exhaustion probabilities distinguish the modeled 6-of-10 senior tail loss");
    check(junior.principal_cash_weighted_average_life_years.has_value() &&
            senior.principal_cash_weighted_average_life_years.has_value() &&
            near(junior.principal_cash_weighted_average_life_years->central,
                2.0) &&
            near(senior.principal_cash_weighted_average_life_years->minimum.value,
                1.8723404255319149) &&
            near(senior.principal_cash_weighted_average_life_years->central,
                18.96 / 9.88) &&
            near(senior.principal_cash_weighted_average_life_years->maximum.value,
                1.937625754527163),
        "WAL divides numerator and denominator under one feasible probability witness");
    check(near(junior.principal_loss_expected_shortfall_95_million.minimum.value,
              4.0) &&
            near(junior.principal_loss_expected_shortfall_99_million.maximum.value,
                4.0),
        "first-loss tail severity is not diluted by probability labels");
    check(summary.maximum_endpoint_probability_error < 1.0e-12,
        "all robust endpoints retain feasible probability witnesses");
}

void test_declared_underlying_q_target_is_reported_not_assumed() {
    cf::SuccessParticipationConfig terms = participation_terms();
    terms.target_worst_expected_npv_million = 1.0;
    const cf::CapitalStackSummary failing = cf::evaluate_capital_stack(
        four_state_portfolio(), four_state_ambiguity(), terms, stack_terms());
    check(near(failing.underlying_target_worst_expected_npv_million, 1.0) &&
            !failing.selected_underlying_success_participation_meets_target &&
            near(failing.selected_underlying_target_gap_million,
                1.0 -
                    failing.expected_underlying_on_demand_npv_million.minimum.value),
        "a declared q that misses the robust pool target remains analyzable and is disclosed as failing");

    terms.target_worst_expected_npv_million =
        failing.expected_underlying_on_demand_npv_million.minimum.value;
    const cf::CapitalStackSummary passing = cf::evaluate_capital_stack(
        four_state_portfolio(), four_state_ambiguity(), terms, stack_terms());
    check(passing.selected_underlying_success_participation_meets_target &&
            near(passing.selected_underlying_target_gap_million, 0.0),
        "a declared q at the robust target is reported as meeting it without re-solving q");
    check(near(passing.underlying_success_participation_fraction,
              stack_terms().underlying_success_participation_fraction),
        "target disclosure never changes the declared underlying q");
}

void test_staged_draw_reserve_and_continuing_exposure() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "staged draw with continuing exposure";
    portfolio.source_note = "synthetic reserve unit-test path";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units";
    portfolio.horizon_months = 12U;
    portfolio.annual_physical_hurdle_rate = 0.10;
    portfolio.projects = {
        {"continuing-project", cf::ProjectStage::Research, 10.0}};
    cf::ProjectJointPath path;
    path.project_id = "continuing-project";
    path.resolution = cf::ProjectPathResolution::Continuing;
    path.capital_draws = {{6U, 4.0}};
    cf::JointScenario scenario;
    scenario.id = "continuing";
    scenario.weight = 1.0;
    scenario.project_paths = {path};
    // The shared participation terms explicitly select commercial cash. A
    // zero-budget source keeps that taxonomy assertion valid without adding
    // an investor receipt or any economic value to this reserve-only path.
    scenario.cash_sources = {
        cf::ScenarioCashSource{"unused-commercial",
            cf::PortfolioCashSource::Commercial, {}}};
    portfolio.joint_scenarios = {scenario};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point probability for reserve test";
    ambiguity.source_note = "synthetic point envelope";
    ambiguity.scenario_probabilities = {
        {"continuing", 1.0, 1.0, 1.0}};
    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches = {
        {"first-loss-residual", 0.0, 3.0, 0.0, 0.0, true},
        {"senior", 3.0, 10.0, 0.0, 0.0, false},
    };

    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, ambiguity, participation_terms(), terms);
    const auto& result = summary.scenarios.front();
    const auto& junior = find_tranche(result, "first-loss-residual");
    const auto& senior = find_tranche(result, "senior");
    check(near(result.unused_commitment_returned_at_horizon_million, 6.0) &&
            near(junior.principal_cash_distribution_million, 0.0) &&
            near(senior.principal_cash_distribution_million, 6.0) &&
            near(senior.underlying_principal_cash_distribution_million, 0.0) &&
            near(senior.unused_reserve_principal_return_million, 6.0),
        "unused prefunded commitment returns through the senior-first principal waterfall");
    check(near(junior.realized_principal_loss_million, 0.0) &&
            near(senior.realized_principal_loss_million, 0.0) &&
            near(junior.unresolved_principal_exposure_million, 3.0) &&
            near(senior.unresolved_principal_exposure_million, 1.0),
        "continuing exposure occupies the stack without being called loss");
    check(result.prefunding_drag_npv_million > 0.0 &&
            near(result.underlying_nominal_net_cash_million,
                result.stack_nominal_net_cash_million),
        "full funding exposes timing drag while conserving nominal cash");
    check(!summary.tranches[0]
               .principal_cash_weighted_average_life_years.has_value() &&
            summary.tranches[1]
                .principal_cash_weighted_average_life_years.has_value() &&
            near(summary.tranches[1]
                     .principal_cash_weighted_average_life_years->central,
                1.0),
        "WAL is absent without principal cash and includes reserve return timing when defined");
    check(near(summary.maximum_reserve_roll_forward_error_million, 0.0) &&
            near(summary.maximum_reserve_shortfall_million, 0.0),
        "reserve ledger closes without a shortfall");
}

void test_same_month_principal_sources_are_attributed_pro_rata() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "horizon source attribution";
    portfolio.source_note = "synthetic same-month source unit-test path";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units";
    portfolio.horizon_months = 12U;
    portfolio.projects = {
        {"attribution-project", cf::ProjectStage::Research, 10.0}};

    cf::ProjectJointPath path;
    path.project_id = "attribution-project";
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.capital_draws = {{0U, 4.0}};
    path.investor_receipts = {
        {12U, "project-recovery", 2.0, 2.0}};
    cf::JointScenario scenario;
    scenario.id = "horizon-recovery";
    scenario.weight = 1.0;
    scenario.cash_sources = {
        make_source("project-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        cf::ScenarioCashSource{"unused-commercial",
            cf::PortfolioCashSource::Commercial, {}},
    };
    scenario.project_paths = {path};
    portfolio.joint_scenarios = {scenario};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point probability for source attribution";
    ambiguity.source_note = "synthetic point envelope";
    ambiguity.scenario_probabilities = {
        {"horizon-recovery", 1.0, 1.0, 1.0}};
    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches = {
        {"first-loss-residual", 0.0, 3.0, 0.0, 0.0, true},
        {"senior", 3.0, 10.0, 0.0, 0.0, false},
    };

    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, ambiguity, participation_terms(), terms);
    const auto& result = summary.scenarios.front();
    const auto& junior = find_tranche(result, "first-loss-residual");
    const auto& senior = find_tranche(result, "senior");
    check(near(senior.principal_cash_distribution_million, 7.0) &&
            near(senior.underlying_principal_cash_distribution_million, 1.75) &&
            near(senior.unused_reserve_principal_return_million, 5.25) &&
            near(junior.principal_cash_distribution_million, 1.0) &&
            near(junior.underlying_principal_cash_distribution_million, 0.25) &&
            near(junior.unused_reserve_principal_return_million, 0.75),
        "same-month project and reserve principal share equal seniority pro rata");
}

void test_compensated_monthly_and_tranche_ledgers() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "capital-stack dynamic-range ledger";
    portfolio.source_note = "synthetic compensated-summation fixture";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units";
    portfolio.horizon_months = 2'400U;
    portfolio.projects = {
        {"ledger-project", cf::ProjectStage::Research, 2.0}};

    cf::ProjectJointPath path;
    path.project_id = "ledger-project";
    path.resolution = cf::ProjectPathResolution::Continuing;
    cf::JointScenario scenario;
    scenario.id = "ledger-state";
    scenario.weight = 1.0;
    scenario.pool_costs.push_back({0U, 1.0e6});
    for (std::size_t month = 1U; month <= 2'400U; ++month) {
        // One same-month record tests the monthly pool ledger; the dated
        // record tests each tranche's cumulative ledger after the large call.
        scenario.pool_costs.push_back({0U, 4.0e-11});
        scenario.pool_costs.push_back({month, 4.0e-11});
    }
    scenario.cash_sources = {
        cf::ScenarioCashSource{"unused-commercial",
            cf::PortfolioCashSource::Commercial, {}}};
    scenario.project_paths = {path};
    portfolio.joint_scenarios = {scenario};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point probability for ledger test";
    ambiguity.source_note = "synthetic point envelope";
    ambiguity.scenario_probabilities = {
        {"ledger-state", 1.0, 1.0, 1.0}};
    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches = {
        {"first-loss-residual", 0.0, 1.0, 0.0, 0.0, true},
        {"senior", 1.0, 2.0, 0.0, 0.0, false},
    };

    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, ambiguity, participation_terms(), terms);
    check(summary.maximum_pool_cost_call_reconciliation_error_million <
                1.0e-8 &&
            summary.maximum_stack_npv_reconciliation_error_million < 1.0e-8,
        "compensated ledgers retain small same-month and later cash after a large call");
}

void test_compensated_reserve_ledger() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "capital-stack dynamic-range reserve";
    portfolio.source_note = "synthetic compensated-reserve fixture";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units";
    portfolio.horizon_months = 2'400U;
    portfolio.projects = {
        {"reserve-project", cf::ProjectStage::Research, 1.0e6}};

    cf::ProjectJointPath path;
    path.project_id = "reserve-project";
    path.resolution = cf::ProjectPathResolution::Continuing;
    for (std::size_t month = 1U; month <= 2'400U; ++month) {
        path.capital_draws.push_back({month, 4.0e-11});
    }
    cf::JointScenario scenario;
    scenario.id = "reserve-state";
    scenario.weight = 1.0;
    scenario.cash_sources = {
        cf::ScenarioCashSource{"unused-commercial",
            cf::PortfolioCashSource::Commercial, {}}};
    scenario.project_paths = {std::move(path)};
    portfolio.joint_scenarios = {std::move(scenario)};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point probability for reserve test";
    ambiguity.source_note = "synthetic point envelope";
    ambiguity.scenario_probabilities = {
        {"reserve-state", 1.0, 1.0, 1.0}};
    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches = {
        {"first-loss-residual", 0.0, 5.0e5, 0.0, 0.0, true},
        {"senior", 5.0e5, 1.0e6, 0.0, 0.0, false},
    };

    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, ambiguity, participation_terms(), terms);
    check(summary.maximum_reserve_roll_forward_error_million < 1.0e-8 &&
            near(summary.maximum_reserve_shortfall_million, 0.0),
        "compensated reserve retains many tiny draws against a large commitment");
}

void test_compensated_waterfall_remaining_balance() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "capital-stack dynamic-range waterfall";
    portfolio.source_note = "synthetic compensated-waterfall fixture";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units";
    portfolio.horizon_months = 1U;
    portfolio.projects = {
        {"waterfall-project", cf::ProjectStage::Research, 1.0e6}};

    cf::ProjectJointPath path;
    path.project_id = "waterfall-project";
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.capital_draws = {{0U, 1.0e6}};
    path.investor_receipts = {
        {1U, "commercial-principal", 1.0e6, 1.0e6},
        {1U, "commercial-upside", 1.0e6, 0.0}};
    cf::JointScenario scenario;
    scenario.id = "waterfall-state";
    scenario.weight = 1.0;
    scenario.cash_sources = {
        make_source("commercial-principal",
            cf::PortfolioCashSource::Commercial, 1U, 1.0e6),
        make_source("commercial-upside",
            cf::PortfolioCashSource::Commercial, 1U, 1.0e6)};
    scenario.project_paths = {std::move(path)};
    portfolio.joint_scenarios = {std::move(scenario)};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point probability for waterfall test";
    ambiguity.source_note = "synthetic point envelope";
    ambiguity.scenario_probabilities = {
        {"waterfall-state", 1.0, 1.0, 1.0}};

    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches.clear();
    for (std::size_t index = 0U; index < 128U; ++index) {
        const double attachment =
            1.0e6 * static_cast<double>(index) / 128.0;
        const double detachment =
            1.0e6 * static_cast<double>(index + 1U) / 128.0;
        terms.tranches.push_back(cf::CapitalStackTrancheConfig{
            "waterfall-layer-" + std::to_string(index), attachment,
            detachment, index == 0U ? 0.0 : 5.0e-11, 0.0,
            index == 0U});
    }

    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, ambiguity, participation_terms(), terms);
    const double expected_residual = 1.0e6 - 127.0 * 5.0e-11;
    check(summary.scenarios.size() == 1U &&
            summary.scenarios[0].tranches.size() == 128U &&
            std::abs(summary.scenarios[0].tranches[0]
                         .nonprincipal_cash_distribution_million -
                     expected_residual) < 1.0e-8 &&
            summary.maximum_nonprincipal_distribution_reconciliation_error_million <
                1.0e-8 &&
            summary.maximum_nominal_net_cash_reconciliation_error_million < 1.0e-8,
        "compensated waterfall preserves tiny priority caps against a large receipt");
}

void test_validation_rejects_economic_ambiguity() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity = four_state_ambiguity();
    const cf::SuccessParticipationConfig participation = participation_terms();

    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches[1].attachment_million = 5.0;
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation, terms); },
        "a gap in the funded loss stack is rejected");

    terms = stack_terms();
    terms.tranches[0].is_first_loss_residual = false;
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation, terms); },
        "a stack without one attachment-zero residual is rejected");

    terms = stack_terms();
    terms.tranches = {
        {"microscopic-residual", 0.0, 1.0e-310, 0.0, 0.0, true},
        {"senior", 1.0e-310, 20.0, 0.0, 0.0, false},
    };
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation, terms); },
        "a tranche below one base currency unit is numerically unsupported");

    terms = stack_terms();
    terms.tranches.clear();
    double prior_detachment = 0.0;
    for (std::size_t index = 0U; index < 128U; ++index) {
        const double detachment =
            20.0 * static_cast<double>(index + 1U) / 128.0;
        const double attachment = index == 0U
            ? 0.0
            : prior_detachment + 5.0e-11;
        terms.tranches.push_back(cf::CapitalStackTrancheConfig{
            "gapped-layer-" + std::to_string(index), attachment, detachment,
            0.0, 0.0, index == 0U});
        prior_detachment = detachment;
    }
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation, terms); },
        "duplicated attachment coordinates must match exactly without cumulative gaps");

    terms = stack_terms();
    terms.premium_discount_or_fair_value_is_claimed = true;
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation, terms); },
        "a premium, discount, or fair-value claim is outside v0.1");

    terms = stack_terms();
    terms.subscription_reserve_is_zero_yield_and_lossless = false;
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation, terms); },
        "reserve economics cannot remain implicit");

    cf::PortfolioConfig already_tranched = portfolio;
    already_tranched.loss_layers = {
        {"junior", 0.0, 4.0}, {"upper", 4.0, 20.0}};
    terms = stack_terms();
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  already_tranched, ambiguity, participation, terms); },
        "capital stack cannot silently override underlying loss layers");
}

void test_explicit_contractual_ledger_is_rejected_at_v01_boundary() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label =
        "explicit contractual ledger capital-stack boundary";
    portfolio.source_note =
        "synthetic above-par position used only to test fail-closed admission";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at purchase";
    portfolio.horizon_months = 1U;

    cf::PortfolioProject project;
    project.id = "explicit-claim";
    project.stage = cf::ProjectStage::FirstIndustrial;
    project.commitment_million = 12.0;
    project.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = 10.0;
    portfolio.projects = {project};

    cf::ProjectJointPath path;
    path.project_id = "explicit-claim";
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.investor_outlays = {{0U,
        cf::InvestorOutlayPurpose::ClaimPurchasePrice, 12.0}};
    path.investor_receipts = {
        {1U, "contractual-principal", 10.0, 10.0}};
    path.principal_movements = {{0U,
        cf::PrincipalMovementKind::FundedPrincipalAddition, 10.0}};

    cf::JointScenario scenario;
    scenario.id = "explicit-performing";
    scenario.weight = 1.0;
    scenario.cash_sources = {make_source("contractual-principal",
        cf::PortfolioCashSource::Commercial, 1U, 10.0)};
    scenario.project_paths = {path};
    portfolio.joint_scenarios = {scenario};
    cf::validate_portfolio_config(portfolio);

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label =
        "point probability for explicit capital-stack boundary";
    ambiguity.source_note = "synthetic exact probability unit test";
    ambiguity.scenario_probabilities = {
        {"explicit-performing", 1.0, 1.0, 1.0}};

    bool rejected_at_explicit_boundary = false;
    try {
        cf::validate_capital_stack_config(portfolio, ambiguity,
            participation_terms(), stack_terms());
    } catch (const std::invalid_argument& error) {
        rejected_at_explicit_boundary = std::string_view(error.what()).find(
            "capital-stack v0.1 cannot consume explicit contractual principal ledgers") !=
            std::string_view::npos;
    } catch (...) {
        // The precise v0.1 boundary must be the reason for rejection.
    }
    check(rejected_at_explicit_boundary,
        "capital-stack v0.1 rejects an otherwise valid explicit-contractual-ledger portfolio at its stated accounting boundary");
}

void test_stack_rejects_aggregate_project_tolerance_dust() {
    const auto make_portfolio = [](bool add_principal_dust) {
        cf::PortfolioConfig portfolio;
        portfolio.scenario_label = "aggregate tolerance dust";
        portfolio.source_note = "synthetic stack interoperability fixture";
        portfolio.currency_label = "TEST";
        portfolio.monetary_basis = "constant test units";
        portfolio.horizon_months = 1U;
        cf::JointScenario scenario;
        scenario.id = "aggregate-dust";
        scenario.weight = 1.0;
        scenario.cash_sources = {make_source("shared-commercial",
            cf::PortfolioCashSource::Commercial, 1U, 129.0)};
        for (std::size_t index = 0U; index < 128U; ++index) {
            const std::string id = "dust-project-" + std::to_string(index);
            portfolio.projects.push_back(
                {id, cf::ProjectStage::Research, 1.0});
            cf::ProjectJointPath path;
            path.project_id = id;
            path.resolution = add_principal_dust
                ? cf::ProjectPathResolution::Resolved
                : cf::ProjectPathResolution::Continuing;
            path.capital_draws = {
                {0U, add_principal_dust ? 1.0 : 1.0 + 8.0e-11}};
            if (add_principal_dust) {
                path.investor_receipts = {{1U, "shared-commercial",
                    1.0 + 8.0e-11, 1.0 + 8.0e-11}};
            }
            scenario.project_paths.push_back(std::move(path));
        }
        portfolio.joint_scenarios = {std::move(scenario)};
        return portfolio;
    };
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point probability for aggregate dust";
    ambiguity.source_note = "synthetic point envelope";
    ambiguity.scenario_probabilities = {
        {"aggregate-dust", 1.0, 1.0, 1.0}};
    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches = {
        {"first-loss-residual", 0.0, 64.0, 0.0, 0.0, true},
        {"senior", 64.0, 128.0, 0.0, 0.0, false},
    };

    cf::PortfolioConfig draw_dust = make_portfolio(false);
    cf::validate_portfolio_config(draw_dust);
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(draw_dust, ambiguity,
                  participation_terms(), terms); },
        "per-project draw tolerances cannot overdraw the aggregate funded reserve");
    cf::PortfolioConfig principal_dust = make_portfolio(true);
    cf::validate_portfolio_config(principal_dust);
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(principal_dust, ambiguity,
                  participation_terms(), terms); },
        "per-project principal tolerances cannot invent aggregate stack principal");
}

void test_resource_bound_includes_scenario_tranche_month_work() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "capital-stack combined work guard";
    portfolio.source_note = "synthetic resource-bound fixture";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units";
    portfolio.horizon_months = 2'400U;
    portfolio.projects = {
        {"bounded-project", cf::ProjectStage::Research, 10.0}};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "resource-bound probability points";
    ambiguity.source_note = "synthetic exact weights";
    for (std::size_t index = 0U; index < 7U; ++index) {
        const std::string id = "resource-state-" + std::to_string(index);
        cf::ProjectJointPath path;
        path.project_id = "bounded-project";
        path.resolution = cf::ProjectPathResolution::Continuing;
        path.capital_draws = {{0U, 1.0}};
        cf::JointScenario scenario;
        scenario.id = id;
        scenario.weight = 1.0 / 7.0;
        scenario.cash_sources = {
            cf::ScenarioCashSource{"unused-commercial",
                cf::PortfolioCashSource::Commercial, {}}};
        scenario.project_paths = {path};
        portfolio.joint_scenarios.push_back(std::move(scenario));
        ambiguity.scenario_probabilities.push_back(
            {id, 1.0 / 7.0, 1.0 / 7.0, 1.0 / 7.0});
    }

    cf::CapitalStackConfig terms = stack_terms();
    terms.tranches.clear();
    for (std::size_t index = 0U; index < 128U; ++index) {
        const double attachment = 10.0 * static_cast<double>(index) / 128.0;
        const double detachment =
            10.0 * static_cast<double>(index + 1U) / 128.0;
        terms.tranches.push_back(cf::CapitalStackTrancheConfig{
            "work-layer-" + std::to_string(index), attachment, detachment,
            0.0, 0.0, index == 0U});
    }
    expect_invalid_argument(
        [&] { cf::validate_capital_stack_config(
                  portfolio, ambiguity, participation_terms(), terms); },
        "scenario by tranche by month work is bounded before allocation");
}

} // namespace

int main() {
    test_hand_waterfalls_and_cash_conservation();
    test_robust_tranche_risk_and_return_ranges();
    test_declared_underlying_q_target_is_reported_not_assumed();
    test_staged_draw_reserve_and_continuing_exposure();
    test_same_month_principal_sources_are_attributed_pro_rata();
    test_compensated_monthly_and_tranche_ledgers();
    test_compensated_reserve_ledger();
    test_compensated_waterfall_remaining_balance();
    test_validation_rejects_economic_ambiguity();
    test_explicit_contractual_ledger_is_rejected_at_v01_boundary();
    test_stack_rejects_aggregate_project_tolerance_dust();
    test_resource_bound_includes_scenario_tranche_month_work();
    if (failures != 0) {
        std::cerr << failures << " capital-stack test(s) failed\n";
        return 1;
    }
    std::cout << "capital-stack tests passed\n";
    return 0;
}
