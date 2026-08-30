// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/probability_polytope_config.hpp>

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
#include <vector>

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

[[nodiscard]] cf::ProbabilityPolytopeConfig full_config() {
    cf::ProbabilityPolytopeConfig config;
    config.scenario_label = "strict reloadable event probability polytope";
    config.source_note = "synthetic parser roundtrip values only";
    // Deliberately unsorted: normalized output must canonicalize by id.
    config.scenario_probabilities = {
        cf::ProbabilityPolytopeScenario{
            "culture-success-scaleup-loss", 0.0, 0.18, 1.0},
        cf::ProbabilityPolytopeScenario{
            "common-success", 0.0, 0.62, 1.0},
        cf::ProbabilityPolytopeScenario{
            "common-loss", 1.0e-20, 0.02, 1.0},
        cf::ProbabilityPolytopeScenario{
            "culture-loss-scaleup-success", 0.0, 0.18, 1.0},
    };
    config.events = {
        cf::ProbabilityEventConstraint{
            "scaleup-impairment",
            "Bioprocess-scaleup principal is impaired by the horizon",
            0.12,
            0.30,
            {"culture-success-scaleup-loss", "common-loss"}},
        cf::ProbabilityEventConstraint{
            "any-project-impairment",
            "At least one project has principal impairment by the horizon",
            0.30,
            0.50,
            {"culture-success-scaleup-loss", "common-loss",
                "culture-loss-scaleup-success"}},
        cf::ProbabilityEventConstraint{
            "common-process-shock",
            "Both projects are impaired by the shared process shock",
            0.01,
            0.10,
            {"common-loss"}},
        cf::ProbabilityEventConstraint{
            "culture-platform-impairment",
            "Culture-platform principal is impaired by the horizon",
            0.12,
            0.30,
            {"culture-loss-scaleup-success", "common-loss"}},
    };
    return config;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(
            "could not create probability polytope parser fixture");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error(
            "could not write probability polytope parser fixture");
    }
}

[[nodiscard]] std::string normalized_config(
    const cf::ProbabilityPolytopeConfig& config) {
    std::ostringstream output;
    cf::print_normalized_probability_polytope_config(output, config);
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

void expect_invalid_config(
    const cf::ProbabilityPolytopeConfig& config, std::string_view message) {
    expect_invalid_argument(
        [&config] {
            std::ostringstream output;
            cf::print_normalized_probability_polytope_config(output, config);
        },
        message);
}

void test_roundtrip_canonical_order_and_output_state(
    const std::filesystem::path& path) {
    const cf::ProbabilityPolytopeConfig original = full_config();
    std::ostringstream output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    output.imbue(caller_locale);
    output << std::fixed << std::hex << std::showbase << std::showpoint
           << std::showpos << std::uppercase << std::setprecision(6);
    output.fill('#');
    output.width(37);
    const std::ios_base::fmtflags caller_flags = output.flags();
    cf::print_normalized_probability_polytope_config(output, original);
    const std::string normalized = output.str();

    check(output.precision() == 6 && output.flags() == caller_flags &&
            output.width() == 37 && output.fill() == '#' &&
            output.getloc() == caller_locale,
        "normalized printer restores caller formatting and locale");
    check(normalized.starts_with(
              "polytope.model_version=0.2.0\n"
              "polytope.label=strict reloadable event probability polytope\n") &&
            normalized.find("polytope.synthetic_inputs=true\n") !=
                std::string::npos &&
            normalized.find("scenario.count=4\n") != std::string::npos &&
            normalized.find("scenario.1.id=common-loss\n") !=
                std::string::npos &&
            normalized.find(
                "scenario.1.lower_weight=9.9999999999999995e-21\n") !=
                std::string::npos &&
            normalized.find("event.count=4\n") != std::string::npos &&
            normalized.find("event.1.id=any-project-impairment\n") !=
                std::string::npos &&
            normalized.find("event.1.scenario.1.id=common-loss\n") !=
                std::string::npos &&
            normalized.find("event.4.id=scaleup-impairment\n") !=
                std::string::npos,
        "printer emits canonical schema, ordering, punctuation, and decimals");

    write_text(path, normalized);
    const cf::ProbabilityPolytopeConfig loaded =
        cf::load_probability_polytope_config(path);
    check(loaded.scenario_probabilities.size() == 4U &&
            loaded.scenario_probabilities[0].scenario_id == "common-loss" &&
            loaded.events.size() == 4U &&
            loaded.events[0].event_id == "any-project-impairment" &&
            loaded.events[0].scenario_ids.front() == "common-loss",
        "canonical scenario, event, and membership order reloads");
    check(normalized_config(loaded) == normalized,
        "print-load-print normalization is byte stable");
}

void test_repository_fixture() {
    const std::filesystem::path fixture =
        std::filesystem::path{__FILE__}.parent_path().parent_path() /
        "scenarios" / "two-project-event-polytope-synthetic.cfg";
    const cf::ProbabilityPolytopeConfig loaded =
        cf::load_probability_polytope_config(fixture);
    check(loaded.scenario_probabilities.size() == 4U &&
            loaded.events.size() == 4U &&
            loaded.events[0].event_id == "culture-platform-impairment" &&
            loaded.events[0].lower_probability == 0.12 &&
            loaded.events[2].event_id == "common-process-shock" &&
            loaded.events[2].scenario_ids.size() == 1U &&
            loaded.events[3].event_id == "any-project-impairment" &&
            loaded.events[3].upper_probability == 0.50,
        "checked-in synthetic event-polytope fixture uses the strict schema");
    const std::string first = normalized_config(loaded);
    check(first.find("scenario.1.lower_weight=0\n") != std::string::npos &&
            first.find("scenario.1.upper_weight=1\n") != std::string::npos &&
            first.find("event.1.id=any-project-impairment\n") !=
                std::string::npos,
        "fixture normalization makes broad component bounds and events visible");
}

void test_zero_events_roundtrip(const std::filesystem::path& path) {
    cf::ProbabilityPolytopeConfig config = full_config();
    config.events.clear();
    const std::string normalized = normalized_config(config);
    write_text(path, normalized);
    const cf::ProbabilityPolytopeConfig loaded =
        cf::load_probability_polytope_config(path);
    check(loaded.events.empty() &&
            normalized.find("event.count=0\n") != std::string::npos &&
            normalized_config(loaded) == normalized,
        "an event-free v0.1-compatible box-simplex round-trips exactly");
}

void test_comments_whitespace_and_bom(const std::filesystem::path& path,
    const std::string& normalized) {
    const std::string bom{"\xEF\xBB\xBF", 3U};
    write_text(path, bom + "# full-line comment\r\n\r\n" + normalized);
    check(cf::load_probability_polytope_config(path).events.size() == 4U,
        "comments, blank lines, CRLF, and a leading UTF-8 BOM are accepted");

    std::string spaced = normalized;
    set_value(spaced, "polytope.synthetic_inputs", "  true  ");
    write_text(path, spaced);
    check(cf::load_probability_polytope_config(path).synthetic_inputs,
        "leading and trailing key-value whitespace is ignored");

    std::string misplaced = normalized;
    const std::size_t second_line = misplaced.find('\n');
    misplaced.insert(second_line + 1U, bom);
    write_text(path, misplaced);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "UTF-8 BOM is rejected anywhere except byte zero");
}

void test_strict_key_set(const std::filesystem::path& path,
    const std::string& normalized) {
    write_text(path, normalized + "unknown.field=1\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "unknown keys are rejected");

    write_text(path, normalized + "event.count=4\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "duplicate keys are rejected at read time");

    std::string missing = normalized;
    remove_key(missing, "event.1.scenario.3.id");
    write_text(path, missing);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "missing event membership keys are rejected");

    missing = normalized;
    remove_key(missing, "polytope.source_note");
    write_text(path, missing);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "missing global keys are rejected");

    write_text(path, "polytope.model_version 0.2.0\n");
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "non-key-value lines are rejected");
}

void test_malformed_values(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string malformed = normalized;
    set_value(malformed, "event.1.lower_probability", "0.30x");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "malformed decimal numbers are rejected");

    malformed = normalized;
    set_value(malformed, "scenario.1.central_weight", "nan");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "non-finite decimal numbers are rejected");

    malformed = normalized;
    set_value(malformed, "event.count", "-4");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "malformed unsigned counts are rejected");

    malformed = normalized;
    set_value(malformed, "polytope.synthetic_inputs", "yes");
    write_text(path, malformed);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "non-canonical booleans are rejected");
}

void test_count_and_resource_guards(const std::filesystem::path& path,
    const std::string& normalized) {
    std::string oversized = normalized;
    set_value(oversized, "scenario.count", "10001");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "scenario count cap is enforced before schema expansion");

    oversized = normalized;
    set_value(oversized, "event.count", "257");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "event count cap is enforced before schema expansion");

    oversized = normalized;
    set_value(oversized, "scenario.count", "513");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "event-constrained scenario cap is enforced before schema expansion");

    oversized = normalized;
    set_value(oversized, "event.1.scenario.count", "4");
    write_text(path, oversized);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "declared event membership must be a proper subset");

    std::string long_line = normalized;
    set_value(long_line, "polytope.source_note", std::string(4'097U, 'x'));
    write_text(path, long_line);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "4096-byte line guard is enforced while reading");

    write_text(path, "x");
    std::filesystem::resize_file(path, 16U * 1024U * 1024U + 1U);
    expect_invalid_argument(
        [&path] { (void)cf::load_probability_polytope_config(path); },
        "16 MiB file guard is enforced before parsing");

    cf::ProbabilityPolytopeConfig membership_heavy;
    membership_heavy.scenario_label = "membership resource guard test";
    membership_heavy.source_note = "synthetic resource guard values only";
    membership_heavy.scenario_probabilities.reserve(512U);
    for (std::size_t scenario = 0U; scenario < 512U; ++scenario) {
        membership_heavy.scenario_probabilities.push_back(
            cf::ProbabilityPolytopeScenario{
                "scenario-" + std::to_string(scenario),
                0.0,
                1.0 / 512.0,
                1.0});
    }
    membership_heavy.events.reserve(129U);
    for (std::size_t event = 0U; event < 129U; ++event) {
        cf::ProbabilityEventConstraint constraint;
        constraint.event_id = "event-" + std::to_string(event);
        constraint.definition = "Synthetic membership resource test event";
        constraint.lower_probability = 0.0;
        constraint.upper_probability = 1.0;
        constraint.scenario_ids.reserve(511U);
        for (std::size_t scenario = 0U; scenario < 512U; ++scenario) {
            if (scenario != event) {
                constraint.scenario_ids.push_back(
                    "scenario-" + std::to_string(scenario));
            }
        }
        membership_heavy.events.push_back(std::move(constraint));
    }
    expect_invalid_config(membership_heavy,
        "aggregate event membership resource cap is enforced");
}

void test_intrinsic_scenario_validation() {
    cf::ProbabilityPolytopeConfig tolerated;
    tolerated.scenario_label = "normalization tolerance alignment";
    tolerated.source_note =
        "synthetic near-unit central-sum regression only";
    tolerated.scenario_probabilities = {
        {"s0", 0.10, 0.10, 0.10},
        {"s1", 0.0, 0.20, 1.0},
        {"s2", 0.0, 0.30, 1.0},
        {"s3", 0.0, 0.40000000000001, 1.0},
    };
    tolerated.events = {cf::ProbabilityEventConstraint{
        "middle-atoms", "two middle atoms exercise normalized validation",
        0.0, 1.0, {"s1", "s2"}}};
    check(!normalized_config(tolerated).empty(),
        "strict config accepts a normalized central value within its documented tolerance");

    cf::ProbabilityPolytopeConfig invalid = full_config();
    invalid.model_version = "9.9.9";
    expect_invalid_config(invalid, "unsupported model versions are rejected");

    invalid = full_config();
    invalid.synthetic_inputs = false;
    expect_invalid_config(invalid,
        "v0.2 rejects non-synthetic probability inputs");

    invalid = full_config();
    invalid.scenario_probabilities[1].scenario_id =
        invalid.scenario_probabilities[0].scenario_id;
    expect_invalid_config(invalid, "duplicate scenario ids are rejected");

    invalid = full_config();
    invalid.scenario_probabilities[1].scenario_id = "unsafe scenario";
    expect_invalid_config(invalid, "unsafe scenario ids are rejected");

    invalid = full_config();
    invalid.scenario_probabilities[0].lower_weight = 0.8;
    invalid.scenario_probabilities[0].upper_weight = 0.7;
    expect_invalid_config(invalid,
        "component bounds must contain their central weights");

    invalid = full_config();
    invalid.scenario_probabilities[0].central_weight = 0.19;
    expect_invalid_config(invalid, "central weights must sum to one");

    invalid = full_config();
    invalid.scenario_probabilities[0].lower_weight = 0.4;
    invalid.scenario_probabilities[1].lower_weight = 0.4;
    invalid.scenario_probabilities[2].lower_weight = 0.4;
    expect_invalid_config(invalid,
        "component lower bounds must admit a probability measure");

    invalid = full_config();
    for (cf::ProbabilityPolytopeScenario& scenario :
         invalid.scenario_probabilities) {
        scenario.upper_weight = 0.2;
    }
    expect_invalid_config(invalid,
        "component upper bounds must admit a probability measure");

    invalid = full_config();
    invalid.scenario_probabilities[1].upper_weight = 0.61;
    expect_invalid_config(invalid,
        "normalized central weights must lie inside component bounds");

    invalid = full_config();
    invalid.scenario_label = " leading";
    expect_invalid_config(invalid,
        "text with non-roundtrippable edge whitespace is rejected");
}

void test_intrinsic_event_validation() {
    cf::ProbabilityPolytopeConfig invalid = full_config();
    invalid.events[1].event_id = invalid.events[0].event_id;
    expect_invalid_config(invalid, "duplicate event ids are rejected");

    invalid = full_config();
    invalid.events[0].event_id = "unsafe event";
    expect_invalid_config(invalid, "unsafe event ids are rejected");

    invalid = full_config();
    invalid.events[0].definition.clear();
    expect_invalid_config(invalid,
        "empty human-readable event definitions are rejected");

    invalid = full_config();
    invalid.events[0].lower_probability = 0.31;
    expect_invalid_config(invalid,
        "event lower bounds may not exceed upper bounds");

    invalid = full_config();
    invalid.events[0].upper_probability = 1.01;
    expect_invalid_config(invalid, "event bounds must lie in [0,1]");

    invalid = full_config();
    invalid.events[0].scenario_ids.clear();
    expect_invalid_config(invalid,
        "empty event scenario subsets are rejected");

    invalid = full_config();
    invalid.events[0].scenario_ids = {
        "common-loss", "common-success",
        "culture-loss-scaleup-success", "culture-success-scaleup-loss"};
    expect_invalid_config(invalid,
        "full-universe event scenario subsets are rejected");

    invalid = full_config();
    invalid.events[0].scenario_ids[1] =
        invalid.events[0].scenario_ids[0];
    expect_invalid_config(invalid,
        "duplicate members inside an event are rejected");

    invalid = full_config();
    invalid.events[0].scenario_ids[0] = "unknown-scenario";
    expect_invalid_config(invalid, "unknown event scenario ids are rejected");

    invalid = full_config();
    invalid.events[0].scenario_ids = invalid.events[3].scenario_ids;
    expect_invalid_config(invalid,
        "duplicate event membership sets are rejected irrespective of order");

    invalid = full_config();
    invalid.events[3].lower_probability = 0.21;
    expect_invalid_config(invalid,
        "declared central measure must satisfy every event bound");

    invalid = full_config();
    invalid.events[0].definition = "trailing ";
    std::ostringstream output;
    expect_invalid_argument(
        [&output, &invalid] {
            cf::print_normalized_probability_polytope_config(output, invalid);
        },
        "printer validates event text before emitting an invalid config");
    check(output.str().empty(),
        "printer emits no partial output when intrinsic validation fails");
}

} // namespace

int main() {
    const std::filesystem::path path = std::filesystem::current_path() /
        "probability-polytope-config-parser-test.cfg";
    try {
        const std::string normalized = normalized_config(full_config());
        test_roundtrip_canonical_order_and_output_state(path);
        test_repository_fixture();
        test_zero_events_roundtrip(path);
        test_comments_whitespace_and_bom(path, normalized);
        test_strict_key_set(path, normalized);
        test_malformed_values(path, normalized);
        test_count_and_resource_guards(path, normalized);
        test_intrinsic_scenario_validation();
        test_intrinsic_event_validation();
        (void)std::filesystem::remove(path);
    } catch (const std::exception& error) {
        (void)std::filesystem::remove(path);
        std::cerr << "FAIL: unexpected test exception: " << error.what()
                  << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures
                  << " probability-polytope-config test(s) failed\n";
        return 1;
    }
    std::cout << "all probability-polytope-config tests passed\n";
    return 0;
}
