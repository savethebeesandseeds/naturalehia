// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_issue_price_support.hpp>

#include "robust_two_claim_grid_work.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kComparisonAbsoluteTolerance = 1.0e-10;
constexpr double kMaximumAnnualEffectiveHurdleRate = 10.0;
constexpr double kMaximumMoneyMillion = 1.0e9;
constexpr double kMinimumMarketNotionalMillion = 1.0e-6;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::size_t kMaximumReferenceProjectionsPerHurdle = 8U;
constexpr std::string_view kStructuralWorkLimitMessage{
    "issue-price-support combined priority-cap, hurdle-stack, reference-"
    "projection, and scenario-month structural work exceeds the "
    "4,000,000-unit resource bound"};

struct ValidatedInputs {
    std::vector<RobustIssuePriceHurdleCaseConfig> hurdle_cases{};
    detail::RobustTwoClaimGridWorkCounts upstream_work{};
    detail::RobustTwoClaimGridWorkCounts hurdle_stack_work{};
    std::size_t reference_projection_work{0U};
    std::size_t scenario_month_audit_work{0U};
    std::size_t total_work{0U};
    double market_notional_million{0.0};
    double issue_uses_million{0.0};
    double issuer_floor_million{0.0};
};

struct EvaluatedCase {
    RobustIssuePriceSupportCaseResult report{};
    CapitalStackProbabilityPolytopeSummary stack{};
};

enum class PrincipalRiskMetricFamily {
    LegacyLossLayeringV01,
    PrincipalCashShortfallV02,
};

[[nodiscard]] PrincipalRiskMetricFamily principal_risk_metric_family(
    std::string_view model_version) {
    if (model_version == kCapitalStackLegacyModelVersion) {
        return PrincipalRiskMetricFamily::LegacyLossLayeringV01;
    }
    if (model_version == kCapitalStackModelVersion) {
        return PrincipalRiskMetricFamily::PrincipalCashShortfallV02;
    }
    throw std::invalid_argument(
        "issue-price support does not support the capital-stack model version");
}

void require_principal_risk_metric_family(
    const CapitalStackConfig& requested,
    const CapitalStackProbabilityPolytopeSummary& evaluated) {
    const PrincipalRiskMetricFamily family =
        principal_risk_metric_family(requested.model_version);
    const bool legacy_is_applicable =
        family == PrincipalRiskMetricFamily::LegacyLossLayeringV01;
    if (evaluated.model_version != requested.model_version ||
        evaluated.legacy_v01_loss_layering_metrics_are_applicable !=
            legacy_is_applicable) {
        throw std::logic_error(
            "issue-price support capital-stack principal-risk metric family is unavailable");
    }
    for (const CapitalStackProbabilityPolytopeTrancheSummary& tranche :
         evaluated.tranches) {
        if (tranche.legacy_v01_loss_layering_metrics_are_applicable !=
            legacy_is_applicable) {
            throw std::logic_error(
                "issue-price support tranche principal-risk metric family is unavailable");
        }
    }
    for (const CapitalStackScenarioResult& scenario : evaluated.scenarios) {
        for (const CapitalStackTrancheScenarioResult& tranche :
             scenario.tranches) {
            if (tranche.legacy_v01_loss_layering_metrics_are_applicable !=
                legacy_is_applicable) {
                throw std::logic_error(
                    "issue-price support scenario principal-risk metric family is unavailable");
            }
        }
    }
}

[[nodiscard]] ProbabilityPolytopeMetricRange divide_metric_range(
    ProbabilityPolytopeMetricRange value, double divisor) noexcept {
    value.minimum.value /= divisor;
    value.minimum.objective_reconciliation_error /= divisor;
    value.minimum.optimality_residual /= divisor;
    value.central /= divisor;
    value.maximum.value /= divisor;
    value.maximum.objective_reconciliation_error /= divisor;
    value.maximum.optimality_residual /= divisor;
    return value;
}

[[nodiscard]] double comparison_tolerance(
    double first, double second) noexcept {
    return kComparisonAbsoluteTolerance +
        256.0 * std::numeric_limits<double>::epsilon() *
            std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] bool nearly_equal(double first, double second) noexcept {
    return std::abs(first - second) <=
        comparison_tolerance(first, second);
}

[[nodiscard]] bool meets_minimum(double actual, double minimum) noexcept {
    return actual + comparison_tolerance(actual, minimum) >= minimum;
}

[[nodiscard]] bool meets_maximum(double actual, double maximum) noexcept {
    return actual <= maximum + comparison_tolerance(actual, maximum);
}

[[nodiscard]] bool materially_negative(double value) noexcept {
    return value < -comparison_tolerance(value, 0.0);
}

[[nodiscard]] double snap_near_zero(double value) noexcept {
    return nearly_equal(value, 0.0) ? 0.0 : value;
}

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool safe_identifier(std::string_view value) noexcept {
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
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

[[nodiscard]] bool is_placeholder(std::string_view value) noexcept {
    return value == "none" || value.starts_with("unvalidated ") ||
        value.starts_with("unnamed-");
}

[[nodiscard]] bool is_iso_date(std::string_view value) noexcept {
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
    if (year == 0U || month < 1U || month > 12U || day < 1U) {
        return false;
    }
    constexpr std::array<unsigned, 12U> kDaysByMonth{
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    unsigned maximum_day = kDaysByMonth[month - 1U];
    const bool leap_year = year % 4U == 0U &&
        (year % 100U != 0U || year % 400U == 0U);
    if (month == 2U && leap_year) {
        maximum_day = 29U;
    }
    return day <= maximum_day;
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

void require_bounded_money(double value, std::string_view description) {
    require_non_negative(value, description);
    if (value > kMaximumMoneyMillion) {
        throw std::invalid_argument(
            std::string(description) + " exceeds the model money guardrail");
    }
}

void validate_reference_price(
    const RobustIssuePriceReferenceConfig& reference) {
    require_safe_identifier(reference.record_id, "reference-price record id");
    require_safe_identifier(
        reference.market_claim_id, "reference-price market claim id");
    require_safe_identifier(reference.normalized_term_result_id,
        "reference-price normalized term/result id");
    require_bounded_money(
        reference.gross_issue_price_million, "reference gross issue price");
    require_bounded_money(
        reference.claim_quantity_million, "reference claim quantity");
    if (reference.claim_quantity_million == 0.0) {
        throw std::invalid_argument(
            "reference claim quantity must be positive");
    }
    require_safe_text(reference.quantity_basis, "reference quantity basis");
    require_safe_text(reference.price_basis, "reference price basis");
    if (reference.quantity_basis !=
        "full contractual market principal") {
        throw std::invalid_argument(
            "reference quantity basis must be full contractual market principal");
    }
    if (reference.price_basis != "gross buyer cash at month zero") {
        throw std::invalid_argument(
            "reference price basis must be gross buyer cash at month zero");
    }
    require_safe_identifier(
        reference.currency_label, "reference currency label");
    require_safe_text(reference.monetary_basis, "reference monetary basis");
    require_safe_text(reference.execution_date, "reference execution date");
    require_safe_text(reference.settlement_date, "reference settlement date");
    require_bounded_money(reference.issuer_cost_million, "issuer cost");
    require_bounded_money(
        reference.buyer_direct_cost_million, "buyer-direct cost");
    require_safe_text(reference.side_rights_or_non_cash_consideration_note,
        "reference side-rights/non-cash note");
    require_safe_text(
        reference.source_reference, "reference-price source reference");
    require_safe_text(
        reference.evidence_record_id, "reference-price evidence record id");
    require_safe_identifier(reference.issue_use_evidence_record_id,
        "reference issue-use evidence record id");
    const bool issue_use_evidence_claimed =
        reference.subscription_reserve_deposit_evidenced ||
        reference.issuer_cost_payment_evidenced;
    if (issue_use_evidence_claimed &&
        (reference.status !=
                RobustIssuePriceReferenceStatus::SettledPrimary ||
            !reference.buyer_cash_payment_evidenced ||
            !reference.settlement_evidenced)) {
        throw std::invalid_argument(
            "issue-use evidence requires a settled-primary price with evidenced buyer cash and settlement");
    }
    if (issue_use_evidence_claimed &&
        is_placeholder(reference.issue_use_evidence_record_id)) {
        throw std::invalid_argument(
            "issue-use evidence requires a non-placeholder evidence record id");
    }
    if (reference.issuer_cost_payment_evidenced &&
        reference.issuer_cost_million == 0.0) {
        throw std::invalid_argument(
            "issuer-cost payment evidence requires a positive issuer cost");
    }
    if (reference.side_rights_or_non_cash_consideration_present) {
        throw std::invalid_argument(
            "issue-price-support reference price must identify only the fixed market claim without side rights or non-cash consideration");
    }
    if (reference.side_rights_or_non_cash_consideration_note != "none") {
        throw std::invalid_argument(
            "an all-cash issue-price-support reference requires the side-rights/non-cash note to be none");
    }
    if (reference.status !=
            RobustIssuePriceReferenceStatus::SettledSecondary &&
        reference.secondary_price_normalized_to_full_month_zero_claim) {
        throw std::invalid_argument(
            "secondary-price normalization may be asserted only for a settled_secondary reference");
    }
    if (reference.status ==
            RobustIssuePriceReferenceStatus::SettledSecondary &&
        reference.secondary_price_normalized_to_full_month_zero_claim &&
        is_placeholder(reference.normalized_term_result_id)) {
        throw std::invalid_argument(
            "numerical use of a settled-secondary price requires a non-placeholder normalized term/result id");
    }
    if (reference.execution_date != "none" &&
        !is_iso_date(reference.execution_date)) {
        throw std::invalid_argument(
            "reference execution date must be none or a calendar date in YYYY-MM-DD");
    }
    if (reference.settlement_date != "none" &&
        !is_iso_date(reference.settlement_date)) {
        throw std::invalid_argument(
            "reference settlement date must be none or a calendar date in YYYY-MM-DD");
    }
    const bool is_real_price_status = reference.status !=
        RobustIssuePriceReferenceStatus::InternalCandidate;
    if (is_real_price_status &&
        is_placeholder(reference.normalized_term_result_id)) {
        throw std::invalid_argument(
            "non-internal price records require a non-placeholder normalized term/result id");
    }
    const bool status_has_no_transaction_dates = reference.status ==
            RobustIssuePriceReferenceStatus::InternalCandidate ||
        reference.status ==
            RobustIssuePriceReferenceStatus::NonbindingIndication ||
        reference.status ==
            RobustIssuePriceReferenceStatus::BindingUnsettledSubscription;
    if (status_has_no_transaction_dates &&
        (reference.execution_date != "none" ||
            reference.settlement_date != "none")) {
        throw std::invalid_argument(
            "internal, nonbinding, and binding-unsettled price records must not claim execution or settlement dates");
    }
    if (reference.status ==
            RobustIssuePriceReferenceStatus::ExecutedUnsettledPrimary &&
        (!is_iso_date(reference.execution_date) ||
            reference.settlement_date != "none")) {
        throw std::invalid_argument(
            "executed-unsettled primary price requires an execution date and no settlement date");
    }
    const bool settled = reference.status ==
            RobustIssuePriceReferenceStatus::SettledPrimary ||
        reference.status == RobustIssuePriceReferenceStatus::SettledSecondary;
    if (settled &&
        (!is_iso_date(reference.execution_date) ||
            !is_iso_date(reference.settlement_date))) {
        throw std::invalid_argument(
            "settled price records require execution and settlement dates");
    }
    if (settled && reference.settlement_date < reference.execution_date) {
        throw std::invalid_argument(
            "reference settlement date must not precede execution date");
    }
    if (reference.settlement_evidenced &&
        !reference.buyer_cash_payment_evidenced) {
        throw std::invalid_argument(
            "reference-price settlement evidence requires funding evidence");
    }
    const bool unsettled = reference.status ==
            RobustIssuePriceReferenceStatus::BindingUnsettledSubscription ||
        reference.status ==
            RobustIssuePriceReferenceStatus::ExecutedUnsettledPrimary;
    if (unsettled && reference.settlement_evidenced) {
        throw std::invalid_argument(
            "an unsettled reference-price status cannot claim evidenced settlement");
    }
}

void validate_evidence_coherence(
    const RobustIssuePriceSupportConfig& config) {
    const RobustIssuePriceReferenceConfig& reference =
        config.reference_price;
    const RobustIssuePriceSupportCapacityConfig& support = config.support;
    if (config.synthetic_inputs) {
        if (reference.status !=
                RobustIssuePriceReferenceStatus::InternalCandidate ||
            support.status !=
                RobustIssuePriceSupportCapacityStatus::SyntheticCandidate ||
            reference.buyer_cash_payment_evidenced ||
            reference.settlement_evidenced ||
            support.settled_support_million != 0.0 ||
            support.funding_evidenced || support.settlement_evidenced ||
            std::any_of(config.hurdle_cases.begin(),
                config.hurdle_cases.end(), [](const auto& hurdle) {
                    return hurdle.source_type !=
                            RobustIssuePriceHurdleSourceType::
                                SyntheticSensitivity ||
                        hurdle.reference_price_relation !=
                            RobustIssuePriceHurdleReferenceRelation::
                                Independent;
                })) {
            throw std::invalid_argument(
                "synthetic issue-price-support inputs require internal_candidate price, synthetic_candidate support, independent synthetic_sensitivity hurdles, and no observed funding or settlement");
        }
        return;
    }

    const auto require_evidence = [](std::string_view source,
                                      std::string_view evidence,
                                      std::string_view description) {
        if (is_placeholder(source) || is_placeholder(evidence)) {
            throw std::invalid_argument(std::string(description) +
                " requires non-placeholder source and evidence references");
        }
    };
    switch (reference.status) {
    case RobustIssuePriceReferenceStatus::InternalCandidate:
        if (reference.buyer_cash_payment_evidenced ||
            reference.settlement_evidenced) {
            throw std::invalid_argument(
                "candidate, indication, and binding-unsettled price records cannot claim completed funding or settlement");
        }
        break;
    case RobustIssuePriceReferenceStatus::NonbindingIndication:
    case RobustIssuePriceReferenceStatus::BindingUnsettledSubscription:
        require_evidence(reference.source_reference,
            reference.evidence_record_id,
            "nonbinding or binding-unsettled price record");
        if (reference.buyer_cash_payment_evidenced ||
            reference.settlement_evidenced) {
            throw std::invalid_argument(
                "candidate, indication, and binding-unsettled price records cannot claim completed funding or settlement");
        }
        break;
    case RobustIssuePriceReferenceStatus::ExecutedUnsettledPrimary:
        require_evidence(reference.source_reference,
            reference.evidence_record_id, "executed-unsettled primary price");
        if (!is_iso_date(reference.execution_date) ||
            reference.settlement_date != "none" ||
            reference.buyer_cash_payment_evidenced ||
            reference.settlement_evidenced) {
            throw std::invalid_argument(
                "executed-unsettled primary price requires an execution date, no settlement date, and no completed-funding claim");
        }
        break;
    case RobustIssuePriceReferenceStatus::SettledPrimary:
    case RobustIssuePriceReferenceStatus::SettledSecondary:
        require_evidence(reference.source_reference,
            reference.evidence_record_id, "settled price record");
        if (!is_iso_date(reference.execution_date) ||
            !is_iso_date(reference.settlement_date) ||
            !reference.buyer_cash_payment_evidenced ||
            !reference.settlement_evidenced) {
            throw std::invalid_argument(
                "settled price status requires execution and settlement dates plus evidenced funding and settlement");
        }
        break;
    }

    if (support.status !=
        RobustIssuePriceSupportCapacityStatus::SyntheticCandidate) {
        require_evidence(support.source_reference,
            support.evidence_record_id, "non-synthetic support status");
        if (!is_iso_date(support.as_of_date)) {
            throw std::invalid_argument(
                "non-synthetic support status requires an ISO as-of date");
        }
    }
    for (const RobustIssuePriceHurdleCaseConfig& hurdle :
        config.hurdle_cases) {
        if (hurdle.source_type !=
            RobustIssuePriceHurdleSourceType::SyntheticSensitivity) {
            require_evidence(hurdle.source_reference,
                hurdle.evidence_record_id, "non-synthetic hurdle");
        }
    }
}

void validate_support(
    const RobustIssuePriceSupportCapacityConfig& support) {
    require_safe_identifier(support.support_id, "issue-support id");
    require_bounded_money(
        support.maximum_support_million, "maximum issue support");
    require_bounded_money(
        support.settled_support_million, "settled issue support");
    if (support.settled_support_million >
        support.maximum_support_million) {
        throw std::invalid_argument(
            "settled issue support must not exceed maximum support capacity");
    }
    require_safe_text(support.as_of_date, "issue-support as-of date");
    require_safe_text(
        support.source_reference, "issue-support source reference");
    require_safe_text(
        support.evidence_record_id, "issue-support evidence record id");
    require_safe_text(support.source_note, "issue-support source note");
    if (support.as_of_date != "none" && !is_iso_date(support.as_of_date)) {
        throw std::invalid_argument(
            "issue-support as-of date must be none or a calendar date in YYYY-MM-DD");
    }
    if (!support.support_is_non_repayable ||
        !support
             .support_receives_no_repayment_participation_security_or_recovery_rights ||
        !support.support_is_not_project_revenue ||
        !support
             .support_does_not_pay_future_pool_costs_or_cover_project_losses) {
        throw std::invalid_argument(
            "issue-price support must be non-repayable, receive no claim rights, remain outside project revenue, and fund no future costs or losses");
    }
    if (support.status !=
            RobustIssuePriceSupportCapacityStatus::SettledToIssue &&
        support.settled_support_million != 0.0) {
        throw std::invalid_argument(
            "settled issue-support cash must be zero unless support status is settled_to_issue");
    }
    if (support.settlement_evidenced && !support.funding_evidenced) {
        throw std::invalid_argument(
            "issue-support settlement evidence requires funding evidence");
    }
    switch (support.status) {
    case RobustIssuePriceSupportCapacityStatus::SyntheticCandidate:
    case RobustIssuePriceSupportCapacityStatus::NonbindingIndication:
    case RobustIssuePriceSupportCapacityStatus::ContractuallyCommitted:
        if (support.funding_evidenced || support.settlement_evidenced) {
            throw std::invalid_argument(
                "unfunded issue-support capacity status cannot claim funding or settlement evidence");
        }
        break;
    case RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed:
        if (!support.funding_evidenced || support.settlement_evidenced) {
            throw std::invalid_argument(
                "funded_or_escrowed support requires funding evidence and no issue-settlement evidence");
        }
        break;
    case RobustIssuePriceSupportCapacityStatus::SettledToIssue:
        if (!support.funding_evidenced || !support.settlement_evidenced) {
            throw std::invalid_argument(
                "settled_to_issue support requires funding and settlement evidence");
        }
        break;
    }
}

[[nodiscard]] std::vector<RobustIssuePriceHurdleCaseConfig>
canonical_hurdle_cases(
    const std::vector<RobustIssuePriceHurdleCaseConfig>& input) {
    if (input.empty() ||
        input.size() > kRobustIssuePriceSupportMaximumHurdleCases) {
        throw std::invalid_argument(
            "issue-price-support term requires between one and 256 hurdle cases");
    }
    std::vector<RobustIssuePriceHurdleCaseConfig> result = input;
    std::unordered_set<std::string> case_ids;
    case_ids.reserve(result.size());
    bool has_literal_zero = false;
    for (const RobustIssuePriceHurdleCaseConfig& hurdle : result) {
        require_safe_identifier(hurdle.case_id, "investor-hurdle case id");
        if (!case_ids.emplace(hurdle.case_id).second) {
            throw std::invalid_argument(
                "investor-hurdle case ids must be unique");
        }
        require_non_negative(
            hurdle.annual_effective_hurdle_rate, "investor hurdle rate");
        if (hurdle.annual_effective_hurdle_rate >
            kMaximumAnnualEffectiveHurdleRate) {
            throw std::invalid_argument(
                "investor hurdle rates must lie in [0,10]");
        }
        has_literal_zero = has_literal_zero ||
            hurdle.annual_effective_hurdle_rate == 0.0;
        require_safe_text(hurdle.as_of_date, "investor-hurdle as-of date");
        if (!is_iso_date(hurdle.as_of_date)) {
            throw std::invalid_argument(
                "investor-hurdle as-of date must be a calendar date in YYYY-MM-DD");
        }
        require_safe_text(
            hurdle.source_reference, "investor-hurdle source reference");
        require_safe_text(hurdle.evidence_record_id,
            "investor-hurdle evidence record id");
        require_safe_text(hurdle.source_note, "investor-hurdle source note");
    }
    if (!has_literal_zero) {
        throw std::invalid_argument(
            "issue-price-support hurdle cases must contain literal zero");
    }
    std::sort(result.begin(), result.end(),
        [](const RobustIssuePriceHurdleCaseConfig& first,
            const RobustIssuePriceHurdleCaseConfig& second) {
            if (first.annual_effective_hurdle_rate !=
                second.annual_effective_hurdle_rate) {
                return first.annual_effective_hurdle_rate <
                    second.annual_effective_hurdle_rate;
            }
            return first.case_id < second.case_id;
        });
    return result;
}

[[nodiscard]] bool stack_config_equal(
    const CapitalStackConfig& first, const CapitalStackConfig& second) {
    if (first.model_version != second.model_version ||
        first.scenario_label != second.scenario_label ||
        first.source_note != second.source_note ||
        first.synthetic_inputs != second.synthetic_inputs ||
        first.aggregate_commitment_is_fully_funded_at_par_at_month_zero !=
            second.aggregate_commitment_is_fully_funded_at_par_at_month_zero ||
        first.subscription_reserve_is_zero_yield_and_lossless !=
            second.subscription_reserve_is_zero_yield_and_lossless ||
        first.undrawn_commitment_cancels_and_returns_only_at_horizon !=
            second.undrawn_commitment_cancels_and_returns_only_at_horizon ||
        first.pool_costs_are_additional_pro_rata_calls !=
            second.pool_costs_are_additional_pro_rata_calls ||
        first.principal_cash_is_paid_most_senior_first !=
            second.principal_cash_is_paid_most_senior_first ||
        first.nonprincipal_cash_is_paid_to_caps_then_residual !=
            second.nonprincipal_cash_is_paid_to_caps_then_residual ||
        first.tranching_does_not_change_project_cash_or_gross_loss !=
            second.tranching_does_not_change_project_cash_or_gross_loss ||
        first.premium_discount_or_fair_value_is_claimed !=
            second.premium_discount_or_fair_value_is_claimed ||
        first.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero !=
            second.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero ||
        first.buyer_direct_costs_are_additional_pro_rata_calls !=
            second.buyer_direct_costs_are_additional_pro_rata_calls ||
        first.principal_base_cash_above_issued_principal_is_nonprincipal !=
            second.principal_base_cash_above_issued_principal_is_nonprincipal ||
        first.principal_limit_capacity_difference_is_reported_without_valuation_claim !=
            second.principal_limit_capacity_difference_is_reported_without_valuation_claim ||
        first.underlying_success_participation_fraction !=
            second.underlying_success_participation_fraction ||
        first.tranches.size() != second.tranches.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& a = first.tranches[index];
        const CapitalStackTrancheConfig& b = second.tranches[index];
        if (a.id != b.id || a.attachment_million != b.attachment_million ||
            a.detachment_million != b.detachment_million ||
            a.priority_nonprincipal_cap_million !=
                b.priority_nonprincipal_cap_million ||
            a.annual_physical_hurdle_rate !=
                b.annual_physical_hurdle_rate ||
            a.is_first_loss_residual != b.is_first_loss_residual) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ValidatedInputs validate_inputs(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& priority_cap,
    const RobustIssuePriceSupportConfig& issue_price) {
    validate_robust_issue_price_support_config(issue_price);
    validate_robust_market_priority_cap_config(portfolio,
        probability_polytope, participation, base_stack, priority_cap);
    (void)principal_risk_metric_family(base_stack.model_version);
    if (base_stack.tranches.size() != 2U) {
        throw std::invalid_argument(
            "issue-price-support base stack must contain exactly two claims");
    }
    if (issue_price.reference_price.market_claim_id !=
            priority_cap.market_claim_id ||
        issue_price.reference_price.market_claim_id !=
            base_stack.tranches[1].id) {
        throw std::invalid_argument(
            "issue-price-support reference price must identify the fixed market claim");
    }
    if (issue_price.reference_price.currency_label !=
            portfolio.currency_label ||
        issue_price.reference_price.monetary_basis !=
            portfolio.monetary_basis) {
        throw std::invalid_argument(
            "issue-price-support reference currency and monetary basis must exactly match the portfolio");
    }

    ValidatedInputs result;
    result.hurdle_cases = canonical_hurdle_cases(issue_price.hurdle_cases);
    result.market_notional_million =
        base_stack.tranches[1].detachment_million -
        base_stack.tranches[1].attachment_million;
    if (!std::isfinite(result.market_notional_million) ||
        result.market_notional_million < kMinimumMarketNotionalMillion) {
        throw std::invalid_argument(
            "issue-price-support market notional must be at least one base currency unit");
    }
    if (!nearly_equal(issue_price.reference_price.claim_quantity_million,
            result.market_notional_million)) {
        throw std::invalid_argument(
            "reference-price claim quantity must equal the fixed market notional");
    }
    result.issue_uses_million = result.market_notional_million +
        issue_price.reference_price.issuer_cost_million;
    if (!std::isfinite(result.issue_uses_million) ||
        result.issue_uses_million > kMaximumMoneyMillion) {
        throw std::invalid_argument(
            "market principal plus issuer cost must remain finite and within the model money guardrail");
    }
    const double buyer_cash_and_direct_cost =
        issue_price.reference_price.gross_issue_price_million +
        issue_price.reference_price.buyer_direct_cost_million;
    const double observed_source_cash =
        issue_price.reference_price.gross_issue_price_million +
        issue_price.support.settled_support_million;
    if (!std::isfinite(buyer_cash_and_direct_cost) ||
        !std::isfinite(observed_source_cash) ||
        buyer_cash_and_direct_cost > kMaximumMoneyMillion ||
        observed_source_cash > kMaximumMoneyMillion) {
        throw std::invalid_argument(
            "reference buyer cash, buyer-direct cost, and settled support sums must remain finite and within the model money guardrail");
    }
    if (issue_price.reference_price.gross_issue_price_million >
        result.issue_uses_million) {
        throw std::invalid_argument(
            "reference gross issue price must not exceed market principal plus issuer cost");
    }
    if (issue_price.support.maximum_support_million >
        result.issue_uses_million) {
        throw std::invalid_argument(
            "maximum issue support must not exceed market principal plus issuer cost");
    }
    if (issue_price.support.settled_support_million >
        issue_price.support.maximum_support_million) {
        throw std::invalid_argument(
            "settled issue support must not exceed maximum support capacity");
    }
    result.issuer_floor_million = snap_near_zero(
        result.issue_uses_million -
        issue_price.support.maximum_support_million);

    const std::size_t scenario_count =
        probability_polytope.scenario_probabilities.size();
    const std::size_t event_count = probability_polytope.events.size();
    result.upstream_work = detail::checked_robust_two_claim_grid_work(
        portfolio,
        priority_cap.market_priority_nonprincipal_cap_million_grid.size(),
        scenario_count, event_count, portfolio.horizon_months,
        kRobustIssuePriceSupportMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);
    result.hurdle_stack_work = detail::checked_robust_two_claim_grid_work(
        portfolio, result.hurdle_cases.size(), scenario_count, event_count,
        portfolio.horizon_months,
        kRobustIssuePriceSupportMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);

    if (event_count >= std::numeric_limits<std::size_t>::max() -
            scenario_count) {
        throw std::invalid_argument(
            "issue-price-support scenario and event counts overflow structural-work calculation");
    }
    const std::size_t structural_width =
        scenario_count + event_count + 1U;
    result.reference_projection_work = detail::checked_grid_product(
        {result.hurdle_cases.size(),
            kMaximumReferenceProjectionsPerHurdle, scenario_count,
            structural_width},
        kRobustIssuePriceSupportMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);
    result.scenario_month_audit_work = detail::checked_grid_product(
        {result.hurdle_cases.size(), scenario_count, 3U,
            portfolio.horizon_months + 1U},
        kRobustIssuePriceSupportMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);
    result.total_work = detail::checked_grid_sum(
        result.upstream_work.total,
        detail::checked_grid_sum(result.hurdle_stack_work.total,
            detail::checked_grid_sum(result.reference_projection_work,
                result.scenario_month_audit_work,
                kRobustIssuePriceSupportMaximumStructuralWorkUnits,
                kStructuralWorkLimitMessage),
            kRobustIssuePriceSupportMaximumStructuralWorkUnits,
            kStructuralWorkLimitMessage),
        kRobustIssuePriceSupportMaximumStructuralWorkUnits,
        kStructuralWorkLimitMessage);
    return result;
}

} // namespace

std::string_view to_string(RobustIssuePriceReferenceStatus status) noexcept {
    switch (status) {
    case RobustIssuePriceReferenceStatus::InternalCandidate:
        return "internal_candidate";
    case RobustIssuePriceReferenceStatus::NonbindingIndication:
        return "nonbinding_indication";
    case RobustIssuePriceReferenceStatus::BindingUnsettledSubscription:
        return "binding_unsettled_subscription";
    case RobustIssuePriceReferenceStatus::ExecutedUnsettledPrimary:
        return "executed_unsettled_primary";
    case RobustIssuePriceReferenceStatus::SettledPrimary:
        return "settled_primary";
    case RobustIssuePriceReferenceStatus::SettledSecondary:
        return "settled_secondary";
    }
    return "unknown";
}

std::string_view to_string(
    RobustIssuePriceSupportCapacityStatus status) noexcept {
    switch (status) {
    case RobustIssuePriceSupportCapacityStatus::SyntheticCandidate:
        return "synthetic_candidate";
    case RobustIssuePriceSupportCapacityStatus::NonbindingIndication:
        return "nonbinding_indication";
    case RobustIssuePriceSupportCapacityStatus::ContractuallyCommitted:
        return "contractually_committed";
    case RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed:
        return "funded_or_escrowed";
    case RobustIssuePriceSupportCapacityStatus::SettledToIssue:
        return "settled_to_issue";
    }
    return "unknown";
}

std::string_view to_string(
    RobustIssuePriceHurdleSourceType source) noexcept {
    switch (source) {
    case RobustIssuePriceHurdleSourceType::SameClaimMarketObservation:
        return "same_claim_market_observation";
    case RobustIssuePriceHurdleSourceType::ComparableMarketObservation:
        return "comparable_market_observation";
    case RobustIssuePriceHurdleSourceType::ModelAdjustedComparable:
        return "model_adjusted_comparable";
    case RobustIssuePriceHurdleSourceType::InvestorTarget:
        return "investor_target";
    case RobustIssuePriceHurdleSourceType::PolicyTarget:
        return "policy_target";
    case RobustIssuePriceHurdleSourceType::SyntheticSensitivity:
        return "synthetic_sensitivity";
    }
    return "unknown";
}

std::string_view to_string(
    RobustIssuePriceHurdleReferenceRelation relation) noexcept {
    switch (relation) {
    case RobustIssuePriceHurdleReferenceRelation::Independent:
        return "independent";
    case RobustIssuePriceHurdleReferenceRelation::
        ModelImpliedFromReferencePrice:
        return "model_implied_from_reference_price";
    case RobustIssuePriceHurdleReferenceRelation::Unresolved:
        return "unresolved";
    }
    return "unknown";
}

std::string_view to_string(
    RobustIssuePriceSupportCaseStatus status) noexcept {
    switch (status) {
    case RobustIssuePriceSupportCaseStatus::
        HurdleNotIndependentOfReferencePrice:
        return "hurdle-not-independent-of-reference-price";
    case RobustIssuePriceSupportCaseStatus::NoNonnegativeInvestorPrice:
        return "no-nonnegative-investor-price";
    case RobustIssuePriceSupportCaseStatus::
        InvestorAndIssuerRequirementsDoNotOverlap:
        return "investor-and-issuer-requirements-do-not-overlap";
    case RobustIssuePriceSupportCaseStatus::FinanceablePriceWindow:
        return "financeable-price-window";
    }
    return "unknown";
}

std::string_view to_string(RobustIssuePriceSupportStatus status) noexcept {
    switch (status) {
    case RobustIssuePriceSupportStatus::PriorityCapSelectionUnavailable:
        return "priority-cap-selection-unavailable";
    case RobustIssuePriceSupportStatus::NoFinanceableWindow:
        return "no-financeable-window";
    case RobustIssuePriceSupportStatus::FinanceableWindowFound:
        return "financeable-window-found";
    }
    return "unknown";
}

void validate_robust_issue_price_support_config(
    const RobustIssuePriceSupportConfig& config) {
    if (config.model_version != kRobustIssuePriceSupportModelVersion) {
        throw std::invalid_argument(
            "unsupported robust issue-price-support model version");
    }
    require_safe_text(config.scenario_label, "issue-price-support label");
    require_safe_text(config.source_note, "issue-price-support source note");
    if (!config.market_claim_principal_is_fully_funded_at_issue ||
        !config.issue_support_and_price_fund_only_principal_and_issuer_costs ||
        !config.buyer_direct_cost_stays_outside_subscription_reserve ||
        !config.support_changes_no_claim_right_or_project_cash ||
        !config.physical_probability_polytope_is_unchanged ||
        config.fair_value_or_market_price_is_estimated) {
        throw std::invalid_argument(
            "issue-price-support transaction assertions must preserve full principal funding, no-rights support, buyer-direct costs, project cash, physical probabilities, and the no-fair-value boundary");
    }
    validate_reference_price(config.reference_price);
    validate_support(config.support);
    (void)canonical_hurdle_cases(config.hurdle_cases);
    validate_evidence_coherence(config);
}

void validate_robust_issue_price_support_config(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& priority_cap,
    const RobustIssuePriceSupportConfig& issue_price) {
    (void)validate_inputs(portfolio, probability_polytope, participation,
        base_stack, priority_cap, issue_price);
}

} // namespace naturalehia::cellular_finance

namespace naturalehia::cellular_finance {
namespace {

void copy_stack_audits(const CapitalStackProbabilityPolytopeSummary& stack,
    RobustMarketPriorityCapCandidateAudit& audit) {
    audit.maximum_commitment_identity_error_million =
        stack.maximum_commitment_identity_error_million;
    audit.maximum_reserve_roll_forward_error_million =
        stack.maximum_reserve_roll_forward_error_million;
    audit.maximum_reserve_shortfall_million =
        stack.maximum_reserve_shortfall_million;
    audit.maximum_subscription_reconciliation_error_million =
        stack.maximum_subscription_reconciliation_error_million;
    audit.maximum_pool_cost_call_reconciliation_error_million =
        stack.maximum_pool_cost_call_reconciliation_error_million;
    audit.maximum_principal_distribution_reconciliation_error_million =
        stack.maximum_principal_distribution_reconciliation_error_million;
    audit.maximum_nonprincipal_distribution_reconciliation_error_million =
        stack.maximum_nonprincipal_distribution_reconciliation_error_million;
    audit.maximum_priority_nonprincipal_cap_violation_million =
        stack.maximum_priority_nonprincipal_cap_violation_million;
    audit.maximum_realized_loss_reconciliation_error_million =
        stack.maximum_realized_loss_reconciliation_error_million;
    audit.maximum_unresolved_exposure_reconciliation_error_million =
        stack.maximum_unresolved_exposure_reconciliation_error_million;
    audit.maximum_nominal_net_cash_reconciliation_error_million =
        stack.maximum_nominal_net_cash_reconciliation_error_million;
    audit.maximum_stack_npv_reconciliation_error_million =
        stack.maximum_stack_npv_reconciliation_error_million;
    audit.maximum_probability_constraint_violation =
        stack.maximum_probability_constraint_violation;
    audit.maximum_objective_reconciliation_error =
        stack.maximum_objective_reconciliation_error;
    audit.maximum_reduced_cost_optimality_residual =
        stack.maximum_reduced_cost_optimality_residual;
    audit.maximum_tail_mass_violation = stack.maximum_tail_mass_violation;
    audit.maximum_tail_objective_reconciliation_error =
        stack.maximum_tail_objective_reconciliation_error;
    audit.maximum_tail_threshold_formula_reconciliation_error =
        stack.maximum_tail_threshold_formula_reconciliation_error;
    audit.maximum_tail_threshold_enumeration_optimality_residual =
        stack.maximum_tail_threshold_enumeration_optimality_residual;
    audit.maximum_wal_numerator_reconciliation_error_million_years =
        stack.maximum_wal_numerator_reconciliation_error_million_years;
    audit.maximum_wal_denominator_reconciliation_error_million =
        stack.maximum_wal_denominator_reconciliation_error_million;
    audit.maximum_wal_ratio_reconciliation_error_years =
        stack.maximum_wal_ratio_reconciliation_error_years;
    audit.maximum_wal_root_objective_reconciliation_error_million_years =
        stack.maximum_wal_root_objective_reconciliation_error_million_years;
    audit.maximum_wal_root_objective_absolute_residual_million_years =
        stack.maximum_wal_root_objective_absolute_residual_million_years;
}

void update_linear_audits(const ProbabilityPolytopeMetricProjection& value,
    RobustMarketPriorityCapCandidateAudit& audit) {
    audit.maximum_probability_constraint_violation = std::max(
        audit.maximum_probability_constraint_violation,
        value.maximum_endpoint_constraint_violation);
    audit.maximum_objective_reconciliation_error = std::max(
        audit.maximum_objective_reconciliation_error,
        value.maximum_endpoint_objective_reconciliation_error);
    audit.maximum_reduced_cost_optimality_residual = std::max(
        audit.maximum_reduced_cost_optimality_residual,
        value.maximum_endpoint_optimality_residual);
}

void update_tail_audits(
    const ProbabilityPolytopeUpperExpectedShortfallProjection& value,
    RobustMarketPriorityCapCandidateAudit& audit) {
    audit.maximum_probability_constraint_violation = std::max(
        audit.maximum_probability_constraint_violation,
        value.maximum_endpoint_constraint_violation);
    audit.maximum_tail_mass_violation = std::max(
        audit.maximum_tail_mass_violation,
        value.maximum_endpoint_tail_mass_violation);
    audit.maximum_tail_objective_reconciliation_error = std::max(
        audit.maximum_tail_objective_reconciliation_error,
        value.maximum_endpoint_objective_reconciliation_error);
    audit.maximum_tail_threshold_formula_reconciliation_error = std::max(
        audit.maximum_tail_threshold_formula_reconciliation_error,
        value.maximum_endpoint_threshold_formula_reconciliation_error);
    audit.maximum_reduced_cost_optimality_residual = std::max(
        audit.maximum_reduced_cost_optimality_residual,
        value.maximum_endpoint_optimality_residual);
    audit.maximum_tail_threshold_enumeration_optimality_residual = std::max(
        audit.maximum_tail_threshold_enumeration_optimality_residual,
        value.maximum_threshold_enumeration_optimality_residual);
}

[[nodiscard]] std::vector<ProbabilityPolytopeScenarioValue> scenario_values(
    const CapitalStackProbabilityPolytopeSummary& stack,
    const std::vector<double>& values) {
    if (stack.scenarios.size() != values.size()) {
        throw std::logic_error(
            "issue-price-support scenario values lost stack alignment");
    }
    std::vector<ProbabilityPolytopeScenarioValue> result;
    result.reserve(values.size());
    for (std::size_t index = 0U; index < values.size(); ++index) {
        result.push_back(ProbabilityPolytopeScenarioValue{
            stack.scenarios[index].scenario_id, values[index]});
    }
    return result;
}

[[nodiscard]] bool scenario_probabilities_equal(
    const std::vector<ProbabilityPolytopeScenario>& first,
    const std::vector<ProbabilityPolytopeScenario>& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.size(); ++index) {
        const ProbabilityPolytopeScenario& a = first[index];
        const ProbabilityPolytopeScenario& b = second[index];
        if (a.scenario_id != b.scenario_id ||
            a.lower_weight != b.lower_weight ||
            a.central_weight != b.central_weight ||
            a.upper_weight != b.upper_weight) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool events_equal(
    const std::vector<ProbabilityEventConstraint>& first,
    const std::vector<ProbabilityEventConstraint>& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.size(); ++index) {
        const ProbabilityEventConstraint& a = first[index];
        const ProbabilityEventConstraint& b = second[index];
        if (a.event_id != b.event_id || a.definition != b.definition ||
            a.lower_probability != b.lower_probability ||
            a.upper_probability != b.upper_probability ||
            a.scenario_ids != b.scenario_ids) {
            return false;
        }
    }
    return true;
}

void update_change(double first, double second, double& maximum,
    bool& invariant) {
    const double change = std::abs(first - second);
    maximum = std::max(maximum, change);
    invariant = invariant &&
        change <= comparison_tolerance(first, second);
}

void update_range_change(const ProbabilityPolytopeMetricRange& first,
    const ProbabilityPolytopeMetricRange& second, double& maximum,
    bool& invariant) {
    update_change(first.minimum.value, second.minimum.value, maximum,
        invariant);
    update_change(first.central, second.central, maximum, invariant);
    update_change(first.maximum.value, second.maximum.value, maximum,
        invariant);
}

void update_tail_change(
    const ProbabilityPolytopeUpperExpectedShortfallProjection& first,
    const ProbabilityPolytopeUpperExpectedShortfallProjection& second,
    double& maximum, bool& invariant) {
    update_change(first.minimum.value, second.minimum.value, maximum,
        invariant);
    update_change(first.central, second.central, maximum, invariant);
    update_change(first.maximum.value, second.maximum.value, maximum,
        invariant);
}

void update_wal_change(
    const std::optional<CapitalStackProbabilityPolytopeWalRange>& first,
    const std::optional<CapitalStackProbabilityPolytopeWalRange>& second,
    double& maximum, bool& invariant) {
    if (first.has_value() != second.has_value()) {
        maximum = std::numeric_limits<double>::infinity();
        invariant = false;
        return;
    }
    if (!first.has_value()) {
        return;
    }
    update_change(first->minimum.value_years, second->minimum.value_years,
        maximum, invariant);
    update_change(
        first->central_years, second->central_years, maximum, invariant);
    update_change(first->maximum.value_years, second->maximum.value_years,
        maximum, invariant);
}

void audit_sparse_market_ledger(
    const CapitalStackProbabilityPolytopeSummary& stack,
    std::size_t horizon_months, double market_notional_million,
    RobustIssuePriceSupportCaseAudit& audit) {
    audit.sparse_market_monthly_ledger_reconciles = true;
    for (const CapitalStackScenarioResult& scenario : stack.scenarios) {
        if (scenario.tranches.size() != 2U) {
            audit.sparse_market_monthly_ledger_reconciles = false;
            audit.maximum_sparse_monthly_ledger_error_million =
                std::numeric_limits<double>::infinity();
            continue;
        }
        const CapitalStackTrancheScenarioResult& market =
            scenario.tranches[1];
        long double subscription = 0.0L;
        long double cost_calls = 0.0L;
        long double principal = 0.0L;
        long double nonprincipal = 0.0L;
        long double net = 0.0L;
        bool first_row = true;
        std::size_t previous_month = 0U;
        for (const CapitalStackMonthlyTrancheCashFlow& row :
            market.monthly_cash_flows) {
            if (row.month > horizon_months ||
                (!first_row && row.month <= previous_month) ||
                (row.month != 0U && row.par_subscription_million != 0.0)) {
                audit.sparse_market_monthly_ledger_reconciles = false;
                audit.maximum_sparse_monthly_ledger_error_million =
                    std::numeric_limits<double>::infinity();
            }
            first_row = false;
            previous_month = row.month;
            const double recomputed_net =
                row.principal_cash_distribution_million +
                row.nonprincipal_cash_distribution_million -
                row.par_subscription_million -
                row.pro_rata_pool_cost_call_million;
            const double row_error =
                std::abs(recomputed_net - row.net_cash_flow_million);
            audit.maximum_sparse_monthly_ledger_error_million = std::max(
                audit.maximum_sparse_monthly_ledger_error_million,
                row_error);
            audit.sparse_market_monthly_ledger_reconciles =
                audit.sparse_market_monthly_ledger_reconciles &&
                row_error <= comparison_tolerance(
                    recomputed_net, row.net_cash_flow_million);
            subscription += row.par_subscription_million;
            cost_calls += row.pro_rata_pool_cost_call_million;
            principal += row.principal_cash_distribution_million;
            nonprincipal += row.nonprincipal_cash_distribution_million;
            net += row.net_cash_flow_million;
        }

        const auto reconcile = [&audit](long double accumulated,
                                   double reported) {
            const double accumulated_double =
                static_cast<double>(accumulated);
            const double error = std::abs(accumulated_double - reported);
            audit.maximum_sparse_monthly_ledger_error_million = std::max(
                audit.maximum_sparse_monthly_ledger_error_million, error);
            audit.sparse_market_monthly_ledger_reconciles =
                audit.sparse_market_monthly_ledger_reconciles &&
                error <= comparison_tolerance(accumulated_double, reported);
        };
        reconcile(subscription, market.par_subscription_million);
        reconcile(subscription, market_notional_million);
        reconcile(cost_calls, market.pro_rata_pool_cost_calls_million);
        reconcile(principal, market.principal_cash_distribution_million);
        reconcile(
            nonprincipal, market.nonprincipal_cash_distribution_million);
        reconcile(principal + nonprincipal,
            market.total_distributions_million);
        reconcile(subscription + cost_calls,
            market.total_contributions_million);
        reconcile(net, market.nominal_net_cash_million);
    }
}

[[nodiscard]] ProbabilityPolytopeMetricRange shift_range(
    ProbabilityPolytopeMetricRange value, double shift) {
    value.minimum.value += shift;
    value.central += shift;
    value.maximum.value += shift;
    return value;
}

[[nodiscard]] EvaluatedCase evaluate_case(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& selected_stack,
    const RobustIssuePriceSupportConfig& terms,
    const RobustIssuePriceHurdleCaseConfig& hurdle,
    const ProbabilityPolytopeProjector& projector,
    double market_notional_million, double issue_uses_million,
    double issuer_floor_million) {
    CapitalStackConfig hurdle_stack = selected_stack;
    hurdle_stack.tranches[1].annual_physical_hurdle_rate =
        hurdle.annual_effective_hurdle_rate;

    EvaluatedCase result;
    result.stack = evaluate_capital_stack_probability_polytope(portfolio,
        probability_polytope, participation, hurdle_stack);
    require_principal_risk_metric_family(hurdle_stack, result.stack);
    if (result.stack.tranches.size() != 2U ||
        result.stack.tranches[0].tranche_id !=
            selected_stack.tranches[0].id ||
        result.stack.tranches[1].tranche_id !=
            selected_stack.tranches[1].id) {
        throw std::logic_error(
            "issue-price-support hurdle evaluation lost fixed claim ordering");
    }

    RobustIssuePriceSupportCaseResult& report = result.report;
    report.case_id = hurdle.case_id;
    report.annual_effective_hurdle_rate =
        hurdle.annual_effective_hurdle_rate;
    report.hurdle_source_type = hurdle.source_type;
    report.hurdle_reference_price_relation =
        hurdle.reference_price_relation;
    report.hurdle_as_of_date = hurdle.as_of_date;
    report.hurdle_source_reference = hurdle.source_reference;
    report.hurdle_evidence_record_id = hurdle.evidence_record_id;
    report.hurdle_source_note = hurdle.source_note;

    const CapitalStackProbabilityPolytopeTrancheSummary& market =
        result.stack.tranches[1];
    report.market_par_npv_million =
        market.expected_npv_at_tranche_hurdle_million;
    const double buyer_direct_cost =
        terms.reference_price.buyer_direct_cost_million;
    report.raw_robust_investor_price_ceiling_million =
        market_notional_million - buyer_direct_cost +
        report.market_par_npv_million.minimum.value;
    report.raw_central_investor_price_boundary_million =
        market_notional_million - buyer_direct_cost +
        report.market_par_npv_million.central;
    report.raw_maximum_investor_price_boundary_million =
        market_notional_million - buyer_direct_cost +
        report.market_par_npv_million.maximum.value;
    report.raw_robust_investor_price_ceiling_million = snap_near_zero(
        report.raw_robust_investor_price_ceiling_million);
    report.raw_central_investor_price_boundary_million = snap_near_zero(
        report.raw_central_investor_price_boundary_million);
    report.raw_maximum_investor_price_boundary_million = snap_near_zero(
        report.raw_maximum_investor_price_boundary_million);
    report.admissible_investor_price_ceiling_million = std::min(
        issue_uses_million,
        report.raw_robust_investor_price_ceiling_million);
    if (nearly_equal(report.admissible_investor_price_ceiling_million,
            issue_uses_million)) {
        report.admissible_investor_price_ceiling_million = issue_uses_million;
    }
    report.issuer_funding_floor_million = issuer_floor_million;
    if (report.admissible_investor_price_ceiling_million <
            issuer_floor_million &&
        meets_minimum(report.admissible_investor_price_ceiling_million,
            issuer_floor_million)) {
        report.admissible_investor_price_ceiling_million =
            issuer_floor_million;
    }
    if (!materially_negative(
            report.raw_robust_investor_price_ceiling_million)) {
        report.minimum_support_capacity_for_overlap_million =
            snap_near_zero(issue_uses_million -
                report.admissible_investor_price_ceiling_million);
        report.support_shortfall_million = std::max(0.0,
            snap_near_zero(
                *report.minimum_support_capacity_for_overlap_million -
                terms.support.maximum_support_million));
    }

    if (hurdle.reference_price_relation !=
        RobustIssuePriceHurdleReferenceRelation::Independent) {
        report.status = RobustIssuePriceSupportCaseStatus::
            HurdleNotIndependentOfReferencePrice;
    } else if (materially_negative(
            report.raw_robust_investor_price_ceiling_million)) {
        report.status =
            RobustIssuePriceSupportCaseStatus::NoNonnegativeInvestorPrice;
    } else if (meets_minimum(
                   report.admissible_investor_price_ceiling_million,
                   issuer_floor_million)) {
        report.status =
            RobustIssuePriceSupportCaseStatus::FinanceablePriceWindow;
        report.modeled_financeable_price_window_exists = true;
        report.financeable_price_window_lower_million = issuer_floor_million;
        report.financeable_price_window_upper_million =
            report.admissible_investor_price_ceiling_million;
    } else {
        report.status = RobustIssuePriceSupportCaseStatus::
            InvestorAndIssuerRequirementsDoNotOverlap;
    }

    const bool support_needed_for_overlap =
        report.minimum_support_capacity_for_overlap_million.has_value() &&
        *report.minimum_support_capacity_for_overlap_million >
            comparison_tolerance(
                *report.minimum_support_capacity_for_overlap_million, 0.0);
    const bool support_is_documented_commitment =
        terms.support.status ==
            RobustIssuePriceSupportCapacityStatus::ContractuallyCommitted ||
        terms.support.status ==
            RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed ||
        terms.support.status ==
            RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    const bool support_is_funded =
        terms.support.status ==
            RobustIssuePriceSupportCapacityStatus::FundedOrEscrowed ||
        terms.support.status ==
            RobustIssuePriceSupportCapacityStatus::SettledToIssue;
    report.modeled_overlap_exists_without_support =
        report.modeled_financeable_price_window_exists &&
        report.minimum_support_capacity_for_overlap_million.has_value() &&
        !support_needed_for_overlap;
    report.documented_support_commitment_covers_overlap =
        report.modeled_financeable_price_window_exists &&
        support_needed_for_overlap && !terms.synthetic_inputs &&
        support_is_documented_commitment;
    report.funded_support_capacity_covers_overlap =
        report.modeled_financeable_price_window_exists &&
        support_needed_for_overlap && !terms.synthetic_inputs &&
        support_is_funded;
    report.funded_support_covered_price_window_exists =
        report.funded_support_capacity_covers_overlap;

    RobustIssuePriceSupportPrincipalRiskMetrics& risk =
        report.principal_risk;
    risk.contractual_market_notional_million = market_notional_million;
    risk.expected_principal_cash_distribution_million =
        market.expected_principal_cash_distribution_million;
    const PrincipalRiskMetricFamily risk_family =
        principal_risk_metric_family(hurdle_stack.model_version);
    if (risk_family == PrincipalRiskMetricFamily::LegacyLossLayeringV01) {
        risk.expected_principal_loss_fraction =
            market.expected_realized_principal_loss_fraction;
        risk.principal_loss_es95_million =
            market.principal_loss_expected_shortfall_95_million;
        risk.principal_loss_es99_million =
            market.principal_loss_expected_shortfall_99_million;
        risk.principal_impairment_probability =
            market.principal_impairment_probability;
    } else {
        // Compatibility result names retain their v0.1 spelling. Under the
        // v0.2 explicit asset/liability bridge, their controlling investor-
        // risk quantities are issued-principal cash shortfall Q, its upper
        // tails, and Pr[Q>0].
        risk.expected_principal_loss_fraction = divide_metric_range(
            market.expected_principal_cash_shortfall_million,
            market_notional_million);
        risk.principal_loss_es95_million =
            market.principal_cash_shortfall_expected_shortfall_95_million;
        risk.principal_loss_es99_million =
            market.principal_cash_shortfall_expected_shortfall_99_million;
        risk.principal_impairment_probability =
            market.principal_cash_shortfall_probability;
    }
    risk.worst_principal_loss_es95_fraction =
        risk.principal_loss_es95_million.maximum.value /
        market_notional_million;
    risk.worst_principal_loss_es99_fraction =
        risk.principal_loss_es99_million.maximum.value /
        market_notional_million;
    risk.principal_cash_wal_years =
        market.principal_cash_weighted_average_life_years;

    RobustMarketPriorityCapCandidateAudit& numerical =
        report.audit.numerical_audit;
    copy_stack_audits(result.stack, numerical);
    report.audit.raw_price_ceiling_zero_npv_error_million = std::abs(
        report.market_par_npv_million.minimum.value + market_notional_million -
        buyer_direct_cost -
        report.raw_robust_investor_price_ceiling_million);
    report.audit.raw_price_ceiling_zero_npv_reconciles =
        report.audit.raw_price_ceiling_zero_npv_error_million <=
        comparison_tolerance(
            report.raw_robust_investor_price_ceiling_million, 0.0);
    audit_sparse_market_ledger(result.stack, portfolio.horizon_months,
        market_notional_million, report.audit);

    report.reference_price_numerically_eligible =
        terms.reference_price.status !=
            RobustIssuePriceReferenceStatus::SettledSecondary ||
        terms.reference_price
            .secondary_price_normalized_to_full_month_zero_claim;
    if (!report.reference_price_numerically_eligible) {
        report.reference_price_numerical_block_reason =
            "settled-secondary price lacks explicit normalization to the full month-zero fixed claim";
        return result;
    }
    report.reference_price_numerical_block_reason.clear();

    std::vector<double> actual_contributions;
    std::vector<double> reference_npvs;
    std::vector<double> npv_margins;
    std::vector<double> cash_multiples;
    std::vector<double> net_returns;
    std::vector<double> negative_npv_indicators;
    std::vector<double> npv_shortfalls;
    actual_contributions.reserve(result.stack.scenarios.size());
    reference_npvs.reserve(result.stack.scenarios.size());
    npv_margins.reserve(result.stack.scenarios.size());
    cash_multiples.reserve(result.stack.scenarios.size());
    net_returns.reserve(result.stack.scenarios.size());
    negative_npv_indicators.reserve(result.stack.scenarios.size());
    npv_shortfalls.reserve(result.stack.scenarios.size());
    bool ratios_available = true;
    const double reference_price =
        terms.reference_price.gross_issue_price_million;
    const double npv_shift =
        market_notional_million - reference_price - buyer_direct_cost;
    for (const CapitalStackScenarioResult& scenario : result.stack.scenarios) {
        if (scenario.tranches.size() != 2U) {
            throw std::logic_error(
                "issue-price-support scenario lost fixed two-claim structure");
        }
        const CapitalStackTrancheScenarioResult& market_scenario =
            scenario.tranches[1];
        const double contributions = reference_price + buyer_direct_cost +
            market_scenario.pro_rata_pool_cost_calls_million;
        const double npv = market_scenario.npv_at_tranche_hurdle_million +
            npv_shift;
        if (!std::isfinite(contributions) || !std::isfinite(npv)) {
            throw std::runtime_error(
                "issue-price-support reference cash calculation is non-finite");
        }
        actual_contributions.push_back(contributions);
        reference_npvs.push_back(npv);
        negative_npv_indicators.push_back(
            materially_negative(npv) ? 1.0 : 0.0);
        npv_shortfalls.push_back(materially_negative(npv) ? -npv : 0.0);
        if (contributions <= comparison_tolerance(contributions, 0.0)) {
            ratios_available = false;
        } else {
            const double multiple =
                market_scenario.total_distributions_million / contributions;
            const double margin = npv / contributions;
            if (!std::isfinite(multiple) || !std::isfinite(margin)) {
                throw std::runtime_error(
                    "issue-price-support reference return calculation is non-finite");
            }
            cash_multiples.push_back(multiple);
            net_returns.push_back(multiple - 1.0);
            npv_margins.push_back(margin);
        }
    }

    RobustIssuePriceSupportReferenceMetrics& reference =
        report.reference_price.emplace();

    const ProbabilityPolytopeMetricProjection contributions_projection =
        projector.project_expectation(
            scenario_values(result.stack, actual_contributions));
    reference.expected_investor_contributions_million =
        contributions_projection.expectation;
    update_linear_audits(contributions_projection, numerical);

    reference.expected_distributions_million =
        market.expected_total_distributions_million;
    const ProbabilityPolytopeMetricProjection npv_projection =
        projector.project_expectation(
            scenario_values(result.stack, reference_npvs));
    reference.investor_npv_million = npv_projection.expectation;
    update_linear_audits(npv_projection, numerical);

    if (ratios_available) {
        const ProbabilityPolytopeMetricProjection margin_projection =
            projector.project_expectation(
                scenario_values(result.stack, npv_margins));
        reference.expected_scenario_npv_margin_fraction =
            margin_projection.expectation;
        reference.robust_expected_scenario_npv_margin_fraction =
            margin_projection.expectation.minimum.value;
        update_linear_audits(margin_projection, numerical);

        const ProbabilityPolytopeMetricProjection multiple_projection =
            projector.project_expectation(
                scenario_values(result.stack, cash_multiples));
        reference.expected_scenario_cash_multiple =
            multiple_projection.expectation;
        update_linear_audits(multiple_projection, numerical);

        const ProbabilityPolytopeMetricProjection return_projection =
            projector.project_expectation(
                scenario_values(result.stack, net_returns));
        reference.expected_scenario_net_return_fraction =
            return_projection.expectation;
        update_linear_audits(return_projection, numerical);
    }

    const ProbabilityPolytopeMetricProjection negative_projection =
        projector.project_expectation(
            scenario_values(result.stack, negative_npv_indicators));
    reference.negative_npv_probability = negative_projection.expectation;
    update_linear_audits(negative_projection, numerical);

    reference.npv_shortfall_es95_million =
        projector.project_upper_expected_shortfall(
            scenario_values(result.stack, npv_shortfalls), 0.05);
    reference.npv_shortfall_es99_million =
        projector.project_upper_expected_shortfall(
            scenario_values(result.stack, npv_shortfalls), 0.01);
    update_tail_audits(reference.npv_shortfall_es95_million, numerical);
    update_tail_audits(reference.npv_shortfall_es99_million, numerical);

    reference.robust_investor_npv_million =
        reference.investor_npv_million.minimum.value;
    reference.investor_term_adequate =
        hurdle.reference_price_relation ==
            RobustIssuePriceHurdleReferenceRelation::Independent &&
        meets_minimum(reference.robust_investor_npv_million, 0.0);
    reference.required_issue_support_million =
        snap_near_zero(issue_uses_million - reference_price);
    reference.observed_settled_support_million =
        terms.support.settled_support_million;
    reference.support_capacity_margin_million =
        snap_near_zero(terms.support.maximum_support_million -
            reference.required_issue_support_million);
    reference.unused_support_capacity_million =
        std::max(0.0, reference.support_capacity_margin_million);
    reference.support_capacity_shortfall_million =
        std::max(0.0, -reference.support_capacity_margin_million);
    reference.modeled_full_funding_adequate =
        meets_maximum(reference.required_issue_support_million,
            terms.support.maximum_support_million);
    reference.modeled_joint_term_adequate =
        reference.investor_term_adequate &&
        reference.modeled_full_funding_adequate;
    reference.modeled_required_issue_sources_million = reference_price +
        reference.required_issue_support_million;
    reference.issue_uses_million = issue_uses_million;
    reference.modeled_amount_entering_subscription_reserve_million =
        market_notional_million;
    reference.modeled_issuer_cost_paid_million =
        terms.reference_price.issuer_cost_million;
    reference.buyer_direct_cost_outside_reserve_million = buyer_direct_cost;
    reference.issue_funding_identity_error_million =
        std::abs(reference.modeled_required_issue_sources_million -
            issue_uses_million);

    reference.reference_is_settled_primary =
        terms.reference_price.status ==
        RobustIssuePriceReferenceStatus::SettledPrimary;
    reference.reference_is_settled_secondary =
        terms.reference_price.status ==
        RobustIssuePriceReferenceStatus::SettledSecondary;
    reference.observed_primary_price_cash_completed =
        reference.reference_is_settled_primary &&
        !terms.synthetic_inputs &&
        terms.reference_price.buyer_cash_payment_evidenced &&
        terms.reference_price.settlement_evidenced;
    const bool observed_support_cash_evidenced =
        reference.reference_is_settled_primary && !terms.synthetic_inputs &&
        terms.support.status ==
            RobustIssuePriceSupportCapacityStatus::SettledToIssue &&
        terms.support.funding_evidenced &&
        terms.support.settlement_evidenced;
    reference.observed_support_cash_completed =
        reference.reference_is_settled_primary && !terms.synthetic_inputs &&
        ((nearly_equal(reference.required_issue_support_million, 0.0) &&
             nearly_equal(terms.support.settled_support_million, 0.0)) ||
            (observed_support_cash_evidenced &&
                nearly_equal(terms.support.settled_support_million,
                    reference.required_issue_support_million)));
    reference.observed_primary_buyer_cash_million =
        reference.observed_primary_price_cash_completed ? reference_price
                                                        : 0.0;
    reference.observed_issue_support_cash_million =
        observed_support_cash_evidenced
        ? terms.support.settled_support_million
        : 0.0;
    const bool observed_source_components_available =
        reference.observed_primary_price_cash_completed &&
        (nearly_equal(terms.support.settled_support_million, 0.0) ||
            observed_support_cash_evidenced);
    if (observed_source_components_available) {
        reference.observed_issue_sources_million =
            reference.observed_primary_buyer_cash_million +
            reference.observed_issue_support_cash_million;
        reference.observed_issue_funding_identity_error_million = std::abs(
            *reference.observed_issue_sources_million - issue_uses_million);
    }
    const bool observed_identity_reconciles =
        reference.observed_issue_funding_identity_error_million.has_value() &&
        *reference.observed_issue_funding_identity_error_million <=
            comparison_tolerance(
                *reference.observed_issue_sources_million,
                issue_uses_million);
    reference.observed_issue_sources_settled_and_reconciled =
        observed_identity_reconciles;
    if (observed_identity_reconciles &&
        terms.reference_price.subscription_reserve_deposit_evidenced) {
        reference.observed_amount_entering_subscription_reserve_million =
            market_notional_million;
    }
    if (observed_identity_reconciles &&
        terms.reference_price.issuer_cost_payment_evidenced) {
        reference.observed_issuer_cost_paid_million =
            terms.reference_price.issuer_cost_million;
    }
    reference.observed_primary_funding_completed =
        observed_identity_reconciles &&
        terms.reference_price.subscription_reserve_deposit_evidenced &&
        (terms.reference_price.issuer_cost_million == 0.0 ||
            terms.reference_price.issuer_cost_payment_evidenced);
    const ProbabilityPolytopeMetricRange shifted_par =
        shift_range(report.market_par_npv_million, npv_shift);
    report.audit.maximum_reference_price_npv_shift_error_million =
        std::max({std::abs(shifted_par.minimum.value -
                      reference.investor_npv_million.minimum.value),
            std::abs(shifted_par.central -
                reference.investor_npv_million.central),
            std::abs(shifted_par.maximum.value -
                reference.investor_npv_million.maximum.value)});
    report.audit.reference_price_npv_shift_reconciles =
        report.audit.maximum_reference_price_npv_shift_error_million <=
        comparison_tolerance(
            shifted_par.minimum.value,
            reference.investor_npv_million.minimum.value);
    report.audit.issue_funding_identity_error_million =
        reference.issue_funding_identity_error_million;
    report.audit.issue_funding_identity_reconciles =
        reference.issue_funding_identity_error_million <=
        comparison_tolerance(
            reference.modeled_required_issue_sources_million,
            reference.issue_uses_million) &&
        nearly_equal(
            reference.modeled_amount_entering_subscription_reserve_million,
            market_notional_million) &&
        nearly_equal(reference.modeled_issuer_cost_paid_million,
            terms.reference_price.issuer_cost_million) &&
        nearly_equal(reference.buyer_direct_cost_outside_reserve_million,
            buyer_direct_cost);
    return result;
}

} // namespace
} // namespace naturalehia::cellular_finance

namespace naturalehia::cellular_finance {
namespace {

[[nodiscard]] std::optional<std::size_t> selected_priority_cap_index(
    const RobustMarketPriorityCapSummary& upstream) {
    if (upstream.junior_concession_constraint_is_declared) {
        if (upstream.status !=
                RobustMarketPriorityCapStatus::
                    MinimumTestedBalancedCapFound ||
            !upstream.minimum_tested_balanced_candidate_index.has_value()) {
            return std::nullopt;
        }
        const std::size_t index =
            *upstream.minimum_tested_balanced_candidate_index;
        if (index >= upstream.candidates.size() ||
            !upstream.candidates[index].balanced) {
            throw std::logic_error(
                "issue-price-support upstream balanced-cap selection is inconsistent");
        }
        return index;
    }
    if (upstream.status != RobustMarketPriorityCapStatus::
            MinimumTestedMarketAdequateCapFound ||
        !upstream.minimum_tested_market_adequate_candidate_index.has_value()) {
        return std::nullopt;
    }
    const std::size_t index =
        *upstream.minimum_tested_market_adequate_candidate_index;
    if (index >= upstream.candidates.size() ||
        !upstream.candidates[index].market_adequate) {
        throw std::logic_error(
            "issue-price-support upstream market-adequate selection is inconsistent");
    }
    return index;
}

void mark_structural_mismatch(double& maximum, bool& invariant) {
    maximum = std::numeric_limits<double>::infinity();
    invariant = false;
}

void compare_monthly_cash(
    const CapitalStackTrancheScenarioResult& baseline,
    const CapitalStackTrancheScenarioResult& current, double& maximum,
    bool& invariant) {
    if (baseline.tranche_id != current.tranche_id ||
        baseline.monthly_cash_flows.size() !=
            current.monthly_cash_flows.size()) {
        mark_structural_mismatch(maximum, invariant);
        return;
    }
    for (std::size_t index = 0U;
         index < baseline.monthly_cash_flows.size(); ++index) {
        const CapitalStackMonthlyTrancheCashFlow& a =
            baseline.monthly_cash_flows[index];
        const CapitalStackMonthlyTrancheCashFlow& b =
            current.monthly_cash_flows[index];
        if (a.month != b.month) {
            mark_structural_mismatch(maximum, invariant);
            continue;
        }
        update_change(a.par_subscription_million,
            b.par_subscription_million, maximum, invariant);
        update_change(a.pro_rata_pool_cost_call_million,
            b.pro_rata_pool_cost_call_million, maximum, invariant);
        update_change(a.underlying_principal_cash_distribution_million,
            b.underlying_principal_cash_distribution_million, maximum,
            invariant);
        update_change(a.unused_reserve_principal_return_million,
            b.unused_reserve_principal_return_million, maximum, invariant);
        update_change(a.principal_cash_distribution_million,
            b.principal_cash_distribution_million, maximum, invariant);
        update_change(a.nonprincipal_cash_distribution_million,
            b.nonprincipal_cash_distribution_million, maximum, invariant);
        update_change(
            a.net_cash_flow_million, b.net_cash_flow_million, maximum,
            invariant);
    }
}

void compare_market_contractual_cash(
    const CapitalStackProbabilityPolytopeSummary& baseline,
    const CapitalStackProbabilityPolytopeSummary& current,
    RobustIssuePriceSupportCaseAudit& audit) {
    audit.market_contractual_cash_is_unchanged = true;
    if (baseline.scenarios.size() != current.scenarios.size()) {
        mark_structural_mismatch(
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        return;
    }
    for (std::size_t index = 0U; index < baseline.scenarios.size(); ++index) {
        const CapitalStackScenarioResult& a = baseline.scenarios[index];
        const CapitalStackScenarioResult& b = current.scenarios[index];
        if (a.scenario_id != b.scenario_id || a.tranches.size() != 2U ||
            b.tranches.size() != 2U) {
            mark_structural_mismatch(
                audit.maximum_contractual_cash_change_million,
                audit.market_contractual_cash_is_unchanged);
            continue;
        }
        update_change(a.underlying_nominal_net_cash_million,
            b.underlying_nominal_net_cash_million,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        update_change(a.stack_nominal_net_cash_million,
            b.stack_nominal_net_cash_million,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        const CapitalStackTrancheScenarioResult& market_a = a.tranches[1];
        const CapitalStackTrancheScenarioResult& market_b = b.tranches[1];
        update_change(market_a.total_contributions_million,
            market_b.total_contributions_million,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        update_change(market_a.principal_cash_distribution_million,
            market_b.principal_cash_distribution_million,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        update_change(market_a.nonprincipal_cash_distribution_million,
            market_b.nonprincipal_cash_distribution_million,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        update_change(market_a.total_distributions_million,
            market_b.total_distributions_million,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
        compare_monthly_cash(market_a, market_b,
            audit.maximum_contractual_cash_change_million,
            audit.market_contractual_cash_is_unchanged);
    }
}

void compare_principal_risk(
    const CapitalStackProbabilityPolytopeSummary& baseline,
    const CapitalStackProbabilityPolytopeSummary& current,
    RobustIssuePriceSupportCaseAudit& audit) {
    audit.market_principal_risk_is_unchanged = true;
    audit.market_principal_wal_is_unchanged = true;
    if (baseline.model_version != current.model_version ||
        baseline.scenarios.size() != current.scenarios.size() ||
        baseline.tranches.size() != 2U || current.tranches.size() != 2U) {
        mark_structural_mismatch(audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        mark_structural_mismatch(audit.maximum_market_wal_change_years,
            audit.market_principal_wal_is_unchanged);
        return;
    }
    const PrincipalRiskMetricFamily risk_family =
        principal_risk_metric_family(baseline.model_version);
    for (std::size_t index = 0U; index < baseline.scenarios.size(); ++index) {
        const CapitalStackScenarioResult& a = baseline.scenarios[index];
        const CapitalStackScenarioResult& b = current.scenarios[index];
        if (a.tranches.size() != 2U || b.tranches.size() != 2U ||
            a.scenario_id != b.scenario_id) {
            mark_structural_mismatch(audit.maximum_principal_risk_change,
                audit.market_principal_risk_is_unchanged);
            continue;
        }
        const CapitalStackTrancheScenarioResult& market_a = a.tranches[1];
        const CapitalStackTrancheScenarioResult& market_b = b.tranches[1];
        if (risk_family ==
            PrincipalRiskMetricFamily::LegacyLossLayeringV01) {
            update_change(market_a.realized_principal_loss_million,
                market_b.realized_principal_loss_million,
                audit.maximum_principal_risk_change,
                audit.market_principal_risk_is_unchanged);
            update_change(market_a.unresolved_principal_exposure_million,
                market_b.unresolved_principal_exposure_million,
                audit.maximum_principal_risk_change,
                audit.market_principal_risk_is_unchanged);
        }
        update_change(market_a.principal_cash_shortfall_million,
            market_b.principal_cash_shortfall_million,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
    }
    const CapitalStackProbabilityPolytopeTrancheSummary& market_a =
        baseline.tranches[1];
    const CapitalStackProbabilityPolytopeTrancheSummary& market_b =
        current.tranches[1];
    update_range_change(market_a.expected_principal_cash_distribution_million,
        market_b.expected_principal_cash_distribution_million,
        audit.maximum_principal_risk_change,
        audit.market_principal_risk_is_unchanged);
    if (risk_family == PrincipalRiskMetricFamily::LegacyLossLayeringV01) {
        update_range_change(market_a.expected_realized_principal_loss_fraction,
            market_b.expected_realized_principal_loss_fraction,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_range_change(market_a.principal_impairment_probability,
            market_b.principal_impairment_probability,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_tail_change(
            market_a.principal_loss_expected_shortfall_95_million,
            market_b.principal_loss_expected_shortfall_95_million,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_tail_change(
            market_a.principal_loss_expected_shortfall_99_million,
            market_b.principal_loss_expected_shortfall_99_million,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
    } else {
        update_range_change(
            market_a.expected_principal_cash_shortfall_million,
            market_b.expected_principal_cash_shortfall_million,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_range_change(market_a.principal_cash_shortfall_probability,
            market_b.principal_cash_shortfall_probability,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_range_change(
            market_a.full_principal_cash_shortfall_probability,
            market_b.full_principal_cash_shortfall_probability,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_tail_change(
            market_a.principal_cash_shortfall_expected_shortfall_95_million,
            market_b.principal_cash_shortfall_expected_shortfall_95_million,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
        update_tail_change(
            market_a.principal_cash_shortfall_expected_shortfall_99_million,
            market_b.principal_cash_shortfall_expected_shortfall_99_million,
            audit.maximum_principal_risk_change,
            audit.market_principal_risk_is_unchanged);
    }
    update_wal_change(
        market_a.principal_cash_weighted_average_life_years,
        market_b.principal_cash_weighted_average_life_years,
        audit.maximum_market_wal_change_years,
        audit.market_principal_wal_is_unchanged);
}

void compare_junior(
    const CapitalStackProbabilityPolytopeSummary& baseline,
    const CapitalStackProbabilityPolytopeSummary& current,
    RobustIssuePriceSupportCaseAudit& audit) {
    audit.junior_cash_and_own_hurdle_npv_are_unchanged = true;
    if (baseline.scenarios.size() != current.scenarios.size() ||
        baseline.tranches.size() != 2U || current.tranches.size() != 2U) {
        mark_structural_mismatch(audit.maximum_junior_change_million,
            audit.junior_cash_and_own_hurdle_npv_are_unchanged);
        return;
    }
    for (std::size_t index = 0U; index < baseline.scenarios.size(); ++index) {
        const CapitalStackScenarioResult& a = baseline.scenarios[index];
        const CapitalStackScenarioResult& b = current.scenarios[index];
        if (a.tranches.size() != 2U || b.tranches.size() != 2U ||
            a.scenario_id != b.scenario_id) {
            mark_structural_mismatch(audit.maximum_junior_change_million,
                audit.junior_cash_and_own_hurdle_npv_are_unchanged);
            continue;
        }
        const CapitalStackTrancheScenarioResult& junior_a = a.tranches[0];
        const CapitalStackTrancheScenarioResult& junior_b = b.tranches[0];
        update_change(junior_a.total_contributions_million,
            junior_b.total_contributions_million,
            audit.maximum_junior_change_million,
            audit.junior_cash_and_own_hurdle_npv_are_unchanged);
        update_change(junior_a.total_distributions_million,
            junior_b.total_distributions_million,
            audit.maximum_junior_change_million,
            audit.junior_cash_and_own_hurdle_npv_are_unchanged);
        update_change(junior_a.npv_at_tranche_hurdle_million,
            junior_b.npv_at_tranche_hurdle_million,
            audit.maximum_junior_change_million,
            audit.junior_cash_and_own_hurdle_npv_are_unchanged);
        compare_monthly_cash(junior_a, junior_b,
            audit.maximum_junior_change_million,
            audit.junior_cash_and_own_hurdle_npv_are_unchanged);
    }
    update_range_change(
        baseline.tranches[0].expected_npv_at_tranche_hurdle_million,
        current.tranches[0].expected_npv_at_tranche_hurdle_million,
        audit.maximum_junior_change_million,
        audit.junior_cash_and_own_hurdle_npv_are_unchanged);
}

void audit_against_zero_case(
    const CapitalStackProbabilityPolytopeSummary& baseline,
    EvaluatedCase& current) {
    RobustIssuePriceSupportCaseAudit& audit = current.report.audit;
    audit.physical_probability_polytope_is_unchanged =
        scenario_probabilities_equal(baseline.scenario_probabilities,
            current.stack.scenario_probabilities) &&
        events_equal(baseline.events, current.stack.events);
    compare_market_contractual_cash(baseline, current.stack, audit);
    compare_principal_risk(baseline, current.stack, audit);
    compare_junior(baseline, current.stack, audit);
}

} // namespace

RobustIssuePriceSupportSummary evaluate_robust_issue_price_support(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& priority_cap,
    const RobustIssuePriceSupportConfig& issue_price) {
    const CapitalStackConfig base_stack_snapshot = base_stack;
    const ValidatedInputs inputs = validate_inputs(portfolio,
        probability_polytope, participation, base_stack, priority_cap,
        issue_price);
    const RobustMarketPriorityCapSummary upstream =
        evaluate_robust_market_priority_cap(portfolio, probability_polytope,
            participation, base_stack, priority_cap);

    RobustIssuePriceSupportSummary summary;
    summary.fixed_underlying_success_participation_fraction =
        base_stack.underlying_success_participation_fraction;
    summary.fixed_junior_first_loss_million =
        base_stack.tranches[0].detachment_million -
        base_stack.tranches[0].attachment_million;
    summary.aggregate_commitment_and_stack_detachment_million =
        base_stack.tranches[1].detachment_million;
    summary.fixed_market_notional_million = inputs.market_notional_million;
    summary.upstream_priority_cap_status = upstream.status;
    summary.reference_gross_issue_price_million =
        issue_price.reference_price.gross_issue_price_million;
    summary.issuer_cost_million =
        issue_price.reference_price.issuer_cost_million;
    summary.buyer_direct_cost_million =
        issue_price.reference_price.buyer_direct_cost_million;
    summary.maximum_issue_support_million =
        issue_price.support.maximum_support_million;
    summary.settled_issue_support_million =
        issue_price.support.settled_support_million;
    summary.issuer_funding_floor_million = inputs.issuer_floor_million;
    summary.reference_price_status = issue_price.reference_price.status;
    summary.support_capacity_status = issue_price.support.status;
    summary.portfolio_cash_record_count =
        inputs.upstream_work.portfolio_cash_records;
    summary.portfolio_auxiliary_record_count =
        inputs.upstream_work.portfolio_auxiliary_records;
    summary.portfolio_record_count = inputs.upstream_work.portfolio_records;
    summary.upstream_priority_cap_work_units = inputs.upstream_work.total;
    summary.hurdle_stack_work_units = inputs.hurdle_stack_work.total;
    summary.reference_projection_work_units =
        inputs.reference_projection_work;
    summary.scenario_month_audit_work_units =
        inputs.scenario_month_audit_work;
    summary.structural_work_units = inputs.total_work;
    summary.base_stack_was_not_mutated =
        stack_config_equal(base_stack, base_stack_snapshot);
    summary.model_limitation =
        "Finite issue-price support sensitivity over fixed synthetic physical-"
        "probability cash paths. Each hurdle is supplied externally. Raw and "
        "admissible investor ceilings, issuer floors, and support gaps are not "
        "fair value, a quote, a negotiated price, a calibrated return, demand, "
        "or evidence of support authority. Required no-rights support is not "
        "project revenue or investor cash, and observed funding is reported "
        "only for coherent non-synthetic settled-primary buyer cash plus any "
        "exact support cash settled to the same issue. Every endpoint retains "
        "its own physical-P witness.";

    const std::optional<std::size_t> selected_index =
        selected_priority_cap_index(upstream);
    if (!selected_index.has_value()) {
        summary.status =
            RobustIssuePriceSupportStatus::PriorityCapSelectionUnavailable;
        return summary;
    }
    summary.selected_priority_cap_candidate_index = *selected_index;
    const RobustMarketPriorityCapCandidate& selected_candidate =
        upstream.candidates[*selected_index];
    summary.selected_market_priority_nonprincipal_cap_million =
        selected_candidate.market_priority_nonprincipal_cap_million;
    summary.selected_priority_cap_is_balanced = selected_candidate.balanced;

    CapitalStackConfig selected_stack = base_stack;
    selected_stack.tranches[1].priority_nonprincipal_cap_million =
        selected_candidate.market_priority_nonprincipal_cap_million;
    const ProbabilityPolytopeProjector projector(
        portfolio, probability_polytope);

    std::vector<EvaluatedCase> evaluated;
    evaluated.reserve(inputs.hurdle_cases.size());
    for (const RobustIssuePriceHurdleCaseConfig& hurdle :
        inputs.hurdle_cases) {
        evaluated.push_back(evaluate_case(portfolio, probability_polytope,
            participation, selected_stack, issue_price, hurdle, projector,
            inputs.market_notional_million, inputs.issue_uses_million,
            inputs.issuer_floor_million));
    }

    const auto zero = std::find_if(evaluated.begin(), evaluated.end(),
        [](const EvaluatedCase& candidate) {
            return candidate.report.annual_effective_hurdle_rate == 0.0;
        });
    if (zero == evaluated.end()) {
        throw std::logic_error(
            "issue-price-support literal-zero hurdle disappeared after validation");
    }
    const std::size_t zero_index =
        static_cast<std::size_t>(std::distance(evaluated.begin(), zero));
    summary.literal_zero_hurdle_case_index = zero_index;
    const CapitalStackProbabilityPolytopeSummary& zero_stack = zero->stack;
    summary.only_market_hurdle_changed_across_cases = true;
    summary.all_contractual_cash_and_principal_risk_invariants_hold = true;
    summary.hurdle_cases.reserve(evaluated.size());
    for (std::size_t index = 0U; index < evaluated.size(); ++index) {
        audit_against_zero_case(zero_stack, evaluated[index]);
        const RobustIssuePriceSupportCaseResult& report =
            evaluated[index].report;
        const bool invariants =
            report.audit.physical_probability_polytope_is_unchanged &&
            report.audit.market_contractual_cash_is_unchanged &&
            report.audit.sparse_market_monthly_ledger_reconciles &&
            report.audit.market_principal_risk_is_unchanged &&
            report.audit.market_principal_wal_is_unchanged &&
            report.audit.junior_cash_and_own_hurdle_npv_are_unchanged &&
            report.audit.raw_price_ceiling_zero_npv_reconciles &&
            (!report.reference_price_numerically_eligible ||
                (report.audit.reference_price_npv_shift_reconciles &&
                    report.audit.issue_funding_identity_reconciles));
        summary.all_contractual_cash_and_principal_risk_invariants_hold =
            summary.all_contractual_cash_and_principal_risk_invariants_hold &&
            invariants;
        if (report.status ==
            RobustIssuePriceSupportCaseStatus::FinanceablePriceWindow) {
            summary.financeable_hurdle_case_indices.push_back(index);
        }
        if (report.funded_support_covered_price_window_exists) {
            summary.funded_support_covered_hurdle_case_indices.push_back(
                index);
        }
        if (report.status ==
            RobustIssuePriceSupportCaseStatus::NoNonnegativeInvestorPrice) {
            summary.no_nonnegative_price_hurdle_case_indices.push_back(index);
        }
        summary.hurdle_cases.push_back(report);
    }
    summary.status = summary.financeable_hurdle_case_indices.empty()
        ? RobustIssuePriceSupportStatus::NoFinanceableWindow
        : RobustIssuePriceSupportStatus::FinanceableWindowFound;
    summary.base_stack_was_not_mutated =
        stack_config_equal(base_stack, base_stack_snapshot);
    return summary;
}

} // namespace naturalehia::cellular_finance
