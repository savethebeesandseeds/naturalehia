// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_package.hpp>

#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::uintmax_t kMaximumConfigBytes = 1024U * 1024U;
constexpr std::uintmax_t kMaximumTsvBytes = 256U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumSourceManifestBytes = 64U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumRetainedSourceBytes =
    512U * 1024U * 1024U;
constexpr std::uintmax_t kMaximumAggregateRetainedSourceBytes =
    2U * 1024U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 8'192U;
constexpr std::size_t kMaximumTsvLineBytes = 16'384U;
constexpr std::size_t kMaximumTerms = 100'000U;
constexpr std::size_t kMaximumProviders = 4'096U;
constexpr std::size_t kMaximumCovenantEvents =
    kClaimLedgerMaximumCovenantEvents;
constexpr std::size_t kMaximumSourceManifestRows = 100'000U;
constexpr std::size_t kMaximumConversionContext = 100'000U;
constexpr std::size_t kMaximumReportedBlockers = 1'024U;
constexpr std::size_t kReservedCriticalBlockers = 64U;
constexpr std::size_t kMaximumReportedBlockerBytes = 1024U * 1024U;
constexpr double kMaximumMoneyMillion = 1.0e9;
constexpr double kMaximumAnnualEffectiveRate = 10.0;

constexpr std::string_view kTermsHeader =
    "term_id\tvalue_status\tvalue\tinput_status\tknown_at_period\t"
    "source_record_id\tnote";
constexpr std::string_view kCommonEntriesHeader =
    "entry_id\teconomic_fact_id\tevent_group_id\tkind\tperiod\tknown_at_period\t"
    "value_status\tlower_value\tupper_value\tinput_status\t"
    "source_record_id\tprovider_claim_id";
constexpr std::string_view kScenariosHeader =
    "scenario_id\tprobability_status\tprobability_lower\t"
    "probability_upper\tprobability_known_at_period\t"
    "probability_input_status\tprobability_source_record_id\t"
    "cash_path_status\tcash_path_status_known_at_period\t"
    "cash_path_input_status\tcash_path_status_source_record_id";
constexpr std::string_view kScenarioEntriesHeader =
    "scenario_id\tentry_id\teconomic_fact_id\tevent_group_id\tkind\tperiod\t"
    "known_at_period\tvalue_status\tlower_value\tupper_value\t"
    "input_status\tsource_record_id\tprovider_claim_id";
constexpr std::string_view kProviderClaimsHeader =
    "provider_claim_id\tprovider_id\tknown_at_period\t"
    "shortfall_allocation_status\tshortfall_allocation_lower\t"
    "shortfall_allocation_upper\tcoverage_status\tcoverage_lower\t"
    "coverage_upper\tdeductible_status\tdeductible_lower_million\t"
    "deductible_upper_million\tmaximum_cash_status\t"
    "maximum_cash_lower_million\tmaximum_cash_upper_million\t"
    "settlement_lag_status\tsettlement_lag_lower_periods\t"
    "settlement_lag_upper_periods\tcovers_principal_due\t"
    "covers_interest_due\tpayment_right_evidenced\t"
    "provider_identity_evidenced\tcoverage_and_priority_evidenced\t"
    "obligation_priority\tinput_status\tsource_record_id";
constexpr std::string_view kCovenantEventsHeader =
    "scope\tscenario_id\tevent_id\tcovenant_id\tperiod\t"
    "event_date_status\tevent_date\tknown_at_period\tstate\t"
    "input_status\tsource_record_id\tnote";
constexpr std::string_view kConversionContextHeader =
    "context_id\tobserved_date\tvalue_status\tvalue\tinput_status\t"
    "known_at_period\tsource_record_id\tadmission_status\tnote";
constexpr std::string_view kSourceManifestHeader =
    "source_id\trecord_date\taccess_date\tevidence_class\tprovenance_tag\t"
    "distribution_channel\toriginating_record\tsource_uri\tretention_status\t"
    "retained_copy\tretained_sha256\tbytes\tclaim_scope\tlimitations";

const std::unordered_set<std::string>& config_keys() {
    static const std::unordered_set<std::string> keys{
        "claim_ledger.model_version",
        "package.package_id",
        "package.status",
        "package.economic_cluster_id",
        "package.economic_cluster_boundary_status",
        "package.economic_cluster_boundary_input_status",
        "package.economic_cluster_boundary_known_at_period",
        "package.economic_cluster_boundary_source_record_id",
        "claim.claim_id",
        "claim.project_id",
        "claim.instrument_kind",
        "claim.obligor_id",
        "claim.obligor_scope_note",
        "claim.investor_id",
        "claim.investor_scope_note",
        "claim.currency_label",
        "claim.monetary_basis",
        "claim.conversion_unit_label",
        "claim.conversion_unit_basis",
        "timeline.period_unit",
        "timeline.periods_per_year",
        "timeline.execution_date_status",
        "timeline.execution_date",
        "timeline.execution_date_source_record_id",
        "timeline.funding_date_status",
        "timeline.funding_date",
        "timeline.funding_date_source_record_id",
        "timeline.settlement_date_status",
        "timeline.settlement_date",
        "timeline.settlement_date_source_record_id",
        "timeline.observation_date_status",
        "timeline.observation_date",
        "timeline.observation_date_source_record_id",
        "timeline.decision_date_status",
        "timeline.decision_date",
        "timeline.decision_date_source_record_id",
        "timeline.maturity_date_status",
        "timeline.maturity_date",
        "timeline.maturity_date_source_record_id",
        "timeline.horizon_date_status",
        "timeline.horizon_date",
        "timeline.horizon_date_source_record_id",
        "timeline.period_origin_date_status",
        "timeline.period_origin_date",
        "timeline.period_origin_date_source_record_id",
        "timeline.decision_period_status",
        "timeline.decision_period",
        "timeline.horizon_period_status",
        "timeline.horizon_period",
        "claim.contractual_face_amount_status",
        "claim.contractual_face_amount_lower_million",
        "claim.contractual_face_amount_upper_million",
        "claim.contractual_face_amount_known_at_period_status",
        "claim.contractual_face_amount_known_at_period",
        "claim.contractual_face_amount_input_status",
        "claim.contractual_face_amount_source_record_id",
        "claim.opening_principal_status",
        "claim.opening_principal_lower_million",
        "claim.opening_principal_upper_million",
        "claim.opening_principal_known_at_period_status",
        "claim.opening_principal_known_at_period",
        "claim.opening_principal_input_status",
        "claim.opening_principal_source_record_id",
        "claim.opening_accrued_interest_status",
        "claim.opening_accrued_interest_lower_million",
        "claim.opening_accrued_interest_upper_million",
        "claim.opening_accrued_interest_known_at_period_status",
        "claim.opening_accrued_interest_known_at_period",
        "claim.opening_accrued_interest_input_status",
        "claim.opening_accrued_interest_source_record_id",
        "valuation.annual_effective_discount_rate_status",
        "valuation.annual_effective_discount_rate_lower",
        "valuation.annual_effective_discount_rate_upper",
        "valuation.annual_effective_discount_rate_known_at_period_status",
        "valuation.annual_effective_discount_rate_known_at_period",
        "valuation.annual_effective_discount_rate_input_status",
        "valuation.annual_effective_discount_rate_source_record_id",
        "source_manifest.sha256",
        "file.terms.path",
        "file.terms.sha256",
        "file.common_entries.path",
        "file.common_entries.sha256",
        "file.scenarios.path",
        "file.scenarios.sha256",
        "file.scenario_entries.path",
        "file.scenario_entries.sha256",
        "file.provider_claims.path",
        "file.provider_claims.sha256",
        "file.covenant_events.path",
        "file.covenant_events.sha256",
        "file.conversion_context.path",
        "file.conversion_context.sha256",
    };
    return keys;
}

[[noreturn]] void invalid(std::string message) {
    throw std::invalid_argument("claim-ledger package: " + std::move(message));
}

[[nodiscard]] std::string trim(std::string_view value) {
    std::size_t first = 0U;
    while (first < value.size() &&
           (value[first] == ' ' || value[first] == '\t' ||
            value[first] == '\r' || value[first] == '\n')) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           (value[last - 1U] == ' ' || value[last - 1U] == '\t' ||
            value[last - 1U] == '\r' || value[last - 1U] == '\n')) {
        --last;
    }
    return std::string(value.substr(first, last - first));
}

[[nodiscard]] bool contains_unsafe_control(
    std::string_view value, bool allow_tab = false) noexcept {
    constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF"};
    if (value.find(kUtf8Bom) != std::string_view::npos) {
        return true;
    }
    return std::any_of(value.begin(), value.end(), [allow_tab](char raw) {
        const auto character = static_cast<unsigned char>(raw);
        return (character < 0x20U && !(allow_tab && character == 0x09U)) ||
            character == 0x7fU;
    });
}

void require_safe_text(
    std::string_view value, std::string_view label,
    std::size_t maximum_size = 4'096U) {
    if (value.empty() || value.size() > maximum_size ||
        value.front() == ' ' || value.back() == ' ' ||
        contains_unsafe_control(value)) {
        invalid(std::string(label) + " is empty, oversized, or unsafe");
    }
}

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

void require_safe_identifier(
    std::string_view value, std::string_view label) {
    if (value.empty() || value.size() > 128U ||
        !is_ascii_alphanumeric(value.front()) ||
        !std::all_of(value.begin() + 1U, value.end(), [](char character) {
            return is_ascii_alphanumeric(character) || character == '-' ||
                character == '_' || character == '.';
        })) {
        invalid(std::string(label) + " is not a safe bounded identifier");
    }
}

[[nodiscard]] bool is_leap_year(int year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

[[nodiscard]] bool is_iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4U] != '-' || value[7U] != '-') {
        return false;
    }
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (index == 4U || index == 7U) {
            continue;
        }
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    const auto number = [](std::string_view text) noexcept {
        int result = 0;
        for (char character : text) {
            result = result * 10 + (character - '0');
        }
        return result;
    };
    const int year = number(value.substr(0U, 4U));
    const int month = number(value.substr(5U, 2U));
    const int day = number(value.substr(8U, 2U));
    if (year == 0 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    constexpr std::array<int, 12U> days{{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    }};
    const int maximum = month == 2 && is_leap_year(year)
        ? 29
        : days[static_cast<std::size_t>(month - 1)];
    return day <= maximum;
}

struct IsoDateParts {
    int year{};
    int month{};
    int day{};
};

[[nodiscard]] IsoDateParts parse_iso_date_parts(
    std::string_view value) noexcept {
    const auto number = [](std::string_view text) noexcept {
        int result = 0;
        for (const char character : text) {
            result = result * 10 + (character - '0');
        }
        return result;
    };
    return IsoDateParts{number(value.substr(0U, 4U)),
        number(value.substr(5U, 2U)), number(value.substr(8U, 2U))};
}

[[nodiscard]] int days_in_month(int year, int month) noexcept {
    constexpr std::array<int, 12U> days{{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    }};
    return month == 2 && is_leap_year(year)
        ? 29
        : days[static_cast<std::size_t>(month - 1)];
}

[[nodiscard]] bool date_is_month_offset(std::string_view origin,
    std::string_view target, std::size_t months) noexcept {
    const IsoDateParts first = parse_iso_date_parts(origin);
    const IsoDateParts second = parse_iso_date_parts(target);
    const std::uint64_t total_month =
        static_cast<std::uint64_t>(first.year) * 12U +
        static_cast<std::uint64_t>(first.month - 1) + months;
    const int expected_year = static_cast<int>(total_month / 12U);
    const int expected_month = static_cast<int>(total_month % 12U) + 1;
    if (expected_year < 1 || expected_year > 9'999) return false;
    const int expected_day =
        std::min(first.day, days_in_month(expected_year, expected_month));
    return second.year == expected_year && second.month == expected_month &&
        second.day == expected_day;
}

[[nodiscard]] bool date_not_after(
    std::string_view first, std::string_view second) noexcept {
    const IsoDateParts left = parse_iso_date_parts(first);
    const IsoDateParts right = parse_iso_date_parts(second);
    if (left.year != right.year) return left.year < right.year;
    if (left.month != right.month) return left.month < right.month;
    return left.day <= right.day;
}

[[nodiscard]] bool date_in_month_period(std::string_view origin,
    std::string_view target, std::size_t period) noexcept {
    const IsoDateParts first = parse_iso_date_parts(origin);
    const IsoDateParts candidate = parse_iso_date_parts(target);
    const auto shifted = [&](std::size_t months,
                             IsoDateParts& output) noexcept {
        const std::uint64_t total_month =
            static_cast<std::uint64_t>(first.year) * 12U +
            static_cast<std::uint64_t>(first.month - 1) + months;
        const int year = static_cast<int>(total_month / 12U);
        const int month = static_cast<int>(total_month % 12U) + 1;
        if (year < 1 || year > 9'999) return false;
        output = IsoDateParts{year, month,
            std::min(first.day, days_in_month(year, month))};
        return true;
    };
    IsoDateParts lower;
    IsoDateParts upper;
    if (!shifted(period, lower) || !shifted(period + 1U, upper)) {
        return false;
    }
    const auto ordinal = [](const IsoDateParts& value) noexcept {
        return std::array<int, 3U>{value.year, value.month, value.day};
    };
    return ordinal(lower) <= ordinal(candidate) &&
        ordinal(candidate) < ordinal(upper);
}

[[nodiscard]] bool is_lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

[[nodiscard]] double parse_double(
    std::string_view value, std::string_view label) {
    if (value.empty()) {
        invalid(std::string(label) + " is blank");
    }
    double result = 0.0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        !std::isfinite(result) || (result == 0.0 && std::signbit(result))) {
        invalid(std::string(label) + " must be one finite canonical number");
    }
    return result;
}

[[nodiscard]] std::size_t parse_period(
    std::string_view value, std::string_view label) {
    if (value.empty() || value.front() == '+' || value.front() == '-') {
        invalid(std::string(label) + " must be a non-negative integer period");
    }
    std::uint64_t parsed_value = 0U;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, parsed_value);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        parsed_value > kClaimLedgerMaximumPeriods) {
        invalid(std::string(label) + " exceeds the supported period horizon");
    }
    return static_cast<std::size_t>(parsed_value);
}

[[nodiscard]] std::optional<std::size_t> parse_optional_period(
    std::string_view value, std::string_view label) {
    if (value == "UNKNOWN") {
        return std::nullopt;
    }
    if (value == "NOT_APPLICABLE" || value.empty()) {
        invalid(std::string(label) + " must be an integer or UNKNOWN");
    }
    return parse_period(value, label);
}

[[nodiscard]] std::optional<std::size_t> parse_typed_period(
    std::string_view status, std::string_view value,
    std::string_view label) {
    if (status == "known") {
        if (value == "UNKNOWN" || value == "NOT_APPLICABLE") {
            invalid(std::string(label) + " known status requires an integer");
        }
        return parse_period(value, label);
    }
    if (status == "unknown") {
        if (value != "UNKNOWN") {
            invalid(std::string(label) + " unknown status requires UNKNOWN");
        }
        return std::nullopt;
    }
    invalid(std::string(label) + " status must be known or unknown");
}

[[nodiscard]] ClaimLedgerValueStatus parse_value_status(
    std::string_view value, std::string_view label) {
    if (value == "known") {
        return ClaimLedgerValueStatus::Known;
    }
    if (value == "bounded") {
        return ClaimLedgerValueStatus::Bounded;
    }
    if (value == "unknown") {
        return ClaimLedgerValueStatus::Unknown;
    }
    if (value == "not-applicable") {
        return ClaimLedgerValueStatus::NotApplicable;
    }
    invalid(std::string(label) + " has an unknown value status");
}

[[nodiscard]] ClaimLedgerValue parse_typed_number(
    std::string_view status_text, std::string_view lower_text,
    std::string_view upper_text, std::string_view label, double maximum,
    double minimum = 0.0) {
    const ClaimLedgerValueStatus status =
        parse_value_status(status_text, label);
    if (status == ClaimLedgerValueStatus::Unknown) {
        if (lower_text != "UNKNOWN" || upper_text != "UNKNOWN") {
            invalid(std::string(label) +
                    " unknown status requires two UNKNOWN cells");
        }
        return claim_ledger_unknown();
    }
    if (status == ClaimLedgerValueStatus::NotApplicable) {
        if (lower_text != "NOT_APPLICABLE" ||
            upper_text != "NOT_APPLICABLE") {
            invalid(std::string(label) +
                    " not-applicable status requires two NOT_APPLICABLE cells");
        }
        return claim_ledger_not_applicable();
    }
    if (lower_text == "UNKNOWN" || upper_text == "UNKNOWN" ||
        lower_text == "NOT_APPLICABLE" ||
        upper_text == "NOT_APPLICABLE") {
        invalid(std::string(label) + " numeric status requires two numbers");
    }
    const double lower = parse_double(lower_text, label);
    const double upper = parse_double(upper_text, label);
    if (lower < minimum || upper < minimum || lower > maximum ||
        upper > maximum) {
        invalid(std::string(label) + " is outside its numeric guardrail");
    }
    if (status == ClaimLedgerValueStatus::Known) {
        if (lower != upper) {
            invalid(std::string(label) + " known endpoints must be equal");
        }
        return claim_ledger_known(lower);
    }
    if (!(lower < upper)) {
        invalid(std::string(label) +
                " bounded endpoints must be strictly increasing");
    }
    return claim_ledger_bounded(lower, upper);
}

[[nodiscard]] ClaimLedgerInputStatus parse_input_status(
    std::string_view value, std::string_view label) {
    if (value == "observed") return ClaimLedgerInputStatus::Observed;
    if (value == "contractual") return ClaimLedgerInputStatus::Contractual;
    if (value == "derived") return ClaimLedgerInputStatus::Derived;
    if (value == "estimated") return ClaimLedgerInputStatus::Estimated;
    if (value == "stress") return ClaimLedgerInputStatus::Stress;
    if (value == "backtest") return ClaimLedgerInputStatus::Backtest;
    if (value == "unknown") return ClaimLedgerInputStatus::Unknown;
    invalid(std::string(label) + " has an unknown input_status");
}

[[nodiscard]] ClaimLedgerPackageStatus parse_package_status(
    std::string_view value) {
    if (value == "retained-public-incomplete") {
        return ClaimLedgerPackageStatus::RetainedPublicIncomplete;
    }
    if (value == "synthetic-complete") {
        return ClaimLedgerPackageStatus::SyntheticComplete;
    }
    if (value == "controlled-candidate") {
        return ClaimLedgerPackageStatus::ControlledCandidate;
    }
    invalid("package.status is outside the closed v0.1 vocabulary");
}

[[nodiscard]] ClaimLedgerEconomicClusterBoundaryStatus
parse_cluster_boundary(std::string_view value) {
    if (value == "defined") {
        return ClaimLedgerEconomicClusterBoundaryStatus::Defined;
    }
    if (value == "unresolved") {
        return ClaimLedgerEconomicClusterBoundaryStatus::Unresolved;
    }
    invalid("economic cluster boundary status must be defined or unresolved");
}

void validate_source_token(std::string_view value, std::string_view label) {
    require_safe_identifier(value, label);
    if (value == "NONE" || value == "NOT_APPLICABLE") {
        invalid(std::string(label) + " is not a source-record marker");
    }
}

[[nodiscard]] ClaimLedgerTypedDate parse_typed_date(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view name);

[[nodiscard]] std::string read_regular_file(
    const std::filesystem::path& path, std::uintmax_t maximum_bytes,
    std::string_view label) {
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || !std::filesystem::is_regular_file(path, error) || error) {
        invalid(std::string(label) + " is missing or is not a regular file");
    }
    if (size > maximum_bytes ||
        size > static_cast<std::uintmax_t>(
                   std::numeric_limits<std::size_t>::max())) {
        invalid(std::string(label) + " exceeds its byte cap");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        invalid("could not open " + std::string(label));
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!bytes.empty()) {
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    if (!input || input.peek() != std::char_traits<char>::eof()) {
        invalid(std::string(label) + " changed or could not be read exactly");
    }
    return bytes;
}

[[nodiscard]] std::unordered_map<std::string, std::string>
parse_config_bytes(std::string_view bytes) {
    std::unordered_map<std::string, std::string> values;
    std::size_t position = 0U;
    std::size_t line_number = 0U;
    while (position < bytes.size()) {
        const std::size_t newline = bytes.find('\n', position);
        const std::size_t end = newline == std::string_view::npos
            ? bytes.size()
            : newline;
        std::string_view raw = bytes.substr(position, end - position);
        if (!raw.empty() && raw.back() == '\r') {
            raw.remove_suffix(1U);
        }
        ++line_number;
        if (raw.size() > kMaximumConfigLineBytes) {
            invalid("claim.cfg line " + std::to_string(line_number) +
                    " exceeds 8192 bytes");
        }
        const std::string line = trim(raw);
        if (!line.empty() && line.front() != '#') {
            if (contains_unsafe_control(line)) {
                invalid("unsafe claim.cfg line " +
                        std::to_string(line_number));
            }
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos ||
                line.find('=', separator + 1U) != std::string::npos) {
                invalid("claim.cfg line " + std::to_string(line_number) +
                        " must contain exactly one '='");
            }
            const std::string key = trim(
                std::string_view(line).substr(0U, separator));
            const std::string value = trim(
                std::string_view(line).substr(separator + 1U));
            if (!config_keys().contains(key)) {
                invalid("unknown claim.cfg key: " + key);
            }
            if (value.empty()) {
                invalid("empty claim.cfg value for key: " + key);
            }
            if (!values.emplace(key, value).second) {
                invalid("duplicate claim.cfg key: " + key);
            }
        }
        if (newline == std::string_view::npos) break;
        position = newline + 1U;
    }
    for (const std::string& key : config_keys()) {
        if (!values.contains(key)) {
            invalid("missing claim.cfg key: " + key);
        }
    }
    return values;
}

[[nodiscard]] const std::string& required(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view key) {
    const auto found = values.find(std::string(key));
    if (found == values.end()) {
        invalid("internal missing claim.cfg key: " + std::string(key));
    }
    return found->second;
}

[[nodiscard]] ClaimLedgerTypedDate parse_typed_date(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view name) {
    const std::string prefix = "timeline." + std::string(name) + "_date";
    const std::string& status = required(values, prefix + "_status");
    const std::string& date = required(values, prefix);
    ClaimLedgerTypedDate result;
    if (status == "known") {
        if (!is_iso_date(date)) {
            invalid(prefix + " known value must be an ISO YYYY-MM-DD date");
        }
        result.status = ClaimLedgerDateStatus::Known;
        result.value = date;
    } else if (status == "unknown") {
        if (date != "UNKNOWN") {
            invalid(prefix + " unknown status requires UNKNOWN");
        }
        result.status = ClaimLedgerDateStatus::Unknown;
    } else {
        invalid(prefix + " status must be known or unknown");
    }
    result.source_record_id = required(values, prefix + "_source_record_id");
    validate_source_token(result.source_record_id, prefix + " source_record_id");
    return result;
}

[[nodiscard]] std::filesystem::path parse_portable_relative_path(
    std::string_view raw_path, std::string_view label) {
    require_safe_text(raw_path, label, 512U);
    if (raw_path.find('\\') != std::string::npos ||
        raw_path.find(':') != std::string::npos) {
        invalid(std::string(label) + " must use a portable relative path");
    }
    const std::filesystem::path result{std::string(raw_path)};
    if (result.empty() || result.is_absolute() || result.has_root_name() ||
        result.has_root_directory()) {
        invalid(std::string(label) + " must be relative");
    }
    for (const auto& component : result) {
        if (component.empty() || component == "." || component == "..") {
            invalid(std::string(label) + " contains an unsafe component");
        }
    }
    if (!std::all_of(raw_path.begin(), raw_path.end(), [](char character) {
            return is_ascii_alphanumeric(character) || character == '-' ||
                character == '_' || character == '.' || character == '/';
        })) {
        invalid(std::string(label) + " contains a non-portable character");
    }
    return result;
}

[[nodiscard]] ClaimLedgerBoundFile parse_bound_file(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view stem) {
    const std::string prefix = "file." + std::string(stem);
    const std::string& raw_path = required(values, prefix + ".path");
    ClaimLedgerBoundFile result;
    result.relative_path =
        parse_portable_relative_path(raw_path, prefix + ".path");
    result.sha256 = required(values, prefix + ".sha256");
    if (!is_lower_hex_sha256(result.sha256)) {
        invalid(prefix + ".sha256 must be 64 lowercase hexadecimal digits");
    }
    return result;
}

[[nodiscard]] ClaimLedgerPackageScalar parse_package_scalar(
    const std::unordered_map<std::string, std::string>& values,
    std::string_view prefix, std::string_view lower_suffix,
    std::string_view upper_suffix, double maximum, double minimum = 0.0) {
    ClaimLedgerPackageScalar result;
    result.value = parse_typed_number(
        required(values, std::string(prefix) + "_status"),
        required(values, std::string(prefix) + std::string(lower_suffix)),
        required(values, std::string(prefix) + std::string(upper_suffix)),
        prefix, maximum, minimum);
    result.known_at_period = parse_typed_period(
        required(values, std::string(prefix) + "_known_at_period_status"),
        required(values, std::string(prefix) + "_known_at_period"),
        std::string(prefix) + "_known_at_period");
    result.input_status = parse_input_status(
        required(values, std::string(prefix) + "_input_status"),
        std::string(prefix) + "_input_status");
    result.source_record_id =
        required(values, std::string(prefix) + "_source_record_id");
    validate_source_token(
        result.source_record_id,
        std::string(prefix) + "_source_record_id");
    return result;
}

void validate_config_identity(ClaimLedgerPackageConfig& config) {
    if (config.model_version != kClaimLedgerPackageVersion ||
        config.model_version != kClaimLedgerModelVersion) {
        invalid("claim_ledger.model_version must be 0.1.0");
    }
    require_safe_identifier(config.package_id, "package.package_id");
    require_safe_identifier(
        config.economic_cluster_id, "package.economic_cluster_id");
    require_safe_identifier(config.claim_id, "claim.claim_id");
    require_safe_identifier(config.project_id, "claim.project_id");
    require_safe_identifier(config.instrument_kind, "claim.instrument_kind");
    require_safe_identifier(config.obligor_id, "claim.obligor_id");
    require_safe_text(
        config.obligor_scope_note, "claim.obligor_scope_note", 2'048U);
    require_safe_identifier(config.investor_id, "claim.investor_id");
    require_safe_text(
        config.investor_scope_note, "claim.investor_scope_note", 2'048U);
    require_safe_identifier(config.currency_label, "claim.currency_label");
    require_safe_text(config.monetary_basis, "claim.monetary_basis", 512U);
    require_safe_identifier(
        config.conversion_unit_label, "claim.conversion_unit_label");
    require_safe_text(config.conversion_unit_basis,
        "claim.conversion_unit_basis", 512U);
    if ((config.conversion_unit_label == "not-applicable") !=
        (config.conversion_unit_basis == "not-applicable")) {
        invalid("conversion unit label and basis must both be applicable or both be not-applicable");
    }
    if (config.period_unit_label != "month" ||
        config.periods_per_year != 12U) {
        invalid("v0.1 timeline period unit must be month with 12 periods per year");
    }
    if (config.decision_period.has_value() &&
        config.horizon_period.has_value() &&
        *config.decision_period > *config.horizon_period) {
        invalid("decision period follows horizon period");
    }
    if (config.period_origin_date.status == ClaimLedgerDateStatus::Known) {
        const auto require_mapping = [&](const ClaimLedgerTypedDate& date,
                                         const std::optional<std::size_t>& period,
                                         std::string_view label) {
            if (date.status != ClaimLedgerDateStatus::Known ||
                !period.has_value()) {
                return;
            }
            if (!date_is_month_offset(*config.period_origin_date.value,
                    *date.value, *period)) {
                invalid(std::string(label) +
                    " does not equal period_origin_date plus its monthly period");
            }
        };
        require_mapping(
            config.decision_date, config.decision_period, "decision_date");
        require_mapping(
            config.horizon_date, config.horizon_period, "horizon_date");
    }
    const auto require_order = [](const ClaimLedgerTypedDate& first,
                                  const ClaimLedgerTypedDate& second,
                                  std::string_view description) {
        if (first.status == ClaimLedgerDateStatus::Known &&
            second.status == ClaimLedgerDateStatus::Known &&
            !date_not_after(*first.value, *second.value)) {
            invalid(std::string(description) +
                " has a contradictory known-date order");
        }
    };
    require_order(config.decision_date, config.execution_date,
        "decision_date <= execution_date");
    require_order(config.execution_date, config.funding_date,
        "execution_date <= funding_date");
    require_order(config.execution_date, config.settlement_date,
        "execution_date <= settlement_date");
    require_order(config.decision_date, config.funding_date,
        "decision_date <= funding_date");
    require_order(config.decision_date, config.settlement_date,
        "decision_date <= settlement_date");
    require_order(config.decision_date, config.observation_date,
        "decision_date <= observation_date");
    require_order(config.funding_date, config.maturity_date,
        "funding_date <= maturity_date");
    require_order(config.settlement_date, config.maturity_date,
        "settlement_date <= maturity_date");
    require_order(config.decision_date, config.horizon_date,
        "decision_date <= horizon_date");
    require_order(config.execution_date, config.horizon_date,
        "execution_date <= horizon_date");
    require_order(config.funding_date, config.horizon_date,
        "funding_date <= horizon_date");
    require_order(config.settlement_date, config.horizon_date,
        "settlement_date <= horizon_date");
    require_order(config.maturity_date, config.horizon_date,
        "maturity_date <= horizon_date");
    const std::array<const ClaimLedgerBoundFile*, 7U> files{{
        &config.terms, &config.common_entries, &config.scenarios,
        &config.scenario_entries, &config.provider_claims,
        &config.covenant_events, &config.conversion_context,
    }};
    std::unordered_set<std::string> paths;
    for (const ClaimLedgerBoundFile* file : files) {
        if (!paths.insert(file->relative_path.generic_string()).second) {
            invalid("every bound TSV path must be distinct");
        }
    }
}

struct ParsedEntry {
    std::optional<std::string> scenario_id{};
    std::string entry_id{};
    std::string economic_fact_id{};
    std::string event_group_id{};
    ClaimLedgerEntryKind kind{ClaimLedgerEntryKind::BuyerPrice};
    std::optional<std::size_t> period{};
    std::optional<std::size_t> known_at_period{};
    ClaimLedgerValue value{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    std::string source_record_id{};
    std::string provider_claim_id{};
};

struct ParsedScenario {
    std::string scenario_id{};
    ClaimLedgerValue probability{};
    std::optional<std::size_t> probability_known_at_period{};
    ClaimLedgerCashPathStatus cash_path_status{
        ClaimLedgerCashPathStatus::Incomplete};
    std::optional<std::size_t> cash_path_status_known_at_period{};
    bool cash_path_status_unknown{};
    ClaimLedgerInputStatus probability_input_status{
        ClaimLedgerInputStatus::Unknown};
    std::string probability_source_record_id{};
    ClaimLedgerInputStatus cash_path_input_status{
        ClaimLedgerInputStatus::Unknown};
    std::string cash_path_status_source_record_id{};
};

struct ParsedProvider {
    std::string provider_claim_id{};
    std::string provider_id{};
    std::optional<std::size_t> known_at_period{};
    ClaimLedgerValue shortfall_allocation{};
    ClaimLedgerValue coverage{};
    ClaimLedgerValue deductible{};
    ClaimLedgerValue maximum_cash{};
    ClaimLedgerValue settlement_lag{};
    std::optional<bool> covers_principal_due{};
    std::optional<bool> covers_interest_due{};
    std::optional<bool> payment_right_evidenced{};
    std::optional<bool> provider_identity_evidenced{};
    std::optional<bool> coverage_and_priority_evidenced{};
    std::optional<ClaimLedgerProviderAllocationPriority> obligation_priority{};
    bool obligation_priority_unknown{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    std::string source_record_id{};
};

struct ParsedCovenant {
    bool common{};
    std::optional<std::string> scenario_id{};
    std::string event_id{};
    std::string covenant_id{};
    std::optional<std::size_t> period{};
    ClaimLedgerTypedDate event_date{};
    std::optional<std::size_t> known_at_period{};
    ClaimLedgerCovenantState state{ClaimLedgerCovenantState::Pass};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    std::string source_record_id{};
};

struct ParsedTables {
    std::vector<ParsedEntry> entries{};
    std::vector<ParsedScenario> scenarios{};
    std::vector<ParsedProvider> providers{};
    std::vector<ParsedCovenant> covenants{};
    std::unordered_set<std::string> source_record_ids{};
};

[[nodiscard]] std::vector<std::string_view> split_tabs(
    std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t position = 0U;
    for (;;) {
        const std::size_t tab = line.find('\t', position);
        const std::size_t end =
            tab == std::string_view::npos ? line.size() : tab;
        fields.push_back(line.substr(position, end - position));
        if (tab == std::string_view::npos) break;
        position = tab + 1U;
    }
    return fields;
}

template <typename Function>
std::size_t parse_tsv(
    std::string_view bytes, std::string_view expected_header,
    std::size_t expected_fields, std::size_t maximum_rows,
    std::string_view label, Function&& function) {
    if (bytes.empty()) {
        invalid(std::string(label) + " is empty");
    }
    const std::size_t first_newline = bytes.find('\n');
    const std::size_t first_end = first_newline == std::string_view::npos
        ? bytes.size()
        : first_newline;
    std::string_view header = bytes.substr(0U, first_end);
    if (!header.empty() && header.back() == '\r') header.remove_suffix(1U);
    if (header != expected_header) {
        invalid(std::string(label) + " TSV header is not exact");
    }
    std::size_t rows = 0U;
    std::size_t position = first_newline == std::string_view::npos
        ? bytes.size()
        : first_newline + 1U;
    std::size_t line_number = 1U;
    while (position < bytes.size()) {
        const std::size_t newline = bytes.find('\n', position);
        const std::size_t end = newline == std::string_view::npos
            ? bytes.size()
            : newline;
        std::string_view line = bytes.substr(position, end - position);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1U);
        ++line_number;
        if (line.empty()) {
            invalid(std::string(label) + " contains a blank row at line " +
                    std::to_string(line_number));
        }
        if (line.size() > kMaximumTsvLineBytes ||
            contains_unsafe_control(line, true)) {
            invalid(std::string(label) + " has an oversized or unsafe line " +
                    std::to_string(line_number));
        }
        if (rows >= maximum_rows) {
            invalid(std::string(label) + " exceeds its row cap");
        }
        const std::vector<std::string_view> fields = split_tabs(line);
        if (fields.size() != expected_fields) {
            invalid(std::string(label) + " line " +
                    std::to_string(line_number) + " must have exactly " +
                    std::to_string(expected_fields) + " fields");
        }
        for (std::string_view field : fields) {
            if (field.empty()) {
                invalid(std::string(label) + " line " +
                        std::to_string(line_number) +
                        " contains a blank field");
            }
        }
        function(fields, line_number);
        ++rows;
        if (newline == std::string_view::npos) break;
        position = newline + 1U;
    }
    return rows;
}

void add_blocker(std::vector<std::string>& blockers, std::string message) {
    constexpr std::string_view omitted_prefix =
        "additional blockers omitted after report cap: ";
    if (!blockers.empty() && blockers.back().starts_with(omitted_prefix)) {
        std::size_t omitted = 0U;
        const std::string_view count =
            std::string_view(blockers.back()).substr(omitted_prefix.size());
        const auto parsed = std::from_chars(
            count.data(), count.data() + count.size(), omitted);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != count.data() + count.size()) {
            throw std::logic_error("invalid internal blocker omission count");
        }
        if (omitted < std::numeric_limits<std::size_t>::max()) ++omitted;
        blockers.back() = std::string(omitted_prefix) +
            std::to_string(omitted);
        return;
    }
    if (std::find(blockers.begin(), blockers.end(), message) !=
        blockers.end()) {
        return;
    }
    std::size_t bytes = 0U;
    for (const std::string& blocker : blockers) bytes += blocker.size();
    if (blockers.size() + 1U <
            kMaximumReportedBlockers - kReservedCriticalBlockers &&
        message.size() <= kMaximumReportedBlockerBytes -
                std::min(bytes, kMaximumReportedBlockerBytes)) {
        blockers.push_back(std::move(message));
        return;
    }
    blockers.push_back(std::string(omitted_prefix) + "1");
}

void add_critical_blocker(
    std::vector<std::string>& blockers, std::string message) {
    if (std::find(blockers.begin(), blockers.end(), message) !=
        blockers.end()) {
        return;
    }
    if (blockers.size() >= kMaximumReportedBlockers) {
        throw std::logic_error(
            "critical blocker reserve is smaller than the admission gate set");
    }
    blockers.push_back(std::move(message));
}

void collect_source(
    ParsedTables& tables, std::string_view source_record_id,
    std::string_view label) {
    validate_source_token(source_record_id, label);
    tables.source_record_ids.emplace(source_record_id);
}

[[nodiscard]] ClaimLedgerEntryKind parse_entry_kind(std::string_view value) {
    if (value == "buyer-price") return ClaimLedgerEntryKind::BuyerPrice;
    if (value == "buyer-direct-cost") return ClaimLedgerEntryKind::BuyerDirectCost;
    if (value == "borrower-gross-proceeds") return ClaimLedgerEntryKind::BorrowerGrossProceeds;
    if (value == "borrower-net-proceeds") return ClaimLedgerEntryKind::BorrowerNetProceeds;
    if (value == "cash-fee") return ClaimLedgerEntryKind::CashFee;
    if (value == "borrower-third-party-cost") return ClaimLedgerEntryKind::BorrowerThirdPartyCost;
    if (value == "funded-principal") return ClaimLedgerEntryKind::FundedPrincipal;
    if (value == "original-issue-discount") return ClaimLedgerEntryKind::OriginalIssueDiscount;
    if (value == "original-issue-premium") return ClaimLedgerEntryKind::OriginalIssuePremium;
    if (value == "capitalized-fee") return ClaimLedgerEntryKind::CapitalizedFee;
    if (value == "interest-accrual") return ClaimLedgerEntryKind::InterestAccrual;
    if (value == "capitalized-interest") return ClaimLedgerEntryKind::CapitalizedInterest;
    if (value == "principal-due") return ClaimLedgerEntryKind::PrincipalDue;
    if (value == "interest-due") return ClaimLedgerEntryKind::InterestDue;
    if (value == "principal-cash") return ClaimLedgerEntryKind::PrincipalCash;
    if (value == "interest-cash") return ClaimLedgerEntryKind::InterestCash;
    if (value == "recovery-principal-cash") return ClaimLedgerEntryKind::RecoveryPrincipalCash;
    if (value == "recovery-interest-cash") return ClaimLedgerEntryKind::RecoveryInterestCash;
    if (value == "conversion-principal-extinguishment") return ClaimLedgerEntryKind::ConversionPrincipalExtinguishment;
    if (value == "conversion-interest-extinguishment") return ClaimLedgerEntryKind::ConversionInterestExtinguishment;
    if (value == "conversion-units") return ClaimLedgerEntryKind::ConversionUnits;
    if (value == "principal-writeoff") return ClaimLedgerEntryKind::PrincipalWriteoff;
    if (value == "accrued-interest-writeoff") return ClaimLedgerEntryKind::AccruedInterestWriteoff;
    if (value == "guarantee-principal-cash") return ClaimLedgerEntryKind::GuaranteePrincipalCash;
    if (value == "guarantee-interest-cash") return ClaimLedgerEntryKind::GuaranteeInterestCash;
    invalid("unknown claim-ledger entry kind: " + std::string(value));
}

[[nodiscard]] bool guarantee_kind(ClaimLedgerEntryKind value) noexcept {
    return value == ClaimLedgerEntryKind::GuaranteePrincipalCash ||
        value == ClaimLedgerEntryKind::GuaranteeInterestCash;
}

[[nodiscard]] ClaimLedgerCovenantState parse_covenant_state(
    std::string_view value) {
    if (value == "pass") return ClaimLedgerCovenantState::Pass;
    if (value == "breach") return ClaimLedgerCovenantState::Breach;
    if (value == "breach-with-cure") return ClaimLedgerCovenantState::BreachWithCure;
    if (value == "breach-with-waiver") return ClaimLedgerCovenantState::BreachWithWaiver;
    if (value == "breach-with-non-exercise-consent") return ClaimLedgerCovenantState::BreachWithNonExerciseConsent;
    if (value == "default") return ClaimLedgerCovenantState::Default;
    if (value == "acceleration") return ClaimLedgerCovenantState::Acceleration;
    invalid("unknown covenant state: " + std::string(value));
}

[[nodiscard]] std::optional<bool> parse_optional_bool(
    std::string_view value, std::string_view label) {
    if (value == "true") return true;
    if (value == "false") return false;
    if (value == "UNKNOWN") return std::nullopt;
    invalid(std::string(label) + " must be true, false, or UNKNOWN");
}

[[nodiscard]] ClaimLedgerCashPathStatus parse_cash_path_status(
    std::string_view value, bool& unknown) {
    unknown = false;
    if (value == "complete-resolved") {
        return ClaimLedgerCashPathStatus::CompleteResolved;
    }
    if (value == "incomplete") {
        return ClaimLedgerCashPathStatus::Incomplete;
    }
    if (value == "UNKNOWN") {
        unknown = true;
        return ClaimLedgerCashPathStatus::Incomplete;
    }
    invalid("cash_path_status is outside the closed vocabulary");
}

[[nodiscard]] std::optional<ClaimLedgerProviderAllocationPriority>
parse_optional_priority(std::string_view value, bool& unknown) {
    unknown = false;
    if (value == "principal-first") {
        return ClaimLedgerProviderAllocationPriority::PrincipalFirst;
    }
    if (value == "interest-first") {
        return ClaimLedgerProviderAllocationPriority::InterestFirst;
    }
    if (value == "pro-rata") {
        return ClaimLedgerProviderAllocationPriority::ProRata;
    }
    if (value == "UNKNOWN") {
        unknown = true;
        return std::nullopt;
    }
    invalid("obligation_priority is outside the closed vocabulary");
}

[[nodiscard]] bool exact_known(const ClaimLedgerValue& value) noexcept {
    return value.status == ClaimLedgerValueStatus::Known &&
        value.lower.has_value() && value.upper.has_value();
}

} // namespace

namespace {

[[nodiscard]] ClaimLedgerPackageConfig parse_claim_ledger_package_config(
    std::string_view bytes) {
    const auto values = parse_config_bytes(bytes);

    ClaimLedgerPackageConfig config;
    config.model_version = required(values, "claim_ledger.model_version");
    config.package_id = required(values, "package.package_id");
    config.package_status =
        parse_package_status(required(values, "package.status"));
    config.economic_cluster_id =
        required(values, "package.economic_cluster_id");
    config.economic_cluster_boundary_status = parse_cluster_boundary(
        required(values, "package.economic_cluster_boundary_status"));
    config.economic_cluster_boundary_input_status = parse_input_status(
        required(values, "package.economic_cluster_boundary_input_status"),
        "package economic-cluster boundary");
    config.economic_cluster_boundary_known_at_period =
        parse_optional_period(required(values,
            "package.economic_cluster_boundary_known_at_period"),
            "package economic-cluster boundary known_at_period");
    config.economic_cluster_boundary_source_record_id = required(values,
        "package.economic_cluster_boundary_source_record_id");
    validate_source_token(
        config.economic_cluster_boundary_source_record_id,
        "package economic-cluster boundary source_record_id");
    config.claim_id = required(values, "claim.claim_id");
    config.project_id = required(values, "claim.project_id");
    config.instrument_kind = required(values, "claim.instrument_kind");
    config.obligor_id = required(values, "claim.obligor_id");
    config.obligor_scope_note =
        required(values, "claim.obligor_scope_note");
    config.investor_id = required(values, "claim.investor_id");
    config.investor_scope_note =
        required(values, "claim.investor_scope_note");
    config.currency_label = required(values, "claim.currency_label");
    config.monetary_basis = required(values, "claim.monetary_basis");
    config.conversion_unit_label =
        required(values, "claim.conversion_unit_label");
    config.conversion_unit_basis =
        required(values, "claim.conversion_unit_basis");
    config.period_unit_label = required(values, "timeline.period_unit");
    config.periods_per_year = parse_period(
        required(values, "timeline.periods_per_year"),
        "timeline.periods_per_year");

    config.execution_date = parse_typed_date(values, "execution");
    config.funding_date = parse_typed_date(values, "funding");
    config.settlement_date = parse_typed_date(values, "settlement");
    config.observation_date = parse_typed_date(values, "observation");
    config.decision_date = parse_typed_date(values, "decision");
    config.maturity_date = parse_typed_date(values, "maturity");
    config.horizon_date = parse_typed_date(values, "horizon");
    config.period_origin_date =
        parse_typed_date(values, "period_origin");
    config.decision_period = parse_typed_period(
        required(values, "timeline.decision_period_status"),
        required(values, "timeline.decision_period"),
        "timeline.decision_period");
    config.horizon_period = parse_typed_period(
        required(values, "timeline.horizon_period_status"),
        required(values, "timeline.horizon_period"),
        "timeline.horizon_period");

    config.contractual_face_amount_million = parse_package_scalar(
        values, "claim.contractual_face_amount", "_lower_million",
        "_upper_million", kMaximumMoneyMillion);
    config.opening_principal_million = parse_package_scalar(
        values, "claim.opening_principal", "_lower_million",
        "_upper_million", kMaximumMoneyMillion);
    config.opening_accrued_interest_million = parse_package_scalar(
        values, "claim.opening_accrued_interest", "_lower_million",
        "_upper_million", kMaximumMoneyMillion);
    config.annual_effective_discount_rate = parse_package_scalar(
        values, "valuation.annual_effective_discount_rate", "_lower",
        "_upper", kMaximumAnnualEffectiveRate, -1.0);
    if (config.annual_effective_discount_rate.value.lower.has_value() &&
        *config.annual_effective_discount_rate.value.lower <= -1.0) {
        invalid("valuation annual effective discount rate must be greater than -1");
    }

    config.terms = parse_bound_file(values, "terms");
    config.common_entries = parse_bound_file(values, "common_entries");
    config.scenarios = parse_bound_file(values, "scenarios");
    config.scenario_entries =
        parse_bound_file(values, "scenario_entries");
    config.provider_claims = parse_bound_file(values, "provider_claims");
    config.covenant_events = parse_bound_file(values, "covenant_events");
    config.conversion_context =
        parse_bound_file(values, "conversion_context");
    config.source_manifest_sha256 =
        required(values, "source_manifest.sha256");
    if (config.package_status == ClaimLedgerPackageStatus::SyntheticComplete) {
        if (config.source_manifest_sha256 != "NOT_APPLICABLE") {
            invalid("synthetic-complete source_manifest.sha256 must be NOT_APPLICABLE");
        }
    } else if (!is_lower_hex_sha256(config.source_manifest_sha256)) {
        invalid("non-synthetic source_manifest.sha256 must bind the parent manifest");
    }
    validate_config_identity(config);
    return config;
}

} // namespace

ClaimLedgerPackageConfig load_claim_ledger_package_config(
    const std::filesystem::path& path) {
    const std::string bytes =
        read_regular_file(path, kMaximumConfigBytes, "claim.cfg");
    return parse_claim_ledger_package_config(bytes);
}

namespace {

struct BoundSnapshot {
    std::filesystem::path canonical_path{};
    std::string bytes{};
};

[[nodiscard]] bool path_is_confined(
    const std::filesystem::path& child,
    const std::filesystem::path& directory) {
    const std::filesystem::path relative = child.lexically_relative(directory);
    if (relative.empty() || relative.is_absolute()) return false;
    return std::none_of(
        relative.begin(), relative.end(),
        [](const std::filesystem::path& part) { return part == ".."; });
}

[[nodiscard]] BoundSnapshot load_bound_snapshot(
    const std::filesystem::path& directory,
    const ClaimLedgerBoundFile& bound, std::string_view label) {
    std::error_code error;
    const std::filesystem::path target = std::filesystem::canonical(
        directory / bound.relative_path, error);
    if (error || !path_is_confined(target, directory)) {
        invalid(std::string(label) + " resolves outside the package directory");
    }
    BoundSnapshot result;
    result.canonical_path = target;
    result.bytes = read_regular_file(target, kMaximumTsvBytes, label);
    if (sha256_bytes_lower_hex(result.bytes) != bound.sha256) {
        invalid(std::string(label) + " SHA-256 mismatch");
    }
    const std::filesystem::path rechecked = std::filesystem::canonical(
        directory / bound.relative_path, error);
    if (error || rechecked != target) {
        invalid(std::string(label) + " resolved path changed during review");
    }
    return result;
}

void collect_config_sources(
    ParsedTables& tables, const ClaimLedgerPackageConfig& config) {
    collect_source(tables,
        config.economic_cluster_boundary_source_record_id,
        "economic-cluster boundary source_record_id");
    const std::array<const ClaimLedgerTypedDate*, 8U> dates{{
        &config.execution_date, &config.funding_date,
        &config.settlement_date, &config.observation_date,
        &config.decision_date, &config.maturity_date, &config.horizon_date,
        &config.period_origin_date,
    }};
    for (const ClaimLedgerTypedDate* date : dates) {
        collect_source(
            tables, date->source_record_id, "timeline source_record_id");
    }
    const std::array<const ClaimLedgerPackageScalar*, 4U> scalars{{
        &config.contractual_face_amount_million,
        &config.opening_principal_million,
        &config.opening_accrued_interest_million,
        &config.annual_effective_discount_rate,
    }};
    for (const ClaimLedgerPackageScalar* scalar : scalars) {
        collect_source(
            tables, scalar->source_record_id, "scalar source_record_id");
    }
}

void validate_textual_typed_value(
    std::string_view status_text, std::string_view value,
    std::string_view label) {
    const ClaimLedgerValueStatus status =
        parse_value_status(status_text, label);
    if (status == ClaimLedgerValueStatus::Unknown) {
        if (value != "UNKNOWN") {
            invalid(std::string(label) + " unknown status requires UNKNOWN");
        }
        return;
    }
    if (status == ClaimLedgerValueStatus::NotApplicable) {
        if (value != "NOT_APPLICABLE") {
            invalid(std::string(label) +
                    " not-applicable status requires NOT_APPLICABLE");
        }
        return;
    }
    if (value == "UNKNOWN" || value == "NOT_APPLICABLE") {
        invalid(std::string(label) + " known/bounded status requires a value");
    }
    require_safe_text(value, label, 4'096U);
}

void parse_terms(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables) {
    std::unordered_set<std::string> term_ids;
    package.row_counts.terms = parse_tsv(
        bytes, kTermsHeader, 7U, kMaximumTerms, "terms.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            require_safe_identifier(fields[0U], "term_id");
            if (!term_ids.emplace(fields[0U]).second) {
                invalid("duplicate terms.tsv term_id: " +
                        std::string(fields[0U]));
            }
            validate_textual_typed_value(
                fields[1U], fields[2U], "terms.tsv value");
            const ClaimLedgerInputStatus input =
                parse_input_status(fields[3U], "terms.tsv");
            static_cast<void>(
                parse_optional_period(fields[4U], "terms.tsv known_at_period"));
            collect_source(
                tables, fields[5U], "terms.tsv source_record_id");
            require_safe_text(fields[6U], "terms.tsv note", 4'096U);
            const ClaimLedgerValueStatus value_status =
                parse_value_status(fields[1U], "terms.tsv");
            if (value_status == ClaimLedgerValueStatus::Unknown) {
                add_blocker(package.blockers,
                    "term " + std::string(fields[0U]) + " remains UNKNOWN");
            }
            if (input == ClaimLedgerInputStatus::Unknown) {
                add_blocker(package.blockers,
                    "term " + std::string(fields[0U]) +
                    " has unknown input status");
            }
        });
}

[[nodiscard]] ParsedEntry parse_entry_fields(
    const std::vector<std::string_view>& fields, std::size_t offset,
    std::optional<std::string> scenario_id, std::string_view label,
    ParsedTables& tables, ClaimLedgerPackage& package) {
    ParsedEntry row;
    row.scenario_id = std::move(scenario_id);
    row.entry_id = std::string(fields[offset + 0U]);
    row.economic_fact_id = std::string(fields[offset + 1U]);
    row.event_group_id = std::string(fields[offset + 2U]);
    require_safe_identifier(row.entry_id, std::string(label) + " entry_id");
    require_safe_identifier(row.economic_fact_id,
        std::string(label) + " economic_fact_id");
    require_safe_identifier(
        row.event_group_id, std::string(label) + " event_group_id");
    row.kind = parse_entry_kind(fields[offset + 3U]);
    row.period = parse_optional_period(
        fields[offset + 4U], std::string(label) + " period");
    row.known_at_period = parse_optional_period(
        fields[offset + 5U], std::string(label) + " known_at_period");
    row.value = parse_typed_number(
        fields[offset + 6U], fields[offset + 7U], fields[offset + 8U],
        std::string(label) + " value", kMaximumMoneyMillion);
    if (row.value.status == ClaimLedgerValueStatus::NotApplicable) {
        invalid(std::string(label) +
                " monetary and conversion entries cannot be not-applicable");
    }
    row.input_status = parse_input_status(fields[offset + 9U], label);
    row.source_record_id = std::string(fields[offset + 10U]);
    collect_source(
        tables, row.source_record_id,
        std::string(label) + " source_record_id");
    row.provider_claim_id = std::string(fields[offset + 11U]);
    if (guarantee_kind(row.kind)) {
        require_safe_identifier(
            row.provider_claim_id,
            std::string(label) + " provider_claim_id");
        if (row.provider_claim_id == "NONE" ||
            row.provider_claim_id == "NOT_APPLICABLE") {
            invalid(std::string(label) +
                    " guarantee cash requires a provider claim ID");
        }
    } else if (row.provider_claim_id != "NONE") {
        invalid(std::string(label) +
                " non-provider entry requires provider_claim_id NONE");
    }
    if (!row.period.has_value()) {
        add_blocker(package.blockers,
            std::string(label) + " entry " + row.entry_id +
            " has UNKNOWN period");
    }
    if (!row.known_at_period.has_value()) {
        add_blocker(package.blockers,
            std::string(label) + " entry " + row.entry_id +
            " has UNKNOWN known_at_period");
    }
    if (row.value.status == ClaimLedgerValueStatus::Unknown ||
        row.value.status == ClaimLedgerValueStatus::Bounded) {
        add_blocker(package.blockers,
            std::string(label) + " entry " + row.entry_id +
            " is not an exact cash or balance value");
    }
    if (row.input_status == ClaimLedgerInputStatus::Unknown) {
        add_blocker(package.blockers,
            std::string(label) + " entry " + row.entry_id +
            " has unknown input status");
    }
    return row;
}

void parse_common_entries(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables, std::unordered_set<std::string>& entry_ids) {
    package.row_counts.common_entries = parse_tsv(
        bytes, kCommonEntriesHeader, 12U, kClaimLedgerMaximumEntries,
        "common_entries.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            ParsedEntry row = parse_entry_fields(
                fields, 0U, std::nullopt, "common_entries.tsv", tables,
                package);
            if (!entry_ids.insert(row.entry_id).second) {
                invalid("duplicate claim-ledger entry_id: " + row.entry_id);
            }
            tables.entries.push_back(std::move(row));
        });
}

void parse_scenarios(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables, std::unordered_set<std::string>& scenario_ids) {
    package.row_counts.scenarios = parse_tsv(
        bytes, kScenariosHeader, 11U, kClaimLedgerMaximumScenarios,
        "scenarios.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            ParsedScenario row;
            row.scenario_id = std::string(fields[0U]);
            require_safe_identifier(row.scenario_id, "scenario_id");
            if (!scenario_ids.insert(row.scenario_id).second) {
                invalid("duplicate scenarios.tsv scenario_id: " +
                        row.scenario_id);
            }
            row.probability = parse_typed_number(
                fields[1U], fields[2U], fields[3U],
                "scenario probability", 1.0);
            row.probability_known_at_period = parse_optional_period(
                fields[4U], "scenario probability_known_at_period");
            row.probability_input_status =
                parse_input_status(fields[5U], "scenarios.tsv probability");
            row.probability_source_record_id = std::string(fields[6U]);
            collect_source(tables, row.probability_source_record_id,
                "scenarios.tsv probability_source_record_id");
            row.cash_path_status = parse_cash_path_status(
                fields[7U], row.cash_path_status_unknown);
            row.cash_path_status_known_at_period = parse_optional_period(
                fields[8U], "scenario cash_path_status_known_at_period");
            row.cash_path_input_status =
                parse_input_status(fields[9U], "scenarios.tsv cash path");
            row.cash_path_status_source_record_id =
                std::string(fields[10U]);
            collect_source(tables, row.cash_path_status_source_record_id,
                "scenarios.tsv cash_path_status_source_record_id");
            if (!exact_known(row.probability)) {
                add_blocker(package.blockers,
                    "scenario " + row.scenario_id +
                    " probability is not exact");
            }
            if (!row.probability_known_at_period.has_value()) {
                add_blocker(package.blockers,
                    "scenario " + row.scenario_id +
                    " probability has UNKNOWN known_at_period");
            }
            if (row.cash_path_status_unknown ||
                !row.cash_path_status_known_at_period.has_value() ||
                row.cash_path_status !=
                    ClaimLedgerCashPathStatus::CompleteResolved) {
                add_blocker(package.blockers,
                    "scenario " + row.scenario_id +
                        " cash path is not explicitly complete and resolved");
            }
            if (row.probability_input_status !=
                    ClaimLedgerInputStatus::Observed &&
                row.probability_input_status !=
                    ClaimLedgerInputStatus::Derived &&
                row.probability_input_status !=
                    ClaimLedgerInputStatus::Estimated) {
                add_blocker(package.blockers,
                    "scenario " + row.scenario_id +
                    " is not an independently fixed ex-ante physical probability");
            }
            if (row.cash_path_input_status !=
                    ClaimLedgerInputStatus::Observed &&
                row.cash_path_input_status !=
                    ClaimLedgerInputStatus::Contractual &&
                row.cash_path_input_status !=
                    ClaimLedgerInputStatus::Derived) {
                add_blocker(package.blockers,
                    "scenario " + row.scenario_id +
                    " has no admissible completeness attestation");
            }
            tables.scenarios.push_back(std::move(row));
        });
}

void parse_scenario_entries(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables, const std::unordered_set<std::string>& scenario_ids,
    std::unordered_set<std::string>& entry_ids) {
    const std::size_t remaining =
        kClaimLedgerMaximumEntries - tables.entries.size();
    package.row_counts.scenario_entries = parse_tsv(
        bytes, kScenarioEntriesHeader, 13U, remaining,
        "scenario_entries.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            require_safe_identifier(fields[0U], "scenario_entries scenario_id");
            if (!scenario_ids.contains(std::string(fields[0U]))) {
                invalid("scenario_entries.tsv references unknown scenario_id: " +
                        std::string(fields[0U]));
            }
            ParsedEntry row = parse_entry_fields(
                fields, 1U, std::string(fields[0U]),
                "scenario_entries.tsv", tables, package);
            if (!entry_ids.insert(row.entry_id).second) {
                invalid("duplicate claim-ledger entry_id: " + row.entry_id);
            }
            tables.entries.push_back(std::move(row));
        });
}

void parse_provider_claims(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables, std::unordered_set<std::string>& provider_ids) {
    package.row_counts.provider_claims = parse_tsv(
        bytes, kProviderClaimsHeader, 26U, kMaximumProviders,
        "provider_claims.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            ParsedProvider row;
            row.provider_claim_id = std::string(fields[0U]);
            row.provider_id = std::string(fields[1U]);
            require_safe_identifier(
                row.provider_claim_id, "provider_claim_id");
            require_safe_identifier(row.provider_id, "provider_id");
            if (!provider_ids.insert(row.provider_claim_id).second) {
                invalid("duplicate provider_claim_id: " +
                        row.provider_claim_id);
            }
            row.known_at_period = parse_optional_period(
                fields[2U], "provider known_at_period");
            row.shortfall_allocation = parse_typed_number(
                fields[3U], fields[4U], fields[5U],
                "provider shortfall allocation", 1.0);
            row.coverage = parse_typed_number(
                fields[6U], fields[7U], fields[8U],
                "provider coverage", 1.0);
            row.deductible = parse_typed_number(
                fields[9U], fields[10U], fields[11U],
                "provider deductible", kMaximumMoneyMillion);
            row.maximum_cash = parse_typed_number(
                fields[12U], fields[13U], fields[14U],
                "provider maximum cash", kMaximumMoneyMillion);
            row.settlement_lag = parse_typed_number(
                fields[15U], fields[16U], fields[17U],
                "provider settlement lag",
                static_cast<double>(kClaimLedgerMaximumPeriods));
            if (row.settlement_lag.lower.has_value()) {
                if (*row.settlement_lag.lower !=
                        std::round(*row.settlement_lag.lower) ||
                    *row.settlement_lag.upper !=
                        std::round(*row.settlement_lag.upper)) {
                    invalid("provider settlement lag endpoints must be integer periods");
                }
            }
            row.covers_principal_due = parse_optional_bool(
                fields[18U], "covers_principal_due");
            row.covers_interest_due = parse_optional_bool(
                fields[19U], "covers_interest_due");
            row.payment_right_evidenced = parse_optional_bool(
                fields[20U], "payment_right_evidenced");
            row.provider_identity_evidenced = parse_optional_bool(
                fields[21U], "provider_identity_evidenced");
            row.coverage_and_priority_evidenced = parse_optional_bool(
                fields[22U], "coverage_and_priority_evidenced");
            row.obligation_priority = parse_optional_priority(
                fields[23U], row.obligation_priority_unknown);
            row.input_status =
                parse_input_status(fields[24U], "provider_claims.tsv");
            row.source_record_id = std::string(fields[25U]);
            collect_source(
                tables, row.source_record_id,
                "provider_claims.tsv source_record_id");

            if (!row.known_at_period.has_value()) {
                add_blocker(package.blockers,
                    "provider claim " + row.provider_claim_id +
                    " has UNKNOWN known_at_period");
            }
            if (!exact_known(row.shortfall_allocation) ||
                !exact_known(row.coverage) || !exact_known(row.deductible) ||
                !exact_known(row.maximum_cash) ||
                !exact_known(row.settlement_lag)) {
                add_blocker(package.blockers,
                    "provider claim " + row.provider_claim_id +
                    " has incomplete numerical guarantee terms");
            }
            if (!row.covers_principal_due.has_value() ||
                !row.covers_interest_due.has_value() ||
                !row.payment_right_evidenced.has_value() ||
                !row.provider_identity_evidenced.has_value() ||
                !row.coverage_and_priority_evidenced.has_value() ||
                row.obligation_priority_unknown) {
                add_blocker(package.blockers,
                    "provider claim " + row.provider_claim_id +
                    " has UNKNOWN structural rights or priority");
            }
            if (row.input_status == ClaimLedgerInputStatus::Backtest ||
                row.input_status == ClaimLedgerInputStatus::Unknown) {
                add_blocker(package.blockers,
                    "provider claim " + row.provider_claim_id +
                    " is not established as an ex-ante claim");
            }
            tables.providers.push_back(std::move(row));
        });
}

void parse_covenant_events(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables, const std::unordered_set<std::string>& scenario_ids,
    std::unordered_set<std::string>& covenant_event_ids) {
    package.row_counts.covenant_events = parse_tsv(
        bytes, kCovenantEventsHeader, 12U, kMaximumCovenantEvents,
        "covenant_events.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            ParsedCovenant row;
            if (fields[0U] == "common") {
                row.common = true;
                if (fields[1U] != "NOT_APPLICABLE") {
                    invalid("common covenant row requires scenario_id NOT_APPLICABLE");
                }
            } else if (fields[0U] == "scenario") {
                row.common = false;
                require_safe_identifier(
                    fields[1U], "covenant scenario_id");
                if (!scenario_ids.contains(std::string(fields[1U]))) {
                    invalid("covenant row references unknown scenario_id: " +
                            std::string(fields[1U]));
                }
                row.scenario_id = std::string(fields[1U]);
            } else {
                invalid("covenant scope must be common or scenario");
            }
            row.event_id = std::string(fields[2U]);
            row.covenant_id = std::string(fields[3U]);
            require_safe_identifier(row.event_id, "covenant event_id");
            require_safe_identifier(row.covenant_id, "covenant_id");
            if (!covenant_event_ids.insert(row.event_id).second) {
                invalid("duplicate covenant event_id: " + row.event_id);
            }
            row.period = parse_optional_period(
                fields[4U], "covenant period");
            if (fields[5U] == "known") {
                if (!is_iso_date(fields[6U])) {
                    invalid("known covenant event_date must be ISO YYYY-MM-DD");
                }
                row.event_date.status = ClaimLedgerDateStatus::Known;
                row.event_date.value = std::string(fields[6U]);
            } else if (fields[5U] == "unknown") {
                if (fields[6U] != "UNKNOWN") {
                    invalid("unknown covenant event_date requires UNKNOWN");
                }
                row.event_date.status = ClaimLedgerDateStatus::Unknown;
            } else {
                invalid("covenant event_date_status must be known or unknown");
            }
            row.known_at_period = parse_optional_period(
                fields[7U], "covenant known_at_period");
            row.state = parse_covenant_state(fields[8U]);
            row.input_status =
                parse_input_status(fields[9U], "covenant_events.tsv");
            row.source_record_id = std::string(fields[10U]);
            row.event_date.source_record_id = row.source_record_id;
            collect_source(
                tables, row.source_record_id,
                "covenant_events.tsv source_record_id");
            require_safe_text(fields[11U], "covenant note", 4'096U);
            if (!row.period.has_value()) {
                add_blocker(package.blockers,
                    "covenant event " + row.event_id +
                    " has UNKNOWN period");
            }
            if (!row.known_at_period.has_value()) {
                add_blocker(package.blockers,
                    "covenant event " + row.event_id +
                    " has UNKNOWN known_at_period");
            }
            if (row.event_date.status == ClaimLedgerDateStatus::Known &&
                row.period.has_value() &&
                package.config.period_origin_date.status ==
                    ClaimLedgerDateStatus::Known &&
                !date_in_month_period(
                    *package.config.period_origin_date.value,
                    *row.event_date.value, *row.period)) {
                invalid("covenant event date does not match period_origin_date plus period: " +
                        row.event_id);
            }
            tables.covenants.push_back(std::move(row));
        });
}

void parse_conversion_context(
    std::string_view bytes, ClaimLedgerPackage& package,
    ParsedTables& tables) {
    std::unordered_set<std::string> context_ids;
    package.row_counts.conversion_context = parse_tsv(
        bytes, kConversionContextHeader, 9U, kMaximumConversionContext,
        "conversion_context.tsv",
        [&](const std::vector<std::string_view>& fields, std::size_t) {
            require_safe_identifier(fields[0U], "conversion context_id");
            if (!context_ids.emplace(fields[0U]).second) {
                invalid("duplicate conversion context_id: " +
                        std::string(fields[0U]));
            }
            if (fields[1U] != "UNKNOWN" && !is_iso_date(fields[1U])) {
                invalid("conversion observed_date must be ISO YYYY-MM-DD or UNKNOWN");
            }
            validate_textual_typed_value(
                fields[2U], fields[3U], "conversion context value");
            static_cast<void>(
                parse_input_status(fields[4U], "conversion_context.tsv"));
            static_cast<void>(parse_optional_period(
                fields[5U], "conversion context known_at_period"));
            collect_source(
                tables, fields[6U],
                "conversion_context.tsv source_record_id");
            if (fields[7U] != "context-only") {
                invalid("conversion admission_status must be context-only");
            }
            require_safe_text(
                fields[8U], "conversion context note", 4'096U);
        });
}

struct EntryLineage {
    ClaimLedgerEntryKind kind{ClaimLedgerEntryKind::BuyerPrice};
    std::string event_group_id{};
    std::string provider_claim_id{};
    std::set<std::optional<std::size_t>> known_at_periods{};
};

void validate_entry_lineage(const std::vector<ParsedEntry>& entries) {
    using Lineages = std::unordered_map<std::string, EntryLineage>;
    Lineages common;
    std::unordered_map<std::string, Lineages> scenarios;
    const auto add = [](Lineages& lineages, const ParsedEntry& row) {
        const auto [position, inserted] = lineages.emplace(
            row.economic_fact_id,
            EntryLineage{row.kind, row.event_group_id,
                row.provider_claim_id, {}});
        EntryLineage& lineage = position->second;
        if (!inserted &&
            (lineage.kind != row.kind ||
                lineage.event_group_id != row.event_group_id ||
                lineage.provider_claim_id != row.provider_claim_id)) {
            invalid("economic-fact versions change accounting kind, provider, or event group: " +
                    row.economic_fact_id);
        }
        if (!lineage.known_at_periods.insert(row.known_at_period).second) {
            invalid("economic fact has competing versions at one information cut: " +
                    row.economic_fact_id);
        }
    };
    for (const ParsedEntry& row : entries) {
        if (row.scenario_id.has_value()) {
            add(scenarios[*row.scenario_id], row);
        } else {
            add(common, row);
        }
    }
    for (const auto& [scenario_id, lineages] : scenarios) {
        static_cast<void>(scenario_id);
        for (const auto& [fact_id, lineage] : lineages) {
            const auto common_position = common.find(fact_id);
            if (common_position == common.end()) continue;
            const EntryLineage& common_lineage = common_position->second;
            if (lineage.kind != common_lineage.kind ||
                lineage.event_group_id != common_lineage.event_group_id ||
                lineage.provider_claim_id !=
                    common_lineage.provider_claim_id) {
                invalid("scenario version changes a common economic fact's accounting identity: " +
                        fact_id);
            }
            for (const auto& known_at : lineage.known_at_periods) {
                if (common_lineage.known_at_periods.contains(known_at)) {
                    invalid("common and scenario versions compete at one information cut: " +
                            fact_id);
                }
            }
        }
    }
}

struct SourceManifestEvidence {
    std::string record_date{};
    std::string evidence_class{};
    std::string provenance_tag{};
};

struct SourceManifestIndex {
    std::unordered_set<std::string> ids{};
    std::unordered_map<std::string, SourceManifestEvidence> all_sources{};
    std::unordered_map<std::string, SourceManifestEvidence>
        retained_verified_sources{};
};

[[nodiscard]] bool valid_evidence_class(std::string_view value) noexcept {
    return value == "A" || value == "B" || value == "C" || value == "D";
}

[[nodiscard]] bool valid_provenance_tag(std::string_view value) noexcept {
    constexpr std::array<std::string_view, 10U> tags{{
        "AUD", "REG", "CTR", "MGT", "PR-E", "PR-M", "STD", "EXT",
        "CLM", "HYP",
    }};
    return std::find(tags.begin(), tags.end(), value) != tags.end();
}

[[nodiscard]] bool base_underwriting_evidence_admissible(
    const SourceManifestEvidence& evidence) noexcept {
    return (evidence.evidence_class == "A" ||
               evidence.evidence_class == "B") &&
        evidence.provenance_tag != "CLM" &&
        evidence.provenance_tag != "HYP";
}

[[nodiscard]] bool closing_observation_evidence_admissible(
    const SourceManifestEvidence& evidence) noexcept {
    return (evidence.evidence_class == "A" ||
               evidence.evidence_class == "B") &&
        (evidence.provenance_tag == "AUD" ||
            evidence.provenance_tag == "REG" ||
            evidence.provenance_tag == "CTR" ||
            evidence.provenance_tag == "MGT");
}

[[nodiscard]] SourceManifestIndex load_source_manifest(
    const std::filesystem::path& package_directory,
    std::string_view expected_sha256) {
    std::error_code error;
    const std::filesystem::path parent =
        std::filesystem::canonical(package_directory.parent_path(), error);
    if (error) invalid("could not resolve parent transaction directory");
    const std::filesystem::path manifest = std::filesystem::canonical(
        parent / "source_manifest.tsv", error);
    if (error || !path_is_confined(manifest, parent)) {
        invalid("parent source_manifest.tsv is missing or unconfined");
    }
    const std::string bytes = read_regular_file(
        manifest, kMaximumSourceManifestBytes, "parent source_manifest.tsv");
    if (sha256_bytes_lower_hex(bytes) != expected_sha256) {
        invalid("parent source_manifest.tsv SHA-256 mismatch");
    }
    const std::size_t first_newline = bytes.find('\n');
    const std::size_t header_end = first_newline == std::string::npos
        ? bytes.size()
        : first_newline;
    std::string_view header(bytes.data(), header_end);
    if (!header.empty() && header.back() == '\r') header.remove_suffix(1U);
    const std::vector<std::string_view> header_fields = split_tabs(header);
    if (header != kSourceManifestHeader) {
        invalid("parent source manifest has the wrong closed schema");
    }
    SourceManifestIndex result;
    std::uintmax_t aggregate_retained_bytes = 0U;
    std::unordered_set<std::string> retained_canonical_paths;
    std::size_t position = first_newline == std::string::npos
        ? bytes.size()
        : first_newline + 1U;
    std::size_t line_number = 1U;
    while (position < bytes.size()) {
        const std::size_t newline = bytes.find('\n', position);
        const std::size_t end = newline == std::string::npos
            ? bytes.size()
            : newline;
        std::string_view line(bytes.data() + position, end - position);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1U);
        ++line_number;
        if (line_number - 1U > kMaximumSourceManifestRows) {
            invalid("parent source manifest exceeds its row guardrail");
        }
        if (line.empty() || line.size() > kMaximumTsvLineBytes ||
            contains_unsafe_control(line, true)) {
            invalid("parent source manifest contains a blank, unsafe, or oversized row");
        }
        const std::vector<std::string_view> fields = split_tabs(line);
        if (fields.size() != header_fields.size() || fields.front().empty()) {
            invalid("parent source manifest row has the wrong field count");
        }
        require_safe_identifier(fields.front(), "source manifest source_id");
        if (fields.front() == "NO_PUBLIC_SOURCE" ||
            fields.front() == "SYNTHETIC") {
            invalid("reserved absence/source markers cannot be source-manifest IDs");
        }
        if (!result.ids.emplace(fields.front()).second) {
            invalid("duplicate parent source manifest source_id at line " +
                    std::to_string(line_number));
        }
        if (!is_iso_date(fields[1U]) || !is_iso_date(fields[2U])) {
            invalid("parent source manifest record/access date is not ISO YYYY-MM-DD");
        }
        if (fields[1U] > fields[2U]) {
            invalid("parent source manifest record_date follows access_date");
        }
        if (!valid_evidence_class(fields[3U])) {
            invalid("parent source manifest evidence_class is outside A/B/C/D");
        }
        if (!valid_provenance_tag(fields[4U])) {
            invalid("parent source manifest provenance_tag is outside the closed vocabulary");
        }
        const SourceManifestEvidence source_evidence{
            std::string(fields[1U]), std::string(fields[3U]),
            std::string(fields[4U])};
        result.all_sources.emplace(
            std::string(fields.front()), source_evidence);
        require_safe_text(fields[5U],
            "source manifest distribution_channel", 1'024U);
        require_safe_text(fields[6U],
            "source manifest originating_record", 4'096U);
        require_safe_text(
            fields[7U], "source manifest source_uri", 4'096U);
        if (!(fields[7U].starts_with("https://") ||
                fields[7U].starts_with("http://") ||
                fields[7U].starts_with("urn:"))) {
            invalid("source manifest source_uri requires an http, https, or urn scheme");
        }
        require_safe_text(fields[8U],
            "source manifest retention_status", 32U);
        require_safe_text(
            fields[12U], "source manifest claim_scope", 8'192U);
        require_safe_text(
            fields[13U], "source manifest limitations", 8'192U);
        if (fields[8U] == "RETAINED") {
            if (fields[9U].empty() ||
                !is_lower_hex_sha256(fields[10U]) ||
                fields[11U].empty()) {
                invalid("RETAINED source manifest row lacks path, SHA-256, or byte count");
            }
            std::uintmax_t declared_bytes = 0U;
            const auto parsed = std::from_chars(fields[11U].data(),
                fields[11U].data() + fields[11U].size(), declared_bytes);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != fields[11U].data() + fields[11U].size() ||
                declared_bytes == 0U ||
                declared_bytes > kMaximumRetainedSourceBytes) {
                invalid("retained source byte count is zero, invalid, or exceeds its guardrail");
            }
            const std::filesystem::path relative =
                parse_portable_relative_path(
                    fields[9U], "retained source path");
            const auto first_component = relative.begin();
            if (first_component == relative.end() ||
                *first_component != "retained_sources") {
                invalid("retained source path must be inside retained_sources/");
            }
            const std::filesystem::path retained_root =
                std::filesystem::canonical(parent / "retained_sources", error);
            if (error || !path_is_confined(retained_root, parent)) {
                invalid("retained_sources directory is missing or unconfined");
            }
            const std::filesystem::path retained =
                std::filesystem::canonical(parent / relative, error);
            if (error || !path_is_confined(retained, retained_root) ||
                retained == retained_root) {
                invalid("retained source path is missing or unconfined");
            }
            if (!retained_canonical_paths.emplace(
                    retained.generic_string()).second) {
                invalid("one retained source file cannot be hashed repeatedly under multiple manifest rows");
            }
            if (declared_bytes >
                kMaximumAggregateRetainedSourceBytes -
                    aggregate_retained_bytes) {
                invalid("retained source bytes exceed the aggregate hash-work guardrail");
            }
            aggregate_retained_bytes += declared_bytes;
            const std::uintmax_t actual_bytes =
                std::filesystem::file_size(retained, error);
            if (error || actual_bytes != declared_bytes) {
                invalid("retained source size does not match the bound manifest row");
            }
            const std::string retained_bytes = read_regular_file(
                retained, declared_bytes, "retained source file");
            std::size_t substantive_start = 0U;
            if (retained_bytes.size() >= 3U &&
                static_cast<unsigned char>(retained_bytes[0U]) == 0xEFU &&
                static_cast<unsigned char>(retained_bytes[1U]) == 0xBBU &&
                static_cast<unsigned char>(retained_bytes[2U]) == 0xBFU) {
                substantive_start = 3U;
            }
            const bool has_substantive_byte = std::any_of(
                retained_bytes.begin() +
                    static_cast<std::ptrdiff_t>(substantive_start),
                retained_bytes.end(), [](unsigned char character) {
                    return character != ' ' && character != '\t' &&
                        character != '\r' && character != '\n' &&
                        character != '\f' && character != '\v';
                });
            if (retained_bytes.size() != declared_bytes ||
                !has_substantive_byte ||
                sha256_bytes_lower_hex(retained_bytes) != fields[10U]) {
                invalid("retained source bytes are empty/blank or do not match the bound manifest row");
            }
            const std::filesystem::path rechecked_retained =
                std::filesystem::canonical(parent / relative, error);
            const std::uintmax_t rechecked_bytes =
                error ? 0U :
                        std::filesystem::file_size(
                            rechecked_retained, error);
            if (error || rechecked_retained != retained ||
                rechecked_bytes != declared_bytes) {
                invalid("retained source path or size changed during review");
            }
            result.retained_verified_sources.emplace(
                std::string(fields.front()), source_evidence);
        } else if (fields[8U] == "NOT_RETAINED") {
            if (!fields[9U].empty() || !fields[10U].empty() ||
                !fields[11U].empty()) {
                invalid("NOT_RETAINED source manifest row must leave retained-byte fields blank");
            }
        } else {
            invalid("source manifest retention_status is outside the closed vocabulary");
        }
        if (newline == std::string_view::npos) break;
        position = newline + 1U;
    }
    return result;
}

[[nodiscard]] SourceManifestIndex validate_source_resolution(
    const std::filesystem::path& directory,
    const ClaimLedgerPackageConfig& config,
    const std::unordered_set<std::string>& references) {
    const ClaimLedgerPackageStatus status = config.package_status;
    for (const std::string& source : references) {
        if (status == ClaimLedgerPackageStatus::SyntheticComplete &&
            source != "SYNTHETIC") {
            invalid("synthetic-complete packages must use only the SYNTHETIC source marker");
        }
        if (status != ClaimLedgerPackageStatus::SyntheticComplete &&
            source == "SYNTHETIC") {
            invalid("SYNTHETIC source marker is restricted to synthetic-complete packages");
        }
    }
    if (status == ClaimLedgerPackageStatus::SyntheticComplete) {
        return {};
    }
    const SourceManifestIndex manifest = load_source_manifest(
        directory, config.source_manifest_sha256);
    for (const std::string& source : references) {
        if (source == "NO_PUBLIC_SOURCE" || source == "SYNTHETIC") continue;
        if (!manifest.ids.contains(source)) {
            invalid("source_record_id does not resolve in parent source_manifest.tsv: " +
                    source);
        }
    }
    return manifest;
}

void add_config_blockers(ClaimLedgerPackage& package) {
    const ClaimLedgerPackageConfig& config = package.config;
    if (config.economic_cluster_boundary_status ==
        ClaimLedgerEconomicClusterBoundaryStatus::Unresolved) {
        add_blocker(package.blockers,
            "economic cluster boundary is unresolved");
    }
    if (!config.economic_cluster_boundary_known_at_period.has_value()) {
        add_blocker(package.blockers,
            "economic cluster boundary has UNKNOWN known_at_period");
    }
    if (config.economic_cluster_boundary_input_status !=
            ClaimLedgerInputStatus::Observed &&
        config.economic_cluster_boundary_input_status !=
            ClaimLedgerInputStatus::Derived) {
        add_blocker(package.blockers,
            "economic cluster boundary is not observed or derived");
    }
    const std::array<std::pair<std::string_view, const ClaimLedgerTypedDate*>,
                     8U>
        dates{{
            {"execution", &config.execution_date},
            {"funding", &config.funding_date},
            {"settlement", &config.settlement_date},
            {"observation", &config.observation_date},
            {"decision", &config.decision_date},
            {"maturity", &config.maturity_date},
            {"horizon", &config.horizon_date},
            {"period_origin", &config.period_origin_date},
        }};
    for (const auto& [name, date] : dates) {
        if (date->status == ClaimLedgerDateStatus::Unknown) {
            add_blocker(package.blockers,
                "timeline." + std::string(name) + "_date is UNKNOWN");
        }
    }
    if (!config.decision_period.has_value()) {
        add_blocker(package.blockers,
            "timeline.decision_period is UNKNOWN");
    }
    if (!config.horizon_period.has_value()) {
        add_blocker(package.blockers,
            "timeline.horizon_period is UNKNOWN");
    }
    const std::array<
        std::pair<std::string_view, const ClaimLedgerPackageScalar*>, 4U>
        scalars{{
            {"contractual face amount",
             &config.contractual_face_amount_million},
            {"opening principal", &config.opening_principal_million},
            {"opening accrued interest",
             &config.opening_accrued_interest_million},
            {"annual effective discount rate",
             &config.annual_effective_discount_rate},
        }};
    for (const auto& [name, scalar] : scalars) {
        if (!scalar->known_at_period.has_value()) {
            add_blocker(package.blockers,
                std::string(name) + " has UNKNOWN known_at_period");
        }
        if (!exact_known(scalar->value)) {
            add_blocker(package.blockers,
                std::string(name) + " is not an exact known value");
        }
    }
}

[[nodiscard]] bool structural_core_ready(
    const ClaimLedgerPackageConfig& config, const ParsedTables& tables,
    ClaimLedgerPackage& package) {
    bool ready = config.decision_period.has_value() &&
        config.horizon_period.has_value() &&
        config.period_origin_date.status == ClaimLedgerDateStatus::Known &&
        config.contractual_face_amount_million.known_at_period.has_value() &&
        config.opening_principal_million.known_at_period.has_value() &&
        config.opening_accrued_interest_million.known_at_period.has_value();
    std::unordered_map<std::string, std::optional<std::size_t>>
        provider_known_at;
    for (const ParsedProvider& row : tables.providers) {
        provider_known_at.emplace(
            row.provider_claim_id, row.known_at_period);
    }
    for (const ParsedEntry& row : tables.entries) {
        ready = ready && row.known_at_period.has_value();
        if (config.decision_period.has_value() &&
            row.known_at_period.has_value() &&
            *row.known_at_period <= *config.decision_period) {
            ready = ready && row.period.has_value();
            if (guarantee_kind(row.kind)) {
                const auto provider =
                    provider_known_at.find(row.provider_claim_id);
                if (provider == provider_known_at.end() ||
                    !provider->second.has_value() ||
                    *provider->second > *config.decision_period) {
                    ready = false;
                    add_blocker(package.blockers,
                        "decision-cut guarantee cash references provider terms unavailable at the decision period: " +
                            row.provider_claim_id);
                }
            }
        }
    }
    for (const ParsedScenario& row : tables.scenarios) {
        ready = ready && row.probability_known_at_period.has_value() &&
            row.cash_path_status_known_at_period.has_value() &&
            !row.cash_path_status_unknown;
    }
    for (const ParsedProvider& row : tables.providers) {
        ready = ready && row.known_at_period.has_value();
        if (config.decision_period.has_value() &&
            row.known_at_period.has_value() &&
            *row.known_at_period <= *config.decision_period) {
            ready = ready && row.covers_principal_due.has_value() &&
                row.covers_interest_due.has_value() &&
                row.payment_right_evidenced.has_value() &&
                row.provider_identity_evidenced.has_value() &&
                row.coverage_and_priority_evidenced.has_value() &&
                !row.obligation_priority_unknown &&
                row.obligation_priority.has_value();
        }
    }
    if (!ready) {
        add_critical_blocker(package.blockers,
            "decision-cut core is blocked by UNKNOWN entry timing, scenario timing, provider timing, rights, or priority");
    }
    return ready;
}

[[nodiscard]] bool full_core_structurally_representable(
    const ParsedTables& tables) {
    return std::all_of(tables.entries.begin(), tables.entries.end(),
               [](const ParsedEntry& row) {
                   return row.period.has_value() &&
                       row.known_at_period.has_value();
               }) &&
        std::all_of(tables.providers.begin(), tables.providers.end(),
            [](const ParsedProvider& row) {
                return row.known_at_period.has_value() &&
                    row.covers_principal_due.has_value() &&
                    row.covers_interest_due.has_value() &&
                    row.payment_right_evidenced.has_value() &&
                    row.provider_identity_evidenced.has_value() &&
                    row.coverage_and_priority_evidenced.has_value() &&
                    !row.obligation_priority_unknown &&
                    row.obligation_priority.has_value();
            }) &&
        std::all_of(tables.covenants.begin(), tables.covenants.end(),
            [](const ParsedCovenant& row) {
                return row.period.has_value() &&
                    row.known_at_period.has_value();
            });
}

void remove_unavailable_full_state(ClaimLedgerSummary& summary) {
    summary.common_full_path = {};
    summary.common_backtest_covenant_events.clear();
    for (ClaimLedgerScenarioResult& scenario : summary.scenarios) {
        scenario.full_path = {};
        scenario.backtest_entry_ids.clear();
        scenario.backtest_covenant_events.clear();
    }
}

[[nodiscard]] ClaimLedgerEntry make_core_entry(const ParsedEntry& row) {
    ClaimLedgerEntry entry;
    entry.entry_id = row.entry_id;
    entry.economic_fact_id = row.economic_fact_id;
    entry.event_group_id = row.event_group_id == "NONE"
        ? "none"
        : row.event_group_id;
    entry.kind = row.kind;
    entry.period = *row.period;
    entry.known_at_period = *row.known_at_period;
    entry.value = row.value;
    entry.source_record_id = row.source_record_id;
    entry.provider_claim_id = row.provider_claim_id == "NONE"
        ? "none"
        : row.provider_claim_id;
    return entry;
}

[[nodiscard]] ClaimLedgerConfig make_core_config(
    const ClaimLedgerPackageConfig& package_config,
    const ParsedTables& tables, bool decision_only) {
    ClaimLedgerConfig config;
    config.model_version = package_config.model_version;
    config.ledger_id = package_config.package_id;
    config.project_id = package_config.project_id;
    config.claim_id = package_config.claim_id;
    config.currency_label = package_config.currency_label;
    config.monetary_basis = package_config.monetary_basis;
    config.period_unit_label = package_config.period_unit_label;
    config.periods_per_year = package_config.periods_per_year;
    config.conversion_unit_label = package_config.conversion_unit_label;
    config.conversion_unit_basis = package_config.conversion_unit_basis;
    config.decision_period = *package_config.decision_period;
    config.horizon_period = *package_config.horizon_period;
    config.contractual_face_amount_million =
        package_config.contractual_face_amount_million.value;
    config.face_amount_known_at_period =
        *package_config.contractual_face_amount_million.known_at_period;
    config.opening_principal_million =
        package_config.opening_principal_million.value;
    config.opening_principal_known_at_period =
        *package_config.opening_principal_million.known_at_period;
    config.opening_accrued_interest_million =
        package_config.opening_accrued_interest_million.value;
    config.opening_accrued_interest_known_at_period =
        *package_config.opening_accrued_interest_million.known_at_period;
    config.annual_effective_discount_rate =
        package_config.annual_effective_discount_rate.value;
    config.discount_rate_known_at_period =
        package_config.annual_effective_discount_rate.known_at_period;

    if (decision_only) {
        if (config.face_amount_known_at_period > config.decision_period) {
            config.contractual_face_amount_million =
                claim_ledger_unknown();
        }
        if (config.opening_principal_known_at_period >
            config.decision_period) {
            config.opening_principal_million = claim_ledger_unknown();
        }
        if (config.opening_accrued_interest_known_at_period >
            config.decision_period) {
            config.opening_accrued_interest_million =
                claim_ledger_unknown();
        }
        if (config.discount_rate_known_at_period.has_value() &&
            *config.discount_rate_known_at_period >
                config.decision_period) {
            config.annual_effective_discount_rate =
                claim_ledger_unknown();
        }
    }

    std::unordered_map<std::string, std::size_t> scenario_indices;
    for (const ParsedScenario& parsed : tables.scenarios) {
        ClaimLedgerScenario scenario;
        scenario.scenario_id = parsed.scenario_id;
        scenario.physical_probability = parsed.probability;
        scenario.probability_known_at_period =
            *parsed.probability_known_at_period;
        scenario.cash_path_status = parsed.cash_path_status;
        scenario.cash_path_status_known_at_period =
            *parsed.cash_path_status_known_at_period;
        scenario.probability_source_record_id =
            parsed.probability_source_record_id;
        scenario.cash_path_status_source_record_id =
            parsed.cash_path_status_source_record_id;
        if (decision_only && scenario.probability_known_at_period >
                config.decision_period) {
            scenario.physical_probability = claim_ledger_unknown();
        }
        if (decision_only && scenario.cash_path_status_known_at_period >
                config.decision_period) {
            scenario.cash_path_status =
                ClaimLedgerCashPathStatus::Incomplete;
        }
        scenario_indices.emplace(
            scenario.scenario_id, config.scenarios.size());
        config.scenarios.push_back(std::move(scenario));
    }
    bool selected_conversion_units = false;
    for (const ParsedEntry& parsed : tables.entries) {
        if (!parsed.known_at_period.has_value() || !parsed.period.has_value() ||
            (decision_only &&
                *parsed.known_at_period > config.decision_period)) {
            continue;
        }
        ClaimLedgerEntry entry = make_core_entry(parsed);
        selected_conversion_units = selected_conversion_units ||
            entry.kind == ClaimLedgerEntryKind::ConversionUnits;
        if (!parsed.scenario_id.has_value()) {
            config.common_entries.push_back(std::move(entry));
        } else {
            config.scenarios.at(
                scenario_indices.at(*parsed.scenario_id))
                .entries.push_back(std::move(entry));
        }
    }
    for (const ParsedProvider& parsed : tables.providers) {
        if (!parsed.known_at_period.has_value() ||
            (decision_only &&
                *parsed.known_at_period > config.decision_period)) {
            continue;
        }
        ClaimLedgerProviderClaim provider;
        provider.provider_claim_id = parsed.provider_claim_id;
        provider.provider_id = parsed.provider_id;
        provider.known_at_period = *parsed.known_at_period;
        provider.shortfall_allocation_fraction =
            parsed.shortfall_allocation;
        provider.coverage_fraction = parsed.coverage;
        provider.deductible_million = parsed.deductible;
        provider.maximum_cash_million = parsed.maximum_cash;
        provider.settlement_lag_periods = parsed.settlement_lag;
        provider.covers_principal_due = *parsed.covers_principal_due;
        provider.covers_interest_due = *parsed.covers_interest_due;
        provider.payment_right_evidenced =
            *parsed.payment_right_evidenced;
        provider.provider_identity_evidenced =
            *parsed.provider_identity_evidenced;
        provider.coverage_and_priority_evidenced =
            *parsed.coverage_and_priority_evidenced;
        provider.obligation_priority = parsed.obligation_priority;
        provider.source_record_id = parsed.source_record_id;
        config.provider_claims.push_back(std::move(provider));
    }
    for (const ParsedCovenant& parsed : tables.covenants) {
        if (!parsed.period.has_value() ||
            !parsed.known_at_period.has_value() ||
            (decision_only &&
                *parsed.known_at_period > config.decision_period)) {
            continue;
        }
        ClaimLedgerCovenantEvent event;
        event.event_id = parsed.event_id;
        event.covenant_id = parsed.covenant_id;
        event.period = *parsed.period;
        event.known_at_period = *parsed.known_at_period;
        event.state = parsed.state;
        event.source_record_id = parsed.source_record_id;
        if (parsed.common) {
            config.common_covenant_events.push_back(std::move(event));
        } else {
            config.scenarios.at(
                scenario_indices.at(*parsed.scenario_id))
                .covenant_events.push_back(std::move(event));
        }
    }
    if (decision_only && !selected_conversion_units) {
        config.conversion_unit_label = "not-applicable";
        config.conversion_unit_basis = "not-applicable";
    }
    return config;
}

void append_core_blockers(ClaimLedgerPackage& package) {
    if (!package.evaluation.has_value()) return;
    const ClaimLedgerReadiness& readiness = package.evaluation->readiness;
    for (const std::string& blocker : readiness.expected_cash_blockers) {
        add_blocker(package.blockers,
            "core expected-cash blocker: " + blocker);
    }
    for (const std::string& blocker : readiness.npv_blockers) {
        add_blocker(package.blockers,
            "core NPV blocker: " + blocker);
    }
    for (const std::string& blocker : readiness.rate_preimage_blockers) {
        add_blocker(package.blockers,
            "core rate-preimage blocker: " + blocker);
    }
    for (const std::string& blocker : readiness.provider_claim_blockers) {
        add_blocker(package.blockers,
            "core provider-claim blocker: " + blocker);
    }
}

[[nodiscard]] bool empirically_admissible(
    ClaimLedgerInputStatus status) noexcept {
    return status == ClaimLedgerInputStatus::Observed ||
        status == ClaimLedgerInputStatus::Contractual ||
        status == ClaimLedgerInputStatus::Derived ||
        status == ClaimLedgerInputStatus::Estimated;
}

[[nodiscard]] bool completeness_attestation_admissible(
    ClaimLedgerInputStatus status) noexcept {
    return status == ClaimLedgerInputStatus::Observed ||
        status == ClaimLedgerInputStatus::Contractual ||
        status == ClaimLedgerInputStatus::Derived;
}

[[nodiscard]] bool physical_probability_admissible(
    ClaimLedgerInputStatus status) noexcept {
    return status == ClaimLedgerInputStatus::Observed ||
        status == ClaimLedgerInputStatus::Derived ||
        status == ClaimLedgerInputStatus::Estimated;
}

[[nodiscard]] bool verified_source_at_or_before(std::string_view source,
    const SourceManifestIndex& source_index,
    std::string_view cutoff_date) {
    if (source == "NO_PUBLIC_SOURCE" || source == "SYNTHETIC") {
        return false;
    }
    const auto found = source_index.retained_verified_sources.find(
        std::string(source));
    return found != source_index.retained_verified_sources.end() &&
        found->second.record_date <= cutoff_date &&
        base_underwriting_evidence_admissible(found->second);
}

[[nodiscard]] bool verified_closing_source_on_date(
    std::string_view source, const SourceManifestIndex& source_index,
    std::string_view closing_date) {
    if (source == "NO_PUBLIC_SOURCE" || source == "SYNTHETIC") {
        return false;
    }
    const auto found = source_index.retained_verified_sources.find(
        std::string(source));
    return found != source_index.retained_verified_sources.end() &&
        found->second.record_date == closing_date &&
        closing_observation_evidence_admissible(found->second);
}

[[nodiscard]] bool verified_closing_source_not_before(
    std::string_view source, const SourceManifestIndex& source_index,
    std::string_view closing_date) {
    if (source == "NO_PUBLIC_SOURCE" || source == "SYNTHETIC") {
        return false;
    }
    const auto found = source_index.retained_verified_sources.find(
        std::string(source));
    return found != source_index.retained_verified_sources.end() &&
        found->second.record_date >= closing_date &&
        closing_observation_evidence_admissible(found->second);
}

[[nodiscard]] std::vector<const ParsedEntry*>
selected_entry_versions_for_scenario(
    const ParsedTables& tables,
    std::optional<std::size_t> maximum_known_at_period,
    std::optional<std::string_view> scenario_id) {
    std::unordered_map<std::string, const ParsedEntry*> latest;
    for (const ParsedEntry& row : tables.entries) {
        if (row.scenario_id.has_value()) {
            if (!scenario_id.has_value() ||
                *row.scenario_id != *scenario_id) {
                continue;
            }
        }
        if (!row.known_at_period.has_value() ||
            (maximum_known_at_period.has_value() &&
                *row.known_at_period > *maximum_known_at_period)) {
            continue;
        }
        const auto found = latest.find(row.economic_fact_id);
        if (found == latest.end() ||
            *row.known_at_period > *found->second->known_at_period) {
            latest[row.economic_fact_id] = &row;
        }
    }
    std::vector<const ParsedEntry*> selected;
    selected.reserve(latest.size());
    for (const auto& [fact_id, row] : latest) {
        static_cast<void>(fact_id);
        selected.push_back(row);
    }
    std::sort(selected.begin(), selected.end(),
        [](const ParsedEntry* left, const ParsedEntry* right) {
            if (left->economic_fact_id != right->economic_fact_id) {
                return left->economic_fact_id < right->economic_fact_id;
            }
            return left->entry_id < right->entry_id;
        });
    return selected;
}

[[nodiscard]] std::vector<const ParsedEntry*>
selected_decision_entry_versions_for_scenario(
    const ParsedTables& tables, std::size_t decision_period,
    std::optional<std::string_view> scenario_id) {
    return selected_entry_versions_for_scenario(
        tables, decision_period, scenario_id);
}

[[nodiscard]] std::vector<const ParsedEntry*>
selected_full_entry_versions_for_scenario(
    const ParsedTables& tables,
    std::optional<std::string_view> scenario_id) {
    return selected_entry_versions_for_scenario(
        tables, std::nullopt, scenario_id);
}

[[nodiscard]] std::vector<const ParsedEntry*>
selected_decision_entry_versions(const ParsedTables& tables,
    std::size_t decision_period) {
    std::vector<const ParsedEntry*> selected;
    std::unordered_set<const ParsedEntry*> already_selected;
    const auto append_path =
        [&](std::optional<std::string_view> scenario_id) {
            const auto path = selected_decision_entry_versions_for_scenario(
                tables, decision_period, scenario_id);
            for (const ParsedEntry* row : path) {
                if (already_selected.insert(row).second) {
                    selected.push_back(row);
                }
            }
        };
    if (tables.scenarios.empty()) {
        append_path(std::nullopt);
    } else {
        for (const ParsedScenario& scenario : tables.scenarios) {
            append_path(scenario.scenario_id);
        }
    }
    return selected;
}

[[nodiscard]] bool expected_return_sources_evidenced(
    const ClaimLedgerPackageConfig& config, const ParsedTables& tables,
    const std::vector<const ParsedEntry*>& selected_entries,
    const SourceManifestIndex& source_index) {
    if (config.decision_date.status != ClaimLedgerDateStatus::Known ||
        !config.decision_date.value.has_value()) {
        return false;
    }
    const std::string& decision_date = *config.decision_date.value;
    const std::array<const ClaimLedgerTypedDate*, 7U> dates{{
        &config.execution_date, &config.funding_date,
        &config.settlement_date, &config.decision_date,
        &config.maturity_date, &config.horizon_date,
        &config.period_origin_date,
    }};
    const std::array<const ClaimLedgerPackageScalar*, 3U> scalars{{
        &config.contractual_face_amount_million,
        &config.opening_principal_million,
        &config.opening_accrued_interest_million,
    }};
    return std::all_of(dates.begin(), dates.end(),
               [&](const auto* date) {
               return date->status == ClaimLedgerDateStatus::Known &&
                   verified_source_at_or_before(
                       date->source_record_id, source_index, decision_date);
           }) &&
        std::all_of(scalars.begin(), scalars.end(), [&](const auto* scalar) {
            return verified_source_at_or_before(
                scalar->source_record_id, source_index, decision_date);
        }) &&
        std::all_of(selected_entries.begin(), selected_entries.end(),
            [&](const ParsedEntry* row) {
                return verified_source_at_or_before(
                    row->source_record_id, source_index, decision_date);
            }) &&
        std::all_of(tables.scenarios.begin(), tables.scenarios.end(),
            [&](const ParsedScenario& row) {
                return verified_source_at_or_before(
                           row.probability_source_record_id, source_index,
                           decision_date) &&
                    verified_source_at_or_before(
                        row.cash_path_status_source_record_id, source_index,
                        decision_date);
            }) &&
        std::all_of(tables.providers.begin(), tables.providers.end(),
            [&](const ParsedProvider& row) {
                if (!row.known_at_period.has_value() ||
                    !config.decision_period.has_value()) {
                    return false;
                }
                return *row.known_at_period > *config.decision_period ||
                    verified_source_at_or_before(
                        row.source_record_id, source_index, decision_date);
            });
}

[[nodiscard]] bool primary_settlement_timing_consistent(
    const ClaimLedgerPackageConfig& config,
    const ParsedTables& tables) {
    if (config.period_origin_date.status != ClaimLedgerDateStatus::Known ||
        config.funding_date.status != ClaimLedgerDateStatus::Known ||
        config.settlement_date.status != ClaimLedgerDateStatus::Known ||
        !config.period_origin_date.value.has_value() ||
        !config.funding_date.value.has_value() ||
        !config.settlement_date.value.has_value() ||
        !config.decision_period.has_value()) {
        return false;
    }
    const auto path_is_consistent =
        [&](std::optional<std::string_view> scenario_id) {
            const auto selected =
                selected_decision_entry_versions_for_scenario(
                    tables, *config.decision_period, scenario_id);
            std::optional<std::size_t> buyer_price_period;
            for (const ParsedEntry* row : selected) {
                if (row->kind != ClaimLedgerEntryKind::BuyerPrice) continue;
                if (!row->period.has_value()) return false;
                buyer_price_period = buyer_price_period.has_value()
                    ? std::min(*buyer_price_period, *row->period)
                    : *row->period;
            }
            return buyer_price_period.has_value() &&
                date_in_month_period(*config.period_origin_date.value,
                    *config.funding_date.value, *buyer_price_period) &&
                date_in_month_period(*config.period_origin_date.value,
                    *config.settlement_date.value, *buyer_price_period);
        };
    if (tables.scenarios.empty()) {
        return path_is_consistent(std::nullopt);
    }
    return std::all_of(tables.scenarios.begin(), tables.scenarios.end(),
        [&](const ParsedScenario& scenario) {
            return path_is_consistent(scenario.scenario_id);
        });
}

[[nodiscard]] bool closing_funding_kind(
    ClaimLedgerEntryKind kind) noexcept {
    return kind == ClaimLedgerEntryKind::BuyerPrice ||
        kind == ClaimLedgerEntryKind::BuyerDirectCost ||
        kind == ClaimLedgerEntryKind::BorrowerGrossProceeds ||
        kind == ClaimLedgerEntryKind::BorrowerNetProceeds ||
        kind == ClaimLedgerEntryKind::CashFee ||
        kind == ClaimLedgerEntryKind::BorrowerThirdPartyCost ||
        kind == ClaimLedgerEntryKind::FundedPrincipal ||
        kind == ClaimLedgerEntryKind::OriginalIssueDiscount ||
        kind == ClaimLedgerEntryKind::OriginalIssuePremium ||
        kind == ClaimLedgerEntryKind::CapitalizedFee;
}

[[nodiscard]] bool exact_values_equal(
    const ClaimLedgerValue& first, const ClaimLedgerValue& second) noexcept;

[[nodiscard]] bool closing_status_observed(
    ClaimLedgerEntryKind kind, ClaimLedgerInputStatus status) noexcept {
    const bool directly_observed =
        kind == ClaimLedgerEntryKind::BuyerPrice ||
        kind == ClaimLedgerEntryKind::BuyerDirectCost;
    return directly_observed
        ? status == ClaimLedgerInputStatus::Observed
        : status == ClaimLedgerInputStatus::Observed ||
            status == ClaimLedgerInputStatus::Derived;
}

[[nodiscard]] bool common_primary_settlement_observed(
    const ClaimLedgerPackageConfig& config, const ParsedTables& tables,
    const SourceManifestIndex& source_index) {
    if (!config.decision_period.has_value() ||
        config.period_origin_date.status != ClaimLedgerDateStatus::Known ||
        config.funding_date.status != ClaimLedgerDateStatus::Known ||
        config.settlement_date.status != ClaimLedgerDateStatus::Known ||
        !config.period_origin_date.value.has_value() ||
        !config.funding_date.value.has_value() ||
        !config.settlement_date.value.has_value()) {
        return false;
    }
    const auto common = selected_decision_entry_versions_for_scenario(
        tables, *config.decision_period, std::nullopt);
    const ParsedEntry* primary_price = nullptr;
    for (const ParsedEntry* row : common) {
        if (row->kind != ClaimLedgerEntryKind::BuyerPrice ||
            !row->period.has_value()) {
            continue;
        }
        if (primary_price == nullptr ||
            *row->period < *primary_price->period) {
            primary_price = row;
        }
    }
    if (primary_price == nullptr || primary_price->event_group_id == "NONE") {
        return false;
    }
    const std::size_t anchor_period = *primary_price->period;
    if (!(
        date_in_month_period(*config.period_origin_date.value,
            *config.funding_date.value, anchor_period) &&
        date_in_month_period(*config.period_origin_date.value,
            *config.settlement_date.value, anchor_period))) {
        return false;
    }

    constexpr std::array<ClaimLedgerEntryKind, 10U> required{{
        ClaimLedgerEntryKind::BuyerPrice,
        ClaimLedgerEntryKind::BuyerDirectCost,
        ClaimLedgerEntryKind::BorrowerGrossProceeds,
        ClaimLedgerEntryKind::BorrowerNetProceeds,
        ClaimLedgerEntryKind::CashFee,
        ClaimLedgerEntryKind::BorrowerThirdPartyCost,
        ClaimLedgerEntryKind::FundedPrincipal,
        ClaimLedgerEntryKind::OriginalIssueDiscount,
        ClaimLedgerEntryKind::OriginalIssuePremium,
        ClaimLedgerEntryKind::CapitalizedFee,
    }};
    std::unordered_set<std::string> admitted_fact_ids;
    for (const ClaimLedgerEntryKind kind : required) {
        const ParsedEntry* admitted = nullptr;
        for (const ParsedEntry* row : common) {
            if (row->period == primary_price->period && row->kind == kind &&
                row->event_group_id == primary_price->event_group_id) {
                if (admitted != nullptr) return false;
                admitted = row;
            }
        }
        if (admitted == nullptr) return false;
        if (!closing_status_observed(kind, admitted->input_status) ||
            !verified_closing_source_on_date(admitted->source_record_id,
                source_index, *config.settlement_date.value)) {
            return false;
        }
        admitted_fact_ids.emplace(admitted->economic_fact_id);
        for (const ParsedEntry& version : tables.entries) {
            if (version.economic_fact_id != admitted->economic_fact_id ||
                !version.known_at_period.has_value() ||
                *version.known_at_period <= *config.decision_period) {
                continue;
            }
            if (version.kind != admitted->kind ||
                version.event_group_id != admitted->event_group_id ||
                version.period != admitted->period ||
                !exact_values_equal(version.value, admitted->value) ||
                !closing_status_observed(version.kind,
                    version.input_status) ||
                !verified_closing_source_not_before(
                    version.source_record_id, source_index,
                    *config.settlement_date.value)) {
                return false;
            }
        }
    }
    for (const ParsedEntry* row : common) {
        if (row->period == primary_price->period &&
            closing_funding_kind(row->kind) &&
            row->event_group_id != primary_price->event_group_id) {
            return false;
        }
    }
    for (const ParsedScenario& scenario : tables.scenarios) {
        const auto selected = selected_decision_entry_versions_for_scenario(
            tables, *config.decision_period, scenario.scenario_id);
        if (std::any_of(selected.begin(), selected.end(),
                [&](const ParsedEntry* row) {
                    return row->scenario_id.has_value() &&
                        row->period.has_value() &&
                        *row->period <= anchor_period &&
                        closing_funding_kind(row->kind);
                })) {
            return false;
        }
    }
    for (const ParsedEntry& row : tables.entries) {
        if (!row.known_at_period.has_value() ||
            *row.known_at_period <= *config.decision_period ||
            !closing_funding_kind(row.kind)) {
            continue;
        }
        if (!row.period.has_value()) return false;
        if ((*row.period <= anchor_period ||
                row.event_group_id == primary_price->event_group_id) &&
            !admitted_fact_ids.contains(row.economic_fact_id)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::size_t> containing_month_period(
    std::string_view origin, std::string_view target,
    std::size_t horizon_period) noexcept {
    for (std::size_t period = 0U; period <= horizon_period; ++period) {
        if (date_in_month_period(origin, target, period)) return period;
    }
    return std::nullopt;
}

[[nodiscard]] bool exact_values_equal(
    const ClaimLedgerValue& first, const ClaimLedgerValue& second) noexcept {
    if (!exact_known(first) || !exact_known(second) ||
        *first.lower != *first.upper || *second.lower != *second.upper) {
        return false;
    }
    const double scale =
        std::max({1.0, std::abs(*first.lower), std::abs(*second.lower)});
    return std::abs(*first.lower - *second.lower) <= 1.0e-10 * scale;
}

[[nodiscard]] bool exact_number(
    const ClaimLedgerValue& value, double& result) noexcept {
    if (!exact_known(value) || *value.lower != *value.upper) return false;
    result = *value.lower;
    return true;
}

[[nodiscard]] bool amounts_equal(double first, double second) noexcept {
    const double scale = std::max({1.0, std::abs(first), std::abs(second)});
    return std::abs(first - second) <= 1.0e-10 * scale;
}

[[nodiscard]] bool maturity_obligations_consistent(
    const ClaimLedgerPackageConfig& config, const ParsedTables& tables,
    const ClaimLedgerSummary* evaluation) {
    if (evaluation == nullptr || evaluation->scenarios.empty() ||
        !config.decision_period.has_value() ||
        !config.horizon_period.has_value() ||
        config.period_origin_date.status != ClaimLedgerDateStatus::Known ||
        config.maturity_date.status != ClaimLedgerDateStatus::Known ||
        !config.period_origin_date.value.has_value() ||
        !config.maturity_date.value.has_value()) {
        return false;
    }
    const auto maturity_period = containing_month_period(
        *config.period_origin_date.value, *config.maturity_date.value,
        *config.horizon_period);
    if (!maturity_period.has_value()) return false;

    const auto selected = selected_decision_entry_versions(
        tables, *config.decision_period);
    for (const ParsedEntry* row : selected) {
        if (!row->period.has_value() || *row->period <= *maturity_period ||
            !closing_funding_kind(row->kind)) {
            continue;
        }
        if (!exact_known(row->value) || *row->value.lower != *row->value.upper ||
            *row->value.upper > 1.0e-10) {
            return false;
        }
    }

    return std::all_of(evaluation->scenarios.begin(),
        evaluation->scenarios.end(), [&](const ClaimLedgerScenarioResult& row) {
            const auto& periods = row.decision_path.periods;
            if (periods.size() <= *maturity_period) return false;
            for (std::size_t period = *maturity_period;
                 period < periods.size(); ++period) {
                const ClaimLedgerPeriodResult& result = periods[period];
                double opening_principal = 0.0;
                double funded_principal = 0.0;
                double capitalized_fee = 0.0;
                double capitalized_interest = 0.0;
                double new_principal_due = 0.0;
                double prior_principal_due = 0.0;
                double opening_interest = 0.0;
                double interest_accrual = 0.0;
                double new_interest_due = 0.0;
                double prior_interest_due = 0.0;
                if (!exact_number(result.opening_principal_million,
                        opening_principal) ||
                    !exact_number(result.funded_principal_million,
                        funded_principal) ||
                    !exact_number(result.capitalized_fee_million,
                        capitalized_fee) ||
                    !exact_number(result.capitalized_interest_million,
                        capitalized_interest) ||
                    !exact_number(result.principal_due_million,
                        new_principal_due) ||
                    !exact_number(result.opening_accrued_interest_million,
                        opening_interest) ||
                    !exact_number(result.interest_accrual_million,
                        interest_accrual) ||
                    !exact_number(result.interest_due_million,
                        new_interest_due)) {
                    return false;
                }
                if (period > 0U) {
                    if (!exact_number(
                            periods[period - 1U]
                                .outstanding_principal_due_million,
                            prior_principal_due) ||
                        !exact_number(
                            periods[period - 1U]
                                .outstanding_interest_due_million,
                            prior_interest_due)) {
                        return false;
                    }
                }
                const double principal_before_resolution =
                    opening_principal + funded_principal + capitalized_fee +
                    capitalized_interest;
                const double principal_due_before_resolution =
                    prior_principal_due + new_principal_due;
                const double interest_before_resolution =
                    opening_interest + interest_accrual -
                    capitalized_interest;
                const double interest_due_before_resolution = std::max(0.0,
                    prior_interest_due + new_interest_due -
                        capitalized_interest);
                if (!amounts_equal(principal_before_resolution,
                        principal_due_before_resolution) ||
                    !amounts_equal(interest_before_resolution,
                        interest_due_before_resolution)) {
                    return false;
                }
                if (!exact_values_equal(result.closing_principal_million,
                        result.outstanding_principal_due_million) ||
                    !exact_values_equal(
                        result.closing_accrued_interest_million,
                        result.outstanding_interest_due_million)) {
                    return false;
                }
            }
            return true;
        });
}

[[nodiscard]] ClaimLedgerSourceEvidenceSnapshot make_source_snapshot(
    std::string_view source_record_id,
    const SourceManifestIndex& source_index) {
    ClaimLedgerSourceEvidenceSnapshot result;
    result.source_record_id = std::string(source_record_id);
    const auto source = source_index.all_sources.find(result.source_record_id);
    if (source == source_index.all_sources.end()) {
        if (source_record_id != "NO_PUBLIC_SOURCE" &&
            source_record_id != "SYNTHETIC") {
            throw std::logic_error(
                "full-path source evidence lost ordinary source-manifest metadata: " +
                result.source_record_id);
        }
    } else {
        result.record_date = source->second.record_date;
        result.evidence_class = source->second.evidence_class;
        result.provenance_tag = source->second.provenance_tag;
    }
    result.retained_copy_verified =
        source_index.retained_verified_sources.contains(
            result.source_record_id);
    return result;
}

[[nodiscard]] ClaimLedgerCovenantEvidenceSnapshot
make_covenant_evidence_snapshot(const ParsedCovenant& parsed,
    const SourceManifestIndex& source_index) {
    if (!parsed.period.has_value() ||
        !parsed.known_at_period.has_value()) {
        throw std::logic_error(
            "full-path covenant evidence lost structural timing");
    }
    ClaimLedgerCovenantEvidenceSnapshot result;
    result.common = parsed.common;
    result.scenario_id = parsed.scenario_id;
    result.event.event_id = parsed.event_id;
    result.event.covenant_id = parsed.covenant_id;
    result.event.period = *parsed.period;
    result.event.known_at_period = *parsed.known_at_period;
    result.event.state = parsed.state;
    result.event.source_record_id = parsed.source_record_id;
    result.event_date = parsed.event_date;
    result.input_status = parsed.input_status;
    result.source = make_source_snapshot(
        parsed.source_record_id, source_index);
    return result;
}

[[nodiscard]] ClaimLedgerPathEvidenceSnapshot
make_full_path_evidence_snapshot(const ClaimLedgerPackage& package,
    const ParsedTables& tables,
    const SourceManifestIndex& source_index,
    std::string_view scenario_id) {
    if (!package.full_path_evaluation_available ||
        !package.full_core_config.has_value() ||
        !package.full_evaluation.has_value()) {
        throw std::logic_error(
            "full-path evidence requires the verified full evaluation");
    }

    ClaimLedgerPathEvidenceSnapshot result;
    result.package_directory = package.directory;
    result.model_version = package.config.model_version;
    result.package_id = package.config.package_id;
    result.project_id = package.config.project_id;
    result.claim_id = package.config.claim_id;
    result.scenario_id = std::string(scenario_id);
    result.currency_label = package.config.currency_label;
    result.monetary_basis = package.config.monetary_basis;
    result.decision_period = *package.config.decision_period;
    result.horizon_period = *package.config.horizon_period;
    result.claim_config_sha256 = package.claim_config_sha256;
    result.source_manifest_sha256 =
        package.config.source_manifest_sha256;
    result.contractual_face_amount_million =
        package.config.contractual_face_amount_million;
    result.contractual_face_source = make_source_snapshot(
        package.config.contractual_face_amount_million.source_record_id,
        source_index);
    result.opening_principal_million =
        package.config.opening_principal_million;
    result.opening_principal_source = make_source_snapshot(
        package.config.opening_principal_million.source_record_id,
        source_index);
    result.opening_accrued_interest_million =
        package.config.opening_accrued_interest_million;
    result.opening_accrued_interest_source = make_source_snapshot(
        package.config.opening_accrued_interest_million.source_record_id,
        source_index);

    const auto parsed_scenario = std::find_if(tables.scenarios.begin(),
        tables.scenarios.end(), [&](const ParsedScenario& candidate) {
            return candidate.scenario_id == scenario_id;
        });
    const auto evaluated = std::find_if(
        package.full_evaluation->scenarios.begin(),
        package.full_evaluation->scenarios.end(),
        [&](const ClaimLedgerScenarioResult& candidate) {
            return candidate.scenario_id == scenario_id;
        });
    if (parsed_scenario == tables.scenarios.end() ||
        evaluated == package.full_evaluation->scenarios.end()) {
        invalid("requested full-path evidence scenario is unknown");
    }
    result.full_evaluation_scenario_index =
        static_cast<std::size_t>(std::distance(
            package.full_evaluation->scenarios.begin(), evaluated));
    result.cash_path_status.cash_path_status =
        parsed_scenario->cash_path_status;
    result.cash_path_status.known_at_period =
        parsed_scenario->cash_path_status_known_at_period;
    result.cash_path_status.status_was_unknown =
        parsed_scenario->cash_path_status_unknown;
    result.cash_path_status.input_status =
        parsed_scenario->cash_path_input_status;
    result.cash_path_status.source = make_source_snapshot(
        parsed_scenario->cash_path_status_source_record_id,
        source_index);

    const auto selected = selected_full_entry_versions_for_scenario(
        tables, scenario_id);
    result.selected_entries.reserve(selected.size());
    for (const ParsedEntry* parsed : selected) {
        ClaimLedgerSelectedEntryEvidenceSnapshot evidence;
        evidence.entry = make_core_entry(*parsed);
        evidence.scenario_id = parsed->scenario_id;
        evidence.input_status = parsed->input_status;
        evidence.source = make_source_snapshot(
            parsed->source_record_id, source_index);
        result.selected_entries.push_back(std::move(evidence));
    }

    std::unordered_set<std::string> evaluated_provider_ids;
    for (const ClaimLedgerProviderPathResult& provider :
         evaluated->full_path.provider_claims) {
        evaluated_provider_ids.emplace(provider.provider_claim_id);
    }

    for (const ParsedProvider& parsed : tables.providers) {
        if (!evaluated_provider_ids.contains(parsed.provider_claim_id)) {
            continue;
        }
        const auto term = std::find_if(
            package.full_core_config->provider_claims.begin(),
            package.full_core_config->provider_claims.end(),
            [&](const ClaimLedgerProviderClaim& candidate) {
                return candidate.provider_claim_id ==
                    parsed.provider_claim_id;
            });
        if (term == package.full_core_config->provider_claims.end()) {
            throw std::logic_error(
                "full-path provider evidence lost its evaluated term");
        }
        ClaimLedgerProviderTermEvidenceSnapshot evidence;
        evidence.term = *term;
        evidence.input_status = parsed.input_status;
        evidence.source = make_source_snapshot(
            parsed.source_record_id, source_index);
        result.provider_terms.push_back(std::move(evidence));
    }
    std::sort(result.provider_terms.begin(), result.provider_terms.end(),
        [](const auto& left, const auto& right) {
            return left.term.provider_claim_id <
                right.term.provider_claim_id;
        });
    if (result.provider_terms.size() != evaluated_provider_ids.size()) {
        throw std::logic_error(
            "full-path provider evidence does not match the evaluated path");
    }

    for (const ParsedCovenant& parsed : tables.covenants) {
        if (!parsed.common && parsed.scenario_id != scenario_id) {
            continue;
        }
        result.covenant_events.push_back(
            make_covenant_evidence_snapshot(parsed, source_index));
    }
    std::sort(result.covenant_events.begin(),
        result.covenant_events.end(),
        [](const auto& left, const auto& right) {
            if (left.event.event_id != right.event.event_id) {
                return left.event.event_id < right.event.event_id;
            }
            return left.event.period < right.event.period;
        });
    return result;
}

} // namespace

namespace {

struct LoadedClaimLedgerPackage {
    ClaimLedgerPackage package{};
    std::optional<ClaimLedgerPathEvidenceSnapshot> full_path_evidence{};
};

[[nodiscard]] LoadedClaimLedgerPackage load_claim_ledger_package_impl(
    const std::filesystem::path& claim_config_path,
    std::optional<std::string_view> requested_scenario_id) {
    ClaimLedgerPackage package;
    std::optional<ClaimLedgerPathEvidenceSnapshot> full_path_evidence;
    std::error_code error;
    const std::filesystem::path absolute_config =
        std::filesystem::absolute(claim_config_path, error);
    if (error) invalid("could not make claim.cfg path absolute");
    const std::filesystem::path canonical_config =
        std::filesystem::canonical(absolute_config, error);
    if (error) invalid("could not resolve claim.cfg");
    if (canonical_config.filename() != "claim.cfg") {
        invalid("claim-ledger package root must use the exact portable filename claim.cfg");
    }
    package.directory = std::filesystem::canonical(
        canonical_config.parent_path(), error);
    if (error) invalid("could not resolve claim package directory");
    package.claim_config_filename = canonical_config.filename();
    const std::string config_bytes = read_regular_file(
        canonical_config, kMaximumConfigBytes, "claim.cfg");
    package.claim_config_sha256 = sha256_bytes_lower_hex(config_bytes);
    package.config = parse_claim_ledger_package_config(config_bytes);
    add_config_blockers(package);

    ParsedTables tables;
    collect_config_sources(tables, package.config);
    std::unordered_set<std::string> entry_ids;
    std::unordered_set<std::string> scenario_ids;
    std::unordered_set<std::string> provider_ids;
    std::unordered_set<std::string> covenant_event_ids;

    {
        const BoundSnapshot snapshot = load_bound_snapshot(
            package.directory, package.config.terms, "terms.tsv");
        parse_terms(snapshot.bytes, package, tables);
    }
    {
        const BoundSnapshot snapshot = load_bound_snapshot(package.directory,
            package.config.common_entries, "common_entries.tsv");
        parse_common_entries(snapshot.bytes, package, tables, entry_ids);
    }
    {
        const BoundSnapshot snapshot = load_bound_snapshot(
            package.directory, package.config.scenarios, "scenarios.tsv");
        parse_scenarios(snapshot.bytes, package, tables, scenario_ids);
    }
    {
        const BoundSnapshot snapshot = load_bound_snapshot(package.directory,
            package.config.scenario_entries, "scenario_entries.tsv");
        parse_scenario_entries(snapshot.bytes, package, tables, scenario_ids,
            entry_ids);
    }
    {
        const BoundSnapshot snapshot = load_bound_snapshot(package.directory,
            package.config.provider_claims, "provider_claims.tsv");
        parse_provider_claims(snapshot.bytes, package, tables, provider_ids);
    }
    {
        const BoundSnapshot snapshot = load_bound_snapshot(package.directory,
            package.config.covenant_events, "covenant_events.tsv");
        parse_covenant_events(snapshot.bytes, package, tables, scenario_ids,
            covenant_event_ids);
    }
    {
        const BoundSnapshot snapshot = load_bound_snapshot(package.directory,
            package.config.conversion_context, "conversion_context.tsv");
        parse_conversion_context(snapshot.bytes, package, tables);
    }

    const std::size_t selector_path_count =
        std::max<std::size_t>(1U, tables.scenarios.size());
    if (!tables.entries.empty() &&
        selector_path_count >
            kClaimLedgerMaximumPathEntryVisits / tables.entries.size()) {
        invalid("package decision-version selection work product exceeds the resource guardrail");
    }

    validate_entry_lineage(tables.entries);

    for (const ParsedEntry& entry : tables.entries) {
        if (guarantee_kind(entry.kind) &&
            !provider_ids.contains(entry.provider_claim_id)) {
            invalid("guarantee entry references unknown provider_claim_id: " +
                    entry.provider_claim_id);
        }
    }
    const SourceManifestIndex retained_verified_sources =
        validate_source_resolution(
            package.directory, package.config, tables.source_record_ids);

    package.package_integrity = true;
    package.core_config_ready =
        structural_core_ready(package.config, tables, package);
    if (package.core_config_ready) {
        package.core_config = make_core_config(
            package.config, tables, true);
        validate_claim_ledger_config(*package.core_config);
        package.evaluation = evaluate_claim_ledger(*package.core_config);
        remove_unavailable_full_state(*package.evaluation);
        append_core_blockers(package);

        if (full_core_structurally_representable(tables)) {
            try {
                package.full_core_config = make_core_config(
                    package.config, tables, false);
                validate_claim_ledger_config(*package.full_core_config);
                package.full_evaluation =
                    evaluate_claim_ledger(*package.full_core_config);
                package.full_path_evaluation_available = true;
            } catch (const std::invalid_argument& error) {
                package.full_core_config.reset();
                package.full_evaluation.reset();
                add_critical_blocker(package.blockers,
                    "full/backtest evaluation is unavailable because retained later state is structurally inconsistent: " +
                        std::string(error.what()));
            }
        } else {
            add_critical_blocker(package.blockers,
                "full/backtest evaluation is unavailable because retained later rows are structurally incomplete");
        }
    }

    if (requested_scenario_id.has_value()) {
        if (!package.full_path_evaluation_available) {
            invalid(
                "requested full-path evidence requires an available package-wide full evaluation");
        }
        full_path_evidence = make_full_path_evidence_snapshot(
            package, tables, retained_verified_sources,
            *requested_scenario_id);
    }

    const bool controlled_candidate =
        package.config.package_status ==
        ClaimLedgerPackageStatus::ControlledCandidate;
    const bool mechanical_expected_cash_ready =
        package.evaluation.has_value() &&
        package.evaluation->readiness.expected_cash_ready;
    const bool rate_ready = package.evaluation.has_value() &&
        package.evaluation->readiness.rate_preimage_ready;
    const std::vector<const ParsedEntry*> selected_entries =
        package.config.decision_period.has_value()
        ? selected_decision_entry_versions(
              tables, *package.config.decision_period)
        : std::vector<const ParsedEntry*>{};
    const bool core_inputs_admissible =
        empirically_admissible(
            package.config.contractual_face_amount_million.input_status) &&
        empirically_admissible(
            package.config.opening_principal_million.input_status) &&
        empirically_admissible(
            package.config.opening_accrued_interest_million.input_status) &&
        std::all_of(selected_entries.begin(), selected_entries.end(),
            [](const ParsedEntry* row) {
                return empirically_admissible(row->input_status);
            }) &&
        std::all_of(tables.scenarios.begin(), tables.scenarios.end(),
            [](const ParsedScenario& row) {
                return physical_probability_admissible(
                           row.probability_input_status) &&
                    completeness_attestation_admissible(
                        row.cash_path_input_status);
            }) &&
        std::all_of(tables.providers.begin(), tables.providers.end(),
            [&package](const ParsedProvider& row) {
                if (!row.known_at_period.has_value() ||
                    !package.config.decision_period.has_value()) {
                    return false;
                }
                return *row.known_at_period >
                        *package.config.decision_period ||
                    empirically_admissible(row.input_status);
            });
    const bool exact_return_timing =
        package.config.execution_date.status ==
            ClaimLedgerDateStatus::Known &&
        package.config.funding_date.status == ClaimLedgerDateStatus::Known &&
        package.config.settlement_date.status ==
            ClaimLedgerDateStatus::Known &&
        package.config.period_origin_date.status ==
            ClaimLedgerDateStatus::Known &&
        package.config.decision_date.status == ClaimLedgerDateStatus::Known &&
        package.config.maturity_date.status == ClaimLedgerDateStatus::Known &&
        package.config.horizon_date.status == ClaimLedgerDateStatus::Known &&
        package.config.decision_period.has_value() &&
        package.config.horizon_period.has_value();
    const bool primary_settlement_timing_ready =
        primary_settlement_timing_consistent(
            package.config, tables);
    const bool sources_evidenced = expected_return_sources_evidenced(
        package.config, tables, selected_entries,
        retained_verified_sources);
    const bool maturity_obligations_ready =
        maturity_obligations_consistent(package.config, tables,
            package.evaluation.has_value() ? &*package.evaluation : nullptr);
    package.expected_return_admissible = controlled_candidate &&
        mechanical_expected_cash_ready && rate_ready &&
        core_inputs_admissible && exact_return_timing && sources_evidenced &&
        maturity_obligations_ready;
    package.expected_return_admissible =
        package.expected_return_admissible &&
        primary_settlement_timing_ready;
    const bool mechanical_npv_ready = package.evaluation.has_value() &&
        package.evaluation->readiness.npv_ready;
    const bool discount_rate_evidenced =
        package.config.decision_period.has_value() &&
        package.config.decision_date.value.has_value() &&
        package.config.annual_effective_discount_rate.known_at_period
            .has_value() &&
        *package.config.annual_effective_discount_rate.known_at_period <=
            *package.config.decision_period &&
        empirically_admissible(
            package.config.annual_effective_discount_rate.input_status) &&
        verified_source_at_or_before(
            package.config.annual_effective_discount_rate.source_record_id,
            retained_verified_sources,
            *package.config.decision_date.value);
    package.npv_admissible = package.expected_return_admissible &&
        mechanical_npv_ready && discount_rate_evidenced;
    const bool observation_boundary_ready =
        package.config.economic_cluster_boundary_status ==
            ClaimLedgerEconomicClusterBoundaryStatus::Defined &&
        package.config.economic_cluster_boundary_known_at_period.has_value() &&
        package.config.decision_period.has_value() &&
        *package.config.economic_cluster_boundary_known_at_period <=
            *package.config.decision_period &&
        (package.config.economic_cluster_boundary_input_status ==
                ClaimLedgerInputStatus::Observed ||
            package.config.economic_cluster_boundary_input_status ==
                ClaimLedgerInputStatus::Derived) &&
        package.config.decision_date.value.has_value() &&
        verified_source_at_or_before(
            package.config.economic_cluster_boundary_source_record_id,
            retained_verified_sources,
            *package.config.decision_date.value) &&
        package.config.observation_date.status == ClaimLedgerDateStatus::Known &&
        package.config.settlement_date.status == ClaimLedgerDateStatus::Known &&
        package.config.observation_date.value ==
            package.config.settlement_date.value &&
        package.config.settlement_date.value.has_value() &&
        verified_closing_source_on_date(
            package.config.observation_date.source_record_id,
            retained_verified_sources,
            *package.config.settlement_date.value);
    const bool primary_settlement_observed =
        common_primary_settlement_observed(package.config, tables,
            retained_verified_sources);
    package.observation_admissible =
        package.expected_return_admissible && observation_boundary_ready &&
        primary_settlement_observed;
    if (!controlled_candidate) {
        add_critical_blocker(package.blockers,
            package.config.package_status ==
                    ClaimLedgerPackageStatus::SyntheticComplete
                ? "synthetic package is not an admissible expected-return observation"
                : "retained public incomplete package is not a controlled expected-return candidate");
    }
    if (!mechanical_expected_cash_ready) {
        add_critical_blocker(package.blockers,
            "mechanical expected-cash ledger is not ready for expected-return admission");
    }
    if (!rate_ready) {
        add_critical_blocker(package.blockers,
            "expected-cash rate preimage is not ready for expected-return admission");
    }
    if (!core_inputs_admissible) {
        add_critical_blocker(package.blockers,
            "stress, backtest, or unknown decision-cut inputs block expected-return admission");
    }
    if (!exact_return_timing) {
        add_critical_blocker(package.blockers,
            "monthly origin, decision, and horizon timing is incomplete");
    }
    if (!primary_settlement_timing_ready) {
        add_critical_blocker(package.blockers,
            "funding or settlement date is outside at least one scenario path's earliest selected buyer-price monthly bucket");
    }
    if (!sources_evidenced) {
        add_critical_blocker(package.blockers,
            "one or more decision-cut expected-return inputs lack retained evidence dated no later than the decision date");
    }
    if (!maturity_obligations_ready) {
        add_critical_blocker(package.blockers,
            "maturity is not reconciled to every scenario path: surviving obligations must be due from the maturity bucket onward and no positive funding may occur later");
    }
    if (controlled_candidate && mechanical_npv_ready &&
        !discount_rate_evidenced) {
        add_critical_blocker(package.blockers,
            "mechanical NPV is not admissible because its discount rate lacks decision-cut retained evidence");
    }
    if (!observation_boundary_ready) {
        add_critical_blocker(package.blockers,
            "economic-cluster evidence, observation-date equality, or settlement-dated observation evidence blocks market-observation admission");
    }
    if (!primary_settlement_observed) {
        add_critical_blocker(package.blockers,
            "market-observation admission requires one common primary closing group with observed buyer cash facts, observed-or-derived bridge facts, settlement-dated transaction evidence, and no scenario-specific funding in its anchor bucket");
    }
    std::sort(package.blockers.begin(), package.blockers.end());
    package.blockers.erase(
        std::unique(package.blockers.begin(), package.blockers.end()),
        package.blockers.end());
    return {std::move(package), std::move(full_path_evidence)};
}

} // namespace

ClaimLedgerPackage load_claim_ledger_package(
    const std::filesystem::path& claim_config_path) {
    return load_claim_ledger_package_impl(
        claim_config_path, std::nullopt).package;
}

ClaimLedgerPackageWithPathEvidence
load_claim_ledger_package_with_full_path_evidence(
    const std::filesystem::path& claim_config_path,
    std::string_view scenario_id) {
    require_safe_identifier(
        scenario_id, "requested full-path evidence scenario_id");
    LoadedClaimLedgerPackage loaded = load_claim_ledger_package_impl(
        claim_config_path, scenario_id);
    if (!loaded.full_path_evidence.has_value()) {
        throw std::logic_error(
            "full-path evidence load returned without its snapshot");
    }
    return {std::move(loaded.package),
        std::move(*loaded.full_path_evidence)};
}

namespace {

[[nodiscard]] ClaimLedgerValue report_value(
    double lower, double upper) {
    if (lower == upper) return claim_ledger_known(lower);
    return claim_ledger_bounded(lower, upper);
}

[[nodiscard]] ClaimLedgerValue add_report_values(
    const ClaimLedgerValue& left, const ClaimLedgerValue& right) {
    if (left.status == ClaimLedgerValueStatus::Unknown ||
        right.status == ClaimLedgerValueStatus::Unknown) {
        return claim_ledger_unknown();
    }
    if (left.status == ClaimLedgerValueStatus::NotApplicable ||
        right.status == ClaimLedgerValueStatus::NotApplicable) {
        return claim_ledger_not_applicable();
    }
    if (!left.lower.has_value() || !left.upper.has_value() ||
        !right.lower.has_value() || !right.upper.has_value()) {
        return claim_ledger_unknown();
    }
    return report_value(
        *left.lower + *right.lower, *left.upper + *right.upper);
}

[[nodiscard]] ClaimLedgerValue multiply_non_negative_report_values(
    const ClaimLedgerValue& left, const ClaimLedgerValue& right) {
    if (left.status == ClaimLedgerValueStatus::Unknown ||
        right.status == ClaimLedgerValueStatus::Unknown) {
        return claim_ledger_unknown();
    }
    if (left.status == ClaimLedgerValueStatus::NotApplicable ||
        right.status == ClaimLedgerValueStatus::NotApplicable) {
        return claim_ledger_not_applicable();
    }
    if (!left.lower.has_value() || !left.upper.has_value() ||
        !right.lower.has_value() || !right.upper.has_value() ||
        *left.lower < 0.0 || *right.lower < 0.0) {
        return claim_ledger_unknown();
    }
    return report_value(
        *left.lower * *right.lower, *left.upper * *right.upper);
}

[[nodiscard]] ClaimLedgerValue period_receipts(
    const ClaimLedgerPeriodResult& period) {
    ClaimLedgerValue result = claim_ledger_known(0.0);
    const std::array<const ClaimLedgerValue*, 7U> receipts{{
        &period.cash_fee_million,
        &period.principal_cash_million,
        &period.interest_cash_million,
        &period.recovery_principal_cash_million,
        &period.recovery_interest_cash_million,
        &period.guarantee_principal_cash_million,
        &period.guarantee_interest_cash_million,
    }};
    for (const ClaimLedgerValue* value : receipts) {
        result = add_report_values(result, *value);
    }
    return result;
}

[[nodiscard]] ClaimLedgerValue period_buyer_outflow(
    const ClaimLedgerPeriodResult& period) {
    return add_report_values(
        period.buyer_price_million, period.buyer_direct_cost_million);
}

template <typename Extractor>
[[nodiscard]] ClaimLedgerValue expected_scenario_value(
    const ClaimLedgerSummary& summary, Extractor&& extract) {
    if (summary.scenarios.empty()) {
        return claim_ledger_not_applicable();
    }
    if (!summary.readiness.expected_cash_ready) {
        return claim_ledger_unknown();
    }
    ClaimLedgerValue result = claim_ledger_known(0.0);
    for (const ClaimLedgerScenarioResult& scenario : summary.scenarios) {
        result = add_report_values(
            result,
            multiply_non_negative_report_values(
                scenario.physical_probability, extract(scenario)));
    }
    return result;
}

[[nodiscard]] ClaimLedgerValue expected_buyer_outflow_t0(
    const ClaimLedgerSummary& summary) {
    return expected_scenario_value(
        summary, [](const ClaimLedgerScenarioResult& scenario) {
            if (scenario.decision_path.periods.empty()) {
                return claim_ledger_unknown();
            }
            return period_buyer_outflow(scenario.decision_path.periods[0U]);
        });
}

[[nodiscard]] ClaimLedgerValue expected_terminal_receipts(
    const ClaimLedgerSummary& summary) {
    return expected_scenario_value(
        summary, [](const ClaimLedgerScenarioResult& scenario) {
            if (scenario.decision_path.periods.empty()) {
                return claim_ledger_unknown();
            }
            return period_receipts(scenario.decision_path.periods.back());
        });
}

[[nodiscard]] ClaimLedgerValue expected_total_receipts(
    const ClaimLedgerSummary& summary) {
    return expected_scenario_value(
        summary, [](const ClaimLedgerScenarioResult& scenario) {
            ClaimLedgerValue result = claim_ledger_known(0.0);
            for (const ClaimLedgerPeriodResult& period :
                 scenario.decision_path.periods) {
                result = add_report_values(result, period_receipts(period));
            }
            return result;
        });
}

[[nodiscard]] ClaimLedgerValue peak_expected_ead(
    const ClaimLedgerSummary& summary) {
    if (summary.expected_ead_million.empty()) {
        return claim_ledger_not_applicable();
    }
    double lower = 0.0;
    double upper = 0.0;
    bool initialized = false;
    for (const ClaimLedgerValue& value : summary.expected_ead_million) {
        if (value.status == ClaimLedgerValueStatus::Unknown) {
            return claim_ledger_unknown();
        }
        if (value.status == ClaimLedgerValueStatus::NotApplicable ||
            !value.lower.has_value() || !value.upper.has_value()) {
            return claim_ledger_not_applicable();
        }
        if (!initialized) {
            lower = *value.lower;
            upper = *value.upper;
            initialized = true;
        } else {
            lower = std::max(lower, *value.lower);
            upper = std::max(upper, *value.upper);
        }
    }
    return initialized
        ? report_value(lower, upper)
        : claim_ledger_not_applicable();
}

struct ExpectedProviderAmounts {
    ClaimLedgerValue claim_generated{claim_ledger_not_applicable()};
    ClaimLedgerValue guarantee_cash{claim_ledger_not_applicable()};
};

[[nodiscard]] ExpectedProviderAmounts expected_provider_amounts(
    const ClaimLedgerSummary& summary) {
    bool any_provider = false;
    for (const ClaimLedgerScenarioResult& scenario : summary.scenarios) {
        any_provider = any_provider ||
            !scenario.decision_path.provider_claims.empty();
    }
    if (!any_provider) return {};
    if (!summary.readiness.expected_cash_ready) {
        return {claim_ledger_unknown(), claim_ledger_unknown()};
    }
    ExpectedProviderAmounts result{
        claim_ledger_known(0.0), claim_ledger_known(0.0)};
    for (const ClaimLedgerScenarioResult& scenario : summary.scenarios) {
        ClaimLedgerValue scenario_claim = claim_ledger_known(0.0);
        ClaimLedgerValue scenario_cash = claim_ledger_known(0.0);
        for (const ClaimLedgerProviderPathResult& provider :
             scenario.decision_path.provider_claims) {
            scenario_claim = add_report_values(
                scenario_claim, provider.total_claim_generated_million);
            scenario_cash = add_report_values(
                scenario_cash, provider.total_guarantee_cash_million);
        }
        result.claim_generated = add_report_values(
            result.claim_generated,
            multiply_non_negative_report_values(
                scenario.physical_probability, scenario_claim));
        result.guarantee_cash = add_report_values(
            result.guarantee_cash,
            multiply_non_negative_report_values(
                scenario.physical_probability, scenario_cash));
    }
    return result;
}

[[nodiscard]] std::string format_number(double value) {
    if (value == 0.0) value = 0.0;
    std::array<char, 128U> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value,
        std::chars_format::general);
    if (converted.ec != std::errc{}) {
        throw std::logic_error("could not format claim-ledger report number");
    }
    return std::string(buffer.data(), converted.ptr);
}

[[nodiscard]] std::string format_report_value(
    const ClaimLedgerValue& value) {
    if (value.status == ClaimLedgerValueStatus::Unknown) return "UNKNOWN";
    if (value.status == ClaimLedgerValueStatus::NotApplicable) {
        return "NOT_APPLICABLE";
    }
    if (!value.lower.has_value() || !value.upper.has_value()) {
        throw std::logic_error(
            "known/bounded report value is missing its endpoints");
    }
    if (value.status == ClaimLedgerValueStatus::Known) {
        return format_number(*value.lower);
    }
    return "BOUNDED[" + format_number(*value.lower) + "," +
        format_number(*value.upper) + "]";
}

[[nodiscard]] std::string format_report_date(
    const ClaimLedgerTypedDate& value) {
    return value.status == ClaimLedgerDateStatus::Known &&
            value.value.has_value()
        ? *value.value
        : "UNKNOWN";
}

[[nodiscard]] std::string format_report_period(
    const std::optional<std::size_t>& value) {
    return value.has_value() ? std::to_string(*value) : "UNKNOWN";
}

} // namespace

namespace {

void render_claim_ledger_package_report(
    std::ostream& output, const ClaimLedgerPackage& package) {
    if (!package.package_integrity) {
        throw std::logic_error(
            "claim-ledger package report requires verified package integrity");
    }
    output
        << "PROJECT CLAIM LEDGER v0.1 PACKAGE REVIEW\n"
        << "package_id=" << package.config.package_id << '\n'
        << "package_status=" << to_string(package.config.package_status) << '\n'
        << "project_id=" << package.config.project_id << '\n'
        << "claim_id=" << package.config.claim_id << '\n'
        << "economic_cluster_id=" << package.config.economic_cluster_id << '\n'
        << "economic_cluster_boundary_status="
        << to_string(package.config.economic_cluster_boundary_status) << '\n'
        << "package_integrity=verified\n"
        << "core_config_ready="
        << (package.core_config_ready ? "true" : "false") << '\n'
        << "core_evaluation="
        << (package.evaluation.has_value() ? "performed" : "not-performed")
        << '\n'
        << "full_backtest_evaluation="
        << (package.full_path_evaluation_available
                ? "performed"
                : "unavailable")
        << '\n'
        << "core_readiness_scope=mechanical-claim-ledger-evaluation\n"
        << "expected_return_admissible="
        << (package.expected_return_admissible ? "true" : "false") << '\n'
        << "npv_admissible="
        << (package.npv_admissible ? "true" : "false") << '\n'
        << "observation_admissible="
        << (package.observation_admissible ? "true" : "false")
        << "\n\nAnalysis basis\n"
        << "  currency_label=" << package.config.currency_label << '\n'
        << "  monetary_basis=" << package.config.monetary_basis << '\n'
        << "  period_unit=" << package.config.period_unit_label << '\n'
        << "  periods_per_year=" << package.config.periods_per_year << '\n'
        << "  period_origin_date="
        << format_report_date(package.config.period_origin_date) << '\n'
        << "  decision_date="
        << format_report_date(package.config.decision_date) << '\n'
        << "  execution_date="
        << format_report_date(package.config.execution_date) << '\n'
        << "  funding_date="
        << format_report_date(package.config.funding_date) << '\n'
        << "  settlement_date="
        << format_report_date(package.config.settlement_date) << '\n'
        << "  observation_date="
        << format_report_date(package.config.observation_date) << '\n'
        << "  maturity_date="
        << format_report_date(package.config.maturity_date) << '\n'
        << "  horizon_date="
        << format_report_date(package.config.horizon_date) << '\n'
        << "  decision_period="
        << format_report_period(package.config.decision_period) << '\n'
        << "  horizon_period="
        << format_report_period(package.config.horizon_period) << '\n'
        << "  annual_effective_discount_rate="
        << format_report_value(
               package.config.annual_effective_discount_rate.value)
        << '\n'
        << "  discount_rate_input_status="
        << to_string(
               package.config.annual_effective_discount_rate.input_status)
        << '\n'
        << "  discount_rate_known_at_period="
        << format_report_period(
               package.config.annual_effective_discount_rate.known_at_period)
        << '\n';
    if (package.evaluation.has_value()) {
        const ClaimLedgerSummary& summary = *package.evaluation;
        const ClaimLedgerValue buyer_outflow =
            expected_buyer_outflow_t0(summary);
        const ClaimLedgerValue terminal_receipts =
            expected_terminal_receipts(summary);
        const ClaimLedgerValue total_receipts =
            expected_total_receipts(summary);
        const ClaimLedgerValue peak_ead = peak_expected_ead(summary);
        const ExpectedProviderAmounts provider =
            expected_provider_amounts(summary);
        output
            << "mechanical_expected_cash_ready="
            << (summary.readiness.expected_cash_ready
                    ? "true"
                    : "false")
            << '\n'
            << "mechanical_npv_ready="
            << (summary.readiness.npv_ready
                    ? "true"
                    : "false")
            << '\n'
            << "mechanical_rate_preimage_ready="
            << (summary.readiness.rate_preimage_ready
                    ? "true"
                    : "false")
            << '\n'
            << "provider_claim_ready="
            << (summary.readiness.provider_claim_ready
                    ? "true"
                    : "false")
            << "\n\nFinancial reconstruction\n"
            << "  expected_buyer_cash_outflow_t0_million="
            << format_report_value(buyer_outflow) << '\n'
            << "  expected_investor_cashflow_t0_million="
            << (summary.expected_investor_cashflows_million.empty()
                    ? "NOT_APPLICABLE"
                    : format_report_value(
                          summary.expected_investor_cashflows_million.front()))
            << '\n'
            << "  expected_terminal_receipts_million="
            << format_report_value(terminal_receipts) << '\n'
            << "  expected_total_receipts_million="
            << format_report_value(total_receipts) << '\n'
            << "  expected_npv_million="
            << format_report_value(summary.expected_npv_million) << '\n'
            << "  expected_principal_loss_million="
            << format_report_value(summary.expected_principal_loss_million)
            << '\n'
            << "  expected_total_loss_million="
            << format_report_value(summary.expected_total_loss_million)
            << '\n'
            << "  expected_principal_cash_wal_months="
            << format_report_value(
                   summary.expected_principal_cash_wal_months)
            << '\n'
            << "  annual_effective_rate_preimage="
            << format_report_value(summary.annual_effective_rate_preimage)
            << '\n'
            << "  peak_expected_ead_million="
            << format_report_value(peak_ead) << '\n'
            << "  expected_provider_claim_generated_million="
            << format_report_value(provider.claim_generated) << '\n'
            << "  expected_provider_guarantee_cash_million="
            << format_report_value(provider.guarantee_cash)
            << "\n\nExpected investor cash-flow schedule\n"
            << "  period|expected_investor_cashflow_million\n";
        for (std::size_t period = 0U;
             period < summary.expected_investor_cashflows_million.size();
             ++period) {
            output << "  " << period << '|'
                   << format_report_value(
                          summary.expected_investor_cashflows_million[period])
                   << '\n';
        }
    }
    output
        << "\nBound row counts\n"
        << "  terms=" << package.row_counts.terms << '\n'
        << "  common_entries=" << package.row_counts.common_entries << '\n'
        << "  scenarios=" << package.row_counts.scenarios << '\n'
        << "  scenario_entries=" << package.row_counts.scenario_entries << '\n'
        << "  provider_claims=" << package.row_counts.provider_claims << '\n'
        << "  covenant_events=" << package.row_counts.covenant_events << '\n'
        << "  conversion_context="
        << package.row_counts.conversion_context
        << "\n\nBound snapshots\n"
        << "  claim_config="
        << package.claim_config_filename.generic_string() << '|'
        << package.claim_config_sha256 << '\n'
        << "  terms=" << package.config.terms.relative_path.generic_string()
        << '|' << package.config.terms.sha256 << '\n'
        << "  common_entries="
        << package.config.common_entries.relative_path.generic_string() << '|'
        << package.config.common_entries.sha256 << '\n'
        << "  scenarios="
        << package.config.scenarios.relative_path.generic_string() << '|'
        << package.config.scenarios.sha256 << '\n'
        << "  scenario_entries="
        << package.config.scenario_entries.relative_path.generic_string()
        << '|' << package.config.scenario_entries.sha256 << '\n'
        << "  provider_claims="
        << package.config.provider_claims.relative_path.generic_string()
        << '|' << package.config.provider_claims.sha256 << '\n'
        << "  covenant_events="
        << package.config.covenant_events.relative_path.generic_string()
        << '|' << package.config.covenant_events.sha256 << '\n'
        << "  conversion_context="
        << package.config.conversion_context.relative_path.generic_string()
        << '|' << package.config.conversion_context.sha256 << '\n'
        << "  source_manifest_sha256="
        << package.config.source_manifest_sha256 << "\n\n"
        << "Derived blockers\n";
    if (package.blockers.empty()) {
        output << "  NONE\n";
    } else {
        for (const std::string& blocker : package.blockers) {
            output << "  - " << blocker << '\n';
        }
    }
    output
        << "\nInterpretation boundary\n"
        << "  Integrity verifies confined bytes, exact schemas, SHA-256 bindings,\n"
        << "  identifiers, and source-manifest resolution. It does not verify source\n"
        << "  truth, legal enforceability, credit quality, market value, fair value,\n"
        << "  a hurdle rate, an offering, or an investment recommendation. UNKNOWN\n"
        << "  remains a blocker and is never printed or evaluated as zero. Mechanical\n"
        << "  expected cash is not called an admissible expected return unless its\n"
        << "  decision-cut classifications, timing, and provenance also pass. A\n"
        << "  source record_date cutoff is a chronology check, not proof that the\n"
        << "  record was available before a same-day decision; controlled review\n"
        << "  must verify actual availability and ordering outside this package.\n";
}

} // namespace

void print_claim_ledger_package_report(
    std::ostream& output, const ClaimLedgerPackage& package) {
    if (!package.package_integrity || package.directory.empty() ||
        package.claim_config_filename != "claim.cfg" ||
        !is_lower_hex_sha256(package.claim_config_sha256)) {
        throw std::logic_error(
            "claim-ledger report requires a loader-verified root snapshot");
    }
    const ClaimLedgerPackage verified = load_claim_ledger_package(
        package.directory / package.claim_config_filename);
    if (verified.claim_config_sha256 != package.claim_config_sha256) {
        throw std::logic_error(
            "claim-ledger root snapshot changed after package verification");
    }
    render_claim_ledger_package_report(output, verified);
}

std::string_view to_string(ClaimLedgerPackageStatus value) noexcept {
    switch (value) {
    case ClaimLedgerPackageStatus::RetainedPublicIncomplete:
        return "retained-public-incomplete";
    case ClaimLedgerPackageStatus::SyntheticComplete:
        return "synthetic-complete";
    case ClaimLedgerPackageStatus::ControlledCandidate:
        return "controlled-candidate";
    }
    return "unknown";
}

std::string_view to_string(
    ClaimLedgerEconomicClusterBoundaryStatus value) noexcept {
    switch (value) {
    case ClaimLedgerEconomicClusterBoundaryStatus::Defined:
        return "defined";
    case ClaimLedgerEconomicClusterBoundaryStatus::Unresolved:
        return "unresolved";
    }
    return "unknown";
}

std::string_view to_string(ClaimLedgerInputStatus value) noexcept {
    switch (value) {
    case ClaimLedgerInputStatus::Observed: return "observed";
    case ClaimLedgerInputStatus::Contractual: return "contractual";
    case ClaimLedgerInputStatus::Derived: return "derived";
    case ClaimLedgerInputStatus::Estimated: return "estimated";
    case ClaimLedgerInputStatus::Stress: return "stress";
    case ClaimLedgerInputStatus::Backtest: return "backtest";
    case ClaimLedgerInputStatus::Unknown: return "unknown";
    }
    return "unknown";
}

std::string_view to_string(ClaimLedgerDateStatus value) noexcept {
    switch (value) {
    case ClaimLedgerDateStatus::Known: return "known";
    case ClaimLedgerDateStatus::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace naturalehia::cellular_finance
