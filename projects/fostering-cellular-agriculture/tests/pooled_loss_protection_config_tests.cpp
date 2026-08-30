// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/pooled_loss_protection_config.hpp>

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

[[nodiscard]] cf::PooledLossProtectionConfig full_config() {
    cf::PooledLossProtectionConfig config;
    config.scenario_label = "strict reloadable pooled loss protection";
    config.source_note = "synthetic parser roundtrip terms only";
    config.provider_id = "catalytic-provider-1";
    config.synthetic_inputs = true;
    config.portfolio_principal_loss_is_contractual_reference_amount = true;
    config.gross_project_loss_remains_visible = true;
    config.support_is_assumed_fully_funded_and_performing_in_all_scenarios =
        true;
    config.premium_is_upfront_at_month_zero = true;
    config.underlying_success_participation_fraction = 0.75;
    config.settlement_month = 24U;
    config.support_cap_million = 1.0e-20;
    config.provider_annual_physical_hurdle_rate = 0.125;
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "could not create pooled-loss-protection parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write pooled-loss-protection parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::PooledLossProtectionConfig& config) {
    std::ostringstream output;
    cf::print_normalized_pooled_loss_protection_config(output, config);
    return output.str();
}

[[nodiscard]] cf::PooledLossProtectionConfig parse_text(
    const std::string& text) {
    std::istringstream input{text};
    return cf::parse_pooled_loss_protection_config(input);
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

void test_full_roundtrip_and_output_state(
    const std::filesystem::path& path) {
    const cf::PooledLossProtectionConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_pooled_loss_protection_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller flags, precision, width, fill, and locale");
    check(normalized.starts_with(
              "protection.model_version=0.1.0\n"
              "protection.label=strict reloadable pooled loss protection\n") &&
            normalized.find("protection.provider_id=catalytic-provider-1\n") !=
                std::string::npos &&
            normalized.find("protection.synthetic_inputs=true\n") !=
                std::string::npos &&
            normalized.find(
                "protection.portfolio_principal_loss_is_contractual_reference_amount=true\n") !=
                std::string::npos &&
            normalized.find(
                "protection.support_is_assumed_fully_funded_and_performing_"
                "in_all_scenarios=true\n") !=
                std::string::npos &&
            normalized.find(
                "protection.underlying_success_participation_fraction=0.75\n") !=
                std::string::npos &&
            normalized.find(
                "protection.support_cap_million=9.9999999999999995e-21\n") !=
                std::string::npos &&
            normalized.ends_with(
                "protection.provider_annual_physical_hurdle_rate=0.125\n"),
        "printer emits the complete canonical schema with classic punctuation "
        "and max_digits10 decimals");

    write_text(path, normalized);
    const cf::PooledLossProtectionConfig loaded =
        cf::load_pooled_loss_protection_config(path);
    check(loaded.model_version == original.model_version &&
            loaded.scenario_label == original.scenario_label &&
            loaded.source_note == original.source_note &&
            loaded.provider_id == original.provider_id &&
            loaded.synthetic_inputs == original.synthetic_inputs &&
            loaded.portfolio_principal_loss_is_contractual_reference_amount ==
                original.portfolio_principal_loss_is_contractual_reference_amount &&
            loaded.gross_project_loss_remains_visible ==
                original.gross_project_loss_remains_visible &&
            loaded.support_is_assumed_fully_funded_and_performing_in_all_scenarios ==
                original.support_is_assumed_fully_funded_and_performing_in_all_scenarios &&
            loaded.premium_is_upfront_at_month_zero ==
                original.premium_is_upfront_at_month_zero &&
            loaded.underlying_success_participation_fraction ==
                original.underlying_success_participation_fraction &&
            loaded.settlement_month == original.settlement_month &&
            loaded.support_cap_million == original.support_cap_million &&
            loaded.provider_annual_physical_hurdle_rate ==
                original.provider_annual_physical_hurdle_rate,
        "the complete protection schema round-trips exactly through a file");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");
    check(normalized_config(parse_text(normalized)) == normalized,
        "stream parsing and file loading implement the same closed schema");
}

void test_comments_whitespace_and_bom(const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    const cf::PooledLossProtectionConfig parsed = parse_text(
        bom + "# full-line comment\r\n\r\n" + normalized);
    check(parsed.provider_id == full_config().provider_id,
        "comments, blank lines, CRLF, and a UTF-8 BOM at byte zero are accepted");

    std::string spaced = normalized;
    set_value(spaced, "protection.synthetic_inputs", "  true  ");
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
        [&normalized] { (void)parse_text(normalized + "unknown.field=1\n"); },
        "unknown keys are rejected");
    expect_invalid_argument(
        [&normalized] {
            (void)parse_text(
                normalized + "protection.support_cap_million=1\n");
        },
        "duplicate keys are rejected at read time");

    std::string invalid = normalized;
    remove_key(invalid, "protection.provider_id");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing identity keys are rejected");

    invalid = normalized;
    remove_key(invalid, "protection.gross_project_loss_remains_visible");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing assertion keys are rejected");

    invalid = normalized;
    remove_key(invalid, "protection.settlement_month");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing numeric term keys are rejected");

    expect_invalid_argument(
        [] { (void)parse_text("protection.model_version 0.1.0\n"); },
        "non-key-value lines are rejected");
    expect_invalid_argument([] { (void)parse_text(""); },
        "an empty configuration is rejected");
}

void test_malformed_values(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid,
        "protection.underlying_success_participation_fraction", "0x");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "malformed decimal fractions are rejected");

    for (const std::string_view key : {
             "protection.underlying_success_participation_fraction",
             "protection.support_cap_million",
             "protection.provider_annual_physical_hurdle_rate"}) {
        for (const std::string_view nonfinite : {"nan", "inf", "-inf"}) {
            invalid = normalized;
            set_value(invalid, std::string(key), std::string(nonfinite));
            expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
                "non-finite numeric terms are rejected");
        }
    }

    invalid = normalized;
    set_value(invalid, "protection.settlement_month", "-1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "negative settlement months are rejected");

    invalid = normalized;
    set_value(invalid, "protection.settlement_month",
        "184467440737095516160");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "overflowing settlement months are rejected");

    invalid = normalized;
    set_value(invalid, "protection.synthetic_inputs", "yes");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "non-canonical booleans are rejected");
}

void test_assertions_text_and_provider_id(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "protection.model_version", "9.9.9");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unsupported model versions are rejected");

    invalid = normalized;
    set_value(invalid, "protection.synthetic_inputs", "false");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "v0.1 rejects non-synthetic inputs");

    for (const std::string_view assertion : {
             "protection.portfolio_principal_loss_is_contractual_reference_amount",
             "protection.gross_project_loss_remains_visible",
             "protection.support_is_assumed_fully_funded_and_performing_in_all_scenarios",
             "protection.premium_is_upfront_at_month_zero"}) {
        invalid = normalized;
        set_value(invalid, std::string(assertion), "false");
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "every economic-boundary assertion must be explicitly true");
    }

    invalid = normalized;
    set_value(invalid, "protection.label", std::string(1'025U, 'x'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "free-form text length is bounded");

    invalid = normalized;
    set_value(invalid, "protection.source_note", "safe\x01unsafe");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "control characters in free-form text are rejected");

    for (const std::string_view provider : {
             "-provider", "provider name", "provider:one", "provider/one"}) {
        invalid = normalized;
        set_value(invalid, "protection.provider_id", std::string(provider));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "provider identifiers outside the common safe taxonomy are rejected");
    }
    invalid = normalized;
    set_value(invalid, "protection.provider_id", std::string(129U, 'p'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "provider identifier length is bounded");
}

void test_numeric_boundaries(const std::string& normalized) {
    for (const std::string_view fraction : {"-0.0001", "1.0001"}) {
        std::string invalid = normalized;
        set_value(invalid,
            "protection.underlying_success_participation_fraction",
            std::string(fraction));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "underlying participation must stay within [0, 1]");
    }

    std::string invalid = normalized;
    set_value(invalid, "protection.support_cap_million", "-0.0001");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "negative legal support caps are rejected");

    for (const std::string_view hurdle : {"-0.0001", "10.0001"}) {
        invalid = normalized;
        set_value(invalid,
            "protection.provider_annual_physical_hurdle_rate",
            std::string(hurdle));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "provider physical hurdle must stay within [0, 10]");
    }

    cf::PooledLossProtectionConfig boundary = full_config();
    boundary.underlying_success_participation_fraction = 0.0;
    boundary.settlement_month = 0U;
    boundary.support_cap_million = 0.0;
    boundary.provider_annual_physical_hurdle_rate = 0.0;
    check(parse_text(normalized_config(boundary)).settlement_month == 0U,
        "intrinsic parsing accepts zero boundaries and leaves horizon matching to the core");

    boundary.underlying_success_participation_fraction = 1.0;
    boundary.provider_annual_physical_hurdle_rate = 10.0;
    const cf::PooledLossProtectionConfig upper =
        parse_text(normalized_config(boundary));
    check(upper.underlying_success_participation_fraction == 1.0 &&
            upper.provider_annual_physical_hurdle_rate == 10.0,
        "inclusive upper numeric boundaries are accepted exactly");
}

void test_resource_guards(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string long_line = normalized;
    set_value(long_line, "protection.source_note", std::string(4'097U, 'x'));
    expect_invalid_argument([&long_line] { (void)parse_text(long_line); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_pooled_loss_protection_config(path); },
        "16 MiB file guard is enforced before parsing");

    const std::filesystem::path missing = path.string() + ".missing";
    (void)std::filesystem::remove(missing);
    expect_runtime_error(
        [&missing] { (void)cf::load_pooled_loss_protection_config(missing); },
        "an unreadable path is reported as a runtime error");
}

void test_printer_rejects_invalid_config() {
    cf::PooledLossProtectionConfig invalid = full_config();
    invalid.synthetic_inputs = false;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid] {
            cf::print_normalized_pooled_loss_protection_config(output, invalid);
        },
        "printer rejects false synthetic-input assertions");
    check(output.str().empty(),
        "printer emits no partial output when intrinsic validation fails");

    invalid = full_config();
    invalid.portfolio_principal_loss_is_contractual_reference_amount = false;
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects false reference-amount assertions");

    invalid = full_config();
    invalid.provider_id = "invalid provider";
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects invalid provider identifiers");

    invalid = full_config();
    invalid.support_cap_million =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects non-finite support caps");

    invalid = full_config();
    invalid.scenario_label = " padded";
    expect_invalid_argument([&invalid] { (void)normalized_config(invalid); },
        "printer rejects text that would not round-trip exactly after trimming");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "pooled-loss-protection-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_full_roundtrip_and_output_state(path);
        test_comments_whitespace_and_bom(normalized);
        test_strict_key_set(normalized);
        test_malformed_values(normalized);
        test_assertions_text_and_provider_id(normalized);
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
                  << " pooled-loss-protection-config test(s) failed\n";
        return 1;
    }
    std::cout << "all pooled-loss-protection-config tests passed\n";
    return 0;
}
