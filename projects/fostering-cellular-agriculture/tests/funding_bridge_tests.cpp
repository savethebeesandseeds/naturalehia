// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/funding_bridge.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cf = naturalehia::cellular_finance;

namespace {

constexpr double kTolerance = 1.0e-9;
int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(
    double left, double right, double tolerance = kTolerance) {
    return std::abs(left - right) <=
        tolerance * std::max({1.0, std::abs(left), std::abs(right)});
}

void expect_invalid(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        check(true, message);
    }
}

[[nodiscard]] cf::FundingProvider provider(
    std::string id, std::string group, double capacity) {
    cf::FundingProvider result;
    result.id = std::move(id);
    result.economic_group_id = std::move(group);
    result.declared_capacity_million = capacity;
    result.identity_evidenced = true;
    result.affiliation_evidenced = true;
    result.capacity_evidenced = true;
    result.source_record_id = "synthetic-provider-record";
    return result;
}

[[nodiscard]] cf::FundingSettlementOutcome settled_outcome(
    std::string request_id, std::size_t month, double amount) {
    cf::FundingSettlementOutcome result;
    result.request_id = std::move(request_id);
    result.settlement_month = month;
    result.status = cf::FundingSettlementStatus::SettledInFull;
    result.actual_cash_million = amount;
    result.source_record_id = "synthetic-settlement-record";
    return result;
}

[[nodiscard]] cf::FundingSettlementOutcome failed_outcome(
    std::string request_id, std::size_t month) {
    cf::FundingSettlementOutcome result;
    result.request_id = std::move(request_id);
    result.settlement_month = month;
    result.status = cf::FundingSettlementStatus::Failed;
    result.actual_cash_million = 0.0;
    result.source_record_id = "synthetic-failed-settlement-record";
    return result;
}

[[nodiscard]] cf::EligibleBasisMovement basis_movement(std::string id,
    std::size_t month, cf::EligibleBasisMovementKind kind, double amount,
    std::string reference) {
    cf::EligibleBasisMovement result;
    result.id = std::move(id);
    result.month = month;
    result.kind = kind;
    result.amount_million = amount;
    result.reference_id = std::move(reference);
    result.source_record_id = "synthetic-basis-record";
    return result;
}

[[nodiscard]] cf::ProtectionAbsorption protection_absorption(
    std::string id, std::size_t month, double amount,
    std::string reference) {
    cf::ProtectionAbsorption result;
    result.id = std::move(id);
    result.month = month;
    result.amount_million = amount;
    result.reference_id = std::move(reference);
    result.source_record_id = "synthetic-protection-absorption-record";
    return result;
}

[[nodiscard]] cf::SupplementalFundingReceipt supplemental_receipt(
    std::string id, std::string provider_id, std::size_t month,
    cf::SupplementalFundingPurpose purpose, double amount) {
    cf::SupplementalFundingReceipt result;
    result.id = std::move(id);
    result.provider_id = std::move(provider_id);
    result.month = month;
    result.purpose = purpose;
    result.actual_cash_million = amount;
    result.source_record_id = "synthetic-supplement-record";
    return result;
}

[[nodiscard]] cf::ScenarioCashSource cash_source(
    std::string id, std::size_t month, double amount) {
    cf::ScenarioCashSource result;
    result.id = std::move(id);
    result.kind = cf::PortfolioCashSource::Commercial;
    result.cash_available = {{month, amount}};
    return result;
}

[[nodiscard]] cf::PortfolioProject project(
    double outlay_limit, double principal_limit) {
    cf::PortfolioProject result;
    result.id = "cellular-plant";
    result.stage = cf::ProjectStage::FirstIndustrial;
    result.commitment_million = outlay_limit;
    result.principal_accounting_mode =
        cf::PrincipalAccountingMode::ExplicitContractualLedger;
    result.principal_limit_million = principal_limit;
    return result;
}

[[nodiscard]] cf::PortfolioConfig two_draw_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.model_version = std::string(cf::kPortfolioModelVersion);
    portfolio.scenario_label = "two-draw funding-bridge fixture";
    portfolio.source_note =
        "synthetic exact cash, cost, and principal paths for bridge tests";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant synthetic test units";
    portfolio.horizon_months = 4U;
    portfolio.annual_physical_hurdle_rate = 0.12;
    portfolio.projects = {project(10.3, 10.0)};

    cf::ProjectJointPath path;
    path.project_id = "cellular-plant";
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.investor_outlays = {
        {1U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 6.0},
        {1U, cf::InvestorOutlayPurpose::BuyerDirectCost, 0.3},
        {2U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 4.0},
    };
    path.principal_movements = {
        {1U, cf::PrincipalMovementKind::FundedPrincipalAddition, 6.0},
        {2U, cf::PrincipalMovementKind::FundedPrincipalAddition, 4.0},
        {3U, cf::PrincipalMovementKind::Writeoff, 2.0},
    };
    path.investor_receipts = {{3U, "plant-principal", 8.0, 8.0}};

    cf::JointScenario scenario;
    scenario.id = "base-path";
    scenario.weight = 1.0;
    scenario.project_paths = {std::move(path)};
    scenario.cash_sources = {cash_source("plant-principal", 3U, 8.0)};
    scenario.pool_costs = {{1U, 0.2}};
    portfolio.joint_scenarios = {std::move(scenario)};
    return portfolio;
}

[[nodiscard]] cf::PortfolioConfig one_draw_portfolio(
    std::size_t horizon_months = 4U) {
    cf::PortfolioConfig portfolio;
    portfolio.model_version = std::string(cf::kPortfolioModelVersion);
    portfolio.scenario_label = "one-draw warehouse fixture";
    portfolio.source_note =
        "synthetic continuing asset used to isolate warehouse accounting";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant synthetic test units";
    portfolio.horizon_months = horizon_months;
    portfolio.annual_physical_hurdle_rate = 0.12;
    portfolio.projects = {project(6.0, 6.0)};

    cf::ProjectJointPath path;
    path.project_id = "cellular-plant";
    path.resolution = cf::ProjectPathResolution::Continuing;
    path.investor_outlays = {
        {1U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 6.0}};
    path.principal_movements = {
        {1U, cf::PrincipalMovementKind::FundedPrincipalAddition, 6.0}};

    cf::JointScenario scenario;
    scenario.id = "warehouse-path";
    scenario.weight = 1.0;
    scenario.project_paths = {std::move(path)};
    // The shared participation term selects Commercial cash. This continuing
    // path declares that source kind but supplies no receipt or cash budget.
    scenario.cash_sources = {
        {"unused-commercial", cf::PortfolioCashSource::Commercial, {}}};
    portfolio.joint_scenarios = {std::move(scenario)};
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig point_ambiguity(
    std::string scenario_id) {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "point bridge probability fixture";
    ambiguity.source_note = "synthetic exact probability for bridge tests";
    ambiguity.scenario_probabilities = {
        {std::move(scenario_id), 1.0, 1.0, 1.0}};
    return ambiguity;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig two_scenario_ambiguity() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "two-state bridge probability fixture";
    ambiguity.source_note = "synthetic exact equal-weight probability";
    ambiguity.scenario_probabilities = {
        {"provider-good", 0.5, 0.5, 0.5},
        {"provider-bad", 0.5, 0.5, 0.5},
    };
    return ambiguity;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_fixture() {
    cf::SuccessParticipationConfig participation;
    participation.scenario_label = "fixed full participation fixture";
    participation.source_note =
        "synthetic contractual scaling assertion for bridge tests";
    participation.selected_nonprincipal_cash_is_contractually_scalable = true;
    participation.scalable_source_kinds = {
        cf::PortfolioCashSource::Commercial};
    return participation;
}

[[nodiscard]] cf::CapitalStackConfig stack_fixture(
    double issued_principal, double first_loss = 2.0) {
    cf::CapitalStackConfig stack;
    stack.model_version = std::string(cf::kCapitalStackModelVersion);
    stack.scenario_label = "funding bridge stack fixture";
    stack.source_note =
        "synthetic funded first-loss and callable priority notionals";
    stack.subscription_reserve_is_zero_yield_and_lossless = true;
    stack.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    stack.pool_costs_are_additional_pro_rata_calls = true;
    stack.principal_cash_is_paid_most_senior_first = true;
    stack.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    stack.tranching_does_not_change_project_cash_or_gross_loss = true;
    stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero =
        true;
    stack.buyer_direct_costs_are_additional_pro_rata_calls = true;
    stack.principal_base_cash_above_issued_principal_is_nonprincipal = true;
    stack.principal_limit_capacity_difference_is_reported_without_valuation_claim =
        true;
    stack.underlying_success_participation_fraction = 1.0;
    stack.tranches = {
        {"first-loss-residual", 0.0, first_loss, 0.0, 0.12, true},
        {"priority", first_loss, issued_principal, 0.0, 0.12, false},
    };
    return stack;
}

[[nodiscard]] cf::FundingBridgeConfig bridge_terms(
    double callable_commitment, double warehouse_limit,
    std::size_t maturity_month) {
    cf::FundingBridgeConfig bridge;
    bridge.scenario_label = "explicit bridge funding terms";
    bridge.source_note =
        "synthetic named-provider performance for adversarial tests";
    bridge.synthetic_inputs = true;
    bridge.funded_at_close_cash_million = 2.0;
    bridge.funded_at_close_provider_id = "initial-provider";
    bridge.funded_at_close_source_record_id = "synthetic-close-record";
    bridge.providers = {
        provider("initial-provider", "initial-group", 2.0),
        provider("call-provider", "call-group", callable_commitment),
        provider("warehouse-provider", "warehouse-group", warehouse_limit),
        provider("support-provider", "support-group", 10.0),
        provider("takeout-provider", "takeout-group", 10.0),
    };

    bridge.callable_facility.id = "callable-facility";
    bridge.callable_facility.provider_id = "call-provider";
    bridge.callable_facility.source_record_id = "callable-terms-record";
    bridge.callable_facility.commitment_million = callable_commitment;
    bridge.callable_facility.availability_start_month = 0U;
    bridge.callable_facility.contractual_expiry_month = maturity_month;
    bridge.callable_facility.settlement_lag_months = 1U;
    bridge.callable_facility.permitted_purpose =
        "eligible acquisition and primary-funding uses only";

    bridge.warehouse_facility.id = "warehouse-facility";
    bridge.warehouse_facility.provider_id = "warehouse-provider";
    bridge.warehouse_facility.source_record_id = "warehouse-terms-record";
    bridge.warehouse_facility.committed_limit_million = warehouse_limit;
    bridge.warehouse_facility.availability_start_month = 0U;
    bridge.warehouse_facility.availability_end_month = maturity_month;
    bridge.warehouse_facility.legal_maturity_month = maturity_month;
    bridge.warehouse_facility.settlement_lag_months = 1U;
    bridge.warehouse_facility.permitted_purpose =
        "temporary funding of retained eligible asset cost basis";
    bridge.warehouse_facility.collateral_advance_rate = 1.0;

    bridge.uncalled_commitment_is_not_cash_or_loss_absorption = true;
    bridge.acquisition_and_primary_funding_uses_precede_same_month_receipts =
        true;
    bridge.warehouse_is_external_temporary_debt = true;
    bridge.warehouse_proceeds_cannot_fund_interest_fees_or_costs = true;
    bridge.project_receipts_sweep_warehouse_principal_before_investor_cash =
        true;
    bridge.policy_uses_observed_history_only = true;
    bridge.no_dynamic_tranche_allocation_or_pricing_is_claimed = true;
    return bridge;
}

[[nodiscard]] cf::FundingBridgeConfig two_draw_bridge() {
    cf::FundingBridgeConfig bridge = bridge_terms(8.0, 5.0, 4U);
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "base-path";
    performance.capital_call_requests = {
        {"call-one", "callable-facility", 0U, 4.0},
        {"call-two", "callable-facility", 1U, 4.0},
    };
    performance.capital_call_outcomes = {
        settled_outcome("call-one", 1U, 4.0),
        settled_outcome("call-two", 2U, 4.0),
    };
    performance.supplemental_receipts = {supplemental_receipt("cost-support",
        "support-provider", 1U, cf::SupplementalFundingPurpose::CostSupport,
        0.5)};
    performance.eligible_basis_movements = {
        basis_movement("basis-add-one", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant"),
        basis_movement("basis-add-two", 2U,
            cf::EligibleBasisMovementKind::EligibleAddition, 4.0,
            "cellular-plant"),
        basis_movement("basis-return", 3U,
            cf::EligibleBasisMovementKind::PrincipalBasisReturn, 8.0,
            "plant-principal"),
        basis_movement("basis-writeoff", 3U,
            cf::EligibleBasisMovementKind::Writeoff, 2.0,
            "cellular-plant"),
    };
    performance.protection_absorptions = {protection_absorption(
        "protection-loss", 3U, 2.0, "basis-writeoff")};
    bridge.scenario_performance = {std::move(performance)};
    return bridge;
}

[[nodiscard]] cf::FundingBridgeConfig warehouse_bridge(
    std::size_t maturity_month = 4U) {
    cf::FundingBridgeConfig bridge =
        bridge_terms(4.0, 4.0, maturity_month);
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.warehouse_draw_requests = {
        {"warehouse-draw", "warehouse-facility", 0U, 4.0}};
    performance.warehouse_draw_outcomes = {
        settled_outcome("warehouse-draw", 1U, 4.0)};
    performance.eligible_basis_movements = {
        basis_movement("basis-add", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant")};
    bridge.scenario_performance = {std::move(performance)};
    return bridge;
}

[[nodiscard]] const cf::FundingBridgeScenarioResult& find_scenario(
    const cf::FundingBridgeSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const auto& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error("funding-bridge scenario not found");
    }
    return *found;
}

[[nodiscard]] const cf::FundingProviderScenarioResult& find_provider(
    const cf::FundingBridgeScenarioResult& scenario, std::string_view id) {
    const auto found = std::find_if(scenario.providers.begin(),
        scenario.providers.end(), [id](const auto& item) {
            return item.provider_id == id;
        });
    if (found == scenario.providers.end()) {
        throw std::logic_error("funding provider result not found");
    }
    return *found;
}

[[nodiscard]] const cf::FundingEconomicGroupScenarioResult& find_group(
    const cf::FundingBridgeScenarioResult& scenario, std::string_view id) {
    const auto found = std::find_if(scenario.economic_groups.begin(),
        scenario.economic_groups.end(), [id](const auto& item) {
            return item.economic_group_id == id;
        });
    if (found == scenario.economic_groups.end()) {
        throw std::logic_error("funding economic-group result not found");
    }
    return *found;
}

void check_exact_reconciliations(const cf::FundingBridgeSummary& summary,
    const cf::FundingBridgeScenarioResult& scenario,
    std::string_view message) {
    bool rows_reconcile = true;
    for (const auto& month : scenario.months) {
        if (!month.processed) {
            continue;
        }
        rows_reconcile = rows_reconcile &&
            near(month.cash_reconciliation_error_million, 0.0) &&
            near(month.warehouse_reconciliation_error_million, 0.0) &&
            near(month.warehouse_charge_reconciliation_error_million, 0.0) &&
            near(month.eligible_basis_reconciliation_error_million, 0.0) &&
            near(month.funded_protection_reconciliation_error_million, 0.0) &&
            near(month.warehouse_closing_principal_million,
                month.warehouse_opening_principal_million +
                    month.warehouse_advance_settled_million -
                    month.warehouse_principal_repayment_million) &&
            near(month.eligible_basis_closing_million,
                month.eligible_basis_opening_million +
                    month.eligible_basis_additions_million -
                    month.eligible_basis_principal_returns_million -
                    month.eligible_basis_dispositions_million -
                    month.eligible_basis_writeoffs_million -
                    month.eligible_basis_removals_million) &&
            near(month.funded_protection_closing_million,
                month.funded_protection_opening_million +
                    month.funded_protection_paid_million -
                    month.funded_protection_released_million -
                    month.funded_protection_absorbed_million);
    }
    check(rows_reconcile &&
            scenario.maximum_cash_reconciliation_error_million < kTolerance &&
            scenario.maximum_warehouse_reconciliation_error_million <
                kTolerance &&
            scenario.maximum_warehouse_charge_reconciliation_error_million <
                kTolerance &&
            scenario.maximum_eligible_basis_reconciliation_error_million <
                kTolerance &&
            scenario.maximum_funded_protection_reconciliation_error_million <
                kTolerance &&
            summary.maximum_cash_reconciliation_error_million < kTolerance &&
            summary.maximum_warehouse_reconciliation_error_million <
                kTolerance &&
            summary.maximum_warehouse_charge_reconciliation_error_million <
                kTolerance &&
            summary.maximum_eligible_basis_reconciliation_error_million <
                kTolerance &&
            summary.maximum_funded_protection_reconciliation_error_million <
                kTolerance,
        message);
}

void test_explicit_cost_support_and_no_automatic_cure() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        point_ambiguity("base-path");
    const cf::FundingBridgeSummary supported = cf::evaluate_funding_bridge(
        portfolio, ambiguity, participation_fixture(), stack_fixture(10.0),
        two_draw_bridge());
    const auto& supported_path = supported.scenarios.front();
    check(supported_path.feasible &&
            near(supported_path.total_cost_support_receipts_million, 0.5) &&
            near(supported_path.total_buyer_direct_cost_uses_million, 0.3) &&
            near(supported_path.total_pool_cost_uses_million, 0.2) &&
            near(supported_path.months[1U].total_nonasset_cost_paid_million,
                0.5) &&
            near(supported_path.months[1U].closing_cost_support_cash_million,
                0.0),
        "named settled cost support pays only the explicit non-asset costs");
    check(near(supported_path.months[1U]
                   .asset_uses_paid_from_qualifying_protection_cash_million,
              2.0) &&
            near(supported_path.months[1U]
                    .asset_uses_paid_from_callable_cash_million,
                4.0) &&
            near(supported_path.months[1U]
                    .asset_uses_paid_from_warehouse_proceeds_million,
                0.0),
        "month-one asset cash identifies protection and callable sources without overlap");
    check(near(find_provider(supported_path, "support-provider")
                   .settled_initial_and_supplemental_capital_million,
              0.5),
        "the named support provider retains its settled exposure");

    cf::FundingBridgeConfig unsupported = two_draw_bridge();
    unsupported.scenario_performance.front().supplemental_receipts.clear();
    const cf::FundingBridgeSummary failed = cf::evaluate_funding_bridge(
        portfolio, ambiguity, participation_fixture(), stack_fixture(10.0),
        unsupported);
    const auto& failed_path = failed.scenarios.front();
    check(!failed_path.feasible &&
            failed_path.failure_kind ==
                cf::FundingBridgeFailureKind::NonAssetCostShortfall &&
            failed_path.failure_phase ==
                cf::FundingBridgeFailurePhase::NonAssetCosts &&
            failed_path.first_infeasible_month ==
                std::optional<std::size_t>{1U} &&
            near(failed_path.months[1U].total_nonasset_cost_due_million, 0.5) &&
            near(failed_path.months[1U].total_nonasset_cost_paid_million, 0.0) &&
            near(failed_path.months[1U].total_nonasset_cost_unpaid_million,
                0.5) &&
            near(failed_path.months[1U].closing_controlled_cash_million, 6.0),
        "asset capital cannot become an unnamed same-month cost cure");
    check(failed_path.failure.has_value() &&
            near(failed_path.failure->shortfall_million, 0.5) &&
            !failed_path.investor_cash_npv_million.has_value(),
        "cost shortfall remains structured and suppresses successful-path NPV");
    check(near(find_provider(failed_path, "warehouse-provider")
                   .ending_warehouse_contingent_commitment_million,
              5.0) &&
            near(find_group(failed_path, "warehouse-group")
                    .ending_contingent_funding_dependency_million,
                5.0) &&
            near(find_group(failed_path, "call-group")
                    .ending_contingent_funding_dependency_million,
                4.0) &&
            near(failed_path
                    .ending_economic_group_contingent_funding_dependency_hhi,
                41.0 / 81.0),
        "an early unrelated failure retains callable and still-open warehouse commitments at the failure snapshot");

    cf::FundingBridgeConfig unnamed = two_draw_bridge();
    unnamed.scenario_performance.front().supplemental_receipts.front()
        .provider_id = "unnamed-provider";
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack_fixture(10.0), unnamed);
    }, "supplemental cash must reference a declared legal provider");
}

void test_requests_are_not_cash_and_partial_outcomes_remain_visible() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        point_ambiguity("base-path");

    cf::FundingBridgeConfig partial = two_draw_bridge();
    auto& partial_performance = partial.scenario_performance.front();
    partial_performance.capital_call_outcomes.front().status =
        cf::FundingSettlementStatus::FinalPartialSettlement;
    partial_performance.capital_call_outcomes.front().actual_cash_million = 3.0;
    const cf::FundingBridgeSummary partial_summary =
        cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack_fixture(10.0), partial);
    const auto& partial_path = partial_summary.scenarios.front();
    check(!partial_path.feasible &&
            partial_path.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            near(partial_path.total_actual_call_receipts_million, 3.0) &&
            near(partial_path.callable_defaulted_million, 1.0) &&
            near(partial_path.first_funding_shortfall_million, 1.0),
        "a final partial settlement records both cash received and defaulted remainder");

    cf::FundingBridgeConfig covered_partial = partial;
    covered_partial.scenario_performance.front().supplemental_receipts.push_back(
        supplemental_receipt("replacement-protection", "support-provider", 1U,
            cf::SupplementalFundingPurpose::ProtectionReplenishment, 1.0));
    const cf::FundingBridgeSummary covered_summary =
        cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack_fixture(10.0), covered_partial);
    const auto& covered_path = covered_summary.scenarios.front();
    check(covered_path.feasible &&
            near(covered_path.callable_defaulted_million, 1.0) &&
            find_provider(covered_path, "call-provider")
                .provider_nonperformance_observed &&
            near(find_provider(covered_path, "call-provider")
                    .callable_defaulted_million,
                1.0) &&
            near(covered_path.total_protection_replenishment_receipts_million,
                1.0),
        "explicit settled replacement cash may preserve feasibility without erasing provider default");
}

void test_terminal_warehouse_exposure_is_retained_without_inferred_loss() {
    const cf::PortfolioConfig portfolio = one_draw_portfolio();
    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), warehouse_bridge());
    const auto& scenario = summary.scenarios.front();
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseMaturityUnpaid &&
            scenario.failure_phase ==
                cf::FundingBridgeFailurePhase::LegalMaturity &&
            scenario.first_infeasible_month ==
                std::optional<std::size_t>{4U} &&
            near(scenario.ending_warehouse_principal_million, 4.0) &&
            near(scenario.ending_warehouse_funded_ead_million, 4.0) &&
            near(scenario.months[4U].warehouse_past_due_principal_million,
                4.0),
        "unpaid maturity principal remains funded exposure rather than disappearing");
    check(scenario.months[0U].warehouse_advance_tests.size() == 1U &&
            scenario.months[0U].warehouse_advance_tests.front().passed &&
            scenario.months[0U].warehouse_advance_tests.front().phase ==
                cf::WarehouseAdvanceTestPhase::Request &&
            scenario.months[1U].warehouse_advance_tests.size() == 1U &&
            scenario.months[1U].warehouse_advance_tests.front().passed &&
            scenario.months[1U].warehouse_advance_tests.front().phase ==
                cf::WarehouseAdvanceTestPhase::Settlement &&
            near(scenario.months[1U]
                    .warehouse_advance_tests.front()
                    .borrowing_base_million,
                4.0) &&
            near(scenario.months[1U]
                    .asset_uses_paid_from_warehouse_proceeds_million,
                4.0),
        "request and settlement each retain their own borrowing-base and source tests");
    check(!summary.warehouse_loss_or_recovery_is_inferred &&
            near(summary.expected_ending_warehouse_funded_ead_million.central,
                4.0) &&
            near(summary.ending_warehouse_funded_ead_expected_shortfall_99_million
                    .central,
                4.0),
        "warehouse tail exposure remains reportable without an invented writeoff or recovery");
    check(!scenario.investor_cash_npv_million.has_value(),
        "unpaid legal maturity suppresses successful-path NPV");
    check_exact_reconciliations(summary, scenario,
        "failed maturity preserves exact cash, warehouse, E, and F ledgers");

    cf::PortfolioConfig longer = one_draw_portfolio(5U);
    cf::FundingBridgeConfig early_maturity = warehouse_bridge(2U);
    const cf::FundingBridgeSummary early_summary =
        cf::evaluate_funding_bridge(longer,
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), early_maturity);
    const auto& early_path = early_summary.scenarios.front();
    check(!early_path.feasible &&
            early_path.failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseMaturityUnpaid &&
            early_path.first_infeasible_month ==
                std::optional<std::size_t>{2U} &&
            !early_path.months[3U].processed && !early_path.months[4U].processed &&
            !early_path.months[5U].processed,
        "legal maturity is enforced when due, not deferred to portfolio horizon");
}

void test_protection_absorption_and_eligibility_reduction_fail_separately() {
    cf::PortfolioConfig writeoff_portfolio = one_draw_portfolio();
    writeoff_portfolio.joint_scenarios.front()
        .project_paths.front()
        .principal_movements.push_back(
            {2U, cf::PrincipalMovementKind::Writeoff, 1.0});
    cf::FundingBridgeConfig writeoff_bridge = warehouse_bridge();
    auto& writeoff_performance = writeoff_bridge.scenario_performance.front();
    writeoff_performance.eligible_basis_movements.push_back(basis_movement(
        "basis-writeoff", 2U, cf::EligibleBasisMovementKind::Writeoff, 1.0,
        "cellular-plant"));
    writeoff_performance.protection_absorptions.push_back(
        protection_absorption(
            "protection-absorption", 2U, 1.0, "basis-writeoff"));
    const cf::FundingBridgeSummary writeoff_summary =
        cf::evaluate_funding_bridge(writeoff_portfolio,
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), writeoff_bridge);
    const auto& writeoff = writeoff_summary.scenarios.front();
    check(!writeoff.feasible &&
            writeoff.failure_kind ==
                cf::FundingBridgeFailureKind::FundedProtectionDeficit &&
            writeoff.failure_phase ==
                cf::FundingBridgeFailurePhase::EligibilityAndProtectionTest &&
            writeoff.first_infeasible_month ==
                std::optional<std::size_t>{2U} &&
            near(writeoff.months[2U].eligible_basis_writeoffs_million, 1.0) &&
            near(writeoff.months[2U].funded_protection_absorbed_million, 1.0) &&
            near(writeoff.months[2U].funded_protection_closing_million, 1.0) &&
            near(writeoff.months[2U].required_funded_protection_million, 2.0) &&
            near(writeoff.months[2U].funded_protection_headroom_million, -1.0),
        "an evidenced absorption, not the writeoff by itself, reduces funded protection");
    check_exact_reconciliations(writeoff_summary, writeoff,
        "writeoff and linked absorption retain exact E and F roll-forwards");

    cf::FundingBridgeConfig removal_bridge = warehouse_bridge();
    removal_bridge.scenario_performance.front()
        .eligible_basis_movements.push_back(basis_movement("eligibility-removal",
            2U, cf::EligibleBasisMovementKind::EligibilityRemoval, 2.0,
            "eligibility-review"));
    const cf::FundingBridgeSummary removal_summary =
        cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), removal_bridge);
    const auto& removal = removal_summary.scenarios.front();
    check(!removal.feasible &&
            removal.failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseBorrowingBaseDeficit &&
            removal.first_infeasible_month ==
                std::optional<std::size_t>{2U} &&
            near(removal.months[2U].eligible_basis_removals_million, 2.0) &&
            near(removal.months[2U].eligible_basis_closing_million, 4.0) &&
            near(removal.months[2U].funded_protection_closing_million, 2.0) &&
            near(removal.months[2U]
                    .warehouse_borrowing_base_after_reductions_million,
                2.0) &&
            near(removal.months[2U].warehouse_borrowing_base_headroom_million,
                -2.0),
        "eligibility removal leaves F intact but reduces the post-reduction borrowing base");
    check_exact_reconciliations(removal_summary, removal,
        "eligibility-removal failure retains exact cash, warehouse, E, and F ledgers");
}

void test_same_month_receipt_cannot_cure_an_earlier_use() {
    cf::PortfolioConfig portfolio = one_draw_portfolio();
    auto& path = portfolio.joint_scenarios.front().project_paths.front();
    path.investor_receipts = {{1U, "same-month-principal", 4.0, 4.0}};
    portfolio.joint_scenarios.front().cash_sources = {
        cash_source("same-month-principal", 1U, 4.0)};

    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 4.0, 4U);
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.eligible_basis_movements = {
        basis_movement("basis-add", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant"),
        basis_movement("basis-return", 1U,
            cf::EligibleBasisMovementKind::PrincipalBasisReturn, 4.0,
            "same-month-principal"),
    };
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            scenario.first_infeasible_month ==
                std::optional<std::size_t>{1U} &&
            near(scenario.first_funding_shortfall_million, 4.0) &&
            near(scenario.months[1U]
                    .declared_counterfactual_project_receipts_million,
                4.0) &&
            near(scenario.months[1U].actual_project_receipts_million, 0.0) &&
            near(scenario.total_project_receipts_million, 0.0),
        "same-month project cash remains counterfactual after the prior funding failure");
    check(!scenario.months[1U].investor_net_cash_flow_million.has_value(),
        "the failed month does not publish a synthetic successful cash flow");
}

[[nodiscard]] cf::PortfolioConfig two_provider_state_portfolio() {
    cf::PortfolioConfig portfolio = one_draw_portfolio();
    portfolio.scenario_label = "two provider-performance states";
    portfolio.horizon_months = 3U;
    portfolio.joint_scenarios.clear();

    for (const std::string& id : {std::string("provider-good"),
             std::string("provider-bad")}) {
        cf::ProjectJointPath path;
        path.project_id = "cellular-plant";
        path.resolution = cf::ProjectPathResolution::Resolved;
        path.investor_outlays = {
            {1U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 2.0},
            {1U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 4.0},
        };
        path.principal_movements = {
            {1U, cf::PrincipalMovementKind::FundedPrincipalAddition, 2.0},
            {1U, cf::PrincipalMovementKind::FundedPrincipalAddition, 4.0},
        };
        path.investor_receipts = {
            {3U, id + "-principal", 6.0, 6.0}};

        cf::JointScenario scenario;
        scenario.id = id;
        scenario.weight = 0.5;
        scenario.project_paths = {std::move(path)};
        scenario.cash_sources = {
            cash_source(id + "-principal", 3U, 6.0)};
        portfolio.joint_scenarios.push_back(std::move(scenario));
    }
    return portfolio;
}

[[nodiscard]] cf::FundingBridgeScenarioPerformance provider_performance(
    std::string scenario_id, double settled_amount,
    cf::FundingSettlementStatus status) {
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = std::move(scenario_id);
    performance.capital_call_requests = {
        {"call-part-one", "callable-facility", 0U, 1.0},
        {"call-part-two", "callable-facility", 0U, 3.0},
    };
    performance.capital_call_outcomes = {
        settled_outcome("call-part-one", 1U, 1.0),
        settled_outcome("call-part-two", 1U, settled_amount - 1.0),
    };
    performance.capital_call_outcomes.back().status = status;
    performance.eligible_basis_movements = {
        basis_movement("basis-add-one", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 2.0,
            "cellular-plant"),
        basis_movement("basis-add-two", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 4.0,
            "cellular-plant"),
        basis_movement("basis-return-one", 3U,
            cf::EligibleBasisMovementKind::PrincipalBasisReturn, 1.0,
            performance.scenario_id + "-principal"),
        basis_movement("basis-return-two", 3U,
            cf::EligibleBasisMovementKind::PrincipalBasisReturn, 5.0,
            performance.scenario_id + "-principal"),
    };
    return performance;
}

[[nodiscard]] cf::FundingBridgeConfig two_provider_state_bridge(
    bool bad_state_is_partial) {
    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 4.0, 3U);
    bridge.scenario_performance = {
        provider_performance("provider-good", 4.0,
            cf::FundingSettlementStatus::SettledInFull),
        provider_performance("provider-bad", bad_state_is_partial ? 3.0 : 4.0,
            bad_state_is_partial
                ? cf::FundingSettlementStatus::FinalPartialSettlement
                : cf::FundingSettlementStatus::SettledInFull),
    };
    return bridge;
}

void test_nonanticipativity_and_mixed_feasibility_suppress_expected_npv() {
    const cf::PortfolioConfig portfolio = two_provider_state_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity = two_scenario_ambiguity();
    const cf::CapitalStackConfig stack = stack_fixture(6.0);

    cf::FundingBridgeConfig hindsight = two_provider_state_bridge(false);
    auto& bad = hindsight.scenario_performance.back();
    bad.capital_call_requests.back().requested_million = 2.0;
    bad.capital_call_outcomes.back().actual_cash_million = 2.0;
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack, hindsight);
    }, "identical observed histories cannot choose different call requests");

    cf::FundingBridgeConfig missing_request = two_provider_state_bridge(false);
    missing_request.scenario_performance.back().capital_call_requests.clear();
    missing_request.scenario_performance.back().capital_call_outcomes.clear();
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack, missing_request);
    }, "a missing request is an explicit zero decision, not missing data");

    const cf::FundingBridgeSummary mixed = cf::evaluate_funding_bridge(
        portfolio, ambiguity, participation_fixture(), stack,
        two_provider_state_bridge(true));
    const auto& good = find_scenario(mixed, "provider-good");
    const auto& failed = find_scenario(mixed, "provider-bad");
    check(mixed.monthly_path_nonanticipativity_validated && good.feasible &&
            !failed.feasible &&
            failed.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            near(failed.callable_defaulted_million, 1.0),
        "same policy may have different provider outcomes without using hindsight");
    check(!mixed.expected_investor_cash_npv_million.has_value() &&
            !mixed.expected_cash_npv_change_vs_fully_prefunded_million
                 .has_value() &&
            !mixed.expected_economic_npv_change_after_callable_liquidity_cost_million
                 .has_value() &&
            near(mixed.funding_failure_probability.central, 0.5) &&
            near(mixed.expected_funding_shortfall_million.central, 0.5),
        "one admitted failed state suppresses unconditional expected-return outputs");
}

void test_facility_availability_provider_capacity_and_takeout_boundary() {
    const cf::PortfolioConfig portfolio = two_provider_state_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity = two_scenario_ambiguity();
    const cf::CapitalStackConfig stack = stack_fixture(6.0);

    cf::FundingBridgeConfig unavailable = two_provider_state_bridge(false);
    unavailable.callable_facility.availability_start_month = 1U;
    const cf::FundingBridgeSummary unavailable_summary =
        cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack, unavailable);
    check(std::all_of(unavailable_summary.scenarios.begin(),
              unavailable_summary.scenarios.end(), [](const auto& scenario) {
                  return !scenario.feasible &&
                      scenario.failure_kind ==
                      cf::FundingBridgeFailureKind::CallableFacilityUnavailable &&
                      scenario.first_infeasible_month ==
                      std::optional<std::size_t>{0U};
              }),
        "call requests outside the contractual window fail as unavailable capacity");

    cf::FundingBridgeConfig capacity = two_provider_state_bridge(false);
    capacity.providers[1U].declared_capacity_million = 3.0;
    const cf::FundingBridgeSummary capacity_summary =
        cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack, capacity);
    check(std::all_of(capacity_summary.scenarios.begin(),
              capacity_summary.scenarios.end(), [](const auto& scenario) {
                  return !scenario.feasible &&
                      scenario.failure_kind ==
                      cf::FundingBridgeFailureKind::ProviderCapacityExceeded;
              }),
        "declared provider capacity binds funded plus contingent dependency");

    cf::FundingBridgeConfig warehouse_unavailable = warehouse_bridge();
    warehouse_unavailable.warehouse_facility.availability_start_month = 1U;
    const cf::FundingBridgeSummary warehouse_unavailable_summary =
        cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), warehouse_unavailable);
    check(!warehouse_unavailable_summary.scenarios.front().feasible &&
            warehouse_unavailable_summary.scenarios.front().failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseFacilityUnavailable,
        "warehouse request cannot precede its contractual availability");

    cf::FundingBridgeConfig takeout = warehouse_bridge(3U);
    takeout.scenario_performance.front().supplemental_receipts = {
        supplemental_receipt("settled-takeout", "takeout-provider", 3U,
            cf::SupplementalFundingPurpose::SettledTakeout, 4.0)};
    const cf::FundingBridgeSummary takeout_summary =
        cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), takeout);
    const auto& takeout_path = takeout_summary.scenarios.front();
    check(takeout_path.feasible &&
            near(takeout_path.total_settled_takeout_receipts_million, 4.0) &&
            near(takeout_path.total_warehouse_principal_repayments_million,
                4.0) &&
            near(takeout_path.ending_warehouse_principal_million, 0.0) &&
            near(takeout_path.months[3U]
                    .warehouse_principal_repaid_from_takeout_million,
                4.0) &&
            near(find_provider(takeout_path, "takeout-provider")
                    .settled_takeout_cash_million,
                4.0),
        "only settled named takeout cash can repay warehouse principal");
    check(!takeout_path.investor_cash_npv_million.has_value() &&
            !takeout_summary.expected_investor_cash_npv_million.has_value(),
        "takeout settlement suppresses NPV while its replacement liability is outside scope");
}

void test_frictionless_calls_and_liquidity_opportunity_cost() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        point_ambiguity("base-path");
    const cf::CapitalStackConfig stack = stack_fixture(10.0);
    const cf::FundingBridgeSummary frictionless =
        cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack, two_draw_bridge());
    const auto& free_path = frictionless.scenarios.front();
    check(free_path.feasible && free_path.investor_cash_npv_million.has_value() &&
            free_path.fully_prefunded_baseline_npv_million.has_value() &&
            free_path.cash_npv_change_vs_fully_prefunded_million.has_value() &&
            free_path.economic_npv_change_after_callable_liquidity_cost_million
                .has_value() &&
            *free_path.cash_npv_change_vs_fully_prefunded_million > 0.0 &&
            free_path.fully_prefunded_prefunding_drag_million.has_value() &&
            near(free_path.callable_liquidity_opportunity_cost_pv_million,
                0.0) &&
            near(*free_path.economic_npv_change_after_callable_liquidity_cost_million,
                *free_path.cash_npv_change_vs_fully_prefunded_million),
        "frictionless delayed calls isolate the mechanical prefunding-timing benefit");

    cf::FundingBridgeConfig reserved = two_draw_bridge();
    reserved.callable_facility.liquidity_reserve_fraction = 0.5;
    reserved.callable_facility.annual_liquidity_hurdle_rate = 0.08;
    reserved.callable_facility.annual_reserve_yield_rate = 0.02;
    const cf::FundingBridgeSummary costly = cf::evaluate_funding_bridge(
        portfolio, ambiguity, participation_fixture(), stack, reserved);
    const auto& costly_path = costly.scenarios.front();
    check(costly_path.feasible &&
            costly_path.investor_cash_npv_million.has_value() &&
            free_path.investor_cash_npv_million.has_value() &&
            costly_path.cash_npv_change_vs_fully_prefunded_million.has_value() &&
            free_path.cash_npv_change_vs_fully_prefunded_million.has_value() &&
            costly_path.economic_npv_change_after_callable_liquidity_cost_million
                .has_value() &&
            near(*costly_path.investor_cash_npv_million,
                *free_path.investor_cash_npv_million) &&
            near(*costly_path.cash_npv_change_vs_fully_prefunded_million,
                *free_path.cash_npv_change_vs_fully_prefunded_million) &&
            costly_path.callable_liquidity_opportunity_cost_pv_million > 0.0 &&
            near(*costly_path.economic_npv_change_after_callable_liquidity_cost_million,
                *costly_path.cash_npv_change_vs_fully_prefunded_million -
                    costly_path.callable_liquidity_opportunity_cost_pv_million),
        "liquidity reserve drag is a separate non-cash economic diagnostic");
    check_exact_reconciliations(costly, costly_path,
        "opportunity-cost diagnostics do not disturb cash accounting");
}

void test_frictionless_on_demand_calls_equal_prefunding_drag() {
    cf::PortfolioConfig portfolio = two_draw_portfolio();
    auto& project_path =
        portfolio.joint_scenarios.front().project_paths.front();
    project_path.investor_outlays = {
        {0U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 2.0},
        {1U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 4.0},
        {1U, cf::InvestorOutlayPurpose::BuyerDirectCost, 0.3},
        {2U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 4.0},
    };
    project_path.principal_movements = {
        {0U, cf::PrincipalMovementKind::FundedPrincipalAddition, 2.0},
        {1U, cf::PrincipalMovementKind::FundedPrincipalAddition, 4.0},
        {2U, cf::PrincipalMovementKind::FundedPrincipalAddition, 4.0},
        {3U, cf::PrincipalMovementKind::Writeoff, 2.0},
    };

    cf::FundingBridgeConfig bridge = two_draw_bridge();
    bridge.scenario_performance.front().eligible_basis_movements = {
        basis_movement("basis-add-close", 0U,
            cf::EligibleBasisMovementKind::EligibleAddition, 2.0,
            "cellular-plant"),
        basis_movement("basis-add-one", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 4.0,
            "cellular-plant"),
        basis_movement("basis-add-two", 2U,
            cf::EligibleBasisMovementKind::EligibleAddition, 4.0,
            "cellular-plant"),
        basis_movement("basis-return", 3U,
            cf::EligibleBasisMovementKind::PrincipalBasisReturn, 8.0,
            "plant-principal"),
        basis_movement("basis-writeoff", 3U,
            cf::EligibleBasisMovementKind::Writeoff, 2.0,
            "cellular-plant"),
    };

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("base-path"), participation_fixture(),
        stack_fixture(10.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const double month_one_discount = std::pow(1.12, 1.0 / 12.0);
    const double month_two_discount = std::pow(1.12, 2.0 / 12.0);
    const double month_three_discount = std::pow(1.12, 3.0 / 12.0);
    const double expected_bridge_npv = -2.0 -
        4.5 / month_one_discount - 4.0 / month_two_discount +
        8.0 / month_three_discount;
    const double expected_prefunded_npv = -10.0 -
        0.5 / month_one_discount + 8.0 / month_three_discount;
    const double expected_prefunding_drag = 8.0 -
        4.0 / month_one_discount - 4.0 / month_two_discount;
    check(scenario.feasible && scenario.investor_cash_npv_million.has_value() &&
            scenario.fully_prefunded_baseline_npv_million.has_value() &&
            scenario.cash_npv_change_vs_fully_prefunded_million.has_value() &&
            scenario.fully_prefunded_prefunding_drag_million.has_value() &&
            near(*scenario.investor_cash_npv_million,
                expected_bridge_npv) &&
            near(*scenario.fully_prefunded_baseline_npv_million,
                expected_prefunded_npv) &&
            near(*scenario.cash_npv_change_vs_fully_prefunded_million,
                *scenario.fully_prefunded_prefunding_drag_million) &&
            near(*scenario.cash_npv_change_vs_fully_prefunded_million,
                expected_prefunding_drag),
        "frictionless calls timed exactly to asset uses make NPV improvement equal the avoided prefunding drag");
}

void test_every_request_has_exactly_one_final_outcome() {
    const cf::PortfolioConfig call_portfolio = two_draw_portfolio();
    const cf::PortfolioAmbiguityConfig call_ambiguity =
        point_ambiguity("base-path");
    cf::FundingBridgeConfig missing_call = two_draw_bridge();
    missing_call.scenario_performance.front().capital_call_outcomes.pop_back();
    expect_invalid([&] {
        cf::validate_funding_bridge_config(call_portfolio, call_ambiguity,
            participation_fixture(), stack_fixture(10.0), missing_call);
    }, "every capital-call request requires exactly one final outcome");

    const cf::PortfolioConfig warehouse_portfolio = one_draw_portfolio();
    const cf::PortfolioAmbiguityConfig warehouse_ambiguity =
        point_ambiguity("warehouse-path");
    cf::FundingBridgeConfig missing_draw = warehouse_bridge();
    missing_draw.scenario_performance.front().warehouse_draw_outcomes.clear();
    expect_invalid([&] {
        cf::validate_funding_bridge_config(warehouse_portfolio,
            warehouse_ambiguity, participation_fixture(), stack_fixture(6.0),
            missing_draw);
    }, "every warehouse-draw request requires exactly one final outcome");

    cf::FundingBridgeConfig invalid_status = two_draw_bridge();
    invalid_status.scenario_performance.front()
        .capital_call_outcomes.front()
        .status = static_cast<cf::FundingSettlementStatus>(255U);
    expect_invalid([&] {
        cf::validate_funding_bridge_config(call_portfolio, call_ambiguity,
            participation_fixture(), stack_fixture(10.0), invalid_status);
    }, "the direct C++ contract rejects an out-of-domain settlement status");

    cf::FundingBridgeConfig invalid_purpose = two_draw_bridge();
    invalid_purpose.scenario_performance.front()
        .supplemental_receipts.front()
        .purpose = static_cast<cf::SupplementalFundingPurpose>(255U);
    expect_invalid([&] {
        cf::validate_funding_bridge_config(call_portfolio, call_ambiguity,
            participation_fixture(), stack_fixture(10.0), invalid_purpose);
    }, "the direct C++ contract rejects an out-of-domain supplemental purpose");

    cf::FundingBridgeConfig invalid_basis_kind = two_draw_bridge();
    invalid_basis_kind.scenario_performance.front()
        .eligible_basis_movements.front()
        .kind = static_cast<cf::EligibleBasisMovementKind>(255U);
    expect_invalid([&] {
        cf::validate_funding_bridge_config(call_portfolio, call_ambiguity,
            participation_fixture(), stack_fixture(10.0),
            invalid_basis_kind);
    }, "the direct C++ contract rejects an out-of-domain eligible-basis movement kind");
}

void test_partitioned_same_month_actions_fail_nonanticipativity() {
    const cf::PortfolioConfig portfolio = two_provider_state_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity = two_scenario_ambiguity();
    const cf::CapitalStackConfig stack = stack_fixture(6.0);

    cf::FundingBridgeConfig calls = two_provider_state_bridge(false);
    auto& whole_call = calls.scenario_performance.front();
    whole_call.capital_call_requests = {
        {"call-whole", "callable-facility", 0U, 4.0}};
    whole_call.capital_call_outcomes = {
        settled_outcome("call-whole", 1U, 4.0)};
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack, calls);
    }, "one call of four and calls of one plus three are different policy actions");

    cf::FundingBridgeConfig draws = two_provider_state_bridge(false);
    auto& whole_draw = draws.scenario_performance.front();
    whole_draw.warehouse_draw_requests = {
        {"draw-whole", "warehouse-facility", 0U, 4.0}};
    whole_draw.warehouse_draw_outcomes = {
        settled_outcome("draw-whole", 1U, 4.0)};
    auto& split_draw = draws.scenario_performance.back();
    split_draw.warehouse_draw_requests = {
        {"draw-part-one", "warehouse-facility", 0U, 1.0},
        {"draw-part-two", "warehouse-facility", 0U, 3.0},
    };
    split_draw.warehouse_draw_outcomes = {
        settled_outcome("draw-part-one", 1U, 1.0),
        settled_outcome("draw-part-two", 1U, 3.0),
    };
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack, draws);
    }, "one draw of four and draws of one plus three are different policy actions");
}

void test_request_phase_separates_contingent_limit_from_funded_tests() {
    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        one_draw_portfolio(), point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), warehouse_bridge());
    const auto& scenario = summary.scenarios.front();
    const auto& request = scenario.months[0U].warehouse_advance_tests.front();
    const auto& settlement = scenario.months[1U].warehouse_advance_tests.front();
    check(request.phase == cf::WarehouseAdvanceTestPhase::Request &&
            near(request.prospective_funded_and_contingent_dependency_million,
                4.0) &&
            request.facility_limit_passed &&
            near(request.funded_protection_million, 2.0) &&
            near(request.required_funded_protection_million, 0.0) &&
            near(request.borrowing_base_million, 0.0) &&
            !request.funded_protection_test_applicable &&
            !request.borrowing_base_test_applicable && request.passed,
        "a notice tests prospective facility dependency but is not funded debt for F or BB");
    check(settlement.phase == cf::WarehouseAdvanceTestPhase::Settlement &&
            near(settlement.principal_after_settlement_million, 4.0) &&
            near(settlement.funded_protection_million, 2.0) &&
            near(settlement.required_funded_protection_million, 2.0) &&
            near(settlement.borrowing_base_million, 4.0) &&
            settlement.facility_limit_passed &&
            settlement.funded_protection_test_applicable &&
            settlement.funded_protection_test_passed &&
            settlement.borrowing_base_test_applicable &&
            settlement.borrowing_base_test_passed,
        "settled warehouse cash must pass facility, funded-protection, and borrowing-base tests");
}

void test_warehouse_settlement_cannot_cross_legal_maturity() {
    const cf::PortfolioConfig portfolio = one_draw_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        point_ambiguity("warehouse-path");
    cf::FundingBridgeConfig bridge = warehouse_bridge(2U);
    auto& performance = bridge.scenario_performance.front();
    performance.warehouse_draw_requests.front().request_month = 2U;
    performance.warehouse_draw_outcomes.front().settlement_month = 3U;
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack_fixture(6.0), bridge);
    }, "a warehouse request that contractually settles after legal maturity is inadmissible");
}

void test_failed_warehouse_capacity_is_burned_not_reused() {
    cf::PortfolioConfig portfolio = one_draw_portfolio();
    auto& path = portfolio.joint_scenarios.front().project_paths.front();
    path.investor_outlays.front().month = 0U;
    path.principal_movements.front().month = 0U;

    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 4.0, 4U);
    bridge.callable_facility.settlement_lag_months = 0U;
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.capital_call_requests = {
        {"same-day-call", "callable-facility", 0U, 4.0}};
    performance.capital_call_outcomes = {
        settled_outcome("same-day-call", 0U, 4.0)};
    performance.warehouse_draw_requests = {
        {"failed-draw", "warehouse-facility", 0U, 4.0},
        {"replacement-draw", "warehouse-facility", 2U, 4.0},
    };
    performance.warehouse_draw_outcomes = {
        failed_outcome("failed-draw", 1U),
        failed_outcome("replacement-draw", 3U),
    };
    performance.eligible_basis_movements = {basis_movement("basis-add", 0U,
        cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
        "cellular-plant")};
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& rejected =
        scenario.months[2U].warehouse_advance_tests.front();
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseLimitExceeded &&
            scenario.first_infeasible_month ==
                std::optional<std::size_t>{2U} &&
            near(scenario.warehouse_draw_defaulted_million, 4.0) &&
            rejected.phase == cf::WarehouseAdvanceTestPhase::Request &&
            near(rejected.prospective_funded_and_contingent_dependency_million,
                8.0) &&
            !rejected.facility_limit_passed,
        "a final failed draw burns facility capacity and cannot be silently requested again");
    check(scenario.failure.has_value() &&
            scenario.failure->provider_id == "warehouse-provider" &&
            scenario.failure->facility_id == "warehouse-facility" &&
            scenario.failure->request_id == "replacement-draw" &&
            near(scenario.failure->amount_due_million, 8.0) &&
            near(scenario.failure->amount_available_million, 4.0) &&
            near(scenario.failure->shortfall_million, 4.0),
        "burned-capacity rejection retains exact warehouse provider lineage");
}

void test_partial_cost_support_keeps_atomic_use_and_separate_gap() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    cf::FundingBridgeConfig bridge = two_draw_bridge();
    bridge.scenario_performance.front().supplemental_receipts.front()
        .actual_cash_million = 0.2;
    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("base-path"), participation_fixture(),
        stack_fixture(10.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& month = scenario.months[1U];
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::NonAssetCostShortfall &&
            near(month.total_nonasset_cost_due_million, 0.5) &&
            near(month.total_nonasset_cost_paid_million, 0.0) &&
            near(month.total_nonasset_cost_unpaid_million, 0.5) &&
            near(month.nonasset_cost_liquidity_gap_million, 0.3) &&
            near(month.closing_cost_support_cash_million, 0.2) &&
            near(month.acquisition_and_primary_funding_due_million, 0.0) &&
            near(month.acquisition_and_primary_funding_paid_million, 0.0) &&
            near(month.acquisition_and_primary_funding_unpaid_million, 0.0),
        "an atomic nonasset-cost failure leaves the whole transaction unpaid while retaining available cash");
    check(scenario.failure.has_value() &&
            near(scenario.failure->rejected_transaction_million, 0.5) &&
            near(scenario.failure->amount_due_million, 0.5) &&
            near(scenario.failure->amount_available_million, 0.2) &&
            near(scenario.failure->shortfall_million, 0.3) &&
            near(scenario.first_funding_shortfall_million, 0.3) &&
            near(scenario.total_funding_shortfall_million, 0.3),
        "cost-use status is distinct from the smaller liquidity gap");
}

void test_restricted_source_asset_failure_is_atomic() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    cf::FundingBridgeConfig bridge = two_draw_bridge();
    auto& performance = bridge.scenario_performance.front();
    performance.capital_call_requests = {
        {"restricted-call", "callable-facility", 0U, 3.0}};
    performance.capital_call_outcomes = {
        settled_outcome("restricted-call", 1U, 3.0)};
    performance.supplemental_receipts.push_back(supplemental_receipt(
        "restricted-protection", "support-provider", 1U,
        cf::SupplementalFundingPurpose::ProtectionReplenishment, 3.0));
    performance.eligible_basis_movements = {basis_movement(
        "restricted-eligible-basis", 1U,
        cf::EligibleBasisMovementKind::EligibleAddition, 2.0,
        "cellular-plant")};
    performance.protection_absorptions.clear();

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("base-path"), participation_fixture(),
        stack_fixture(10.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& month = scenario.months[1U];
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            near(month.acquisition_and_primary_funding_due_million, 6.0) &&
            near(month.acquisition_and_primary_funding_paid_million, 0.0) &&
            near(month.acquisition_and_primary_funding_unpaid_million, 6.0) &&
            near(month.acquisition_and_primary_funding_liquidity_gap_million,
                1.0) &&
            near(month.asset_uses_paid_from_qualifying_protection_cash_million,
                0.0) &&
            near(month.asset_uses_paid_from_callable_cash_million, 0.0) &&
            near(month.asset_uses_paid_from_warehouse_proceeds_million, 0.0),
        "restricted cash cannot partially settle an atomic asset transaction");
    check(scenario.failure.has_value() &&
            near(scenario.failure->rejected_transaction_million, 6.0) &&
            near(scenario.failure->amount_due_million, 4.0) &&
            near(scenario.failure->amount_available_million, 3.0) &&
            near(scenario.failure->shortfall_million, 1.0) &&
            near(scenario.first_funding_shortfall_million, 1.0) &&
            near(scenario.total_funding_shortfall_million, 1.0) &&
            near(month.closing_qualifying_protection_cash_million, 5.0) &&
            near(month.closing_callable_cash_million, 3.0),
        "only the restricted-source liquidity gap is reported as shortfall and all cash remains in its source bucket");
}

void test_basis_returns_and_dispositions_cannot_create_phantom_protection() {
    for (const cf::EligibleBasisMovementKind reduction_kind : {
             cf::EligibleBasisMovementKind::PrincipalBasisReturn,
             cf::EligibleBasisMovementKind::Disposition}) {
        cf::PortfolioConfig portfolio = one_draw_portfolio(3U);
        auto& path = portfolio.joint_scenarios.front().project_paths.front();
        path.resolution = cf::ProjectPathResolution::Resolved;
        path.investor_receipts = {
            {2U, "protection-return-cash", 6.0, 6.0}};
        portfolio.joint_scenarios.front().cash_sources = {
            cash_source("protection-return-cash", 2U, 6.0)};

        cf::FundingBridgeConfig bridge = bridge_terms(4.0, 1.0, 3U);
        cf::FundingBridgeScenarioPerformance performance;
        performance.scenario_id = "warehouse-path";
        performance.capital_call_requests = {
            {"protection-call", "callable-facility", 0U, 4.0}};
        performance.capital_call_outcomes = {
            settled_outcome("protection-call", 1U, 4.0)};
        performance.eligible_basis_movements = {
            basis_movement("protection-basis-add", 1U,
                cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
                "cellular-plant"),
            basis_movement("protection-basis-reduction", 2U, reduction_kind,
                6.0, "protection-return-cash"),
        };
        bridge.scenario_performance = {std::move(performance)};

        const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
            portfolio, point_ambiguity("warehouse-path"),
            participation_fixture(), stack_fixture(6.0), bridge);
        const auto& scenario = summary.scenarios.front();
        const auto& month = scenario.months[2U];
        check(scenario.feasible && near(month.eligible_basis_closing_million, 0.0) &&
                near(month.funded_protection_closing_million, 2.0) &&
                near(month.qualifying_protection_invested_basis_closing_million,
                    0.0) &&
                near(month.retained_protection_supporting_asset_basis_closing_million,
                    0.0) &&
                near(month.protection_cash_reclassified_from_project_receipts_million,
                    2.0) &&
                near(month.closing_qualifying_protection_cash_million, 2.0) &&
                near(month.investor_distribution_million, 4.0),
            "basis cash backing funded protection is retained before any investor distribution");
        check(month.qualifying_protection_invested_basis_closing_million <=
                    month.retained_protection_supporting_asset_basis_closing_million +
                        kTolerance &&
                near(scenario.ending_eligible_basis_million, 0.0) &&
                near(scenario.ending_funded_protection_million, 2.0),
            "returned or disposed basis cannot leave phantom invested protection");
        check_exact_reconciliations(summary, scenario,
            "basis reduction and protection-cash reclassification reconcile exactly");
    }
}

void test_writeoff_absorption_is_same_month_and_complete() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        point_ambiguity("base-path");
    const cf::CapitalStackConfig stack = stack_fixture(10.0);

    cf::FundingBridgeConfig wrong_month = two_draw_bridge();
    wrong_month.scenario_performance.front().protection_absorptions.front()
        .month = 2U;
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack, wrong_month);
    }, "protection absorption must share the effective month of its referenced writeoff");

    cf::FundingBridgeConfig incomplete = two_draw_bridge();
    incomplete.scenario_performance.front().protection_absorptions.front()
        .amount_million = 1.0;
    expect_invalid([&] {
        (void)cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack, incomplete);
    }, "each eligible-basis writeoff requires complete protection absorption");

    cf::FundingBridgeConfig missing = two_draw_bridge();
    missing.scenario_performance.front().protection_absorptions.clear();
    expect_invalid([&] {
        (void)cf::evaluate_funding_bridge(portfolio, ambiguity,
            participation_fixture(), stack, missing);
    }, "a writeoff cannot omit its protection-absorption record");
}

void test_same_economic_group_hhi_aggregates_affiliates() {
    const cf::PortfolioConfig portfolio = one_draw_portfolio();
    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 1.0, 3U);
    bridge.providers[0U].economic_group_id = "capital-group";
    bridge.providers[1U].economic_group_id = "capital-group";
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.capital_call_requests = {
        {"group-call", "callable-facility", 0U, 4.0}};
    performance.capital_call_outcomes = {
        settled_outcome("group-call", 1U, 4.0)};
    performance.supplemental_receipts = {supplemental_receipt(
        "group-protection", "support-provider", 1U,
        cf::SupplementalFundingPurpose::ProtectionReplenishment, 2.0)};
    performance.eligible_basis_movements = {basis_movement("group-basis", 1U,
        cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
        "cellular-plant")};
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& capital_group = find_group(scenario, "capital-group");
    const auto& support_group = find_group(scenario, "support-group");
    check(scenario.feasible &&
            near(capital_group.cumulative_gross_settled_funding_million,
                6.0) &&
            near(capital_group.cumulative_gross_settled_funding_share,
                0.75) &&
            near(support_group.cumulative_gross_settled_funding_million,
                2.0) &&
            near(support_group.cumulative_gross_settled_funding_share,
                0.25) &&
            near(scenario.cumulative_gross_settled_funding_source_hhi,
                0.625),
        "providers in one economic group aggregate before HHI shares are squared");
    check(near(scenario.ending_economic_group_contingent_funding_dependency_hhi,
              0.0),
        "fully called and matured facilities leave no terminal contingent dependency");
}

void test_v01_rejects_live_evidence_overclaim() {
    const cf::PortfolioConfig portfolio = two_draw_portfolio();
    const cf::PortfolioAmbiguityConfig ambiguity =
        point_ambiguity("base-path");
    cf::FundingBridgeConfig bridge = two_draw_bridge();
    bridge.synthetic_inputs = false;
    expect_invalid([&] {
        cf::validate_funding_bridge_config(portfolio, ambiguity,
            participation_fixture(), stack_fixture(10.0), bridge);
    }, "v0.1 rejects a live-evidence claim because known-at facts, facility evidence, and enforceability are not validated");
}

void test_expiry_day_notice_retains_reserve_cost_until_due_outcome() {
    cf::PortfolioConfig portfolio = one_draw_portfolio();
    auto& project_path =
        portfolio.joint_scenarios.front().project_paths.front();
    project_path.investor_outlays.front().month = 3U;
    project_path.principal_movements.front().month = 3U;

    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 1.0, 4U);
    bridge.callable_facility.contractual_expiry_month = 1U;
    bridge.callable_facility.settlement_lag_months = 2U;
    bridge.callable_facility.liquidity_reserve_fraction = 0.5;
    bridge.callable_facility.annual_liquidity_hurdle_rate = 0.12;
    bridge.callable_facility.annual_reserve_yield_rate = 0.0;
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.capital_call_requests = {
        {"expiry-day-call", "callable-facility", 1U, 2.0}};
    performance.capital_call_outcomes = {
        settled_outcome("expiry-day-call", 3U, 2.0)};
    performance.supplemental_receipts = {supplemental_receipt(
        "expiry-day-protection", "support-provider", 3U,
        cf::SupplementalFundingPurpose::ProtectionReplenishment, 2.0)};
    performance.eligible_basis_movements = {basis_movement(
        "expiry-day-basis", 3U,
        cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
        "cellular-plant")};
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const double month_one_discount = std::pow(1.12, 1.0 / 12.0);
    const double month_two_discount = std::pow(1.12, 2.0 / 12.0);
    const double month_three_discount = std::pow(1.12, 3.0 / 12.0);
    const double expected_opportunity_cost_pv = 0.02 +
        0.02 / month_one_discount + 0.01 / month_two_discount +
        0.01 / month_three_discount;
    check(scenario.feasible &&
            near(scenario.months[0U]
                    .callable_liquidity_opportunity_cost_million,
                0.02) &&
            near(scenario.months[1U]
                    .callable_liquidity_opportunity_cost_million,
                0.02) &&
            near(scenario.months[2U]
                    .callable_liquidity_opportunity_cost_million,
                0.01) &&
            near(scenario.months[3U]
                    .callable_liquidity_opportunity_cost_million,
                0.01) &&
            near(scenario.callable_liquidity_opportunity_cost_pv_million,
                expected_opportunity_cost_pv),
        "an expiry-day notice retains its noncash reserve cost through the fixed due outcome");
    check(near(scenario.months[1U].callable_available_undrawn_million,
              0.0) &&
            near(scenario.months[1U].callable_called_unsettled_million,
                2.0) &&
            near(scenario.months[1U].callable_expired_uncalled_million,
                2.0) &&
            near(scenario.months[3U].capital_call_settled_million, 2.0) &&
            near(scenario.months[3U].callable_called_unsettled_million,
                0.0) &&
            near(scenario.ending_available_callable_commitment_million,
                0.0) &&
            near(scenario.expired_uncalled_callable_million, 2.0),
        "uncalled capacity expires at expiry close while a valid noticed amount remains due and settles later");
}

void test_callable_commitment_fee_suppresses_incomplete_npv_boundary() {
    cf::FundingBridgeConfig bridge = two_draw_bridge();
    bridge.callable_facility.annual_commitment_fee_rate = 0.12;
    auto& performance = bridge.scenario_performance.front();
    performance.supplemental_receipts.front().actual_cash_million = 0.58;
    performance.supplemental_receipts.push_back(supplemental_receipt(
        "close-fee-support", "support-provider", 0U,
        cf::SupplementalFundingPurpose::CostSupport, 0.08));
    performance.supplemental_receipts.push_back(supplemental_receipt(
        "second-fee-support", "support-provider", 2U,
        cf::SupplementalFundingPurpose::CostSupport, 0.04));

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        two_draw_portfolio(), point_ambiguity("base-path"),
        participation_fixture(), stack_fixture(10.0), bridge);
    const auto& scenario = summary.scenarios.front();
    check(scenario.feasible &&
            near(scenario.months[0U].callable_commitment_fee_due_million,
                0.08) &&
            near(scenario.months[1U].callable_commitment_fee_due_million,
                0.08) &&
            near(scenario.months[2U].callable_commitment_fee_due_million,
                0.04) &&
            near(scenario.total_callable_commitment_fees_million, 0.20),
        "positive callable commitment fees remain visible as exact paid cash costs");
    check(!scenario.investor_cash_npv_million.has_value() &&
            !scenario.fully_prefunded_baseline_npv_million.has_value() &&
            !scenario.cash_npv_change_vs_fully_prefunded_million.has_value() &&
            !scenario.economic_npv_change_after_callable_liquidity_cost_million
                 .has_value() &&
            !scenario.fully_prefunded_prefunding_drag_million.has_value() &&
            !summary.expected_investor_cash_npv_million.has_value() &&
            !summary.expected_cash_npv_change_vs_fully_prefunded_million
                 .has_value() &&
            !summary.expected_economic_npv_change_after_callable_liquidity_cost_million
                 .has_value(),
        "NPV outputs stay absent until the callable-fee recipient cash-flow boundary is modeled");
    check_exact_reconciliations(summary, scenario,
        "paid callable commitment fees preserve exact bridge ledgers");
}

void test_warehouse_only_shortfall_retains_warehouse_lineage() {
    const auto run_case = [](cf::FundingSettlementStatus status,
                              double actual_cash, double missing_cash,
                              std::string source_record_id) {
        cf::FundingBridgeConfig bridge = warehouse_bridge();
        auto& outcome = bridge.scenario_performance.front()
                            .warehouse_draw_outcomes.front();
        outcome.status = status;
        outcome.actual_cash_million = actual_cash;
        outcome.source_record_id = std::move(source_record_id);

        const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
            one_draw_portfolio(), point_ambiguity("warehouse-path"),
            participation_fixture(), stack_fixture(6.0), bridge);
        const auto& scenario = summary.scenarios.front();
        const auto& month = scenario.months[1U];
        check(!scenario.feasible &&
                scenario.failure_kind ==
                    cf::FundingBridgeFailureKind::FundingUseShortfall &&
                scenario.failure_phase ==
                    cf::FundingBridgeFailurePhase::AssetFundingUse &&
                near(month.acquisition_and_primary_funding_due_million,
                    6.0) &&
                near(month.acquisition_and_primary_funding_paid_million,
                    0.0) &&
                near(month.acquisition_and_primary_funding_unpaid_million,
                    6.0) &&
                near(month.acquisition_and_primary_funding_liquidity_gap_million,
                    missing_cash) &&
                near(scenario.warehouse_draw_defaulted_million,
                    missing_cash) &&
                near(scenario.callable_defaulted_million, 0.0) &&
                scenario.provider_settlements.size() == 1U &&
                scenario.provider_settlement_shortfalls.size() == 1U,
            "a deficient warehouse settlement and the contemporaneous eligible-use gap remain separately visible without a partial asset closing");
        if (scenario.provider_settlements.size() == 1U) {
            const auto& settlement = scenario.provider_settlements.front();
            check(settlement.settlement_month == 1U &&
                    settlement.status == status &&
                    settlement.provider_id == "warehouse-provider" &&
                    settlement.facility_id == "warehouse-facility" &&
                    settlement.request_id == "warehouse-draw" &&
                    settlement.source_record_id == outcome.source_record_id &&
                    near(settlement.requested_million, 4.0) &&
                    near(settlement.actual_cash_million, actual_cash) &&
                    near(settlement.missing_cash_million, missing_cash),
                "every final warehouse outcome retains its complete dated request and cash lineage");
        }
        check(month.warehouse_advance_tests.size() == 1U &&
                near(month.warehouse_advance_tests.front().requested_million,
                    4.0) &&
                near(month.warehouse_advance_tests.front().settled_million,
                    actual_cash) &&
                (actual_cash > 0.0
                        ? month.warehouse_advance_tests.front()
                              .funded_protection_test_applicable &&
                            month.warehouse_advance_tests.front()
                                .borrowing_base_test_applicable
                        : !month.warehouse_advance_tests.front()
                                  .funded_protection_test_applicable &&
                            !month.warehouse_advance_tests.front()
                                 .borrowing_base_test_applicable),
            "zero-cash failed outcomes disclose the request but do not claim funded protection or borrowing-base tests apply");
        check(scenario.failure.has_value() &&
                near(scenario.failure->amount_due_million, 6.0) &&
                near(scenario.failure->amount_available_million,
                    2.0 + actual_cash) &&
                near(scenario.failure->shortfall_million, missing_cash) &&
                near(scenario.failure->rejected_transaction_million, 6.0) &&
                near(scenario.failure->eligible_purpose_cash_available_million,
                    2.0 + actual_cash) &&
                scenario.failure->provider_id.empty() &&
                scenario.failure->facility_id.empty() &&
                scenario.failure->request_id.empty() &&
                scenario.failure->source_record_id.empty() &&
                scenario.failure->settlement_shortfalls.size() == 1U,
            "the asset-gap headline stays causally neutral while the warehouse shortfall remains explicit context");
        if (scenario.failure.has_value() &&
            scenario.failure->settlement_shortfalls.size() == 1U) {
            const auto& shortfall =
                scenario.failure->settlement_shortfalls.front();
            check(shortfall.settlement_month == 1U &&
                    shortfall.provider_id == "warehouse-provider" &&
                    shortfall.facility_id == "warehouse-facility" &&
                    shortfall.request_id == "warehouse-draw" &&
                    shortfall.source_record_id == outcome.source_record_id &&
                    near(shortfall.missing_cash_million, missing_cash) &&
                    shortfall.provider_id != "call-provider" &&
                    shortfall.facility_id != "callable-facility" &&
                    !find_provider(scenario, "call-provider")
                         .provider_nonperformance_observed &&
                    find_provider(scenario, "warehouse-provider")
                        .provider_nonperformance_observed,
                "warehouse-only nonperformance never acquires callable-provider lineage");
        }
    };

    run_case(cf::FundingSettlementStatus::FinalPartialSettlement, 3.0, 1.0,
        "warehouse-partial-outcome");
    run_case(cf::FundingSettlementStatus::Failed, 0.0, 4.0,
        "warehouse-failed-outcome");
}

void test_same_month_request_batches_are_all_or_none() {
    const auto warehouse_case = [](bool reverse_ids) {
        cf::FundingBridgeConfig bridge = bridge_terms(4.0, 5.0, 4U);
        cf::FundingBridgeScenarioPerformance performance;
        performance.scenario_id = "warehouse-path";
        performance.capital_call_requests = {
            {"valid-call", "callable-facility", 0U, 1.0}};
        performance.capital_call_outcomes = {
            settled_outcome("valid-call", 1U, 1.0)};
        const std::string two_id = reverse_ids ? "z-two" : "a-two";
        const std::string four_id = reverse_ids ? "a-four" : "z-four";
        performance.warehouse_draw_requests = {
            {two_id, "warehouse-facility", 0U, 2.0},
            {four_id, "warehouse-facility", 0U, 4.0},
        };
        performance.warehouse_draw_outcomes = {
            settled_outcome(two_id, 1U, 2.0),
            settled_outcome(four_id, 1U, 4.0),
        };
        bridge.scenario_performance = {std::move(performance)};
        return cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), bridge);
    };
    const cf::FundingBridgeSummary first_warehouse = warehouse_case(false);
    const cf::FundingBridgeSummary second_warehouse = warehouse_case(true);
    for (const cf::FundingBridgeSummary* summary :
        {&first_warehouse, &second_warehouse}) {
        const auto& scenario = summary->scenarios.front();
        check(!scenario.feasible &&
                scenario.failure_kind ==
                    cf::FundingBridgeFailureKind::WarehouseLimitExceeded &&
                scenario.failure.has_value() &&
                scenario.failure->request_id.empty() &&
                near(scenario.failure->amount_due_million, 6.0) &&
                near(scenario.failure->amount_available_million, 5.0) &&
                near(scenario.ending_called_unsettled_warehouse_million,
                    0.0) &&
                near(scenario.ending_available_callable_commitment_million,
                    4.0) &&
                near(scenario.ending_called_unsettled_callable_million, 0.0) &&
                near(scenario.total_callable_commitment_fees_million, 0.0) &&
                scenario.provider_settlements.empty() &&
                scenario.provider_requests.size() == 3U &&
                scenario.provider_requests.front().kind ==
                    cf::FundingRequestKind::CallableCapital &&
                scenario.provider_requests.front().provider_id ==
                    "call-provider" &&
                scenario.provider_requests.front().facility_id ==
                    "callable-facility" &&
                scenario.provider_requests.front().request_id ==
                    "valid-call" &&
                near(scenario.provider_requests.front().requested_million,
                    1.0) &&
                std::none_of(scenario.provider_requests.begin(),
                    scenario.provider_requests.end(),
                    [](const auto& request) { return request.accepted; }) &&
                scenario.months[0U].warehouse_advance_tests.size() == 2U &&
                std::all_of(
                    scenario.months[0U].warehouse_advance_tests.begin(),
                    scenario.months[0U].warehouse_advance_tests.end(),
                    [](const auto& test) {
                        return near(
                                   test.prospective_funded_and_contingent_dependency_million,
                                   6.0) &&
                            !test.facility_limit_passed && !test.passed;
                    }),
            "an over-limit same-month warehouse batch rejects every request without ID-order priority or partial contingent state");
    }

    cf::FundingBridgeConfig invalid_call = warehouse_bridge();
    invalid_call.callable_facility.availability_start_month = 1U;
    invalid_call.scenario_performance.front().capital_call_requests = {
        {"unavailable-call", "callable-facility", 0U, 1.0}};
    invalid_call.scenario_performance.front().capital_call_outcomes = {
        settled_outcome("unavailable-call", 1U, 1.0)};
    const cf::FundingBridgeSummary invalid_call_summary =
        cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), invalid_call);
    const auto& invalid_call_path = invalid_call_summary.scenarios.front();
    check(!invalid_call_path.feasible &&
            invalid_call_path.failure_kind ==
                cf::FundingBridgeFailureKind::CallableFacilityUnavailable &&
            near(invalid_call_path.ending_called_unsettled_warehouse_million,
                0.0) &&
            invalid_call_path.months[0U].warehouse_advance_tests.size() == 1U &&
            invalid_call_path.months[0U]
                .warehouse_advance_tests.front()
                .passed &&
            invalid_call_path.provider_requests.size() == 2U &&
            std::none_of(invalid_call_path.provider_requests.begin(),
                invalid_call_path.provider_requests.end(),
                [](const auto& request) { return request.accepted; }) &&
            invalid_call_path.provider_settlements.empty(),
        "an invalid callable side likewise cannot leave the otherwise valid warehouse half committed");

    cf::FundingBridgeConfig both_invalid = invalid_call;
    both_invalid.warehouse_facility.committed_limit_million = 3.0;
    const cf::FundingBridgeSummary both_invalid_summary =
        cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), both_invalid);
    const auto& both_invalid_path = both_invalid_summary.scenarios.front();
    check(!both_invalid_path.feasible &&
            both_invalid_path.failure_kind ==
                cf::FundingBridgeFailureKind::SimultaneousFacilityRequestFailure &&
            both_invalid_path.failure.has_value() &&
            both_invalid_path.failure->provider_id.empty() &&
            both_invalid_path.failure->facility_id.empty() &&
            both_invalid_path.failure->causal_source_record_ids.size() == 2U &&
            near(both_invalid_path.ending_called_unsettled_callable_million,
                0.0) &&
            near(both_invalid_path.ending_called_unsettled_warehouse_million,
                0.0),
        "simultaneous call and warehouse request failures receive one neutral batch classification with both facility lineages");
}

void test_stop_at_first_failure_closes_unreached_due_fields() {
    cf::PortfolioConfig portfolio = two_draw_portfolio();
    auto& path = portfolio.joint_scenarios.front().project_paths.front();
    path.investor_outlays[0U].month = 0U;
    path.investor_outlays[1U].month = 0U;
    path.principal_movements[0U].month = 0U;
    portfolio.joint_scenarios.front().pool_costs.front().month = 0U;

    cf::FundingBridgeConfig bridge = two_draw_bridge();
    bridge.callable_facility.availability_start_month = 1U;
    bridge.scenario_performance.front()
        .eligible_basis_movements.front()
        .month = 0U;
    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("base-path"), participation_fixture(),
        stack_fixture(10.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& month = scenario.months[0U];
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::CallableFacilityUnavailable &&
            near(month.buyer_direct_cost_due_million, 0.0) &&
            near(month.pool_cost_due_million, 0.0) &&
            near(month.total_nonasset_cost_due_million, 0.0) &&
            near(month.total_nonasset_cost_paid_million, 0.0) &&
            near(month.total_nonasset_cost_unpaid_million, 0.0) &&
            near(month.acquisition_and_primary_funding_due_million, 0.0) &&
            near(month.acquisition_and_primary_funding_paid_million, 0.0) &&
            near(month.acquisition_and_primary_funding_unpaid_million, 0.0) &&
            near(scenario.total_buyer_direct_cost_uses_million, 0.0) &&
            near(scenario.total_pool_cost_uses_million, 0.0) &&
            near(scenario.total_acquisition_and_primary_funding_uses_million,
                0.0),
        "a request-phase failure does not report later scheduled cost or asset uses as due without paid/unpaid closure");
}

void test_simultaneous_provider_capacity_breaches_are_order_invariant() {
    const auto run_case = [](bool reverse_providers) {
        cf::FundingBridgeConfig bridge = bridge_terms(4.0, 4.0, 4U);
        for (auto& provider_row : bridge.providers) {
            if (provider_row.id == "call-provider") {
                provider_row.declared_capacity_million = 3.0;
                provider_row.source_record_id = "call-capacity-record";
            } else if (provider_row.id == "warehouse-provider") {
                provider_row.declared_capacity_million = 3.0;
                provider_row.source_record_id = "warehouse-capacity-record";
            }
        }
        if (reverse_providers) {
            std::reverse(bridge.providers.begin(), bridge.providers.end());
        }
        cf::FundingBridgeScenarioPerformance performance;
        performance.scenario_id = "warehouse-path";
        bridge.scenario_performance = {std::move(performance)};
        return cf::evaluate_funding_bridge(one_draw_portfolio(),
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), bridge);
    };
    const cf::FundingBridgeSummary first = run_case(false);
    const cf::FundingBridgeSummary second = run_case(true);
    for (const cf::FundingBridgeSummary* summary : {&first, &second}) {
        const auto& scenario = summary->scenarios.front();
        check(!scenario.feasible &&
                scenario.failure_kind ==
                    cf::FundingBridgeFailureKind::ProviderCapacityExceeded &&
                scenario.failure.has_value() &&
                scenario.failure->provider_id.empty() &&
                scenario.failure->source_record_id.empty() &&
                near(scenario.failure->amount_due_million, 8.0) &&
                near(scenario.failure->amount_available_million, 6.0) &&
                scenario.failure->provider_capacity_breaches.size() == 2U &&
                scenario.failure->provider_capacity_breaches[0U].provider_id ==
                    "call-provider" &&
                scenario.failure->provider_capacity_breaches[1U].provider_id ==
                    "warehouse-provider" &&
                scenario.failure->causal_source_record_ids.size() == 2U,
            "simultaneous provider-capacity breaches retain a sorted vector and neutral headline independent of provider row order");
    }
}

void test_cross_month_provider_shortfall_is_context_not_invented_causality() {
    cf::PortfolioConfig portfolio = one_draw_portfolio();
    auto& project_path =
        portfolio.joint_scenarios.front().project_paths.front();
    project_path.investor_outlays.front().month = 2U;
    project_path.principal_movements.front().month = 2U;

    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 0.0, 4U);
    for (auto& provider_row : bridge.providers) {
        if (provider_row.id == "warehouse-provider") {
            provider_row.declared_capacity_million = 1.0;
        }
    }
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.capital_call_requests = {
        {"early-call", "callable-facility", 0U, 4.0}};
    cf::FundingSettlementOutcome partial =
        settled_outcome("early-call", 1U, 4.0);
    partial.status = cf::FundingSettlementStatus::FinalPartialSettlement;
    partial.actual_cash_million = 3.0;
    partial.source_record_id = "early-call-partial-record";
    performance.capital_call_outcomes = {partial};
    performance.eligible_basis_movements = {
        basis_movement("later-basis-add", 2U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant")};
    bridge.scenario_performance = {performance};

    const cf::FundingBridgeSummary failed_summary =
        cf::evaluate_funding_bridge(portfolio,
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), bridge);
    const auto& failed = failed_summary.scenarios.front();
    check(!failed.feasible &&
            failed.first_infeasible_month ==
                std::optional<std::size_t>{2U} &&
            failed.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            failed.provider_settlements.size() == 1U &&
            failed.provider_settlement_shortfalls.size() == 1U &&
            failed.failure.has_value() &&
            failed.failure->settlement_shortfalls.size() == 1U &&
            failed.failure->provider_id.empty() &&
            failed.failure->facility_id.empty() &&
            failed.failure->request_id.empty(),
        "a prior-month provider default remains fully disclosed without inventing but-for attribution for a later gap");
    if (failed.failure.has_value() &&
        failed.failure->settlement_shortfalls.size() == 1U) {
        const auto& context = failed.failure->settlement_shortfalls.front();
        check(context.settlement_month == 1U &&
                context.provider_id == "call-provider" &&
                context.facility_id == "callable-facility" &&
                context.request_id == "early-call" &&
                context.source_record_id == "early-call-partial-record" &&
                near(context.missing_cash_million, 1.0),
            "cross-month failure context retains the original dated provider, facility, request, and outcome source");
    }

    cf::FundingBridgeConfig cured = bridge;
    cured.scenario_performance.front().supplemental_receipts.push_back(
        supplemental_receipt("explicit-replacement", "support-provider", 2U,
            cf::SupplementalFundingPurpose::ProtectionReplenishment, 1.0));
    const cf::FundingBridgeSummary cured_summary =
        cf::evaluate_funding_bridge(portfolio,
            point_ambiguity("warehouse-path"), participation_fixture(),
            stack_fixture(6.0), cured);
    const auto& cured_path = cured_summary.scenarios.front();
    check(cured_path.feasible &&
            cured_path.provider_settlements.size() == 1U &&
            cured_path.provider_settlement_shortfalls.size() == 1U &&
            near(cured_path.callable_defaulted_million, 1.0) &&
            near(cured_path.total_protection_replenishment_receipts_million,
                1.0),
        "explicit dated replacement cash may cure liquidity without erasing the earlier provider event ledger");
    check_exact_reconciliations(cured_summary, cured_path,
        "the explicit cross-month cure preserves every bridge ledger");
}

void test_same_month_default_does_not_invent_asset_gap_causality() {
    cf::PortfolioConfig portfolio = one_draw_portfolio();
    portfolio.projects.front().commitment_million = 7.0;
    portfolio.projects.front().principal_limit_million = 7.0;
    auto& path = portfolio.joint_scenarios.front().project_paths.front();
    path.investor_outlays.push_back(
        {2U, cf::InvestorOutlayPurpose::PrimaryProjectFunding, 1.0});
    path.principal_movements.push_back(
        {2U, cf::PrincipalMovementKind::FundedPrincipalAddition, 1.0});

    cf::FundingBridgeConfig bridge = bridge_terms(5.0, 4.0, 4U);
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.warehouse_draw_requests = {
        {"opening-warehouse", "warehouse-facility", 0U, 4.0}};
    performance.warehouse_draw_outcomes = {
        settled_outcome("opening-warehouse", 1U, 4.0)};
    performance.capital_call_requests = {
        {"failed-takeout-call", "callable-facility", 1U, 4.0}};
    performance.capital_call_outcomes = {
        failed_outcome("failed-takeout-call", 2U)};
    performance.capital_call_outcomes.front().source_record_id =
        "failed-takeout-call-record";
    performance.eligible_basis_movements = {
        basis_movement("opening-basis", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant"),
        basis_movement("later-basis", 2U,
            cf::EligibleBasisMovementKind::EligibleAddition, 1.0,
            "cellular-plant"),
    };
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        portfolio, point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(7.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& month = scenario.months[2U];
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            scenario.failure.has_value() &&
            scenario.failure->provider_id.empty() &&
            scenario.failure->facility_id.empty() &&
            scenario.failure->request_id.empty() &&
            near(scenario.failure->shortfall_million, 1.0) &&
            scenario.failure->settlement_shortfalls.size() == 1U &&
            near(scenario.callable_defaulted_million, 4.0) &&
            near(scenario.ending_warehouse_principal_million, 4.0) &&
            near(month.warehouse_principal_repaid_from_call_settlement_million,
                0.0),
        "a same-month failed call stays disclosure context when its counterfactual cash would first repay opening warehouse debt rather than cure the asset gap");
}

void test_same_month_settlement_batch_retains_cash_lineage_and_charges() {
    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 6.0, 4U);
    bridge.warehouse_facility.annual_interest_rate = 0.12;
    bridge.warehouse_facility.advance_fee_rate = 0.01;
    bridge.warehouse_facility.upfront_fee_rate = 0.02;
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.warehouse_draw_requests = {
        {"a-large-draw", "warehouse-facility", 0U, 5.0},
        {"z-later-draw", "warehouse-facility", 0U, 1.0},
    };
    performance.warehouse_draw_outcomes = {
        settled_outcome("a-large-draw", 1U, 5.0),
        settled_outcome("z-later-draw", 1U, 1.0),
    };
    performance.warehouse_draw_outcomes[0U].source_record_id =
        "large-settlement-record";
    performance.warehouse_draw_outcomes[1U].source_record_id =
        "later-settlement-record";
    performance.supplemental_receipts = {
        supplemental_receipt("opening-fee-support", "support-provider", 0U,
            cf::SupplementalFundingPurpose::CostSupport, 0.12),
        supplemental_receipt("same-month-support", "support-provider", 1U,
            cf::SupplementalFundingPurpose::CostSupport, 0.5),
    };
    performance.eligible_basis_movements = {
        basis_movement("basis-add", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant")};
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        one_draw_portfolio(), point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& month = scenario.months[1U];
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseBorrowingBaseDeficit &&
            scenario.provider_settlements.size() == 2U &&
            near(scenario.total_warehouse_advances_million, 6.0) &&
            near(scenario.total_cost_support_receipts_million, 0.62) &&
            near(month.warehouse_advance_settled_million, 6.0) &&
            near(month.cost_support_receipts_million, 0.5) &&
            month.warehouse_advance_tests.size() == 2U &&
            scenario.failure.has_value() &&
            scenario.failure->request_id.empty() &&
            scenario.failure->source_record_id.empty() &&
            scenario.failure->causal_source_record_ids.size() == 3U,
        "every same-month outcome and supplemental receipt is booked before one aggregate warehouse test");
    check(near(month.warehouse_interest_due_million, 0.06) &&
            near(month.warehouse_advance_fee_due_million, 0.06) &&
            near(month.warehouse_upfront_fee_due_million, 0.0) &&
            near(month.warehouse_charges_unpaid_million, 0.12) &&
            near(month.total_nonasset_cost_due_million, 0.12) &&
            near(month.total_nonasset_cost_paid_million, 0.0) &&
            near(month.total_nonasset_cost_unpaid_million, 0.12) &&
            near(month.acquisition_and_primary_funding_due_million, 0.0) &&
            near(month.acquisition_and_primary_funding_paid_million, 0.0) &&
            near(month.acquisition_and_primary_funding_unpaid_million, 0.0) &&
            near(scenario.total_warehouse_interest_million, 0.06) &&
            near(scenario.total_warehouse_fees_million, 0.18) &&
            near(scenario.ending_warehouse_unpaid_charges_million, 0.12) &&
            near(scenario.ending_warehouse_funded_ead_million, 6.12),
        "interest and fees crystallized on settled cash remain unpaid EAD after the batch covenant failure");
    check_exact_reconciliations(summary, scenario,
        "the failed aggregate settlement batch preserves cash, debt, charge, basis, and protection identities");
}

void test_failure_snapshot_precedes_expiry_close() {
    cf::FundingBridgeConfig bridge = bridge_terms(4.0, 5.0, 4U);
    bridge.callable_facility.contractual_expiry_month = 1U;
    bridge.warehouse_facility.availability_end_month = 1U;
    cf::FundingBridgeScenarioPerformance performance;
    performance.scenario_id = "warehouse-path";
    performance.capital_call_requests = {
        {"partial-policy-call", "callable-facility", 0U, 2.0}};
    performance.capital_call_outcomes = {
        settled_outcome("partial-policy-call", 1U, 2.0)};
    performance.eligible_basis_movements = {
        basis_movement("basis-add", 1U,
            cf::EligibleBasisMovementKind::EligibleAddition, 6.0,
            "cellular-plant")};
    bridge.scenario_performance = {std::move(performance)};

    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        one_draw_portfolio(), point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    check(!scenario.feasible &&
            scenario.first_infeasible_month ==
                std::optional<std::size_t>{1U} &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::FundingUseShortfall &&
            near(scenario.ending_available_callable_commitment_million, 2.0) &&
            near(scenario.expired_uncalled_callable_million, 0.0) &&
            near(find_provider(scenario, "warehouse-provider")
                    .ending_warehouse_contingent_commitment_million,
                5.0) &&
            near(scenario
                    .ending_economic_group_contingent_funding_dependency_hhi,
                29.0 / 49.0),
        "a failure inside the final availability month measures open commitments before close-of-month expiry mutation");
}

void test_maturity_waterfall_uses_protection_cash_and_reports_gross_due() {
    cf::FundingBridgeConfig bridge = warehouse_bridge();
    auto& performance = bridge.scenario_performance.front();
    performance.warehouse_draw_requests.front().requested_million = 3.0;
    performance.warehouse_draw_outcomes.front().actual_cash_million = 3.0;
    performance.capital_call_requests = {
        {"asset-call", "callable-facility", 0U, 1.0}};
    performance.capital_call_outcomes = {
        settled_outcome("asset-call", 1U, 1.0)};
    bridge.scenario_performance.front().supplemental_receipts.push_back(
        supplemental_receipt("late-protection", "support-provider", 3U,
            cf::SupplementalFundingPurpose::ProtectionReplenishment, 1.0));
    const cf::FundingBridgeSummary summary = cf::evaluate_funding_bridge(
        one_draw_portfolio(), point_ambiguity("warehouse-path"),
        participation_fixture(), stack_fixture(6.0), bridge);
    const auto& scenario = summary.scenarios.front();
    const auto& maturity = scenario.months[4U];
    check(!scenario.feasible &&
            scenario.failure_kind ==
                cf::FundingBridgeFailureKind::WarehouseMaturityUnpaid &&
            near(maturity.warehouse_principal_repaid_from_protection_cash_million,
                1.0) &&
            near(maturity.warehouse_past_due_principal_million, 2.0) &&
            near(scenario.ending_warehouse_principal_million, 2.0) &&
            near(scenario.ending_warehouse_funded_ead_million, 2.0) &&
            near(maturity.closing_qualifying_protection_cash_million, 0.0) &&
            near(maturity.qualifying_protection_invested_basis_closing_million,
                3.0),
        "segregated protection cash pays senior warehouse principal at maturity and becomes supported invested protection");
    check(scenario.failure.has_value() &&
            near(scenario.failure->amount_due_million, 3.0) &&
            near(scenario.failure->amount_available_million, 1.0) &&
            near(scenario.failure->shortfall_million, 2.0),
        "maturity failure separates the gross obligation, cash applied, and residual EAD");
    check_exact_reconciliations(summary, scenario,
        "partial maturity repayment preserves the complete bridge ledgers");
}

void test_same_month_protection_releases_are_atomic() {
    const auto run_case = [](bool reverse_ids) {
        cf::FundingBridgeConfig bridge = two_draw_bridge();
        const std::string small_id =
            reverse_ids ? "z-small-release" : "a-small-release";
        const std::string large_id =
            reverse_ids ? "a-large-release" : "z-large-release";
        bridge.scenario_performance.front().protection_releases = {
            {small_id, 0U, 0.5, "small-release-record"},
            {large_id, 0U, 2.0, "large-release-record"},
        };
        return cf::evaluate_funding_bridge(two_draw_portfolio(),
            point_ambiguity("base-path"), participation_fixture(),
            stack_fixture(10.0), bridge);
    };
    const cf::FundingBridgeSummary first = run_case(false);
    const cf::FundingBridgeSummary second = run_case(true);
    for (const cf::FundingBridgeSummary* summary : {&first, &second}) {
        const auto& scenario = summary->scenarios.front();
        const auto& month = scenario.months[0U];
        check(!scenario.feasible &&
                scenario.failure_kind ==
                    cf::FundingBridgeFailureKind::ProtectionReleaseShortfall &&
                scenario.failure.has_value() &&
                scenario.failure->request_id.empty() &&
                scenario.failure->source_record_id.empty() &&
                near(scenario.failure->amount_due_million, 2.5) &&
                near(scenario.failure->amount_available_million, 2.0) &&
                scenario.failure->causal_source_record_ids.size() == 2U &&
                near(month.protection_release_million, 0.0) &&
                near(month.funded_protection_released_million, 0.0) &&
                near(month.funded_protection_closing_million, 2.0) &&
                near(month.closing_qualifying_protection_cash_million, 2.0),
            "same-month protection releases pass or fail as one batch, so row IDs cannot select a partial release");
    }
}

void test_scenario_and_row_permutation_invariance() {
    cf::PortfolioConfig first_portfolio = two_provider_state_portfolio();
    cf::PortfolioAmbiguityConfig first_ambiguity = two_scenario_ambiguity();
    cf::FundingBridgeConfig first_bridge = two_provider_state_bridge(false);

    cf::PortfolioConfig second_portfolio = first_portfolio;
    cf::PortfolioAmbiguityConfig second_ambiguity = first_ambiguity;
    cf::FundingBridgeConfig second_bridge = first_bridge;
    std::reverse(second_portfolio.joint_scenarios.begin(),
        second_portfolio.joint_scenarios.end());
    std::reverse(second_ambiguity.scenario_probabilities.begin(),
        second_ambiguity.scenario_probabilities.end());
    std::reverse(second_bridge.providers.begin(), second_bridge.providers.end());
    std::reverse(second_bridge.scenario_performance.begin(),
        second_bridge.scenario_performance.end());
    for (auto& scenario : second_bridge.scenario_performance) {
        std::reverse(scenario.capital_call_requests.begin(),
            scenario.capital_call_requests.end());
        std::reverse(scenario.capital_call_outcomes.begin(),
            scenario.capital_call_outcomes.end());
        std::reverse(scenario.eligible_basis_movements.begin(),
            scenario.eligible_basis_movements.end());
    }
    for (auto& scenario : second_portfolio.joint_scenarios) {
        auto& path = scenario.project_paths.front();
        std::reverse(path.investor_outlays.begin(), path.investor_outlays.end());
        std::reverse(path.principal_movements.begin(),
            path.principal_movements.end());
    }

    const cf::FundingBridgeSummary first = cf::evaluate_funding_bridge(
        first_portfolio, first_ambiguity, participation_fixture(),
        stack_fixture(6.0), first_bridge);
    const cf::FundingBridgeSummary second = cf::evaluate_funding_bridge(
        second_portfolio, second_ambiguity, participation_fixture(),
        stack_fixture(6.0), second_bridge);
    bool invariant = first.monthly_path_nonanticipativity_validated ==
            second.monthly_path_nonanticipativity_validated &&
        near(first.funding_failure_probability.central,
            second.funding_failure_probability.central) &&
        near(first.expected_funding_shortfall_million.central,
            second.expected_funding_shortfall_million.central) &&
        first.scenarios.size() == second.scenarios.size();
    for (const auto& first_path : first.scenarios) {
        const auto& second_path = find_scenario(second, first_path.scenario_id);
        invariant = invariant && first_path.feasible == second_path.feasible &&
            first_path.investor_cash_npv_million.has_value() &&
            second_path.investor_cash_npv_million.has_value();
        if (first_path.investor_cash_npv_million.has_value() &&
            second_path.investor_cash_npv_million.has_value()) {
            invariant = invariant &&
                near(*first_path.investor_cash_npv_million,
                    *second_path.investor_cash_npv_million);
        }
        invariant = invariant &&
            near(first_path.total_call_requested_million,
                second_path.total_call_requested_million) &&
            near(first_path.total_actual_call_receipts_million,
                second_path.total_actual_call_receipts_million) &&
            near(first_path.ending_eligible_basis_million,
                second_path.ending_eligible_basis_million) &&
            near(first_path.ending_funded_protection_million,
                second_path.ending_funded_protection_million);
    }
    check(invariant,
        "scenario, provider, request, outcome, outlay, and basis row order cannot change results");
}

} // namespace

int main() {
    try {
        test_explicit_cost_support_and_no_automatic_cure();
        test_requests_are_not_cash_and_partial_outcomes_remain_visible();
        test_terminal_warehouse_exposure_is_retained_without_inferred_loss();
        test_protection_absorption_and_eligibility_reduction_fail_separately();
        test_same_month_receipt_cannot_cure_an_earlier_use();
        test_nonanticipativity_and_mixed_feasibility_suppress_expected_npv();
        test_facility_availability_provider_capacity_and_takeout_boundary();
        test_frictionless_calls_and_liquidity_opportunity_cost();
        test_frictionless_on_demand_calls_equal_prefunding_drag();
        test_every_request_has_exactly_one_final_outcome();
        test_partitioned_same_month_actions_fail_nonanticipativity();
        test_request_phase_separates_contingent_limit_from_funded_tests();
        test_warehouse_settlement_cannot_cross_legal_maturity();
        test_failed_warehouse_capacity_is_burned_not_reused();
        test_partial_cost_support_keeps_atomic_use_and_separate_gap();
        test_restricted_source_asset_failure_is_atomic();
        test_basis_returns_and_dispositions_cannot_create_phantom_protection();
        test_writeoff_absorption_is_same_month_and_complete();
        test_same_economic_group_hhi_aggregates_affiliates();
        test_v01_rejects_live_evidence_overclaim();
        test_expiry_day_notice_retains_reserve_cost_until_due_outcome();
        test_callable_commitment_fee_suppresses_incomplete_npv_boundary();
        test_warehouse_only_shortfall_retains_warehouse_lineage();
        test_same_month_request_batches_are_all_or_none();
        test_stop_at_first_failure_closes_unreached_due_fields();
        test_simultaneous_provider_capacity_breaches_are_order_invariant();
        test_cross_month_provider_shortfall_is_context_not_invented_causality();
        test_same_month_default_does_not_invent_asset_gap_causality();
        test_same_month_settlement_batch_retains_cash_lineage_and_charges();
        test_failure_snapshot_precedes_expiry_close();
        test_maturity_waterfall_uses_protection_cash_and_reports_gross_due();
        test_same_month_protection_releases_are_atomic();
        test_scenario_and_row_permutation_invariance();
    } catch (const std::exception& error) {
        std::cerr << "UNEXPECTED EXCEPTION: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " funding-bridge test(s) failed\n";
        return 1;
    }
    std::cout << "funding-bridge tests passed\n";
    return 0;
}
