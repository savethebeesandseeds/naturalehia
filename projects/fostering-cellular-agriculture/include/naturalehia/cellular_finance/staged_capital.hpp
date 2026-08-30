// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/model.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kStagedCapitalModelVersion{"0.1.0"};

// A certification is an explicit scenario input. The model does not infer it
// from elapsed time, construction spend, the public evidence dossier, or the
// separate evidence-readiness gate.
enum class CertificationDecision : unsigned char {
    Certified,
    FinalFailure,
};

enum class StagedCapitalOutcome : unsigned char {
    Completed,
    MilestoneFailure,
    CostToCompleteFailure,
    SponsorFundingFailure,
    ProviderFundingFailure,
};

enum class CapitalCashFlowKind : unsigned char {
    UpfrontFee,
    ProtectedReserveFunding,
    SponsorConstructionContribution,
    ProviderDraw,
    EligibleConstructionUse,
    CommitmentFee,
    CompletionOrRecoveryProceeds,
    ProviderRepayment,
    SponsorResidualDistribution,
    ProtectedWorkoutUse,
    WorkoutShortfallUseFromRecovery,
    ProtectedReserveRelease,
};

struct StagedCapitalPhaseTerms {
    std::string id{};
    std::size_t duration_months{1U};
    double provider_stage_cap_million{0.0};
};

struct StagedCapitalTerms {
    // The aggregate provider commitment must equal the sum of phase caps.
    // Undrawn capacity belonging to the current single-draw phase does not
    // roll forward after that draw decision.
    double provider_commitment_million{0.0};
    // This covers construction contributions only. Upfront and commitment
    // fees and the protected reserve are additional sponsor cash requirements.
    double sponsor_construction_commitment_million{0.0};
    double provider_cost_share{0.0};
    double annual_pik_rate{0.0};
    double claim_cap_multiple{1.0};
    // Paid monthly by the sponsor on modeled committed undrawn availability.
    double annual_commitment_fee_rate{0.0};
    // Paid by the sponsor to the provider at financial close.
    double upfront_fee_million{0.0};
    // Used only for provider physical-measure NPV sensitivity.
    double provider_hurdle_rate{0.0};
    double sponsor_discount_rate{0.0};
    // Sponsor-funded at close, segregated from ordinary project cash and
    // unavailable to the provider recovery waterfall.
    double protected_workout_reserve_million{0.0};
};

struct StagedCapitalPhaseCase {
    double actual_eligible_cost_million{0.0};
    // Independent cost-to-complete estimate at the phase draw date, including
    // the current phase. This is an input, not model-generated foresight.
    double estimated_cost_to_complete_million{0.0};
    CertificationDecision certification{CertificationDecision::Certified};
    // Separates contractual availability from provider cash performance.
    bool provider_funds{true};
};

struct StagedCapitalCase {
    std::string id{};
    // Analyst-declared synthetic physical-measure case weight. Path records
    // retain this declared value; aggregate outputs normalize the accepted
    // near-one configured sum.
    double weight{0.0};
    std::vector<StagedCapitalPhaseCase> phases{};
    // External cash proceeds available only if all reached certifications pass
    // and construction completes; this is not an enterprise-value estimate.
    double completion_value_million{0.0};
    // Gross ProjectCo recovery cash before the modeled workout-shortfall
    // diversion and excluding the protected reserve. Realization/enforcement
    // costs are not modeled and must already be netted from this input.
    double recovery_value_million{0.0};
    std::size_t recovery_delay_months{0U};
    double required_workout_cost_million{0.0};
};

struct StagedCapitalConfig {
    std::string model_version{kStagedCapitalModelVersion};
    std::string scenario_label{"unnamed synthetic staged-capital analysis"};
    std::string source_note{
        "Unvalidated synthetic assumptions for contract-mechanics testing"};
    std::string currency_label{"DEMO"};
    std::string monetary_basis{"unspecified-synthetic"};
    bool synthetic_inputs{true};
    StagedCapitalTerms terms{};
    std::vector<StagedCapitalPhaseTerms> phases{};
    std::vector<StagedCapitalCase> cases{};
};

// Positive values are receipts by the named account; negative values are
// payments. Every accepted entry must sum to zero across all five accounts.
struct CapitalCashPosting {
    double sponsor_million{0.0};
    double project_unrestricted_million{0.0};
    double provider_million{0.0};
    double protected_reserve_million{0.0};
    double external_million{0.0};
};

struct CapitalCashLedgerEntry {
    std::size_t month{0U};
    CapitalCashFlowKind kind{CapitalCashFlowKind::UpfrontFee};
    std::string reference{};
    CapitalCashPosting posting{};
};

struct StagedCapitalPhaseResult {
    std::string id{};
    std::size_t start_month{0U};
    std::size_t end_month{0U};
    bool reached{false};
    bool funded{false};
    CertificationDecision certification{CertificationDecision::Certified};
    double eligible_cost_million{0.0};
    double sponsor_contribution_required_million{0.0};
    double sponsor_contribution_million{0.0};
    double provider_draw_entitlement_million{0.0};
    double provider_draw_received_million{0.0};
    double funding_gap_million{0.0};
    double commitment_fees_million{0.0};

    // Commitment memo-account roll-forward.
    double opening_undrawn_commitment_million{0.0};
    double cancelled_availability_million{0.0};
    double closing_undrawn_commitment_million{0.0};

    // Provider funded-claim / ProjectCo obligation memo roll-forward.
    double opening_funded_claim_million{0.0};
    double contractual_return_accrued_million{0.0};
    double provider_repayment_million{0.0};
    double claim_writeoff_million{0.0};
    double closing_funded_claim_million{0.0};
};

struct StagedCapitalPathResult {
    std::string case_id{};
    double weight{0.0};
    StagedCapitalOutcome outcome{StagedCapitalOutcome::Completed};
    std::optional<std::size_t> stop_phase_index{};
    std::size_t outcome_month{0U};
    std::size_t recovery_month{0U};
    std::vector<StagedCapitalPhaseResult> phases{};
    std::vector<CapitalCashLedgerEntry> cash_ledger{};

    double cumulative_eligible_spend_million{0.0};
    double total_provider_draws_million{0.0};
    double peak_provider_funded_principal_million{0.0};
    double peak_provider_net_cash_outlay_million{0.0};
    double total_sponsor_construction_contributions_million{0.0};
    double total_commitment_fees_million{0.0};
    double provider_claim_at_exit_million{0.0};
    double provider_nominal_recovery_million{0.0};
    double provider_recovery_pv_million{0.0};
    // Cash draws not recovered by the terminal provider payment. This is a
    // cash-capital shortfall, not the writeoff of the full PIK-bearing claim.
    double provider_principal_loss_million{0.0};
    double provider_claim_writeoff_million{0.0};
    double sponsor_residual_receipt_million{0.0};
    double unused_commitment_cancelled_million{0.0};
    double stranded_spend_million{0.0};
    double funding_gap_million{0.0};
    double protected_workout_spend_million{0.0};
    double protected_reserve_release_million{0.0};
    double protected_reserve_shortfall_at_stop_million{0.0};
    double workout_shortfall_paid_from_recovery_million{0.0};
    double safety_funding_shortfall_million{0.0};

    double provider_npv_before_upfront_fee_million{0.0};
    double provider_npv_after_upfront_fee_million{0.0};
    double sponsor_npv_million{0.0};
    double project_closing_unrestricted_cash_million{0.0};
    double protected_reserve_closing_cash_million{0.0};
    double closing_undrawn_commitment_million{0.0};
    double closing_funded_claim_million{0.0};
    double maximum_cash_entry_imbalance_million{0.0};
    double maximum_memo_rollforward_imbalance_million{0.0};
};

struct WeightedDistributionSummary {
    double mean{0.0};
    double standard_deviation{0.0};
    double p50{0.0};
    double p95{0.0};
    double p99{0.0};
    double maximum{0.0};
    double expected_shortfall_95{0.0};
    double expected_shortfall_99{0.0};
};

struct StagedCapitalFeeCaseResult {
    std::string case_id{};
    double weight{0.0};
    StagedCapitalOutcome all_provider_performs_outcome{
        StagedCapitalOutcome::Completed};
    std::size_t outcome_month{0U};
    std::size_t recovery_month{0U};
    double provider_draws_million{0.0};
    double provider_nominal_recovery_million{0.0};
    double provider_npv_before_upfront_fee_million{0.0};
    double provider_npv_after_upfront_fee_million{0.0};
};

struct StagedCapitalSummary {
    std::vector<StagedCapitalPathResult> cases{};
    // Exact paired paths used by the fee calculation. They preserve every
    // physical case and its relative weight while holding provider cash
    // performance true; the exposed weights are normalized to sum to one.
    std::vector<StagedCapitalFeeCaseResult> fee_basis_cases{};
    // Raw declared sum before the accepted near-one normalization used by all
    // aggregate statistics.
    double configured_case_weight_sum{0.0};
    double completion_weight{0.0};
    double milestone_failure_weight{0.0};
    double cost_to_complete_failure_weight{0.0};
    double sponsor_funding_failure_weight{0.0};
    double provider_funding_failure_weight{0.0};
    double provider_draw_weight{0.0};
    double provider_principal_loss_weight{0.0};
    double provider_claim_writeoff_weight{0.0};
    double protected_reserve_shortfall_at_stop_weight{0.0};
    double safety_funding_shortfall_weight{0.0};
    // Full configured physical-case weight replayed with provider cash
    // performance held true. It must reconcile to one.
    double fee_sensitivity_included_weight{0.0};
    double expected_commitment_utilization{0.0};

    WeightedDistributionSummary provider_draws_million{};
    WeightedDistributionSummary peak_provider_net_cash_outlay_million{};
    WeightedDistributionSummary provider_principal_loss_million{};
    WeightedDistributionSummary provider_claim_writeoff_million{};
    WeightedDistributionSummary sponsor_total_cash_call_million{};
    WeightedDistributionSummary stranded_spend_million{};
    WeightedDistributionSummary funding_gap_million{};
    WeightedDistributionSummary
        protected_reserve_shortfall_at_stop_million{};
    WeightedDistributionSummary safety_funding_shortfall_million{};
    WeightedDistributionSummary outcome_month{};

    double expected_provider_nominal_recovery_million{0.0};
    double expected_provider_recovery_pv_million{0.0};
    double expected_provider_principal_loss_million{0.0};
    std::optional<double> conditional_provider_principal_loss_million{};
    double expected_provider_claim_writeoff_million{0.0};
    std::optional<double> conditional_provider_claim_writeoff_million{};
    double expected_provider_npv_before_upfront_fee_million{0.0};
    double expected_provider_npv_after_charged_upfront_fee_million{0.0};
    // A physical-measure, declared-hurdle sensitivity. It is not fair value,
    // a market quote, an arbitrage-free price, or a recommendation.
    double physical_measure_break_even_upfront_fee_million{0.0};
    double charged_upfront_fee_million{0.0};
    // Charged fee minus break-even fee. It equals expected provider NPV after
    // the charged time-zero fee on the declared case/hurdle basis.
    double upfront_fee_adequacy_gap_million{0.0};
    double maximum_cash_entry_imbalance_million{0.0};
    double maximum_memo_rollforward_imbalance_million{0.0};
};

[[nodiscard]] std::string_view to_string(
    CertificationDecision decision) noexcept;
[[nodiscard]] std::string_view to_string(
    StagedCapitalOutcome outcome) noexcept;
[[nodiscard]] std::string_view to_string(
    CapitalCashFlowKind kind) noexcept;

void validate_staged_capital_config(const StagedCapitalConfig& config);

// Deterministic contract evaluation. All construction costs, certification
// decisions, provider-performance flags, terminal values, and case weights are
// explicit inputs.
[[nodiscard]] StagedCapitalPathResult evaluate_staged_capital_case(
    const StagedCapitalConfig& config, std::string_view case_id);

// Evaluates every configured project-risk case. All aggregate calculations
// normalize the accepted near-one declared weight sum. The fee sensitivity
// replays every same-relative-weight case with provider cash performance held
// true; it never resamples, drops adverse states, or introduces a hidden
// probability model.
[[nodiscard]] StagedCapitalSummary evaluate_staged_capital_cases(
    const StagedCapitalConfig& config);

} // namespace naturalehia::cellular_finance
