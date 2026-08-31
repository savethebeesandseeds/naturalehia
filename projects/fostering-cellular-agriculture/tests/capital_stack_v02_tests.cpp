// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack.hpp>
#include <naturalehia/cellular_finance/capital_stack_probability_polytope.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
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

[[nodiscard]] bool zero_range(const cf::AmbiguityMetricRange& range) {
    return near(range.minimum.value, 0.0) && near(range.central, 0.0) &&
        near(range.maximum.value, 0.0);
}

[[nodiscard]] bool zero_range(
    const cf::ProbabilityPolytopeMetricRange& range) {
    return near(range.minimum.value, 0.0) && near(range.central, 0.0) &&
        near(range.maximum.value, 0.0);
}

[[nodiscard]] bool zero_tail(
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& tail) {
    return near(tail.minimum.value, 0.0) && near(tail.central, 0.0) &&
        near(tail.maximum.value, 0.0);
}

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, std::size_t month, double amount) {
    cf::ScenarioCashSource source;
    source.id = std::move(id);
    source.kind = kind;
    source.cash_available.push_back(cf::MonthlyAmount{month, amount});
    return source;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_terms() {
    cf::SuccessParticipationConfig terms;
    terms.scenario_label = "explicit-ledger stack participation fixture";
    terms.source_note =
        "synthetic unit-test assertion for already-declared commercial cash";
    terms.selected_nonprincipal_cash_is_contractually_scalable = true;
    terms.scalable_source_kinds = {cf::PortfolioCashSource::Commercial};
    return terms;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig point_ambiguity(
    std::string scenario_id) {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "explicit-ledger point probability";
    ambiguity.source_note = "synthetic exact-probability unit-test fixture";
    ambiguity.scenario_probabilities = {
        {std::move(scenario_id), 1.0, 1.0, 1.0}};
    return ambiguity;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig two_state_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "two-state reserve and cash-shortfall envelope";
    ambiguity.source_note = "synthetic event-compatible unit-test fixture";
    ambiguity.scenario_probabilities = {
        {"asset-a-used", 0.25, 0.50, 0.75},
        {"asset-b-used", 0.25, 0.50, 0.75},
    };
    return ambiguity;
}

[[nodiscard]] cf::PortfolioConfig explicit_claim_portfolio(
    std::string scenario_id, double purchase_price, double principal_limit,
    double principal_receipt, double writeoff,
    double buyer_direct_cost = 0.0, bool continuing = false) {
    cf::PortfolioConfig portfolio;
    portfolio.model_version = std::string(cf::kPortfolioModelVersion);
    portfolio.scenario_label = "explicit asset-liability bridge hand fixture";
    portfolio.source_note =
        "synthetic cash and contractual-principal ledgers for unit tests";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at acquisition";
    portfolio.horizon_months = 1U;
    portfolio.annual_physical_hurdle_rate = 0.0;

    cf::PortfolioProject project;
    project.id = "cellular-agriculture-claim";
    project.stage = cf::ProjectStage::FirstIndustrial;
    project.commitment_million = purchase_price + buyer_direct_cost;
    project.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = principal_limit;
    portfolio.projects = {project};

    cf::ProjectJointPath path;
    path.project_id = project.id;
    path.resolution = continuing
        ? cf::ProjectPathResolution::Continuing
        : cf::ProjectPathResolution::Resolved;
    path.investor_outlays.push_back(cf::InvestorOutlay{0U,
        cf::InvestorOutlayPurpose::ClaimPurchasePrice, purchase_price});
    if (buyer_direct_cost > 0.0) {
        path.investor_outlays.push_back(cf::InvestorOutlay{0U,
            cf::InvestorOutlayPurpose::BuyerDirectCost,
            buyer_direct_cost});
    }
    path.principal_movements.push_back(cf::PrincipalMovement{0U,
        cf::PrincipalMovementKind::FundedPrincipalAddition,
        principal_limit});
    if (principal_receipt > 0.0) {
        path.investor_receipts.push_back(cf::InvestorReceipt{1U,
            "contractual-principal-cash", principal_receipt,
            principal_receipt});
    }
    if (writeoff > 0.0) {
        path.principal_movements.push_back(cf::PrincipalMovement{1U,
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

[[nodiscard]] cf::ProjectJointPath unused_resolved_path(
    std::string project_id) {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = cf::ProjectPathResolution::Resolved;
    return path;
}

[[nodiscard]] cf::ProjectJointPath used_resolved_path(std::string project_id,
    std::string receipt_source_id, double purchase_price,
    double principal_added, double principal_received, double writeoff) {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.investor_outlays.push_back(cf::InvestorOutlay{0U,
        cf::InvestorOutlayPurpose::ClaimPurchasePrice, purchase_price});
    path.principal_movements.push_back(cf::PrincipalMovement{0U,
        cf::PrincipalMovementKind::FundedPrincipalAddition,
        principal_added});
    path.investor_receipts.push_back(cf::InvestorReceipt{1U,
        std::move(receipt_source_id), principal_received,
        principal_received});
    if (writeoff > 0.0) {
        path.principal_movements.push_back(cf::PrincipalMovement{1U,
            cf::PrincipalMovementKind::Writeoff, writeoff});
    }
    return path;
}

[[nodiscard]] cf::PortfolioConfig staggered_use_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.model_version = std::string(cf::kPortfolioModelVersion);
    portfolio.scenario_label =
        "staggered project reserve and simultaneous source fixture";
    portfolio.source_note =
        "synthetic two-project paths for v0.2 reserve and event tests";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at acquisition";
    portfolio.horizon_months = 1U;
    portfolio.annual_physical_hurdle_rate = 0.0;

    cf::PortfolioProject first;
    first.id = "asset-a";
    first.stage = cf::ProjectStage::FirstIndustrial;
    first.commitment_million = 10.0;
    first.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    first.principal_limit_million = 15.0;

    cf::PortfolioProject second = first;
    second.id = "asset-b";
    second.principal_limit_million = 10.0;
    portfolio.projects = {first, second};

    cf::JointScenario first_used;
    first_used.id = "asset-a-used";
    first_used.weight = 0.5;
    first_used.cash_sources = {make_source("asset-a-principal",
        cf::PortfolioCashSource::Commercial, 1U, 15.0)};
    first_used.project_paths = {
        used_resolved_path(
            "asset-a", "asset-a-principal", 10.0, 15.0, 15.0, 0.0),
        unused_resolved_path("asset-b"),
    };

    cf::JointScenario second_used;
    second_used.id = "asset-b-used";
    second_used.weight = 0.5;
    second_used.cash_sources = {make_source("asset-b-principal",
        cf::PortfolioCashSource::Commercial, 1U, 5.0)};
    second_used.project_paths = {
        unused_resolved_path("asset-a"),
        used_resolved_path(
            "asset-b", "asset-b-principal", 10.0, 10.0, 5.0, 5.0),
    };
    portfolio.joint_scenarios = {
        std::move(first_used), std::move(second_used)};
    return portfolio;
}

[[nodiscard]] cf::PortfolioConfig distinct_observables_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.model_version = std::string(cf::kPortfolioModelVersion);
    portfolio.scenario_label = "distinct L O Q projection fixture";
    portfolio.source_note =
        "synthetic resolved and continuing paths for selector isolation";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at acquisition";
    portfolio.horizon_months = 1U;
    portfolio.annual_physical_hurdle_rate = 0.0;

    cf::PortfolioProject project;
    project.id = "claim";
    project.stage = cf::ProjectStage::FirstIndustrial;
    project.commitment_million = 8.0;
    project.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = 10.0;
    portfolio.projects = {project};

    cf::JointScenario resolved;
    resolved.id = "resolved-loss";
    resolved.weight = 0.5;
    resolved.cash_sources = {make_source("resolved-principal",
        cf::PortfolioCashSource::Commercial, 1U, 8.0)};
    resolved.project_paths = {used_resolved_path(
        "claim", "resolved-principal", 8.0, 10.0, 8.0, 2.0)};

    cf::ProjectJointPath continuing_path;
    continuing_path.project_id = "claim";
    continuing_path.resolution = cf::ProjectPathResolution::Continuing;
    continuing_path.investor_outlays.push_back(cf::InvestorOutlay{0U,
        cf::InvestorOutlayPurpose::ClaimPurchasePrice, 8.0});
    continuing_path.principal_movements.push_back(cf::PrincipalMovement{0U,
        cf::PrincipalMovementKind::FundedPrincipalAddition, 10.0});
    continuing_path.investor_receipts.push_back(cf::InvestorReceipt{1U,
        "continuing-principal", 3.0, 3.0});

    cf::JointScenario continuing;
    continuing.id = "continuing";
    continuing.weight = 0.5;
    continuing.cash_sources = {make_source("continuing-principal",
        cf::PortfolioCashSource::Commercial, 1U, 3.0)};
    continuing.project_paths = {std::move(continuing_path)};

    portfolio.joint_scenarios = {
        std::move(resolved), std::move(continuing)};
    return portfolio;
}

[[nodiscard]] cf::CapitalStackConfig v02_stack_terms(
    double issued_principal) {
    cf::CapitalStackConfig terms;
    terms.model_version = std::string(cf::kCapitalStackModelVersion);
    terms.scenario_label = "explicit asset-liability bridge stack terms";
    terms.source_note =
        "synthetic first-loss and priority terms for v0.2 unit tests";

    terms.aggregate_commitment_is_fully_funded_at_par_at_month_zero = false;
    terms.subscription_reserve_is_zero_yield_and_lossless = true;
    terms.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    terms.pool_costs_are_additional_pro_rata_calls = true;
    terms.principal_cash_is_paid_most_senior_first = true;
    terms.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    terms.tranching_does_not_change_project_cash_or_gross_loss = true;
    terms.premium_discount_or_fair_value_is_claimed = false;

    terms.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero =
        true;
    terms.buyer_direct_costs_are_additional_pro_rata_calls = true;
    terms.principal_base_cash_above_issued_principal_is_nonprincipal = true;
    terms.principal_limit_capacity_difference_is_reported_without_valuation_claim =
        true;
    terms.underlying_success_participation_fraction = 1.0;

    const double first_loss_detachment =
        std::min(4.0, issued_principal / 2.0);
    terms.tranches = {
        {"first-loss-residual", 0.0, first_loss_detachment, 0.0, 0.0,
            true},
        {"priority", first_loss_detachment, issued_principal, 0.0, 0.0,
            false},
    };
    return terms;
}

[[nodiscard]] cf::CapitalStackConfig v02_minimum_junior_terms(
    double issued_principal) {
    cf::CapitalStackConfig terms = v02_stack_terms(issued_principal);
    constexpr double minimum_junior_notional_million = 1.0e-6;
    terms.tranches = {
        {"minimum-junior", 0.0, minimum_junior_notional_million, 0.0,
            0.0, true},
        {"priority", minimum_junior_notional_million, issued_principal,
            0.0, 0.0, false},
    };
    return terms;
}

[[nodiscard]] cf::CapitalStackConfig v01_stack_terms(
    double aggregate_commitment) {
    cf::CapitalStackConfig terms;
    terms.model_version = std::string(cf::kCapitalStackLegacyModelVersion);
    terms.scenario_label = "legacy capital-stack explicit-ledger boundary";
    terms.source_note = "synthetic fail-closed boundary unit test";
    terms.aggregate_commitment_is_fully_funded_at_par_at_month_zero = true;
    terms.subscription_reserve_is_zero_yield_and_lossless = true;
    terms.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    terms.pool_costs_are_additional_pro_rata_calls = true;
    terms.principal_cash_is_paid_most_senior_first = true;
    terms.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    terms.tranching_does_not_change_project_cash_or_gross_loss = true;
    terms.underlying_success_participation_fraction = 1.0;
    terms.tranches = {
        {"first-loss-residual", 0.0, aggregate_commitment / 2.0, 0.0,
            0.0, true},
        {"priority", aggregate_commitment / 2.0, aggregate_commitment, 0.0,
            0.0, false},
    };
    return terms;
}

[[nodiscard]] const cf::CapitalStackTrancheScenarioResult& find_tranche(
    const cf::CapitalStackScenarioResult& scenario, std::string_view id) {
    const auto found = std::find_if(scenario.tranches.begin(),
        scenario.tranches.end(), [id](const auto& tranche) {
            return tranche.tranche_id == id;
        });
    if (found == scenario.tranches.end()) {
        throw std::logic_error("v0.2 test tranche not found");
    }
    return *found;
}

[[nodiscard]] const cf::CapitalStackScenarioResult& find_scenario(
    const cf::CapitalStackSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const auto& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error("v0.2 test scenario not found");
    }
    return *found;
}

void check_all_path_reconciliations(
    const cf::CapitalStackSummary& summary, std::string_view message) {
    check(summary.maximum_commitment_identity_error_million < 1.0e-9 &&
            summary.maximum_reserve_roll_forward_error_million < 1.0e-9 &&
            summary.maximum_reserve_shortfall_million < 1.0e-9 &&
            summary.maximum_subscription_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_pool_cost_call_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_buyer_direct_cost_call_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_principal_distribution_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_contractual_principal_surplus_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_unused_reserve_surplus_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_nonprincipal_distribution_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_priority_nonprincipal_cap_violation_million <
                1.0e-9 &&
            summary.maximum_realized_loss_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_contractual_asset_loss_preservation_error_million <
                1.0e-9 &&
            summary.maximum_contractual_asset_outstanding_preservation_error_million <
                1.0e-9 &&
            summary.maximum_unresolved_exposure_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_nominal_net_cash_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_stack_npv_reconciliation_error_million < 1.0e-9 &&
            summary.maximum_endpoint_probability_error < 1.0e-9,
        message);
    check(!summary.legacy_v01_loss_layering_metrics_are_applicable &&
            std::all_of(summary.scenarios.begin(), summary.scenarios.end(),
                [](const auto& scenario) {
                    return std::all_of(scenario.tranches.begin(),
                        scenario.tranches.end(), [](const auto& tranche) {
                            return !tranche
                                        .legacy_v01_loss_layering_metrics_are_applicable &&
                                near(tranche.realized_principal_loss_million,
                                    0.0) &&
                                near(tranche
                                         .unresolved_principal_exposure_million,
                                    0.0);
                        });
                }) &&
            std::all_of(summary.tranches.begin(), summary.tranches.end(),
                [](const auto& tranche) {
                    return !tranche
                                .legacy_v01_loss_layering_metrics_are_applicable &&
                        zero_range(
                            tranche.expected_realized_principal_loss_million) &&
                        zero_range(tranche
                                .expected_realized_principal_loss_fraction) &&
                        zero_range(tranche
                                .expected_unresolved_principal_exposure_million) &&
                        zero_range(tranche.principal_impairment_probability) &&
                        zero_range(tranche.principal_exhaustion_probability) &&
                        zero_range(tranche
                                .principal_loss_expected_shortfall_95_million) &&
                        zero_range(tranche
                                .principal_loss_expected_shortfall_99_million);
                }),
        "v0.2 marks every legacy tranche loss-layering output inapplicable and leaves its compatibility scalar zero");
}

void test_resolved_claim_separates_asset_loss_from_liability_cash_shortfall() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "resolved-8-for-10", 8.0, 10.0, 8.0, 2.0);
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("resolved-8-for-10"),
        participation_terms(), v02_stack_terms(8.0));
    const auto& scenario = summary.scenarios.front();

    check(summary.model_version == cf::kCapitalStackModelVersion &&
            summary.uses_explicit_asset_liability_accounting &&
            scenario.uses_explicit_asset_liability_accounting,
        "v0.2 explicitly identifies the asset-to-liability accounting mode");
    check(near(scenario.aggregate_contractual_asset_principal_limit_million,
              10.0) &&
            near(scenario.aggregate_commitment_million, 8.0) &&
            near(scenario.total_claim_purchase_price_million, 8.0) &&
            near(scenario
                     .contractual_principal_limit_minus_funding_uses_million,
                2.0),
        "claim keeps its principal limit, purchase cash, and issued principal separate");
    check(near(scenario.contractual_asset_principal_loss_million, 2.0) &&
            near(scenario.contractual_asset_outstanding_principal_million,
                0.0) &&
            near(scenario.issued_principal_cash_shortfall_million, 0.0) &&
            near(scenario.fully_funded_stack_npv_at_pool_hurdle_million,
                0.0) &&
            near(summary.expected_contractual_asset_principal_loss_million
                     .central,
                2.0) &&
            near(summary
                     .expected_contractual_asset_outstanding_principal_million
                     .central,
                0.0) &&
            near(summary.expected_issued_principal_cash_shortfall_million
                     .central,
                0.0),
        "a two-unit contractual writeoff remains separate from an exactly repaid eight-unit liability");
    check(near(scenario.stack_nominal_net_cash_million, 0.0) &&
            near(scenario.distributable_principal_cash_million, 8.0) &&
            near(find_tranche(scenario, "first-loss-residual")
                     .principal_cash_shortfall_million,
                0.0) &&
            near(find_tranche(scenario, "priority")
                     .principal_cash_shortfall_million,
                0.0),
        "resolved-loss hand table conserves the actual eight-unit cash round trip");
    check_all_path_reconciliations(summary,
        "resolved 8-for-10 claim passes every v0.2 accounting control");
}

void test_performing_claim_can_leave_an_issued_principal_cash_shortfall() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "performing-12-for-10", 12.0, 10.0, 10.0, 0.0);
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("performing-12-for-10"),
        participation_terms(), v02_stack_terms(12.0));
    const auto& scenario = summary.scenarios.front();
    const auto& junior = find_tranche(scenario, "first-loss-residual");
    const auto& priority = find_tranche(scenario, "priority");

    check(near(scenario.contractual_asset_principal_loss_million, 0.0) &&
            near(scenario
                     .contractual_principal_limit_minus_funding_uses_million,
                -2.0) &&
            near(scenario.contractual_asset_outstanding_principal_million,
                0.0) &&
            near(scenario.issued_principal_cash_shortfall_million, 2.0) &&
            near(scenario.fully_funded_stack_npv_at_pool_hurdle_million,
                -2.0),
        "performing ten-unit asset remains lossless while twelve units of issued principal have a two-unit horizon cash shortfall");
    check(near(junior.notional_million, 4.0) &&
            near(junior.principal_cash_distribution_million, 2.0) &&
            near(junior.principal_cash_shortfall_million, 2.0) &&
            near(junior.realized_principal_loss_million, 0.0) &&
            near(priority.notional_million, 8.0) &&
            near(priority.principal_cash_distribution_million, 8.0) &&
            near(priority.principal_cash_shortfall_million, 0.0),
        "senior-first cash and first-loss shortfall layering reproduce the hand table without an impairment claim");
    check(near(junior.principal_cash_distribution_million +
                  priority.principal_cash_distribution_million,
              scenario.underlying_principal_cash_million) &&
            near(scenario.stack_nominal_net_cash_million, -2.0),
        "12-for-10 hand table allocates only the ten units of actual cash");
    check_all_path_reconciliations(summary,
        "performing 12-for-10 claim passes every v0.2 accounting control");
}

void test_fully_performing_nonpar_claim_routes_asset_principal_surplus() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "performing-8-for-10", 8.0, 10.0, 10.0, 0.0);
    cf::CapitalStackConfig terms = v02_stack_terms(8.0);
    terms.tranches.back().priority_nonprincipal_cap_million = 1.0;
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("performing-8-for-10"),
        participation_terms(), terms);
    const auto& scenario = summary.scenarios.front();
    const auto& junior = find_tranche(scenario, "first-loss-residual");
    const auto& priority = find_tranche(scenario, "priority");

    check(near(scenario.underlying_principal_cash_million, 10.0) &&
            near(scenario.distributable_principal_cash_million, 8.0) &&
            near(scenario.contractual_principal_surplus_cash_million, 2.0) &&
            near(scenario.underlying_nonprincipal_cash_million, 0.0) &&
            near(scenario.distributable_nonprincipal_cash_million, 2.0),
        "contractual principal above issued notional is disclosed as surplus cash rather than manufactured liability principal");
    check(near(junior.principal_cash_distribution_million +
                  priority.principal_cash_distribution_million,
              8.0) &&
            near(junior.contractual_principal_surplus_cash_distribution_million +
                  priority.contractual_principal_surplus_cash_distribution_million,
              2.0) &&
            near(junior.nonprincipal_cash_distribution_million +
                  priority.nonprincipal_cash_distribution_million,
              2.0) &&
            near(priority.nonprincipal_cash_distribution_million, 1.0) &&
            near(junior.nonprincipal_cash_distribution_million, 1.0) &&
            near(junior.total_distributions_million +
                  priority.total_distributions_million,
              10.0) &&
            near(scenario.stack_nominal_net_cash_million, 2.0),
        "non-par surplus enters the declared one-unit priority cap then residual waterfall and conserves all ten units");
    check_all_path_reconciliations(summary,
        "fully performing 8-for-10 claim passes every v0.2 accounting control");
}

void test_per_project_reserve_maxima_and_simultaneous_source_memo() {
    const cf::PortfolioConfig portfolio = staggered_use_portfolio();
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, two_state_ambiguity(), participation_terms(),
        v02_stack_terms(20.0));
    const auto& surplus_state = find_scenario(summary, "asset-a-used");
    const auto& shortfall_state = find_scenario(summary, "asset-b-used");
    const auto& surplus_junior =
        find_tranche(surplus_state, "first-loss-residual");
    const auto& surplus_priority = find_tranche(surplus_state, "priority");

    check(near(summary.aggregate_commitment_million, 20.0) &&
            near(surplus_state.total_asset_acquisition_and_primary_funding_uses_million,
                10.0) &&
            near(shortfall_state.total_asset_acquisition_and_primary_funding_uses_million,
                10.0) &&
            near(surplus_state.unused_commitment_returned_at_horizon_million,
                10.0) &&
            near(shortfall_state.unused_commitment_returned_at_horizon_million,
                10.0),
        "reserve principal is the sum of project-level maximum uses, not the maximum aggregate state use");

    double underlying_principal_distribution = 0.0;
    double reserve_principal_distribution = 0.0;
    double contractual_surplus_distribution = 0.0;
    double reserve_surplus_distribution = 0.0;
    for (const auto& tranche : surplus_state.tranches) {
        underlying_principal_distribution +=
            tranche.underlying_principal_cash_distribution_million;
        reserve_principal_distribution +=
            tranche.unused_reserve_principal_return_million;
        contractual_surplus_distribution +=
            tranche.contractual_principal_surplus_cash_distribution_million;
        reserve_surplus_distribution +=
            tranche.unused_reserve_surplus_cash_distribution_million;
    }
    check(near(surplus_state.underlying_principal_cash_million, 15.0) &&
            near(surplus_state.distributable_principal_cash_million, 20.0) &&
            near(surplus_state.contractual_principal_surplus_cash_million,
                3.0) &&
            near(surplus_state.unused_reserve_surplus_cash_million, 2.0) &&
            near(underlying_principal_distribution, 12.0) &&
            near(reserve_principal_distribution, 8.0) &&
            near(contractual_surplus_distribution, 3.0) &&
            near(reserve_surplus_distribution, 2.0),
        "simultaneous asset-principal and reserve cash use the disclosed equal-seniority pro-rata memo convention without changing total cash");
    check(near(surplus_junior
                   .underlying_principal_cash_distribution_million,
              2.4) &&
            near(surplus_junior.unused_reserve_principal_return_million,
                1.6) &&
            near(surplus_priority
                   .underlying_principal_cash_distribution_million,
                9.6) &&
            near(surplus_priority.unused_reserve_principal_return_million,
                6.4) &&
            near(surplus_junior
                   .contractual_principal_surplus_cash_distribution_million,
                3.0) &&
            near(surplus_junior
                   .unused_reserve_surplus_cash_distribution_million,
                2.0),
        "each paid tranche and the residual surplus retain the same sixty-forty source memo ratio");
    check(near(shortfall_state.contractual_asset_principal_loss_million,
              5.0) &&
            near(shortfall_state
                     .contractual_asset_outstanding_principal_million,
                0.0) &&
            near(shortfall_state.issued_principal_cash_shortfall_million,
                5.0) &&
            near(find_tranche(shortfall_state, "first-loss-residual")
                     .principal_cash_shortfall_million,
                4.0) &&
            near(find_tranche(shortfall_state, "priority")
                     .principal_cash_shortfall_million,
                1.0),
        "the adverse state reports exact asset loss and separately layers the five-unit liability cash shortfall");
    check_all_path_reconciliations(summary,
        "staggered-use fixture passes every v0.2 accounting control");
}

void test_one_aggregate_money_tolerance_controls_all_shortfall_layers() {
    constexpr double issued_principal = 1'000'000.0;
    const double principal_received = issued_principal - 4.0e-10;
    const double contractual_writeoff =
        issued_principal - principal_received;
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "rounding-boundary", issued_principal, issued_principal,
        principal_received, contractual_writeoff);
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("rounding-boundary"),
        participation_terms(), v02_minimum_junior_terms(issued_principal));
    const auto& scenario = summary.scenarios.front();

    check(contractual_writeoff > 0.0 &&
            near(scenario.issued_principal_cash_shortfall_million, 0.0,
                0.0) &&
            near(find_tranche(scenario, "minimum-junior")
                     .principal_cash_shortfall_million,
                0.0, 0.0) &&
            near(find_tranche(scenario, "priority")
                     .principal_cash_shortfall_million,
                0.0, 0.0),
        "a sub-tolerance aggregate residual is canonicalized once and every tranche layer derives from that same zero Q");
    check(summary.tranches.front()
                  .principal_cash_shortfall_probability.central == 0.0 &&
            summary.tranches.back()
                    .principal_cash_shortfall_probability.central == 0.0,
        "binary shortfall events use the same canonical aggregate monetary boundary at large scale and minimum tranche size");
    check_all_path_reconciliations(summary,
        "rounding-boundary fixture remains inside every declared numerical control");
}

void test_legacy_stack_rejects_explicit_principal_ledger_at_boundary() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "legacy-explicit-boundary", 12.0, 10.0, 10.0, 0.0);
    bool rejected_at_named_boundary = false;
    try {
        cf::validate_capital_stack_config(portfolio,
            point_ambiguity("legacy-explicit-boundary"),
            participation_terms(), v01_stack_terms(12.0));
    } catch (const std::invalid_argument& error) {
        rejected_at_named_boundary = std::string_view(error.what()).find(
            "capital-stack v0.1 cannot consume explicit contractual principal ledgers") !=
            std::string_view::npos;
    } catch (...) {
        // The named v0.1 accounting boundary must be the rejection reason.
    }
    check(rejected_at_named_boundary,
        "v0.1 still fails closed at its explicit-contractual-ledger boundary");
}

void test_buyer_direct_cost_is_an_additional_call() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "buyer-cost-performing", 8.0, 8.0, 8.0, 0.0, 0.5);
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("buyer-cost-performing"),
        participation_terms(), v02_stack_terms(8.0));
    const auto& scenario = summary.scenarios.front();
    const auto& junior = find_tranche(scenario, "first-loss-residual");
    const auto& priority = find_tranche(scenario, "priority");

    check(near(scenario.aggregate_project_outlay_limit_million, 8.5) &&
            near(scenario.aggregate_commitment_million, 8.0) &&
            near(scenario.total_asset_acquisition_and_primary_funding_uses_million,
                8.0) &&
            near(scenario.total_buyer_direct_costs_million, 0.5) &&
            near(scenario
                     .contractual_principal_limit_minus_funding_uses_million,
                0.0),
        "buyer direct cost is excluded from acquisition reserve and issued principal");
    check(near(junior.pro_rata_buyer_direct_cost_calls_million, 0.25) &&
            near(priority.pro_rata_buyer_direct_cost_calls_million, 0.25) &&
            near(junior.pro_rata_buyer_direct_cost_calls_million +
                  priority.pro_rata_buyer_direct_cost_calls_million,
              0.5) &&
            near(junior.total_contributions_million +
                  priority.total_contributions_million,
              8.5) &&
            near(scenario.stack_nominal_net_cash_million, -0.5),
        "buyer direct cost is called pro rata without becoming asset or tranche principal");
    check_all_path_reconciliations(summary,
        "buyer direct cost fixture passes every v0.2 accounting control");
}

void test_continuing_asset_outstanding_is_not_attributed_to_liability_layers() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "continuing-claim", 8.0, 10.0, 3.0, 0.0, 0.0, true);
    const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("continuing-claim"),
        participation_terms(), v02_stack_terms(8.0));
    const auto& scenario = summary.scenarios.front();
    const auto& junior = find_tranche(scenario, "first-loss-residual");
    const auto& priority = find_tranche(scenario, "priority");

    check(near(scenario.contractual_asset_principal_loss_million, 0.0) &&
            near(scenario.contractual_asset_outstanding_principal_million,
                7.0) &&
            near(scenario.issued_principal_cash_shortfall_million, 5.0),
        "continuing contractual principal and the horizon liability cash shortfall remain separate exact observables");
    check(near(priority.principal_cash_distribution_million, 3.0) &&
            near(priority.principal_cash_shortfall_million, 1.0) &&
            near(junior.principal_cash_distribution_million, 0.0) &&
            near(junior.principal_cash_shortfall_million, 4.0) &&
            near(junior.realized_principal_loss_million, 0.0) &&
            near(priority.realized_principal_loss_million, 0.0),
        "senior-first cash layers the five-unit horizon shortfall without assigning asset causality or impairment");
    check(near(scenario.stack_nominal_net_cash_million, -5.0) &&
            near(scenario.fully_funded_stack_npv_at_pool_hurdle_million,
                -5.0) &&
            near(summary.expected_contractual_asset_principal_loss_million
                     .central,
                0.0) &&
            near(summary
                     .expected_contractual_asset_outstanding_principal_million
                     .central,
                7.0) &&
            near(summary.expected_issued_principal_cash_shortfall_million
                     .central,
                5.0),
        "asset outstanding O and issued-principal cash shortfall Q are reported without an assumed recovery value");
    check_all_path_reconciliations(summary,
        "continuing claim passes every v0.2 accounting control");
}

void test_singleton_event_polytope_preserves_v02_deterministic_path() {
    const cf::PortfolioConfig portfolio = explicit_claim_portfolio(
        "singleton-12-for-10", 12.0, 10.0, 10.0, 0.0);
    const cf::CapitalStackConfig stack = v02_stack_terms(12.0);
    const cf::SuccessParticipationConfig participation = participation_terms();
    const cf::CapitalStackSummary deterministic = cf::evaluate_capital_stack(
        portfolio, point_ambiguity("singleton-12-for-10"), participation,
        stack);

    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "singleton explicit-ledger event polytope";
    polytope.source_note =
        "synthetic exact-probability event projection equivalence fixture";
    polytope.scenario_probabilities = {
        {"singleton-12-for-10", 1.0, 1.0, 1.0}};
    // Events must be proper subsets of the scenario taxonomy, so a one-state
    // taxonomy has no non-trivial event row. This still exercises the event-
    // polytope bridge at its exact singleton boundary.

    const cf::CapitalStackProbabilityPolytopeSummary projected =
        cf::evaluate_capital_stack_probability_polytope(
            portfolio, polytope, participation, stack);
    const auto& expected = deterministic.scenarios.front();
    const auto& actual = projected.scenarios.front();
    check(projected.scenarios.size() == 1U && projected.events.empty() &&
            near(actual.aggregate_commitment_million,
                expected.aggregate_commitment_million) &&
            near(actual.contractual_asset_principal_loss_million,
                expected.contractual_asset_principal_loss_million) &&
            near(actual.contractual_asset_outstanding_principal_million,
                expected.contractual_asset_outstanding_principal_million) &&
            near(actual.issued_principal_cash_shortfall_million,
                expected.issued_principal_cash_shortfall_million) &&
            near(actual.fully_funded_stack_npv_at_pool_hurdle_million,
                expected.fully_funded_stack_npv_at_pool_hurdle_million) &&
            near(actual.stack_nominal_net_cash_million,
                expected.stack_nominal_net_cash_million),
        "singleton event projection consumes the exact deterministic v0.2 cash path");
    check(near(projected.tranches.front()
                   .expected_principal_cash_shortfall_million.central,
              deterministic.tranches.front()
                  .expected_principal_cash_shortfall_million.central) &&
            near(projected.expected_issued_principal_cash_shortfall_million
                     .central,
                deterministic.expected_issued_principal_cash_shortfall_million
                    .central) &&
            projected.maximum_probability_constraint_violation < 1.0e-9 &&
            projected.maximum_objective_reconciliation_error < 1.0e-9,
        "singleton event projection reproduces deterministic L/O/Q observables with an audited probability witness");
}

void test_nontrivial_event_polytope_projects_v02_cash_shortfall() {
    const cf::PortfolioConfig portfolio = staggered_use_portfolio();
    const cf::CapitalStackConfig stack = v02_stack_terms(20.0);
    const cf::SuccessParticipationConfig participation = participation_terms();

    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "two-state shortfall event polytope";
    polytope.source_note =
        "synthetic bounded shortfall-event probability for unit tests";
    polytope.scenario_probabilities = {
        {"asset-a-used", 0.0, 0.5, 1.0},
        {"asset-b-used", 0.0, 0.5, 1.0},
    };
    polytope.events = {{"issued-principal-shortfall-state",
        "The state has a positive horizon issued-principal cash shortfall",
        0.25, 0.75, {"asset-b-used"}}};

    const cf::CapitalStackProbabilityPolytopeSummary projected =
        cf::evaluate_capital_stack_probability_polytope(
            portfolio, polytope, participation, stack);
    const auto& junior = projected.tranches.front();
    const auto& priority = projected.tranches.back();

    check(projected.events.size() == 1U &&
            near(projected
                     .expected_contractual_asset_principal_loss_million
                     .minimum.value,
                1.25) &&
            near(projected
                     .expected_contractual_asset_principal_loss_million
                     .central,
                2.5) &&
            near(projected
                     .expected_contractual_asset_principal_loss_million
                     .maximum.value,
                3.75) &&
            near(projected.expected_issued_principal_cash_shortfall_million
                     .minimum.value,
                1.25) &&
            near(projected.expected_issued_principal_cash_shortfall_million
                     .central,
                2.5) &&
            near(projected.expected_issued_principal_cash_shortfall_million
                     .maximum.value,
                3.75),
        "nontrivial event bounds project the separate asset-loss and liability-shortfall observables over complete witnesses");
    check(near(junior.principal_cash_shortfall_probability.minimum.value,
              0.25) &&
            near(junior.principal_cash_shortfall_probability.central, 0.5) &&
            near(junior.principal_cash_shortfall_probability.maximum.value,
                0.75) &&
            near(junior.full_principal_cash_shortfall_probability.minimum.value,
                0.25) &&
            near(junior
                     .principal_cash_shortfall_expected_shortfall_95_million
                     .maximum.value,
                4.0) &&
            near(priority.principal_cash_shortfall_probability.minimum.value,
                0.25) &&
            near(priority
                     .principal_cash_shortfall_expected_shortfall_95_million
                     .maximum.value,
                1.0),
        "v0.2 event risk uses tranche cash-shortfall incidence, exhaustion, and tail severity rather than legacy impairment fields");
    check(projected.maximum_probability_constraint_violation < 1.0e-9 &&
            projected.maximum_objective_reconciliation_error < 1.0e-9 &&
            projected.maximum_tail_mass_violation < 1.0e-9,
        "nontrivial event shortfall projections retain audited feasible witnesses");
}

void test_event_polytope_keeps_distinct_asset_and_liability_selectors() {
    const cf::PortfolioConfig portfolio = distinct_observables_portfolio();
    const cf::CapitalStackConfig stack = v02_stack_terms(8.0);
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "distinct L O Q event polytope";
    polytope.source_note =
        "synthetic selector-isolation probability bounds";
    polytope.scenario_probabilities = {
        {"continuing", 0.0, 0.5, 1.0},
        {"resolved-loss", 0.0, 0.5, 1.0},
    };
    polytope.events = {{"continuing-state",
        "The contractual asset remains continuing at the horizon", 0.25,
        0.75, {"continuing"}}};

    const cf::CapitalStackProbabilityPolytopeSummary projected =
        cf::evaluate_capital_stack_probability_polytope(portfolio, polytope,
            participation_terms(), stack);
    const auto& loss =
        projected.expected_contractual_asset_principal_loss_million;
    const auto& outstanding =
        projected.expected_contractual_asset_outstanding_principal_million;
    const auto& shortfall =
        projected.expected_issued_principal_cash_shortfall_million;

    check(near(loss.minimum.value, 0.5) && near(loss.central, 1.0) &&
            near(loss.maximum.value, 1.5) &&
            near(outstanding.minimum.value, 1.75) &&
            near(outstanding.central, 3.5) &&
            near(outstanding.maximum.value, 5.25) &&
            near(shortfall.minimum.value, 1.25) &&
            near(shortfall.central, 2.5) &&
            near(shortfall.maximum.value, 3.75),
        "event projection keeps the distinct state vectors L=(2,0), O=(0,7), and Q=(0,5)");
    check(loss.minimum.scenario_weights.size() == 2U &&
            near(loss.minimum.scenario_weights[0U], 0.75) &&
            near(loss.minimum.scenario_weights[1U], 0.25) &&
            outstanding.maximum.scenario_weights.size() == 2U &&
            near(outstanding.maximum.scenario_weights[0U], 0.75) &&
            near(outstanding.maximum.scenario_weights[1U], 0.25) &&
            shortfall.minimum.scenario_weights.size() == 2U &&
            near(shortfall.minimum.scenario_weights[0U], 0.25) &&
            near(shortfall.minimum.scenario_weights[1U], 0.75),
        "L O and Q endpoints retain their own complete feasible probability witnesses");
    check(!projected.legacy_v01_loss_layering_metrics_are_applicable &&
            std::all_of(projected.tranches.begin(), projected.tranches.end(),
                [](const auto& tranche) {
                    return !tranche
                                .legacy_v01_loss_layering_metrics_are_applicable &&
                        zero_range(
                            tranche.expected_realized_principal_loss_million) &&
                        zero_range(tranche
                                .expected_realized_principal_loss_fraction) &&
                        zero_range(tranche
                                .expected_unresolved_principal_exposure_million) &&
                        zero_range(tranche.principal_impairment_probability) &&
                        zero_range(tranche.principal_exhaustion_probability) &&
                        zero_tail(tranche
                                .principal_loss_expected_shortfall_95_million) &&
                        zero_tail(tranche
                                .principal_loss_expected_shortfall_99_million);
                }),
        "event-polytope v0.2 marks every legacy tranche loss metric inapplicable and leaves its compatibility output empty-zero");
}

} // namespace

int main() {
    test_resolved_claim_separates_asset_loss_from_liability_cash_shortfall();
    test_performing_claim_can_leave_an_issued_principal_cash_shortfall();
    test_fully_performing_nonpar_claim_routes_asset_principal_surplus();
    test_per_project_reserve_maxima_and_simultaneous_source_memo();
    test_one_aggregate_money_tolerance_controls_all_shortfall_layers();
    test_legacy_stack_rejects_explicit_principal_ledger_at_boundary();
    test_buyer_direct_cost_is_an_additional_call();
    test_continuing_asset_outstanding_is_not_attributed_to_liability_layers();
    test_singleton_event_polytope_preserves_v02_deterministic_path();
    test_nontrivial_event_polytope_projects_v02_cash_shortfall();
    test_event_polytope_keeps_distinct_asset_and_liability_selectors();
    if (failures != 0) {
        std::cerr << failures << " capital-stack v0.2 test(s) failed\n";
        return 1;
    }
    std::cout << "capital-stack v0.2 tests passed\n";
    return 0;
}
