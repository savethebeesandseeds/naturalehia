// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_credit_stress_config.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

class NonCanonicalPunctuation final : public std::numpunct<char> {
protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
    [[nodiscard]] std::string do_truename() const override { return "yes"; }
    [[nodiscard]] std::string do_falsename() const override { return "no"; }
};

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

[[nodiscard]] cf::ProviderCreditStressConfig full_config() {
    cf::ProviderCreditStressConfig config;
    config.scenario_label = "strict provider counterparty-credit stress";
    config.source_note =
        "synthetic physical conditional states for parser testing";
    config.provider_id = "synthetic-catalytic-provider";
    config.synthetic_inputs = true;
    config.gross_contractual_claim_remains_unchanged = true;
    config.provider_price_remains_full_performance_and_unchanged = true;
    config.conditional_provider_state_weights_are_fixed_physical = true;
    config.price_ladder_collateral_is_pledged_to_investor = true;
    config.collateral_yield_remains_in_pledged_account = true;
    config.collateral_applies_before_unsecured_recovery = true;
    config.provider_default_occurs_at_claim_settlement = true;
    config.provider_default_is_physical_stress_not_pricing_measure = true;
    config.legal_enforceability_is_validated = false;
    config.market_cva_or_fair_value_is_claimed = false;
    config.scenarios = {
        cf::ProviderCreditScenarioConfig{
            "common-success",
            {
                cf::ProviderCreditOutcomeConfig{
                    "performs", 0.75, true, 0.0, 0.0, 0U},
                cf::ProviderCreditOutcomeConfig{
                    "defaults", 0.25, false, 0.75, 0.25, 6U},
            }},
        cf::ProviderCreditScenarioConfig{
            "common-loss",
            {
                cf::ProviderCreditOutcomeConfig{
                    "performs", 0.25, true, 0.0, 0.0, 0U},
                // Zero collateral realization is a valid adverse default
                // state; the parser must not manufacture collateral benefit.
                cf::ProviderCreditOutcomeConfig{
                    "defaults", 0.75, false, 0.0, 0.1, 1'200U},
            }},
    };
    return config;
}

[[nodiscard]] std::string normalized_config(
    const cf::ProviderCreditStressConfig& config) {
    std::ostringstream output;
    cf::print_normalized_provider_credit_stress_config(output, config);
    return output.str();
}

[[nodiscard]] cf::ProviderCreditStressConfig parse_text(
    const std::string& text) {
    std::istringstream input(text);
    return cf::parse_provider_credit_stress_config(input);
}

void set_value(
    std::string& text, const std::string& key, const std::string& value) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::logic_error("test key not found: " + key);
    }
    const std::size_t value_begin = position + prefix.size();
    const std::size_t value_end = text.find('\n', value_begin);
    text.replace(value_begin, value_end - value_begin, value);
}

void remove_key(std::string& text, const std::string& key) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::logic_error("test key not found: " + key);
    }
    const std::size_t line_end = text.find('\n', position);
    text.erase(position, line_end - position + 1U);
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create parser test file");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error("could not write parser test file");
    }
}

[[nodiscard]] bool configs_equal(
    const cf::ProviderCreditStressConfig& left,
    const cf::ProviderCreditStressConfig& right) {
    if (left.model_version != right.model_version ||
        left.scenario_label != right.scenario_label ||
        left.source_note != right.source_note ||
        left.provider_id != right.provider_id ||
        left.synthetic_inputs != right.synthetic_inputs ||
        left.gross_contractual_claim_remains_unchanged !=
            right.gross_contractual_claim_remains_unchanged ||
        left.provider_price_remains_full_performance_and_unchanged !=
            right.provider_price_remains_full_performance_and_unchanged ||
        left.conditional_provider_state_weights_are_fixed_physical !=
            right.conditional_provider_state_weights_are_fixed_physical ||
        left.price_ladder_collateral_is_pledged_to_investor !=
            right.price_ladder_collateral_is_pledged_to_investor ||
        left.collateral_yield_remains_in_pledged_account !=
            right.collateral_yield_remains_in_pledged_account ||
        left.collateral_applies_before_unsecured_recovery !=
            right.collateral_applies_before_unsecured_recovery ||
        left.provider_default_occurs_at_claim_settlement !=
            right.provider_default_occurs_at_claim_settlement ||
        left.provider_default_is_physical_stress_not_pricing_measure !=
            right.provider_default_is_physical_stress_not_pricing_measure ||
        left.legal_enforceability_is_validated !=
            right.legal_enforceability_is_validated ||
        left.market_cva_or_fair_value_is_claimed !=
            right.market_cva_or_fair_value_is_claimed ||
        left.scenarios.size() != right.scenarios.size()) {
        return false;
    }
    for (std::size_t scenario = 0U; scenario < left.scenarios.size();
         ++scenario) {
        const cf::ProviderCreditScenarioConfig& left_scenario =
            left.scenarios[scenario];
        const cf::ProviderCreditScenarioConfig& right_scenario =
            right.scenarios[scenario];
        if (left_scenario.scenario_id != right_scenario.scenario_id ||
            left_scenario.outcomes.size() != right_scenario.outcomes.size()) {
            return false;
        }
        for (std::size_t state = 0U;
             state < left_scenario.outcomes.size(); ++state) {
            const cf::ProviderCreditOutcomeConfig& left_outcome =
                left_scenario.outcomes[state];
            const cf::ProviderCreditOutcomeConfig& right_outcome =
                right_scenario.outcomes[state];
            if (left_outcome.outcome_id != right_outcome.outcome_id ||
                left_outcome.conditional_weight !=
                    right_outcome.conditional_weight ||
                left_outcome.provider_performs !=
                    right_outcome.provider_performs ||
                left_outcome.collateral_realization_fraction !=
                    right_outcome.collateral_realization_fraction ||
                left_outcome.unsecured_recovery_fraction !=
                    right_outcome.unsecured_recovery_fraction ||
                left_outcome.unsecured_recovery_delay_months !=
                    right_outcome.unsecured_recovery_delay_months) {
                return false;
            }
        }
    }
    return true;
}

void test_roundtrip_and_output_state(const std::filesystem::path& path) {
    const cf::ProviderCreditStressConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_provider_credit_stress_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller formatting state and locale");
    check(normalized.starts_with(
              "provider_credit.model_version=0.1.0\n"
              "provider_credit.label=strict provider counterparty-credit stress\n") &&
            normalized.find(
                "provider_credit.provider_id=synthetic-catalytic-provider\n") !=
                std::string::npos &&
            normalized.find(
                "provider_credit.provider_price_remains_full_performance_and_unchanged=true\n") !=
                std::string::npos &&
            normalized.find(
                "provider_credit.scenario.1.state.2.unsecured_recovery_delay_months=6\n") !=
                std::string::npos &&
            normalized.ends_with(
                "provider_credit.scenario.2.state.2.unsecured_recovery_delay_months=1200\n"),
        "printer emits the complete closed nested provider-credit schema");

    write_text(path, normalized);
    const cf::ProviderCreditStressConfig loaded =
        cf::load_provider_credit_stress_config(path);
    check(configs_equal(loaded, original),
        "the complete provider-credit schema round-trips exactly");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");
    check(configs_equal(parse_text(normalized), original),
        "stream parsing and file loading implement the same schema");
}

void test_comments_whitespace_and_bom(const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    std::string crlf = normalized;
    std::size_t position = 0U;
    while ((position = crlf.find('\n', position)) != std::string::npos) {
        crlf.replace(position, 1U, "\r\n");
        position += 2U;
    }
    const cf::ProviderCreditStressConfig parsed = parse_text(
        bom + "# full-line comment\r\n\r\n" + crlf);
    check(parsed.scenarios.size() == 2U,
        "comments, blank lines, CRLF, and a BOM at byte zero are accepted");

    std::string spaced = normalized;
    set_value(spaced, "provider_credit.synthetic_inputs", "  true  ");
    check(parse_text(spaced).synthetic_inputs,
        "surrounding whitespace is accepted for non-text values");

    std::string invalid = normalized;
    set_value(invalid, "provider_credit.label", " padded");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "leading text whitespace is rejected rather than normalized away");
    invalid = normalized;
    set_value(invalid, "provider_credit.source_note", "padded ");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "trailing text whitespace is rejected rather than normalized away");

    std::string misplaced = normalized;
    const std::size_t second_line = misplaced.find('\n');
    misplaced.insert(second_line + 1U, bom);
    expect_invalid_argument([&misplaced] { (void)parse_text(misplaced); },
        "UTF-8 BOM is rejected anywhere except byte zero");
}

void test_strict_dynamic_key_set(const std::string& normalized) {
    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(normalized + "provider_credit.unknown=1\n");
        },
        "unknown provider-credit keys are rejected");
    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(normalized +
                "provider_credit.scenario.1.state.1.id=again\n");
        },
        "duplicate nested keys are rejected at read time");

    for (const std::string_view key : {
             "provider_credit.provider_price_remains_full_performance_and_unchanged",
             "provider_credit.provider_id",
             "provider_credit.scenario.1.id",
             "provider_credit.scenario.1.state.count",
             "provider_credit.scenario.2.state.2.unsecured_recovery_fraction"}) {
        std::string invalid = normalized;
        remove_key(invalid, std::string(key));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "missing global and nested keys are rejected");
    }

    std::string invalid = normalized;
    set_value(invalid, "provider_credit.scenario.count", "1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "keys beyond a reduced declared scenario count are unknown");
    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.1.state.count", "1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "keys beyond a reduced declared state count are unknown");

    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.count", "0");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "zero scenario count is rejected");
    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.count", "10001");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "scenario count guardrail is enforced before allocation");
    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.1.state.count", "0");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "zero state count is rejected");
    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.1.state.count", "1001");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "per-scenario state count guardrail is enforced before allocation");

    expect_invalid_argument(
        [] { (void)parse_text("provider_credit.model_version 0.1.0\n"); },
        "non-key-value lines are rejected");
    expect_invalid_argument([] { (void)parse_text(""); },
        "an empty configuration is rejected");
}

void test_assertions_and_metadata(const std::string& normalized) {
    for (const std::string_view required_true : {
             "provider_credit.gross_contractual_claim_remains_unchanged",
             "provider_credit.provider_price_remains_full_performance_and_unchanged",
             "provider_credit.conditional_provider_state_weights_are_fixed_physical",
             "provider_credit.collateral_applies_before_unsecured_recovery",
             "provider_credit.provider_default_occurs_at_claim_settlement",
             "provider_credit.provider_default_is_physical_stress_not_pricing_measure"}) {
        std::string invalid = normalized;
        set_value(invalid, std::string(required_true), "false");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "every preservation, pledge, timing, and physical-weight assertion must be true");
    }

    std::string unpledged = normalized;
    set_value(unpledged,
        "provider_credit.price_ladder_collateral_is_pledged_to_investor",
        "false");
    set_value(unpledged,
        "provider_credit.collateral_yield_remains_in_pledged_account",
        "false");
    set_value(unpledged,
        "provider_credit.scenario.1.state.2.collateral_realization_fraction",
        "0");
    const cf::ProviderCreditStressConfig accepted_unpledged =
        parse_text(unpledged);
    check(!accepted_unpledged.price_ladder_collateral_is_pledged_to_investor &&
            accepted_unpledged.scenarios[0].outcomes[1]
                    .collateral_realization_fraction == 0.0,
        "carry-priced but unpledged collateral is accepted with no investor benefit");

    std::string invalid = unpledged;
    set_value(invalid,
        "provider_credit.scenario.1.state.2.collateral_realization_fraction",
        "0.01");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unpledged price-ladder collateral rejects nonzero realization benefit");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.collateral_yield_remains_in_pledged_account",
        "false");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "a declared collateral pledge requires yield retention");
    invalid = unpledged;
    set_value(invalid,
        "provider_credit.collateral_yield_remains_in_pledged_account",
        "true");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unpledged collateral cannot claim retained pledged-account yield");

    for (const std::string_view required_false : {
             "provider_credit.legal_enforceability_is_validated",
             "provider_credit.market_cva_or_fair_value_is_claimed"}) {
        invalid = normalized;
        set_value(invalid, std::string(required_false), "true");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "legal-enforceability and market-value claims must remain false");
    }

    invalid = normalized;
    set_value(invalid, "provider_credit.synthetic_inputs", "false");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "v0.1 rejects non-synthetic counterparty-credit inputs");
    invalid = normalized;
    set_value(invalid, "provider_credit.synthetic_inputs", "yes");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "booleans use exact true/false tokens");
    invalid = normalized;
    set_value(invalid, "provider_credit.model_version", "9.9.9");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unsupported provider-credit model versions are rejected");
    invalid = normalized;
    set_value(invalid, "provider_credit.label", std::string(1'025U, 'x'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "free-form provider-credit text length is bounded");
    invalid = normalized;
    set_value(invalid, "provider_credit.source_note", "safe\x01unsafe");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "control characters in provider-credit text are rejected");
}

void test_weights_and_state_values(const std::string& normalized) {
    for (const std::string_view value : {"0", "-0.1", "1.1", "nan", "inf"}) {
        std::string invalid = normalized;
        set_value(invalid,
            "provider_credit.scenario.1.state.1.conditional_weight",
            std::string(value));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "conditional weights must be finite values within (0, 1]");
    }

    std::string invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.1.conditional_weight", "0.7");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "conditional weights must sum to one within strict tolerance");

    std::string near = normalized;
    set_value(near,
        "provider_credit.scenario.1.state.1.conditional_weight",
        "0.7500000000004");
    const cf::ProviderCreditStressConfig normalized_near = parse_text(near);
    const double normalized_sum =
        normalized_near.scenarios[0].outcomes[0].conditional_weight +
        normalized_near.scenarios[0].outcomes[1].conditional_weight;
    check(std::abs(normalized_sum - 1.0) <=
            std::numeric_limits<double>::epsilon() &&
            normalized_near.scenarios[0].outcomes[0].conditional_weight <
                0.7500000000004,
        "within-tolerance conditional weights are normalized on input");
    check(normalized_config(normalized_near) ==
            normalized_config(parse_text(normalized_config(normalized_near))),
        "normalized within-tolerance weights print idempotently");

    for (const std::string_view key : {
             "provider_credit.scenario.1.state.2.collateral_realization_fraction",
             "provider_credit.scenario.1.state.2.unsecured_recovery_fraction"}) {
        for (const std::string_view value : {"-0.1", "1.1", "nan", "inf"}) {
            invalid = normalized;
            set_value(invalid, std::string(key), std::string(value));
            expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
                "collateral and recovery fractions stay finite within [0, 1]");
        }
    }

    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.1.collateral_realization_fraction",
        "0.1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "performing states reject collateral application");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.1.unsecured_recovery_fraction",
        "0.1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "performing states reject unsecured recovery");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.1.unsecured_recovery_delay_months",
        "1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "performing states reject recovery delay");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.1.collateral_realization_fraction",
        "-0");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "performing zero fractions must use the canonical non-negative zero");

    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.2.collateral_realization_fraction",
        "0");
    const cf::ProviderCreditStressConfig zero_collateral = parse_text(invalid);
    check(zero_collateral.scenarios[0].outcomes[1]
              .collateral_realization_fraction == 0.0,
        "default states may explicitly realize no pledged collateral");

    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.2.unsecured_recovery_delay_months",
        "1201");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unsecured recovery delay is bounded at 1200 months");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.2.unsecured_recovery_delay_months",
        "-1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "negative recovery delays are rejected as malformed integers");
}

void test_identifier_rules(const std::string& normalized) {
    std::string different_provider = normalized;
    set_value(different_provider, "provider_credit.provider_id",
        "different-safe-provider");
    const cf::ProviderCreditStressConfig explicit_mismatch =
        parse_text(different_provider);
    check(explicit_mismatch.provider_id == "different-safe-provider" &&
            normalized_config(explicit_mismatch).find(
                "provider_credit.provider_id=different-safe-provider\n") !=
                std::string::npos,
        "a different safe provider ID remains explicit for core cross-input mismatch rejection");

    std::string invalid = normalized;
    set_value(invalid, "provider_credit.provider_id", "unsafe/provider");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "provider IDs are restricted to safe identifier syntax");
    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.2.id", "common-success");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "portfolio scenario IDs in the credit overlay must be unique");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.2.id", "performs");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "provider state IDs must be unique within a portfolio scenario");
    invalid = normalized;
    set_value(invalid, "provider_credit.scenario.1.id", "unsafe/id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "portfolio scenario IDs are restricted to safe identifier syntax");
    invalid = normalized;
    set_value(invalid,
        "provider_credit.scenario.1.state.1.id", ".leading-dot");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "provider state IDs must begin with an ASCII alphanumeric");
}

void test_resource_guards(
    const std::filesystem::path& path, const std::string& normalized) {
    std::string long_line = normalized;
    set_value(long_line,
        "provider_credit.source_note", std::string(4'097U, 'x'));
    expect_invalid_argument([&long_line] { (void)parse_text(long_line); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_provider_credit_stress_config(path); },
        "16 MiB file guard is enforced before parsing");

    const std::filesystem::path missing = path.string() + ".missing";
    (void)std::filesystem::remove(missing);
    expect_runtime_error(
        [&missing] { (void)cf::load_provider_credit_stress_config(missing); },
        "an unreadable provider-credit path is a runtime error");
}

void test_printer_rejects_invalid_config() {
    cf::ProviderCreditStressConfig invalid = full_config();
    invalid.provider_price_remains_full_performance_and_unchanged = false;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid] {
            cf::print_normalized_provider_credit_stress_config(
                output, invalid);
        },
        "printer rejects a false provider-price preservation assertion");
    check(output.str().empty(),
        "printer emits no partial output when validation fails");

    invalid = full_config();
    invalid.market_cva_or_fair_value_is_claimed = true;
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects a market-CVA or fair-value claim");
    invalid = full_config();
    invalid.scenarios[0].outcomes[0].conditional_weight =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects non-finite in-memory weights");
    invalid = full_config();
    invalid.scenario_label = " padded";
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects text that would not round-trip after trimming");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "provider-credit-stress-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_roundtrip_and_output_state(path);
        test_comments_whitespace_and_bom(normalized);
        test_strict_dynamic_key_set(normalized);
        test_assertions_and_metadata(normalized);
        test_weights_and_state_values(normalized);
        test_identifier_rules(normalized);
        test_resource_guards(path, normalized);
        test_printer_rejects_invalid_config();
        (void)std::filesystem::remove(path);
    } catch (const std::exception& error) {
        (void)std::filesystem::remove(path);
        std::cerr << "FAIL: unexpected test exception: " << error.what()
                  << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " provider-credit-stress-config test(s) failed\n";
        return 1;
    }
    std::cout << "all provider-credit-stress-config tests passed\n";
    return 0;
}
