// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/probability_polytope_config.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumParsedScenarios = 10'000U;
constexpr std::size_t kMaximumEventPolytopeScenarios = 512U;
constexpr std::size_t kMaximumParsedEvents = 256U;
constexpr std::size_t kMaximumEventMemberships = 65'536U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;
constexpr double kWeightSumTolerance = 1.0e-12;
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};

struct RawValue {
    std::string value{};
    std::size_t line{0U};
};

using RawMap = std::unordered_map<std::string, RawValue>;

class OutputStateGuard {
public:
    explicit OutputStateGuard(std::ostream& output)
        : output_(output), flags_(output.flags()),
          precision_(output.precision()), width_(output.width()),
          fill_(output.fill()), locale_(output.getloc()) {}

    OutputStateGuard(const OutputStateGuard&) = delete;
    OutputStateGuard& operator=(const OutputStateGuard&) = delete;

    ~OutputStateGuard() noexcept {
        try {
            output_.flags(flags_);
            output_.precision(precision_);
            output_.width(width_);
            output_.fill(fill_);
            output_.imbue(locale_);
        } catch (...) {
            // Formatting restoration must not throw during stack unwinding.
        }
    }

private:
    std::ostream& output_;
    std::ios_base::fmtflags flags_;
    std::streamsize precision_;
    std::streamsize width_;
    char fill_;
    std::locale locale_;
};

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
};

[[nodiscard]] std::string_view trim_view(std::string_view value) noexcept {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1U);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1U);
    }
    return value;
}

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument(
        "probability polytope configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "probability polytope configuration is missing required key: " +
            key);
    }
    return iterator->second;
}

[[nodiscard]] double parse_double(const RawValue& raw) {
    double result{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end || !std::isfinite(result)) {
        parse_error(raw.line, "expected a finite decimal number");
    }
    return result;
}

[[nodiscard]] std::uint64_t parse_unsigned(const RawValue& raw) {
    std::uint64_t result{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end) {
        parse_error(raw.line, "expected a non-negative integer");
    }
    return result;
}

[[nodiscard]] std::size_t parse_size(const RawValue& raw) {
    const std::uint64_t parsed = parse_unsigned(raw);
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        if (parsed > static_cast<std::uint64_t>(
                         std::numeric_limits<std::size_t>::max())) {
            parse_error(raw.line, "integer is too large for this platform");
        }
    }
    return static_cast<std::size_t>(parsed);
}

[[nodiscard]] bool parse_bool(const RawValue& raw) {
    if (raw.value == "true") {
        return true;
    }
    if (raw.value == "false") {
        return false;
    }
    parse_error(raw.line, "expected true or false");
}

[[nodiscard]] bool is_ascii_alpha_numeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alpha_numeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alpha_numeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    if (trim_view(value).size() != value.size()) {
        throw std::invalid_argument(
            std::string(description) +
            " must not have leading or trailing whitespace");
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

[[nodiscard]] long double central_constraint_tolerance(
    long double value, long double bound) noexcept {
    return 64.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        std::max({1.0L, std::abs(value), std::abs(bound)});
}

[[nodiscard]] std::string scenario_key(
    std::size_t scenario, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string event_key(
    std::size_t event, std::string_view field) {
    return "event." + std::to_string(event + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string event_scenario_key(
    std::size_t event, std::size_t scenario, std::string_view field) {
    return "event." + std::to_string(event + 1U) + ".scenario." +
        std::to_string(scenario + 1U) + "." + std::string(field);
}

void validate_intrinsic_config(const ProbabilityPolytopeConfig& config) {
    if (config.model_version != kProbabilityPolytopeModelVersion) {
        throw std::invalid_argument(
            "probability polytope model_version does not match this engine");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "probability polytope v0.2 accepts synthetic inputs only");
    }
    require_safe_text(config.scenario_label,
        "probability polytope scenario_label");
    require_safe_text(config.source_note,
        "probability polytope source_note");
    if (config.scenario_probabilities.empty() ||
        config.scenario_probabilities.size() > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "probability polytope scenarios must be non-empty and bounded");
    }
    if (!config.events.empty() &&
        config.scenario_probabilities.size() >
            kMaximumEventPolytopeScenarios) {
        throw std::invalid_argument(
            "event-constrained probability polytope exceeds the 512-scenario guardrail");
    }
    if (config.events.size() > kMaximumParsedEvents) {
        throw std::invalid_argument(
            "probability polytope exceeds the 256-event guardrail");
    }

    std::unordered_map<std::string, long double> central_by_scenario;
    central_by_scenario.reserve(config.scenario_probabilities.size());
    CompensatedSum central_sum;
    CompensatedSum lower_sum;
    CompensatedSum upper_sum;
    for (const ProbabilityPolytopeScenario& scenario :
         config.scenario_probabilities) {
        if (!is_safe_identifier(scenario.scenario_id)) {
            throw std::invalid_argument(
                "probability polytope scenario id must be a safe identifier");
        }
        if (!central_by_scenario
                 .emplace(scenario.scenario_id,
                     static_cast<long double>(scenario.central_weight))
                 .second) {
            throw std::invalid_argument(
                "probability polytope scenario ids must be unique");
        }
        if (!std::isfinite(scenario.lower_weight) ||
            !std::isfinite(scenario.central_weight) ||
            !std::isfinite(scenario.upper_weight)) {
            throw std::invalid_argument(
                "probability polytope scenario probabilities must be finite");
        }
        if (scenario.lower_weight < 0.0 || scenario.lower_weight > 1.0 ||
            scenario.central_weight < 0.0 ||
            scenario.central_weight > 1.0 ||
            scenario.upper_weight < 0.0 || scenario.upper_weight > 1.0) {
            throw std::invalid_argument(
                "probability polytope scenario probabilities must lie in [0,1]");
        }
        if (scenario.lower_weight > scenario.central_weight ||
            scenario.central_weight > scenario.upper_weight) {
            throw std::invalid_argument(
                "probability polytope component bounds must contain their central weight");
        }
        central_sum.add(static_cast<long double>(scenario.central_weight));
        lower_sum.add(static_cast<long double>(scenario.lower_weight));
        upper_sum.add(static_cast<long double>(scenario.upper_weight));
    }

    const long double configured_central_sum = central_sum.value();
    if (std::abs(configured_central_sum - 1.0L) >
        static_cast<long double>(kWeightSumTolerance)) {
        throw std::invalid_argument(
            "probability polytope central weights must sum to one within tolerance");
    }
    if (lower_sum.value() > 1.0L) {
        throw std::invalid_argument(
            "probability polytope lower weights leave no feasible probability measure");
    }
    if (upper_sum.value() < 1.0L) {
        throw std::invalid_argument(
            "probability polytope upper weights leave no feasible probability measure");
    }
    for (const ProbabilityPolytopeScenario& scenario :
         config.scenario_probabilities) {
        const long double central =
            static_cast<long double>(scenario.central_weight) /
            configured_central_sum;
        const long double lower =
            static_cast<long double>(scenario.lower_weight);
        const long double upper =
            static_cast<long double>(scenario.upper_weight);
        if (central + central_constraint_tolerance(central, lower) < lower ||
            central - central_constraint_tolerance(central, upper) > upper) {
            throw std::invalid_argument(
                "normalized probability polytope central weight lies outside its component bounds");
        }
        central_by_scenario.at(scenario.scenario_id) = central;
    }

    std::unordered_set<std::string> event_ids;
    event_ids.reserve(config.events.size());
    std::set<std::vector<std::string>> event_membership_sets;
    std::size_t total_memberships = 0U;
    for (const ProbabilityEventConstraint& event : config.events) {
        if (!is_safe_identifier(event.event_id)) {
            throw std::invalid_argument(
                "probability event id must be a safe identifier");
        }
        if (!event_ids.insert(event.event_id).second) {
            throw std::invalid_argument(
                "probability event ids must be unique");
        }
        require_safe_text(event.definition, "probability event definition");
        if (!std::isfinite(event.lower_probability) ||
            !std::isfinite(event.upper_probability)) {
            throw std::invalid_argument(
                "probability event bounds must be finite");
        }
        if (event.lower_probability < 0.0 ||
            event.lower_probability > 1.0 ||
            event.upper_probability < 0.0 ||
            event.upper_probability > 1.0) {
            throw std::invalid_argument(
                "probability event bounds must lie in [0,1]");
        }
        if (event.lower_probability > event.upper_probability) {
            throw std::invalid_argument(
                "probability event lower probability exceeds upper probability");
        }
        if (event.scenario_ids.empty() ||
            event.scenario_ids.size() >= config.scenario_probabilities.size()) {
            throw std::invalid_argument(
                "probability events must be non-empty proper scenario subsets");
        }
        if (event.scenario_ids.size() >
            kMaximumEventMemberships - total_memberships) {
            throw std::invalid_argument(
                "probability event memberships exceed the resource guardrail");
        }
        total_memberships += event.scenario_ids.size();

        std::vector<std::string> canonical_members = event.scenario_ids;
        std::sort(canonical_members.begin(), canonical_members.end());
        if (std::adjacent_find(
                canonical_members.begin(), canonical_members.end()) !=
            canonical_members.end()) {
            throw std::invalid_argument(
                "probability event scenario members must be unique");
        }

        CompensatedSum event_central;
        for (const std::string& scenario_id : canonical_members) {
            const auto matching = central_by_scenario.find(scenario_id);
            if (matching == central_by_scenario.end()) {
                throw std::invalid_argument(
                    "probability event names an unknown scenario id");
            }
            event_central.add(matching->second);
        }
        if (!event_membership_sets.insert(canonical_members).second) {
            throw std::invalid_argument(
                "probability events must have unique scenario membership sets");
        }
        const long double central = event_central.value();
        const long double lower =
            static_cast<long double>(event.lower_probability);
        const long double upper =
            static_cast<long double>(event.upper_probability);
        if (central + central_constraint_tolerance(central, lower) < lower ||
            central - central_constraint_tolerance(central, upper) > upper) {
            throw std::invalid_argument(
                "normalized central event probability lies outside its event bounds");
        }
    }
}

[[nodiscard]] RawMap read_raw(const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "probability polytope configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open probability polytope configuration file: " +
            path.string());
    }

    RawMap raw;
    std::string line_text;
    std::size_t line_number = 0U;
    std::size_t bytes_read = 0U;
    while (std::getline(input, line_text)) {
        ++line_number;
        if (line_text.size() > kMaximumConfigLineBytes) {
            parse_error(line_number,
                "configuration line exceeds the 4096-byte guardrail");
        }
        bytes_read += line_text.size() + 1U;
        if (bytes_read > kMaximumConfigBytes) {
            parse_error(line_number,
                "configuration exceeds the 16 MiB guardrail");
        }
        std::string_view line{line_text};
        if (line_number == 1U && line.starts_with(kUtf8Bom)) {
            line.remove_prefix(kUtf8Bom.size());
        }
        if (line.find(kUtf8Bom) != std::string_view::npos) {
            parse_error(line_number,
                "UTF-8 BOM is permitted only at the start of the file");
        }
        line = trim_view(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            parse_error(line_number, "expected key=value");
        }
        const std::string_view key = trim_view(line.substr(0U, equals));
        const std::string_view value = trim_view(line.substr(equals + 1U));
        if (key.empty() || value.empty()) {
            parse_error(line_number, "key and value must not be empty");
        }
        const auto [iterator, inserted] = raw.emplace(
            std::string(key), RawValue{std::string(value), line_number});
        if (!inserted) {
            parse_error(line_number, "duplicate key: " + iterator->first);
        }
    }
    if (!input.eof()) {
        throw std::runtime_error(
            "failed while reading probability polytope configuration file: " +
            path.string());
    }
    return raw;
}

[[nodiscard]] std::size_t minimum_declared_key_count(
    std::size_t scenario_count, std::size_t event_count) {
    constexpr std::size_t global_key_count = 6U;
    constexpr std::size_t scenario_field_count = 4U;
    constexpr std::size_t event_field_count = 5U;
    if (scenario_count >
            (std::numeric_limits<std::size_t>::max() - global_key_count) /
                scenario_field_count ||
        event_count >
            (std::numeric_limits<std::size_t>::max() - global_key_count -
                scenario_field_count * scenario_count) /
                event_field_count) {
        throw std::invalid_argument(
            "probability polytope declared key count exceeds platform limits");
    }
    return global_key_count + scenario_field_count * scenario_count +
        event_field_count * event_count;
}

} // namespace

ProbabilityPolytopeConfig load_probability_polytope_config(
    const std::filesystem::path& path) {
    const RawMap raw = read_raw(path);
    const std::size_t scenario_count =
        parse_size(required(raw, "scenario.count"));
    const std::size_t event_count =
        parse_size(required(raw, "event.count"));
    if (scenario_count == 0U || scenario_count > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "parsed probability polytope scenario count must be between one and 10000");
    }
    if (event_count > kMaximumParsedEvents) {
        throw std::invalid_argument(
            "parsed probability polytope event count exceeds 256");
    }
    if (event_count != 0U &&
        scenario_count > kMaximumEventPolytopeScenarios) {
        throw std::invalid_argument(
            "event-constrained probability polytope exceeds the 512-scenario guardrail");
    }

    const std::size_t minimum_key_count =
        minimum_declared_key_count(scenario_count, event_count);
    if (raw.size() < minimum_key_count) {
        throw std::invalid_argument(
            "probability polytope declared counts require missing keys");
    }

    std::vector<std::size_t> event_member_counts(event_count, 0U);
    std::size_t total_event_memberships = 0U;
    for (std::size_t event = 0U; event < event_count; ++event) {
        const std::size_t member_count = parse_size(required(
            raw, event_key(event, "scenario.count")));
        if (member_count == 0U || member_count >= scenario_count) {
            throw std::invalid_argument(
                "parsed probability events must be non-empty proper scenario subsets");
        }
        if (member_count >
            kMaximumEventMemberships - total_event_memberships) {
            throw std::invalid_argument(
                "parsed probability event memberships exceed 65536");
        }
        event_member_counts[event] = member_count;
        total_event_memberships += member_count;
    }
    if (raw.size() < minimum_key_count + total_event_memberships) {
        throw std::invalid_argument(
            "probability polytope declared event memberships require missing keys");
    }

    std::unordered_set<std::string> expected{
        "polytope.model_version",
        "polytope.label",
        "polytope.source_note",
        "polytope.synthetic_inputs",
        "scenario.count",
        "event.count",
    };
    expected.reserve(minimum_key_count + total_event_memberships);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        expected.insert(scenario_key(scenario, "id"));
        expected.insert(scenario_key(scenario, "central_weight"));
        expected.insert(scenario_key(scenario, "lower_weight"));
        expected.insert(scenario_key(scenario, "upper_weight"));
    }
    for (std::size_t event = 0U; event < event_count; ++event) {
        expected.insert(event_key(event, "id"));
        expected.insert(event_key(event, "definition"));
        expected.insert(event_key(event, "lower_probability"));
        expected.insert(event_key(event, "upper_probability"));
        expected.insert(event_key(event, "scenario.count"));
        for (std::size_t member = 0U;
             member < event_member_counts[event]; ++member) {
            expected.insert(event_scenario_key(event, member, "id"));
        }
    }

    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    ProbabilityPolytopeConfig config;
    config.model_version = required(raw, "polytope.model_version").value;
    config.scenario_label = required(raw, "polytope.label").value;
    config.source_note = required(raw, "polytope.source_note").value;
    config.synthetic_inputs =
        parse_bool(required(raw, "polytope.synthetic_inputs"));
    config.scenario_probabilities.resize(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        ProbabilityPolytopeScenario& probability =
            config.scenario_probabilities[scenario];
        probability.scenario_id =
            required(raw, scenario_key(scenario, "id")).value;
        probability.central_weight = parse_double(
            required(raw, scenario_key(scenario, "central_weight")));
        probability.lower_weight = parse_double(
            required(raw, scenario_key(scenario, "lower_weight")));
        probability.upper_weight = parse_double(
            required(raw, scenario_key(scenario, "upper_weight")));
    }
    config.events.resize(event_count);
    for (std::size_t event = 0U; event < event_count; ++event) {
        ProbabilityEventConstraint& constraint = config.events[event];
        constraint.event_id = required(raw, event_key(event, "id")).value;
        constraint.definition =
            required(raw, event_key(event, "definition")).value;
        constraint.lower_probability = parse_double(
            required(raw, event_key(event, "lower_probability")));
        constraint.upper_probability = parse_double(
            required(raw, event_key(event, "upper_probability")));
        constraint.scenario_ids.resize(event_member_counts[event]);
        for (std::size_t member = 0U;
             member < event_member_counts[event]; ++member) {
            constraint.scenario_ids[member] = required(
                raw, event_scenario_key(event, member, "id")).value;
        }
    }

    validate_intrinsic_config(config);
    return config;
}

void print_normalized_probability_polytope_config(
    std::ostream& output, const ProbabilityPolytopeConfig& config) {
    validate_intrinsic_config(config);

    std::vector<ProbabilityPolytopeScenario> scenarios =
        config.scenario_probabilities;
    std::sort(scenarios.begin(), scenarios.end(),
        [](const ProbabilityPolytopeScenario& first,
            const ProbabilityPolytopeScenario& second) {
            return first.scenario_id < second.scenario_id;
        });
    std::vector<ProbabilityEventConstraint> events = config.events;
    for (ProbabilityEventConstraint& event : events) {
        std::sort(event.scenario_ids.begin(), event.scenario_ids.end());
    }
    std::sort(events.begin(), events.end(),
        [](const ProbabilityEventConstraint& first,
            const ProbabilityEventConstraint& second) {
            return first.event_id < second.event_id;
        });

    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "polytope.model_version=" << config.model_version << '\n';
    output << "polytope.label=" << config.scenario_label << '\n';
    output << "polytope.source_note=" << config.source_note << '\n';
    output << "polytope.synthetic_inputs=" << config.synthetic_inputs
           << '\n';
    output << "scenario.count=" << scenarios.size() << '\n';
    for (std::size_t scenario = 0U; scenario < scenarios.size(); ++scenario) {
        const ProbabilityPolytopeScenario& probability = scenarios[scenario];
        output << scenario_key(scenario, "id") << '='
               << probability.scenario_id << '\n';
        output << scenario_key(scenario, "central_weight") << '='
               << probability.central_weight << '\n';
        output << scenario_key(scenario, "lower_weight") << '='
               << probability.lower_weight << '\n';
        output << scenario_key(scenario, "upper_weight") << '='
               << probability.upper_weight << '\n';
    }
    output << "event.count=" << events.size() << '\n';
    for (std::size_t event = 0U; event < events.size(); ++event) {
        const ProbabilityEventConstraint& constraint = events[event];
        output << event_key(event, "id") << '=' << constraint.event_id
               << '\n';
        output << event_key(event, "definition") << '='
               << constraint.definition << '\n';
        output << event_key(event, "lower_probability") << '='
               << constraint.lower_probability << '\n';
        output << event_key(event, "upper_probability") << '='
               << constraint.upper_probability << '\n';
        output << event_key(event, "scenario.count") << '='
               << constraint.scenario_ids.size() << '\n';
        for (std::size_t member = 0U;
             member < constraint.scenario_ids.size(); ++member) {
            output << event_scenario_key(event, member, "id") << '='
                   << constraint.scenario_ids[member] << '\n';
        }
    }
}

} // namespace naturalehia::cellular_finance
