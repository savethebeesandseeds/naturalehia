// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_config.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <istream>
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

constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;
constexpr std::size_t kMaximumTranches = 128U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMinimumTrancheNotionalMillion = 1.0e-6;
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};

struct RawValue {
    std::string value{};
    std::size_t line{0U};
    bool had_surrounding_whitespace{false};
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
            // Stream-state restoration must not throw while unwinding.
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

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength ||
        std::isspace(static_cast<unsigned char>(value.front())) != 0 ||
        std::isspace(static_cast<unsigned char>(value.back())) != 0 ||
        value.find(kUtf8Bom) != std::string_view::npos) {
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

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument("capital-stack configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "capital-stack configuration is missing required key: " + key);
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

[[nodiscard]] std::size_t parse_size(const RawValue& raw) {
    std::uint64_t parsed{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, parsed);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end ||
        parsed > static_cast<std::uint64_t>(
                     std::numeric_limits<std::size_t>::max())) {
        parse_error(raw.line, "expected a bounded non-negative integer");
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

[[nodiscard]] std::string parse_text(const RawValue& raw) {
    if (raw.had_surrounding_whitespace) {
        parse_error(raw.line,
            "text values must not begin or end with whitespace");
    }
    return raw.value;
}

[[nodiscard]] RawMap read_raw(std::istream& input) {
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
        if (bytes_read >= kMaximumConfigBytes ||
            line_text.size() + 1U > kMaximumConfigBytes - bytes_read) {
            parse_error(line_number,
                "configuration exceeds the 16 MiB guardrail");
        }
        bytes_read += line_text.size() + 1U;
        std::string_view line{line_text};
        if (line_number == 1U && line.starts_with(kUtf8Bom)) {
            line.remove_prefix(kUtf8Bom.size());
        }
        if (line.find(kUtf8Bom) != std::string_view::npos) {
            parse_error(line_number,
                "UTF-8 BOM is permitted only at the start of the file");
        }
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        const std::string_view trimmed = trim_view(line);
        if (trimmed.empty() || trimmed.front() == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            parse_error(line_number, "expected key=value");
        }
        const std::string_view key = trim_view(line.substr(0U, equals));
        const std::string_view untrimmed_value = line.substr(equals + 1U);
        const std::string_view value = trim_view(untrimmed_value);
        if (key.empty() || value.empty()) {
            parse_error(line_number, "key and value must not be empty");
        }
        const auto [iterator, inserted] = raw.emplace(std::string(key),
            RawValue{std::string(value), line_number,
                value.size() != untrimmed_value.size()});
        if (!inserted) {
            parse_error(line_number, "duplicate key: " + iterator->first);
        }
    }
    if (!input.eof()) {
        throw std::runtime_error(
            "failed while reading capital-stack configuration");
    }
    return raw;
}

[[nodiscard]] std::string tranche_key(
    std::size_t index, std::string_view field) {
    return "tranche." + std::to_string(index + 1U) + "." +
        std::string(field);
}

void validate_intrinsic(const CapitalStackConfig& config) {
    const bool is_v01 =
        config.model_version == kCapitalStackLegacyModelVersion;
    const bool is_v02 = config.model_version == kCapitalStackModelVersion;
    if (!is_v01 && !is_v02) {
        throw std::invalid_argument(
            "unsupported capital-stack model version");
    }
    require_safe_text(config.scenario_label, "capital-stack label");
    require_safe_text(config.source_note, "capital-stack source note");
    if (!config.synthetic_inputs ||
        !config.subscription_reserve_is_zero_yield_and_lossless ||
        !config.undrawn_commitment_cancels_and_returns_only_at_horizon ||
        !config.pool_costs_are_additional_pro_rata_calls ||
        !config.principal_cash_is_paid_most_senior_first ||
        !config.nonprincipal_cash_is_paid_to_caps_then_residual ||
        !config.tranching_does_not_change_project_cash_or_gross_loss ||
        config.premium_discount_or_fair_value_is_claimed) {
        throw std::invalid_argument(
            "capital-stack shared assertions have an unsupported value");
    }
    if (is_v01 &&
        (!config.aggregate_commitment_is_fully_funded_at_par_at_month_zero ||
            config.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero ||
            config.buyer_direct_costs_are_additional_pro_rata_calls ||
            config.principal_base_cash_above_issued_principal_is_nonprincipal ||
            config.principal_limit_capacity_difference_is_reported_without_valuation_claim)) {
        throw std::invalid_argument(
            "capital-stack v0.1 assertions have an unsupported value");
    }
    if (is_v02 &&
        (config.aggregate_commitment_is_fully_funded_at_par_at_month_zero ||
            !config.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero ||
            !config.buyer_direct_costs_are_additional_pro_rata_calls ||
            !config.principal_base_cash_above_issued_principal_is_nonprincipal ||
            !config.principal_limit_capacity_difference_is_reported_without_valuation_claim)) {
        throw std::invalid_argument(
            "capital-stack v0.2 assertions have an unsupported value");
    }
    if (!std::isfinite(config.underlying_success_participation_fraction) ||
        config.underlying_success_participation_fraction < 0.0 ||
        config.underlying_success_participation_fraction > 1.0) {
        throw std::invalid_argument(
            "capital-stack participation fraction must lie in [0,1]");
    }
    if (config.tranches.size() < 2U ||
        config.tranches.size() > kMaximumTranches) {
        throw std::invalid_argument(
            "capital-stack tranche count must be between two and 128");
    }
    double expected_attachment = 0.0;
    std::unordered_set<std::string> ids;
    ids.reserve(config.tranches.size());
    std::size_t residuals = 0U;
    for (std::size_t index = 0U; index < config.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& tranche = config.tranches[index];
        if (!is_safe_identifier(tranche.id) ||
            !ids.emplace(tranche.id).second ||
            !std::isfinite(tranche.attachment_million) ||
            !std::isfinite(tranche.detachment_million) ||
            tranche.attachment_million < 0.0 ||
            tranche.attachment_million != expected_attachment ||
            tranche.detachment_million - tranche.attachment_million <
                kMinimumTrancheNotionalMillion ||
            !std::isfinite(tranche.priority_nonprincipal_cap_million) ||
            tranche.priority_nonprincipal_cap_million < 0.0 ||
            !std::isfinite(tranche.annual_physical_hurdle_rate) ||
            tranche.annual_physical_hurdle_rate < 0.0 ||
            tranche.annual_physical_hurdle_rate > 10.0) {
            throw std::invalid_argument(
                "capital-stack contains invalid or non-contiguous tranche terms");
        }
        expected_attachment = tranche.detachment_million;
        if (tranche.is_first_loss_residual) {
            ++residuals;
            if (index != 0U ||
                tranche.priority_nonprincipal_cap_million != 0.0) {
                throw std::invalid_argument(
                    "capital-stack residual must be the first tranche with zero cap");
            }
        } else if (index == 0U) {
            throw std::invalid_argument(
                "capital-stack first tranche must be the residual");
        }
    }
    if (residuals != 1U) {
        throw std::invalid_argument(
            "capital-stack requires exactly one residual tranche");
    }
}

[[nodiscard]] CapitalStackConfig parse_raw(const RawMap& raw) {
    static const std::unordered_set<std::string> common_fixed_keys{
        "capital_stack.model_version",
        "capital_stack.label",
        "capital_stack.source_note",
        "capital_stack.synthetic_inputs",
        "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero",
        "capital_stack.subscription_reserve_is_zero_yield_and_lossless",
        "capital_stack.undrawn_commitment_cancels_and_returns_only_at_horizon",
        "capital_stack.pool_costs_are_additional_pro_rata_calls",
        "capital_stack.principal_cash_is_paid_most_senior_first",
        "capital_stack.nonprincipal_cash_is_paid_to_caps_then_residual",
        "capital_stack.tranching_does_not_change_project_cash_or_gross_loss",
        "capital_stack.premium_discount_or_fair_value_is_claimed",
        "capital_stack.underlying_success_participation_fraction",
        "tranche.count",
    };
    static const std::unordered_set<std::string> v02_fixed_keys{
        "capital_stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero",
        "capital_stack.buyer_direct_costs_are_additional_pro_rata_calls",
        "capital_stack.principal_base_cash_above_issued_principal_is_nonprincipal",
        "capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim",
    };
    const std::string& model_version =
        required(raw, "capital_stack.model_version").value;
    const bool is_v02 = model_version == kCapitalStackModelVersion;
    if (model_version != kCapitalStackLegacyModelVersion && !is_v02) {
        throw std::invalid_argument(
            "unsupported capital-stack model version");
    }
    std::unordered_set<std::string> expected = common_fixed_keys;
    if (is_v02) {
        expected.insert(v02_fixed_keys.begin(), v02_fixed_keys.end());
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }
    const std::size_t count = parse_size(required(raw, "tranche.count"));
    if (count > kMaximumTranches) {
        throw std::invalid_argument(
            "capital-stack tranche count exceeds the resource bound");
    }
    for (std::size_t index = 0U; index < count; ++index) {
        expected.emplace(tranche_key(index, "id"));
        expected.emplace(tranche_key(index, "attachment_million"));
        expected.emplace(tranche_key(index, "detachment_million"));
        expected.emplace(
            tranche_key(index, "priority_nonprincipal_cap_million"));
        expected.emplace(
            tranche_key(index, "annual_physical_hurdle_rate"));
        expected.emplace(tranche_key(index, "is_first_loss_residual"));
    }
    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    CapitalStackConfig config;
    config.model_version =
        required(raw, "capital_stack.model_version").value;
    config.scenario_label =
        parse_text(required(raw, "capital_stack.label"));
    config.source_note =
        parse_text(required(raw, "capital_stack.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "capital_stack.synthetic_inputs"));
    config.aggregate_commitment_is_fully_funded_at_par_at_month_zero =
        parse_bool(required(raw,
            "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero"));
    config.subscription_reserve_is_zero_yield_and_lossless = parse_bool(required(
        raw, "capital_stack.subscription_reserve_is_zero_yield_and_lossless"));
    config.undrawn_commitment_cancels_and_returns_only_at_horizon =
        parse_bool(required(raw,
            "capital_stack.undrawn_commitment_cancels_and_returns_only_at_horizon"));
    config.pool_costs_are_additional_pro_rata_calls = parse_bool(required(
        raw, "capital_stack.pool_costs_are_additional_pro_rata_calls"));
    config.principal_cash_is_paid_most_senior_first = parse_bool(required(
        raw, "capital_stack.principal_cash_is_paid_most_senior_first"));
    config.nonprincipal_cash_is_paid_to_caps_then_residual = parse_bool(required(
        raw, "capital_stack.nonprincipal_cash_is_paid_to_caps_then_residual"));
    config.tranching_does_not_change_project_cash_or_gross_loss =
        parse_bool(required(raw,
            "capital_stack.tranching_does_not_change_project_cash_or_gross_loss"));
    config.premium_discount_or_fair_value_is_claimed = parse_bool(required(
        raw, "capital_stack.premium_discount_or_fair_value_is_claimed"));
    if (is_v02) {
        config.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero =
            parse_bool(required(raw,
                "capital_stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero"));
        config.buyer_direct_costs_are_additional_pro_rata_calls = parse_bool(
            required(raw,
                "capital_stack.buyer_direct_costs_are_additional_pro_rata_calls"));
        config.principal_base_cash_above_issued_principal_is_nonprincipal =
            parse_bool(required(raw,
                "capital_stack.principal_base_cash_above_issued_principal_is_nonprincipal"));
        config.principal_limit_capacity_difference_is_reported_without_valuation_claim =
            parse_bool(required(raw,
                "capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim"));
    }
    config.underlying_success_participation_fraction = parse_double(required(
        raw, "capital_stack.underlying_success_participation_fraction"));
    config.tranches.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        CapitalStackTrancheConfig tranche;
        tranche.id = parse_text(required(raw, tranche_key(index, "id")));
        tranche.attachment_million = parse_double(
            required(raw, tranche_key(index, "attachment_million")));
        tranche.detachment_million = parse_double(
            required(raw, tranche_key(index, "detachment_million")));
        tranche.priority_nonprincipal_cap_million = parse_double(required(
            raw, tranche_key(index, "priority_nonprincipal_cap_million")));
        tranche.annual_physical_hurdle_rate = parse_double(required(
            raw, tranche_key(index, "annual_physical_hurdle_rate")));
        tranche.is_first_loss_residual = parse_bool(
            required(raw, tranche_key(index, "is_first_loss_residual")));
        config.tranches.push_back(std::move(tranche));
    }
    validate_intrinsic(config);
    return config;
}

} // namespace

CapitalStackConfig parse_capital_stack_config(std::istream& input) {
    return parse_raw(read_raw(input));
}

CapitalStackConfig load_capital_stack_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "capital-stack configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open capital-stack configuration file: " +
            path.string());
    }
    try {
        return parse_capital_stack_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading capital-stack configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_capital_stack_config(
    std::ostream& output, const CapitalStackConfig& config) {
    validate_intrinsic(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "capital_stack.model_version=" << config.model_version << '\n';
    output << "capital_stack.label=" << config.scenario_label << '\n';
    output << "capital_stack.source_note=" << config.source_note << '\n';
    output << "capital_stack.synthetic_inputs=" << config.synthetic_inputs
           << '\n';
    output
        << "capital_stack.aggregate_commitment_is_fully_funded_at_par_at_month_zero="
        << config.aggregate_commitment_is_fully_funded_at_par_at_month_zero
        << '\n';
    output << "capital_stack.subscription_reserve_is_zero_yield_and_lossless="
           << config.subscription_reserve_is_zero_yield_and_lossless << '\n';
    output
        << "capital_stack.undrawn_commitment_cancels_and_returns_only_at_horizon="
        << config.undrawn_commitment_cancels_and_returns_only_at_horizon
        << '\n';
    output << "capital_stack.pool_costs_are_additional_pro_rata_calls="
           << config.pool_costs_are_additional_pro_rata_calls << '\n';
    output << "capital_stack.principal_cash_is_paid_most_senior_first="
           << config.principal_cash_is_paid_most_senior_first << '\n';
    output << "capital_stack.nonprincipal_cash_is_paid_to_caps_then_residual="
           << config.nonprincipal_cash_is_paid_to_caps_then_residual << '\n';
    output
        << "capital_stack.tranching_does_not_change_project_cash_or_gross_loss="
        << config.tranching_does_not_change_project_cash_or_gross_loss << '\n';
    output << "capital_stack.premium_discount_or_fair_value_is_claimed="
           << config.premium_discount_or_fair_value_is_claimed << '\n';
    if (config.model_version == kCapitalStackModelVersion) {
        output
            << "capital_stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero="
            << config.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero
            << '\n';
        output
            << "capital_stack.buyer_direct_costs_are_additional_pro_rata_calls="
            << config.buyer_direct_costs_are_additional_pro_rata_calls << '\n';
        output
            << "capital_stack.principal_base_cash_above_issued_principal_is_nonprincipal="
            << config.principal_base_cash_above_issued_principal_is_nonprincipal
            << '\n';
        output
            << "capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim="
            << config.principal_limit_capacity_difference_is_reported_without_valuation_claim
            << '\n';
    }
    output << "capital_stack.underlying_success_participation_fraction="
           << config.underlying_success_participation_fraction << '\n';
    output << "tranche.count=" << config.tranches.size() << '\n';
    for (std::size_t index = 0U; index < config.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& tranche = config.tranches[index];
        output << tranche_key(index, "id") << '=' << tranche.id << '\n';
        output << tranche_key(index, "attachment_million") << '='
               << tranche.attachment_million << '\n';
        output << tranche_key(index, "detachment_million") << '='
               << tranche.detachment_million << '\n';
        output << tranche_key(index, "priority_nonprincipal_cap_million")
               << '=' << tranche.priority_nonprincipal_cap_million << '\n';
        output << tranche_key(index, "annual_physical_hurdle_rate") << '='
               << tranche.annual_physical_hurdle_rate << '\n';
        output << tranche_key(index, "is_first_loss_residual") << '='
               << tranche.is_first_loss_residual << '\n';
    }
}

} // namespace naturalehia::cellular_finance
