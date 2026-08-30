// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
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

[[nodiscard]] cf::PortfolioAmbiguityConfig full_config() {
    cf::PortfolioAmbiguityConfig config;
    config.scenario_label = "strict reloadable probability envelope";
    config.source_note = "synthetic parser roundtrip values only";
    config.scenario_probabilities = {
        cf::ScenarioProbabilityBounds{
            "common-success", 1.0e-20, 0.62, 0.70},
        cf::ScenarioProbabilityBounds{
            "culture-loss-scaleup-success", 0.10, 0.18, 0.25},
        cf::ScenarioProbabilityBounds{
            "culture-success-scaleup-loss", 0.10, 0.18, 0.25},
        cf::ScenarioProbabilityBounds{
            "common-loss", 0.01, 0.02, 0.10},
    };
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "could not create portfolio ambiguity parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write portfolio ambiguity parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::PortfolioAmbiguityConfig& config) {
    std::ostringstream output;
    cf::print_normalized_portfolio_ambiguity_config(output, config);
    return output.str();
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
    const cf::PortfolioAmbiguityConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_portfolio_ambiguity_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller flags, precision, width, fill, and locale");
    check(normalized.starts_with(
              "ambiguity.model_version=0.1.0\n"
              "ambiguity.label=strict reloadable probability envelope\n") &&
            normalized.find("ambiguity.synthetic_inputs=true\n") !=
                std::string::npos &&
            normalized.find("scenario.count=4\n") != std::string::npos &&
            normalized.find("scenario.1.central_weight=0.62\n") !=
                std::string::npos &&
            normalized.find("scenario.1.lower_weight=9.9999999999999995e-21\n") !=
                std::string::npos,
        "printer emits canonical schema, classic punctuation, booleans, and max_digits10 decimals");

    write_text(path, normalized);
    const cf::PortfolioAmbiguityConfig loaded =
        cf::load_portfolio_ambiguity_config(path);
    check(loaded.scenario_probabilities.size() == 4U &&
            loaded.scenario_probabilities[0].scenario_id ==
                "common-success" &&
            loaded.scenario_probabilities[0].lower_weight ==
                original.scenario_probabilities[0].lower_weight &&
            loaded.scenario_probabilities[3].central_weight == 0.02,
        "full companion schema and max_digits10 doubles round-trip exactly");
    check(normalized_config(loaded) == normalized,
        "load-print normalization is idempotent");
}

void test_repository_fixture() {
    const std::filesystem::path fixture =
        std::filesystem::path{__FILE__}.parent_path().parent_path() /
        "scenarios" / "two-project-probability-envelope-synthetic.cfg";
    const cf::PortfolioAmbiguityConfig loaded =
        cf::load_portfolio_ambiguity_config(fixture);
    check(loaded.scenario_probabilities.size() == 4U &&
            loaded.scenario_probabilities[0].scenario_id ==
                "common-success" &&
            loaded.scenario_probabilities[0].central_weight == 0.62 &&
            loaded.scenario_probabilities[3].scenario_id == "common-loss" &&
            loaded.scenario_probabilities[3].upper_weight == 0.10,
        "the checked-in synthetic probability-envelope fixture uses the strict schema");
}

void test_zero_central_roundtrip(const std::filesystem::path& path) {
    cf::PortfolioAmbiguityConfig config = full_config();
    config.scenario_probabilities[0].central_weight = 0.64;
    config.scenario_probabilities[3].lower_weight = 0.0;
    config.scenario_probabilities[3].central_weight = 0.0;
    const std::string normalized = normalized_config(config);
    write_text(path, normalized);
    const cf::PortfolioAmbiguityConfig loaded =
        cf::load_portfolio_ambiguity_config(path);
    check(loaded.scenario_probabilities[3].central_weight == 0.0 &&
            normalized.find("scenario.4.central_weight=0\n") !=
                std::string::npos &&
            normalized_config(loaded) == normalized,
        "zero central probability atoms print and reload exactly");
}

void test_comments_whitespace_and_bom(const std::filesystem::path& path,
    const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    write_text(path, bom + "# a full-line comment\r\n\r\n" + normalized);
    const cf::PortfolioAmbiguityConfig loaded =
        cf::load_portfolio_ambiguity_config(path);
    check(loaded.scenario_label == full_config().scenario_label,
        "comments, blank lines, CRLF, and a UTF-8 BOM at byte zero are accepted");

    std::string spaced = normalized;
    set_value(spaced, "ambiguity.synthetic_inputs", "  true  ");
    write_text(path, spaced);
    check(cf::load_portfolio_ambiguity_config(path).synthetic_inputs,
        "leading and trailing key-value whitespace is ignored");

    std::string misplaced = normalized;
    const std::size_t second_line = misplaced.find('\n');
    misplaced.insert(second_line + 1U, bom);
    write_text(path, misplaced);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "UTF-8 BOM is rejected anywhere except byte zero");
}

void test_strict_key_set(const std::filesystem::path& path,
    const std::string& normalized) {
    write_text(path, normalized + "unknown.field=1\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "unknown keys are rejected");

    write_text(path, normalized + "scenario.count=4\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "duplicate keys are rejected at read time");

    std::string missing = normalized;
    remove_key(missing, "scenario.3.upper_weight");
    write_text(path, missing);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "missing nested keys are rejected");

    missing = normalized;
    remove_key(missing, "ambiguity.source_note");
    write_text(path, missing);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "missing global keys are rejected");

    write_text(path, "ambiguity.model_version 0.1.0\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "non-key-value lines are rejected");
}

void test_malformed_values(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string malformed = normalized;
    set_value(malformed, "scenario.1.central_weight", "0.62x");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "malformed decimal numbers are rejected");

    malformed = normalized;
    set_value(malformed, "scenario.1.central_weight", "nan");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "non-finite decimal numbers are rejected");

    malformed = normalized;
    set_value(malformed, "scenario.count", "-4");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "malformed unsigned counts are rejected");

    malformed = normalized;
    set_value(malformed, "ambiguity.synthetic_inputs", "yes");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "non-canonical booleans are rejected");
}

void test_count_and_resource_guards(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string oversized = normalized;
    set_value(oversized, "scenario.count", "10001");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "scenario count cap is enforced before schema expansion");

    oversized = normalized;
    set_value(oversized, "scenario.count", "10000");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "declared counts cannot force allocation for absent keys");

    std::string long_line = normalized;
    set_value(long_line, "ambiguity.source_note", std::string(4'097U, 'x'));
    write_text(path, long_line);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "16 MiB file guard is enforced before parsing");
}

void test_intrinsic_validation(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string invalid = normalized;
    set_value(invalid, "ambiguity.model_version", "9.9.9");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "unsupported model versions are rejected");

    invalid = normalized;
    set_value(invalid, "ambiguity.synthetic_inputs", "false");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "v0.1 rejects non-synthetic probability inputs");

    invalid = normalized;
    set_value(invalid, "scenario.2.id", "common-success");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "duplicate scenario identities are rejected");

    invalid = normalized;
    set_value(invalid, "scenario.2.id", "unsafe scenario");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "unsafe scenario identities are rejected");

    invalid = normalized;
    set_value(invalid, "scenario.1.lower_weight", "0.8");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "lower weights may not exceed upper weights");

    invalid = normalized;
    set_value(invalid, "scenario.4.central_weight", "0.03");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "central weights must sum to one");

    invalid = normalized;
    set_value(invalid, "scenario.1.lower_weight", "0.7");
    set_value(invalid, "scenario.2.lower_weight", "0.2");
    set_value(invalid, "scenario.3.lower_weight", "0.2");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "lower bounds must admit a probability measure");

    invalid = normalized;
    set_value(invalid, "scenario.1.upper_weight", "0.62");
    set_value(invalid, "scenario.2.upper_weight", "0.18");
    set_value(invalid, "scenario.3.upper_weight", "0.18");
    set_value(invalid, "scenario.4.upper_weight", "0.01");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "upper bounds must admit a probability measure");

    invalid = normalized;
    set_value(invalid, "scenario.1.upper_weight", "0.61");
    write_text(path, invalid);
    expect_invalid_argument(
        [&path] { (void)cf::load_portfolio_ambiguity_config(path); },
        "central weights must lie inside their component bounds");

    cf::PortfolioAmbiguityConfig tolerated = full_config();
    tolerated.scenario_probabilities[0].lower_weight = 0.62;
    tolerated.scenario_probabilities[0].upper_weight = 0.62;
    tolerated.scenario_probabilities[3].central_weight =
        0.02000000000001;
    const std::string tolerated_normalized = normalized_config(tolerated);
    write_text(path, tolerated_normalized);
    check(normalized_config(
              cf::load_portfolio_ambiguity_config(path)) ==
            tolerated_normalized,
        "near-unit central sums round-trip when normalization stays within the documented bound tolerance");

    cf::PortfolioAmbiguityConfig invalid_config = full_config();
    invalid_config.scenario_probabilities[0].central_weight = 0.63;
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid_config] {
            cf::print_normalized_portfolio_ambiguity_config(
                output, invalid_config);
        },
        "printer validates before emitting an invalid envelope");
    check(output.str().empty(),
        "printer emits no partial output when intrinsic validation fails");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "portfolio-ambiguity-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_full_roundtrip_and_output_state(path);
        test_repository_fixture();
        test_zero_central_roundtrip(path);
        test_comments_whitespace_and_bom(path, normalized);
        test_strict_key_set(path, normalized);
        test_malformed_values(path, normalized);
        test_count_and_resource_guards(path, normalized);
        test_intrinsic_validation(path, normalized);
        (void)std::filesystem::remove(path);
    } catch (const std::exception& error) {
        (void)std::filesystem::remove(path);
        std::cerr << "FAIL: unexpected test exception: " << error.what()
                  << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " portfolio-ambiguity-config test(s) failed\n";
        return 1;
    }
    std::cout << "all portfolio-ambiguity-config tests passed\n";
    return 0;
}
