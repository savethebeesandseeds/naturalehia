// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_market_priority_cap.hpp>

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

[[nodiscard]] bool near_vector(const std::vector<double>& first,
    const std::vector<double>& second, double tolerance = 1.0e-10) {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.size(); ++index) {
        if (!near(first[index], second[index], tolerance)) {
            return false;
        }
    }
    return true;
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
        check(std::string_view(error.what()) == expected_text, message);
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

[[nodiscard]] cf::PortfolioConfig four_state_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "four-state priority-cap hand table";
    portfolio.source_note = "synthetic unit-test cash paths only";
    portfolio.currency_label = "DEMO";
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

[[nodiscard]] cf::PortfolioConfig explicit_ledger_four_state_portfolio() {
    cf::PortfolioConfig portfolio = four_state_portfolio();
    for (cf::PortfolioProject& project : portfolio.projects) {
        project.principal_accounting_mode =
            cf::PrincipalAccountingMode::ExplicitContractualLedger;
        project.principal_limit_million = project.commitment_million;
    }
    for (cf::JointScenario& scenario : portfolio.joint_scenarios) {
        for (cf::ProjectJointPath& path : scenario.project_paths) {
            path.capital_draws.clear();
            path.investor_outlays = {{0U,
                cf::InvestorOutlayPurpose::ClaimPurchasePrice, 10.0}};
            path.principal_movements = {{0U,
                cf::PrincipalMovementKind::FundedPrincipalAddition, 10.0}};
            double principal_cash = 0.0;
            for (const cf::InvestorReceipt& receipt : path.investor_receipts) {
                principal_cash += receipt.principal_component_million;
            }
            if (principal_cash < 10.0) {
                path.principal_movements.push_back({portfolio.horizon_months,
                    cf::PrincipalMovementKind::Writeoff,
                    10.0 - principal_cash});
            }
        }
    }
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
        "repeated-state priority-cap resource fixture";
    fixture.portfolio.joint_scenarios.clear();
    fixture.polytope.scenario_label =
        "repeated-state priority-cap resource polytope";
    fixture.polytope.source_note =
        "synthetic repeated states for validation-only resource tests";
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

[[nodiscard]] cf::CapitalStackConfig base_stack() {
    cf::CapitalStackConfig stack;
    stack.scenario_label = "fixed q and A priority-cap base stack";
    stack.source_note = "invented fully funded two-claim unit-test terms";
    stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero = true;
    stack.subscription_reserve_is_zero_yield_and_lossless = true;
    stack.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    stack.pool_costs_are_additional_pro_rata_calls = true;
    stack.principal_cash_is_paid_most_senior_first = true;
    stack.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    stack.tranching_does_not_change_project_cash_or_gross_loss = true;
    stack.premium_discount_or_fair_value_is_claimed = false;
    stack.underlying_success_participation_fraction = 25.0 / 28.0;
    stack.tranches = {
        {"catalytic-first-loss", 0.0, 12.0, 0.0, 0.0, true},
        {"market-priority", 12.0, 20.0, 1.0, 0.0, false},
    };
    return stack;
}

[[nodiscard]] cf::CapitalStackConfig explicit_asset_liability_stack() {
    cf::CapitalStackConfig stack = base_stack();
    stack.model_version = std::string(cf::kCapitalStackModelVersion);
    stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero = false;
    stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero =
        true;
    stack.buyer_direct_costs_are_additional_pro_rata_calls = true;
    stack.principal_base_cash_above_issued_principal_is_nonprincipal = true;
    stack.principal_limit_capacity_difference_is_reported_without_valuation_claim =
        true;
    return stack;
}

[[nodiscard]] cf::RobustMarketPriorityCapConfig cap_terms() {
    cf::RobustMarketPriorityCapConfig terms;
    terms.scenario_label = "finite synthetic hand priority-cap term";
    terms.source_note = "invented cap grid and mandate";
    terms.market_priority_nonprincipal_cap_million_grid = {
        1.0, 0.08, 8.0 / 15.0, 0.0, 0.50};
    terms.contractual_ceiling_million = 1.0;
    terms.junior_target_npv_million = 0.0;
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
    terms.constraints.maximum_catalytic_first_loss_million = 12.0;
    terms.constraints.maximum_catalytic_npv_concession_million = 0.0;
    return terms;
}

[[nodiscard]] cf::RobustCapitalMobilizationFrontierConfig
single_frontier_terms() {
    cf::RobustCapitalMobilizationFrontierConfig terms;
    terms.scenario_label = "single B=1 cross-economics frontier";
    terms.source_note = "invented fixed frontier point";
    terms.participation_fraction_grid = {25.0 / 28.0};
    terms.catalytic_first_loss_million_grid = {12.0};
    terms.market_priority_nonprincipal_cap_million = 1.0;
    terms.catalytic_target_npv_million = 0.0;
    terms.constraints = cap_terms().constraints;
    terms.constraints.maximum_catalytic_npv_concession_million = 0.42;
    return terms;
}

[[nodiscard]] cf::RobustMarketPriorityCapConfig large_cap_grid() {
    cf::RobustMarketPriorityCapConfig terms = cap_terms();
    terms.market_priority_nonprincipal_cap_million_grid.clear();
    for (std::size_t index = 0U; index < 1'024U; ++index) {
        terms.market_priority_nonprincipal_cap_million_grid.push_back(
            static_cast<double>(index) * 1.0e-6);
    }
    terms.contractual_ceiling_million = 1'023.0e-6;
    return terms;
}

[[nodiscard]] cf::RobustMarketPriorityCapConfig minimally_constrained_terms(
    std::vector<double> grid, double ceiling) {
    cf::RobustMarketPriorityCapConfig terms = cap_terms();
    terms.market_priority_nonprincipal_cap_million_grid = std::move(grid);
    terms.contractual_ceiling_million = ceiling;
    terms.constraints = {};
    terms.constraints.minimum_market_robust_npv_margin_fraction = -1.0e6;
    return terms;
}

[[nodiscard]] cf::PortfolioConfig multiple_payment_date_portfolio() {
    cf::PortfolioConfig portfolio = four_state_portfolio();
    for (cf::JointScenario& scenario : portfolio.joint_scenarios) {
        for (cf::ScenarioCashSource& source : scenario.cash_sources) {
            if (source.kind == cf::PortfolioCashSource::Commercial) {
                source.cash_available = {{12U, 7.0}, {24U, 7.0}};
            }
        }
        for (cf::ProjectJointPath& path : scenario.project_paths) {
            if (path.investor_receipts.size() == 1U &&
                path.investor_receipts.front().amount_million == 14.0) {
                const std::string source_id =
                    path.investor_receipts.front().cash_source_id;
                path.investor_receipts = {
                    {12U, source_id, 7.0, 5.0},
                    {24U, source_id, 7.0, 5.0},
                };
            }
        }
    }
    return portfolio;
}

void test_hand_boundary_and_complete_report() {
    const cf::CapitalStackConfig original_stack = base_stack();
    const cf::RobustMarketPriorityCapSummary summary =
        cf::evaluate_robust_market_priority_cap(four_state_portfolio(),
            event_polytope(), participation_terms(), original_stack,
            cap_terms());

    check(summary.evaluated_market_priority_cap_million_grid ==
                std::vector<double>({0.0, 0.08, 0.50, 8.0 / 15.0, 1.0}) &&
            summary.candidates.size() == 5U &&
            summary.portfolio_cash_record_count == 28U &&
            summary.portfolio_auxiliary_record_count == 8U &&
            summary.portfolio_record_count == 36U &&
            summary.probability_projection_work_units == 180U &&
            summary.cash_path_work_units == 2'180U &&
            summary.structural_work_units == 2'360U &&
            summary.declared_fixed_structure_constraint_count == 7U &&
            summary.declared_cap_sensitive_market_constraint_count == 4U &&
            summary.junior_concession_constraint_is_declared,
        "cap term canonicalizes the five-point hand grid and publishes the complete work preflight");

    const auto& zero = summary.candidates[0];
    const auto& sign_boundary = summary.candidates[1];
    const auto& lower = summary.candidates[2];
    const auto& root = summary.candidates[3];
    const auto& reference = summary.candidates[4];
    check(near(zero.robust_market_npv_million, -0.48) &&
            near(zero.worst_market_negative_npv_probability, 1.0) &&
            near(sign_boundary.robust_market_npv_million, -0.408) &&
            near(sign_boundary.worst_market_negative_npv_probability, 0.10) &&
            near(lower.robust_market_npv_million, -0.03) &&
            near(root.market_priority_nonprincipal_cap_million, 8.0 / 15.0) &&
            near(root.robust_market_npv_million, 0.0) &&
            near(root.robust_junior_npv_million, 0.0) &&
            near(root.junior_npv_concession_million, 0.0) &&
            near(reference.robust_market_npv_million, 0.42) &&
            near(reference.robust_junior_npv_million, -0.42) &&
            near(reference.junior_npv_concession_million, 0.42),
        "B=8/15 is the exact tested market/junior NPV boundary and B=1 transfers 0.42 farther");

    check(near(root.market_expected_contributions_million.minimum.value, 8.08) &&
            near(root.market_expected_contributions_million.central, 8.08) &&
            near(root.market_expected_contributions_million.maximum.value, 8.08) &&
            near(root.junior_expected_contributions_million.minimum.value, 12.12) &&
            near(root.market_npv_shortfall_es95_million.maximum.value, 4.08) &&
            near(root.market_npv_shortfall_es99_million.maximum.value, 4.08) &&
            near(root.worst_market_principal_loss_es95_fraction, 0.50) &&
            near(root.worst_market_principal_loss_es99_fraction, 0.50) &&
            near(root.worst_market_expected_loss_fraction, 0.05) &&
            near(root.worst_market_principal_impairment_probability, 0.10) &&
            root.worst_market_wal_years.has_value() &&
            near(*root.worst_market_wal_years, 1.92211055, 1.0e-6),
        "candidate retains contributions, NPV tails, and every fixed principal-risk measure");

    check(near(root.market_expired_priority_cap_capacity_million.minimum.value,
               (8.0 / 15.0) * 0.01) &&
            near(root.market_expired_priority_cap_capacity_million.central,
                (8.0 / 15.0) * 0.02) &&
            near(root.market_expired_priority_cap_capacity_million.maximum.value,
                (8.0 / 15.0) * 0.10) &&
            root.market_expired_priority_cap_capacity_million.minimum
                    .scenario_weights.size() == 4U &&
            root.market_npv_million.minimum.scenario_weights.size() == 4U &&
            root.junior_npv_million.minimum.scenario_weights.size() == 4U &&
            root.market_npv_shortfall_es95_million.maximum
                    .tail_mass_weights.size() == 4U,
        "expired capacity, NPV, and tail endpoints retain their complete witnesses");

    check(root.fixed_structure_eligible &&
            root.cap_sensitive_market_mandates_pass && root.market_adequate &&
            root.junior_concession_limit_passes && root.balanced &&
            reference.market_adequate &&
            !reference.junior_concession_limit_passes &&
            !reference.balanced &&
            summary.market_adequate_candidate_indices ==
                std::vector<std::size_t>({3U, 4U}) &&
            summary.balanced_candidate_indices ==
                std::vector<std::size_t>({3U}) &&
            summary.minimum_tested_market_adequate_candidate_index == 3U &&
            summary.previous_tested_candidate_before_market_adequate_index ==
                2U &&
            summary.minimum_tested_balanced_candidate_index == 3U &&
            summary.previous_tested_candidate_before_balanced_index == 2U &&
            summary.base_reference_candidate_index == 4U &&
            summary.contractual_ceiling_candidate_index == 4U &&
            summary.status ==
                cf::RobustMarketPriorityCapStatus::
                    MinimumTestedBalancedCapFound &&
            cf::to_string(summary.status) ==
                "minimum-tested-balanced-cap-found",
        "selection distinguishes fixed eligibility, market adequacy, balance, previous cap, reference, and ceiling");

    const auto& audit = summary.grid_audit;
    check(audit.base_stack_was_not_mutated &&
            audit.market_contributions_are_invariant &&
            audit.market_principal_cash_is_invariant &&
            audit.market_principal_risk_is_invariant &&
            audit.market_principal_wal_is_invariant &&
            audit.market_nonprincipal_cash_is_nondecreasing &&
            audit.market_path_npv_is_nondecreasing &&
            audit.junior_nonprincipal_cash_is_nonincreasing &&
            audit.junior_path_npv_is_nonincreasing &&
            audit.market_negative_npv_probability_is_nonincreasing &&
            audit.market_npv_shortfall_tails_are_nonincreasing &&
            audit.market_cash_gained_equals_junior_cash_surrendered &&
            audit.aggregate_cash_is_invariant &&
            audit.pool_hurdle_npv_is_invariant &&
            audit.maximum_cash_transfer_reconciliation_error_million < 1.0e-8 &&
            root.audit.maximum_probability_constraint_violation < 1.0e-8 &&
            root.audit.maximum_tail_mass_violation < 1.0e-8,
        "grid controls establish cap monotonicity, fixed economics, cash conservation, and small audits");
    check(original_stack.tranches[1].priority_nonprincipal_cap_million == 1.0,
        "caller-owned base stack remains unchanged");

    check(!summary.continuous_minimum_or_optimized_contract_is_claimed &&
            !summary.market_hurdle_is_solved_or_empirically_calibrated &&
            !summary.expected_investor_return_or_annualized_yield_is_estimated &&
            !summary.fair_value_issue_price_or_market_spread_is_estimated &&
            !summary.investor_demand_or_suitability_is_established &&
            !summary.legal_form_enforceability_or_regulatory_treatment_is_validated &&
            !summary.capital_mobilization_or_crowding_in_is_established,
        "every prohibited pricing, demand, legal, and mobilization claim remains false");
}

void test_b1_matches_frontier_including_witnesses() {
    const auto cap_summary = cf::evaluate_robust_market_priority_cap(
        four_state_portfolio(), event_polytope(), participation_terms(),
        base_stack(), cap_terms());
    const auto frontier_summary =
        cf::evaluate_robust_capital_mobilization_frontier(
            four_state_portfolio(), event_polytope(), participation_terms(),
            single_frontier_terms());
    const auto& cap = cap_summary.candidates[4];
    const auto& frontier = frontier_summary.candidates.front();

    check(near(cap.robust_market_npv_million,
              frontier.robust_market_npv_million) &&
            near(cap.robust_junior_npv_million,
                frontier.robust_catalytic_npv_million) &&
            near(cap.junior_npv_concession_million,
                frontier.catalytic_npv_concession_million) &&
            near(cap.market_npv_million.minimum.value,
                frontier.market_npv_million.minimum.value) &&
            near(cap.market_npv_million.central,
                frontier.market_npv_million.central) &&
            near(cap.market_npv_million.maximum.value,
                frontier.market_npv_million.maximum.value) &&
            near_vector(cap.market_npv_million.minimum.scenario_weights,
                frontier.market_npv_million.minimum.scenario_weights) &&
            near_vector(cap.market_npv_million.maximum.scenario_weights,
                frontier.market_npv_million.maximum.scenario_weights) &&
            near_vector(cap.junior_npv_million.minimum.scenario_weights,
                frontier.catalytic_npv_million.minimum.scenario_weights) &&
            near_vector(
                cap.market_principal_loss_es95_million.maximum.scenario_weights,
                frontier.market_principal_loss_es95_million.maximum
                    .scenario_weights) &&
            near_vector(cap.market_npv_shortfall_es99_million.maximum
                            .tail_mass_weights,
                frontier.market_npv_shortfall_es99_million.maximum
                    .tail_mass_weights),
        "B=1 reproduces the frontier economics and endpoint witnesses");
}

void test_status_falsification_and_unequal_hurdles() {
    cf::RobustMarketPriorityCapConfig terms = cap_terms();
    terms.market_priority_nonprincipal_cap_million_grid = {0.0, 1.0};
    terms.constraints.maximum_catalytic_npv_concession_million = 0.0;
    const auto no_overlap = cf::evaluate_robust_market_priority_cap(
        four_state_portfolio(), event_polytope(), participation_terms(),
        base_stack(), terms);
    check(no_overlap.status ==
                cf::RobustMarketPriorityCapStatus::
                    MarketAndJuniorRequirementsDoNotOverlap &&
            no_overlap.minimum_tested_market_adequate_candidate_index == 1U &&
            !no_overlap.minimum_tested_balanced_candidate_index.has_value(),
        "market adequacy at B=1 and a zero junior limit reports no overlap");

    terms = cap_terms();
    terms.market_priority_nonprincipal_cap_million_grid = {0.0, 0.50, 1.0};
    terms.contractual_ceiling_million = 1.0;
    terms.constraints.minimum_market_robust_npv_margin_fraction = 0.10;
    terms.constraints.maximum_catalytic_npv_concession_million.reset();
    const auto no_pass = cf::evaluate_robust_market_priority_cap(
        four_state_portfolio(), event_polytope(), participation_terms(),
        base_stack(), terms);
    check(no_pass.status ==
                cf::RobustMarketPriorityCapStatus::
                    NoTestedMarketAdequateCap &&
            no_pass.market_adequate_candidate_indices.empty(),
        "an insufficient ceiling reports no tested market-adequate cap");

    terms = cap_terms();
    terms.constraints.maximum_market_expected_loss_fraction = 0.049;
    const auto fixed_ineligible = cf::evaluate_robust_market_priority_cap(
        four_state_portfolio(), event_polytope(), participation_terms(),
        base_stack(), terms);
    check(fixed_ineligible.status ==
                cf::RobustMarketPriorityCapStatus::FixedStructureIneligible &&
            std::none_of(fixed_ineligible.candidates.begin(),
                fixed_ineligible.candidates.end(), [](const auto& candidate) {
                    return candidate.fixed_structure_eligible ||
                        candidate.market_adequate;
                }),
        "a fixed principal-risk failure is not presented as curable by cap");

    cf::CapitalStackConfig unequal = base_stack();
    unequal.tranches[0].annual_physical_hurdle_rate = 0.10;
    unequal.tranches[1].annual_physical_hurdle_rate = 0.05;
    terms = cap_terms();
    terms.constraints.minimum_market_robust_npv_margin_fraction = -1.0;
    terms.constraints.maximum_catalytic_npv_concession_million.reset();
    const auto unequal_summary = cf::evaluate_robust_market_priority_cap(
        four_state_portfolio(), event_polytope(), participation_terms(),
        unequal, terms);
    check(unequal_summary.grid_audit.market_path_npv_is_nondecreasing &&
            unequal_summary.grid_audit.junior_path_npv_is_nonincreasing &&
            unequal_summary.grid_audit.pool_hurdle_npv_is_invariant &&
            unequal_summary.model_limitation.find(
                "must not be added when their hurdles differ") !=
                std::string::npos,
        "unequal own hurdles preserve monotonicity without a false NPV conservation claim");
}

void test_v02_uses_issued_principal_cash_shortfall_risk() {
    const cf::PortfolioConfig portfolio =
        explicit_ledger_four_state_portfolio();
    const cf::CapitalStackConfig stack = explicit_asset_liability_stack();
    const cf::RobustMarketPriorityCapSummary summary =
        cf::evaluate_robust_market_priority_cap(portfolio, event_polytope(),
            participation_terms(), stack, cap_terms());
    const auto& root = summary.candidates[3];
    check(near(root.worst_market_expected_loss_fraction, 0.05) &&
            near(root.worst_market_principal_loss_es95_fraction, 0.50) &&
            near(root.worst_market_principal_loss_es99_fraction, 0.50) &&
            near(root.worst_market_principal_impairment_probability, 0.10) &&
            root.fixed_structure_eligible && root.market_adequate &&
            summary.grid_audit.market_principal_risk_is_invariant,
        "v0.2 market mandates use nonzero issued-principal cash-shortfall expectation, tails, and incidence");

    const cf::PortfolioConfig legacy_projects = four_state_portfolio();
    const cf::RobustMarketPriorityCapSummary legacy_project_summary =
        cf::evaluate_robust_market_priority_cap(legacy_projects,
            event_polytope(), participation_terms(), stack, cap_terms());
    const auto& legacy_project_root = legacy_project_summary.candidates[3];
    check(std::all_of(legacy_projects.projects.begin(),
              legacy_projects.projects.end(), [](const auto& project) {
                  return project.principal_accounting_mode ==
                      cf::PrincipalAccountingMode::DrawEqualsPrincipalLegacy;
              }) &&
            near(legacy_project_root.worst_market_expected_loss_fraction,
                0.05) &&
            near(legacy_project_root
                    .worst_market_principal_loss_es95_fraction,
                0.50) &&
            near(legacy_project_root
                    .worst_market_principal_loss_es99_fraction,
                0.50) &&
            near(legacy_project_root
                    .worst_market_principal_impairment_probability,
                0.10) &&
            legacy_project_root.fixed_structure_eligible &&
            legacy_project_summary.grid_audit
                .market_principal_risk_is_invariant,
        "v0.2 cash-shortfall metrics remain available when every project uses the legacy at-par ledger");

    cf::RobustMarketPriorityCapConfig strict = cap_terms();
    strict.constraints.maximum_market_expected_loss_fraction = 0.049;
    const cf::RobustMarketPriorityCapSummary rejected =
        cf::evaluate_robust_market_priority_cap(legacy_projects,
            event_polytope(),
            participation_terms(), stack, strict);
    check(rejected.status ==
                cf::RobustMarketPriorityCapStatus::FixedStructureIneligible &&
            std::none_of(rejected.candidates.begin(),
                rejected.candidates.end(), [](const auto& candidate) {
                    return candidate.fixed_structure_eligible;
                }),
        "v0.2 cash-shortfall risk fails a binding fixed mandate instead of reading legacy zero placeholders");
}

void test_cash_boundaries_and_structural_edge_cases() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::ProbabilityPolytopeConfig polytope = event_polytope();
    const cf::SuccessParticipationConfig participation = participation_terms();

    cf::CapitalStackConfig zero_q_stack = base_stack();
    zero_q_stack.underlying_success_participation_fraction = 0.0;
    cf::RobustMarketPriorityCapConfig terms =
        minimally_constrained_terms({0.0, 1.0}, 1.0);
    terms.constraints.minimum_market_robust_npv_margin_fraction = 0.0;
    const auto zero_q = cf::evaluate_robust_market_priority_cap(portfolio,
        polytope, participation, zero_q_stack, terms);
    check(near(zero_q.candidates[0]
                   .market_expected_nonprincipal_cash_distribution_million
                   .maximum.value,
               0.0) &&
            near(zero_q.candidates[1]
                    .market_expected_nonprincipal_cash_distribution_million
                    .maximum.value,
                0.0) &&
            near(zero_q.candidates[1]
                    .market_expired_priority_cap_capacity_million.minimum.value,
                1.0) &&
            near(zero_q.candidates[1]
                    .market_expired_priority_cap_capacity_million.maximum.value,
                1.0) &&
            zero_q.status ==
                cf::RobustMarketPriorityCapStatus::
                    NoTestedMarketAdequateCap,
        "q=0 creates no transferable nonprincipal cash and cannot repair the market mandate");

    constexpr double saturation = 50.0 / 7.0;
    constexpr double saturation_step = 1.0e-6;
    terms = minimally_constrained_terms(
        {0.0, 1.0, saturation - saturation_step, saturation,
            saturation + saturation_step},
        saturation + saturation_step);
    const auto saturated = cf::evaluate_robust_market_priority_cap(portfolio,
        polytope, participation, base_stack(), terms);
    const auto& below_saturation = saturated.candidates[2];
    const auto& at_saturation = saturated.candidates[3];
    const auto& above_saturation = saturated.candidates[4];
    check(below_saturation.market_expected_nonprincipal_cash_distribution_million
                  .central <
            at_saturation.market_expected_nonprincipal_cash_distribution_million
                .central &&
            near(at_saturation.market_expected_nonprincipal_cash_distribution_million
                     .minimum.value,
                above_saturation
                    .market_expected_nonprincipal_cash_distribution_million
                    .minimum.value) &&
            near(at_saturation.market_expected_nonprincipal_cash_distribution_million
                     .central,
                above_saturation
                    .market_expected_nonprincipal_cash_distribution_million
                    .central) &&
            near(at_saturation.market_npv_million.minimum.value,
                above_saturation.market_npv_million.minimum.value) &&
            above_saturation.market_expired_priority_cap_capacity_million
                    .minimum.value >
                at_saturation.market_expired_priority_cap_capacity_million
                    .minimum.value &&
            saturated.grid_audit.market_nonprincipal_cash_is_nondecreasing &&
            saturated.grid_audit.junior_nonprincipal_cash_is_nonincreasing,
        "below/at/above cash saturation stops changing economics and leaves only additional expired capacity");

    cf::CapitalStackConfig dated_stack = base_stack();
    dated_stack.tranches[0].annual_physical_hurdle_rate = 0.10;
    dated_stack.tranches[1].annual_physical_hurdle_rate = 0.05;
    terms = minimally_constrained_terms({0.0, 0.50, 1.0}, 1.0);
    const auto dated = cf::evaluate_robust_market_priority_cap(
        multiple_payment_date_portfolio(), polytope, participation,
        dated_stack, terms);
    check(near(dated.candidates.back()
                   .market_expected_nonprincipal_cash_distribution_million
                   .minimum.value,
               0.90) &&
            dated.grid_audit.market_path_npv_is_nondecreasing &&
            dated.grid_audit.junior_path_npv_is_nonincreasing &&
            dated.grid_audit.market_cash_gained_equals_junior_cash_surrendered &&
            dated.grid_audit.aggregate_cash_is_invariant,
        "two nonprincipal payment dates reconcile dated transfers under positive unequal hurdles");

    cf::CapitalStackConfig thin_market = base_stack();
    thin_market.tranches[0].detachment_million = 20.0 - 1.0e-6;
    thin_market.tranches[1].attachment_million = 20.0 - 1.0e-6;
    terms = minimally_constrained_terms({0.0, 1.0}, 1.0);
    const auto thin = cf::evaluate_robust_market_priority_cap(portfolio,
        polytope, participation, thin_market, terms);
    check(near(thin.fixed_market_notional_million, 1.0e-6, 1.0e-6) &&
            near(thin.candidates.back().market_notional_million,
                1.0e-6, 1.0e-6) &&
            std::isfinite(
                thin.candidates.back().robust_market_npv_margin_fraction),
        "one-base-currency-unit market notional stays finite at the allowed boundary");

    cf::CapitalStackConfig protected_market = base_stack();
    protected_market.tranches[0].detachment_million = 16.0;
    protected_market.tranches[1].attachment_million = 16.0;
    terms = minimally_constrained_terms({0.0, 1.0}, 1.0);
    terms.constraints = {};
    terms.constraints.maximum_market_negative_npv_probability = 0.10;
    const auto protected_summary = cf::evaluate_robust_market_priority_cap(
        portfolio, polytope, participation, protected_market, terms);
    const auto& protected_b1 = protected_summary.candidates.back();
    check(near(protected_b1.worst_market_expected_loss_fraction, 0.0) &&
            near(protected_b1.worst_market_principal_loss_es95_fraction, 0.0) &&
            near(protected_b1.worst_market_principal_impairment_probability,
                0.0) &&
            near(protected_b1.worst_market_negative_npv_probability, 0.10) &&
            near(protected_b1.market_npv_shortfall_es95_million.maximum.value,
                0.04) &&
            near(protected_b1.worst_market_npv_shortfall_es95_fraction, 0.01),
        "A=16 removes market principal write-down but not the common-loss return shortfall");

    constexpr double sign_step = 2.0e-10;
    terms = minimally_constrained_terms(
        {0.0, 0.08 - sign_step, 0.08, 0.08 + sign_step, 1.0}, 1.0);
    terms.constraints = {};
    terms.constraints.maximum_market_negative_npv_probability = 0.10;
    const auto sign = cf::evaluate_robust_market_priority_cap(portfolio,
        polytope, participation, base_stack(), terms);
    check(near(sign.candidates[1].worst_market_negative_npv_probability, 1.0) &&
            near(sign.candidates[2].worst_market_negative_npv_probability,
                0.10) &&
            near(sign.candidates[3].worst_market_negative_npv_probability,
                0.10) &&
            !sign.candidates[1]
                 .constraint_passes.market_negative_npv_probability
                 .value_or(true) &&
            sign.candidates[2]
                .constraint_passes.market_negative_npv_probability
                .value_or(false) &&
            sign.candidates[3]
                .constraint_passes.market_negative_npv_probability
                .value_or(false),
        "just-below/at/above B=0.08 uses the disclosed money tolerance for pathwise sign classification");
}

void test_invalid_grids_and_base_stacks() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::ProbabilityPolytopeConfig polytope = event_polytope();
    const cf::SuccessParticipationConfig participation = participation_terms();
    auto validate = [&](const cf::CapitalStackConfig& stack,
                        const cf::RobustMarketPriorityCapConfig& term) {
        cf::validate_robust_market_priority_cap_config(
            portfolio, polytope, participation, stack, term);
    };

    cf::RobustMarketPriorityCapConfig invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid.clear();
    check_invalid([&]() { validate(base_stack(), invalid); },
        "empty cap grid is rejected");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid = {0.0, 0.5, 0.5, 1.0};
    check_invalid([&]() { validate(base_stack(), invalid); },
        "duplicate cap is rejected");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid = {0.08, 1.0};
    check_invalid([&]() { validate(base_stack(), invalid); },
        "omitted zero is rejected");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid = {0.0, 1.0e-7, 1.0};
    check_invalid([&]() { validate(base_stack(), invalid); },
        "positive sub-unit cap is rejected");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid = {-0.0, 1.0};
    check_invalid([&]() { validate(base_stack(), invalid); },
        "negative zero cap is rejected rather than silently canonicalized");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid = {0.0, 1.0};
    invalid.contractual_ceiling_million = 2.0;
    check_invalid([&]() { validate(base_stack(), invalid); },
        "omitted contractual ceiling is rejected");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid = {0.0, 0.5, 2.0};
    invalid.contractual_ceiling_million = 2.0;
    check_invalid([&]() { validate(base_stack(), invalid); },
        "omitted base reference cap is rejected");

    invalid = cap_terms();
    invalid.market_priority_nonprincipal_cap_million_grid[0] =
        std::numeric_limits<double>::quiet_NaN();
    check_invalid([&]() { validate(base_stack(), invalid); },
        "non-finite cap is rejected");

    invalid = cap_terms();
    invalid.constraints = {};
    invalid.constraints.maximum_market_expected_loss_fraction = 0.05;
    check_invalid([&]() { validate(base_stack(), invalid); },
        "term without a cap-sensitive market mandate is rejected");

    cf::CapitalStackConfig invalid_stack = base_stack();
    invalid_stack.tranches[1].id = "wrong-market-id";
    check_invalid([&]() { validate(invalid_stack, cap_terms()); },
        "mismatched market claim id is rejected");

    invalid_stack = base_stack();
    invalid_stack.tranches.push_back(
        {"third", 20.0, 21.0, 0.0, 0.0, false});
    check_invalid([&]() { validate(invalid_stack, cap_terms()); },
        "base stack with other than two tranches is rejected");

    invalid_stack = base_stack();
    invalid_stack.nonprincipal_cash_is_paid_to_caps_then_residual = false;
    check_invalid([&]() { validate(invalid_stack, cap_terms()); },
        "base stack failing structural assertions is rejected");
}

void test_resource_boundaries_fail_closed() {
    constexpr std::string_view structural_work_error =
        "market priority-cap combined probability-projection and cash-path "
        "structural work exceeds the 4,000,000-unit resource bound";
    const cf::SuccessParticipationConfig participation = participation_terms();
    const cf::CapitalStackConfig stack = base_stack();
    cf::RobustMarketPriorityCapConfig term = large_cap_grid();

    RepeatedStateFixture repeated = repeated_success_states(64U);
    check_invalid_equals(
        [&]() {
            cf::validate_robust_market_priority_cap_config(repeated.portfolio,
                repeated.polytope, participation, stack, term);
        },
        structural_work_error,
        "probability-heavy cap grid has the exact fail-closed diagnostic");

    repeated = repeated_success_states(1U);
    repeated.portfolio.horizon_months = 2'400U;
    check_invalid_equals(
        [&]() {
            cf::validate_robust_market_priority_cap_config(repeated.portfolio,
                repeated.polytope, participation, stack, term);
        },
        structural_work_error,
        "low-state long-horizon cap grid has the exact fail-closed diagnostic");

    repeated = repeated_success_states(1U);
    for (std::size_t index = 0U; index < 3'900U; ++index) {
        repeated.portfolio.joint_scenarios.front().pool_costs.push_back(
            cf::MonthlyAmount{0U, 0.0});
    }
    check_invalid_equals(
        [&]() {
            cf::validate_robust_market_priority_cap_config(repeated.portfolio,
                repeated.polytope, participation, stack, term);
        },
        structural_work_error,
        "high portfolio-record cap grid has the exact fail-closed diagnostic");

    repeated = repeated_success_states(1U);
    for (std::size_t index = 0U; index < 3'900U; ++index) {
        repeated.portfolio.loss_layers.push_back(cf::LossLayer{
            "resource-layer-" + std::to_string(index), 0.0, 20.0});
    }
    check_invalid_equals(
        [&]() {
            cf::validate_robust_market_priority_cap_config(repeated.portfolio,
                repeated.polytope, participation, stack, term);
        },
        structural_work_error,
        "loss-layer record multiplier has the exact fail-closed diagnostic");
}

} // namespace

int main() {
    test_hand_boundary_and_complete_report();
    test_b1_matches_frontier_including_witnesses();
    test_status_falsification_and_unequal_hurdles();
    test_v02_uses_issued_principal_cash_shortfall_risk();
    test_cash_boundaries_and_structural_edge_cases();
    test_invalid_grids_and_base_stacks();
    test_resource_boundaries_fail_closed();

    if (failures != 0) {
        std::cerr << failures << " robust market priority-cap test(s) failed\n";
        return 1;
    }
    std::cout << "robust market priority-cap tests passed\n";
    return 0;
}
