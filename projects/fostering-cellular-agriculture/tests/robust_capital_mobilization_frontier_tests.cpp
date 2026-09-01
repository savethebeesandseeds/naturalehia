// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <stdexcept>
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
    double first, double second, double tolerance = 1.0e-8) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

template <typename Callable>
void check_invalid(Callable&& callable, std::string_view message) {
    try {
        callable();
        check(false, message);
    } catch (const std::invalid_argument&) {
        check(true, message);
    } catch (...) {
        check(false, message);
    }
}

template <typename Callable>
void check_invalid_equals(Callable&& callable,
    std::string_view expected_text, std::string_view message) {
    try {
        callable();
        check(false, message);
    } catch (const std::invalid_argument& error) {
        check(std::string_view(error.what()) == expected_text,
            message);
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

// In a successful project the selected commercial cash is 10 principal plus
// 4 non-principal at q=1. A failed project returns 2 principal. With a fixed
// market priority cap of 1, q >= 0.25 saturates that cap whenever at least one
// project succeeds. This makes the small frontier independently calculable.
[[nodiscard]] cf::PortfolioConfig four_state_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "four-state capital-mobilization hand table";
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
            24U, 14.0),
        make_source("scaleup-commercial", cf::PortfolioCashSource::Commercial,
            24U, 14.0),
    };
    success.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 14.0, 10.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 14.0,
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
            24U, 14.0),
    };
    culture_loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 14.0,
            10.0),
    };

    cf::JointScenario scaleup_loss;
    scaleup_loss.id = "culture-success-scaleup-loss";
    scaleup_loss.weight = 0.18;
    scaleup_loss.pool_costs = {{0U, 0.2}};
    scaleup_loss.cash_sources = {
        make_source("culture-commercial", cf::PortfolioCashSource::Commercial,
            24U, 14.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    scaleup_loss.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 14.0, 10.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0, 2.0),
    };

    cf::JointScenario common_loss;
    common_loss.id = "common-loss";
    common_loss.weight = 0.02;
    common_loss.pool_costs = {{0U, 0.2}};
    common_loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    common_loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0, 2.0),
    };
    portfolio.joint_scenarios = {
        scaleup_loss, common_loss, success, culture_loss};
    return portfolio;
}

[[nodiscard]] cf::ProbabilityPolytopeConfig event_polytope() {
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "overlapping event constraint hand table";
    polytope.source_note = "invented probability events for unit tests";
    polytope.scenario_probabilities = {
        {"common-success", 0.0, 0.62, 1.0},
        {"culture-loss-scaleup-success", 0.0, 0.18, 1.0},
        {"culture-success-scaleup-loss", 0.0, 0.18, 1.0},
        {"common-loss", 0.0, 0.02, 1.0},
    };
    polytope.events = {
        {"culture-platform-impairment", "Culture platform is impaired",
            0.12, 0.30,
            {"culture-loss-scaleup-success", "common-loss"}},
        {"scaleup-impairment", "Scaleup project is impaired", 0.12, 0.30,
            {"culture-success-scaleup-loss", "common-loss"}},
        {"common-process-shock", "Both projects are impaired", 0.01, 0.10,
            {"common-loss"}},
        {"any-project-impairment", "At least one project is impaired", 0.30,
            0.50,
            {"culture-loss-scaleup-success",
                "culture-success-scaleup-loss", "common-loss"}},
    };
    return polytope;
}

struct RepeatedStateFixture {
    cf::PortfolioConfig portfolio{};
    cf::ProbabilityPolytopeConfig polytope{};
};

[[nodiscard]] RepeatedStateFixture repeated_success_states(
    std::size_t count) {
    RepeatedStateFixture fixture;
    fixture.portfolio = four_state_portfolio();
    const auto success = std::find_if(
        fixture.portfolio.joint_scenarios.begin(),
        fixture.portfolio.joint_scenarios.end(),
        [](const cf::JointScenario& scenario) {
            return scenario.id == "common-success";
        });
    if (success == fixture.portfolio.joint_scenarios.end() || count == 0U) {
        throw std::logic_error("test repeated-state source is unavailable");
    }
    const cf::JointScenario success_template = *success;
    fixture.portfolio.scenario_label =
        "repeated-state frontier resource-guard fixture";
    fixture.portfolio.joint_scenarios.clear();
    fixture.polytope.scenario_label =
        "repeated-state frontier resource-guard polytope";
    fixture.polytope.source_note =
        "synthetic repeated states for a validation-only resource test";
    const double weight = 1.0 / static_cast<double>(count);
    for (std::size_t index = 0U; index < count; ++index) {
        cf::JointScenario scenario = success_template;
        scenario.id = "repeated-success-" + std::to_string(index);
        scenario.weight = weight;
        fixture.portfolio.joint_scenarios.push_back(scenario);
        fixture.polytope.scenario_probabilities.push_back(
            cf::ProbabilityPolytopeScenario{
                scenario.id, 0.0, weight, 1.0});
    }
    return fixture;
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

[[nodiscard]] cf::PortfolioConfig explicit_claim_portfolio(
    std::string scenario_id, double purchase_price, double principal_limit,
    double principal_receipt, double writeoff) {
    cf::PortfolioConfig portfolio;
    portfolio.model_version = std::string(cf::kPortfolioModelVersion);
    portfolio.scenario_label = "explicit frontier L O Q hand fixture";
    portfolio.source_note =
        "synthetic non-par asset and issued-liability ledger for unit tests";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at acquisition";
    portfolio.horizon_months = 1U;

    cf::PortfolioProject project;
    project.id = "cellular-agriculture-claim";
    project.stage = cf::ProjectStage::FirstIndustrial;
    project.commitment_million = purchase_price;
    project.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = principal_limit;
    portfolio.projects = {project};

    cf::ProjectJointPath path;
    path.project_id = project.id;
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.investor_outlays = {{0U,
        cf::InvestorOutlayPurpose::ClaimPurchasePrice, purchase_price}};
    path.principal_movements = {{0U,
        cf::PrincipalMovementKind::FundedPrincipalAddition,
        principal_limit}};
    if (principal_receipt > 0.0) {
        path.investor_receipts = {{1U, "contractual-principal-cash",
            principal_receipt, principal_receipt}};
    }
    if (writeoff > 0.0) {
        path.principal_movements.push_back({1U,
            cf::PrincipalMovementKind::Writeoff, writeoff});
    }

    cf::JointScenario scenario;
    scenario.id = std::move(scenario_id);
    scenario.weight = 1.0;
    if (principal_receipt > 0.0) {
        scenario.cash_sources = {make_source("contractual-principal-cash",
            cf::PortfolioCashSource::Commercial, 1U,
            principal_receipt)};
    }
    scenario.project_paths = {std::move(path)};
    portfolio.joint_scenarios = {std::move(scenario)};
    return portfolio;
}

[[nodiscard]] cf::ProbabilityPolytopeConfig point_polytope(
    std::string scenario_id) {
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "point probability v0.2 frontier fixture";
    polytope.source_note = "synthetic one-state Q selector test";
    polytope.scenario_probabilities = {
        {std::move(scenario_id), 1.0, 1.0, 1.0}};
    return polytope;
}

[[nodiscard]] cf::CapitalStackConfig v02_base_stack(
    double reserve_million, double junior_million = 1.0) {
    cf::CapitalStackConfig stack;
    stack.model_version = std::string(cf::kCapitalStackModelVersion);
    stack.scenario_label = "v0.2 frontier base stack template";
    stack.source_note = "synthetic fixed terms for Q frontier testing";
    stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero =
        true;
    stack.subscription_reserve_is_zero_yield_and_lossless = true;
    stack.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    stack.pool_costs_are_additional_pro_rata_calls = true;
    stack.buyer_direct_costs_are_additional_pro_rata_calls = true;
    stack.principal_cash_is_paid_most_senior_first = true;
    stack.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    stack.principal_base_cash_above_issued_principal_is_nonprincipal = true;
    stack.principal_limit_capacity_difference_is_reported_without_valuation_claim =
        true;
    stack.tranching_does_not_change_project_cash_or_gross_loss = true;
    stack.premium_discount_or_fair_value_is_claimed = false;
    stack.underlying_success_participation_fraction = 1.0;
    stack.tranches = {
        {"junior", 0.0, junior_million, 0.0, 0.0, true},
        {"market", junior_million, reserve_million, 0.0, 0.0, false},
    };
    return stack;
}

[[nodiscard]] cf::RobustCapitalMobilizationFrontierConfig
v02_frontier_terms() {
    cf::RobustCapitalMobilizationFrontierConfig terms;
    terms.model_version =
        std::string(cf::kRobustCapitalMobilizationFrontierV02ModelVersion);
    terms.scenario_label = "finite v0.2 q and junior-principal frontier";
    terms.source_note = "synthetic L O Q selector regression terms";
    terms.participation_fraction_grid = {1.0};
    terms.catalytic_first_loss_million_grid = {1.0};
    terms.catalytic_claim_id = "junior";
    terms.market_claim_id = "market";
    terms.market_priority_nonprincipal_cap_million = 0.0;
    terms.catalytic_annual_physical_hurdle_rate = 0.0;
    terms.market_annual_physical_hurdle_rate = 0.0;
    terms.catalytic_target_npv_million = 0.0;
    return terms;
}

[[nodiscard]] cf::RobustCapitalMobilizationFrontierConfig frontier_terms() {
    cf::RobustCapitalMobilizationFrontierConfig terms;
    terms.model_version =
        std::string(cf::kRobustCapitalMobilizationFrontierModelVersion);
    terms.scenario_label = "finite two-claim synthetic hand frontier";
    terms.source_note = "invented mandate and fixed claim terms";
    terms.participation_fraction_grid = {271.0 / 280.0, 25.0 / 28.0};
    terms.catalytic_first_loss_million_grid = {
        14.0, 90.0 / 11.0, 16.0, 12.0};
    terms.market_priority_nonprincipal_cap_million = 1.0;
    terms.catalytic_annual_physical_hurdle_rate = 0.0;
    terms.market_annual_physical_hurdle_rate = 0.0;
    terms.catalytic_target_npv_million = 0.0;
    terms.constraints.minimum_robust_aggregate_npv_million = 0.0;
    terms.constraints.minimum_market_robust_npv_margin_fraction = 0.0;
    terms.constraints.maximum_market_expected_loss_fraction = 0.05;
    terms.constraints.maximum_market_principal_loss_es95_fraction = 0.50;
    terms.constraints.maximum_market_principal_loss_es99_fraction = 0.50;
    terms.constraints.maximum_market_principal_impairment_probability = 0.10;
    terms.constraints.maximum_market_negative_npv_probability = 0.10;
    terms.constraints.maximum_market_npv_shortfall_es95_fraction = 0.51;
    terms.constraints.maximum_market_npv_shortfall_es99_fraction = 0.51;
    terms.constraints.maximum_market_wal_years = 2.0;
    terms.constraints.maximum_catalytic_first_loss_million = 14.0;
    terms.constraints.maximum_catalytic_npv_concession_million = 0.42;
    return terms;
}

[[nodiscard]] const cf::RobustCapitalMobilizationFrontierCandidate&
find_candidate(const cf::RobustCapitalMobilizationFrontierSummary& summary,
    double participation_fraction, double first_loss_million) {
    const auto matching = std::find_if(summary.candidates.begin(),
        summary.candidates.end(),
        [participation_fraction, first_loss_million](const auto& candidate) {
            return near(candidate.participation_fraction,
                       participation_fraction) &&
                near(candidate.catalytic_first_loss_million,
                    first_loss_million);
        });
    if (matching == summary.candidates.end()) {
        throw std::logic_error("test frontier candidate not found");
    }
    return *matching;
}

void test_hand_frontier_and_constraints() {
    const cf::RobustCapitalMobilizationFrontierConfig legacy_terms =
        frontier_terms();
    const cf::RobustCapitalMobilizationFrontierSummary summary =
        cf::evaluate_robust_capital_mobilization_frontier(
            four_state_portfolio(), event_polytope(), participation_terms(),
            legacy_terms);

    check(cf::kRobustCapitalMobilizationFrontierModelVersion == "0.1.0" &&
            cf::kRobustCapitalMobilizationFrontierLegacyModelVersion ==
                cf::kRobustCapitalMobilizationFrontierModelVersion &&
            legacy_terms.model_version ==
                cf::kRobustCapitalMobilizationFrontierModelVersion &&
            summary.model_version ==
                cf::kRobustCapitalMobilizationFrontierModelVersion &&
            near(summary.aggregate_commitment_and_stack_detachment_million,
              20.0) &&
            summary.portfolio_cash_record_count == 28U &&
            summary.portfolio_auxiliary_record_count == 8U &&
            summary.portfolio_record_count == 36U &&
            summary.probability_projection_work_units == 288U &&
            summary.cash_path_work_units == 3'488U &&
            summary.structural_work_units == 3'776U &&
            summary.structural_work_unit_limit == 4'000'000U &&
            summary.candidates.size() == 8U &&
            summary.declared_constraint_count == 12U &&
            summary.evaluated_participation_fraction_grid.front() ==
                25.0 / 28.0 &&
            summary.evaluated_catalytic_first_loss_million_grid.front() ==
                90.0 / 11.0,
        "the original public model-version constant remains an accepted v0.1 four-input contract and the frontier canonicalizes its grid");

    const auto& boundary = find_candidate(summary, 25.0 / 28.0, 12.0);
    check(near(boundary.robust_aggregate_npv_million, 0.0) &&
            near(boundary.robust_market_npv_million, 0.42) &&
            near(boundary.robust_market_npv_margin_fraction, 0.42 / 8.0) &&
            near(boundary.robust_catalytic_npv_million, -0.42) &&
            near(boundary.catalytic_npv_concession_million, 0.42),
        "q=25/28 and A=12 reconcile to the hand-calculated robust NPVs");
    check(near(boundary.worst_market_expected_loss_fraction, 0.05) &&
            near(boundary.worst_market_principal_loss_es95_fraction, 0.50) &&
            near(boundary.worst_market_principal_loss_es99_fraction, 0.50) &&
            near(boundary.worst_market_principal_impairment_probability, 0.10) &&
            near(boundary.worst_market_negative_npv_probability, 0.10) &&
            near(boundary.worst_market_npv_shortfall_es95_fraction, 0.51) &&
            near(boundary.worst_market_npv_shortfall_es99_fraction, 0.51) &&
            boundary.worst_market_wal_years.has_value() &&
            *boundary.worst_market_wal_years < 2.0,
        "market principal loss, NPV downside, and WAL use their own robust projections");
    check(near(boundary.market_expected_principal_cash_distribution_million
                   .minimum.value,
               7.6) &&
            near(boundary.market_expected_principal_cash_distribution_million
                    .maximum.value,
                7.96),
        "market expected principal cash is retained beside WAL");
    check(near(boundary.market_expected_contributions_million.minimum.value,
               8.08) &&
            near(boundary.market_expected_contributions_million.central,
                8.08) &&
            near(boundary.market_expected_contributions_million.maximum.value,
                8.08) &&
            near(boundary.market_expected_total_distributions_million.minimum
                    .value,
                8.50) &&
            near(boundary.market_expected_total_distributions_million.central,
                8.90) &&
            near(boundary.market_expected_total_distributions_million.maximum
                    .value,
                8.95),
        "market contributions expose pool-cost calls while total distributions retain their own range");
    check(boundary.aggregate_fully_funded_npv_million.minimum
                  .scenario_weights.size() == 4U &&
            boundary.catalytic_npv_million.minimum.scenario_weights.size() ==
                4U &&
            boundary.market_npv_million.minimum.scenario_weights.size() == 4U &&
            boundary.market_expected_contributions_million.minimum
                    .scenario_weights.size() == 4U &&
            boundary.market_expected_contributions_million.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_expected_total_distributions_million.minimum
                    .scenario_weights.size() == 4U &&
            boundary.market_expected_total_distributions_million.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_expected_loss_fraction.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_principal_impairment_probability.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_negative_npv_probability.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_principal_loss_es95_million.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_principal_loss_es99_million.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_npv_shortfall_es95_million.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_npv_shortfall_es99_million.maximum
                    .scenario_weights.size() == 4U &&
            boundary.market_principal_cash_wal_years->maximum
                    .scenario_weights.size() == 4U,
        "every mandate metric retains its own complete endpoint witness");
    check(boundary.constraint_passes.robust_aggregate_npv.value_or(false) &&
            boundary.constraint_passes.market_robust_npv_margin.value_or(false) &&
            boundary.constraint_passes.market_expected_loss_fraction.value_or(false) &&
            boundary.constraint_passes.market_principal_loss_es95_fraction.value_or(false) &&
            boundary.constraint_passes.market_principal_loss_es99_fraction.value_or(false) &&
            boundary.constraint_passes.market_principal_impairment_probability.value_or(false) &&
            boundary.constraint_passes.market_negative_npv_probability.value_or(false) &&
            boundary.constraint_passes.market_npv_shortfall_es95_fraction.value_or(false) &&
            boundary.constraint_passes.market_npv_shortfall_es99_fraction.value_or(false) &&
            boundary.constraint_passes.market_wal.value_or(false) &&
            boundary.constraint_passes.catalytic_first_loss.value_or(false) &&
            boundary.constraint_passes.catalytic_npv_concession.value_or(false) &&
            boundary.all_declared_constraints_pass,
        "boundary candidate publishes and passes every declared mandate separately");

    const auto& catalytic_zero =
        find_candidate(summary, 271.0 / 280.0, 12.0);
    check(near(catalytic_zero.robust_aggregate_npv_million, 0.42) &&
            near(catalytic_zero.robust_catalytic_npv_million, 0.0) &&
            near(catalytic_zero.catalytic_npv_concession_million, 0.0),
        "q=271/280 closes the separately projected catalytic concession at A=12");

    const auto& lower_attachment =
        find_candidate(summary, 25.0 / 28.0, 90.0 / 11.0);
    check(near(lower_attachment.robust_market_npv_million, 0.0) &&
            near(lower_attachment.worst_market_expected_loss_fraction,
                0.1 * (16.0 - 90.0 / 11.0) /
                    (20.0 - 90.0 / 11.0)) &&
            near(lower_attachment.worst_market_principal_loss_es95_fraction,
                (16.0 - 90.0 / 11.0) /
                    (20.0 - 90.0 / 11.0)) &&
            !lower_attachment.constraint_passes
                 .market_expected_loss_fraction.value_or(true) &&
            !lower_attachment.all_declared_constraints_pass,
        "A=90/11 is the zero market-NPV boundary but fails the declared loss mandate");

    const auto& protected_market =
        find_candidate(summary, 25.0 / 28.0, 16.0);
    check(near(protected_market.robust_market_npv_million, 0.86) &&
            near(protected_market.worst_market_expected_loss_fraction, 0.0) &&
            near(protected_market.worst_market_principal_loss_es95_fraction,
                0.0) &&
            near(protected_market.worst_market_principal_impairment_probability,
                0.0) &&
            near(protected_market.worst_market_negative_npv_probability,
                0.10) &&
            near(protected_market.worst_market_npv_shortfall_es95_fraction,
                0.01) &&
            !protected_market.constraint_passes.catalytic_first_loss
                 .value_or(true),
        "A=16 eliminates principal loss but retains the disclosed NPV downside");

    check(summary.feasible_candidate_indices.size() == 3U &&
            summary.nondominated_feasible_candidate_indices.size() == 3U &&
            summary.least_first_loss_feasible_by_participation.size() == 2U,
        "frontier reports all feasible, Pareto, and least-A-per-q selections without a score");
    for (const auto& point :
         summary.least_first_loss_feasible_by_participation) {
        check(near(summary.candidates[point.candidate_index]
                       .catalytic_first_loss_million,
                   12.0) &&
                summary.candidates[point.candidate_index]
                    .all_declared_constraints_pass,
            "least-A point references the first feasible candidate at its q");
    }
    check(summary.minimum_tested_feasible_participation_fraction.has_value() &&
            near(*summary.minimum_tested_feasible_participation_fraction,
                25.0 / 28.0) &&
            !summary.weighted_score_or_continuous_optimum_is_claimed &&
            !summary.fair_value_or_market_price_is_estimated &&
            !summary.capital_mobilization_is_established &&
            boundary.audit.maximum_stack_accounting_error_million < 1.0e-8 &&
            boundary.audit.maximum_probability_constraint_violation < 1.0e-8 &&
            boundary.audit.maximum_objective_reconciliation_error < 1.0e-8 &&
            boundary.audit.maximum_tail_mass_violation < 1.0e-8 &&
            boundary.audit
                    .maximum_wal_root_objective_absolute_residual_million_years <
                1.0e-8,
        "frontier disclaims pricing and publishes small numerical controls");
}

void test_undeclared_constraints_do_not_silently_bind() {
    cf::RobustCapitalMobilizationFrontierConfig terms = frontier_terms();
    terms.participation_fraction_grid = {0.50};
    terms.catalytic_first_loss_million_grid = {12.0};
    terms.constraints = {};
    const cf::RobustCapitalMobilizationFrontierSummary summary =
        cf::evaluate_robust_capital_mobilization_frontier(
            four_state_portfolio(), event_polytope(), participation_terms(),
            terms);
    const auto& candidate = summary.candidates.front();
    check(summary.declared_constraint_count == 0U &&
            summary.feasible_candidate_indices.size() == 1U &&
            candidate.all_declared_constraints_pass &&
            !candidate.constraint_passes.robust_aggregate_npv.has_value() &&
            !candidate.constraint_passes.market_robust_npv_margin.has_value() &&
            !candidate.constraint_passes.market_expected_loss_fraction.has_value() &&
            !candidate.constraint_passes.market_principal_loss_es95_fraction.has_value() &&
            !candidate.constraint_passes.market_principal_loss_es99_fraction.has_value() &&
            !candidate.constraint_passes.market_principal_impairment_probability.has_value() &&
            !candidate.constraint_passes.market_negative_npv_probability.has_value() &&
            !candidate.constraint_passes.market_npv_shortfall_es95_fraction.has_value() &&
            !candidate.constraint_passes.market_npv_shortfall_es99_fraction.has_value() &&
            !candidate.constraint_passes.market_wal.has_value() &&
            !candidate.constraint_passes.catalytic_first_loss.has_value() &&
            !candidate.constraint_passes.catalytic_npv_concession.has_value(),
        "absent mandates remain visibly absent and do not create defaults");
}

void test_v02_frontier_uses_issued_principal_cash_shortfall_q() {
    const cf::SuccessParticipationConfig participation = participation_terms();
    check(cf::kRobustCapitalMobilizationFrontierV02ModelVersion == "0.2.0" &&
            v02_frontier_terms().model_version ==
                cf::kRobustCapitalMobilizationFrontierV02ModelVersion,
        "the five-input v0.2 fixture uses its explicit additive model-version constant");

    // A resolved asset bought for 8 with contractual principal 10 returns 8.
    // Asset loss L is 2, but the issued eight-unit liability has Q=0.
    const cf::PortfolioConfig asset_loss = explicit_claim_portfolio(
        "asset-loss-without-liability-shortfall", 8.0, 10.0, 8.0, 2.0);
    const cf::ProbabilityPolytopeConfig asset_loss_polytope =
        point_polytope("asset-loss-without-liability-shortfall");
    const cf::CapitalStackConfig asset_loss_stack = v02_base_stack(8.0);
    cf::RobustCapitalMobilizationFrontierConfig terms =
        v02_frontier_terms();
    terms.constraints.maximum_market_expected_loss_fraction = 0.0;
    terms.constraints.maximum_market_principal_loss_es95_fraction = 0.0;
    terms.constraints.maximum_market_principal_loss_es99_fraction = 0.0;
    terms.constraints.maximum_market_principal_impairment_probability = 0.0;

    const cf::RobustCapitalMobilizationFrontierSummary no_q =
        cf::evaluate_robust_capital_mobilization_frontier(asset_loss,
            asset_loss_polytope, participation, asset_loss_stack, terms);
    const auto& no_q_candidate = no_q.candidates.front();
    check(no_q.model_version == "0.2.0" &&
            no_q.capital_stack_model_version == "0.2.0" &&
            no_q.principal_risk_uses_issued_principal_cash_shortfall_q &&
            no_q.uses_explicit_asset_liability_accounting &&
            near(no_q.aggregate_project_outlay_limit_million, 8.0) &&
            near(no_q.aggregate_contractual_asset_principal_limit_million,
                10.0) &&
            near(no_q.funded_reserve_and_stack_detachment_million, 8.0) &&
            near(no_q_candidate.market_notional_million, 7.0) &&
            no_q_candidate
                .principal_risk_uses_issued_principal_cash_shortfall_q &&
            near(no_q_candidate.worst_market_expected_loss_fraction, 0.0) &&
            near(no_q_candidate.worst_market_principal_loss_es95_fraction,
                0.0) &&
            near(no_q_candidate
                    .worst_market_principal_impairment_probability,
                0.0) &&
            no_q_candidate.all_declared_constraints_pass,
        "v0.2 frontier keeps contractual asset loss L separate from zero market issued-principal shortfall Q");

    // A fully performing ten-unit asset bought with twelve units has L=0 but
    // total issued-liability Q=2. With A=1, the market claim bears Q=1.
    const cf::PortfolioConfig liability_shortfall = explicit_claim_portfolio(
        "liability-shortfall-without-asset-loss", 12.0, 10.0, 10.0, 0.0);
    const cf::ProbabilityPolytopeConfig liability_shortfall_polytope =
        point_polytope("liability-shortfall-without-asset-loss");
    const cf::CapitalStackConfig liability_shortfall_stack =
        v02_base_stack(12.0);
    terms = v02_frontier_terms();
    terms.constraints.maximum_market_expected_loss_fraction = 0.05;

    const cf::RobustCapitalMobilizationFrontierSummary with_q =
        cf::evaluate_robust_capital_mobilization_frontier(
            liability_shortfall, liability_shortfall_polytope,
            participation, liability_shortfall_stack, terms);
    const auto& q_candidate = with_q.candidates.front();
    const cf::CapitalStackProbabilityPolytopeSummary direct =
        cf::evaluate_capital_stack_probability_polytope(liability_shortfall,
            liability_shortfall_polytope, participation,
            liability_shortfall_stack);
    const auto& direct_market = direct.tranches[1];
    check(near(with_q.aggregate_project_outlay_limit_million, 12.0) &&
            near(with_q.aggregate_contractual_asset_principal_limit_million,
                10.0) &&
            near(with_q.funded_reserve_and_stack_detachment_million, 12.0) &&
            near(q_candidate.market_notional_million, 11.0) &&
            near(direct_market.expected_principal_cash_shortfall_million
                    .maximum.value,
                1.0) &&
            near(direct_market.expected_realized_principal_loss_fraction
                    .maximum.value,
                0.0) &&
            near(q_candidate.worst_market_expected_loss_fraction,
                1.0 / 11.0) &&
            near(q_candidate.worst_market_principal_loss_es95_fraction,
                1.0 / 11.0) &&
            near(q_candidate.worst_market_principal_loss_es99_fraction,
                1.0 / 11.0) &&
            near(q_candidate.worst_market_principal_impairment_probability,
                1.0) &&
            !q_candidate.constraint_passes.market_expected_loss_fraction
                 .value_or(true) &&
            !q_candidate.all_declared_constraints_pass &&
            with_q.feasible_candidate_indices.empty(),
        "v0.2 frontier binds the market Q family even when every legacy loss-layering placeholder is zero");

    check_invalid(
        [&] {
            (void)cf::evaluate_robust_capital_mobilization_frontier(
                asset_loss, asset_loss_polytope, participation, terms);
        },
        "v0.2 frontier cannot enter the legacy four-input evaluator");
    cf::RobustCapitalMobilizationFrontierConfig legacy = frontier_terms();
    check_invalid(
        [&] {
            (void)cf::evaluate_robust_capital_mobilization_frontier(
                asset_loss, asset_loss_polytope, participation,
                asset_loss_stack, legacy);
        },
        "v0.1 frontier cannot enter the v0.2 five-input evaluator");
    cf::CapitalStackConfig mismatched = asset_loss_stack;
    mismatched.tranches[1].priority_nonprincipal_cap_million = 0.5;
    check_invalid(
        [&] {
            (void)cf::evaluate_robust_capital_mobilization_frontier(
                asset_loss, asset_loss_polytope, participation, mismatched,
                v02_frontier_terms());
        },
        "v0.2 frontier rejects a base stack whose fixed cap differs from the term record");
}

void test_invalid_frontier_inputs() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::ProbabilityPolytopeConfig polytope = event_polytope();
    const cf::SuccessParticipationConfig participation = participation_terms();

    auto validate = [&](const cf::RobustCapitalMobilizationFrontierConfig& value) {
        cf::validate_robust_capital_mobilization_frontier_config(
            portfolio, polytope, participation, value);
    };

    cf::RobustCapitalMobilizationFrontierConfig invalid = frontier_terms();
    invalid.participation_fraction_grid.clear();
    check_invalid([&]() { validate(invalid); }, "empty q grid is rejected");

    invalid = frontier_terms();
    invalid.catalytic_first_loss_million_grid.clear();
    check_invalid([&]() { validate(invalid); }, "empty A grid is rejected");

    invalid = frontier_terms();
    invalid.participation_fraction_grid = {0.5, 0.5};
    check_invalid([&]() { validate(invalid); }, "duplicate q is rejected");

    invalid = frontier_terms();
    invalid.participation_fraction_grid = {
        std::numeric_limits<double>::quiet_NaN()};
    check_invalid([&]() { validate(invalid); }, "non-finite q is rejected");

    invalid = frontier_terms();
    invalid.participation_fraction_grid = {1.01};
    check_invalid([&]() { validate(invalid); }, "q above one is rejected");

    invalid = frontier_terms();
    invalid.catalytic_first_loss_million_grid = {0.0};
    check_invalid([&]() { validate(invalid); }, "zero first loss is rejected");

    invalid = frontier_terms();
    invalid.catalytic_first_loss_million_grid = {20.0};
    check_invalid([&]() { validate(invalid); }, "A at K is rejected");

    invalid = frontier_terms();
    invalid.market_priority_nonprincipal_cap_million = -0.01;
    check_invalid([&]() { validate(invalid); }, "negative market cap is rejected");

    invalid = frontier_terms();
    invalid.market_annual_physical_hurdle_rate = 11.0;
    check_invalid([&]() { validate(invalid); }, "unsupported claim hurdle is rejected");

    invalid = frontier_terms();
    invalid.market_claim_id = invalid.catalytic_claim_id;
    check_invalid([&]() { validate(invalid); }, "duplicate claim ids are rejected");

    invalid = frontier_terms();
    invalid.catalytic_target_npv_million =
        std::numeric_limits<double>::infinity();
    check_invalid([&]() { validate(invalid); }, "non-finite catalytic target is rejected");

    invalid = frontier_terms();
    invalid.constraints.maximum_market_principal_loss_es95_fraction = 1.01;
    check_invalid([&]() { validate(invalid); }, "ES fraction above one is rejected");

    invalid = frontier_terms();
    invalid.constraints.maximum_catalytic_npv_concession_million = -0.01;
    check_invalid([&]() { validate(invalid); }, "negative concession limit is rejected");

    invalid = frontier_terms();
    invalid.synthetic_inputs = false;
    check_invalid([&]() { validate(invalid); }, "non-synthetic frontier is rejected");

    invalid = frontier_terms();
    invalid.participation_fraction_grid.clear();
    invalid.catalytic_first_loss_million_grid.clear();
    for (std::size_t index = 0U; index < 33U; ++index) {
        invalid.participation_fraction_grid.push_back(
            static_cast<double>(index) / 32.0);
    }
    for (std::size_t index = 0U; index < 32U; ++index) {
        invalid.catalytic_first_loss_million_grid.push_back(
            0.5 + static_cast<double>(index) * 0.5);
    }
    check_invalid([&]() { validate(invalid); },
        "more than 1,024 finite grid candidates are rejected");

    RepeatedStateFixture repeated = repeated_success_states(64U);
    invalid = frontier_terms();
    invalid.participation_fraction_grid.clear();
    invalid.catalytic_first_loss_million_grid.clear();
    for (std::size_t index = 0U; index < 32U; ++index) {
        invalid.participation_fraction_grid.push_back(
            static_cast<double>(index) / 31.0);
        invalid.catalytic_first_loss_million_grid.push_back(
            0.5 + static_cast<double>(index) * 0.5);
    }
    constexpr std::string_view structural_work_error =
        "frontier combined probability-projection and cash-path structural "
        "work exceeds the 4,000,000-unit resource bound";
    check_invalid_equals(
        [&]() {
            cf::validate_robust_capital_mobilization_frontier_config(
                repeated.portfolio, repeated.polytope, participation,
                invalid);
        },
        structural_work_error,
        "combined structural work above 4,000,000 has an exact fail-closed diagnostic");

    repeated = repeated_success_states(1U);
    repeated.portfolio.horizon_months = 2'400U;
    check_invalid_equals(
        [&]() {
            cf::validate_robust_capital_mobilization_frontier_config(
                repeated.portfolio, repeated.polytope, participation,
                invalid);
        },
        structural_work_error,
        "low-state long-horizon cash-path repetition is resource bounded even when probability work is small");

    repeated = repeated_success_states(1U);
    for (std::size_t index = 0U; index < 3'900U; ++index) {
        repeated.portfolio.joint_scenarios.front().pool_costs.push_back(
            cf::MonthlyAmount{0U, 0.0});
    }
    check_invalid_equals(
        [&]() {
            cf::validate_robust_capital_mobilization_frontier_config(
                repeated.portfolio, repeated.polytope, participation,
                invalid);
        },
        structural_work_error,
        "low-state high-record cash-path repetition has an exact fail-closed diagnostic");
}

} // namespace

int main() {
    test_hand_frontier_and_constraints();
    test_undeclared_constraints_do_not_silently_bind();
    test_v02_frontier_uses_issued_principal_cash_shortfall_q();
    test_invalid_frontier_inputs();

    if (failures != 0) {
        std::cerr << failures << " robust capital-mobilization frontier test(s) failed\n";
        return 1;
    }
    std::cout << "robust capital-mobilization frontier tests passed\n";
    return 0;
}
