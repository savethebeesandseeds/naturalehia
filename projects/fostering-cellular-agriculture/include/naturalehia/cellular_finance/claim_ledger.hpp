// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kClaimLedgerModelVersion{"0.1.0"};
inline constexpr std::size_t kClaimLedgerMaximumPeriods{1'200U};
inline constexpr std::size_t kClaimLedgerMaximumScenarios{4'096U};
inline constexpr std::size_t kClaimLedgerMaximumEntries{1'000'000U};
inline constexpr std::size_t kClaimLedgerMaximumProviderClaims{4'096U};
inline constexpr std::size_t kClaimLedgerMaximumRetainedPathPeriodCells{
    50'000U};
inline constexpr std::size_t kClaimLedgerMaximumProviderPeriodCells{25'000U};
inline constexpr std::size_t kClaimLedgerMaximumPathEntryVisits{2'000'000U};
inline constexpr std::size_t kClaimLedgerMaximumCovenantEvents{100'000U};

// Unknown and not-applicable values have no numeric payload. Known values use
// equal lower and upper endpoints. Bounded values retain both endpoints and
// are never silently collapsed to a point estimate.
enum class ClaimLedgerValueStatus {
    Known,
    Bounded,
    Unknown,
    NotApplicable,
};

struct ClaimLedgerValue {
    ClaimLedgerValueStatus status{ClaimLedgerValueStatus::Unknown};
    std::optional<double> lower{};
    std::optional<double> upper{};
};

[[nodiscard]] ClaimLedgerValue claim_ledger_known(double value);
[[nodiscard]] ClaimLedgerValue claim_ledger_bounded(
    double lower, double upper);
[[nodiscard]] ClaimLedgerValue claim_ledger_unknown() noexcept;
[[nodiscard]] ClaimLedgerValue claim_ledger_not_applicable() noexcept;

enum class ClaimLedgerEntryKind {
    BuyerPrice,
    BuyerDirectCost,
    BorrowerGrossProceeds,
    BorrowerNetProceeds,
    CashFee,
    BorrowerThirdPartyCost,
    FundedPrincipal,
    OriginalIssueDiscount,
    OriginalIssuePremium,
    CapitalizedFee,
    InterestAccrual,
    CapitalizedInterest,
    PrincipalDue,
    InterestDue,
    PrincipalCash,
    InterestCash,
    RecoveryPrincipalCash,
    RecoveryInterestCash,
    ConversionPrincipalExtinguishment,
    ConversionInterestExtinguishment,
    ConversionUnits,
    PrincipalWriteoff,
    AccruedInterestWriteoff,
    GuaranteePrincipalCash,
    GuaranteeInterestCash,
};

enum class ClaimLedgerCovenantState {
    Pass,
    Breach,
    BreachWithCure,
    BreachWithWaiver,
    BreachWithNonExerciseConsent,
    Default,
    Acceleration,
};

enum class ClaimLedgerProviderAllocationPriority {
    PrincipalFirst,
    InterestFirst,
    ProRata,
};

enum class ClaimLedgerCashPathStatus {
    Incomplete,
    CompleteResolved,
};

[[nodiscard]] std::string_view to_string(
    ClaimLedgerValueStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerEntryKind kind) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerCovenantState state) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerProviderAllocationPriority priority) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerCashPathStatus status) noexcept;

// A line is one version of an economic fact. At any information cut, the
// latest known version of each economic_fact_id is used; equal-known-at
// competing versions are invalid. event_group_id joins the selected lines of
// one funding or conversion event. source_record_id preserves provenance.
// provider_claim_id is required only for guarantee-cash lines.
struct ClaimLedgerEntry {
    std::string entry_id{"unnamed-entry"};
    std::string economic_fact_id{"unnamed-fact"};
    std::string event_group_id{"none"};
    ClaimLedgerEntryKind kind{ClaimLedgerEntryKind::BuyerPrice};
    std::size_t period{0U};
    std::size_t known_at_period{0U};
    ClaimLedgerValue value{};
    std::string source_record_id{"none"};
    std::string provider_claim_id{"none"};
};

struct ClaimLedgerCovenantEvent {
    std::string event_id{"unnamed-covenant-event"};
    std::string covenant_id{"unnamed-covenant"};
    std::size_t period{0U};
    std::size_t known_at_period{0U};
    ClaimLedgerCovenantState state{ClaimLedgerCovenantState::Pass};
    std::string source_record_id{"none"};
};

// shortfall_allocation_fraction prevents two providers from being assumed to
// cover the same dollar. V0.1 computes multiple providers only when their
// exact ex-ante allocations sum to no more than one within each covered
// obligation category. V0.1 applies the
// deductible after allocation to cumulative lifetime due-date shortfall,
// then applies coverage and the lifetime cap. V0.1 does not guess how a late
// borrower cure, writeoff, conversion, cancellation, or subrogation changes an
// earlier allocated shortfall, even before the deductible has generated a
// provider claim: that interaction is explicitly uncomputable.
// coverage_and_priority_evidenced
// attests the stated lifetime aggregation and obligation-priority terms.
// Contracts using a per-occurrence deductible or paths requiring late-cure
// allocation need a later schema and are not ready claims in v0.1.
struct ClaimLedgerProviderClaim {
    std::string provider_claim_id{"unnamed-provider-claim"};
    std::string provider_id{"unnamed-provider"};
    std::size_t known_at_period{0U};
    ClaimLedgerValue shortfall_allocation_fraction{};
    ClaimLedgerValue coverage_fraction{};
    ClaimLedgerValue deductible_million{};
    ClaimLedgerValue maximum_cash_million{};
    ClaimLedgerValue settlement_lag_periods{};
    bool covers_principal_due{true};
    bool covers_interest_due{true};
    bool payment_right_evidenced{false};
    bool provider_identity_evidenced{false};
    bool coverage_and_priority_evidenced{false};
    std::optional<ClaimLedgerProviderAllocationPriority>
        obligation_priority{};
    std::string source_record_id{"none"};
};

struct ClaimLedgerScenario {
    std::string scenario_id{"unnamed-scenario"};
    ClaimLedgerValue physical_probability{};
    std::size_t probability_known_at_period{0U};
    ClaimLedgerCashPathStatus cash_path_status{
        ClaimLedgerCashPathStatus::Incomplete};
    std::size_t cash_path_status_known_at_period{0U};
    std::string probability_source_record_id{"none"};
    std::string cash_path_status_source_record_id{"none"};
    std::vector<ClaimLedgerEntry> entries{};
    std::vector<ClaimLedgerCovenantEvent> covenant_events{};
};

struct ClaimLedgerConfig {
    std::string model_version{kClaimLedgerModelVersion};
    std::string ledger_id{"unnamed-ledger"};
    std::string project_id{"unnamed-project"};
    std::string claim_id{"unnamed-claim"};
    std::string currency_label{"UNSPECIFIED"};
    std::string monetary_basis{"unspecified monetary basis"};
    std::string period_unit_label{"month"};
    std::size_t periods_per_year{12U};
    std::string conversion_unit_label{"not-applicable"};
    std::string conversion_unit_basis{"not-applicable"};

    std::size_t decision_period{0U};
    std::size_t horizon_period{0U};
    ClaimLedgerValue contractual_face_amount_million{};
    std::size_t face_amount_known_at_period{0U};
    ClaimLedgerValue opening_principal_million{};
    std::size_t opening_principal_known_at_period{0U};
    ClaimLedgerValue opening_accrued_interest_million{};
    std::size_t opening_accrued_interest_known_at_period{0U};
    ClaimLedgerValue annual_effective_discount_rate{};
    std::optional<std::size_t> discount_rate_known_at_period{};

    std::vector<ClaimLedgerEntry> common_entries{};
    std::vector<ClaimLedgerCovenantEvent> common_covenant_events{};
    std::vector<ClaimLedgerScenario> scenarios{};
    std::vector<ClaimLedgerProviderClaim> provider_claims{};
};

struct ClaimLedgerProviderPeriodResult {
    std::size_t period{0U};
    ClaimLedgerValue allocated_principal_shortfall_million{};
    ClaimLedgerValue allocated_interest_shortfall_million{};
    ClaimLedgerValue allocated_shortfall_million{};
    ClaimLedgerValue principal_claim_generated_million{};
    ClaimLedgerValue interest_claim_generated_million{};
    ClaimLedgerValue claim_generated_million{};
    ClaimLedgerValue principal_claim_payable_million{};
    ClaimLedgerValue interest_claim_payable_million{};
    ClaimLedgerValue claim_payable_million{};
    ClaimLedgerValue guarantee_principal_cash_million{};
    ClaimLedgerValue guarantee_interest_cash_million{};
    ClaimLedgerValue guarantee_cash_million{};
    ClaimLedgerValue unpaid_principal_payable_claim_million{};
    ClaimLedgerValue unpaid_interest_payable_claim_million{};
    ClaimLedgerValue unpaid_payable_claim_million{};
};

struct ClaimLedgerProviderPathResult {
    std::string provider_claim_id{};
    bool computable{false};
    std::vector<std::string> blockers{};
    std::vector<ClaimLedgerProviderPeriodResult> periods{};
    ClaimLedgerValue total_claim_generated_million{};
    ClaimLedgerValue total_guarantee_cash_million{};
    ClaimLedgerValue terminal_unpaid_payable_claim_million{};
    ClaimLedgerValue claim_payable_after_horizon_million{};
};

struct ClaimLedgerPeriodResult {
    std::size_t period{0U};

    ClaimLedgerValue buyer_price_million{};
    ClaimLedgerValue buyer_direct_cost_million{};
    ClaimLedgerValue borrower_gross_proceeds_million{};
    ClaimLedgerValue borrower_net_proceeds_million{};
    ClaimLedgerValue cash_fee_million{};
    ClaimLedgerValue borrower_third_party_cost_million{};

    ClaimLedgerValue opening_principal_million{};
    ClaimLedgerValue funded_principal_million{};
    ClaimLedgerValue original_issue_discount_million{};
    ClaimLedgerValue original_issue_premium_million{};
    ClaimLedgerValue capitalized_fee_million{};
    ClaimLedgerValue capitalized_interest_million{};
    ClaimLedgerValue principal_due_million{};
    ClaimLedgerValue outstanding_principal_due_million{};
    ClaimLedgerValue principal_cash_million{};
    ClaimLedgerValue recovery_principal_cash_million{};
    ClaimLedgerValue guarantee_principal_cash_million{};
    ClaimLedgerValue conversion_principal_extinguishment_million{};
    ClaimLedgerValue principal_writeoff_million{};
    ClaimLedgerValue closing_principal_million{};

    ClaimLedgerValue opening_accrued_interest_million{};
    ClaimLedgerValue interest_accrual_million{};
    ClaimLedgerValue interest_due_million{};
    ClaimLedgerValue outstanding_interest_due_million{};
    ClaimLedgerValue interest_cash_million{};
    ClaimLedgerValue recovery_interest_cash_million{};
    ClaimLedgerValue guarantee_interest_cash_million{};
    ClaimLedgerValue conversion_interest_extinguishment_million{};
    ClaimLedgerValue accrued_interest_writeoff_million{};
    ClaimLedgerValue closing_accrued_interest_million{};

    ClaimLedgerValue conversion_units{};
    ClaimLedgerValue principal_shortfall_after_borrower_recovery_million{};
    ClaimLedgerValue interest_shortfall_after_borrower_recovery_million{};
    ClaimLedgerValue investor_cashflow_million{};
    ClaimLedgerValue ead_before_resolution_million{};
};

struct ClaimLedgerPathResult {
    std::vector<ClaimLedgerPeriodResult> periods{};
    std::vector<ClaimLedgerProviderPathResult> provider_claims{};
    bool exact{false};
    bool settlement_reconciled{false};
    bool rollforwards_reconciled{false};
    bool contractual_face_reconciled{false};
    std::vector<std::string> blockers{};
    ClaimLedgerValue npv_million{};
    ClaimLedgerValue principal_cash_wal_months{};
    ClaimLedgerValue peak_ead_million{};
    ClaimLedgerValue terminal_principal_million{};
    ClaimLedgerValue terminal_accrued_interest_million{};
    ClaimLedgerValue terminal_total_exposure_million{};
    ClaimLedgerValue total_conversion_units{};
    ClaimLedgerValue principal_loss_million{};
    ClaimLedgerValue accrued_interest_loss_million{};
    ClaimLedgerValue total_loss_million{};
};

struct ClaimLedgerScenarioResult {
    std::string scenario_id{};
    ClaimLedgerValue physical_probability{};
    bool probability_available_at_decision{false};
    bool complete_resolved_cash_path_at_decision{false};
    std::vector<std::string> decision_entry_ids{};
    std::vector<std::string> backtest_entry_ids{};
    std::vector<ClaimLedgerCovenantEvent> decision_covenant_events{};
    std::vector<ClaimLedgerCovenantEvent> backtest_covenant_events{};
    ClaimLedgerPathResult decision_path{};
    ClaimLedgerPathResult full_path{};
};

struct ClaimLedgerReadiness {
    bool expected_cash_ready{false};
    bool npv_ready{false};
    bool rate_preimage_ready{false};
    bool provider_claim_applicable{false};
    bool provider_claim_ready{false};
    std::vector<std::string> expected_cash_blockers{};
    std::vector<std::string> npv_blockers{};
    std::vector<std::string> rate_preimage_blockers{};
    std::vector<std::string> provider_claim_blockers{};
};

struct ClaimLedgerSummary {
    ClaimLedgerPathResult common_decision_path{};
    ClaimLedgerPathResult common_full_path{};
    std::vector<ClaimLedgerCovenantEvent>
        common_decision_covenant_events{};
    std::vector<ClaimLedgerCovenantEvent>
        common_backtest_covenant_events{};
    std::vector<ClaimLedgerScenarioResult> scenarios{};
    std::vector<ClaimLedgerValue> expected_investor_cashflows_million{};
    std::vector<ClaimLedgerValue> expected_ead_million{};
    ClaimLedgerValue expected_npv_million{};
    ClaimLedgerValue expected_principal_loss_million{};
    ClaimLedgerValue expected_accrued_interest_loss_million{};
    ClaimLedgerValue expected_total_loss_million{};
    ClaimLedgerValue expected_principal_cash_wal_months{};
    ClaimLedgerValue annual_effective_rate_preimage{};
    ClaimLedgerReadiness readiness{};
};

// Structural contradictions throw invalid_argument. Evidentiary incompleteness
// (unknown values, missing probabilities or unproved provider terms) validates
// and appears as explicit readiness blockers in the result.
void validate_claim_ledger_config(const ClaimLedgerConfig& config);

[[nodiscard]] ClaimLedgerSummary evaluate_claim_ledger(
    const ClaimLedgerConfig& config);

} // namespace naturalehia::cellular_finance
