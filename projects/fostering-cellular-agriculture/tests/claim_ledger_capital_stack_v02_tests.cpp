// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack.hpp>
#include <naturalehia/cellular_finance/claim_ledger_joint_portfolio.hpp>
#include <naturalehia/cellular_finance/claim_ledger_package.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
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
    double first, double second, double tolerance = 1.0e-9) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] cf::ClaimLedgerPortfolioAdapterTerms adapter_terms(
    std::string project_id, std::string source_prefix,
    std::string ordinary_counterparty) {
    cf::ClaimLedgerPortfolioAdapterTerms terms;
    terms.project_id = std::move(project_id);
    terms.stage = cf::ProjectStage::FirstIndustrial;
    terms.annual_physical_hurdle_rate = 0.10;

    const std::string fee = source_prefix + ".fee";
    const std::string operations = source_prefix + ".operations";
    const std::string recovery = source_prefix + ".recovery";
    const std::string provider = source_prefix + ".provider";
    terms.receipt_source_allocations = {
        {std::nullopt, "closing-investor-cash-fee", fee,
            cf::PortfolioCashSource::FinancingFee, 0.3},
        {"performing-maturity", "success-principal-cash", operations,
            cf::PortfolioCashSource::Commercial, 10.0},
        {"performing-maturity", "success-interest-cash", operations,
            cf::PortfolioCashSource::Commercial, 1.0},
        {"failure-with-provider", "failure-recovery-principal-cash",
            recovery, cf::PortfolioCashSource::Recovery, 2.0},
        {"failure-with-provider", "failure-guarantee-principal-cash",
            provider, cf::PortfolioCashSource::ExplicitSupport, 4.0},
    };
    terms.cash_source_budgets = {
        {std::nullopt, fee, cf::PortfolioCashSource::FinancingFee, 0U, 0.3,
            ordinary_counterparty, std::nullopt},
        {"performing-maturity", operations,
            cf::PortfolioCashSource::Commercial, 12U, 11.0,
            ordinary_counterparty, std::nullopt},
        {"failure-with-provider", recovery,
            cf::PortfolioCashSource::Recovery, 12U, 2.0,
            ordinary_counterparty, std::nullopt},
        {"failure-with-provider", provider,
            cf::PortfolioCashSource::ExplicitSupport, 12U, 4.0,
            "synthetic-catalytic-provider",
            std::optional<std::string>{"synthetic-provider-claim"}},
    };
    const std::string suffix = terms.project_id == "claim-a" ? "a" : "b";
    terms.scenario_factor_sets = {
        {"performing-maturity", {"facility-" + suffix + "-perform"}},
        {"failure-with-provider", {"facility-" + suffix + "-fail"}},
    };
    return terms;
}

[[nodiscard]] std::vector<cf::ClaimLedgerJointPortfolioAssetInput>
asset_inputs(const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    return {
        {first, adapter_terms("claim-a", "claim-a",
                    "synthetic-project-obligor")},
        {second, adapter_terms("claim-b", "claim-b",
                     "synthetic-peer-project-obligor")},
    };
}

[[nodiscard]] cf::ClaimLedgerJointPortfolioScenario joint_state(
    std::string id, double probability, std::string first_state,
    std::string second_state, std::string factor) {
    cf::ClaimLedgerJointPortfolioScenario result;
    result.scenario_id = std::move(id);
    result.physical_probability = probability;
    result.probability_basis_id = "declared-coupling-v0.1";
    result.factor_tags = {std::move(factor)};
    result.selections = {
        {"claim-a", std::move(first_state)},
        {"claim-b", std::move(second_state)},
    };
    return result;
}

[[nodiscard]] cf::ClaimLedgerJointPortfolioTerms joint_terms() {
    cf::ClaimLedgerJointPortfolioTerms terms;
    terms.scenario_label =
        "Two verified synthetic claims entering capital-stack v0.2";
    terms.source_note =
        "Hand-reconciled coupling; no empirical probability or fair-value claim";
    terms.joint_scenarios = {
        joint_state("both-perform", 0.6, "performing-maturity",
            "performing-maturity", "joint-demand-up"),
        joint_state("a-perform-b-fail", 0.2, "performing-maturity",
            "failure-with-provider", "joint-b-specific-failure"),
        joint_state("a-fail-b-perform", 0.1, "failure-with-provider",
            "performing-maturity", "joint-a-specific-failure"),
        joint_state("both-fail", 0.1, "failure-with-provider",
            "failure-with-provider", "joint-common-failure"),
    };
    return terms;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig point_ambiguity(
    const cf::ClaimLedgerJointPortfolioResult& assembled) {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label =
        "Exact assembled joint probabilities for integration testing";
    ambiguity.source_note =
        "Point bounds copied from the assembler's authoritative measure";
    ambiguity.scenario_probabilities.reserve(
        assembled.joint_scenario_lineage.size());
    for (const cf::ClaimLedgerJointScenarioLineage& row :
         assembled.joint_scenario_lineage) {
        ambiguity.scenario_probabilities.push_back(
            {row.joint_scenario_id, row.physical_probability,
                row.physical_probability, row.physical_probability});
    }
    return ambiguity;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_terms() {
    cf::SuccessParticipationConfig terms;
    terms.scenario_label =
        "Verified-claim capital-stack success participation";
    terms.source_note =
        "Synthetic assertion that declared commercial non-principal cash is scalable";
    terms.selected_nonprincipal_cash_is_contractually_scalable = true;
    terms.scalable_source_kinds = {cf::PortfolioCashSource::Commercial};
    return terms;
}

[[nodiscard]] cf::CapitalStackConfig v02_stack_terms() {
    cf::CapitalStackConfig terms;
    terms.model_version = std::string(cf::kCapitalStackModelVersion);
    terms.scenario_label =
        "Verified-claim asset-to-liability capital stack";
    terms.source_note =
        "Synthetic junior and priority terms for accounting integration testing";
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
    terms.tranches = {
        {"junior", 0.0, 6.0, 0.0, 0.10, true},
        {"priority", 6.0, 18.0, 0.0, 0.10, false},
    };
    return terms;
}

[[nodiscard]] cf::CapitalStackConfig v01_stack_terms(
    double aggregate_project_outlay_limit) {
    cf::CapitalStackConfig terms;
    terms.model_version = std::string(cf::kCapitalStackLegacyModelVersion);
    terms.scenario_label = "Legacy explicit-ledger accounting boundary";
    terms.source_note = "Synthetic fail-closed integration test";
    terms.aggregate_commitment_is_fully_funded_at_par_at_month_zero = true;
    terms.subscription_reserve_is_zero_yield_and_lossless = true;
    terms.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    terms.pool_costs_are_additional_pro_rata_calls = true;
    terms.principal_cash_is_paid_most_senior_first = true;
    terms.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    terms.tranching_does_not_change_project_cash_or_gross_loss = true;
    terms.underlying_success_participation_fraction = 1.0;
    terms.tranches = {
        {"junior", 0.0, 6.0, 0.0, 0.10, true},
        {"priority", 6.0, aggregate_project_outlay_limit, 0.0, 0.10,
            false},
    };
    return terms;
}

[[nodiscard]] const cf::CapitalStackScenarioResult& stack_scenario(
    const cf::CapitalStackSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const auto& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error("integration-test stack scenario is missing");
    }
    return *found;
}

[[nodiscard]] const cf::JointScenarioResult& portfolio_scenario(
    const cf::ClaimLedgerJointPortfolioResult& assembled,
    std::string_view id) {
    const auto found = std::find_if(
        assembled.portfolio_summary.scenarios.begin(),
        assembled.portfolio_summary.scenarios.end(),
        [id](const auto& scenario) { return scenario.scenario_id == id; });
    if (found == assembled.portfolio_summary.scenarios.end()) {
        throw std::logic_error(
            "integration-test portfolio scenario is missing");
    }
    return *found;
}

[[nodiscard]] double aggregate_project_outlay_limit(
    const cf::PortfolioConfig& portfolio) {
    double result = 0.0;
    for (const cf::PortfolioProject& project : portfolio.projects) {
        result += project.commitment_million;
    }
    return result;
}

void check_zero_reconciliations(const cf::CapitalStackSummary& summary) {
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
            summary.maximum_stack_npv_reconciliation_error_million <
                1.0e-9 &&
            summary.maximum_wal_ratio_objective_residual_million_years <
                1.0e-9 &&
            summary.maximum_endpoint_probability_error < 1.0e-9,
        "every published capital-stack cash and probability reconciliation is zero");
}

struct ExpectedPath {
    std::string_view scenario_id{};
    double contractual_asset_loss_million{};
    double issued_principal_cash_shortfall_million{};
    double underlying_principal_cash_million{};
};

void test_verified_packages_flow_directly_into_v02(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    const cf::ClaimLedgerJointPortfolioResult assembled =
        cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), joint_terms());
    const cf::PortfolioAmbiguityConfig ambiguity = point_ambiguity(assembled);
    const cf::SuccessParticipationConfig participation = participation_terms();

    // The exact Portfolio emitted by the verified-package assembler is the
    // stack input. No project, path, cash event, or principal movement is
    // copied into a hand-built PortfolioConfig in this integration test.
    const cf::CapitalStackSummary stack = cf::evaluate_capital_stack(
        assembled.portfolio, ambiguity, participation, v02_stack_terms());

    check(assembled.portfolio.projects.size() == 2U &&
            std::all_of(assembled.portfolio.projects.begin(),
                assembled.portfolio.projects.end(), [](const auto& project) {
                    return project.principal_accounting_mode ==
                        cf::PrincipalAccountingMode::ExplicitContractualLedger;
                }) &&
            stack.model_version == cf::kCapitalStackModelVersion &&
            stack.uses_explicit_asset_liability_accounting,
        "loader-verified claims enter the v0.2 explicit asset-liability mode without re-encoding");
    check(near(stack.aggregate_commitment_million, 18.0) &&
            near(stack.aggregate_contractual_asset_principal_limit_million,
                20.0) &&
            near(stack.aggregate_project_outlay_limit_million, 18.4),
        "issued reserve, contractual face, and total project outlay remain three separate amounts");
    check(stack.tranches.size() == 2U &&
            stack.tranches[0U].tranche_id == "junior" &&
            near(stack.tranches[0U].notional_million, 6.0) &&
            stack.tranches[1U].tranche_id == "priority" &&
            near(stack.tranches[1U].notional_million, 12.0),
        "the derived eighteen-unit reserve is partitioned into six junior and twelve priority units");

    const std::vector<ExpectedPath> expected_paths = {
        {"both-perform", 0.0, 0.0, 20.0},
        {"a-perform-b-fail", 4.0, 2.0, 16.0},
        {"a-fail-b-perform", 4.0, 2.0, 16.0},
        {"both-fail", 8.0, 6.0, 12.0},
    };
    std::size_t distinct_asset_loss_and_cash_shortfall_paths = 0U;
    for (const ExpectedPath& expected : expected_paths) {
        const cf::CapitalStackScenarioResult& scenario =
            stack_scenario(stack, expected.scenario_id);
        const cf::JointScenarioResult& underlying =
            portfolio_scenario(assembled, expected.scenario_id);

        double subscriptions = 0.0;
        double direct_cost_calls = 0.0;
        double pool_cost_calls = 0.0;
        double contributions = 0.0;
        double distributions = 0.0;
        for (const cf::CapitalStackTrancheScenarioResult& tranche :
             scenario.tranches) {
            subscriptions += tranche.par_subscription_million;
            direct_cost_calls +=
                tranche.pro_rata_buyer_direct_cost_calls_million;
            pool_cost_calls += tranche.pro_rata_pool_cost_calls_million;
            contributions += tranche.total_contributions_million;
            distributions += tranche.total_distributions_million;
        }

        check(scenario.uses_explicit_asset_liability_accounting &&
                near(scenario.aggregate_commitment_million, 18.0) &&
                near(scenario.aggregate_contractual_asset_principal_limit_million,
                    20.0) &&
                near(scenario.aggregate_project_outlay_limit_million, 18.4) &&
                near(scenario.total_asset_acquisition_and_primary_funding_uses_million,
                    18.0) &&
                near(scenario.total_claim_purchase_price_million, 18.0) &&
                near(scenario.total_primary_project_funding_million, 0.0) &&
                near(scenario.total_buyer_direct_costs_million, 0.4) &&
                near(scenario.unused_commitment_returned_at_horizon_million,
                    0.0),
            "every assembled state retains the separate acquisition, direct-cost, face, and reserve ledgers");
        check(near(subscriptions, 18.0) &&
                near(direct_cost_calls, 0.4) &&
                near(pool_cost_calls, 0.0) &&
                near(contributions, 18.4),
            "every state calls eighteen of subscriptions and only four-tenths of additional buyer cost");
        check(near(scenario.contractual_asset_principal_loss_million,
                  expected.contractual_asset_loss_million) &&
                near(scenario.contractual_asset_principal_loss_million,
                    underlying.principal_loss_million) &&
                near(scenario.issued_principal_cash_shortfall_million,
                    expected.issued_principal_cash_shortfall_million) &&
                near(scenario.underlying_principal_cash_million,
                    expected.underlying_principal_cash_million),
            "contractual asset loss is preserved while issued-principal cash shortfall follows the exact horizon cash ledger");
        check(near(distributions, underlying.total_receipts_million) &&
                near(scenario.stack_nominal_net_cash_million,
                    underlying.total_receipts_million -
                        underlying.total_investor_outlays_million -
                        underlying.total_pool_costs_million) &&
                near(scenario.underlying_nominal_net_cash_million,
                    scenario.stack_nominal_net_cash_million),
            "the tranche waterfall distributes only assembled claim cash and exactly preserves nominal investor cash");
        if (!near(expected.contractual_asset_loss_million,
                expected.issued_principal_cash_shortfall_million)) {
            ++distinct_asset_loss_and_cash_shortfall_paths;
        }
    }
    check(distinct_asset_loss_and_cash_shortfall_paths == 3U,
        "three states demonstrate that asset writeoff and horizon issued-principal cash shortfall are separate observables");
    check_zero_reconciliations(stack);

    check(assembled.package_lineage.size() == 2U &&
            assembled.selection_lineage.size() == 8U &&
            assembled.joint_scenario_lineage.size() == 4U &&
            std::all_of(assembled.package_lineage.begin(),
                assembled.package_lineage.end(), [](const auto& row) {
                    return !row.package_id.empty() &&
                        row.claim_config_sha256.size() == 64U;
                }) &&
            std::all_of(assembled.selection_lineage.begin(),
                assembled.selection_lineage.end(), [](const auto& row) {
                    return !row.package_id.empty() &&
                        row.claim_config_sha256.size() == 64U &&
                        !row.marginal_scenario_id.empty();
                }),
        "verified package roots and all eight selected marginal-state lineage rows remain available beside the stack result");

    bool rejected_at_named_boundary = false;
    try {
        cf::validate_capital_stack_config(assembled.portfolio, ambiguity,
            participation,
            v01_stack_terms(
                aggregate_project_outlay_limit(assembled.portfolio)));
    } catch (const std::invalid_argument& error) {
        rejected_at_named_boundary = std::string_view(error.what()).find(
            "capital-stack v0.1 cannot consume explicit contractual principal ledgers") !=
            std::string_view::npos;
    } catch (...) {
        // The exact v0.1 asset-accounting boundary must reject this input.
    }
    check(rejected_at_named_boundary,
        "v0.1 still fails closed when given the assembler's explicit-contractual-ledger portfolio");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: claim_ledger_capital_stack_v02_tests "
                     "<first-claim.cfg> <second-claim.cfg>\n";
        return 2;
    }
    try {
        const cf::ClaimLedgerPackage first =
            cf::load_claim_ledger_package(std::filesystem::path(argv[1]));
        const cf::ClaimLedgerPackage second =
            cf::load_claim_ledger_package(std::filesystem::path(argv[2]));
        check(first.package_integrity && second.package_integrity &&
                first.full_path_evaluation_available &&
                second.full_path_evaluation_available,
            "both input packages are loader-verified complete full-path packages");
        test_verified_packages_flow_directly_into_v02(first, second);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " claim-ledger capital-stack v0.2 test(s) failed\n";
        return 1;
    }
    std::cout << "claim-ledger capital-stack v0.2 tests passed\n";
    return 0;
}
