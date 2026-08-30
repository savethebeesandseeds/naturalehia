// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_price_ladder.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>

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

[[nodiscard]] bool near_relative(double first, double second,
    double relative_tolerance = 1.0e-8) {
    const double scale = std::max(std::abs(first), std::abs(second));
    return scale == 0.0 ||
        std::abs(first - second) <= relative_tolerance * scale;
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

[[nodiscard]] cf::AmbiguityEndpoint endpoint(
    double value, double low_weight, double high_weight) {
    return cf::AmbiguityEndpoint{value, {low_weight, high_weight}};
}

[[nodiscard]] cf::PooledLossProtectionRobustPoint protection_point(
    double coverage = 1.0 / 6.0, double investor_headroom = 0.0) {
    cf::PooledLossProtectionRobustPoint point;
    point.coverage_fraction = coverage;
    point.investor_target_worst_expected_npv_million = 0.0;
    point.investor_expected_npv_before_premium_million = {
        endpoint(investor_headroom, 0.0, 1.0),
        investor_headroom + 0.5,
        endpoint(investor_headroom + 1.0, 1.0, 0.0)};
    point.investor_signed_premium_headroom_million = investor_headroom;
    if (investor_headroom >= 0.0) {
        point.investor_maximum_nonnegative_premium_million =
            investor_headroom;
    }
    point.provider_minimum_robust_break_even_premium_million = 0.8;
    point.provider_risk.legal_support_cap_million = 20.0;
    point.provider_risk.contractual_maximum_exposure_million =
        coverage * 20.0;
    point.provider_risk.modeled_maximum_claim_million = coverage * 16.0;
    point.provider_risk.expected_claim_present_value_million = {
        endpoint(0.4, 1.0, 0.0),
        0.6,
        endpoint(0.8, 0.0, 1.0)};
    return point;
}

[[nodiscard]] cf::PooledLossProtectionSummary protection_summary(
    const cf::PooledLossProtectionRobustPoint& point) {
    cf::PooledLossProtectionSummary summary;
    summary.status =
        cf::PooledLossProtectionSolveStatus::CertifiedInteriorBracket;
    summary.investor_target_worst_expected_npv_million = 0.0;
    summary.legal_support_cap_million = 20.0;
    summary.aggregate_reference_principal_million = 20.0;
    summary.aggregate_covered_commitment_million = 20.0;
    summary.maximum_supported_coverage_fraction = 1.0;
    summary.investor_target_passing_coverage_fraction_upper_bound =
        point.coverage_fraction;
    summary.reported_coverage_fraction = point.coverage_fraction;
    summary.reported = point;
    summary.scenario_probability_bounds = {
        {"low-claim", 0.0, 0.5, 1.0},
        {"high-claim", 0.0, 0.5, 1.0},
    };
    return summary;
}

[[nodiscard]] cf::PooledLossProtectionConfig protection_terms() {
    cf::PooledLossProtectionConfig terms;
    terms.scenario_label = "synthetic provider price ladder protection";
    terms.source_note = "unit-test protection term";
    terms.provider_id = "test-provider";
    terms.settlement_month = 24U;
    terms.support_cap_million = 20.0;
    return terms;
}

[[nodiscard]] cf::ProviderPriceLadderConfig pricing_terms() {
    cf::ProviderPriceLadderConfig terms;
    terms.scenario_label = "synthetic provider cost ladder";
    terms.source_note = "unit-test cost assertions";
    terms.cost_bases_use_contractual_maximum_exposure = true;
    terms.collateral_and_capital_are_held_until_settlement = true;
    terms.variable_claim_expense_is_paid_at_claim_settlement = true;
    terms.fixed_expense_and_target_profit_are_month_zero_values = true;
    terms.incremental_cost_terms_are_separate_and_nonduplicative = true;
    terms.collateral_fraction_of_contractual_maximum_exposure = 0.5;
    terms.collateral_annual_effective_funding_rate = 0.06;
    terms.collateral_annual_effective_yield_rate = 0.02;
    terms.risk_capital_fraction_of_contractual_maximum_exposure = 0.3;
    terms.risk_capital_annual_effective_charge_rate = 0.10;
    terms.fixed_expense_upfront_million = 0.05;
    terms.variable_claim_expense_fraction = 0.10;
    terms.target_profit_upfront_million = 0.10;
    return terms;
}

void test_hand_calculated_reported_price_ladder() {
    const cf::PooledLossProtectionRobustPoint point = protection_point();
    const cf::PooledLossProtectionSummary summary =
        protection_summary(point);
    const cf::ProviderPriceLadderSummary priced =
        cf::evaluate_provider_price_ladder(
            protection_terms(), summary, point, pricing_terms());

    check(priced.status ==
            cf::ProviderPriceLadderStatus::
                ProviderAllInFloorExceedsInvestorCeiling,
        "the nonzero provider ladder exceeds a zero investor ceiling");
    check(near(priced.costs.contractual_maximum_exposure_million,
              10.0 / 3.0) &&
            near(priced.costs.collateral_base_million, 5.0 / 3.0) &&
            near(priced.costs.risk_capital_base_million, 1.0),
        "collateral and economic-capital bases use contractual maximum exposure");
    check(near(
              priced.costs.collateral_funding_carry_present_value_million,
              0.13866666666666666) &&
            near(priced.costs.risk_capital_charge_present_value_million,
              0.21),
        "two-year collateral carry and incremental economic-capital charge match hand calculations");
    check(near(priced.costs.claim_only_robust_floor_million, 0.8) &&
            near(priced.costs
                    .variable_claim_expense_at_robust_endpoint_million,
              0.08) &&
            near(priced.costs.provider_cost_recovery_floor_million,
              1.2786666666666666) &&
            near(priced.costs.provider_all_in_floor_million,
              1.3786666666666667),
        "the cumulative price ladder reconciles claim, expenses, carry, capital, and profit");
    check(priced.provider_cost_recovery_requirement_million.maximum
                .scenario_weights ==
            point.provider_risk.expected_claim_present_value_million.maximum
                .scenario_weights &&
            priced.provider_all_in_revenue_requirement_million.minimum
                    .scenario_weights ==
                point.provider_risk.expected_claim_present_value_million
                    .minimum.scenario_weights,
        "positive affine provider costs preserve both ambiguity endpoint witnesses");
    check(near(priced.provider_premium_support_required_million,
              1.3786666666666667) &&
            priced.investor_target_restoration_required_million == 0.0 &&
            near(priced.all_in_support_gap_million,
              1.3786666666666667) &&
            priced.support_gap_decomposition_reconciliation_error_million <=
                1.0e-12,
        "provider premium support is separated from investor target restoration and reconciles");
    check(priced.maximum_cost_ladder_reconciliation_error_million <=
                1.0e-12 &&
            priced.maximum_transformed_range_reconciliation_error_million <=
                1.0e-12 &&
            !priced.provider_default_risk_is_modeled &&
            !priced.fair_value_is_estimated,
        "controls reconcile while provider default and fair value remain outside the model");
}

void test_interval_and_negative_investor_capacity() {
    cf::PooledLossProtectionRobustPoint feasible =
        protection_point(1.0 / 6.0, 2.0);
    cf::PooledLossProtectionSummary feasible_summary =
        protection_summary(feasible);
    cf::ProviderPriceLadderConfig zero_cost = pricing_terms();
    zero_cost.collateral_fraction_of_contractual_maximum_exposure = 0.0;
    zero_cost.collateral_annual_effective_funding_rate = 0.0;
    zero_cost.collateral_annual_effective_yield_rate = 0.0;
    zero_cost.risk_capital_fraction_of_contractual_maximum_exposure = 0.0;
    zero_cost.risk_capital_annual_effective_charge_rate = 0.0;
    zero_cost.fixed_expense_upfront_million = 0.0;
    zero_cost.variable_claim_expense_fraction = 0.0;
    zero_cost.target_profit_upfront_million = 0.1;
    const cf::ProviderPriceLadderSummary interval =
        cf::evaluate_provider_price_ladder(protection_terms(),
            feasible_summary, feasible, zero_cost);
    check(interval.status ==
            cf::ProviderPriceLadderStatus::
                RobustAllInPremiumIntervalExists &&
            interval.robust_price_interval_lower_bound_million.has_value() &&
            near(*interval.robust_price_interval_lower_bound_million, 0.9) &&
            interval.robust_price_interval_upper_bound_million.has_value() &&
            near(*interval.robust_price_interval_upper_bound_million, 2.0),
        "a provider all-in floor below investor capacity creates the exact robust interval");

    cf::PooledLossProtectionRobustPoint certified =
        protection_point(1.0 / 6.0, 1.0);
    certified.investor_maximum_nonnegative_premium_million = 0.9;
    const cf::PooledLossProtectionSummary certified_summary =
        protection_summary(certified);
    cf::ProviderPriceLadderConfig narrow = zero_cost;
    narrow.target_profit_upfront_million = 0.15;
    const cf::ProviderPriceLadderSummary rounded_ceiling =
        cf::evaluate_provider_price_ladder(protection_terms(),
            certified_summary, certified, narrow);
    check(rounded_ceiling.status ==
            cf::ProviderPriceLadderStatus::
                ProviderAllInFloorExceedsInvestorCeiling &&
            near(rounded_ceiling.costs.provider_all_in_floor_million, 0.95) &&
            near(rounded_ceiling.signed_all_in_bilateral_margin_million,
              -0.05) &&
            near(rounded_ceiling.provider_premium_support_required_million,
              0.05) &&
            near(rounded_ceiling.all_in_support_gap_million, 0.05),
        "gap and status use the certified payable ceiling rather than larger raw signed headroom");

    cf::PooledLossProtectionRobustPoint deficient =
        protection_point(1.0 / 6.0, -0.25);
    const cf::PooledLossProtectionSummary deficient_summary =
        protection_summary(deficient);
    const cf::ProviderPriceLadderSummary no_capacity =
        cf::evaluate_provider_price_ladder(protection_terms(),
            deficient_summary, deficient, zero_cost);
    check(no_capacity.status ==
            cf::ProviderPriceLadderStatus::
                InvestorCannotPayNonnegativePremium &&
            near(no_capacity.investor_target_restoration_required_million,
              0.25) &&
            near(no_capacity.provider_premium_support_required_million,
              0.9) &&
            near(no_capacity.all_in_support_gap_million, 1.15) &&
            no_capacity.support_gap_decomposition_reconciliation_error_million <=
                1.0e-12,
        "negative headroom is decomposed into investor restoration and provider premium support");
}

void test_tiny_rate_and_near_equal_carry_stability() {
    const cf::PooledLossProtectionRobustPoint point = protection_point();
    const cf::PooledLossProtectionSummary summary =
        protection_summary(point);
    cf::ProviderPriceLadderConfig tiny = pricing_terms();
    tiny.collateral_annual_effective_funding_rate = 1.0001e-12;
    tiny.collateral_annual_effective_yield_rate = 1.0e-12;
    tiny.risk_capital_annual_effective_charge_rate = 1.0e-16;
    tiny.fixed_expense_upfront_million = 0.0;
    tiny.variable_claim_expense_fraction = 0.0;
    tiny.target_profit_upfront_million = 0.0;

    const cf::ProviderPriceLadderSummary priced =
        cf::evaluate_provider_price_ladder(
            protection_terms(), summary, point, tiny);
    const double expected_carry_first_order =
        (5.0 / 3.0) * 2.0 *
        (tiny.collateral_annual_effective_funding_rate -
            tiny.collateral_annual_effective_yield_rate);
    const double expected_capital_first_order = 1.0 * 2.0 *
        tiny.risk_capital_annual_effective_charge_rate;
    check(priced.costs.collateral_funding_carry_present_value_million >
                0.0 &&
            near_relative(
                priced.costs
                    .collateral_funding_carry_present_value_million,
                expected_carry_first_order) &&
            priced.costs.risk_capital_charge_present_value_million > 0.0 &&
            near_relative(
                priced.costs.risk_capital_charge_present_value_million,
                expected_capital_first_order),
        "near-equal collateral rates and sub-epsilon capital rates retain their positive economic carry");
}

void test_selection_and_term_validation() {
    const cf::PooledLossProtectionRobustPoint reported = protection_point();
    cf::PooledLossProtectionSummary summary =
        protection_summary(reported);
    cf::ProviderPriceLadderConfig terms = pricing_terms();

    summary.investor_target_passing_coverage_fraction_upper_bound.reset();
    expect_invalid_argument(
        [&summary, &reported, &terms] {
            (void)cf::evaluate_provider_price_ladder(
                protection_terms(), summary, reported, terms);
        },
        "reported selection does not silently substitute a failing maximum-supported point");

    summary = protection_summary(reported);
    terms.coverage_selection =
        cf::ProviderPriceCoverageSelection::ExplicitCoverageFraction;
    terms.explicit_coverage_fraction = 0.5;
    expect_invalid_argument(
        [&summary, &reported, &terms] {
            (void)cf::evaluate_provider_price_ladder(
                protection_terms(), summary, reported, terms);
        },
        "explicit selection requires a point at the exact requested fraction");

    summary = protection_summary(reported);
    terms = pricing_terms();
    summary.aggregate_covered_commitment_million = 19.0;
    expect_invalid_argument(
        [&summary, &reported, &terms] {
            (void)cf::evaluate_provider_price_ladder(
                protection_terms(), summary, reported, terms);
        },
        "provider pricing rejects a stale compatibility alias that differs from aggregate contractual reference principal");

    terms = pricing_terms();
    terms.collateral_annual_effective_funding_rate = 0.01;
    terms.collateral_annual_effective_yield_rate = 0.02;
    expect_invalid_argument(
        [&terms] { cf::validate_provider_price_ladder_config(terms); },
        "negative collateral carry is excluded from the v0.1 cost convention");
    terms = pricing_terms();
    terms.incremental_cost_terms_are_separate_and_nonduplicative = false;
    expect_invalid_argument(
        [&terms] { cf::validate_provider_price_ladder_config(terms); },
        "separate and nonduplicative incremental cost terms must be asserted");
    terms = pricing_terms();
    terms.provider_default_risk_is_modeled = true;
    expect_invalid_argument(
        [&terms] { cf::validate_provider_price_ladder_config(terms); },
        "v0.1 rejects an unsupported provider-default modeling claim");
    terms = pricing_terms();
    terms.fair_value_is_claimed = true;
    expect_invalid_argument(
        [&terms] { cf::validate_provider_price_ladder_config(terms); },
        "v0.1 rejects a fair-value claim");
    terms = pricing_terms();
    terms.variable_claim_expense_fraction =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&terms] { cf::validate_provider_price_ladder_config(terms); },
        "non-finite variable expense is rejected");
}

} // namespace

int main() {
    test_hand_calculated_reported_price_ladder();
    test_interval_and_negative_investor_capacity();
    test_tiny_rate_and_near_equal_carry_stability();
    test_selection_and_term_validation();

    if (failures != 0) {
        std::cerr << failures << " provider price-ladder test(s) failed\n";
        return 1;
    }
    std::cout << "all provider price-ladder tests passed\n";
    return 0;
}
