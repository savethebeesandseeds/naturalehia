// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/success_participation_config.hpp>

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

constexpr std::size_t kMinimumEligibleSourceCount = 1U;
constexpr std::size_t kMaximumEligibleSourceCount = 3U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;
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
        "success-participation configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "success-participation configuration is missing required key: " +
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

[[nodiscard]] PortfolioCashSource parse_eligible_source(
    const RawValue& raw) {
    if (raw.value == "commercial") {
        return PortfolioCashSource::Commercial;
    }
    if (raw.value == "licensing_royalty") {
        return PortfolioCashSource::LicensingRoyalty;
    }
    if (raw.value == "exit_sale") {
        return PortfolioCashSource::ExitSale;
    }
    parse_error(raw.line,
        "expected commercial, licensing_royalty, or exit_sale");
}

[[nodiscard]] std::string_view eligible_source_text(
    PortfolioCashSource source) {
    switch (source) {
    case PortfolioCashSource::Commercial:
        return "commercial";
    case PortfolioCashSource::LicensingRoyalty:
        return "licensing_royalty";
    case PortfolioCashSource::ExitSale:
        return "exit_sale";
    case PortfolioCashSource::Recovery:
    case PortfolioCashSource::Refinancing:
    case PortfolioCashSource::ExplicitSupport:
    case PortfolioCashSource::SponsorFee:
    case PortfolioCashSource::FinancingFee:
        break;
    }
    throw std::invalid_argument(
        "success-participation scalable source kind is not permitted in v0.1");
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    if (trim_view(value).size() != value.size()) {
        throw std::invalid_argument(
            std::string(description) +
            " must not begin or end with whitespace");
    }
    if (value.find(kUtf8Bom) != std::string_view::npos) {
        throw std::invalid_argument(
            std::string(description) + " contains a UTF-8 BOM");
    }
    for (const unsigned char character : value) {
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

[[nodiscard]] std::string eligible_source_key(std::size_t source) {
    return "eligible_source." + std::to_string(source + 1U) + ".kind";
}

void validate_intrinsic_config(const SuccessParticipationConfig& config) {
    if (config.model_version != kSuccessParticipationModelVersion) {
        throw std::invalid_argument(
            "success-participation model_version does not match this engine");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "success-participation v0.1 accepts synthetic inputs only");
    }
    if (!config.selected_nonprincipal_cash_is_contractually_scalable) {
        throw std::invalid_argument(
            "success-participation v0.1 requires the contractual-scalability "
            "assertion");
    }
    require_safe_text(config.scenario_label, "participation label");
    require_safe_text(config.source_note, "participation source_note");
    if (!std::isfinite(config.target_worst_expected_npv_million)) {
        throw std::invalid_argument(
            "success-participation robust NPV target must be finite");
    }
    if (config.scalable_source_kinds.size() <
            kMinimumEligibleSourceCount ||
        config.scalable_source_kinds.size() >
            kMaximumEligibleSourceCount) {
        throw std::invalid_argument(
            "success-participation eligible source count must be between one "
            "and three");
    }

    std::unordered_set<unsigned int> sources;
    sources.reserve(config.scalable_source_kinds.size());
    for (const PortfolioCashSource source : config.scalable_source_kinds) {
        (void)eligible_source_text(source);
        if (!sources.insert(static_cast<unsigned int>(source)).second) {
            throw std::invalid_argument(
                "success-participation eligible source kinds must be unique");
        }
    }
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
            "failed while reading success-participation configuration");
    }
    return raw;
}

[[nodiscard]] SuccessParticipationConfig parse_raw(const RawMap& raw) {
    const std::size_t source_count =
        parse_size(required(raw, "eligible_source.count"));
    if (source_count < kMinimumEligibleSourceCount ||
        source_count > kMaximumEligibleSourceCount) {
        throw std::invalid_argument(
            "parsed success-participation eligible source count must be "
            "between one and three");
    }

    constexpr std::size_t global_key_count = 7U;
    constexpr std::size_t source_field_count = 1U;
    if (raw.size() < global_key_count ||
        source_count >
            (raw.size() - global_key_count) / source_field_count) {
        throw std::invalid_argument(
            "success-participation configuration declared count requires missing keys");
    }

    std::unordered_set<std::string> expected{
        "participation.model_version",
        "participation.label",
        "participation.source_note",
        "participation.synthetic_inputs",
        "participation.selected_nonprincipal_cash_is_contractually_scalable",
        "participation.target_robust_npv_million",
        "eligible_source.count",
    };
    expected.reserve(global_key_count + source_field_count * source_count);
    for (std::size_t source = 0U; source < source_count; ++source) {
        expected.insert(eligible_source_key(source));
    }

    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    SuccessParticipationConfig config;
    config.model_version =
        required(raw, "participation.model_version").value;
    config.scenario_label = required(raw, "participation.label").value;
    config.source_note = required(raw, "participation.source_note").value;
    config.synthetic_inputs =
        parse_bool(required(raw, "participation.synthetic_inputs"));
    config.selected_nonprincipal_cash_is_contractually_scalable = parse_bool(
        required(raw,
            "participation.selected_nonprincipal_cash_is_contractually_scalable"));
    config.target_worst_expected_npv_million = parse_double(
        required(raw, "participation.target_robust_npv_million"));
    config.scalable_source_kinds.reserve(source_count);
    for (std::size_t source = 0U; source < source_count; ++source) {
        config.scalable_source_kinds.push_back(
            parse_eligible_source(required(raw, eligible_source_key(source))));
    }

    validate_intrinsic_config(config);
    return config;
}

} // namespace

SuccessParticipationConfig parse_success_participation_config(
    std::istream& input) {
    return parse_raw(read_raw(input));
}

SuccessParticipationConfig load_success_participation_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "success-participation configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open success-participation configuration file: " +
            path.string());
    }
    try {
        return parse_success_participation_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading success-participation configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_success_participation_config(
    std::ostream& output, const SuccessParticipationConfig& config) {
    validate_intrinsic_config(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;
    output << "participation.model_version=" << config.model_version << '\n';
    output << "participation.label=" << config.scenario_label << '\n';
    output << "participation.source_note=" << config.source_note << '\n';
    output << "participation.synthetic_inputs=" << config.synthetic_inputs
           << '\n';
    output
        << "participation.selected_nonprincipal_cash_is_contractually_scalable="
        << config.selected_nonprincipal_cash_is_contractually_scalable << '\n';
    output << "participation.target_robust_npv_million="
           << config.target_worst_expected_npv_million << '\n';
    output << "eligible_source.count=" << config.scalable_source_kinds.size()
           << '\n';
    for (std::size_t source = 0U;
         source < config.scalable_source_kinds.size(); ++source) {
        output << eligible_source_key(source) << '='
               << eligible_source_text(config.scalable_source_kinds[source])
               << '\n';
    }
}

} // namespace naturalehia::cellular_finance
