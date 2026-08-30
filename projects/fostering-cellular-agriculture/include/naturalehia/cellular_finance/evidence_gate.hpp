// SPDX-License-Identifier: MIT

#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kEvidenceSchemaVersion = "0.2.0";
inline constexpr std::string_view kClaimPopulationEvidenceSchemaVersion =
    "0.3.0";
inline constexpr std::string_view kClaimPopulationFrameRequirementId =
    "FIN-CLAIM-POPULATION-FRAME";

enum class DossierStatus {
    PublicResearchOnly,
    ControlledDiligence,
};

enum class DossierSubjectKind {
    ReferenceProject,
    ClaimPopulation,
};

enum class GateKind {
    ReferenceBoundary,
    ModelCalibration,
    CrossStructureDiligence,
    AnimalImpact,
};

enum class AssertionStatus {
    Supports,
    Partial,
    Contradicts,
    Mixed,
    Gap,
};

enum class SourceClass {
    None,
    Regulator,
    GovernmentDisclosure,
    CourtRecord,
    PublicFiling,
    ExecutedContract,
    OperatorDisclosure,
    IndependentReport,
    OperationalRecord,
    EngineeringRecord,
    FinancialRecord,
    CapitalProviderRecord,
    MarketDataRecord,
    CounterpartyRecord,
    AcademicResearch,
    Media,
    LegalOpinion,
    AccountingAnalysis,
    TaxAnalysis,
};

enum class VerificationLevel {
    V0,
    V1,
    V2,
    V3,
};

enum class Applicability {
    Illustrative,
    Adjacent,
    Near,
    Exact,
};

enum class Confidentiality {
    Public,
    Controlled,
    Restricted,
    Privileged,
};

enum class DecisionUse {
    QuestionOnly,
    Corroboration,
    Gate,
};

enum class ResolutionStatus {
    NotApplicable,
    Open,
    Resolved,
};

enum class ConflictStatus {
    NotAssessed,
    NoneDisclosed,
    Managed,
    Unresolved,
};

enum class AllowedUse {
    PublicResearch,
    DefinedReference,
    ControlledCalibration,
    CrossStructureDiligence,
    ImpactReporting,
};

struct DossierMetadata {
    std::string schema_version;
    std::string id;
    std::string as_of_date;
    DossierStatus status{DossierStatus::PublicResearchOnly};
    DossierSubjectKind subject_kind{DossierSubjectKind::ReferenceProject};
    std::string owner;
    std::string operator_legal_name;
    std::string financing_obligor;
    std::string authorized_controller;
    std::string facility_name;
    std::string site_city;
    std::string site_country;
    std::string jurisdiction;
    std::string governing_law;
    std::string product;
    std::string species;
    std::string process_scope;
    std::string facility_scope;
    std::string asset_perimeter;
    std::string intended_use;
    std::string financing_use_of_funds;
    std::string buyer_or_channel;
    std::string reporting_currency;
    std::string population_authority_legal_name;
    std::string population_program_or_book_id;
    std::string population_scope;
    std::string population_reporting_currency;
    bool negative_evidence_preserved{};
    bool public_claims_not_model_calibration{};
    bool no_bankability_claim_without_gate{};
    bool no_animal_impact_claim_without_gate{};
};

struct EvidenceRecord {
    std::string record_id;
    std::string requirement_id;
    AssertionStatus assertion_status{AssertionStatus::Gap};
    SourceClass source_class{SourceClass::Media};
    VerificationLevel verification{VerificationLevel::V0};
    Applicability applicability{Applicability::Illustrative};
    std::string source_date;
    std::string access_date;
    std::string next_review_date;
    std::string record_owner;
    std::string source_uri;
    std::filesystem::path retained_copy;
    std::string retained_sha256;
    std::string document_version;
    std::string extract_reference;
    Confidentiality confidentiality{Confidentiality::Public};
    bool adverse_evidence{};
    ResolutionStatus resolution_status{ResolutionStatus::NotApplicable};
    std::string resolved_by;
    std::string resolution_date;
    std::string resolution_authority;
    std::string resolution_basis;
    DecisionUse decision_use{DecisionUse::QuestionOnly};
    std::string verified_by;
    std::string verification_date;
    std::string verification_procedures;
    std::string approved_by;
    ConflictStatus conflict_status{ConflictStatus::NotAssessed};
    std::string limitations;
};

struct EvidenceDossier {
    std::filesystem::path directory;
    DossierMetadata metadata;
    std::vector<EvidenceRecord> records;
};

struct RequirementAssessment {
    std::string requirement_id;
    std::string title;
    bool passed{};
    bool adverse_evidence_present{};
    bool unresolved_adverse_evidence_present{};
    std::vector<std::string> reasons;
};

struct GateAssessment {
    GateKind gate{GateKind::ReferenceBoundary};
    bool passed{};
    std::size_t requirements_met{};
    std::size_t requirements_total{};
    std::vector<RequirementAssessment> requirements;
};

struct EvidenceAssessment {
    AllowedUse highest_allowed_use{AllowedUse::PublicResearch};
    std::string evaluation_date;
    std::vector<GateAssessment> gates;
    // Compiled analytical-profile requirements assessed by the same engine
    // but intentionally excluded from the four reference-project gates.
    std::vector<RequirementAssessment> supplemental_requirements;
    std::size_t adverse_records{};
    std::size_t unresolved_adverse_records{};
};

struct EvidenceRecordGateUseAssessment {
    std::string record_id;
    std::string requirement_id;
    bool record_qualifies{};
    bool requirement_passed{};
    std::vector<std::string> reasons;
};

struct EvidenceGateUseBatchAssessment {
    EvidenceAssessment dossier_assessment;
    std::vector<EvidenceRecordGateUseAssessment> records;
};

// Loads a strict key=value dossier and a strict tab-separated evidence
// manifest. Unknown fields, duplicate records, and unknown requirement IDs are
// rejected. The loader validates provenance metadata, not source truth.
[[nodiscard]] EvidenceDossier load_evidence_dossier(
    const std::filesystem::path& dossier_path,
    const std::filesystem::path& manifest_path);

// Parses the supplied bounded immutable byte snapshots while resolving
// retained evidence beneath canonical_directory. Hash-bound analytical
// packages use this overload so the bytes they verify are the bytes parsed.
[[nodiscard]] EvidenceDossier load_evidence_dossier_bytes(
    const std::filesystem::path& canonical_directory,
    std::string_view dossier_bytes,
    std::string_view manifest_bytes);

// Computes the lowercase hexadecimal SHA-256 digest of a regular file. This
// is the same implementation used to verify retained evidence copies, exposed
// so hash-bound analytical packages do not need a second digest engine.
[[nodiscard]] std::string sha256_file_lower_hex(
    const std::filesystem::path& path);

// Computes the same digest over an already bounded immutable byte snapshot.
// Hash-bound package loaders use this to parse exactly the bytes they verify.
[[nodiscard]] std::string sha256_bytes_lower_hex(std::string_view bytes);

// Applies hard-coded minimum evidence rules. A missing or stale retained copy,
// unresolved contrary evidence, or a public-research-only dossier fails closed.
[[nodiscard]] EvidenceAssessment assess_evidence_dossier(
    const EvidenceDossier& dossier,
    std::string_view evaluation_date);

// Validates and prepares one dossier assessment, then exposes gate-use results
// for every manifest record. Record indexing, retained-copy qualification, and
// each compiled requirement assessment are performed once per batch.
[[nodiscard]] EvidenceGateUseBatchAssessment
assess_evidence_gate_use_batch(
    const EvidenceDossier& dossier,
    std::string_view evaluation_date);

// Applies the same hard-coded requirement lookup and record qualification
// logic used by the dossier gate, then reports whether the cited record itself
// qualifies and whether its complete requirement assessment passes.
[[nodiscard]] EvidenceRecordGateUseAssessment
assess_evidence_record_gate_use(
    const EvidenceDossier& dossier,
    std::string_view record_id,
    std::string_view evaluation_date);

void print_evidence_report(
    std::ostream& output,
    const EvidenceDossier& dossier,
    const EvidenceAssessment& assessment);

[[nodiscard]] bool all_gates_pass(
    const EvidenceAssessment& assessment) noexcept;

// Returns true only when the separately compiled claim-population profile is
// present exactly once and has passed. It never promotes project AllowedUse.
[[nodiscard]] bool claim_population_frame_passes(
    const EvidenceAssessment& assessment) noexcept;

[[nodiscard]] std::string_view to_string(DossierStatus value) noexcept;
[[nodiscard]] std::string_view to_string(
    DossierSubjectKind value) noexcept;
[[nodiscard]] std::string_view to_string(GateKind value) noexcept;
[[nodiscard]] std::string_view to_string(AllowedUse value) noexcept;

} // namespace naturalehia::cellular_finance
