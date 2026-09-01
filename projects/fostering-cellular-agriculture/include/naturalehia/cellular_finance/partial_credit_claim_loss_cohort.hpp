// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/claim_ledger_package.hpp>
#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kPartialCreditClaimLossCohortVersion{
    "0.1.0"};
inline constexpr std::size_t kMaximumPartialCreditCohortObservations{10'000U};
inline constexpr std::size_t kMaximumPartialCreditCohortExclusionRules{100U};
inline constexpr std::size_t
    kMaximumPartialCreditCohortEvidenceIdsPerList{256U};
inline constexpr std::size_t
    kMaximumPartialCreditCohortRetainedEvidenceIds{1'000'000U};
inline constexpr std::string_view
    kPartialCreditClaimLossCohortMechanicalImplementationId{
        "partial-credit-claim-loss-cohort-v0.1-mechanical-kernel"};

struct PartialCreditClaimLossCohortEvaluation;
namespace detail {
struct PartialCreditClaimLossCohortLoadAccess;
}

struct PartialCreditClaimLossCohortBoundFile {
    std::filesystem::path relative_path{};
    std::string sha256{};
};

enum class PartialCreditClaimLossMethodPurpose : unsigned char {
    Population,
    SamplingUnit,
    Cluster,
    TermStratum,
    Horizon,
    Loss,
    Resolution,
    Censoring,
    Denominator,
    MonetaryBasis,
    AmountBound,
    Metric,
};

struct PartialCreditClaimLossMethod {
    std::string id{};
    PartialCreditClaimLossMethodPurpose purpose{
        PartialCreditClaimLossMethodPurpose::Population};
    std::string version{};
    std::string implementation_id{};
    std::string effective_date{};
    std::string definition{};
    std::vector<std::string> inputs{};
    std::string output{};
    std::vector<std::string> evidence_record_ids{};
    std::vector<std::string> evidence_requirement_ids{};
};

enum class PartialCreditClaimLossDisposition : unsigned char {
    Resolved,
    NotYetMatured,
    Unresolved,
    Excluded,
};

enum class PartialCreditClaimTriggerStatus : unsigned char {
    Triggered,
    NotTriggered,
    Unknown,
    NotApplicable,
};

struct PartialCreditClaimLossExclusionRule {
    std::string id{};
    std::string frozen_date{};
    bool outcome_blind_asserted{};
    std::string statement{};
    std::vector<std::string> evidence_record_ids{};
};

// Definition fields are stable method IDs, not free-form substitutes for a
// bound methods file. The programmatic kernel validates their identities but
// grants no empirical authority; the five-file package loader binds and
// evidences the corresponding records while preserving that hard boundary.
struct PartialCreditClaimLossCohortConfig {
    std::string version{kPartialCreditClaimLossCohortVersion};
    std::string cohort_id{};
    std::string as_of_date{};
    std::string frame_start_date{};
    std::string frame_end_date{};
    std::string source_note{};
    std::string population_definition{};
    std::string sampling_unit_definition{};
    std::string economic_cluster_definition{};
    std::string protection_term_stratum_definition{};
    std::string outcome_horizon_definition{};
    std::string loss_definition{};
    std::string resolution_definition{};
    std::string censoring_definition{};
    std::string denominator_definition{};
    std::string currency_label{};
    std::string monetary_basis{};
    std::string monetary_basis_definition{};
    std::size_t population_frame_count{};
    bool candidate_only{true};
    PartialCreditClaimLossCohortBoundFile observations_file{};
    PartialCreditClaimLossCohortBoundFile methods_file{};
    PartialCreditClaimLossCohortBoundFile dossier_file{};
    PartialCreditClaimLossCohortBoundFile evidence_manifest_file{};
    std::vector<PartialCreditClaimLossExclusionRule> exclusion_rules{};
};

// Non-excluded observations carry a loader-verified Claim Ledger package.
// Evaluation reloads the package root and ignores mutable caller summaries.
// Excluded frame members remain visible but need not carry a claim package.
struct PartialCreditClaimLossObservation {
    std::string observation_id{};
    std::string economic_cluster_id{};
    std::string eligible_date{};
    std::string horizon_end_date{};
    PartialCreditClaimLossDisposition disposition{
        PartialCreditClaimLossDisposition::NotYetMatured};
    PartialCreditClaimTriggerStatus trigger_status{
        PartialCreditClaimTriggerStatus::Unknown};
    std::string trigger_date{"NONE"};
    std::string classification_date{"NONE"};
    std::string resolution_date{"NONE"};
    std::string exclusion_rule_id{"NONE"};
    std::filesystem::path claim_cfg_path{};
    std::optional<ClaimLedgerPackage> claim_package{};
    std::string expected_claim_config_sha256{"NONE"};
    std::string realized_scenario_id{"NONE"};
    std::string provider_claim_id{"NONE"};
    std::vector<std::string> population_evidence_record_ids{};
    std::vector<std::string> population_requirement_ids{};
    std::vector<std::string> classification_evidence_record_ids{};
    std::vector<std::string> classification_requirement_ids{};
};

struct PartialCreditClaimLossObservationResult {
    std::string observation_id{};
    std::string economic_cluster_id{};
    PartialCreditClaimLossDisposition disposition{
        PartialCreditClaimLossDisposition::NotYetMatured};
    PartialCreditClaimTriggerStatus trigger_status{
        PartialCreditClaimTriggerStatus::Unknown};
    std::string package_id{"NONE"};
    std::string claim_id{"NONE"};
    std::string claim_config_sha256{"NONE"};
    std::string realized_scenario_id{"NONE"};
    std::string provider_claim_id{"NONE"};
    bool synthetic_package{};
    bool claim_package_has_blockers{};
    bool resolved_path_exact{};
    // True means the resolved row was freshly reloaded and checked through
    // Claim Ledger's one-scenario selected-latest provenance seam during this
    // evaluation. It does not by itself make the row empirical: census,
    // method, as-of, status, and evidence gates are still required.
    bool selected_full_path_provenance_verified_during_evaluation{};
    bool mechanical_amount_bounds_available{};
    // The programmatic v0.1 kernel cannot prove field-level controlled outcome
    // evidence. This remains false even for a mechanically exact path.
    bool empirical_realized_cash_admissible{};
    // Contractual face is a legal cap/reference denominator. It is not funded
    // principal and is never used as the resolved-path conservation balance.
    ClaimLedgerValue contractual_face_million{};
    ClaimLedgerValue opening_principal_million{};
    ClaimLedgerValue funded_principal_created_million{};
    ClaimLedgerValue capitalized_principal_million{};
    // Opening principal plus lifetime funded and capitalized additions. This is
    // the resolved principal roll-forward basis, not point-in-time exposure.
    ClaimLedgerValue principal_rollforward_basis_million{};
    // This is Claim Ledger peak EAD, not EAD at a verified default event.
    ClaimLedgerValue peak_ead_million{};
    ClaimLedgerValue borrower_principal_cash_million{};
    ClaimLedgerValue recovery_principal_cash_million{};
    ClaimLedgerValue pre_support_principal_shortfall_million{};
    ClaimLedgerValue provider_claim_generated_million{};
    ClaimLedgerValue provider_claim_payable_million{};
    ClaimLedgerValue provider_principal_cash_million{};
    ClaimLedgerValue provider_unpaid_payable_claim_million{};
    ClaimLedgerValue provider_claim_payable_after_horizon_million{};
    ClaimLedgerValue conversion_principal_extinguishment_million{};
    ClaimLedgerValue final_principal_writeoff_million{};
    std::vector<std::string> blockers{};
};

class PartialCreditClaimLossCohortPackage {
public:
    std::filesystem::path directory{};
    std::string cohort_config_sha256{};
    PartialCreditClaimLossCohortConfig config{};
    std::vector<PartialCreditClaimLossMethod> methods{};
    EvidenceDossier evidence_dossier{};
    EvidenceGateUseBatchAssessment evidence_gate_assessment{};
    std::vector<std::string> admission_blockers{};
    std::vector<PartialCreditClaimLossObservation> observations{};

    [[nodiscard]] bool five_file_integrity_verified() const noexcept {
        return load_seal_ != nullptr;
    }

    [[nodiscard]] bool population_frame_evidence_passed() const noexcept {
        return load_seal_ != nullptr && load_seal_->population_frame_passed;
    }

    [[nodiscard]] bool candidate_package_valid() const noexcept {
        return load_seal_ != nullptr && load_seal_->candidate_valid;
    }

    [[nodiscard]] bool empirical_realized_cash_admissible() const noexcept {
        return false;
    }

private:
    struct LoadSeal {
        std::filesystem::path canonical_directory{};
        std::string cohort_config_sha256{};
        bool population_frame_passed{};
        bool candidate_valid{};
    };

    std::shared_ptr<const LoadSeal> load_seal_{};

    friend PartialCreditClaimLossCohortPackage
    load_partial_credit_claim_loss_cohort_package(
        const std::filesystem::path& root);
    friend PartialCreditClaimLossCohortEvaluation
    evaluate_partial_credit_claim_loss_cohort(
        const PartialCreditClaimLossCohortPackage& package);
    friend struct detail::PartialCreditClaimLossCohortLoadAccess;
};

struct PartialCreditClaimLossCohortEvaluation {
    bool candidate_only{true};
    bool calibrated_execution_authorized{};
    bool portfolio_export_authorized{};
    bool empirical_realized_cash_admissible{};
    bool five_file_integrity_verified{};
    bool population_frame_evidence_passed{};
    bool candidate_package_valid{};
    bool synthetic_package_present{};
    bool claim_ledger_package_blockers_present{};
    bool frame_cluster_ids_unique{};
    bool mechanical_amount_ranges_available{};
    bool all_included_resolved{};
    bool resolved_principal_conservation_reconciled{};

    std::size_t frame_count{};
    std::size_t included_count{};
    std::size_t resolved_count{};
    std::size_t not_yet_matured_count{};
    std::size_t unresolved_count{};
    std::size_t excluded_count{};
    std::size_t censored_count{};
    std::size_t trigger_known_count{};
    std::size_t triggered_count{};
    std::size_t trigger_unknown_count{};
    // These two counts are deliberately resolved-path counts. The current
    // kernel has no observed-to-date provenance seam for open rows.
    std::size_t resolved_provider_claim_generated_count{};
    std::size_t resolved_provider_claim_paid_count{};
    // A possible-positive count is not an observed unpaid or disputed claim.
    // Dispute status is outside the current schema.
    std::size_t provider_unpaid_claim_known_positive_count{};
    std::size_t provider_unpaid_claim_possible_positive_count{};

    ClaimLedgerValue total_contractual_face_million{};
    ClaimLedgerValue total_resolved_opening_principal_million{};
    ClaimLedgerValue total_resolved_funded_principal_created_million{};
    ClaimLedgerValue total_resolved_capitalized_principal_million{};
    ClaimLedgerValue total_resolved_principal_rollforward_basis_million{};
    // Sum of claim-level peaks; it is not a time-aligned portfolio peak.
    ClaimLedgerValue sum_resolved_claim_peak_ead_million{};
    ClaimLedgerValue total_resolved_borrower_principal_cash_million{};
    ClaimLedgerValue total_resolved_recovery_principal_cash_million{};
    ClaimLedgerValue total_resolved_provider_principal_cash_million{};
    ClaimLedgerValue total_resolved_conversion_principal_million{};
    ClaimLedgerValue total_resolved_final_principal_writeoff_million{};
    ClaimLedgerValue total_pre_support_principal_shortfall_million{};
    ClaimLedgerValue total_provider_claim_generated_million{};
    ClaimLedgerValue total_provider_claim_payable_million{};
    ClaimLedgerValue total_provider_principal_cash_million{};
    ClaimLedgerValue total_provider_unpaid_payable_claim_million{};
    ClaimLedgerValue total_provider_claim_payable_after_horizon_million{};
    ClaimLedgerValue total_final_principal_writeoff_million{};

    // Arithmetic identification ranges over the fixed included denominator.
    // They are not sampling confidence intervals or transferable probabilities.
    ClaimLedgerValue positive_pre_support_shortfall_frequency{};
    ClaimLedgerValue positive_provider_cash_frequency{};
    ClaimLedgerValue positive_final_writeoff_frequency{};
    ClaimLedgerValue provider_cash_to_contractual_face{};
    ClaimLedgerValue final_writeoff_to_contractual_face{};

    std::vector<PartialCreditClaimLossObservationResult> observations{};
    std::vector<std::string> blockers{};
};

[[nodiscard]] std::string_view to_string(
    PartialCreditClaimLossDisposition value) noexcept;
[[nodiscard]] std::string_view to_string(
    PartialCreditClaimTriggerStatus value) noexcept;
[[nodiscard]] std::string_view to_string(
    PartialCreditClaimLossMethodPurpose value) noexcept;

void validate_partial_credit_claim_loss_cohort_config(
    const PartialCreditClaimLossCohortConfig& config);

// This is the first implemented boundary: it re-verifies Claim Ledger roots,
// extracts mechanically exact resolved-path cash without reading scenario
// probabilities, keeps open members in coarse contractual amount/frequency
// envelopes, and emits no empirical admission, Portfolio, price,
// expected-return, or calibration object.
[[nodiscard]] PartialCreditClaimLossCohortEvaluation
evaluate_partial_credit_claim_loss_cohort(
    const PartialCreditClaimLossCohortPackage& package);

} // namespace naturalehia::cellular_finance
