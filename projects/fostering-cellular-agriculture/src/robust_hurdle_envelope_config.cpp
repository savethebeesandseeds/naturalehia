// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_hurdle_envelope_config.hpp>

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
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};

constexpr std::array<std::string_view, 24U> kFixedKeys{
    "hurdle_envelope.model_version",
    "hurdle_envelope.analysis_id",
    "hurdle_envelope.as_of_date",
    "hurdle_envelope.source_note",
    "hurdle_envelope.synthetic_inputs",
    "hurdle_envelope.universe_manifest_id",
    "hurdle_envelope.inclusion_rule_id",
    "hurdle_envelope.deduplication_manifest_id",
    "hurdle_envelope.source_clustering_rule_id",
    "hurdle_envelope.observation_universe_is_frozen",
    "hurdle_envelope.inclusion_rule_is_predeclared",
    "hurdle_envelope.deduplication_rule_is_predeclared",
    "hurdle_envelope.all_in_scope_economic_observation_clusters_are_included",
    "hurdle_envelope.same_dealer_same_window_quotes_are_clustered",
    "hurdle_envelope.target_market_claim_id",
    "hurdle_envelope.target_normalized_term_result_id",
    "hurdle_envelope.target_currency_label",
    "hurdle_envelope.target_monetary_basis",
    "hurdle_envelope.target_claim_quantity_million",
    "hurdle_envelope.annual_effective_domain_lower",
    "hurdle_envelope.annual_effective_domain_upper",
    "hurdle_envelope.maximum_contaminated_clusters",
    "hurdle_envelope.minimum_consensus_cluster_coverage",
    "observation.count",
};

constexpr std::array<std::string_view, 75U> kObservationFields{
    "id",
    "economic_observation_cluster_id",
    "status",
    "transaction_type",
    "observation_date",
    "execution_date",
    "settlement_date",
    "source_reference",
    "evidence_record_id",
    "settlement_evidence_record_id",
    "orderly_arms_length_evidence_record_id",
    "transaction_market_anchor_id",
    "transaction_execution_is_evidenced",
    "buyer_cash_payment_is_evidenced",
    "transaction_settlement_is_evidenced",
    "orderly_transaction_is_evidenced",
    "arms_length_transaction_is_evidenced",
    "forced_or_distressed_transaction",
    "related_party_transaction",
    "settled_claim_quantity_million",
    "observed_market_claim_id",
    "normalized_term_result_id",
    "claim_relation",
    "currency_label",
    "monetary_basis",
    "return_basis",
    "annual_effective_hurdle_lower",
    "annual_effective_hurdle_upper",
    "return_normalization_result_id",
    "observed_gross_buyer_price_million",
    "buyer_direct_cost_million",
    "observed_gross_buyer_price_is_evidenced",
    "buyer_direct_cost_is_evidenced",
    "buyer_direct_cost_evidence_record_id",
    "quoted_bid_gross_buyer_price_million",
    "quoted_ask_gross_buyer_price_million",
    "quoted_bid_buyer_direct_cost_million",
    "quoted_ask_buyer_direct_cost_million",
    "quoted_bid_claim_quantity_million",
    "quoted_ask_claim_quantity_million",
    "quoted_bid_timestamp_utc",
    "quoted_ask_timestamp_utc",
    "quoted_bid_currency_label",
    "quoted_ask_currency_label",
    "quoted_bid_and_ask_are_executable",
    "quoted_bid_and_ask_are_evidenced",
    "quote_evidence_record_id",
    "quote_valid_until_utc",
    "discounted_expected_cash_is_strictly_decreasing_over_rate_interval",
    "quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper",
    "settled_price_preimage_is_single_connected_interval",
    "expected_cash_operator",
    "full_dated_scenario_cash_input_sha256",
    "probability_input_sha256",
    "expected_cash_calculation_run_sha256",
    "expected_cash_reconstruction_identity_is_evidenced",
    "maximum_expected_cash_reconstruction_residual_million",
    "physical_expected_full_claim_cash_includes_loss_and_timing",
    "normalized_to_full_claim_at_observation_date",
    "fixed_probability_to_robust_bridge_is_present",
    "fixed_probability_to_robust_bridge_lower_log_delta",
    "fixed_probability_to_robust_bridge_upper_log_delta",
    "fixed_probability_to_robust_bridge_method_id",
    "fixed_probability_to_robust_bridge_evidence_record_id",
    "target_reference_price_relation",
    "side_rights_or_non_cash_consideration_present",
    "side_rights_or_non_cash_consideration_note",
    "adjustment_set_basis",
    "jointly_feasible_total_lower_log_gross_return_delta",
    "jointly_feasible_total_upper_log_gross_return_delta",
    "joint_adjustment_set_method_id",
    "joint_adjustment_set_evidence_record_id",
    "expected_loss_recovery_and_timing_are_excluded_from_adjustments",
    "source_note",
    "adjustment.count",
};

constexpr std::array<std::string_view,
    kRobustHurdleEnvelopeComparabilityAxisCount>
    kComparabilityFields{
        "contractual_cashflow_rights",
        "seniority_and_residual_tail_risk_premium_after_expected_cash",
        "systematic_covariance_concentration_and_residual_model_risk_premium",
        "contractual_term_and_cashflow_timing",
        "currency_and_monetary_basis",
        "liquidity_and_transferability",
        "transaction_size",
        "observation_date_and_market_regime",
    };

constexpr std::array<std::string_view, 8U> kAdjustmentFields{
    "id",
    "axis",
    "lower_log_gross_return_delta",
    "upper_log_gross_return_delta",
    "method_id",
    "source_reference",
    "evidence_record_id",
    "source_note",
};

constexpr std::size_t kMaximumRawEntries = kFixedKeys.size() +
    kRobustHurdleEnvelopeMaximumObservations *
        (kObservationFields.size() + kComparabilityFields.size() +
            kRobustHurdleEnvelopeMaximumAdjustmentsPerObservation *
                kAdjustmentFields.size());

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

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!is_safe_identifier(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a safe bounded identifier");
    }
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
        "robust-hurdle-envelope configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "robust-hurdle-envelope configuration is missing required key: " +
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

[[nodiscard]] bool is_fixed_key(std::string_view key) noexcept {
    return std::find(kFixedKeys.begin(), kFixedKeys.end(), key) !=
        kFixedKeys.end();
}

[[nodiscard]] bool is_observation_field(std::string_view field) noexcept {
    return std::find(kObservationFields.begin(), kObservationFields.end(),
               field) != kObservationFields.end();
}

[[nodiscard]] bool is_comparability_field(
    std::string_view field) noexcept {
    return std::find(kComparabilityFields.begin(),
               kComparabilityFields.end(), field) !=
        kComparabilityFields.end();
}

[[nodiscard]] bool is_adjustment_field(std::string_view field) noexcept {
    return std::find(kAdjustmentFields.begin(), kAdjustmentFields.end(),
               field) != kAdjustmentFields.end();
}

[[nodiscard]] bool parse_bounded_one_based_index(std::string_view text,
    std::size_t maximum, std::size_t& parsed) noexcept {
    if (text.empty() || text.front() == '0') {
        return false;
    }
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto conversion = std::from_chars(begin, end, value);
    if (conversion.ec != std::errc{} || conversion.ptr != end ||
        value < 1U || value > maximum) {
        return false;
    }
    parsed = static_cast<std::size_t>(value);
    return true;
}

[[nodiscard]] bool is_bounded_observation_key(
    std::string_view key) noexcept {
    constexpr std::string_view prefix{"observation."};
    if (!key.starts_with(prefix)) {
        return false;
    }
    std::string_view remainder = key.substr(prefix.size());
    const std::size_t first_dot = remainder.find('.');
    if (first_dot == std::string_view::npos) {
        return false;
    }
    std::size_t observation_index{};
    if (!parse_bounded_one_based_index(remainder.substr(0U, first_dot),
            kRobustHurdleEnvelopeMaximumObservations,
            observation_index)) {
        return false;
    }
    (void)observation_index;
    remainder.remove_prefix(first_dot + 1U);
    if (is_observation_field(remainder)) {
        return true;
    }
    constexpr std::string_view comparability_prefix{"comparability."};
    if (remainder.starts_with(comparability_prefix)) {
        return is_comparability_field(
            remainder.substr(comparability_prefix.size()));
    }
    constexpr std::string_view adjustment_prefix{"adjustment."};
    if (!remainder.starts_with(adjustment_prefix)) {
        return false;
    }
    remainder.remove_prefix(adjustment_prefix.size());
    const std::size_t adjustment_dot = remainder.find('.');
    if (adjustment_dot == std::string_view::npos) {
        return false;
    }
    std::size_t adjustment_index{};
    return parse_bounded_one_based_index(
               remainder.substr(0U, adjustment_dot),
               kRobustHurdleEnvelopeMaximumAdjustmentsPerObservation,
               adjustment_index) &&
        is_adjustment_field(remainder.substr(adjustment_dot + 1U));
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
                    "failed while reading robust-hurdle-envelope configuration");
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
        if (!is_fixed_key(key) && !is_bounded_observation_key(key)) {
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
            "failed while reading robust-hurdle-envelope configuration");
    }
    return raw;
}

[[nodiscard]] std::string observation_key(
    std::size_t index, std::string_view field) {
    return "observation." + std::to_string(index + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string comparability_key(
    std::size_t index, std::string_view field) {
    return observation_key(index, "comparability." + std::string(field));
}

[[nodiscard]] std::string adjustment_key(std::size_t observation_index,
    std::size_t adjustment_index, std::string_view field) {
    return observation_key(observation_index,
        "adjustment." + std::to_string(adjustment_index + 1U) + "." +
            std::string(field));
}

[[nodiscard]] RobustHurdleObservationStatus parse_observation_status(
    const RawValue& raw) {
    if (raw.value == "settled_orderly_arms_length_transaction") {
        return RobustHurdleObservationStatus::
            SettledOrderlyArmsLengthTransaction;
    }
    if (raw.value == "executable_two_sided_quote") {
        return RobustHurdleObservationStatus::ExecutableTwoSidedQuote;
    }
    if (raw.value == "nonbinding_indication") {
        return RobustHurdleObservationStatus::NonbindingIndication;
    }
    if (raw.value == "model_mark") {
        return RobustHurdleObservationStatus::ModelMark;
    }
    parse_error(raw.line, "unknown observation status");
}

[[nodiscard]] RobustHurdleObservationTransactionType
parse_transaction_type(const RawValue& raw) {
    if (raw.value == "primary_issuance") {
        return RobustHurdleObservationTransactionType::PrimaryIssuance;
    }
    if (raw.value == "secondary_trade") {
        return RobustHurdleObservationTransactionType::SecondaryTrade;
    }
    if (raw.value == "two_sided_market_quote") {
        return RobustHurdleObservationTransactionType::TwoSidedMarketQuote;
    }
    parse_error(raw.line, "unknown observation transaction type");
}

[[nodiscard]] RobustHurdleObservationReturnBasis parse_return_basis(
    const RawValue& raw) {
    if (raw.value ==
        "annual_effective_all_in_buyer_cash_discount_rate_on_physical_expected_full_claim_cash_flows") {
        return RobustHurdleObservationReturnBasis::
            AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows;
    }
    if (raw.value == "gross_price_ex_buyer_cost_discount_rate") {
        return RobustHurdleObservationReturnBasis::
            GrossPriceExBuyerCostDiscountRate;
    }
    if (raw.value == "promised_yield_to_maturity") {
        return RobustHurdleObservationReturnBasis::PromisedYieldToMaturity;
    }
    if (raw.value == "coupon_rate") {
        return RobustHurdleObservationReturnBasis::CouponRate;
    }
    if (raw.value == "internal_rate_of_return") {
        return RobustHurdleObservationReturnBasis::InternalRateOfReturn;
    }
    if (raw.value == "target_price_implied_discount_rate") {
        return RobustHurdleObservationReturnBasis::
            TargetPriceImpliedDiscountRate;
    }
    if (raw.value == "other_or_unresolved") {
        return RobustHurdleObservationReturnBasis::OtherOrUnresolved;
    }
    parse_error(raw.line, "unknown observation return basis");
}

[[nodiscard]] RobustHurdleObservationClaimRelation parse_claim_relation(
    const RawValue& raw) {
    if (raw.value == "same_target_claim") {
        return RobustHurdleObservationClaimRelation::SameTargetClaim;
    }
    if (raw.value == "comparable_claim") {
        return RobustHurdleObservationClaimRelation::ComparableClaim;
    }
    parse_error(raw.line, "unknown observation claim relation");
}

[[nodiscard]] RobustHurdleExpectedCashOperator parse_expected_cash_operator(
    const RawValue& raw) {
    if (raw.value == "robust_minimum_over_declared_probability_set") {
        return RobustHurdleExpectedCashOperator::
            RobustMinimumOverDeclaredProbabilitySet;
    }
    if (raw.value == "fixed_declared_probability_vector") {
        return RobustHurdleExpectedCashOperator::
            FixedDeclaredProbabilityVector;
    }
    parse_error(raw.line, "unknown expected-cash operator");
}

[[nodiscard]] RobustHurdleAdjustmentSetBasis parse_adjustment_set_basis(
    const RawValue& raw) {
    if (raw.value == "jointly_feasible_total_interval") {
        return RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval;
    }
    if (raw.value == "componentwise_box_outer_hull") {
        return RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull;
    }
    parse_error(raw.line, "unknown adjustment-set basis");
}

[[nodiscard]] RobustHurdleComparabilityTreatment parse_treatment(
    const RawValue& raw) {
    if (raw.value == "matched") {
        return RobustHurdleComparabilityTreatment::Matched;
    }
    if (raw.value == "bounded_log_gross_return_adjustment") {
        return RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;
    }
    if (raw.value == "unresolved") {
        return RobustHurdleComparabilityTreatment::Unresolved;
    }
    parse_error(raw.line, "unknown comparability treatment");
}

[[nodiscard]] RobustHurdleComparabilityAxis parse_axis(
    const RawValue& raw) {
    for (std::size_t index = 0U; index < kComparabilityFields.size();
         ++index) {
        if (raw.value == kComparabilityFields[index]) {
            return static_cast<RobustHurdleComparabilityAxis>(index);
        }
    }
    parse_error(raw.line, "unknown comparability axis");
}

[[nodiscard]] RobustIssuePriceHurdleReferenceRelation
parse_reference_relation(const RawValue& raw) {
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
        "unknown hurdle relation to the target reference price");
}

[[nodiscard]] std::string_view observation_status_text(
    RobustHurdleObservationStatus value) {
    switch (value) {
    case RobustHurdleObservationStatus::
        SettledOrderlyArmsLengthTransaction:
        return "settled_orderly_arms_length_transaction";
    case RobustHurdleObservationStatus::ExecutableTwoSidedQuote:
        return "executable_two_sided_quote";
    case RobustHurdleObservationStatus::NonbindingIndication:
        return "nonbinding_indication";
    case RobustHurdleObservationStatus::ModelMark:
        return "model_mark";
    }
    throw std::invalid_argument("invalid observation status enum value");
}

[[nodiscard]] std::string_view transaction_type_text(
    RobustHurdleObservationTransactionType value) {
    switch (value) {
    case RobustHurdleObservationTransactionType::PrimaryIssuance:
        return "primary_issuance";
    case RobustHurdleObservationTransactionType::SecondaryTrade:
        return "secondary_trade";
    case RobustHurdleObservationTransactionType::TwoSidedMarketQuote:
        return "two_sided_market_quote";
    }
    throw std::invalid_argument("invalid transaction-type enum value");
}

[[nodiscard]] std::string_view return_basis_text(
    RobustHurdleObservationReturnBasis value) {
    switch (value) {
    case RobustHurdleObservationReturnBasis::
        AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows:
        return "annual_effective_all_in_buyer_cash_discount_rate_on_physical_expected_full_claim_cash_flows";
    case RobustHurdleObservationReturnBasis::
        GrossPriceExBuyerCostDiscountRate:
        return "gross_price_ex_buyer_cost_discount_rate";
    case RobustHurdleObservationReturnBasis::PromisedYieldToMaturity:
        return "promised_yield_to_maturity";
    case RobustHurdleObservationReturnBasis::CouponRate:
        return "coupon_rate";
    case RobustHurdleObservationReturnBasis::InternalRateOfReturn:
        return "internal_rate_of_return";
    case RobustHurdleObservationReturnBasis::TargetPriceImpliedDiscountRate:
        return "target_price_implied_discount_rate";
    case RobustHurdleObservationReturnBasis::OtherOrUnresolved:
        return "other_or_unresolved";
    }
    throw std::invalid_argument("invalid return-basis enum value");
}

[[nodiscard]] std::string_view claim_relation_text(
    RobustHurdleObservationClaimRelation value) {
    switch (value) {
    case RobustHurdleObservationClaimRelation::SameTargetClaim:
        return "same_target_claim";
    case RobustHurdleObservationClaimRelation::ComparableClaim:
        return "comparable_claim";
    }
    throw std::invalid_argument("invalid claim-relation enum value");
}

[[nodiscard]] std::string_view expected_cash_operator_text(
    RobustHurdleExpectedCashOperator value) {
    switch (value) {
    case RobustHurdleExpectedCashOperator::
        RobustMinimumOverDeclaredProbabilitySet:
        return "robust_minimum_over_declared_probability_set";
    case RobustHurdleExpectedCashOperator::FixedDeclaredProbabilityVector:
        return "fixed_declared_probability_vector";
    }
    throw std::invalid_argument("invalid expected-cash operator enum value");
}

[[nodiscard]] std::string_view adjustment_set_basis_text(
    RobustHurdleAdjustmentSetBasis value) {
    switch (value) {
    case RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval:
        return "jointly_feasible_total_interval";
    case RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull:
        return "componentwise_box_outer_hull";
    }
    throw std::invalid_argument("invalid adjustment-set basis enum value");
}

[[nodiscard]] std::string_view treatment_text(
    RobustHurdleComparabilityTreatment value) {
    switch (value) {
    case RobustHurdleComparabilityTreatment::Matched:
        return "matched";
    case RobustHurdleComparabilityTreatment::
        BoundedLogGrossReturnAdjustment:
        return "bounded_log_gross_return_adjustment";
    case RobustHurdleComparabilityTreatment::Unresolved:
        return "unresolved";
    }
    throw std::invalid_argument("invalid comparability-treatment enum value");
}

[[nodiscard]] std::string_view reference_relation_text(
    RobustIssuePriceHurdleReferenceRelation value) {
    switch (value) {
    case RobustIssuePriceHurdleReferenceRelation::Independent:
        return "independent";
    case RobustIssuePriceHurdleReferenceRelation::
        ModelImpliedFromReferencePrice:
        return "model_implied_from_reference_price";
    case RobustIssuePriceHurdleReferenceRelation::Unresolved:
        return "unresolved";
    }
    throw std::invalid_argument("invalid reference-relation enum value");
}

void require_finite(double value, std::string_view description) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be finite");
    }
}

void validate_safe_structure(const RobustHurdleEnvelopeConfig& config) {
    if (config.model_version != kRobustHurdleEnvelopeModelVersion) {
        throw std::invalid_argument(
            "unsupported robust hurdle-envelope model version");
    }
    if (config.observations.size() >
        kRobustHurdleEnvelopeMaximumObservations) {
        throw std::invalid_argument(
            "robust hurdle-envelope observation count exceeds 128");
    }
    require_safe_identifier(config.analysis_id, "analysis id");
    require_safe_text(config.as_of_date, "analysis as-of date");
    require_safe_text(config.source_note, "envelope source note");
    require_safe_identifier(
        config.universe_manifest_id, "universe manifest id");
    require_safe_identifier(config.inclusion_rule_id, "inclusion rule id");
    require_safe_identifier(
        config.deduplication_manifest_id, "deduplication manifest id");
    require_safe_identifier(
        config.source_clustering_rule_id, "source clustering rule id");
    require_safe_identifier(
        config.target_market_claim_id, "target market claim id");
    require_safe_identifier(config.target_normalized_term_result_id,
        "target normalized term/result id");
    require_safe_identifier(
        config.target_currency_label, "target currency label");
    require_safe_text(
        config.target_monetary_basis, "target monetary basis");
    require_finite(
        config.target_claim_quantity_million, "target claim quantity");
    require_finite(config.annual_effective_domain_lower,
        "annual-effective domain lower bound");
    require_finite(config.annual_effective_domain_upper,
        "annual-effective domain upper bound");

    std::unordered_set<std::string> record_ids;
    record_ids.reserve(config.observations.size());
    for (const RobustHurdleMarketObservationConfig& observation :
        config.observations) {
        require_safe_identifier(observation.record_id, "observation id");
        if (!record_ids.emplace(observation.record_id).second) {
            throw std::invalid_argument("observation ids must be unique");
        }
        require_safe_identifier(observation.economic_observation_cluster_id,
            "economic-observation cluster id");
        (void)observation_status_text(observation.status);
        (void)transaction_type_text(observation.transaction_type);
        require_safe_text(observation.observation_date,
            "observation date");
        require_safe_text(observation.execution_date, "execution date");
        require_safe_text(observation.settlement_date, "settlement date");
        require_safe_text(
            observation.source_reference, "observation source reference");
        require_safe_identifier(
            observation.evidence_record_id, "observation evidence record id");
        require_safe_identifier(observation.settlement_evidence_record_id,
            "settlement evidence record id");
        require_safe_identifier(
            observation.orderly_arms_length_evidence_record_id,
            "orderly arm's-length evidence record id");
        require_safe_identifier(observation.transaction_market_anchor_id,
            "transaction market anchor id");
        require_finite(observation.settled_claim_quantity_million,
            "settled claim quantity");
        require_safe_identifier(observation.observed_market_claim_id,
            "observed market claim id");
        require_safe_identifier(observation.normalized_term_result_id,
            "observation normalized term/result id");
        (void)claim_relation_text(observation.claim_relation);
        require_safe_identifier(
            observation.currency_label, "observation currency label");
        require_safe_text(
            observation.monetary_basis, "observation monetary basis");
        (void)return_basis_text(observation.return_basis);
        require_finite(observation.annual_effective_hurdle_lower,
            "observation hurdle lower bound");
        require_finite(observation.annual_effective_hurdle_upper,
            "observation hurdle upper bound");
        require_safe_identifier(observation.return_normalization_result_id,
            "return normalization result id");
        require_finite(observation.observed_gross_buyer_price_million,
            "observed gross buyer price");
        require_finite(observation.buyer_direct_cost_million,
            "buyer-direct cost");
        require_safe_identifier(observation.buyer_direct_cost_evidence_record_id,
            "buyer-direct-cost evidence record id");
        require_finite(observation.quoted_bid_gross_buyer_price_million,
            "quoted bid gross buyer price");
        require_finite(observation.quoted_ask_gross_buyer_price_million,
            "quoted ask gross buyer price");
        require_finite(observation.quoted_bid_buyer_direct_cost_million,
            "quoted bid buyer-direct cost");
        require_finite(observation.quoted_ask_buyer_direct_cost_million,
            "quoted ask buyer-direct cost");
        require_finite(observation.quoted_bid_claim_quantity_million,
            "quoted bid claim quantity");
        require_finite(observation.quoted_ask_claim_quantity_million,
            "quoted ask claim quantity");
        require_safe_text(
            observation.quoted_bid_timestamp_utc, "quoted bid timestamp");
        require_safe_text(
            observation.quoted_ask_timestamp_utc, "quoted ask timestamp");
        require_safe_identifier(observation.quoted_bid_currency_label,
            "quoted bid currency label");
        require_safe_identifier(observation.quoted_ask_currency_label,
            "quoted ask currency label");
        require_safe_identifier(observation.quote_evidence_record_id,
            "quote evidence record id");
        require_safe_text(
            observation.quote_valid_until_utc, "quote valid-until timestamp");
        (void)expected_cash_operator_text(observation.expected_cash_operator);
        require_safe_identifier(
            observation.full_dated_scenario_cash_input_sha256,
            "full dated scenario-cash input hash");
        require_safe_identifier(observation.probability_input_sha256,
            "probability input hash");
        require_safe_identifier(
            observation.expected_cash_calculation_run_sha256,
            "expected-cash calculation-run hash");
        require_finite(
            observation.maximum_expected_cash_reconstruction_residual_million,
            "maximum expected-cash reconstruction residual");
        require_finite(
            observation.fixed_probability_to_robust_bridge_lower_log_delta,
            "fixed-probability bridge lower log delta");
        require_finite(
            observation.fixed_probability_to_robust_bridge_upper_log_delta,
            "fixed-probability bridge upper log delta");
        require_safe_identifier(
            observation.fixed_probability_to_robust_bridge_method_id,
            "fixed-probability bridge method id");
        require_safe_identifier(
            observation.fixed_probability_to_robust_bridge_evidence_record_id,
            "fixed-probability bridge evidence record id");
        (void)reference_relation_text(
            observation.target_reference_price_relation);
        require_safe_text(observation.side_rights_or_non_cash_consideration_note,
            "side-rights/non-cash consideration note");
        for (const RobustHurdleComparabilityTreatment treatment :
            observation.comparability_treatments) {
            (void)treatment_text(treatment);
        }
        if (observation.adjustments.size() >
            kRobustHurdleEnvelopeMaximumAdjustmentsPerObservation) {
            throw std::invalid_argument(
                "an observation cannot contain more than eight adjustments");
        }
        std::unordered_set<std::string> adjustment_ids;
        adjustment_ids.reserve(observation.adjustments.size());
        for (const RobustHurdleLogGrossReturnAdjustmentConfig& adjustment :
            observation.adjustments) {
            require_safe_identifier(
                adjustment.adjustment_id, "adjustment id");
            if (!adjustment_ids.emplace(adjustment.adjustment_id).second) {
                throw std::invalid_argument(
                    "adjustment ids must be unique within an observation");
            }
            const std::size_t axis_index = robust_hurdle_axis_index(
                adjustment.axis);
            if (axis_index >= kComparabilityFields.size()) {
                throw std::invalid_argument(
                    "invalid comparability-axis enum value");
            }
            require_finite(adjustment.lower_log_gross_return_delta,
                "adjustment lower log-gross-return delta");
            require_finite(adjustment.upper_log_gross_return_delta,
                "adjustment upper log-gross-return delta");
            require_safe_identifier(adjustment.method_id,
                "adjustment method id");
            require_safe_text(adjustment.source_reference,
                "adjustment source reference");
            require_safe_identifier(adjustment.evidence_record_id,
                "adjustment evidence record id");
            require_safe_text(
                adjustment.source_note, "adjustment source note");
        }
        (void)adjustment_set_basis_text(observation.adjustment_set_basis);
        require_finite(
            observation.jointly_feasible_total_lower_log_gross_return_delta,
            "jointly feasible total lower log-gross-return delta");
        require_finite(
            observation.jointly_feasible_total_upper_log_gross_return_delta,
            "jointly feasible total upper log-gross-return delta");
        require_safe_identifier(observation.joint_adjustment_set_method_id,
            "joint adjustment-set method id");
        require_safe_identifier(
            observation.joint_adjustment_set_evidence_record_id,
            "joint adjustment-set evidence record id");
        require_safe_text(observation.source_note, "observation source note");
    }
}

[[nodiscard]] RobustHurdleEnvelopeConfig canonicalized_and_validated(
    RobustHurdleEnvelopeConfig config) {
    validate_safe_structure(config);
    for (RobustHurdleMarketObservationConfig& observation :
        config.observations) {
        std::sort(observation.adjustments.begin(),
            observation.adjustments.end(),
            [](const RobustHurdleLogGrossReturnAdjustmentConfig& left,
               const RobustHurdleLogGrossReturnAdjustmentConfig& right) {
                return left.adjustment_id < right.adjustment_id;
            });
    }
    std::sort(config.observations.begin(), config.observations.end(),
        [](const RobustHurdleMarketObservationConfig& left,
           const RobustHurdleMarketObservationConfig& right) {
            return left.record_id < right.record_id;
        });
    validate_robust_hurdle_envelope_config(config);
    return config;
}

[[nodiscard]] RobustHurdleEnvelopeConfig parse_raw(const RawMap& raw) {
    std::unordered_set<std::string> expected;
    expected.reserve(kFixedKeys.size());
    for (const std::string_view key : kFixedKeys) {
        expected.emplace(key);
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }
    const std::size_t observation_count =
        parse_size(required(raw, "observation.count"));
    if (observation_count > kRobustHurdleEnvelopeMaximumObservations) {
        throw std::invalid_argument(
            "robust hurdle-envelope observation count exceeds 128");
    }
    std::vector<std::size_t> adjustment_counts;
    adjustment_counts.reserve(observation_count);
    for (std::size_t index = 0U; index < observation_count; ++index) {
        for (const std::string_view field : kObservationFields) {
            expected.emplace(observation_key(index, field));
        }
        for (const std::string_view field : kComparabilityFields) {
            expected.emplace(comparability_key(index, field));
        }
        const std::size_t adjustment_count = parse_size(required(raw,
            observation_key(index, "adjustment.count")));
        if (adjustment_count >
            kRobustHurdleEnvelopeMaximumAdjustmentsPerObservation) {
            throw std::invalid_argument(
                "an observation cannot contain more than eight adjustments");
        }
        adjustment_counts.push_back(adjustment_count);
        for (std::size_t adjustment_index = 0U;
             adjustment_index < adjustment_count; ++adjustment_index) {
            for (const std::string_view field : kAdjustmentFields) {
                expected.emplace(
                    adjustment_key(index, adjustment_index, field));
            }
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

    RobustHurdleEnvelopeConfig config;
    config.model_version =
        required(raw, "hurdle_envelope.model_version").value;
    config.analysis_id =
        parse_text(required(raw, "hurdle_envelope.analysis_id"));
    config.as_of_date =
        parse_text(required(raw, "hurdle_envelope.as_of_date"));
    config.source_note =
        parse_text(required(raw, "hurdle_envelope.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "hurdle_envelope.synthetic_inputs"));
    config.universe_manifest_id = parse_text(
        required(raw, "hurdle_envelope.universe_manifest_id"));
    config.inclusion_rule_id =
        parse_text(required(raw, "hurdle_envelope.inclusion_rule_id"));
    config.deduplication_manifest_id = parse_text(
        required(raw, "hurdle_envelope.deduplication_manifest_id"));
    config.source_clustering_rule_id = parse_text(
        required(raw, "hurdle_envelope.source_clustering_rule_id"));
    config.observation_universe_is_frozen = parse_bool(
        required(raw, "hurdle_envelope.observation_universe_is_frozen"));
    config.inclusion_rule_is_predeclared = parse_bool(
        required(raw, "hurdle_envelope.inclusion_rule_is_predeclared"));
    config.deduplication_rule_is_predeclared = parse_bool(
        required(raw, "hurdle_envelope.deduplication_rule_is_predeclared"));
    config.all_in_scope_economic_observation_clusters_are_included =
        parse_bool(required(raw,
            "hurdle_envelope.all_in_scope_economic_observation_clusters_are_included"));
    config.same_dealer_same_window_quotes_are_clustered = parse_bool(
        required(raw,
            "hurdle_envelope.same_dealer_same_window_quotes_are_clustered"));
    config.target_market_claim_id = parse_text(
        required(raw, "hurdle_envelope.target_market_claim_id"));
    config.target_normalized_term_result_id = parse_text(required(raw,
        "hurdle_envelope.target_normalized_term_result_id"));
    config.target_currency_label = parse_text(
        required(raw, "hurdle_envelope.target_currency_label"));
    config.target_monetary_basis = parse_text(
        required(raw, "hurdle_envelope.target_monetary_basis"));
    config.target_claim_quantity_million = parse_double(required(raw,
        "hurdle_envelope.target_claim_quantity_million"));
    config.annual_effective_domain_lower = parse_double(required(raw,
        "hurdle_envelope.annual_effective_domain_lower"));
    config.annual_effective_domain_upper = parse_double(required(raw,
        "hurdle_envelope.annual_effective_domain_upper"));
    config.maximum_contaminated_clusters = parse_size(required(raw,
        "hurdle_envelope.maximum_contaminated_clusters"));
    config.minimum_consensus_cluster_coverage = parse_size(required(raw,
        "hurdle_envelope.minimum_consensus_cluster_coverage"));

    config.observations.reserve(observation_count);
    for (std::size_t index = 0U; index < observation_count; ++index) {
        const auto field = [&](std::string_view name) -> const RawValue& {
            return required(raw, observation_key(index, name));
        };
        RobustHurdleMarketObservationConfig observation;
        observation.record_id = parse_text(field("id"));
        observation.economic_observation_cluster_id =
            parse_text(field("economic_observation_cluster_id"));
        observation.status = parse_observation_status(field("status"));
        observation.transaction_type =
            parse_transaction_type(field("transaction_type"));
        observation.observation_date =
            parse_text(field("observation_date"));
        observation.execution_date = parse_text(field("execution_date"));
        observation.settlement_date = parse_text(field("settlement_date"));
        observation.source_reference =
            parse_text(field("source_reference"));
        observation.evidence_record_id =
            parse_text(field("evidence_record_id"));
        observation.settlement_evidence_record_id =
            parse_text(field("settlement_evidence_record_id"));
        observation.orderly_arms_length_evidence_record_id =
            parse_text(field("orderly_arms_length_evidence_record_id"));
        observation.transaction_market_anchor_id =
            parse_text(field("transaction_market_anchor_id"));
        observation.transaction_execution_is_evidenced =
            parse_bool(field("transaction_execution_is_evidenced"));
        observation.buyer_cash_payment_is_evidenced =
            parse_bool(field("buyer_cash_payment_is_evidenced"));
        observation.transaction_settlement_is_evidenced =
            parse_bool(field("transaction_settlement_is_evidenced"));
        observation.orderly_transaction_is_evidenced =
            parse_bool(field("orderly_transaction_is_evidenced"));
        observation.arms_length_transaction_is_evidenced =
            parse_bool(field("arms_length_transaction_is_evidenced"));
        observation.forced_or_distressed_transaction =
            parse_bool(field("forced_or_distressed_transaction"));
        observation.related_party_transaction =
            parse_bool(field("related_party_transaction"));
        observation.settled_claim_quantity_million =
            parse_double(field("settled_claim_quantity_million"));
        observation.observed_market_claim_id =
            parse_text(field("observed_market_claim_id"));
        observation.normalized_term_result_id =
            parse_text(field("normalized_term_result_id"));
        observation.claim_relation =
            parse_claim_relation(field("claim_relation"));
        observation.currency_label = parse_text(field("currency_label"));
        observation.monetary_basis = parse_text(field("monetary_basis"));
        observation.return_basis = parse_return_basis(field("return_basis"));
        observation.annual_effective_hurdle_lower =
            parse_double(field("annual_effective_hurdle_lower"));
        observation.annual_effective_hurdle_upper =
            parse_double(field("annual_effective_hurdle_upper"));
        observation.return_normalization_result_id =
            parse_text(field("return_normalization_result_id"));
        observation.observed_gross_buyer_price_million =
            parse_double(field("observed_gross_buyer_price_million"));
        observation.buyer_direct_cost_million =
            parse_double(field("buyer_direct_cost_million"));
        observation.observed_gross_buyer_price_is_evidenced =
            parse_bool(field("observed_gross_buyer_price_is_evidenced"));
        observation.buyer_direct_cost_is_evidenced =
            parse_bool(field("buyer_direct_cost_is_evidenced"));
        observation.buyer_direct_cost_evidence_record_id =
            parse_text(field("buyer_direct_cost_evidence_record_id"));
        observation.quoted_bid_gross_buyer_price_million =
            parse_double(field("quoted_bid_gross_buyer_price_million"));
        observation.quoted_ask_gross_buyer_price_million =
            parse_double(field("quoted_ask_gross_buyer_price_million"));
        observation.quoted_bid_buyer_direct_cost_million =
            parse_double(field("quoted_bid_buyer_direct_cost_million"));
        observation.quoted_ask_buyer_direct_cost_million =
            parse_double(field("quoted_ask_buyer_direct_cost_million"));
        observation.quoted_bid_claim_quantity_million =
            parse_double(field("quoted_bid_claim_quantity_million"));
        observation.quoted_ask_claim_quantity_million =
            parse_double(field("quoted_ask_claim_quantity_million"));
        observation.quoted_bid_timestamp_utc =
            parse_text(field("quoted_bid_timestamp_utc"));
        observation.quoted_ask_timestamp_utc =
            parse_text(field("quoted_ask_timestamp_utc"));
        observation.quoted_bid_currency_label =
            parse_text(field("quoted_bid_currency_label"));
        observation.quoted_ask_currency_label =
            parse_text(field("quoted_ask_currency_label"));
        observation.quoted_bid_and_ask_are_executable =
            parse_bool(field("quoted_bid_and_ask_are_executable"));
        observation.quoted_bid_and_ask_are_evidenced =
            parse_bool(field("quoted_bid_and_ask_are_evidenced"));
        observation.quote_evidence_record_id =
            parse_text(field("quote_evidence_record_id"));
        observation.quote_valid_until_utc =
            parse_text(field("quote_valid_until_utc"));
        observation
            .discounted_expected_cash_is_strictly_decreasing_over_rate_interval =
            parse_bool(field(
                "discounted_expected_cash_is_strictly_decreasing_over_rate_interval"));
        observation
            .quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper =
            parse_bool(field(
                "quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper"));
        observation.settled_price_preimage_is_single_connected_interval =
            parse_bool(field(
                "settled_price_preimage_is_single_connected_interval"));
        observation.expected_cash_operator =
            parse_expected_cash_operator(field("expected_cash_operator"));
        observation.full_dated_scenario_cash_input_sha256 =
            parse_text(field("full_dated_scenario_cash_input_sha256"));
        observation.probability_input_sha256 =
            parse_text(field("probability_input_sha256"));
        observation.expected_cash_calculation_run_sha256 =
            parse_text(field("expected_cash_calculation_run_sha256"));
        observation.expected_cash_reconstruction_identity_is_evidenced =
            parse_bool(field(
                "expected_cash_reconstruction_identity_is_evidenced"));
        observation.maximum_expected_cash_reconstruction_residual_million =
            parse_double(field(
                "maximum_expected_cash_reconstruction_residual_million"));
        observation.physical_expected_full_claim_cash_includes_loss_and_timing =
            parse_bool(field(
                "physical_expected_full_claim_cash_includes_loss_and_timing"));
        observation.normalized_to_full_claim_at_observation_date =
            parse_bool(field(
                "normalized_to_full_claim_at_observation_date"));
        observation.fixed_probability_to_robust_bridge_is_present =
            parse_bool(field(
                "fixed_probability_to_robust_bridge_is_present"));
        observation.fixed_probability_to_robust_bridge_lower_log_delta =
            parse_double(field(
                "fixed_probability_to_robust_bridge_lower_log_delta"));
        observation.fixed_probability_to_robust_bridge_upper_log_delta =
            parse_double(field(
                "fixed_probability_to_robust_bridge_upper_log_delta"));
        observation.fixed_probability_to_robust_bridge_method_id =
            parse_text(field(
                "fixed_probability_to_robust_bridge_method_id"));
        observation.fixed_probability_to_robust_bridge_evidence_record_id =
            parse_text(field(
                "fixed_probability_to_robust_bridge_evidence_record_id"));
        observation.target_reference_price_relation =
            parse_reference_relation(field("target_reference_price_relation"));
        observation.side_rights_or_non_cash_consideration_present =
            parse_bool(field(
                "side_rights_or_non_cash_consideration_present"));
        observation.side_rights_or_non_cash_consideration_note =
            parse_text(field(
                "side_rights_or_non_cash_consideration_note"));
        for (std::size_t axis_index = 0U;
             axis_index < kComparabilityFields.size(); ++axis_index) {
            observation.comparability_treatments[axis_index] =
                parse_treatment(required(raw,
                    comparability_key(
                        index, kComparabilityFields[axis_index])));
        }
        observation.adjustment_set_basis =
            parse_adjustment_set_basis(field("adjustment_set_basis"));
        observation.jointly_feasible_total_lower_log_gross_return_delta =
            parse_double(field(
                "jointly_feasible_total_lower_log_gross_return_delta"));
        observation.jointly_feasible_total_upper_log_gross_return_delta =
            parse_double(field(
                "jointly_feasible_total_upper_log_gross_return_delta"));
        observation.joint_adjustment_set_method_id =
            parse_text(field("joint_adjustment_set_method_id"));
        observation.joint_adjustment_set_evidence_record_id =
            parse_text(field("joint_adjustment_set_evidence_record_id"));
        observation
            .expected_loss_recovery_and_timing_are_excluded_from_adjustments =
            parse_bool(field(
                "expected_loss_recovery_and_timing_are_excluded_from_adjustments"));
        observation.source_note = parse_text(field("source_note"));

        observation.adjustments.reserve(adjustment_counts[index]);
        for (std::size_t adjustment_index = 0U;
             adjustment_index < adjustment_counts[index];
             ++adjustment_index) {
            const auto adjustment_field =
                [&](std::string_view name) -> const RawValue& {
                return required(raw,
                    adjustment_key(index, adjustment_index, name));
            };
            RobustHurdleLogGrossReturnAdjustmentConfig adjustment;
            adjustment.adjustment_id =
                parse_text(adjustment_field("id"));
            adjustment.axis = parse_axis(adjustment_field("axis"));
            adjustment.lower_log_gross_return_delta = parse_double(
                adjustment_field("lower_log_gross_return_delta"));
            adjustment.upper_log_gross_return_delta = parse_double(
                adjustment_field("upper_log_gross_return_delta"));
            adjustment.method_id =
                parse_text(adjustment_field("method_id"));
            adjustment.source_reference =
                parse_text(adjustment_field("source_reference"));
            adjustment.evidence_record_id =
                parse_text(adjustment_field("evidence_record_id"));
            adjustment.source_note =
                parse_text(adjustment_field("source_note"));
            observation.adjustments.push_back(std::move(adjustment));
        }
        config.observations.push_back(std::move(observation));
    }
    return canonicalized_and_validated(std::move(config));
}

} // namespace

RobustHurdleEnvelopeConfig parse_robust_hurdle_envelope_config(
    std::istream& input) {
    return parse_raw(read_raw(input));
}

RobustHurdleEnvelopeConfig load_robust_hurdle_envelope_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "robust-hurdle-envelope configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open robust-hurdle-envelope configuration file: " +
            path.string());
    }
    try {
        return parse_robust_hurdle_envelope_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading robust-hurdle-envelope configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_robust_hurdle_envelope_config(
    std::ostream& output, const RobustHurdleEnvelopeConfig& config) {
    const RobustHurdleEnvelopeConfig canonical =
        canonicalized_and_validated(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;

    output << "hurdle_envelope.model_version=" << canonical.model_version
           << '\n';
    output << "hurdle_envelope.analysis_id=" << canonical.analysis_id
           << '\n';
    output << "hurdle_envelope.as_of_date=" << canonical.as_of_date << '\n';
    output << "hurdle_envelope.source_note=" << canonical.source_note
           << '\n';
    output << "hurdle_envelope.synthetic_inputs="
           << canonical.synthetic_inputs << '\n';
    output << "hurdle_envelope.universe_manifest_id="
           << canonical.universe_manifest_id << '\n';
    output << "hurdle_envelope.inclusion_rule_id="
           << canonical.inclusion_rule_id << '\n';
    output << "hurdle_envelope.deduplication_manifest_id="
           << canonical.deduplication_manifest_id << '\n';
    output << "hurdle_envelope.source_clustering_rule_id="
           << canonical.source_clustering_rule_id << '\n';
    output << "hurdle_envelope.observation_universe_is_frozen="
           << canonical.observation_universe_is_frozen << '\n';
    output << "hurdle_envelope.inclusion_rule_is_predeclared="
           << canonical.inclusion_rule_is_predeclared << '\n';
    output << "hurdle_envelope.deduplication_rule_is_predeclared="
           << canonical.deduplication_rule_is_predeclared << '\n';
    output << "hurdle_envelope.all_in_scope_economic_observation_clusters_are_included="
           << canonical
                  .all_in_scope_economic_observation_clusters_are_included
           << '\n';
    output << "hurdle_envelope.same_dealer_same_window_quotes_are_clustered="
           << canonical.same_dealer_same_window_quotes_are_clustered
           << '\n';
    output << "hurdle_envelope.target_market_claim_id="
           << canonical.target_market_claim_id << '\n';
    output << "hurdle_envelope.target_normalized_term_result_id="
           << canonical.target_normalized_term_result_id << '\n';
    output << "hurdle_envelope.target_currency_label="
           << canonical.target_currency_label << '\n';
    output << "hurdle_envelope.target_monetary_basis="
           << canonical.target_monetary_basis << '\n';
    output << "hurdle_envelope.target_claim_quantity_million="
           << canonical.target_claim_quantity_million << '\n';
    output << "hurdle_envelope.annual_effective_domain_lower="
           << canonical.annual_effective_domain_lower << '\n';
    output << "hurdle_envelope.annual_effective_domain_upper="
           << canonical.annual_effective_domain_upper << '\n';
    output << "hurdle_envelope.maximum_contaminated_clusters="
           << canonical.maximum_contaminated_clusters << '\n';
    output << "hurdle_envelope.minimum_consensus_cluster_coverage="
           << canonical.minimum_consensus_cluster_coverage << '\n';
    output << "observation.count=" << canonical.observations.size() << '\n';

    for (std::size_t index = 0U; index < canonical.observations.size();
         ++index) {
        const RobustHurdleMarketObservationConfig& observation =
            canonical.observations[index];
        const auto key = [&](std::string_view field) {
            return observation_key(index, field);
        };
        output << key("id") << '=' << observation.record_id << '\n';
        output << key("economic_observation_cluster_id") << '='
               << observation.economic_observation_cluster_id << '\n';
        output << key("status") << '='
               << observation_status_text(observation.status) << '\n';
        output << key("transaction_type") << '='
               << transaction_type_text(observation.transaction_type)
               << '\n';
        output << key("observation_date") << '='
               << observation.observation_date << '\n';
        output << key("execution_date") << '=' << observation.execution_date
               << '\n';
        output << key("settlement_date") << '='
               << observation.settlement_date << '\n';
        output << key("source_reference") << '='
               << observation.source_reference << '\n';
        output << key("evidence_record_id") << '='
               << observation.evidence_record_id << '\n';
        output << key("settlement_evidence_record_id") << '='
               << observation.settlement_evidence_record_id << '\n';
        output << key("orderly_arms_length_evidence_record_id") << '='
               << observation.orderly_arms_length_evidence_record_id
               << '\n';
        output << key("transaction_market_anchor_id") << '='
               << observation.transaction_market_anchor_id << '\n';
        output << key("transaction_execution_is_evidenced") << '='
               << observation.transaction_execution_is_evidenced << '\n';
        output << key("buyer_cash_payment_is_evidenced") << '='
               << observation.buyer_cash_payment_is_evidenced << '\n';
        output << key("transaction_settlement_is_evidenced") << '='
               << observation.transaction_settlement_is_evidenced << '\n';
        output << key("orderly_transaction_is_evidenced") << '='
               << observation.orderly_transaction_is_evidenced << '\n';
        output << key("arms_length_transaction_is_evidenced") << '='
               << observation.arms_length_transaction_is_evidenced << '\n';
        output << key("forced_or_distressed_transaction") << '='
               << observation.forced_or_distressed_transaction << '\n';
        output << key("related_party_transaction") << '='
               << observation.related_party_transaction << '\n';
        output << key("settled_claim_quantity_million") << '='
               << observation.settled_claim_quantity_million << '\n';
        output << key("observed_market_claim_id") << '='
               << observation.observed_market_claim_id << '\n';
        output << key("normalized_term_result_id") << '='
               << observation.normalized_term_result_id << '\n';
        output << key("claim_relation") << '='
               << claim_relation_text(observation.claim_relation) << '\n';
        output << key("currency_label") << '=' << observation.currency_label
               << '\n';
        output << key("monetary_basis") << '=' << observation.monetary_basis
               << '\n';
        output << key("return_basis") << '='
               << return_basis_text(observation.return_basis) << '\n';
        output << key("annual_effective_hurdle_lower") << '='
               << observation.annual_effective_hurdle_lower << '\n';
        output << key("annual_effective_hurdle_upper") << '='
               << observation.annual_effective_hurdle_upper << '\n';
        output << key("return_normalization_result_id") << '='
               << observation.return_normalization_result_id << '\n';
        output << key("observed_gross_buyer_price_million") << '='
               << observation.observed_gross_buyer_price_million << '\n';
        output << key("buyer_direct_cost_million") << '='
               << observation.buyer_direct_cost_million << '\n';
        output << key("observed_gross_buyer_price_is_evidenced") << '='
               << observation.observed_gross_buyer_price_is_evidenced
               << '\n';
        output << key("buyer_direct_cost_is_evidenced") << '='
               << observation.buyer_direct_cost_is_evidenced << '\n';
        output << key("buyer_direct_cost_evidence_record_id") << '='
               << observation.buyer_direct_cost_evidence_record_id << '\n';
        output << key("quoted_bid_gross_buyer_price_million") << '='
               << observation.quoted_bid_gross_buyer_price_million << '\n';
        output << key("quoted_ask_gross_buyer_price_million") << '='
               << observation.quoted_ask_gross_buyer_price_million << '\n';
        output << key("quoted_bid_buyer_direct_cost_million") << '='
               << observation.quoted_bid_buyer_direct_cost_million << '\n';
        output << key("quoted_ask_buyer_direct_cost_million") << '='
               << observation.quoted_ask_buyer_direct_cost_million << '\n';
        output << key("quoted_bid_claim_quantity_million") << '='
               << observation.quoted_bid_claim_quantity_million << '\n';
        output << key("quoted_ask_claim_quantity_million") << '='
               << observation.quoted_ask_claim_quantity_million << '\n';
        output << key("quoted_bid_timestamp_utc") << '='
               << observation.quoted_bid_timestamp_utc << '\n';
        output << key("quoted_ask_timestamp_utc") << '='
               << observation.quoted_ask_timestamp_utc << '\n';
        output << key("quoted_bid_currency_label") << '='
               << observation.quoted_bid_currency_label << '\n';
        output << key("quoted_ask_currency_label") << '='
               << observation.quoted_ask_currency_label << '\n';
        output << key("quoted_bid_and_ask_are_executable") << '='
               << observation.quoted_bid_and_ask_are_executable << '\n';
        output << key("quoted_bid_and_ask_are_evidenced") << '='
               << observation.quoted_bid_and_ask_are_evidenced << '\n';
        output << key("quote_evidence_record_id") << '='
               << observation.quote_evidence_record_id << '\n';
        output << key("quote_valid_until_utc") << '='
               << observation.quote_valid_until_utc << '\n';
        output << key("discounted_expected_cash_is_strictly_decreasing_over_rate_interval")
               << '='
               << observation
                      .discounted_expected_cash_is_strictly_decreasing_over_rate_interval
               << '\n';
        output << key("quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper")
               << '='
               << observation
                      .quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper
               << '\n';
        output << key("settled_price_preimage_is_single_connected_interval")
               << '='
               << observation
                      .settled_price_preimage_is_single_connected_interval
               << '\n';
        output << key("expected_cash_operator") << '='
               << expected_cash_operator_text(
                      observation.expected_cash_operator)
               << '\n';
        output << key("full_dated_scenario_cash_input_sha256") << '='
               << observation.full_dated_scenario_cash_input_sha256 << '\n';
        output << key("probability_input_sha256") << '='
               << observation.probability_input_sha256 << '\n';
        output << key("expected_cash_calculation_run_sha256") << '='
               << observation.expected_cash_calculation_run_sha256 << '\n';
        output << key("expected_cash_reconstruction_identity_is_evidenced")
               << '='
               << observation
                      .expected_cash_reconstruction_identity_is_evidenced
               << '\n';
        output << key("maximum_expected_cash_reconstruction_residual_million")
               << '='
               << observation
                      .maximum_expected_cash_reconstruction_residual_million
               << '\n';
        output << key("physical_expected_full_claim_cash_includes_loss_and_timing")
               << '='
               << observation
                      .physical_expected_full_claim_cash_includes_loss_and_timing
               << '\n';
        output << key("normalized_to_full_claim_at_observation_date") << '='
               << observation.normalized_to_full_claim_at_observation_date
               << '\n';
        output << key("fixed_probability_to_robust_bridge_is_present") << '='
               << observation.fixed_probability_to_robust_bridge_is_present
               << '\n';
        output << key("fixed_probability_to_robust_bridge_lower_log_delta")
               << '='
               << observation
                      .fixed_probability_to_robust_bridge_lower_log_delta
               << '\n';
        output << key("fixed_probability_to_robust_bridge_upper_log_delta")
               << '='
               << observation
                      .fixed_probability_to_robust_bridge_upper_log_delta
               << '\n';
        output << key("fixed_probability_to_robust_bridge_method_id") << '='
               << observation.fixed_probability_to_robust_bridge_method_id
               << '\n';
        output << key("fixed_probability_to_robust_bridge_evidence_record_id")
               << '='
               << observation
                      .fixed_probability_to_robust_bridge_evidence_record_id
               << '\n';
        output << key("target_reference_price_relation") << '='
               << reference_relation_text(
                      observation.target_reference_price_relation)
               << '\n';
        output << key("side_rights_or_non_cash_consideration_present") << '='
               << observation
                      .side_rights_or_non_cash_consideration_present
               << '\n';
        output << key("side_rights_or_non_cash_consideration_note") << '='
               << observation.side_rights_or_non_cash_consideration_note
               << '\n';
        for (std::size_t axis_index = 0U;
             axis_index < kComparabilityFields.size(); ++axis_index) {
            output << comparability_key(
                          index, kComparabilityFields[axis_index])
                   << '='
                   << treatment_text(
                          observation.comparability_treatments[axis_index])
                   << '\n';
        }
        output << key("adjustment_set_basis") << '='
               << adjustment_set_basis_text(observation.adjustment_set_basis)
               << '\n';
        output << key("jointly_feasible_total_lower_log_gross_return_delta")
               << '='
               << observation
                      .jointly_feasible_total_lower_log_gross_return_delta
               << '\n';
        output << key("jointly_feasible_total_upper_log_gross_return_delta")
               << '='
               << observation
                      .jointly_feasible_total_upper_log_gross_return_delta
               << '\n';
        output << key("joint_adjustment_set_method_id") << '='
               << observation.joint_adjustment_set_method_id << '\n';
        output << key("joint_adjustment_set_evidence_record_id") << '='
               << observation.joint_adjustment_set_evidence_record_id
               << '\n';
        output << key("expected_loss_recovery_and_timing_are_excluded_from_adjustments")
               << '='
               << observation
                      .expected_loss_recovery_and_timing_are_excluded_from_adjustments
               << '\n';
        output << key("source_note") << '=' << observation.source_note
               << '\n';
        output << key("adjustment.count") << '='
               << observation.adjustments.size() << '\n';

        for (std::size_t adjustment_index = 0U;
             adjustment_index < observation.adjustments.size();
             ++adjustment_index) {
            const RobustHurdleLogGrossReturnAdjustmentConfig& adjustment =
                observation.adjustments[adjustment_index];
            const auto adjustment_output_key = [&](std::string_view field) {
                return adjustment_key(index, adjustment_index, field);
            };
            output << adjustment_output_key("id") << '='
                   << adjustment.adjustment_id << '\n';
            output << adjustment_output_key("axis") << '='
                   << kComparabilityFields[robust_hurdle_axis_index(
                          adjustment.axis)]
                   << '\n';
            output << adjustment_output_key(
                          "lower_log_gross_return_delta")
                   << '=' << adjustment.lower_log_gross_return_delta << '\n';
            output << adjustment_output_key(
                          "upper_log_gross_return_delta")
                   << '=' << adjustment.upper_log_gross_return_delta << '\n';
            output << adjustment_output_key("method_id") << '='
                   << adjustment.method_id << '\n';
            output << adjustment_output_key("source_reference") << '='
                   << adjustment.source_reference << '\n';
            output << adjustment_output_key("evidence_record_id") << '='
                   << adjustment.evidence_record_id << '\n';
            output << adjustment_output_key("source_note") << '='
                   << adjustment.source_note << '\n';
        }
    }
    if (!output) {
        throw std::runtime_error(
            "failed while writing robust-hurdle-envelope configuration");
    }
}

} // namespace naturalehia::cellular_finance
