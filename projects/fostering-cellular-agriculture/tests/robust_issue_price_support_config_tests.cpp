// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_issue_price_support_config.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_invalid_argument(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

void expect_runtime_error(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::runtime_error&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] std::string valid_config() {
    return
        "issue_price_support.model_version=0.1.0\n"
        "issue_price_support.label=Finite synthetic issue-price parser test\n"
        "issue_price_support.source_note=Unvalidated synthetic price support and hurdle inputs only\n"
        "issue_price_support.synthetic_inputs=true\n"
        "issue_price_support.market_claim_principal_is_fully_funded_at_issue=true\n"
        "issue_price_support.issue_support_and_price_fund_only_principal_and_issuer_costs=true\n"
        "issue_price_support.buyer_direct_cost_stays_outside_subscription_reserve=true\n"
        "issue_price_support.support_changes_no_claim_right_or_project_cash=true\n"
        "issue_price_support.physical_probability_polytope_is_unchanged=true\n"
        "issue_price_support.fair_value_or_market_price_is_estimated=false\n"
        "reference_price.id=synthetic-reference-price\n"
        "reference_price.status=internal_candidate\n"
        "reference_price.market_claim_id=market-priority\n"
        "reference_price.normalized_term_result_id=fixed-priority-cap-result\n"
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=false\n"
        "reference_price.gross_issue_price_million=6.5\n"
        "reference_price.claim_quantity_million=8\n"
        "reference_price.quantity_basis=full contractual market principal\n"
        "reference_price.price_basis=gross buyer cash at month zero\n"
        "reference_price.currency_label=DEMO\n"
        "reference_price.monetary_basis=constant synthetic monetary units at analysis close\n"
        "reference_price.execution_date=none\n"
        "reference_price.settlement_date=none\n"
        "reference_price.issuer_cost_million=0\n"
        "reference_price.buyer_direct_cost_million=0\n"
        "reference_price.side_rights_or_non_cash_consideration_present=false\n"
        "reference_price.side_rights_or_non_cash_consideration_note=none\n"
        "reference_price.source_reference=Unvalidated synthetic reference price\n"
        "reference_price.evidence_record_id=none\n"
        "reference_price.buyer_cash_payment_evidenced=false\n"
        "reference_price.settlement_evidenced=false\n"
        "reference_price.subscription_reserve_deposit_evidenced=false\n"
        "reference_price.issuer_cost_payment_evidenced=false\n"
        "reference_price.issue_use_evidence_record_id=none\n"
        "support.id=synthetic-issue-support\n"
        "support.status=synthetic_candidate\n"
        "support.maximum_support_million=1.5\n"
        "support.settled_support_million=0\n"
        "support.funding_evidenced=false\n"
        "support.settlement_evidenced=false\n"
        "support.as_of_date=2026-08-30\n"
        "support.source_reference=Unvalidated synthetic support capacity\n"
        "support.evidence_record_id=none\n"
        "support.source_note=No provider authority budget or cash is established\n"
        "support.support_is_non_repayable=true\n"
        "support.support_receives_no_repayment_participation_security_or_recovery_rights=true\n"
        "support.support_is_not_project_revenue=true\n"
        "support.support_does_not_pay_future_pool_costs_or_cover_project_losses=true\n"
        "hurdle_case.count=3\n"
        "hurdle_case.1.id=hurdle-20-percent\n"
        "hurdle_case.1.annual_effective_hurdle_rate=0.2\n"
        "hurdle_case.1.source_type=synthetic_sensitivity\n"
        "hurdle_case.1.reference_price_relation=independent\n"
        "hurdle_case.1.as_of_date=2026-08-30\n"
        "hurdle_case.1.source_reference=Unvalidated synthetic twenty-percent hurdle\n"
        "hurdle_case.1.evidence_record_id=none\n"
        "hurdle_case.1.source_note=Decision sensitivity not market calibration\n"
        "hurdle_case.2.id=hurdle-zero\n"
        "hurdle_case.2.annual_effective_hurdle_rate=0\n"
        "hurdle_case.2.source_type=synthetic_sensitivity\n"
        "hurdle_case.2.reference_price_relation=independent\n"
        "hurdle_case.2.as_of_date=2026-08-30\n"
        "hurdle_case.2.source_reference=Unvalidated synthetic zero-hurdle baseline\n"
        "hurdle_case.2.evidence_record_id=none\n"
        "hurdle_case.2.source_note=Undiscounted reconciliation baseline only\n"
        "hurdle_case.3.id=hurdle-10-percent\n"
        "hurdle_case.3.annual_effective_hurdle_rate=0.1\n"
        "hurdle_case.3.source_type=synthetic_sensitivity\n"
        "hurdle_case.3.reference_price_relation=independent\n"
        "hurdle_case.3.as_of_date=2026-08-30\n"
        "hurdle_case.3.source_reference=Unvalidated synthetic ten-percent hurdle\n"
        "hurdle_case.3.evidence_record_id=none\n"
        "hurdle_case.3.source_note=Decision sensitivity not market calibration\n";
}

[[nodiscard]] cf::RobustIssuePriceSupportConfig parse(std::string text) {
    std::istringstream input(std::move(text));
    return cf::parse_robust_issue_price_support_config(input);
}

void replace_once(std::string& text, std::string_view old_value,
    std::string_view new_value) {
    const std::size_t position = text.find(old_value);
    if (position == std::string::npos) {
        throw std::logic_error("test replacement source is missing");
    }
    text.replace(position, old_value.size(), new_value);
}

void test_parse_canonical_round_trip() {
    const cf::RobustIssuePriceSupportConfig config = parse(valid_config());
    check(config.reference_price.status ==
                cf::RobustIssuePriceReferenceStatus::InternalCandidate &&
            config.reference_price.gross_issue_price_million == 6.5 &&
            !config.reference_price
                 .secondary_price_normalized_to_full_month_zero_claim &&
            config.support.maximum_support_million == 1.5 &&
            config.support.settled_support_million == 0.0,
        "parser preserves the single reference-price and support records");
    check(config.hurdle_cases.size() == 3U &&
            config.hurdle_cases[0].annual_effective_hurdle_rate == 0.0 &&
            config.hurdle_cases[1].annual_effective_hurdle_rate == 0.1 &&
            config.hurdle_cases[2].annual_effective_hurdle_rate == 0.2 &&
            config.hurdle_cases[0].reference_price_relation ==
                cf::RobustIssuePriceHurdleReferenceRelation::Independent,
        "hurdle cases retain provenance and canonical rate ordering");

    std::ostringstream first;
    cf::print_normalized_robust_issue_price_support_config(first, config);
    const cf::RobustIssuePriceSupportConfig reparsed = parse(first.str());
    std::ostringstream second;
    cf::print_normalized_robust_issue_price_support_config(second, reparsed);
    check(first.str() == second.str(),
        "normalized issue-price config print-load-print is byte stable");
    check(first.str().find(
              "support.settled_support_million=0\n") != std::string::npos &&
            first.str().find(
              "reference_price.buyer_cash_payment_evidenced=false\n") !=
                std::string::npos &&
            first.str().find(
              "reference_price.secondary_price_normalized_to_full_month_zero_claim=false\n") !=
                std::string::npos &&
            first.str().find(
              "reference_price.subscription_reserve_deposit_evidenced=false\n") !=
                std::string::npos &&
            first.str().find(
              "reference_price.issue_use_evidence_record_id=none\n") !=
                std::string::npos &&
            first.str().find(
              "hurdle_case.1.reference_price_relation=independent\n") !=
                std::string::npos &&
            first.str().find(
              "hurdle_case.1.id=hurdle-zero\n") != std::string::npos,
        "normalized text emits settlement evidence and independent hurdles explicitly");

    const cf::RobustIssuePriceSupportConfig bom =
        parse(std::string("\xEF\xBB\xBF", 3U) + valid_config());
    check(bom.model_version == "0.1.0",
        "one leading UTF-8 BOM is accepted");

    std::string crlf_text = valid_config();
    std::size_t newline = 0U;
    while ((newline = crlf_text.find('\n', newline)) != std::string::npos) {
        crlf_text.replace(newline, 1U, "\r\n");
        newline += 2U;
    }
    const cf::RobustIssuePriceSupportConfig crlf =
        parse(std::move(crlf_text));
    check(crlf.hurdle_cases.size() == config.hurdle_cases.size() &&
            crlf.hurdle_cases[0].case_id ==
                config.hurdle_cases[0].case_id &&
            crlf.hurdle_cases[1].annual_effective_hurdle_rate ==
                config.hurdle_cases[1].annual_effective_hurdle_rate &&
            crlf.hurdle_cases[2].reference_price_relation ==
                config.hurdle_cases[2].reference_price_relation,
        "bounded incremental line reading preserves CRLF semantics");
}

void test_closed_schema_and_numeric_guardrails() {
    expect_invalid_argument(
        [] { (void)parse(valid_config() + "issue_price_support.unknown=true\n"); },
        "unknown fixed keys are rejected");
    expect_invalid_argument(
        [] { (void)parse(valid_config() + "hurdle_case.1.unknown=value\n"); },
        "unknown indexed fields are rejected before insertion");
    expect_invalid_argument(
        [] {
            (void)parse(valid_config() +
                "support.required_issue_support_million=1.5\n");
        },
        "modeled required support is an output identity and cannot overwrite actual settled support evidence");

    std::string duplicate = valid_config();
    duplicate += "reference_price.id=duplicate\n";
    expect_invalid_argument([&] { (void)parse(duplicate); },
        "duplicate keys are rejected");

    std::string missing = valid_config();
    replace_once(missing, "support.settled_support_million=0\n", "");
    expect_invalid_argument([&] { (void)parse(missing); },
        "every support evidence field is mandatory");

    std::string missing_secondary_normalization = valid_config();
    replace_once(missing_secondary_normalization,
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=false\n",
        "");
    expect_invalid_argument(
        [&] { (void)parse(missing_secondary_normalization); },
        "secondary-price numerical eligibility must be explicit");

    std::string missing_use_evidence = valid_config();
    replace_once(missing_use_evidence,
        "reference_price.issue_use_evidence_record_id=none\n", "");
    expect_invalid_argument([&] { (void)parse(missing_use_evidence); },
        "issue-use evidence fields are mandatory even when explicitly absent");

    std::string zero_count = valid_config();
    replace_once(zero_count, "hurdle_case.count=3", "hurdle_case.count=0");
    expect_invalid_argument([&] { (void)parse(zero_count); },
        "zero hurdle counts are rejected before indexed expansion");

    std::string huge_count = valid_config();
    replace_once(huge_count, "hurdle_case.count=3", "hurdle_case.count=257");
    expect_invalid_argument([&] { (void)parse(huge_count); },
        "hurdle counts above 256 are rejected");

    std::string extra_index = valid_config();
    extra_index +=
        "hurdle_case.4.id=extra\n"
        "hurdle_case.4.annual_effective_hurdle_rate=0.3\n"
        "hurdle_case.4.source_type=synthetic_sensitivity\n"
        "hurdle_case.4.reference_price_relation=independent\n"
        "hurdle_case.4.as_of_date=2026-08-30\n"
        "hurdle_case.4.source_reference=Extra bounded case\n"
        "hurdle_case.4.evidence_record_id=none\n"
        "hurdle_case.4.source_note=Extra bounded case\n";
    expect_invalid_argument([&] { (void)parse(extra_index); },
        "indexed records beyond the declared count are rejected");

    for (const std::string_view key : {
             "reference_price.gross_issue_price_million",
             "reference_price.claim_quantity_million",
             "reference_price.issuer_cost_million",
             "reference_price.buyer_direct_cost_million",
             "support.maximum_support_million",
             "support.settled_support_million",
             "hurdle_case.3.annual_effective_hurdle_rate"}) {
        std::string nonfinite = valid_config();
        const std::string prefix = std::string(key) + '=';
        const std::size_t start = nonfinite.find(prefix);
        const std::size_t end = nonfinite.find('\n', start);
        nonfinite.replace(start, end - start, prefix + "nan");
        expect_invalid_argument([&] { (void)parse(nonfinite); },
            "non-finite issuance and hurdle inputs are rejected");
    }

    std::string negative_price = valid_config();
    replace_once(negative_price,
        "reference_price.gross_issue_price_million=6.5",
        "reference_price.gross_issue_price_million=-1");
    expect_invalid_argument([&] { (void)parse(negative_price); },
        "negative reference prices are rejected");

    std::string excessive_money = valid_config();
    replace_once(excessive_money,
        "reference_price.gross_issue_price_million=6.5",
        "reference_price.gross_issue_price_million=1000000001");
    expect_invalid_argument([&] { (void)parse(excessive_money); },
        "money inputs above the conservative absolute guardrail are rejected");

    std::string negative_zero = valid_config();
    replace_once(negative_zero,
        "support.maximum_support_million=1.5",
        "support.maximum_support_million=-0");
    expect_invalid_argument([&] { (void)parse(negative_zero); },
        "negative-zero amounts are rejected");

    std::string zero_quantity = valid_config();
    replace_once(zero_quantity,
        "reference_price.claim_quantity_million=8",
        "reference_price.claim_quantity_million=0");
    expect_invalid_argument([&] { (void)parse(zero_quantity); },
        "the one reference record must identify positive claim quantity");

    std::string settled_above_capacity = valid_config();
    replace_once(settled_above_capacity,
        "support.settled_support_million=0",
        "support.settled_support_million=2");
    expect_invalid_argument([&] { (void)parse(settled_above_capacity); },
        "settled support cannot exceed maximum capacity");

    std::string negative_settled_support = valid_config();
    replace_once(negative_settled_support,
        "support.settled_support_million=0",
        "support.settled_support_million=-0.1");
    expect_invalid_argument([&] { (void)parse(negative_settled_support); },
        "actual settled support cannot be negative");

    std::string lower_rate = valid_config();
    replace_once(lower_rate,
        "hurdle_case.3.annual_effective_hurdle_rate=0.1",
        "hurdle_case.3.annual_effective_hurdle_rate=-1");
    expect_invalid_argument([&] { (void)parse(lower_rate); },
        "negative hurdles are rejected");

    std::string upper_rate = valid_config();
    replace_once(upper_rate,
        "hurdle_case.3.annual_effective_hurdle_rate=0.1",
        "hurdle_case.3.annual_effective_hurdle_rate=10.0001");
    expect_invalid_argument([&] { (void)parse(upper_rate); },
        "hurdles above ten are rejected");

    std::string negative_rate = valid_config();
    replace_once(negative_rate,
        "hurdle_case.3.annual_effective_hurdle_rate=0.1",
        "hurdle_case.3.annual_effective_hurdle_rate=-0.5");
    expect_invalid_argument([&] { (void)parse(negative_rate); },
        "all finite negative hurdle rates fail closed");

    std::string negative_zero_rate = valid_config();
    replace_once(negative_zero_rate,
        "hurdle_case.2.annual_effective_hurdle_rate=0",
        "hurdle_case.2.annual_effective_hurdle_rate=-0");
    expect_invalid_argument([&] { (void)parse(negative_zero_rate); },
        "negative zero cannot satisfy the literal-zero baseline");

    std::string missing_zero = valid_config();
    replace_once(missing_zero,
        "hurdle_case.2.annual_effective_hurdle_rate=0",
        "hurdle_case.2.annual_effective_hurdle_rate=0.01");
    expect_invalid_argument([&] { (void)parse(missing_zero); },
        "a literal-zero hurdle case is required");

    std::string duplicate_id = valid_config();
    replace_once(duplicate_id,
        "hurdle_case.3.id=hurdle-10-percent",
        "hurdle_case.3.id=hurdle-zero");
    expect_invalid_argument([&] { (void)parse(duplicate_id); },
        "hurdle case ids are unique");

    std::string invalid_relation = valid_config();
    replace_once(invalid_relation,
        "hurdle_case.3.reference_price_relation=independent",
        "hurdle_case.3.reference_price_relation=derived");
    expect_invalid_argument([&] { (void)parse(invalid_relation); },
        "unknown hurdle/reference-price relations are rejected");

    std::string unsupported = valid_config();
    replace_once(unsupported,
        "issue_price_support.model_version=0.1.0",
        "issue_price_support.model_version=9.9.9");
    expect_invalid_argument([&] { (void)parse(unsupported); },
        "unsupported model versions fail closed");

    std::string false_assertion = valid_config();
    replace_once(false_assertion,
        "issue_price_support.support_changes_no_claim_right_or_project_cash=true",
        "issue_price_support.support_changes_no_claim_right_or_project_cash=false");
    expect_invalid_argument([&] { (void)parse(false_assertion); },
        "required no-rights support assertions cannot be disabled");

    std::string fair_value_claim = valid_config();
    replace_once(fair_value_claim,
        "issue_price_support.fair_value_or_market_price_is_estimated=false",
        "issue_price_support.fair_value_or_market_price_is_estimated=true");
    expect_invalid_argument([&] { (void)parse(fair_value_claim); },
        "the configuration cannot claim fair value or a market price");
}

void make_non_synthetic(std::string& text) {
    replace_once(text,
        "issue_price_support.synthetic_inputs=true",
        "issue_price_support.synthetic_inputs=false");
}

void test_evidence_and_settlement_semantics() {
    std::string synthetic_settled = valid_config();
    replace_once(synthetic_settled,
        "reference_price.status=internal_candidate",
        "reference_price.status=settled_primary");
    expect_invalid_argument([&] { (void)parse(synthetic_settled); },
        "synthetic inputs cannot masquerade as settled primary evidence");

    std::string synthetic_use_evidence = valid_config();
    replace_once(synthetic_use_evidence,
        "reference_price.subscription_reserve_deposit_evidenced=false",
        "reference_price.subscription_reserve_deposit_evidenced=true");
    expect_invalid_argument([&] { (void)parse(synthetic_use_evidence); },
        "synthetic internal candidates cannot masquerade as evidenced issue uses");

    std::string synthetic_market_hurdle = valid_config();
    replace_once(synthetic_market_hurdle,
        "hurdle_case.3.source_type=synthetic_sensitivity",
        "hurdle_case.3.source_type=same_claim_market_observation");
    expect_invalid_argument([&] { (void)parse(synthetic_market_hurdle); },
        "synthetic hurdle cases cannot masquerade as market observations");

    std::string nonbinding_support = valid_config();
    make_non_synthetic(nonbinding_support);
    replace_once(nonbinding_support,
        "support.status=synthetic_candidate",
        "support.status=nonbinding_indication");
    replace_once(nonbinding_support,
        "support.source_reference=Unvalidated synthetic support capacity",
        "support.source_reference=Nonbinding provider indication");
    replace_once(nonbinding_support,
        "support.evidence_record_id=none",
        "support.evidence_record_id=SUPPORT-INDICATION-001");
    const cf::RobustIssuePriceSupportConfig nonbinding =
        parse(nonbinding_support);
    check(nonbinding.support.status ==
                cf::RobustIssuePriceSupportCapacityStatus::
                    NonbindingIndication &&
            !nonbinding.support.funding_evidenced &&
            !nonbinding.support.settlement_evidenced &&
            nonbinding.support.settled_support_million == 0.0,
        "nonbinding support is only a conditional capacity input, not funding or settlement evidence");

    std::string nonbinding_claiming_funding = nonbinding_support;
    replace_once(nonbinding_claiming_funding,
        "support.funding_evidenced=false",
        "support.funding_evidenced=true");
    expect_invalid_argument(
        [&] { (void)parse(nonbinding_claiming_funding); },
        "nonbinding capacity cannot claim capital readiness");

    std::string unevidenced_nonbinding_price = valid_config();
    make_non_synthetic(unevidenced_nonbinding_price);
    replace_once(unevidenced_nonbinding_price,
        "reference_price.status=internal_candidate",
        "reference_price.status=nonbinding_indication");
    expect_invalid_argument(
        [&] { (void)parse(unevidenced_nonbinding_price); },
        "a real nonbinding price indication requires source and evidence records");

    std::string nonbinding_price = unevidenced_nonbinding_price;
    replace_once(nonbinding_price,
        "reference_price.source_reference=Unvalidated synthetic reference price",
        "reference_price.source_reference=Nonbinding subscription indication");
    replace_once(nonbinding_price,
        "reference_price.evidence_record_id=none",
        "reference_price.evidence_record_id=PRICE-INDICATION-001");
    check(parse(nonbinding_price).reference_price.status ==
            cf::RobustIssuePriceReferenceStatus::NonbindingIndication,
        "an evidenced nonbinding price remains distinct from execution and settlement");

    std::string binding_subscription = nonbinding_price;
    replace_once(binding_subscription,
        "reference_price.status=nonbinding_indication",
        "reference_price.status=binding_unsettled_subscription");
    check(parse(binding_subscription).reference_price.status ==
            cf::RobustIssuePriceReferenceStatus::
                BindingUnsettledSubscription,
        "binding-unsettled subscription evidence does not claim execution or settlement");

    std::string dated_nonbinding = nonbinding_price;
    replace_once(dated_nonbinding,
        "reference_price.execution_date=none",
        "reference_price.execution_date=2026-08-28");
    expect_invalid_argument([&] { (void)parse(dated_nonbinding); },
        "nonbinding and binding-unsettled statuses cannot claim transaction dates");

    std::string executed = valid_config();
    make_non_synthetic(executed);
    replace_once(executed,
        "reference_price.status=internal_candidate",
        "reference_price.status=executed_unsettled_primary");
    replace_once(executed,
        "reference_price.execution_date=none",
        "reference_price.execution_date=2026-08-28");
    replace_once(executed,
        "reference_price.source_reference=Unvalidated synthetic reference price",
        "reference_price.source_reference=Executed subscription record");
    replace_once(executed,
        "reference_price.evidence_record_id=none",
        "reference_price.evidence_record_id=PRICE-EXEC-001");
    check(parse(executed).reference_price.status ==
            cf::RobustIssuePriceReferenceStatus::ExecutedUnsettledPrimary,
        "an evidenced executed-unsettled primary price remains unsettled");

    std::string executed_without_normalized_result = executed;
    replace_once(executed_without_normalized_result,
        "reference_price.normalized_term_result_id=fixed-priority-cap-result",
        "reference_price.normalized_term_result_id=unnamed-result");
    expect_invalid_argument(
        [&] { (void)parse(executed_without_normalized_result); },
        "executed records require a non-placeholder normalized term result");

    std::string settled_primary = executed;
    replace_once(settled_primary,
        "reference_price.status=executed_unsettled_primary",
        "reference_price.status=settled_primary");
    replace_once(settled_primary,
        "reference_price.settlement_date=none",
        "reference_price.settlement_date=2026-08-29");
    replace_once(settled_primary,
        "reference_price.buyer_cash_payment_evidenced=false",
        "reference_price.buyer_cash_payment_evidenced=true");
    replace_once(settled_primary,
        "reference_price.settlement_evidenced=false",
        "reference_price.settlement_evidenced=true");
    replace_once(settled_primary,
        "reference_price.subscription_reserve_deposit_evidenced=false",
        "reference_price.subscription_reserve_deposit_evidenced=true");
    replace_once(settled_primary,
        "reference_price.issue_use_evidence_record_id=none",
        "reference_price.issue_use_evidence_record_id=ISSUE-USES-001");
    replace_once(settled_primary,
        "support.status=synthetic_candidate",
        "support.status=settled_to_issue");
    replace_once(settled_primary,
        "support.settled_support_million=0",
        "support.settled_support_million=1.5");
    replace_once(settled_primary,
        "support.funding_evidenced=false",
        "support.funding_evidenced=true");
    replace_once(settled_primary,
        "support.settlement_evidenced=false",
        "support.settlement_evidenced=true");
    replace_once(settled_primary,
        "support.source_reference=Unvalidated synthetic support capacity",
        "support.source_reference=Settled support transfer record");
    replace_once(settled_primary,
        "support.evidence_record_id=none",
        "support.evidence_record_id=SUPPORT-SETTLED-001");
    const cf::RobustIssuePriceSupportConfig settled = parse(settled_primary);
    check(settled.reference_price.buyer_cash_payment_evidenced &&
            settled.reference_price
                .subscription_reserve_deposit_evidenced &&
            !settled.reference_price.issuer_cost_payment_evidenced &&
            settled.support.settled_support_million == 1.5 &&
            settled.support.settlement_evidenced &&
            settled.hurdle_cases[1].source_type ==
                cf::RobustIssuePriceHurdleSourceType::SyntheticSensitivity,
        "settled transaction evidence and actual support remain separate from hurdle provenance");

    std::string settled_without_use_evidence = settled_primary;
    replace_once(settled_without_use_evidence,
        "reference_price.subscription_reserve_deposit_evidenced=true",
        "reference_price.subscription_reserve_deposit_evidenced=false");
    check(!parse(settled_without_use_evidence).reference_price
              .subscription_reserve_deposit_evidenced,
        "settled primary source cash remains representable but distinct from completed funding without use-side evidence");

    std::string zero_cost_claiming_payment = settled_primary;
    replace_once(zero_cost_claiming_payment,
        "reference_price.issuer_cost_payment_evidenced=false",
        "reference_price.issuer_cost_payment_evidenced=true");
    expect_invalid_argument(
        [&] { (void)parse(zero_cost_claiming_payment); },
        "zero issuer cost cannot claim a payment evidence record");

    std::string settled_with_issuer_cost_evidence = settled_primary;
    replace_once(settled_with_issuer_cost_evidence,
        "reference_price.gross_issue_price_million=6.5",
        "reference_price.gross_issue_price_million=7");
    replace_once(settled_with_issuer_cost_evidence,
        "reference_price.issuer_cost_million=0",
        "reference_price.issuer_cost_million=0.5");
    replace_once(settled_with_issuer_cost_evidence,
        "reference_price.issuer_cost_payment_evidenced=false",
        "reference_price.issuer_cost_payment_evidenced=true");
    const cf::RobustIssuePriceSupportConfig cost_evidenced =
        parse(settled_with_issuer_cost_evidence);
    check(cost_evidenced.reference_price.issuer_cost_million == 0.5 &&
            cost_evidenced.reference_price
                .issuer_cost_payment_evidenced &&
            cost_evidenced.reference_price
                .subscription_reserve_deposit_evidenced &&
            cost_evidenced.reference_price.issue_use_evidence_record_id ==
                "ISSUE-USES-001" &&
            cost_evidenced.reference_price.gross_issue_price_million +
                    cost_evidenced.support.settled_support_million ==
                cost_evidenced.reference_price.claim_quantity_million +
                    cost_evidenced.reference_price.issuer_cost_million,
        "positive issuer cost can carry explicit payment evidence when settled sources and uses reconcile");

    std::string settled_secondary = executed;
    replace_once(settled_secondary,
        "reference_price.status=executed_unsettled_primary",
        "reference_price.status=settled_secondary");
    replace_once(settled_secondary,
        "reference_price.settlement_date=none",
        "reference_price.settlement_date=2026-08-29");
    replace_once(settled_secondary,
        "reference_price.buyer_cash_payment_evidenced=false",
        "reference_price.buyer_cash_payment_evidenced=true");
    replace_once(settled_secondary,
        "reference_price.settlement_evidenced=false",
        "reference_price.settlement_evidenced=true");
    const cf::RobustIssuePriceSupportConfig secondary_evidence_only =
        parse(settled_secondary);
    check(secondary_evidence_only.reference_price.status ==
                cf::RobustIssuePriceReferenceStatus::SettledSecondary &&
            !secondary_evidence_only.reference_price
                 .secondary_price_normalized_to_full_month_zero_claim,
        "settled secondary buyer-seller cash remains evidence-only without full-claim month-zero normalization");

    std::string normalized_secondary = settled_secondary;
    replace_once(normalized_secondary,
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=false",
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=true");
    check(parse(normalized_secondary).reference_price
              .secondary_price_normalized_to_full_month_zero_claim,
        "settled secondary numerical use requires an explicit full-claim month-zero normalization assertion");

    std::string primary_claiming_secondary_normalization = settled_primary;
    replace_once(primary_claiming_secondary_normalization,
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=false",
        "reference_price.secondary_price_normalized_to_full_month_zero_claim=true");
    expect_invalid_argument(
        [&] { (void)parse(primary_claiming_secondary_normalization); },
        "secondary-price normalization cannot be asserted for a primary record");

    std::string normalized_secondary_without_result = normalized_secondary;
    replace_once(normalized_secondary_without_result,
        "reference_price.normalized_term_result_id=fixed-priority-cap-result",
        "reference_price.normalized_term_result_id=unnamed-result");
    expect_invalid_argument(
        [&] { (void)parse(normalized_secondary_without_result); },
        "numerical secondary use requires a non-placeholder normalized term result");

    std::string settlement_without_cash = settled_primary;
    replace_once(settlement_without_cash,
        "reference_price.buyer_cash_payment_evidenced=true",
        "reference_price.buyer_cash_payment_evidenced=false");
    expect_invalid_argument([&] { (void)parse(settlement_without_cash); },
        "settlement evidence cannot exist without evidenced buyer cash");

    std::string escrow_without_funding = valid_config();
    make_non_synthetic(escrow_without_funding);
    replace_once(escrow_without_funding,
        "support.status=synthetic_candidate",
        "support.status=funded_or_escrowed");
    replace_once(escrow_without_funding,
        "support.source_reference=Unvalidated synthetic support capacity",
        "support.source_reference=Escrow control record");
    replace_once(escrow_without_funding,
        "support.evidence_record_id=none",
        "support.evidence_record_id=SUPPORT-ESCROW-001");
    expect_invalid_argument([&] { (void)parse(escrow_without_funding); },
        "funded-or-escrowed status requires funding evidence");

    std::string funded_escrow = escrow_without_funding;
    replace_once(funded_escrow,
        "support.funding_evidenced=false",
        "support.funding_evidenced=true");
    check(parse(funded_escrow).support.status ==
            cf::RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed,
        "funded-or-escrowed support remains distinct from issue settlement");

    std::string implied_hurdle = valid_config();
    make_non_synthetic(implied_hurdle);
    replace_once(implied_hurdle,
        "hurdle_case.3.reference_price_relation=independent",
        "hurdle_case.3.reference_price_relation=model_implied_from_reference_price");
    check(parse(implied_hurdle).hurdle_cases[1].reference_price_relation ==
            cf::RobustIssuePriceHurdleReferenceRelation::
                ModelImpliedFromReferencePrice,
        "model-implied hurdles remain explicitly non-independent");

    std::string observed_hurdle = valid_config();
    make_non_synthetic(observed_hurdle);
    replace_once(observed_hurdle,
        "hurdle_case.3.source_type=synthetic_sensitivity",
        "hurdle_case.3.source_type=comparable_market_observation");
    replace_once(observed_hurdle,
        "hurdle_case.3.source_reference=Unvalidated synthetic ten-percent hurdle",
        "hurdle_case.3.source_reference=Comparable transaction record");
    replace_once(observed_hurdle,
        "hurdle_case.3.evidence_record_id=none",
        "hurdle_case.3.evidence_record_id=HURDLE-COMP-001");
    check(parse(observed_hurdle).hurdle_cases[1].source_type ==
            cf::RobustIssuePriceHurdleSourceType::ComparableMarketObservation,
        "market-observation hurdle provenance requires controlled evidence");

    std::string unevidenced_investor_target = valid_config();
    make_non_synthetic(unevidenced_investor_target);
    replace_once(unevidenced_investor_target,
        "hurdle_case.3.source_type=synthetic_sensitivity",
        "hurdle_case.3.source_type=investor_target");
    expect_invalid_argument(
        [&] { (void)parse(unevidenced_investor_target); },
        "a real investor-target hurdle requires its own source and evidence record");

    std::string side_rights = valid_config();
    replace_once(side_rights,
        "reference_price.side_rights_or_non_cash_consideration_present=false",
        "reference_price.side_rights_or_non_cash_consideration_present=true");
    replace_once(side_rights,
        "reference_price.side_rights_or_non_cash_consideration_note=none",
        "reference_price.side_rights_or_non_cash_consideration_note=warrant");
    expect_invalid_argument([&] { (void)parse(side_rights); },
        "v0.1 rejects a price contaminated by side rights");

    std::string impossible_date = valid_config();
    replace_once(impossible_date,
        "hurdle_case.3.as_of_date=2026-08-30",
        "hurdle_case.3.as_of_date=2026-02-30");
    expect_invalid_argument([&] { (void)parse(impossible_date); },
        "provenance dates must be real calendar dates");

    std::string reversed_dates = settled_primary;
    replace_once(reversed_dates,
        "reference_price.settlement_date=2026-08-29",
        "reference_price.settlement_date=2026-08-27");
    expect_invalid_argument([&] { (void)parse(reversed_dates); },
        "settlement cannot precede execution");
}

void test_text_bom_and_stream_guardrails() {
    std::string unsafe_id = valid_config();
    replace_once(unsafe_id,
        "reference_price.market_claim_id=market-priority",
        "reference_price.market_claim_id=../../market");
    expect_invalid_argument([&] { (void)parse(unsafe_id); },
        "unsafe claim identifiers are rejected");

    std::string padded_text = valid_config();
    replace_once(padded_text,
        "issue_price_support.label=Finite synthetic issue-price parser test",
        "issue_price_support.label= Finite synthetic issue-price parser test");
    expect_invalid_argument([&] { (void)parse(padded_text); },
        "text surrounding whitespace is rejected");

    std::string embedded_bom = valid_config();
    embedded_bom += std::string("# second ") +
        std::string("\xEF\xBB\xBF", 3U) + " marker\n";
    expect_invalid_argument([&] { (void)parse(embedded_bom); },
        "embedded BOMs are rejected even on comments");

    std::string long_line(4'097U, 'x');
    expect_invalid_argument([&] { (void)parse(long_line); },
        "individual lines are size bounded");

    std::istringstream failed_input(valid_config());
    failed_input.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            (void)cf::parse_robust_issue_price_support_config(failed_input);
        },
        "non-EOF stream failures are not mistaken for empty configs");

    cf::RobustIssuePriceSupportConfig injected = parse(valid_config());
    injected.source_note =
        "line one\nissue_price_support.synthetic_inputs=false";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_issue_price_support_config(
                output, injected);
        },
        "normalized output rejects control-character schema injection");

    injected = parse(valid_config());
    injected.reference_price.source_reference = std::string("embedded ") +
        std::string("\xEF\xBB\xBF", 3U) + " BOM";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_issue_price_support_config(
                output, injected);
        },
        "normalized output rejects embedded BOM text");

    injected = parse(valid_config());
    injected.reference_price.gross_issue_price_million =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_issue_price_support_config(
                output, injected);
        },
        "normalized output validates in-memory numeric terms");

    const cf::RobustIssuePriceSupportConfig config = parse(valid_config());
    std::ostringstream failed_output;
    failed_output.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            cf::print_normalized_robust_issue_price_support_config(
                failed_output, config);
        },
        "normalized writer reports output stream failures");
}

void test_bounded_adversarial_stream_consumption() {
    std::istringstream delimiter_free(
        std::string(2U * 1024U * 1024U, 'x'));
    bool long_line_rejected = false;
    try {
        (void)cf::parse_robust_issue_price_support_config(delimiter_free);
    } catch (const std::invalid_argument& error) {
        long_line_rejected =
            std::string_view(error.what()).find("4096-byte") !=
            std::string_view::npos;
    } catch (...) {
        // Checked below.
    }
    const std::streampos long_line_position = delimiter_free.tellg();
    check(long_line_rejected && long_line_position >= std::streampos{0} &&
            long_line_position <= std::streampos{4'097},
        "delimiter-free input is rejected after bounded incremental consumption");

    std::string unknown_flood;
    unknown_flood.reserve(512U * 1024U);
    for (std::size_t index = 0U; index < 20'000U; ++index) {
        unknown_flood +=
            "unknown_issue_key_" + std::to_string(index) + "=value\n";
    }
    const std::size_t first_line_bytes = unknown_flood.find('\n') + 1U;
    std::istringstream unknown_input(std::move(unknown_flood));
    bool unknown_rejected_at_first_line = false;
    try {
        (void)cf::parse_robust_issue_price_support_config(unknown_input);
    } catch (const std::invalid_argument& error) {
        const std::string_view message{error.what()};
        unknown_rejected_at_first_line =
            message.find("configuration line 1: unknown key") !=
            std::string_view::npos;
    } catch (...) {
        // Checked below.
    }
    const std::streampos unknown_position = unknown_input.tellg();
    check(unknown_rejected_at_first_line &&
            unknown_position >= std::streampos{0} &&
            unknown_position <=
                static_cast<std::streamoff>(first_line_bytes),
        "unknown-key floods are rejected before later records are read or inserted");

    std::string indexed_flood;
    indexed_flood.reserve(256U * 1024U);
    for (std::size_t index = 0U; index < 10'000U; ++index) {
        indexed_flood += "hurdle_case.257.id=case-" +
            std::to_string(index) + "\n";
    }
    std::istringstream indexed_input(std::move(indexed_flood));
    bool indexed_rejected_at_first_line = false;
    try {
        (void)cf::parse_robust_issue_price_support_config(indexed_input);
    } catch (const std::invalid_argument& error) {
        indexed_rejected_at_first_line =
            std::string_view(error.what()).find("configuration line 1") !=
            std::string_view::npos;
    } catch (...) {
        // Checked below.
    }
    check(indexed_rejected_at_first_line,
        "out-of-bound indexed-key floods fail on the first record");
}

void test_file_size_and_load_guardrails() {
    const std::filesystem::path oversized =
        std::filesystem::current_path() /
        "robust-issue-price-support-config-oversized.tmp";
    {
        std::ofstream output(oversized, std::ios::binary | std::ios::trunc);
        check(static_cast<bool>(output),
            "oversized-loader fixture opens for writing");
    }
    std::filesystem::resize_file(oversized,
        static_cast<std::uintmax_t>(16U * 1024U * 1024U) + 1U);
    expect_invalid_argument(
        [&] {
            (void)cf::load_robust_issue_price_support_config(oversized);
        },
        "loader rejects files above 16 MiB before parsing");
    std::error_code remove_error;
    (void)std::filesystem::remove(oversized, remove_error);

    const std::filesystem::path missing =
        std::filesystem::current_path() /
        "robust-issue-price-support-config-does-not-exist.tmp";
    expect_runtime_error(
        [&] {
            (void)cf::load_robust_issue_price_support_config(missing);
        },
        "loader distinguishes missing files from configuration errors");
}

} // namespace

int main() {
    test_parse_canonical_round_trip();
    test_closed_schema_and_numeric_guardrails();
    test_evidence_and_settlement_semantics();
    test_text_bom_and_stream_guardrails();
    test_bounded_adversarial_stream_consumption();
    test_file_size_and_load_guardrails();

    if (failures != 0) {
        std::cerr << failures
                  << " robust issue-price-support config test(s) failed\n";
        return 1;
    }
    std::cout << "robust issue-price-support config tests passed\n";
    return 0;
}
