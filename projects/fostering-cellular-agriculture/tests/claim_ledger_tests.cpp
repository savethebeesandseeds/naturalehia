// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
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

[[nodiscard]] bool exact_value(
    const cf::ClaimLedgerValue& value, double expected) {
    return value.status == cf::ClaimLedgerValueStatus::Known &&
        value.lower.has_value() && value.upper.has_value() &&
        near(*value.lower, expected) && near(*value.upper, expected);
}

[[nodiscard]] bool has_blocker(const std::vector<std::string>& blockers,
    std::string_view fragment) {
    return std::any_of(blockers.begin(), blockers.end(),
        [fragment](const std::string& blocker) {
            return blocker.find(fragment) != std::string::npos;
        });
}

template <typename Function>
void expect_invalid_argument(Function&& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

template <typename Function>
void expect_invalid_argument_containing(Function&& operation,
    std::string_view fragment, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument& error) {
        check(std::string_view(error.what()).find(fragment) !=
                std::string_view::npos,
            message);
    } catch (...) {
        check(false, message);
    }
}

template <typename Function>
void run_test(std::string_view name, Function&& operation) {
    try {
        operation();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << name
                  << " raised an unexpected exception: " << error.what()
                  << '\n';
        ++failures;
    } catch (...) {
        std::cerr << "FAIL: " << name
                  << " raised a non-standard exception\n";
        ++failures;
    }
}

[[nodiscard]] cf::ClaimLedgerEntry entry(std::string id,
    cf::ClaimLedgerEntryKind kind, double amount, std::size_t period,
    std::string group = "none", std::size_t known_at = 0U,
    std::string provider = "none") {
    cf::ClaimLedgerEntry result;
    result.entry_id = std::move(id);
    result.economic_fact_id = result.entry_id;
    result.event_group_id = std::move(group);
    result.kind = kind;
    result.period = period;
    result.known_at_period = known_at;
    result.value = cf::claim_ledger_known(amount);
    result.source_record_id = "SYNTHETIC";
    result.provider_claim_id = std::move(provider);
    return result;
}

[[nodiscard]] std::vector<cf::ClaimLedgerEntry> closing_entries() {
    constexpr std::size_t close = 0U;
    const std::string group = "synthetic-closing";
    return {
        entry("buyer-price", cf::ClaimLedgerEntryKind::BuyerPrice, 9.0,
            close, group),
        entry("buyer-cost", cf::ClaimLedgerEntryKind::BuyerDirectCost, 0.2,
            close, group),
        entry("gross-proceeds",
            cf::ClaimLedgerEntryKind::BorrowerGrossProceeds, 9.0, close,
            group),
        entry("net-proceeds",
            cf::ClaimLedgerEntryKind::BorrowerNetProceeds, 8.5, close,
            group),
        entry("cash-fee", cf::ClaimLedgerEntryKind::CashFee, 0.3, close,
            group),
        entry("borrower-third-party-cost",
            cf::ClaimLedgerEntryKind::BorrowerThirdPartyCost, 0.2, close,
            group),
        entry("funded-principal",
            cf::ClaimLedgerEntryKind::FundedPrincipal, 9.0, close, group),
        entry("original-issue-discount",
            cf::ClaimLedgerEntryKind::OriginalIssueDiscount, 0.0, close,
            group),
        entry("original-issue-premium",
            cf::ClaimLedgerEntryKind::OriginalIssuePremium, 0.0, close,
            group),
        entry("capitalized-fee",
            cf::ClaimLedgerEntryKind::CapitalizedFee, 1.0, close, group),
    };
}

[[nodiscard]] cf::ClaimLedgerProviderClaim provider_claim() {
    cf::ClaimLedgerProviderClaim provider;
    provider.provider_claim_id = "synthetic-provider-claim";
    provider.provider_id = "synthetic-provider";
    provider.known_at_period = 0U;
    provider.shortfall_allocation_fraction = cf::claim_ledger_known(1.0);
    provider.coverage_fraction = cf::claim_ledger_known(0.5);
    provider.deductible_million = cf::claim_ledger_known(0.0);
    provider.maximum_cash_million = cf::claim_ledger_known(4.0);
    provider.settlement_lag_periods = cf::claim_ledger_known(0.0);
    provider.covers_principal_due = true;
    provider.covers_interest_due = false;
    provider.payment_right_evidenced = true;
    provider.provider_identity_evidenced = true;
    provider.coverage_and_priority_evidenced = true;
    provider.obligation_priority =
        cf::ClaimLedgerProviderAllocationPriority::PrincipalFirst;
    provider.source_record_id = "SYNTHETIC";
    return provider;
}

[[nodiscard]] cf::ClaimLedgerConfig hand_config() {
    cf::ClaimLedgerConfig config;
    config.model_version = std::string(cf::kClaimLedgerModelVersion);
    config.ledger_id = "one-claim-hand-ledger";
    config.project_id = "synthetic-project";
    config.claim_id = "synthetic-claim";
    config.currency_label = "TEST";
    config.monetary_basis = "nominal synthetic millions";
    config.decision_period = 0U;
    config.horizon_period = 12U;
    config.contractual_face_amount_million =
        cf::claim_ledger_known(10.0);
    config.face_amount_known_at_period = 0U;
    config.opening_principal_million = cf::claim_ledger_known(0.0);
    config.opening_principal_known_at_period = 0U;
    config.opening_accrued_interest_million =
        cf::claim_ledger_known(0.0);
    config.opening_accrued_interest_known_at_period = 0U;
    config.annual_effective_discount_rate = cf::claim_ledger_known(0.10);
    config.discount_rate_known_at_period = 0U;
    config.common_entries = closing_entries();
    config.common_covenant_events = {cf::ClaimLedgerCovenantEvent{
        "closing-covenant-pass", "minimum-liquidity", 0U, 0U,
        cf::ClaimLedgerCovenantState::Pass, "SYNTHETIC"}};
    config.provider_claims = {provider_claim()};

    cf::ClaimLedgerScenario success;
    success.scenario_id = "performing-maturity";
    success.physical_probability = cf::claim_ledger_known(0.8);
    success.probability_known_at_period = 0U;
    success.probability_source_record_id = "SYNTHETIC";
    success.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    success.cash_path_status_known_at_period = 0U;
    success.cash_path_status_source_record_id = "SYNTHETIC";
    success.entries = {
        entry("success-interest-accrual",
            cf::ClaimLedgerEntryKind::InterestAccrual, 1.0, 12U,
            "success-maturity"),
        entry("success-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 12U,
            "success-maturity"),
        entry("success-interest-due",
            cf::ClaimLedgerEntryKind::InterestDue, 1.0, 12U,
            "success-maturity"),
        entry("success-principal-cash",
            cf::ClaimLedgerEntryKind::PrincipalCash, 10.0, 12U,
            "success-maturity"),
        entry("success-interest-cash",
            cf::ClaimLedgerEntryKind::InterestCash, 1.0, 12U,
            "success-maturity"),
    };

    cf::ClaimLedgerScenario failure;
    failure.scenario_id = "failure-with-provider";
    failure.physical_probability = cf::claim_ledger_known(0.2);
    failure.probability_known_at_period = 0U;
    failure.probability_source_record_id = "SYNTHETIC";
    failure.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    failure.cash_path_status_known_at_period = 0U;
    failure.cash_path_status_source_record_id = "SYNTHETIC";
    failure.entries = {
        entry("failure-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 12U,
            "failure-maturity"),
        entry("failure-recovery-principal-cash",
            cf::ClaimLedgerEntryKind::RecoveryPrincipalCash, 2.0, 12U,
            "failure-maturity"),
        entry("failure-guarantee-principal-cash",
            cf::ClaimLedgerEntryKind::GuaranteePrincipalCash, 4.0, 12U,
            "failure-maturity", 0U, "synthetic-provider-claim"),
        entry("failure-principal-writeoff",
            cf::ClaimLedgerEntryKind::PrincipalWriteoff, 4.0, 12U,
            "failure-maturity"),
    };
    config.scenarios = {std::move(success), std::move(failure)};
    return config;
}

void test_hand_reconciliation() {
    const cf::ClaimLedgerConfig config = hand_config();
    cf::validate_claim_ledger_config(config);
    const cf::ClaimLedgerSummary summary =
        cf::evaluate_claim_ledger(config);

    check(summary.scenarios.size() == 2U,
        "both mutually exclusive physical scenarios remain visible");
    check(summary.expected_investor_cashflows_million.size() == 13U &&
            exact_value(summary.expected_investor_cashflows_million[0U],
                -8.9) &&
            exact_value(summary.expected_investor_cashflows_million[12U],
                10.0),
        "expected cash reconstructs all-in buyer outflow and weighted receipts");
    check(exact_value(summary.expected_npv_million, 0.190909090909091) &&
            exact_value(summary.expected_principal_loss_million, 0.8) &&
            exact_value(summary.expected_total_loss_million, 0.8),
        "hand NPV and physical expected loss reconcile");
    check(exact_value(summary.annual_effective_rate_preimage,
              0.123595505617978) &&
            summary.readiness.expected_cash_ready &&
            summary.readiness.npv_ready &&
            summary.readiness.rate_preimage_ready,
        "the expected-cash annual rate preimage reconstructs the all-in price");
    check(summary.readiness.provider_claim_applicable &&
            summary.readiness.provider_claim_ready,
        "the evidenced provider claim is derived rather than assumed");

    const cf::ClaimLedgerScenarioResult& success = summary.scenarios[0U];
    const cf::ClaimLedgerScenarioResult& failure = summary.scenarios[1U];
    check(success.decision_path.settlement_reconciled &&
            success.decision_path.rollforwards_reconciled &&
            success.decision_path.contractual_face_reconciled &&
            exact_value(success.decision_path.terminal_total_exposure_million,
                0.0),
        "the performing path settles and closes both ledgers");
    check(exact_value(failure.decision_path.principal_loss_million, 4.0) &&
            failure.decision_path.provider_claims.size() == 1U &&
            exact_value(failure.decision_path.provider_claims[0U]
                            .total_claim_generated_million,
                4.0) &&
            exact_value(failure.decision_path.provider_claims[0U]
                            .total_guarantee_cash_million,
                4.0),
        "failure recovery, derived provider claim, provider cash and writeoff reconcile");
    check(exact_value(success.decision_path.periods[0U]
                          .borrower_net_proceeds_million,
              8.5) &&
            exact_value(success.decision_path.periods[0U]
                          .closing_principal_million,
              10.0),
        "borrower proceeds, buyer price and contractual balance remain distinct");
}

void test_missing_is_not_zero() {
    cf::ClaimLedgerConfig config = hand_config();
    config.annual_effective_discount_rate = cf::claim_ledger_unknown();
    const cf::ClaimLedgerSummary no_discount =
        cf::evaluate_claim_ledger(config);
    check(no_discount.expected_npv_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            !no_discount.expected_npv_million.lower.has_value() &&
            no_discount.readiness.expected_cash_ready &&
            !no_discount.readiness.npv_ready &&
            no_discount.readiness.rate_preimage_ready,
        "an unknown discount input blocks NPV without blocking cash or its rate preimage");

    config = hand_config();
    config.scenarios[0U].physical_probability =
        cf::claim_ledger_unknown();
    const cf::ClaimLedgerSummary no_probability =
        cf::evaluate_claim_ledger(config);
    const bool cash_is_unavailable =
        no_probability.expected_investor_cashflows_million.empty() ||
        std::all_of(
            no_probability.expected_investor_cashflows_million.begin(),
            no_probability.expected_investor_cashflows_million.end(),
            [](const cf::ClaimLedgerValue& value) {
                return value.status ==
                    cf::ClaimLedgerValueStatus::Unknown;
            });
    check(cash_is_unavailable &&
            no_probability.expected_total_loss_million.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            !no_probability.readiness.expected_cash_ready &&
            !no_probability.readiness.npv_ready &&
            !no_probability.readiness.rate_preimage_ready,
        "an unknown physical probability never becomes a zero-weight path");
}

void test_backtest_and_covenant_separation() {
    const cf::ClaimLedgerConfig before = hand_config();
    cf::ClaimLedgerConfig after = before;
    after.scenarios[1U].covenant_events.push_back(
        cf::ClaimLedgerCovenantEvent{"later-consent-record",
            "minimum-liquidity", 6U, 24U,
            cf::ClaimLedgerCovenantState::BreachWithNonExerciseConsent,
            "SYNTHETIC"});
    cf::ClaimLedgerEntry actual_principal_cash = entry(
        "success-principal-cash-actual",
        cf::ClaimLedgerEntryKind::PrincipalCash, 10.0, 12U,
        "success-maturity", 24U);
    actual_principal_cash.economic_fact_id = "success-principal-cash";
    after.scenarios[0U].entries.push_back(std::move(actual_principal_cash));
    const cf::ClaimLedgerSummary first =
        cf::evaluate_claim_ledger(before);
    const cf::ClaimLedgerSummary second =
        cf::evaluate_claim_ledger(after);
    check(second.expected_npv_million.lower.has_value() &&
            second.annual_effective_rate_preimage.lower.has_value() &&
            exact_value(first.expected_npv_million,
                *second.expected_npv_million.lower) &&
            exact_value(first.annual_effective_rate_preimage,
                *second.annual_effective_rate_preimage.lower),
        "adding later covenant evidence leaves the ex-ante return hash inputs unchanged economically");
    check(second.common_decision_covenant_events.size() == 1U &&
            second.common_decision_covenant_events[0U].state ==
                cf::ClaimLedgerCovenantState::Pass &&
            second.scenarios[1U].backtest_covenant_events.size() == 1U &&
            second.scenarios[1U].backtest_covenant_events[0U].state ==
                cf::ClaimLedgerCovenantState::
                    BreachWithNonExerciseConsent,
        "later non-exercise consent is a backtest state and is not renamed default or waiver");
    check(exact_value(second.scenarios[0U]
                          .decision_path.periods[12U]
                          .principal_cash_million,
              10.0) &&
            exact_value(second.scenarios[0U]
                            .full_path.periods[12U]
                            .principal_cash_million,
                10.0) &&
            second.scenarios[0U].backtest_entry_ids.size() == 1U,
        "a later actual replaces its forecast economic fact in the full cut instead of being summed with it");
}

void test_conversion_is_noncash() {
    cf::ClaimLedgerConfig config = hand_config();
    config.provider_claims.clear();
    cf::ClaimLedgerScenario conversion;
    conversion.scenario_id = "conversion";
    conversion.physical_probability = cf::claim_ledger_known(1.0);
    conversion.probability_known_at_period = 0U;
    conversion.probability_source_record_id = "SYNTHETIC";
    conversion.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    conversion.cash_path_status_known_at_period = 0U;
    conversion.cash_path_status_source_record_id = "SYNTHETIC";
    conversion.entries = {
        entry("converted-principal",
            cf::ClaimLedgerEntryKind::ConversionPrincipalExtinguishment,
            10.0, 12U, "conversion-event"),
        entry("conversion-units", cf::ClaimLedgerEntryKind::ConversionUnits,
            100.0, 12U, "conversion-event"),
    };
    config.conversion_unit_label = "synthetic-equity-unit";
    config.conversion_unit_basis =
        "one synthetic unit per recorded conversion unit";
    config.scenarios = {std::move(conversion)};
    const cf::ClaimLedgerSummary summary =
        cf::evaluate_claim_ledger(config);
    const cf::ClaimLedgerPeriodResult& maturity =
        summary.scenarios[0U].decision_path.periods[12U];
    check(exact_value(maturity.conversion_units, 100.0) &&
            exact_value(maturity.investor_cashflow_million, 0.0) &&
            exact_value(summary.scenarios[0U].decision_path
                            .terminal_principal_million,
                0.0),
        "conversion extinguishes debt and records units without inventing cash");
    check(!summary.readiness.expected_cash_ready &&
            !summary.readiness.npv_ready &&
            !summary.readiness.rate_preimage_ready &&
            has_blocker(summary.readiness.expected_cash_blockers,
                "non-cash conversion units"),
        "noncash conversion alone cannot produce a complete expected-cash return");
}

void test_provider_and_balance_adversaries() {
    cf::ClaimLedgerConfig config = hand_config();
    config.scenarios[1U].entries[2U].value = cf::claim_ledger_known(4.1);
    config.scenarios[1U].entries[3U].value = cf::claim_ledger_known(3.9);
    expect_invalid_argument(
        [&config] { (void)cf::evaluate_claim_ledger(config); },
        "provider cash above the derived payable claim is rejected");

    config = hand_config();
    config.provider_claims[0U].covers_interest_due = true;
    config.provider_claims[0U].obligation_priority =
        cf::ClaimLedgerProviderAllocationPriority::InterestFirst;
    config.scenarios[1U].entries.push_back(entry(
        "failure-interest-accrual",
        cf::ClaimLedgerEntryKind::InterestAccrual, 4.0, 12U,
        "failure-maturity"));
    config.scenarios[1U].entries.push_back(entry(
        "failure-interest-due", cf::ClaimLedgerEntryKind::InterestDue,
        4.0, 12U, "failure-maturity"));
    config.scenarios[1U].entries.push_back(entry(
        "failure-interest-writeoff",
        cf::ClaimLedgerEntryKind::AccruedInterestWriteoff, 4.0, 12U,
        "failure-maturity"));
    expect_invalid_argument(
        [&config] { (void)cf::evaluate_claim_ledger(config); },
        "provider principal cash cannot consume an interest-priority claim");

    config = hand_config();
    config.provider_claims[0U].covers_interest_due = true;
    config.provider_claims[0U].obligation_priority =
        cf::ClaimLedgerProviderAllocationPriority::ProRata;
    config.scenarios[1U].entries.push_back(entry(
        "failure-interest-accrual",
        cf::ClaimLedgerEntryKind::InterestAccrual, 8.0, 12U,
        "failure-maturity"));
    config.scenarios[1U].entries.push_back(entry(
        "failure-interest-due", cf::ClaimLedgerEntryKind::InterestDue,
        8.0, 12U, "failure-maturity"));
    config.scenarios[1U].entries.push_back(entry(
        "failure-interest-writeoff",
        cf::ClaimLedgerEntryKind::AccruedInterestWriteoff, 8.0, 12U,
        "failure-maturity"));
    expect_invalid_argument(
        [&config] { (void)cf::evaluate_claim_ledger(config); },
        "provider principal cash cannot consume the interest share of a pro-rata claim");

    config = hand_config();
    config.provider_claims[0U].coverage_fraction =
        cf::claim_ledger_unknown();
    const cf::ClaimLedgerSummary unproved =
        cf::evaluate_claim_ledger(config);
    check(!unproved.readiness.provider_claim_ready &&
            !unproved.readiness.expected_cash_ready,
        "cash from a provider with unknown coverage cannot support expected cash");

    config = hand_config();
    cf::ClaimLedgerProviderClaim second = provider_claim();
    second.provider_claim_id = "second-provider-claim";
    second.provider_id = "second-provider";
    second.shortfall_allocation_fraction = cf::claim_ledger_known(0.5);
    config.provider_claims.push_back(std::move(second));
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "overlapping provider shortfall allocations are rejected");

    config = hand_config();
    second = provider_claim();
    second.provider_claim_id = "interest-only-provider-claim";
    second.provider_id = "interest-only-provider";
    second.covers_principal_due = false;
    second.covers_interest_due = true;
    second.obligation_priority =
        cf::ClaimLedgerProviderAllocationPriority::InterestFirst;
    config.provider_claims.push_back(std::move(second));
    cf::validate_claim_ledger_config(config);
    const cf::ClaimLedgerSummary disjoint_provider_categories =
        cf::evaluate_claim_ledger(config);
    check(std::all_of(disjoint_provider_categories.scenarios.begin(),
              disjoint_provider_categories.scenarios.end(),
              [](const cf::ClaimLedgerScenarioResult& scenario) {
                  return scenario.decision_path.provider_claims.size() ==
                          2U &&
                      std::all_of(
                          scenario.decision_path.provider_claims.begin(),
                          scenario.decision_path.provider_claims.end(),
                          [](const cf::ClaimLedgerProviderPathResult& path) {
                              return path.computable;
                          });
              }),
        "full principal-only and interest-only provider allocations remain independent rather than being treated as overlapping coverage");

    config = hand_config();
    config.provider_claims[0U].shortfall_allocation_fraction =
        cf::claim_ledger_known(0.5);
    config.provider_claims[0U].coverage_fraction =
        cf::claim_ledger_known(1.0);
    config.provider_claims[0U].maximum_cash_million =
        cf::claim_ledger_known(10.0);
    second = provider_claim();
    second.provider_claim_id = "second-provider-claim";
    second.provider_id = "second-provider";
    second.shortfall_allocation_fraction = cf::claim_ledger_known(0.5);
    second.coverage_fraction = cf::claim_ledger_known(1.0);
    second.maximum_cash_million = cf::claim_ledger_known(10.0);
    config.provider_claims.push_back(std::move(second));
    cf::ClaimLedgerScenario shared_provider_path;
    shared_provider_path.scenario_id = "two-provider-resolution";
    shared_provider_path.physical_probability = cf::claim_ledger_known(1.0);
    shared_provider_path.probability_known_at_period = 0U;
    shared_provider_path.probability_source_record_id = "SYNTHETIC";
    shared_provider_path.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    shared_provider_path.cash_path_status_known_at_period = 0U;
    shared_provider_path.cash_path_status_source_record_id = "SYNTHETIC";
    shared_provider_path.entries = {
        entry("two-provider-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 12U,
            "two-provider-due"),
        entry("first-provider-principal-cash",
            cf::ClaimLedgerEntryKind::GuaranteePrincipalCash, 5.0, 12U,
            "first-provider-payment", 0U,
            "synthetic-provider-claim"),
        entry("second-provider-principal-cash",
            cf::ClaimLedgerEntryKind::GuaranteePrincipalCash, 5.0, 12U,
            "second-provider-payment", 0U, "second-provider-claim"),
    };
    config.scenarios = {std::move(shared_provider_path)};
    const cf::ClaimLedgerSummary shared_providers =
        cf::evaluate_claim_ledger(config);
    const auto& provider_paths =
        shared_providers.scenarios[0U].decision_path.provider_claims;
    check(provider_paths.size() == 2U &&
            exact_value(provider_paths[0U].total_claim_generated_million,
                5.0) &&
            exact_value(provider_paths[0U].total_guarantee_cash_million,
                5.0) &&
            exact_value(provider_paths[1U].total_claim_generated_million,
                5.0) &&
            exact_value(provider_paths[1U].total_guarantee_cash_million,
                5.0) &&
            exact_value(shared_providers.scenarios[0U].decision_path
                            .terminal_total_exposure_million,
                0.0),
        "two non-overlapping provider allocations route independently and close one underlying exposure exactly once");

    config = hand_config();
    second = provider_claim();
    second.provider_claim_id = "post-decision-provider-claim";
    second.provider_id = "post-decision-provider";
    second.known_at_period = 1U;
    second.shortfall_allocation_fraction =
        cf::claim_ledger_bounded(0.1, 0.2);
    second.payment_right_evidenced = false;
    config.provider_claims.push_back(std::move(second));
    const cf::ClaimLedgerSummary future_provider =
        cf::evaluate_claim_ledger(config);
    check(future_provider.readiness.expected_cash_ready &&
            future_provider.scenarios[1U].decision_path.provider_claims
                    .size() == 1U &&
            future_provider.scenarios[1U].full_path.provider_claims.size() ==
                2U &&
            !future_provider.scenarios[1U].full_path.provider_claims[0U]
                 .computable &&
            has_blocker(future_provider.scenarios[1U]
                            .full_path.provider_claims[0U]
                            .blockers,
                "overlap above 100 percent"),
        "a later provider cannot contaminate the valid decision cut, and even its minimum bounded overlapping allocation blocks every full-cut provider when another term is unevidenced");

    config = hand_config();
    config.scenarios[0U].entries[1U].value =
        cf::claim_ledger_known(10.1);
    expect_invalid_argument(
        [&config] { (void)cf::evaluate_claim_ledger(config); },
        "contractual principal due above the claim balance is rejected");

    config = hand_config();
    config.scenarios[0U].entries.push_back(entry(
        "excess-capitalized-interest",
        cf::ClaimLedgerEntryKind::CapitalizedInterest, 1.1, 12U,
        "success-maturity"));
    expect_invalid_argument(
        [&config] { (void)cf::evaluate_claim_ledger(config); },
        "PIK capitalization cannot exceed accrued interest");

    config = hand_config();
    config.scenarios[0U].entries.push_back(entry(
        "first-full-principal-due",
        cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 6U,
        "first-due-event"));
    expect_invalid_argument(
        [&config] { (void)cf::evaluate_claim_ledger(config); },
        "a repeated due notice cannot generate a second shortfall on the same principal");

    config = hand_config();
    config.provider_claims[0U].covers_interest_due = true;
    config.provider_claims[0U].maximum_cash_million =
        cf::claim_ledger_known(100.0);
    config.contractual_face_amount_million = cf::claim_ledger_known(20.0);
    config.provider_claims[0U].obligation_priority =
        cf::ClaimLedgerProviderAllocationPriority::PrincipalFirst;
    cf::ClaimLedgerScenario capitalized;
    capitalized.scenario_id = "capitalized-interest-provider";
    capitalized.physical_probability = cf::claim_ledger_known(1.0);
    capitalized.probability_known_at_period = 0U;
    capitalized.probability_source_record_id = "SYNTHETIC";
    capitalized.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    capitalized.cash_path_status_known_at_period = 0U;
    capitalized.cash_path_status_source_record_id = "SYNTHETIC";
    capitalized.entries = {
        entry("capitalized-interest-accrual",
            cf::ClaimLedgerEntryKind::InterestAccrual, 10.0, 12U,
            "capitalized-provider-maturity"),
        entry("capitalized-interest-due",
            cf::ClaimLedgerEntryKind::InterestDue, 10.0, 12U,
            "capitalized-provider-maturity"),
        entry("capitalized-interest-transfer",
            cf::ClaimLedgerEntryKind::CapitalizedInterest, 10.0, 12U,
            "capitalized-provider-maturity"),
        entry("capitalized-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 20.0, 12U,
            "capitalized-provider-maturity"),
        entry("capitalized-guarantee-principal",
            cf::ClaimLedgerEntryKind::GuaranteePrincipalCash, 10.0, 12U,
            "capitalized-provider-maturity", 0U,
            "synthetic-provider-claim"),
        entry("capitalized-principal-writeoff",
            cf::ClaimLedgerEntryKind::PrincipalWriteoff, 10.0, 12U,
            "capitalized-provider-maturity"),
    };
    config.scenarios = {std::move(capitalized)};
    const cf::ClaimLedgerSummary capitalization =
        cf::evaluate_claim_ledger(config);
    const cf::ClaimLedgerProviderPeriodResult& provider_period =
        capitalization.scenarios[0U].decision_path.provider_claims[0U]
            .periods[12U];
    check(exact_value(
              provider_period.allocated_principal_shortfall_million, 20.0) &&
            exact_value(
                provider_period.allocated_interest_shortfall_million, 0.0) &&
            exact_value(provider_period.claim_generated_million, 10.0),
        "capitalized interest transfers categories once and cannot create both an interest and principal provider shortfall");

    const auto late_resolution_config = [](std::size_t lag,
                                            std::size_t cure_period,
                                            std::size_t payment_period) {
        cf::ClaimLedgerConfig late = hand_config();
        late.provider_claims[0U].settlement_lag_periods =
            cf::claim_ledger_known(static_cast<double>(lag));
        cf::ClaimLedgerScenario scenario;
        scenario.scenario_id = "late-underlying-resolution";
        scenario.physical_probability = cf::claim_ledger_known(1.0);
        scenario.probability_known_at_period = 0U;
        scenario.probability_source_record_id = "SYNTHETIC";
        scenario.cash_path_status =
            cf::ClaimLedgerCashPathStatus::CompleteResolved;
        scenario.cash_path_status_known_at_period = 0U;
        scenario.cash_path_status_source_record_id = "SYNTHETIC";
        scenario.entries = {
            entry("late-resolution-principal-due",
                cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 6U,
                "late-resolution-due"),
            entry("late-resolution-guarantee",
                cf::ClaimLedgerEntryKind::GuaranteePrincipalCash, 4.0,
                payment_period, "late-resolution-guarantee", 0U,
                "synthetic-provider-claim"),
            entry("late-resolution-recovery",
                cf::ClaimLedgerEntryKind::RecoveryPrincipalCash, 1.0,
                cure_period, "late-resolution-recovery"),
            entry("late-resolution-writeoff",
                cf::ClaimLedgerEntryKind::PrincipalWriteoff, 5.0, 12U,
                "late-resolution-writeoff"),
        };
        late.scenarios = {std::move(scenario)};
        return late;
    };
    const cf::ClaimLedgerSummary cure_during_lag =
        cf::evaluate_claim_ledger(late_resolution_config(2U, 7U, 8U));
    check(!cure_during_lag.readiness.provider_claim_ready &&
            !cure_during_lag.readiness.expected_cash_ready &&
            has_blocker(cure_during_lag.readiness.provider_claim_blockers,
                "late underlying cure"),
        "an underlying recovery during provider settlement lag is not silently netted against an already generated claim");
    const cf::ClaimLedgerSummary cure_after_payment =
        cf::evaluate_claim_ledger(late_resolution_config(1U, 8U, 7U));
    check(!cure_after_payment.readiness.provider_claim_ready &&
            !cure_after_payment.readiness.expected_cash_ready &&
            has_blocker(cure_after_payment.readiness.provider_claim_blockers,
                "late underlying cure"),
        "an underlying recovery after provider payment remains an explicit unsupported allocation case");

    config = hand_config();
    config.provider_claims[0U].covers_interest_due = true;
    config.provider_claims[0U].maximum_cash_million =
        cf::claim_ledger_known(100.0);
    config.provider_claims[0U].obligation_priority =
        cf::ClaimLedgerProviderAllocationPriority::PrincipalFirst;
    cf::ClaimLedgerScenario cross_category;
    cross_category.scenario_id = "cross-category-late-cure";
    cross_category.physical_probability = cf::claim_ledger_known(1.0);
    cross_category.probability_known_at_period = 0U;
    cross_category.probability_source_record_id = "SYNTHETIC";
    cross_category.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    cross_category.cash_path_status_known_at_period = 0U;
    cross_category.cash_path_status_source_record_id = "SYNTHETIC";
    cross_category.entries = {
        entry("cross-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 6U,
            "cross-category-default"),
        entry("cross-interest-accrual",
            cf::ClaimLedgerEntryKind::InterestAccrual, 5.0, 6U,
            "cross-category-default"),
        entry("cross-interest-due",
            cf::ClaimLedgerEntryKind::InterestDue, 5.0, 6U,
            "cross-category-default"),
        entry("cross-guarantee-principal",
            cf::ClaimLedgerEntryKind::GuaranteePrincipalCash, 7.5, 6U,
            "cross-category-default", 0U,
            "synthetic-provider-claim"),
        entry("cross-principal-writeoff",
            cf::ClaimLedgerEntryKind::PrincipalWriteoff, 2.5, 6U,
            "cross-category-default"),
        entry("cross-late-interest-cure",
            cf::ClaimLedgerEntryKind::InterestCash, 5.0, 7U,
            "cross-category-late-cure"),
    };
    config.scenarios = {std::move(cross_category)};
    const cf::ClaimLedgerSummary cross_category_cure =
        cf::evaluate_claim_ledger(config);
    check(!cross_category_cure.readiness.provider_claim_ready &&
            !cross_category_cure.readiness.expected_cash_ready &&
            has_blocker(
                cross_category_cure.readiness.provider_claim_blockers,
                "late underlying cure"),
        "a later cure in a shortfall category cannot bypass a combined provider claim allocated to another priority category");

    config = hand_config();
    config.provider_claims[0U].coverage_fraction =
        cf::claim_ledger_known(1.0);
    config.provider_claims[0U].deductible_million =
        cf::claim_ledger_known(10.0);
    config.provider_claims[0U].maximum_cash_million =
        cf::claim_ledger_known(100.0);
    cf::ClaimLedgerScenario preclaim_cure;
    preclaim_cure.scenario_id = "deductible-before-claim-cure";
    preclaim_cure.physical_probability = cf::claim_ledger_known(1.0);
    preclaim_cure.probability_known_at_period = 0U;
    preclaim_cure.probability_source_record_id = "SYNTHETIC";
    preclaim_cure.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    preclaim_cure.cash_path_status_known_at_period = 0U;
    preclaim_cure.cash_path_status_source_record_id = "SYNTHETIC";
    preclaim_cure.entries = {
        entry("deductible-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 10.0, 6U,
            "deductible-shortfall"),
        entry("deductible-late-borrower-cure",
            cf::ClaimLedgerEntryKind::PrincipalCash, 10.0, 7U,
            "deductible-late-cure"),
    };
    config.scenarios = {std::move(preclaim_cure)};
    const cf::ClaimLedgerSummary deductible_cure =
        cf::evaluate_claim_ledger(config);
    check(!deductible_cure.readiness.provider_claim_ready &&
            has_blocker(deductible_cure.readiness.provider_claim_blockers,
                "earlier allocated due-date shortfall"),
        "a late cure before deductible exhaustion is not left as stale lifetime shortfall for a later provider claim");
}

void test_issue_price_bridge() {
    cf::ClaimLedgerConfig config = hand_config();
    config.common_entries[6U].value = cf::claim_ledger_known(10.0);
    config.common_entries[7U].value = cf::claim_ledger_known(1.0);
    config.common_entries[8U].value = cf::claim_ledger_known(0.0);
    config.common_entries[9U].value = cf::claim_ledger_known(0.0);
    const cf::ClaimLedgerSummary discounted =
        cf::evaluate_claim_ledger(config);
    const cf::ClaimLedgerPeriodResult& closing =
        discounted.scenarios[0U].decision_path.periods[0U];
    check(discounted.scenarios[0U].decision_path.settlement_reconciled &&
            exact_value(closing.buyer_price_million, 9.0) &&
            exact_value(closing.funded_principal_million, 10.0) &&
            exact_value(closing.original_issue_discount_million, 1.0) &&
            exact_value(closing.original_issue_premium_million, 0.0) &&
            exact_value(closing.closing_principal_million, 10.0),
        "a discounted issuance distinguishes cash price from face principal without inventing proceeds");

    config = hand_config();
    config.common_entries.erase(config.common_entries.begin() + 7);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "an omitted issue-discount field is not inferred to be zero");

    config = hand_config();
    config.common_entries[7U].value = cf::claim_ledger_known(0.5);
    config.common_entries[8U].value = cf::claim_ledger_known(0.5);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "one issuance cannot be labeled both discounted and premium");
}

void test_cash_path_completeness() {
    cf::ClaimLedgerConfig config = hand_config();
    config.scenarios[0U].cash_path_status =
        cf::ClaimLedgerCashPathStatus::Incomplete;
    const cf::ClaimLedgerSummary declared_incomplete =
        cf::evaluate_claim_ledger(config);
    check(!declared_incomplete.readiness.expected_cash_ready &&
            has_blocker(
                declared_incomplete.readiness.expected_cash_blockers,
                "cash path is not explicitly complete"),
        "a mechanically closed path is not treated as complete without an explicit ex-ante assertion");

    config = hand_config();
    config.scenarios[0U].cash_path_status_known_at_period = 1U;
    const cf::ClaimLedgerSummary completeness_known_later =
        cf::evaluate_claim_ledger(config);
    check(!completeness_known_later.readiness.expected_cash_ready &&
            !completeness_known_later.scenarios[0U]
                 .complete_resolved_cash_path_at_decision,
        "a cash-path completion assertion learned after the decision cannot support an ex-ante return");

    config = hand_config();
    auto& performing_entries = config.scenarios[0U].entries;
    performing_entries.erase(std::remove_if(performing_entries.begin(),
                                performing_entries.end(),
                                [](const cf::ClaimLedgerEntry& row) {
                                    return row.kind ==
                                        cf::ClaimLedgerEntryKind::
                                            PrincipalCash;
                                }),
        performing_entries.end());
    const cf::ClaimLedgerSummary unresolved =
        cf::evaluate_claim_ledger(config);
    check(!unresolved.readiness.expected_cash_ready &&
            exact_value(unresolved.scenarios[0U]
                            .decision_path.terminal_total_exposure_million,
                10.0) &&
            has_blocker(unresolved.readiness.expected_cash_blockers,
                "terminal claim exposure is not exactly zero"),
        "an omitted terminal resolution remains exposure and blocks expected cash");
}

void test_conversion_adversaries() {
    cf::ClaimLedgerConfig config = hand_config();
    config.provider_claims.clear();
    config.conversion_unit_label = "synthetic-equity-unit";
    config.conversion_unit_basis =
        "one synthetic unit per recorded conversion unit";
    cf::ClaimLedgerScenario conversion;
    conversion.scenario_id = "zero-unit-conversion";
    conversion.physical_probability = cf::claim_ledger_known(1.0);
    conversion.probability_known_at_period = 0U;
    conversion.probability_source_record_id = "SYNTHETIC";
    conversion.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    conversion.cash_path_status_known_at_period = 0U;
    conversion.cash_path_status_source_record_id = "SYNTHETIC";
    conversion.entries = {
        entry("converted-principal",
            cf::ClaimLedgerEntryKind::ConversionPrincipalExtinguishment,
            10.0, 12U, "zero-unit-conversion-event"),
        entry("zero-conversion-units",
            cf::ClaimLedgerEntryKind::ConversionUnits, 0.0, 12U,
            "zero-unit-conversion-event"),
    };
    config.scenarios = {conversion};
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "positive extinguishment with zero conversion units is rejected");

    config.scenarios[0U].entries[1U].value =
        cf::claim_ledger_known(100.0);
    config.conversion_unit_basis = "not-applicable";
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "conversion units require both a declared unit label and basis");
}

void test_long_horizon_rate_preimage() {
    cf::ClaimLedgerConfig config = hand_config();
    config.horizon_period = cf::kClaimLedgerMaximumPeriods;
    config.provider_claims.clear();
    cf::ClaimLedgerScenario scenario;
    scenario.scenario_id = "century-maturity";
    scenario.physical_probability = cf::claim_ledger_known(1.0);
    scenario.probability_known_at_period = 0U;
    scenario.probability_source_record_id = "SYNTHETIC";
    scenario.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    scenario.cash_path_status_known_at_period = 0U;
    scenario.cash_path_status_source_record_id = "SYNTHETIC";
    scenario.entries = {
        entry("century-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 10.0,
            cf::kClaimLedgerMaximumPeriods, "century-maturity-event"),
        entry("century-principal-cash",
            cf::ClaimLedgerEntryKind::PrincipalCash, 10.0,
            cf::kClaimLedgerMaximumPeriods, "century-maturity-event"),
    };
    config.scenarios = {std::move(scenario)};
    const cf::ClaimLedgerSummary summary =
        cf::evaluate_claim_ledger(config);
    const double expected_rate =
        std::pow(10.0 / 8.9,
            static_cast<double>(config.periods_per_year) /
                static_cast<double>(config.horizon_period)) -
        1.0;
    check(summary.readiness.expected_cash_ready &&
            summary.readiness.rate_preimage_ready &&
            exact_value(summary.annual_effective_rate_preimage,
                expected_rate),
        "the sign-preserving solver finds a delayed-cash root without overflowing near minus one");
}

void test_negative_rate_and_discount() {
    cf::ClaimLedgerConfig config = hand_config();
    config.contractual_face_amount_million = cf::claim_ledger_known(5.0);
    config.annual_effective_discount_rate = cf::claim_ledger_known(-0.5);
    config.common_entries[0U].value = cf::claim_ledger_known(5.0);
    config.common_entries[1U].value = cf::claim_ledger_known(5.0);
    config.common_entries[2U].value = cf::claim_ledger_known(5.0);
    config.common_entries[3U].value = cf::claim_ledger_known(5.0);
    config.common_entries[4U].value = cf::claim_ledger_known(0.0);
    config.common_entries[5U].value = cf::claim_ledger_known(0.0);
    config.common_entries[6U].value = cf::claim_ledger_known(5.0);
    config.common_entries[7U].value = cf::claim_ledger_known(0.0);
    config.common_entries[8U].value = cf::claim_ledger_known(0.0);
    config.common_entries[9U].value = cf::claim_ledger_known(0.0);
    config.provider_claims.clear();
    cf::ClaimLedgerScenario scenario;
    scenario.scenario_id = "negative-return";
    scenario.physical_probability = cf::claim_ledger_known(1.0);
    scenario.probability_known_at_period = 0U;
    scenario.probability_source_record_id = "SYNTHETIC";
    scenario.cash_path_status =
        cf::ClaimLedgerCashPathStatus::CompleteResolved;
    scenario.cash_path_status_known_at_period = 0U;
    scenario.cash_path_status_source_record_id = "SYNTHETIC";
    scenario.entries = {
        entry("loss-principal-due",
            cf::ClaimLedgerEntryKind::PrincipalDue, 5.0, 12U,
            "loss-maturity"),
        entry("loss-principal-cash",
            cf::ClaimLedgerEntryKind::PrincipalCash, 5.0, 12U,
            "loss-maturity"),
    };
    config.scenarios = {std::move(scenario)};
    const cf::ClaimLedgerSummary summary =
        cf::evaluate_claim_ledger(config);
    check(summary.readiness.expected_cash_ready &&
            summary.readiness.npv_ready &&
            summary.readiness.rate_preimage_ready &&
            summary.expected_investor_cashflows_million.size() == 13U &&
            exact_value(summary.expected_investor_cashflows_million[0U],
                -10.0) &&
            exact_value(summary.expected_investor_cashflows_million[12U],
                5.0) &&
            exact_value(summary.annual_effective_rate_preimage, -0.5) &&
            exact_value(summary.expected_npv_million, 0.0),
        "a negative annual return remains valid and reproduces zero NPV at the same discount rate");
}

void test_resource_preflight_guardrails() {
    cf::ClaimLedgerConfig config = hand_config();
    config.horizon_period = cf::kClaimLedgerMaximumPeriods;
    config.provider_claims.clear();
    const cf::ClaimLedgerScenario scenario_seed = config.scenarios[0U];
    config.scenarios.assign(20U, scenario_seed);
    expect_invalid_argument_containing(
        [&config] { cf::validate_claim_ledger_config(config); },
        "scenario-period result product",
        "scenario-period products above the retained-result cap reject in preflight");

    config = hand_config();
    config.horizon_period = cf::kClaimLedgerMaximumPeriods;
    const cf::ClaimLedgerProviderClaim provider_seed = provider_claim();
    config.provider_claims.assign(4U, provider_seed);
    expect_invalid_argument_containing(
        [&config] { cf::validate_claim_ledger_config(config); },
        "provider-scenario-period result product",
        "provider-scenario-period products above the provider-result cap reject in preflight");

    config = hand_config();
    config.horizon_period = 0U;
    config.provider_claims.clear();
    const cf::ClaimLedgerScenario work_scenario_seed =
        config.scenarios[0U];
    config.scenarios.assign(100U, work_scenario_seed);
    const cf::ClaimLedgerEntry work_entry_seed = config.common_entries[0U];
    config.common_entries.assign(7'000U, work_entry_seed);
    expect_invalid_argument_containing(
        [&config] { cf::validate_claim_ledger_config(config); },
        "common-entry scenario work product",
        "common-entry times scenario work above the visit cap rejects before path allocation");
}

void test_structural_validation() {
    cf::ClaimLedgerConfig config = hand_config();
    config.common_entries.erase(config.common_entries.begin() + 1);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "a missing buyer-cost closing field is not assumed to be zero");

    config = hand_config();
    config.common_entries[3U].value = cf::claim_ledger_known(8.4);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "a settlement identity with no feasible reconciliation is rejected");

    config = hand_config();
    config.common_entries[0U].value =
        cf::claim_ledger_bounded(8.0, 10.0);
    config.common_entries[2U].value =
        cf::claim_ledger_bounded(9.0, 11.0);
    config.common_entries[3U].value =
        cf::claim_ledger_bounded(9.5, 10.5);
    config.common_entries[6U].value =
        cf::claim_ledger_bounded(8.0, 9.0);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "pairwise-overlapping funding ranges with no common settlement value are rejected");

    config = hand_config();
    config.common_entries[0U].value =
        cf::claim_ledger_bounded(0.0, 10.0);
    config.common_entries[1U].value = cf::claim_ledger_known(0.0);
    config.common_entries[2U].value = cf::claim_ledger_known(0.0);
    config.common_entries[3U].value = cf::claim_ledger_known(0.0);
    config.common_entries[4U].value = cf::claim_ledger_known(0.0);
    config.common_entries[5U].value = cf::claim_ledger_known(0.0);
    config.common_entries[6U].value = cf::claim_ledger_known(10.0);
    config.common_entries[7U].value = cf::claim_ledger_known(0.0);
    config.common_entries[8U].value = cf::claim_ledger_known(0.0);
    config.common_entries[9U].value = cf::claim_ledger_known(0.0);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "cash proceeds and the issue-price bridge must share one jointly feasible buyer price");

    config = hand_config();
    config.common_entries[0U].entry_id = "unsafe/id";
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "unsafe ledger identifiers are rejected");

    config = hand_config();
    config.common_entries[0U].value = cf::claim_ledger_known(-1.0);
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "negative unsigned cash entries are rejected");

    config = hand_config();
    config.common_entries[0U].value =
        cf::claim_ledger_not_applicable();
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "a monetary entry cannot use not-applicable as a disguised zero");

    config = hand_config();
    config.opening_principal_million =
        cf::claim_ledger_not_applicable();
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "an opening monetary balance cannot use not-applicable as a disguised zero");

    config = hand_config();
    config.common_entries[9U].event_group_id = "none";
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "a capitalized fee cannot sit outside its complete funding event");

    config = hand_config();
    cf::ClaimLedgerEntry competing = config.scenarios[0U].entries[3U];
    competing.entry_id = "competing-principal-cash-version";
    competing.value = cf::claim_ledger_known(9.5);
    config.scenarios[0U].entries.push_back(std::move(competing));
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "two versions of one economic fact at the same information cut are rejected");

    config = hand_config();
    competing = config.scenarios[0U].entries[3U];
    competing.entry_id = "reclassified-principal-cash-version";
    competing.kind = cf::ClaimLedgerEntryKind::PrincipalWriteoff;
    competing.known_at_period = 1U;
    config.scenarios[0U].entries.push_back(std::move(competing));
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "a later version cannot reclassify one economic fact from cash to writeoff");

    config = hand_config();
    config.scenarios[0U].entries[0U].period = 13U;
    expect_invalid_argument(
        [&config] { cf::validate_claim_ledger_config(config); },
        "cash events after the declared horizon are rejected");

    expect_invalid_argument(
        [] {
            (void)cf::claim_ledger_known(
                std::numeric_limits<double>::infinity());
        },
        "infinite financial values are rejected at construction");
}

} // namespace

int main() {
    run_test("hand reconciliation", test_hand_reconciliation);
    run_test("missing is not zero", test_missing_is_not_zero);
    run_test("backtest and covenant separation",
        test_backtest_and_covenant_separation);
    run_test("conversion is noncash", test_conversion_is_noncash);
    run_test("provider and balance adversaries",
        test_provider_and_balance_adversaries);
    run_test("issue-price bridge", test_issue_price_bridge);
    run_test("cash-path completeness", test_cash_path_completeness);
    run_test("conversion adversaries", test_conversion_adversaries);
    run_test("long-horizon rate preimage", test_long_horizon_rate_preimage);
    run_test("negative rate and discount", test_negative_rate_and_discount);
    run_test("resource preflight guardrails",
        test_resource_preflight_guardrails);
    run_test("structural validation", test_structural_validation);

    if (failures != 0) {
        std::cerr << failures << " claim-ledger test(s) failed\n";
        return 1;
    }
    std::cout << "all claim-ledger tests passed\n";
    return 0;
}
