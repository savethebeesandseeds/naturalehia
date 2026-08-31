// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_config.hpp>

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
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

[[nodiscard]] std::string valid_config() {
    return
        "capital_stack.model_version=0.1.0\n"
        "capital_stack.label=Strict parser test\n"
        "capital_stack.source_note=Synthetic parser fixture only\n"
        "capital_stack.synthetic_inputs=true\n"
        "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero=true\n"
        "capital_stack.subscription_reserve_is_zero_yield_and_lossless=true\n"
        "capital_stack.undrawn_commitment_cancels_and_returns_only_at_horizon=true\n"
        "capital_stack.pool_costs_are_additional_pro_rata_calls=true\n"
        "capital_stack.principal_cash_is_paid_most_senior_first=true\n"
        "capital_stack.nonprincipal_cash_is_paid_to_caps_then_residual=true\n"
        "capital_stack.tranching_does_not_change_project_cash_or_gross_loss=true\n"
        "capital_stack.premium_discount_or_fair_value_is_claimed=false\n"
        "capital_stack.underlying_success_participation_fraction=1\n"
        "tranche.count=3\n"
        "tranche.1.id=first-loss\n"
        "tranche.1.attachment_million=0\n"
        "tranche.1.detachment_million=4\n"
        "tranche.1.priority_nonprincipal_cap_million=0\n"
        "tranche.1.annual_physical_hurdle_rate=0.12\n"
        "tranche.1.is_first_loss_residual=true\n"
        "tranche.2.id=intermediate\n"
        "tranche.2.attachment_million=4\n"
        "tranche.2.detachment_million=10\n"
        "tranche.2.priority_nonprincipal_cap_million=2\n"
        "tranche.2.annual_physical_hurdle_rate=0.08\n"
        "tranche.2.is_first_loss_residual=false\n"
        "tranche.3.id=senior\n"
        "tranche.3.attachment_million=10\n"
        "tranche.3.detachment_million=20\n"
        "tranche.3.priority_nonprincipal_cap_million=1\n"
        "tranche.3.annual_physical_hurdle_rate=0.05\n"
        "tranche.3.is_first_loss_residual=false\n";
}

[[nodiscard]] std::string valid_v02_config() {
    std::string result = valid_config();
    const std::string legacy_version =
        "capital_stack.model_version=0.1.0";
    result.replace(result.find(legacy_version), legacy_version.size(),
        "capital_stack.model_version=0.2.0");
    const std::string legacy_funding_assertion =
        "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero=true";
    result.replace(result.find(legacy_funding_assertion),
        legacy_funding_assertion.size(),
        "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero=false");
    const std::string fair_value_assertion =
        "capital_stack.premium_discount_or_fair_value_is_claimed=false\n";
    const std::size_t insertion =
        result.find(fair_value_assertion) + fair_value_assertion.size();
    result.insert(insertion,
        "capital_stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero=true\n"
        "capital_stack.buyer_direct_costs_are_additional_pro_rata_calls=true\n"
        "capital_stack.principal_base_cash_above_issued_principal_is_nonprincipal=true\n"
        "capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim=true\n");
    return result;
}

[[nodiscard]] cf::CapitalStackConfig parse(std::string text) {
    std::istringstream input(std::move(text));
    return cf::parse_capital_stack_config(input);
}

void test_parse_and_normalized_round_trip() {
    const cf::CapitalStackConfig config = parse(valid_config());
    check(config.model_version == cf::kCapitalStackLegacyModelVersion &&
            config.tranches.size() == 3U &&
            config.tranches.front().is_first_loss_residual &&
            config.tranches[1].priority_nonprincipal_cap_million == 2.0 &&
            config.tranches.back().annual_physical_hurdle_rate == 0.05,
        "strict v0.1 parser retains every tranche term");
    std::ostringstream normalized;
    cf::print_normalized_capital_stack_config(normalized, config);
    check(normalized.str().find(
              "asset_acquisition_and_primary_funding_limit") ==
            std::string::npos,
        "normalized v0.1 output does not add v0.2 keys");
    const cf::CapitalStackConfig reparsed = parse(normalized.str());
    check(reparsed.model_version == cf::kCapitalStackLegacyModelVersion &&
            reparsed.scenario_label == config.scenario_label &&
            reparsed.tranches.size() == config.tranches.size() &&
            reparsed.tranches[0].detachment_million == 4.0 &&
            reparsed.tranches[2].priority_nonprincipal_cap_million == 1.0,
        "normalized v0.1 output is deterministic and reloadable");

    const cf::CapitalStackConfig v02 = parse(valid_v02_config());
    check(v02.model_version == cf::kCapitalStackModelVersion &&
            !v02.aggregate_commitment_is_fully_funded_at_par_at_month_zero &&
            v02.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero &&
            v02.buyer_direct_costs_are_additional_pro_rata_calls &&
            v02.principal_base_cash_above_issued_principal_is_nonprincipal &&
            v02.principal_limit_capacity_difference_is_reported_without_valuation_claim,
        "strict v0.2 parser retains every asset-liability assertion");
    std::ostringstream normalized_v02;
    cf::print_normalized_capital_stack_config(normalized_v02, v02);
    const cf::CapitalStackConfig reparsed_v02 = parse(normalized_v02.str());
    check(reparsed_v02.model_version == cf::kCapitalStackModelVersion &&
            reparsed_v02.scenario_label == v02.scenario_label &&
            reparsed_v02.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero &&
            reparsed_v02.buyer_direct_costs_are_additional_pro_rata_calls &&
            reparsed_v02.principal_base_cash_above_issued_principal_is_nonprincipal &&
            reparsed_v02.principal_limit_capacity_difference_is_reported_without_valuation_claim,
        "normalized v0.2 output is deterministic and reloadable");
}

void test_closed_schema_and_guardrails() {
    expect_invalid_argument(
        [] { (void)parse(valid_config() + "capital_stack.unknown=true\n"); },
        "unknown keys are rejected");

    expect_invalid_argument(
        [] {
            (void)parse(valid_config() +
                "capital_stack.buyer_direct_costs_are_additional_pro_rata_calls=true\n");
        },
        "v0.1 rejects v0.2-only keys");

    std::string unsupported = valid_config();
    const std::string supported_version =
        "capital_stack.model_version=0.1.0";
    unsupported.replace(unsupported.find(supported_version),
        supported_version.size(), "capital_stack.model_version=0.3.0");
    expect_invalid_argument([&] { (void)parse(unsupported); },
        "unsupported model versions are rejected");

    std::string missing_v02_key = valid_v02_config();
    const std::string required_v02_line =
        "capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim=true\n";
    missing_v02_key.erase(missing_v02_key.find(required_v02_line),
        required_v02_line.size());
    expect_invalid_argument([&] { (void)parse(missing_v02_key); },
        "v0.2 requires every asset-liability assertion key");

    std::string legacy_assertion_in_v02 = valid_v02_config();
    const std::string legacy_assertion_false =
        "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero=false";
    legacy_assertion_in_v02.replace(
        legacy_assertion_in_v02.find(legacy_assertion_false),
        legacy_assertion_false.size(),
        "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero=true");
    expect_invalid_argument([&] { (void)parse(legacy_assertion_in_v02); },
        "v0.2 rejects the legacy aggregate-commitment assertion");

    const std::string v02_true_assertions[] = {
        "capital_stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero=true",
        "capital_stack.buyer_direct_costs_are_additional_pro_rata_calls=true",
        "capital_stack.principal_base_cash_above_issued_principal_is_nonprincipal=true",
        "capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim=true",
    };
    for (const std::string& assertion : v02_true_assertions) {
        std::string false_assertion = valid_v02_config();
        false_assertion.replace(false_assertion.find(assertion),
            assertion.size(), assertion.substr(0U, assertion.size() - 4U) +
                "false");
        expect_invalid_argument([&] { (void)parse(false_assertion); },
            "v0.2 accounting assertions must all be true");
    }

    std::string duplicate = valid_config();
    duplicate += "tranche.count=3\n";
    expect_invalid_argument(
        [&] { (void)parse(duplicate); }, "duplicate keys are rejected");

    std::string nonfinite = valid_config();
    const std::string needle = "tranche.3.annual_physical_hurdle_rate=0.05";
    nonfinite.replace(nonfinite.find(needle), needle.size(),
        "tranche.3.annual_physical_hurdle_rate=nan");
    expect_invalid_argument(
        [&] { (void)parse(nonfinite); }, "non-finite numbers are rejected");

    std::string gap = valid_config();
    const std::string attachment = "tranche.2.attachment_million=4";
    gap.replace(gap.find(attachment), attachment.size(),
        "tranche.2.attachment_million=5");
    expect_invalid_argument(
        [&] { (void)parse(gap); }, "non-contiguous layers are rejected");

    std::string false_reserve = valid_config();
    const std::string reserve =
        "capital_stack.subscription_reserve_is_zero_yield_and_lossless=true";
    false_reserve.replace(false_reserve.find(reserve), reserve.size(),
        "capital_stack.subscription_reserve_is_zero_yield_and_lossless=false");
    expect_invalid_argument([&] { (void)parse(false_reserve); },
        "mandatory accounting assertions cannot be omitted");

    std::string unsafe_id = valid_config();
    const std::string safe_id = "tranche.2.id=intermediate";
    unsafe_id.replace(unsafe_id.find(safe_id), safe_id.size(),
        "tranche.2.id=intermediate/../../claim");
    expect_invalid_argument(
        [&] { (void)parse(unsafe_id); }, "unsafe tranche ids are rejected");

    std::string microscopic = valid_config();
    const std::string first_detachment = "tranche.1.detachment_million=4";
    microscopic.replace(microscopic.find(first_detachment),
        first_detachment.size(), "tranche.1.detachment_million=1e-310");
    const std::string second_attachment = "tranche.2.attachment_million=4";
    microscopic.replace(microscopic.find(second_attachment),
        second_attachment.size(), "tranche.2.attachment_million=1e-310");
    expect_invalid_argument([&] { (void)parse(microscopic); },
        "tranches below one base currency unit are rejected");

    std::string negative_attachment = valid_config();
    const std::string zero_attachment = "tranche.1.attachment_million=0";
    negative_attachment.replace(negative_attachment.find(zero_attachment),
        zero_attachment.size(), "tranche.1.attachment_million=-1e-12");
    expect_invalid_argument([&] { (void)parse(negative_attachment); },
        "negative attachment cannot hide inside the comparison tolerance");

    cf::CapitalStackConfig injected = parse(valid_config());
    injected.scenario_label = "first line\ncapital_stack.synthetic_inputs=false";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_capital_stack_config(output, injected);
        },
        "normalized output rejects control-character schema injection");

    injected = parse(valid_config());
    injected.source_note = "embedded \xEF\xBB\xBF BOM";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_capital_stack_config(output, injected);
        },
        "normalized output rejects an embedded BOM it cannot reload");

    injected = parse(valid_config());
    injected.buyer_direct_costs_are_additional_pro_rata_calls = true;
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_capital_stack_config(output, injected);
        },
        "programmatic v0.1 cannot silently carry a v0.2 assertion");
}

} // namespace

int main() {
    test_parse_and_normalized_round_trip();
    test_closed_schema_and_guardrails();
    if (failures != 0) {
        std::cerr << failures << " capital-stack config test(s) failed\n";
        return 1;
    }
    std::cout << "capital-stack config tests passed\n";
    return 0;
}
