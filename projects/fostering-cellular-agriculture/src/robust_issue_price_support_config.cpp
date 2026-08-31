// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_issue_price_support_config.hpp>

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
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMaximumAnnualEffectiveHurdleRate = 10.0;
constexpr double kMaximumMoneyMillion = 1.0e9;
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};
constexpr std::string_view kAbsent{"none"};

constexpr std::array<std::string_view, 49U> kFixedKeys{
    "issue_price_support.model_version",
    "issue_price_support.label",
    "issue_price_support.source_note",
    "issue_price_support.synthetic_inputs",
    "issue_price_support.market_claim_principal_is_fully_funded_at_issue",
    "issue_price_support.issue_support_and_price_fund_only_principal_and_issuer_costs",
    "issue_price_support.buyer_direct_cost_stays_outside_subscription_reserve",
    "issue_price_support.support_changes_no_claim_right_or_project_cash",
    "issue_price_support.physical_probability_polytope_is_unchanged",
    "issue_price_support.fair_value_or_market_price_is_estimated",
    "reference_price.id",
    "reference_price.status",
    "reference_price.market_claim_id",
    "reference_price.normalized_term_result_id",
    "reference_price.secondary_price_normalized_to_full_month_zero_claim",
    "reference_price.gross_issue_price_million",
    "reference_price.claim_quantity_million",
    "reference_price.quantity_basis",
    "reference_price.price_basis",
    "reference_price.currency_label",
    "reference_price.monetary_basis",
    "reference_price.execution_date",
    "reference_price.settlement_date",
    "reference_price.issuer_cost_million",
    "reference_price.buyer_direct_cost_million",
    "reference_price.side_rights_or_non_cash_consideration_present",
    "reference_price.side_rights_or_non_cash_consideration_note",
    "reference_price.source_reference",
    "reference_price.evidence_record_id",
    "reference_price.buyer_cash_payment_evidenced",
    "reference_price.settlement_evidenced",
    "reference_price.subscription_reserve_deposit_evidenced",
    "reference_price.issuer_cost_payment_evidenced",
    "reference_price.issue_use_evidence_record_id",
    "support.id",
    "support.status",
    "support.maximum_support_million",
    "support.settled_support_million",
    "support.funding_evidenced",
    "support.settlement_evidenced",
    "support.as_of_date",
    "support.source_reference",
    "support.evidence_record_id",
    "support.source_note",
    "support.support_is_non_repayable",
    "support.support_receives_no_repayment_participation_security_or_recovery_rights",
    "support.support_is_not_project_revenue",
    "support.support_does_not_pay_future_pool_costs_or_cover_project_losses",
    "hurdle_case.count",
};

constexpr std::array<std::string_view, 8U> kHurdleFields{
    "id",
    "annual_effective_hurdle_rate",
    "source_type",
    "reference_price_relation",
    "as_of_date",
    "source_reference",
    "evidence_record_id",
    "source_note",
};

constexpr std::size_t kMaximumRawEntries =
    kFixedKeys.size() +
    kRobustIssuePriceSupportMaximumHurdleCases * kHurdleFields.size();

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

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!is_safe_identifier(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a safe bounded identifier");
    }
}

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument(
        "issue-price-support configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "issue-price-support configuration is missing required key: " +
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

[[nodiscard]] RobustIssuePriceReferenceStatus parse_reference_status(
    const RawValue& raw) {
    if (raw.value == "internal_candidate") {
        return RobustIssuePriceReferenceStatus::InternalCandidate;
    }
    if (raw.value == "nonbinding_indication") {
        return RobustIssuePriceReferenceStatus::NonbindingIndication;
    }
    if (raw.value == "binding_unsettled_subscription") {
        return RobustIssuePriceReferenceStatus::BindingUnsettledSubscription;
    }
    if (raw.value == "executed_unsettled_primary") {
        return RobustIssuePriceReferenceStatus::ExecutedUnsettledPrimary;
    }
    if (raw.value == "settled_primary") {
        return RobustIssuePriceReferenceStatus::SettledPrimary;
    }
    if (raw.value == "settled_secondary") {
        return RobustIssuePriceReferenceStatus::SettledSecondary;
    }
    parse_error(raw.line, "unknown reference-price status");
}

[[nodiscard]] RobustIssuePriceSupportCapacityStatus parse_support_status(
    const RawValue& raw) {
    if (raw.value == "synthetic_candidate") {
        return RobustIssuePriceSupportCapacityStatus::SyntheticCandidate;
    }
    if (raw.value == "nonbinding_indication") {
        return RobustIssuePriceSupportCapacityStatus::NonbindingIndication;
    }
    if (raw.value == "contractually_committed") {
        return RobustIssuePriceSupportCapacityStatus::ContractuallyCommitted;
    }
    if (raw.value == "funded_or_escrowed") {
        return RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed;
    }
    if (raw.value == "settled_to_issue") {
        return RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    }
    parse_error(raw.line, "unknown issue-support evidence status");
}

[[nodiscard]] RobustIssuePriceHurdleSourceType parse_hurdle_source_type(
    const RawValue& raw) {
    if (raw.value == "same_claim_market_observation") {
        return RobustIssuePriceHurdleSourceType::SameClaimMarketObservation;
    }
    if (raw.value == "comparable_market_observation") {
        return RobustIssuePriceHurdleSourceType::ComparableMarketObservation;
    }
    if (raw.value == "model_adjusted_comparable") {
        return RobustIssuePriceHurdleSourceType::ModelAdjustedComparable;
    }
    if (raw.value == "investor_target") {
        return RobustIssuePriceHurdleSourceType::InvestorTarget;
    }
    if (raw.value == "policy_target") {
        return RobustIssuePriceHurdleSourceType::PolicyTarget;
    }
    if (raw.value == "synthetic_sensitivity") {
        return RobustIssuePriceHurdleSourceType::SyntheticSensitivity;
    }
    parse_error(raw.line, "unknown investor-hurdle source type");
}

[[nodiscard]] RobustIssuePriceHurdleReferenceRelation
parse_hurdle_reference_relation(const RawValue& raw) {
    if (raw.value == "independent") {
        return RobustIssuePriceHurdleReferenceRelation::Independent;
    }
    if (raw.value == "model_implied_from_reference_price") {
        return RobustIssuePriceHurdleReferenceRelation::
            ModelImpliedFromReferencePrice;
    }
    if (raw.value == "unresolved") {
        return RobustIssuePriceHurdleReferenceRelation::Unresolved;
    }
    parse_error(raw.line,
        "unknown hurdle relation to the reference price");
}

[[nodiscard]] bool is_fixed_key(std::string_view key) noexcept {
    return std::find(kFixedKeys.begin(), kFixedKeys.end(), key) !=
        kFixedKeys.end();
}

[[nodiscard]] bool is_hurdle_field(std::string_view field) noexcept {
    return std::find(kHurdleFields.begin(), kHurdleFields.end(), field) !=
        kHurdleFields.end();
}

[[nodiscard]] bool is_bounded_hurdle_key(std::string_view key) noexcept {
    constexpr std::string_view prefix{"hurdle_case."};
    if (!key.starts_with(prefix)) {
        return false;
    }
    const std::string_view remainder = key.substr(prefix.size());
    const std::size_t dot = remainder.find('.');
    if (dot == std::string_view::npos) {
        return false;
    }
    const std::string_view index_text = remainder.substr(0U, dot);
    const std::string_view field = remainder.substr(dot + 1U);
    if (index_text.empty() || index_text.front() == '0' ||
        !is_hurdle_field(field)) {
        return false;
    }
    std::uint64_t index{};
    const char* const begin = index_text.data();
    const char* const end = begin + index_text.size();
    const auto conversion = std::from_chars(begin, end, index);
    return conversion.ec == std::errc{} && conversion.ptr == end &&
        index >= 1U &&
        index <= kRobustIssuePriceSupportMaximumHurdleCases;
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
                    "failed while reading issue-price-support configuration");
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
        if (!is_fixed_key(key) && !is_bounded_hurdle_key(key)) {
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
            "failed while reading issue-price-support configuration");
    }
    return raw;
}

[[nodiscard]] std::string hurdle_key(
    std::size_t index, std::string_view field) {
    return "hurdle_case." + std::to_string(index + 1U) + "." +
        std::string(field);
}

[[nodiscard]] bool is_leap_year(int year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

[[nodiscard]] bool is_iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4] != '-' || value[7] != '-') {
        return false;
    }
    for (const std::size_t index :
         std::array<std::size_t, 8U>{0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U}) {
        if (value[index] < '0' || value[index] > '9') {
            return false;
        }
    }
    const int year =
        (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
        (value[2] - '0') * 10 + (value[3] - '0');
    const int month = (value[5] - '0') * 10 + (value[6] - '0');
    const int day = (value[8] - '0') * 10 + (value[9] - '0');
    if (year == 0 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    constexpr std::array<int, 12U> days_by_month{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maximum_day = days_by_month[static_cast<std::size_t>(month - 1)];
    if (month == 2 && is_leap_year(year)) {
        maximum_day = 29;
    }
    return day <= maximum_day;
}

void require_date_or_none(
    std::string_view value, std::string_view description) {
    if (value != kAbsent && !is_iso_date(value)) {
        throw std::invalid_argument(
            std::string(description) +
            " must be none or a calendar date in YYYY-MM-DD");
    }
}

void require_date(std::string_view value, std::string_view description) {
    if (!is_iso_date(value)) {
        throw std::invalid_argument(
            std::string(description) +
            " must be a calendar date in YYYY-MM-DD");
    }
}

void require_non_negative_amount(
    double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 ||
        (value == 0.0 && std::signbit(value))) {
        throw std::invalid_argument(
            std::string(description) + " must be finite and non-negative");
    }
}

void require_bounded_money(
    double value, std::string_view description) {
    require_non_negative_amount(value, description);
    if (value > kMaximumMoneyMillion) {
        throw std::invalid_argument(
            std::string(description) +
            " exceeds the 1,000,000,000-million money guardrail");
    }
}

void validate_reference_semantics(
    const RobustIssuePriceReferenceConfig& reference) {
    require_safe_identifier(reference.record_id, "reference-price id");
    require_safe_identifier(
        reference.market_claim_id, "reference-price market claim id");
    require_safe_identifier(reference.normalized_term_result_id,
        "reference-price normalized term/result id");
    require_safe_text(reference.quantity_basis,
        "reference-price quantity basis");
    require_safe_text(reference.price_basis, "reference-price price basis");
    require_safe_text(reference.currency_label,
        "reference-price currency label");
    require_safe_text(reference.monetary_basis,
        "reference-price monetary basis");
    require_safe_text(reference.execution_date,
        "reference-price execution date");
    require_safe_text(reference.settlement_date,
        "reference-price settlement date");
    require_safe_text(reference.side_rights_or_non_cash_consideration_note,
        "reference-price side-rights note");
    require_safe_text(reference.source_reference,
        "reference-price source reference");
    require_safe_text(reference.evidence_record_id,
        "reference-price evidence record id");
    require_safe_text(reference.issue_use_evidence_record_id,
        "reference-price issue-use evidence record id");

    require_bounded_money(reference.gross_issue_price_million,
        "reference gross issue price");
    require_bounded_money(reference.claim_quantity_million,
        "reference claim quantity");
    if (reference.claim_quantity_million == 0.0) {
        throw std::invalid_argument(
            "reference claim quantity must be positive");
    }
    require_bounded_money(reference.issuer_cost_million,
        "reference issuer cost");
    require_bounded_money(reference.buyer_direct_cost_million,
        "reference buyer-direct cost");
    require_date_or_none(
        reference.execution_date, "reference-price execution date");
    require_date_or_none(
        reference.settlement_date, "reference-price settlement date");

    if (reference.side_rights_or_non_cash_consideration_present ==
            (reference.side_rights_or_non_cash_consideration_note ==
                kAbsent)) {
        throw std::invalid_argument(
            "reference side-rights presence and note are inconsistent");
    }

    const bool has_execution = reference.execution_date != kAbsent;
    const bool has_settlement = reference.settlement_date != kAbsent;
    if (has_execution && has_settlement &&
        reference.settlement_date < reference.execution_date) {
        throw std::invalid_argument(
            "reference settlement date must not precede execution date");
    }

}

void validate_support_semantics(
    const RobustIssuePriceSupportCapacityConfig& support) {
    require_safe_identifier(support.support_id, "issue-support id");
    require_safe_text(support.as_of_date, "issue-support as-of date");
    require_safe_text(
        support.source_reference, "issue-support source reference");
    require_safe_text(
        support.evidence_record_id, "issue-support evidence record id");
    require_safe_text(support.source_note, "issue-support source note");
    require_date_or_none(support.as_of_date, "issue-support as-of date");
    require_bounded_money(
        support.maximum_support_million, "maximum issue support");
    require_bounded_money(
        support.settled_support_million, "settled issue support");
    if (support.settled_support_million >
        support.maximum_support_million) {
        throw std::invalid_argument(
            "settled issue support must not exceed maximum support capacity");
    }

    if (!support.support_is_non_repayable ||
        !support
             .support_receives_no_repayment_participation_security_or_recovery_rights ||
        !support.support_is_not_project_revenue ||
        !support
             .support_does_not_pay_future_pool_costs_or_cover_project_losses) {
        throw std::invalid_argument(
            "all v0.1 issue-support boundary assertions must be true");
    }

    const bool controlled_status =
        support.status ==
            RobustIssuePriceSupportCapacityStatus::ContractuallyCommitted ||
        support.status ==
            RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed ||
        support.status ==
            RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    if (controlled_status &&
        (support.as_of_date == kAbsent ||
         support.evidence_record_id == kAbsent)) {
        throw std::invalid_argument(
            "committed, funded, and settled support statuses require dated evidence");
    }
}

void validate_hurdle_semantics(
    const std::vector<RobustIssuePriceHurdleCaseConfig>& hurdles) {
    if (hurdles.empty() ||
        hurdles.size() > kRobustIssuePriceSupportMaximumHurdleCases) {
        throw std::invalid_argument(
            "issue-price hurdle-case count must be between one and 256");
    }
    std::unordered_set<std::string> ids;
    ids.reserve(hurdles.size());
    bool has_literal_zero = false;
    for (const RobustIssuePriceHurdleCaseConfig& hurdle : hurdles) {
        require_safe_identifier(hurdle.case_id, "investor-hurdle case id");
        if (!ids.emplace(hurdle.case_id).second) {
            throw std::invalid_argument(
                "investor-hurdle case ids must be unique");
        }
        if (!std::isfinite(hurdle.annual_effective_hurdle_rate) ||
            hurdle.annual_effective_hurdle_rate < 0.0 ||
            hurdle.annual_effective_hurdle_rate >
                kMaximumAnnualEffectiveHurdleRate ||
            (hurdle.annual_effective_hurdle_rate == 0.0 &&
             std::signbit(hurdle.annual_effective_hurdle_rate))) {
            throw std::invalid_argument(
                "annual effective investor hurdle must be between 0 and 10 inclusive");
        }
        has_literal_zero = has_literal_zero ||
            hurdle.annual_effective_hurdle_rate == 0.0;
        require_safe_text(hurdle.as_of_date,
            "investor-hurdle as-of date");
        require_date(hurdle.as_of_date, "investor-hurdle as-of date");
        require_safe_text(hurdle.source_reference,
            "investor-hurdle source reference");
        require_safe_text(hurdle.evidence_record_id,
            "investor-hurdle evidence record id");
        require_safe_text(hurdle.source_note,
            "investor-hurdle source note");
    }
    if (!has_literal_zero) {
        throw std::invalid_argument(
            "issue-price hurdle cases must include literal zero");
    }
}

[[nodiscard]] RobustIssuePriceSupportConfig canonicalized_and_validated(
    RobustIssuePriceSupportConfig config) {
    if (config.model_version != kRobustIssuePriceSupportModelVersion) {
        throw std::invalid_argument(
            "unsupported robust issue-price-support model version");
    }
    require_safe_text(config.scenario_label, "issue-price-support label");
    require_safe_text(
        config.source_note, "issue-price-support source note");
    validate_reference_semantics(config.reference_price);
    validate_support_semantics(config.support);
    validate_hurdle_semantics(config.hurdle_cases);

    std::sort(config.hurdle_cases.begin(), config.hurdle_cases.end(),
        [](const RobustIssuePriceHurdleCaseConfig& left,
           const RobustIssuePriceHurdleCaseConfig& right) {
            if (left.annual_effective_hurdle_rate !=
                right.annual_effective_hurdle_rate) {
                return left.annual_effective_hurdle_rate <
                    right.annual_effective_hurdle_rate;
            }
            return left.case_id < right.case_id;
        });
    validate_robust_issue_price_support_config(config);
    return config;
}

[[nodiscard]] RobustIssuePriceSupportConfig parse_raw(const RawMap& raw) {
    std::unordered_set<std::string> expected;
    expected.reserve(kFixedKeys.size());
    for (const std::string_view key : kFixedKeys) {
        expected.emplace(key);
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    const std::size_t hurdle_count =
        parse_size(required(raw, "hurdle_case.count"));
    if (hurdle_count == 0U ||
        hurdle_count > kRobustIssuePriceSupportMaximumHurdleCases) {
        throw std::invalid_argument(
            "issue-price hurdle-case count must be between one and 256");
    }
    expected.reserve(kFixedKeys.size() + hurdle_count * kHurdleFields.size());
    for (std::size_t index = 0U; index < hurdle_count; ++index) {
        for (const std::string_view field : kHurdleFields) {
            expected.emplace(hurdle_key(index, field));
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

    RobustIssuePriceSupportConfig config;
    config.model_version =
        required(raw, "issue_price_support.model_version").value;
    config.scenario_label =
        parse_text(required(raw, "issue_price_support.label"));
    config.source_note =
        parse_text(required(raw, "issue_price_support.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "issue_price_support.synthetic_inputs"));
    config.market_claim_principal_is_fully_funded_at_issue = parse_bool(
        required(raw,
            "issue_price_support.market_claim_principal_is_fully_funded_at_issue"));
    config.issue_support_and_price_fund_only_principal_and_issuer_costs =
        parse_bool(required(raw,
            "issue_price_support.issue_support_and_price_fund_only_principal_and_issuer_costs"));
    config.buyer_direct_cost_stays_outside_subscription_reserve = parse_bool(
        required(raw,
            "issue_price_support.buyer_direct_cost_stays_outside_subscription_reserve"));
    config.support_changes_no_claim_right_or_project_cash = parse_bool(
        required(raw,
            "issue_price_support.support_changes_no_claim_right_or_project_cash"));
    config.physical_probability_polytope_is_unchanged = parse_bool(
        required(raw,
            "issue_price_support.physical_probability_polytope_is_unchanged"));
    config.fair_value_or_market_price_is_estimated = parse_bool(
        required(raw,
            "issue_price_support.fair_value_or_market_price_is_estimated"));

    RobustIssuePriceReferenceConfig& reference = config.reference_price;
    reference.record_id = parse_text(required(raw, "reference_price.id"));
    reference.status = parse_reference_status(
        required(raw, "reference_price.status"));
    reference.market_claim_id =
        parse_text(required(raw, "reference_price.market_claim_id"));
    reference.normalized_term_result_id = parse_text(
        required(raw, "reference_price.normalized_term_result_id"));
    reference.secondary_price_normalized_to_full_month_zero_claim =
        parse_bool(required(raw,
            "reference_price.secondary_price_normalized_to_full_month_zero_claim"));
    reference.gross_issue_price_million = parse_double(
        required(raw, "reference_price.gross_issue_price_million"));
    reference.claim_quantity_million = parse_double(
        required(raw, "reference_price.claim_quantity_million"));
    reference.quantity_basis =
        parse_text(required(raw, "reference_price.quantity_basis"));
    reference.price_basis =
        parse_text(required(raw, "reference_price.price_basis"));
    reference.currency_label =
        parse_text(required(raw, "reference_price.currency_label"));
    reference.monetary_basis =
        parse_text(required(raw, "reference_price.monetary_basis"));
    reference.execution_date =
        parse_text(required(raw, "reference_price.execution_date"));
    reference.settlement_date =
        parse_text(required(raw, "reference_price.settlement_date"));
    reference.issuer_cost_million = parse_double(
        required(raw, "reference_price.issuer_cost_million"));
    reference.buyer_direct_cost_million = parse_double(
        required(raw, "reference_price.buyer_direct_cost_million"));
    reference.side_rights_or_non_cash_consideration_present = parse_bool(
        required(raw,
            "reference_price.side_rights_or_non_cash_consideration_present"));
    reference.side_rights_or_non_cash_consideration_note = parse_text(
        required(raw,
            "reference_price.side_rights_or_non_cash_consideration_note"));
    reference.source_reference =
        parse_text(required(raw, "reference_price.source_reference"));
    reference.evidence_record_id =
        parse_text(required(raw, "reference_price.evidence_record_id"));
    reference.buyer_cash_payment_evidenced = parse_bool(
        required(raw, "reference_price.buyer_cash_payment_evidenced"));
    reference.settlement_evidenced =
        parse_bool(required(raw, "reference_price.settlement_evidenced"));
    reference.subscription_reserve_deposit_evidenced = parse_bool(required(
        raw, "reference_price.subscription_reserve_deposit_evidenced"));
    reference.issuer_cost_payment_evidenced = parse_bool(required(
        raw, "reference_price.issuer_cost_payment_evidenced"));
    reference.issue_use_evidence_record_id = parse_text(required(
        raw, "reference_price.issue_use_evidence_record_id"));

    RobustIssuePriceSupportCapacityConfig& support = config.support;
    support.support_id = parse_text(required(raw, "support.id"));
    support.status = parse_support_status(required(raw, "support.status"));
    support.maximum_support_million =
        parse_double(required(raw, "support.maximum_support_million"));
    support.settled_support_million =
        parse_double(required(raw, "support.settled_support_million"));
    support.funding_evidenced =
        parse_bool(required(raw, "support.funding_evidenced"));
    support.settlement_evidenced =
        parse_bool(required(raw, "support.settlement_evidenced"));
    support.as_of_date = parse_text(required(raw, "support.as_of_date"));
    support.source_reference =
        parse_text(required(raw, "support.source_reference"));
    support.evidence_record_id =
        parse_text(required(raw, "support.evidence_record_id"));
    support.source_note = parse_text(required(raw, "support.source_note"));
    support.support_is_non_repayable =
        parse_bool(required(raw, "support.support_is_non_repayable"));
    support
        .support_receives_no_repayment_participation_security_or_recovery_rights =
        parse_bool(required(raw,
            "support.support_receives_no_repayment_participation_security_or_recovery_rights"));
    support.support_is_not_project_revenue = parse_bool(
        required(raw, "support.support_is_not_project_revenue"));
    support.support_does_not_pay_future_pool_costs_or_cover_project_losses =
        parse_bool(required(raw,
            "support.support_does_not_pay_future_pool_costs_or_cover_project_losses"));

    config.hurdle_cases.reserve(hurdle_count);
    for (std::size_t index = 0U; index < hurdle_count; ++index) {
        RobustIssuePriceHurdleCaseConfig hurdle;
        hurdle.case_id =
            parse_text(required(raw, hurdle_key(index, "id")));
        hurdle.annual_effective_hurdle_rate = parse_double(required(
            raw, hurdle_key(index, "annual_effective_hurdle_rate")));
        hurdle.source_type = parse_hurdle_source_type(
            required(raw, hurdle_key(index, "source_type")));
        hurdle.reference_price_relation = parse_hurdle_reference_relation(
            required(raw, hurdle_key(index, "reference_price_relation")));
        hurdle.as_of_date =
            parse_text(required(raw, hurdle_key(index, "as_of_date")));
        hurdle.source_reference = parse_text(
            required(raw, hurdle_key(index, "source_reference")));
        hurdle.evidence_record_id = parse_text(
            required(raw, hurdle_key(index, "evidence_record_id")));
        hurdle.source_note =
            parse_text(required(raw, hurdle_key(index, "source_note")));
        config.hurdle_cases.push_back(std::move(hurdle));
    }
    return canonicalized_and_validated(std::move(config));
}

} // namespace

RobustIssuePriceSupportConfig parse_robust_issue_price_support_config(
    std::istream& input) {
    return parse_raw(read_raw(input));
}

RobustIssuePriceSupportConfig load_robust_issue_price_support_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "issue-price-support configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open issue-price-support configuration file: " +
            path.string());
    }
    try {
        return parse_robust_issue_price_support_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading issue-price-support configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_robust_issue_price_support_config(
    std::ostream& output, const RobustIssuePriceSupportConfig& config) {
    const RobustIssuePriceSupportConfig canonical =
        canonicalized_and_validated(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;

    output << "issue_price_support.model_version=" << canonical.model_version
           << '\n';
    output << "issue_price_support.label=" << canonical.scenario_label
           << '\n';
    output << "issue_price_support.source_note=" << canonical.source_note
           << '\n';
    output << "issue_price_support.synthetic_inputs="
           << canonical.synthetic_inputs << '\n';
    output << "issue_price_support.market_claim_principal_is_fully_funded_at_issue="
           << canonical.market_claim_principal_is_fully_funded_at_issue
           << '\n';
    output << "issue_price_support.issue_support_and_price_fund_only_principal_and_issuer_costs="
           << canonical
                  .issue_support_and_price_fund_only_principal_and_issuer_costs
           << '\n';
    output << "issue_price_support.buyer_direct_cost_stays_outside_subscription_reserve="
           << canonical.buyer_direct_cost_stays_outside_subscription_reserve
           << '\n';
    output << "issue_price_support.support_changes_no_claim_right_or_project_cash="
           << canonical.support_changes_no_claim_right_or_project_cash
           << '\n';
    output << "issue_price_support.physical_probability_polytope_is_unchanged="
           << canonical.physical_probability_polytope_is_unchanged << '\n';
    output << "issue_price_support.fair_value_or_market_price_is_estimated="
           << canonical.fair_value_or_market_price_is_estimated << '\n';

    const RobustIssuePriceReferenceConfig& reference =
        canonical.reference_price;
    output << "reference_price.id=" << reference.record_id << '\n';
    output << "reference_price.status=" << to_string(reference.status)
           << '\n';
    output << "reference_price.market_claim_id=" << reference.market_claim_id
           << '\n';
    output << "reference_price.normalized_term_result_id="
           << reference.normalized_term_result_id << '\n';
    output << "reference_price.secondary_price_normalized_to_full_month_zero_claim="
           << reference.secondary_price_normalized_to_full_month_zero_claim
           << '\n';
    output << "reference_price.gross_issue_price_million="
           << reference.gross_issue_price_million << '\n';
    output << "reference_price.claim_quantity_million="
           << reference.claim_quantity_million << '\n';
    output << "reference_price.quantity_basis=" << reference.quantity_basis
           << '\n';
    output << "reference_price.price_basis=" << reference.price_basis << '\n';
    output << "reference_price.currency_label=" << reference.currency_label
           << '\n';
    output << "reference_price.monetary_basis=" << reference.monetary_basis
           << '\n';
    output << "reference_price.execution_date=" << reference.execution_date
           << '\n';
    output << "reference_price.settlement_date=" << reference.settlement_date
           << '\n';
    output << "reference_price.issuer_cost_million="
           << reference.issuer_cost_million << '\n';
    output << "reference_price.buyer_direct_cost_million="
           << reference.buyer_direct_cost_million << '\n';
    output << "reference_price.side_rights_or_non_cash_consideration_present="
           << reference.side_rights_or_non_cash_consideration_present << '\n';
    output << "reference_price.side_rights_or_non_cash_consideration_note="
           << reference.side_rights_or_non_cash_consideration_note << '\n';
    output << "reference_price.source_reference="
           << reference.source_reference << '\n';
    output << "reference_price.evidence_record_id="
           << reference.evidence_record_id << '\n';
    output << "reference_price.buyer_cash_payment_evidenced="
           << reference.buyer_cash_payment_evidenced << '\n';
    output << "reference_price.settlement_evidenced="
           << reference.settlement_evidenced << '\n';
    output << "reference_price.subscription_reserve_deposit_evidenced="
           << reference.subscription_reserve_deposit_evidenced << '\n';
    output << "reference_price.issuer_cost_payment_evidenced="
           << reference.issuer_cost_payment_evidenced << '\n';
    output << "reference_price.issue_use_evidence_record_id="
           << reference.issue_use_evidence_record_id << '\n';

    const RobustIssuePriceSupportCapacityConfig& support = canonical.support;
    output << "support.id=" << support.support_id << '\n';
    output << "support.status=" << to_string(support.status) << '\n';
    output << "support.maximum_support_million="
           << support.maximum_support_million << '\n';
    output << "support.settled_support_million="
           << support.settled_support_million << '\n';
    output << "support.funding_evidenced=" << support.funding_evidenced
           << '\n';
    output << "support.settlement_evidenced="
           << support.settlement_evidenced << '\n';
    output << "support.as_of_date=" << support.as_of_date << '\n';
    output << "support.source_reference=" << support.source_reference << '\n';
    output << "support.evidence_record_id=" << support.evidence_record_id
           << '\n';
    output << "support.source_note=" << support.source_note << '\n';
    output << "support.support_is_non_repayable="
           << support.support_is_non_repayable << '\n';
    output << "support.support_receives_no_repayment_participation_security_or_recovery_rights="
           << support
                  .support_receives_no_repayment_participation_security_or_recovery_rights
           << '\n';
    output << "support.support_is_not_project_revenue="
           << support.support_is_not_project_revenue << '\n';
    output << "support.support_does_not_pay_future_pool_costs_or_cover_project_losses="
           << support
                  .support_does_not_pay_future_pool_costs_or_cover_project_losses
           << '\n';

    output << "hurdle_case.count=" << canonical.hurdle_cases.size() << '\n';
    for (std::size_t index = 0U; index < canonical.hurdle_cases.size();
         ++index) {
        const RobustIssuePriceHurdleCaseConfig& hurdle =
            canonical.hurdle_cases[index];
        output << hurdle_key(index, "id") << '=' << hurdle.case_id << '\n';
        output << hurdle_key(index, "annual_effective_hurdle_rate") << '='
               << hurdle.annual_effective_hurdle_rate << '\n';
        output << hurdle_key(index, "source_type") << '='
               << to_string(hurdle.source_type) << '\n';
        output << hurdle_key(index, "reference_price_relation") << '='
               << to_string(hurdle.reference_price_relation) << '\n';
        output << hurdle_key(index, "as_of_date") << '=' << hurdle.as_of_date
               << '\n';
        output << hurdle_key(index, "source_reference") << '='
               << hurdle.source_reference << '\n';
        output << hurdle_key(index, "evidence_record_id") << '='
               << hurdle.evidence_record_id << '\n';
        output << hurdle_key(index, "source_note") << '='
               << hurdle.source_note << '\n';
    }

    if (!output) {
        throw std::runtime_error(
            "failed while writing normalized issue-price-support configuration");
    }
}

} // namespace naturalehia::cellular_finance
