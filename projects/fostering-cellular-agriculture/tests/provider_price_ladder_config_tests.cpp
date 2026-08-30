// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_price_ladder_config.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <optional>
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

[[nodiscard]] cf::ProviderPriceLadderConfig full_config() {
    cf::ProviderPriceLadderConfig config;
    config.scenario_label = "strict reloadable provider price ladder";
    config.source_note = "synthetic provider cost assumptions for parser test";
    config.synthetic_inputs = true;
    config.coverage_selection =
        cf::ProviderPriceCoverageSelection::ExplicitCoverageFraction;
    config.explicit_coverage_fraction = 0.625;
    config.cost_bases_use_contractual_maximum_exposure = true;
    config.collateral_and_capital_are_held_until_settlement = true;
    config.variable_claim_expense_is_paid_at_claim_settlement = true;
    config.fixed_expense_and_target_profit_are_month_zero_values = true;
    config.incremental_cost_terms_are_separate_and_nonduplicative = true;
    config.collateral_fraction_of_contractual_maximum_exposure = 0.8;
    config.collateral_annual_effective_funding_rate = 0.07;
    config.collateral_annual_effective_yield_rate = 0.02;
    config.risk_capital_fraction_of_contractual_maximum_exposure = 0.3;
    config.risk_capital_annual_effective_charge_rate = 0.12;
    config.fixed_expense_upfront_million = 1.0e-20;
    config.variable_claim_expense_fraction = 0.04;
    config.target_profit_upfront_million = 0.125;
    config.provider_default_risk_is_modeled = false;
    config.fair_value_is_claimed = false;
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "could not create provider-price-ladder parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write provider-price-ladder parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::ProviderPriceLadderConfig& config) {
    std::ostringstream output;
    cf::print_normalized_provider_price_ladder_config(output, config);
    return output.str();
}

[[nodiscard]] cf::ProviderPriceLadderConfig parse_text(
    const std::string& text) {
    std::istringstream input{text};
    return cf::parse_provider_price_ladder_config(input);
}

void set_value(
    std::string& text, const std::string& key, const std::string& value) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture key not found: " + key);
    }
    const std::size_t value_begin = position + prefix.size();
    const std::size_t line_end = text.find('\n', value_begin);
    text.replace(value_begin,
        line_end == std::string::npos ? std::string::npos
                                     : line_end - value_begin,
        value);
}

void remove_key(std::string& text, const std::string& key) {
    const std::string prefix = key + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test fixture key not found: " + key);
    }
    const std::size_t line_end = text.find('\n', position);
    text.erase(position, line_end == std::string::npos
            ? std::string::npos
            : line_end - position + 1U);
}

[[nodiscard]] bool configs_equal(
    const cf::ProviderPriceLadderConfig& left,
    const cf::ProviderPriceLadderConfig& right) {
    return left.model_version == right.model_version &&
        left.scenario_label == right.scenario_label &&
        left.source_note == right.source_note &&
        left.synthetic_inputs == right.synthetic_inputs &&
        left.coverage_selection == right.coverage_selection &&
        left.explicit_coverage_fraction == right.explicit_coverage_fraction &&
        left.cost_bases_use_contractual_maximum_exposure ==
            right.cost_bases_use_contractual_maximum_exposure &&
        left.collateral_and_capital_are_held_until_settlement ==
            right.collateral_and_capital_are_held_until_settlement &&
        left.variable_claim_expense_is_paid_at_claim_settlement ==
            right.variable_claim_expense_is_paid_at_claim_settlement &&
        left.fixed_expense_and_target_profit_are_month_zero_values ==
            right.fixed_expense_and_target_profit_are_month_zero_values &&
        left.incremental_cost_terms_are_separate_and_nonduplicative ==
            right.incremental_cost_terms_are_separate_and_nonduplicative &&
        left.collateral_fraction_of_contractual_maximum_exposure ==
            right.collateral_fraction_of_contractual_maximum_exposure &&
        left.collateral_annual_effective_funding_rate ==
            right.collateral_annual_effective_funding_rate &&
        left.collateral_annual_effective_yield_rate ==
            right.collateral_annual_effective_yield_rate &&
        left.risk_capital_fraction_of_contractual_maximum_exposure ==
            right.risk_capital_fraction_of_contractual_maximum_exposure &&
        left.risk_capital_annual_effective_charge_rate ==
            right.risk_capital_annual_effective_charge_rate &&
        left.fixed_expense_upfront_million ==
            right.fixed_expense_upfront_million &&
        left.variable_claim_expense_fraction ==
            right.variable_claim_expense_fraction &&
        left.target_profit_upfront_million ==
            right.target_profit_upfront_million &&
        left.provider_default_risk_is_modeled ==
            right.provider_default_risk_is_modeled &&
        left.fair_value_is_claimed == right.fair_value_is_claimed;
}

void test_full_roundtrip_and_output_state(
    const std::filesystem::path& path) {
    const cf::ProviderPriceLadderConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_provider_price_ladder_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller flags, precision, width, fill, and locale");
    check(normalized.starts_with(
              "provider_price.model_version=0.1.0\n"
              "provider_price.label=strict reloadable provider price ladder\n") &&
            normalized.find("provider_price.coverage_selection=explicit\n") !=
                std::string::npos &&
            normalized.find(
                "provider_price.explicit_coverage_fraction=0.625\n") !=
                std::string::npos &&
            normalized.find(
                "provider_price.cost_bases_use_contractual_maximum_exposure=true\n") !=
                std::string::npos &&
            normalized.find(
                "provider_price.incremental_cost_terms_are_separate_and_nonduplicative=true\n") !=
                std::string::npos &&
            normalized.find(
                "provider_price.fixed_expense_upfront_million=9.9999999999999995e-21\n") !=
                std::string::npos &&
            normalized.ends_with(
                "provider_price.provider_default_risk_is_modeled=false\n"
                "provider_price.fair_value_is_claimed=false\n"),
        "printer emits the complete canonical provider_price schema with exact tokens");

    write_text(path, normalized);
    const cf::ProviderPriceLadderConfig loaded =
        cf::load_provider_price_ladder_config(path);
    check(configs_equal(loaded, original),
        "the complete provider-price schema round-trips exactly through a file");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");
    check(normalized_config(parse_text(normalized)) == normalized,
        "stream parsing and file loading implement the same closed schema");
}

void test_reported_selection_and_conditionals() {
    cf::ProviderPriceLadderConfig reported = full_config();
    reported.coverage_selection = cf::ProviderPriceCoverageSelection::
        ReportedInvestorTargetPassingFraction;
    reported.explicit_coverage_fraction.reset();
    const std::string normalized = normalized_config(reported);
    check(normalized.find(
              "provider_price.coverage_selection=reported-investor-target-passing\n") !=
            std::string::npos &&
            normalized.find(
                "provider_price.explicit_coverage_fraction=none\n") !=
                std::string::npos,
        "reported-point mode has explicit canonical selection and none tokens");
    check(configs_equal(parse_text(normalized), reported),
        "reported-point mode round-trips without inventing a coverage fraction");

    std::string invalid = normalized;
    set_value(invalid, "provider_price.explicit_coverage_fraction", "0.5");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "reported-point selection rejects an explicit fraction");

    invalid = normalized_config(full_config());
    set_value(invalid, "provider_price.explicit_coverage_fraction", "none");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "explicit selection requires an explicit fraction");

    invalid = normalized;
    set_value(invalid, "provider_price.coverage_selection", "reported");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "coverage selection rejects aliases and unknown enum tokens");

    invalid = normalized;
    set_value(invalid, "provider_price.explicit_coverage_fraction", "None");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "the none sentinel is canonical and case-sensitive");
}

void test_comments_whitespace_and_bom(const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    const cf::ProviderPriceLadderConfig parsed = parse_text(
        bom + "# full-line comment\r\n\r\n" + normalized);
    check(parsed.scenario_label == full_config().scenario_label,
        "comments, blank lines, CRLF, and a UTF-8 BOM at byte zero are accepted");

    std::string spaced = normalized;
    set_value(spaced, "provider_price.synthetic_inputs", "  true  ");
    check(parse_text(spaced).synthetic_inputs,
        "leading and trailing key-value whitespace is ignored");

    std::string misplaced = normalized;
    const std::size_t second_line = misplaced.find('\n');
    misplaced.insert(second_line + 1U, bom);
    expect_invalid_argument([&misplaced] { (void)parse_text(misplaced); },
        "UTF-8 BOM is rejected anywhere except byte zero");
}

void test_strict_key_set(const std::string& normalized) {
    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(normalized + "provider_price.unknown=1\n");
        },
        "unknown provider_price keys are rejected");
    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(normalized +
                "provider_price.target_profit_upfront_million=1\n");
        },
        "duplicate keys are rejected at read time");

    std::string invalid = normalized;
    remove_key(invalid, "provider_price.coverage_selection");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing selection keys are rejected");

    invalid = normalized;
    remove_key(invalid,
        "provider_price.incremental_cost_terms_are_separate_and_nonduplicative");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing accounting assertion keys are rejected");

    invalid = normalized;
    remove_key(invalid,
        "provider_price.risk_capital_annual_effective_charge_rate");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing numeric cost keys are rejected");

    expect_invalid_argument(
        [] { (void)parse_text("provider_price.model_version 0.1.0\n"); },
        "non-key-value lines are rejected");
    expect_invalid_argument([] { (void)parse_text(""); },
        "an empty configuration is rejected");
}

void test_malformed_values(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "provider_price.variable_claim_expense_fraction", "0x");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "malformed decimal cost terms are rejected");

    for (const std::string_view key : {
             "provider_price.explicit_coverage_fraction",
             "provider_price.collateral_fraction_of_contractual_maximum_exposure",
             "provider_price.collateral_annual_effective_funding_rate",
             "provider_price.collateral_annual_effective_yield_rate",
             "provider_price.risk_capital_fraction_of_contractual_maximum_exposure",
             "provider_price.risk_capital_annual_effective_charge_rate",
             "provider_price.fixed_expense_upfront_million",
             "provider_price.variable_claim_expense_fraction",
             "provider_price.target_profit_upfront_million"}) {
        for (const std::string_view nonfinite : {"nan", "inf", "-inf"}) {
            invalid = normalized;
            set_value(invalid, std::string(key), std::string(nonfinite));
            expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
                "non-finite provider-price terms are rejected");
        }
    }

    invalid = normalized;
    set_value(invalid, "provider_price.synthetic_inputs", "yes");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "non-canonical booleans are rejected");
}

void test_assertions_and_text(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "provider_price.model_version", "9.9.9");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unsupported provider-price model versions are rejected");

    invalid = normalized;
    set_value(invalid, "provider_price.synthetic_inputs", "false");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "v0.1 rejects non-synthetic provider-price inputs");

    for (const std::string_view assertion : {
             "provider_price.cost_bases_use_contractual_maximum_exposure",
             "provider_price.collateral_and_capital_are_held_until_settlement",
             "provider_price.variable_claim_expense_is_paid_at_claim_settlement",
             "provider_price.fixed_expense_and_target_profit_are_month_zero_values",
             "provider_price.incremental_cost_terms_are_separate_and_nonduplicative"}) {
        invalid = normalized;
        set_value(invalid, std::string(assertion), "false");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "every timing, cost-base, and nonduplication assertion must be explicitly true");
    }

    for (const std::string_view prohibited_true : {
             "provider_price.provider_default_risk_is_modeled",
             "provider_price.fair_value_is_claimed"}) {
        invalid = normalized;
        set_value(invalid, std::string(prohibited_true), "true");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "v0.1 requires provider-default and fair-value claims to remain false");
    }

    invalid = normalized;
    set_value(invalid, "provider_price.label", std::string(1'025U, 'x'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "free-form provider-price text length is bounded");

    invalid = normalized;
    set_value(invalid, "provider_price.source_note", "safe\x01unsafe");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "control characters in provider-price text are rejected");

    invalid = normalized;
    set_value(invalid, "provider_price.label", " padded");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "leading whitespace in a text value is rejected rather than silently normalized");

    invalid = normalized;
    set_value(invalid, "provider_price.source_note", "padded ");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "trailing whitespace in a text value is rejected rather than silently normalized");
}

void test_numeric_boundaries(const std::string& normalized) {
    for (const std::string_view key : {
             "provider_price.explicit_coverage_fraction",
             "provider_price.collateral_fraction_of_contractual_maximum_exposure",
             "provider_price.risk_capital_fraction_of_contractual_maximum_exposure"}) {
        for (const std::string_view value : {"-0.0001", "1.0001"}) {
            std::string invalid = normalized;
            set_value(invalid, std::string(key), std::string(value));
            expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
                "coverage, collateral, and risk-capital fractions stay within [0, 1]");
        }
    }

    for (const std::string_view key : {
             "provider_price.collateral_annual_effective_funding_rate",
             "provider_price.collateral_annual_effective_yield_rate",
             "provider_price.risk_capital_annual_effective_charge_rate",
             "provider_price.variable_claim_expense_fraction"}) {
        for (const std::string_view value : {"-0.0001", "10.0001"}) {
            std::string invalid = normalized;
            set_value(invalid, std::string(key), std::string(value));
            expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
                "annual effective and variable-expense rates stay within [0, 10]");
        }
    }

    for (const std::string_view key : {
             "provider_price.fixed_expense_upfront_million",
             "provider_price.target_profit_upfront_million"}) {
        std::string invalid = normalized;
        set_value(invalid, std::string(key), "-0.0001");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "upfront expense and target-profit amounts are non-negative");
    }

    std::string invalid = normalized;
    set_value(invalid,
        "provider_price.collateral_annual_effective_funding_rate", "0.01");
    set_value(invalid,
        "provider_price.collateral_annual_effective_yield_rate", "0.02");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "collateral yield cannot exceed its funding rate in v0.1");

    cf::ProviderPriceLadderConfig boundary = full_config();
    boundary.explicit_coverage_fraction = 0.0;
    boundary.collateral_fraction_of_contractual_maximum_exposure = 0.0;
    boundary.risk_capital_fraction_of_contractual_maximum_exposure = 0.0;
    boundary.collateral_annual_effective_funding_rate = 0.0;
    boundary.collateral_annual_effective_yield_rate = 0.0;
    boundary.risk_capital_annual_effective_charge_rate = 0.0;
    boundary.fixed_expense_upfront_million = 0.0;
    boundary.variable_claim_expense_fraction = 0.0;
    boundary.target_profit_upfront_million = 0.0;
    check(parse_text(normalized_config(boundary)).explicit_coverage_fraction ==
            std::optional<double>{0.0},
        "all inclusive zero boundaries are accepted exactly");

    boundary.explicit_coverage_fraction = 1.0;
    boundary.collateral_fraction_of_contractual_maximum_exposure = 1.0;
    boundary.risk_capital_fraction_of_contractual_maximum_exposure = 1.0;
    boundary.collateral_annual_effective_funding_rate = 10.0;
    boundary.collateral_annual_effective_yield_rate = 10.0;
    boundary.risk_capital_annual_effective_charge_rate = 10.0;
    boundary.variable_claim_expense_fraction = 10.0;
    const cf::ProviderPriceLadderConfig upper =
        parse_text(normalized_config(boundary));
    check(upper.explicit_coverage_fraction == std::optional<double>{1.0} &&
            upper.collateral_annual_effective_funding_rate == 10.0 &&
            upper.variable_claim_expense_fraction == 10.0,
        "all inclusive upper fraction and rate boundaries are accepted exactly");
}

void test_resource_guards(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string long_line = normalized;
    set_value(long_line, "provider_price.source_note", std::string(4'097U, 'x'));
    expect_invalid_argument([&long_line] { (void)parse_text(long_line); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_provider_price_ladder_config(path); },
        "16 MiB file guard is enforced before parsing");

    const std::filesystem::path missing = path.string() + ".missing";
    (void)std::filesystem::remove(missing);
    expect_runtime_error(
        [&missing] { (void)cf::load_provider_price_ladder_config(missing); },
        "an unreadable provider-price path is reported as a runtime error");
}

void test_printer_rejects_invalid_config() {
    cf::ProviderPriceLadderConfig invalid = full_config();
    invalid.synthetic_inputs = false;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid] {
            cf::print_normalized_provider_price_ladder_config(output, invalid);
        },
        "printer rejects false synthetic-input assertions");
    check(output.str().empty(),
        "printer emits no partial output when validation fails");

    invalid = full_config();
    invalid.incremental_cost_terms_are_separate_and_nonduplicative = false;
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects a false nonduplication assertion");

    invalid = full_config();
    invalid.provider_default_risk_is_modeled = true;
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects a modeled-provider-default claim in v0.1");

    invalid = full_config();
    invalid.fair_value_is_claimed = true;
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects a fair-value claim in v0.1");

    invalid = full_config();
    invalid.explicit_coverage_fraction =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects non-finite optional coverage fractions");

    invalid = full_config();
    invalid.scenario_label = " padded";
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects text that would not round-trip after trimming");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "provider-price-ladder-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_full_roundtrip_and_output_state(path);
        test_reported_selection_and_conditionals();
        test_comments_whitespace_and_bom(normalized);
        test_strict_key_set(normalized);
        test_malformed_values(normalized);
        test_assertions_and_text(normalized);
        test_numeric_boundaries(normalized);
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
                  << " provider-price-ladder-config test(s) failed\n";
        return 1;
    }
    std::cout << "all provider-price-ladder-config tests passed\n";
    return 0;
}
