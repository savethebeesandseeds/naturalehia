// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_hurdle_envelope.hpp>

#include "robust_two_claim_grid_work.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kMaximumAnnualEffectiveDomainRate = 10.0;
constexpr double kMaximumMoneyMillion = 1.0e9;
constexpr double kMaximumAbsoluteLogGrossDelta = 16.0;
constexpr double kComparisonAbsoluteTolerance = 1.0e-10;
constexpr std::string_view kAbsent{"none"};
constexpr std::string_view kWorkLimitMessage{
    "robust hurdle-envelope structural work exceeds the 4,000,000-unit resource bound"};

struct EligibleInterval {
    double lower{0.0};
    double upper{0.0};
    std::size_t observation_index{0U};
    std::string cluster_id{};
};

struct Event {
    double value{0.0};
    std::size_t starts{0U};
    std::size_t ends{0U};
};

[[nodiscard]] bool ascii_alphanumeric(char value) noexcept {
    return (value >= 'a' && value <= 'z') ||
        (value >= 'A' && value <= 'Z') ||
        (value >= '0' && value <= '9');
}

[[nodiscard]] bool safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!safe_identifier(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a safe bounded identifier");
    }
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength ||
        value.front() == ' ' || value.back() == ' ') {
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

[[nodiscard]] bool placeholder(std::string_view value) noexcept {
    return value == kAbsent || value.starts_with("unnamed-") ||
        value.starts_with("unvalidated ");
}

[[nodiscard]] bool iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4] != '-' || value[7] != '-') {
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
    const unsigned year = static_cast<unsigned>(value[0] - '0') * 1'000U +
        static_cast<unsigned>(value[1] - '0') * 100U +
        static_cast<unsigned>(value[2] - '0') * 10U +
        static_cast<unsigned>(value[3] - '0');
    const unsigned month = static_cast<unsigned>(value[5] - '0') * 10U +
        static_cast<unsigned>(value[6] - '0');
    const unsigned day = static_cast<unsigned>(value[8] - '0') * 10U +
        static_cast<unsigned>(value[9] - '0');
    if (year == 0U || month == 0U || month > 12U || day == 0U) {
        return false;
    }
    constexpr std::array<unsigned, 12U> days_by_month{
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    unsigned maximum_day = days_by_month[month - 1U];
    if (month == 2U && year % 4U == 0U &&
        (year % 100U != 0U || year % 400U == 0U)) {
        maximum_day = 29U;
    }
    return day <= maximum_day;
}

[[nodiscard]] bool utc_timestamp(std::string_view value) noexcept {
    if (value.size() != 20U || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || value[19] != 'Z' ||
        !iso_date(value.substr(0U, 10U))) {
        return false;
    }
    const auto digit = [value](std::size_t index) {
        return value[index] >= '0' && value[index] <= '9';
    };
    if (!digit(11U) || !digit(12U) || !digit(14U) || !digit(15U) ||
        !digit(17U) || !digit(18U)) {
        return false;
    }
    const unsigned hour = static_cast<unsigned>(value[11] - '0') * 10U +
        static_cast<unsigned>(value[12] - '0');
    const unsigned minute = static_cast<unsigned>(value[14] - '0') * 10U +
        static_cast<unsigned>(value[15] - '0');
    const unsigned second = static_cast<unsigned>(value[17] - '0') * 10U +
        static_cast<unsigned>(value[18] - '0');
    return hour < 24U && minute < 60U && second < 60U;
}

[[nodiscard]] bool sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
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

void require_money(double value, std::string_view description) {
    require_non_negative(value, description);
    if (value > kMaximumMoneyMillion) {
        throw std::invalid_argument(
            std::string(description) + " exceeds the model money guardrail");
    }
}

void require_rate(double value, std::string_view description) {
    require_finite(value, description);
    if (!(value > -1.0) || (value == 0.0 && std::signbit(value))) {
        throw std::invalid_argument(std::string(description) +
            " must exceed -1 and must not be negative zero");
    }
}

void require_log_delta(double value, std::string_view description) {
    require_finite(value, description);
    if (std::abs(value) > kMaximumAbsoluteLogGrossDelta ||
        (value == 0.0 && std::signbit(value))) {
        throw std::invalid_argument(std::string(description) +
            " must lie in [-16,16] and must not be negative zero");
    }
}

[[nodiscard]] double comparison_tolerance(double first, double second) {
    const double scale = std::max({1.0, std::abs(first), std::abs(second)});
    return kComparisonAbsoluteTolerance +
        256.0 * std::numeric_limits<double>::epsilon() * scale;
}

[[nodiscard]] bool nearly_equal(double first, double second) {
    return std::abs(first - second) <= comparison_tolerance(first, second);
}

[[nodiscard]] bool valid_status(RobustHurdleObservationStatus value) {
    switch (value) {
    case RobustHurdleObservationStatus::
        SettledOrderlyArmsLengthTransaction:
    case RobustHurdleObservationStatus::ExecutableTwoSidedQuote:
    case RobustHurdleObservationStatus::NonbindingIndication:
    case RobustHurdleObservationStatus::ModelMark:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_transaction_type(
    RobustHurdleObservationTransactionType value) {
    switch (value) {
    case RobustHurdleObservationTransactionType::PrimaryIssuance:
    case RobustHurdleObservationTransactionType::SecondaryTrade:
    case RobustHurdleObservationTransactionType::TwoSidedMarketQuote:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_return_basis(
    RobustHurdleObservationReturnBasis value) {
    switch (value) {
    case RobustHurdleObservationReturnBasis::
        AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows:
    case RobustHurdleObservationReturnBasis::GrossPriceExBuyerCostDiscountRate:
    case RobustHurdleObservationReturnBasis::PromisedYieldToMaturity:
    case RobustHurdleObservationReturnBasis::CouponRate:
    case RobustHurdleObservationReturnBasis::InternalRateOfReturn:
    case RobustHurdleObservationReturnBasis::TargetPriceImpliedDiscountRate:
    case RobustHurdleObservationReturnBasis::OtherOrUnresolved:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_claim_relation(
    RobustHurdleObservationClaimRelation value) {
    switch (value) {
    case RobustHurdleObservationClaimRelation::SameTargetClaim:
    case RobustHurdleObservationClaimRelation::ComparableClaim:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_axis(RobustHurdleComparabilityAxis value) {
    return robust_hurdle_axis_index(value) <
        kRobustHurdleEnvelopeComparabilityAxisCount;
}

[[nodiscard]] bool valid_treatment(
    RobustHurdleComparabilityTreatment value) {
    switch (value) {
    case RobustHurdleComparabilityTreatment::Matched:
    case RobustHurdleComparabilityTreatment::
        BoundedLogGrossReturnAdjustment:
    case RobustHurdleComparabilityTreatment::Unresolved:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_operator(RobustHurdleExpectedCashOperator value) {
    switch (value) {
    case RobustHurdleExpectedCashOperator::
        RobustMinimumOverDeclaredProbabilitySet:
    case RobustHurdleExpectedCashOperator::FixedDeclaredProbabilityVector:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_adjustment_basis(
    RobustHurdleAdjustmentSetBasis value) {
    switch (value) {
    case RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval:
    case RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_reference_relation(
    RobustIssuePriceHurdleReferenceRelation value) {
    switch (value) {
    case RobustIssuePriceHurdleReferenceRelation::Independent:
    case RobustIssuePriceHurdleReferenceRelation::
        ModelImpliedFromReferencePrice:
    case RobustIssuePriceHurdleReferenceRelation::Unresolved:
        return true;
    }
    return false;
}

void validate_adjustment(
    const RobustHurdleLogGrossReturnAdjustmentConfig& adjustment) {
    require_safe_identifier(adjustment.adjustment_id, "adjustment id");
    if (!valid_axis(adjustment.axis)) {
        throw std::invalid_argument("unknown hurdle comparability axis");
    }
    require_log_delta(adjustment.lower_log_gross_return_delta,
        "lower log-gross-return adjustment");
    require_log_delta(adjustment.upper_log_gross_return_delta,
        "upper log-gross-return adjustment");
    if (adjustment.lower_log_gross_return_delta >
        adjustment.upper_log_gross_return_delta) {
        throw std::invalid_argument(
            "log-gross-return adjustment lower bound exceeds upper bound");
    }
    require_safe_identifier(adjustment.method_id, "adjustment method id");
    require_safe_text(adjustment.source_reference,
        "adjustment source reference");
    require_safe_identifier(
        adjustment.evidence_record_id, "adjustment evidence record id");
    require_safe_text(adjustment.source_note, "adjustment source note");
}

void validate_observation(
    const RobustHurdleMarketObservationConfig& observation) {
    require_safe_identifier(observation.record_id, "observation record id");
    require_safe_identifier(observation.economic_observation_cluster_id,
        "economic-observation cluster id");
    if (!valid_status(observation.status) ||
        !valid_transaction_type(observation.transaction_type) ||
        !valid_return_basis(observation.return_basis) ||
        !valid_claim_relation(observation.claim_relation) ||
        !valid_operator(observation.expected_cash_operator) ||
        !valid_adjustment_basis(observation.adjustment_set_basis) ||
        !valid_reference_relation(observation.target_reference_price_relation)) {
        throw std::invalid_argument(
            "hurdle observation contains an unknown enum value");
    }
    for (const auto treatment : observation.comparability_treatments) {
        if (!valid_treatment(treatment)) {
            throw std::invalid_argument(
                "hurdle observation contains an unknown comparability treatment");
        }
    }
    if (!iso_date(observation.observation_date)) {
        throw std::invalid_argument(
            "hurdle observation date must be a calendar date in YYYY-MM-DD");
    }
    require_safe_text(observation.execution_date,
        "observation execution date");
    require_safe_text(observation.settlement_date,
        "observation settlement date");
    require_safe_text(observation.source_reference,
        "observation source reference");
    require_safe_identifier(
        observation.evidence_record_id, "observation evidence record id");
    require_safe_identifier(observation.settlement_evidence_record_id,
        "observation settlement evidence record id");
    require_safe_identifier(observation.orderly_arms_length_evidence_record_id,
        "orderly arm's-length evidence record id");
    require_safe_identifier(observation.transaction_market_anchor_id,
        "transaction-market anchor id");

    const bool settled = observation.status ==
        RobustHurdleObservationStatus::
            SettledOrderlyArmsLengthTransaction;
    const bool quote = observation.status ==
        RobustHurdleObservationStatus::ExecutableTwoSidedQuote;
    if (settled) {
        if (observation.transaction_type ==
            RobustHurdleObservationTransactionType::TwoSidedMarketQuote) {
            throw std::invalid_argument(
                "a settled observation cannot have the quote transaction type");
        }
        if (!iso_date(observation.execution_date) ||
            !iso_date(observation.settlement_date) ||
            observation.settlement_date < observation.execution_date ||
            observation.observation_date != observation.execution_date) {
            throw std::invalid_argument(
                "settled observation dates must be valid, ordered, and observed on execution date");
        }
    } else if (observation.execution_date != kAbsent ||
        observation.settlement_date != kAbsent) {
        throw std::invalid_argument(
            "non-settled observations must not carry execution or settlement dates");
    }
    if (!settled &&
        (observation.transaction_execution_is_evidenced ||
            observation.buyer_cash_payment_is_evidenced ||
            observation.transaction_settlement_is_evidenced ||
            observation.orderly_transaction_is_evidenced ||
            observation.arms_length_transaction_is_evidenced ||
            observation.forced_or_distressed_transaction ||
            observation.related_party_transaction ||
            observation.settled_claim_quantity_million != 0.0 ||
            observation.settlement_evidence_record_id != kAbsent ||
            observation.orderly_arms_length_evidence_record_id != kAbsent ||
            observation.settled_price_preimage_is_single_connected_interval)) {
        throw std::invalid_argument(
            "non-settled observations must not carry settled-transaction evidence fields");
    }
    if (quote) {
        if (observation.transaction_type !=
            RobustHurdleObservationTransactionType::TwoSidedMarketQuote) {
            throw std::invalid_argument(
                "an executable quote requires the two-sided-quote transaction type");
        }
    } else if (observation.transaction_type ==
        RobustHurdleObservationTransactionType::TwoSidedMarketQuote) {
        throw std::invalid_argument(
            "the two-sided-quote transaction type requires executable-quote status");
    }

    require_safe_identifier(
        observation.observed_market_claim_id, "observed market claim id");
    require_safe_identifier(observation.normalized_term_result_id,
        "normalized term result id");
    require_safe_text(observation.currency_label, "observation currency");
    require_safe_text(
        observation.monetary_basis, "observation monetary basis");
    require_rate(observation.annual_effective_hurdle_lower,
        "observation hurdle lower bound");
    require_rate(observation.annual_effective_hurdle_upper,
        "observation hurdle upper bound");
    if (observation.annual_effective_hurdle_lower >
        observation.annual_effective_hurdle_upper) {
        throw std::invalid_argument(
            "observation hurdle lower bound exceeds upper bound");
    }
    require_safe_identifier(observation.return_normalization_result_id,
        "return normalization result id");
    require_money(observation.observed_gross_buyer_price_million,
        "observed gross buyer price");
    require_money(
        observation.buyer_direct_cost_million, "buyer direct cost");
    require_safe_identifier(observation.buyer_direct_cost_evidence_record_id,
        "buyer-direct-cost evidence record id");
    require_money(observation.settled_claim_quantity_million,
        "settled claim quantity");

    require_money(observation.quoted_bid_gross_buyer_price_million,
        "quoted bid gross buyer price");
    require_money(observation.quoted_ask_gross_buyer_price_million,
        "quoted ask gross buyer price");
    require_money(observation.quoted_bid_buyer_direct_cost_million,
        "quoted bid buyer direct cost");
    require_money(observation.quoted_ask_buyer_direct_cost_million,
        "quoted ask buyer direct cost");
    require_money(observation.quoted_bid_claim_quantity_million,
        "quoted bid claim quantity");
    require_money(observation.quoted_ask_claim_quantity_million,
        "quoted ask claim quantity");
    require_safe_text(
        observation.quoted_bid_timestamp_utc, "quoted bid timestamp");
    require_safe_text(
        observation.quoted_ask_timestamp_utc, "quoted ask timestamp");
    require_safe_text(
        observation.quoted_bid_currency_label, "quoted bid currency");
    require_safe_text(
        observation.quoted_ask_currency_label, "quoted ask currency");
    require_safe_identifier(
        observation.quote_evidence_record_id, "quote evidence record id");
    require_safe_text(observation.quote_valid_until_utc,
        "quote valid-until timestamp");
    const std::array<double, 3U> all_in_cash{
        observation.observed_gross_buyer_price_million +
            observation.buyer_direct_cost_million,
        observation.quoted_bid_gross_buyer_price_million +
            observation.quoted_bid_buyer_direct_cost_million,
        observation.quoted_ask_gross_buyer_price_million +
            observation.quoted_ask_buyer_direct_cost_million};
    if (std::any_of(all_in_cash.begin(), all_in_cash.end(), [](double value) {
            return !std::isfinite(value) || value > kMaximumMoneyMillion;
        })) {
        throw std::invalid_argument(
            "all-in buyer cash exceeds the model money guardrail");
    }

    require_safe_text(observation.full_dated_scenario_cash_input_sha256,
        "full dated scenario cash hash");
    require_safe_text(
        observation.probability_input_sha256, "probability input hash");
    require_safe_text(observation.expected_cash_calculation_run_sha256,
        "expected-cash calculation run hash");
    require_money(observation.maximum_expected_cash_reconstruction_residual_million,
        "maximum expected-cash reconstruction residual");
    require_log_delta(
        observation.fixed_probability_to_robust_bridge_lower_log_delta,
        "fixed-probability bridge lower log delta");
    require_log_delta(
        observation.fixed_probability_to_robust_bridge_upper_log_delta,
        "fixed-probability bridge upper log delta");
    if (observation.fixed_probability_to_robust_bridge_lower_log_delta >
        observation.fixed_probability_to_robust_bridge_upper_log_delta) {
        throw std::invalid_argument(
            "fixed-probability bridge lower bound exceeds upper bound");
    }
    require_safe_identifier(
        observation.fixed_probability_to_robust_bridge_method_id,
        "fixed-probability bridge method id");
    require_safe_identifier(
        observation.fixed_probability_to_robust_bridge_evidence_record_id,
        "fixed-probability bridge evidence record id");
    if (!observation.fixed_probability_to_robust_bridge_is_present &&
        (observation.fixed_probability_to_robust_bridge_lower_log_delta != 0.0 ||
            observation.fixed_probability_to_robust_bridge_upper_log_delta != 0.0 ||
            observation.fixed_probability_to_robust_bridge_method_id != kAbsent ||
            observation.fixed_probability_to_robust_bridge_evidence_record_id !=
                kAbsent)) {
        throw std::invalid_argument(
            "an absent fixed-probability bridge must have zero deltas and none identifiers");
    }
    if (observation.expected_cash_operator ==
            RobustHurdleExpectedCashOperator::
                RobustMinimumOverDeclaredProbabilitySet &&
        observation.fixed_probability_to_robust_bridge_is_present) {
        throw std::invalid_argument(
            "a robust expected-cash operator cannot also carry a fixed-probability bridge");
    }

    require_safe_text(observation.side_rights_or_non_cash_consideration_note,
        "side-rights note");
    if (observation.side_rights_or_non_cash_consideration_present ==
        (observation.side_rights_or_non_cash_consideration_note == kAbsent)) {
        throw std::invalid_argument(
            "side-rights presence requires a note and absence requires note none");
    }
    require_log_delta(
        observation.jointly_feasible_total_lower_log_gross_return_delta,
        "joint total lower log-gross-return delta");
    require_log_delta(
        observation.jointly_feasible_total_upper_log_gross_return_delta,
        "joint total upper log-gross-return delta");
    if (observation.jointly_feasible_total_lower_log_gross_return_delta >
        observation.jointly_feasible_total_upper_log_gross_return_delta) {
        throw std::invalid_argument(
            "joint total lower log-gross-return delta exceeds upper bound");
    }
    require_safe_identifier(observation.joint_adjustment_set_method_id,
        "joint adjustment-set method id");
    require_safe_identifier(observation.joint_adjustment_set_evidence_record_id,
        "joint adjustment-set evidence record id");
    require_safe_text(observation.source_note, "observation source note");

    if (observation.adjustments.size() >
        kRobustHurdleEnvelopeMaximumAdjustmentsPerObservation) {
        throw std::invalid_argument(
            "an observation cannot contain more than eight adjustments");
    }
    std::unordered_set<std::string> adjustment_ids;
    for (const auto& adjustment : observation.adjustments) {
        validate_adjustment(adjustment);
        if (!adjustment_ids.emplace(adjustment.adjustment_id).second) {
            throw std::invalid_argument(
                "adjustment ids must be unique within an observation");
        }
    }
}

void add_reason(RobustHurdleObservationResult& result,
    RobustHurdleObservationIneligibilityReason reason) {
    if (std::find(result.ineligibility_reasons.begin(),
            result.ineligibility_reasons.end(), reason) ==
        result.ineligibility_reasons.end()) {
        result.ineligibility_reasons.push_back(reason);
    }
}

[[nodiscard]] std::size_t coverage_at(
    const std::vector<EligibleInterval>& intervals, double value) {
    return static_cast<std::size_t>(std::count_if(intervals.begin(),
        intervals.end(), [value](const EligibleInterval& interval) {
            return interval.lower <= value && value <= interval.upper;
        }));
}

[[nodiscard]] std::vector<std::string> boundary_witnesses(
    const std::vector<EligibleInterval>& intervals, double value) {
    std::vector<std::string> result;
    for (const EligibleInterval& interval : intervals) {
        if (interval.lower == value || interval.upper == value) {
            result.push_back(interval.cluster_id);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void append_component(std::vector<RobustHurdleClosedInterval>& components,
    double lower, double upper) {
    if (!components.empty() && lower <= components.back().upper) {
        components.back().upper = std::max(components.back().upper, upper);
        return;
    }
    RobustHurdleClosedInterval component;
    component.lower = lower;
    component.upper = upper;
    components.push_back(std::move(component));
}

[[nodiscard]] RobustHurdleIdentifiedSet build_identified_set(
    const std::vector<EligibleInterval>& intervals,
    std::size_t eligible_cluster_count, std::size_t contaminated_clusters,
    double domain_lower, double domain_upper) {
    RobustHurdleIdentifiedSet result;
    result.maximum_contaminated_clusters = contaminated_clusters;
    result.required_cluster_coverage =
        eligible_cluster_count - contaminated_clusters;

    std::vector<Event> events;
    events.reserve(intervals.size() * 2U + 2U);
    events.push_back(Event{domain_lower, 0U, 0U});
    events.push_back(Event{domain_upper, 0U, 0U});
    for (const EligibleInterval& interval : intervals) {
        events.push_back(Event{interval.lower, 1U, 0U});
        events.push_back(Event{interval.upper, 0U, 1U});
    }
    std::sort(events.begin(), events.end(), [](const Event& first,
                                               const Event& second) {
        return first.value < second.value;
    });
    std::vector<Event> merged;
    merged.reserve(events.size());
    for (const Event& event : events) {
        if (!merged.empty() && merged.back().value == event.value) {
            merged.back().starts += event.starts;
            merged.back().ends += event.ends;
        } else {
            merged.push_back(event);
        }
    }

    std::size_t coverage_before = 0U;
    for (std::size_t index = 0U; index < merged.size(); ++index) {
        const Event& event = merged[index];
        const std::size_t coverage_on = coverage_before + event.starts;
        result.maximum_attained_coverage =
            std::max(result.maximum_attained_coverage, coverage_on);
        if (coverage_on >= result.required_cluster_coverage) {
            append_component(result.components, event.value, event.value);
        }
        if (coverage_on < event.ends) {
            throw std::logic_error(
                "hurdle interval event sweep underflowed coverage");
        }
        const std::size_t coverage_after = coverage_on - event.ends;
        result.maximum_attained_coverage =
            std::max(result.maximum_attained_coverage, coverage_after);
        if (index + 1U < merged.size() &&
            event.value < merged[index + 1U].value &&
            coverage_after >= result.required_cluster_coverage) {
            append_component(result.components, event.value,
                merged[index + 1U].value);
        }
        coverage_before = coverage_after;
    }
    if (coverage_before != 0U) {
        throw std::logic_error(
            "hurdle interval event sweep did not return to zero coverage");
    }
    for (RobustHurdleClosedInterval& component : result.components) {
        component.lower_endpoint_coverage =
            coverage_at(intervals, component.lower);
        component.upper_endpoint_coverage =
            coverage_at(intervals, component.upper);
        component.lower_boundary_witness_cluster_ids =
            boundary_witnesses(intervals, component.lower);
        component.upper_boundary_witness_cluster_ids =
            boundary_witnesses(intervals, component.upper);
    }
    return result;
}

[[nodiscard]] bool canonical_components(
    const std::vector<RobustHurdleClosedInterval>& components) {
    for (std::size_t index = 0U; index < components.size(); ++index) {
        if (!std::isfinite(components[index].lower) ||
            !std::isfinite(components[index].upper) ||
            components[index].lower > components[index].upper) {
            return false;
        }
        if (index != 0U &&
            components[index - 1U].upper >= components[index].lower) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool same_components(
    const std::vector<RobustHurdleClosedInterval>& first,
    const std::vector<RobustHurdleClosedInterval>& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.size(); ++index) {
        if (first[index].lower != second[index].lower ||
            first[index].upper != second[index].upper) {
            return false;
        }
    }
    return true;
}

struct IdentifiedSetAudit {
    bool selected_points_and_cells_meet_threshold{true};
    bool excluded_points_and_cells_are_below_threshold{true};
};

[[nodiscard]] bool contains_rate(
    const std::vector<RobustHurdleClosedInterval>& components, double value) {
    return std::any_of(components.begin(), components.end(),
        [value](const RobustHurdleClosedInterval& component) {
            return component.lower <= value && value <= component.upper;
        });
}

[[nodiscard]] IdentifiedSetAudit audit_identified_set(
    const std::vector<EligibleInterval>& intervals,
    const RobustHurdleIdentifiedSet& identified_set, double domain_lower,
    double domain_upper) {
    std::vector<double> endpoints{domain_lower, domain_upper};
    endpoints.reserve(intervals.size() * 2U + 2U);
    for (const EligibleInterval& interval : intervals) {
        endpoints.push_back(interval.lower);
        endpoints.push_back(interval.upper);
    }
    std::sort(endpoints.begin(), endpoints.end());
    endpoints.erase(std::unique(endpoints.begin(), endpoints.end()),
        endpoints.end());

    IdentifiedSetAudit audit;
    const auto inspect = [&](double value) {
        const bool selected = contains_rate(identified_set.components, value);
        const bool enough = coverage_at(intervals, value) >=
            identified_set.required_cluster_coverage;
        if (selected && !enough) {
            audit.selected_points_and_cells_meet_threshold = false;
        }
        if (!selected && enough) {
            audit.excluded_points_and_cells_are_below_threshold = false;
        }
    };
    for (std::size_t index = 0U; index < endpoints.size(); ++index) {
        inspect(endpoints[index]);
        if (index + 1U < endpoints.size() &&
            endpoints[index] < endpoints[index + 1U]) {
            inspect(endpoints[index] +
                (endpoints[index + 1U] - endpoints[index]) / 2.0);
        }
    }
    return audit;
}

[[nodiscard]] bool equals_domain(
    const std::vector<RobustHurdleClosedInterval>& components,
    double lower, double upper) {
    return components.size() == 1U && components.front().lower == lower &&
        components.front().upper == upper;
}

[[nodiscard]] bool singleton_set(
    const std::vector<RobustHurdleClosedInterval>& components) {
    return components.size() == 1U &&
        components.front().lower == components.front().upper;
}

[[nodiscard]] RobustHurdleClosedInterval hull_diagnostic(
    const std::vector<EligibleInterval>& intervals) {
    RobustHurdleClosedInterval result;
    result.lower = intervals.front().lower;
    result.upper = intervals.front().upper;
    for (const EligibleInterval& interval : intervals) {
        result.lower = std::min(result.lower, interval.lower);
        result.upper = std::max(result.upper, interval.upper);
    }
    result.lower_endpoint_coverage = coverage_at(intervals, result.lower);
    result.upper_endpoint_coverage = coverage_at(intervals, result.upper);
    result.lower_boundary_witness_cluster_ids =
        boundary_witnesses(intervals, result.lower);
    result.upper_boundary_witness_cluster_ids =
        boundary_witnesses(intervals, result.upper);
    return result;
}

[[nodiscard]] bool has_non_placeholder_adjustment_provenance(
    const RobustHurdleLogGrossReturnAdjustmentConfig& adjustment) {
    return !placeholder(adjustment.method_id) &&
        !placeholder(adjustment.source_reference) &&
        !placeholder(adjustment.evidence_record_id);
}

[[nodiscard]] RobustHurdleObservationResult evaluate_observation(
    const RobustHurdleEnvelopeConfig& config,
    const RobustHurdleMarketObservationConfig& observation) {
    RobustHurdleObservationResult result;
    result.record_id = observation.record_id;
    result.economic_observation_cluster_id =
        observation.economic_observation_cluster_id;
    result.source_interval_lower = observation.annual_effective_hurdle_lower;
    result.source_interval_upper = observation.annual_effective_hurdle_upper;
    const bool settled = observation.status ==
        RobustHurdleObservationStatus::
            SettledOrderlyArmsLengthTransaction;
    const bool quote = observation.status ==
        RobustHurdleObservationStatus::ExecutableTwoSidedQuote;

    if (observation.status !=
            RobustHurdleObservationStatus::
                SettledOrderlyArmsLengthTransaction &&
        observation.status !=
            RobustHurdleObservationStatus::ExecutableTwoSidedQuote) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ObservationStatusIsNotEligible);
    }
    if (observation.observation_date > config.as_of_date) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ObservationDateIsAfterAnalysisAsOfDate);
    }
    if (settled && observation.settlement_date > config.as_of_date) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            SettlementDateIsAfterAnalysisAsOfDate);
    }
    if (placeholder(observation.source_reference) ||
        placeholder(observation.evidence_record_id)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            SourceOrEvidenceIsMissing);
    }

    if (settled &&
        (!observation.transaction_execution_is_evidenced ||
            !observation.buyer_cash_payment_is_evidenced ||
            !observation.transaction_settlement_is_evidenced ||
            placeholder(observation.settlement_evidence_record_id))) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            SettledTransactionEvidenceIsIncomplete);
    }
    if (settled &&
        (!observation.orderly_transaction_is_evidenced ||
            !observation.arms_length_transaction_is_evidenced ||
            observation.forced_or_distressed_transaction ||
            observation.related_party_transaction ||
            placeholder(observation.orderly_arms_length_evidence_record_id) ||
            observation.settled_claim_quantity_million == 0.0)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            SettledOrderlyArmsLengthEvidenceIsIncomplete);
    }
    if (settled &&
        !observation.settled_price_preimage_is_single_connected_interval) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            SettledPricePreimageIsNotSingleConnectedInterval);
    }
    if (quote && placeholder(observation.transaction_market_anchor_id)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            TransactionMarketAnchorIsMissing);
    }
    if (quote) {
        const bool timestamps =
            utc_timestamp(observation.quoted_bid_timestamp_utc) &&
            utc_timestamp(observation.quoted_ask_timestamp_utc) &&
            observation.quoted_bid_timestamp_utc ==
                observation.quoted_ask_timestamp_utc &&
            observation.quoted_bid_timestamp_utc.substr(0U, 10U) ==
                observation.observation_date;
        const double all_in_bid =
            observation.quoted_bid_gross_buyer_price_million +
            observation.quoted_bid_buyer_direct_cost_million;
        const double all_in_ask =
            observation.quoted_ask_gross_buyer_price_million +
            observation.quoted_ask_buyer_direct_cost_million;
        const bool coherent = observation.quoted_bid_and_ask_are_executable &&
            observation.quoted_bid_and_ask_are_evidenced &&
            !placeholder(observation.quote_evidence_record_id) &&
            observation.quoted_bid_gross_buyer_price_million <=
                observation.quoted_ask_gross_buyer_price_million &&
            all_in_bid <= all_in_ask &&
            observation.quoted_bid_claim_quantity_million > 0.0 &&
            observation.quoted_bid_claim_quantity_million ==
                observation.quoted_ask_claim_quantity_million &&
            observation.quoted_bid_currency_label ==
                observation.quoted_ask_currency_label &&
            observation.quoted_bid_currency_label ==
                observation.currency_label && timestamps &&
            utc_timestamp(observation.quote_valid_until_utc) &&
            observation.quote_valid_until_utc >=
                observation.quoted_ask_timestamp_utc &&
            observation.buyer_direct_cost_is_evidenced &&
            !placeholder(observation.buyer_direct_cost_evidence_record_id);
        if (!coherent) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                ExecutableQuoteSidesAreIncompleteOrIncoherent);
        }
        if (!observation.
                discounted_expected_cash_is_strictly_decreasing_over_rate_interval ||
            !observation.
                quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                ExecutableQuoteRatePreimageIsNotEvidenced);
        }
    }

    if (observation.return_basis !=
        RobustHurdleObservationReturnBasis::
            AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ReturnBasisIsNotPhysicalExpectedFullClaimDiscountRate);
    }
    if (!observation.physical_expected_full_claim_cash_includes_loss_and_timing) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ExpectedFullClaimLossAndTimingAreNotIncluded);
    }
    if (!observation.normalized_to_full_claim_at_observation_date) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            FullClaimObservationDateNormalizationIsMissing);
    }
    if (placeholder(observation.return_normalization_result_id)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ReturnNormalizationResultIsMissing);
    }
    if (settled &&
        (!observation.observed_gross_buyer_price_is_evidenced ||
            !observation.buyer_direct_cost_is_evidenced ||
            placeholder(observation.buyer_direct_cost_evidence_record_id))) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            PriceOrBuyerCostEvidenceIsMissing);
    }
    if (!observation.expected_cash_reconstruction_identity_is_evidenced ||
        !sha256(observation.full_dated_scenario_cash_input_sha256) ||
        !sha256(observation.probability_input_sha256) ||
        !sha256(observation.expected_cash_calculation_run_sha256)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ExpectedCashReconstructionMetadataIsMissing);
    }
    const double reconstruction_base = quote
        ? observation.quoted_ask_gross_buyer_price_million +
            observation.quoted_ask_buyer_direct_cost_million
        : observation.observed_gross_buyer_price_million +
            observation.buyer_direct_cost_million;
    if (observation.maximum_expected_cash_reconstruction_residual_million >
        comparison_tolerance(reconstruction_base, 0.0)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ExpectedCashReconstructionDoesNotReconcile);
    }
    if (observation.expected_cash_operator ==
        RobustHurdleExpectedCashOperator::FixedDeclaredProbabilityVector) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            FixedProbabilityOperatorIsReportOnlyInV01);
    }
    if (observation.target_reference_price_relation !=
        RobustIssuePriceHurdleReferenceRelation::Independent) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ReferencePriceRelationIsNotIndependent);
    }
    if (observation.side_rights_or_non_cash_consideration_present) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            SideRightsOrNonCashConsiderationIsPresent);
    }
    if (placeholder(observation.observed_market_claim_id) ||
        placeholder(observation.normalized_term_result_id)) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            ClaimOrTermIdentityIsMissing);
    }

    std::array<std::size_t, kRobustHurdleEnvelopeComparabilityAxisCount>
        adjustment_counts{};
    double component_lower = 0.0;
    double component_upper = 0.0;
    for (const auto& adjustment : observation.adjustments) {
        const std::size_t axis = robust_hurdle_axis_index(adjustment.axis);
        ++adjustment_counts[axis];
        component_lower += adjustment.lower_log_gross_return_delta;
        component_upper += adjustment.upper_log_gross_return_delta;
        if (!std::isfinite(component_lower) || !std::isfinite(component_upper)) {
            throw std::invalid_argument(
                "component log-gross-return adjustment sum is not finite");
        }
        if (!has_non_placeholder_adjustment_provenance(adjustment)) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                AdjustmentProvenanceIsMissing);
        }
    }
    result.summed_lower_log_gross_return_adjustment = component_lower;
    result.summed_upper_log_gross_return_adjustment = component_upper;

    const bool same_claim = observation.claim_relation ==
        RobustHurdleObservationClaimRelation::SameTargetClaim;
    const bool any_adjustment = !observation.adjustments.empty();
    const double observed_quantity = quote
        ? observation.quoted_bid_claim_quantity_million
        : observation.settled_claim_quantity_million;
    const std::size_t date_axis = robust_hurdle_axis_index(
        RobustHurdleComparabilityAxis::ObservationDateAndMarketRegime);
    if (observation.comparability_treatments[date_axis] ==
            RobustHurdleComparabilityTreatment::Matched &&
        observation.observation_date != config.as_of_date) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            MatchedObservationDateDoesNotEqualAnalysisAsOfDate);
    }
    if (same_claim) {
        if (observation.observed_market_claim_id !=
                config.target_market_claim_id ||
            observation.normalized_term_result_id !=
                config.target_normalized_term_result_id ||
            observation.currency_label != config.target_currency_label ||
            observation.monetary_basis != config.target_monetary_basis ||
            !nearly_equal(
                observed_quantity, config.target_claim_quantity_million)) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                SameClaimIdentityDoesNotMatchTarget);
        }
        if (any_adjustment ||
            std::any_of(observation.comparability_treatments.begin(),
                observation.comparability_treatments.end(), [](auto treatment) {
                    return treatment !=
                        RobustHurdleComparabilityTreatment::Matched;
                })) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                SameClaimRequiresAllAxesMatchedAndNoAdjustments);
        }
    } else {
        for (std::size_t axis = 0U;
             axis < kRobustHurdleEnvelopeComparabilityAxisCount; ++axis) {
            const auto treatment = observation.comparability_treatments[axis];
            if (treatment == RobustHurdleComparabilityTreatment::Unresolved) {
                add_reason(result, RobustHurdleObservationIneligibilityReason::
                    ComparableAxisIsUnresolved);
            } else if (treatment ==
                    RobustHurdleComparabilityTreatment::
                        BoundedLogGrossReturnAdjustment) {
                if (adjustment_counts[axis] == 0U) {
                    add_reason(result,
                        RobustHurdleObservationIneligibilityReason::
                            ComparableAdjustmentIsMissing);
                } else if (adjustment_counts[axis] > 1U) {
                    add_reason(result,
                        RobustHurdleObservationIneligibilityReason::
                            ComparableAdjustmentIsDuplicated);
                }
            } else if (adjustment_counts[axis] != 0U) {
                add_reason(result, RobustHurdleObservationIneligibilityReason::
                    ComparableAdjustmentIsUnexpected);
            }
        }
        const std::size_t currency_axis = robust_hurdle_axis_index(
            RobustHurdleComparabilityAxis::CurrencyAndMonetaryBasis);
        if (observation.comparability_treatments[currency_axis] ==
                RobustHurdleComparabilityTreatment::Matched &&
            (observation.currency_label != config.target_currency_label ||
                observation.monetary_basis !=
                    config.target_monetary_basis)) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                MatchedCurrencyOrMonetaryBasisDoesNotMatchTarget);
        }
        const std::size_t size_axis = robust_hurdle_axis_index(
            RobustHurdleComparabilityAxis::TransactionSize);
        if (observation.comparability_treatments[size_axis] ==
                RobustHurdleComparabilityTreatment::Matched &&
            !nearly_equal(
                observed_quantity, config.target_claim_quantity_million)) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                MatchedTransactionSizeDoesNotMatchTarget);
        }
    }

    if (any_adjustment) {
        if (!observation.
                expected_loss_recovery_and_timing_are_excluded_from_adjustments) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                ExpectedLossRecoveryOrTimingWouldBeDoubleCounted);
        }
        if (observation.adjustment_set_basis ==
            RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                AdjustmentSetIsOnlyAComponentwiseBoxOuterHull);
        }
        if (placeholder(observation.joint_adjustment_set_method_id) ||
            placeholder(observation.joint_adjustment_set_evidence_record_id)) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                JointAdjustmentSetProvenanceIsMissing);
        }
        if (observation.
                    jointly_feasible_total_lower_log_gross_return_delta <
                component_lower - comparison_tolerance(
                    observation.
                        jointly_feasible_total_lower_log_gross_return_delta,
                    component_lower) ||
            observation.
                    jointly_feasible_total_upper_log_gross_return_delta >
                component_upper + comparison_tolerance(
                    observation.
                        jointly_feasible_total_upper_log_gross_return_delta,
                    component_upper)) {
            add_reason(result, RobustHurdleObservationIneligibilityReason::
                JointAdjustmentTotalIsOutsideComponentBounds);
        }
    } else if (observation.
                       jointly_feasible_total_lower_log_gross_return_delta !=
                   0.0 ||
        observation.jointly_feasible_total_upper_log_gross_return_delta !=
            0.0) {
        add_reason(result, RobustHurdleObservationIneligibilityReason::
            JointAdjustmentTotalIsOutsideComponentBounds);
    }

    if (observation.adjustment_set_basis ==
        RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval) {
        result.applied_lower_log_gross_return_adjustment =
            observation.jointly_feasible_total_lower_log_gross_return_delta;
        result.applied_upper_log_gross_return_adjustment =
            observation.jointly_feasible_total_upper_log_gross_return_delta;
    } else {
        result.applied_lower_log_gross_return_adjustment = component_lower;
        result.applied_upper_log_gross_return_adjustment = component_upper;
    }

    const double mapped_log_lower =
        std::log1p(observation.annual_effective_hurdle_lower) +
        result.applied_lower_log_gross_return_adjustment;
    const double mapped_log_upper =
        std::log1p(observation.annual_effective_hurdle_upper) +
        result.applied_upper_log_gross_return_adjustment;
    if (!std::isfinite(mapped_log_lower) || !std::isfinite(mapped_log_upper) ||
        mapped_log_lower > mapped_log_upper) {
        throw std::invalid_argument(
            "normalized hurdle log interval is non-finite or reversed");
    }
    const double mapped_lower =
        result.applied_lower_log_gross_return_adjustment == 0.0
        ? observation.annual_effective_hurdle_lower
        : std::expm1(mapped_log_lower);
    const double mapped_upper =
        result.applied_upper_log_gross_return_adjustment == 0.0
        ? observation.annual_effective_hurdle_upper
        : std::expm1(mapped_log_upper);
    if (!std::isfinite(mapped_lower) || !std::isfinite(mapped_upper) ||
        !(mapped_lower > -1.0) || !(mapped_upper > -1.0) ||
        mapped_lower > mapped_upper) {
        throw std::invalid_argument(
            "normalized hurdle interval is non-finite, at or below -1, or reversed");
    }
    result.mapped_interval_lower = mapped_lower;
    result.mapped_interval_upper = mapped_upper;
    result.maximum_normalization_identity_error = std::max(
        std::abs(std::log1p(mapped_lower) - mapped_log_lower),
        std::abs(std::log1p(mapped_upper) - mapped_log_upper));
    if (!std::isfinite(result.maximum_normalization_identity_error)) {
        throw std::invalid_argument(
            "normalized hurdle identity error is non-finite");
    }

    if (mapped_upper < config.annual_effective_domain_lower) {
        result.mapped_interval_status =
            RobustHurdleMappedIntervalStatus::EmptyBelowDeclaredDomain;
    } else if (mapped_lower > config.annual_effective_domain_upper) {
        result.mapped_interval_status =
            RobustHurdleMappedIntervalStatus::EmptyAboveDeclaredDomain;
    } else {
        result.mapped_interval_status =
            RobustHurdleMappedIntervalStatus::NonemptyAfterDomainClipping;
        RobustHurdleClosedInterval interval;
        interval.lower =
            std::max(mapped_lower, config.annual_effective_domain_lower);
        interval.upper =
            std::min(mapped_upper, config.annual_effective_domain_upper);
        result.clipped_interval = std::move(interval);
    }

    if (result.ineligibility_reasons.empty()) {
        if (same_claim) {
            result.eligibility = RobustHurdleObservationEligibility::
                EligibleSameClaimMarketObservation;
        } else if (any_adjustment) {
            result.eligibility = RobustHurdleObservationEligibility::
                EligibleModelAdjustedComparable;
        } else {
            result.eligibility = RobustHurdleObservationEligibility::
                EligibleComparableMarketObservation;
        }
        if (quote) {
            result.evidence_tier =
                RobustHurdleEvidenceTier::ExecutableTwoSidedQuote;
        } else if (same_claim) {
            result.evidence_tier =
                RobustHurdleEvidenceTier::DirectSettledIdenticalClaim;
        } else {
            result.evidence_tier =
                RobustHurdleEvidenceTier::SettledComparable;
        }
    }
    return result;
}

[[nodiscard]] std::size_t tier_position(RobustHurdleEvidenceTier tier) {
    switch (tier) {
    case RobustHurdleEvidenceTier::DirectSettledIdenticalClaim:
        return 0U;
    case RobustHurdleEvidenceTier::SettledComparable:
        return 1U;
    case RobustHurdleEvidenceTier::ExecutableTwoSidedQuote:
        return 2U;
    }
    throw std::logic_error("unknown hurdle evidence tier");
}

[[nodiscard]] std::vector<EligibleInterval> intervals_for_tier(
    const RobustHurdleEnvelopeSummary& summary,
    const RobustHurdleEvidenceTierResult& tier) {
    std::vector<EligibleInterval> result;
    for (const std::size_t index : tier.eligible_observation_indices) {
        const RobustHurdleObservationResult& observation =
            summary.observations[index];
        if (!observation.clipped_interval.has_value()) {
            continue;
        }
        result.push_back(EligibleInterval{
            observation.clipped_interval->lower,
            observation.clipped_interval->upper, index,
            observation.economic_observation_cluster_id});
    }
    return result;
}

void evaluate_tier(const RobustHurdleEnvelopeConfig& config,
    RobustHurdleEnvelopeSummary& summary,
    RobustHurdleEvidenceTierResult& tier) {
    const std::size_t n = tier.eligible_observation_indices.size();
    tier.eligible_cluster_count = n;
    tier.declared_quorum = tier.tier ==
            RobustHurdleEvidenceTier::DirectSettledIdenticalClaim
        ? 1U
        : config.minimum_consensus_cluster_coverage;
    if (n == 0U) {
        tier.status = RobustHurdleEvidenceTierStatus::NoEligibleClusters;
        return;
    }
    if (config.maximum_contaminated_clusters >= n ||
        config.maximum_contaminated_clusters >=
            n - config.maximum_contaminated_clusters) {
        tier.status = RobustHurdleEvidenceTierStatus::
            ContaminationBudgetIsInadmissible;
        return;
    }
    tier.required_cluster_coverage =
        n - config.maximum_contaminated_clusters;
    const std::vector<EligibleInterval> intervals =
        intervals_for_tier(summary, tier);
    if (!intervals.empty()) {
        tier.eligible_interval_hull_diagnostic =
            hull_diagnostic(intervals);
    }
    for (std::size_t omitted = 0U;
         omitted <= config.maximum_contaminated_clusters; ++omitted) {
        tier.identified_sets_s0_through_sk.push_back(build_identified_set(
            intervals, n, omitted, config.annual_effective_domain_lower,
            config.annual_effective_domain_upper));
    }

    const RobustHurdleIdentifiedSet& selected =
        tier.identified_sets_s0_through_sk.back();
    for (const std::size_t index : tier.eligible_observation_indices) {
        const auto& observation = summary.observations[index];
        const bool informative = !observation.clipped_interval.has_value() ||
            observation.clipped_interval->lower !=
                config.annual_effective_domain_lower ||
            observation.clipped_interval->upper !=
                config.annual_effective_domain_upper;
        if (informative) {
            tier.informative_observation_indices.push_back(index);
        }
    }

    tier.leave_one_cluster_out.reserve(n);
    for (std::size_t local = 0U; local < n; ++local) {
        RobustHurdleLeaveOneClusterOutResult sensitivity;
        sensitivity.omitted_eligible_observation_index =
            tier.eligible_observation_indices[local];
        sensitivity.omitted_economic_observation_cluster_id =
            summary.observations[sensitivity.omitted_eligible_observation_index]
                .economic_observation_cluster_id;
        sensitivity.remaining_eligible_cluster_count = n - 1U;
        const std::size_t remaining = n - 1U;
        sensitivity.declared_contamination_budget_remains_admissible =
            remaining != 0U &&
            config.maximum_contaminated_clusters < remaining &&
            config.maximum_contaminated_clusters <
                remaining - config.maximum_contaminated_clusters;
        if (sensitivity.declared_contamination_budget_remains_admissible) {
            sensitivity.required_cluster_coverage =
                remaining - config.maximum_contaminated_clusters;
            std::vector<EligibleInterval> reduced;
            reduced.reserve(intervals.size());
            const std::size_t omitted_global =
                tier.eligible_observation_indices[local];
            std::copy_if(intervals.begin(), intervals.end(),
                std::back_inserter(reduced),
                [omitted_global](const EligibleInterval& interval) {
                    return interval.observation_index != omitted_global;
                });
            sensitivity.components = build_identified_set(reduced, remaining,
                config.maximum_contaminated_clusters,
                config.annual_effective_domain_lower,
                config.annual_effective_domain_upper)
                                         .components;
            if (!same_components(sensitivity.components,
                    selected.components)) {
                tier.binding_observation_indices.push_back(omitted_global);
            }
        } else if (n == 1U &&
            std::find(tier.informative_observation_indices.begin(),
                tier.informative_observation_indices.end(),
                sensitivity.omitted_eligible_observation_index) !=
                tier.informative_observation_indices.end()) {
            tier.binding_observation_indices.push_back(
                sensitivity.omitted_eligible_observation_index);
        }
        tier.leave_one_cluster_out.push_back(std::move(sensitivity));
    }

    if (equals_domain(selected.components,
                   config.annual_effective_domain_lower,
                   config.annual_effective_domain_upper)) {
        tier.status = RobustHurdleEvidenceTierStatus::
            UninformativeDeclaredDomainIsNotNarrowed;
    } else if (selected.components.empty()) {
        tier.status = RobustHurdleEvidenceTierStatus::IdentifiedSetIsEmpty;
    } else {
        tier.status = RobustHurdleEvidenceTierStatus::IdentifiedSetFound;
    }
    tier.comparable_consensus_label_threshold_met =
        tier.tier != RobustHurdleEvidenceTier::DirectSettledIdenticalClaim &&
        tier.required_cluster_coverage >=
            config.minimum_consensus_cluster_coverage &&
        tier.status == RobustHurdleEvidenceTierStatus::IdentifiedSetFound;
}

[[nodiscard]] bool tier_has_eligible_clusters(
    const RobustHurdleEvidenceTierResult& tier) {
    return tier.eligible_cluster_count != 0U;
}

} // namespace

std::string_view to_string(RobustHurdleObservationStatus value) noexcept {
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
    return "unknown";
}

std::string_view to_string(
    RobustHurdleObservationTransactionType value) noexcept {
    switch (value) {
    case RobustHurdleObservationTransactionType::PrimaryIssuance:
        return "primary_issuance";
    case RobustHurdleObservationTransactionType::SecondaryTrade:
        return "secondary_trade";
    case RobustHurdleObservationTransactionType::TwoSidedMarketQuote:
        return "two_sided_market_quote";
    }
    return "unknown";
}

std::string_view to_string(
    RobustHurdleObservationReturnBasis value) noexcept {
    switch (value) {
    case RobustHurdleObservationReturnBasis::
        AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows:
        return "annual_effective_all_in_buyer_cash_discount_rate_on_physical_expected_full_claim_cash_flows";
    case RobustHurdleObservationReturnBasis::GrossPriceExBuyerCostDiscountRate:
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
    return "unknown";
}

std::string_view to_string(
    RobustHurdleObservationClaimRelation value) noexcept {
    switch (value) {
    case RobustHurdleObservationClaimRelation::SameTargetClaim:
        return "same_target_claim";
    case RobustHurdleObservationClaimRelation::ComparableClaim:
        return "comparable_claim";
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleExpectedCashOperator value) noexcept {
    switch (value) {
    case RobustHurdleExpectedCashOperator::
        RobustMinimumOverDeclaredProbabilitySet:
        return "robust_minimum_over_declared_probability_set";
    case RobustHurdleExpectedCashOperator::FixedDeclaredProbabilityVector:
        return "fixed_declared_probability_vector";
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleAdjustmentSetBasis value) noexcept {
    switch (value) {
    case RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval:
        return "jointly_feasible_total_interval";
    case RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull:
        return "componentwise_box_outer_hull";
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleEvidenceTier value) noexcept {
    switch (value) {
    case RobustHurdleEvidenceTier::DirectSettledIdenticalClaim:
        return "direct_settled_identical_claim";
    case RobustHurdleEvidenceTier::SettledComparable:
        return "settled_comparable";
    case RobustHurdleEvidenceTier::ExecutableTwoSidedQuote:
        return "executable_two_sided_quote";
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleEvidenceTierStatus value) noexcept {
    switch (value) {
    case RobustHurdleEvidenceTierStatus::NoEligibleClusters:
        return "no_eligible_clusters";
    case RobustHurdleEvidenceTierStatus::ContaminationBudgetIsInadmissible:
        return "contamination_budget_is_inadmissible";
    case RobustHurdleEvidenceTierStatus::
        UninformativeDeclaredDomainIsNotNarrowed:
        return "uninformative_declared_domain_is_not_narrowed";
    case RobustHurdleEvidenceTierStatus::IdentifiedSetIsEmpty:
        return "identified_set_is_empty";
    case RobustHurdleEvidenceTierStatus::IdentifiedSetFound:
        return "identified_set_found";
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleComparabilityAxis value) noexcept {
    switch (value) {
    case RobustHurdleComparabilityAxis::ContractualCashflowRights:
        return "contractual_cashflow_rights";
    case RobustHurdleComparabilityAxis::
        SeniorityAndResidualTailRiskPremiumAfterExpectedCash:
        return "seniority_and_residual_tail_risk_premium_after_expected_cash";
    case RobustHurdleComparabilityAxis::
        SystematicCovarianceConcentrationAndResidualModelRiskPremium:
        return "systematic_covariance_concentration_and_residual_model_risk_premium";
    case RobustHurdleComparabilityAxis::ContractualTermAndCashflowTiming:
        return "contractual_term_and_cashflow_timing";
    case RobustHurdleComparabilityAxis::CurrencyAndMonetaryBasis:
        return "currency_and_monetary_basis";
    case RobustHurdleComparabilityAxis::LiquidityAndTransferability:
        return "liquidity_and_transferability";
    case RobustHurdleComparabilityAxis::TransactionSize:
        return "transaction_size";
    case RobustHurdleComparabilityAxis::ObservationDateAndMarketRegime:
        return "observation_date_and_market_regime";
    }
    return "unknown";
}

std::string_view to_string(
    RobustHurdleComparabilityTreatment value) noexcept {
    switch (value) {
    case RobustHurdleComparabilityTreatment::Matched:
        return "matched";
    case RobustHurdleComparabilityTreatment::
        BoundedLogGrossReturnAdjustment:
        return "bounded_log_gross_return_adjustment";
    case RobustHurdleComparabilityTreatment::Unresolved:
        return "unresolved";
    }
    return "unknown";
}

std::string_view to_string(
    RobustHurdleObservationEligibility value) noexcept {
    switch (value) {
    case RobustHurdleObservationEligibility::Ineligible:
        return "ineligible";
    case RobustHurdleObservationEligibility::
        EligibleSameClaimMarketObservation:
        return "eligible_same_claim_market_observation";
    case RobustHurdleObservationEligibility::
        EligibleComparableMarketObservation:
        return "eligible_comparable_market_observation";
    case RobustHurdleObservationEligibility::EligibleModelAdjustedComparable:
        return "eligible_model_adjusted_comparable";
    }
    return "unknown";
}

std::string_view to_string(
    RobustHurdleObservationIneligibilityReason value) noexcept {
    switch (value) {
#define NATURALEHIA_REASON_CASE(name, text)                                      \
    case RobustHurdleObservationIneligibilityReason::name:                      \
        return text
        NATURALEHIA_REASON_CASE(ObservationStatusIsNotEligible,
            "observation_status_is_not_eligible");
        NATURALEHIA_REASON_CASE(ObservationDateIsAfterAnalysisAsOfDate,
            "observation_date_is_after_analysis_as_of_date");
        NATURALEHIA_REASON_CASE(SettlementDateIsAfterAnalysisAsOfDate,
            "settlement_date_is_after_analysis_as_of_date");
        NATURALEHIA_REASON_CASE(SourceOrEvidenceIsMissing,
            "source_or_evidence_is_missing");
        NATURALEHIA_REASON_CASE(TransactionMarketAnchorIsMissing,
            "transaction_market_anchor_is_missing");
        NATURALEHIA_REASON_CASE(ReturnBasisIsNotPhysicalExpectedFullClaimDiscountRate,
            "return_basis_is_not_physical_expected_full_claim_discount_rate");
        NATURALEHIA_REASON_CASE(ExpectedFullClaimLossAndTimingAreNotIncluded,
            "expected_full_claim_loss_and_timing_are_not_included");
        NATURALEHIA_REASON_CASE(SettledTransactionEvidenceIsIncomplete,
            "settled_transaction_evidence_is_incomplete");
        NATURALEHIA_REASON_CASE(SettledOrderlyArmsLengthEvidenceIsIncomplete,
            "settled_orderly_arms_length_evidence_is_incomplete");
        NATURALEHIA_REASON_CASE(SettledPricePreimageIsNotSingleConnectedInterval,
            "settled_price_preimage_is_not_single_connected_interval");
        NATURALEHIA_REASON_CASE(FullClaimObservationDateNormalizationIsMissing,
            "full_claim_observation_date_normalization_is_missing");
        NATURALEHIA_REASON_CASE(ReturnNormalizationResultIsMissing,
            "return_normalization_result_is_missing");
        NATURALEHIA_REASON_CASE(PriceOrBuyerCostEvidenceIsMissing,
            "price_or_buyer_cost_evidence_is_missing");
        NATURALEHIA_REASON_CASE(ExecutableQuoteSidesAreIncompleteOrIncoherent,
            "executable_quote_sides_are_incomplete_or_incoherent");
        NATURALEHIA_REASON_CASE(ExecutableQuoteRatePreimageIsNotEvidenced,
            "executable_quote_rate_preimage_is_not_evidenced");
        NATURALEHIA_REASON_CASE(ExpectedCashReconstructionMetadataIsMissing,
            "expected_cash_reconstruction_metadata_is_missing");
        NATURALEHIA_REASON_CASE(ExpectedCashReconstructionDoesNotReconcile,
            "expected_cash_reconstruction_does_not_reconcile");
        NATURALEHIA_REASON_CASE(FixedProbabilityOperatorIsReportOnlyInV01,
            "fixed_probability_operator_is_report_only_in_v0_1");
        NATURALEHIA_REASON_CASE(ReferencePriceRelationIsNotIndependent,
            "reference_price_relation_is_not_independent");
        NATURALEHIA_REASON_CASE(SideRightsOrNonCashConsiderationIsPresent,
            "side_rights_or_non_cash_consideration_is_present");
        NATURALEHIA_REASON_CASE(ClaimOrTermIdentityIsMissing,
            "claim_or_term_identity_is_missing");
        NATURALEHIA_REASON_CASE(SameClaimIdentityDoesNotMatchTarget,
            "same_claim_identity_does_not_match_target");
        NATURALEHIA_REASON_CASE(SameClaimRequiresAllAxesMatchedAndNoAdjustments,
            "same_claim_requires_all_axes_matched_and_no_adjustments");
        NATURALEHIA_REASON_CASE(ComparableAxisIsUnresolved,
            "comparable_axis_is_unresolved");
        NATURALEHIA_REASON_CASE(ComparableAdjustmentIsMissing,
            "comparable_adjustment_is_missing");
        NATURALEHIA_REASON_CASE(ComparableAdjustmentIsDuplicated,
            "comparable_adjustment_is_duplicated");
        NATURALEHIA_REASON_CASE(ComparableAdjustmentIsUnexpected,
            "comparable_adjustment_is_unexpected");
        NATURALEHIA_REASON_CASE(MatchedCurrencyOrMonetaryBasisDoesNotMatchTarget,
            "matched_currency_or_monetary_basis_does_not_match_target");
        NATURALEHIA_REASON_CASE(MatchedTransactionSizeDoesNotMatchTarget,
            "matched_transaction_size_does_not_match_target");
        NATURALEHIA_REASON_CASE(MatchedObservationDateDoesNotEqualAnalysisAsOfDate,
            "matched_observation_date_does_not_equal_analysis_as_of_date");
        NATURALEHIA_REASON_CASE(AdjustmentProvenanceIsMissing,
            "adjustment_provenance_is_missing");
        NATURALEHIA_REASON_CASE(AdjustmentSetIsOnlyAComponentwiseBoxOuterHull,
            "adjustment_set_is_only_a_componentwise_box_outer_hull");
        NATURALEHIA_REASON_CASE(JointAdjustmentSetProvenanceIsMissing,
            "joint_adjustment_set_provenance_is_missing");
        NATURALEHIA_REASON_CASE(JointAdjustmentTotalIsOutsideComponentBounds,
            "joint_adjustment_total_is_outside_component_bounds");
        NATURALEHIA_REASON_CASE(ExpectedLossRecoveryOrTimingWouldBeDoubleCounted,
            "expected_loss_recovery_or_timing_would_be_double_counted");
#undef NATURALEHIA_REASON_CASE
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleMappedIntervalStatus value) noexcept {
    switch (value) {
    case RobustHurdleMappedIntervalStatus::NonemptyAfterDomainClipping:
        return "nonempty_after_domain_clipping";
    case RobustHurdleMappedIntervalStatus::EmptyBelowDeclaredDomain:
        return "empty_below_declared_domain";
    case RobustHurdleMappedIntervalStatus::EmptyAboveDeclaredDomain:
        return "empty_above_declared_domain";
    }
    return "unknown";
}

std::string_view to_string(RobustHurdleEnvelopeStatus value) noexcept {
    switch (value) {
    case RobustHurdleEnvelopeStatus::NoEligibleEconomicObservationClusters:
        return "no_eligible_economic_observation_clusters";
    case RobustHurdleEnvelopeStatus::
        InsufficientEligibleClustersForDeclaredContaminationBudget:
        return "insufficient_eligible_clusters_for_declared_contamination_budget";
    case RobustHurdleEnvelopeStatus::
        UninformativeDeclaredDomainIsNotNarrowed:
        return "uninformative_declared_domain_is_not_narrowed";
    case RobustHurdleEnvelopeStatus::IdentifiedSetIsEmpty:
        return "identified_set_is_empty";
    case RobustHurdleEnvelopeStatus::IdentifiedSetFound:
        return "identified_set_found";
    }
    return "unknown";
}

std::string_view to_string(
    RobustHurdleEnvelopeIdentificationBasis value) noexcept {
    switch (value) {
    case RobustHurdleEnvelopeIdentificationBasis::Unavailable:
        return "unavailable";
    case RobustHurdleEnvelopeIdentificationBasis::
        DirectSingleTransactionConditionedCase:
        return "direct_single_transaction_conditioned_case";
    case RobustHurdleEnvelopeIdentificationBasis::
        LimitedMultiObservationIdentification:
        return "limited_multi_observation_identification";
    case RobustHurdleEnvelopeIdentificationBasis::
        ComparableConsensusThresholdMet:
        return "comparable_consensus_threshold_met";
    }
    return "unknown";
}

void validate_robust_hurdle_envelope_config(
    const RobustHurdleEnvelopeConfig& config) {
    if (config.model_version != kRobustHurdleEnvelopeModelVersion) {
        throw std::invalid_argument(
            "unsupported robust hurdle-envelope model version");
    }
    require_safe_identifier(config.analysis_id, "hurdle-envelope analysis id");
    if (!iso_date(config.as_of_date)) {
        throw std::invalid_argument(
            "hurdle-envelope as-of date must be a calendar date in YYYY-MM-DD");
    }
    require_safe_text(config.source_note, "hurdle-envelope source note");
    require_safe_identifier(
        config.universe_manifest_id, "universe manifest id");
    require_safe_identifier(config.inclusion_rule_id, "inclusion rule id");
    require_safe_identifier(
        config.deduplication_manifest_id, "deduplication manifest id");
    require_safe_identifier(
        config.source_clustering_rule_id, "source clustering rule id");
    if (placeholder(config.universe_manifest_id) ||
        placeholder(config.inclusion_rule_id) ||
        placeholder(config.deduplication_manifest_id) ||
        placeholder(config.source_clustering_rule_id) ||
        !config.observation_universe_is_frozen ||
        !config.inclusion_rule_is_predeclared ||
        !config.deduplication_rule_is_predeclared ||
        !config.all_in_scope_economic_observation_clusters_are_included ||
        !config.same_dealer_same_window_quotes_are_clustered) {
        throw std::invalid_argument(
            "hurdle-envelope v0.1 requires frozen universe, inclusion, deduplication, and same-source clustering manifests and assertions");
    }
    require_safe_identifier(
        config.target_market_claim_id, "target market claim id");
    require_safe_identifier(config.target_normalized_term_result_id,
        "target normalized term result id");
    require_safe_text(config.target_currency_label, "target currency");
    require_safe_text(config.target_monetary_basis, "target monetary basis");
    require_money(
        config.target_claim_quantity_million, "target claim quantity");
    if (placeholder(config.target_market_claim_id) ||
        placeholder(config.target_normalized_term_result_id)) {
        throw std::invalid_argument(
            "target claim and normalized term identities must be non-placeholder");
    }
    if (config.target_claim_quantity_million == 0.0) {
        throw std::invalid_argument(
            "target claim quantity must be positive");
    }
    require_non_negative(config.annual_effective_domain_lower,
        "annual effective domain lower bound");
    require_non_negative(config.annual_effective_domain_upper,
        "annual effective domain upper bound");
    if (config.annual_effective_domain_lower >=
            config.annual_effective_domain_upper ||
        config.annual_effective_domain_upper >
            kMaximumAnnualEffectiveDomainRate) {
        throw std::invalid_argument(
            "declared annual effective domain must be a non-degenerate closed subinterval of [0,10]");
    }
    if (config.minimum_consensus_cluster_coverage < 3U ||
        config.minimum_consensus_cluster_coverage >
            kRobustHurdleEnvelopeMaximumObservations) {
        throw std::invalid_argument(
            "minimum comparable consensus coverage must lie between 3 and 128");
    }
    if (config.observations.empty() ||
        config.observations.size() >
            kRobustHurdleEnvelopeMaximumObservations) {
        throw std::invalid_argument(
            "hurdle-envelope observation count must lie between one and 128");
    }
    if (config.maximum_contaminated_clusters >= config.observations.size() ||
        config.maximum_contaminated_clusters >=
            config.observations.size() -
                config.maximum_contaminated_clusters) {
        throw std::invalid_argument(
            "predeclared contamination budget k must satisfy k<n/2 for the frozen in-scope cluster universe");
    }
    std::unordered_set<std::string> record_ids;
    std::unordered_set<std::string> cluster_ids;
    std::size_t adjustments = 0U;
    for (const auto& observation : config.observations) {
        validate_observation(observation);
        if (!record_ids.emplace(observation.record_id).second) {
            throw std::invalid_argument(
                "hurdle observation record ids must be unique");
        }
        if (!cluster_ids.emplace(observation.economic_observation_cluster_id)
                 .second) {
            throw std::invalid_argument(
                "economic-observation cluster ids must be unique after deduplication");
        }
        adjustments = detail::checked_grid_sum(adjustments,
            observation.adjustments.size(),
            kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
            kWorkLimitMessage);
    }
    const std::size_t axis_work = detail::checked_grid_product(
        {config.observations.size(),
            kRobustHurdleEnvelopeComparabilityAxisCount},
        kRobustHurdleEnvelopeMaximumStructuralWorkUnits, kWorkLimitMessage);
    (void)detail::checked_grid_sum(axis_work, adjustments,
        kRobustHurdleEnvelopeMaximumStructuralWorkUnits, kWorkLimitMessage);
}

RobustHurdleEnvelopeSummary evaluate_robust_hurdle_envelope(
    const RobustHurdleEnvelopeConfig& config) {
    validate_robust_hurdle_envelope_config(config);
    std::vector<RobustHurdleMarketObservationConfig> canonical_observations =
        config.observations;
    std::sort(canonical_observations.begin(), canonical_observations.end(),
        [](const auto& first, const auto& second) {
            return first.record_id < second.record_id;
        });
    RobustHurdleEnvelopeSummary summary;
    summary.synthetic_inputs = config.synthetic_inputs;
    summary.mechanical_candidate_set_only = true;
    summary.empirical_hurdle_evidence_release_authorized = false;
    summary.source_identifiers_or_assertion_booleans_authenticate_documents =
        false;
    summary.input_observation_count = canonical_observations.size();
    summary.maximum_contaminated_clusters =
        config.maximum_contaminated_clusters;
    summary.observations.reserve(canonical_observations.size());
    summary.evidence_tiers = {
        RobustHurdleEvidenceTierResult{
            RobustHurdleEvidenceTier::DirectSettledIdenticalClaim},
        RobustHurdleEvidenceTierResult{
            RobustHurdleEvidenceTier::SettledComparable},
        RobustHurdleEvidenceTierResult{
            RobustHurdleEvidenceTier::ExecutableTwoSidedQuote}};

    std::size_t total_adjustments = 0U;
    for (const auto& observation : canonical_observations) {
        RobustHurdleObservationResult result =
            evaluate_observation(config, observation);
        summary.maximum_normalization_identity_error =
            std::max(summary.maximum_normalization_identity_error,
                result.maximum_normalization_identity_error);
        const std::size_t index = summary.observations.size();
        if (result.eligibility ==
            RobustHurdleObservationEligibility::Ineligible) {
            ++summary.financially_ineligible_cluster_count;
        } else {
            ++summary.eligible_cluster_count;
            summary.eligible_observation_indices.push_back(index);
            if (!result.clipped_interval.has_value()) {
                ++summary.eligible_empty_interval_cluster_count;
            }
            summary.evidence_tiers[tier_position(*result.evidence_tier)]
                .eligible_observation_indices.push_back(index);
        }
        total_adjustments = detail::checked_grid_sum(total_adjustments,
            observation.adjustments.size(),
            kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
            kWorkLimitMessage);
        summary.observations.push_back(std::move(result));
    }

    summary.work.comparability_axis_work_units =
        detail::checked_grid_product({canonical_observations.size(),
            kRobustHurdleEnvelopeComparabilityAxisCount},
            kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
            kWorkLimitMessage);
    summary.work.adjustment_normalization_work_units = total_adjustments;
    for (const auto& tier : summary.evidence_tiers) {
        const std::size_t n = tier.eligible_observation_indices.size();
        if (n != 0U && config.maximum_contaminated_clusters < n &&
            config.maximum_contaminated_clusters <
                n - config.maximum_contaminated_clusters) {
            const std::size_t event_width = detail::checked_grid_sum(
                detail::checked_grid_product({2U, n},
                    kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
                    kWorkLimitMessage),
                2U, kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
                kWorkLimitMessage);
            summary.work.identified_set_event_work_units =
                detail::checked_grid_sum(
                    summary.work.identified_set_event_work_units,
                    detail::checked_grid_product(
                        {config.maximum_contaminated_clusters + 1U,
                            event_width},
                        kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
                        kWorkLimitMessage),
                    kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
                    kWorkLimitMessage);
            summary.work.leave_one_cluster_out_event_work_units =
                detail::checked_grid_sum(
                    summary.work.leave_one_cluster_out_event_work_units,
                    detail::checked_grid_product({n, 2U, n},
                        kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
                        kWorkLimitMessage),
                    kRobustHurdleEnvelopeMaximumStructuralWorkUnits,
                    kWorkLimitMessage);
        }
    }
    summary.work.structural_work_units = detail::checked_grid_sum(
        summary.work.comparability_axis_work_units,
        summary.work.adjustment_normalization_work_units,
        kRobustHurdleEnvelopeMaximumStructuralWorkUnits, kWorkLimitMessage);
    summary.work.structural_work_units = detail::checked_grid_sum(
        summary.work.structural_work_units,
        summary.work.identified_set_event_work_units,
        kRobustHurdleEnvelopeMaximumStructuralWorkUnits, kWorkLimitMessage);
    summary.work.structural_work_units = detail::checked_grid_sum(
        summary.work.structural_work_units,
        summary.work.leave_one_cluster_out_event_work_units,
        kRobustHurdleEnvelopeMaximumStructuralWorkUnits, kWorkLimitMessage);

    for (auto& tier : summary.evidence_tiers) {
        evaluate_tier(config, summary, tier);
    }

    summary.all_identified_sets_are_closed_disjoint_and_canonical = true;
    for (const auto& tier : summary.evidence_tiers) {
        for (const auto& set : tier.identified_sets_s0_through_sk) {
            summary.all_identified_sets_are_closed_disjoint_and_canonical =
                summary.all_identified_sets_are_closed_disjoint_and_canonical &&
                canonical_components(set.components);
        }
    }
    summary.every_selected_component_meets_its_coverage_threshold = false;
    summary.every_selected_gap_is_below_its_coverage_threshold = false;

    for (std::size_t tier_index = 0U;
         tier_index < summary.evidence_tiers.size(); ++tier_index) {
        const auto& tier = summary.evidence_tiers[tier_index];
        if (tier_has_eligible_clusters(tier)) {
            summary.selected_evidence_tier = tier.tier;
            if (tier.status != RobustHurdleEvidenceTierStatus::
                    ContaminationBudgetIsInadmissible) {
                summary.selected_identified_set =
                    tier.identified_sets_s0_through_sk.back();
                summary.required_cluster_coverage =
                    tier.required_cluster_coverage;
            }
            break;
        }
    }

    if (summary.eligible_cluster_count == 0U) {
        summary.status = RobustHurdleEnvelopeStatus::
            NoEligibleEconomicObservationClusters;
    } else if (!summary.selected_identified_set.has_value()) {
        summary.status = RobustHurdleEnvelopeStatus::
            InsufficientEligibleClustersForDeclaredContaminationBudget;
    } else {
        const auto& selected = *summary.selected_identified_set;
        const auto& selected_tier_result = summary.evidence_tiers[
            tier_position(*summary.selected_evidence_tier)];
        const IdentifiedSetAudit set_audit = audit_identified_set(
            intervals_for_tier(summary, selected_tier_result), selected,
            config.annual_effective_domain_lower,
            config.annual_effective_domain_upper);
        summary.every_selected_component_meets_its_coverage_threshold =
            set_audit.selected_points_and_cells_meet_threshold;
        summary.every_selected_gap_is_below_its_coverage_threshold =
            set_audit.excluded_points_and_cells_are_below_threshold;
        if (equals_domain(selected.components,
                config.annual_effective_domain_lower,
                config.annual_effective_domain_upper)) {
            summary.status = RobustHurdleEnvelopeStatus::
                UninformativeDeclaredDomainIsNotNarrowed;
        } else if (selected.components.empty()) {
            summary.status =
                RobustHurdleEnvelopeStatus::IdentifiedSetIsEmpty;
        } else {
            summary.status = RobustHurdleEnvelopeStatus::IdentifiedSetFound;
        }
        summary.selected_rate_set_is_singleton =
            singleton_set(selected.components);
        const auto& tier = selected_tier_result;
        if (summary.status == RobustHurdleEnvelopeStatus::IdentifiedSetFound &&
            tier.tier ==
                RobustHurdleEvidenceTier::DirectSettledIdenticalClaim &&
            tier.eligible_cluster_count == 1U &&
            config.maximum_contaminated_clusters == 0U) {
            summary.identification_basis =
                RobustHurdleEnvelopeIdentificationBasis::
                    DirectSingleTransactionConditionedCase;
        } else if (summary.status ==
                RobustHurdleEnvelopeStatus::IdentifiedSetFound &&
            tier.comparable_consensus_label_threshold_met) {
            summary.identification_basis =
                RobustHurdleEnvelopeIdentificationBasis::
                    ComparableConsensusThresholdMet;
        } else if (summary.status ==
            RobustHurdleEnvelopeStatus::IdentifiedSetFound) {
            summary.identification_basis =
                RobustHurdleEnvelopeIdentificationBasis::
                    LimitedMultiObservationIdentification;
        }
    }
    summary.model_limitation =
        "Finite model-conditioned annual-effective rate sets show values not ruled out by retained transaction records under declared expected-cash reconstructions and jointly feasible normalization intervals. They do not observe investor beliefs, estimate a holding-period return or statistical confidence, average evidence tiers, establish fair value or demand, or turn physical probabilities into a pricing measure.";
    return summary;
}

} // namespace naturalehia::cellular_finance
