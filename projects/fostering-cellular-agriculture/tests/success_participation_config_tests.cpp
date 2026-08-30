// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/success_participation_config.hpp>

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

[[nodiscard]] cf::SuccessParticipationConfig full_config() {
    cf::SuccessParticipationConfig config;
    config.scenario_label = "strict reloadable participation term";
    config.source_note = "synthetic parser roundtrip values only";
    config.synthetic_inputs = true;
    config.selected_nonprincipal_cash_is_contractually_scalable = true;
    config.target_worst_expected_npv_million = -1.0e-20;
    config.scalable_source_kinds = {
        cf::PortfolioCashSource::Commercial,
        cf::PortfolioCashSource::LicensingRoyalty,
        cf::PortfolioCashSource::ExitSale,
    };
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "could not create success-participation parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write success-participation parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::SuccessParticipationConfig& config) {
    std::ostringstream output;
    cf::print_normalized_success_participation_config(output, config);
    return output.str();
}

[[nodiscard]] cf::SuccessParticipationConfig parse_text(
    const std::string& text) {
    std::istringstream input{text};
    return cf::parse_success_participation_config(input);
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
    const cf::SuccessParticipationConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_success_participation_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller flags, precision, width, fill, and locale");
    check(normalized.starts_with(
              "participation.model_version=0.1.0\n"
              "participation.label=strict reloadable participation term\n") &&
            normalized.find("participation.synthetic_inputs=true\n") !=
                std::string::npos &&
            normalized.find(
                "participation.selected_nonprincipal_cash_is_contractually_"
                "scalable=true\n") !=
                std::string::npos &&
            normalized.find(
                "participation.target_robust_npv_million=-9.9999999999999995e-21\n") !=
                std::string::npos &&
            normalized.find("eligible_source.count=3\n") !=
                std::string::npos &&
            normalized.find(
                "eligible_source.2.kind=licensing_royalty\n") !=
                std::string::npos &&
            normalized.find("eligible_source.3.kind=exit_sale\n") !=
                std::string::npos,
        "printer emits the canonical schema, classic punctuation, booleans, "
        "and max_digits10 decimals");

    write_text(path, normalized);
    const cf::SuccessParticipationConfig loaded =
        cf::load_success_participation_config(path);
    check(loaded.model_version == original.model_version &&
            loaded.scenario_label == original.scenario_label &&
            loaded.source_note == original.source_note &&
            loaded.synthetic_inputs == original.synthetic_inputs &&
            loaded.selected_nonprincipal_cash_is_contractually_scalable ==
                original.selected_nonprincipal_cash_is_contractually_scalable &&
            loaded.target_worst_expected_npv_million ==
                original.target_worst_expected_npv_million &&
            loaded.scalable_source_kinds == original.scalable_source_kinds,
        "the complete schema and max_digits10 target round-trip exactly "
        "through a file");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");

    const cf::SuccessParticipationConfig parsed = parse_text(normalized);
    check(normalized_config(parsed) == normalized,
        "stream parsing and file loading implement the same schema");
}

void test_comments_whitespace_and_bom(const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    const cf::SuccessParticipationConfig parsed = parse_text(
        bom + "# a full-line comment\r\n\r\n" + normalized);
    check(parsed.scenario_label == full_config().scenario_label,
        "comments, blank lines, CRLF, and a UTF-8 BOM at byte zero are accepted");

    std::string spaced = normalized;
    set_value(spaced, "participation.synthetic_inputs", "  true  ");
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
            (void)parse_text(normalized + "eligible_source.count=3\n");
        },
        "duplicate keys are rejected at read time");

    std::string invalid = normalized;
    remove_key(invalid, "eligible_source.3.kind");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing nested keys are rejected");

    invalid = normalized;
    remove_key(invalid, "participation.source_note");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "missing global keys are rejected");

    expect_invalid_argument(
        [] { (void)parse_text("participation.model_version 0.1.0\n"); },
        "non-key-value lines are rejected");
    expect_invalid_argument([] { (void)parse_text(""); },
        "an empty configuration is rejected");
}

void test_malformed_values(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "participation.target_robust_npv_million", "0x");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "malformed decimal targets are rejected");

    for (const std::string_view nonfinite : {"nan", "inf", "-inf"}) {
        invalid = normalized;
        set_value(invalid, "participation.target_robust_npv_million",
            std::string(nonfinite));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "non-finite decimal targets are rejected");
    }

    invalid = normalized;
    set_value(invalid, "eligible_source.count", "-1");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "negative source counts are rejected");

    invalid = normalized;
    set_value(invalid, "participation.synthetic_inputs", "yes");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "non-canonical booleans are rejected");
}

void test_assertions_and_text(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "participation.model_version", "9.9.9");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "unsupported model versions are rejected");

    invalid = normalized;
    set_value(invalid, "participation.synthetic_inputs", "false");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "v0.1 rejects non-synthetic inputs");

    invalid = normalized;
    set_value(invalid,
        "participation.selected_nonprincipal_cash_is_contractually_scalable",
        "false");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "v0.1 rejects a false contractual-scalability assertion");

    invalid = normalized;
    set_value(invalid, "participation.label", std::string(1'025U, 'x'));
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "free-form text length is bounded");

    invalid = normalized;
    set_value(invalid, "participation.source_note", "safe\x01unsafe");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "control characters in free-form text are rejected");
}

void test_source_count_and_taxonomy(const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "eligible_source.count", "0");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "zero selected sources are rejected");

    invalid = normalized;
    set_value(invalid, "eligible_source.count", "4");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "the three-source cap is enforced before schema expansion");

    invalid = normalized;
    set_value(invalid, "eligible_source.count", "2");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "count mismatches that leave extra indexed keys are rejected");

    invalid = normalized;
    set_value(invalid, "eligible_source.2.kind", "commercial");
    expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
        "duplicate selected source kinds are rejected");

    for (const std::string_view disallowed :
         {"recovery", "refinancing", "explicit-support", "sponsor-fee",
             "financing-fee", "licensing-royalty", "exit-sale"}) {
        invalid = normalized;
        set_value(invalid, "eligible_source.1.kind",
            std::string(disallowed));
        expect_invalid_argument([&invalid] { (void)parse_text(invalid); },
            "disallowed or non-canonical source-kind spellings are rejected");
    }
}

void test_resource_guards(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string long_line = normalized;
    set_value(long_line, "participation.source_note",
        std::string(4'097U, 'x'));
    expect_invalid_argument([&long_line] { (void)parse_text(long_line); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_success_participation_config(path); },
        "16 MiB file guard is enforced before parsing");

    const std::filesystem::path missing = path.string() + ".missing";
    (void)std::filesystem::remove(missing);
    expect_runtime_error(
        [&missing] { (void)cf::load_success_participation_config(missing); },
        "an unreadable path is reported as a runtime error");
}

void test_printer_rejects_invalid_config() {
    cf::SuccessParticipationConfig invalid = full_config();
    invalid.synthetic_inputs = false;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid] {
            cf::print_normalized_success_participation_config(output, invalid);
        },
        "printer rejects false synthetic-input assertions");
    check(output.str().empty(),
        "printer emits no partial output when intrinsic validation fails");

    invalid = full_config();
    invalid.selected_nonprincipal_cash_is_contractually_scalable = false;
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects false contractual-scalability assertions");

    invalid = full_config();
    invalid.target_worst_expected_npv_million =
        std::numeric_limits<double>::infinity();
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects non-finite targets");

    invalid = full_config();
    invalid.scalable_source_kinds.clear();
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects an empty selected-source set");

    invalid = full_config();
    invalid.scalable_source_kinds[1] = cf::PortfolioCashSource::Commercial;
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects duplicate selected-source kinds");

    invalid = full_config();
    invalid.scalable_source_kinds[0] = cf::PortfolioCashSource::Recovery;
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects source kinds outside the participation taxonomy");

    invalid = full_config();
    invalid.scenario_label = " padded";
    expect_invalid_argument(
        [&invalid] { (void)normalized_config(invalid); },
        "printer rejects text that would not round-trip exactly after trimming");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "success-participation-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_full_roundtrip_and_output_state(path);
        test_comments_whitespace_and_bom(normalized);
        test_strict_key_set(normalized);
        test_malformed_values(normalized);
        test_assertions_and_text(normalized);
        test_source_count_and_taxonomy(normalized);
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
                  << " success-participation-config test(s) failed\n";
        return 1;
    }
    std::cout << "all success-participation-config tests passed\n";
    return 0;
}
