// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_adapters.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(
    double first, double second, double relative_tolerance = 1.0e-9) {
    return std::abs(first - second) <= relative_tolerance *
        std::max({1.0, std::abs(first), std::abs(second)});
}

void expect_invalid_argument(
    const std::function<void()>& operation, const std::string& message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        check(true, message);
    } catch (...) {
        check(false, message + " (wrong exception type)");
    }
}

[[nodiscard]] cf::StagedCapitalCase base_case() {
    cf::StagedCapitalCase scenario_case;
    scenario_case.completion_value_million = 200.0;
    scenario_case.recovery_value_million = 0.0;
    scenario_case.recovery_delay_months = 0U;
    scenario_case.required_workout_cost_million = 0.0;
    scenario_case.phases = {
        cf::StagedCapitalPhaseCase{50.0, 150.0,
            cf::CertificationDecision::Certified, true},
        cf::StagedCapitalPhaseCase{100.0, 100.0,
            cf::CertificationDecision::Certified, true},
    };
    return scenario_case;
}

[[nodiscard]] cf::StagedCapitalConfig adapter_fixture() {
    cf::StagedCapitalConfig config;
    config.scenario_label = "staged adapter actual-path fixture";
    config.source_note =
        "synthetic unit-test paths, not observed or investable claims";
    config.currency_label = "TEST";
    config.monetary_basis = "constant synthetic millions at close";
    config.terms.provider_commitment_million = 80.0;
    config.terms.sponsor_construction_commitment_million = 120.0;
    config.terms.provider_cost_share = 0.40;
    config.terms.annual_pik_rate = 0.12;
    config.terms.claim_cap_multiple = 2.0;
    config.terms.annual_commitment_fee_rate = 0.12;
    config.terms.upfront_fee_million = 2.0;
    config.terms.provider_hurdle_rate = 0.10;
    config.terms.sponsor_discount_rate = 0.08;
    config.terms.protected_workout_reserve_million = 1.0;
    config.phases = {
        cf::StagedCapitalPhaseTerms{"engineering", 12U, 40.0},
        cf::StagedCapitalPhaseTerms{"commissioning", 12U, 40.0},
    };

    cf::StagedCapitalCase completion = base_case();
    completion.id = "completion-path";
    completion.weight = 0.35;

    cf::StagedCapitalCase sale_completion = base_case();
    sale_completion.id = "sale-completion-path";
    sale_completion.weight = 0.15;

    cf::StagedCapitalCase failure = base_case();
    failure.id = "milestone-failure-path";
    failure.weight = 0.3;
    failure.completion_value_million = 0.0;
    failure.recovery_value_million = 10.0;
    failure.recovery_delay_months = 6U;
    failure.phases.front().certification =
        cf::CertificationDecision::FinalFailure;

    cf::StagedCapitalCase provider_nonperformance = base_case();
    provider_nonperformance.id = "provider-nonperformance-path";
    provider_nonperformance.weight = 0.2;
    provider_nonperformance.recovery_value_million = 10.0;
    provider_nonperformance.recovery_delay_months = 6U;
    provider_nonperformance.phases.front().provider_funds = false;

    config.cases = {
        completion, sale_completion, failure, provider_nonperformance};
    return config;
}

template <typename Range, typename Predicate>
[[nodiscard]] const typename Range::value_type& require_one(
    const Range& range, Predicate predicate, std::string_view description) {
    const auto found = std::find_if(range.begin(), range.end(), predicate);
    if (found == range.end()) {
        throw std::runtime_error(
            std::string("missing test fixture object: ") +
            std::string(description));
    }
    return *found;
}

[[nodiscard]] const cf::StagedCapitalPathResult& staged_path(
    const cf::StagedCapitalSummary& summary, std::string_view id) {
    return require_one(summary.cases,
        [id](const cf::StagedCapitalPathResult& path) {
            return path.case_id == id;
        }, id);
}

[[nodiscard]] std::vector<cf::StagedCompletionSourceAllocation>
valid_completion_allocations(const cf::StagedCapitalSummary& summary) {
    const double first_repayment = staged_path(
        summary, "completion-path").provider_nominal_recovery_million;
    const double commercial = first_repayment * 0.40;
    const double licensing = first_repayment - commercial;
    const double sale_repayment = staged_path(
        summary, "sale-completion-path").provider_nominal_recovery_million;
    // Deliberately supply the first case out of taxonomy order; the adapter
    // must emit deterministic source records independent of caller ordering.
    return {
        {"completion-path", "royalty-platform",
            cf::PortfolioCashSource::LicensingRoyalty, licensing},
        {"completion-path", "offtaker-alpha",
            cf::PortfolioCashSource::Commercial, commercial * 0.50},
        {"completion-path", "offtaker-beta",
            cf::PortfolioCashSource::Commercial, commercial * 0.50},
        {"sale-completion-path", "buyer-gamma",
            cf::PortfolioCashSource::ExitSale, sale_repayment},
    };
}

[[nodiscard]] const cf::JointScenario& configured_scenario(
    const cf::PortfolioConfig& config, std::string_view id) {
    return require_one(config.joint_scenarios,
        [id](const cf::JointScenario& scenario) {
            return scenario.id == id;
        }, id);
}

[[nodiscard]] const cf::JointScenarioResult& evaluated_scenario(
    const cf::PortfolioSummary& summary, std::string_view id) {
    return require_one(summary.scenarios,
        [id](const cf::JointScenarioResult& scenario) {
            return scenario.scenario_id == id;
        }, id);
}

[[nodiscard]] const cf::ScenarioCashSource& configured_source(
    const cf::JointScenario& scenario, std::string_view id) {
    return require_one(scenario.cash_sources,
        [id](const cf::ScenarioCashSource& source) {
            return source.id == id;
        }, id);
}

[[nodiscard]] double monthly_source_budget(
    const cf::ScenarioCashSource& source, std::size_t month) {
    long double total = 0.0L;
    for (const cf::MonthlyAmount& amount : source.cash_available) {
        if (amount.month == month) {
            total += static_cast<long double>(amount.amount_million);
        }
    }
    return static_cast<double>(total);
}

[[nodiscard]] double monthly_source_receipts(
    const cf::ProjectJointPath& path, std::string_view source_id,
    std::size_t month) {
    long double total = 0.0L;
    for (const cf::InvestorReceipt& receipt : path.investor_receipts) {
        if (receipt.cash_source_id == source_id && receipt.month == month) {
            total += static_cast<long double>(receipt.amount_million);
        }
    }
    return static_cast<double>(total);
}

void check_exact_source_budgets(const cf::JointScenario& scenario) {
    const cf::ProjectJointPath& path = scenario.project_paths.front();
    for (const cf::ScenarioCashSource& source : scenario.cash_sources) {
        for (std::size_t month = 0U; month <= 24U; ++month) {
            check(near(monthly_source_budget(source, month),
                       monthly_source_receipts(path, source.id, month)),
                scenario.id + ": cash-source budget exactly funds mapped receipts by month");
        }
    }
}

void test_actual_paths_translate_and_reconcile() {
    const cf::StagedCapitalConfig staged = adapter_fixture();
    const cf::StagedCapitalSummary staged_summary =
        cf::evaluate_staged_capital_cases(staged);
    const auto completion_allocations =
        valid_completion_allocations(staged_summary);
    const cf::PortfolioConfig adapted =
        cf::adapt_staged_capital_to_portfolio(staged,
            "cultivated-meat-line", cf::ProjectStage::FirstIndustrial,
            completion_allocations);
    const cf::PortfolioSummary portfolio = cf::evaluate_portfolio(adapted);

    check(adapted.model_version == cf::kPortfolioModelVersion,
        "adapter emits the portfolio model version rather than copying the staged version");
    check(adapted.scenario_label == staged.scenario_label &&
            adapted.source_note == staged.source_note &&
            adapted.currency_label == staged.currency_label &&
            adapted.monetary_basis == staged.monetary_basis &&
            adapted.synthetic_inputs == staged.synthetic_inputs,
        "adapter preserves metadata and the synthetic-input boundary");
    check(adapted.horizon_months == 24U,
        "adapter horizon covers every staged ledger and recovery month");
    check(near(adapted.annual_physical_hurdle_rate,
                   staged.terms.provider_hurdle_rate) &&
            adapted.projects.size() == 1U &&
            adapted.projects.front().id == "cultivated-meat-line" &&
            adapted.projects.front().stage ==
                cf::ProjectStage::FirstIndustrial &&
            near(adapted.projects.front().commitment_million,
                staged.terms.provider_commitment_million),
        "adapter preserves the provider hurdle and maps legal commitment and stage explicitly");
    check(adapted.loss_layers.empty() &&
            adapted.joint_scenarios.size() == staged.cases.size(),
        "adapter adds neither loss-layer packaging nor hidden cases");
    check(near(portfolio.configured_scenario_weight_sum,
                   staged_summary.configured_case_weight_sum),
        "adapted raw scenario weight sum equals the staged actual-case weight sum");

    for (const cf::StagedCapitalPathResult& path : staged_summary.cases) {
        const cf::JointScenario& scenario =
            configured_scenario(adapted, path.case_id);
        const cf::JointScenarioResult& result =
            evaluated_scenario(portfolio, path.case_id);
        check(near(scenario.weight, path.weight),
            path.case_id + ": declared path weight is unchanged");
        check(scenario.pool_costs.empty() &&
                scenario.project_paths.size() == 1U &&
                scenario.project_paths.front().resolution ==
                    cf::ProjectPathResolution::Resolved,
            path.case_id + ": path is resolved and has no invented pool costs");
        check(scenario.factor_tags.empty(),
            path.case_id +
                ": adapter does not misstate provenance or outcomes as common risk factors");
        check(near(result.total_draws_million,
                       path.total_provider_draws_million) &&
                near(result.principal_loss_million,
                    path.provider_principal_loss_million) &&
                near(result.outstanding_principal_million, 0.0) &&
                near(result.npv_million,
                    path.provider_npv_after_upfront_fee_million),
            path.case_id + ": draws, resolved loss, and provider NPV reconcile pathwise");
        for (const cf::MonthlyInvestorCashFlow& monthly :
             result.monthly_cash_flows) {
            long double staged_provider_cash = 0.0L;
            for (const cf::CapitalCashLedgerEntry& entry : path.cash_ledger) {
                if (entry.month == monthly.month) {
                    staged_provider_cash += static_cast<long double>(
                        entry.posting.provider_million);
                }
            }
            check(near(monthly.net_cash_flow_million,
                       static_cast<double>(staged_provider_cash)),
                path.case_id +
                    ": dated provider net cash is preserved without equating different liquidity conventions");
        }
        check_exact_source_budgets(scenario);
    }

    const cf::StagedCapitalPathResult& completion_path =
        staged_path(staged_summary, "completion-path");
    const cf::JointScenario& completion =
        configured_scenario(adapted, "completion-path");
    const cf::ProjectJointPath& completion_project =
        completion.project_paths.front();
    const cf::ScenarioCashSource& commercial_source = configured_source(
        completion, "offtaker-alpha");
    const cf::ScenarioCashSource& second_commercial_source =
        configured_source(completion, "offtaker-beta");
    const cf::ScenarioCashSource& licensing_source = configured_source(
        completion, "royalty-platform");
    check(commercial_source.kind == cf::PortfolioCashSource::Commercial &&
            second_commercial_source.kind ==
                cf::PortfolioCashSource::Commercial &&
            licensing_source.kind ==
                cf::PortfolioCashSource::LicensingRoyalty,
        "one completed path retains multiple payer IDs, including two in one taxonomy");
    const cf::ScenarioCashSource& sale_completion_source = configured_source(
        configured_scenario(adapted, "sale-completion-path"),
        "buyer-gamma");
    check(sale_completion_source.kind ==
            cf::PortfolioCashSource::ExitSale,
        "different completed scenarios retain different explicit cash provenance");
    const cf::InvestorReceipt& commercial_receipt = require_one(
        completion_project.investor_receipts,
        [](const cf::InvestorReceipt& receipt) {
            return receipt.cash_source_id == "offtaker-alpha";
        }, "commercial completion repayment receipt");
    const cf::InvestorReceipt& second_commercial_receipt = require_one(
        completion_project.investor_receipts,
        [](const cf::InvestorReceipt& receipt) {
            return receipt.cash_source_id == "offtaker-beta";
        }, "second commercial completion repayment receipt");
    const cf::InvestorReceipt& licensing_receipt = require_one(
        completion_project.investor_receipts,
        [](const cf::InvestorReceipt& receipt) {
            return receipt.cash_source_id == "royalty-platform";
        }, "licensing completion repayment receipt");
    check(near(commercial_receipt.amount_million +
                   second_commercial_receipt.amount_million +
                   licensing_receipt.amount_million,
                   completion_path.provider_nominal_recovery_million) &&
            near(commercial_receipt.principal_component_million +
                    second_commercial_receipt
                        .principal_component_million +
                    licensing_receipt.principal_component_million,
                60.0) &&
            near(commercial_receipt.principal_component_million /
                    commercial_receipt.amount_million,
                second_commercial_receipt.principal_component_million /
                    second_commercial_receipt.amount_million) &&
            near(second_commercial_receipt.principal_component_million /
                    second_commercial_receipt.amount_million,
                licensing_receipt.principal_component_million /
                    licensing_receipt.amount_million) &&
            completion_path.provider_nominal_recovery_million > 60.0,
        "mixed completion sources exhaust repayment and share funded principal versus PIK pro rata");
    check(near(completion_path.provider_npv_after_upfront_fee_million,
            completion_path.provider_npv_before_upfront_fee_million + 2.0),
        "adapted path NPV includes the charged upfront sponsor fee");

    const cf::JointScenario& failure =
        configured_scenario(adapted, "milestone-failure-path");
    const cf::ScenarioCashSource& recovery_source = configured_source(
        failure, "cultivated-meat-line.recovery");
    const cf::InvestorReceipt& recovery_receipt = require_one(
        failure.project_paths.front().investor_receipts,
        [](const cf::InvestorReceipt& receipt) {
            return receipt.cash_source_id ==
                "cultivated-meat-line.recovery";
        }, "failure recovery receipt");
    check(recovery_source.kind == cf::PortfolioCashSource::Recovery &&
            near(recovery_receipt.amount_million, 10.0) &&
            near(recovery_receipt.principal_component_million, 10.0),
        "failed paths always classify terminal cash as recovery and retire only funded principal");
    const cf::StagedCapitalPathResult& failure_path =
        staged_path(staged_summary, "milestone-failure-path");
    check(failure_path.provider_claim_writeoff_million >
                failure_path.provider_principal_loss_million &&
            near(evaluated_scenario(portfolio, "milestone-failure-path")
                    .principal_loss_million,
                failure_path.provider_principal_loss_million),
        "portfolio loss is cash-principal shortfall and does not relabel unpaid PIK claim writeoff as principal loss");

    for (const cf::JointScenario& scenario : adapted.joint_scenarios) {
        for (const cf::InvestorReceipt& receipt :
             scenario.project_paths.front().investor_receipts) {
            if (receipt.cash_source_id ==
                "cultivated-meat-line.sponsor-fee") {
                check(near(receipt.principal_component_million, 0.0),
                    scenario.id +
                        ": upfront and commitment fees are non-principal SponsorFee receipts");
            }
        }
    }

    const cf::JointScenario& provider_nonperformance =
        configured_scenario(adapted, "provider-nonperformance-path");
    check(provider_nonperformance.project_paths.front().capital_draws.empty() &&
            provider_nonperformance.cash_sources.size() == 1U &&
            provider_nonperformance.cash_sources.front().kind ==
                cf::PortfolioCashSource::SponsorFee,
        "provider nonperformance remains an actual zero-draw path with only its charged close fee");

    check(near(portfolio.principal_loss_million.mean,
                   staged_summary.expected_provider_principal_loss_million),
        "adapted expected principal loss equals the staged actual-case aggregate");
    long double weighted_actual_npv = 0.0L;
    long double weight_sum = 0.0L;
    for (const cf::StagedCapitalPathResult& path : staged_summary.cases) {
        weighted_actual_npv += static_cast<long double>(path.weight) *
            static_cast<long double>(
                path.provider_npv_after_upfront_fee_million);
        weight_sum += static_cast<long double>(path.weight);
    }
    const double expected_actual_npv =
        static_cast<double>(weighted_actual_npv / weight_sum);
    check(near(portfolio.npv_million.mean, expected_actual_npv),
        "adapted expected NPV weights actual configured paths, including provider nonperformance");
    check(!near(expected_actual_npv,
            staged_summary
                .expected_provider_npv_after_charged_upfront_fee_million,
            1.0e-6),
        "actual-path NPV is not confused with the all-provider-performs fee-replay sensitivity");

    std::array<long double, 8U> expected_source_nominal{};
    std::array<long double, 8U> expected_source_pv{};
    const long double configured_weight = static_cast<long double>(
        staged_summary.configured_case_weight_sum);
    const auto add_expected_source = [&](cf::PortfolioCashSource source,
                                         double amount,
                                         std::size_t month,
                                         long double normalized_weight) {
        const std::size_t index = static_cast<std::size_t>(source);
        const long double weighted = normalized_weight *
            static_cast<long double>(amount);
        expected_source_nominal[index] += weighted;
        expected_source_pv[index] += weighted /
            std::pow(1.0L + static_cast<long double>(
                    staged.terms.provider_hurdle_rate),
                static_cast<long double>(month) / 12.0L);
    };
    for (const cf::StagedCapitalPathResult& path : staged_summary.cases) {
        const long double normalized_weight =
            static_cast<long double>(path.weight) / configured_weight;
        for (const cf::CapitalCashLedgerEntry& entry : path.cash_ledger) {
            if (!(entry.posting.provider_million > 0.0)) {
                continue;
            }
            if (entry.kind == cf::CapitalCashFlowKind::UpfrontFee ||
                entry.kind == cf::CapitalCashFlowKind::CommitmentFee) {
                add_expected_source(cf::PortfolioCashSource::SponsorFee,
                    entry.posting.provider_million, entry.month,
                    normalized_weight);
            } else if (entry.kind ==
                cf::CapitalCashFlowKind::ProviderRepayment) {
                if (path.outcome == cf::StagedCapitalOutcome::Completed) {
                    for (const auto& allocation : completion_allocations) {
                        if (allocation.case_id == path.case_id) {
                            add_expected_source(allocation.source,
                                allocation.amount_million, entry.month,
                                normalized_weight);
                        }
                    }
                } else {
                    add_expected_source(cf::PortfolioCashSource::Recovery,
                        entry.posting.provider_million, entry.month,
                        normalized_weight);
                }
            }
        }
    }
    check(portfolio.expected_return_sources.size() == 8U,
        "adapted expected return-source output retains the full taxonomy");
    for (const cf::ReturnSourceTotal& source :
         portfolio.expected_return_sources) {
        const std::size_t index = static_cast<std::size_t>(source.source);
        check(near(source.nominal_million,
                       static_cast<double>(expected_source_nominal[index])) &&
                near(source.present_value_million,
                    static_cast<double>(expected_source_pv[index])),
            "expected return-source nominal and PV totals reconcile independently by taxonomy");
    }
}

void test_principal_liquidity_horizon_and_common_domain_boundaries() {
    cf::StagedCapitalConfig liquidity = adapter_fixture();
    liquidity.terms.provider_commitment_million = 40.0;
    liquidity.terms.sponsor_construction_commitment_million = 60.0;
    liquidity.phases = {
        cf::StagedCapitalPhaseTerms{"single-stage", 12U, 40.0}};
    cf::StagedCapitalCase completion;
    completion.id = "same-month-liquidity";
    completion.weight = 1.0;
    completion.completion_value_million = 200.0;
    completion.phases = {cf::StagedCapitalPhaseCase{100.0, 100.0,
        cf::CertificationDecision::Certified, true}};
    liquidity.cases = {completion};
    const cf::StagedCapitalSummary liquidity_staged =
        cf::evaluate_staged_capital_cases(liquidity);
    const auto& liquidity_path = liquidity_staged.cases.front();
    const cf::PortfolioConfig liquidity_adapted =
        cf::adapt_staged_capital_to_portfolio(liquidity,
            "liquidity-project", cf::ProjectStage::Demonstration,
            {{"same-month-liquidity", "completion-payer",
                cf::PortfolioCashSource::Commercial,
                liquidity_path.provider_nominal_recovery_million}});
    const cf::PortfolioSummary liquidity_portfolio =
        cf::evaluate_portfolio(liquidity_adapted);
    const cf::JointScenarioResult& liquidity_result =
        liquidity_portfolio.scenarios.front();
    check(near(liquidity_path.peak_provider_net_cash_outlay_million, 38.0) &&
            near(liquidity_result.peak_same_month_funding_need_million,
                40.0) &&
            near(liquidity_result.peak_cumulative_net_outlay_million,
                40.0),
        "portfolio liquidity conservatively funds the month-zero draw before releasing the staged upfront-fee receipt (staged=" +
            std::to_string(
                liquidity_path.peak_provider_net_cash_outlay_million) +
            ", same-month=" +
            std::to_string(
                liquidity_result.peak_same_month_funding_need_million) +
            ", cumulative=" +
            std::to_string(
                liquidity_result.peak_cumulative_net_outlay_million) + ")");

    cf::StagedCapitalConfig pik_writeoff = liquidity;
    pik_writeoff.cases.front().id = "principal-repaid-pik-written-off";
    pik_writeoff.cases.front().completion_value_million = 0.0;
    pik_writeoff.cases.front().recovery_value_million = 40.0;
    pik_writeoff.cases.front().phases.front().certification =
        cf::CertificationDecision::FinalFailure;
    const cf::StagedCapitalSummary writeoff_staged =
        cf::evaluate_staged_capital_cases(pik_writeoff);
    const cf::StagedCapitalPathResult& writeoff_path =
        writeoff_staged.cases.front();
    const cf::PortfolioSummary writeoff_portfolio = cf::evaluate_portfolio(
        cf::adapt_staged_capital_to_portfolio(pik_writeoff,
            "writeoff-project", cf::ProjectStage::Demonstration, {}));
    check(near(writeoff_path.provider_nominal_recovery_million,
                   writeoff_path.total_provider_draws_million) &&
            near(writeoff_path.provider_principal_loss_million, 0.0) &&
            writeoff_path.provider_claim_writeoff_million > 0.0 &&
            near(writeoff_portfolio.principal_loss_million.mean, 0.0) &&
            near(writeoff_portfolio.principal_impairment_probability, 0.0),
        "full cash-principal return has zero pool impairment even when unpaid PIK is written off");

    cf::StagedCapitalConfig long_horizon = liquidity;
    long_horizon.terms.annual_pik_rate = 0.0;
    long_horizon.terms.annual_commitment_fee_rate = 0.0;
    long_horizon.terms.upfront_fee_million = 0.0;
    long_horizon.phases.front().duration_months = 1'200U;
    long_horizon.cases.front().id = "maximum-staged-recovery-horizon";
    long_horizon.cases.front().completion_value_million = 0.0;
    long_horizon.cases.front().recovery_value_million = 40.0;
    long_horizon.cases.front().recovery_delay_months = 1'200U;
    long_horizon.cases.front().phases.front().certification =
        cf::CertificationDecision::FinalFailure;
    const cf::PortfolioConfig long_horizon_adapted =
        cf::adapt_staged_capital_to_portfolio(long_horizon,
            "long-horizon-project", cf::ProjectStage::Research, {});
    check(long_horizon_adapted.horizon_months == 2'400U,
        "adapter horizon explicitly covers the outcome and maximum valid delayed recovery month");

    cf::StagedCapitalConfig outside_money_domain = adapter_fixture();
    outside_money_domain.terms.provider_commitment_million = 1'000'001.0;
    outside_money_domain.phases[0].provider_stage_cap_million = 500'000.5;
    outside_money_domain.phases[1].provider_stage_cap_million = 500'000.5;
    cf::StagedCapitalCase zero_draw = base_case();
    zero_draw.id = "large-legal-commitment";
    zero_draw.weight = 1.0;
    zero_draw.completion_value_million = 0.0;
    for (cf::StagedCapitalPhaseCase& phase : zero_draw.phases) {
        phase.actual_eligible_cost_million = 0.0;
        phase.estimated_cost_to_complete_million = 0.0;
    }
    outside_money_domain.cases = {zero_draw};
    cf::validate_staged_capital_config(outside_money_domain);
    expect_invalid_argument(
        [&outside_money_domain] {
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(
                outside_money_domain, "large-project",
                cf::ProjectStage::FirstIndustrial, {}));
        },
        "staged amounts above the portfolio 1e6-million common domain are rejected rather than scaled");
}

void test_explicit_allocation_and_identifier_boundaries() {
    const cf::StagedCapitalConfig staged = adapter_fixture();
    const cf::StagedCapitalSummary staged_summary =
        cf::evaluate_staged_capital_cases(staged);
    const std::vector<cf::StagedCompletionSourceAllocation> valid =
        valid_completion_allocations(staged_summary);

    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().source = cf::PortfolioCashSource::Recovery;
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "recovery cannot be allocated as completion proceeds");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().source = cf::PortfolioCashSource::SponsorFee;
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "sponsor fees cannot be allocated as completion proceeds");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().source =
                static_cast<cf::PortfolioCashSource>(255);
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "unknown completion-source taxonomy values are rejected");

    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.erase(std::remove_if(allocations.begin(),
                                  allocations.end(),
                                  [](const auto& allocation) {
                                      return allocation.case_id ==
                                          "sale-completion-path";
                                  }),
                allocations.end());
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "every positive-repayment completed case needs source allocations");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.push_back(valid.front());
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "duplicate case and cash-source-id allocation rows are rejected");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.push_back({"milestone-failure-path",
                "failure-offtaker", cf::PortfolioCashSource::Commercial,
                1.0});
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "allocations for non-completed paths are rejected as extraneous");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.push_back({"unknown-path", "unknown-offtaker",
                cf::PortfolioCashSource::Commercial, 1.0});
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "allocations for unknown paths are rejected as extraneous");

    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().amount_million -= 1.0;
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "under-allocation of a completed provider repayment is rejected");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().amount_million += 1.0;
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "over-allocation of a completed provider repayment is rejected");
    for (const double invalid_amount : {0.0, -1.0,
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity()}) {
        expect_invalid_argument(
            [&staged, &valid, invalid_amount] {
                auto allocations = valid;
                allocations.front().amount_million = invalid_amount;
                static_cast<void>(
                    cf::adapt_staged_capital_to_portfolio(staged, "project",
                        cf::ProjectStage::Pilot, allocations));
            },
            "completion allocations require strictly positive finite amounts");
    }

    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.back().cash_source_id =
                allocations.front().cash_source_id;
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "one cash-source id cannot change taxonomy across cases");
    const double completion_repayment = staged_path(
        staged_summary, "completion-path").provider_nominal_recovery_million;
    const double sale_repayment = staged_path(staged_summary,
        "sale-completion-path").provider_nominal_recovery_million;
    const cf::PortfolioConfig shared_source =
        cf::adapt_staged_capital_to_portfolio(staged, "project",
            cf::ProjectStage::Pilot,
            {{"completion-path", "shared-offtaker",
                 cf::PortfolioCashSource::Commercial,
                 completion_repayment},
                {"sale-completion-path", "shared-offtaker",
                    cf::PortfolioCashSource::Commercial,
                    sale_repayment}});
    check(configured_source(configured_scenario(
                                shared_source, "completion-path"),
              "shared-offtaker")
                  .kind == cf::PortfolioCashSource::Commercial &&
            configured_source(configured_scenario(
                                  shared_source, "sale-completion-path"),
                "shared-offtaker")
                    .kind == cf::PortfolioCashSource::Commercial,
        "one declared source id may recur across cases when its taxonomy remains consistent");

    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().cash_source_id = "unsafe|source";
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "caller-supplied completion source IDs must be safe portfolio identifiers");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().cash_source_id = "project.sponsor-fee";
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "completion source IDs cannot change the generated sponsor-fee ID taxonomy");
    expect_invalid_argument(
        [&staged, &valid] {
            auto allocations = valid;
            allocations.front().cash_source_id = "project.recovery";
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "project", cf::ProjectStage::Pilot, allocations));
        },
        "completion source IDs cannot change the generated recovery ID taxonomy");

    const std::string too_long_id(117U, 'p');
    expect_invalid_argument(
        [&staged, &too_long_id, &valid] {
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                too_long_id, cf::ProjectStage::Pilot, valid));
        },
        "project IDs must leave room for transparent source suffixes rather than being hashed");
    expect_invalid_argument(
        [&staged, &valid] {
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(staged,
                "unsafe|project", cf::ProjectStage::Pilot, valid));
        },
        "adapter-generated identifiers retain the portfolio safe-ID boundary");

    cf::StagedCapitalConfig non_synthetic = staged;
    non_synthetic.synthetic_inputs = false;
    expect_invalid_argument(
        [&non_synthetic, &valid] {
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(
                non_synthetic, "project", cf::ProjectStage::Pilot,
                valid));
        },
        "staged semantic validation propagates before adaptation");

    cf::StagedCapitalConfig zero_repayment = staged;
    cf::StagedCapitalCase zero_case = base_case();
    zero_case.id = "zero-repayment-completion";
    zero_case.weight = 1.0;
    zero_case.completion_value_million = 0.0;
    for (cf::StagedCapitalPhaseCase& phase : zero_case.phases) {
        phase.actual_eligible_cost_million = 0.0;
        phase.estimated_cost_to_complete_million = 0.0;
    }
    zero_repayment.cases = {zero_case};
    const cf::PortfolioConfig zero_adapted =
        cf::adapt_staged_capital_to_portfolio(zero_repayment,
            "zero-draw-project", cf::ProjectStage::Research, {});
    check(zero_adapted.joint_scenarios.size() == 1U &&
            zero_adapted.joint_scenarios.front()
                .project_paths.front().capital_draws.empty(),
        "a completed path with no provider repayment requires no invented cash classification");
    expect_invalid_argument(
        [&zero_repayment] {
            static_cast<void>(cf::adapt_staged_capital_to_portfolio(
                zero_repayment, "zero-draw-project",
                cf::ProjectStage::Research,
                {{"zero-repayment-completion", "zero-offtaker",
                    cf::PortfolioCashSource::Commercial, 1.0}}));
        },
        "an allocation for a zero-repayment completion is extraneous");
}

} // namespace

int main() {
    test_actual_paths_translate_and_reconcile();
    test_principal_liquidity_horizon_and_common_domain_boundaries();
    test_explicit_allocation_and_identifier_boundaries();
    if (failures != 0) {
        std::cerr << failures << " portfolio adapter test(s) failed\n";
        return 1;
    }
    std::cout << "portfolio adapter tests passed\n";
    return 0;
}
