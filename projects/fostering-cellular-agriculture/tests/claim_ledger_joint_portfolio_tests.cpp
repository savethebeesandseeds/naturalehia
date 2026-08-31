// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_joint_portfolio.hpp>
#include <naturalehia/cellular_finance/claim_ledger_package.hpp>
#include <naturalehia/cellular_finance/detail/claim_ledger_joint_portfolio_resource_guard.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
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
    double first, double second, double tolerance = 1.0e-10) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

template <typename Function>
void expect_invalid_argument(Function&& operation,
    std::string_view diagnostic, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument& error) {
        check(std::string_view(error.what()).find(diagnostic) !=
                std::string_view::npos,
            std::string(message) + " (stable diagnostic)");
    } catch (const std::exception&) {
        check(false,
            std::string(message) + " (wrong exception category)");
    }
}

[[nodiscard]] cf::ClaimLedgerPortfolioAdapterTerms adapter_terms(
    std::string project_id, std::string source_prefix,
    std::string ordinary_counterparty, double hurdle = 0.10) {
    cf::ClaimLedgerPortfolioAdapterTerms terms;
    terms.project_id = std::move(project_id);
    terms.stage = cf::ProjectStage::FirstIndustrial;
    terms.annual_physical_hurdle_rate = hurdle;
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
        {std::nullopt, fee, cf::PortfolioCashSource::FinancingFee,
            0U, 0.3, ordinary_counterparty, std::nullopt},
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
    const std::string suffix = terms.project_id == "claim-a" ? "a" :
        (terms.project_id == "claim-b" ? "b" : "rare");
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

[[nodiscard]] std::vector<cf::ClaimLedgerJointPortfolioAssetInput>
rare_asset_inputs(const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& second) {
    return {
        {rare, adapter_terms("rare-claim", "rare-claim",
                   "synthetic-rare-project-obligor")},
        {second, adapter_terms("claim-b", "claim-b",
                     "synthetic-peer-project-obligor")},
    };
}

[[nodiscard]] cf::ClaimLedgerPortfolioAdapterTerms rounding_adapter_terms() {
    cf::ClaimLedgerPortfolioAdapterTerms terms;
    terms.project_id = "rounding-claim";
    terms.stage = cf::ProjectStage::FirstIndustrial;
    terms.annual_physical_hurdle_rate = 0.10;
    terms.receipt_source_allocations.push_back(
        {std::nullopt, "closing-investor-cash-fee", "rounding.fee",
            cf::PortfolioCashSource::FinancingFee, 0.3});
    terms.cash_source_budgets.push_back(
        {std::nullopt, "rounding.fee",
            cf::PortfolioCashSource::FinancingFee, 0U, 0.3,
            "synthetic-rounding-obligor", std::nullopt});
    for (const std::string& scenario_id :
         {"rounding-a", "rounding-b", "rounding-c", "rounding-d"}) {
        const std::string source_id = scenario_id + ".operations";
        terms.receipt_source_allocations.push_back(
            {scenario_id, scenario_id + "-principal-cash", source_id,
                cf::PortfolioCashSource::Commercial, 10.0});
        terms.receipt_source_allocations.push_back(
            {scenario_id, scenario_id + "-interest-cash", source_id,
                cf::PortfolioCashSource::Commercial, 1.0});
        terms.cash_source_budgets.push_back(
            {scenario_id, source_id, cf::PortfolioCashSource::Commercial,
                12U, 11.0, "synthetic-rounding-obligor",
                std::nullopt});
        terms.scenario_factor_sets.push_back(
            {scenario_id, {scenario_id + "-factor"}});
    }
    return terms;
}

[[nodiscard]] std::vector<cf::ClaimLedgerJointPortfolioAssetInput>
rounding_asset_inputs(const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& rounding) {
    return {
        {first, adapter_terms("claim-a", "claim-a",
                    "synthetic-project-obligor")},
        {rounding, rounding_adapter_terms()},
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

[[nodiscard]] cf::ClaimLedgerJointPortfolioTerms base_joint_terms() {
    cf::ClaimLedgerJointPortfolioTerms terms;
    terms.scenario_label = "Two verified synthetic claims with explicit dependence";
    terms.source_note =
        "Hand-reconciled mechanics fixture; no empirical probability claim";
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

[[nodiscard]] const cf::JointScenario& configured_scenario(
    const cf::PortfolioConfig& portfolio, std::string_view id) {
    const auto found = std::find_if(portfolio.joint_scenarios.begin(),
        portfolio.joint_scenarios.end(), [id](const auto& scenario) {
            return scenario.id == id;
        });
    if (found == portfolio.joint_scenarios.end()) {
        throw std::runtime_error("configured joint scenario is missing");
    }
    return *found;
}

[[nodiscard]] const cf::ProjectPortfolioSummary& project_summary(
    const cf::PortfolioSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.projects.begin(),
        summary.projects.end(), [id](const auto& project) {
            return project.project_id == id;
        });
    if (found == summary.projects.end()) {
        throw std::runtime_error("project summary is missing");
    }
    return *found;
}

[[nodiscard]] const cf::JointScenarioResult& evaluated_scenario(
    const cf::PortfolioSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const auto& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::runtime_error("evaluated joint scenario is missing");
    }
    return *found;
}

[[nodiscard]] const cf::ProjectPathResult& evaluated_project(
    const cf::JointScenarioResult& scenario, std::string_view id) {
    const auto found = std::find_if(scenario.projects.begin(),
        scenario.projects.end(), [id](const auto& project) {
            return project.project_id == id;
        });
    if (found == scenario.projects.end()) {
        throw std::runtime_error("evaluated project path is missing");
    }
    return *found;
}

[[nodiscard]] const cf::ClaimLedgerPortfolioAdapterResult& marginal_asset(
    const cf::ClaimLedgerJointPortfolioResult& result,
    std::string_view project_id) {
    const auto found = std::find_if(result.marginal_assets.begin(),
        result.marginal_assets.end(), [project_id](const auto& asset) {
            return asset.portfolio.projects.front().id == project_id;
        });
    if (found == result.marginal_assets.end()) {
        throw std::runtime_error("marginal asset is missing");
    }
    return *found;
}

[[nodiscard]] const cf::ProjectJointPath& configured_project_path(
    const cf::JointScenario& scenario, std::string_view project_id) {
    const auto found = std::find_if(scenario.project_paths.begin(),
        scenario.project_paths.end(), [project_id](const auto& path) {
            return path.project_id == project_id;
        });
    if (found == scenario.project_paths.end()) {
        throw std::runtime_error("configured project path is missing");
    }
    return *found;
}

[[nodiscard]] bool same_monthly_amount(
    const cf::MonthlyAmount& first, const cf::MonthlyAmount& second) {
    return first.month == second.month &&
        near(first.amount_million, second.amount_million);
}

[[nodiscard]] bool same_configured_path(
    const cf::ProjectJointPath& first,
    const cf::ProjectJointPath& second) {
    if (first.project_id != second.project_id ||
        first.resolution != second.resolution ||
        first.capital_draws.size() != second.capital_draws.size() ||
        first.investor_outlays.size() != second.investor_outlays.size() ||
        first.investor_receipts.size() != second.investor_receipts.size() ||
        first.principal_movements.size() !=
            second.principal_movements.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.capital_draws.size();
         ++index) {
        if (!same_monthly_amount(first.capital_draws[index],
                second.capital_draws[index])) return false;
    }
    for (std::size_t index = 0U; index < first.investor_outlays.size();
         ++index) {
        const auto& left = first.investor_outlays[index];
        const auto& right = second.investor_outlays[index];
        if (left.month != right.month || left.purpose != right.purpose ||
            !near(left.amount_million, right.amount_million)) return false;
    }
    for (std::size_t index = 0U; index < first.investor_receipts.size();
         ++index) {
        const auto& left = first.investor_receipts[index];
        const auto& right = second.investor_receipts[index];
        if (left.month != right.month ||
            left.cash_source_id != right.cash_source_id ||
            !near(left.amount_million, right.amount_million) ||
            !near(left.principal_component_million,
                right.principal_component_million)) return false;
    }
    for (std::size_t index = 0U;
         index < first.principal_movements.size(); ++index) {
        const auto& left = first.principal_movements[index];
        const auto& right = second.principal_movements[index];
        if (left.month != right.month || left.kind != right.kind ||
            !near(left.amount_million, right.amount_million)) return false;
    }
    return true;
}

[[nodiscard]] bool same_cash_source(
    const cf::ScenarioCashSource& first,
    const cf::ScenarioCashSource& second) {
    if (first.id != second.id || first.kind != second.kind ||
        first.cash_available.size() != second.cash_available.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.cash_available.size();
         ++index) {
        if (!same_monthly_amount(first.cash_available[index],
                second.cash_available[index])) return false;
    }
    return true;
}

[[nodiscard]] bool same_return_sources(
    const std::vector<cf::ReturnSourceTotal>& first,
    const std::vector<cf::ReturnSourceTotal>& second) {
    if (first.size() != second.size()) return false;
    for (std::size_t index = 0U; index < first.size(); ++index) {
        if (first[index].source != second[index].source ||
            !near(first[index].nominal_million,
                second[index].nominal_million) ||
            !near(first[index].present_value_million,
                second[index].present_value_million)) return false;
    }
    return true;
}

[[nodiscard]] bool same_evaluated_project(
    const cf::ProjectPathResult& first,
    const cf::ProjectPathResult& second) {
    return first.project_id == second.project_id &&
        first.resolution == second.resolution &&
        near(first.total_draws_million, second.total_draws_million) &&
        near(first.total_investor_outlays_million,
            second.total_investor_outlays_million) &&
        near(first.total_receipts_million, second.total_receipts_million) &&
        near(first.opening_principal_million,
            second.opening_principal_million) &&
        near(first.principal_added_million,
            second.principal_added_million) &&
        near(first.principal_returned_million,
            second.principal_returned_million) &&
        near(first.principal_converted_million,
            second.principal_converted_million) &&
        near(first.outstanding_principal_million,
            second.outstanding_principal_million) &&
        near(first.principal_loss_million,
            second.principal_loss_million) &&
        near(first.npv_before_pool_costs_million,
            second.npv_before_pool_costs_million) &&
        same_return_sources(first.return_sources, second.return_sources);
}

[[nodiscard]] bool same_monthly_cash(
    const cf::MonthlyInvestorCashFlow& first,
    const cf::MonthlyInvestorCashFlow& second) {
    return first.month == second.month &&
        near(first.capital_draws_million,
            second.capital_draws_million) &&
        near(first.investor_outlays_million,
            second.investor_outlays_million) &&
        near(first.investor_receipts_million,
            second.investor_receipts_million) &&
        near(first.pool_costs_million, second.pool_costs_million) &&
        near(first.funding_need_million, second.funding_need_million) &&
        near(first.net_cash_flow_million, second.net_cash_flow_million) &&
        near(first.cumulative_net_cash_flow_million,
            second.cumulative_net_cash_flow_million);
}

[[nodiscard]] bool same_cash_lineage(
    const cf::ClaimLedgerPortfolioCashLineage& first,
    const cf::ClaimLedgerPortfolioCashLineage& second) {
    const bool same_principal =
        first.principal_component_million.has_value() ==
            second.principal_component_million.has_value() &&
        (!first.principal_component_million.has_value() ||
            near(*first.principal_component_million,
                *second.principal_component_million));
    return first.portfolio_scenario_id == second.portfolio_scenario_id &&
        first.claim_scenario_id == second.claim_scenario_id &&
        first.entry_id == second.entry_id &&
        first.economic_fact_id == second.economic_fact_id &&
        first.event_group_id == second.event_group_id &&
        first.entry_kind == second.entry_kind &&
        first.known_at_period == second.known_at_period &&
        first.source_record_id == second.source_record_id &&
        first.provider_claim_id == second.provider_claim_id &&
        first.output_kind == second.output_kind &&
        first.portfolio_project_id == second.portfolio_project_id &&
        first.cash_source_id == second.cash_source_id &&
        first.month == second.month &&
        near(first.amount_million, second.amount_million) &&
        same_principal &&
        first.cash_budget_counterparty_id ==
            second.cash_budget_counterparty_id &&
        first.cash_budget_provider_claim_id ==
            second.cash_budget_provider_claim_id;
}

[[nodiscard]] bool same_budget_lineage(
    const cf::ClaimLedgerPortfolioCashBudgetLineage& first,
    const cf::ClaimLedgerPortfolioCashBudgetLineage& second) {
    return first.portfolio_scenario_id == second.portfolio_scenario_id &&
        first.declared_claim_scenario_id ==
            second.declared_claim_scenario_id &&
        first.cash_source_id == second.cash_source_id &&
        first.source == second.source && first.month == second.month &&
        near(first.amount_million, second.amount_million) &&
        first.counterparty_id == second.counterparty_id &&
        first.provider_claim_id == second.provider_claim_id;
}

void test_explicit_non_independent_coupling(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    const auto result = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), base_joint_terms());

    check(result.portfolio.projects.size() == 2U &&
            result.portfolio.joint_scenarios.size() == 4U &&
            result.portfolio.synthetic_inputs &&
            result.portfolio.loss_layers.empty() &&
            result.package_lineage.size() == 2U &&
            result.marginal_assets.size() == 2U,
        "assembler emits two verified marginals and only the four explicitly supplied joint states");
    check(result.package_lineage[0U].portfolio_project_id == "claim-a" &&
            result.package_lineage[1U].portfolio_project_id == "claim-b" &&
            result.package_lineage[0U].period_origin_date == "2026-01-01" &&
            result.package_lineage[1U].decision_date == "2026-01-01" &&
            result.package_lineage[1U].horizon_date == "2027-01-01",
        "canonical package lineage retains the common verified calendar and information cut");
    check(near(result.portfolio_summary.total_draws_million.mean, 18.4) &&
            near(result.portfolio_summary.principal_loss_million.mean, 2.0) &&
            near(result.portfolio_summary.principal_loss_million
                     .standard_deviation,
                std::sqrt(7.2)) &&
            near(project_summary(result.portfolio_summary, "claim-a")
                     .expected_principal_loss_million,
                0.8) &&
            near(project_summary(result.portfolio_summary, "claim-b")
                     .expected_principal_loss_million,
                1.2),
        "joint evaluation preserves hand-calculated expected outlay and marginal losses");
    check(result.portfolio_summary.pairwise_loss_correlations.size() == 1U &&
            result.portfolio_summary.pairwise_loss_correlations.front()
                .correlation.has_value() &&
            *result.portfolio_summary.pairwise_loss_correlations.front()
                 .correlation > 0.2,
        "explicit coupling produces dependence without an independence inference");

    check(result.marginal_reconciliations.size() == 4U &&
            result.project_reconciliations.size() == 2U &&
            result.maximum_marginal_probability_reconciliation_error <
                1.0e-14 &&
            result.maximum_project_reconciliation_error_million < 1.0e-12,
        "every evaluated joint weight and financial marginal reconciles to its verified one-claim result");
    check(result.selection_lineage.size() == 8U &&
            result.cash_lineage.size() == 60U &&
            result.cash_budget_lineage.size() == 20U,
        "expanded lineage retains every selected path, cash event and external budget under both scenario identities");

    const cf::JointScenario& both_perform = configured_scenario(
        result.portfolio, "both-perform");
    check(both_perform.factor_tags ==
            std::vector<std::string>({"facility-a-perform",
                "facility-b-perform", "joint-demand-up"}),
        "joint factor tags are the deterministic union of explicit joint and selected marginal factors");
    check(std::all_of(result.selection_lineage.begin(),
              result.selection_lineage.end(), [](const auto& row) {
                  return !row.package_id.empty() &&
                      row.claim_config_sha256.size() == 64U &&
                      row.probability_known_at_period == 0U &&
                      row.probability_source_record_id == "SYNTHETIC";
              }),
        "selection lineage preserves package roots and upstream probability provenance");

    const cf::JointScenario& both_fail = configured_scenario(
        result.portfolio, "both-fail");
    std::vector<std::string> provider_sources;
    for (const cf::ScenarioCashSource& source : both_fail.cash_sources) {
        if (source.kind == cf::PortfolioCashSource::ExplicitSupport) {
            provider_sources.push_back(source.id);
            check(source.cash_available.size() == 1U &&
                    near(source.cash_available.front().amount_million, 4.0),
                "each provider right retains its own finite four-million budget");
        }
    }
    std::sort(provider_sources.begin(), provider_sources.end());
    check(provider_sources == std::vector<std::string>(
              {"claim-a.provider", "claim-b.provider"}),
        "same provider and claim label do not merge two distinct cash rights");
    std::vector<std::string> provider_budget_roots;
    for (const cf::ClaimLedgerJointCashBudgetLineage& row :
         result.cash_budget_lineage) {
        if (row.joint_scenario_id == "both-fail" &&
            row.marginal_budget.source ==
                cf::PortfolioCashSource::ExplicitSupport) {
            check(row.marginal_budget.counterparty_id ==
                        "synthetic-catalytic-provider" &&
                    row.marginal_budget.provider_claim_id ==
                        std::optional<std::string>{
                            "synthetic-provider-claim"} &&
                    near(row.marginal_budget.amount_million, 4.0),
                "provider budget lineage retains counterparty, claim and amount");
            provider_budget_roots.push_back(row.claim_config_sha256);
        }
    }
    std::sort(provider_budget_roots.begin(), provider_budget_roots.end());
    provider_budget_roots.erase(std::unique(provider_budget_roots.begin(),
        provider_budget_roots.end()), provider_budget_roots.end());
    check(provider_budget_roots.size() == 2U,
        "two provider rights retain two distinct verified package roots");
}

void test_selected_paths_and_lineage_are_conserved(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    const auto result = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), base_joint_terms());

    for (const cf::ClaimLedgerJointSelectionLineage& selection :
         result.selection_lineage) {
        const cf::JointScenario& joint_config = configured_scenario(
            result.portfolio, selection.joint_scenario_id);
        const cf::JointScenarioResult& joint_result = evaluated_scenario(
            result.portfolio_summary, selection.joint_scenario_id);
        const auto& marginal = marginal_asset(
            result, selection.portfolio_project_id);
        const cf::JointScenario& marginal_config = configured_scenario(
            marginal.portfolio, selection.marginal_scenario_id);
        const cf::JointScenarioResult& marginal_result = evaluated_scenario(
            marginal.portfolio_summary, selection.marginal_scenario_id);
        const cf::ProjectJointPath& joint_path = configured_project_path(
            joint_config, selection.portfolio_project_id);
        const cf::ProjectJointPath& marginal_path =
            marginal_config.project_paths.front();
        const cf::ProjectPathResult& joint_project = evaluated_project(
            joint_result, selection.portfolio_project_id);
        const cf::ProjectPathResult& marginal_project =
            marginal_result.projects.front();

        check(same_configured_path(joint_path, marginal_path),
            "every joint configured path is an exact copy of its verified marginal path");
        check(same_evaluated_project(joint_project, marginal_project),
            "every joint evaluated project path conserves its verified marginal financial result");
        check(near(joint_project.opening_principal_million +
                       joint_project.principal_added_million,
                   joint_project.principal_returned_million +
                       joint_project.principal_converted_million +
                       joint_project.outstanding_principal_million +
                       joint_project.principal_loss_million),
            "every selected project path conserves contractual principal");
    }

    for (const cf::JointScenarioResult& joint :
         result.portfolio_summary.scenarios) {
        const cf::JointScenario& configured = configured_scenario(
            result.portfolio, joint.scenario_id);
        std::vector<cf::ScenarioCashSource> expected_sources;
        std::vector<cf::MonthlyInvestorCashFlow> expected_months(
            joint.monthly_cash_flows.size());
        for (std::size_t month = 0U; month < expected_months.size();
             ++month) {
            expected_months[month].month = month;
        }
        for (const cf::ClaimLedgerJointSelectionLineage& selection :
             result.selection_lineage) {
            if (selection.joint_scenario_id != joint.scenario_id) continue;
            const auto& marginal = marginal_asset(
                result, selection.portfolio_project_id);
            const cf::JointScenario& marginal_config = configured_scenario(
                marginal.portfolio, selection.marginal_scenario_id);
            expected_sources.insert(expected_sources.end(),
                marginal_config.cash_sources.begin(),
                marginal_config.cash_sources.end());
            const cf::JointScenarioResult& marginal_result =
                evaluated_scenario(marginal.portfolio_summary,
                    selection.marginal_scenario_id);
            check(marginal_result.monthly_cash_flows.size() ==
                    expected_months.size(),
                "marginal and joint monthly axes are identical");
            for (std::size_t month = 0U; month < expected_months.size();
                 ++month) {
                const auto& source =
                    marginal_result.monthly_cash_flows[month];
                auto& target = expected_months[month];
                target.capital_draws_million +=
                    source.capital_draws_million;
                target.investor_outlays_million +=
                    source.investor_outlays_million;
                target.investor_receipts_million +=
                    source.investor_receipts_million;
                target.pool_costs_million += source.pool_costs_million;
                target.funding_need_million += source.funding_need_million;
                target.net_cash_flow_million += source.net_cash_flow_million;
                target.cumulative_net_cash_flow_million +=
                    source.cumulative_net_cash_flow_million;
            }
        }
        check(configured.cash_sources.size() == expected_sources.size(),
            "joint scenario retains every selected external cash budget");
        if (configured.cash_sources.size() == expected_sources.size()) {
            for (std::size_t index = 0U; index < expected_sources.size();
                 ++index) {
                check(same_cash_source(configured.cash_sources[index],
                          expected_sources[index]),
                    "joint cash budgets equal their marginal origins");
            }
        }
        check(joint.monthly_cash_flows.size() == expected_months.size(),
            "joint monthly result keeps the common horizon");
        if (joint.monthly_cash_flows.size() == expected_months.size()) {
            for (std::size_t month = 0U; month < expected_months.size();
                 ++month) {
                check(same_monthly_cash(joint.monthly_cash_flows[month],
                          expected_months[month]),
                    "joint monthly cash equals the sum of selected marginal cash");
            }
        }

        double draws = 0.0;
        double outlays = 0.0;
        double receipts = 0.0;
        double opening = 0.0;
        double added = 0.0;
        double returned = 0.0;
        double converted = 0.0;
        double outstanding = 0.0;
        double loss = 0.0;
        double npv = 0.0;
        for (const cf::ProjectPathResult& project : joint.projects) {
            draws += project.total_draws_million;
            outlays += project.total_investor_outlays_million;
            receipts += project.total_receipts_million;
            opening += project.opening_principal_million;
            added += project.principal_added_million;
            returned += project.principal_returned_million;
            converted += project.principal_converted_million;
            outstanding += project.outstanding_principal_million;
            loss += project.principal_loss_million;
            npv += project.npv_before_pool_costs_million;
        }
        check(near(joint.total_draws_million, draws) &&
                near(joint.total_investor_outlays_million, outlays) &&
                near(joint.total_receipts_million, receipts) &&
                near(joint.opening_principal_million, opening) &&
                near(joint.principal_added_million, added) &&
                near(joint.principal_returned_million, returned) &&
                near(joint.principal_converted_million, converted) &&
                near(joint.outstanding_principal_million, outstanding) &&
                near(joint.principal_loss_million, loss) &&
                near(joint.npv_million, npv) &&
                near(joint.total_pool_costs_million, 0.0),
            "pool fields equal the conserved sums of selected project paths");
    }

    for (const cf::ClaimLedgerJointCashLineage& row :
         result.cash_lineage) {
        const auto found_asset = std::find_if(
            result.marginal_assets.begin(), result.marginal_assets.end(),
            [&row](const auto& asset) {
                return asset.package_lineage.has_value() &&
                    asset.package_lineage->package_id == row.package_id &&
                    asset.package_lineage->claim_config_sha256 ==
                        row.claim_config_sha256;
            });
        const bool found_row = found_asset != result.marginal_assets.end() &&
            std::any_of(found_asset->cash_lineage.begin(),
                found_asset->cash_lineage.end(), [&row](const auto& source) {
                    return same_cash_lineage(source, row.marginal_cash);
                });
        check(found_row,
            "every expanded cash-lineage row is an exact verified marginal row");
    }
    for (const cf::ClaimLedgerJointCashBudgetLineage& row :
         result.cash_budget_lineage) {
        const auto found_asset = std::find_if(
            result.marginal_assets.begin(), result.marginal_assets.end(),
            [&row](const auto& asset) {
                return asset.package_lineage.has_value() &&
                    asset.package_lineage->package_id == row.package_id &&
                    asset.package_lineage->claim_config_sha256 ==
                        row.claim_config_sha256;
            });
        const bool found_row = found_asset != result.marginal_assets.end() &&
            std::any_of(found_asset->cash_budget_lineage.begin(),
                found_asset->cash_budget_lineage.end(),
                [&row](const auto& source) {
                    return same_budget_lineage(
                        source, row.marginal_budget);
                });
        check(found_row,
            "every expanded budget-lineage row is an exact verified marginal row");
    }
}

void test_dependence_changes_tail_not_marginals(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    const auto base = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), base_joint_terms());
    auto stronger_terms = base_joint_terms();
    for (auto& scenario : stronger_terms.joint_scenarios) {
        if (scenario.scenario_id == "both-perform") {
            scenario.physical_probability = 0.7;
        } else if (scenario.scenario_id == "a-perform-b-fail") {
            scenario.physical_probability = 0.1;
        } else if (scenario.scenario_id == "a-fail-b-perform") {
            scenario.physical_probability = 0.0;
        } else if (scenario.scenario_id == "both-fail") {
            scenario.physical_probability = 0.2;
        }
    }
    const auto stronger = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), stronger_terms);

    check(near(base.portfolio_summary.principal_loss_million.mean,
              stronger.portfolio_summary.principal_loss_million.mean) &&
            !near(base.portfolio_summary.principal_loss_million
                      .standard_deviation,
                stronger.portfolio_summary.principal_loss_million
                    .standard_deviation) &&
            !near(*base.portfolio_summary.pairwise_loss_correlations.front()
                       .correlation,
                *stronger.portfolio_summary.pairwise_loss_correlations.front()
                     .correlation),
        "a different valid coupling changes pool volatility and correlation while expected loss stays fixed");
    for (const std::string id : {"claim-a", "claim-b"}) {
        const auto& first_project = project_summary(
            base.portfolio_summary, id);
        const auto& second_project = project_summary(
            stronger.portfolio_summary, id);
        check(near(first_project.expected_principal_loss_million,
                  second_project.expected_principal_loss_million) &&
                near(first_project.expected_npv_before_pool_costs_million,
                    second_project.expected_npv_before_pool_costs_million),
            "dependence changes do not alter project expected loss or NPV");
    }
}

void test_authoritative_probability_lineage(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    constexpr double scale = 1.0 + 5.0e-13;
    const std::vector<double> weights{
        0.5623326678584947, 0.03766733214150526,
        0.19728530328594396, 0.002714696714056042,
        0.03858042538099943, 0.061419574619000585,
        0.014613886137778817, 0.08538611386222118};
    const std::vector<std::pair<std::string, std::string>> states{
        {"performing-maturity", "performing-maturity"},
        {"performing-maturity", "performing-maturity"},
        {"performing-maturity", "failure-with-provider"},
        {"performing-maturity", "failure-with-provider"},
        {"failure-with-provider", "performing-maturity"},
        {"failure-with-provider", "performing-maturity"},
        {"failure-with-provider", "failure-with-provider"},
        {"failure-with-provider", "failure-with-provider"}};

    cf::ClaimLedgerJointPortfolioTerms terms;
    terms.scenario_label =
        "Rounding-sensitive explicit joint probability lineage";
    terms.source_note =
        "Synthetic split cells test declared configured and evaluated weights";
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        terms.joint_scenarios.push_back(joint_state(
            "split-" + std::to_string(index), weights[index] * scale,
            states[index].first, states[index].second,
            "split-factor-" + std::to_string(index)));
    }

    const auto result = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), terms);
    double configured_weight_sum = 0.0;
    double evaluated_weight_sum = 0.0;
    for (const cf::ClaimLedgerJointScenarioLineage& lineage :
         result.joint_scenario_lineage) {
        const cf::JointScenario& configured = configured_scenario(
            result.portfolio, lineage.joint_scenario_id);
        const cf::JointScenarioResult& evaluated = evaluated_scenario(
            result.portfolio_summary, lineage.joint_scenario_id);
        check(lineage.configured_physical_probability ==
                    configured.weight &&
                lineage.physical_probability ==
                    evaluated.normalized_weight,
            "joint lineage separates configured weight from the authoritative evaluated measure");
        check(lineage.declared_physical_probability !=
                lineage.configured_physical_probability,
            "declared near-one weights remain distinct from assembler normalization");
        configured_weight_sum += lineage.configured_physical_probability;
        evaluated_weight_sum += lineage.physical_probability;
    }
    for (const cf::ClaimLedgerJointSelectionLineage& lineage :
         result.selection_lineage) {
        const cf::JointScenarioResult& evaluated = evaluated_scenario(
            result.portfolio_summary, lineage.joint_scenario_id);
        check(lineage.joint_physical_probability ==
                    evaluated.normalized_weight,
            "selection lineage uses the authoritative evaluated joint weight");
    }
    check(near(configured_weight_sum, 1.0, 1.0e-14) &&
            near(evaluated_weight_sum, 1.0, 1.0e-14),
        "rounding fixture reaches Portfolio's authoritative unit measure even when its final normalization is exactly representable");
}

void test_marginal_reconciliation_uses_authoritative_final_measure(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& rounding) {
    const cf::ClaimLedgerPortfolioAdapterResult first_marginal =
        cf::adapt_claim_ledger_package_to_portfolio(
            first, adapter_terms("claim-a", "claim-a",
                       "synthetic-project-obligor"));
    const cf::ClaimLedgerPortfolioAdapterResult rounding_marginal =
        cf::adapt_claim_ledger_package_to_portfolio(
            rounding, rounding_adapter_terms());

    cf::ClaimLedgerJointPortfolioTerms terms;
    terms.scenario_label =
        "Four-state marginal final-normalization lineage test";
    terms.source_note =
        "Synthetic final-normalization fixture; no empirical probability claim";
    for (const cf::ClaimLedgerPortfolioPathBridge& rounding_path :
         rounding_marginal.paths) {
        for (const cf::ClaimLedgerPortfolioPathBridge& first_path :
             first_marginal.paths) {
            cf::ClaimLedgerJointPortfolioScenario scenario;
            scenario.scenario_id = rounding_path.scenario_id + "-" +
                (first_path.scenario_id == "performing-maturity"
                    ? "base-perform" : "base-fail");
            scenario.physical_probability =
                rounding_path.physical_probability *
                first_path.physical_probability;
            scenario.probability_basis_id =
                "rounding-final-measure-test-v0.1";
            scenario.selections = {
                {"claim-a", first_path.scenario_id},
                {"rounding-claim", rounding_path.scenario_id},
            };
            terms.joint_scenarios.push_back(std::move(scenario));
        }
    }

    const auto result = cf::assemble_claim_ledger_joint_portfolio(
        rounding_asset_inputs(first, rounding), terms);
    const auto& retained_rounding = marginal_asset(result, "rounding-claim");
    std::size_t reconciled_marginals = 0U;
    for (const cf::JointScenario& configured :
         retained_rounding.portfolio.joint_scenarios) {
        const auto bridge = std::find_if(retained_rounding.paths.begin(),
            retained_rounding.paths.end(), [&](const auto& row) {
                return row.scenario_id == configured.id;
            });
        check(bridge != retained_rounding.paths.end(),
            "rounding fixture retains every authoritative marginal bridge");
        if (bridge == retained_rounding.paths.end()) continue;
        const cf::JointScenarioResult& evaluated = evaluated_scenario(
            retained_rounding.portfolio_summary, configured.id);
        check(bridge->physical_probability == evaluated.normalized_weight,
            "rounding fixture bridge retains Portfolio's authoritative marginal measure");
        const auto reconciliation = std::find_if(
            result.marginal_reconciliations.begin(),
            result.marginal_reconciliations.end(), [&](const auto& row) {
                return row.portfolio_project_id == "rounding-claim" &&
                    row.marginal_scenario_id == configured.id;
            });
        check(reconciliation != result.marginal_reconciliations.end(),
            "rounding fixture retains every marginal reconciliation row");
        if (reconciliation == result.marginal_reconciliations.end()) continue;
        ++reconciled_marginals;
        check(reconciliation->configured_marginal_probability ==
                configured.weight,
            "marginal reconciliation separately retains configured weight");
        check(reconciliation->marginal_probability ==
                bridge->physical_probability,
            "marginal reconciliation targets authoritative evaluated weight");
        check(near(reconciliation->probability_from_joint_states,
                bridge->physical_probability, 1.0e-14),
            "evaluated joint mass reproduces the authoritative marginal measure");
        check(reconciliation->reconciliation_error ==
                reconciliation->probability_from_joint_states -
                    bridge->physical_probability,
            "marginal reconciliation error uses the authoritative target");
    }
    check(reconciled_marginals ==
            retained_rounding.portfolio.joint_scenarios.size(),
        "four-state fixture reconciles every authoritative marginal Portfolio weight");
}

void test_factor_provenance_and_union_bounds(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    auto overlap = base_joint_terms();
    overlap.joint_scenarios.front().factor_tags =
        {"facility-a-perform"};
    const auto overlap_result =
        cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), overlap);
    const auto& overlap_lineage = *std::find_if(
        overlap_result.joint_scenario_lineage.begin(),
        overlap_result.joint_scenario_lineage.end(), [](const auto& row) {
            return row.joint_scenario_id == "both-perform";
        });
    check(overlap_lineage.declared_factor_tags ==
                std::vector<std::string>({"facility-a-perform"}) &&
            overlap_lineage.factor_tags ==
                std::vector<std::string>({"facility-a-perform",
                    "facility-b-perform"}),
        "declared coupling factors remain visible when they overlap a marginal factor");

    auto exact_union = base_joint_terms();
    exact_union.joint_scenarios.front().factor_tags.clear();
    for (std::size_t index = 0U; index < 62U; ++index) {
        exact_union.joint_scenarios.front().factor_tags.push_back(
            "union-factor-" + std::to_string(index));
    }
    const auto exact_result = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), exact_union);
    check(configured_scenario(exact_result.portfolio, "both-perform")
              .factor_tags.size() == 64U,
        "an exact 64-tag effective union is accepted");

    auto excessive_union = exact_union;
    excessive_union.joint_scenarios.front().factor_tags.push_back(
        "union-factor-62");
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), excessive_union);
    }, "joint factor union exceeds",
        "a 65-tag effective union is rejected");
}

void test_resource_accumulator_boundaries() {
    namespace detail = cf::detail;
    detail::RetainedMarginalResourceUsage exact;
    exact.add_project_scenario_pairs(
        detail::kMaximumRetainedMarginalProjectScenarioPairs);
    exact.add_scenario_month_rows(
        detail::kMaximumRetainedMarginalScenarioMonthRows);
    exact.add_cash_records(
        detail::kMaximumRetainedMarginalCashRecords);
    exact.add_lineage_rows(
        detail::kMaximumRetainedMarginalLineageRows);
    check(exact.project_scenario_pairs ==
                detail::kMaximumRetainedMarginalProjectScenarioPairs &&
            exact.scenario_month_rows ==
                detail::kMaximumRetainedMarginalScenarioMonthRows &&
            exact.cash_records ==
                detail::kMaximumRetainedMarginalCashRecords &&
            exact.lineage_rows ==
                detail::kMaximumRetainedMarginalLineageRows,
        "aggregate retained-marginal guards accept their exact limits");

    detail::ExpandedJointResourceUsage expanded_exact;
    expanded_exact.add_lineage_entries(
        detail::kMaximumExpandedJointLineageEntries);
    check(expanded_exact.lineage_entries ==
            detail::kMaximumExpandedJointLineageEntries,
        "aggregate expanded-joint lineage guard accepts its exact limit");

    expect_invalid_argument([&] {
        exact.add_project_scenario_pairs(1U);
    }, "retained marginal project-scenario pairs exceeds",
        "project-scenario limit plus one is rejected");
    expect_invalid_argument([&] {
        exact.add_scenario_month_rows(1U);
    }, "retained marginal scenario-month rows exceeds",
        "scenario-month limit plus one is rejected");
    expect_invalid_argument([&] {
        exact.add_cash_records(1U);
    }, "retained marginal cash records exceeds",
        "cash-record limit plus one is rejected");
    expect_invalid_argument([&] {
        exact.add_lineage_rows(1U);
    }, "retained marginal lineage and decision-entry rows exceeds",
        "lineage limit plus one is rejected");
    expect_invalid_argument([&] {
        expanded_exact.add_lineage_entries(1U);
    }, "expanded joint lineage rows and factor instances exceeds",
        "expanded joint lineage limit plus one is rejected");

    std::size_t near_max = std::numeric_limits<std::size_t>::max() - 1U;
    expect_invalid_argument([&] {
        detail::add_bounded_resource(near_max, 2U,
            std::numeric_limits<std::size_t>::max(),
            "overflow probe");
    }, "overflow probe exceeds",
        "bounded resource addition rejects size_t overflow");
}

void test_positive_rare_marginal_is_not_erased(
    const cf::ClaimLedgerPackage& rare,
    const cf::ClaimLedgerPackage& second) {
    const auto make_state = [](std::string id, double probability,
                                std::string rare_state,
                                std::string peer_state) {
        cf::ClaimLedgerJointPortfolioScenario state;
        state.scenario_id = std::move(id);
        state.physical_probability = probability;
        state.probability_basis_id = "rare-state-test-v0.1";
        state.factor_tags = {"rare-state-explicit"};
        state.selections = {
            {"rare-claim", std::move(rare_state)},
            {"claim-b", std::move(peer_state)},
        };
        return state;
    };

    cf::ClaimLedgerJointPortfolioTerms explicit_terms;
    explicit_terms.scenario_label =
        "Positive one-in-one-quadrillion marginal conservation";
    explicit_terms.source_note =
        "Synthetic rare-state fixture tests relative probability reconciliation";
    explicit_terms.joint_scenarios = {
        make_state("ordinary-peer-perform", 0.7,
            "performing-maturity", "performing-maturity"),
        make_state("ordinary-peer-fail", 0.299999999999999,
            "performing-maturity", "failure-with-provider"),
        make_state("rare-failure", 0.000000000000001,
            "failure-with-provider", "failure-with-provider"),
    };
    const auto explicit_result =
        cf::assemble_claim_ledger_joint_portfolio(
            rare_asset_inputs(rare, second), explicit_terms);
    const auto rare_selection = std::find_if(
        explicit_result.selection_lineage.begin(),
        explicit_result.selection_lineage.end(), [](const auto& row) {
            return row.joint_scenario_id == "rare-failure" &&
                row.portfolio_project_id == "rare-claim";
        });
    check(rare_selection != explicit_result.selection_lineage.end() &&
            rare_selection->marginal_probability > 0.0 &&
            rare_selection->joint_physical_probability > 0.0 &&
            evaluated_scenario(explicit_result.portfolio_summary,
                "rare-failure").normalized_weight > 0.0,
        "an explicit positive rare state survives package, assembler and Portfolio normalization");

    auto omitted = explicit_terms;
    omitted.joint_scenarios[1U].physical_probability = 0.3;
    omitted.joint_scenarios[2U].physical_probability = 0.0;
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            rare_asset_inputs(rare, second), omitted);
    }, "do not reproduce every claim marginal",
        "zero joint mass cannot erase a positive rare marginal below the absolute tolerance");
}

void test_verified_calendar_mismatch_is_rejected(
    const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& shifted) {
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, shifted), base_joint_terms());
    }, "one common calendar and information axis",
        "two hash-valid claims with equal numeric periods but shifted dates cannot be pooled");
}

void test_order_is_canonical(const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    const auto baseline = cf::assemble_claim_ledger_joint_portfolio(
        asset_inputs(first, second), base_joint_terms());
    auto assets = asset_inputs(first, second);
    std::reverse(assets.begin(), assets.end());
    auto terms = base_joint_terms();
    std::reverse(terms.joint_scenarios.begin(), terms.joint_scenarios.end());
    for (auto& scenario : terms.joint_scenarios) {
        std::reverse(scenario.selections.begin(), scenario.selections.end());
    }
    const auto permuted = cf::assemble_claim_ledger_joint_portfolio(
        assets, terms);
    check(baseline.portfolio.projects[0U].id ==
                permuted.portfolio.projects[0U].id &&
            baseline.portfolio.projects[1U].id ==
                permuted.portfolio.projects[1U].id &&
            baseline.portfolio.joint_scenarios[0U].id ==
                permuted.portfolio.joint_scenarios[0U].id &&
            baseline.portfolio.joint_scenarios.back().id ==
                permuted.portfolio.joint_scenarios.back().id &&
            near(baseline.portfolio_summary.principal_loss_million.mean,
                permuted.portfolio_summary.principal_loss_million.mean) &&
            near(baseline.portfolio_summary.principal_loss_million
                     .standard_deviation,
                permuted.portfolio_summary.principal_loss_million
                    .standard_deviation),
        "asset, selection and joint-row permutations produce one canonical portfolio ordering");
}

void test_fail_closed_controls(const cf::ClaimLedgerPackage& first,
    const cf::ClaimLedgerPackage& second) {
    auto drift = base_joint_terms();
    drift.joint_scenarios[0U].physical_probability = 0.61;
    drift.joint_scenarios[1U].physical_probability = 0.19;
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), drift);
    }, "do not reproduce every claim marginal",
        "a unit-sum joint table that changes one marginal is rejected");

    auto missing = base_joint_terms();
    missing.joint_scenarios.front().selections.pop_back();
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), missing);
    }, "must select every claim asset exactly once",
        "a missing project selection is rejected");

    auto duplicate = base_joint_terms();
    duplicate.joint_scenarios.front().selections[1U] =
        duplicate.joint_scenarios.front().selections[0U];
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), duplicate);
    }, "duplicate project selection",
        "a duplicate project selection is rejected");

    auto unknown = base_joint_terms();
    unknown.joint_scenarios.front().selections[0U]
        .marginal_scenario_id = "unknown-path";
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), unknown);
    }, "unknown marginal scenario",
        "an unknown marginal scenario is rejected");

    auto no_basis = base_joint_terms();
    no_basis.joint_scenarios.front().probability_basis_id.clear();
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), no_basis);
    }, "probability_basis_id must be a safe identifier",
        "a joint probability without a retained basis identity is rejected");

    auto duplicate_package = asset_inputs(first, second);
    duplicate_package[1U].package = first;
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            duplicate_package, base_joint_terms());
    }, "must have unique project, package, root, economic-cluster and claim identities",
        "a duplicated package root, claim and economic cluster is rejected");

    auto colliding_sources = asset_inputs(first, second);
    for (auto& allocation : colliding_sources[1U]
             .adapter_terms.receipt_source_allocations) {
        if (allocation.cash_source_id == "claim-b.provider") {
            allocation.cash_source_id = "claim-a.provider";
        }
    }
    for (auto& budget : colliding_sources[1U]
             .adapter_terms.cash_source_budgets) {
        if (budget.cash_source_id == "claim-b.provider") {
            budget.cash_source_id = "claim-a.provider";
        }
    }
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            colliding_sources, base_joint_terms());
    }, "cash-source identifiers must be unique across joint assets",
        "one provider-bound cross-asset cash-source alias is rejected");

    auto mismatched_hurdle = asset_inputs(first, second);
    mismatched_hurdle[1U].adapter_terms.annual_physical_hurdle_rate = 0.11;
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            mismatched_hurdle, base_joint_terms());
    }, "require one model version, currency, monetary basis, horizon and hurdle",
        "claims with different hurdle sensitivities cannot share one NPV distribution");

    auto oversized = base_joint_terms();
    oversized.joint_scenarios.resize(10'001U);
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            asset_inputs(first, second), oversized);
    }, "joint scenario dimensions exceed",
        "joint scenario dimensions are rejected before package adaptation work");

    auto unusable_assets = asset_inputs(first, second);
    unusable_assets[0U].package.claim_config_filename =
        "deliberately-missing-claim.cfg";
    unusable_assets[1U].package.claim_config_filename =
        "deliberately-missing-claim.cfg";
    auto unsafe_id = base_joint_terms();
    unsafe_id.joint_scenarios.front().scenario_id.clear();
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            unusable_assets, unsafe_id);
    }, "joint scenario_id must be a safe identifier",
        "unsafe joint IDs fail before any package reload");

    auto oversized_declared_factors = base_joint_terms();
    oversized_declared_factors.joint_scenarios.front().factor_tags.clear();
    for (std::size_t index = 0U; index < 65U; ++index) {
        oversized_declared_factors.joint_scenarios.front()
            .factor_tags.push_back(
                "declared-factor-" + std::to_string(index));
    }
    expect_invalid_argument([&] {
        (void)cf::assemble_claim_ledger_joint_portfolio(
            unusable_assets, oversized_declared_factors);
    }, "declared joint factor tag count exceeds",
        "65 declared factors fail before any package reload");

    cf::ClaimLedgerPackage caller_mutation = second;
    caller_mutation.config.period_origin_date.value = "2030-01-01";
    auto mutated_assets = asset_inputs(first, caller_mutation);
    const auto reverified = cf::assemble_claim_ledger_joint_portfolio(
        mutated_assets, base_joint_terms());
    check(reverified.package_lineage[1U].period_origin_date == "2026-01-01",
        "calendar checks use reverified package lineage rather than mutable caller metadata");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 6) {
        std::cerr << "usage: claim_ledger_joint_portfolio_tests "
                     "<first-claim.cfg> <second-claim.cfg> "
                     "<rare-claim.cfg> <shifted-claim.cfg> "
                     "<rounding-claim.cfg>\n";
        return 2;
    }
    try {
        const cf::ClaimLedgerPackage first =
            cf::load_claim_ledger_package(std::filesystem::path(argv[1]));
        const cf::ClaimLedgerPackage second =
            cf::load_claim_ledger_package(std::filesystem::path(argv[2]));
        const cf::ClaimLedgerPackage rare =
            cf::load_claim_ledger_package(std::filesystem::path(argv[3]));
        const cf::ClaimLedgerPackage shifted =
            cf::load_claim_ledger_package(std::filesystem::path(argv[4]));
        const cf::ClaimLedgerPackage rounding =
            cf::load_claim_ledger_package(std::filesystem::path(argv[5]));
        test_explicit_non_independent_coupling(first, second);
        test_selected_paths_and_lineage_are_conserved(first, second);
        test_dependence_changes_tail_not_marginals(first, second);
        test_authoritative_probability_lineage(first, second);
        test_marginal_reconciliation_uses_authoritative_final_measure(
            first, rounding);
        test_factor_provenance_and_union_bounds(first, second);
        test_resource_accumulator_boundaries();
        test_positive_rare_marginal_is_not_erased(rare, second);
        test_verified_calendar_mismatch_is_rejected(first, shifted);
        test_order_is_canonical(first, second);
        test_fail_closed_controls(first, second);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " claim-ledger joint portfolio test(s) failed\n";
        return 1;
    }
    std::cout << "all claim-ledger joint portfolio tests passed\n";
    return 0;
}
