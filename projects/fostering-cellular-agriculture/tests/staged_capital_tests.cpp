// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/staged_capital.hpp>
#include <naturalehia/cellular_finance/staged_capital_config.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace cf = naturalehia::cellular_finance;

namespace {

class NonCanonicalPunctuation final : public std::numpunct<char> {
protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
    [[nodiscard]] std::string do_truename() const override { return "yes"; }
    [[nodiscard]] std::string do_falsename() const override { return "no"; }
};

int failures = 0;

void check(bool condition, const std::string& message) {
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

[[nodiscard]] cf::StagedCapitalConfig single_phase_config() {
    cf::StagedCapitalConfig config;
    config.scenario_label = "deterministic staged-capital unit test";
    config.source_note =
        "synthetic values created only for deterministic unit tests";
    config.currency_label = "TEST";
    config.monetary_basis = "constant test units at close";
    config.terms.provider_commitment_million = 40.0;
    config.terms.sponsor_construction_commitment_million = 60.0;
    config.terms.provider_cost_share = 0.40;
    config.terms.annual_pik_rate = 0.0;
    config.terms.claim_cap_multiple = 1.0;
    config.terms.annual_commitment_fee_rate = 0.0;
    config.terms.upfront_fee_million = 0.0;
    config.terms.provider_hurdle_rate = 0.0;
    config.terms.sponsor_discount_rate = 0.0;
    config.terms.protected_workout_reserve_million = 10.0;
    config.phases = {cf::StagedCapitalPhaseTerms{
        "construction-and-acceptance", 12U, 40.0}};
    cf::StagedCapitalCase scenario_case;
    scenario_case.id = "completion";
    scenario_case.weight = 1.0;
    scenario_case.completion_value_million = 150.0;
    scenario_case.recovery_value_million = 0.0;
    scenario_case.recovery_delay_months = 0U;
    scenario_case.required_workout_cost_million = 0.0;
    scenario_case.phases = {cf::StagedCapitalPhaseCase{
        100.0, 100.0, cf::CertificationDecision::Certified, true}};
    config.cases = {scenario_case};
    return config;
}

void check_path_controls(const cf::StagedCapitalPathResult& path,
    const std::string& context) {
    for (const auto& entry : path.cash_ledger) {
        const double sum = entry.posting.sponsor_million +
            entry.posting.project_unrestricted_million +
            entry.posting.provider_million +
            entry.posting.protected_reserve_million +
            entry.posting.external_million;
        check(near(sum, 0.0), context + ": every cash posting balances");
    }
    check(near(path.project_closing_unrestricted_cash_million, 0.0),
        context + ": ProjectCo unrestricted cash closes at zero");
    check(near(path.protected_reserve_closing_cash_million, 0.0),
        context + ": protected reserve closes through a permitted use or release");
    check(near(path.closing_undrawn_commitment_million, 0.0),
        context + ": undrawn commitment closes through draw or cancellation");
    check(near(path.closing_funded_claim_million, 0.0),
        context + ": funded claim closes through repayment or writeoff");
    check(path.maximum_memo_rollforward_imbalance_million <= 1.0e-8,
        context + ": pathwise commitment and claim memo accounts reconcile");
}

void test_deterministic_completion_hand_table() {
    const cf::StagedCapitalConfig config = single_phase_config();
    const cf::StagedCapitalPathResult path =
        cf::evaluate_staged_capital_case(config, config.cases.front().id);

    check(path.outcome == cf::StagedCapitalOutcome::Completed,
        "the fully certified case completes");
    check(near(path.total_provider_draws_million, 40.0),
        "provider funds exactly its 40 percent eligible-cost share");
    check(near(path.total_sponsor_construction_contributions_million, 60.0),
        "sponsor funds the remaining eligible construction cost");
    check(near(path.provider_claim_at_exit_million, 40.0),
        "zero PIK leaves the funded provider claim equal to principal");
    check(near(path.provider_nominal_recovery_million, 40.0),
        "senior provider claim is paid before sponsor residual");
    check(near(path.sponsor_residual_receipt_million, 110.0),
        "sponsor receives only completion value after provider repayment");
    check(near(path.provider_principal_loss_million, 0.0),
        "full provider principal recovery produces no principal loss");
    check(near(path.provider_npv_before_upfront_fee_million, 0.0),
        "zero-rate full recovery produces a zero break-even fee basis");
    check(near(path.sponsor_npv_million, 50.0),
        "sponsor cash flow reconciles reserve, construction, residual, and release");
    check_path_controls(path, "deterministic completion");
}

void test_pik_cap_commitment_fee_and_cancellation() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.annual_pik_rate = 1.0;
    config.terms.claim_cap_multiple = 1.5;
    config.terms.annual_commitment_fee_rate = 0.12;
    config.cases.front().phases.front().actual_eligible_cost_million = 50.0;
    config.cases.front().phases.front().estimated_cost_to_complete_million = 50.0;
    config.cases.front().completion_value_million = 100.0;

    const auto path =
        cf::evaluate_staged_capital_case(config, config.cases.front().id);
    check(near(path.total_provider_draws_million, 20.0),
        "provider draw follows the eligible-cost share below the stage cap");
    check(near(path.provider_claim_at_exit_million, 30.0),
        "PIK accrual stops exactly at the 1.5x funded-principal claim cap");
    check(near(path.total_commitment_fees_million, 0.0),
        "unused one-draw phase capacity cancels before the commitment-fee period");
    check(near(path.unused_commitment_cancelled_million, 20.0),
        "unused current-stage availability cancels rather than rolling forward");
    check(near(path.provider_npv_before_upfront_fee_million, 10.0),
        "provider NPV reconciles the draw and capped claim payment without a fee on cancelled capacity");
    check_path_controls(path, "PIK and commitment fee");
}

void test_milestone_failure_recovery_and_protected_reserve() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.provider_commitment_million = 40.0;
    config.terms.sponsor_construction_commitment_million = 60.0;
    config.terms.protected_workout_reserve_million = 5.0;
    config.terms.annual_commitment_fee_rate = 0.12;
    config.phases = {
        cf::StagedCapitalPhaseTerms{"first-stage", 1U, 20.0},
        cf::StagedCapitalPhaseTerms{"future-stage", 1U, 20.0},
    };
    auto& scenario_case = config.cases.front();
    scenario_case.id = "first-stage-final-failure";
    scenario_case.completion_value_million = 0.0;
    scenario_case.recovery_value_million = 10.0;
    scenario_case.required_workout_cost_million = 7.0;
    scenario_case.phases = {
        cf::StagedCapitalPhaseCase{
            50.0, 100.0, cf::CertificationDecision::FinalFailure, true},
        cf::StagedCapitalPhaseCase{
            50.0, 50.0, cf::CertificationDecision::Certified, true},
    };

    const auto path =
        cf::evaluate_staged_capital_case(config, scenario_case.id);
    check(path.outcome == cf::StagedCapitalOutcome::MilestoneFailure,
        "final failed certification is a distinct absorbing outcome");
    check(path.phases.front().funded && !path.phases.back().reached,
        "failure stops only future construction draws");
    check(near(path.total_provider_draws_million, 20.0),
        "funded exposure survives later milestone failure");
    check(near(path.provider_nominal_recovery_million, 8.0),
        "declared safe-workout shortfall is paid from recovery before the provider");
    check(near(path.provider_principal_loss_million, 12.0),
        "unrecovered funded principal excludes fees and cannot invade protected obligations");
    check(near(path.unused_commitment_cancelled_million, 20.0),
        "all future undrawn commitment cancels on final failure");
    check(near(path.total_commitment_fees_million, 0.2),
        "commitment fee accrues for one month only on reachable future-stage availability");
    check(near(path.stranded_spend_million, 50.0),
        "sunk eligible construction spend remains visible at stop");
    check(near(path.protected_workout_spend_million, 5.0) &&
            near(path.protected_reserve_shortfall_at_stop_million, 2.0) &&
            near(path.workout_shortfall_paid_from_recovery_million, 2.0) &&
            near(path.safety_funding_shortfall_million, 0.0),
        "reserve insufficiency remains visible and is cured ahead of provider recovery");
    check(path.provider_nominal_recovery_million <=
            path.provider_claim_at_exit_million + 1.0e-9,
        "provider cannot recover more than its funded claim");
    check_path_controls(path, "milestone failure");

    config.cases.front().recovery_value_million = 1.0;
    const auto unresolved = cf::evaluate_staged_capital_case(
        config, config.cases.front().id);
    check(near(unresolved.provider_nominal_recovery_million, 0.0) &&
            near(unresolved.workout_shortfall_paid_from_recovery_million,
                1.0) &&
            near(unresolved.safety_funding_shortfall_million, 1.0),
        "provider receives nothing while a declared safe-workout obligation remains unfunded");
}

void test_cost_to_complete_stop_is_atomic() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.provider_commitment_million = 20.0;
    config.terms.sponsor_construction_commitment_million = 30.0;
    config.terms.protected_workout_reserve_million = 2.0;
    config.phases = {
        cf::StagedCapitalPhaseTerms{"current-stage", 1U, 10.0},
        cf::StagedCapitalPhaseTerms{"future-stage", 1U, 10.0},
    };
    auto& scenario_case = config.cases.front();
    scenario_case.completion_value_million = 0.0;
    scenario_case.required_workout_cost_million = 0.0;
    scenario_case.phases = {
        cf::StagedCapitalPhaseCase{
            10.0, 51.0, cf::CertificationDecision::Certified, true},
        cf::StagedCapitalPhaseCase{
            25.0, 25.0, cf::CertificationDecision::Certified, true},
    };

    const auto path =
        cf::evaluate_staged_capital_case(config, scenario_case.id);
    check(path.outcome ==
            cf::StagedCapitalOutcome::CostToCompleteFailure,
        "aggregate cost-to-complete deficit has a named outcome");
    check(near(path.funding_gap_million, 7.0),
        "cost-to-complete failure excludes unreachable unused current-phase capacity");
    check(near(path.total_provider_draws_million, 0.0) &&
            near(path.total_sponsor_construction_contributions_million, 0.0) &&
            near(path.cumulative_eligible_spend_million, 0.0),
        "a failed pre-draw sources test leaves construction postings unchanged");
    check(near(path.unused_commitment_cancelled_million, 20.0),
        "unfunded legal availability cancels without becoming exposure");
    check_path_controls(path, "cost-to-complete failure");

    cf::StagedCapitalConfig cap_slippage = single_phase_config();
    cap_slippage.terms.provider_commitment_million = 40.0;
    cap_slippage.terms.sponsor_construction_commitment_million = 60.0;
    cap_slippage.terms.protected_workout_reserve_million = 2.0;
    cap_slippage.phases = {
        cf::StagedCapitalPhaseTerms{"capped-first-stage", 1U, 10.0},
        cf::StagedCapitalPhaseTerms{"future-stage", 1U, 30.0},
    };
    cap_slippage.cases.front().completion_value_million = 0.0;
    cap_slippage.cases.front().phases = {
        cf::StagedCapitalPhaseCase{
            50.0, 100.0, cf::CertificationDecision::Certified, true},
        cf::StagedCapitalPhaseCase{
            50.0, 50.0, cf::CertificationDecision::Certified, true},
    };
    const auto cap_path = cf::evaluate_staged_capital_case(
        cap_slippage, cap_slippage.cases.front().id);
    check(cap_path.outcome ==
            cf::StagedCapitalOutcome::CostToCompleteFailure &&
            near(cap_path.funding_gap_million, 10.0),
        "pre-draw cost-to-complete test captures current cap slippage and future provider-share limits");
    check(near(cap_path.cumulative_eligible_spend_million, 0.0),
        "cap-driven future source deficit stops the irreversible current draw atomically");
}

void test_sponsor_and_provider_funding_failures_are_distinct() {
    cf::StagedCapitalConfig sponsor_config = single_phase_config();
    sponsor_config.terms.provider_commitment_million = 40.0;
    sponsor_config.terms.sponsor_construction_commitment_million = 30.0;
    sponsor_config.phases.front().provider_stage_cap_million = 40.0;
    sponsor_config.cases.front().completion_value_million = 0.0;
    sponsor_config.cases.front().phases.front().actual_eligible_cost_million =
        60.0;
    sponsor_config.cases.front().phases.front()
        .estimated_cost_to_complete_million = 60.0;
    const auto sponsor_path = cf::evaluate_staged_capital_case(
        sponsor_config, sponsor_config.cases.front().id);
    check(sponsor_path.outcome ==
            cf::StagedCapitalOutcome::SponsorFundingFailure,
        "sponsor matching failure is not mislabeled as a milestone failure");
    check(near(sponsor_path.funding_gap_million, 6.0),
        "sponsor failure exposes its exact required contribution deficit");
    check(near(sponsor_path.phases.front()
                   .sponsor_contribution_required_million,
              36.0) &&
            near(sponsor_path.phases.front().sponsor_contribution_million,
                0.0) &&
            near(sponsor_path.phases.front()
                     .provider_draw_received_million,
                0.0) &&
            near(sponsor_path.cumulative_eligible_spend_million, 0.0),
        "failed sponsor match separates required cash from zero atomic receipts and spend");

    cf::StagedCapitalConfig provider_config = single_phase_config();
    provider_config.terms.annual_commitment_fee_rate = 0.12;
    cf::StagedCapitalCase provider_success = provider_config.cases.front();
    provider_success.id = "provider-performs";
    provider_success.weight = 0.5;
    cf::StagedCapitalCase provider_failure = provider_success;
    provider_failure.id = "provider-does-not-fund";
    provider_failure.weight = 0.5;
    provider_failure.phases.front().provider_funds = false;
    provider_config.cases = {provider_success, provider_failure};
    const auto provider_path = cf::evaluate_staged_capital_case(
        provider_config, provider_config.cases.back().id);
    check(provider_path.outcome ==
            cf::StagedCapitalOutcome::ProviderFundingFailure,
        "provider cash failure is distinct from contractual entitlement");
    check(near(provider_path.phases.front()
                   .provider_draw_entitlement_million,
              40.0) &&
            near(provider_path.phases.front()
                     .provider_draw_received_million,
                0.0),
        "provider entitlement is visible without creating cash or funded exposure");
    check(near(provider_path.total_sponsor_construction_contributions_million,
              0.0) &&
            near(provider_path.cumulative_eligible_spend_million, 0.0),
        "provider non-performance makes the simultaneous draw atomic");
    check(near(provider_path.total_commitment_fees_million, 0.0),
        "first-draw provider failure earns no post-failure commitment fee");
    const auto provider_summary =
        cf::evaluate_staged_capital_cases(provider_config);
    check(near(provider_summary.fee_sensitivity_included_weight, 1.0) &&
            near(provider_summary
                     .physical_measure_break_even_upfront_fee_million,
                0.0),
        "provider non-performance remains a project stress while fee adequacy replays the same full physical case mix with performance held true");
    check_path_controls(sponsor_path, "sponsor funding failure");
    check_path_controls(provider_path, "provider funding failure");
}

void test_physical_measure_break_even_fee_two_state_fixture() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.provider_commitment_million = 10.0;
    config.terms.sponsor_construction_commitment_million = 15.0;
    config.terms.upfront_fee_million = 5.0;
    config.terms.protected_workout_reserve_million = 1.0;
    config.phases.front().provider_stage_cap_million = 10.0;
    config.phases.front().duration_months = 1U;

    cf::StagedCapitalCase success = config.cases.front();
    success.id = "full-recovery";
    success.weight = 0.5;
    success.completion_value_million = 25.0;
    success.phases.front().actual_eligible_cost_million = 25.0;
    success.phases.front().estimated_cost_to_complete_million = 25.0;

    cf::StagedCapitalCase failure = success;
    failure.id = "total-loss";
    failure.weight = 0.5;
    failure.completion_value_million = 0.0;
    failure.recovery_value_million = 0.0;
    failure.phases.front().certification =
        cf::CertificationDecision::FinalFailure;
    config.cases = {success, failure};

    const auto summary = cf::evaluate_staged_capital_cases(config);
    check(near(summary.expected_provider_npv_before_upfront_fee_million, -5.0),
        "two equally weighted states produce the hand-calculated expected provider loss");
    check(near(
            summary.physical_measure_break_even_upfront_fee_million, 5.0),
        "physical-P break-even fee is the negative weighted non-fee provider NPV");
    check(near(summary.expected_provider_npv_after_charged_upfront_fee_million,
              0.0) &&
            near(summary.upfront_fee_adequacy_gap_million, 0.0),
        "the solved time-zero fee makes the same weighted provider cases zero-NPV");
    check(near(summary.provider_principal_loss_weight, 0.5) &&
            near(summary.conditional_provider_principal_loss_million.value(),
                10.0),
        "loss frequency and conditional severity preserve declared case weights");
    check(near(summary.provider_principal_loss_million.p95, 10.0) &&
            near(summary.provider_principal_loss_million
                     .expected_shortfall_95,
                10.0),
        "upper-tail loss statistics handle a discrete two-state distribution");
    double fee_case_reconciliation = 0.0;
    for (const auto& fee_case : summary.fee_basis_cases) {
        fee_case_reconciliation += fee_case.weight *
            fee_case.provider_npv_before_upfront_fee_million;
    }
    check(summary.fee_basis_cases.size() == 2U &&
            near(fee_case_reconciliation,
                summary.expected_provider_npv_before_upfront_fee_million),
        "disclosed paired fee cases reconcile exactly to weighted provider NPV");
}

void test_claim_writeoff_and_cash_draw_shortfall_are_distinct() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.annual_pik_rate = 1.0;
    config.terms.claim_cap_multiple = 1.5;
    config.cases.front().completion_value_million = 0.0;
    config.cases.front().recovery_value_million = 25.0;
    config.cases.front().phases.front().actual_eligible_cost_million = 50.0;
    config.cases.front().phases.front().estimated_cost_to_complete_million =
        50.0;
    config.cases.front().phases.front().certification =
        cf::CertificationDecision::FinalFailure;

    const auto partial_claim = cf::evaluate_staged_capital_case(
        config, config.cases.front().id);
    check(near(partial_claim.provider_claim_at_exit_million, 30.0) &&
            near(partial_claim.provider_nominal_recovery_million, 25.0),
        "recovery between cash draws and the PIK-bearing claim follows the senior waterfall");
    check(near(partial_claim.provider_principal_loss_million, 0.0) &&
            near(partial_claim.provider_claim_writeoff_million, 5.0),
        "cash-draw shortfall and contractual claim writeoff remain distinct");
    check(near(partial_claim.sponsor_residual_receipt_million, 0.0),
        "sponsor receives no residual while the provider claim is unpaid");

    config.cases.front().recovery_value_million = 15.0;
    const auto below_draws = cf::evaluate_staged_capital_case(
        config, config.cases.front().id);
    check(near(below_draws.provider_principal_loss_million, 5.0) &&
            near(below_draws.provider_claim_writeoff_million, 15.0),
        "recovery below cash draws exposes both cash shortfall and larger claim writeoff");
}

void test_later_provider_failure_preserves_prior_claim() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.provider_commitment_million = 40.0;
    config.terms.sponsor_construction_commitment_million = 60.0;
    config.terms.protected_workout_reserve_million = 5.0;
    config.terms.annual_commitment_fee_rate = 0.12;
    config.phases = {
        cf::StagedCapitalPhaseTerms{"performed-stage", 1U, 20.0},
        cf::StagedCapitalPhaseTerms{"failed-provider-stage", 1U, 20.0},
    };
    config.cases.front().completion_value_million = 100.0;
    config.cases.front().recovery_value_million = 10.0;
    config.cases.front().recovery_delay_months = 6U;
    config.cases.front().phases = {
        cf::StagedCapitalPhaseCase{
            50.0, 100.0, cf::CertificationDecision::Certified, true},
        cf::StagedCapitalPhaseCase{
            50.0, 50.0, cf::CertificationDecision::Certified, false},
    };
    const auto path = cf::evaluate_staged_capital_case(
        config, config.cases.front().id);
    check(path.outcome == cf::StagedCapitalOutcome::ProviderFundingFailure &&
            path.stop_phase_index == 1U,
        "provider failure after a funded stage has a distinct later-phase outcome");
    check(path.phases.front().funded && !path.phases.back().funded &&
            near(path.phases.back().sponsor_contribution_million, 0.0),
        "later failed draw is atomic while prior funded construction remains recorded");
    check(near(path.total_provider_draws_million, 20.0) &&
            near(path.provider_nominal_recovery_million, 10.0),
        "current convention preserves the prior senior provider claim despite later nonperformance");
    check(near(path.total_commitment_fees_million, 0.2),
        "later provider failure preserves earned phase-one fee but accrues none after the failed draw");
    check_path_controls(path, "later provider failure");
}

void test_delay_and_negative_break_even_fee_are_visible() {
    cf::StagedCapitalConfig delayed = single_phase_config();
    delayed.terms.provider_commitment_million = 10.0;
    delayed.terms.sponsor_construction_commitment_million = 15.0;
    delayed.terms.provider_hurdle_rate = 0.12;
    delayed.terms.protected_workout_reserve_million = 1.0;
    delayed.phases.front().provider_stage_cap_million = 10.0;
    delayed.phases.front().duration_months = 1U;
    delayed.cases.front().completion_value_million = 0.0;
    delayed.cases.front().recovery_value_million = 10.0;
    delayed.cases.front().phases.front().actual_eligible_cost_million = 25.0;
    delayed.cases.front().phases.front().estimated_cost_to_complete_million =
        25.0;
    delayed.cases.front().phases.front().certification =
        cf::CertificationDecision::FinalFailure;
    const auto prompt_recovery = cf::evaluate_staged_capital_cases(delayed);
    delayed.cases.front().recovery_delay_months = 12U;
    const auto late_recovery = cf::evaluate_staged_capital_cases(delayed);
    check(late_recovery.physical_measure_break_even_upfront_fee_million >
            prompt_recovery.physical_measure_break_even_upfront_fee_million,
        "delayed recovery raises required fee on a positive provider hurdle basis");
    check(late_recovery.cases.front().outcome_month == 1U &&
            late_recovery.cases.front().recovery_month == 13U &&
            late_recovery.fee_basis_cases.front().outcome_month == 1U &&
            late_recovery.fee_basis_cases.front().recovery_month == 13U,
        "actual and paired fee records expose outcome and terminal-settlement timing");

    cf::StagedCapitalConfig profitable = single_phase_config();
    profitable.terms.annual_pik_rate = 0.10;
    profitable.terms.claim_cap_multiple = 2.0;
    profitable.cases.front().completion_value_million = 200.0;
    const auto profitable_summary =
        cf::evaluate_staged_capital_cases(profitable);
    check(profitable_summary
              .physical_measure_break_even_upfront_fee_million < 0.0,
        "a negative break-even fee is retained as an explicit rebate sensitivity");
}

void test_weighted_tail_boundaries_do_not_use_money_tolerance() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.provider_commitment_million = 10.0;
    config.terms.sponsor_construction_commitment_million = 15.0;
    config.terms.protected_workout_reserve_million = 1.0;
    config.phases.front().provider_stage_cap_million = 10.0;
    config.phases.front().duration_months = 1U;

    cf::StagedCapitalCase no_loss = config.cases.front();
    no_loss.id = "no-loss";
    no_loss.weight = 0.9499999995;
    no_loss.completion_value_million = 25.0;
    no_loss.phases.front().actual_eligible_cost_million = 25.0;
    no_loss.phases.front().estimated_cost_to_complete_million = 25.0;
    cf::StagedCapitalCase total_loss = no_loss;
    total_loss.id = "ten-unit-loss";
    total_loss.weight = 0.0500000005;
    total_loss.completion_value_million = 0.0;
    total_loss.recovery_value_million = 0.0;
    total_loss.phases.front().certification =
        cf::CertificationDecision::FinalFailure;
    config.cases = {no_loss, total_loss};
    const auto just_below = cf::evaluate_staged_capital_cases(config);
    check(near(just_below.provider_principal_loss_million.p95, 10.0),
        "a CDF just below 95 percent does not cross the loss quantile through monetary tolerance");

    config.cases.front().weight = 0.95;
    config.cases.back().weight = 0.05;
    const auto exact_boundary = cf::evaluate_staged_capital_cases(config);
    check(near(exact_boundary.provider_principal_loss_million.p95, 0.0) &&
            near(exact_boundary.provider_principal_loss_million
                     .expected_shortfall_95,
                10.0),
        "weighted VaR uses the declared inverse-CDF boundary while ES allocates the full upper tail");
}

void test_near_one_weights_are_normalized_before_large_fee_aggregation() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.terms.upfront_fee_million = 1'000'000'000.0;
    cf::StagedCapitalCase second_case = config.cases.front();
    config.cases.front().id = "declared-half";
    config.cases.front().weight = 0.5;
    second_case.id = "declared-half-plus-roundoff";
    second_case.weight = 0.5000000000005;
    config.cases.push_back(second_case);

    const cf::StagedCapitalSummary summary =
        cf::evaluate_staged_capital_cases(config);
    long double fee_weight_sum = 0.0L;
    for (const auto& fee_case : summary.fee_basis_cases) {
        fee_weight_sum += static_cast<long double>(fee_case.weight);
    }

    check(summary.configured_case_weight_sum > 1.0 &&
            std::abs(summary.configured_case_weight_sum -
                1.0000000000005) < 1.0e-15,
        "the raw accepted near-one weight sum remains visible for audit");
    check(near(summary.cases.front().weight, 0.5, 1.0e-15) &&
            near(summary.cases.back().weight, 0.5000000000005, 1.0e-15),
        "path records retain analyst-declared case weights");
    check(std::abs(summary.completion_weight - 1.0) < 2.0e-15 &&
            std::abs(summary.fee_sensitivity_included_weight - 1.0) <
                2.0e-15 &&
            std::abs(static_cast<double>(fee_weight_sum - 1.0L)) <
                2.0e-15,
        "outcome and paired fee aggregates use weights normalized to one");
    check(std::abs(
              summary.expected_provider_npv_after_charged_upfront_fee_million -
              summary.upfront_fee_adequacy_gap_million) < 2.0e-5 &&
            std::abs(
              summary.expected_provider_npv_after_charged_upfront_fee_million -
              1'000'000'000.0) < 2.0e-5,
        "near-one normalization preserves the large-upfront-fee identity");
}

void test_tiny_positive_obligations_retain_boolean_and_event_meaning() {
    cf::StagedCapitalConfig defaulted = single_phase_config();
    defaulted.terms.provider_commitment_million = 1.0e-11;
    defaulted.terms.sponsor_construction_commitment_million = 1.0e-11;
    defaulted.terms.provider_cost_share = 0.5;
    defaulted.phases.front().provider_stage_cap_million = 1.0e-11;
    defaulted.cases.front().completion_value_million = 2.0e-11;
    defaulted.cases.front().phases.front().actual_eligible_cost_million =
        2.0e-11;
    defaulted.cases.front().phases.front()
        .estimated_cost_to_complete_million = 2.0e-11;
    defaulted.cases.front().phases.front().provider_funds = false;
    const cf::StagedCapitalPathResult default_path =
        cf::evaluate_staged_capital_case(
            defaulted, defaulted.cases.front().id);
    check(default_path.outcome ==
            cf::StagedCapitalOutcome::ProviderFundingFailure &&
            default_path.funding_gap_million > 0.0 &&
            default_path.total_provider_draws_million == 0.0,
        "provider nonperformance governs every strictly positive draw obligation");

    cf::StagedCapitalConfig funded_loss = defaulted;
    funded_loss.cases.front().phases.front().provider_funds = true;
    funded_loss.cases.front().phases.front().certification =
        cf::CertificationDecision::FinalFailure;
    funded_loss.cases.front().required_workout_cost_million =
        funded_loss.terms.protected_workout_reserve_million + 5.0e-11;
    const cf::StagedCapitalSummary loss_summary =
        cf::evaluate_staged_capital_cases(funded_loss);
    check(loss_summary.provider_draw_weight == 1.0 &&
            loss_summary.provider_principal_loss_weight == 1.0 &&
            loss_summary.provider_claim_writeoff_weight == 1.0 &&
            loss_summary.protected_reserve_shortfall_at_stop_weight == 1.0 &&
            loss_summary.safety_funding_shortfall_weight == 1.0,
        "strictly positive modeled amounts count consistently in event weights");
}

void test_large_value_and_resource_guardrails() {
    cf::StagedCapitalConfig large = single_phase_config();
    large.terms.provider_commitment_million = 400'000'000.0;
    large.terms.sponsor_construction_commitment_million = 600'000'000.0;
    large.terms.protected_workout_reserve_million = 1.0;
    large.phases.front().provider_stage_cap_million = 400'000'000.0;
    large.phases.front().duration_months = 1U;
    large.cases.front().completion_value_million = 1'000'000'000.0;
    large.cases.front().phases.front().actual_eligible_cost_million =
        1'000'000'000.0;
    large.cases.front().phases.front().estimated_cost_to_complete_million =
        1'000'000'000.0;
    const auto exact_large = cf::evaluate_staged_capital_case(
        large, large.cases.front().id);
    check(exact_large.outcome == cf::StagedCapitalOutcome::Completed,
        "large-scale exact source boundary remains financeable");
    check(near(exact_large.total_provider_draws_million, 400'000'000.0) &&
            near(exact_large
                     .total_sponsor_construction_contributions_million,
                600'000'000.0) &&
            near(exact_large.provider_claim_at_exit_million,
                400'000'000.0),
        "large-scale source and claim amounts match the exact hand table");
    check_path_controls(exact_large, "large-scale exact boundary");

    large.terms.sponsor_construction_commitment_million -= 0.001;
    const auto outside_large = cf::evaluate_staged_capital_case(
        large, large.cases.front().id);
    check(outside_large.outcome ==
            cf::StagedCapitalOutcome::SponsorFundingFailure &&
            outside_large.funding_gap_million > 0.0009,
        "a material 0.001-million large-scale source deficit is not swallowed by numerical tolerance");

    cf::StagedCapitalConfig oversized = single_phase_config();
    oversized.phases.front().duration_months = 1'200U;
    const cf::StagedCapitalCase prototype = oversized.cases.front();
    oversized.cases.clear();
    double assigned_weight = 0.0;
    constexpr std::size_t case_count = 84U;
    for (std::size_t index = 0U; index < case_count; ++index) {
        cf::StagedCapitalCase scenario_case = prototype;
        scenario_case.id = "resource-case-" + std::to_string(index + 1U);
        scenario_case.weight = index + 1U == case_count
            ? 1.0 - assigned_weight
            : 1.0 / static_cast<double>(case_count);
        assigned_weight += scenario_case.weight;
        oversized.cases.push_back(std::move(scenario_case));
    }
    expect_invalid_argument(
        [&oversized] { cf::validate_staged_capital_config(oversized); },
        "aggregate case-month guardrail rejects a configuration that could exhaust memory");
}

void test_validation_boundaries() {
    cf::StagedCapitalConfig config = single_phase_config();
    config.synthetic_inputs = false;
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "non-synthetic staged-capital inputs are rejected");

    config = single_phase_config();
    config.phases.front().provider_stage_cap_million = 39.0;
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "aggregate legal commitment must reconcile to phase caps");

    config = single_phase_config();
    config.cases.front().weight = 0.99;
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "declared physical-measure case weights must sum to one within tolerance");

    config = single_phase_config();
    config.cases.front().phases.front()
        .estimated_cost_to_complete_million = 101.0;
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "final phase cannot certify completion while a residual cost-to-complete estimate remains");

    config = single_phase_config();
    config.cases.front().phases.front().actual_eligible_cost_million =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "non-finite case values are rejected");

    config = single_phase_config();
    config.phases.push_back(config.phases.front());
    config.terms.provider_commitment_million = 80.0;
    config.cases.front().phases.push_back(config.cases.front().phases.front());
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "duplicate phase identifiers are rejected");

    config = single_phase_config();
    config.scenario_label = std::string(1'025U, 'x');
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "free-text length is bounded for predictable memory use");

    config = single_phase_config();
    config.phases.front().id = "phase|forged-row";
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "identifiers cannot forge pipe-delimited audit rows");

    config = single_phase_config();
    config.source_note = std::string("unsafe") + '\x1b' + "control";
    expect_invalid_argument(
        [&config] { cf::validate_staged_capital_config(config); },
        "free text rejects terminal control characters");
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create parser test fixture");
    }
    output << text;
    if (!output) {
        throw std::runtime_error("could not write parser test fixture");
    }
}

void test_strict_config_round_trip_and_rejection() {
    cf::StagedCapitalConfig original = single_phase_config();
    original.terms.annual_pik_rate = 1.0e-20;
    original.terms.annual_commitment_fee_rate = 2.0e-20;
    std::ostringstream normalized_output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    normalized_output.imbue(caller_locale);
    normalized_output << std::fixed << std::hex << std::showbase
                      << std::showpoint << std::showpos << std::uppercase
                      << std::setprecision(6);
    normalized_output.fill('#');
    normalized_output.width(37);
    const std::ios_base::fmtflags caller_flags = normalized_output.flags();
    cf::print_normalized_staged_capital_config(normalized_output, original);
    const std::string normalized = normalized_output.str();
    const std::filesystem::path path =
        std::filesystem::current_path() /
        "staged-capital-parser-test.cfg";

    write_text(path, normalized);
    const cf::StagedCapitalConfig loaded =
        cf::load_staged_capital_config(path);
    check(loaded.scenario_label == original.scenario_label &&
            near(loaded.terms.provider_commitment_million,
                original.terms.provider_commitment_million) &&
            loaded.terms.annual_pik_rate ==
                original.terms.annual_pik_rate &&
            loaded.terms.annual_commitment_fee_rate ==
                original.terms.annual_commitment_fee_rate &&
            loaded.cases.front().phases.front().certification ==
                cf::CertificationDecision::Certified,
        "normalized staged-capital configuration round-trips tiny doubles exactly despite caller formatting");
    check(normalized_output.precision() == 6 &&
            normalized_output.flags() == caller_flags &&
            normalized_output.width() == 37 &&
            normalized_output.fill() == '#' &&
            normalized_output.getloc() == caller_locale,
        "normalized configuration output restores caller flags, width, fill, and locale");

    write_text(path, normalized + "unknown.field=1\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_staged_capital_config(path); },
        "strict staged-capital parser rejects unknown keys");

    write_text(path, normalized + "case.count=1\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_staged_capital_config(path); },
        "strict staged-capital parser rejects duplicate keys");

    std::string missing = normalized;
    const std::string required_line = "facility.upfront_fee_million=0\n";
    const std::size_t position = missing.find(required_line);
    check(position != std::string::npos,
        "parser fixture contains the required line selected for removal");
    if (position != std::string::npos) {
        missing.erase(position, required_line.size());
    }
    write_text(path, missing);
    expect_invalid_argument(
        [&path] { (void)cf::load_staged_capital_config(path); },
        "strict staged-capital parser rejects missing keys");
    write_text(path, std::string(4'097U, 'x'));
    expect_invalid_argument(
        [&path] { (void)cf::load_staged_capital_config(path); },
        "strict staged-capital parser rejects oversized lines before allocation can amplify them");
    (void)std::filesystem::remove(path);
}

} // namespace

int main() {
    test_deterministic_completion_hand_table();
    test_pik_cap_commitment_fee_and_cancellation();
    test_milestone_failure_recovery_and_protected_reserve();
    test_cost_to_complete_stop_is_atomic();
    test_sponsor_and_provider_funding_failures_are_distinct();
    test_physical_measure_break_even_fee_two_state_fixture();
    test_claim_writeoff_and_cash_draw_shortfall_are_distinct();
    test_later_provider_failure_preserves_prior_claim();
    test_delay_and_negative_break_even_fee_are_visible();
    test_weighted_tail_boundaries_do_not_use_money_tolerance();
    test_near_one_weights_are_normalized_before_large_fee_aggregation();
    test_tiny_positive_obligations_retain_boolean_and_event_meaning();
    test_large_value_and_resource_guardrails();
    test_validation_boundaries();
    test_strict_config_round_trip_and_rejection();

    if (failures != 0) {
        std::cerr << failures << " staged-capital test(s) failed\n";
        return 1;
    }
    std::cout << "all staged-capital tests passed\n";
    return 0;
}
