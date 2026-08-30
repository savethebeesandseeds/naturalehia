// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/staged_capital_config.hpp>

#include <charconv>
#include <cctype>
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

constexpr std::size_t kMaximumParsedPhases = 32U;
constexpr std::size_t kMaximumParsedCases = 256U;
constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;

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
        "staged-capital configuration line " + std::to_string(line) +
        ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "staged-capital configuration is missing required key: " + key);
    }
    return iterator->second;
}

[[nodiscard]] double parse_double(const RawValue& raw) {
    double result{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end) {
        parse_error(raw.line, "expected a decimal number");
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

[[nodiscard]] CertificationDecision parse_certification(
    const RawValue& raw) {
    if (raw.value == "certified") {
        return CertificationDecision::Certified;
    }
    if (raw.value == "final-failure") {
        return CertificationDecision::FinalFailure;
    }
    parse_error(raw.line, "expected certified or final-failure");
}

void add_global_keys(std::unordered_set<std::string>& expected) {
    static constexpr std::string_view keys[] = {
        "scenario.model_version",
        "scenario.label",
        "scenario.source_note",
        "scenario.currency_label",
        "scenario.monetary_basis",
        "scenario.synthetic_inputs",
        "facility.provider_commitment_million",
        "facility.sponsor_construction_commitment_million",
        "facility.provider_cost_share",
        "facility.annual_pik_rate",
        "facility.claim_cap_multiple",
        "facility.annual_commitment_fee_rate",
        "facility.upfront_fee_million",
        "facility.provider_hurdle_rate",
        "facility.sponsor_discount_rate",
        "facility.protected_workout_reserve_million",
        "phase.count",
        "case.count",
    };
    for (const std::string_view key : keys) {
        expected.emplace(key);
    }
}

[[nodiscard]] std::string phase_key(
    std::size_t phase_index, std::string_view field) {
    return "phase." + std::to_string(phase_index + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string case_key(
    std::size_t case_index, std::string_view field) {
    return "case." + std::to_string(case_index + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string case_phase_key(std::size_t case_index,
    std::size_t phase_index, std::string_view field) {
    return "case." + std::to_string(case_index + 1U) + ".phase." +
        std::to_string(phase_index + 1U) + "." + std::string(field);
}

[[nodiscard]] RawMap read_raw(const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "staged-capital configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "could not open staged-capital configuration file: " +
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
        if (line_number == 1U && line.size() >= 3U &&
            static_cast<unsigned char>(line[0]) == 0xEFU &&
            static_cast<unsigned char>(line[1]) == 0xBBU &&
            static_cast<unsigned char>(line[2]) == 0xBFU) {
            line.remove_prefix(3U);
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
            "failed while reading staged-capital configuration file: " +
            path.string());
    }
    return raw;
}

} // namespace

StagedCapitalConfig load_staged_capital_config(
    const std::filesystem::path& path) {
    const RawMap raw = read_raw(path);
    const std::size_t phase_count =
        parse_size(required(raw, "phase.count"));
    const std::size_t case_count =
        parse_size(required(raw, "case.count"));
    if (phase_count == 0U || phase_count > kMaximumParsedPhases) {
        throw std::invalid_argument(
            "parsed phase count must be between one and 32");
    }
    if (case_count == 0U || case_count > kMaximumParsedCases) {
        throw std::invalid_argument(
            "parsed case count must be between one and 256");
    }

    std::unordered_set<std::string> expected;
    add_global_keys(expected);
    for (std::size_t phase = 0U; phase < phase_count; ++phase) {
        expected.insert(phase_key(phase, "id"));
        expected.insert(phase_key(phase, "duration_months"));
        expected.insert(
            phase_key(phase, "provider_stage_cap_million"));
    }
    for (std::size_t case_index = 0U; case_index < case_count;
         ++case_index) {
        expected.insert(case_key(case_index, "id"));
        expected.insert(case_key(case_index, "weight"));
        expected.insert(
            case_key(case_index, "completion_value_million"));
        expected.insert(case_key(case_index, "recovery_value_million"));
        expected.insert(case_key(case_index, "recovery_delay_months"));
        expected.insert(
            case_key(case_index, "required_workout_cost_million"));
        for (std::size_t phase = 0U; phase < phase_count; ++phase) {
            expected.insert(case_phase_key(
                case_index, phase, "actual_eligible_cost_million"));
            expected.insert(case_phase_key(
                case_index, phase, "estimated_cost_to_complete_million"));
            expected.insert(
                case_phase_key(case_index, phase, "certification"));
            expected.insert(
                case_phase_key(case_index, phase, "provider_funds"));
        }
    }
    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const auto& key : expected) {
        (void)required(raw, key);
    }

    StagedCapitalConfig config;
    config.model_version = required(raw, "scenario.model_version").value;
    config.scenario_label = required(raw, "scenario.label").value;
    config.source_note = required(raw, "scenario.source_note").value;
    config.currency_label =
        required(raw, "scenario.currency_label").value;
    config.monetary_basis = required(raw, "scenario.monetary_basis").value;
    config.synthetic_inputs =
        parse_bool(required(raw, "scenario.synthetic_inputs"));
    config.terms.provider_commitment_million = parse_double(
        required(raw, "facility.provider_commitment_million"));
    config.terms.sponsor_construction_commitment_million = parse_double(
        required(raw,
            "facility.sponsor_construction_commitment_million"));
    config.terms.provider_cost_share =
        parse_double(required(raw, "facility.provider_cost_share"));
    config.terms.annual_pik_rate =
        parse_double(required(raw, "facility.annual_pik_rate"));
    config.terms.claim_cap_multiple =
        parse_double(required(raw, "facility.claim_cap_multiple"));
    config.terms.annual_commitment_fee_rate = parse_double(
        required(raw, "facility.annual_commitment_fee_rate"));
    config.terms.upfront_fee_million =
        parse_double(required(raw, "facility.upfront_fee_million"));
    config.terms.provider_hurdle_rate =
        parse_double(required(raw, "facility.provider_hurdle_rate"));
    config.terms.sponsor_discount_rate =
        parse_double(required(raw, "facility.sponsor_discount_rate"));
    config.terms.protected_workout_reserve_million = parse_double(
        required(raw, "facility.protected_workout_reserve_million"));

    config.phases.resize(phase_count);
    for (std::size_t phase = 0U; phase < phase_count; ++phase) {
        config.phases[phase].id = required(raw, phase_key(phase, "id")).value;
        config.phases[phase].duration_months =
            parse_size(required(raw, phase_key(phase, "duration_months")));
        config.phases[phase].provider_stage_cap_million = parse_double(
            required(raw,
                phase_key(phase, "provider_stage_cap_million")));
    }

    config.cases.resize(case_count);
    for (std::size_t case_index = 0U; case_index < case_count;
         ++case_index) {
        auto& scenario_case = config.cases[case_index];
        scenario_case.id = required(raw, case_key(case_index, "id")).value;
        scenario_case.weight =
            parse_double(required(raw, case_key(case_index, "weight")));
        scenario_case.completion_value_million = parse_double(required(
            raw, case_key(case_index, "completion_value_million")));
        scenario_case.recovery_value_million = parse_double(required(
            raw, case_key(case_index, "recovery_value_million")));
        scenario_case.recovery_delay_months = parse_size(required(
            raw, case_key(case_index, "recovery_delay_months")));
        scenario_case.required_workout_cost_million = parse_double(required(
            raw, case_key(case_index, "required_workout_cost_million")));
        scenario_case.phases.resize(phase_count);
        for (std::size_t phase = 0U; phase < phase_count; ++phase) {
            auto& phase_case = scenario_case.phases[phase];
            phase_case.actual_eligible_cost_million = parse_double(required(
                raw, case_phase_key(case_index, phase,
                         "actual_eligible_cost_million")));
            phase_case.estimated_cost_to_complete_million =
                parse_double(required(raw,
                    case_phase_key(case_index, phase,
                        "estimated_cost_to_complete_million")));
            phase_case.certification = parse_certification(required(raw,
                case_phase_key(case_index, phase, "certification")));
            phase_case.provider_funds = parse_bool(required(raw,
                case_phase_key(case_index, phase, "provider_funds")));
        }
    }
    validate_staged_capital_config(config);
    return config;
}

void print_normalized_staged_capital_config(
    std::ostream& output, const StagedCapitalConfig& config) {
    validate_staged_capital_config(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "scenario.model_version=" << config.model_version << '\n';
    output << "scenario.label=" << config.scenario_label << '\n';
    output << "scenario.source_note=" << config.source_note << '\n';
    output << "scenario.currency_label=" << config.currency_label << '\n';
    output << "scenario.monetary_basis=" << config.monetary_basis << '\n';
    output << "scenario.synthetic_inputs=" << config.synthetic_inputs << '\n';
    output << "facility.provider_commitment_million="
           << config.terms.provider_commitment_million << '\n';
    output << "facility.sponsor_construction_commitment_million="
           << config.terms.sponsor_construction_commitment_million << '\n';
    output << "facility.provider_cost_share="
           << config.terms.provider_cost_share << '\n';
    output << "facility.annual_pik_rate="
           << config.terms.annual_pik_rate << '\n';
    output << "facility.claim_cap_multiple="
           << config.terms.claim_cap_multiple << '\n';
    output << "facility.annual_commitment_fee_rate="
           << config.terms.annual_commitment_fee_rate << '\n';
    output << "facility.upfront_fee_million="
           << config.terms.upfront_fee_million << '\n';
    output << "facility.provider_hurdle_rate="
           << config.terms.provider_hurdle_rate << '\n';
    output << "facility.sponsor_discount_rate="
           << config.terms.sponsor_discount_rate << '\n';
    output << "facility.protected_workout_reserve_million="
           << config.terms.protected_workout_reserve_million << '\n';
    output << "phase.count=" << config.phases.size() << '\n';
    for (std::size_t phase = 0U; phase < config.phases.size(); ++phase) {
        output << phase_key(phase, "id") << '=' << config.phases[phase].id
               << '\n';
        output << phase_key(phase, "duration_months") << '='
               << config.phases[phase].duration_months << '\n';
        output << phase_key(phase, "provider_stage_cap_million") << '='
               << config.phases[phase].provider_stage_cap_million << '\n';
    }
    output << "case.count=" << config.cases.size() << '\n';
    for (std::size_t case_index = 0U; case_index < config.cases.size();
         ++case_index) {
        const auto& scenario_case = config.cases[case_index];
        output << case_key(case_index, "id") << '=' << scenario_case.id
               << '\n';
        output << case_key(case_index, "weight") << '='
               << scenario_case.weight << '\n';
        output << case_key(case_index, "completion_value_million") << '='
               << scenario_case.completion_value_million << '\n';
        output << case_key(case_index, "recovery_value_million") << '='
               << scenario_case.recovery_value_million << '\n';
        output << case_key(case_index, "recovery_delay_months") << '='
               << scenario_case.recovery_delay_months << '\n';
        output << case_key(case_index, "required_workout_cost_million")
               << '=' << scenario_case.required_workout_cost_million << '\n';
        for (std::size_t phase = 0U; phase < config.phases.size(); ++phase) {
            const auto& phase_case = scenario_case.phases[phase];
            output << case_phase_key(case_index, phase,
                          "actual_eligible_cost_million")
                   << '=' << phase_case.actual_eligible_cost_million << '\n';
            output << case_phase_key(case_index, phase,
                          "estimated_cost_to_complete_million")
                   << '=' << phase_case.estimated_cost_to_complete_million
                   << '\n';
            output << case_phase_key(case_index, phase, "certification")
                   << '=' << to_string(phase_case.certification) << '\n';
            output << case_phase_key(case_index, phase, "provider_funds")
                   << '=' << phase_case.provider_funds << '\n';
        }
    }
}

} // namespace naturalehia::cellular_finance
