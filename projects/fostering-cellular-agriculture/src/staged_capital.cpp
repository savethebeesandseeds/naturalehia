// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/staged_capital.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kAbsoluteTolerance = 1.0e-10;
constexpr double kProbabilityTolerance = 1.0e-12;
constexpr double kMaximumMoneyMillion = 1.0e9;
constexpr double kMaximumAnnualRate = 5.0;
constexpr double kMaximumClaimMultiple = 10.0;
constexpr std::size_t kMaximumMonths = 1'200U;
constexpr std::size_t kMaximumPhases = 32U;
constexpr std::size_t kMaximumCases = 256U;
constexpr std::size_t kMaximumScheduledPathMonths = 1'200U;
constexpr std::size_t kMaximumAggregateCaseMonths = 100'000U;
constexpr std::size_t kMaximumFreeTextLength = 1'024U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumCurrencyLabelLength = 32U;

[[nodiscard]] bool finite(double value) noexcept {
    return std::isfinite(value);
}

[[nodiscard]] double tolerance_for(double scale) noexcept {
    return kAbsoluteTolerance +
        64.0 * std::numeric_limits<double>::epsilon() *
            std::max(1.0, std::abs(scale));
}

[[nodiscard]] bool nearly_equal(double first, double second) noexcept {
    return std::abs(first - second) <=
        tolerance_for(std::max(std::abs(first), std::abs(second)));
}

void require_finite(double value, std::string_view name) {
    if (!finite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void require_non_negative(double value, std::string_view name) {
    require_finite(value, name);
    if (value < 0.0) {
        throw std::invalid_argument(
            std::string(name) + " must be non-negative");
    }
}

void require_bounded_money(double value, std::string_view name) {
    require_non_negative(value, name);
    if (value > kMaximumMoneyMillion) {
        throw std::invalid_argument(
            std::string(name) + " exceeds the model guardrail");
    }
}

void require_rate(double value, std::string_view name) {
    require_non_negative(value, name);
    if (value > kMaximumAnnualRate) {
        throw std::invalid_argument(
            std::string(name) + " exceeds the model rate guardrail");
    }
}

void require_probability(double value, std::string_view name) {
    require_non_negative(value, name);
    if (value > 1.0) {
        throw std::invalid_argument(
            std::string(name) + " must not exceed one");
    }
}

void require_normalized_text(
    const std::string& value, std::string_view name,
    std::size_t maximum_length) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
    if (value.size() > maximum_length) {
        throw std::invalid_argument(
            std::string(name) + " exceeds the text-length guardrail");
    }
    if (value.front() == ' ' || value.back() == ' ' ||
        value.find('\n') != std::string::npos ||
        value.find('\r') != std::string::npos ||
        value.find('\0') != std::string::npos) {
        throw std::invalid_argument(
            std::string(name) + " must be normalized single-line text");
    }
    for (const unsigned char character : value) {
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(name) + " must not contain control characters");
        }
    }
}

void require_safe_identifier(const std::string& value,
    std::string_view name, std::size_t maximum_length) {
    require_normalized_text(value, name, maximum_length);
    for (const unsigned char character : value) {
        const bool ascii_alphanumeric =
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9');
        if (!ascii_alphanumeric && character != '-' && character != '_' &&
            character != '.') {
            throw std::invalid_argument(std::string(name) +
                " must use only ASCII letters, digits, hyphen, underscore, or period");
        }
    }
}

[[nodiscard]] double discount_factor(
    double annual_rate, std::size_t month) {
    if (annual_rate == 0.0 || month == 0U) {
        return 1.0;
    }
    return std::exp(
        -std::log1p(annual_rate) * static_cast<double>(month) / 12.0);
}

[[nodiscard]] double monthly_effective_rate(double annual_rate) {
    if (annual_rate == 0.0) {
        return 0.0;
    }
    return std::expm1(std::log1p(annual_rate) / 12.0);
}

[[nodiscard]] double posting_sum(const CapitalCashPosting& posting) noexcept {
    return posting.sponsor_million + posting.project_unrestricted_million +
        posting.provider_million + posting.protected_reserve_million +
        posting.external_million;
}

void post_cash(StagedCapitalPathResult& result, std::size_t month,
    CapitalCashFlowKind kind, std::string reference,
    CapitalCashPosting posting) {
    const double gross_scale = std::abs(posting.sponsor_million) +
        std::abs(posting.project_unrestricted_million) +
        std::abs(posting.provider_million) +
        std::abs(posting.protected_reserve_million) +
        std::abs(posting.external_million);
    const double imbalance = std::abs(posting_sum(posting));
    result.maximum_cash_entry_imbalance_million = std::max(
        result.maximum_cash_entry_imbalance_million, imbalance);
    if (imbalance > tolerance_for(gross_scale)) {
        throw std::logic_error("unbalanced staged-capital cash posting");
    }
    result.cash_ledger.push_back(CapitalCashLedgerEntry{
        month, kind, std::move(reference), posting});
}

void post_upfront_items(
    const StagedCapitalConfig& config, StagedCapitalPathResult& result) {
    const auto& terms = config.terms;
    if (terms.upfront_fee_million > 0.0) {
        post_cash(result, 0U, CapitalCashFlowKind::UpfrontFee,
            "sponsor-paid upfront fee",
            CapitalCashPosting{-terms.upfront_fee_million, 0.0,
                terms.upfront_fee_million, 0.0, 0.0});
    }
    if (terms.protected_workout_reserve_million > 0.0) {
        post_cash(result, 0U,
            CapitalCashFlowKind::ProtectedReserveFunding,
            "sponsor-funded protected workout reserve",
            CapitalCashPosting{-terms.protected_workout_reserve_million,
                0.0, 0.0, terms.protected_workout_reserve_million, 0.0});
    }
}

void settle_protected_reserve(const StagedCapitalConfig& config,
    const StagedCapitalCase& scenario_case, bool completed,
    std::size_t month, StagedCapitalPathResult& result) {
    const double reserve = config.terms.protected_workout_reserve_million;
    if (completed) {
        result.protected_reserve_release_million = reserve;
        if (reserve > 0.0) {
            post_cash(result, month,
                CapitalCashFlowKind::ProtectedReserveRelease,
                "protected reserve release after completion",
                CapitalCashPosting{reserve, 0.0, 0.0, -reserve, 0.0});
        }
        return;
    }

    const double workout_spend =
        std::min(reserve, scenario_case.required_workout_cost_million);
    const double release = reserve - workout_spend;
    result.protected_workout_spend_million = workout_spend;
    result.protected_reserve_release_million = release;
    result.protected_reserve_shortfall_at_stop_million = std::max(
        0.0, scenario_case.required_workout_cost_million - reserve);
    result.safety_funding_shortfall_million =
        result.protected_reserve_shortfall_at_stop_million;
    if (workout_spend > 0.0) {
        post_cash(result, month,
            CapitalCashFlowKind::ProtectedWorkoutUse,
            "permitted protected workout use",
            CapitalCashPosting{
                0.0, 0.0, 0.0, -workout_spend, workout_spend});
    }
    if (release > 0.0) {
        post_cash(result, month,
            CapitalCashFlowKind::ProtectedReserveRelease,
            "post-workout protected reserve release",
            CapitalCashPosting{release, 0.0, 0.0, -release, 0.0});
    }
}

[[nodiscard]] double accrue_claim_for_months(
    const StagedCapitalTerms& terms, std::size_t months,
    double cumulative_principal, double& claim) {
    const double monthly_rate = monthly_effective_rate(terms.annual_pik_rate);
    const double claim_cap = terms.claim_cap_multiple * cumulative_principal;
    double total_accrual = 0.0;
    for (std::size_t month = 0U; month < months; ++month) {
        const double next_claim =
            std::min(claim_cap, claim * (1.0 + monthly_rate));
        total_accrual += next_claim - claim;
        claim = next_claim;
    }
    return total_accrual;
}

void settle_exit(const StagedCapitalConfig& config,
    const StagedCapitalCase& scenario_case, bool completed,
    std::size_t event_month, std::size_t recovery_month,
    double cumulative_principal, double& claim,
    StagedCapitalPhaseResult& phase_result,
    StagedCapitalPathResult& result) {
    settle_protected_reserve(
        config, scenario_case, completed, event_month, result);

    if (!completed && recovery_month > event_month) {
        phase_result.contractual_return_accrued_million +=
            accrue_claim_for_months(config.terms,
                recovery_month - event_month, cumulative_principal, claim);
    }

    const double proceeds = completed ? scenario_case.completion_value_million
                                      : scenario_case.recovery_value_million;
    if (proceeds > 0.0) {
        post_cash(result, recovery_month,
            CapitalCashFlowKind::CompletionOrRecoveryProceeds,
            completed ? "completion cash proceeds"
                      : "limited-recourse recovery cash",
            CapitalCashPosting{0.0, proceeds, 0.0, 0.0, -proceeds});
    }

    result.provider_claim_at_exit_million = claim;
    double proceeds_after_protected_obligations = proceeds;
    if (!completed && result.safety_funding_shortfall_million > 0.0) {
        const double shortfall_payment = std::min(
            proceeds_after_protected_obligations,
            result.safety_funding_shortfall_million);
        if (shortfall_payment > 0.0) {
            post_cash(result, recovery_month,
                CapitalCashFlowKind::WorkoutShortfallUseFromRecovery,
                "safe-workout shortfall paid before provider",
                CapitalCashPosting{0.0, -shortfall_payment, 0.0, 0.0,
                    shortfall_payment});
        }
        result.workout_shortfall_paid_from_recovery_million =
            shortfall_payment;
        result.safety_funding_shortfall_million -= shortfall_payment;
        proceeds_after_protected_obligations -= shortfall_payment;
    }
    const double provider_repayment =
        std::min(claim, proceeds_after_protected_obligations);
    const double sponsor_residual =
        proceeds_after_protected_obligations - provider_repayment;
    if (provider_repayment > 0.0) {
        post_cash(result, recovery_month,
            CapitalCashFlowKind::ProviderRepayment,
            "modeled provider terminal repayment after protected obligations and before sponsor residual",
            CapitalCashPosting{
                0.0, -provider_repayment, provider_repayment, 0.0, 0.0});
    }
    if (sponsor_residual > 0.0) {
        post_cash(result, recovery_month,
            CapitalCashFlowKind::SponsorResidualDistribution,
            "sponsor residual after provider claim",
            CapitalCashPosting{
                sponsor_residual, -sponsor_residual, 0.0, 0.0, 0.0});
    }

    phase_result.provider_repayment_million = provider_repayment;
    phase_result.claim_writeoff_million = claim - provider_repayment;
    result.provider_claim_writeoff_million =
        phase_result.claim_writeoff_million;
    claim = 0.0;
    phase_result.closing_funded_claim_million = 0.0;
    result.provider_nominal_recovery_million = provider_repayment;
    result.provider_recovery_pv_million = provider_repayment *
        discount_factor(config.terms.provider_hurdle_rate, recovery_month);
    result.provider_principal_loss_million =
        std::max(0.0, cumulative_principal - provider_repayment);
    result.sponsor_residual_receipt_million = sponsor_residual;
}

[[nodiscard]] double ledger_npv(const StagedCapitalPathResult& result,
    double annual_rate, bool provider, bool exclude_upfront_fee) {
    double npv = 0.0;
    for (const auto& entry : result.cash_ledger) {
        if (exclude_upfront_fee &&
            entry.kind == CapitalCashFlowKind::UpfrontFee) {
            continue;
        }
        const double cash = provider ? entry.posting.provider_million
                                     : entry.posting.sponsor_million;
        npv += cash * discount_factor(annual_rate, entry.month);
    }
    return npv;
}

void finalize_path_invariants(StagedCapitalPathResult& result) {
    double project_cash = 0.0;
    double reserve_cash = 0.0;
    double running_provider_cash = 0.0;
    double minimum_provider_cash = 0.0;
    for (const auto& entry : result.cash_ledger) {
        project_cash += entry.posting.project_unrestricted_million;
        reserve_cash += entry.posting.protected_reserve_million;
        running_provider_cash += entry.posting.provider_million;
        minimum_provider_cash =
            std::min(minimum_provider_cash, running_provider_cash);
    }
    result.project_closing_unrestricted_cash_million = project_cash;
    result.protected_reserve_closing_cash_million = reserve_cash;
    result.peak_provider_net_cash_outlay_million = -minimum_provider_cash;

    for (const auto& phase : result.phases) {
        if (!phase.reached) {
            continue;
        }
        const double commitment_imbalance =
            phase.opening_undrawn_commitment_million -
            phase.provider_draw_received_million -
            phase.cancelled_availability_million -
            phase.closing_undrawn_commitment_million;
        const double claim_imbalance =
            phase.opening_funded_claim_million +
            phase.provider_draw_received_million +
            phase.contractual_return_accrued_million -
            phase.provider_repayment_million - phase.claim_writeoff_million -
            phase.closing_funded_claim_million;
        result.maximum_memo_rollforward_imbalance_million = std::max({
            result.maximum_memo_rollforward_imbalance_million,
            std::abs(commitment_imbalance), std::abs(claim_imbalance)});
    }

    const double scale = std::max({1.0, result.cumulative_eligible_spend_million,
        result.total_provider_draws_million,
        result.total_sponsor_construction_contributions_million,
        result.provider_claim_at_exit_million});
    const double tolerance = tolerance_for(scale);
    if (std::abs(project_cash) > tolerance ||
        std::abs(reserve_cash) > tolerance ||
        std::abs(result.closing_undrawn_commitment_million) > tolerance ||
        std::abs(result.closing_funded_claim_million) > tolerance ||
        result.maximum_memo_rollforward_imbalance_million > tolerance) {
        throw std::logic_error(
            "staged-capital path failed a cash or memo-account invariant");
    }
}

[[nodiscard]] WeightedDistributionSummary summarize_weighted(
    std::vector<std::pair<double, double>> values) {
    if (values.empty()) {
        throw std::logic_error("cannot summarize an empty weighted set");
    }
    long double total_weight = 0.0L;
    long double mean = 0.0L;
    for (const auto& [value, weight] : values) {
        total_weight += static_cast<long double>(weight);
        mean += static_cast<long double>(value) *
            static_cast<long double>(weight);
    }
    mean /= total_weight;
    long double variance = 0.0L;
    for (const auto& [value, weight] : values) {
        const long double difference =
            static_cast<long double>(value) - mean;
        variance += static_cast<long double>(weight) *
            difference * difference;
    }
    variance /= total_weight;

    std::sort(values.begin(), values.end(),
        [](const auto& first, const auto& second) {
            return first.first < second.first;
        });

    const auto quantile = [&values, total_weight](long double probability) {
        long double cumulative = 0.0L;
        const long double threshold = probability * total_weight;
        for (const auto& [value, weight] : values) {
            cumulative += static_cast<long double>(weight);
            if (cumulative >= threshold) {
                return value;
            }
        }
        return values.back().first;
    };

    const auto upper_tail_mean =
        [&values, total_weight](long double probability) {
        long double remaining = (1.0L - probability) * total_weight;
        if (remaining <= 0.0L) {
            return values.back().first;
        }
        const long double tail_weight = remaining;
        long double weighted_sum = 0.0L;
        for (auto iterator = values.rbegin();
             iterator != values.rend() && remaining > 0.0L;
             ++iterator) {
            const long double accepted = std::min(
                static_cast<long double>(iterator->second), remaining);
            weighted_sum +=
                static_cast<long double>(iterator->first) * accepted;
            remaining -= accepted;
        }
        return static_cast<double>(weighted_sum / tail_weight);
    };

    return WeightedDistributionSummary{static_cast<double>(mean),
        static_cast<double>(std::sqrt(variance)), quantile(0.50L),
        quantile(0.95L), quantile(0.99L), values.back().first,
        upper_tail_mean(0.95L), upper_tail_mean(0.99L)};
}

[[nodiscard]] double sponsor_total_cash_call(
    const StagedCapitalPathResult& result) {
    double total = 0.0;
    for (const auto& entry : result.cash_ledger) {
        total += std::max(0.0, -entry.posting.sponsor_million);
    }
    return total;
}

} // namespace

std::string_view to_string(CertificationDecision decision) noexcept {
    switch (decision) {
    case CertificationDecision::Certified:
        return "certified";
    case CertificationDecision::FinalFailure:
        return "final-failure";
    }
    return "invalid-certification";
}

std::string_view to_string(StagedCapitalOutcome outcome) noexcept {
    switch (outcome) {
    case StagedCapitalOutcome::Completed:
        return "completed";
    case StagedCapitalOutcome::MilestoneFailure:
        return "milestone-failure";
    case StagedCapitalOutcome::CostToCompleteFailure:
        return "cost-to-complete-failure";
    case StagedCapitalOutcome::SponsorFundingFailure:
        return "sponsor-funding-failure";
    case StagedCapitalOutcome::ProviderFundingFailure:
        return "provider-funding-failure";
    }
    return "invalid-outcome";
}

std::string_view to_string(CapitalCashFlowKind kind) noexcept {
    switch (kind) {
    case CapitalCashFlowKind::UpfrontFee:
        return "upfront-fee";
    case CapitalCashFlowKind::ProtectedReserveFunding:
        return "protected-reserve-funding";
    case CapitalCashFlowKind::SponsorConstructionContribution:
        return "sponsor-construction-contribution";
    case CapitalCashFlowKind::ProviderDraw:
        return "provider-draw";
    case CapitalCashFlowKind::EligibleConstructionUse:
        return "eligible-construction-use";
    case CapitalCashFlowKind::CommitmentFee:
        return "commitment-fee";
    case CapitalCashFlowKind::CompletionOrRecoveryProceeds:
        return "completion-or-recovery-proceeds";
    case CapitalCashFlowKind::ProviderRepayment:
        return "provider-repayment";
    case CapitalCashFlowKind::SponsorResidualDistribution:
        return "sponsor-residual-distribution";
    case CapitalCashFlowKind::ProtectedWorkoutUse:
        return "protected-workout-use";
    case CapitalCashFlowKind::WorkoutShortfallUseFromRecovery:
        return "workout-shortfall-use-from-recovery";
    case CapitalCashFlowKind::ProtectedReserveRelease:
        return "protected-reserve-release";
    }
    return "invalid-cash-flow-kind";
}

void validate_staged_capital_config(const StagedCapitalConfig& config) {
    if (config.model_version != kStagedCapitalModelVersion) {
        throw std::invalid_argument(
            "unsupported staged-capital model version: " +
            config.model_version);
    }
    if (!config.synthetic_inputs) {
        throw std::invalid_argument(
            "staged-capital v0.1 accepts synthetic inputs only");
    }
    require_normalized_text(config.scenario_label, "scenario label",
        kMaximumFreeTextLength);
    require_normalized_text(
        config.source_note, "source note", kMaximumFreeTextLength);
    require_safe_identifier(config.currency_label, "currency label",
        kMaximumCurrencyLabelLength);
    require_normalized_text(config.monetary_basis, "monetary basis",
        kMaximumFreeTextLength);

    const auto& terms = config.terms;
    require_bounded_money(
        terms.provider_commitment_million, "provider commitment");
    if (terms.provider_commitment_million <= 0.0) {
        throw std::invalid_argument("provider commitment must be positive");
    }
    require_bounded_money(terms.sponsor_construction_commitment_million,
        "sponsor construction commitment");
    if (terms.sponsor_construction_commitment_million <= 0.0) {
        throw std::invalid_argument(
            "sponsor construction commitment must be positive");
    }
    require_probability(terms.provider_cost_share, "provider cost share");
    if (terms.provider_cost_share <= 0.0 ||
        terms.provider_cost_share >= 1.0) {
        throw std::invalid_argument(
            "provider cost share must be strictly between zero and one");
    }
    require_rate(terms.annual_pik_rate, "annual PIK rate");
    require_finite(terms.claim_cap_multiple, "claim cap multiple");
    if (terms.claim_cap_multiple < 1.0 ||
        terms.claim_cap_multiple > kMaximumClaimMultiple) {
        throw std::invalid_argument(
            "claim cap multiple must be between one and ten");
    }
    require_rate(
        terms.annual_commitment_fee_rate, "annual commitment fee rate");
    require_bounded_money(terms.upfront_fee_million, "upfront fee");
    require_rate(terms.provider_hurdle_rate, "provider hurdle rate");
    require_rate(terms.sponsor_discount_rate, "sponsor discount rate");
    require_bounded_money(terms.protected_workout_reserve_million,
        "protected workout reserve");
    if (terms.protected_workout_reserve_million <= 0.0) {
        throw std::invalid_argument(
            "protected workout reserve must be positive");
    }

    if (config.phases.empty() || config.phases.size() > kMaximumPhases) {
        throw std::invalid_argument(
            "phase count must be between one and 32");
    }
    std::unordered_set<std::string> phase_ids;
    double phase_cap_sum = 0.0;
    std::size_t scheduled_path_months = 0U;
    for (const auto& phase : config.phases) {
        require_safe_identifier(
            phase.id, "phase id", kMaximumIdentifierLength);
        if (!phase_ids.insert(phase.id).second) {
            throw std::invalid_argument("duplicate phase id: " + phase.id);
        }
        if (phase.duration_months == 0U ||
            phase.duration_months > kMaximumMonths) {
            throw std::invalid_argument(
                "phase duration must be between one and 1200 months");
        }
        scheduled_path_months += phase.duration_months;
        require_bounded_money(
            phase.provider_stage_cap_million, "provider phase cap");
        if (phase.provider_stage_cap_million <= 0.0) {
            throw std::invalid_argument(
                "each provider phase cap must be positive");
        }
        phase_cap_sum += phase.provider_stage_cap_million;
    }
    if (!nearly_equal(
            phase_cap_sum, terms.provider_commitment_million)) {
        throw std::invalid_argument(
            "provider commitment must equal the sum of phase caps");
    }
    if (scheduled_path_months > kMaximumScheduledPathMonths) {
        throw std::invalid_argument(
            "aggregate scheduled phase duration must not exceed 1200 months");
    }

    if (config.cases.empty() || config.cases.size() > kMaximumCases) {
        throw std::invalid_argument(
            "case count must be between one and 256");
    }
    if (config.cases.size() >
        kMaximumAggregateCaseMonths / scheduled_path_months) {
        throw std::invalid_argument(
            "case count times scheduled path months must not exceed 100000");
    }
    std::unordered_set<std::string> case_ids;
    long double total_weight = 0.0L;
    for (const auto& scenario_case : config.cases) {
        require_safe_identifier(
            scenario_case.id, "case id", kMaximumIdentifierLength);
        if (!case_ids.insert(scenario_case.id).second) {
            throw std::invalid_argument(
                "duplicate staged-capital case id: " + scenario_case.id);
        }
        require_probability(scenario_case.weight, "case weight");
        if (scenario_case.weight <= 0.0) {
            throw std::invalid_argument("case weight must be positive");
        }
        total_weight += static_cast<long double>(scenario_case.weight);
        if (scenario_case.phases.size() != config.phases.size()) {
            throw std::invalid_argument(
                "each case must contain exactly one input for every phase");
        }
        require_bounded_money(
            scenario_case.completion_value_million, "completion value");
        require_bounded_money(
            scenario_case.recovery_value_million, "recovery value");
        if (scenario_case.recovery_delay_months > kMaximumMonths) {
            throw std::invalid_argument(
                "recovery delay must not exceed 1200 months");
        }
        require_bounded_money(scenario_case.required_workout_cost_million,
            "required workout cost");
        for (const auto& phase_case : scenario_case.phases) {
            require_bounded_money(phase_case.actual_eligible_cost_million,
                "actual eligible phase cost");
            require_bounded_money(
                phase_case.estimated_cost_to_complete_million,
                "estimated cost to complete");
            if (phase_case.estimated_cost_to_complete_million +
                    tolerance_for(
                        phase_case.actual_eligible_cost_million) <
                phase_case.actual_eligible_cost_million) {
                throw std::invalid_argument(
                    "cost-to-complete estimate cannot be below the current phase cost");
            }
            switch (phase_case.certification) {
            case CertificationDecision::Certified:
            case CertificationDecision::FinalFailure:
                break;
            default:
                throw std::invalid_argument(
                    "unknown certification decision");
            }
        }
        const auto& final_phase = scenario_case.phases.back();
        if (!nearly_equal(final_phase.estimated_cost_to_complete_million,
                final_phase.actual_eligible_cost_million)) {
            throw std::invalid_argument(
                "final-phase cost-to-complete estimate must equal its actual eligible cost in v0.1");
        }
    }
    if (std::abs(total_weight - 1.0L) >
        static_cast<long double>(kProbabilityTolerance)) {
        throw std::invalid_argument(
            "synthetic case weights must sum to one within tolerance");
    }
}

namespace {

[[nodiscard]] StagedCapitalPathResult evaluate_validated_case(
    const StagedCapitalConfig& config,
    const StagedCapitalCase& configured_case) {
    StagedCapitalPathResult result;
    result.case_id = configured_case.id;
    result.weight = configured_case.weight;
    result.phases.resize(config.phases.size());
    for (std::size_t index = 0U; index < config.phases.size(); ++index) {
        result.phases[index].id = config.phases[index].id;
        result.phases[index].certification =
            configured_case.phases[index].certification;
    }
    post_upfront_items(config, result);

    double remaining_commitment = config.terms.provider_commitment_million;
    double remaining_sponsor =
        config.terms.sponsor_construction_commitment_million;
    double claim = 0.0;
    double cumulative_principal = 0.0;
    std::size_t current_month = 0U;
    bool terminated = false;

    const auto terminate_before_draw = [&](std::size_t phase_index,
                                           StagedCapitalOutcome outcome,
                                           double funding_gap) {
        auto& phase_result = result.phases[phase_index];
        result.outcome = outcome;
        result.stop_phase_index = phase_index;
        result.outcome_month = current_month;
        result.recovery_month =
            current_month + configured_case.recovery_delay_months;
        result.funding_gap_million = funding_gap;
        phase_result.funding_gap_million = funding_gap;
        phase_result.cancelled_availability_million = remaining_commitment;
        result.unused_commitment_cancelled_million += remaining_commitment;
        remaining_commitment = 0.0;
        phase_result.closing_undrawn_commitment_million = 0.0;
        settle_exit(config, configured_case, false, current_month,
            result.recovery_month, cumulative_principal, claim,
            phase_result, result);
        result.stranded_spend_million =
            result.cumulative_eligible_spend_million;
        terminated = true;
    };

    for (std::size_t index = 0U;
         index < config.phases.size() && !terminated; ++index) {
        const auto& phase_terms = config.phases[index];
        const auto& phase_case = configured_case.phases[index];
        auto& phase_result = result.phases[index];
        phase_result.reached = true;
        phase_result.start_month = current_month;
        phase_result.end_month = current_month;
        phase_result.eligible_cost_million =
            phase_case.actual_eligible_cost_million;
        phase_result.opening_undrawn_commitment_million =
            remaining_commitment;
        phase_result.opening_funded_claim_million = claim;

        const double provider_entitlement = std::min({
            phase_case.actual_eligible_cost_million *
                config.terms.provider_cost_share,
            phase_terms.provider_stage_cap_million,
            remaining_commitment});
        const double sponsor_required =
            phase_case.actual_eligible_cost_million - provider_entitlement;
        phase_result.provider_draw_entitlement_million =
            provider_entitlement;
        phase_result.sponsor_contribution_required_million =
            sponsor_required;

        // There is one draw per phase. Any current-phase cap above the draw
        // entitlement is unreachable and cancels at this decision date, so it
        // cannot support the cost-to-complete test.
        const double unused_current_stage = std::max(
            0.0, phase_terms.provider_stage_cap_million -
                     provider_entitlement);
        if (sponsor_required >
            remaining_sponsor + tolerance_for(remaining_sponsor)) {
            terminate_before_draw(index,
                StagedCapitalOutcome::SponsorFundingFailure,
                sponsor_required - remaining_sponsor);
            break;
        }

        const double remaining_cost_estimate = std::max(0.0,
            phase_case.estimated_cost_to_complete_million -
                phase_case.actual_eligible_cost_million);
        const double future_provider_commitment = std::max(0.0,
            remaining_commitment - provider_entitlement -
                unused_current_stage);
        const double future_provider_sources = std::min(
            future_provider_commitment,
            config.terms.provider_cost_share * remaining_cost_estimate);
        const double future_sponsor_sources =
            std::max(0.0, remaining_sponsor - sponsor_required);
        const double future_sources =
            future_sponsor_sources + future_provider_sources;
        if (remaining_cost_estimate >
            future_sources + tolerance_for(future_sources)) {
            terminate_before_draw(index,
                StagedCapitalOutcome::CostToCompleteFailure,
                remaining_cost_estimate - future_sources);
            break;
        }
        if (provider_entitlement > 0.0 && !phase_case.provider_funds) {
            terminate_before_draw(index,
                StagedCapitalOutcome::ProviderFundingFailure,
                provider_entitlement);
            break;
        }

        phase_result.funded = true;
        phase_result.sponsor_contribution_million = sponsor_required;
        phase_result.provider_draw_received_million = provider_entitlement;
        remaining_sponsor =
            std::max(0.0, remaining_sponsor - sponsor_required);
        remaining_commitment =
            std::max(0.0, remaining_commitment - provider_entitlement);
        cumulative_principal += provider_entitlement;
        claim += provider_entitlement;
        result.cumulative_eligible_spend_million +=
            phase_case.actual_eligible_cost_million;
        result.total_provider_draws_million += provider_entitlement;
        result.peak_provider_funded_principal_million = std::max(
            result.peak_provider_funded_principal_million,
            cumulative_principal);
        result.total_sponsor_construction_contributions_million +=
            sponsor_required;

        if (sponsor_required > 0.0) {
            post_cash(result, current_month,
                CapitalCashFlowKind::SponsorConstructionContribution,
                phase_terms.id + ": sponsor construction contribution",
                CapitalCashPosting{-sponsor_required, sponsor_required,
                    0.0, 0.0, 0.0});
        }
        if (provider_entitlement > 0.0) {
            post_cash(result, current_month,
                CapitalCashFlowKind::ProviderDraw,
                phase_terms.id + ": provider draw",
                CapitalCashPosting{0.0, provider_entitlement,
                    -provider_entitlement, 0.0, 0.0});
        }
        if (phase_case.actual_eligible_cost_million > 0.0) {
            post_cash(result, current_month,
                CapitalCashFlowKind::EligibleConstructionUse,
                phase_terms.id + ": modeled aggregate eligible use",
                CapitalCashPosting{0.0,
                    -phase_case.actual_eligible_cost_million, 0.0, 0.0,
                phase_case.actual_eligible_cost_million});
        }

        // Version 0.1 permits one aggregate draw at the phase start. Any
        // unused capacity assigned to that phase therefore ceases to be
        // drawable immediately and cannot remain in the commitment-fee base.
        const double current_stage_cancellation =
            std::min(remaining_commitment, unused_current_stage);
        remaining_commitment =
            std::max(0.0, remaining_commitment - current_stage_cancellation);
        phase_result.cancelled_availability_million +=
            current_stage_cancellation;
        result.unused_commitment_cancelled_million +=
            current_stage_cancellation;

        for (std::size_t elapsed = 0U;
             elapsed < phase_terms.duration_months; ++elapsed) {
            ++current_month;
            phase_result.contractual_return_accrued_million +=
                accrue_claim_for_months(
                    config.terms, 1U, cumulative_principal, claim);
            const double commitment_fee = remaining_commitment *
                config.terms.annual_commitment_fee_rate / 12.0;
            phase_result.commitment_fees_million += commitment_fee;
            result.total_commitment_fees_million += commitment_fee;
            if (commitment_fee > 0.0) {
                post_cash(result, current_month,
                    CapitalCashFlowKind::CommitmentFee,
                    phase_terms.id + ": sponsor-paid commitment fee",
                    CapitalCashPosting{-commitment_fee, 0.0,
                        commitment_fee, 0.0, 0.0});
            }
        }
        phase_result.end_month = current_month;

        if (phase_case.certification ==
            CertificationDecision::FinalFailure) {
            result.outcome = StagedCapitalOutcome::MilestoneFailure;
            result.stop_phase_index = index;
            result.outcome_month = current_month;
            result.recovery_month =
                current_month + configured_case.recovery_delay_months;
            phase_result.cancelled_availability_million +=
                remaining_commitment;
            result.unused_commitment_cancelled_million +=
                remaining_commitment;
            remaining_commitment = 0.0;
            phase_result.closing_undrawn_commitment_million = 0.0;
            settle_exit(config, configured_case, false, current_month,
                result.recovery_month, cumulative_principal, claim,
                phase_result, result);
            result.stranded_spend_million =
                result.cumulative_eligible_spend_million;
            terminated = true;
            break;
        }

        const bool final_phase = index + 1U == config.phases.size();
        if (final_phase) {
            result.outcome = StagedCapitalOutcome::Completed;
            result.outcome_month = current_month;
            result.recovery_month = current_month;
            phase_result.cancelled_availability_million +=
                remaining_commitment;
            result.unused_commitment_cancelled_million +=
                remaining_commitment;
            remaining_commitment = 0.0;
            phase_result.closing_undrawn_commitment_million = 0.0;
            settle_exit(config, configured_case, true, current_month,
                current_month, cumulative_principal, claim,
                phase_result, result);
            terminated = true;
            break;
        }

        phase_result.closing_undrawn_commitment_million =
            remaining_commitment;
        phase_result.closing_funded_claim_million = claim;
    }

    result.closing_undrawn_commitment_million = remaining_commitment;
    result.closing_funded_claim_million = claim;
    result.provider_npv_before_upfront_fee_million = ledger_npv(result,
        config.terms.provider_hurdle_rate, true, true);
    result.provider_npv_after_upfront_fee_million = ledger_npv(result,
        config.terms.provider_hurdle_rate, true, false);
    result.sponsor_npv_million = ledger_npv(result,
        config.terms.sponsor_discount_rate, false, false);
    finalize_path_invariants(result);
    return result;
}

} // namespace

StagedCapitalPathResult evaluate_staged_capital_case(
    const StagedCapitalConfig& config, std::string_view case_id) {
    validate_staged_capital_config(config);
    const auto matching_case = std::find_if(config.cases.begin(),
        config.cases.end(), [case_id](const auto& candidate) {
            return candidate.id == case_id;
        });
    if (matching_case == config.cases.end()) {
        throw std::invalid_argument(
            "staged-capital case id is not part of the validated config");
    }
    return evaluate_validated_case(config, *matching_case);
}

StagedCapitalSummary evaluate_staged_capital_cases(
    const StagedCapitalConfig& config) {
    validate_staged_capital_config(config);
    StagedCapitalSummary summary;
    summary.charged_upfront_fee_million = config.terms.upfront_fee_million;
    long double raw_weight_sum = 0.0L;
    for (const auto& scenario_case : config.cases) {
        raw_weight_sum += static_cast<long double>(scenario_case.weight);
    }
    summary.configured_case_weight_sum =
        static_cast<double>(raw_weight_sum);

    std::vector<std::pair<double, double>> provider_draws;
    std::vector<std::pair<double, double>> peak_outlay;
    std::vector<std::pair<double, double>> provider_losses;
    std::vector<std::pair<double, double>> provider_claim_writeoffs;
    std::vector<std::pair<double, double>> sponsor_calls;
    std::vector<std::pair<double, double>> stranded_spend;
    std::vector<std::pair<double, double>> funding_gaps;
    std::vector<std::pair<double, double>> reserve_shortfalls_at_stop;
    std::vector<std::pair<double, double>> safety_shortfalls;
    std::vector<std::pair<double, double>> outcome_months;
    provider_draws.reserve(config.cases.size());
    peak_outlay.reserve(config.cases.size());
    provider_losses.reserve(config.cases.size());
    provider_claim_writeoffs.reserve(config.cases.size());
    sponsor_calls.reserve(config.cases.size());
    stranded_spend.reserve(config.cases.size());
    funding_gaps.reserve(config.cases.size());
    reserve_shortfalls_at_stop.reserve(config.cases.size());
    safety_shortfalls.reserve(config.cases.size());
    outcome_months.reserve(config.cases.size());

    double weighted_loss_on_loss_paths = 0.0;
    double weighted_claim_writeoff_on_writeoff_paths = 0.0;
    for (const auto& scenario_case : config.cases) {
        StagedCapitalPathResult path =
            evaluate_validated_case(config, scenario_case);
        const double weight = static_cast<double>(
            static_cast<long double>(scenario_case.weight) /
            raw_weight_sum);
        switch (path.outcome) {
        case StagedCapitalOutcome::Completed:
            summary.completion_weight += weight;
            break;
        case StagedCapitalOutcome::MilestoneFailure:
            summary.milestone_failure_weight += weight;
            break;
        case StagedCapitalOutcome::CostToCompleteFailure:
            summary.cost_to_complete_failure_weight += weight;
            break;
        case StagedCapitalOutcome::SponsorFundingFailure:
            summary.sponsor_funding_failure_weight += weight;
            break;
        case StagedCapitalOutcome::ProviderFundingFailure:
            summary.provider_funding_failure_weight += weight;
            break;
        }
        if (path.total_provider_draws_million > 0.0) {
            summary.provider_draw_weight += weight;
        }
        if (path.provider_principal_loss_million > 0.0) {
            summary.provider_principal_loss_weight += weight;
            weighted_loss_on_loss_paths +=
                weight * path.provider_principal_loss_million;
        }
        if (path.provider_claim_writeoff_million > 0.0) {
            summary.provider_claim_writeoff_weight += weight;
            weighted_claim_writeoff_on_writeoff_paths +=
                weight * path.provider_claim_writeoff_million;
        }
        if (path.protected_reserve_shortfall_at_stop_million > 0.0) {
            summary.protected_reserve_shortfall_at_stop_weight += weight;
        }
        if (path.safety_funding_shortfall_million > 0.0) {
            summary.safety_funding_shortfall_weight += weight;
        }
        summary.expected_commitment_utilization += weight *
            path.total_provider_draws_million /
            config.terms.provider_commitment_million;
        summary.expected_provider_nominal_recovery_million +=
            weight * path.provider_nominal_recovery_million;
        summary.expected_provider_recovery_pv_million +=
            weight * path.provider_recovery_pv_million;
        summary.expected_provider_principal_loss_million +=
            weight * path.provider_principal_loss_million;
        summary.expected_provider_claim_writeoff_million +=
            weight * path.provider_claim_writeoff_million;
        summary.maximum_cash_entry_imbalance_million = std::max(
            summary.maximum_cash_entry_imbalance_million,
            path.maximum_cash_entry_imbalance_million);
        summary.maximum_memo_rollforward_imbalance_million = std::max(
            summary.maximum_memo_rollforward_imbalance_million,
            path.maximum_memo_rollforward_imbalance_million);

        provider_draws.emplace_back(path.total_provider_draws_million, weight);
        peak_outlay.emplace_back(
            path.peak_provider_net_cash_outlay_million, weight);
        provider_losses.emplace_back(
            path.provider_principal_loss_million, weight);
        provider_claim_writeoffs.emplace_back(
            path.provider_claim_writeoff_million, weight);
        sponsor_calls.emplace_back(sponsor_total_cash_call(path), weight);
        stranded_spend.emplace_back(path.stranded_spend_million, weight);
        funding_gaps.emplace_back(path.funding_gap_million, weight);
        reserve_shortfalls_at_stop.emplace_back(
            path.protected_reserve_shortfall_at_stop_million, weight);
        safety_shortfalls.emplace_back(
            path.safety_funding_shortfall_million, weight);
        outcome_months.emplace_back(
            static_cast<double>(path.outcome_month), weight);
        summary.cases.push_back(std::move(path));
    }

    if (summary.provider_principal_loss_weight > 0.0) {
        summary.conditional_provider_principal_loss_million =
            weighted_loss_on_loss_paths /
            summary.provider_principal_loss_weight;
    }
    if (summary.provider_claim_writeoff_weight > 0.0) {
        summary.conditional_provider_claim_writeoff_million =
            weighted_claim_writeoff_on_writeoff_paths /
            summary.provider_claim_writeoff_weight;
    }
    summary.provider_draws_million = summarize_weighted(provider_draws);
    summary.peak_provider_net_cash_outlay_million =
        summarize_weighted(peak_outlay);
    summary.provider_principal_loss_million =
        summarize_weighted(provider_losses);
    summary.provider_claim_writeoff_million =
        summarize_weighted(provider_claim_writeoffs);
    summary.sponsor_total_cash_call_million =
        summarize_weighted(sponsor_calls);
    summary.stranded_spend_million = summarize_weighted(stranded_spend);
    summary.funding_gap_million = summarize_weighted(funding_gaps);
    summary.protected_reserve_shortfall_at_stop_million =
        summarize_weighted(reserve_shortfalls_at_stop);
    summary.safety_funding_shortfall_million =
        summarize_weighted(safety_shortfalls);
    summary.outcome_month = summarize_weighted(outcome_months);

    // Fee adequacy must not reward the provider for its own failure or change
    // the declared physical-state mix by dropping correlated cases. Replay
    // every exact case with provider cash performance held true and retain all
    // configured weights. The actual paths above remain the counterparty-risk
    // view reported to the project.
    StagedCapitalConfig fee_config = config;
    for (auto& fee_case : fee_config.cases) {
        for (auto& phase : fee_case.phases) {
            phase.provider_funds = true;
        }
    }
    validate_staged_capital_config(fee_config);
    for (const auto& fee_case : fee_config.cases) {
        const StagedCapitalPathResult fee_path =
            evaluate_validated_case(fee_config, fee_case);
        const double fee_weight = static_cast<double>(
            static_cast<long double>(fee_case.weight) / raw_weight_sum);
        summary.fee_sensitivity_included_weight += fee_weight;
        summary.expected_provider_npv_before_upfront_fee_million +=
            fee_weight *
            fee_path.provider_npv_before_upfront_fee_million;
        summary.expected_provider_npv_after_charged_upfront_fee_million +=
            fee_weight * fee_path.provider_npv_after_upfront_fee_million;
        summary.fee_basis_cases.push_back(StagedCapitalFeeCaseResult{
            fee_case.id, fee_weight, fee_path.outcome,
            fee_path.outcome_month, fee_path.recovery_month,
            fee_path.total_provider_draws_million,
            fee_path.provider_nominal_recovery_million,
            fee_path.provider_npv_before_upfront_fee_million,
            fee_path.provider_npv_after_upfront_fee_million});
    }
    if (std::abs(summary.fee_sensitivity_included_weight - 1.0) >
        kProbabilityTolerance) {
        throw std::logic_error(
            "paired provider-performance fee cases do not sum to one");
    }

    summary.physical_measure_break_even_upfront_fee_million =
        -summary.expected_provider_npv_before_upfront_fee_million;
    summary.upfront_fee_adequacy_gap_million =
        summary.charged_upfront_fee_million -
        summary.physical_measure_break_even_upfront_fee_million;
    if (!nearly_equal(
            summary.expected_provider_npv_after_charged_upfront_fee_million,
            summary.upfront_fee_adequacy_gap_million)) {
        throw std::logic_error(
            "physical-measure fee identity failed to reconcile");
    }
    return summary;
}

} // namespace naturalehia::cellular_finance
