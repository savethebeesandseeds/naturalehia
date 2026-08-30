// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_market_priority_cap_config.hpp>

#include <algorithm>
#include <array>
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
constexpr std::size_t kMaximumGridEntries =
    kRobustMarketPriorityCapMaximumCandidates;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMinimumPositiveCapMillion = 1.0e-6;
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};

constexpr std::array<std::string_view, 21U> kFixedKeys{
    "priority_cap.model_version",
    "priority_cap.label",
    "priority_cap.source_note",
    "priority_cap.synthetic_inputs",
    "priority_cap.junior_claim_id",
    "priority_cap.market_claim_id",
    "priority_cap.contractual_ceiling_million",
    "priority_cap.junior_target_npv_million",
    "market_priority_cap_grid.count",
    "mandate.minimum_robust_aggregate_npv_million",
    "mandate.minimum_market_robust_npv_margin_fraction",
    "mandate.maximum_market_expected_loss_fraction",
    "mandate.maximum_market_principal_loss_es95_fraction",
    "mandate.maximum_market_principal_loss_es99_fraction",
    "mandate.maximum_market_principal_impairment_probability",
    "mandate.maximum_market_negative_npv_probability",
    "mandate.maximum_market_npv_shortfall_es95_fraction",
    "mandate.maximum_market_npv_shortfall_es99_fraction",
    "mandate.maximum_market_wal_years",
    "mandate.maximum_catalytic_first_loss_million",
    "mandate.maximum_catalytic_npv_concession_million",
};
constexpr std::size_t kMaximumRawEntries =
    kFixedKeys.size() + kMaximumGridEntries;

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
    for (const unsigned char character : value) {
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument(
        "market-priority-cap configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "market-priority-cap configuration is missing required key: " +
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

[[nodiscard]] bool is_fixed_key(std::string_view key) noexcept {
    return std::find(kFixedKeys.begin(), kFixedKeys.end(), key) !=
        kFixedKeys.end();
}

[[nodiscard]] bool is_bounded_cap_grid_key(std::string_view key) noexcept {
    constexpr std::string_view prefix{"market_priority_cap_grid."};
    constexpr std::string_view suffix{".amount_million"};
    if (!key.starts_with(prefix) || !key.ends_with(suffix) ||
        key.size() <= prefix.size() + suffix.size()) {
        return false;
    }
    const std::string_view index_text = key.substr(prefix.size(),
        key.size() - prefix.size() - suffix.size());
    if (index_text.empty() || index_text.front() == '0') {
        return false;
    }
    std::uint64_t index{};
    const char* const begin = index_text.data();
    const char* const end = begin + index_text.size();
    const auto conversion = std::from_chars(begin, end, index);
    return conversion.ec == std::errc{} && conversion.ptr == end &&
        index >= 1U && index <= kMaximumGridEntries;
}

[[nodiscard]] bool read_bounded_line(std::istream& input,
    std::string& line, std::size_t line_number,
    std::size_t& bytes_read) {
    line.clear();
    while (true) {
        const std::char_traits<char>::int_type next = input.get();
        if (std::char_traits<char>::eq_int_type(
                next, std::char_traits<char>::eof())) {
            if (!input.eof()) {
                throw std::runtime_error(
                    "failed while reading market-priority-cap configuration");
            }
            return !line.empty();
        }
        if (bytes_read >= kMaximumConfigBytes) {
            parse_error(line_number,
                "configuration exceeds the 16 MiB guardrail");
        }
        ++bytes_read;
        const char character = std::char_traits<char>::to_char_type(next);
        if (character == '\n') {
            return true;
        }
        if (line.size() >= kMaximumConfigLineBytes) {
            parse_error(line_number,
                "configuration line exceeds the 4096-byte guardrail");
        }
        line.push_back(character);
    }
}

[[nodiscard]] RawMap read_raw(std::istream& input) {
    RawMap raw;
    std::string line_text;
    line_text.reserve(kMaximumConfigLineBytes);
    std::size_t line_number = 0U;
    std::size_t bytes_read = 0U;
    while (read_bounded_line(
        input, line_text, line_number + 1U, bytes_read)) {
        ++line_number;

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
        if (!is_fixed_key(key) && !is_bounded_cap_grid_key(key)) {
            parse_error(line_number, "unknown key: " + std::string(key));
        }
        if (raw.size() >= kMaximumRawEntries) {
            parse_error(line_number,
                "configuration entry count exceeds the bounded schema");
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
            "failed while reading market-priority-cap configuration");
    }
    return raw;
}

[[nodiscard]] std::string cap_grid_key(std::size_t index) {
    return "market_priority_cap_grid." + std::to_string(index + 1U) +
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
    if (value < 0.0 || (value == 0.0 && std::signbit(value))) {
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

void canonicalize_grid(std::vector<double>& values) {
    if (values.empty() || values.size() > kMaximumGridEntries) {
        throw std::invalid_argument(
            "market priority-cap grid must contain between one and 1,024 entries");
    }
    for (const double value : values) {
        require_non_negative(value, "market priority-cap grid amount");
        if (value > 0.0 && value < kMinimumPositiveCapMillion) {
            throw std::invalid_argument(
                "positive market priority-cap grid amounts must be at least one base currency unit");
        }
    }
    std::sort(values.begin(), values.end());
    if (std::adjacent_find(values.begin(), values.end()) != values.end()) {
        throw std::invalid_argument(
            "market priority-cap grid values must be unique");
    }
}

[[nodiscard]] bool has_cap_sensitive_market_mandate(
    const RobustMarketPriorityCapConstraints& constraints) noexcept {
    return constraints.minimum_market_robust_npv_margin_fraction.has_value() ||
        constraints.maximum_market_negative_npv_probability.has_value() ||
        constraints.maximum_market_npv_shortfall_es95_fraction.has_value() ||
        constraints.maximum_market_npv_shortfall_es99_fraction.has_value();
}

[[nodiscard]] RobustMarketPriorityCapConfig canonicalized_and_validated(
    RobustMarketPriorityCapConfig config) {
    if (config.model_version != kRobustMarketPriorityCapModelVersion) {
        throw std::invalid_argument(
            "unsupported market-priority-cap model version");
    }
    require_safe_text(config.scenario_label, "market-priority-cap label");
    require_safe_text(config.source_note,
        "market-priority-cap source note");
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "market-priority-cap v0.1 accepts synthetic inputs only");
    }
    if (!is_safe_identifier(config.junior_claim_id) ||
        !is_safe_identifier(config.market_claim_id) ||
        config.junior_claim_id == config.market_claim_id) {
        throw std::invalid_argument(
            "market-priority-cap claim ids must be safe and different");
    }

    canonicalize_grid(
        config.market_priority_nonprincipal_cap_million_grid);
    require_non_negative(config.contractual_ceiling_million,
        "market-priority-cap contractual ceiling");
    if (config.contractual_ceiling_million > 0.0 &&
        config.contractual_ceiling_million < kMinimumPositiveCapMillion) {
        throw std::invalid_argument(
            "positive market-priority-cap contractual ceiling must be at least one base currency unit");
    }
    if (config.market_priority_nonprincipal_cap_million_grid.front() != 0.0) {
        throw std::invalid_argument(
            "market priority-cap grid must contain literal zero");
    }
    if (config.market_priority_nonprincipal_cap_million_grid.back() !=
        config.contractual_ceiling_million) {
        throw std::invalid_argument(
            "market priority-cap grid maximum must equal the contractual ceiling");
    }
    require_finite(config.junior_target_npv_million,
        "market-priority-cap junior target NPV");

    const RobustMarketPriorityCapConstraints& limits = config.constraints;
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
    if (!has_cap_sensitive_market_mandate(limits)) {
        throw std::invalid_argument(
            "market-priority-cap term requires at least one cap-sensitive market mandate");
    }
    return config;
}

[[nodiscard]] RobustMarketPriorityCapConfig parse_raw(const RawMap& raw) {
    std::unordered_set<std::string> expected;
    expected.reserve(kFixedKeys.size());
    for (const std::string_view key : kFixedKeys) {
        expected.emplace(key);
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }
    const std::size_t cap_count =
        parse_size(required(raw, "market_priority_cap_grid.count"));
    if (cap_count == 0U || cap_count > kMaximumGridEntries) {
        throw std::invalid_argument(
            "market-priority-cap grid count must be between one and 1,024");
    }

    expected.reserve(kFixedKeys.size() + cap_count);
    for (std::size_t index = 0U; index < cap_count; ++index) {
        expected.emplace(cap_grid_key(index));
    }
    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    RobustMarketPriorityCapConfig config;
    config.model_version = required(raw, "priority_cap.model_version").value;
    config.scenario_label =
        parse_text(required(raw, "priority_cap.label"));
    config.source_note =
        parse_text(required(raw, "priority_cap.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "priority_cap.synthetic_inputs"));
    config.junior_claim_id =
        parse_text(required(raw, "priority_cap.junior_claim_id"));
    config.market_claim_id =
        parse_text(required(raw, "priority_cap.market_claim_id"));
    config.contractual_ceiling_million = parse_double(
        required(raw, "priority_cap.contractual_ceiling_million"));
    config.junior_target_npv_million = parse_double(
        required(raw, "priority_cap.junior_target_npv_million"));
    config.market_priority_nonprincipal_cap_million_grid.reserve(cap_count);
    for (std::size_t index = 0U; index < cap_count; ++index) {
        config.market_priority_nonprincipal_cap_million_grid.push_back(
            parse_double(required(raw, cap_grid_key(index))));
    }

    RobustMarketPriorityCapConstraints& limits = config.constraints;
    limits.minimum_robust_aggregate_npv_million = parse_optional_double(
        required(raw, "mandate.minimum_robust_aggregate_npv_million"));
    limits.minimum_market_robust_npv_margin_fraction = parse_optional_double(
        required(raw,
            "mandate.minimum_market_robust_npv_margin_fraction"));
    limits.maximum_market_expected_loss_fraction = parse_optional_double(
        required(raw, "mandate.maximum_market_expected_loss_fraction"));
    limits.maximum_market_principal_loss_es95_fraction = parse_optional_double(
        required(raw,
            "mandate.maximum_market_principal_loss_es95_fraction"));
    limits.maximum_market_principal_loss_es99_fraction = parse_optional_double(
        required(raw,
            "mandate.maximum_market_principal_loss_es99_fraction"));
    limits.maximum_market_principal_impairment_probability =
        parse_optional_double(required(raw,
            "mandate.maximum_market_principal_impairment_probability"));
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
        required(raw, "mandate.maximum_catalytic_first_loss_million"));
    limits.maximum_catalytic_npv_concession_million = parse_optional_double(
        required(raw,
            "mandate.maximum_catalytic_npv_concession_million"));
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

RobustMarketPriorityCapConfig parse_robust_market_priority_cap_config(
    std::istream& input) {
    return parse_raw(read_raw(input));
}

RobustMarketPriorityCapConfig load_robust_market_priority_cap_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "market-priority-cap configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open market-priority-cap configuration file: " +
            path.string());
    }
    try {
        return parse_robust_market_priority_cap_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading market-priority-cap configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_robust_market_priority_cap_config(
    std::ostream& output, const RobustMarketPriorityCapConfig& config) {
    const RobustMarketPriorityCapConfig canonical =
        canonicalized_and_validated(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;

    output << "priority_cap.model_version=" << canonical.model_version << '\n';
    output << "priority_cap.label=" << canonical.scenario_label << '\n';
    output << "priority_cap.source_note=" << canonical.source_note << '\n';
    output << "priority_cap.synthetic_inputs=" << canonical.synthetic_inputs
           << '\n';
    output << "priority_cap.junior_claim_id=" << canonical.junior_claim_id
           << '\n';
    output << "priority_cap.market_claim_id=" << canonical.market_claim_id
           << '\n';
    output << "priority_cap.contractual_ceiling_million="
           << canonical.contractual_ceiling_million << '\n';
    output << "priority_cap.junior_target_npv_million="
           << canonical.junior_target_npv_million << '\n';
    output << "market_priority_cap_grid.count="
           << canonical.market_priority_nonprincipal_cap_million_grid.size()
           << '\n';
    for (std::size_t index = 0U;
         index <
            canonical.market_priority_nonprincipal_cap_million_grid.size();
         ++index) {
        output << cap_grid_key(index) << '='
               << canonical.market_priority_nonprincipal_cap_million_grid[
                      index]
               << '\n';
    }

    const RobustMarketPriorityCapConstraints& limits = canonical.constraints;
    print_optional(output, "mandate.minimum_robust_aggregate_npv_million",
        limits.minimum_robust_aggregate_npv_million);
    print_optional(output,
        "mandate.minimum_market_robust_npv_margin_fraction",
        limits.minimum_market_robust_npv_margin_fraction);
    print_optional(output, "mandate.maximum_market_expected_loss_fraction",
        limits.maximum_market_expected_loss_fraction);
    print_optional(output,
        "mandate.maximum_market_principal_loss_es95_fraction",
        limits.maximum_market_principal_loss_es95_fraction);
    print_optional(output,
        "mandate.maximum_market_principal_loss_es99_fraction",
        limits.maximum_market_principal_loss_es99_fraction);
    print_optional(output,
        "mandate.maximum_market_principal_impairment_probability",
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
    print_optional(output, "mandate.maximum_catalytic_first_loss_million",
        limits.maximum_catalytic_first_loss_million);
    print_optional(output,
        "mandate.maximum_catalytic_npv_concession_million",
        limits.maximum_catalytic_npv_concession_million);

    if (!output) {
        throw std::runtime_error(
            "failed while writing normalized market-priority-cap configuration");
    }
}

} // namespace naturalehia::cellular_finance
