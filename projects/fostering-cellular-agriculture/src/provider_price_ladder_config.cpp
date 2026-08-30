// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_price_ladder_config.hpp>

#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <optional>
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
        "provider-price-ladder configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "provider-price-ladder configuration is missing required key: " +
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

[[nodiscard]] std::optional<double> parse_optional_double(
    const RawValue& raw) {
    if (raw.value == "none") {
        return std::nullopt;
    }
    return parse_double(raw);
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

[[nodiscard]] ProviderPriceCoverageSelection parse_coverage_selection(
    const RawValue& raw) {
    if (raw.value == "explicit") {
        return ProviderPriceCoverageSelection::ExplicitCoverageFraction;
    }
    if (raw.value == "reported-investor-target-passing") {
        return ProviderPriceCoverageSelection::
            ReportedInvestorTargetPassingFraction;
    }
    parse_error(raw.line,
        "expected explicit or reported-investor-target-passing");
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
        const std::string_view trimmed_line = trim_view(line);
        if (trimmed_line.empty() || trimmed_line.front() == '#') {
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
        const auto [iterator, inserted] = raw.emplace(
            std::string(key), RawValue{std::string(value), line_number,
                                  value.size() != untrimmed_value.size()});
        if (!inserted) {
            parse_error(line_number, "duplicate key: " + iterator->first);
        }
    }
    if (!input.eof()) {
        throw std::runtime_error(
            "failed while reading provider-price-ladder configuration");
    }
    return raw;
}

[[nodiscard]] ProviderPriceLadderConfig parse_raw(const RawMap& raw) {
    static const std::unordered_set<std::string> expected{
        "provider_price.model_version",
        "provider_price.label",
        "provider_price.source_note",
        "provider_price.synthetic_inputs",
        "provider_price.coverage_selection",
        "provider_price.explicit_coverage_fraction",
        "provider_price.cost_bases_use_contractual_maximum_exposure",
        "provider_price.collateral_and_capital_are_held_until_settlement",
        "provider_price.variable_claim_expense_is_paid_at_claim_settlement",
        "provider_price.fixed_expense_and_target_profit_are_month_zero_values",
        "provider_price.incremental_cost_terms_are_separate_and_nonduplicative",
        "provider_price.collateral_fraction_of_contractual_maximum_exposure",
        "provider_price.collateral_annual_effective_funding_rate",
        "provider_price.collateral_annual_effective_yield_rate",
        "provider_price.risk_capital_fraction_of_contractual_maximum_exposure",
        "provider_price.risk_capital_annual_effective_charge_rate",
        "provider_price.fixed_expense_upfront_million",
        "provider_price.variable_claim_expense_fraction",
        "provider_price.target_profit_upfront_million",
        "provider_price.provider_default_risk_is_modeled",
        "provider_price.fair_value_is_claimed",
    };

    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    ProviderPriceLadderConfig config;
    config.model_version =
        required(raw, "provider_price.model_version").value;
    config.scenario_label =
        parse_text(required(raw, "provider_price.label"));
    config.source_note =
        parse_text(required(raw, "provider_price.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "provider_price.synthetic_inputs"));
    config.coverage_selection = parse_coverage_selection(
        required(raw, "provider_price.coverage_selection"));
    config.explicit_coverage_fraction = parse_optional_double(
        required(raw, "provider_price.explicit_coverage_fraction"));
    config.cost_bases_use_contractual_maximum_exposure = parse_bool(required(
        raw, "provider_price.cost_bases_use_contractual_maximum_exposure"));
    config.collateral_and_capital_are_held_until_settlement = parse_bool(
        required(raw,
            "provider_price.collateral_and_capital_are_held_until_settlement"));
    config.variable_claim_expense_is_paid_at_claim_settlement = parse_bool(
        required(raw,
            "provider_price.variable_claim_expense_is_paid_at_claim_settlement"));
    config.fixed_expense_and_target_profit_are_month_zero_values = parse_bool(
        required(raw,
            "provider_price.fixed_expense_and_target_profit_are_month_zero_values"));
    config.incremental_cost_terms_are_separate_and_nonduplicative = parse_bool(
        required(raw,
            "provider_price.incremental_cost_terms_are_separate_and_nonduplicative"));
    config.collateral_fraction_of_contractual_maximum_exposure = parse_double(
        required(raw,
            "provider_price.collateral_fraction_of_contractual_maximum_exposure"));
    config.collateral_annual_effective_funding_rate = parse_double(required(
        raw, "provider_price.collateral_annual_effective_funding_rate"));
    config.collateral_annual_effective_yield_rate = parse_double(required(
        raw, "provider_price.collateral_annual_effective_yield_rate"));
    config.risk_capital_fraction_of_contractual_maximum_exposure = parse_double(
        required(raw,
            "provider_price.risk_capital_fraction_of_contractual_maximum_exposure"));
    config.risk_capital_annual_effective_charge_rate = parse_double(required(
        raw, "provider_price.risk_capital_annual_effective_charge_rate"));
    config.fixed_expense_upfront_million = parse_double(
        required(raw, "provider_price.fixed_expense_upfront_million"));
    config.variable_claim_expense_fraction = parse_double(
        required(raw, "provider_price.variable_claim_expense_fraction"));
    config.target_profit_upfront_million = parse_double(
        required(raw, "provider_price.target_profit_upfront_million"));
    config.provider_default_risk_is_modeled = parse_bool(
        required(raw, "provider_price.provider_default_risk_is_modeled"));
    config.fair_value_is_claimed =
        parse_bool(required(raw, "provider_price.fair_value_is_claimed"));

    validate_provider_price_ladder_config(config);
    return config;
}

} // namespace

ProviderPriceLadderConfig parse_provider_price_ladder_config(
    std::istream& input) {
    return parse_raw(read_raw(input));
}

ProviderPriceLadderConfig load_provider_price_ladder_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "provider-price-ladder configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open provider-price-ladder configuration file: " +
            path.string());
    }
    try {
        return parse_provider_price_ladder_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading provider-price-ladder configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_provider_price_ladder_config(
    std::ostream& output, const ProviderPriceLadderConfig& config) {
    validate_provider_price_ladder_config(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "provider_price.model_version=" << config.model_version << '\n';
    output << "provider_price.label=" << config.scenario_label << '\n';
    output << "provider_price.source_note=" << config.source_note << '\n';
    output << "provider_price.synthetic_inputs=" << config.synthetic_inputs
           << '\n';
    output << "provider_price.coverage_selection="
           << to_string(config.coverage_selection) << '\n';
    output << "provider_price.explicit_coverage_fraction=";
    if (config.explicit_coverage_fraction.has_value()) {
        output << *config.explicit_coverage_fraction;
    } else {
        output << "none";
    }
    output << '\n';
    output << "provider_price.cost_bases_use_contractual_maximum_exposure="
           << config.cost_bases_use_contractual_maximum_exposure << '\n';
    output << "provider_price.collateral_and_capital_are_held_until_settlement="
           << config.collateral_and_capital_are_held_until_settlement << '\n';
    output << "provider_price.variable_claim_expense_is_paid_at_claim_settlement="
           << config.variable_claim_expense_is_paid_at_claim_settlement
           << '\n';
    output
        << "provider_price.fixed_expense_and_target_profit_are_month_zero_values="
        << config.fixed_expense_and_target_profit_are_month_zero_values << '\n';
    output
        << "provider_price.incremental_cost_terms_are_separate_and_nonduplicative="
        << config.incremental_cost_terms_are_separate_and_nonduplicative << '\n';
    output
        << "provider_price.collateral_fraction_of_contractual_maximum_exposure="
        << config.collateral_fraction_of_contractual_maximum_exposure << '\n';
    output << "provider_price.collateral_annual_effective_funding_rate="
           << config.collateral_annual_effective_funding_rate << '\n';
    output << "provider_price.collateral_annual_effective_yield_rate="
           << config.collateral_annual_effective_yield_rate << '\n';
    output
        << "provider_price.risk_capital_fraction_of_contractual_maximum_exposure="
        << config.risk_capital_fraction_of_contractual_maximum_exposure << '\n';
    output << "provider_price.risk_capital_annual_effective_charge_rate="
           << config.risk_capital_annual_effective_charge_rate << '\n';
    output << "provider_price.fixed_expense_upfront_million="
           << config.fixed_expense_upfront_million << '\n';
    output << "provider_price.variable_claim_expense_fraction="
           << config.variable_claim_expense_fraction << '\n';
    output << "provider_price.target_profit_upfront_million="
           << config.target_profit_upfront_million << '\n';
    output << "provider_price.provider_default_risk_is_modeled="
           << config.provider_default_risk_is_modeled << '\n';
    output << "provider_price.fair_value_is_claimed="
           << config.fair_value_is_claimed << '\n';
}

} // namespace naturalehia::cellular_finance
