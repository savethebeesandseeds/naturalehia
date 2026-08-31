// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/partial_credit_claim_loss_cohort.hpp>

#include <naturalehia/cellular_finance/joint_cohort.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 4'096U;
constexpr double kMoneyTolerance = 1.0e-10;

[[nodiscard]] bool ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
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
            std::string(description) + " must be a safe identifier");
    }
}

void require_safe_text(
    std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
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

[[nodiscard]] bool lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

void require_date(std::string_view value, std::string_view description) {
    if (!is_joint_cohort_iso_date(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a valid ISO date");
    }
}

void require_optional_date(std::string_view value,
    std::string_view as_of_date, std::string_view description) {
    if (value == "NONE") return;
    require_date(value, description);
    if (value > as_of_date) {
        throw std::invalid_argument(
            std::string(description) + " cannot follow the cohort as-of date");
    }
}

void require_sorted_unique_ids(
    const std::vector<std::string>& values,
    std::string_view description, bool may_be_empty) {
    if (values.size() > kMaximumPartialCreditCohortEvidenceIdsPerList) {
        throw std::invalid_argument(
            std::string(description) + " exceed the per-list resource guardrail");
    }
    if (!may_be_empty && values.empty()) {
        throw std::invalid_argument(
            std::string(description) + " must not be empty");
    }
    if (!std::is_sorted(values.begin(), values.end()) ||
        std::adjacent_find(values.begin(), values.end()) != values.end()) {
        throw std::invalid_argument(
            std::string(description) + " must be sorted and unique");
    }
    for (const std::string& value : values) {
        require_safe_identifier(value, description);
    }
}

[[nodiscard]] double checked_double(long double value) {
    const double result = static_cast<double>(value);
    if (!std::isfinite(result)) {
        throw std::overflow_error(
            "partial-credit cohort amount exceeded numeric range");
    }
    return result;
}

[[nodiscard]] bool nearly_equal(double first, double second) noexcept {
    const double scale =
        std::max({1.0, std::abs(first), std::abs(second)});
    return std::abs(first - second) <= kMoneyTolerance * scale;
}

[[nodiscard]] double exact_nonnegative(
    const ClaimLedgerValue& value, std::string_view description) {
    if (value.status != ClaimLedgerValueStatus::Known ||
        !value.lower.has_value() || !value.upper.has_value() ||
        !std::isfinite(*value.lower) || !std::isfinite(*value.upper) ||
        *value.lower < 0.0 || *value.upper != *value.lower) {
        throw std::invalid_argument(
            std::string(description) + " must be an exact non-negative amount");
    }
    return *value.lower;
}

[[nodiscard]] bool exact_zero(
    const ClaimLedgerValue& value) noexcept {
    return value.status == ClaimLedgerValueStatus::Known &&
        value.lower.has_value() && value.upper.has_value() &&
        *value.lower == 0.0 && *value.upper == 0.0;
}

template <typename Selector>
[[nodiscard]] double sum_exact_period_amount(
    const ClaimLedgerPathResult& path, Selector selector,
    std::string_view description) {
    long double total = 0.0L;
    for (const ClaimLedgerPeriodResult& period : path.periods) {
        total += static_cast<long double>(
            exact_nonnegative(selector(period), description));
    }
    return checked_double(total);
}

[[nodiscard]] const ClaimLedgerScenarioResult& selected_full_scenario(
    const ClaimLedgerPackage& package, std::string_view scenario_id) {
    if (!package.full_path_evaluation_available ||
        !package.full_evaluation.has_value()) {
        throw std::invalid_argument(
            "resolved cohort observation requires a verified full Claim Ledger evaluation");
    }
    const auto found = std::find_if(
        package.full_evaluation->scenarios.begin(),
        package.full_evaluation->scenarios.end(),
        [scenario_id](const ClaimLedgerScenarioResult& scenario) {
            return scenario.scenario_id == scenario_id;
        });
    if (found == package.full_evaluation->scenarios.end()) {
        throw std::invalid_argument(
            "resolved cohort observation selects an unknown full scenario");
    }
    return *found;
}

[[nodiscard]] const ClaimLedgerConfig* decision_config(
    const ClaimLedgerPackage& package) noexcept {
    if (package.core_config.has_value()) {
        return &*package.core_config;
    }
    return nullptr;
}

[[nodiscard]] const ClaimLedgerProviderClaim* provider_term(
    const ClaimLedgerPackage& package, std::string_view provider_claim_id) {
    const ClaimLedgerConfig* config = decision_config(package);
    if (config == nullptr) return nullptr;
    const auto found = std::find_if(config->provider_claims.begin(),
        config->provider_claims.end(),
        [provider_claim_id](const ClaimLedgerProviderClaim& provider) {
            return provider.provider_claim_id == provider_claim_id;
        });
    return found == config->provider_claims.end() ? nullptr : &*found;
}

void require_principal_only_provider(
    const ClaimLedgerProviderClaim& provider) {
    if (!provider.covers_principal_due || provider.covers_interest_due) {
        throw std::invalid_argument(
            "partial-credit cohort v0.1 requires a principal-only provider term stratum");
    }
}

void require_provider_structure(
    const ClaimLedgerProviderClaim& provider) {
    require_principal_only_provider(provider);
    if (!provider.payment_right_evidenced ||
        !provider.provider_identity_evidenced ||
        !provider.coverage_and_priority_evidenced ||
        !provider.obligation_priority.has_value()) {
        throw std::invalid_argument(
            "open claim provider rights, identity, coverage and priority must be structurally evidenced");
    }
}

[[nodiscard]] double exact_provider_outer_cash_cap(
    const ClaimLedgerProviderClaim& provider) {
    require_provider_structure(provider);
    if (exact_zero(provider.shortfall_allocation_fraction) ||
        exact_zero(provider.coverage_fraction) ||
        exact_zero(provider.maximum_cash_million)) {
        return 0.0;
    }
    (void)exact_nonnegative(
        provider.shortfall_allocation_fraction,
        "open claim provider shortfall allocation");
    (void)exact_nonnegative(
        provider.coverage_fraction,
        "open claim provider coverage fraction");
    (void)exact_nonnegative(
        provider.deductible_million,
        "open claim provider deductible");
    const double maximum_cash = exact_nonnegative(
        provider.maximum_cash_million,
        "open claim provider maximum cash");
    (void)exact_nonnegative(provider.settlement_lag_periods,
        "open claim provider settlement lag");
    // The exact-zero cases were handled before unrelated numerical fields
    // were required, so an unknown deductible or lag cannot erase a proven
    // zero contractual payout.
    // Face is only a peak-principal cap. Without a separately evidenced
    // lifetime principal/shortfall cap, it cannot safely reduce a revolving
    // claim's cumulative provider exposure. The exact lifetime maximum-cash
    // term is therefore the only non-zero finite outer cap in this kernel.
    return maximum_cash;
}

struct ReverifiedClaimPackage {
    ClaimLedgerPackage package{};
    std::optional<ClaimLedgerPathEvidenceSnapshot> full_path_evidence{};
};

[[nodiscard]] ReverifiedClaimPackage reverify_package(
    const PartialCreditClaimLossObservation& observation) {
    if (!observation.claim_package.has_value() ||
        !observation.claim_package->package_integrity ||
        observation.claim_package->directory.empty() ||
        observation.claim_package->claim_config_filename !=
            std::filesystem::path("claim.cfg") ||
        !lower_hex_sha256(
            observation.claim_package->claim_config_sha256) ||
        !lower_hex_sha256(observation.expected_claim_config_sha256)) {
        throw std::logic_error(
            "partial-credit cohort requires a loader-verified Claim Ledger root");
    }
    const std::filesystem::path claim_config_path =
        observation.claim_package->directory /
        observation.claim_package->claim_config_filename;
    ReverifiedClaimPackage verified;
    if (observation.disposition ==
            PartialCreditClaimLossDisposition::Resolved) {
        ClaimLedgerPackageWithPathEvidence loaded =
            load_claim_ledger_package_with_full_path_evidence(
                claim_config_path, observation.realized_scenario_id);
        verified.package = std::move(loaded.package);
        verified.full_path_evidence = std::move(loaded.full_path);
    } else {
        verified.package = load_claim_ledger_package(claim_config_path);
    }
    if (verified.package.claim_config_sha256 !=
            observation.claim_package->claim_config_sha256 ||
        verified.package.claim_config_sha256 !=
            observation.expected_claim_config_sha256) {
        throw std::logic_error(
            "partial-credit cohort Claim Ledger root changed after verification");
    }
    if (verified.package.config.economic_cluster_boundary_status !=
            ClaimLedgerEconomicClusterBoundaryStatus::Defined ||
        verified.package.config.economic_cluster_id !=
            observation.economic_cluster_id) {
        throw std::invalid_argument(
            "cohort observation and verified Claim Ledger economic cluster do not match");
    }
    return verified;
}

[[nodiscard]] ClaimLedgerValue known_or_bounded(
    double lower, double upper) {
    return lower == upper ? claim_ledger_known(lower) :
        claim_ledger_bounded(lower, upper);
}

[[nodiscard]] bool amount_available(const ClaimLedgerValue& value) noexcept {
    return (value.status == ClaimLedgerValueStatus::Known ||
            value.status == ClaimLedgerValueStatus::Bounded) &&
        value.lower.has_value() && value.upper.has_value();
}

[[nodiscard]] bool known_positive(
    const ClaimLedgerValue& value) noexcept {
    return amount_available(value) && value.lower.has_value() &&
        *value.lower > 0.0;
}

[[nodiscard]] bool possibly_positive(const ClaimLedgerValue& value,
    bool unknown_is_compatible) noexcept {
    if (!amount_available(value)) return unknown_is_compatible;
    return value.upper.has_value() && *value.upper > 0.0;
}

[[nodiscard]] bool valid_disposition(
    PartialCreditClaimLossDisposition value) noexcept {
    switch (value) {
    case PartialCreditClaimLossDisposition::Resolved:
    case PartialCreditClaimLossDisposition::NotYetMatured:
    case PartialCreditClaimLossDisposition::Unresolved:
    case PartialCreditClaimLossDisposition::Excluded:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid_trigger_status(
    PartialCreditClaimTriggerStatus value) noexcept {
    switch (value) {
    case PartialCreditClaimTriggerStatus::Triggered:
    case PartialCreditClaimTriggerStatus::NotTriggered:
    case PartialCreditClaimTriggerStatus::Unknown:
    case PartialCreditClaimTriggerStatus::NotApplicable:
        return true;
    }
    return false;
}

[[nodiscard]] ClaimLedgerValue sum_ranges(
    const std::vector<PartialCreditClaimLossObservationResult>& rows,
    const ClaimLedgerValue PartialCreditClaimLossObservationResult::*member) {
    long double lower = 0.0L;
    long double upper = 0.0L;
    for (const PartialCreditClaimLossObservationResult& row : rows) {
        if (row.disposition == PartialCreditClaimLossDisposition::Excluded) {
            continue;
        }
        const ClaimLedgerValue& value = row.*member;
        if (!amount_available(value)) return claim_ledger_unknown();
        lower += static_cast<long double>(*value.lower);
        upper += static_cast<long double>(*value.upper);
    }
    return known_or_bounded(checked_double(lower), checked_double(upper));
}

[[nodiscard]] ClaimLedgerValue sum_resolved_known(
    const std::vector<PartialCreditClaimLossObservationResult>& rows,
    const ClaimLedgerValue PartialCreditClaimLossObservationResult::*member) {
    long double total = 0.0L;
    for (const PartialCreditClaimLossObservationResult& row : rows) {
        if (!row.resolved_path_exact) continue;
        total += static_cast<long double>(
            exact_nonnegative(row.*member, "resolved cohort amount"));
    }
    return claim_ledger_known(checked_double(total));
}

[[nodiscard]] ClaimLedgerValue frequency_range(
    std::size_t lower_count, std::size_t upper_count,
    std::size_t denominator) {
    if (denominator == 0U) return claim_ledger_not_applicable();
    if (lower_count > upper_count || upper_count > denominator) {
        throw std::logic_error(
            "partial-credit cohort frequency counts do not reconcile");
    }
    return known_or_bounded(
        static_cast<double>(lower_count) /
            static_cast<double>(denominator),
        static_cast<double>(upper_count) /
            static_cast<double>(denominator));
}

[[nodiscard]] ClaimLedgerValue ratio_to_exact_denominator(
    const ClaimLedgerValue& numerator,
    const ClaimLedgerValue& denominator) {
    if (!amount_available(numerator) ||
        denominator.status != ClaimLedgerValueStatus::Known ||
        !denominator.lower.has_value() ||
        !(*denominator.lower > 0.0)) {
        return claim_ledger_unknown();
    }
    return known_or_bounded(
        *numerator.lower / *denominator.lower,
        *numerator.upper / *denominator.lower);
}

[[nodiscard]] PartialCreditClaimLossObservationResult
evaluate_resolved_observation(
    const PartialCreditClaimLossObservation& observation,
    const ClaimLedgerPackage& package,
    const ClaimLedgerPathEvidenceSnapshot& path_evidence) {
    PartialCreditClaimLossObservationResult result;
    result.observation_id = observation.observation_id;
    result.economic_cluster_id = observation.economic_cluster_id;
    result.disposition = observation.disposition;
    result.trigger_status = observation.trigger_status;
    result.package_id = package.config.package_id;
    result.claim_id = package.config.claim_id;
    result.claim_config_sha256 = package.claim_config_sha256;
    result.realized_scenario_id = observation.realized_scenario_id;
    result.provider_claim_id = observation.provider_claim_id;
    result.synthetic_package =
        package.config.package_status ==
        ClaimLedgerPackageStatus::SyntheticComplete;
    result.claim_package_has_blockers = !package.blockers.empty();

    if (path_evidence.package_directory != package.directory ||
        path_evidence.package_id != package.config.package_id ||
        path_evidence.claim_id != package.config.claim_id ||
        path_evidence.claim_config_sha256 !=
            package.claim_config_sha256 ||
        path_evidence.scenario_id != observation.realized_scenario_id ||
        path_evidence.currency_label != package.config.currency_label ||
        path_evidence.monetary_basis != package.config.monetary_basis ||
        !package.full_evaluation.has_value() ||
        path_evidence.full_evaluation_scenario_index >=
            package.full_evaluation->scenarios.size() ||
        package.full_evaluation->scenarios[
            path_evidence.full_evaluation_scenario_index]
                .scenario_id != path_evidence.scenario_id) {
        throw std::logic_error(
            "resolved cohort selected-path evidence identity does not match its verified Claim Ledger package");
    }
    if (path_evidence.cash_path_status.status_was_unknown ||
        !path_evidence.cash_path_status.known_at_period.has_value() ||
        path_evidence.cash_path_status.cash_path_status !=
            ClaimLedgerCashPathStatus::CompleteResolved) {
        throw std::invalid_argument(
            "resolved cohort observation requires an explicit complete-resolved cash-path status");
    }
    result.selected_full_path_provenance_verified_during_evaluation = true;

    const ClaimLedgerScenarioResult& selected = selected_full_scenario(
        package, observation.realized_scenario_id);
    const ClaimLedgerPathResult& path = selected.full_path;
    if (!path.exact || !path.settlement_reconciled ||
        !path.rollforwards_reconciled ||
        !path.contractual_face_reconciled || !path.blockers.empty()) {
        throw std::invalid_argument(
            "resolved cohort observation requires an exact reconciled full path");
    }
    const double terminal_principal = exact_nonnegative(
        path.terminal_principal_million, "terminal principal");
    const double terminal_interest = exact_nonnegative(
        path.terminal_accrued_interest_million,
        "terminal accrued interest");
    if (!nearly_equal(terminal_principal, 0.0) ||
        !nearly_equal(terminal_interest, 0.0)) {
        throw std::invalid_argument(
            "resolved cohort observation has non-zero terminal exposure");
    }
    if (path.periods.empty() ||
        !nearly_equal(exact_nonnegative(
                path.periods.back().outstanding_principal_due_million,
                "terminal outstanding principal due"), 0.0) ||
        !nearly_equal(exact_nonnegative(
                path.periods.back().outstanding_interest_due_million,
                "terminal outstanding interest due"), 0.0)) {
        throw std::invalid_argument(
            "resolved cohort observation has non-zero terminal due balances");
    }

    const ClaimLedgerConfig* core = decision_config(package);
    if (core == nullptr) {
        throw std::invalid_argument(
            "resolved cohort observation requires a verified decision-time Claim Ledger configuration");
    }
    const double contractual_face = exact_nonnegative(
        core->contractual_face_amount_million,
        "decision-time contractual face");
    const double peak_ead = exact_nonnegative(
        path.peak_ead_million, "Claim Ledger peak EAD");
    const double opening_principal = exact_nonnegative(
        path.periods.front().opening_principal_million,
        "resolved opening principal");
    const double funded_principal = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.funded_principal_million;
        }, "funded principal created");
    const double capitalized_fee = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.capitalized_fee_million;
        }, "capitalized fee principal");
    const double capitalized_interest = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.capitalized_interest_million;
        }, "capitalized interest principal");
    const double capitalized_principal = checked_double(
        static_cast<long double>(capitalized_fee) +
        static_cast<long double>(capitalized_interest));
    const double principal_at_risk = checked_double(
        static_cast<long double>(opening_principal) +
        static_cast<long double>(funded_principal) +
        static_cast<long double>(capitalized_principal));
    const double borrower_principal = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.principal_cash_million;
        }, "borrower principal cash");
    const double recovery_principal = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.recovery_principal_cash_million;
        }, "recovery principal cash");
    const double pre_support_shortfall = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.principal_shortfall_after_borrower_recovery_million;
        }, "pre-support principal shortfall");
    const double aggregate_provider_principal_cash =
        sum_exact_period_amount(path,
            [](const ClaimLedgerPeriodResult& period)
                -> const ClaimLedgerValue& {
                return period.guarantee_principal_cash_million;
            }, "aggregate provider principal cash");
    const double conversion_principal = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.conversion_principal_extinguishment_million;
        }, "conversion principal extinguishment");
    const double writeoff = sum_exact_period_amount(path,
        [](const ClaimLedgerPeriodResult& period) -> const ClaimLedgerValue& {
            return period.principal_writeoff_million;
        }, "final principal writeoff");
    const double path_principal_loss = exact_nonnegative(
        path.principal_loss_million, "path principal loss");
    if (!nearly_equal(writeoff, path_principal_loss)) {
        throw std::logic_error(
            "resolved cohort writeoff does not reconcile to Claim Ledger principal loss");
    }

    double provider_claim = 0.0;
    double provider_payable = 0.0;
    double provider_cash = 0.0;
    double provider_unpaid = 0.0;
    double provider_after_horizon = 0.0;
    if (observation.provider_claim_id == "NONE") {
        if (!core->provider_claims.empty() ||
            !path.provider_claims.empty() ||
            !nearly_equal(aggregate_provider_principal_cash, 0.0)) {
            throw std::invalid_argument(
                "resolved cohort row omits an applicable provider claim");
        }
    } else {
        if (core->provider_claims.size() != 1U) {
            throw std::invalid_argument(
                "partial-credit cohort v0.1 requires exactly one ex-ante provider term");
        }
        const ClaimLedgerProviderClaim* term =
            provider_term(package, observation.provider_claim_id);
        if (term == nullptr) {
            throw std::invalid_argument(
                "resolved cohort row names an unknown provider claim");
        }
        require_principal_only_provider(*term);
        if (path.provider_claims.size() != 1U ||
            path.provider_claims.front().provider_claim_id !=
                observation.provider_claim_id) {
            throw std::invalid_argument(
                "partial-credit cohort v0.1 requires exactly one selected provider path");
        }
        const ClaimLedgerProviderPathResult& provider =
            path.provider_claims.front();
        if (!provider.computable || !provider.blockers.empty()) {
            throw std::invalid_argument(
                "resolved cohort provider path must be computable");
        }
        provider_claim = exact_nonnegative(
            provider.total_claim_generated_million,
            "provider claim generated");
        const double total_provider_cash = exact_nonnegative(
            provider.total_guarantee_cash_million,
            "provider guarantee cash");
        provider_unpaid = exact_nonnegative(
            provider.terminal_unpaid_payable_claim_million,
            "provider unpaid payable claim");
        provider_after_horizon = exact_nonnegative(
            provider.claim_payable_after_horizon_million,
            "provider claim payable after horizon");
        long double payable_sum = 0.0L;
        long double cash_sum = 0.0L;
        for (const ClaimLedgerProviderPeriodResult& period :
             provider.periods) {
            payable_sum += static_cast<long double>(exact_nonnegative(
                period.principal_claim_payable_million,
                "provider principal claim payable"));
            cash_sum += static_cast<long double>(exact_nonnegative(
                period.guarantee_principal_cash_million,
                "provider principal cash"));
        }
        provider_payable = checked_double(payable_sum);
        provider_cash = checked_double(cash_sum);
        if (!nearly_equal(provider_after_horizon, 0.0) ||
            !nearly_equal(provider_unpaid, 0.0) ||
            !nearly_equal(provider_cash, total_provider_cash) ||
            !nearly_equal(provider_cash,
                aggregate_provider_principal_cash)) {
            throw std::invalid_argument(
                "resolved cohort provider cash or horizon settlement does not reconcile");
        }
    }

    const double resolved_principal = checked_double(
        static_cast<long double>(borrower_principal) +
        static_cast<long double>(recovery_principal) +
        static_cast<long double>(aggregate_provider_principal_cash) +
        static_cast<long double>(conversion_principal) +
        static_cast<long double>(writeoff) +
        static_cast<long double>(terminal_principal));
    if (!nearly_equal(principal_at_risk, resolved_principal)) {
        throw std::logic_error(
            "resolved cohort principal cash and extinguishment do not conserve actual principal created");
    }
    const bool triggered = pre_support_shortfall > 0.0;
    if ((triggered &&
            observation.trigger_status !=
                PartialCreditClaimTriggerStatus::Triggered) ||
        (!triggered &&
            observation.trigger_status !=
                PartialCreditClaimTriggerStatus::NotTriggered)) {
        throw std::invalid_argument(
            "resolved cohort trigger status contradicts the verified full path");
    }

    result.resolved_path_exact = true;
    result.mechanical_amount_bounds_available = true;
    result.empirical_realized_cash_admissible = false;
    result.contractual_face_million =
        claim_ledger_known(contractual_face);
    result.opening_principal_million =
        claim_ledger_known(opening_principal);
    result.funded_principal_created_million =
        claim_ledger_known(funded_principal);
    result.capitalized_principal_million =
        claim_ledger_known(capitalized_principal);
    result.principal_rollforward_basis_million =
        claim_ledger_known(principal_at_risk);
    result.peak_ead_million = claim_ledger_known(peak_ead);
    result.borrower_principal_cash_million =
        claim_ledger_known(borrower_principal);
    result.recovery_principal_cash_million =
        claim_ledger_known(recovery_principal);
    result.pre_support_principal_shortfall_million =
        claim_ledger_known(pre_support_shortfall);
    result.provider_claim_generated_million =
        claim_ledger_known(provider_claim);
    result.provider_claim_payable_million =
        claim_ledger_known(provider_payable);
    result.provider_principal_cash_million =
        claim_ledger_known(provider_cash);
    result.provider_unpaid_payable_claim_million =
        claim_ledger_known(provider_unpaid);
    result.provider_claim_payable_after_horizon_million =
        claim_ledger_known(provider_after_horizon);
    result.conversion_principal_extinguishment_million =
        claim_ledger_known(conversion_principal);
    result.final_principal_writeoff_million =
        claim_ledger_known(writeoff);
    return result;
}

[[nodiscard]] PartialCreditClaimLossObservationResult
evaluate_open_observation(
    const PartialCreditClaimLossObservation& observation,
    const ClaimLedgerPackage& package) {
    PartialCreditClaimLossObservationResult result;
    result.observation_id = observation.observation_id;
    result.economic_cluster_id = observation.economic_cluster_id;
    result.disposition = observation.disposition;
    result.trigger_status = observation.trigger_status;
    result.package_id = package.config.package_id;
    result.claim_id = package.config.claim_id;
    result.claim_config_sha256 = package.claim_config_sha256;
    result.provider_claim_id = observation.provider_claim_id;
    result.synthetic_package =
        package.config.package_status ==
        ClaimLedgerPackageStatus::SyntheticComplete;
    result.claim_package_has_blockers = !package.blockers.empty();
    result.empirical_realized_cash_admissible = false;
    result.peak_ead_million = claim_ledger_unknown();

    const ClaimLedgerConfig* core = decision_config(package);
    if (core == nullptr) {
        throw std::invalid_argument(
            "open cohort observation requires a verified decision-time Claim Ledger configuration");
    }
    const ClaimLedgerProviderClaim* selected_term = nullptr;
    if (observation.provider_claim_id == "NONE") {
        if (!core->provider_claims.empty()) {
            throw std::invalid_argument(
                "open cohort row omits an applicable ex-ante provider claim");
        }
    } else {
        if (core->provider_claims.size() != 1U) {
            throw std::invalid_argument(
                "partial-credit cohort v0.1 requires exactly one ex-ante provider term");
        }
        selected_term = provider_term(
            package, observation.provider_claim_id);
        if (selected_term == nullptr) {
            throw std::invalid_argument(
                "open cohort row names a provider claim that does not match the unique ex-ante term");
        }
        require_provider_structure(*selected_term);
    }
    std::optional<double> contractual_face;
    try {
        contractual_face = exact_nonnegative(
            core->contractual_face_amount_million,
            "open claim decision-time contractual face");
    } catch (const std::invalid_argument& error) {
        result.blockers.push_back(error.what());
        result.contractual_face_million = claim_ledger_unknown();
    }
    if (contractual_face.has_value()) {
        result.contractual_face_million =
            claim_ledger_known(*contractual_face);
    }
    if (observation.trigger_status ==
            PartialCreditClaimTriggerStatus::Triggered &&
        contractual_face.has_value() && *contractual_face == 0.0) {
        throw std::invalid_argument(
            "triggered open claim cannot have zero contractual face under the v0.1 shortfall definition");
    }
    result.opening_principal_million = claim_ledger_unknown();
    result.funded_principal_created_million = claim_ledger_unknown();
    result.capitalized_principal_million = claim_ledger_unknown();
    result.principal_rollforward_basis_million = claim_ledger_unknown();
    if (contractual_face.has_value() && *contractual_face == 0.0) {
        result.pre_support_principal_shortfall_million =
            claim_ledger_known(0.0);
        result.final_principal_writeoff_million =
            claim_ledger_known(0.0);
    } else {
        result.pre_support_principal_shortfall_million =
            claim_ledger_unknown();
        result.final_principal_writeoff_million =
            claim_ledger_unknown();
        if (contractual_face.has_value()) {
            result.blockers.push_back(
                "contractual face caps peak principal, not cumulative lifetime shortfall or writeoff; open loss amounts require a separate verified lifetime cap");
        }
    }
    result.borrower_principal_cash_million = claim_ledger_unknown();
    result.recovery_principal_cash_million = claim_ledger_unknown();
    result.conversion_principal_extinguishment_million =
        claim_ledger_unknown();

    if (observation.provider_claim_id == "NONE") {
        result.provider_claim_generated_million = claim_ledger_known(0.0);
        result.provider_claim_payable_million = claim_ledger_known(0.0);
        result.provider_principal_cash_million = claim_ledger_known(0.0);
        result.provider_unpaid_payable_claim_million =
            claim_ledger_known(0.0);
        result.provider_claim_payable_after_horizon_million =
            claim_ledger_known(0.0);
        result.mechanical_amount_bounds_available =
            contractual_face.has_value() && *contractual_face == 0.0;
        return result;
    }

    double upper = 0.0;
    if (!(contractual_face.has_value() && *contractual_face == 0.0)) {
        try {
            upper = exact_provider_outer_cash_cap(*selected_term);
        } catch (const std::invalid_argument& error) {
            result.blockers.push_back(error.what());
            result.provider_claim_generated_million = claim_ledger_unknown();
            result.provider_claim_payable_million = claim_ledger_unknown();
            result.provider_principal_cash_million = claim_ledger_unknown();
            result.provider_unpaid_payable_claim_million =
                claim_ledger_unknown();
            result.provider_claim_payable_after_horizon_million =
                claim_ledger_unknown();
            return result;
        }
    }
    result.provider_claim_generated_million =
        known_or_bounded(0.0, upper);
    result.provider_claim_payable_million =
        known_or_bounded(0.0, upper);
    result.provider_principal_cash_million =
        known_or_bounded(0.0, upper);
    result.provider_unpaid_payable_claim_million =
        known_or_bounded(0.0, upper);
    result.provider_claim_payable_after_horizon_million =
        known_or_bounded(0.0, upper);
    result.mechanical_amount_bounds_available =
        contractual_face.has_value() && *contractual_face == 0.0;
    result.blockers.push_back(
        "open provider amounts are coarse ex-ante contractual outer envelopes, not observed-to-date or empirical payout amounts");
    return result;
}

[[nodiscard]] PartialCreditClaimLossObservationResult
excluded_observation_result(
    const PartialCreditClaimLossObservation& observation) {
    PartialCreditClaimLossObservationResult result;
    result.observation_id = observation.observation_id;
    result.economic_cluster_id = observation.economic_cluster_id;
    result.disposition = observation.disposition;
    result.trigger_status = observation.trigger_status;
    result.contractual_face_million = claim_ledger_not_applicable();
    result.opening_principal_million = claim_ledger_not_applicable();
    result.funded_principal_created_million =
        claim_ledger_not_applicable();
    result.capitalized_principal_million = claim_ledger_not_applicable();
    result.principal_rollforward_basis_million =
        claim_ledger_not_applicable();
    result.peak_ead_million = claim_ledger_not_applicable();
    result.borrower_principal_cash_million =
        claim_ledger_not_applicable();
    result.recovery_principal_cash_million =
        claim_ledger_not_applicable();
    result.pre_support_principal_shortfall_million =
        claim_ledger_not_applicable();
    result.provider_claim_generated_million =
        claim_ledger_not_applicable();
    result.provider_claim_payable_million =
        claim_ledger_not_applicable();
    result.provider_principal_cash_million =
        claim_ledger_not_applicable();
    result.provider_unpaid_payable_claim_million =
        claim_ledger_not_applicable();
    result.provider_claim_payable_after_horizon_million =
        claim_ledger_not_applicable();
    result.conversion_principal_extinguishment_million =
        claim_ledger_not_applicable();
    result.final_principal_writeoff_million =
        claim_ledger_not_applicable();
    return result;
}

void validate_observation_metadata(
    const PartialCreditClaimLossCohortConfig& config,
    const PartialCreditClaimLossObservation& observation,
    const std::unordered_map<std::string,
        const PartialCreditClaimLossExclusionRule*>& exclusion_rules) {
    if (!valid_disposition(observation.disposition) ||
        !valid_trigger_status(observation.trigger_status)) {
        throw std::invalid_argument(
            "cohort observation contains an invalid disposition or trigger enum");
    }
    require_safe_identifier(
        observation.observation_id, "observation_id");
    require_safe_identifier(
        observation.economic_cluster_id, "economic_cluster_id");
    require_date(observation.eligible_date, "eligible_date");
    require_date(observation.horizon_end_date, "horizon_end_date");
    if (observation.eligible_date < config.frame_start_date ||
        observation.eligible_date > config.frame_end_date ||
        observation.horizon_end_date < observation.eligible_date) {
        throw std::invalid_argument(
            "cohort observation dates contradict the frozen population frame");
    }
    require_optional_date(
        observation.trigger_date, config.as_of_date, "trigger_date");
    require_optional_date(observation.classification_date,
        config.as_of_date, "classification_date");
    require_optional_date(
        observation.resolution_date, config.as_of_date, "resolution_date");
    if ((observation.trigger_date != "NONE" &&
            observation.trigger_date < observation.eligible_date) ||
        (observation.classification_date != "NONE" &&
            observation.classification_date < observation.eligible_date) ||
        (observation.resolution_date != "NONE" &&
            observation.resolution_date < observation.eligible_date) ||
        (observation.trigger_date != "NONE" &&
            observation.classification_date != "NONE" &&
            observation.classification_date < observation.trigger_date) ||
        (observation.trigger_date != "NONE" &&
            observation.resolution_date != "NONE" &&
            observation.resolution_date < observation.trigger_date) ||
        (observation.resolution_date != "NONE" &&
            observation.classification_date != "NONE" &&
            observation.classification_date < observation.resolution_date)) {
        throw std::invalid_argument(
            "cohort observation outcome dates are not chronologically ordered");
    }
    require_sorted_unique_ids(observation.population_evidence_record_ids,
        "population evidence IDs", false);
    require_sorted_unique_ids(observation.population_requirement_ids,
        "population requirement IDs", false);
    require_sorted_unique_ids(
        observation.classification_evidence_record_ids,
        "classification evidence IDs", false);
    require_sorted_unique_ids(
        observation.classification_requirement_ids,
        "classification requirement IDs", false);

    if (observation.trigger_status ==
            PartialCreditClaimTriggerStatus::Triggered) {
        if (observation.trigger_date == "NONE") {
            throw std::invalid_argument(
                "triggered cohort observation requires a trigger date");
        }
    } else if (observation.trigger_date != "NONE") {
        throw std::invalid_argument(
            "non-triggered cohort observation cannot carry a trigger date");
    }

    if (observation.disposition ==
            PartialCreditClaimLossDisposition::Excluded) {
        if (observation.exclusion_rule_id == "NONE" ||
            observation.claim_package.has_value() ||
            observation.expected_claim_config_sha256 != "NONE" ||
            observation.realized_scenario_id != "NONE" ||
            observation.provider_claim_id != "NONE" ||
            observation.resolution_date != "NONE" ||
            observation.trigger_status !=
                PartialCreditClaimTriggerStatus::NotApplicable) {
            throw std::invalid_argument(
                "excluded cohort row has incompatible claim or outcome fields");
        }
        const auto rule = exclusion_rules.find(
            observation.exclusion_rule_id);
        if (rule == exclusion_rules.end() ||
            !rule->second->outcome_blind_asserted ||
            rule->second->frozen_date > observation.eligible_date) {
            throw std::invalid_argument(
                "excluded cohort row requires a timely outcome-blind rule");
        }
        return;
    }

    if (observation.exclusion_rule_id != "NONE" ||
        !observation.claim_package.has_value() ||
        !lower_hex_sha256(observation.expected_claim_config_sha256)) {
        throw std::invalid_argument(
            "included cohort row requires one bound claim and no exclusion rule");
    }
    require_safe_identifier(
        observation.provider_claim_id, "provider_claim_id");
    if (observation.trigger_status ==
        PartialCreditClaimTriggerStatus::NotApplicable) {
        throw std::invalid_argument(
            "included cohort row cannot use a not-applicable trigger status");
    }

    if (observation.disposition ==
            PartialCreditClaimLossDisposition::Resolved) {
        if (observation.realized_scenario_id == "NONE" ||
            observation.classification_date == "NONE" ||
            observation.resolution_date == "NONE" ||
            observation.trigger_status ==
                PartialCreditClaimTriggerStatus::Unknown ||
            observation.trigger_status ==
                PartialCreditClaimTriggerStatus::NotApplicable) {
            throw std::invalid_argument(
                "resolved cohort row requires final classification, resolution and scenario");
        }
        require_safe_identifier(
            observation.realized_scenario_id, "realized_scenario_id");
        return;
    }

    if (observation.realized_scenario_id != "NONE" ||
        observation.resolution_date != "NONE") {
        throw std::invalid_argument(
            "open cohort row cannot select a final resolved scenario");
    }
    if (observation.disposition ==
            PartialCreditClaimLossDisposition::NotYetMatured &&
        observation.horizon_end_date <= config.as_of_date) {
        throw std::invalid_argument(
            "not-yet-matured row must end after the cohort as-of date");
    }
    if (observation.disposition ==
            PartialCreditClaimLossDisposition::NotYetMatured &&
        observation.trigger_status ==
            PartialCreditClaimTriggerStatus::Triggered) {
        throw std::invalid_argument(
            "a triggered open row must be unresolved rather than not-yet-matured");
    }
    if (observation.disposition ==
            PartialCreditClaimLossDisposition::Unresolved &&
        observation.horizon_end_date > config.as_of_date &&
        observation.trigger_status !=
            PartialCreditClaimTriggerStatus::Triggered) {
        throw std::invalid_argument(
            "unresolved pre-horizon row must be a triggered open case");
    }
}

} // namespace

std::string_view to_string(
    PartialCreditClaimLossDisposition value) noexcept {
    switch (value) {
    case PartialCreditClaimLossDisposition::Resolved:
        return "resolved";
    case PartialCreditClaimLossDisposition::NotYetMatured:
        return "not-yet-matured";
    case PartialCreditClaimLossDisposition::Unresolved:
        return "unresolved";
    case PartialCreditClaimLossDisposition::Excluded:
        return "excluded";
    }
    return "unknown";
}

std::string_view to_string(
    PartialCreditClaimTriggerStatus value) noexcept {
    switch (value) {
    case PartialCreditClaimTriggerStatus::Triggered:
        return "triggered";
    case PartialCreditClaimTriggerStatus::NotTriggered:
        return "not-triggered";
    case PartialCreditClaimTriggerStatus::Unknown:
        return "unknown";
    case PartialCreditClaimTriggerStatus::NotApplicable:
        return "not-applicable";
    }
    return "unknown";
}

void validate_partial_credit_claim_loss_cohort_config(
    const PartialCreditClaimLossCohortConfig& config) {
    if (config.version != kPartialCreditClaimLossCohortVersion) {
        throw std::invalid_argument(
            "unsupported partial-credit cohort version");
    }
    require_safe_identifier(config.cohort_id, "cohort_id");
    require_date(config.as_of_date, "cohort as_of_date");
    require_date(config.frame_start_date, "cohort frame_start_date");
    require_date(config.frame_end_date, "cohort frame_end_date");
    if (config.frame_start_date > config.frame_end_date ||
        config.frame_end_date > config.as_of_date) {
        throw std::invalid_argument(
            "cohort frame dates must end no later than the as-of date");
    }
    require_safe_text(config.source_note, "cohort source_note");
    for (const auto* definition : {
             &config.population_definition,
             &config.sampling_unit_definition,
             &config.economic_cluster_definition,
             &config.protection_term_stratum_definition,
             &config.outcome_horizon_definition,
             &config.loss_definition,
             &config.resolution_definition,
             &config.censoring_definition,
             &config.denominator_definition}) {
        require_safe_identifier(*definition, "cohort method definition");
    }
    require_safe_identifier(config.currency_label, "cohort currency_label");
    require_safe_text(config.monetary_basis, "cohort monetary_basis");
    if (!config.candidate_only) {
        throw std::invalid_argument(
            "partial-credit cohort v0.1 must remain candidate_only");
    }
    if (config.population_frame_count >
        kMaximumPartialCreditCohortObservations) {
        throw std::invalid_argument(
            "partial-credit cohort population exceeds the resource guardrail");
    }
    if (config.exclusion_rules.size() >
        kMaximumPartialCreditCohortExclusionRules) {
        throw std::invalid_argument(
            "partial-credit cohort exclusion rules exceed the resource guardrail");
    }
    std::unordered_set<std::string> rule_ids;
    rule_ids.reserve(config.exclusion_rules.size());
    for (const PartialCreditClaimLossExclusionRule& rule :
         config.exclusion_rules) {
        require_safe_identifier(rule.id, "exclusion rule id");
        require_date(rule.frozen_date, "exclusion rule frozen_date");
        require_safe_text(rule.statement, "exclusion rule statement");
        if (!rule_ids.emplace(rule.id).second) {
            throw std::invalid_argument(
                "partial-credit cohort exclusion rule IDs must be unique");
        }
    }
}

PartialCreditClaimLossCohortEvaluation
evaluate_partial_credit_claim_loss_cohort(
    const PartialCreditClaimLossCohortPackage& package) {
    validate_partial_credit_claim_loss_cohort_config(package.config);
    if (package.observations.size() !=
            package.config.population_frame_count ||
        package.observations.size() >
            kMaximumPartialCreditCohortObservations) {
        throw std::invalid_argument(
            "partial-credit cohort row count must equal the frozen population frame");
    }

    std::unordered_map<std::string,
        const PartialCreditClaimLossExclusionRule*> exclusion_rules;
    exclusion_rules.reserve(package.config.exclusion_rules.size());
    for (const PartialCreditClaimLossExclusionRule& rule :
         package.config.exclusion_rules) {
        exclusion_rules.emplace(rule.id, &rule);
    }

    std::unordered_set<std::string> observation_ids;
    std::unordered_set<std::string> frame_clusters;
    observation_ids.reserve(package.observations.size());
    frame_clusters.reserve(package.observations.size());
    std::size_t retained_evidence_id_count = 0U;
    for (const PartialCreditClaimLossObservation& observation :
         package.observations) {
        for (const std::size_t count : {
                 observation.population_evidence_record_ids.size(),
                 observation.population_requirement_ids.size(),
                 observation.classification_evidence_record_ids.size(),
                 observation.classification_requirement_ids.size()}) {
            if (count >
                kMaximumPartialCreditCohortRetainedEvidenceIds -
                    retained_evidence_id_count) {
                throw std::invalid_argument(
                    "partial-credit cohort retained evidence IDs exceed the aggregate resource guardrail");
            }
            retained_evidence_id_count += count;
        }
        validate_observation_metadata(
            package.config, observation, exclusion_rules);
        if (!observation_ids.emplace(observation.observation_id).second) {
            throw std::invalid_argument(
                "partial-credit cohort observation IDs must be unique");
        }
        if (!frame_clusters.emplace(
                observation.economic_cluster_id).second) {
            throw std::invalid_argument(
                "partial-credit cohort requires one consolidated unique economic cluster per frame row");
        }
    }

    std::vector<const PartialCreditClaimLossObservation*> ordered;
    ordered.reserve(package.observations.size());
    for (const PartialCreditClaimLossObservation& observation :
         package.observations) {
        ordered.push_back(&observation);
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const auto* first, const auto* second) {
            return first->observation_id < second->observation_id;
        });

    PartialCreditClaimLossCohortEvaluation result;
    result.candidate_only = true;
    result.calibrated_execution_authorized = false;
    result.portfolio_export_authorized = false;
    result.empirical_realized_cash_admissible = false;
    result.frame_cluster_ids_unique = true;
    result.frame_count = ordered.size();
    result.observations.reserve(ordered.size());
    std::unordered_set<std::string> included_package_ids;
    std::unordered_set<std::string> included_claim_ids;
    std::unordered_set<std::string> included_package_roots;
    included_package_ids.reserve(ordered.size());
    included_claim_ids.reserve(ordered.size());
    included_package_roots.reserve(ordered.size());

    for (const PartialCreditClaimLossObservation* observation : ordered) {
        if (observation->disposition ==
                PartialCreditClaimLossDisposition::Excluded) {
            ++result.excluded_count;
            result.observations.push_back(
                excluded_observation_result(*observation));
            continue;
        }

        ReverifiedClaimPackage reverified =
            reverify_package(*observation);
        ClaimLedgerPackage& verified = reverified.package;
        if (!included_package_ids.emplace(
                verified.config.package_id).second ||
            !included_claim_ids.emplace(verified.config.claim_id).second ||
            !included_package_roots.emplace(
                verified.claim_config_sha256).second) {
            throw std::invalid_argument(
                "partial-credit cohort included claim packages and roots must be unique");
        }
        if (verified.config.currency_label !=
                package.config.currency_label ||
            verified.config.monetary_basis !=
                package.config.monetary_basis) {
            throw std::invalid_argument(
                "cohort claim currency or monetary basis does not match the frozen stratum");
        }

        PartialCreditClaimLossObservationResult evaluated;
        if (observation->disposition ==
                PartialCreditClaimLossDisposition::Resolved) {
            ++result.resolved_count;
            if (!reverified.full_path_evidence.has_value()) {
                throw std::logic_error(
                    "resolved cohort re-verification omitted selected-path provenance");
            }
            evaluated = evaluate_resolved_observation(
                *observation, verified,
                *reverified.full_path_evidence);
        } else {
            if (observation->disposition ==
                    PartialCreditClaimLossDisposition::NotYetMatured) {
                ++result.not_yet_matured_count;
            } else {
                ++result.unresolved_count;
            }
            evaluated = evaluate_open_observation(
                *observation, verified);
        }
        result.synthetic_package_present =
            result.synthetic_package_present || evaluated.synthetic_package;
        result.claim_ledger_package_blockers_present =
            result.claim_ledger_package_blockers_present ||
            evaluated.claim_package_has_blockers;
        for (const std::string& blocker : verified.blockers) {
            result.blockers.push_back(evaluated.observation_id +
                ": verified Claim Ledger blocker: " + blocker);
        }
        for (const std::string& blocker : evaluated.blockers) {
            result.blockers.push_back(
                evaluated.observation_id + ": " + blocker);
        }
        result.observations.push_back(std::move(evaluated));
    }

    result.included_count = result.resolved_count +
        result.not_yet_matured_count + result.unresolved_count;
    result.censored_count =
        result.not_yet_matured_count + result.unresolved_count;
    result.all_included_resolved =
        result.included_count == result.resolved_count;
    if (result.frame_count !=
            result.included_count + result.excluded_count) {
        throw std::logic_error(
            "partial-credit cohort disposition counts do not reconcile");
    }

    std::size_t pre_support_lower_count = 0U;
    std::size_t pre_support_upper_count = 0U;
    std::size_t provider_cash_lower_count = 0U;
    std::size_t provider_cash_upper_count = 0U;
    std::size_t writeoff_lower_count = 0U;
    std::size_t writeoff_upper_count = 0U;
    for (const PartialCreditClaimLossObservationResult& row :
         result.observations) {
        if (row.disposition ==
            PartialCreditClaimLossDisposition::Excluded) {
            continue;
        }
        if (row.trigger_status ==
                PartialCreditClaimTriggerStatus::Triggered ||
            row.trigger_status ==
                PartialCreditClaimTriggerStatus::NotTriggered) {
            ++result.trigger_known_count;
        }
        if (row.trigger_status ==
            PartialCreditClaimTriggerStatus::Triggered) {
            ++result.triggered_count;
        }
        if (row.trigger_status ==
            PartialCreditClaimTriggerStatus::Unknown) {
            ++result.trigger_unknown_count;
        }

        if (row.resolved_path_exact) {
            const double shortfall = exact_nonnegative(
                row.pre_support_principal_shortfall_million,
                "resolved pre-support shortfall");
            const double claim = exact_nonnegative(
                row.provider_claim_generated_million,
                "resolved provider claim generated");
            const double provider_cash = exact_nonnegative(
                row.provider_principal_cash_million,
                "resolved provider principal cash");
            const double provider_unpaid = exact_nonnegative(
                row.provider_unpaid_payable_claim_million,
                "resolved provider unpaid claim");
            const double writeoff = exact_nonnegative(
                row.final_principal_writeoff_million,
                "resolved final principal writeoff");
            if (shortfall > 0.0) {
                ++pre_support_lower_count;
                ++pre_support_upper_count;
            }
            if (claim > 0.0) {
                ++result.resolved_provider_claim_generated_count;
            }
            if (provider_cash > 0.0) {
                ++result.resolved_provider_claim_paid_count;
                ++provider_cash_lower_count;
                ++provider_cash_upper_count;
            }
            if (provider_unpaid > 0.0) {
                ++result.provider_unpaid_claim_known_positive_count;
                ++result.provider_unpaid_claim_possible_positive_count;
            }
            if (writeoff > 0.0) {
                ++writeoff_lower_count;
                ++writeoff_upper_count;
            }
        } else {
            if (row.trigger_status ==
                    PartialCreditClaimTriggerStatus::Triggered) {
                ++pre_support_lower_count;
            }
            if (row.trigger_status ==
                    PartialCreditClaimTriggerStatus::Triggered ||
                possibly_positive(
                    row.pre_support_principal_shortfall_million, true)) {
                ++pre_support_upper_count;
            }
            if (possibly_positive(row.provider_principal_cash_million,
                    row.provider_claim_id != "NONE")) {
                ++provider_cash_upper_count;
            }
            if (possibly_positive(
                    row.final_principal_writeoff_million, true)) {
                ++writeoff_upper_count;
            }
        }
        if (!row.resolved_path_exact) {
            if (known_positive(
                    row.provider_unpaid_payable_claim_million)) {
                ++result.provider_unpaid_claim_known_positive_count;
                ++result.provider_unpaid_claim_possible_positive_count;
            } else if (possibly_positive(
                           row.provider_unpaid_payable_claim_million,
                           row.provider_claim_id != "NONE")) {
                ++result.provider_unpaid_claim_possible_positive_count;
            }
        }
    }

    if (result.trigger_known_count + result.trigger_unknown_count !=
            result.included_count ||
        result.triggered_count > result.trigger_known_count) {
        throw std::logic_error(
            "partial-credit cohort trigger counts do not reconcile to the included denominator");
    }

    result.total_contractual_face_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            contractual_face_million);
    result.total_resolved_opening_principal_million = sum_resolved_known(
        result.observations,
        &PartialCreditClaimLossObservationResult::opening_principal_million);
    result.total_resolved_funded_principal_created_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                funded_principal_created_million);
    result.total_resolved_capitalized_principal_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                capitalized_principal_million);
    result.total_resolved_principal_rollforward_basis_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                principal_rollforward_basis_million);
    result.sum_resolved_claim_peak_ead_million = sum_resolved_known(
        result.observations,
        &PartialCreditClaimLossObservationResult::peak_ead_million);
    result.total_resolved_borrower_principal_cash_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                borrower_principal_cash_million);
    result.total_resolved_recovery_principal_cash_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                recovery_principal_cash_million);
    result.total_resolved_provider_principal_cash_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                provider_principal_cash_million);
    result.total_resolved_conversion_principal_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                conversion_principal_extinguishment_million);
    result.total_resolved_final_principal_writeoff_million =
        sum_resolved_known(result.observations,
            &PartialCreditClaimLossObservationResult::
                final_principal_writeoff_million);
    const double resolved_basis = exact_nonnegative(
        result.total_resolved_principal_rollforward_basis_million,
        "aggregate resolved principal roll-forward basis");
    const double resolved_uses = checked_double(
        static_cast<long double>(exact_nonnegative(
            result.total_resolved_borrower_principal_cash_million,
            "aggregate resolved borrower principal cash")) +
        static_cast<long double>(exact_nonnegative(
            result.total_resolved_recovery_principal_cash_million,
            "aggregate resolved recovery principal cash")) +
        static_cast<long double>(exact_nonnegative(
            result.total_resolved_provider_principal_cash_million,
            "aggregate resolved provider principal cash")) +
        static_cast<long double>(exact_nonnegative(
            result.total_resolved_conversion_principal_million,
            "aggregate resolved conversion principal")) +
        static_cast<long double>(exact_nonnegative(
            result.total_resolved_final_principal_writeoff_million,
            "aggregate resolved final principal writeoff")));
    if (!nearly_equal(resolved_basis, resolved_uses)) {
        throw std::logic_error(
            "aggregate resolved cohort principal roll-forward does not reconcile");
    }
    result.resolved_principal_conservation_reconciled = true;
    result.total_pre_support_principal_shortfall_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            pre_support_principal_shortfall_million);
    result.total_provider_claim_generated_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            provider_claim_generated_million);
    result.total_provider_claim_payable_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            provider_claim_payable_million);
    result.total_provider_principal_cash_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            provider_principal_cash_million);
    result.total_provider_unpaid_payable_claim_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            provider_unpaid_payable_claim_million);
    result.total_provider_claim_payable_after_horizon_million =
        sum_ranges(result.observations,
            &PartialCreditClaimLossObservationResult::
                provider_claim_payable_after_horizon_million);
    result.total_final_principal_writeoff_million = sum_ranges(
        result.observations,
        &PartialCreditClaimLossObservationResult::
            final_principal_writeoff_million);
    result.positive_pre_support_shortfall_frequency = frequency_range(
        pre_support_lower_count, pre_support_upper_count,
        result.included_count);
    result.positive_provider_cash_frequency = frequency_range(
        provider_cash_lower_count, provider_cash_upper_count,
        result.included_count);
    result.positive_final_writeoff_frequency = frequency_range(
        writeoff_lower_count, writeoff_upper_count,
        result.included_count);
    result.provider_cash_to_contractual_face =
        ratio_to_exact_denominator(
            result.total_provider_principal_cash_million,
            result.total_contractual_face_million);
    result.final_writeoff_to_contractual_face =
        ratio_to_exact_denominator(
            result.total_final_principal_writeoff_million,
            result.total_contractual_face_million);
    result.mechanical_amount_ranges_available =
        amount_available(result.total_contractual_face_million) &&
        amount_available(
            result.total_pre_support_principal_shortfall_million) &&
        amount_available(result.total_provider_claim_generated_million) &&
        amount_available(result.total_provider_claim_payable_million) &&
        amount_available(result.total_provider_principal_cash_million) &&
        amount_available(
            result.total_provider_unpaid_payable_claim_million) &&
        amount_available(
            result.total_provider_claim_payable_after_horizon_million) &&
        amount_available(result.total_final_principal_writeoff_million);

    result.blockers.push_back(
        "field-level controlled outcome evidence and the bound population/method package are not yet implemented");
    result.blockers.push_back(
        "open amount endpoints are metric-specific outer envelopes and do not form one jointly feasible cash, unpaid-claim and writeoff state");
    result.blockers.push_back(
        "calibrated execution, Portfolio export, pricing and expected-return use are prohibited");
    return result;
}

} // namespace naturalehia::cellular_finance
