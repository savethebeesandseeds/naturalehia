// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_issue_price_support.hpp>

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
    std::string_view expected, std::string_view message) {
    try {
        callable();
        check(false, message);
    } catch (const std::invalid_argument& error) {
        check(error.what() == expected, message);
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
    portfolio.scenario_label = "four-state issue-price hand table";
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
    stack.scenario_label = "fixed q and A issue-price base stack";
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

[[nodiscard]] cf::RobustMarketPriorityCapConfig minimally_constrained_terms() {
    cf::RobustMarketPriorityCapConfig terms = cap_terms();
    terms.constraints = {};
    terms.constraints.minimum_market_robust_npv_margin_fraction = -1.0e6;
    return terms;
}

[[nodiscard]] cf::RobustIssuePriceHurdleCaseConfig hurdle(
    std::string id, double rate) {
    cf::RobustIssuePriceHurdleCaseConfig result;
    result.case_id = std::move(id);
    result.annual_effective_hurdle_rate = rate;
    result.source_type =
        cf::RobustIssuePriceHurdleSourceType::SyntheticSensitivity;
    result.reference_price_relation =
        cf::RobustIssuePriceHurdleReferenceRelation::Independent;
    result.as_of_date = "2026-08-30";
    result.source_reference = "unvalidated synthetic hurdle sensitivity";
    result.evidence_record_id = "none";
    result.source_note = "externally supplied test sensitivity";
    return result;
}

[[nodiscard]] cf::RobustIssuePriceSupportConfig issue_terms() {
    cf::RobustIssuePriceSupportConfig terms;
    terms.scenario_label = "finite synthetic issue-price support hand term";
    terms.source_note = "unvalidated synthetic transaction sensitivities";
    terms.synthetic_inputs = true;
    terms.market_claim_principal_is_fully_funded_at_issue = true;
    terms.issue_support_and_price_fund_only_principal_and_issuer_costs = true;
    terms.buyer_direct_cost_stays_outside_subscription_reserve = true;
    terms.support_changes_no_claim_right_or_project_cash = true;
    terms.physical_probability_polytope_is_unchanged = true;
    terms.fair_value_or_market_price_is_estimated = false;

    terms.reference_price.record_id = "synthetic-reference-price";
    terms.reference_price.status =
        cf::RobustIssuePriceReferenceStatus::InternalCandidate;
    terms.reference_price.market_claim_id = "market-priority";
    terms.reference_price.normalized_term_result_id =
        "fixed-priority-cap-result";
    terms.reference_price.gross_issue_price_million = 6.5;
    terms.reference_price.claim_quantity_million = 8.0;
    terms.reference_price.quantity_basis =
        "full contractual market principal";
    terms.reference_price.price_basis = "gross buyer cash at month zero";
    terms.reference_price.currency_label = "DEMO";
    terms.reference_price.monetary_basis = "constant test units at close";
    terms.reference_price.execution_date = "none";
    terms.reference_price.settlement_date = "none";
    terms.reference_price.issuer_cost_million = 0.0;
    terms.reference_price.buyer_direct_cost_million = 0.0;
    terms.reference_price.side_rights_or_non_cash_consideration_present =
        false;
    terms.reference_price.side_rights_or_non_cash_consideration_note = "none";
    terms.reference_price.source_reference =
        "unvalidated synthetic reference price";
    terms.reference_price.evidence_record_id = "none";

    terms.support.support_id = "synthetic-issue-support";
    terms.support.status =
        cf::RobustIssuePriceSupportCapacityStatus::SyntheticCandidate;
    terms.support.maximum_support_million = 1.5;
    terms.support.settled_support_million = 0.0;
    terms.support.as_of_date = "none";
    terms.support.source_reference =
        "unvalidated synthetic support capacity";
    terms.support.evidence_record_id = "none";
    terms.support.source_note = "no provider authority or cash established";
    terms.support.support_is_non_repayable = true;
    terms.support
        .support_receives_no_repayment_participation_security_or_recovery_rights =
        true;
    terms.support.support_is_not_project_revenue = true;
    terms.support
        .support_does_not_pay_future_pool_costs_or_cover_project_losses = true;

    terms.hurdle_cases = {hurdle("twenty-percent", 0.20),
        hurdle("zero-percent", 0.0), hurdle("ten-percent", 0.10),
        hurdle("five-percent", 0.05), hurdle("fifteen-percent", 0.15)};
    return terms;
}

[[nodiscard]] cf::RobustIssuePriceSupportSummary evaluate(
    const cf::RobustIssuePriceSupportConfig& issue,
    const cf::PortfolioConfig& portfolio = four_state_portfolio(),
    const cf::CapitalStackConfig& stack = base_stack(),
    const cf::RobustMarketPriorityCapConfig& caps = cap_terms()) {
    return cf::evaluate_robust_issue_price_support(portfolio,
        event_polytope(), participation_terms(), stack, caps, issue);
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
                path.investor_receipts = {{12U, source_id, 7.0, 5.0},
                    {24U, source_id, 7.0, 5.0}};
            }
        }
    }
    return portfolio;
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
    fixture.portfolio.joint_scenarios.clear();
    fixture.polytope.scenario_label = "repeated issue-price states";
    fixture.polytope.source_note = "resource validation test only";
    const double weight = 1.0 / static_cast<double>(count);
    for (std::size_t index = 0U; index < count; ++index) {
        cf::JointScenario scenario = success_template;
        scenario.id = "repeated-success-" + std::to_string(index);
        scenario.weight = weight;
        fixture.portfolio.joint_scenarios.push_back(scenario);
        fixture.polytope.scenario_probabilities.push_back(
            {scenario.id, 0.0, weight, 1.0});
    }
    return fixture;
}

void test_hand_fixture_and_resource_oracle() {
    const cf::RobustIssuePriceSupportSummary summary = evaluate(issue_terms());
    check(summary.status ==
                cf::RobustIssuePriceSupportStatus::FinanceableWindowFound &&
            summary.upstream_priority_cap_status ==
                cf::RobustMarketPriorityCapStatus::
                    MinimumTestedBalancedCapFound &&
            summary.selected_priority_cap_candidate_index == 3U &&
            summary.selected_market_priority_nonprincipal_cap_million
                .has_value() &&
            near(*summary.selected_market_priority_nonprincipal_cap_million,
                8.0 / 15.0) &&
            summary.selected_priority_cap_is_balanced,
        "the status-driven upstream selection fixes the exact B=8/15 claim");

    check(summary.portfolio_cash_record_count == 28U &&
            summary.portfolio_auxiliary_record_count == 8U &&
            summary.portfolio_record_count == 36U &&
            summary.upstream_priority_cap_work_units == 2'360U &&
            summary.hurdle_stack_work_units == 2'360U &&
            summary.reference_projection_work_units == 1'440U &&
            summary.scenario_month_audit_work_units == 1'500U &&
            summary.structural_work_units == 7'660U &&
            summary.structural_work_unit_limit == 4'000'000U,
        "the hand fixture publishes the exact grid(D)+grid(H)+H*8*N*W+H*N*3*T resource oracle");

    check(summary.hurdle_cases.size() == 5U &&
            summary.financeable_hurdle_case_indices ==
                std::vector<std::size_t>({0U, 1U, 2U}) &&
            summary.funded_support_covered_hurdle_case_indices.empty() &&
            summary.literal_zero_hurdle_case_index == 0U,
        "permuted hurdle inputs canonicalize and synthetic support never becomes funded capacity");

    const std::vector<double> expected_rates{0.0, 0.05, 0.10, 0.15, 0.20};
    for (std::size_t index = 0U; index < expected_rates.size(); ++index) {
        const auto& result = summary.hurdle_cases[index];
        const double d = 1.0 + expected_rates[index];
        const double robust = 0.80 / d + 7.28 / (d * d) - 0.08;
        const double central =
            0.80 / d + (2'866.0 / 375.0) / (d * d) - 0.08;
        const double maximum = 1.02 / d + 7.468 / (d * d) - 0.08;
        check(near(result.annual_effective_hurdle_rate,
                  expected_rates[index]) &&
                near(result.raw_robust_investor_price_ceiling_million,
                    robust) &&
                near(result.raw_central_investor_price_boundary_million,
                    central) &&
                near(result.raw_maximum_investor_price_boundary_million,
                    maximum) &&
                result.minimum_support_capacity_for_overlap_million
                    .has_value() &&
                near(*result.minimum_support_capacity_for_overlap_million,
                    8.0 - robust) &&
                result.support_shortfall_million.has_value() &&
                near(*result.support_shortfall_million,
                    std::max(0.0, 8.0 - robust - 1.5)),
            "each canonical hurdle matches the exact physical-boundary formulas and support arithmetic");
        check(result.reference_price.has_value() &&
                near(result.reference_price->robust_investor_npv_million,
                    robust - 6.5) &&
                result.audit.raw_price_ceiling_zero_npv_reconciles &&
                result.audit.reference_price_npv_shift_reconciles &&
                result.audit.issue_funding_identity_reconciles &&
                result.audit.sparse_market_monthly_ledger_reconciles,
            "raw ceiling, reference shift, sources and uses, and sparse monthly cash reconcile independently");
    }

    const auto& zero = summary.hurdle_cases[0];
    const auto& ten = summary.hurdle_cases[2];
    const auto& fifteen = summary.hurdle_cases[3];
    check(zero.modeled_financeable_price_window_exists &&
            zero.financeable_price_window_lower_million == 6.5 &&
            zero.financeable_price_window_upper_million == 8.0 &&
            zero.modeled_overlap_exists_without_support &&
            !zero.documented_support_commitment_covers_overlap &&
            !zero.funded_support_capacity_covers_overlap &&
            ten.modeled_financeable_price_window_exists &&
            !ten.modeled_overlap_exists_without_support &&
            !ten.funded_support_covered_price_window_exists &&
            !fifteen.modeled_financeable_price_window_exists,
        "modeled windows, zero-support cases, and evidence-qualified support coverage remain distinct");

    const auto& reference = *ten.reference_price;
    const double common_loss_shortfall = 6.58 - 4.0 / 1.10;
    check(near(reference.required_issue_support_million, 1.5) &&
            near(reference.observed_settled_support_million, 0.0) &&
            near(reference.support_capacity_margin_million, 0.0) &&
            near(reference.unused_support_capacity_million, 0.0) &&
            near(reference.support_capacity_shortfall_million, 0.0) &&
            near(reference.modeled_required_issue_sources_million, 8.0) &&
            near(reference.modeled_amount_entering_subscription_reserve_million,
                8.0) &&
            near(reference.modeled_issuer_cost_paid_million, 0.0) &&
            !reference.observed_issue_sources_million.has_value() &&
            !reference.observed_primary_funding_completed,
        "modeled required support never masquerades as an observed draw or reserve entry");
    check(near(reference.negative_npv_probability.minimum.value, 0.01) &&
            near(reference.negative_npv_probability.central, 0.02) &&
            near(reference.negative_npv_probability.maximum.value, 0.10) &&
            near(reference.npv_shortfall_es95_million.minimum.value,
                common_loss_shortfall * 0.01 / 0.05) &&
            near(reference.npv_shortfall_es95_million.central,
                common_loss_shortfall * 0.02 / 0.05) &&
            near(reference.npv_shortfall_es95_million.maximum.value,
                common_loss_shortfall) &&
            near(reference.npv_shortfall_es99_million.minimum.value,
                common_loss_shortfall) &&
            near(reference.npv_shortfall_es99_million.maximum.value,
                common_loss_shortfall),
        "the 10% reference case retains independent probability and tail witnesses");

    check(near(ten.principal_risk.contractual_market_notional_million, 8.0) &&
            near(ten.principal_risk.worst_principal_loss_es95_fraction, 0.5) &&
            near(ten.principal_risk.worst_principal_loss_es99_fraction, 0.5) &&
            summary.base_stack_was_not_mutated &&
            summary.only_market_hurdle_changed_across_cases &&
            summary.all_contractual_cash_and_principal_risk_invariants_hold &&
            !summary.market_hurdle_is_discovered_or_empirically_calibrated &&
            !summary.fair_value_or_accounting_value_is_estimated &&
            !summary.support_provider_authority_or_budget_is_established &&
            !summary.support_counterparty_or_performance_risk_is_modeled,
        "principal denominators, structural invariants, and interpretation boundaries remain explicit");
}

void test_transaction_evidence_and_hurdle_axes() {
    cf::RobustIssuePriceSupportConfig candidate = issue_terms();
    candidate.synthetic_inputs = false;
    const auto candidate_summary = evaluate(candidate);
    check(!candidate_summary.hurdle_cases[1]
               .reference_price->observed_primary_funding_completed,
        "a non-synthetic flag does not turn an internal candidate into observed funding");

    cf::RobustIssuePriceSupportConfig committed = candidate;
    committed.support.status =
        cf::RobustIssuePriceSupportCapacityStatus::ContractuallyCommitted;
    committed.support.as_of_date = "2026-08-28";
    committed.support.source_reference = "signed support commitment";
    committed.support.evidence_record_id = "support-commitment-001";
    const auto committed_summary = evaluate(committed);
    check(committed_summary.hurdle_cases[1]
              .documented_support_commitment_covers_overlap &&
            !committed_summary.hurdle_cases[1]
                 .funded_support_capacity_covers_overlap &&
            !committed_summary.hurdle_cases[1]
                 .funded_support_covered_price_window_exists,
        "documented commitment is disclosed without being called funded capacity");

    cf::RobustIssuePriceSupportConfig funded = committed;
    funded.support.status =
        cf::RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed;
    funded.support.funding_evidenced = true;
    const auto funded_summary = evaluate(funded);
    check(funded_summary.hurdle_cases[1]
              .documented_support_commitment_covers_overlap &&
            funded_summary.hurdle_cases[1]
                .funded_support_capacity_covers_overlap &&
            funded_summary.funded_support_covered_hurdle_case_indices ==
                std::vector<std::size_t>({1U, 2U}),
        "funded or escrowed support covers only positive-support modeled windows");

    cf::RobustIssuePriceSupportConfig settled = candidate;
    settled.reference_price.status =
        cf::RobustIssuePriceReferenceStatus::SettledPrimary;
    settled.reference_price.execution_date = "2026-08-28";
    settled.reference_price.settlement_date = "2026-08-29";
    settled.reference_price.source_reference = "settled primary subscription";
    settled.reference_price.evidence_record_id = "primary-settlement-001";
    settled.reference_price.buyer_cash_payment_evidenced = true;
    settled.reference_price.settlement_evidenced = true;
    settled.support.status =
        cf::RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    settled.support.settled_support_million = 1.5;
    settled.support.funding_evidenced = true;
    settled.support.settlement_evidenced = true;
    settled.support.as_of_date = "2026-08-29";
    settled.support.source_reference = "settled support transfer";
    settled.support.evidence_record_id = "support-settlement-001";
    const auto source_only_summary = evaluate(settled);
    const auto& source_only_reference =
        *source_only_summary.hurdle_cases[2].reference_price;
    check(source_only_reference.observed_primary_price_cash_completed &&
            source_only_reference.observed_support_cash_completed &&
            source_only_reference
                .observed_issue_sources_settled_and_reconciled &&
            source_only_reference.observed_issue_sources_million == 8.0 &&
            !source_only_reference.observed_primary_funding_completed &&
            !source_only_reference
                 .observed_amount_entering_subscription_reserve_million
                 .has_value() &&
            !source_only_reference.observed_issuer_cost_paid_million
                 .has_value(),
        "settled and reconciled source cash alone does not prove either destination use or completed primary funding");

    settled.reference_price.subscription_reserve_deposit_evidenced = true;
    settled.reference_price.issue_use_evidence_record_id =
        "primary-use-evidence-001";
    const auto settled_summary = evaluate(settled);
    const auto& settled_reference =
        *settled_summary.hurdle_cases[2].reference_price;
    check(settled_reference
              .observed_issue_sources_settled_and_reconciled &&
            settled_reference.observed_primary_funding_completed &&
            settled_reference
                    .observed_amount_entering_subscription_reserve_million ==
                8.0 &&
            !settled_reference.observed_issuer_cost_paid_million.has_value(),
        "reconciled sources plus independently evidenced reserve deposit prove a zero-issuer-cost primary funding completion");

    cf::RobustIssuePriceSupportConfig settled_with_cost = settled;
    settled_with_cost.reference_price.issuer_cost_million = 0.25;
    settled_with_cost.reference_price.issuer_cost_payment_evidenced = true;
    settled_with_cost.support.maximum_support_million = 1.75;
    settled_with_cost.support.settled_support_million = 1.75;
    const auto cost_settled_summary = evaluate(settled_with_cost);
    const auto& cost_settled_reference =
        *cost_settled_summary.hurdle_cases[2].reference_price;
    check(cost_settled_reference
              .observed_issue_sources_settled_and_reconciled &&
            cost_settled_reference.observed_primary_funding_completed &&
            cost_settled_reference
                    .observed_amount_entering_subscription_reserve_million ==
                8.0 &&
            cost_settled_reference.observed_issuer_cost_paid_million == 0.25,
        "positive issuer cost requires its own use-side payment evidence before full funding completion");

    cf::RobustIssuePriceSupportConfig secondary = candidate;
    secondary.reference_price.status =
        cf::RobustIssuePriceReferenceStatus::SettledSecondary;
    secondary.reference_price.execution_date = "2026-08-28";
    secondary.reference_price.settlement_date = "2026-08-29";
    secondary.reference_price.source_reference = "settled secondary transfer";
    secondary.reference_price.evidence_record_id = "secondary-settlement-001";
    secondary.reference_price.buyer_cash_payment_evidenced = true;
    secondary.reference_price.settlement_evidenced = true;
    const auto evidence_only = evaluate(secondary);
    check(!evidence_only.hurdle_cases[2].reference_price_numerically_eligible &&
            !evidence_only.hurdle_cases[2].reference_price.has_value() &&
            near(evidence_only.hurdle_cases[2]
                    .raw_robust_investor_price_ceiling_million,
                settled_summary.hurdle_cases[2]
                    .raw_robust_investor_price_ceiling_million),
        "an unnormalized settled secondary record is evidence-only without changing claim cash");
    secondary.reference_price
        .secondary_price_normalized_to_full_month_zero_claim = true;
    const auto normalized_secondary = evaluate(secondary);
    check(normalized_secondary.hurdle_cases[2]
              .reference_price_numerically_eligible &&
            normalized_secondary.hurdle_cases[2].reference_price.has_value() &&
            !normalized_secondary.hurdle_cases[2]
                 .reference_price->observed_primary_funding_completed,
        "external full-claim normalization permits numerical reconciliation but never project-funding evidence");

    cf::RobustIssuePriceSupportConfig nonindependent = candidate;
    nonindependent.hurdle_cases[2].reference_price_relation =
        cf::RobustIssuePriceHurdleReferenceRelation::
            ModelImpliedFromReferencePrice;
    nonindependent.hurdle_cases[4].reference_price_relation =
        cf::RobustIssuePriceHurdleReferenceRelation::Unresolved;
    const auto circular = evaluate(nonindependent);
    const auto model_implied = std::find_if(circular.hurdle_cases.begin(),
        circular.hurdle_cases.end(), [](const auto& item) {
            return item.case_id == "ten-percent";
        });
    const auto unresolved = std::find_if(circular.hurdle_cases.begin(),
        circular.hurdle_cases.end(), [](const auto& item) {
            return item.case_id == "fifteen-percent";
        });
    check(model_implied != circular.hurdle_cases.end() &&
            unresolved != circular.hurdle_cases.end() &&
            model_implied->status ==
                cf::RobustIssuePriceSupportCaseStatus::
                    HurdleNotIndependentOfReferencePrice &&
            unresolved->status ==
                cf::RobustIssuePriceSupportCaseStatus::
                    HurdleNotIndependentOfReferencePrice &&
            !model_implied->modeled_financeable_price_window_exists &&
            model_implied->reference_price.has_value() &&
            !model_implied->reference_price->investor_term_adequate,
        "transaction settlement, hurdle independence, and sensitivity calculation stay separate axes");
}

void test_price_support_and_cash_path_boundaries() {
    cf::RobustIssuePriceSupportConfig terms = issue_terms();
    terms.reference_price.gross_issue_price_million = 8.0;
    terms.support.maximum_support_million = 0.0;
    auto summary = evaluate(terms);
    check(summary.hurdle_cases[0].reference_price->required_issue_support_million ==
                0.0 &&
            summary.hurdle_cases[0]
                .modeled_overlap_exists_without_support &&
            summary.hurdle_cases[0].reference_price
                ->modeled_joint_term_adequate,
        "P=M and G=0 reconcile without manufacturing a support draw");

    terms = issue_terms();
    terms.reference_price.gross_issue_price_million = 0.0;
    terms.support.maximum_support_million = 8.0;
    summary = evaluate(terms);
    check(summary.issuer_funding_floor_million == 0.0 &&
            summary.hurdle_cases[0].reference_price->required_issue_support_million ==
                8.0 &&
            summary.hurdle_cases[0].financeable_price_window_lower_million ==
                0.0 &&
            !summary.hurdle_cases[0]
                 .funded_support_covered_price_window_exists,
        "P=0 and G=M remain modeled arithmetic without synthetic funding readiness");

    terms = issue_terms();
    terms.reference_price.issuer_cost_million = 0.25;
    terms.reference_price.buyer_direct_cost_million = 0.10;
    terms.support.maximum_support_million = 1.75;
    summary = evaluate(terms);
    const auto& cost_case = summary.hurdle_cases[2];
    check(near(summary.issuer_funding_floor_million, 6.50) &&
            near(cost_case.raw_robust_investor_price_ceiling_million,
                6.56380165289256) &&
            near(cost_case.reference_price->required_issue_support_million,
                1.75) &&
            near(cost_case.reference_price
                    ->modeled_amount_entering_subscription_reserve_million,
                8.0) &&
            near(cost_case.reference_price->modeled_issuer_cost_paid_million,
                0.25) &&
            near(cost_case.reference_price
                    ->buyer_direct_cost_outside_reserve_million,
                0.10),
        "F changes issue uses while C changes investor value and stays outside the reserve");

    terms = issue_terms();
    terms.reference_price.buyer_direct_cost_million = 1.0;
    terms.hurdle_cases = {hurdle("zero", 0.0), hurdle("upper", 10.0)};
    summary = evaluate(terms);
    const auto& upper = summary.hurdle_cases[1];
    check(upper.status ==
                cf::RobustIssuePriceSupportCaseStatus::
                    NoNonnegativeInvestorPrice &&
            upper.raw_robust_investor_price_ceiling_million < 0.0 &&
            !upper.minimum_support_capacity_for_overlap_million.has_value() &&
            !upper.support_shortfall_million.has_value() &&
            !upper.modeled_financeable_price_window_exists,
        "negative raw price ceilings leave G_min and support shortfall explicitly N/A");

    cf::RobustMarketPriorityCapConfig high_cap = cap_terms();
    high_cap.constraints = {};
    high_cap.constraints.minimum_market_robust_npv_margin_fraction = 0.05;
    terms = issue_terms();
    summary = evaluate(terms, four_state_portfolio(), base_stack(), high_cap);
    check(summary.selected_market_priority_nonprincipal_cap_million == 1.0 &&
            summary.hurdle_cases[0]
                    .raw_robust_investor_price_ceiling_million >
                8.0 &&
            summary.hurdle_cases[0]
                    .admissible_investor_price_ceiling_million ==
                8.0 &&
            summary.hurdle_cases[0]
                    .minimum_support_capacity_for_overlap_million ==
                0.0,
        "a ceiling above complete issue uses is clipped without creating surplus proceeds");

    terms = issue_terms();
    const auto dated = evaluate(
        terms, multiple_payment_date_portfolio(), base_stack(), cap_terms());
    check(std::all_of(dated.hurdle_cases.begin(), dated.hurdle_cases.end(),
              [](const auto& item) {
                  return item.audit.sparse_market_monthly_ledger_reconciles &&
                      item.audit.market_contractual_cash_is_unchanged &&
                      item.audit.junior_cash_and_own_hurdle_npv_are_unchanged;
              }),
        "future pool calls and multiple distribution dates reconcile in every sparse monthly pass");

    cf::CapitalStackConfig thin = base_stack();
    thin.tranches[0].detachment_million = 20.0 - 1.0e-6;
    thin.tranches[1].attachment_million = 20.0 - 1.0e-6;
    terms = issue_terms();
    terms.reference_price.claim_quantity_million = 1.0e-6;
    terms.reference_price.gross_issue_price_million = 1.0e-6;
    terms.support.maximum_support_million = 0.0;
    const auto unit = evaluate(
        terms, four_state_portfolio(), thin, minimally_constrained_terms());
    check(near(unit.fixed_market_notional_million, 1.0e-6, 1.0e-6) &&
            std::isfinite(unit.hurdle_cases[0]
                    .raw_robust_investor_price_ceiling_million) &&
            near(unit.hurdle_cases[0]
                    .principal_risk.contractual_market_notional_million,
                1.0e-6, 1.0e-6),
        "the one-base-currency-unit notional boundary remains finite and keeps its principal denominator");
}

void test_event_witness_switch_and_no_selection() {
    constexpr double switch_rate = 34.0 / 15.0;
    constexpr double step = 1.0e-9;
    cf::RobustIssuePriceSupportConfig terms = issue_terms();
    terms.hurdle_cases = {hurdle("zero", 0.0),
        hurdle("switch-below", switch_rate - step),
        hurdle("switch-exact", switch_rate),
        hurdle("switch-above", switch_rate + step),
        hurdle("allowed-upper", 10.0)};
    const auto switched = evaluate(terms);
    check(switched.hurdle_cases.size() == 5U &&
            switched.hurdle_cases.back().annual_effective_hurdle_rate ==
                10.0 &&
            switched.hurdle_cases[1]
                    .market_par_npv_million.minimum.scenario_weights !=
                switched.hurdle_cases[3]
                    .market_par_npv_million.minimum.scenario_weights &&
            near(switched.hurdle_cases[1]
                    .raw_robust_investor_price_ceiling_million,
                switched.hurdle_cases[3]
                    .raw_robust_investor_price_ceiling_million,
                1.0e-8),
        "finite cases bracket the exact h=34/15 event-witness switch and accept h=10");

    cf::RobustMarketPriorityCapConfig impossible = cap_terms();
    impossible.constraints.minimum_robust_aggregate_npv_million = 100.0;
    const auto unavailable = evaluate(
        issue_terms(), four_state_portfolio(), base_stack(), impossible);
    check(unavailable.status ==
                cf::RobustIssuePriceSupportStatus::
                    PriorityCapSelectionUnavailable &&
            !unavailable.selected_priority_cap_candidate_index.has_value() &&
            !unavailable
                 .selected_market_priority_nonprincipal_cap_million
                 .has_value() &&
            unavailable.hurdle_cases.empty() &&
            unavailable.financeable_hurdle_case_indices.empty(),
        "upstream no-selection is a successful unavailable result without fake index zero or B=0");
}

void test_direct_api_validation_and_resource_failure() {
    const auto validate = [](const cf::RobustIssuePriceSupportConfig& terms,
                              const cf::CapitalStackConfig& stack =
                                  base_stack()) {
        cf::validate_robust_issue_price_support_config(four_state_portfolio(),
            event_polytope(), participation_terms(), stack, cap_terms(), terms);
    };

    cf::RobustIssuePriceSupportConfig invalid = issue_terms();
    invalid.reference_price.claim_quantity_million = 0.0;
    check_invalid(
        [&] { cf::validate_robust_issue_price_support_config(invalid); },
        "the one-argument public validator rejects zero claim quantity");

    invalid = issue_terms();
    invalid.synthetic_inputs = false;
    invalid.support.status =
        cf::RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    invalid.support.maximum_support_million = 1.0;
    invalid.support.settled_support_million = 1.5;
    invalid.support.funding_evidenced = true;
    invalid.support.settlement_evidenced = true;
    invalid.support.as_of_date = "2026-08-29";
    invalid.support.source_reference = "settled support transfer";
    invalid.support.evidence_record_id = "support-settlement-over-capacity";
    check_invalid(
        [&] { cf::validate_robust_issue_price_support_config(invalid); },
        "the one-argument public validator rejects settled support above capacity");

    invalid = issue_terms();
    invalid.reference_price.subscription_reserve_deposit_evidenced = true;
    invalid.reference_price.issue_use_evidence_record_id = "use-record-001";
    check_invalid(
        [&] { cf::validate_robust_issue_price_support_config(invalid); },
        "use-side evidence cannot be attached to a synthetic internal candidate");

    invalid = issue_terms();
    invalid.synthetic_inputs = false;
    invalid.reference_price.status =
        cf::RobustIssuePriceReferenceStatus::SettledPrimary;
    invalid.reference_price.execution_date = "2026-08-28";
    invalid.reference_price.settlement_date = "2026-08-29";
    invalid.reference_price.source_reference = "settled primary subscription";
    invalid.reference_price.evidence_record_id = "primary-settlement-001";
    invalid.reference_price.buyer_cash_payment_evidenced = true;
    invalid.reference_price.settlement_evidenced = true;
    invalid.reference_price.subscription_reserve_deposit_evidenced = true;
    check_invalid(
        [&] { cf::validate_robust_issue_price_support_config(invalid); },
        "claimed issue uses require a separate non-placeholder evidence record id");
    invalid.reference_price.issue_use_evidence_record_id = "use-record-001";
    invalid.reference_price.issuer_cost_payment_evidenced = true;
    check_invalid(
        [&] { cf::validate_robust_issue_price_support_config(invalid); },
        "zero issuer cost cannot claim an issuer-cost payment record");

    invalid = issue_terms();
    invalid.hurdle_cases[0].annual_effective_hurdle_rate = -0.01;
    check_invalid([&] { validate(invalid); },
        "negative hurdle rates are outside the v0.1 [0,10] domain");
    invalid = issue_terms();
    invalid.hurdle_cases[0].annual_effective_hurdle_rate = -0.0;
    check_invalid([&] { validate(invalid); },
        "negative-zero hurdle rates are rejected");
    invalid = issue_terms();
    invalid.hurdle_cases[0].annual_effective_hurdle_rate =
        std::nextafter(10.0, std::numeric_limits<double>::infinity());
    check_invalid([&] { validate(invalid); },
        "the h=10 upper endpoint is exact");

    invalid = issue_terms();
    invalid.reference_price.gross_issue_price_million =
        std::nextafter(8.0, std::numeric_limits<double>::infinity());
    check_invalid([&] { validate(invalid); },
        "P above M+F is rejected exactly instead of tolerance-passing negative support");
    invalid = issue_terms();
    invalid.support.maximum_support_million =
        std::nextafter(8.0, std::numeric_limits<double>::infinity());
    check_invalid([&] { validate(invalid); },
        "G above M+F is rejected exactly instead of tolerance-passing a negative floor");

    cf::RobustIssuePriceSupportConfig settled = issue_terms();
    settled.synthetic_inputs = false;
    settled.support.status =
        cf::RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    settled.support.settled_support_million =
        std::nextafter(1.5, std::numeric_limits<double>::infinity());
    settled.support.funding_evidenced = true;
    settled.support.settlement_evidenced = true;
    settled.support.as_of_date = "2026-08-29";
    settled.support.source_reference = "settled support transfer";
    settled.support.evidence_record_id = "support-settlement-001";
    check_invalid([&] { validate(settled); },
        "S_obs above G is rejected exactly");

    cf::CapitalStackConfig decimal_stack = base_stack();
    decimal_stack.tranches[0].detachment_million = 19.8;
    decimal_stack.tranches[1].attachment_million = 19.8;
    invalid = issue_terms();
    invalid.reference_price.claim_quantity_million = 0.3 - 0.1;
    invalid.reference_price.gross_issue_price_million = 0.0;
    invalid.support.maximum_support_million = 0.0;
    try {
        cf::validate_robust_issue_price_support_config(four_state_portfolio(),
            event_polytope(), participation_terms(), decimal_stack,
            minimally_constrained_terms(), invalid);
        check(true,
            "claim quantity uses the disclosed money tolerance for decimal-equivalent M");
    } catch (...) {
        check(false,
            "claim quantity uses the disclosed money tolerance for decimal-equivalent M");
    }
    invalid.reference_price.claim_quantity_million = 0.200001;
    check_invalid(
        [&] {
            cf::validate_robust_issue_price_support_config(
                four_state_portfolio(), event_polytope(),
                participation_terms(), decimal_stack,
                minimally_constrained_terms(), invalid);
        },
        "a materially mismatched claim quantity is rejected");

    invalid = issue_terms();
    invalid.reference_price.side_rights_or_non_cash_consideration_present =
        true;
    invalid.reference_price.side_rights_or_non_cash_consideration_note =
        "warrant";
    check_invalid([&] { validate(invalid); },
        "side rights fail closed because the all-cash price shift is unidentified");
    invalid = issue_terms();
    invalid.reference_price.side_rights_or_non_cash_consideration_note =
        "nothing identified";
    check_invalid([&] { validate(invalid); },
        "eligible all-cash runs require the exact none note");
    invalid = issue_terms();
    invalid.reference_price.currency_label = "OTHER";
    check_invalid([&] { validate(invalid); },
        "currency and monetary basis must match upstream exactly");
    invalid = issue_terms();
    invalid.reference_price.quantity_basis = "one quoted unit";
    check_invalid([&] { validate(invalid); },
        "per-unit quantity bases cannot silently enter full-claim arithmetic");
    invalid = issue_terms();
    invalid.reference_price.price_basis = "net cash at later settlement";
    check_invalid([&] { validate(invalid); },
        "net or later-date price bases cannot silently enter month-zero gross-cash arithmetic");

    invalid = issue_terms();
    invalid.synthetic_inputs = false;
    invalid.reference_price.status =
        cf::RobustIssuePriceReferenceStatus::NonbindingIndication;
    invalid.reference_price.normalized_term_result_id = "unnamed-result";
    invalid.reference_price.source_reference = "documented indication";
    invalid.reference_price.evidence_record_id = "indication-001";
    check_invalid([&] { validate(invalid); },
        "every non-internal factual price status requires identified normalized terms");

    invalid = issue_terms();
    invalid.hurdle_cases[2].as_of_date = "2026-02-30";
    check_invalid([&] { validate(invalid); },
        "direct API hurdle provenance rejects impossible calendar dates");
    invalid = issue_terms();
    invalid.synthetic_inputs = false;
    invalid.reference_price.status =
        cf::RobustIssuePriceReferenceStatus::SettledPrimary;
    invalid.reference_price.normalized_term_result_id = "unnamed-result";
    invalid.reference_price.execution_date = "2026-08-30";
    invalid.reference_price.settlement_date = "2026-08-29";
    invalid.reference_price.source_reference = "settled primary record";
    invalid.reference_price.evidence_record_id = "settled-primary-001";
    invalid.reference_price.buyer_cash_payment_evidenced = true;
    invalid.reference_price.settlement_evidenced = true;
    check_invalid([&] { validate(invalid); },
        "real executed or settled records require normalized terms and ordered real dates");
    invalid.reference_price.normalized_term_result_id = "fixed-result";
    check_invalid([&] { validate(invalid); },
        "settlement cannot precede execution through the direct API");

    invalid = issue_terms();
    invalid.reference_price.buyer_direct_cost_million = 1.0e9 + 1.0;
    check_invalid([&] { validate(invalid); },
        "money inputs fail closed above the 1e9-million guardrail");

    cf::RobustIssuePriceSupportConfig many = issue_terms();
    many.hurdle_cases.clear();
    for (std::size_t index = 0U;
         index < cf::kRobustIssuePriceSupportMaximumHurdleCases; ++index) {
        many.hurdle_cases.push_back(hurdle(
            "resource-hurdle-" + std::to_string(index),
            index == 0U ? 0.0 : 0.10));
    }
    const RepeatedStateFixture repeated = repeated_success_states(64U);
    constexpr std::string_view resource_message =
        "issue-price-support combined priority-cap, hurdle-stack, reference-"
        "projection, and scenario-month structural work exceeds the "
        "4,000,000-unit resource bound";
    check_invalid_equals(
        [&] {
            cf::validate_robust_issue_price_support_config(repeated.portfolio,
                repeated.polytope, participation_terms(), base_stack(),
                cap_terms(), many);
        },
        resource_message,
        "checked combined structural work fails closed with the stable diagnostic");
}

} // namespace

int main() {
    test_hand_fixture_and_resource_oracle();
    test_transaction_evidence_and_hurdle_axes();
    test_price_support_and_cash_path_boundaries();
    test_event_witness_switch_and_no_selection();
    test_direct_api_validation_and_resource_failure();

    if (failures != 0) {
        std::cerr << failures << " robust issue-price support test(s) failed\n";
        return 1;
    }
    std::cout << "robust issue-price support tests passed\n";
    return 0;
}
