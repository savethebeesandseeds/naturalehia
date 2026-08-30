// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_credit_stress.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

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

[[nodiscard]] cf::JointScenario make_scenario(
    std::string id, double weight, double principal_return) {
    cf::JointScenario scenario;
    scenario.id = std::move(id);
    scenario.weight = weight;
    cf::ScenarioCashSource source;
    source.id = "commercial";
    source.kind = cf::PortfolioCashSource::Commercial;
    source.cash_available = {cf::MonthlyAmount{12U, principal_return}};
    scenario.cash_sources = {source};

    cf::ProjectJointPath path;
    path.project_id = "project";
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.capital_draws = {cf::MonthlyAmount{0U, 20.0}};
    path.investor_receipts = {cf::InvestorReceipt{
        12U, "commercial", principal_return, principal_return}};
    scenario.project_paths = {path};
    return scenario;
}

[[nodiscard]] cf::PortfolioConfig portfolio_terms() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "two-state provider credit stress fixture";
    portfolio.source_note = "synthetic unit-test cash paths";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant synthetic units at close";
    portfolio.horizon_months = 12U;
    portfolio.annual_physical_hurdle_rate = 0.10;
    portfolio.projects = {cf::PortfolioProject{
        "project", cf::ProjectStage::Pilot, 20.0}};
    portfolio.joint_scenarios = {
        make_scenario("low-loss", 0.5, 16.0),
        make_scenario("high-loss", 0.5, 4.0),
    };
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig ambiguity_terms() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "two-state provider credit envelope";
    ambiguity.source_note = "synthetic bounded-simplex unit test";
    ambiguity.scenario_probabilities = {
        {"low-loss", 0.25, 0.50, 0.75},
        {"high-loss", 0.25, 0.50, 0.75},
    };
    return ambiguity;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_terms() {
    cf::SuccessParticipationConfig participation;
    participation.scenario_label = "provider credit underlying participation";
    participation.source_note = "synthetic contractual scaling assertion";
    participation.selected_nonprincipal_cash_is_contractually_scalable = true;
    participation.scalable_source_kinds = {
        cf::PortfolioCashSource::Commercial};
    participation.target_worst_expected_npv_million = -10.0;
    return participation;
}

[[nodiscard]] cf::PooledLossProtectionConfig protection_terms() {
    cf::PooledLossProtectionConfig protection;
    protection.scenario_label = "provider credit selected protection";
    protection.source_note = "synthetic terminal-loss claim assertion";
    protection.provider_id = "test-provider";
    protection.portfolio_principal_loss_is_contractual_reference_amount = true;
    protection.gross_project_loss_remains_visible = true;
    protection.support_is_assumed_fully_funded_and_performing_in_all_scenarios =
        true;
    protection.premium_is_upfront_at_month_zero = true;
    protection.underlying_success_participation_fraction = 1.0;
    protection.settlement_month = 12U;
    protection.support_cap_million = 10.0;
    protection.provider_annual_physical_hurdle_rate = 0.10;
    return protection;
}

[[nodiscard]] cf::ProviderPriceLadderConfig pricing_terms(
    double coverage = 0.5) {
    cf::ProviderPriceLadderConfig pricing;
    pricing.scenario_label = "provider credit full-performance price";
    pricing.source_note = "synthetic provider cost ladder";
    pricing.coverage_selection =
        cf::ProviderPriceCoverageSelection::ExplicitCoverageFraction;
    pricing.explicit_coverage_fraction = coverage;
    pricing.cost_bases_use_contractual_maximum_exposure = true;
    pricing.collateral_and_capital_are_held_until_settlement = true;
    pricing.variable_claim_expense_is_paid_at_claim_settlement = true;
    pricing.fixed_expense_and_target_profit_are_month_zero_values = true;
    pricing.incremental_cost_terms_are_separate_and_nonduplicative = true;
    pricing.collateral_fraction_of_contractual_maximum_exposure = 3.0 / 11.0;
    pricing.collateral_annual_effective_funding_rate = 0.10;
    pricing.collateral_annual_effective_yield_rate = 0.10;
    pricing.risk_capital_fraction_of_contractual_maximum_exposure = 0.0;
    pricing.risk_capital_annual_effective_charge_rate = 0.0;
    pricing.fixed_expense_upfront_million = 0.5;
    pricing.variable_claim_expense_fraction = 0.10;
    pricing.target_profit_upfront_million = 1.5;
    return pricing;
}

[[nodiscard]] cf::ProviderCreditOutcomeConfig performing(
    std::string id, double weight) {
    cf::ProviderCreditOutcomeConfig outcome;
    outcome.outcome_id = std::move(id);
    outcome.conditional_weight = weight;
    outcome.provider_performs = true;
    return outcome;
}

[[nodiscard]] cf::ProviderCreditOutcomeConfig defaulting(
    std::string id, double weight, double collateral_realization = 1.0) {
    cf::ProviderCreditOutcomeConfig outcome;
    outcome.outcome_id = std::move(id);
    outcome.conditional_weight = weight;
    outcome.provider_performs = false;
    outcome.collateral_realization_fraction = collateral_realization;
    outcome.unsecured_recovery_fraction = 0.40;
    outcome.unsecured_recovery_delay_months = 12U;
    return outcome;
}

[[nodiscard]] cf::ProviderCreditStressConfig credit_terms() {
    cf::ProviderCreditStressConfig credit;
    credit.scenario_label = "provider credit wrong-way stress";
    credit.source_note = "synthetic fixed conditional provider states";
    credit.provider_id = "test-provider";
    credit.gross_contractual_claim_remains_unchanged = true;
    credit.provider_price_remains_full_performance_and_unchanged = true;
    credit.conditional_provider_state_weights_are_fixed_physical = true;
    credit.price_ladder_collateral_is_pledged_to_investor = true;
    credit.collateral_yield_remains_in_pledged_account = true;
    credit.collateral_applies_before_unsecured_recovery = true;
    credit.provider_default_occurs_at_claim_settlement = true;
    credit.provider_default_is_physical_stress_not_pricing_measure = true;
    credit.scenarios = {
        cf::ProviderCreditScenarioConfig{"low-loss",
            {performing("low-performs", 0.90),
                defaulting("low-defaults", 0.10)}},
        cf::ProviderCreditScenarioConfig{"high-loss",
            {performing("high-performs", 0.40),
                defaulting("high-defaults", 0.60)}},
    };
    return credit;
}

[[nodiscard]] const cf::ProviderCreditOutcomeResult& find_outcome(
    const cf::ProviderCreditStressSummary& summary,
    const std::string& scenario_id, const std::string& outcome_id) {
    const auto scenario = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [&scenario_id](const auto& candidate) {
            return candidate.scenario_id == scenario_id;
        });
    if (scenario == summary.scenarios.end()) {
        throw std::logic_error("test fixture lost a scenario");
    }
    const auto outcome = std::find_if(scenario->outcomes.begin(),
        scenario->outcomes.end(), [&outcome_id](const auto& candidate) {
            return candidate.outcome_id == outcome_id;
        });
    if (outcome == scenario->outcomes.end()) {
        throw std::logic_error("test fixture lost an outcome");
    }
    return *outcome;
}

[[nodiscard]] double endpoint_weight(
    const cf::ProviderCreditStressSummary& summary,
    const cf::AmbiguityEndpoint& endpoint, const std::string& scenario_id) {
    const auto bounds = std::find_if(
        summary.scenario_probability_bounds.begin(),
        summary.scenario_probability_bounds.end(),
        [&scenario_id](const auto& candidate) {
            return candidate.scenario_id == scenario_id;
        });
    if (bounds == summary.scenario_probability_bounds.end()) {
        throw std::logic_error("test fixture lost ambiguity bounds");
    }
    const auto index = static_cast<std::size_t>(
        std::distance(summary.scenario_probability_bounds.begin(), bounds));
    if (index >= endpoint.scenario_weights.size()) {
        throw std::logic_error("test fixture received a short witness");
    }
    return endpoint.scenario_weights[index];
}

void test_hand_calculated_credit_waterfall_and_wrong_way_risk() {
    const cf::ProviderCreditStressSummary summary =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), credit_terms());
    const cf::ProviderCreditOutcomeResult& high_default = find_outcome(
        summary, "high-loss", "high-defaults");
    const cf::ProviderCreditOutcomeResult& low_default = find_outcome(
        summary, "low-loss", "low-defaults");

    check(near(summary.exposure.contractual_maximum_exposure_million, 10.0) &&
            near(summary.exposure.modeled_maximum_claim_million, 8.0) &&
            near(summary.exposure.pledged_collateral_base_million,
                30.0 / 11.0) &&
            near(summary.exposure.pledged_collateral_at_settlement_million,
                3.0) &&
            near(summary.exposure.contractual_maximum_unsecured_exposure_million,
                7.0) &&
            near(summary.exposure.modeled_maximum_unsecured_exposure_million,
                5.0),
        "contractual and modeled exposure remain separate around recognized pledged collateral");
    check(near(high_default.gross_contractual_claim_million, 8.0) &&
            high_default.direct_provider_payment_at_settlement_million == 0.0 &&
            near(high_default.collateral_applied_at_settlement_million, 3.0) &&
            near(high_default.unsecured_exposure_at_default_million, 5.0) &&
            near(high_default.delayed_unsecured_recovery_million, 2.0) &&
            near(high_default.ultimate_unpaid_claim_million, 3.0) &&
            near(high_default.actual_support_received_present_value_million,
                530.0 / 121.0) &&
            near(high_default.investor_credit_loss_present_value_million,
                350.0 / 121.0),
        "default applies collateral at settlement before delayed unsecured recovery and unpaid loss");
    check(near(low_default.gross_contractual_claim_million, 2.0) &&
            near(low_default.collateral_applied_at_settlement_million, 2.0) &&
            low_default.unsecured_exposure_at_default_million == 0.0 &&
            low_default.ultimate_unpaid_claim_million == 0.0,
        "collateral application is capped at the claim and can fully cover the low-loss default atom");
    check(near(summary.central.provider_default_probability, 0.35) &&
            near(summary.central.claim_weighted_provider_default_rate.value_or(-1.0),
                0.50) &&
            near(summary.central.expected_contractual_claim_given_provider_default_million.value_or(-1.0),
                50.0 / 7.0) &&
            near(summary.central.expected_unsecured_exposure_given_provider_default_million.value_or(-1.0),
                30.0 / 7.0) &&
            near(summary.central.claim_at_default_severity_multiplier.value_or(-1.0),
                10.0 / 7.0) &&
            near(summary.central.contractual_claim_provider_default_covariance_million,
                0.75) &&
            summary.central.contractual_claim_provider_default_correlation
                .has_value(),
        "central diagnostics expose claim/default wrong-way dependence and conditional severity");
    check(near(summary.central.investor_credit_loss_present_value_million.mean,
              105.0 / 121.0) &&
            near(summary.central.investor_credit_loss_present_value_million
                    .expected_shortfall_95,
                350.0 / 121.0) &&
            near(summary.central.unsecured_exposure_at_default_million.p99,
                5.0),
        "expanded central atoms retain physical credit-loss ES and the unsecured-exposure tail");
}

void test_collapsed_ambiguity_ratio_and_unchanged_price() {
    const cf::ProviderCreditStressSummary summary =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), credit_terms());
    check(near(summary.robust
                   .expected_investor_credit_loss_present_value_million
                   .maximum.value,
              315.0 / 242.0) &&
            summary.robust
                    .expected_investor_credit_loss_present_value_million
                    .maximum.scenario_weights.size() == 2U,
        "fixed conditional defaults collapse to two original scenario objectives before ambiguity projection");
    check(summary.robust.central_claim_present_value_delivery_ratio
                .has_value() &&
            near(*summary.robust.central_claim_present_value_delivery_ratio,
                89.0 / 110.0) &&
            summary.robust
                .robust_minimum_claim_present_value_delivery_ratio
                .has_value() &&
            near(summary.robust
                     .robust_minimum_claim_present_value_delivery_ratio->value,
                223.0 / 286.0) &&
            summary.robust
                    .robust_minimum_claim_present_value_delivery_ratio
                    ->scenario_weights.size() == 2U &&
            summary.robust
                    .robust_minimum_delivery_ratio_objective_residual_million <=
                1.0e-9,
        "claim-PV delivery ratios use common-measure numerator and denominator with a binding robust witness");
    check(near(summary.support
                   .unchanged_full_performance_provider_price_million,
              8.5) &&
            summary.support.full_performance_provider_price_change_million ==
                0.0 &&
            near(summary.support
                   .base_full_performance_all_in_support_gap_million,
              137.0 / 22.0) &&
            near(summary.support
                   .incremental_counterparty_credit_support_gap_million,
              315.0 / 242.0) &&
            near(summary.support.stressed_all_in_support_gap_million,
              911.0 / 121.0),
        "provider default leaves the seller price unchanged and adds a separate investor credit support gap");
    check(summary.provider_id == "test-provider" &&
            !summary.gross_contractual_claim_is_changed &&
            !summary.provider_price_is_reduced_for_default &&
            summary.provider_default_risk_is_modeled &&
            summary.maximum_gross_project_loss_change_million <= 1.0e-12 &&
            summary.maximum_default_waterfall_reconciliation_error_million <=
                1.0e-12 &&
            summary.maximum_credit_loss_reconciliation_error_million <=
                1.0e-12 &&
            summary.maximum_conditional_collapse_reconciliation_error_million <=
                1.0e-9 &&
            summary.maximum_probability_witness_reconciliation_error <=
                1.0e-12 &&
            summary.maximum_monetary_witness_reconciliation_error_million <=
                1.0e-9 &&
            summary
                    .maximum_central_probability_projection_reconciliation_error <=
                1.0e-12 &&
            summary
                    .maximum_central_monetary_projection_reconciliation_error_million <=
                1.0e-9,
        "gross loss, price, waterfalls, collapsed objectives, and ambiguity witnesses reconcile");
}

void test_unpledged_collateral_cannot_benefit_investor() {
    cf::ProviderCreditStressConfig unpledged = credit_terms();
    unpledged.price_ladder_collateral_is_pledged_to_investor = false;
    unpledged.collateral_yield_remains_in_pledged_account = false;
    for (cf::ProviderCreditScenarioConfig& scenario : unpledged.scenarios) {
        for (cf::ProviderCreditOutcomeConfig& outcome : scenario.outcomes) {
            outcome.collateral_realization_fraction = 0.0;
        }
    }
    const cf::ProviderCreditStressSummary summary =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), unpledged);
    const cf::ProviderCreditOutcomeResult& high_default = find_outcome(
        summary, "high-loss", "high-defaults");
    check(summary.full_performance_price_ladder.costs.collateral_base_million >
                0.0 &&
            summary.exposure.pledged_collateral_base_million == 0.0 &&
            summary.exposure.pledged_collateral_at_settlement_million == 0.0 &&
            high_default.collateral_applied_at_settlement_million == 0.0 &&
            near(high_default.unsecured_exposure_at_default_million, 8.0) &&
            near(summary.support
                   .unchanged_full_performance_provider_price_million,
                8.5),
        "price-ladder collateral carry does not become investor collateral without the explicit pledge assertion");

    cf::ProviderCreditStressConfig invalid = unpledged;
    invalid.scenarios[0].outcomes[1].collateral_realization_fraction = 1.0;
    expect_invalid_argument(
        [&invalid] {
            (void)cf::solve_provider_credit_stress(portfolio_terms(),
                ambiguity_terms(), participation_terms(), protection_terms(),
                pricing_terms(), invalid);
        },
        "unpledged collateral cannot retain a positive realization fraction");
}

void test_zero_realization_equal_pd_and_witness_switching() {
    cf::ProviderCreditStressConfig zero_realization = credit_terms();
    for (cf::ProviderCreditScenarioConfig& scenario :
         zero_realization.scenarios) {
        for (cf::ProviderCreditOutcomeConfig& outcome : scenario.outcomes) {
            outcome.collateral_realization_fraction = 0.0;
        }
    }
    const cf::ProviderCreditStressSummary zero_collateral =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), zero_realization);
    const cf::ProviderCreditOutcomeResult& high_zero = find_outcome(
        zero_collateral, "high-loss", "high-defaults");
    check(near(zero_collateral.exposure
                   .pledged_collateral_at_settlement_million,
              3.0) &&
            high_zero.collateral_applied_at_settlement_million == 0.0 &&
            near(high_zero.unsecured_exposure_at_default_million, 8.0),
        "zero realization stress leaves the pledged account visible but gives the investor no collateral cash");

    cf::ProviderCreditStressConfig equal_pd = credit_terms();
    equal_pd.scenarios[0].outcomes[0].conditional_weight = 0.50;
    equal_pd.scenarios[0].outcomes[1].conditional_weight = 0.50;
    equal_pd.scenarios[1].outcomes[0].conditional_weight = 0.50;
    equal_pd.scenarios[1].outcomes[1].conditional_weight = 0.50;
    const cf::ProviderCreditStressSummary independent =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), equal_pd);
    check(near(independent.central.provider_default_probability, 0.50) &&
            near(independent.central
                     .contractual_claim_provider_default_covariance_million,
                0.0) &&
            independent.central
                .contractual_claim_provider_default_correlation
                .has_value() &&
            near(*independent.central
                      .contractual_claim_provider_default_correlation,
                0.0) &&
            near(independent.central.claim_at_default_severity_multiplier
                     .value_or(-1.0),
                1.0),
        "equal conditional default probability in low- and high-claim scenarios creates no artificial wrong-way dependence");

    cf::ProviderCreditStressConfig switching = credit_terms();
    switching.scenarios[0].outcomes[0].conditional_weight = 0.10;
    switching.scenarios[0].outcomes[1].conditional_weight = 0.90;
    switching.scenarios[1].outcomes[0].conditional_weight = 0.90;
    switching.scenarios[1].outcomes[1].conditional_weight = 0.10;
    const cf::ProviderCreditStressSummary switched =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), switching);
    check(switched.robust.provider_default_probability.maximum
                    .scenario_weights !=
                switched.robust
                    .expected_investor_credit_loss_present_value_million
                    .maximum.scenario_weights &&
            near(endpoint_weight(switched,
                     switched.robust.provider_default_probability.maximum,
                     "low-loss"),
                0.75) &&
            near(endpoint_weight(switched,
                     switched.robust
                         .expected_investor_credit_loss_present_value_million
                         .maximum,
                     "high-loss"),
                0.75),
        "default-frequency and credit-loss objectives independently switch to their own valid ambiguity witnesses");
}

void test_delayed_full_recovery_has_present_value_credit_loss() {
    cf::ProviderCreditStressConfig delayed_full_recovery = credit_terms();
    for (cf::ProviderCreditScenarioConfig& scenario :
         delayed_full_recovery.scenarios) {
        for (cf::ProviderCreditOutcomeConfig& outcome : scenario.outcomes) {
            if (!outcome.provider_performs) {
                outcome.collateral_realization_fraction = 0.0;
                outcome.unsecured_recovery_fraction = 1.0;
                outcome.unsecured_recovery_delay_months = 12U;
            }
        }
    }
    const cf::ProviderCreditStressSummary summary =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), delayed_full_recovery);
    const cf::ProviderCreditOutcomeResult& high_default = find_outcome(
        summary, "high-loss", "high-defaults");
    const double expected_full_claim_present_value = 80.0 / 11.0;
    const double expected_delayed_full_recovery_present_value =
        800.0 / 121.0;
    const double expected_timing_credit_loss = 80.0 / 121.0;

    check(high_default.collateral_applied_at_settlement_million == 0.0 &&
            near(high_default.unsecured_exposure_at_default_million, 8.0) &&
            near(high_default.delayed_unsecured_recovery_million, 8.0) &&
            high_default.ultimate_unpaid_claim_million == 0.0,
        "full delayed unsecured recovery leaves no ultimate nominal unpaid claim");
    check(near(high_default.full_claim_present_value_million,
              expected_full_claim_present_value) &&
            near(high_default.unsecured_recovery_present_value_million,
                expected_delayed_full_recovery_present_value) &&
            high_default.investor_credit_loss_present_value_million > 0.0 &&
            near(high_default.investor_credit_loss_present_value_million,
                expected_timing_credit_loss) &&
            near(high_default.investor_credit_loss_present_value_million,
                high_default.full_claim_present_value_million -
                    high_default.unsecured_recovery_present_value_million),
        "positive investor hurdle recognizes the PV loss from delayed full recovery despite zero nominal shortfall");
}

void test_nearly_constant_large_claim_correlation_stability() {
    cf::PortfolioConfig portfolio = portfolio_terms();
    portfolio.scenario_label = "nearly constant million-scale claims";
    portfolio.annual_physical_hurdle_rate = 0.0;
    portfolio.projects[0].commitment_million = 1.0e6;
    portfolio.joint_scenarios = {
        make_scenario("low-loss", 0.5, 1.0e-9),
        make_scenario("high-loss", 0.5, 0.0),
    };
    for (cf::JointScenario& scenario : portfolio.joint_scenarios) {
        scenario.project_paths[0].capital_draws[0].amount_million = 1.0e6;
    }
    // Zero-valued configured cash is not a cash flow. Removing it also makes
    // the high state the exact full-loss endpoint.
    portfolio.joint_scenarios[1].cash_sources.clear();
    portfolio.joint_scenarios[1].project_paths[0].investor_receipts.clear();

    cf::SuccessParticipationConfig participation = participation_terms();
    participation.target_worst_expected_npv_million = -2.0e6;
    cf::PooledLossProtectionConfig protection = protection_terms();
    protection.support_cap_million = 1.0e6;
    protection.provider_annual_physical_hurdle_rate = 0.0;
    cf::ProviderPriceLadderConfig pricing = pricing_terms(1.0);
    pricing.collateral_fraction_of_contractual_maximum_exposure = 0.0;
    pricing.collateral_annual_effective_funding_rate = 0.0;
    pricing.collateral_annual_effective_yield_rate = 0.0;
    pricing.fixed_expense_upfront_million = 0.0;
    pricing.variable_claim_expense_fraction = 0.0;
    pricing.target_profit_upfront_million = 0.0;

    cf::ProviderCreditStressConfig credit = credit_terms();
    credit.price_ladder_collateral_is_pledged_to_investor = false;
    credit.collateral_yield_remains_in_pledged_account = false;
    cf::ProviderCreditOutcomeConfig high_default;
    high_default.outcome_id = "defaults";
    high_default.conditional_weight = 1.0;
    high_default.provider_performs = false;
    credit.scenarios = {
        cf::ProviderCreditScenarioConfig{"low-loss",
            {performing("performs", 1.0)}},
        cf::ProviderCreditScenarioConfig{"high-loss", {high_default}},
    };

    const cf::ProviderCreditStressSummary summary =
        cf::solve_provider_credit_stress(portfolio, ambiguity_terms(),
            participation, protection, pricing, credit);
    check(summary.central.gross_contractual_claim_million
                .standard_deviation > 0.0,
        "near-constant million-scale claims retain their small nonzero variance");
    check(summary.central
                .contractual_claim_provider_default_covariance_million > 0.0,
        "default on the marginally larger claim retains positive covariance");
    check(summary.central
                .contractual_claim_provider_default_correlation
                .has_value(),
        "nonconstant claims and provider states publish a correlation");
    if (summary.central
            .contractual_claim_provider_default_correlation
            .has_value()) {
        const double correlation = *summary.central
                                        .contractual_claim_provider_default_correlation;
        check(correlation >= -1.0 && correlation <= 1.0,
            "near-constant claim/default correlation remains bounded by one");
        check(near(correlation, 1.0, 1.0e-12),
            "centered compensated moments recover perfect positive dependence");
    }
}

void test_full_delivery_zero_claim_and_mapping_edges() {
    cf::ProviderCreditStressConfig all_perform = credit_terms();
    for (cf::ProviderCreditScenarioConfig& scenario : all_perform.scenarios) {
        scenario.outcomes = {performing("performs", 1.0)};
    }
    const cf::ProviderCreditStressSummary full =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(), all_perform);
    check(full.robust.robust_minimum_claim_present_value_delivery_ratio
                .has_value() &&
            near(full.robust
                     .robust_minimum_claim_present_value_delivery_ratio->value,
                1.0) &&
            !full.provider_default_risk_is_modeled &&
            near(full.support
                     .unchanged_full_performance_provider_price_million,
                8.5) &&
            full.support.full_performance_provider_price_change_million ==
                0.0 &&
            full.support.incremental_counterparty_credit_support_gap_million ==
                0.0 &&
            near(full.support.stressed_all_in_support_gap_million,
                full.support
                    .base_full_performance_all_in_support_gap_million) &&
            full.central.investor_credit_loss_present_value_million.maximum ==
                0.0,
        "all-performance states produce exact unit delivery and zero credit loss");

    const cf::ProviderCreditStressSummary zero =
        cf::solve_provider_credit_stress(portfolio_terms(),
            ambiguity_terms(), participation_terms(), protection_terms(),
            pricing_terms(0.0), credit_terms());
    check(!zero.robust.central_claim_present_value_delivery_ratio.has_value() &&
            !zero.robust
                 .robust_minimum_claim_present_value_delivery_ratio
                 .has_value(),
        "zero contractual claim leaves both delivery ratios absent");

    cf::ProviderCreditStressConfig mismatched = credit_terms();
    mismatched.scenarios[0].scenario_id = "not-a-portfolio-scenario";
    expect_invalid_argument(
        [&mismatched] {
            (void)cf::solve_provider_credit_stress(portfolio_terms(),
                ambiguity_terms(), participation_terms(), protection_terms(),
                pricing_terms(), mismatched);
        },
        "credit scenarios must match the original portfolio ids exactly");

    cf::ProviderCreditStressConfig wrong_provider = credit_terms();
    wrong_provider.provider_id = "different-provider";
    expect_invalid_argument(
        [&wrong_provider] {
            (void)cf::solve_provider_credit_stress(portfolio_terms(),
                ambiguity_terms(), participation_terms(), protection_terms(),
                pricing_terms(), wrong_provider);
        },
        "provider-credit and pooled-protection provider ids must match exactly");
}

} // namespace

int main() {
    test_hand_calculated_credit_waterfall_and_wrong_way_risk();
    test_collapsed_ambiguity_ratio_and_unchanged_price();
    test_unpledged_collateral_cannot_benefit_investor();
    test_zero_realization_equal_pd_and_witness_switching();
    test_delayed_full_recovery_has_present_value_credit_loss();
    test_nearly_constant_large_claim_correlation_stability();
    test_full_delivery_zero_claim_and_mapping_edges();

    if (failures != 0) {
        std::cerr << failures << " provider credit-stress test(s) failed\n";
        return 1;
    }
    std::cout << "all provider credit-stress tests passed\n";
    return 0;
}
