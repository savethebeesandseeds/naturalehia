// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/funding_bridge_config.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <initializer_list>
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
constexpr std::size_t kMaximumConfigKeys = 2'000'000U;
constexpr std::size_t kMaximumProviders = 10'000U;
constexpr std::size_t kMaximumScenarios = 10'000U;
constexpr std::size_t kMaximumRecordsPerCollection = 100'000U;
constexpr std::size_t kMaximumPerformanceRecords = 2'000'000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMaximumAmountMillion = 1.0e6;
constexpr double kMaximumAnnualRate = 10.0;
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3U};

struct RawValue {
    std::string value{};
    std::size_t line{0U};
    bool had_surrounding_whitespace{false};
};

using RawMap = std::unordered_map<std::string, RawValue>;

struct ScenarioShape {
    std::size_t capital_call_request_count{0U};
    std::size_t capital_call_outcome_count{0U};
    std::size_t warehouse_draw_request_count{0U};
    std::size_t warehouse_draw_outcome_count{0U};
    std::size_t supplemental_receipt_count{0U};
    std::size_t eligible_basis_movement_count{0U};
    std::size_t protection_absorption_count{0U};
    std::size_t protection_release_count{0U};
};

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

[[nodiscard]] bool is_ascii_space(char character) noexcept {
    return character == ' ' || character == '\t' || character == '\n' ||
        character == '\r' || character == '\f' || character == '\v';
}

[[nodiscard]] std::string_view trim_view(std::string_view value) noexcept {
    while (!value.empty() && is_ascii_space(value.front())) {
        value.remove_prefix(1U);
    }
    while (!value.empty() && is_ascii_space(value.back())) {
        value.remove_suffix(1U);
    }
    return value;
}

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument("funding-bridge configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "funding-bridge configuration is missing required key: " + key);
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

[[nodiscard]] std::string parse_text(const RawValue& raw) {
    if (raw.had_surrounding_whitespace) {
        parse_error(raw.line,
            "text values must not begin or end with whitespace");
    }
    return raw.value;
}

[[nodiscard]] FundingSettlementStatus parse_settlement_status(
    const RawValue& raw) {
    if (raw.value == "settled-in-full") {
        return FundingSettlementStatus::SettledInFull;
    }
    if (raw.value == "final-partial-settlement") {
        return FundingSettlementStatus::FinalPartialSettlement;
    }
    if (raw.value == "failed") {
        return FundingSettlementStatus::Failed;
    }
    parse_error(raw.line,
        "expected settled-in-full, final-partial-settlement, or failed");
}

[[nodiscard]] SupplementalFundingPurpose parse_supplemental_purpose(
    const RawValue& raw) {
    if (raw.value == "cost-support") {
        return SupplementalFundingPurpose::CostSupport;
    }
    if (raw.value == "protection-replenishment") {
        return SupplementalFundingPurpose::ProtectionReplenishment;
    }
    if (raw.value == "settled-takeout") {
        return SupplementalFundingPurpose::SettledTakeout;
    }
    parse_error(raw.line,
        "expected cost-support, protection-replenishment, or settled-takeout");
}

[[nodiscard]] EligibleBasisMovementKind parse_basis_movement_kind(
    const RawValue& raw) {
    if (raw.value == "eligible-addition") {
        return EligibleBasisMovementKind::EligibleAddition;
    }
    if (raw.value == "principal-basis-return") {
        return EligibleBasisMovementKind::PrincipalBasisReturn;
    }
    if (raw.value == "disposition") {
        return EligibleBasisMovementKind::Disposition;
    }
    if (raw.value == "writeoff") {
        return EligibleBasisMovementKind::Writeoff;
    }
    if (raw.value == "eligibility-removal") {
        return EligibleBasisMovementKind::EligibilityRemoval;
    }
    parse_error(raw.line,
        "expected eligible-addition, principal-basis-return, disposition, writeoff, or eligibility-removal");
}

[[nodiscard]] std::string_view settlement_status_text(
    FundingSettlementStatus status) {
    switch (status) {
    case FundingSettlementStatus::SettledInFull:
        return "settled-in-full";
    case FundingSettlementStatus::FinalPartialSettlement:
        return "final-partial-settlement";
    case FundingSettlementStatus::Failed:
        return "failed";
    }
    throw std::invalid_argument("invalid funding settlement status");
}

[[nodiscard]] std::string_view supplemental_purpose_text(
    SupplementalFundingPurpose purpose) {
    switch (purpose) {
    case SupplementalFundingPurpose::CostSupport:
        return "cost-support";
    case SupplementalFundingPurpose::ProtectionReplenishment:
        return "protection-replenishment";
    case SupplementalFundingPurpose::SettledTakeout:
        return "settled-takeout";
    }
    throw std::invalid_argument("invalid supplemental funding purpose");
}

[[nodiscard]] std::string_view basis_movement_kind_text(
    EligibleBasisMovementKind kind) {
    switch (kind) {
    case EligibleBasisMovementKind::EligibleAddition:
        return "eligible-addition";
    case EligibleBasisMovementKind::PrincipalBasisReturn:
        return "principal-basis-return";
    case EligibleBasisMovementKind::Disposition:
        return "disposition";
    case EligibleBasisMovementKind::Writeoff:
        return "writeoff";
    case EligibleBasisMovementKind::EligibilityRemoval:
        return "eligibility-removal";
    }
    throw std::invalid_argument("invalid eligible-basis movement kind");
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
            std::string(description) + " must be a safe identifier");
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

void require_true(bool value, std::string_view description) {
    if (!value) {
        throw std::invalid_argument(
            std::string(description) + " must be explicitly true");
    }
}

void require_nonnegative_amount(
    double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > kMaximumAmountMillion) {
        throw std::invalid_argument(std::string(description) +
            " must be finite, non-negative, and no greater than 1e6 million");
    }
}

void require_positive_amount(double value, std::string_view description) {
    require_nonnegative_amount(value, description);
    if (value <= 0.0) {
        throw std::invalid_argument(
            std::string(description) + " must be positive");
    }
}

void require_fraction(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument(
            std::string(description) + " must be finite and in [0, 1]");
    }
}

void require_annual_rate(double value, std::string_view description) {
    if (!std::isfinite(value) || value < 0.0 ||
        value > kMaximumAnnualRate) {
        throw std::invalid_argument(std::string(description) +
            " must be finite, non-negative, and no greater than 10");
    }
}

[[nodiscard]] std::string provider_key(
    std::size_t provider, std::string_view field) {
    return "provider." + std::to_string(provider + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_key(
    std::size_t scenario, std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) + "." +
        std::string(field);
}

[[nodiscard]] std::string scenario_record_key(std::size_t scenario,
    std::string_view collection, std::size_t record,
    std::string_view field) {
    return "scenario." + std::to_string(scenario + 1U) + "." +
        std::string(collection) + "." + std::to_string(record + 1U) +
        "." + std::string(field);
}

[[nodiscard]] std::size_t checked_add(
    std::size_t left, std::size_t right) {
    if (right > std::numeric_limits<std::size_t>::max() - left) {
        throw std::invalid_argument(
            "funding-bridge declared configuration shape overflows size_t");
    }
    return left + right;
}

[[nodiscard]] std::size_t checked_multiply(
    std::size_t left, std::size_t right) {
    if (left != 0U &&
        right > std::numeric_limits<std::size_t>::max() / left) {
        throw std::invalid_argument(
            "funding-bridge declared configuration shape overflows size_t");
    }
    return left * right;
}

[[nodiscard]] std::size_t scenario_record_count(
    const ScenarioShape& shape) {
    std::size_t result = shape.capital_call_request_count;
    result = checked_add(result, shape.capital_call_outcome_count);
    result = checked_add(result, shape.warehouse_draw_request_count);
    result = checked_add(result, shape.warehouse_draw_outcome_count);
    result = checked_add(result, shape.supplemental_receipt_count);
    result = checked_add(result, shape.eligible_basis_movement_count);
    result = checked_add(result, shape.protection_absorption_count);
    return checked_add(result, shape.protection_release_count);
}

void require_bounded_collection_count(
    std::size_t count, std::string_view description) {
    if (count > kMaximumRecordsPerCollection) {
        throw std::invalid_argument(std::string(description) +
            " exceeds the 100000-record resource bound");
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
        const std::size_t delimiter_bytes = input.eof() ? 0U : 1U;
        const std::size_t consumed =
            checked_add(line_text.size(), delimiter_bytes);
        if (bytes_read > kMaximumConfigBytes ||
            consumed > kMaximumConfigBytes - bytes_read) {
            parse_error(line_number,
                "configuration exceeds the 16 MiB guardrail");
        }
        bytes_read += consumed;
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
        if (raw.size() >= kMaximumConfigKeys) {
            parse_error(line_number,
                "configuration exceeds the 2000000-key resource bound");
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
            "failed while reading funding-bridge configuration");
    }
    return raw;
}

void validate_provider(const FundingProvider& provider) {
    require_safe_identifier(provider.id, "funding-provider id");
    require_safe_identifier(
        provider.economic_group_id, "funding-provider economic_group_id");
    require_nonnegative_amount(provider.declared_capacity_million,
        "funding-provider declared_capacity_million");
    require_safe_identifier(
        provider.source_record_id, "funding-provider source_record_id");
}

void validate_callable_facility(const CallableCapitalFacility& facility) {
    require_safe_identifier(facility.id, "callable-facility id");
    require_safe_identifier(
        facility.provider_id, "callable-facility provider_id");
    require_safe_identifier(
        facility.source_record_id, "callable-facility source_record_id");
    require_nonnegative_amount(
        facility.commitment_million, "callable-facility commitment_million");
    require_safe_text(
        facility.permitted_purpose, "callable-facility permitted_purpose");
    require_annual_rate(facility.annual_commitment_fee_rate,
        "callable-facility annual_commitment_fee_rate");
    require_fraction(facility.liquidity_reserve_fraction,
        "callable-facility liquidity_reserve_fraction");
    require_annual_rate(facility.annual_liquidity_hurdle_rate,
        "callable-facility annual_liquidity_hurdle_rate");
    require_annual_rate(facility.annual_reserve_yield_rate,
        "callable-facility annual_reserve_yield_rate");
}

void validate_warehouse_facility(const WarehouseFacility& facility) {
    require_safe_identifier(facility.id, "warehouse-facility id");
    require_safe_identifier(
        facility.provider_id, "warehouse-facility provider_id");
    require_safe_identifier(
        facility.source_record_id, "warehouse-facility source_record_id");
    require_nonnegative_amount(facility.committed_limit_million,
        "warehouse-facility committed_limit_million");
    require_safe_text(
        facility.permitted_purpose, "warehouse-facility permitted_purpose");
    require_fraction(facility.collateral_advance_rate,
        "warehouse-facility collateral_advance_rate");
    require_annual_rate(facility.annual_interest_rate,
        "warehouse-facility annual_interest_rate");
    require_annual_rate(facility.annual_undrawn_fee_rate,
        "warehouse-facility annual_undrawn_fee_rate");
    require_fraction(
        facility.advance_fee_rate, "warehouse-facility advance_fee_rate");
    require_fraction(
        facility.upfront_fee_rate, "warehouse-facility upfront_fee_rate");
}

void validate_performance(
    const FundingBridgeScenarioPerformance& performance) {
    require_safe_identifier(
        performance.scenario_id, "funding-performance scenario_id");
    require_bounded_collection_count(performance.capital_call_requests.size(),
        "capital-call request count");
    require_bounded_collection_count(performance.capital_call_outcomes.size(),
        "capital-call outcome count");
    require_bounded_collection_count(performance.warehouse_draw_requests.size(),
        "warehouse-draw request count");
    require_bounded_collection_count(performance.warehouse_draw_outcomes.size(),
        "warehouse-draw outcome count");
    require_bounded_collection_count(performance.supplemental_receipts.size(),
        "supplemental-receipt count");
    require_bounded_collection_count(performance.eligible_basis_movements.size(),
        "eligible-basis movement count");
    require_bounded_collection_count(performance.protection_absorptions.size(),
        "protection-absorption count");
    require_bounded_collection_count(performance.protection_releases.size(),
        "protection-release count");

    for (const CapitalCallRequest& request :
        performance.capital_call_requests) {
        require_safe_identifier(request.id, "capital-call request id");
        require_safe_identifier(
            request.facility_id, "capital-call request facility_id");
        require_positive_amount(
            request.requested_million, "capital-call requested_million");
    }
    for (const FundingSettlementOutcome& outcome :
        performance.capital_call_outcomes) {
        require_safe_identifier(
            outcome.request_id, "capital-call outcome request_id");
        (void)settlement_status_text(outcome.status);
        require_nonnegative_amount(
            outcome.actual_cash_million, "capital-call outcome actual cash");
        require_safe_identifier(outcome.source_record_id,
            "capital-call outcome source_record_id");
    }
    for (const WarehouseDrawRequest& request :
        performance.warehouse_draw_requests) {
        require_safe_identifier(request.id, "warehouse-draw request id");
        require_safe_identifier(
            request.facility_id, "warehouse-draw request facility_id");
        require_positive_amount(
            request.requested_million, "warehouse-draw requested_million");
    }
    for (const FundingSettlementOutcome& outcome :
        performance.warehouse_draw_outcomes) {
        require_safe_identifier(
            outcome.request_id, "warehouse-draw outcome request_id");
        (void)settlement_status_text(outcome.status);
        require_nonnegative_amount(
            outcome.actual_cash_million, "warehouse-draw outcome actual cash");
        require_safe_identifier(outcome.source_record_id,
            "warehouse-draw outcome source_record_id");
    }
    for (const SupplementalFundingReceipt& receipt :
        performance.supplemental_receipts) {
        require_safe_identifier(receipt.id, "supplemental-receipt id");
        require_safe_identifier(
            receipt.provider_id, "supplemental-receipt provider_id");
        (void)supplemental_purpose_text(receipt.purpose);
        require_positive_amount(receipt.actual_cash_million,
            "supplemental-receipt actual_cash_million");
        require_safe_identifier(receipt.source_record_id,
            "supplemental-receipt source_record_id");
    }
    for (const EligibleBasisMovement& movement :
        performance.eligible_basis_movements) {
        require_safe_identifier(movement.id, "eligible-basis movement id");
        (void)basis_movement_kind_text(movement.kind);
        require_positive_amount(
            movement.amount_million, "eligible-basis movement amount_million");
        require_safe_identifier(
            movement.reference_id, "eligible-basis movement reference_id");
        require_safe_identifier(movement.source_record_id,
            "eligible-basis movement source_record_id");
    }
    for (const ProtectionAbsorption& absorption :
        performance.protection_absorptions) {
        require_safe_identifier(absorption.id, "protection-absorption id");
        require_positive_amount(absorption.amount_million,
            "protection-absorption amount_million");
        require_safe_identifier(absorption.reference_id,
            "protection-absorption reference_id");
        require_safe_identifier(absorption.source_record_id,
            "protection-absorption source_record_id");
    }
    for (const ProtectionRelease& release : performance.protection_releases) {
        require_safe_identifier(release.id, "protection-release id");
        require_positive_amount(
            release.amount_million, "protection-release amount_million");
        require_safe_identifier(
            release.source_record_id, "protection-release source_record_id");
    }
}

void validate_intrinsic_config(const FundingBridgeConfig& config) {
    if (config.model_version != kFundingBridgeModelVersion) {
        throw std::invalid_argument(
            "unsupported funding-bridge model_version");
    }
    require_safe_text(config.scenario_label,
        "funding-bridge scenario_label");
    require_safe_text(config.source_note, "funding-bridge source_note");
    require_nonnegative_amount(config.funded_at_close_cash_million,
        "funded_at_close_cash_million");
    require_safe_identifier(config.funded_at_close_provider_id,
        "funded_at_close_provider_id");
    require_safe_identifier(config.funded_at_close_source_record_id,
        "funded_at_close_source_record_id");
    if (config.providers.empty() ||
        config.providers.size() > kMaximumProviders) {
        throw std::invalid_argument(
            "funding-provider count must be between one and 10000");
    }
    for (const FundingProvider& provider : config.providers) {
        validate_provider(provider);
    }
    validate_callable_facility(config.callable_facility);
    validate_warehouse_facility(config.warehouse_facility);

    require_true(config.uncalled_commitment_is_not_cash_or_loss_absorption,
        "uncalled_commitment_is_not_cash_or_loss_absorption");
    require_true(
        config.acquisition_and_primary_funding_uses_precede_same_month_receipts,
        "acquisition_and_primary_funding_uses_precede_same_month_receipts");
    require_true(config.warehouse_is_external_temporary_debt,
        "warehouse_is_external_temporary_debt");
    require_true(config.warehouse_proceeds_cannot_fund_interest_fees_or_costs,
        "warehouse_proceeds_cannot_fund_interest_fees_or_costs");
    require_true(
        config.project_receipts_sweep_warehouse_principal_before_investor_cash,
        "project_receipts_sweep_warehouse_principal_before_investor_cash");
    require_true(config.policy_uses_observed_history_only,
        "policy_uses_observed_history_only");
    require_true(config.no_dynamic_tranche_allocation_or_pricing_is_claimed,
        "no_dynamic_tranche_allocation_or_pricing_is_claimed");

    if (config.scenario_performance.empty() ||
        config.scenario_performance.size() > kMaximumScenarios) {
        throw std::invalid_argument(
            "funding-bridge scenario count must be between one and 10000");
    }
    std::size_t total_records = 0U;
    for (const FundingBridgeScenarioPerformance& performance :
        config.scenario_performance) {
        validate_performance(performance);
        ScenarioShape shape;
        shape.capital_call_request_count =
            performance.capital_call_requests.size();
        shape.capital_call_outcome_count =
            performance.capital_call_outcomes.size();
        shape.warehouse_draw_request_count =
            performance.warehouse_draw_requests.size();
        shape.warehouse_draw_outcome_count =
            performance.warehouse_draw_outcomes.size();
        shape.supplemental_receipt_count =
            performance.supplemental_receipts.size();
        shape.eligible_basis_movement_count =
            performance.eligible_basis_movements.size();
        shape.protection_absorption_count =
            performance.protection_absorptions.size();
        shape.protection_release_count =
            performance.protection_releases.size();
        const std::size_t scenario_records = scenario_record_count(shape);
        if (scenario_records > kMaximumPerformanceRecords - total_records) {
            throw std::invalid_argument(
                "aggregate funding-performance records exceed the resource bound");
        }
        total_records += scenario_records;
    }
}

[[nodiscard]] FundingBridgeConfig parse_raw(const RawMap& raw) {
    static const std::unordered_set<std::string> fixed_keys{
        "funding_bridge.model_version",
        "funding_bridge.label",
        "funding_bridge.source_note",
        "funding_bridge.synthetic_inputs",
        "funding_bridge.funded_at_close_cash_million",
        "funding_bridge.funded_at_close_provider_id",
        "funding_bridge.funded_at_close_source_record_id",
        "provider.count",
        "callable_facility.id",
        "callable_facility.provider_id",
        "callable_facility.source_record_id",
        "callable_facility.commitment_million",
        "callable_facility.availability_start_month",
        "callable_facility.contractual_expiry_month",
        "callable_facility.settlement_lag_months",
        "callable_facility.permitted_purpose",
        "callable_facility.annual_commitment_fee_rate",
        "callable_facility.liquidity_reserve_fraction",
        "callable_facility.annual_liquidity_hurdle_rate",
        "callable_facility.annual_reserve_yield_rate",
        "warehouse_facility.id",
        "warehouse_facility.provider_id",
        "warehouse_facility.source_record_id",
        "warehouse_facility.committed_limit_million",
        "warehouse_facility.availability_start_month",
        "warehouse_facility.availability_end_month",
        "warehouse_facility.legal_maturity_month",
        "warehouse_facility.settlement_lag_months",
        "warehouse_facility.permitted_purpose",
        "warehouse_facility.collateral_advance_rate",
        "warehouse_facility.annual_interest_rate",
        "warehouse_facility.annual_undrawn_fee_rate",
        "warehouse_facility.advance_fee_rate",
        "warehouse_facility.upfront_fee_rate",
        "funding_bridge.uncalled_commitment_is_not_cash_or_loss_absorption",
        "funding_bridge.acquisition_and_primary_funding_uses_precede_same_month_receipts",
        "funding_bridge.warehouse_is_external_temporary_debt",
        "funding_bridge.warehouse_proceeds_cannot_fund_interest_fees_or_costs",
        "funding_bridge.project_receipts_sweep_warehouse_principal_before_investor_cash",
        "funding_bridge.policy_uses_observed_history_only",
        "funding_bridge.no_dynamic_tranche_allocation_or_pricing_is_claimed",
        "scenario.count",
    };
    for (const std::string& key : fixed_keys) {
        (void)required(raw, key);
    }

    const std::size_t provider_count =
        parse_size(required(raw, "provider.count"));
    if (provider_count == 0U || provider_count > kMaximumProviders) {
        throw std::invalid_argument(
            "parsed funding-provider count must be between one and 10000");
    }
    const std::size_t scenario_count =
        parse_size(required(raw, "scenario.count"));
    if (scenario_count == 0U || scenario_count > kMaximumScenarios) {
        throw std::invalid_argument(
            "parsed funding-bridge scenario count must be between one and 10000");
    }

    std::vector<ScenarioShape> shapes;
    shapes.reserve(scenario_count);
    std::size_t total_records = 0U;
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        ScenarioShape shape;
        shape.capital_call_request_count = parse_size(required(raw,
            scenario_key(scenario, "capital_call_request.count")));
        shape.capital_call_outcome_count = parse_size(required(raw,
            scenario_key(scenario, "capital_call_outcome.count")));
        shape.warehouse_draw_request_count = parse_size(required(raw,
            scenario_key(scenario, "warehouse_draw_request.count")));
        shape.warehouse_draw_outcome_count = parse_size(required(raw,
            scenario_key(scenario, "warehouse_draw_outcome.count")));
        shape.supplemental_receipt_count = parse_size(required(raw,
            scenario_key(scenario, "supplemental_receipt.count")));
        shape.eligible_basis_movement_count = parse_size(required(raw,
            scenario_key(scenario, "eligible_basis_movement.count")));
        shape.protection_absorption_count = parse_size(required(raw,
            scenario_key(scenario, "protection_absorption.count")));
        shape.protection_release_count = parse_size(required(raw,
            scenario_key(scenario, "protection_release.count")));
        require_bounded_collection_count(shape.capital_call_request_count,
            "parsed capital-call request count");
        require_bounded_collection_count(shape.capital_call_outcome_count,
            "parsed capital-call outcome count");
        require_bounded_collection_count(shape.warehouse_draw_request_count,
            "parsed warehouse-draw request count");
        require_bounded_collection_count(shape.warehouse_draw_outcome_count,
            "parsed warehouse-draw outcome count");
        require_bounded_collection_count(shape.supplemental_receipt_count,
            "parsed supplemental-receipt count");
        require_bounded_collection_count(shape.eligible_basis_movement_count,
            "parsed eligible-basis movement count");
        require_bounded_collection_count(shape.protection_absorption_count,
            "parsed protection-absorption count");
        require_bounded_collection_count(shape.protection_release_count,
            "parsed protection-release count");
        const std::size_t scenario_records = scenario_record_count(shape);
        if (scenario_records > kMaximumPerformanceRecords - total_records) {
            throw std::invalid_argument(
                "parsed aggregate funding-performance records exceed the resource bound");
        }
        total_records += scenario_records;
        shapes.push_back(shape);
    }

    std::size_t expected_count = fixed_keys.size();
    expected_count = checked_add(expected_count,
        checked_multiply(provider_count, 7U));
    expected_count = checked_add(expected_count,
        checked_multiply(scenario_count, 9U));
    for (const ScenarioShape& shape : shapes) {
        expected_count = checked_add(expected_count,
            checked_multiply(shape.capital_call_request_count, 4U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.capital_call_outcome_count, 5U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.warehouse_draw_request_count, 4U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.warehouse_draw_outcome_count, 5U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.supplemental_receipt_count, 6U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.eligible_basis_movement_count, 6U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.protection_absorption_count, 5U));
        expected_count = checked_add(expected_count,
            checked_multiply(shape.protection_release_count, 4U));
    }
    if (expected_count > kMaximumConfigKeys) {
        throw std::invalid_argument(
            "declared funding-bridge schema exceeds the key resource bound");
    }
    if (raw.size() != expected_count) {
        throw std::invalid_argument(
            "funding-bridge declared counts do not match the supplied key population");
    }

    std::unordered_set<std::string> expected = fixed_keys;
    expected.reserve(expected_count);
    for (std::size_t provider = 0U; provider < provider_count; ++provider) {
        for (const std::string_view field : {"id", "economic_group_id",
                 "declared_capacity_million", "identity_evidenced",
                 "affiliation_evidenced", "capacity_evidenced",
                 "source_record_id"}) {
            expected.insert(provider_key(provider, field));
        }
    }
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        expected.insert(scenario_key(scenario, "id"));
        for (const std::string_view collection : {"capital_call_request",
                 "capital_call_outcome", "warehouse_draw_request",
                 "warehouse_draw_outcome", "supplemental_receipt",
                 "eligible_basis_movement", "protection_absorption",
                 "protection_release"}) {
            expected.insert(scenario_key(
                scenario, std::string(collection) + ".count"));
        }
        const auto insert_fields = [&expected, scenario](
                                       std::string_view collection,
                                       std::size_t count,
                                       std::initializer_list<std::string_view>
                                           fields) {
            for (std::size_t record = 0U; record < count; ++record) {
                for (const std::string_view field : fields) {
                    expected.insert(scenario_record_key(
                        scenario, collection, record, field));
                }
            }
        };
        insert_fields("capital_call_request",
            shapes[scenario].capital_call_request_count,
            {"id", "facility_id", "notice_month", "requested_million"});
        insert_fields("capital_call_outcome",
            shapes[scenario].capital_call_outcome_count,
            {"request_id", "settlement_month", "status",
                "actual_cash_million", "source_record_id"});
        insert_fields("warehouse_draw_request",
            shapes[scenario].warehouse_draw_request_count,
            {"id", "facility_id", "request_month", "requested_million"});
        insert_fields("warehouse_draw_outcome",
            shapes[scenario].warehouse_draw_outcome_count,
            {"request_id", "settlement_month", "status",
                "actual_cash_million", "source_record_id"});
        insert_fields("supplemental_receipt",
            shapes[scenario].supplemental_receipt_count,
            {"id", "provider_id", "month", "purpose",
                "actual_cash_million", "source_record_id"});
        insert_fields("eligible_basis_movement",
            shapes[scenario].eligible_basis_movement_count,
            {"id", "month", "kind", "amount_million", "reference_id",
                "source_record_id"});
        insert_fields("protection_absorption",
            shapes[scenario].protection_absorption_count,
            {"id", "month", "amount_million", "reference_id",
                "source_record_id"});
        insert_fields("protection_release",
            shapes[scenario].protection_release_count,
            {"id", "month", "amount_million", "source_record_id"});
    }
    for (const auto& [key, value] : raw) {
        if (!expected.contains(key)) {
            parse_error(value.line, "unknown key: " + key);
        }
    }
    for (const std::string& key : expected) {
        (void)required(raw, key);
    }

    FundingBridgeConfig config;
    config.model_version =
        parse_text(required(raw, "funding_bridge.model_version"));
    config.scenario_label =
        parse_text(required(raw, "funding_bridge.label"));
    config.source_note =
        parse_text(required(raw, "funding_bridge.source_note"));
    config.synthetic_inputs =
        parse_bool(required(raw, "funding_bridge.synthetic_inputs"));
    config.funded_at_close_cash_million = parse_double(
        required(raw, "funding_bridge.funded_at_close_cash_million"));
    config.funded_at_close_provider_id = parse_text(
        required(raw, "funding_bridge.funded_at_close_provider_id"));
    config.funded_at_close_source_record_id = parse_text(
        required(raw, "funding_bridge.funded_at_close_source_record_id"));

    config.providers.reserve(provider_count);
    for (std::size_t provider = 0U; provider < provider_count; ++provider) {
        FundingProvider parsed_provider;
        parsed_provider.id =
            parse_text(required(raw, provider_key(provider, "id")));
        parsed_provider.economic_group_id = parse_text(
            required(raw, provider_key(provider, "economic_group_id")));
        parsed_provider.declared_capacity_million = parse_double(required(
            raw, provider_key(provider, "declared_capacity_million")));
        parsed_provider.identity_evidenced = parse_bool(required(
            raw, provider_key(provider, "identity_evidenced")));
        parsed_provider.affiliation_evidenced = parse_bool(required(
            raw, provider_key(provider, "affiliation_evidenced")));
        parsed_provider.capacity_evidenced = parse_bool(required(
            raw, provider_key(provider, "capacity_evidenced")));
        parsed_provider.source_record_id = parse_text(
            required(raw, provider_key(provider, "source_record_id")));
        config.providers.push_back(std::move(parsed_provider));
    }

    CallableCapitalFacility& callable = config.callable_facility;
    callable.id = parse_text(required(raw, "callable_facility.id"));
    callable.provider_id =
        parse_text(required(raw, "callable_facility.provider_id"));
    callable.source_record_id =
        parse_text(required(raw, "callable_facility.source_record_id"));
    callable.commitment_million =
        parse_double(required(raw, "callable_facility.commitment_million"));
    callable.availability_start_month = parse_size(
        required(raw, "callable_facility.availability_start_month"));
    callable.contractual_expiry_month = parse_size(
        required(raw, "callable_facility.contractual_expiry_month"));
    callable.settlement_lag_months = parse_size(
        required(raw, "callable_facility.settlement_lag_months"));
    callable.permitted_purpose =
        parse_text(required(raw, "callable_facility.permitted_purpose"));
    callable.annual_commitment_fee_rate = parse_double(
        required(raw, "callable_facility.annual_commitment_fee_rate"));
    callable.liquidity_reserve_fraction = parse_double(
        required(raw, "callable_facility.liquidity_reserve_fraction"));
    callable.annual_liquidity_hurdle_rate = parse_double(
        required(raw, "callable_facility.annual_liquidity_hurdle_rate"));
    callable.annual_reserve_yield_rate = parse_double(
        required(raw, "callable_facility.annual_reserve_yield_rate"));

    WarehouseFacility& warehouse = config.warehouse_facility;
    warehouse.id = parse_text(required(raw, "warehouse_facility.id"));
    warehouse.provider_id =
        parse_text(required(raw, "warehouse_facility.provider_id"));
    warehouse.source_record_id =
        parse_text(required(raw, "warehouse_facility.source_record_id"));
    warehouse.committed_limit_million = parse_double(
        required(raw, "warehouse_facility.committed_limit_million"));
    warehouse.availability_start_month = parse_size(
        required(raw, "warehouse_facility.availability_start_month"));
    warehouse.availability_end_month = parse_size(
        required(raw, "warehouse_facility.availability_end_month"));
    warehouse.legal_maturity_month = parse_size(
        required(raw, "warehouse_facility.legal_maturity_month"));
    warehouse.settlement_lag_months = parse_size(
        required(raw, "warehouse_facility.settlement_lag_months"));
    warehouse.permitted_purpose =
        parse_text(required(raw, "warehouse_facility.permitted_purpose"));
    warehouse.collateral_advance_rate = parse_double(
        required(raw, "warehouse_facility.collateral_advance_rate"));
    warehouse.annual_interest_rate = parse_double(
        required(raw, "warehouse_facility.annual_interest_rate"));
    warehouse.annual_undrawn_fee_rate = parse_double(
        required(raw, "warehouse_facility.annual_undrawn_fee_rate"));
    warehouse.advance_fee_rate = parse_double(
        required(raw, "warehouse_facility.advance_fee_rate"));
    warehouse.upfront_fee_rate = parse_double(
        required(raw, "warehouse_facility.upfront_fee_rate"));

    config.uncalled_commitment_is_not_cash_or_loss_absorption = parse_bool(
        required(raw,
            "funding_bridge.uncalled_commitment_is_not_cash_or_loss_absorption"));
    config.acquisition_and_primary_funding_uses_precede_same_month_receipts =
        parse_bool(required(raw,
            "funding_bridge.acquisition_and_primary_funding_uses_precede_same_month_receipts"));
    config.warehouse_is_external_temporary_debt = parse_bool(required(
        raw, "funding_bridge.warehouse_is_external_temporary_debt"));
    config.warehouse_proceeds_cannot_fund_interest_fees_or_costs = parse_bool(
        required(raw,
            "funding_bridge.warehouse_proceeds_cannot_fund_interest_fees_or_costs"));
    config.project_receipts_sweep_warehouse_principal_before_investor_cash =
        parse_bool(required(raw,
            "funding_bridge.project_receipts_sweep_warehouse_principal_before_investor_cash"));
    config.policy_uses_observed_history_only = parse_bool(required(
        raw, "funding_bridge.policy_uses_observed_history_only"));
    config.no_dynamic_tranche_allocation_or_pricing_is_claimed = parse_bool(
        required(raw,
            "funding_bridge.no_dynamic_tranche_allocation_or_pricing_is_claimed"));

    config.scenario_performance.reserve(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        FundingBridgeScenarioPerformance performance;
        performance.scenario_id =
            parse_text(required(raw, scenario_key(scenario, "id")));

        performance.capital_call_requests.reserve(
            shapes[scenario].capital_call_request_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].capital_call_request_count; ++record) {
            CapitalCallRequest request;
            request.id = parse_text(required(raw, scenario_record_key(
                scenario, "capital_call_request", record, "id")));
            request.facility_id = parse_text(required(raw,
                scenario_record_key(scenario, "capital_call_request", record,
                    "facility_id")));
            request.notice_month = parse_size(required(raw,
                scenario_record_key(scenario, "capital_call_request", record,
                    "notice_month")));
            request.requested_million = parse_double(required(raw,
                scenario_record_key(scenario, "capital_call_request", record,
                    "requested_million")));
            performance.capital_call_requests.push_back(std::move(request));
        }

        performance.capital_call_outcomes.reserve(
            shapes[scenario].capital_call_outcome_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].capital_call_outcome_count; ++record) {
            FundingSettlementOutcome outcome;
            outcome.request_id = parse_text(required(raw,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "request_id")));
            outcome.settlement_month = parse_size(required(raw,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "settlement_month")));
            outcome.status = parse_settlement_status(required(raw,
                scenario_record_key(
                    scenario, "capital_call_outcome", record, "status")));
            outcome.actual_cash_million = parse_double(required(raw,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "actual_cash_million")));
            outcome.source_record_id = parse_text(required(raw,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "source_record_id")));
            performance.capital_call_outcomes.push_back(std::move(outcome));
        }

        performance.warehouse_draw_requests.reserve(
            shapes[scenario].warehouse_draw_request_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].warehouse_draw_request_count; ++record) {
            WarehouseDrawRequest request;
            request.id = parse_text(required(raw,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "id")));
            request.facility_id = parse_text(required(raw,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "facility_id")));
            request.request_month = parse_size(required(raw,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "request_month")));
            request.requested_million = parse_double(required(raw,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "requested_million")));
            performance.warehouse_draw_requests.push_back(std::move(request));
        }

        performance.warehouse_draw_outcomes.reserve(
            shapes[scenario].warehouse_draw_outcome_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].warehouse_draw_outcome_count; ++record) {
            FundingSettlementOutcome outcome;
            outcome.request_id = parse_text(required(raw,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "request_id")));
            outcome.settlement_month = parse_size(required(raw,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "settlement_month")));
            outcome.status = parse_settlement_status(required(raw,
                scenario_record_key(
                    scenario, "warehouse_draw_outcome", record, "status")));
            outcome.actual_cash_million = parse_double(required(raw,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "actual_cash_million")));
            outcome.source_record_id = parse_text(required(raw,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "source_record_id")));
            performance.warehouse_draw_outcomes.push_back(std::move(outcome));
        }

        performance.supplemental_receipts.reserve(
            shapes[scenario].supplemental_receipt_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].supplemental_receipt_count; ++record) {
            SupplementalFundingReceipt receipt;
            receipt.id = parse_text(required(raw,
                scenario_record_key(
                    scenario, "supplemental_receipt", record, "id")));
            receipt.provider_id = parse_text(required(raw,
                scenario_record_key(scenario, "supplemental_receipt", record,
                    "provider_id")));
            receipt.month = parse_size(required(raw,
                scenario_record_key(
                    scenario, "supplemental_receipt", record, "month")));
            receipt.purpose = parse_supplemental_purpose(required(raw,
                scenario_record_key(
                    scenario, "supplemental_receipt", record, "purpose")));
            receipt.actual_cash_million = parse_double(required(raw,
                scenario_record_key(scenario, "supplemental_receipt", record,
                    "actual_cash_million")));
            receipt.source_record_id = parse_text(required(raw,
                scenario_record_key(scenario, "supplemental_receipt", record,
                    "source_record_id")));
            performance.supplemental_receipts.push_back(std::move(receipt));
        }

        performance.eligible_basis_movements.reserve(
            shapes[scenario].eligible_basis_movement_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].eligible_basis_movement_count; ++record) {
            EligibleBasisMovement movement;
            movement.id = parse_text(required(raw,
                scenario_record_key(
                    scenario, "eligible_basis_movement", record, "id")));
            movement.month = parse_size(required(raw,
                scenario_record_key(
                    scenario, "eligible_basis_movement", record, "month")));
            movement.kind = parse_basis_movement_kind(required(raw,
                scenario_record_key(
                    scenario, "eligible_basis_movement", record, "kind")));
            movement.amount_million = parse_double(required(raw,
                scenario_record_key(scenario, "eligible_basis_movement", record,
                    "amount_million")));
            movement.reference_id = parse_text(required(raw,
                scenario_record_key(scenario, "eligible_basis_movement", record,
                    "reference_id")));
            movement.source_record_id = parse_text(required(raw,
                scenario_record_key(scenario, "eligible_basis_movement", record,
                    "source_record_id")));
            performance.eligible_basis_movements.push_back(std::move(movement));
        }

        performance.protection_absorptions.reserve(
            shapes[scenario].protection_absorption_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].protection_absorption_count; ++record) {
            ProtectionAbsorption absorption;
            absorption.id = parse_text(required(raw,
                scenario_record_key(
                    scenario, "protection_absorption", record, "id")));
            absorption.month = parse_size(required(raw,
                scenario_record_key(
                    scenario, "protection_absorption", record, "month")));
            absorption.amount_million = parse_double(required(raw,
                scenario_record_key(scenario, "protection_absorption", record,
                    "amount_million")));
            absorption.reference_id = parse_text(required(raw,
                scenario_record_key(scenario, "protection_absorption", record,
                    "reference_id")));
            absorption.source_record_id = parse_text(required(raw,
                scenario_record_key(scenario, "protection_absorption", record,
                    "source_record_id")));
            performance.protection_absorptions.push_back(
                std::move(absorption));
        }

        performance.protection_releases.reserve(
            shapes[scenario].protection_release_count);
        for (std::size_t record = 0U;
             record < shapes[scenario].protection_release_count; ++record) {
            ProtectionRelease release;
            release.id = parse_text(required(raw,
                scenario_record_key(
                    scenario, "protection_release", record, "id")));
            release.month = parse_size(required(raw,
                scenario_record_key(
                    scenario, "protection_release", record, "month")));
            release.amount_million = parse_double(required(raw,
                scenario_record_key(scenario, "protection_release", record,
                    "amount_million")));
            release.source_record_id = parse_text(required(raw,
                scenario_record_key(scenario, "protection_release", record,
                    "source_record_id")));
            performance.protection_releases.push_back(std::move(release));
        }
        config.scenario_performance.push_back(std::move(performance));
    }

    validate_intrinsic_config(config);
    return config;
}

template <typename Value>
void print_key_value(
    std::ostream& output, std::string_view key, const Value& value) {
    output << key << '=' << value << '\n';
}

} // namespace

FundingBridgeConfig parse_funding_bridge_config(std::istream& input) {
    return parse_raw(read_raw(input));
}

FundingBridgeConfig load_funding_bridge_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "funding-bridge configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open funding-bridge configuration file: " +
            path.string());
    }
    try {
        return parse_funding_bridge_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading funding-bridge configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_funding_bridge_config(
    std::ostream& output, const FundingBridgeConfig& config) {
    validate_intrinsic_config(config);
    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;

    print_key_value(output, "funding_bridge.model_version",
        config.model_version);
    print_key_value(
        output, "funding_bridge.label", config.scenario_label);
    print_key_value(
        output, "funding_bridge.source_note", config.source_note);
    print_key_value(output, "funding_bridge.synthetic_inputs",
        config.synthetic_inputs);
    print_key_value(output, "funding_bridge.funded_at_close_cash_million",
        config.funded_at_close_cash_million);
    print_key_value(output, "funding_bridge.funded_at_close_provider_id",
        config.funded_at_close_provider_id);
    print_key_value(output,
        "funding_bridge.funded_at_close_source_record_id",
        config.funded_at_close_source_record_id);

    print_key_value(output, "provider.count", config.providers.size());
    for (std::size_t provider = 0U; provider < config.providers.size();
         ++provider) {
        const FundingProvider& value = config.providers[provider];
        print_key_value(output, provider_key(provider, "id"), value.id);
        print_key_value(output, provider_key(provider, "economic_group_id"),
            value.economic_group_id);
        print_key_value(output,
            provider_key(provider, "declared_capacity_million"),
            value.declared_capacity_million);
        print_key_value(output,
            provider_key(provider, "identity_evidenced"),
            value.identity_evidenced);
        print_key_value(output,
            provider_key(provider, "affiliation_evidenced"),
            value.affiliation_evidenced);
        print_key_value(output,
            provider_key(provider, "capacity_evidenced"),
            value.capacity_evidenced);
        print_key_value(output, provider_key(provider, "source_record_id"),
            value.source_record_id);
    }

    const CallableCapitalFacility& callable = config.callable_facility;
    print_key_value(output, "callable_facility.id", callable.id);
    print_key_value(
        output, "callable_facility.provider_id", callable.provider_id);
    print_key_value(output, "callable_facility.source_record_id",
        callable.source_record_id);
    print_key_value(output, "callable_facility.commitment_million",
        callable.commitment_million);
    print_key_value(output, "callable_facility.availability_start_month",
        callable.availability_start_month);
    print_key_value(output, "callable_facility.contractual_expiry_month",
        callable.contractual_expiry_month);
    print_key_value(output, "callable_facility.settlement_lag_months",
        callable.settlement_lag_months);
    print_key_value(output, "callable_facility.permitted_purpose",
        callable.permitted_purpose);
    print_key_value(output, "callable_facility.annual_commitment_fee_rate",
        callable.annual_commitment_fee_rate);
    print_key_value(output, "callable_facility.liquidity_reserve_fraction",
        callable.liquidity_reserve_fraction);
    print_key_value(output, "callable_facility.annual_liquidity_hurdle_rate",
        callable.annual_liquidity_hurdle_rate);
    print_key_value(output, "callable_facility.annual_reserve_yield_rate",
        callable.annual_reserve_yield_rate);

    const WarehouseFacility& warehouse = config.warehouse_facility;
    print_key_value(output, "warehouse_facility.id", warehouse.id);
    print_key_value(
        output, "warehouse_facility.provider_id", warehouse.provider_id);
    print_key_value(output, "warehouse_facility.source_record_id",
        warehouse.source_record_id);
    print_key_value(output, "warehouse_facility.committed_limit_million",
        warehouse.committed_limit_million);
    print_key_value(output, "warehouse_facility.availability_start_month",
        warehouse.availability_start_month);
    print_key_value(output, "warehouse_facility.availability_end_month",
        warehouse.availability_end_month);
    print_key_value(output, "warehouse_facility.legal_maturity_month",
        warehouse.legal_maturity_month);
    print_key_value(output, "warehouse_facility.settlement_lag_months",
        warehouse.settlement_lag_months);
    print_key_value(output, "warehouse_facility.permitted_purpose",
        warehouse.permitted_purpose);
    print_key_value(output, "warehouse_facility.collateral_advance_rate",
        warehouse.collateral_advance_rate);
    print_key_value(output, "warehouse_facility.annual_interest_rate",
        warehouse.annual_interest_rate);
    print_key_value(output, "warehouse_facility.annual_undrawn_fee_rate",
        warehouse.annual_undrawn_fee_rate);
    print_key_value(output, "warehouse_facility.advance_fee_rate",
        warehouse.advance_fee_rate);
    print_key_value(output, "warehouse_facility.upfront_fee_rate",
        warehouse.upfront_fee_rate);

    print_key_value(output,
        "funding_bridge.uncalled_commitment_is_not_cash_or_loss_absorption",
        config.uncalled_commitment_is_not_cash_or_loss_absorption);
    print_key_value(output,
        "funding_bridge.acquisition_and_primary_funding_uses_precede_same_month_receipts",
        config.acquisition_and_primary_funding_uses_precede_same_month_receipts);
    print_key_value(output,
        "funding_bridge.warehouse_is_external_temporary_debt",
        config.warehouse_is_external_temporary_debt);
    print_key_value(output,
        "funding_bridge.warehouse_proceeds_cannot_fund_interest_fees_or_costs",
        config.warehouse_proceeds_cannot_fund_interest_fees_or_costs);
    print_key_value(output,
        "funding_bridge.project_receipts_sweep_warehouse_principal_before_investor_cash",
        config.project_receipts_sweep_warehouse_principal_before_investor_cash);
    print_key_value(output,
        "funding_bridge.policy_uses_observed_history_only",
        config.policy_uses_observed_history_only);
    print_key_value(output,
        "funding_bridge.no_dynamic_tranche_allocation_or_pricing_is_claimed",
        config.no_dynamic_tranche_allocation_or_pricing_is_claimed);

    print_key_value(
        output, "scenario.count", config.scenario_performance.size());
    for (std::size_t scenario = 0U;
         scenario < config.scenario_performance.size(); ++scenario) {
        const FundingBridgeScenarioPerformance& performance =
            config.scenario_performance[scenario];
        print_key_value(output, scenario_key(scenario, "id"),
            performance.scenario_id);

        print_key_value(output,
            scenario_key(scenario, "capital_call_request.count"),
            performance.capital_call_requests.size());
        for (std::size_t record = 0U;
             record < performance.capital_call_requests.size(); ++record) {
            const CapitalCallRequest& request =
                performance.capital_call_requests[record];
            print_key_value(output, scenario_record_key(
                scenario, "capital_call_request", record, "id"), request.id);
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_request", record,
                    "facility_id"),
                request.facility_id);
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_request", record,
                    "notice_month"),
                request.notice_month);
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_request", record,
                    "requested_million"),
                request.requested_million);
        }

        print_key_value(output,
            scenario_key(scenario, "capital_call_outcome.count"),
            performance.capital_call_outcomes.size());
        for (std::size_t record = 0U;
             record < performance.capital_call_outcomes.size(); ++record) {
            const FundingSettlementOutcome& outcome =
                performance.capital_call_outcomes[record];
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "request_id"),
                outcome.request_id);
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "settlement_month"),
                outcome.settlement_month);
            print_key_value(output,
                scenario_record_key(
                    scenario, "capital_call_outcome", record, "status"),
                settlement_status_text(outcome.status));
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "actual_cash_million"),
                outcome.actual_cash_million);
            print_key_value(output,
                scenario_record_key(scenario, "capital_call_outcome", record,
                    "source_record_id"),
                outcome.source_record_id);
        }

        print_key_value(output,
            scenario_key(scenario, "warehouse_draw_request.count"),
            performance.warehouse_draw_requests.size());
        for (std::size_t record = 0U;
             record < performance.warehouse_draw_requests.size(); ++record) {
            const WarehouseDrawRequest& request =
                performance.warehouse_draw_requests[record];
            print_key_value(output,
                scenario_record_key(
                    scenario, "warehouse_draw_request", record, "id"),
                request.id);
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "facility_id"),
                request.facility_id);
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "request_month"),
                request.request_month);
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_request", record,
                    "requested_million"),
                request.requested_million);
        }

        print_key_value(output,
            scenario_key(scenario, "warehouse_draw_outcome.count"),
            performance.warehouse_draw_outcomes.size());
        for (std::size_t record = 0U;
             record < performance.warehouse_draw_outcomes.size(); ++record) {
            const FundingSettlementOutcome& outcome =
                performance.warehouse_draw_outcomes[record];
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "request_id"),
                outcome.request_id);
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "settlement_month"),
                outcome.settlement_month);
            print_key_value(output,
                scenario_record_key(
                    scenario, "warehouse_draw_outcome", record, "status"),
                settlement_status_text(outcome.status));
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "actual_cash_million"),
                outcome.actual_cash_million);
            print_key_value(output,
                scenario_record_key(scenario, "warehouse_draw_outcome", record,
                    "source_record_id"),
                outcome.source_record_id);
        }

        print_key_value(output,
            scenario_key(scenario, "supplemental_receipt.count"),
            performance.supplemental_receipts.size());
        for (std::size_t record = 0U;
             record < performance.supplemental_receipts.size(); ++record) {
            const SupplementalFundingReceipt& receipt =
                performance.supplemental_receipts[record];
            print_key_value(output,
                scenario_record_key(
                    scenario, "supplemental_receipt", record, "id"),
                receipt.id);
            print_key_value(output,
                scenario_record_key(scenario, "supplemental_receipt", record,
                    "provider_id"),
                receipt.provider_id);
            print_key_value(output,
                scenario_record_key(
                    scenario, "supplemental_receipt", record, "month"),
                receipt.month);
            print_key_value(output,
                scenario_record_key(
                    scenario, "supplemental_receipt", record, "purpose"),
                supplemental_purpose_text(receipt.purpose));
            print_key_value(output,
                scenario_record_key(scenario, "supplemental_receipt", record,
                    "actual_cash_million"),
                receipt.actual_cash_million);
            print_key_value(output,
                scenario_record_key(scenario, "supplemental_receipt", record,
                    "source_record_id"),
                receipt.source_record_id);
        }

        print_key_value(output,
            scenario_key(scenario, "eligible_basis_movement.count"),
            performance.eligible_basis_movements.size());
        for (std::size_t record = 0U;
             record < performance.eligible_basis_movements.size(); ++record) {
            const EligibleBasisMovement& movement =
                performance.eligible_basis_movements[record];
            print_key_value(output,
                scenario_record_key(
                    scenario, "eligible_basis_movement", record, "id"),
                movement.id);
            print_key_value(output,
                scenario_record_key(
                    scenario, "eligible_basis_movement", record, "month"),
                movement.month);
            print_key_value(output,
                scenario_record_key(
                    scenario, "eligible_basis_movement", record, "kind"),
                basis_movement_kind_text(movement.kind));
            print_key_value(output,
                scenario_record_key(scenario, "eligible_basis_movement", record,
                    "amount_million"),
                movement.amount_million);
            print_key_value(output,
                scenario_record_key(scenario, "eligible_basis_movement", record,
                    "reference_id"),
                movement.reference_id);
            print_key_value(output,
                scenario_record_key(scenario, "eligible_basis_movement", record,
                    "source_record_id"),
                movement.source_record_id);
        }

        print_key_value(output,
            scenario_key(scenario, "protection_absorption.count"),
            performance.protection_absorptions.size());
        for (std::size_t record = 0U;
             record < performance.protection_absorptions.size(); ++record) {
            const ProtectionAbsorption& absorption =
                performance.protection_absorptions[record];
            print_key_value(output,
                scenario_record_key(
                    scenario, "protection_absorption", record, "id"),
                absorption.id);
            print_key_value(output,
                scenario_record_key(
                    scenario, "protection_absorption", record, "month"),
                absorption.month);
            print_key_value(output,
                scenario_record_key(scenario, "protection_absorption", record,
                    "amount_million"),
                absorption.amount_million);
            print_key_value(output,
                scenario_record_key(scenario, "protection_absorption", record,
                    "reference_id"),
                absorption.reference_id);
            print_key_value(output,
                scenario_record_key(scenario, "protection_absorption", record,
                    "source_record_id"),
                absorption.source_record_id);
        }

        print_key_value(output,
            scenario_key(scenario, "protection_release.count"),
            performance.protection_releases.size());
        for (std::size_t record = 0U;
             record < performance.protection_releases.size(); ++record) {
            const ProtectionRelease& release =
                performance.protection_releases[record];
            print_key_value(output,
                scenario_record_key(
                    scenario, "protection_release", record, "id"),
                release.id);
            print_key_value(output,
                scenario_record_key(
                    scenario, "protection_release", record, "month"),
                release.month);
            print_key_value(output,
                scenario_record_key(scenario, "protection_release", record,
                    "amount_million"),
                release.amount_million);
            print_key_value(output,
                scenario_record_key(scenario, "protection_release", record,
                    "source_record_id"),
                release.source_record_id);
        }
    }
}

} // namespace naturalehia::cellular_finance
