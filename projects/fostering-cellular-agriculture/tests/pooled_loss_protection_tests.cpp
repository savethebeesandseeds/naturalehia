// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/pooled_loss_protection.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(double first, double second,
    double tolerance = 1.0e-9) {
    return std::abs(first - second) <= tolerance;
}

[[nodiscard]] double upper_tail_average(const std::vector<double>& values,
    const std::vector<double>& weights, double tail_probability) {
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(), [&values](std::size_t first,
                                           std::size_t second) {
        if (values[first] != values[second]) {
            return values[first] > values[second];
        }
        return first < second;
    });
    long double weight_sum = 0.0L;
    for (const double weight : weights) {
        weight_sum += static_cast<long double>(weight);
    }
    const long double requested =
        static_cast<long double>(tail_probability) * weight_sum;
    long double remaining = requested;
    long double total = 0.0L;
    for (const std::size_t index : order) {
        if (remaining <= 0.0L) {
            break;
        }
        const long double included = std::min(remaining,
            static_cast<long double>(weights[index]));
        total += included * static_cast<long double>(values[index]);
        remaining -= included;
    }
    return static_cast<double>(total / requested);
}

void expect_invalid_argument(
    const std::function<void()>& action, const char* message) {
    try {
        action();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, std::size_t month, double amount) {
    cf::ScenarioCashSource result;
    result.id = std::move(id);
    result.kind = kind;
    result.cash_available.push_back(cf::MonthlyAmount{month, amount});
    return result;
}

[[nodiscard]] cf::ProjectJointPath make_path(std::string project_id,
    double draw, std::size_t receipt_month, std::string source_id,
    double receipt, double principal) {
    cf::ProjectJointPath result;
    result.project_id = std::move(project_id);
    result.resolution = cf::ProjectPathResolution::Resolved;
    result.capital_draws.push_back(cf::MonthlyAmount{0U, draw});
    result.investor_receipts.push_back(cf::InvestorReceipt{receipt_month,
        std::move(source_id), receipt, principal});
    return result;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_terms(
    double target = 0.0) {
    cf::SuccessParticipationConfig result;
    result.scenario_label = "synthetic success cash underlying protection";
    result.source_note = "unit-test assertion for selected commercial cash";
    result.selected_nonprincipal_cash_is_contractually_scalable = true;
    result.scalable_source_kinds = {cf::PortfolioCashSource::Commercial};
    result.target_worst_expected_npv_million = target;
    return result;
}

[[nodiscard]] cf::PooledLossProtectionConfig protection_terms(
    double cap = 20.0 / 6.0, double q = 1.0) {
    cf::PooledLossProtectionConfig result;
    result.scenario_label = "synthetic pooled terminal-loss protection";
    result.source_note = "unit-test external support assertion only";
    result.provider_id = "synthetic-provider";
    result.portfolio_principal_loss_is_contractual_reference_amount = true;
    result.gross_project_loss_remains_visible = true;
    result.support_is_assumed_fully_funded_and_performing_in_all_scenarios =
        true;
    result.premium_is_upfront_at_month_zero = true;
    result.underlying_success_participation_fraction = q;
    result.settlement_month = 24U;
    result.support_cap_million = cap;
    return result;
}

[[nodiscard]] cf::PortfolioConfig four_state_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "four-state pooled loss protection table";
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
        make_path("culture-platform", 10.0, 24U,
            "culture-commercial", 13.0, 10.0),
        make_path("bioprocess-scaleup", 10.0, 24U,
            "scaleup-commercial", 13.0, 10.0),
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
        make_path("culture-platform", 10.0, 12U,
            "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 10.0, 24U,
            "scaleup-commercial", 13.0, 10.0),
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
        make_path("culture-platform", 10.0, 24U,
            "culture-commercial", 13.0, 10.0),
        make_path("bioprocess-scaleup", 10.0, 12U,
            "scaleup-recovery", 2.0, 2.0),
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
        make_path("culture-platform", 10.0, 12U,
            "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 10.0, 12U,
            "scaleup-recovery", 2.0, 2.0),
    };
    portfolio.joint_scenarios =
        {success, culture_loss, scaleup_loss, loss};
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig four_state_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "four-state protection probability envelope";
    ambiguity.source_note = "invented probability bounds for unit tests";
    ambiguity.scenario_probabilities = {
        {"common-success", 0.50, 0.62, 0.70},
        {"culture-loss-scaleup-success", 0.10, 0.18, 0.25},
        {"culture-success-scaleup-loss", 0.10, 0.18, 0.25},
        {"common-loss", 0.01, 0.02, 0.10},
    };
    return ambiguity;
}

void test_explicit_contractual_reference_uses_principal_limit() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label =
        "above-par explicit contractual loss-protection reference";
    portfolio.source_note =
        "synthetic unit test separating purchase cash from legal principal";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at purchase";
    portfolio.horizon_months = 24U;

    cf::PortfolioProject project;
    project.id = "above-par-claim";
    project.stage = cf::ProjectStage::FirstIndustrial;
    project.commitment_million = 12.0;
    project.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = 10.0;
    portfolio.projects = {project};

    const auto make_explicit_path = [](std::string source_id,
                                        double principal_cash,
                                        double writeoff) {
        cf::ProjectJointPath path;
        path.project_id = "above-par-claim";
        path.resolution = cf::ProjectPathResolution::Resolved;
        path.investor_outlays = {{0U,
            cf::InvestorOutlayPurpose::ClaimPurchasePrice, 12.0}};
        path.investor_receipts = {{24U, std::move(source_id),
            principal_cash, principal_cash}};
        path.principal_movements = {{0U,
            cf::PrincipalMovementKind::FundedPrincipalAddition, 10.0}};
        if (writeoff > 0.0) {
            path.principal_movements.push_back({24U,
                cf::PrincipalMovementKind::Writeoff, writeoff});
        }
        return path;
    };

    cf::JointScenario performing;
    performing.id = "above-par-performing";
    performing.weight = 0.5;
    performing.cash_sources = {make_source("performing-principal",
        cf::PortfolioCashSource::Commercial, 24U, 10.0)};
    performing.project_paths = {
        make_explicit_path("performing-principal", 10.0, 0.0)};

    cf::JointScenario written_off;
    written_off.id = "contractual-writeoff";
    written_off.weight = 0.5;
    written_off.cash_sources = {make_source("writeoff-recovery",
        cf::PortfolioCashSource::Recovery, 24U, 2.0)};
    written_off.project_paths = {
        make_explicit_path("writeoff-recovery", 2.0, 8.0)};
    portfolio.joint_scenarios = {performing, written_off};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label =
        "point probabilities for explicit contractual protection";
    ambiguity.source_note = "synthetic exact probability unit test";
    ambiguity.scenario_probabilities = {
        {"above-par-performing", 0.5, 0.5, 0.5},
        {"contractual-writeoff", 0.5, 0.5, 0.5},
    };

    const cf::PortfolioSummary underlying = cf::evaluate_portfolio(portfolio);
    const auto performing_underlying = std::find_if(
        underlying.scenarios.begin(), underlying.scenarios.end(),
        [](const auto& scenario) {
            return scenario.scenario_id == "above-par-performing";
        });
    check(performing_underlying != underlying.scenarios.end() &&
            near(performing_underlying->total_investor_outlays_million,
                12.0) &&
            near(performing_underlying->principal_added_million, 10.0) &&
            near(performing_underlying->principal_returned_million, 10.0) &&
            near(performing_underlying->principal_loss_million, 0.0),
        "above-par purchase cash remains separate from a fully repaid contractual principal ledger");

    const cf::PooledLossProtectionConfig terms = protection_terms(5.0);
    const cf::PooledLossProtectionRobustPoint half =
        cf::evaluate_pooled_loss_protection_coverage(portfolio, ambiguity,
            participation_terms(-5.0), terms, 0.5);
    check(near(cf::portfolio_aggregate_reference_principal(portfolio), 10.0) &&
            near(half.coverage_fraction, 0.5) &&
            near(half.provider_risk.contractual_maximum_exposure_million,
                5.0) &&
            near(half.provider_risk.modeled_maximum_claim_million, 4.0) &&
            near(half.provider_risk.expected_claim_nominal_million.central,
                2.0),
        "explicit-ledger protection uses the ten-unit principal limit, not the twelve-unit investor cash commitment, for cap coverage and claims");

    const cf::PooledLossProtectionSummary solved =
        cf::solve_pooled_loss_protection(portfolio, ambiguity,
            participation_terms(-5.0), terms);
    const auto protected_performing = std::find_if(
        solved.scenarios.begin(), solved.scenarios.end(),
        [](const auto& scenario) {
            return scenario.scenario_id == "above-par-performing";
        });
    const auto protected_writeoff = std::find_if(
        solved.scenarios.begin(), solved.scenarios.end(),
        [](const auto& scenario) {
            return scenario.scenario_id == "contractual-writeoff";
        });
    check(near(solved.aggregate_reference_principal_million, 10.0) &&
            near(solved.aggregate_covered_commitment_million, 10.0) &&
            near(solved.maximum_supported_coverage_fraction, 0.5) &&
            protected_performing != solved.scenarios.end() &&
            near(protected_performing->gross_principal_loss_million, 0.0) &&
            near(protected_performing->protection_claim_million, 0.0) &&
            protected_writeoff != solved.scenarios.end() &&
            near(protected_writeoff->gross_principal_loss_million, 8.0) &&
            near(protected_writeoff->protection_claim_million,
                8.0 * solved.reported_coverage_fraction),
        "above-par economics create no protection claim without contractual writeoff while an actual writeoff remains the reference loss");
}

[[nodiscard]] cf::PortfolioConfig witness_switch_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "two-state loss-protection witness switch";
    portfolio.source_note = "synthetic threshold unit-test paths only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 24U;
    portfolio.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 20.0}};

    cf::JointScenario deep;
    deep.id = "deep-loss";
    deep.weight = 0.5;
    deep.cash_sources = {make_source("commercial",
        cf::PortfolioCashSource::Commercial, 24U, 10.0)};
    deep.project_paths = {make_path(
        "project", 20.0, 24U, "commercial", 10.0, 0.0)};

    cf::JointScenario moderate;
    moderate.id = "moderate-loss";
    moderate.weight = 0.5;
    moderate.cash_sources = {make_source("commercial",
        cf::PortfolioCashSource::Commercial, 24U, 1.0)};
    moderate.project_paths = {make_path(
        "project", 3.0, 24U, "commercial", 1.0, 0.0)};
    portfolio.joint_scenarios = {moderate, deep};
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig witness_switch_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "two-state full-simplex ambiguity";
    ambiguity.source_note = "synthetic witness switch bounds";
    ambiguity.scenario_probabilities = {
        {"moderate-loss", 0.0, 0.5, 1.0},
        {"deep-loss", 0.0, 0.5, 1.0},
    };
    return ambiguity;
}

void test_four_state_cap_boundary_and_premium_gap() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::PooledLossProtectionSummary summary =
        cf::solve_pooled_loss_protection(portfolio,
            four_state_ambiguity(), participation_terms(),
            protection_terms());

    check(summary.status == cf::PooledLossProtectionSolveStatus::
                CertifiedInteriorBracket,
        "the four-state asset reaches its investor target at one-sixth protection");
    check(near(summary.maximum_supported_coverage_fraction, 1.0 / 6.0) &&
            near(summary.reported_coverage_fraction, 1.0 / 6.0) &&
            summary.failing_coverage_fraction_lower_bound.has_value() &&
            summary.investor_target_passing_coverage_fraction_upper_bound
                .has_value(),
        "non-boundary coverage is a certified bracket at the legal cap");
    check(near(summary.zero.investor_expected_npv_before_premium_million
                   .minimum.value,
              -0.8) &&
            near(summary.reported.investor_expected_npv_before_premium_million
                   .minimum.value,
              0.0),
        "robust investor NPV follows -0.8+4.8g at q=1");
    check(near(summary.reported.provider_minimum_robust_break_even_premium_million,
              0.8) &&
            near(summary.reported.investor_signed_premium_headroom_million,
              0.0) &&
            near(summary.reported.premium_feasibility_gap_million, 0.8) &&
            !summary.reported.robust_nonnegative_premium_interval_exists,
        "the external transfer closes pre-premium loss but cannot erase the pricing gap");
    check(near(summary.aggregate_covered_commitment_million, 20.0) &&
            near(summary.reported.provider_risk
                    .contractual_maximum_exposure_million,
              20.0 / 6.0) &&
            near(summary.reported.provider_risk.modeled_maximum_claim_million,
              16.0 / 6.0) &&
            summary.reported.provider_risk.maximum_cap_utilization.has_value() &&
            near(*summary.reported.provider_risk.maximum_cap_utilization,
              1.0),
        "legal exposure uses aggregate commitment while modeled claim remains separate");
    check(near(summary.reported.provider_risk.expected_claim_nominal_million
                   .minimum.value,
              2.48 / 6.0) &&
            near(summary.reported.provider_risk.expected_claim_nominal_million
                    .central,
              3.2 / 6.0) &&
            near(summary.reported.provider_risk.expected_claim_nominal_million
                   .maximum.value,
              4.8 / 6.0),
        "provider expected claims preserve exact ambiguity witnesses");
    check(near(summary.reported.provider_risk
                    .claim_expected_shortfall_95_nominal_million.minimum.value,
              9.6 / 6.0) &&
            near(summary.reported.provider_risk
                    .claim_expected_shortfall_95_nominal_million.central,
              11.2 / 6.0) &&
            near(summary.reported.provider_risk
                    .claim_expected_shortfall_95_nominal_million.maximum.value,
              16.0 / 6.0) &&
            near(summary.reported.provider_risk
                    .claim_expected_shortfall_99_nominal_million.central,
              16.0 / 6.0),
        "provider ES95 and ES99 are projected as nonlinear tail metrics");
    std::vector<double> provider_present_values;
    provider_present_values.reserve(summary.scenarios.size());
    bool canonical_scenario_order =
        summary.scenarios.size() == summary.scenario_probability_bounds.size();
    for (std::size_t index = 0U; index < summary.scenarios.size(); ++index) {
        provider_present_values.push_back(
            summary.scenarios[index]
                .claim_present_value_to_provider_million);
        canonical_scenario_order = canonical_scenario_order &&
            summary.scenarios[index].scenario_id ==
                summary.scenario_probability_bounds[index].scenario_id;
    }
    const auto& provider_es95 = summary.reported.provider_risk
                                    .claim_expected_shortfall_95_present_value_million;
    const auto& provider_es99 = summary.reported.provider_risk
                                    .claim_expected_shortfall_99_present_value_million;
    check(canonical_scenario_order &&
            near(upper_tail_average(provider_present_values,
                     provider_es95.minimum.scenario_weights, 0.05),
                provider_es95.minimum.value) &&
            near(upper_tail_average(provider_present_values,
                     provider_es95.maximum.scenario_weights, 0.05),
                provider_es95.maximum.value) &&
            near(upper_tail_average(provider_present_values,
                     provider_es99.minimum.scenario_weights, 0.01),
                provider_es99.minimum.value) &&
            near(upper_tail_average(provider_present_values,
                     provider_es99.maximum.scenario_weights, 0.01),
                provider_es99.maximum.value) &&
            provider_es95.minimum.scenario_weights ==
                summary.reported.provider_risk
                    .claim_expected_shortfall_95_nominal_million.minimum
                    .scenario_weights &&
            provider_es95.maximum.scenario_weights ==
                summary.reported.provider_risk
                    .claim_expected_shortfall_95_nominal_million.maximum
                    .scenario_weights,
        "provider-PV ES endpoints reconstruct from scenario PV cash and retain their ambiguity witnesses");
    check(summary.maximum_underlying_loss_change_million <= 1.0e-12 &&
            summary.maximum_project_claim_reconciliation_error_million <=
                1.0e-12 &&
            summary.maximum_two_party_settlement_cash_reconciliation_error_million <=
                1.0e-12 &&
            summary.maximum_support_cap_violation_million <= 1.0e-12 &&
            summary.maximum_combined_npv_reconstruction_error_million <=
                1.0e-12 &&
            summary.maximum_witness_reconciliation_error_million <= 1.0e-9 &&
            summary.maximum_endpoint_probability_error <= 1.0e-12,
        "gross loss, external claims, cap, NPV, tail, and probability controls reconcile");
    bool scenario_cash_and_capacity_reconcile = true;
    for (const auto& scenario : summary.scenarios) {
        scenario_cash_and_capacity_reconcile =
            scenario_cash_and_capacity_reconcile &&
            near(scenario.investor_external_support_cash_million +
                    scenario.provider_external_support_cash_million,
                0.0) &&
            near(scenario.remaining_contractual_claim_headroom_million,
                summary.reported.provider_risk
                        .contractual_maximum_exposure_million -
                    scenario.protection_claim_million) &&
            near(scenario.uncommitted_legal_cap_capacity_million,
                summary.legal_support_cap_million -
                    summary.reported.provider_risk
                        .contractual_maximum_exposure_million);
        for (const auto& project : scenario.projects) {
            scenario_cash_and_capacity_reconcile =
                scenario_cash_and_capacity_reconcile &&
                near(project.investor_external_support_cash_million +
                        project.provider_external_support_cash_million,
                    0.0) &&
                near(project.protection_claim_million +
                        project.residual_unprotected_loss_million,
                    project.gross_principal_loss_million);
        }
    }
    check(scenario_cash_and_capacity_reconcile,
        "scenario output separates contractual headroom, uncommitted cap, and equal-opposite settlement cash");
    check(!summary.provider_default_risk_is_modeled &&
            !summary.provider_funding_and_collateral_costs_are_modeled &&
            !summary.legal_enforceability_is_validated &&
            summary.provider_model_limitation.find("not modeled") !=
                std::string::npos,
        "provider performance assumptions remain prominent report facts");
}

void test_solver_reprojects_complete_npv_when_witness_switches() {
    const cf::PooledLossProtectionSummary summary =
        cf::solve_pooled_loss_protection(witness_switch_portfolio(),
            witness_switch_ambiguity(), participation_terms(),
            protection_terms(20.0, 1.0));
    const double expected = 2.0 / 3.0;
    check(summary.status == cf::PooledLossProtectionSolveStatus::
                CertifiedInteriorBracket &&
            summary.failing_coverage_fraction_lower_bound.has_value() &&
            summary.investor_target_passing_coverage_fraction_upper_bound
                .has_value() &&
            *summary.failing_coverage_fraction_lower_bound <= expected &&
            *summary
                    .investor_target_passing_coverage_fraction_upper_bound >=
                expected &&
            *summary
                    .investor_target_passing_coverage_fraction_upper_bound -
                    *summary.failing_coverage_fraction_lower_bound <
                1.0e-12,
        "the robust solver certifies the true two-thirds threshold");
    check(near(summary.zero.investor_expected_npv_before_premium_million
                   .minimum.value,
              -10.0) &&
            near(summary.reported.investor_expected_npv_before_premium_million
                   .minimum.value,
              0.0) &&
            summary.zero.investor_expected_npv_before_premium_million.minimum
                    .scenario_weights !=
                summary.reported.investor_expected_npv_before_premium_million
                    .minimum.scenario_weights,
        "the adverse probability witness switches instead of being frozen at g=0");
    check(near(summary.reported.provider_minimum_robust_break_even_premium_million,
              40.0 / 3.0),
        "provider robust floor uses its own maximum-loss witness");
}

void test_cap_and_boundary_statuses() {
    const cf::PortfolioConfig witness = witness_switch_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        witness_switch_ambiguity();

    const cf::PooledLossProtectionSummary capped =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(), protection_terms(10.0, 1.0));
    check(capped.status == cf::PooledLossProtectionSolveStatus::
                UnattainableAtMaximumSupportedCoverage &&
            near(capped.maximum_supported_coverage_fraction, 0.5) &&
            near(capped.investor_target_gap_at_maximum_supported_coverage_million,
              0.5) &&
            capped.maximum_support_cap_violation_million <= 1.0e-12,
        "legal cap blocks coverage beyond one half even though only modeled paths are shown");

    const cf::PooledLossProtectionSummary cap_boundary =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(-0.5), protection_terms(10.0, 1.0));
    check(cap_boundary.status == cf::PooledLossProtectionSolveStatus::
                CertifiedSupportCapBoundaryBracket &&
            near(cap_boundary.reported_coverage_fraction, 0.5),
        "a non-unit legal cap boundary remains a certified bracket rather than an exact root");

    const cf::PooledLossProtectionSummary already =
        cf::solve_pooled_loss_protection(four_state_portfolio(),
            four_state_ambiguity(), participation_terms(-1.0),
            protection_terms());
    check(already.status == cf::PooledLossProtectionSolveStatus::
                AlreadyMeetsInvestorTargetAtZero &&
            already.exact_minimum_coverage_fraction == 0.0 &&
            already.reported_coverage_fraction == 0.0,
        "a target met by the underlying has exact zero protection");

    cf::PooledLossProtectionConfig no_capacity = protection_terms(0.0, 1.0);
    const cf::PooledLossProtectionSummary none =
        cf::solve_pooled_loss_protection(four_state_portfolio(),
            four_state_ambiguity(), participation_terms(), no_capacity);
    check(none.status ==
            cf::PooledLossProtectionSolveStatus::NoSupportCapacity &&
            none.maximum_supported_coverage_fraction == 0.0,
        "a lossy pool with zero legal support has its own status");

    const cf::PooledLossProtectionSummary full =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(1.0), protection_terms(20.0, 1.0));
    check(full.status ==
            cf::PooledLossProtectionSolveStatus::FullCoverageRequired &&
            full.exact_minimum_coverage_fraction == 1.0,
        "a target reached only at literal full coverage has an exact boundary term");

    const cf::PooledLossProtectionSummary above_full =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(
                std::nextafter(1.0, std::numeric_limits<double>::infinity())),
            protection_terms(20.0, 1.0));
    const cf::PooledLossProtectionSummary below_full =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(std::nextafter(1.0, 0.0)),
            protection_terms(20.0, 1.0));
    check(above_full.status == cf::PooledLossProtectionSolveStatus::
                UnattainableAtMaximumSupportedCoverage &&
            below_full.status == cf::PooledLossProtectionSolveStatus::
                CertifiedInteriorBracket,
        "one-ulp targets on either side of full coverage preserve exact status boundaries");

    const cf::PooledLossProtectionSummary above_cap =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(std::nextafter(
                -0.5, std::numeric_limits<double>::infinity())),
            protection_terms(10.0, 1.0));
    const cf::PooledLossProtectionSummary below_cap =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(std::nextafter(
                -0.5, -std::numeric_limits<double>::infinity())),
            protection_terms(10.0, 1.0));
    check(above_cap.status == cf::PooledLossProtectionSolveStatus::
                UnattainableAtMaximumSupportedCoverage &&
            below_cap.status == cf::PooledLossProtectionSolveStatus::
                CertifiedSupportCapBoundaryBracket,
        "one-ulp targets distinguish an unattainable cap from the nearest representable passing cap term");

    const cf::PooledLossProtectionSummary interior_cap =
        cf::solve_pooled_loss_protection(witness, ambiguity,
            participation_terms(-0.500001),
            protection_terms(10.0, 1.0));
    check(interior_cap.status == cf::PooledLossProtectionSolveStatus::
                CertifiedInteriorBracket &&
            interior_cap.reported_coverage_fraction <
                interior_cap.maximum_supported_coverage_fraction,
        "a target materially below the cap-boundary value produces a strict interior bracket");
}

void test_lossless_paths_and_separate_hurdles() {
    cf::PortfolioConfig lossless = four_state_portfolio();
    for (cf::JointScenario& scenario : lossless.joint_scenarios) {
        for (cf::ScenarioCashSource& source : scenario.cash_sources) {
            if (source.kind == cf::PortfolioCashSource::Recovery) {
                source.cash_available[0].amount_million = 10.0;
            }
        }
        for (cf::ProjectJointPath& path : scenario.project_paths) {
            for (cf::InvestorReceipt& receipt : path.investor_receipts) {
                if (receipt.cash_source_id.find("recovery") !=
                    std::string::npos) {
                    receipt.amount_million = 10.0;
                    receipt.principal_component_million = 10.0;
                }
            }
        }
    }
    const cf::PooledLossProtectionSummary no_loss =
        cf::solve_pooled_loss_protection(lossless,
            four_state_ambiguity(), participation_terms(100.0),
            protection_terms());
    check(no_loss.status ==
            cf::PooledLossProtectionSolveStatus::NoGrossReferenceLoss &&
            no_loss.modeled_full_coverage_maximum_claim_million == 0.0 &&
            no_loss.reported.provider_risk.modeled_maximum_claim_million ==
                0.0,
        "a target deficit without realized principal loss cannot be cured by this overlay");

    cf::PortfolioConfig dated = four_state_portfolio();
    dated.annual_physical_hurdle_rate = 0.21;
    cf::PooledLossProtectionConfig terms = protection_terms(20.0, 1.0);
    terms.provider_annual_physical_hurdle_rate = 0.10;
    const cf::PooledLossProtectionSummary discounted =
        cf::solve_pooled_loss_protection(dated,
            four_state_ambiguity(), participation_terms(100.0), terms);
    const auto common_loss = std::find_if(discounted.scenarios.begin(),
        discounted.scenarios.end(), [](const auto& scenario) {
            return scenario.scenario_id == "common-loss";
        });
    check(common_loss != discounted.scenarios.end() &&
            near(common_loss->protection_claim_million, 16.0) &&
            near(common_loss->claim_present_value_to_investor_million,
              16.0 / (1.21 * 1.21)) &&
            near(common_loss->claim_present_value_to_provider_million,
              16.0 / (1.10 * 1.10)),
        "investor and provider discount the same nominal claim at separate declared hurdles");
}

void test_continuing_exposure_is_not_terminal_realized_loss() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "continuing principal exposure boundary";
    portfolio.source_note = "synthetic continuing-path unit test";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 24U;
    portfolio.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 10.0}};
    cf::JointScenario scenario;
    scenario.id = "continuing";
    scenario.weight = 1.0;
    scenario.cash_sources = {make_source("commercial",
        cf::PortfolioCashSource::Commercial, 24U, 3.0)};
    cf::ProjectJointPath path = make_path(
        "project", 10.0, 24U, "commercial", 3.0, 3.0);
    path.resolution = cf::ProjectPathResolution::Continuing;
    scenario.project_paths = {path};
    portfolio.joint_scenarios = {scenario};

    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "certain continuing path";
    ambiguity.source_note = "synthetic point probability";
    ambiguity.scenario_probabilities = {
        {"continuing", 1.0, 1.0, 1.0}};
    const cf::PortfolioSummary underlying = cf::evaluate_portfolio(portfolio);
    const cf::PooledLossProtectionSummary protected_summary =
        cf::solve_pooled_loss_protection(portfolio, ambiguity,
            participation_terms(100.0), protection_terms(10.0, 1.0));
    check(near(underlying.scenarios[0].outstanding_principal_million, 7.0) &&
            underlying.scenarios[0].principal_loss_million == 0.0 &&
            protected_summary.status ==
                cf::PooledLossProtectionSolveStatus::NoGrossReferenceLoss &&
            protected_summary.scenarios[0].gross_principal_loss_million ==
                0.0 &&
            protected_summary.scenarios[0].protection_claim_million == 0.0,
        "continuing unreturned principal stays outstanding and cannot trigger final-loss protection");
}

void test_exact_caller_selected_coverage_projection() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity = four_state_ambiguity();
    const cf::SuccessParticipationConfig participation =
        participation_terms();
    const cf::PooledLossProtectionConfig full_cap =
        protection_terms(20.0, 1.0);
    const cf::PooledLossProtectionRobustPoint half =
        cf::evaluate_pooled_loss_protection_coverage(portfolio, ambiguity,
            participation, full_cap, 0.5);
    check(half.coverage_fraction == 0.5 &&
            near(half.investor_expected_npv_before_premium_million.minimum
                    .value,
              1.6) &&
            near(half.provider_minimum_robust_break_even_premium_million,
              2.4) &&
            near(half.provider_risk.contractual_maximum_exposure_million,
              10.0) &&
            near(half.provider_risk.modeled_maximum_claim_million, 8.0),
        "an arbitrary supported coverage fraction is freshly projected with complete investor and provider economics");

    const cf::PooledLossProtectionConfig capped = protection_terms();
    expect_invalid_argument(
        [&portfolio, &ambiguity, &participation, &capped] {
            (void)cf::evaluate_pooled_loss_protection_coverage(portfolio,
                ambiguity, participation, capped, 0.5);
        },
        "an arbitrary coverage fraction above the reference-principal cap is rejected");
    expect_invalid_argument(
        [&portfolio, &ambiguity, &participation, &full_cap] {
            (void)cf::evaluate_pooled_loss_protection_coverage(portfolio,
                ambiguity, participation, full_cap,
                std::numeric_limits<double>::quiet_NaN());
        },
        "a non-finite arbitrary coverage fraction is rejected");
}

void test_upfront_premium_and_validation() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity = four_state_ambiguity();
    const cf::SuccessParticipationConfig participation =
        participation_terms();
    const cf::PooledLossProtectionSummary summary =
        cf::solve_pooled_loss_protection(portfolio, ambiguity,
            participation, protection_terms());

    const cf::PooledLossProtectionPremiumEvaluation free =
        cf::evaluate_pooled_loss_protection_upfront_premium(
            summary.reported, 0.0);
    check(free.investor_target_is_met &&
            !free.provider_break_even_is_met &&
            near(free.provider_break_even_gap_after_premium_million, 0.8),
        "a free guarantee meets the investor target but leaves provider claim cost explicit");
    const cf::PooledLossProtectionPremiumEvaluation provider_floor =
        cf::evaluate_pooled_loss_protection_upfront_premium(
            summary.reported,
            summary.reported
                .provider_minimum_robust_break_even_premium_million);
    check(provider_floor.provider_break_even_is_met &&
            !provider_floor.investor_target_is_met &&
            near(provider_floor.investor_target_gap_after_premium_million,
              0.8) &&
            near(provider_floor.investor_month_zero_premium_cash_million +
                    provider_floor.provider_month_zero_premium_cash_million,
                0.0) &&
            provider_floor.month_zero_premium_cash_reconciliation_error_million ==
                0.0,
        "month-zero premium exposes the same robust gap at the provider floor");
    const cf::AmbiguityMetricRange& provider_claim_pv =
        summary.reported.provider_risk.expected_claim_present_value_million;
    check(provider_floor.provider_expected_npv_after_premium_million.minimum
                .scenario_weights ==
            provider_claim_pv.maximum.scenario_weights &&
            provider_floor.provider_expected_npv_after_premium_million.maximum
                    .scenario_weights ==
                provider_claim_pv.minimum.scenario_weights &&
            near(provider_floor.provider_expected_npv_after_premium_million
                     .minimum.value,
                provider_floor.upfront_premium_million -
                    provider_claim_pv.maximum.value) &&
            near(provider_floor.provider_expected_npv_after_premium_million
                    .central,
                provider_floor.upfront_premium_million -
                    provider_claim_pv.central) &&
            near(provider_floor.provider_expected_npv_after_premium_million
                     .maximum.value,
                provider_floor.upfront_premium_million -
                    provider_claim_pv.minimum.value),
        "provider NPV reverses claim-PV endpoints and preserves the binding witnesses");

    expect_invalid_argument(
        [&summary] {
            (void)cf::evaluate_pooled_loss_protection_upfront_premium(
                summary.reported, -0.01);
        },
        "negative premiums are rejected");
    expect_invalid_argument(
        [&summary] {
            (void)cf::evaluate_pooled_loss_protection_upfront_premium(
                summary.reported,
                std::numeric_limits<double>::infinity());
        },
        "non-finite premiums are rejected");

    cf::PortfolioConfig large;
    large.scenario_label = "large NPV premium cancellation regression";
    large.source_note = "synthetic precision test only";
    large.currency_label = "TEST";
    large.monetary_basis = "constant test units at close";
    large.horizon_months = 24U;
    large.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 1.0}};
    cf::JointScenario large_success;
    large_success.id = "success";
    large_success.weight = 1.0;
    large_success.cash_sources = {make_source("commercial",
        cf::PortfolioCashSource::Commercial, 24U, 1.0e6)};
    large_success.project_paths = {make_path(
        "project", 1.0, 24U, "commercial", 1.0e6, 1.0)};
    large.joint_scenarios = {large_success};
    cf::PortfolioAmbiguityConfig certain;
    certain.scenario_label = "certain precision scenario";
    certain.source_note = "synthetic point probability";
    certain.scenario_probabilities = {
        {"success", 1.0, 1.0, 1.0}};
    const cf::PooledLossProtectionSummary precision =
        cf::solve_pooled_loss_protection(large, certain,
            participation_terms(1.0e-12), protection_terms(0.0, 1.0));
    check(precision.zero.investor_maximum_nonnegative_premium_million
                .has_value() &&
            *precision.zero.investor_maximum_nonnegative_premium_million <=
                precision.zero.investor_signed_premium_headroom_million,
        "published investor premium ceiling is conservatively representable");
    if (precision.zero.investor_maximum_nonnegative_premium_million
            .has_value()) {
        const cf::PooledLossProtectionPremiumEvaluation at_ceiling =
            cf::evaluate_pooled_loss_protection_upfront_premium(
                precision.zero,
                *precision.zero
                     .investor_maximum_nonnegative_premium_million);
        check(at_ceiling.investor_target_is_met,
            "evaluating the published premium ceiling meets its own small positive target without cancellation error");
    }

    cf::PooledLossProtectionConfig invalid = protection_terms();
    invalid.support_is_assumed_fully_funded_and_performing_in_all_scenarios =
        false;
    expect_invalid_argument(
        [&portfolio, &ambiguity, &participation, &invalid] {
            cf::validate_pooled_loss_protection_config(
                portfolio, ambiguity, participation, invalid);
        },
        "fully funded performance must be an explicit assumption");
    invalid = protection_terms();
    invalid.settlement_month = 12U;
    expect_invalid_argument(
        [&portfolio, &ambiguity, &participation, &invalid] {
            cf::validate_pooled_loss_protection_config(
                portfolio, ambiguity, participation, invalid);
        },
        "settlement before final loss determination is rejected");
    invalid = protection_terms(21.0, 1.0);
    expect_invalid_argument(
        [&portfolio, &ambiguity, &participation, &invalid] {
            cf::validate_pooled_loss_protection_config(
                portfolio, ambiguity, participation, invalid);
        },
        "support cap above aggregate reference principal is rejected");
    invalid = protection_terms();
    invalid.underlying_success_participation_fraction = 1.01;
    expect_invalid_argument(
        [&portfolio, &ambiguity, &participation, &invalid] {
            cf::validate_pooled_loss_protection_config(
                portfolio, ambiguity, participation, invalid);
        },
        "underlying success participation outside [0,1] is rejected");

    cf::PortfolioConfig tranched = portfolio;
    tranched.loss_layers = {cf::LossLayer{"whole", 0.0, 20.0}};
    invalid = protection_terms();
    expect_invalid_argument(
        [&tranched, &ambiguity, &participation, &invalid] {
            cf::validate_pooled_loss_protection_config(
                tranched, ambiguity, participation, invalid);
        },
        "v0.1 rejects a protection overlay on an already tranched loss table");
}

} // namespace

int main() {
    test_explicit_contractual_reference_uses_principal_limit();
    test_four_state_cap_boundary_and_premium_gap();
    test_solver_reprojects_complete_npv_when_witness_switches();
    test_cap_and_boundary_statuses();
    test_lossless_paths_and_separate_hurdles();
    test_continuing_exposure_is_not_terminal_realized_loss();
    test_exact_caller_selected_coverage_projection();
    test_upfront_premium_and_validation();

    if (failures != 0) {
        std::cerr << failures << " pooled loss protection test(s) failed\n";
        return 1;
    }
    std::cout << "all pooled loss protection tests passed\n";
    return 0;
}
