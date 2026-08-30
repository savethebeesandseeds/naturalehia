// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/model.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(double left, double right, double tolerance = 1.0e-9) {
    return std::abs(left - right) <=
        tolerance * (1.0 + std::max(std::abs(left), std::abs(right)));
}

template <typename Function>
void expect_invalid_argument(Function&& function, std::string_view message) {
    try {
        function();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] cf::SimulationConfig deterministic_config() {
    cf::SimulationConfig config;
    config.scenario_label = "deterministic test fixture";
    config.source_note = "synthetic values created only for invariant tests";
    config.synthetic_inputs = true;
    config.trials = 32U;
    config.seed = 123'456U;

    config.risk.capex_log_sigma = 0.0;
    config.risk.construction_duration_log_sigma = 0.0;
    config.risk.utilization_logit_sigma = 0.0;
    config.risk.biological_yield_log_sigma = 0.0;
    config.risk.output_price_log_sigma = 0.0;
    config.risk.variable_cost_log_sigma = 0.0;
    config.risk.fixed_opex_log_sigma = 0.0;
    config.risk.annual_contamination_probability = 0.0;
    config.risk.contamination_logit_sigma = 0.0;
    config.risk.contamination_output_loss_fraction = 0.0;
    config.risk.persistent_factor_loading = 0.0;
    return config;
}

void check_distribution_finite(
    const cf::DistributionSummary& summary, std::string_view message) {
    check(std::isfinite(summary.mean), message);
    check(std::isfinite(summary.standard_deviation), message);
    check(std::isfinite(summary.p05), message);
    check(std::isfinite(summary.p50), message);
    check(std::isfinite(summary.p95), message);
    check(std::isfinite(summary.shortfall_value_at_risk_95), message);
    check(std::isfinite(summary.shortfall_expected_shortfall_95), message);
}

void test_validation_boundaries() {
    cf::SimulationConfig config = deterministic_config();
    cf::validate_config(config);

    config.synthetic_inputs = false;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "validated-data claims are rejected before governance exists");

    config = deterministic_config();
    config.model_version = "0.2.0";
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "a scenario cannot silently change meaning across model versions");

    config = deterministic_config();
    config.facility.base_capex_million =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "non-finite financial inputs are rejected");

    config = deterministic_config();
    config.risk.output_price_log_sigma = 5.01;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "dispersion guardrail rejects probable data-entry errors");

    config = deterministic_config();
    config.instrument.offtake_fraction = 0.60;
    config.instrument.offtake_price_per_kg = 15.0;
    config.instrument.price_support_kind = cf::PriceSupportKind::OneWayFloor;
    config.instrument.price_support_fraction = 0.50;
    config.instrument.price_support_strike_per_kg = 13.0;
    config.instrument.price_support_annual_cap_million = 10.0;
    config.instrument.price_support_lifetime_cap_million = 50.0;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "the same output cannot be double-covered");

    config = deterministic_config();
    config.instrument.price_support_fraction = 0.25;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "disabled support requires zero economic terms");

    config = deterministic_config();
    config.instrument.price_support_kind =
        static_cast<cf::PriceSupportKind>(255U);
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "unknown instrument enum values are rejected");

    config = deterministic_config();
    config.scenario_label = "line one\nline two";
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "audit labels cannot inject configuration lines");

    config = deterministic_config();
    config.source_note = " surrounding whitespace ";
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "normalized audit text must round-trip exactly");

    config = deterministic_config();
    config.facility.project_discount_rate = -0.01;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "the reference engine rejects unsupported negative discount rates");

    config = deterministic_config();
    config.debt.assume_terminal_balance_paid_by_sponsor = false;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "terminal debt treatment must remain explicit");

    config = deterministic_config();
    config.instrument.completion_payout_per_delay_year_million = 5.0;
    expect_invalid_argument(
        [&config] { cf::validate_config(config); },
        "completion protection cannot omit its cap");
}

void test_empty_instrument_identity_and_repeatability() {
    cf::SimulationConfig config = deterministic_config();
    config.trials = 64U;

    const cf::ComparisonSummary first = cf::run_paired_simulation(config);
    const cf::ComparisonSummary second = cf::run_paired_simulation(config);

    check(first.paired_project_npv_change_million.mean == 0.0,
        "zero support produces zero project-value change");
    check(first.paired_sponsor_npv_change_million.mean == 0.0,
        "zero support produces zero sponsor-value change");
    check(first.paired_instrument_transfer_pv_million.mean == 0.0,
        "zero support produces zero provider transfer");
    check(first.paired_default_probability_change == 0.0,
        "zero support produces zero default-probability change");
    check(first.project_npv_negative_to_nonnegative_count == 0U &&
            first.project_npv_nonnegative_to_negative_count == 0U &&
            first.sponsor_npv_negative_to_nonnegative_count == 0U &&
            first.sponsor_npv_nonnegative_to_negative_count == 0U &&
            first.debt_default_avoided_within_horizon_count == 0U &&
            first.debt_default_introduced_within_horizon_count == 0U &&
            first.debt_default_delayed_count == 0U &&
            first.debt_default_accelerated_count == 0U,
        "zero support produces no paired threshold transitions");
    check(first.without_instrument.debt_default_probability ==
            first.with_instrument.debt_default_probability,
        "zero support preserves default events");
    check(first.without_instrument.project_npv_after_instruments_million.mean ==
            first.with_instrument.project_npv_after_instruments_million.mean,
        "zero support preserves project NPV");
    check(first.with_instrument.project_npv_after_instruments_million.mean ==
            second.with_instrument.project_npv_after_instruments_million.mean,
        "a fixed seed is exactly repeatable in one build environment");
    check(first.with_instrument
                .unconditional_expected_debt_loss_at_default_date_million ==
            second.with_instrument
                .unconditional_expected_debt_loss_at_default_date_million,
        "repeatability includes lender loss");
}

void test_deterministic_cash_flow_matches_hand_calculation() {
    cf::SimulationConfig config = deterministic_config();
    config.trials = 2U;
    config.facility.analysis_years = 4U;
    config.facility.planned_construction_years = 1.0;
    config.facility.base_capex_million = 100.0;
    config.facility.annual_nameplate_output_million_kg = 10.0;
    config.facility.steady_state_utilization = 0.5;
    config.facility.ramp_at_commercial_operation = 1.0;
    config.facility.annual_ramp_increment = 0.0;
    config.facility.base_spot_price_per_kg = 10.0;
    config.facility.base_variable_cost_per_kg = 2.0;
    config.facility.base_fixed_opex_million = 3.0;
    config.facility.project_discount_rate = 0.0;
    config.debt.debt_fraction_of_base_capex = 0.0;

    // Three operating years each produce 5 million kg and CFADS of
    // 5*10 - 5*2 - 3 = 37 million. NPV is -100 + 3*37 = 11.
    const cf::ComparisonSummary result = cf::run_paired_simulation(config);
    check(near(
            result.without_instrument
                .project_npv_before_instruments_million.mean,
            11.0),
        "deterministic project NPV matches a hand-built cash-flow table");
    check(near(
            result.without_instrument.sponsor_npv_after_financing_million.mean,
            11.0),
        "unlevered sponsor cash flow matches deterministic project NPV");
    check(near(
            result.without_instrument.total_qualified_output_million_kg.mean,
            15.0),
        "deterministic qualified output matches the physical calculation");
    check(near(
            result.without_instrument
                .commercial_operation_timing_years_after_close.mean,
            1.0),
        "deterministic commercial-operation timing matches construction input");
}

void test_fee_is_a_visible_transfer() {
    cf::SimulationConfig config = deterministic_config();
    constexpr double fee = 7.5;
    config.instrument.upfront_fee_million = fee;

    const cf::ComparisonSummary result = cf::run_paired_simulation(config);
    check(near(result.paired_project_npv_change_million.mean, -fee),
        "an upfront fee reduces project NPV at time zero");
    check(near(result.paired_sponsor_npv_change_million.mean, -fee),
        "an upfront fee reduces sponsor NPV at time zero");
    check(near(result.paired_instrument_transfer_pv_million.mean, -fee),
        "the provider transfer reports the fee with project sign convention");
    check(result.with_instrument.expected_positive_support_payout_pv_million ==
            0.0,
        "a fee is not mislabeled as a provider payout");
}

void test_deterministic_price_support_caps() {
    cf::SimulationConfig config = deterministic_config();
    config.trials = 2U;
    config.facility.analysis_years = 4U;
    config.facility.planned_construction_years = 1.0;
    config.facility.base_capex_million = 100.0;
    config.facility.annual_nameplate_output_million_kg = 10.0;
    config.facility.steady_state_utilization = 0.5;
    config.facility.ramp_at_commercial_operation = 1.0;
    config.facility.annual_ramp_increment = 0.0;
    config.facility.base_spot_price_per_kg = 10.0;
    config.facility.project_discount_rate = 0.0;
    config.debt.debt_fraction_of_base_capex = 0.0;
    config.instrument.price_support_kind = cf::PriceSupportKind::OneWayFloor;
    config.instrument.price_support_fraction = 1.0;
    config.instrument.price_support_strike_per_kg = 20.0;
    config.instrument.price_support_annual_cap_million = 2.0;
    config.instrument.price_support_lifetime_cap_million = 5.0;

    const cf::ComparisonSummary floor = cf::run_paired_simulation(config);
    check(near(floor.paired_instrument_transfer_pv_million.mean, 5.0),
        "a deterministic floor exhausts but never exceeds its lifetime cap");
    check(near(
            floor.with_instrument.expected_positive_support_payout_pv_million,
            5.0),
        "the deterministic floor reports the exact capped positive payout");

    config.facility.base_spot_price_per_kg = 20.0;
    config.instrument.price_support_kind =
        cf::PriceSupportKind::TwoWayDifference;
    config.instrument.price_support_strike_per_kg = 10.0;
    const cf::ComparisonSummary two_way = cf::run_paired_simulation(config);
    check(near(two_way.paired_instrument_transfer_pv_million.mean, -5.0),
        "a deterministic two-way return exhausts the same absolute cap");
    check(two_way.with_instrument.expected_positive_support_payout_pv_million ==
            0.0,
        "a capped project return is not reported as provider support");
}

void test_one_way_floor_sample_reconciles() {
    cf::SimulationConfig config;
    config.scenario_label = "stochastic one-way floor test";
    config.source_note = "synthetic values created only for invariant tests";
    config.trials = 1'024U;
    config.seed = 99'001U;
    config.instrument.price_support_kind = cf::PriceSupportKind::OneWayFloor;
    config.instrument.price_support_fraction = 0.50;
    config.instrument.price_support_strike_per_kg = 18.0;
    config.instrument.price_support_annual_cap_million = 20.0;
    config.instrument.price_support_lifetime_cap_million = 75.0;

    const cf::ComparisonSummary result = cf::run_paired_simulation(config);
    check(result.paired_project_npv_change_million.p05 >= -1.0e-9,
        "the sampled one-way transfer has a non-negative fifth percentile");
    check(result.paired_default_probability_change <= 1.0e-12,
        "one-way support cannot increase default on unchanged paths");
    check(result.debt_default_introduced_within_horizon_count == 0U,
        "one-way support cannot induce a debt default on a fixed path");
    check(result.debt_default_accelerated_count == 0U,
        "one-way support cannot accelerate default on a fixed path");
    const double transition_default_change =
        (static_cast<double>(
             result.debt_default_introduced_within_horizon_count) -
         static_cast<double>(
             result.debt_default_avoided_within_horizon_count)) /
        static_cast<double>(config.trials);
    check(near(result.paired_default_probability_change,
            transition_default_change),
        "default-probability change reconciles to raw paired transitions");
    check(result.with_instrument.expected_positive_support_payout_pv_million >=
            0.0,
        "positive payout is reported with a non-negative sign");
    check(result.with_instrument.expected_positive_support_payout_pv_million <=
            config.instrument.price_support_lifetime_cap_million + 1.0e-9,
        "expected present-value payout cannot exceed the nominal lifetime cap");
    check(near(result.paired_project_npv_change_million.mean,
            result.paired_instrument_transfer_pv_million.mean),
        "paired project-value change reconciles to the signed transfer");
    check(near(result.with_instrument.expected_instrument_net_receipts_pv_million,
            result.with_instrument.expected_offtake_repricing_pv_million +
                result.with_instrument
                    .expected_price_support_net_settlement_pv_million +
                result.with_instrument
                    .expected_completion_delay_cover_payout_pv_million -
                result.with_instrument.expected_upfront_fee_million),
        "each instrument leg reconciles to total signed project receipts");
    const cf::SimulationSummary& lender = result.with_instrument;
    check(lender.mean_debt_loss_given_default_at_default_date_million.has_value(),
        "defaulted sampled paths report conditional lender-loss severity");
    if (lender.mean_debt_loss_given_default_at_default_date_million.has_value()) {
        check(near(
                lender
                    .unconditional_expected_debt_loss_at_default_date_million,
                lender.debt_default_probability *
                    *lender
                         .mean_debt_loss_given_default_at_default_date_million),
            "unconditional lender loss reconciles to PD times mean loss given default");
    }
    const std::size_t both_default_count =
        result.debt_default_delayed_count +
        result.debt_default_accelerated_count +
        result.debt_default_same_timing_count;
    const auto baseline_default_count = static_cast<std::size_t>(std::llround(
        result.without_instrument.debt_default_probability *
        static_cast<double>(config.trials)));
    const auto structured_default_count = static_cast<std::size_t>(std::llround(
        result.with_instrument.debt_default_probability *
        static_cast<double>(config.trials)));
    check(baseline_default_count ==
            result.debt_default_avoided_within_horizon_count +
                both_default_count,
        "baseline defaults reconcile to cures and both-default paths");
    check(structured_default_count ==
            result.debt_default_introduced_within_horizon_count +
                both_default_count,
        "structured defaults reconcile to introduced and both-default paths");
    check(result.paired_default_timing_change_years.has_value(),
        "both-default paths report a paired default-timing change");
    if (result.paired_default_timing_change_years.has_value()) {
        check(result.paired_default_timing_change_years->p05 >= -1.0e-9,
            "one-way support does not accelerate the sampled paired defaults");
    }
}

void test_two_way_contract_preserves_downside_to_project() {
    cf::SimulationConfig config = deterministic_config();
    config.debt.debt_fraction_of_base_capex = 0.0;
    config.facility.base_spot_price_per_kg = 20.0;
    config.instrument.price_support_kind =
        cf::PriceSupportKind::TwoWayDifference;
    config.instrument.price_support_fraction = 1.0;
    config.instrument.price_support_strike_per_kg = 10.0;
    config.instrument.price_support_annual_cap_million = 50.0;
    config.instrument.price_support_lifetime_cap_million = 200.0;

    const cf::ComparisonSummary result = cf::run_paired_simulation(config);
    check(result.paired_instrument_transfer_pv_million.mean < 0.0,
        "a two-way contract returns value when market price exceeds strike");
    check(result.with_instrument.expected_positive_support_payout_pv_million ==
            0.0,
        "returned value is not counted as a positive provider payout");
    check(near(result.paired_project_npv_change_million.mean,
            result.paired_instrument_transfer_pv_million.mean),
        "two-way settlement reconciles to project NPV");
}

void test_unlevered_case_and_finite_outputs() {
    cf::SimulationConfig config = deterministic_config();
    config.debt.debt_fraction_of_base_capex = 0.0;
    config.facility.analysis_years = 4U;
    const cf::ComparisonSummary result = cf::run_paired_simulation(config);

    check(!result.without_instrument.minimum_dscr.has_value(),
        "DSCR is not applicable when there is no debt");
    check(result.without_instrument.debt_default_probability == 0.0,
        "an unlevered project cannot default on debt");
    check(result.without_instrument
                .unconditional_expected_debt_loss_at_default_date_million ==
            0.0,
        "an unlevered project has no lender loss");
    check(!result.without_instrument
               .mean_debt_loss_given_default_at_default_date_million.has_value(),
        "conditional lender-loss severity is not applicable without defaults");
    check(!result.without_instrument
               .debt_default_timing_years_after_close.has_value(),
        "an unlevered project has no conditional default timing");
    check_distribution_finite(
        result.without_instrument.project_npv_before_instruments_million,
        "project NPV statistics are finite");
    check_distribution_finite(
        result.without_instrument.actual_capex_million,
        "capex statistics are finite");
    check_distribution_finite(
        result.without_instrument.total_qualified_output_million_kg,
        "output statistics are finite");
}

void test_negative_cfads_produces_negative_dscr() {
    cf::SimulationConfig config = deterministic_config();
    config.facility.base_spot_price_per_kg = 1.0;
    config.facility.base_variable_cost_per_kg = 10.0;
    config.facility.base_fixed_opex_million = 20.0;
    const cf::ComparisonSummary result = cf::run_paired_simulation(config);

    check(result.without_instrument.minimum_dscr.has_value(),
        "levered paths report a debt-service observation");
    if (result.without_instrument.minimum_dscr.has_value()) {
        check(result.without_instrument.minimum_dscr->mean < 0.0,
            "DSCR preserves negative CFADS instead of flooring it to zero");
    }
    check(result.without_instrument
              .debt_default_timing_years_after_close.has_value(),
        "a defaulted levered case reports conditional default timing");
}

void test_horizon_limited_completion_cover() {
    cf::SimulationConfig config = deterministic_config();
    config.trials = 4'096U;
    config.facility.analysis_years = 10U;
    config.risk.construction_duration_log_sigma = 1.0;
    config.instrument.completion_delay_trigger_years = 2.0;
    config.instrument.completion_payout_per_delay_year_million = 3.0;
    config.instrument.completion_delay_cover_cap_million = 1'000.0;

    const cf::ComparisonSummary result = cf::run_paired_simulation(config);
    const double maximum_elapsed_delay_payout =
        (static_cast<double>(config.facility.analysis_years) -
         config.instrument.completion_delay_trigger_years) *
        config.instrument.completion_payout_per_delay_year_million;
    check(result.with_instrument
                .expected_completion_delay_cover_payout_pv_million <=
            maximum_elapsed_delay_payout + 1.0e-9,
        "delay cover cannot use delay beyond the modeled observation horizon");
    check(result.without_instrument.fraction_paths_with_debt_service < 1.0,
        "very delayed paths remain visible through DSCR observation coverage");
    check(result.without_instrument.probability_terminal_debt_outstanding > 0.0,
        "the explicit terminal sponsor-payment convention is reported");
}

} // namespace

int main() {
    test_validation_boundaries();
    test_empty_instrument_identity_and_repeatability();
    test_deterministic_cash_flow_matches_hand_calculation();
    test_fee_is_a_visible_transfer();
    test_deterministic_price_support_caps();
    test_one_way_floor_sample_reconciles();
    test_two_way_contract_preserves_downside_to_project();
    test_unlevered_case_and_finite_outputs();
    test_negative_cfads_produces_negative_dscr();
    test_horizon_limited_completion_cover();

    if (failures != 0) {
        std::cerr << failures << " model test(s) failed\n";
        return 1;
    }
    std::cout << "all model tests passed\n";
    return 0;
}
