// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/success_participation.hpp>

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
    portfolio.scenario_label = "four-state success participation hand table";
    portfolio.source_note = "synthetic unit-test cash paths only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 24U;
    portfolio.projects = {
        cf::PortfolioProject{
            "culture-platform", cf::ProjectStage::Research, 10.0},
        cf::PortfolioProject{
            "bioprocess-scaleup", cf::ProjectStage::Pilot, 10.0},
    };

    cf::JointScenario success;
    success.id = "common-success";
    success.weight = 0.62;
    success.pool_costs = {cf::MonthlyAmount{0U, 0.2}};
    success.cash_sources = {
        make_source("culture-commercial",
            cf::PortfolioCashSource::Commercial, 24U, 13.0),
        make_source("scaleup-commercial",
            cf::PortfolioCashSource::Commercial, 24U, 13.0),
    };
    success.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 13.0,
            10.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 13.0,
            10.0),
    };

    cf::JointScenario culture_loss;
    culture_loss.id = "culture-loss-scaleup-success";
    culture_loss.weight = 0.18;
    culture_loss.pool_costs = {cf::MonthlyAmount{0U, 0.2}};
    culture_loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-commercial",
            cf::PortfolioCashSource::Commercial, 24U, 13.0),
    };
    culture_loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 13.0,
            10.0),
    };

    cf::JointScenario scaleup_loss;
    scaleup_loss.id = "culture-success-scaleup-loss";
    scaleup_loss.weight = 0.18;
    scaleup_loss.pool_costs = {cf::MonthlyAmount{0U, 0.2}};
    scaleup_loss.cash_sources = {
        make_source("culture-commercial",
            cf::PortfolioCashSource::Commercial, 24U, 13.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    scaleup_loss.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 13.0,
            10.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0,
            2.0),
    };

    cf::JointScenario loss;
    loss.id = "common-loss";
    loss.weight = 0.02;
    loss.pool_costs = {cf::MonthlyAmount{0U, 0.2}};
    loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0,
            2.0),
    };

    // Deliberately not identifier order.
    portfolio.joint_scenarios =
        {success, culture_loss, scaleup_loss, loss};
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

[[nodiscard]] cf::PortfolioConfig witness_switch_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "two-state robust witness switch";
    portfolio.source_note = "synthetic threshold unit-test paths only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 12U;
    portfolio.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 10.0}};

    cf::JointScenario deep;
    deep.id = "deep-upside";
    deep.weight = 0.5;
    deep.cash_sources = {make_source("commercial",
        cf::PortfolioCashSource::Commercial, 12U, 20.0)};
    deep.project_paths = {
        make_path("project", 12U, "commercial", 20.0, 0.0)};

    cf::JointScenario moderate;
    moderate.id = "moderate";
    moderate.weight = 0.5;
    moderate.cash_sources = {make_source("commercial",
        cf::PortfolioCashSource::Commercial, 12U, 11.0)};
    moderate.project_paths = {
        make_path("project", 12U, "commercial", 11.0, 8.0)};

    portfolio.joint_scenarios = {moderate, deep};
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig witness_switch_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "two-state full-simplex ambiguity";
    ambiguity.source_note = "synthetic witness-switch probability set";
    ambiguity.scenario_probabilities = {
        {"moderate", 0.0, 0.5, 1.0},
        {"deep-upside", 0.0, 0.5, 1.0},
    };
    return ambiguity;
}

void test_current_four_state_terms_are_honestly_unattainable() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::SuccessParticipationSummary summary =
        cf::solve_success_participation(portfolio,
            four_state_ambiguity(), participation_terms());

    check(summary.status == cf::SuccessParticipationSolveStatus::
            UnattainableAtFullParticipation &&
            summary.reported_fraction == 1.0 &&
            !summary.feasible_fraction_upper_bound.has_value(),
        "an unattainable target reports the economically strongest q=1 terms");
    check(near(summary.q0.expected_npv_million.minimum.value, -5.0) &&
            near(summary.q1.expected_npv_million.minimum.value, -0.8) &&
            near(summary.target_gap_at_full_participation_million, 0.8),
        "the four-state robust NPV follows W(q)=-5+4.2q");
    check(near(summary.full_participation_present_value_million.minimum.value,
              4.2) &&
            near(summary.full_participation_present_value_million.central,
              4.8),
        "selected commercial payoff is projected under the same probability envelope");
    check(summary.q0.expected_npv_million.minimum.scenario_weights.size() ==
                4U &&
            summary.q1.expected_npv_million.minimum.scenario_weights.size() ==
                4U &&
            summary.reported.expected_npv_million.minimum.scenario_weights.size() ==
                4U &&
            summary.scenario_probability_bounds.front().scenario_id ==
                "common-loss",
        "q0, q1, and reported points retain canonical keyed witnesses");
    check(summary.maximum_q1_cash_reconstruction_error_million <= 1.0e-9 &&
            summary.maximum_principal_loss_reconciliation_error_million <=
                1.0e-9 &&
            summary.maximum_source_capacity_violation_million <= 1.0e-12 &&
            summary.maximum_witness_reconciliation_error_million <= 1.0e-9 &&
            summary.maximum_endpoint_probability_error <= 1.0e-12,
        "cash, loss, capacity, and probability controls reconcile");

    for (const cf::SuccessParticipationScenarioResult& scenario :
         summary.scenarios) {
        check(near(scenario.npv_at_reported_fraction_million,
                  scenario.configured_q1_npv_million),
            "unattainable scenario reporting remains meaningful at q=1");
    }
}

void test_adjustment_scales_only_selected_nonprincipal_cash() {
    cf::PortfolioConfig portfolio = four_state_portfolio();
    // A non-selected recovery excess remains a fixed receipt at every q.
    cf::JointScenario& common_loss = portfolio.joint_scenarios.back();
    common_loss.cash_sources[0].cash_available[0].amount_million = 3.0;
    common_loss.project_paths[0].investor_receipts[0].amount_million = 3.0;

    const cf::SuccessParticipationConfig terms = participation_terms();
    const cf::PortfolioConfig q0 =
        cf::apply_success_participation_fraction(portfolio, terms, 0.0);
    const cf::PortfolioConfig half =
        cf::apply_success_participation_fraction(portfolio, terms, 0.5);
    const cf::PortfolioConfig q1 =
        cf::apply_success_participation_fraction(portfolio, terms, 1.0);

    check(near(q0.joint_scenarios[0].project_paths[0]
                   .investor_receipts[0]
                   .amount_million,
              10.0) &&
            near(half.joint_scenarios[0].project_paths[0]
                   .investor_receipts[0]
                   .amount_million,
              11.5) &&
            near(q1.joint_scenarios[0].project_paths[0]
                   .investor_receipts[0]
                   .amount_million,
              13.0),
        "selected commercial excess is interpolated between off and configured terms");
    check(near(q0.joint_scenarios.back().project_paths[0]
                   .investor_receipts[0]
                   .amount_million,
              3.0) &&
            near(half.joint_scenarios.back().project_paths[0]
                   .investor_receipts[0]
                   .amount_million,
              3.0),
        "unselected recovery excess is not relabeled or scaled");
    check(q1.joint_scenarios[0].project_paths[0].investor_receipts[0]
              .amount_million ==
            portfolio.joint_scenarios[0].project_paths[0]
                .investor_receipts[0]
                .amount_million,
        "q=1 exactly preserves the configured receipt schedule");
}

void test_solver_reprojects_combined_npv_when_witness_switches() {
    const cf::PortfolioConfig portfolio = witness_switch_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        witness_switch_ambiguity();
    const cf::SuccessParticipationConfig terms = participation_terms();
    const cf::SuccessParticipationSummary summary =
        cf::solve_success_participation(portfolio, ambiguity, terms);

    const double expected = 2.0 / 3.0;
    check(summary.status == cf::SuccessParticipationSolveStatus::
            CertifiedInteriorBracket &&
            summary.failing_fraction_lower_bound.has_value() &&
            summary.feasible_fraction_upper_bound.has_value() &&
            *summary.failing_fraction_lower_bound <= expected &&
            *summary.feasible_fraction_upper_bound >= expected &&
            *summary.feasible_fraction_upper_bound -
                    *summary.failing_fraction_lower_bound <
                1.0e-12,
        "robust solver certifies the true two-thirds witness-switch threshold");
    check(near(summary.q0.expected_npv_million.minimum.value, -10.0) &&
            near(summary.q1.expected_npv_million.minimum.value, 1.0) &&
            summary.reported.expected_npv_million.minimum.value >= 0.0,
        "reported upper bracket is feasible under the complete combined NPV objective");
    check(summary.q0.expected_npv_million.minimum.scenario_weights !=
            summary.reported.expected_npv_million.minimum.scenario_weights,
        "the adverse probability witness is allowed to switch as participation changes");
}

void test_dated_payoff_present_value_is_not_nominal_cash() {
    cf::PortfolioConfig portfolio = four_state_portfolio();
    portfolio.annual_physical_hurdle_rate = 0.10;
    const cf::SuccessParticipationSummary summary =
        cf::solve_success_participation(portfolio,
            four_state_ambiguity(), participation_terms());

    check(near(summary.full_participation_nominal_million.minimum.value,
              4.2) &&
            near(summary.full_participation_nominal_million.central, 4.8) &&
            near(summary.full_participation_nominal_million.maximum.value,
              5.07),
        "nominal success participation retains its dated cash amounts");
    check(near(summary.full_participation_present_value_million.minimum.value,
              420.0 / 121.0) &&
            near(summary.full_participation_present_value_million.central,
              480.0 / 121.0) &&
            near(summary.full_participation_present_value_million.maximum.value,
              507.0 / 121.0),
        "two-year success payoff is discounted at the declared physical hurdle");

    const auto success = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [](const auto& scenario) {
            return scenario.scenario_id == "common-success";
        });
    const auto single = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [](const auto& scenario) {
            return scenario.scenario_id ==
                "culture-loss-scaleup-success";
        });
    check(success != summary.scenarios.end() &&
            single != summary.scenarios.end() &&
            near(success->full_participation_present_value_million,
              600.0 / 121.0) &&
            near(single->full_participation_present_value_million,
              300.0 / 121.0),
        "scenario attribution preserves payoff timing before robust projection");
}

void test_boundary_statuses_and_validation() {
    const cf::PortfolioConfig portfolio = witness_switch_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        witness_switch_ambiguity();

    cf::SuccessParticipationConfig terms = participation_terms();
    terms.target_worst_expected_npv_million = -11.0;
    const cf::SuccessParticipationSummary already =
        cf::solve_success_participation(portfolio, ambiguity, terms);
    check(already.status == cf::SuccessParticipationSolveStatus::
            AlreadyMeetsTargetAtZero &&
            already.exact_minimum_fraction == 0.0 &&
            already.reported_fraction == 0.0,
        "a target met without selected payoff has exact q=0 terms");

    terms = participation_terms();
    terms.target_worst_expected_npv_million = 1.0;
    const cf::SuccessParticipationSummary full =
        cf::solve_success_participation(portfolio, ambiguity, terms);
    check(full.status == cf::SuccessParticipationSolveStatus::
            FullParticipationRequired &&
            full.exact_minimum_fraction == 1.0 &&
            full.reported_fraction == 1.0 &&
            near(full.reported.expected_npv_million.minimum.value, 1.0),
        "a target reached only at the configured payoff ceiling requires q=1");

    cf::PortfolioConfig no_cash = witness_switch_portfolio();
    no_cash.joint_scenarios[0].cash_sources[0]
        .cash_available[0]
        .amount_million = 8.0;
    no_cash.joint_scenarios[0].project_paths[0]
        .investor_receipts[0]
        .amount_million = 8.0;
    no_cash.joint_scenarios[0].project_paths[0]
        .investor_receipts[0]
        .principal_component_million = 8.0;
    no_cash.joint_scenarios[1].cash_sources[0]
        .cash_available[0]
        .amount_million = 0.0;
    no_cash.joint_scenarios[1].project_paths[0]
        .investor_receipts[0]
        .amount_million = 0.0;
    no_cash.joint_scenarios[1].project_paths[0]
        .investor_receipts[0]
        .principal_component_million = 0.0;
    terms = participation_terms();
    const cf::SuccessParticipationSummary none =
        cf::solve_success_participation(no_cash, ambiguity, terms);
    check(none.status == cf::SuccessParticipationSolveStatus::
            NoSelectedParticipationCash &&
            none.reported_fraction == 0.0 &&
            near(none.full_participation_nominal_million.maximum.value, 0.0),
        "a selected taxonomy with no modeled excess cash has its own honest status");

    terms = participation_terms();
    terms.selected_nonprincipal_cash_is_contractually_scalable = false;
    expect_invalid_argument(
        [&portfolio, &ambiguity, &terms] {
            cf::validate_success_participation_config(
                portfolio, ambiguity, terms);
        },
        "contractual scalability must be asserted explicitly");

    terms = participation_terms();
    terms.scalable_source_kinds.push_back(
        cf::PortfolioCashSource::Commercial);
    expect_invalid_argument(
        [&portfolio, &ambiguity, &terms] {
            cf::validate_success_participation_config(
                portfolio, ambiguity, terms);
        },
        "scalable source kinds must be unique");

    terms = participation_terms();
    terms.scalable_source_kinds = {cf::PortfolioCashSource::ExplicitSupport};
    expect_invalid_argument(
        [&portfolio, &ambiguity, &terms] {
            cf::validate_success_participation_config(
                portfolio, ambiguity, terms);
        },
        "support cash cannot be selected as project success participation");

    terms = participation_terms();
    terms.target_worst_expected_npv_million =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&portfolio, &ambiguity, &terms] {
            cf::validate_success_participation_config(
                portfolio, ambiguity, terms);
        },
        "non-finite robust NPV targets are rejected");

    terms = participation_terms();
    expect_invalid_argument(
        [&portfolio, &terms] {
            (void)cf::apply_success_participation_fraction(
                portfolio, terms, -0.01);
        },
        "negative participation fractions are rejected");
    expect_invalid_argument(
        [&portfolio, &terms] {
            (void)cf::apply_success_participation_fraction(
                portfolio, terms, 1.01);
        },
        "participation above the configured payoff ceiling is rejected");
    expect_invalid_argument(
        [&portfolio, &terms] {
            (void)cf::apply_success_participation_fraction(portfolio, terms,
                std::numeric_limits<double>::infinity());
        },
        "non-finite participation fractions are rejected");

    cf::PortfolioConfig rounding = witness_switch_portfolio();
    cf::InvestorReceipt& selected_receipt = rounding.joint_scenarios[0]
        .project_paths[0]
        .investor_receipts[0];
    selected_receipt.principal_component_million =
        selected_receipt.amount_million + 1.0e-12;
    expect_invalid_argument(
        [&rounding, &ambiguity, &terms] {
            cf::validate_success_participation_config(
                rounding, ambiguity, terms);
        },
        "selected receipts require strict non-negative participation cash even inside core input tolerance");
}

} // namespace

int main() {
    test_current_four_state_terms_are_honestly_unattainable();
    test_adjustment_scales_only_selected_nonprincipal_cash();
    test_solver_reprojects_combined_npv_when_witness_switches();
    test_dated_payoff_present_value_is_not_nominal_cash();
    test_boundary_statuses_and_validation();

    if (failures != 0) {
        std::cerr << failures << " success participation test(s) failed\n";
        return 1;
    }
    std::cout << "all success participation tests passed\n";
    return 0;
}
