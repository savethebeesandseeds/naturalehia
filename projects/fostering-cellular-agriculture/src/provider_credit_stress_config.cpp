// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_credit_stress_config.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
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
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumParsedScenarios = 10'000U;
constexpr std::size_t kMaximumOutcomesPerScenario = 1'000U;
constexpr std::size_t kMaximumParsedOutcomes = 100'000U;
constexpr std::size_t kMaximumRecoveryDelayMonths = 1'200U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::uintmax_t kMaximumConfigBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaximumConfigLineBytes = 4'096U;
constexpr long double kWeightSumTolerance = 1.0e-12L;
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
            // Formatting restoration must not throw while unwinding.
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

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
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
        "provider-credit-stress configuration line " +
        std::to_string(line) + ": " + std::string(message));
}

[[nodiscard]] const RawValue& required(
    const RawMap& raw, const std::string& key) {
    const auto iterator = raw.find(key);
    if (iterator == raw.end()) {
        throw std::invalid_argument(
            "provider-credit-stress configuration is missing required key: " +
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

[[nodiscard]] std::string parse_text(const RawValue& raw) {
    if (raw.had_surrounding_whitespace) {
        parse_error(raw.line,
            "text values must not begin or end with whitespace");
    }
    return raw.value;
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
            std::string(description) + " must be explicitly true in v0.1");
    }
}

void require_false(bool value, std::string_view description) {
    if (value) {
        throw std::invalid_argument(
            std::string(description) + " must be explicitly false in v0.1");
    }
}

[[nodiscard]] std::string scenario_key(
    std::size_t scenario, std::string_view field) {
    return "provider_credit.scenario." + std::to_string(scenario + 1U) +
        "." + std::string(field);
}

[[nodiscard]] std::string state_key(std::size_t scenario,
    std::size_t state, std::string_view field) {
    return "provider_credit.scenario." + std::to_string(scenario + 1U) +
        ".state." + std::to_string(state + 1U) + "." +
        std::string(field);
}

[[nodiscard]] long double conditional_weight_sum(
    const ProviderCreditScenarioConfig& scenario) noexcept {
    CompensatedSum sum;
    for (const ProviderCreditOutcomeConfig& outcome : scenario.outcomes) {
        sum.add(static_cast<long double>(outcome.conditional_weight));
    }
    return sum.value();
}

void normalize_conditional_weights(ProviderCreditStressConfig& config) {
    for (ProviderCreditScenarioConfig& scenario : config.scenarios) {
        const long double configured_sum = conditional_weight_sum(scenario);
        std::size_t largest_index = 0U;
        double normalized_sum = 0.0;
        for (std::size_t index = 0U; index < scenario.outcomes.size();
             ++index) {
            ProviderCreditOutcomeConfig& outcome = scenario.outcomes[index];
            outcome.conditional_weight = static_cast<double>(
                static_cast<long double>(outcome.conditional_weight) /
                configured_sum);
            normalized_sum += outcome.conditional_weight;
            if (outcome.conditional_weight >
                scenario.outcomes[largest_index].conditional_weight) {
                largest_index = index;
            }
        }
        // Assign the final floating-point residual to the largest atom. This
        // keeps every strictly positive small atom away from a zero boundary.
        scenario.outcomes[largest_index].conditional_weight +=
            1.0 - normalized_sum;
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
            "failed while reading provider-credit-stress configuration");
    }
    return raw;
}

[[nodiscard]] ProviderCreditStressConfig parse_raw(const RawMap& raw) {
    const std::size_t scenario_count =
        parse_size(required(raw, "provider_credit.scenario.count"));
    if (scenario_count == 0U ||
        scenario_count > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "provider-credit scenario count must be between one and 10000");
    }

    std::unordered_set<std::string> expected{
        "provider_credit.model_version",
        "provider_credit.label",
        "provider_credit.source_note",
        "provider_credit.provider_id",
        "provider_credit.synthetic_inputs",
        "provider_credit.gross_contractual_claim_remains_unchanged",
        "provider_credit.provider_price_remains_full_performance_and_unchanged",
        "provider_credit.conditional_provider_state_weights_are_fixed_physical",
        "provider_credit.price_ladder_collateral_is_pledged_to_investor",
        "provider_credit.collateral_yield_remains_in_pledged_account",
        "provider_credit.collateral_applies_before_unsecured_recovery",
        "provider_credit.provider_default_occurs_at_claim_settlement",
        "provider_credit.provider_default_is_physical_stress_not_pricing_measure",
        "provider_credit.legal_enforceability_is_validated",
        "provider_credit.market_cva_or_fair_value_is_claimed",
        "provider_credit.scenario.count",
    };

    std::vector<std::size_t> outcome_counts(scenario_count, 0U);
    std::size_t total_outcomes = 0U;
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        const std::string id_key = scenario_key(scenario, "id");
        const std::string count_key = scenario_key(scenario, "state.count");
        expected.insert(id_key);
        expected.insert(count_key);
        const std::size_t outcome_count =
            parse_size(required(raw, count_key));
        if (outcome_count == 0U ||
            outcome_count > kMaximumOutcomesPerScenario) {
            throw std::invalid_argument(
                "provider-credit state count must be between one and 1000");
        }
        if (total_outcomes > kMaximumParsedOutcomes - outcome_count) {
            throw std::invalid_argument(
                "provider-credit configuration exceeds the 100000-state guardrail");
        }
        total_outcomes += outcome_count;
        outcome_counts[scenario] = outcome_count;
        for (std::size_t state = 0U; state < outcome_count; ++state) {
            expected.insert(state_key(scenario, state, "id"));
            expected.insert(
                state_key(scenario, state, "conditional_weight"));
            expected.insert(
                state_key(scenario, state, "provider_performs"));
            expected.insert(state_key(
                scenario, state, "collateral_realization_fraction"));
            expected.insert(state_key(
                scenario, state, "unsecured_recovery_fraction"));
            expected.insert(state_key(
                scenario, state, "unsecured_recovery_delay_months"));
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

    ProviderCreditStressConfig config;
    config.model_version =
        required(raw, "provider_credit.model_version").value;
    config.scenario_label =
        parse_text(required(raw, "provider_credit.label"));
    config.source_note =
        parse_text(required(raw, "provider_credit.source_note"));
    config.provider_id = required(raw, "provider_credit.provider_id").value;
    config.synthetic_inputs =
        parse_bool(required(raw, "provider_credit.synthetic_inputs"));
    config.gross_contractual_claim_remains_unchanged = parse_bool(required(
        raw, "provider_credit.gross_contractual_claim_remains_unchanged"));
    config.provider_price_remains_full_performance_and_unchanged = parse_bool(
        required(raw,
            "provider_credit.provider_price_remains_full_performance_and_unchanged"));
    config.conditional_provider_state_weights_are_fixed_physical = parse_bool(
        required(raw,
            "provider_credit.conditional_provider_state_weights_are_fixed_physical"));
    config.price_ladder_collateral_is_pledged_to_investor = parse_bool(
        required(raw,
            "provider_credit.price_ladder_collateral_is_pledged_to_investor"));
    config.collateral_yield_remains_in_pledged_account = parse_bool(required(
        raw,
        "provider_credit.collateral_yield_remains_in_pledged_account"));
    config.collateral_applies_before_unsecured_recovery = parse_bool(required(
        raw,
        "provider_credit.collateral_applies_before_unsecured_recovery"));
    config.provider_default_occurs_at_claim_settlement = parse_bool(required(
        raw,
        "provider_credit.provider_default_occurs_at_claim_settlement"));
    config.provider_default_is_physical_stress_not_pricing_measure =
        parse_bool(required(raw,
            "provider_credit.provider_default_is_physical_stress_not_pricing_measure"));
    config.legal_enforceability_is_validated = parse_bool(required(
        raw, "provider_credit.legal_enforceability_is_validated"));
    config.market_cva_or_fair_value_is_claimed = parse_bool(required(
        raw, "provider_credit.market_cva_or_fair_value_is_claimed"));

    config.scenarios.resize(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        ProviderCreditScenarioConfig& scenario_config =
            config.scenarios[scenario];
        scenario_config.scenario_id =
            required(raw, scenario_key(scenario, "id")).value;
        scenario_config.outcomes.resize(outcome_counts[scenario]);
        for (std::size_t state = 0U; state < outcome_counts[scenario];
             ++state) {
            ProviderCreditOutcomeConfig& outcome =
                scenario_config.outcomes[state];
            outcome.outcome_id =
                required(raw, state_key(scenario, state, "id")).value;
            outcome.conditional_weight = parse_double(required(
                raw, state_key(scenario, state, "conditional_weight")));
            outcome.provider_performs = parse_bool(required(
                raw, state_key(scenario, state, "provider_performs")));
            outcome.collateral_realization_fraction = parse_double(required(
                raw, state_key(scenario, state,
                         "collateral_realization_fraction")));
            outcome.unsecured_recovery_fraction = parse_double(required(
                raw,
                state_key(scenario, state, "unsecured_recovery_fraction")));
            outcome.unsecured_recovery_delay_months = parse_size(required(
                raw, state_key(scenario, state,
                         "unsecured_recovery_delay_months")));
        }
    }

    validate_provider_credit_stress_config(config);
    normalize_conditional_weights(config);
    validate_provider_credit_stress_config(config);
    return config;
}

} // namespace

void validate_provider_credit_stress_config(
    const ProviderCreditStressConfig& config) {
    if (config.model_version != kProviderCreditStressModelVersion) {
        throw std::invalid_argument(
            "provider-credit-stress model_version does not match this engine");
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "provider-credit-stress v0.1 accepts synthetic inputs only");
    }
    require_safe_text(config.scenario_label, "provider-credit label");
    require_safe_text(config.source_note, "provider-credit source_note");
    require_safe_identifier(config.provider_id, "provider-credit provider_id");

    require_true(config.gross_contractual_claim_remains_unchanged,
        "gross-contractual-claim preservation assertion");
    require_true(
        config.provider_price_remains_full_performance_and_unchanged,
        "full-performance unchanged provider-price assertion");
    require_true(
        config.conditional_provider_state_weights_are_fixed_physical,
        "fixed physical conditional-state-weight assertion");
    if (config.price_ladder_collateral_is_pledged_to_investor !=
        config.collateral_yield_remains_in_pledged_account) {
        throw std::invalid_argument(
            "provider-credit v0.1 requires retained collateral yield exactly when price-ladder collateral is pledged to the investor");
    }
    require_true(config.collateral_applies_before_unsecured_recovery,
        "collateral-before-unsecured-recovery assertion");
    require_true(config.provider_default_occurs_at_claim_settlement,
        "claim-settlement provider-default timing assertion");
    require_true(
        config.provider_default_is_physical_stress_not_pricing_measure,
        "physical-stress-not-pricing-measure assertion");
    require_false(config.legal_enforceability_is_validated,
        "legal-enforceability validation claim");
    require_false(config.market_cva_or_fair_value_is_claimed,
        "market-CVA-or-fair-value claim");

    if (config.scenarios.empty() ||
        config.scenarios.size() > kMaximumParsedScenarios) {
        throw std::invalid_argument(
            "provider-credit scenarios must be non-empty and bounded");
    }

    std::unordered_set<std::string> scenario_ids;
    scenario_ids.reserve(config.scenarios.size());
    std::size_t total_outcomes = 0U;
    for (const ProviderCreditScenarioConfig& scenario : config.scenarios) {
        require_safe_identifier(
            scenario.scenario_id, "provider-credit scenario id");
        if (!scenario_ids.insert(scenario.scenario_id).second) {
            throw std::invalid_argument(
                "provider-credit scenario ids must be unique");
        }
        if (scenario.outcomes.empty() ||
            scenario.outcomes.size() > kMaximumOutcomesPerScenario) {
            throw std::invalid_argument(
                "provider-credit scenario states must be non-empty and bounded");
        }
        if (total_outcomes >
            kMaximumParsedOutcomes - scenario.outcomes.size()) {
            throw std::invalid_argument(
                "provider-credit configuration exceeds the state guardrail");
        }
        total_outcomes += scenario.outcomes.size();

        std::unordered_set<std::string> outcome_ids;
        outcome_ids.reserve(scenario.outcomes.size());
        for (const ProviderCreditOutcomeConfig& outcome : scenario.outcomes) {
            require_safe_identifier(
                outcome.outcome_id, "provider-credit state id");
            if (!outcome_ids.insert(outcome.outcome_id).second) {
                throw std::invalid_argument(
                    "provider-credit state ids must be unique within each scenario");
            }
            if (!std::isfinite(outcome.conditional_weight) ||
                outcome.conditional_weight <= 0.0 ||
                outcome.conditional_weight > 1.0) {
                throw std::invalid_argument(
                    "provider-credit conditional weights must be finite and within (0, 1]");
            }
            for (const double fraction : {
                     outcome.collateral_realization_fraction,
                     outcome.unsecured_recovery_fraction}) {
                if (!std::isfinite(fraction) || fraction < 0.0 ||
                    fraction > 1.0 ||
                    (fraction == 0.0 && std::signbit(fraction))) {
                    throw std::invalid_argument(
                        "provider-credit collateral and recovery fractions must be finite canonical values within [0, 1]");
                }
            }
            if (!config.price_ladder_collateral_is_pledged_to_investor &&
                outcome.collateral_realization_fraction != 0.0) {
                throw std::invalid_argument(
                    "unpledged price-ladder collateral cannot create investor collateral realization benefit");
            }
            if (outcome.unsecured_recovery_delay_months >
                kMaximumRecoveryDelayMonths) {
                throw std::invalid_argument(
                    "provider-credit unsecured recovery delay exceeds 1200 months");
            }
            if (outcome.provider_performs &&
                (outcome.collateral_realization_fraction != 0.0 ||
                    outcome.unsecured_recovery_fraction != 0.0 ||
                    outcome.unsecured_recovery_delay_months != 0U ||
                    std::signbit(outcome.collateral_realization_fraction) ||
                    std::signbit(outcome.unsecured_recovery_fraction))) {
                throw std::invalid_argument(
                    "performing provider states require canonical zero collateral, recovery, and delay fields");
            }
        }

        const long double weight_sum = conditional_weight_sum(scenario);
        if (std::abs(weight_sum - 1.0L) > kWeightSumTolerance) {
            throw std::invalid_argument(
                "provider-credit conditional weights must sum to one within tolerance in every scenario");
        }
    }
}

ProviderCreditStressConfig parse_provider_credit_stress_config(
    std::istream& input) {
    return parse_raw(read_raw(input));
}

ProviderCreditStressConfig load_provider_credit_stress_config(
    const std::filesystem::path& path) {
    std::error_code size_error;
    const std::uintmax_t file_bytes =
        std::filesystem::file_size(path, size_error);
    if (!size_error && file_bytes > kMaximumConfigBytes) {
        throw std::invalid_argument(
            "provider-credit-stress configuration exceeds the 16 MiB guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open provider-credit-stress configuration file: " +
            path.string());
    }
    try {
        return parse_provider_credit_stress_config(input);
    } catch (const std::runtime_error&) {
        if (!input.eof()) {
            throw std::runtime_error(
                "failed while reading provider-credit-stress configuration file: " +
                path.string());
        }
        throw;
    }
}

void print_normalized_provider_credit_stress_config(
    std::ostream& output, const ProviderCreditStressConfig& config) {
    validate_provider_credit_stress_config(config);
    ProviderCreditStressConfig normalized = config;
    normalize_conditional_weights(normalized);
    validate_provider_credit_stress_config(normalized);

    const OutputStateGuard output_state{output};
    output.imbue(std::locale::classic());
    output.width(0);
    output.fill(' ');
    output << std::defaultfloat << std::dec << std::noshowbase
           << std::noshowpoint << std::noshowpos << std::nouppercase
           << std::setprecision(std::numeric_limits<double>::max_digits10)
           << std::boolalpha;

    output << "provider_credit.model_version=" << normalized.model_version
           << '\n';
    output << "provider_credit.label=" << normalized.scenario_label << '\n';
    output << "provider_credit.source_note=" << normalized.source_note
           << '\n';
    output << "provider_credit.provider_id=" << normalized.provider_id
           << '\n';
    output << "provider_credit.synthetic_inputs="
           << normalized.synthetic_inputs << '\n';
    output << "provider_credit.gross_contractual_claim_remains_unchanged="
           << normalized.gross_contractual_claim_remains_unchanged << '\n';
    output
        << "provider_credit.provider_price_remains_full_performance_and_unchanged="
        << normalized.provider_price_remains_full_performance_and_unchanged
        << '\n';
    output
        << "provider_credit.conditional_provider_state_weights_are_fixed_physical="
        << normalized.conditional_provider_state_weights_are_fixed_physical
        << '\n';
    output << "provider_credit.price_ladder_collateral_is_pledged_to_investor="
           << normalized.price_ladder_collateral_is_pledged_to_investor
           << '\n';
    output << "provider_credit.collateral_yield_remains_in_pledged_account="
           << normalized.collateral_yield_remains_in_pledged_account << '\n';
    output << "provider_credit.collateral_applies_before_unsecured_recovery="
           << normalized.collateral_applies_before_unsecured_recovery << '\n';
    output << "provider_credit.provider_default_occurs_at_claim_settlement="
           << normalized.provider_default_occurs_at_claim_settlement << '\n';
    output
        << "provider_credit.provider_default_is_physical_stress_not_pricing_measure="
        << normalized.provider_default_is_physical_stress_not_pricing_measure
        << '\n';
    output << "provider_credit.legal_enforceability_is_validated="
           << normalized.legal_enforceability_is_validated << '\n';
    output << "provider_credit.market_cva_or_fair_value_is_claimed="
           << normalized.market_cva_or_fair_value_is_claimed << '\n';
    output << "provider_credit.scenario.count=" << normalized.scenarios.size()
           << '\n';

    for (std::size_t scenario = 0U; scenario < normalized.scenarios.size();
         ++scenario) {
        const ProviderCreditScenarioConfig& scenario_config =
            normalized.scenarios[scenario];
        output << scenario_key(scenario, "id") << '='
               << scenario_config.scenario_id << '\n';
        output << scenario_key(scenario, "state.count") << '='
               << scenario_config.outcomes.size() << '\n';
        for (std::size_t state = 0U;
             state < scenario_config.outcomes.size(); ++state) {
            const ProviderCreditOutcomeConfig& outcome =
                scenario_config.outcomes[state];
            output << state_key(scenario, state, "id") << '='
                   << outcome.outcome_id << '\n';
            output << state_key(scenario, state, "conditional_weight") << '='
                   << outcome.conditional_weight << '\n';
            output << state_key(scenario, state, "provider_performs") << '='
                   << outcome.provider_performs << '\n';
            output << state_key(
                          scenario, state, "collateral_realization_fraction")
                   << '=' << outcome.collateral_realization_fraction << '\n';
            output << state_key(
                          scenario, state, "unsecured_recovery_fraction")
                   << '=' << outcome.unsecured_recovery_fraction << '\n';
            output << state_key(
                          scenario, state, "unsecured_recovery_delay_months")
                   << '=' << outcome.unsecured_recovery_delay_months << '\n';
        }
    }
}

} // namespace naturalehia::cellular_finance
