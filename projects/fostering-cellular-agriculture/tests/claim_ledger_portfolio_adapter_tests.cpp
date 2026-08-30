// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_package.hpp>
#include <naturalehia/cellular_finance/claim_ledger_portfolio_adapter.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
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

template <typename Function>
void expect_invalid(Function&& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] cf::ClaimLedgerPortfolioAdapterTerms adapter_terms() {
    cf::ClaimLedgerPortfolioAdapterTerms terms;
    terms.project_id = "synthetic-cellular-claim";
    terms.stage = cf::ProjectStage::FirstIndustrial;
    terms.annual_physical_hurdle_rate = 0.10;
    terms.receipt_source_allocations = {
        {std::nullopt, "closing-investor-cash-fee",
            "synthetic-cellular-claim.fee",
            cf::PortfolioCashSource::FinancingFee, 0.3},
        {"performing-maturity", "success-principal-cash",
            "synthetic-cellular-claim.operations",
            cf::PortfolioCashSource::Commercial, 10.0},
        {"performing-maturity", "success-interest-cash",
            "synthetic-cellular-claim.operations",
            cf::PortfolioCashSource::Commercial, 1.0},
        {"failure-with-provider", "failure-recovery-principal-cash",
            "synthetic-cellular-claim.recovery",
            cf::PortfolioCashSource::Recovery, 2.0},
        {"failure-with-provider", "failure-guarantee-principal-cash",
            "synthetic-provider-claim",
            cf::PortfolioCashSource::ExplicitSupport, 4.0},
    };
    terms.cash_source_budgets = {
        {std::nullopt, "synthetic-cellular-claim.fee",
            cf::PortfolioCashSource::FinancingFee, 0U, 0.3,
            "synthetic-project-obligor", std::nullopt},
        {"performing-maturity", "synthetic-cellular-claim.operations",
            cf::PortfolioCashSource::Commercial, 12U, 11.0,
            "synthetic-project-obligor", std::nullopt},
        {"failure-with-provider", "synthetic-cellular-claim.recovery",
            cf::PortfolioCashSource::Recovery, 12U, 2.0,
            "synthetic-project-obligor", std::nullopt},
        {"failure-with-provider", "synthetic-provider-claim",
            cf::PortfolioCashSource::ExplicitSupport, 12U, 4.0,
            "synthetic-catalytic-provider",
            std::optional<std::string>{"synthetic-provider-claim"}},
    };
    terms.scenario_factor_sets = {
        {"performing-maturity", {"facility-and-market-perform"}},
        {"failure-with-provider", {"facility-failure"}},
    };
    return terms;
}

[[nodiscard]] const cf::JointScenarioResult& scenario_result(
    const cf::PortfolioSummary& summary, std::string_view id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [id](const auto& scenario) {
            return scenario.scenario_id == id;
        });
    if (found == summary.scenarios.end()) {
        throw std::runtime_error("test scenario result is missing");
    }
    return *found;
}

[[nodiscard]] const cf::ClaimLedgerPortfolioPathBridge& bridge_path(
    const cf::ClaimLedgerPortfolioAdapterResult& result,
    std::string_view id) {
    const auto found = std::find_if(result.paths.begin(), result.paths.end(),
        [id](const auto& path) { return path.scenario_id == id; });
    if (found == result.paths.end()) {
        throw std::runtime_error("test bridge path is missing");
    }
    return *found;
}

[[nodiscard]] const cf::ClaimLedgerPortfolioCashLineage& receipt_lineage(
    const cf::ClaimLedgerPortfolioAdapterResult& result,
    std::string_view portfolio_scenario_id, std::string_view entry_id) {
    const auto found = std::find_if(result.cash_lineage.begin(),
        result.cash_lineage.end(),
        [portfolio_scenario_id, entry_id](const auto& row) {
            return row.portfolio_scenario_id == portfolio_scenario_id &&
                row.entry_id == entry_id &&
                row.output_kind ==
                    cf::ClaimLedgerPortfolioOutputKind::InvestorReceipt;
        });
    if (found == result.cash_lineage.end()) {
        throw std::runtime_error("test receipt lineage is missing");
    }
    return *found;
}

[[nodiscard]] double expected_source(
    const cf::PortfolioSummary& summary, cf::PortfolioCashSource source) {
    const auto found = std::find_if(summary.expected_return_sources.begin(),
        summary.expected_return_sources.end(),
        [source](const auto& total) { return total.source == source; });
    if (found == summary.expected_return_sources.end()) {
        throw std::runtime_error("test expected source is missing");
    }
    return found->nominal_million;
}

void test_exact_cash_and_loss_basis_bridge(
    const cf::ClaimLedgerConfig& ledger) {
    const cf::ClaimLedgerPortfolioAdapterResult adapted =
        cf::adapt_claim_ledger_to_portfolio(ledger, adapter_terms());
    const cf::PortfolioConfig& portfolio = adapted.portfolio;
    const cf::PortfolioSummary& summary = adapted.portfolio_summary;

    check(portfolio.synthetic_inputs && portfolio.projects.size() == 1U &&
            portfolio.projects.front().id == "synthetic-cellular-claim" &&
            portfolio.projects.front().stage ==
                cf::ProjectStage::FirstIndustrial &&
            near(portfolio.projects.front().commitment_million, 9.2) &&
            portfolio.projects.front().principal_accounting_mode ==
                cf::PrincipalAccountingMode::ExplicitContractualLedger &&
            near(portfolio.projects.front().principal_limit_million, 10.0) &&
            near(portfolio.projects.front().opening_principal_million, 0.0) &&
            portfolio.horizon_months == 12U &&
            near(portfolio.annual_physical_hurdle_rate, 0.10),
        "adapter emits one synthetic mechanical project with separate investor-cash and contractual-principal limits");
    check(portfolio.loss_layers.empty() &&
            portfolio.joint_scenarios.size() == 2U &&
            near(summary.configured_scenario_weight_sum, 1.0),
        "adapter preserves the complete declared scenario measure and adds no packaging");

    const cf::JointScenarioResult& success =
        scenario_result(summary, "performing-maturity");
    const cf::JointScenarioResult& failure =
        scenario_result(summary, "failure-with-provider");
    check(near(success.total_investor_outlays_million, 9.2) &&
            near(success.total_pool_costs_million, 0.0) &&
            near(success.total_receipts_million, 11.3) &&
            near(success.projects.front().principal_added_million, 10.0) &&
            near(success.projects.front().principal_returned_million, 10.0) &&
            near(success.principal_loss_million, 0.0),
        "performing path preserves investor cash while its contractual principal ledger closes independently");
    check(near(failure.total_investor_outlays_million, 9.2) &&
            near(failure.total_pool_costs_million, 0.0) &&
            near(failure.total_receipts_million, 6.3) &&
            near(failure.projects.front().principal_returned_million, 6.0) &&
            near(failure.principal_loss_million, 4.0),
        "failure path reports the exact contractual writeoff after recovery and explicit support");

    const auto& success_bridge = bridge_path(adapted, "performing-maturity");
    const auto& failure_bridge = bridge_path(adapted,
        "failure-with-provider");
    check(near(success_bridge.contractual_principal_loss_million, 0.0) &&
            near(success_bridge.portfolio_principal_loss_million, 0.0) &&
            near(failure_bridge.contractual_principal_loss_million, 4.0) &&
            near(failure_bridge.portfolio_principal_loss_million, 4.0) &&
            near(failure_bridge.nominal_investor_cash_shortfall_million, 2.9),
        "bridge reconciles contractual writeoff while separately disclosing the investor's nominal cash shortfall");
    check(near(adapted.expected_contractual_principal_loss_million, 0.8) &&
            near(adapted.expected_nominal_investor_cash_shortfall_million,
                0.58) &&
            near(adapted.maximum_monthly_cash_reconciliation_error_million,
                0.0),
        "expected contractual loss and nominal investor shortfall remain distinct and reconciled");

    check(near(summary.total_draws_million.mean, 9.2) &&
            near(summary.principal_loss_million.mean, 0.8) &&
            near(summary.npv_million.mean, 0.190909090909091) &&
            near(expected_source(summary,
                     cf::PortfolioCashSource::FinancingFee),
                0.3) &&
            near(expected_source(summary,
                     cf::PortfolioCashSource::Commercial),
                8.8) &&
            near(expected_source(summary,
                     cf::PortfolioCashSource::Recovery),
                0.4) &&
            near(expected_source(summary,
                     cf::PortfolioCashSource::ExplicitSupport),
                0.8),
        "adapted expected cash, NPV, loss and external-source attribution match the hand reconstruction");
    check(near(success.monthly_cash_flows[0U].net_cash_flow_million, -8.9) &&
            near(success.monthly_cash_flows[12U].net_cash_flow_million,
                11.0) &&
            near(failure.monthly_cash_flows[0U].net_cash_flow_million,
                -8.9) &&
            near(failure.monthly_cash_flows[12U].net_cash_flow_million,
                6.0),
        "pathwise monthly investor cash is conserved exactly, including project-attributed buyer direct cost");
}

void set_common_entry(cf::ClaimLedgerConfig& ledger,
    std::string_view entry_id, double value) {
    const auto found = std::find_if(ledger.common_entries.begin(),
        ledger.common_entries.end(), [entry_id](const auto& entry) {
            return entry.entry_id == entry_id;
        });
    if (found == ledger.common_entries.end()) {
        throw std::runtime_error("premium regression entry is missing");
    }
    found->value = cf::claim_ledger_known(value);
}

void test_performing_premium_issue_is_not_principal_loss(
    const cf::ClaimLedgerConfig& ledger) {
    cf::ClaimLedgerConfig premium = ledger;
    set_common_entry(premium, "closing-buyer-price", 11.0);
    set_common_entry(premium, "closing-borrower-gross-proceeds", 11.0);
    set_common_entry(premium, "closing-borrower-net-proceeds", 10.5);
    set_common_entry(premium, "closing-funded-principal", 10.0);
    set_common_entry(premium, "closing-original-issue-premium", 1.0);
    set_common_entry(premium, "closing-capitalized-fee", 0.0);

    const cf::ClaimLedgerPortfolioAdapterResult adapted =
        cf::adapt_claim_ledger_to_portfolio(premium, adapter_terms());
    const auto& success =
        scenario_result(adapted.portfolio_summary, "performing-maturity");
    check(near(success.total_investor_outlays_million, 11.2) &&
            near(success.projects.front().principal_added_million, 10.0) &&
            near(success.projects.front().principal_returned_million, 10.0) &&
            near(success.projects.front().principal_loss_million, 0.0),
        "a fully repaid claim bought above par has investor cash risk but no contractual principal loss");
}

void test_budget_identity_is_retained(
    const cf::ClaimLedgerConfig& ledger) {
    const cf::ClaimLedgerPortfolioAdapterResult original =
        cf::adapt_claim_ledger_to_portfolio(ledger, adapter_terms());
    cf::ClaimLedgerPortfolioAdapterTerms changed_terms = adapter_terms();
    changed_terms.cash_source_budgets.front().counterparty_id =
        "alternate-financing-fee-payer";
    const cf::ClaimLedgerPortfolioAdapterResult changed =
        cf::adapt_claim_ledger_to_portfolio(ledger, changed_terms);

    const auto& original_fee = receipt_lineage(original,
        "performing-maturity", "closing-investor-cash-fee");
    const auto& changed_fee = receipt_lineage(changed,
        "performing-maturity", "closing-investor-cash-fee");
    const auto provider_budget = std::find_if(
        original.cash_budget_lineage.begin(),
        original.cash_budget_lineage.end(), [](const auto& row) {
            return row.cash_source_id == "synthetic-provider-claim";
        });
    const std::size_t common_fee_budget_rows =
        static_cast<std::size_t>(std::count_if(
            original.cash_budget_lineage.begin(),
            original.cash_budget_lineage.end(), [](const auto& row) {
                return row.cash_source_id ==
                        "synthetic-cellular-claim.fee" &&
                    !row.declared_claim_scenario_id.has_value() &&
                    row.counterparty_id == "synthetic-project-obligor";
            }));
    check(original.cash_budget_lineage.size() == 5U &&
            common_fee_budget_rows == 2U &&
            original_fee.cash_budget_counterparty_id ==
                "synthetic-project-obligor" &&
            !original_fee.cash_budget_provider_claim_id.has_value() &&
            changed_fee.cash_budget_counterparty_id ==
                "alternate-financing-fee-payer" &&
            provider_budget != original.cash_budget_lineage.end() &&
            provider_budget->declared_claim_scenario_id ==
                std::optional<std::string>{"failure-with-provider"} &&
            provider_budget->counterparty_id ==
                "synthetic-catalytic-provider" &&
            provider_budget->provider_claim_id ==
                std::optional<std::string>{"synthetic-provider-claim"},
        "returned lineage retains common/scenario budget scope, payer identity and exact provider claim");
}

void test_probability_measure_is_normalized_once(
    const cf::ClaimLedgerConfig& ledger) {
    for (const double total :
         {0.99999999995, 0.9999999999995}) {
        cf::ClaimLedgerConfig perturbed = ledger;
        auto success = std::find_if(perturbed.scenarios.begin(),
            perturbed.scenarios.end(), [](const auto& scenario) {
                return scenario.scenario_id == "performing-maturity";
            });
        auto failure = std::find_if(perturbed.scenarios.begin(),
            perturbed.scenarios.end(), [](const auto& scenario) {
                return scenario.scenario_id == "failure-with-provider";
            });
        if (success == perturbed.scenarios.end() ||
            failure == perturbed.scenarios.end()) {
            throw std::runtime_error(
                "probability normalization fixture scenarios are missing");
        }
        success->physical_probability =
            cf::claim_ledger_known(total - 0.2);
        failure->physical_probability = cf::claim_ledger_known(0.2);

        const cf::ClaimLedgerPortfolioAdapterResult adapted =
            cf::adapt_claim_ledger_to_portfolio(
                perturbed, adapter_terms());
        double raw_sum = 0.0;
        double normalized_sum = 0.0;
        for (const auto& path : adapted.paths) {
            raw_sum += path.claim_ledger_physical_probability;
            normalized_sum += path.physical_probability;
        }
        const auto& success_path = bridge_path(
            adapted, "performing-maturity");
        check(near(raw_sum, total, 1.0e-12) &&
                near(normalized_sum, 1.0, 1.0e-12) &&
                near(adapted.portfolio_summary
                         .configured_scenario_weight_sum,
                    1.0, 1.0e-12) &&
                near(adapted.expected_contractual_principal_loss_million,
                    0.8 / total, 1.0e-12) &&
                near(adapted.expected_contractual_principal_loss_million,
                    adapted.portfolio_summary.principal_loss_million.mean,
                    1.0e-12) &&
                near(success_path.claim_ledger_physical_probability,
                    total - 0.2, 1.0e-12) &&
                near(success_path.physical_probability,
                    (total - 0.2) / total, 1.0e-12),
            "adapter retains the accepted raw probability measure and uses one normalized Portfolio measure consistently");
    }
}

void test_expanded_budget_lineage_is_preflighted(
    const cf::ClaimLedgerConfig& ledger) {
    const auto performing = std::find_if(ledger.scenarios.begin(),
        ledger.scenarios.end(), [](const auto& scenario) {
            return scenario.scenario_id == "performing-maturity";
        });
    if (performing == ledger.scenarios.end()) {
        throw std::runtime_error(
            "budget-lineage guardrail fixture scenario is missing");
    }

    cf::ClaimLedgerConfig expanded = ledger;
    expanded.scenarios.clear();
    constexpr std::size_t scenario_count = 1'000U;
    expanded.scenarios.reserve(scenario_count);
    for (std::size_t index = 0U; index < scenario_count; ++index) {
        cf::ClaimLedgerScenario scenario = *performing;
        const std::string suffix = "-g" + std::to_string(index);
        scenario.scenario_id = "guard" + std::to_string(index);
        scenario.physical_probability =
            cf::claim_ledger_known(1.0 / scenario_count);
        for (auto& entry : scenario.entries) {
            entry.entry_id += suffix;
        }
        for (auto& event : scenario.covenant_events) {
            event.event_id += suffix;
        }
        expanded.scenarios.push_back(std::move(scenario));
    }

    cf::ClaimLedgerPortfolioAdapterTerms terms;
    terms.project_id = "budget-lineage-guardrail";
    constexpr std::size_t common_budget_count = 1'001U;
    terms.cash_source_budgets.reserve(common_budget_count);
    for (std::size_t index = 0U; index < common_budget_count; ++index) {
        terms.cash_source_budgets.push_back({std::nullopt,
            "guardrail-cash", cf::PortfolioCashSource::Commercial, 12U,
            1.0, "guardrail-payer", std::nullopt});
    }
    expect_invalid(
        [&] {
            (void)cf::adapt_claim_ledger_to_portfolio(expanded, terms);
        },
        "expanded common-budget lineage is rejected before scenario-by-budget work can exceed its guardrail");
}

void test_verified_package_boundary(const cf::ClaimLedgerPackage& package) {
    const cf::ClaimLedgerPortfolioAdapterResult adapted =
        cf::adapt_claim_ledger_package_to_portfolio(
            package, adapter_terms());
    check(adapted.package_lineage.has_value() &&
            adapted.package_lineage->package_id == package.config.package_id &&
            adapted.package_lineage->claim_config_sha256 ==
                package.claim_config_sha256 &&
            adapted.package_lineage->package_status ==
                cf::ClaimLedgerPackageStatus::SyntheticComplete &&
            adapted.package_lineage->admission_basis ==
                cf::ClaimLedgerPortfolioAdmissionBasis::SyntheticMechanics &&
            adapted.package_lineage->source_scope ==
                cf::ClaimLedgerPortfolioSourceScope::DecisionCut &&
            adapted.package_lineage->economic_cluster_id ==
                package.config.economic_cluster_id,
        "package adapter retains the verified root hash, cluster and decision-cut admission route");
    check(adapted.cash_lineage.size() == 15U &&
            std::all_of(adapted.cash_lineage.begin(),
                adapted.cash_lineage.end(), [](const auto& row) {
                    return row.known_at_period == 0U &&
                        !row.source_record_id.empty();
                }) &&
            std::any_of(adapted.cash_lineage.begin(),
                adapted.cash_lineage.end(), [](const auto& row) {
                    return row.entry_id ==
                            "failure-guarantee-principal-cash" &&
                        row.cash_source_id ==
                            std::optional<std::string>{
                                "synthetic-provider-claim"};
                }),
        "every generated cash or principal event retains selected-entry lineage and guarantee source identity");
    check(adapted.portfolio.source_note.find(package.claim_config_sha256) !=
            std::string::npos,
        "persistent portfolio provenance names the immutable claim-package root");

    cf::ClaimLedgerPackage caller_mutation = package;
    caller_mutation.config.package_status =
        cf::ClaimLedgerPackageStatus::RetainedPublicIncomplete;
    caller_mutation.core_config.reset();
    const auto reverified = cf::adapt_claim_ledger_package_to_portfolio(
        caller_mutation, adapter_terms());
    check(reverified.package_lineage.has_value() &&
            reverified.package_lineage->package_status ==
                cf::ClaimLedgerPackageStatus::SyntheticComplete,
        "adapter reloads the immutable root rather than trusting mutable caller fields");
}

void test_adapter_fails_closed(const cf::ClaimLedgerConfig& ledger) {
    cf::ClaimLedgerPortfolioAdapterTerms terms = adapter_terms();
    terms.receipt_source_allocations.pop_back();
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "a positive guarantee receipt without a source allocation is rejected");

    cf::ClaimLedgerConfig scope_collision = ledger;
    auto common_named = std::find_if(scope_collision.scenarios.begin(),
        scope_collision.scenarios.end(), [](const auto& scenario) {
            return scenario.scenario_id == "performing-maturity";
        });
    if (common_named == scope_collision.scenarios.end()) {
        throw std::runtime_error("scope-collision scenario is missing");
    }
    common_named->scenario_id = "COMMON";
    terms = adapter_terms();
    for (auto& allocation : terms.receipt_source_allocations) {
        if (allocation.scenario_id ==
            std::optional<std::string>{"performing-maturity"}) {
            allocation.scenario_id = "COMMON";
        }
    }
    terms.receipt_source_allocations.front().scenario_id = "COMMON";
    for (auto& budget : terms.cash_source_budgets) {
        if (budget.scenario_id ==
            std::optional<std::string>{"performing-maturity"}) {
            budget.scenario_id = "COMMON";
        }
    }
    for (auto& factors : terms.scenario_factor_sets) {
        if (factors.scenario_id == "performing-maturity") {
            factors.scenario_id = "COMMON";
        }
    }
    expect_invalid(
        [&] {
            (void)cf::adapt_claim_ledger_to_portfolio(
                scope_collision, terms);
        },
        "a literal COMMON scenario cannot collide with null/common allocation scope");

    terms = adapter_terms();
    terms.receipt_source_allocations.back().source =
        cf::PortfolioCashSource::Recovery;
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "guarantee cash cannot be relabeled as recovery");

    terms = adapter_terms();
    terms.receipt_source_allocations.front().amount_million = 0.2;
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "receipt source allocations must exhaust exact ledger cash");

    terms = adapter_terms();
    terms.receipt_source_allocations.push_back({std::nullopt,
        "closing-buyer-price", "not-a-receipt",
        cf::PortfolioCashSource::Commercial, 9.0});
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "an allocation to a non-receipt entry is rejected as unused");

    terms = adapter_terms();
    terms.receipt_source_allocations.back().cash_source_id =
        "synthetic-cellular-claim.operations";
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "one source id cannot change taxonomy between commercial and support cash");

    terms = adapter_terms();
    terms.scenario_factor_sets.push_back(
        {"not-a-scenario", {"invented-factor"}});
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "factor tags cannot be assigned to an unknown path");

    cf::ClaimLedgerConfig incomplete = ledger;
    incomplete.scenarios.front().cash_path_status =
        cf::ClaimLedgerCashPathStatus::Incomplete;
    expect_invalid(
        [&] {
            (void)cf::adapt_claim_ledger_to_portfolio(
                incomplete, adapter_terms());
        },
        "an incomplete claim cash path cannot enter the portfolio engine");

    terms = adapter_terms();
    terms.cash_source_budgets.pop_back();
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "a receipt allocation cannot manufacture its own missing cash budget");

    terms = adapter_terms();
    terms.cash_source_budgets.front().amount_million = 0.4;
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "unused source capacity cannot be invented by the one-claim adapter");

    terms = adapter_terms();
    terms.cash_source_budgets.back().provider_claim_id.reset();
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "guarantee cash must preserve its exact provider-claim identity");

    terms = adapter_terms();
    terms.cash_source_budgets.back().counterparty_id =
        "not-the-contractual-provider";
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "guarantee cash counterparty must match the provider claim");

    terms = adapter_terms();
    terms.receipt_source_allocations[1U].cash_source_id =
        "synthetic-provider-claim";
    terms.receipt_source_allocations[1U].source =
        cf::PortfolioCashSource::ExplicitSupport;
    terms.cash_source_budgets[1U].amount_million = 1.0;
    terms.cash_source_budgets.push_back(
        {"performing-maturity", "synthetic-provider-claim",
            cf::PortfolioCashSource::ExplicitSupport, 12U, 10.0,
            "synthetic-catalytic-provider",
            std::optional<std::string>{"synthetic-provider-claim"}});
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "a provider-bound guarantee budget cannot service ordinary principal cash");

    terms = adapter_terms();
    terms.annual_physical_hurdle_rate = -0.01;
    expect_invalid(
        [&] { (void)cf::adapt_claim_ledger_to_portfolio(ledger, terms); },
        "an unsupported portfolio hurdle fails rather than being normalized");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: claim_ledger_portfolio_adapter_tests "
                     "<synthetic-claim.cfg>\n";
        return 2;
    }
    try {
        const cf::ClaimLedgerPackage package =
            cf::load_claim_ledger_package(std::filesystem::path(argv[1]));
        if (!package.core_config.has_value()) {
            throw std::runtime_error(
                "synthetic claim package did not expose a decision config");
        }
        test_exact_cash_and_loss_basis_bridge(*package.core_config);
        test_performing_premium_issue_is_not_principal_loss(
            *package.core_config);
        test_budget_identity_is_retained(*package.core_config);
        test_probability_measure_is_normalized_once(
            *package.core_config);
        test_expanded_budget_lineage_is_preflighted(
            *package.core_config);
        test_verified_package_boundary(package);
        test_adapter_fails_closed(*package.core_config);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " claim-ledger portfolio adapter test(s) failed\n";
        return 1;
    }
    std::cout << "all claim-ledger portfolio adapter tests passed\n";
    return 0;
}
