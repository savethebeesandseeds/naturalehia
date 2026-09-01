// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier_config.hpp>

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
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;
constexpr std::size_t kMaximumGridEntries = 1'024U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMinimumClaimNotionalMillion = 1.0e-6;
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
            // Stream-format restoration must not throw while unwinding.
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
        trim_view(value).size() != value.size() ||
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
    throw std::invalid_argument(
        "capital-mobilization-frontier configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "capital-mobilization-frontier configuration is missing required key: " +
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

[[nodiscard]] std::size_t parse_size(const RawValue& raw) {
    std::uint64_t result{};
    const char* const begin = raw.value.data();
    const char* const end = begin + raw.value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (raw.value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end ||
        result > static_cast<std::uint64_t>(
                     std::numeric_limits<std::size_t>::max())) {
        parse_error(raw.line, "expected a bounded non-negative integer");
    }
    return static_cast<std::size_t>(result);
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
            "failed while reading capital-mobilization-frontier configuration");
    }
    return raw;
}

[[nodiscard]] std::string participation_grid_key(std::size_t index) {
    return "participation_grid." + std::to_string(index + 1U) +
        ".fraction";
}

[[nodiscard]] std::string first_loss_grid_key(std::size_t index) {
    return "catalytic_first_loss_grid." + std::to_string(index + 1U) +
        ".amount_million";
}

[[nodiscard]] std::string junior_issued_principal_grid_key(
    std::size_t index) {
    return "junior_issued_principal_grid." + std::to_string(index + 1U) +
        ".amount_million";
}

void require_finite(double value, std::string_view description) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be finite");
    }
}

void require_non_negative(double value, std::string_view description) {
    require_finite(value, description);
    if (value < 0.0) {
        throw std::invalid_argument(
            std::string(description) + " must be non-negative");
    }
}

void require_unit_interval(double value, std::string_view description) {
    require_finite(value, description);
    if (value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(description) + " must lie in [0,1]");
    }
}

template <typename Validator>
void validate_optional(const std::optional<double>& value,
    std::string_view description, Validator validator) {
    if (value.has_value()) {
        validator(*value, description);
    }
}

void canonicalize_grid(std::vector<double>& values,
    std::string_view description) {
    if (values.empty() || values.size() > kMaximumGridEntries) {
        throw std::invalid_argument(
            std::string(description) +
            " must contain between one and 1,024 entries");
    }
    for (const double value : values) {
        require_finite(value, description);
    }
    std::sort(values.begin(), values.end());
    if (std::adjacent_find(values.begin(), values.end()) != values.end()) {
        throw std::invalid_argument(
            std::string(description) + " values must be unique");
    }
}

[[nodiscard]] RobustCapitalMobilizationFrontierConfig
canonicalized_and_validated(
    RobustCapitalMobilizationFrontierConfig config) {
    const bool is_v01 = config.model_version ==
        kRobustCapitalMobilizationFrontierModelVersion;
    const bool is_v02 = config.model_version ==
        kRobustCapitalMobilizationFrontierV02ModelVersion;
    if (!is_v01 && !is_v02) {
        throw std::invalid_argument(
            "unsupported capital-mobilization-frontier model version");
    }
    require_safe_text(config.scenario_label,
        "capital-mobilization-frontier label");
    require_safe_text(config.source_note,
        "capital-mobilization-frontier source note");
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "capital-mobilization-frontier accepts synthetic inputs only");
    }
    if (!is_safe_identifier(config.catalytic_claim_id) ||
        !is_safe_identifier(config.market_claim_id) ||
        config.catalytic_claim_id == config.market_claim_id) {
        throw std::invalid_argument(
            "capital-mobilization-frontier claim ids must be safe and different");
    }

    canonicalize_grid(config.participation_fraction_grid,
        "participation grid");
    canonicalize_grid(config.catalytic_first_loss_million_grid,
        "catalytic first-loss grid");
    if (config.participation_fraction_grid.size() >
        kRobustCapitalMobilizationFrontierMaximumCandidates /
            config.catalytic_first_loss_million_grid.size()) {
        throw std::invalid_argument(
            "capital-mobilization-frontier grid exceeds 1,024 candidates");
    }
    for (const double fraction : config.participation_fraction_grid) {
        require_unit_interval(fraction, "participation grid fraction");
    }
    for (const double amount :
         config.catalytic_first_loss_million_grid) {
        require_finite(amount, "catalytic first-loss grid amount");
        if (amount < kMinimumClaimNotionalMillion) {
            throw std::invalid_argument(
                "catalytic first-loss grid amounts must be at least one base currency unit");
        }
    }

    require_non_negative(config.market_priority_nonprincipal_cap_million,
        "market priority non-principal cap");
    require_non_negative(config.catalytic_annual_physical_hurdle_rate,
        "catalytic annual physical hurdle rate");
    require_non_negative(config.market_annual_physical_hurdle_rate,
        "market annual physical hurdle rate");
    if (config.catalytic_annual_physical_hurdle_rate > 10.0 ||
        config.market_annual_physical_hurdle_rate > 10.0) {
        throw std::invalid_argument(
            "capital-mobilization-frontier hurdle rates must not exceed 10");
    }
    require_finite(config.catalytic_target_npv_million,
        "catalytic target NPV");

    const RobustCapitalMobilizationFrontierConstraints& limits =
        config.constraints;
    validate_optional(limits.minimum_robust_aggregate_npv_million,
        "minimum robust aggregate NPV", require_finite);
    validate_optional(limits.minimum_market_robust_npv_margin_fraction,
        "minimum market robust NPV margin", require_finite);
    validate_optional(limits.maximum_market_expected_loss_fraction,
        "maximum market expected-loss fraction", require_unit_interval);
    validate_optional(limits.maximum_market_principal_loss_es95_fraction,
        "maximum market principal-loss ES95 fraction", require_unit_interval);
    validate_optional(limits.maximum_market_principal_loss_es99_fraction,
        "maximum market principal-loss ES99 fraction", require_unit_interval);
    validate_optional(limits.maximum_market_principal_impairment_probability,
        "maximum market principal-impairment probability",
        require_unit_interval);
    validate_optional(limits.maximum_market_negative_npv_probability,
        "maximum market negative-NPV probability", require_unit_interval);
    validate_optional(limits.maximum_market_npv_shortfall_es95_fraction,
        "maximum market NPV-shortfall ES95 fraction", require_non_negative);
    validate_optional(limits.maximum_market_npv_shortfall_es99_fraction,
        "maximum market NPV-shortfall ES99 fraction", require_non_negative);
    validate_optional(limits.maximum_market_wal_years,
        "maximum market WAL", require_non_negative);
    validate_optional(limits.maximum_catalytic_first_loss_million,
        "maximum catalytic first loss", require_non_negative);
    validate_optional(limits.maximum_catalytic_npv_concession_million,
        "maximum catalytic NPV concession", require_non_negative);
    return config;
}

[[nodiscard]] RobustCapitalMobilizationFrontierConfig parse_raw(
    const RawMap& raw) {
    const std::string& model_version =
        required(raw, "frontier.model_version").value;
    const bool is_v02 =
        model_version == kRobustCapitalMobilizationFrontierV02ModelVersion;
    if (model_version !=
            kRobustCapitalMobilizationFrontierModelVersion &&
        !is_v02) {
        throw std::invalid_argument(
            "unsupported capital-mobilization-frontier model version");
    }

    std::unordered_set<std::string> fixed_keys{
        "frontier.model_version",
        "frontier.label",
        "frontier.source_note",
        "frontier.synthetic_inputs",
        "frontier.market_claim_id",
        "frontier.market_priority_nonprincipal_cap_million",
        "frontier.market_annual_physical_hurdle_rate",
        "participation_grid.count",
        "mandate.minimum_robust_aggregate_npv_million",
        "mandate.minimum_market_robust_npv_margin_fraction",
        "mandate.maximum_market_negative_npv_probability",
        "mandate.maximum_market_npv_shortfall_es95_fraction",
        "mandate.maximum_market_npv_shortfall_es99_fraction",
        "mandate.maximum_market_wal_years",
    };
    if (is_v02) {
        fixed_keys.insert({
            "frontier.junior_claim_id",
            "frontier.junior_annual_physical_hurdle_rate",
            "frontier.junior_target_npv_million",
            "junior_issued_principal_grid.count",
            "mandate.maximum_market_expected_issued_principal_cash_shortfall_fraction",
            "mandate.maximum_market_issued_principal_cash_shortfall_es95_fraction",
            "mandate.maximum_market_issued_principal_cash_shortfall_es99_fraction",
            "mandate.maximum_market_principal_cash_shortfall_probability",
            "mandate.maximum_junior_issued_principal_million",
            "mandate.maximum_junior_npv_concession_million",
        });
    } else {
        fixed_keys.insert({
            "frontier.catalytic_claim_id",
            "frontier.catalytic_annual_physical_hurdle_rate",
            "frontier.catalytic_target_npv_million",
            "catalytic_first_loss_grid.count",
            "mandate.maximum_market_expected_loss_fraction",
            "mandate.maximum_market_principal_loss_es95_fraction",
            "mandate.maximum_market_principal_loss_es99_fraction",
            "mandate.maximum_market_principal_impairment_probability",
            "mandate.maximum_catalytic_first_loss_million",
            "mandate.maximum_catalytic_npv_concession_million",
        });
    }
    for (const std::string& key : fixed_keys) {
        (void)required(raw, key);
    }
    const std::size_t participation_count =
        parse_size(required(raw, "participation_grid.count"));
    const std::size_t first_loss_count = parse_size(required(raw,
        is_v02 ? "junior_issued_principal_grid.count"
               : "catalytic_first_loss_grid.count"));
    if (participation_count == 0U ||
        participation_count > kMaximumGridEntries ||
        first_loss_count == 0U || first_loss_count > kMaximumGridEntries) {
        throw std::invalid_argument(
            "capital-mobilization-frontier grid counts must be between one and 1,024");
    }
    if (participation_count >
        kRobustCapitalMobilizationFrontierMaximumCandidates /
            first_loss_count) {
        throw std::invalid_argument(
            "capital-mobilization-frontier grid exceeds 1,024 candidates");
    }

    std::unordered_set<std::string> expected = fixed_keys;
    expected.reserve(fixed_keys.size() + participation_count +
        first_loss_count);
    for (std::size_t index = 0U; index < participation_count; ++index) {
        expected.emplace(participation_grid_key(index));
    }
    for (std::size_t index = 0U; index < first_loss_count; ++index) {
        expected.emplace(is_v02 ? junior_issued_principal_grid_key(index)
                                : first_loss_grid_key(index));
    }
    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    RobustCapitalMobilizationFrontierConfig config;
    config.model_version = required(raw, "frontier.model_version").value;
    config.scenario_label = parse_text(required(raw, "frontier.label"));
    config.source_note = parse_text(required(raw, "frontier.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "frontier.synthetic_inputs"));
    config.catalytic_claim_id = parse_text(required(raw,
        is_v02 ? "frontier.junior_claim_id"
               : "frontier.catalytic_claim_id"));
    config.market_claim_id =
        parse_text(required(raw, "frontier.market_claim_id"));
    config.market_priority_nonprincipal_cap_million = parse_double(required(
        raw, "frontier.market_priority_nonprincipal_cap_million"));
    config.catalytic_annual_physical_hurdle_rate = parse_double(required(raw,
        is_v02 ? "frontier.junior_annual_physical_hurdle_rate"
               : "frontier.catalytic_annual_physical_hurdle_rate"));
    config.market_annual_physical_hurdle_rate = parse_double(required(
        raw, "frontier.market_annual_physical_hurdle_rate"));
    config.catalytic_target_npv_million = parse_double(required(raw,
        is_v02 ? "frontier.junior_target_npv_million"
               : "frontier.catalytic_target_npv_million"));

    config.participation_fraction_grid.reserve(participation_count);
    for (std::size_t index = 0U; index < participation_count; ++index) {
        config.participation_fraction_grid.push_back(
            parse_double(required(raw, participation_grid_key(index))));
    }
    config.catalytic_first_loss_million_grid.reserve(first_loss_count);
    for (std::size_t index = 0U; index < first_loss_count; ++index) {
        config.catalytic_first_loss_million_grid.push_back(parse_double(
            required(raw, is_v02 ? junior_issued_principal_grid_key(index)
                                 : first_loss_grid_key(index))));
    }

    RobustCapitalMobilizationFrontierConstraints& limits =
        config.constraints;
    limits.minimum_robust_aggregate_npv_million = parse_optional_double(
        required(raw, "mandate.minimum_robust_aggregate_npv_million"));
    limits.minimum_market_robust_npv_margin_fraction = parse_optional_double(
        required(raw,
            "mandate.minimum_market_robust_npv_margin_fraction"));
    limits.maximum_market_expected_loss_fraction = parse_optional_double(
        required(raw, is_v02
                ? "mandate.maximum_market_expected_issued_principal_cash_shortfall_fraction"
                : "mandate.maximum_market_expected_loss_fraction"));
    limits.maximum_market_principal_loss_es95_fraction = parse_optional_double(
        required(raw, is_v02
                ? "mandate.maximum_market_issued_principal_cash_shortfall_es95_fraction"
                : "mandate.maximum_market_principal_loss_es95_fraction"));
    limits.maximum_market_principal_loss_es99_fraction = parse_optional_double(
        required(raw, is_v02
                ? "mandate.maximum_market_issued_principal_cash_shortfall_es99_fraction"
                : "mandate.maximum_market_principal_loss_es99_fraction"));
    limits.maximum_market_principal_impairment_probability =
        parse_optional_double(required(raw,
            is_v02
                ? "mandate.maximum_market_principal_cash_shortfall_probability"
                : "mandate.maximum_market_principal_impairment_probability"));
    limits.maximum_market_negative_npv_probability = parse_optional_double(
        required(raw, "mandate.maximum_market_negative_npv_probability"));
    limits.maximum_market_npv_shortfall_es95_fraction = parse_optional_double(
        required(raw,
            "mandate.maximum_market_npv_shortfall_es95_fraction"));
    limits.maximum_market_npv_shortfall_es99_fraction = parse_optional_double(
        required(raw,
            "mandate.maximum_market_npv_shortfall_es99_fraction"));
    limits.maximum_market_wal_years = parse_optional_double(
        required(raw, "mandate.maximum_market_wal_years"));
    limits.maximum_catalytic_first_loss_million = parse_optional_double(
        required(raw, is_v02
                ? "mandate.maximum_junior_issued_principal_million"
                : "mandate.maximum_catalytic_first_loss_million"));
    limits.maximum_catalytic_npv_concession_million = parse_optional_double(
        required(raw, is_v02
                ? "mandate.maximum_junior_npv_concession_million"
                : "mandate.maximum_catalytic_npv_concession_million"));
    return canonicalized_and_validated(std::move(config));
}

void print_optional(std::ostream& output, std::string_view key,
    const std::optional<double>& value) {
    output << key << '=';
    if (value.has_value()) {
        output << *value;
    } else {
        output << "none";
    }
    output << '\n';
}

} // namespace

RobustCapitalMobilizationFrontierConfig
parse_robust_capital_mobilization_frontier_config(std::istream& input) {
    return parse_raw(read_raw(input));
}

RobustCapitalMobilizationFrontierConfig
load_robust_capital_mobilization_frontier_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "capital-mobilization-frontier configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open capital-mobilization-frontier configuration file: " +
            path.string());
    }
    try {
        return parse_robust_capital_mobilization_frontier_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading capital-mobilization-frontier configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_robust_capital_mobilization_frontier_config(
    std::ostream& output,
    const RobustCapitalMobilizationFrontierConfig& config) {
    const RobustCapitalMobilizationFrontierConfig canonical =
        canonicalized_and_validated(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;

    output << "frontier.model_version=" << canonical.model_version << '\n';
    output << "frontier.label=" << canonical.scenario_label << '\n';
    output << "frontier.source_note=" << canonical.source_note << '\n';
    output << "frontier.synthetic_inputs=" << canonical.synthetic_inputs
           << '\n';
    const bool is_v02 = canonical.model_version ==
        kRobustCapitalMobilizationFrontierV02ModelVersion;
    output << (is_v02 ? "frontier.junior_claim_id="
                      : "frontier.catalytic_claim_id=")
           << canonical.catalytic_claim_id << '\n';
    output << "frontier.market_claim_id=" << canonical.market_claim_id
           << '\n';
    output << "frontier.market_priority_nonprincipal_cap_million="
           << canonical.market_priority_nonprincipal_cap_million << '\n';
    output << (is_v02
            ? "frontier.junior_annual_physical_hurdle_rate="
            : "frontier.catalytic_annual_physical_hurdle_rate=")
           << canonical.catalytic_annual_physical_hurdle_rate << '\n';
    output << "frontier.market_annual_physical_hurdle_rate="
           << canonical.market_annual_physical_hurdle_rate << '\n';
    output << (is_v02 ? "frontier.junior_target_npv_million="
                      : "frontier.catalytic_target_npv_million=")
           << canonical.catalytic_target_npv_million << '\n';
    output << "participation_grid.count="
           << canonical.participation_fraction_grid.size() << '\n';
    for (std::size_t index = 0U;
         index < canonical.participation_fraction_grid.size(); ++index) {
        output << participation_grid_key(index) << '='
               << canonical.participation_fraction_grid[index] << '\n';
    }
    output << (is_v02 ? "junior_issued_principal_grid.count="
                      : "catalytic_first_loss_grid.count=")
           << canonical.catalytic_first_loss_million_grid.size() << '\n';
    for (std::size_t index = 0U;
         index < canonical.catalytic_first_loss_million_grid.size(); ++index) {
        output << (is_v02 ? junior_issued_principal_grid_key(index)
                          : first_loss_grid_key(index))
               << '='
               << canonical.catalytic_first_loss_million_grid[index] << '\n';
    }

    const RobustCapitalMobilizationFrontierConstraints& limits =
        canonical.constraints;
    print_optional(output, "mandate.minimum_robust_aggregate_npv_million",
        limits.minimum_robust_aggregate_npv_million);
    print_optional(output,
        "mandate.minimum_market_robust_npv_margin_fraction",
        limits.minimum_market_robust_npv_margin_fraction);
    print_optional(output,
        is_v02
            ? "mandate.maximum_market_expected_issued_principal_cash_shortfall_fraction"
            : "mandate.maximum_market_expected_loss_fraction",
        limits.maximum_market_expected_loss_fraction);
    print_optional(output,
        is_v02
            ? "mandate.maximum_market_issued_principal_cash_shortfall_es95_fraction"
            : "mandate.maximum_market_principal_loss_es95_fraction",
        limits.maximum_market_principal_loss_es95_fraction);
    print_optional(output,
        is_v02
            ? "mandate.maximum_market_issued_principal_cash_shortfall_es99_fraction"
            : "mandate.maximum_market_principal_loss_es99_fraction",
        limits.maximum_market_principal_loss_es99_fraction);
    print_optional(output,
        is_v02
            ? "mandate.maximum_market_principal_cash_shortfall_probability"
            : "mandate.maximum_market_principal_impairment_probability",
        limits.maximum_market_principal_impairment_probability);
    print_optional(output,
        "mandate.maximum_market_negative_npv_probability",
        limits.maximum_market_negative_npv_probability);
    print_optional(output,
        "mandate.maximum_market_npv_shortfall_es95_fraction",
        limits.maximum_market_npv_shortfall_es95_fraction);
    print_optional(output,
        "mandate.maximum_market_npv_shortfall_es99_fraction",
        limits.maximum_market_npv_shortfall_es99_fraction);
    print_optional(output, "mandate.maximum_market_wal_years",
        limits.maximum_market_wal_years);
    print_optional(output,
        is_v02 ? "mandate.maximum_junior_issued_principal_million"
               : "mandate.maximum_catalytic_first_loss_million",
        limits.maximum_catalytic_first_loss_million);
    print_optional(output,
        is_v02 ? "mandate.maximum_junior_npv_concession_million"
               : "mandate.maximum_catalytic_npv_concession_million",
        limits.maximum_catalytic_npv_concession_million);

    if (!output) {
        throw std::runtime_error(
            "failed while writing normalized capital-mobilization-frontier configuration");
    }
}

} // namespace naturalehia::cellular_finance
