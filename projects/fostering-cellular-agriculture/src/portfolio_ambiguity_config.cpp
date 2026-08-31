// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>

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
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumParsedScenarios = 10'000U;
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
        "portfolio ambiguity configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "portfolio ambiguity configuration is missing required key: " +
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

void validate_intrinsic_config(const PortfolioAmbiguityConfig& config) {
    if (config.model_version != kPortfolioAmbiguityModelVersion) {
        throw std::invalid_argument(
            "portfolio ambiguity model_version does not match this engine");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "portfolio ambiguity v0.1 accepts synthetic inputs only");
    }
    require_safe_text(config.scenario_label, "ambiguity scenario_label");
    require_safe_text(config.source_note, "ambiguity source_note");
    if (config.scenario_probabilities.empty() ||
        config.scenario_probabilities.size() > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "ambiguity scenario probabilities must be non-empty and bounded");
    }

    std::unordered_set<std::string> scenario_ids;
    scenario_ids.reserve(config.scenario_probabilities.size());
    CompensatedSum central_sum;
    CompensatedSum lower_sum;
    CompensatedSum upper_sum;
    for (const ScenarioProbabilityBounds& bounds :
         config.scenario_probabilities) {
        if (!is_safe_identifier(bounds.scenario_id)) {
            throw std::invalid_argument(
                "ambiguity scenario id must be a safe identifier");
        }
        if (!scenario_ids.insert(bounds.scenario_id).second) {
            throw std::invalid_argument(
                "ambiguity scenario ids must be unique");
        }
        if (!std::isfinite(bounds.lower_weight) ||
            !std::isfinite(bounds.central_weight) ||
            !std::isfinite(bounds.upper_weight)) {
            throw std::invalid_argument(
                "ambiguity probability bounds must be finite");
        }
        if (bounds.lower_weight < 0.0 || bounds.lower_weight > 1.0 ||
            bounds.upper_weight < 0.0 || bounds.upper_weight > 1.0 ||
            bounds.central_weight < 0.0) {
            throw std::invalid_argument(
                "ambiguity probabilities are outside their supported ranges");
        }
        if (bounds.lower_weight > bounds.upper_weight) {
            throw std::invalid_argument(
                "ambiguity lower weight exceeds upper weight");
        }
        central_sum.add(static_cast<long double>(bounds.central_weight));
        lower_sum.add(static_cast<long double>(bounds.lower_weight));
        upper_sum.add(static_cast<long double>(bounds.upper_weight));
    }

    const long double configured_sum = central_sum.value();
    if (std::abs(configured_sum - 1.0L) >
        static_cast<long double>(kWeightSumTolerance)) {
        throw std::invalid_argument(
            "ambiguity central weights must sum to one within tolerance");
    }
    if (lower_sum.value() - 1.0L >
        static_cast<long double>(kWeightSumTolerance)) {
        throw std::invalid_argument(
            "ambiguity lower weights leave no feasible probability measure");
    }
    if (1.0L - upper_sum.value() >
        static_cast<long double>(kWeightSumTolerance)) {
        throw std::invalid_argument(
            "ambiguity upper weights leave no feasible probability measure");
    }
    for (const ScenarioProbabilityBounds& bounds :
         config.scenario_probabilities) {
        const long double normalized_central =
            static_cast<long double>(bounds.central_weight) / configured_sum;
        const long double lower =
            static_cast<long double>(bounds.lower_weight);
        const long double upper =
            static_cast<long double>(bounds.upper_weight);
        if (normalized_central <
                lower - central_constraint_tolerance(
                            normalized_central, lower) ||
            normalized_central >
                upper + central_constraint_tolerance(
                            normalized_central, upper)) {
            throw std::invalid_argument(
                "normalized ambiguity central weight lies outside its bounds");
        }
    }
}

[[nodiscard]] RawMap read_raw(const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "portfolio ambiguity configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open portfolio ambiguity configuration file: " +
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
            "failed while reading portfolio ambiguity configuration file: " +
            path.string());
    }
    return raw;
}

} // namespace

PortfolioAmbiguityConfig load_portfolio_ambiguity_config(
    const std::filesystem::path& path) {
    const RawMap raw = read_raw(path);
    const std::size_t scenario_count =
        parse_size(required(raw, "scenario.count"));
    if (scenario_count == 0U ||
        scenario_count > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "parsed ambiguity scenario count must be between one and 10000");
    }

    constexpr std::size_t global_key_count = 5U;
    constexpr std::size_t scenario_field_count = 4U;
    if (raw.size() < global_key_count ||
        scenario_count >
            (raw.size() - global_key_count) / scenario_field_count) {
        throw std::invalid_argument(
            "portfolio ambiguity configuration declared count requires missing keys");
    }

    std::unordered_set<std::string> expected{
        "ambiguity.model_version",
        "ambiguity.label",
        "ambiguity.source_note",
        "ambiguity.synthetic_inputs",
        "scenario.count",
    };
    expected.reserve(global_key_count + scenario_field_count * scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        expected.insert(scenario_key(scenario, "id"));
        expected.insert(scenario_key(scenario, "central_weight"));
        expected.insert(scenario_key(scenario, "lower_weight"));
        expected.insert(scenario_key(scenario, "upper_weight"));
    }

    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    PortfolioAmbiguityConfig config;
    config.model_version =
        required(raw, "ambiguity.model_version").value;
    config.scenario_label = required(raw, "ambiguity.label").value;
    config.source_note = required(raw, "ambiguity.source_note").value;
    config.synthetic_inputs =
        parse_bool(required(raw, "ambiguity.synthetic_inputs"));
    config.scenario_probabilities.resize(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        ScenarioProbabilityBounds& bounds =
            config.scenario_probabilities[scenario];
        bounds.scenario_id = required(raw, scenario_key(scenario, "id")).value;
        bounds.central_weight = parse_double(
            required(raw, scenario_key(scenario, "central_weight")));
        bounds.lower_weight = parse_double(
            required(raw, scenario_key(scenario, "lower_weight")));
        bounds.upper_weight = parse_double(
            required(raw, scenario_key(scenario, "upper_weight")));
    }

    validate_intrinsic_config(config);
    return config;
}

void print_normalized_portfolio_ambiguity_config(
    std::ostream& output, const PortfolioAmbiguityConfig& config) {
    validate_intrinsic_config(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "ambiguity.model_version=" << config.model_version << '\n';
    output << "ambiguity.label=" << config.scenario_label << '\n';
    output << "ambiguity.source_note=" << config.source_note << '\n';
    output << "ambiguity.synthetic_inputs=" << config.synthetic_inputs
           << '\n';
    output << "scenario.count=" << config.scenario_probabilities.size()
           << '\n';
    for (std::size_t scenario = 0U;
         scenario < config.scenario_probabilities.size(); ++scenario) {
        const ScenarioProbabilityBounds& bounds =
            config.scenario_probabilities[scenario];
        output << scenario_key(scenario, "id") << '=' << bounds.scenario_id
               << '\n';
        output << scenario_key(scenario, "central_weight") << '='
               << bounds.central_weight << '\n';
        output << scenario_key(scenario, "lower_weight") << '='
               << bounds.lower_weight << '\n';
        output << scenario_key(scenario, "upper_weight") << '='
               << bounds.upper_weight << '\n';
    }
}

} // namespace naturalehia::cellular_finance
