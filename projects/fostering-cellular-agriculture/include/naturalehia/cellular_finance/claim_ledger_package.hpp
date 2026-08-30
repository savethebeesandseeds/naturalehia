// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/claim_ledger.hpp>

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kClaimLedgerPackageVersion{"0.1.0"};

enum class ClaimLedgerPackageStatus {
    RetainedPublicIncomplete,
    SyntheticComplete,
    ControlledCandidate,
};

enum class ClaimLedgerEconomicClusterBoundaryStatus {
    Defined,
    Unresolved,
};

enum class ClaimLedgerInputStatus {
    Observed,
    Contractual,
    Derived,
    Estimated,
    Stress,
    Backtest,
    Unknown,
};

enum class ClaimLedgerDateStatus {
    Known,
    Unknown,
};

struct ClaimLedgerBoundFile {
    std::filesystem::path relative_path{};
    std::string sha256{};
};

struct ClaimLedgerTypedDate {
    ClaimLedgerDateStatus status{ClaimLedgerDateStatus::Unknown};
    std::optional<std::string> value{};
    std::string source_record_id{"NO_PUBLIC_SOURCE"};
};

// A missing period is represented by std::nullopt. It is never converted to
// period zero. The separate source belongs to the scalar whose timing is being
// described, as required by the canonical claim.cfg schema.
struct ClaimLedgerPackageScalar {
    ClaimLedgerValue value{};
    std::optional<std::size_t> known_at_period{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    std::string source_record_id{"NO_PUBLIC_SOURCE"};
};

struct ClaimLedgerPackageConfig {
    std::string model_version{};
    std::string package_id{};
    ClaimLedgerPackageStatus package_status{
        ClaimLedgerPackageStatus::RetainedPublicIncomplete};
    std::string economic_cluster_id{};
    ClaimLedgerEconomicClusterBoundaryStatus economic_cluster_boundary_status{
        ClaimLedgerEconomicClusterBoundaryStatus::Unresolved};
    ClaimLedgerInputStatus economic_cluster_boundary_input_status{
        ClaimLedgerInputStatus::Unknown};
    std::optional<std::size_t>
        economic_cluster_boundary_known_at_period{};
    std::string economic_cluster_boundary_source_record_id{
        "NO_PUBLIC_SOURCE"};

    std::string claim_id{};
    std::string project_id{};
    std::string instrument_kind{};
    std::string obligor_id{};
    std::string obligor_scope_note{};
    std::string investor_id{};
    std::string investor_scope_note{};
    std::string currency_label{};
    std::string monetary_basis{};
    std::string period_unit_label{};
    std::size_t periods_per_year{};
    std::string conversion_unit_label{};
    std::string conversion_unit_basis{};

    ClaimLedgerTypedDate execution_date{};
    ClaimLedgerTypedDate funding_date{};
    ClaimLedgerTypedDate settlement_date{};
    ClaimLedgerTypedDate observation_date{};
    ClaimLedgerTypedDate decision_date{};
    ClaimLedgerTypedDate maturity_date{};
    ClaimLedgerTypedDate horizon_date{};
    ClaimLedgerTypedDate period_origin_date{};
    std::optional<std::size_t> decision_period{};
    std::optional<std::size_t> horizon_period{};

    ClaimLedgerPackageScalar contractual_face_amount_million{};
    ClaimLedgerPackageScalar opening_principal_million{};
    ClaimLedgerPackageScalar opening_accrued_interest_million{};
    ClaimLedgerPackageScalar annual_effective_discount_rate{};

    ClaimLedgerBoundFile terms{};
    ClaimLedgerBoundFile common_entries{};
    ClaimLedgerBoundFile scenarios{};
    ClaimLedgerBoundFile scenario_entries{};
    ClaimLedgerBoundFile provider_claims{};
    ClaimLedgerBoundFile covenant_events{};
    ClaimLedgerBoundFile conversion_context{};
    std::string source_manifest_sha256{};
};

struct ClaimLedgerPackageRowCounts {
    std::size_t terms{};
    std::size_t common_entries{};
    std::size_t scenarios{};
    std::size_t scenario_entries{};
    std::size_t provider_claims{};
    std::size_t covenant_events{};
    std::size_t conversion_context{};
};

// Field-level provenance retained beside the authoritative selected full path.
// A source-manifest row can be identified without being a verified retained
// source. Callers performing empirical admission must require
// retained_copy_verified and must still apply the relevant status/date rule.
struct ClaimLedgerSourceEvidenceSnapshot {
    std::string source_record_id{"NO_PUBLIC_SOURCE"};
    std::optional<std::string> record_date{};
    std::optional<std::string> evidence_class{};
    std::optional<std::string> provenance_tag{};
    bool retained_copy_verified{};
};

struct ClaimLedgerSelectedEntryEvidenceSnapshot {
    ClaimLedgerEntry entry{};
    // nullopt identifies a common entry; otherwise this is the selected
    // scenario-entry scope. It is retained so downstream consumers need not
    // infer scope from entry IDs or reparse the package TSVs.
    std::optional<std::string> scenario_id{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    ClaimLedgerSourceEvidenceSnapshot source{};
};

struct ClaimLedgerScenarioCashStatusEvidenceSnapshot {
    ClaimLedgerCashPathStatus cash_path_status{
        ClaimLedgerCashPathStatus::Incomplete};
    std::optional<std::size_t> known_at_period{};
    bool status_was_unknown{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    ClaimLedgerSourceEvidenceSnapshot source{};
};

struct ClaimLedgerProviderTermEvidenceSnapshot {
    ClaimLedgerProviderClaim term{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    ClaimLedgerSourceEvidenceSnapshot source{};
};

struct ClaimLedgerCovenantEvidenceSnapshot {
    bool common{};
    std::optional<std::string> scenario_id{};
    ClaimLedgerCovenantEvent event{};
    ClaimLedgerTypedDate event_date{};
    ClaimLedgerInputStatus input_status{ClaimLedgerInputStatus::Unknown};
    ClaimLedgerSourceEvidenceSnapshot source{};
};

// This is an on-demand, one-scenario snapshot. selected_entries contains
// exactly the latest full-information version of each economic_fact_id used
// for that scenario, including common entries. The index binds those facts to
// the authoritative evaluated path in the package returned beside it. The
// evaluated path is not copied or recomputed.
struct ClaimLedgerPathEvidenceSnapshot {
    std::filesystem::path package_directory{};
    std::string model_version{};
    std::string package_id{};
    std::string project_id{};
    std::string claim_id{};
    std::string scenario_id{};
    std::string currency_label{};
    std::string monetary_basis{};
    std::size_t decision_period{};
    std::size_t horizon_period{};
    std::string claim_config_sha256{};
    std::string source_manifest_sha256{};
    ClaimLedgerPackageScalar contractual_face_amount_million{};
    ClaimLedgerSourceEvidenceSnapshot contractual_face_source{};
    ClaimLedgerPackageScalar opening_principal_million{};
    ClaimLedgerSourceEvidenceSnapshot opening_principal_source{};
    ClaimLedgerPackageScalar opening_accrued_interest_million{};
    ClaimLedgerSourceEvidenceSnapshot opening_accrued_interest_source{};
    std::size_t full_evaluation_scenario_index{};
    ClaimLedgerScenarioCashStatusEvidenceSnapshot cash_path_status{};
    std::vector<ClaimLedgerSelectedEntryEvidenceSnapshot> selected_entries{};
    std::vector<ClaimLedgerProviderTermEvidenceSnapshot> provider_terms{};
    std::vector<ClaimLedgerCovenantEvidenceSnapshot> covenant_events{};
};

struct ClaimLedgerPackage {
    std::filesystem::path directory{};
    std::filesystem::path claim_config_filename{};
    std::string claim_config_sha256{};
    ClaimLedgerPackageConfig config{};
    ClaimLedgerPackageRowCounts row_counts{};

    // A successful load verifies the closed schemas, path confinement,
    // immutable-byte SHA-256 bindings, source-ID resolution and cross-table
    // identifiers. Invalid packages throw and are never returned partially.
    bool package_integrity{};

    // These are derived, never declared by claim.cfg. Expected-return
    // admission permits evidenced estimates but requires a mechanically
    // complete expected-cash ledger and rate preimage. Observation admission
    // is narrower: v0.1 requires a common primary closing with directly
    // observed buyer cash facts and settlement-dated transaction evidence.
    bool observation_admissible{};
    bool expected_return_admissible{};
    // NPV admission is deliberately separate: it additionally requires an
    // exact, decision-cut, retained-source discount rate. A mechanical NPV
    // may still be computed for diagnostics without becoming admissible.
    bool npv_admissible{};

    // True means the package contains enough structural timing to construct a
    // frozen decision-cut ClaimLedgerConfig. Later rows remain retained package
    // evidence and cannot erase an otherwise valid ex-ante calculation. This
    // flag does not mean the decision-cut economics or returns are ready.
    bool core_config_ready{};
    std::vector<std::string> blockers{};
    std::optional<ClaimLedgerConfig> core_config{};
    std::optional<ClaimLedgerSummary> evaluation{};

    // Full/backtest state is separate from the decision-cut state. It is
    // present only when every retained later row is structurally representable
    // and the resulting full core configuration validates. Callers must never
    // infer a full path from the decision-only evaluation when this is absent.
    bool full_path_evaluation_available{};
    std::optional<ClaimLedgerConfig> full_core_config{};
    std::optional<ClaimLedgerSummary> full_evaluation{};
};

// Both members are produced from one immutable-byte package load. This avoids
// accepting a mutable earlier package beside a freshly parsed provenance view.
struct ClaimLedgerPackageWithPathEvidence {
    ClaimLedgerPackage package{};
    ClaimLedgerPathEvidenceSnapshot full_path{};
};

// Parses the exact, closed claim.cfg schema. This validates field shapes and
// bound paths/hashes but does not open or verify the seven bound TSV files.
[[nodiscard]] ClaimLedgerPackageConfig load_claim_ledger_package_config(
    const std::filesystem::path& path);

// Loads one canonical directory through claim.cfg, verifies and parses each
// hash-bound TSV snapshot, resolves public source IDs against the parent
// source_manifest.tsv, derives blockers, and evaluates the core only when the
// structural timing fields can be represented without invented values.
[[nodiscard]] ClaimLedgerPackage load_claim_ledger_package(
    const std::filesystem::path& claim_config_path);

// Loads and evaluates the package once, then retains one requested scenario's
// authoritative selected latest fact versions, input statuses, source dates,
// provider terms, and covenant provenance. It fails closed unless the package-
// wide full evaluation is available. The snapshot grants no empirical,
// calibration, pricing, or Portfolio authority by itself.
[[nodiscard]] ClaimLedgerPackageWithPathEvidence
load_claim_ledger_package_with_full_path_evidence(
    const std::filesystem::path& claim_config_path,
    std::string_view scenario_id);

void print_claim_ledger_package_report(
    std::ostream& output, const ClaimLedgerPackage& package);

[[nodiscard]] std::string_view to_string(
    ClaimLedgerPackageStatus value) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerEconomicClusterBoundaryStatus value) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerInputStatus value) noexcept;
[[nodiscard]] std::string_view to_string(
    ClaimLedgerDateStatus value) noexcept;

} // namespace naturalehia::cellular_finance
