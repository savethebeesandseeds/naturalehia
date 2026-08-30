// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <ostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumEvidenceDossierBytes = 1024U * 1024U;
constexpr std::size_t kMaximumEvidenceManifestBytes = 64U * 1024U * 1024U;
constexpr std::size_t kMaximumEvidenceTextLineBytes = 128U * 1024U;
constexpr std::size_t kMaximumEvidenceManifestRows = 100'000U;

constexpr std::array<std::string_view, 26U> kReferenceDossierKeys{{
    "dossier.schema_version",
    "dossier.id",
    "dossier.as_of_date",
    "dossier.status",
    "dossier.owner",
    "project.operator_legal_name",
    "project.financing_obligor",
    "project.authorized_controller",
    "project.facility_name",
    "project.site_city",
    "project.site_country",
    "project.jurisdiction",
    "project.governing_law",
    "project.product",
    "project.species",
    "project.process_scope",
    "project.facility_scope",
    "project.asset_perimeter",
    "project.intended_use",
    "project.financing_use_of_funds",
    "project.buyer_or_channel",
    "project.reporting_currency",
    "governance.negative_evidence_preserved",
    "governance.public_claims_not_model_calibration",
    "governance.no_bankability_claim_without_gate",
    "governance.no_animal_impact_claim_without_gate",
}};

constexpr std::array<std::string_view, 14U> kClaimPopulationDossierKeys{{
    "dossier.schema_version",
    "dossier.id",
    "dossier.as_of_date",
    "dossier.status",
    "dossier.subject_kind",
    "dossier.owner",
    "population.authority_legal_name",
    "population.program_or_book_id",
    "population.scope",
    "population.reporting_currency",
    "governance.negative_evidence_preserved",
    "governance.public_claims_not_model_calibration",
    "governance.no_bankability_claim_without_gate",
    "governance.no_animal_impact_claim_without_gate",
}};

constexpr std::string_view kManifestHeader =
    "record_id\trequirement_id\tassertion_status\tsource_class\t"
    "verification\tapplicability\tsource_date\taccess_date\t"
    "next_review_date\trecord_owner\tsource_uri\tretained_copy\t"
    "retained_sha256\tdocument_version\textract_reference\t"
    "confidentiality\tadverse_evidence\tresolution_status\tresolved_by\t"
    "resolution_date\tresolution_authority\tresolution_basis\t"
    "decision_use\tverified_by\tverification_date\t"
    "verification_procedures\tapproved_by\tconflict_status\tlimitations";

using SourceMask = std::uint32_t;

[[nodiscard]] constexpr SourceMask source_bit(SourceClass value) noexcept {
    return SourceMask{1U} << static_cast<unsigned int>(value);
}

struct RequirementDefinition {
    std::string_view id;
    std::string_view title;
    GateKind gate;
    VerificationLevel minimum_verification;
    Applicability minimum_applicability;
    SourceMask accepted_sources;
    SourceMask additional_required_sources{};
    bool included_in_reference_gates{true};
};

constexpr SourceMask kOfficialSources =
    source_bit(SourceClass::Regulator) |
    source_bit(SourceClass::CourtRecord) |
    source_bit(SourceClass::PublicFiling);
constexpr SourceMask kLocationSources =
    kOfficialSources | source_bit(SourceClass::GovernmentDisclosure);
constexpr SourceMask kControlledFinancialSources =
    source_bit(SourceClass::FinancialRecord) |
    source_bit(SourceClass::ExecutedContract) |
    source_bit(SourceClass::IndependentReport);

constexpr std::array<RequirementDefinition, 58U> kRequirements{{
    {"REF-LEGAL-OPERATOR", "current legal operator",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact,
     kOfficialSources | source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::LegalOpinion)},
    {"REF-FINANCING-OBLIGOR-AUTHORITY",
     "financing obligor and authorized controller",
     GateKind::ReferenceBoundary, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CourtRecord) |
         source_bit(SourceClass::PublicFiling) |
         source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::LegalOpinion)},
    {"REF-FACILITY-SITE", "facility and site boundary",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact,
     kLocationSources | source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::ExecutedContract)},
    {"REF-ASSET-PERIMETER", "owned, leased, shared, and collateral assets",
     GateKind::ReferenceBoundary, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::CourtRecord) |
         source_bit(SourceClass::PublicFiling),
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::LegalOpinion)},
    {"REF-PRODUCT-SPEC", "financeable product specification",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::Regulator) |
         source_bit(SourceClass::ExecutedContract)},
    {"REF-PROCESS-BOUNDARY", "process and unit-operation boundary",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord),
     source_bit(SourceClass::OperationalRecord) |
         source_bit(SourceClass::Regulator) |
         source_bit(SourceClass::IndependentReport)},
    {"REF-JURISDICTION", "operating jurisdiction",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact, kLocationSources},
    {"REF-GOVERNING-LAW", "governing law and dispute forum",
     GateKind::ReferenceBoundary, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::LegalOpinion)},
    {"REF-USE-OF-FUNDS", "defined financing use of funds",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact, kControlledFinancialSources},
    {"REF-REGULATORY-FACILITY-SCOPE", "facility regulatory scope",
     GateKind::ReferenceBoundary, VerificationLevel::V2,
     Applicability::Exact, source_bit(SourceClass::Regulator)},
    {"REF-IP-RIGHTS-CONTINUITY",
     "IP title, freedom to operate, and insolvency continuity",
     GateKind::ReferenceBoundary, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CourtRecord) |
         source_bit(SourceClass::PublicFiling) |
         source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::LegalOpinion)},

    {"TEC-COMPLETE-RUN-HISTORY", "complete successful run history",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::IndependentReport)},
    {"TEC-FAILED-RUNS", "failed and contaminated run history",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::IndependentReport)},
    {"TEC-MASS-BALANCE", "batch mass balance and yield evidence",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord) |
         source_bit(SourceClass::EngineeringRecord),
     source_bit(SourceClass::IndependentReport)},
    {"TEC-UTILITY-COST", "metered utility and consumables evidence",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord) |
         source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::IndependentReport)},
    {"TEC-INDEPENDENT-REVIEW", "independent technical review",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::IndependentReport)},
    {"CON-DESIGN-BASIS", "issued design basis",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord)},
    {"CON-CAPEX-QUOTES", "vendor-backed capex estimate",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord),
     source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::FinancialRecord)},
    {"CON-SCHEDULE", "resource-loaded construction schedule",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord)},
    {"CON-CONTINGENCY", "quantified cost and schedule contingency",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::IndependentReport)},
    {"FIN-OPERATING-LEDGER", "operating ledger and unit economics",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::FinancialRecord)},
    {"TEC-CRITICAL-SUPPLY",
     "critical-input supply, concentration, and termination",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::IndependentReport)},
    {"CON-UTILITY-CAPACITY",
     "executed utility, wastewater, tariff, and curtailment capacity",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::Regulator),
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::FinancialRecord)},
    {"TEC-OM-RESILIENCE",
     "O&M, staffing, data integrity, cybersecurity, and continuity",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord) |
         source_bit(SourceClass::EngineeringRecord),
     source_bit(SourceClass::IndependentReport)},
    {"FIN-INSURANCE",
     "insurance limits, exclusions, claims, and interruption coverage",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::IndependentReport)},
    {"CON-DECOMMISSIONING",
     "closure, decommissioning, remediation, and financial assurance",
     GateKind::ModelCalibration, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::Regulator),
     source_bit(SourceClass::FinancialRecord)},

    {"REG-PRODUCT-SCOPE", "current product regulatory scope",
     GateKind::CrossStructureDiligence, VerificationLevel::V2,
     Applicability::Exact, source_bit(SourceClass::Regulator)},
    {"REG-OPEN-CONDITIONS", "open regulatory conditions and controls",
     GateKind::CrossStructureDiligence, VerificationLevel::V2,
     Applicability::Exact, source_bit(SourceClass::Regulator)},
    {"FIN-FINANCING-FAILURE-DIAGNOSIS",
     "binding financing constraint and unsupported alternatives",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::IndependentReport)},
    {"FIN-CAPITAL-ALTERNATIVES",
     "direct capital-provider evidence and rejected ordinary alternatives",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CapitalProviderRecord),
     source_bit(SourceClass::IndependentReport)},
    {"COM-EXECUTED-OFFTAKE", "executed offtake or reservation contract",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::ExecutedContract)},
    {"COM-ACCEPTANCE", "objective output acceptance protocol",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::OperationalRecord) |
         source_bit(SourceClass::IndependentReport)},
    {"COM-COUNTERPARTY-CREDIT", "counterparty credit evidence",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::IndependentReport) |
         source_bit(SourceClass::PublicFiling)},
    {"FIN-SOURCES-USES", "reconciled sources and uses",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::FinancialRecord)},
    {"FIN-TERM-SHEET", "current financing term sheet",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::ExecutedContract)},
    {"FIN-TAX-ANALYSIS", "tax characterization and consequences",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::TaxAnalysis)},
    {"FIN-ACCOUNTING-ANALYSIS", "accounting and valuation analysis",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::AccountingAnalysis)},
    {"FIN-LEGAL-ANALYSIS", "legal and regulatory characterization",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact, source_bit(SourceClass::LegalOpinion)},
    {"CON-COMPLETION-TEST", "contractual completion and cure test",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::IndependentReport)},
    {"FIN-CLAIMS-RECOVERY",
     "claims, liens, enforcement, insolvency waterfall, and recovery",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CourtRecord),
     source_bit(SourceClass::LegalOpinion)},
    {"FIN-RECOVERY-VALUATION",
     "claims register, recovery values, costs, and distribution waterfall",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::IndependentReport)},
    {"FIN-SPONSOR-EQUITY",
     "executed sponsor equity commitment and proof of funds",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::FinancialRecord)},
    {"INS-MILESTONE-MECHANICS",
     "staged-capital milestones, eligible costs, draws, cure, and workout",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::EngineeringRecord) |
         source_bit(SourceClass::IndependentReport)},
    {"INS-PRICE-INDEX-GOVERNANCE",
     "governed benchmark, transaction history, basis, and manipulation controls",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::MarketDataRecord),
     source_bit(SourceClass::IndependentReport)},
    {"INS-PRICE-SETTLEMENT-MECHANICS",
     "physical notional, settlement, cap, fallback, and disruption mechanics",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::FinancialRecord)},
    {"INS-SUPPORT-PROVIDER-CREDIT",
     "support-provider authority, capacity, and collateral",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::PublicFiling),
     source_bit(SourceClass::LegalOpinion)},
    {"INS-SUPPORT-CLAIMS-PROTOCOL",
     "executed support trigger, claims, exclusions, and recovery protocol",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::LegalOpinion) |
         source_bit(SourceClass::IndependentReport)},
    {"INS-COLLATERAL-CLOSEOUT",
     "collateral, liquidity, netting, close-out, and no-double-recovery",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::ExecutedContract) |
         source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::LegalOpinion)},
    {kClaimPopulationFrameRequirementId,
     "complete issued-or-at-risk partial-credit claim population and status register",
     GateKind::CrossStructureDiligence, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CapitalProviderRecord) |
         source_bit(SourceClass::GovernmentDisclosure) |
         source_bit(SourceClass::Regulator),
     source_bit(SourceClass::IndependentReport), false},

    {"IMP-FINANCING-ADDITIONALITY", "financing additionality method",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::ExecutedContract),
     source_bit(SourceClass::IndependentReport)},
    {"IMP-DISPLACEMENT-METHOD", "animal-product displacement method",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::IndependentReport) |
         source_bit(SourceClass::AcademicResearch)},
    {"IMP-ANIMAL-INPUT-BASELINE", "animal-derived input baseline",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord) |
         source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::IndependentReport)},
    {"IMP-OBSERVED-FINANCING",
     "observed financing decision, funding, terms, timing, and counterfactual",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::FinancialRecord),
     source_bit(SourceClass::CapitalProviderRecord) |
         source_bit(SourceClass::IndependentReport)},
    {"IMP-OBSERVED-OUTPUT-SALES",
     "released, sold, paid output and cultivated-fraction reconciliation",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::FinancialRecord)},
    {"IMP-BUYER-SUBSTITUTION",
     "direct buyer conventional purchasing and substitution evidence",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CounterpartyRecord),
     source_bit(SourceClass::FinancialRecord) |
         source_bit(SourceClass::IndependentReport)},
    {"IMP-ATTRIBUTION-RESULT",
     "observed attribution, leakage, rebound, and uncertainty result",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::CounterpartyRecord) |
         source_bit(SourceClass::MarketDataRecord),
     source_bit(SourceClass::IndependentReport)},
    {"IMP-OUTCOME-ASSURANCE",
     "independent observed-outcome assurance and correction release",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::IndependentReport)},
    {"IMP-WELFARE-CONVERSION",
     "animal-product and welfare conversion with coproduct allocation",
     GateKind::AnimalImpact, VerificationLevel::V3,
     Applicability::Exact,
     source_bit(SourceClass::OperationalRecord),
     source_bit(SourceClass::IndependentReport) |
         source_bit(SourceClass::AcademicResearch)},
}};

[[nodiscard]] constexpr bool requirement_source_groups_are_disjoint()
    noexcept {
    for (const RequirementDefinition& requirement : kRequirements) {
        if ((requirement.accepted_sources &
             requirement.additional_required_sources) != SourceMask{0U}) {
            return false;
        }
    }
    return true;
}

static_assert(
    requirement_source_groups_are_disjoint(),
    "primary and conjunctive requirement source groups must be disjoint");

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

void strip_terminal_carriage_return(std::string& value) noexcept {
    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
}

[[noreturn]] void data_error(
    const std::filesystem::path& path,
    std::size_t line,
    std::string_view message) {
    throw std::invalid_argument(
        path.string() + ":" + std::to_string(line) + ": " +
        std::string(message));
}

[[nodiscard]] bool parse_bool(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    data_error(path, line, "expected true or false");
}

[[nodiscard]] bool is_leap_year(int year) noexcept {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

[[nodiscard]] int decimal_part(std::string_view value) noexcept {
    int result = 0;
    for (const char character : value) {
        result = result * 10 + (character - '0');
    }
    return result;
}

[[nodiscard]] bool is_iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4] != '-' || value[7] != '-') {
        return false;
    }
    for (std::size_t index = 0U; index < value.size(); ++index) {
        if (index == 4U || index == 7U) {
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(value[index])) == 0) {
            return false;
        }
    }

    const int year = decimal_part(value.substr(0U, 4U));
    const int month = decimal_part(value.substr(5U, 2U));
    const int day = decimal_part(value.substr(8U, 2U));
    if (year < 1900 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    constexpr std::array<int, 12U> days_per_month{{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    }};
    int maximum_day = days_per_month[static_cast<std::size_t>(month - 1)];
    if (month == 2 && is_leap_year(year)) {
        maximum_day = 29;
    }
    return day <= maximum_day;
}

void validate_date(
    std::string_view value,
    bool allow_none,
    bool allow_undated,
    const std::filesystem::path& path,
    std::size_t line,
    std::string_view field) {
    if (allow_none && value == "NONE") {
        return;
    }
    if (allow_undated && value == "UNDATED") {
        return;
    }
    if (!is_iso_date(value)) {
        data_error(
            path, line, std::string(field) + " must be YYYY-MM-DD");
    }
}

[[nodiscard]] DossierStatus parse_dossier_status(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "public-research-only") {
        return DossierStatus::PublicResearchOnly;
    }
    if (value == "controlled-diligence") {
        return DossierStatus::ControlledDiligence;
    }
    data_error(
        path, line,
        "expected public-research-only or controlled-diligence");
}

[[nodiscard]] DossierSubjectKind parse_dossier_subject_kind(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "reference-project") {
        return DossierSubjectKind::ReferenceProject;
    }
    if (value == "claim-population") {
        return DossierSubjectKind::ClaimPopulation;
    }
    data_error(path, line,
        "expected reference-project or claim-population");
}

[[nodiscard]] AssertionStatus parse_assertion_status(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "supports") {
        return AssertionStatus::Supports;
    }
    if (value == "partial") {
        return AssertionStatus::Partial;
    }
    if (value == "contradicts") {
        return AssertionStatus::Contradicts;
    }
    if (value == "mixed") {
        return AssertionStatus::Mixed;
    }
    if (value == "gap") {
        return AssertionStatus::Gap;
    }
    data_error(path, line, "invalid assertion_status");
}

[[nodiscard]] SourceClass parse_source_class(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    constexpr std::array<std::pair<std::string_view, SourceClass>, 19U>
        values{{
            {"none", SourceClass::None},
            {"regulator", SourceClass::Regulator},
            {"government-disclosure", SourceClass::GovernmentDisclosure},
            {"court-record", SourceClass::CourtRecord},
            {"public-filing", SourceClass::PublicFiling},
            {"executed-contract", SourceClass::ExecutedContract},
            {"operator-disclosure", SourceClass::OperatorDisclosure},
            {"independent-report", SourceClass::IndependentReport},
            {"operational-record", SourceClass::OperationalRecord},
            {"engineering-record", SourceClass::EngineeringRecord},
            {"financial-record", SourceClass::FinancialRecord},
            {"capital-provider-record", SourceClass::CapitalProviderRecord},
            {"market-data-record", SourceClass::MarketDataRecord},
            {"counterparty-record", SourceClass::CounterpartyRecord},
            {"academic-research", SourceClass::AcademicResearch},
            {"media", SourceClass::Media},
            {"legal-opinion", SourceClass::LegalOpinion},
            {"accounting-analysis", SourceClass::AccountingAnalysis},
            {"tax-analysis", SourceClass::TaxAnalysis},
        }};
    for (const auto& [name, parsed] : values) {
        if (value == name) {
            return parsed;
        }
    }
    data_error(path, line, "invalid source_class");
}

[[nodiscard]] ConflictStatus parse_conflict_status(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "not-assessed") {
        return ConflictStatus::NotAssessed;
    }
    if (value == "none-disclosed") {
        return ConflictStatus::NoneDisclosed;
    }
    if (value == "managed") {
        return ConflictStatus::Managed;
    }
    if (value == "unresolved") {
        return ConflictStatus::Unresolved;
    }
    data_error(path, line, "invalid conflict_status");
}

[[nodiscard]] VerificationLevel parse_verification(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "V0") {
        return VerificationLevel::V0;
    }
    if (value == "V1") {
        return VerificationLevel::V1;
    }
    if (value == "V2") {
        return VerificationLevel::V2;
    }
    if (value == "V3") {
        return VerificationLevel::V3;
    }
    data_error(path, line, "verification must be V0, V1, V2, or V3");
}

[[nodiscard]] Applicability parse_applicability(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "illustrative") {
        return Applicability::Illustrative;
    }
    if (value == "adjacent") {
        return Applicability::Adjacent;
    }
    if (value == "near") {
        return Applicability::Near;
    }
    if (value == "exact") {
        return Applicability::Exact;
    }
    data_error(path, line, "invalid applicability");
}

[[nodiscard]] Confidentiality parse_confidentiality(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "public") {
        return Confidentiality::Public;
    }
    if (value == "controlled") {
        return Confidentiality::Controlled;
    }
    if (value == "restricted") {
        return Confidentiality::Restricted;
    }
    if (value == "privileged") {
        return Confidentiality::Privileged;
    }
    data_error(path, line, "invalid confidentiality");
}

[[nodiscard]] DecisionUse parse_decision_use(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "question-only") {
        return DecisionUse::QuestionOnly;
    }
    if (value == "corroboration") {
        return DecisionUse::Corroboration;
    }
    if (value == "gate") {
        return DecisionUse::Gate;
    }
    data_error(path, line, "invalid decision_use");
}

[[nodiscard]] ResolutionStatus parse_resolution_status(
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (value == "not-applicable") {
        return ResolutionStatus::NotApplicable;
    }
    if (value == "open") {
        return ResolutionStatus::Open;
    }
    if (value == "resolved") {
        return ResolutionStatus::Resolved;
    }
    data_error(path, line, "invalid resolution_status");
}

[[nodiscard]] const RequirementDefinition* find_requirement(
    std::string_view id) noexcept {
    const auto found = std::find_if(
        kRequirements.begin(), kRequirements.end(),
        [id](const RequirementDefinition& requirement) {
            return requirement.id == id;
        });
    return found == kRequirements.end() ? nullptr : &*found;
}

[[nodiscard]] std::vector<std::string_view> split_tsv(
    std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0U;
    while (true) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1U;
    }
}

[[nodiscard]] bool safe_relative_path(
    const std::filesystem::path& path) noexcept {
    if (path.empty() || path.is_absolute() || path.has_root_name() ||
        path.has_root_directory()) {
        return false;
    }
    for (const auto& part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(
            value.begin(), value.end(),
            [](char character) {
                return (character >= '0' && character <= '9') ||
                    (character >= 'a' && character <= 'f');
            });
}

void assign_metadata(
    DossierMetadata& metadata,
    std::string_view key,
    std::string_view value,
    const std::filesystem::path& path,
    std::size_t line) {
    if (key == "dossier.schema_version") {
        metadata.schema_version = value;
    } else if (key == "dossier.id") {
        metadata.id = value;
    } else if (key == "dossier.as_of_date") {
        validate_date(value, false, false, path, line, key);
        metadata.as_of_date = value;
    } else if (key == "dossier.status") {
        metadata.status = parse_dossier_status(value, path, line);
    } else if (key == "dossier.subject_kind") {
        metadata.subject_kind =
            parse_dossier_subject_kind(value, path, line);
    } else if (key == "dossier.owner") {
        metadata.owner = value;
    } else if (key == "project.operator_legal_name") {
        metadata.operator_legal_name = value;
    } else if (key == "project.financing_obligor") {
        metadata.financing_obligor = value;
    } else if (key == "project.authorized_controller") {
        metadata.authorized_controller = value;
    } else if (key == "project.facility_name") {
        metadata.facility_name = value;
    } else if (key == "project.site_city") {
        metadata.site_city = value;
    } else if (key == "project.site_country") {
        metadata.site_country = value;
    } else if (key == "project.jurisdiction") {
        metadata.jurisdiction = value;
    } else if (key == "project.governing_law") {
        metadata.governing_law = value;
    } else if (key == "project.product") {
        metadata.product = value;
    } else if (key == "project.species") {
        metadata.species = value;
    } else if (key == "project.process_scope") {
        metadata.process_scope = value;
    } else if (key == "project.facility_scope") {
        metadata.facility_scope = value;
    } else if (key == "project.asset_perimeter") {
        metadata.asset_perimeter = value;
    } else if (key == "project.intended_use") {
        metadata.intended_use = value;
    } else if (key == "project.financing_use_of_funds") {
        metadata.financing_use_of_funds = value;
    } else if (key == "project.buyer_or_channel") {
        metadata.buyer_or_channel = value;
    } else if (key == "project.reporting_currency") {
        metadata.reporting_currency = value;
    } else if (key == "population.authority_legal_name") {
        metadata.population_authority_legal_name = value;
    } else if (key == "population.program_or_book_id") {
        metadata.population_program_or_book_id = value;
    } else if (key == "population.scope") {
        metadata.population_scope = value;
    } else if (key == "population.reporting_currency") {
        metadata.population_reporting_currency = value;
    } else if (key == "governance.negative_evidence_preserved") {
        metadata.negative_evidence_preserved =
            parse_bool(value, path, line);
    } else if (key ==
               "governance.public_claims_not_model_calibration") {
        metadata.public_claims_not_model_calibration =
            parse_bool(value, path, line);
    } else if (key ==
               "governance.no_bankability_claim_without_gate") {
        metadata.no_bankability_claim_without_gate =
            parse_bool(value, path, line);
    } else if (key ==
               "governance.no_animal_impact_claim_without_gate") {
        metadata.no_animal_impact_claim_without_gate =
            parse_bool(value, path, line);
    } else {
        data_error(path, line, "unknown key: " + std::string(key));
    }
}

[[nodiscard]] DossierMetadata parse_metadata_bytes(
    const std::filesystem::path& path, std::string_view bytes) {
    if (bytes.size() > kMaximumEvidenceDossierBytes) {
        throw std::invalid_argument(
            path.string() + ": dossier exceeds its byte guardrail");
    }
    std::istringstream input{std::string(bytes)};
    DossierMetadata metadata;
    std::unordered_set<std::string> seen;
    std::string line_text;
    std::size_t line_number = 0U;
    while (std::getline(input, line_text)) {
        ++line_number;
        if (line_text.size() > kMaximumEvidenceTextLineBytes) {
            data_error(path, line_number,
                "dossier line exceeds its byte guardrail");
        }
        std::string_view line{line_text};
        if (line_number == 1U && line.size() >= 3U &&
            static_cast<unsigned char>(line[0]) == 0xEFU &&
            static_cast<unsigned char>(line[1]) == 0xBBU &&
            static_cast<unsigned char>(line[2]) == 0xBFU) {
            line.remove_prefix(3U);
        }
        line = trim_view(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            data_error(path, line_number, "expected key=value");
        }
        const std::string_view key = trim_view(line.substr(0U, equals));
        const std::string_view value = trim_view(line.substr(equals + 1U));
        if (key.empty() || value.empty()) {
            data_error(path, line_number, "key and value must not be empty");
        }
        if (!seen.insert(std::string(key)).second) {
            data_error(
                path, line_number, "duplicate key: " + std::string(key));
        }
        assign_metadata(metadata, key, value, path, line_number);
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while reading dossier: " +
                                 path.string());
    }
    const auto require_exact_keys = [&](const auto& required) {
        std::unordered_set<std::string> allowed;
        allowed.reserve(required.size());
        for (const std::string_view key : required) {
            allowed.emplace(key);
            if (!seen.contains(std::string(key))) {
                throw std::invalid_argument(
                    path.string() + ": missing required key: " +
                    std::string(key));
            }
        }
        for (const std::string& key : seen) {
            if (!allowed.contains(key)) {
                throw std::invalid_argument(
                    path.string() + ": key is not permitted by this dossier schema: " +
                    key);
            }
        }
    };
    if (metadata.schema_version == kEvidenceSchemaVersion) {
        require_exact_keys(kReferenceDossierKeys);
        metadata.subject_kind = DossierSubjectKind::ReferenceProject;
    } else if (metadata.schema_version ==
               kClaimPopulationEvidenceSchemaVersion) {
        require_exact_keys(kClaimPopulationDossierKeys);
        if (metadata.subject_kind !=
            DossierSubjectKind::ClaimPopulation) {
            throw std::invalid_argument(
                path.string() +
                ": claim-population schema requires dossier.subject_kind=claim-population");
        }
    } else {
        throw std::invalid_argument(
            path.string() + ": unsupported schema version: " +
            metadata.schema_version);
    }
    if (!metadata.negative_evidence_preserved ||
        !metadata.public_claims_not_model_calibration ||
        !metadata.no_bankability_claim_without_gate ||
        !metadata.no_animal_impact_claim_without_gate) {
        throw std::invalid_argument(
            path.string() +
            ": all four governance commitments must be true");
    }
    return metadata;
}

[[nodiscard]] EvidenceRecord parse_record(
    const std::vector<std::string_view>& fields,
    const std::filesystem::path& path,
    std::size_t line) {
    if (fields.size() != 29U) {
        data_error(path, line, "expected exactly 29 tab-separated fields");
    }
    for (std::size_t index = 0U; index < fields.size(); ++index) {
        if (fields[index].empty()) {
            data_error(path, line, "manifest fields must not be empty");
        }
        if (trim_view(fields[index]) != fields[index]) {
            data_error(
                path, line,
                "manifest fields must not have surrounding whitespace");
        }
    }

    EvidenceRecord record;
    record.record_id = fields[0];
    record.requirement_id = fields[1];
    if (find_requirement(record.requirement_id) == nullptr) {
        data_error(
            path, line,
            "unknown requirement_id: " + record.requirement_id);
    }
    record.assertion_status =
        parse_assertion_status(fields[2], path, line);
    record.source_class = parse_source_class(fields[3], path, line);
    record.verification = parse_verification(fields[4], path, line);
    record.applicability = parse_applicability(fields[5], path, line);
    const bool is_gap = record.assertion_status == AssertionStatus::Gap;
    validate_date(fields[6], is_gap, !is_gap, path, line, "source_date");
    validate_date(fields[7], false, false, path, line, "access_date");
    validate_date(fields[8], false, false, path, line, "next_review_date");
    record.source_date = fields[6];
    record.access_date = fields[7];
    record.next_review_date = fields[8];
    record.record_owner = fields[9];
    record.source_uri = fields[10];
    if (fields[11] != "NONE") {
        record.retained_copy = std::filesystem::path(fields[11]);
        if (!safe_relative_path(record.retained_copy)) {
            data_error(
                path, line,
                "retained_copy must be a safe relative path or NONE");
        }
    }
    record.retained_sha256 = fields[12];
    record.document_version = fields[13];
    record.extract_reference = fields[14];
    record.confidentiality =
        parse_confidentiality(fields[15], path, line);
    record.adverse_evidence = parse_bool(fields[16], path, line);
    record.resolution_status =
        parse_resolution_status(fields[17], path, line);
    record.resolved_by = fields[18];
    record.resolution_date = fields[19];
    record.resolution_authority = fields[20];
    record.resolution_basis = fields[21];
    record.decision_use = parse_decision_use(fields[22], path, line);
    record.verified_by = fields[23];
    validate_date(
        fields[24], is_gap, false, path, line, "verification_date");
    record.verification_date = fields[24];
    record.verification_procedures = fields[25];
    record.approved_by = fields[26];
    record.conflict_status =
        parse_conflict_status(fields[27], path, line);
    record.limitations = fields[28];

    const bool is_contrary =
        record.assertion_status == AssertionStatus::Contradicts ||
        record.assertion_status == AssertionStatus::Mixed;
    if (is_contrary && !record.adverse_evidence) {
        data_error(
            path, line,
            "contradicts and mixed records must flag adverse_evidence=true");
    }
    if (!is_contrary && record.adverse_evidence) {
        data_error(
            path, line,
            "only contradicts and mixed records may flag adverse evidence");
    }
    if (is_contrary &&
        record.resolution_status == ResolutionStatus::NotApplicable) {
        data_error(
            path, line,
            "contradicts and mixed records require open or resolved status");
    }
    if (!is_contrary &&
        (record.resolution_status != ResolutionStatus::NotApplicable ||
         record.resolved_by != "NONE" ||
         record.resolution_date != "NONE" ||
         record.resolution_authority != "NONE" ||
         record.resolution_basis != "NONE")) {
        data_error(
            path, line,
            "non-adverse records require not-applicable resolution sentinels");
    }
    if (record.resolution_status == ResolutionStatus::Open &&
        (record.resolved_by != "NONE" ||
         record.resolution_date != "NONE" ||
         record.resolution_authority != "NONE" ||
         record.resolution_basis == "NONE")) {
        data_error(
            path, line,
            "open adverse records require an open basis and other resolution "
            "fields NONE");
    }
    if (record.resolution_status == ResolutionStatus::Resolved &&
        (record.resolved_by == "NONE" ||
         !is_iso_date(record.resolution_date) ||
         record.resolution_authority == "NONE" ||
         record.resolution_basis == "NONE" ||
         record.resolution_basis.find(record.record_id) ==
             std::string::npos)) {
        data_error(
            path, line,
            "resolved adverse records require link, date, authority, and a "
            "basis naming the adverse record ID");
    }
    if (is_gap &&
        (record.source_class != SourceClass::None ||
         record.verification != VerificationLevel::V0 ||
         record.source_date != "NONE" || record.source_uri != "NONE" ||
         !record.retained_copy.empty() ||
         record.retained_sha256 != "NONE" ||
         record.document_version != "NONE" ||
         record.extract_reference != "NONE" ||
         record.resolution_status != ResolutionStatus::NotApplicable ||
         record.resolved_by != "NONE" ||
         record.decision_use != DecisionUse::QuestionOnly ||
         record.adverse_evidence || record.verified_by != "NONE" ||
         record.verification_date != "NONE" ||
         record.verification_procedures != "NONE" ||
         record.approved_by != "NONE" ||
         record.conflict_status != ConflictStatus::NotAssessed)) {
        data_error(
            path, line,
            "gap records must use none/V0/NONE and question-only fields");
    }
    if (!is_gap &&
        (record.source_class == SourceClass::None ||
         record.source_date == "NONE" || record.source_uri == "NONE" ||
         record.document_version == "NONE" ||
         record.verification_date == "NONE")) {
        data_error(
            path, line,
            "non-gap records require source, document, and review provenance");
    }
    if (record.retained_copy.empty() !=
        (record.retained_sha256 == "NONE")) {
        data_error(
            path, line,
            "retained_copy and retained_sha256 must both be present or NONE");
    }
    if (!record.retained_copy.empty() &&
        !is_lower_hex_sha256(record.retained_sha256)) {
        data_error(path, line, "retained_sha256 must be 64 lowercase hex");
    }
    return record;
}

[[nodiscard]] std::vector<EvidenceRecord> parse_manifest_bytes(
    const std::filesystem::path& path, std::string_view bytes) {
    if (bytes.size() > kMaximumEvidenceManifestBytes) {
        throw std::invalid_argument(
            path.string() + ": manifest exceeds its byte guardrail");
    }
    std::istringstream input{std::string(bytes)};
    std::string line_text;
    if (!std::getline(input, line_text)) {
        throw std::invalid_argument(path.string() + ": empty manifest");
    }
    strip_terminal_carriage_return(line_text);
    if (line_text.size() > kMaximumEvidenceTextLineBytes) {
        data_error(path, 1U,
            "manifest header exceeds its byte guardrail");
    }
    std::string_view header{line_text};
    if (header.size() >= 3U &&
        static_cast<unsigned char>(header[0]) == 0xEFU &&
        static_cast<unsigned char>(header[1]) == 0xBBU &&
        static_cast<unsigned char>(header[2]) == 0xBFU) {
        header.remove_prefix(3U);
    }
    if (header != kManifestHeader) {
        data_error(path, 1U, "manifest header does not match schema");
    }

    std::vector<EvidenceRecord> records;
    std::unordered_set<std::string> record_ids;
    std::size_t line_number = 1U;
    while (std::getline(input, line_text)) {
        ++line_number;
        if (line_number - 1U > kMaximumEvidenceManifestRows) {
            data_error(path, line_number,
                "manifest exceeds its row guardrail");
        }
        strip_terminal_carriage_return(line_text);
        if (line_text.size() > kMaximumEvidenceTextLineBytes) {
            data_error(path, line_number,
                "manifest row exceeds its byte guardrail");
        }
        if (line_text.empty()) {
            continue;
        }
        EvidenceRecord record =
            parse_record(split_tsv(line_text), path, line_number);
        if (!record_ids.insert(record.record_id).second) {
            data_error(
                path, line_number,
                "duplicate record_id: " + record.record_id);
        }
        records.push_back(std::move(record));
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while reading manifest: " +
                                 path.string());
    }
    return records;
}

[[noreturn]] void invariant_error(std::string_view message) {
    throw std::invalid_argument(
        "evidence dossier invariant: " + std::string(message));
}

[[nodiscard]] bool is_unresolved_metadata(std::string_view value);

[[nodiscard]] bool is_contrary(const EvidenceRecord& record) noexcept {
    return record.assertion_status == AssertionStatus::Contradicts ||
        record.assertion_status == AssertionStatus::Mixed;
}

void validate_dossier_invariants(const EvidenceDossier& dossier) {
    const DossierMetadata& metadata = dossier.metadata;
    const bool reference_project =
        metadata.schema_version == kEvidenceSchemaVersion &&
        metadata.subject_kind == DossierSubjectKind::ReferenceProject;
    const bool claim_population =
        metadata.schema_version == kClaimPopulationEvidenceSchemaVersion &&
        metadata.subject_kind == DossierSubjectKind::ClaimPopulation;
    if (!reference_project && !claim_population) {
        invariant_error("unsupported schema version");
    }
    if (!is_iso_date(metadata.as_of_date)) {
        invariant_error("dossier as-of date must be YYYY-MM-DD");
    }
    if (static_cast<int>(metadata.status) <
            static_cast<int>(DossierStatus::PublicResearchOnly) ||
        static_cast<int>(metadata.status) >
            static_cast<int>(DossierStatus::ControlledDiligence)) {
        invariant_error("invalid dossier status");
    }
    const std::array<const std::string*, 5U> common_required{{
        &metadata.schema_version,
        &metadata.id,
        &metadata.as_of_date,
        &metadata.owner,
        claim_population
            ? &metadata.population_reporting_currency
            : &metadata.reporting_currency,
    }};
    if (std::any_of(common_required.begin(), common_required.end(),
            [](const std::string* value) { return value->empty(); })) {
        invariant_error("required metadata fields must not be empty");
    }
    const std::array<const std::string*, 16U> reference_required{{
        &metadata.operator_legal_name,
        &metadata.financing_obligor,
        &metadata.authorized_controller,
        &metadata.facility_name,
        &metadata.site_city,
        &metadata.site_country,
        &metadata.jurisdiction,
        &metadata.governing_law,
        &metadata.product,
        &metadata.species,
        &metadata.process_scope,
        &metadata.facility_scope,
        &metadata.asset_perimeter,
        &metadata.intended_use,
        &metadata.financing_use_of_funds,
        &metadata.buyer_or_channel,
    }};
    const std::array<const std::string*, 3U> population_required{{
        &metadata.population_authority_legal_name,
        &metadata.population_program_or_book_id,
        &metadata.population_scope,
    }};
    if ((reference_project && std::any_of(reference_required.begin(),
            reference_required.end(),
            [](const std::string* value) { return value->empty(); })) ||
        (claim_population && std::any_of(population_required.begin(),
            population_required.end(),
            [](const std::string* value) { return value->empty(); }))) {
        invariant_error("required metadata fields must not be empty");
    }
    const std::array<const std::string*, 4U> population_only{{
        &metadata.population_authority_legal_name,
        &metadata.population_program_or_book_id,
        &metadata.population_scope,
        &metadata.population_reporting_currency,
    }};
    const std::array<const std::string*, 17U> reference_only{{
        &metadata.operator_legal_name,
        &metadata.financing_obligor,
        &metadata.authorized_controller,
        &metadata.facility_name,
        &metadata.site_city,
        &metadata.site_country,
        &metadata.jurisdiction,
        &metadata.governing_law,
        &metadata.product,
        &metadata.species,
        &metadata.process_scope,
        &metadata.facility_scope,
        &metadata.asset_perimeter,
        &metadata.intended_use,
        &metadata.financing_use_of_funds,
        &metadata.buyer_or_channel,
        &metadata.reporting_currency,
    }};
    if ((reference_project && std::any_of(population_only.begin(),
            population_only.end(),
            [](const std::string* value) { return !value->empty(); })) ||
        (claim_population && std::any_of(reference_only.begin(),
            reference_only.end(),
            [](const std::string* value) { return !value->empty(); }))) {
        invariant_error(
            "metadata fields from another dossier subject are not permitted");
    }
    if (!metadata.negative_evidence_preserved ||
        !metadata.public_claims_not_model_calibration ||
        !metadata.no_bankability_claim_without_gate ||
        !metadata.no_animal_impact_claim_without_gate) {
        invariant_error("all governance commitments must be true");
    }
    if (!dossier.directory.is_absolute()) {
        invariant_error("dossier directory must be absolute");
    }
    std::error_code directory_error;
    if (!std::filesystem::is_directory(
            dossier.directory, directory_error) || directory_error) {
        invariant_error("dossier directory must exist");
    }

    std::unordered_set<std::string> record_ids;
    for (const EvidenceRecord& record : dossier.records) {
        if (record.record_id.empty() || record.requirement_id.empty() ||
            record.source_date.empty() || record.access_date.empty() ||
            record.next_review_date.empty() || record.record_owner.empty() ||
            record.source_uri.empty() || record.retained_sha256.empty() ||
            record.document_version.empty() ||
            record.extract_reference.empty() || record.resolved_by.empty() ||
            record.resolution_date.empty() ||
            record.resolution_authority.empty() ||
            record.resolution_basis.empty() || record.verified_by.empty() ||
            record.verification_date.empty() ||
            record.verification_procedures.empty() ||
            record.approved_by.empty() || record.limitations.empty()) {
            invariant_error("record string fields must not be empty");
        }
        if (!record_ids.insert(record.record_id).second) {
            invariant_error("record IDs must be unique");
        }
        if (find_requirement(record.requirement_id) == nullptr) {
            invariant_error("record has an unknown requirement ID");
        }
        if (static_cast<int>(record.assertion_status) <
                static_cast<int>(AssertionStatus::Supports) ||
            static_cast<int>(record.assertion_status) >
                static_cast<int>(AssertionStatus::Gap) ||
            static_cast<int>(record.source_class) <
                static_cast<int>(SourceClass::None) ||
            static_cast<int>(record.source_class) >
                static_cast<int>(SourceClass::TaxAnalysis) ||
            static_cast<int>(record.verification) <
                static_cast<int>(VerificationLevel::V0) ||
            static_cast<int>(record.verification) >
                static_cast<int>(VerificationLevel::V3) ||
            static_cast<int>(record.applicability) <
                static_cast<int>(Applicability::Illustrative) ||
            static_cast<int>(record.applicability) >
                static_cast<int>(Applicability::Exact) ||
            static_cast<int>(record.confidentiality) <
                static_cast<int>(Confidentiality::Public) ||
            static_cast<int>(record.confidentiality) >
                static_cast<int>(Confidentiality::Privileged) ||
            static_cast<int>(record.decision_use) <
                static_cast<int>(DecisionUse::QuestionOnly) ||
            static_cast<int>(record.decision_use) >
                static_cast<int>(DecisionUse::Gate) ||
            static_cast<int>(record.resolution_status) <
                static_cast<int>(ResolutionStatus::NotApplicable) ||
            static_cast<int>(record.resolution_status) >
                static_cast<int>(ResolutionStatus::Resolved) ||
            static_cast<int>(record.conflict_status) <
                static_cast<int>(ConflictStatus::NotAssessed) ||
            static_cast<int>(record.conflict_status) >
                static_cast<int>(ConflictStatus::Unresolved)) {
            invariant_error("record contains an invalid enum value");
        }
        if (!is_iso_date(record.access_date) ||
            !is_iso_date(record.next_review_date)) {
            invariant_error("record access and review dates must be ISO dates");
        }
        const bool gap = record.assertion_status == AssertionStatus::Gap;
        const bool dated_source = is_iso_date(record.source_date);
        if ((!gap && !dated_source && record.source_date != "UNDATED") ||
            (gap && record.source_date != "NONE")) {
            invariant_error("record source date is invalid for its status");
        }
        if (dated_source && record.source_date > record.access_date) {
            invariant_error("record source date must not follow access date");
        }
        if (record.access_date > metadata.as_of_date) {
            invariant_error("record access date follows dossier as-of date");
        }
        if ((!gap && !is_iso_date(record.verification_date)) ||
            (gap && record.verification_date != "NONE")) {
            invariant_error("record verification date is invalid");
        }
        if (!gap && record.verification_date > metadata.as_of_date) {
            invariant_error(
                "record verification date follows dossier as-of date");
        }
        if (!gap && record.verification_date < record.access_date) {
            invariant_error(
                "record verification date precedes source access date");
        }
        if (!record.retained_copy.empty() &&
            !safe_relative_path(record.retained_copy)) {
            invariant_error("retained-copy path is not safely relative");
        }
        if (record.retained_copy.empty() !=
            (record.retained_sha256 == "NONE")) {
            invariant_error(
                "retained-copy and SHA-256 presence must agree");
        }
        if (!record.retained_copy.empty() &&
            !is_lower_hex_sha256(record.retained_sha256)) {
            invariant_error("retained SHA-256 is not lowercase hexadecimal");
        }
        const bool contrary = is_contrary(record);
        if (contrary != record.adverse_evidence) {
            invariant_error(
                "contrary status and adverse-evidence flag must agree");
        }
        if (contrary &&
            record.resolution_status == ResolutionStatus::NotApplicable) {
            invariant_error("contrary record lacks a resolution status");
        }
        if (!contrary &&
            (record.resolution_status != ResolutionStatus::NotApplicable ||
             record.resolved_by != "NONE" ||
             record.resolution_date != "NONE" ||
             record.resolution_authority != "NONE" ||
             record.resolution_basis != "NONE")) {
            invariant_error("non-contrary record has resolution metadata");
        }
        if (record.resolution_status == ResolutionStatus::Open &&
            (record.resolved_by != "NONE" ||
             record.resolution_date != "NONE" ||
             record.resolution_authority != "NONE" ||
             record.resolution_basis == "NONE")) {
            invariant_error("open adverse record has invalid resolution data");
        }
        if (record.resolution_status == ResolutionStatus::Resolved &&
            (record.resolved_by == "NONE" ||
             !is_iso_date(record.resolution_date) ||
             is_unresolved_metadata(record.resolution_authority) ||
             is_unresolved_metadata(record.resolution_basis) ||
             record.resolution_date > metadata.as_of_date ||
             record.resolution_basis.find(record.record_id) ==
                 std::string::npos)) {
            invariant_error("resolved adverse record lacks a resolution link");
        }
        if (gap &&
            (record.source_class != SourceClass::None ||
             record.verification != VerificationLevel::V0 ||
             record.source_uri != "NONE" ||
             !record.retained_copy.empty() ||
             record.retained_sha256 != "NONE" ||
             record.document_version != "NONE" ||
             record.extract_reference != "NONE" ||
             record.decision_use != DecisionUse::QuestionOnly ||
             record.verified_by != "NONE" ||
             record.verification_procedures != "NONE" ||
             record.approved_by != "NONE" ||
             record.conflict_status != ConflictStatus::NotAssessed)) {
            invariant_error("gap record does not use the gap sentinels");
        }
        if (!gap &&
            (record.source_class == SourceClass::None ||
             record.source_uri == "NONE" ||
             record.document_version == "NONE")) {
            invariant_error("non-gap record lacks source provenance");
        }
        if (metadata.status == DossierStatus::ControlledDiligence &&
            contrary &&
            (record.retained_copy.empty() ||
             record.extract_reference == "NONE")) {
            invariant_error(
                "controlled adverse history must retain source and extract");
        }
    }

    for (const EvidenceRecord& record : dossier.records) {
        if (record.resolution_status != ResolutionStatus::Resolved) {
            continue;
        }
        const auto resolution = std::find_if(
            dossier.records.begin(), dossier.records.end(),
            [&record](const EvidenceRecord& candidate) {
                return candidate.record_id == record.resolved_by;
            });
        if (resolution == dossier.records.end() ||
            resolution->requirement_id != record.requirement_id ||
            resolution->assertion_status != AssertionStatus::Supports ||
            resolution->access_date <= record.access_date ||
            resolution->verification_date < resolution->access_date ||
            record.resolution_date < resolution->verification_date) {
            invariant_error(
                "resolved adverse record must link to later reviewed support "
                "for the same requirement");
        }
    }
}

void add_unique_reason(
    std::vector<std::string>& reasons,
    std::string reason) {
    if (std::find(reasons.begin(), reasons.end(), reason) == reasons.end()) {
        reasons.push_back(std::move(reason));
    }
}

struct Sha256State {
    std::array<std::uint32_t, 8U> hash{{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U,
    }};
    std::array<std::uint8_t, 64U> buffer{};
    std::size_t buffer_size{};
    std::uint64_t total_bytes{};
};

constexpr std::array<std::uint32_t, 64U> kSha256RoundConstants{{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
}};

[[nodiscard]] constexpr std::uint32_t rotate_right(
    std::uint32_t value,
    unsigned int shift) noexcept {
    return (value >> shift) | (value << (32U - shift));
}

void sha256_transform(
    Sha256State& state,
    const std::array<std::uint8_t, 64U>& block) noexcept {
    std::array<std::uint32_t, 64U> words{};
    for (std::size_t index = 0U; index < 16U; ++index) {
        const std::size_t offset = index * 4U;
        words[index] =
            (static_cast<std::uint32_t>(block[offset]) << 24U) |
            (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
            (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
            static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
        const std::uint32_t small0 =
            rotate_right(words[index - 15U], 7U) ^
            rotate_right(words[index - 15U], 18U) ^
            (words[index - 15U] >> 3U);
        const std::uint32_t small1 =
            rotate_right(words[index - 2U], 17U) ^
            rotate_right(words[index - 2U], 19U) ^
            (words[index - 2U] >> 10U);
        words[index] = words[index - 16U] + small0 +
            words[index - 7U] + small1;
    }

    std::uint32_t a = state.hash[0U];
    std::uint32_t b = state.hash[1U];
    std::uint32_t c = state.hash[2U];
    std::uint32_t d = state.hash[3U];
    std::uint32_t e = state.hash[4U];
    std::uint32_t f = state.hash[5U];
    std::uint32_t g = state.hash[6U];
    std::uint32_t h = state.hash[7U];
    for (std::size_t index = 0U; index < words.size(); ++index) {
        const std::uint32_t large1 =
            rotate_right(e, 6U) ^ rotate_right(e, 11U) ^
            rotate_right(e, 25U);
        const std::uint32_t choose = (e & f) ^ ((~e) & g);
        const std::uint32_t temporary1 = h + large1 + choose +
            kSha256RoundConstants[index] + words[index];
        const std::uint32_t large0 =
            rotate_right(a, 2U) ^ rotate_right(a, 13U) ^
            rotate_right(a, 22U);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temporary2 = large0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    state.hash[0U] += a;
    state.hash[1U] += b;
    state.hash[2U] += c;
    state.hash[3U] += d;
    state.hash[4U] += e;
    state.hash[5U] += f;
    state.hash[6U] += g;
    state.hash[7U] += h;
}

void sha256_update(
    Sha256State& state,
    const char* data,
    std::size_t size) noexcept {
    state.total_bytes += static_cast<std::uint64_t>(size);
    for (std::size_t index = 0U; index < size; ++index) {
        state.buffer[state.buffer_size++] =
            static_cast<std::uint8_t>(
                static_cast<unsigned char>(data[index]));
        if (state.buffer_size == state.buffer.size()) {
            sha256_transform(state, state.buffer);
            state.buffer_size = 0U;
        }
    }
}

[[nodiscard]] std::string sha256_finish(Sha256State& state) {
    const std::uint64_t bit_length = state.total_bytes * 8U;
    state.buffer[state.buffer_size++] = 0x80U;
    if (state.buffer_size > 56U) {
        std::fill(
            state.buffer.begin() +
                static_cast<std::ptrdiff_t>(state.buffer_size),
            state.buffer.end(), std::uint8_t{0U});
        sha256_transform(state, state.buffer);
        state.buffer_size = 0U;
    }
    std::fill(
        state.buffer.begin() +
            static_cast<std::ptrdiff_t>(state.buffer_size),
        state.buffer.begin() + 56, std::uint8_t{0U});
    for (std::size_t index = 0U; index < 8U; ++index) {
        const unsigned int shift =
            static_cast<unsigned int>((7U - index) * 8U);
        state.buffer[56U + index] =
            static_cast<std::uint8_t>(bit_length >> shift);
    }
    sha256_transform(state, state.buffer);

    constexpr std::string_view hexadecimal = "0123456789abcdef";
    std::string digest;
    digest.reserve(64U);
    for (const std::uint32_t word : state.hash) {
        for (unsigned int shift = 28U;; shift -= 4U) {
            const std::size_t nibble =
                static_cast<std::size_t>((word >> shift) & 0x0fU);
            digest.push_back(hexadecimal[nibble]);
            if (shift == 0U) {
                break;
            }
        }
    }
    return digest;
}

[[nodiscard]] std::optional<std::filesystem::path> retained_copy_path(
    const EvidenceDossier& dossier,
    const EvidenceRecord& record) {
    if (record.retained_copy.empty()) {
        return std::nullopt;
    }
    std::error_code error;
    const std::filesystem::path base =
        std::filesystem::canonical(dossier.directory, error);
    if (error) {
        return std::nullopt;
    }
    const std::filesystem::path target = std::filesystem::canonical(
        dossier.directory / record.retained_copy, error);
    if (error || !std::filesystem::is_regular_file(target, error) || error) {
        return std::nullopt;
    }
    const std::filesystem::path relative = target.lexically_relative(base);
    if (relative.empty() || relative.is_absolute()) {
        return std::nullopt;
    }
    if (!std::none_of(
        relative.begin(), relative.end(),
        [](const std::filesystem::path& part) { return part == ".."; })) {
        return std::nullopt;
    }
    return target;
}

[[nodiscard]] std::optional<std::string> sha256_file(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    Sha256State state;
    std::array<char, 8192U> buffer{};
    while (input) {
        input.read(buffer.data(),
                   static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            sha256_update(
                state, buffer.data(), static_cast<std::size_t>(count));
        }
    }
    if (!input.eof()) {
        return std::nullopt;
    }
    return sha256_finish(state);
}

[[nodiscard]] bool retained_copy_hash_matches(
    const EvidenceDossier& dossier,
    const EvidenceRecord& record) {
    const std::optional<std::filesystem::path> retained_path =
        retained_copy_path(dossier, record);
    if (!retained_path.has_value()) {
        return false;
    }
    const std::optional<std::string> actual_hash =
        sha256_file(*retained_path);
    return actual_hash.has_value() &&
        *actual_hash == record.retained_sha256;
}

void validate_controlled_adverse_files(const EvidenceDossier& dossier) {
    if (dossier.metadata.status != DossierStatus::ControlledDiligence) {
        return;
    }
    for (const EvidenceRecord& record : dossier.records) {
        if (is_contrary(record) &&
            !retained_copy_hash_matches(dossier, record)) {
            invariant_error(
                "controlled adverse record " + record.record_id +
                " lacks a confined, hash-matching retained copy");
        }
    }
}

[[nodiscard]] bool record_qualifies(
    const EvidenceDossier& dossier,
    const EvidenceRecord& record,
    const RequirementDefinition& requirement,
    std::string_view evaluation_date,
    std::vector<std::string>& reasons) {
    bool qualifies = true;
    const auto fail = [&reasons, &qualifies, &record](std::string reason) {
        qualifies = false;
        add_unique_reason(
            reasons, record.record_id + ": " + std::move(reason));
    };

    if (record.assertion_status != AssertionStatus::Supports) {
        fail("no supporting record is designated for this requirement");
        return false;
    }
    if (dossier.metadata.status != DossierStatus::ControlledDiligence) {
        fail("dossier status permits question formation only");
    }
    if (record.decision_use != DecisionUse::Gate) {
        fail("supporting record is not authorized for gate use");
    }
    if (static_cast<int>(record.verification) <
        static_cast<int>(requirement.minimum_verification)) {
        fail("verification level is below the hard-coded minimum");
    }
    if (static_cast<int>(record.applicability) <
        static_cast<int>(requirement.minimum_applicability)) {
        fail("project applicability is below exact");
    }
    const SourceMask all_accepted_sources =
        requirement.accepted_sources |
        requirement.additional_required_sources;
    if ((all_accepted_sources & source_bit(record.source_class)) ==
        SourceMask{0U}) {
        fail("source class is not accepted for this requirement");
    }
    if (record.source_date == "UNDATED") {
        fail("source date is explicitly undated");
    }
    if (record.verification_date > evaluation_date) {
        fail("verification date follows the evaluation date");
    }
    if (record.next_review_date < evaluation_date) {
        fail("record review date is overdue");
    }
    const std::optional<std::filesystem::path> retained_path =
        retained_copy_path(dossier, record);
    if (!retained_path.has_value()) {
        fail("retained copy is missing, not regular, or outside the dossier");
    } else {
        const std::optional<std::string> actual_hash =
            sha256_file(*retained_path);
        if (!actual_hash.has_value() ||
            *actual_hash != record.retained_sha256) {
            fail("retained-copy SHA-256 does not match the manifest");
        }
    }
    if (record.extract_reference == "NONE") {
        fail("source extract reference is absent");
    }
    if (is_unresolved_metadata(record.record_owner)) {
        fail("record owner is absent or unresolved");
    }
    if (is_unresolved_metadata(record.source_uri)) {
        fail("source URI is absent or unresolved");
    }
    if (is_unresolved_metadata(record.document_version)) {
        fail("document version is absent or unresolved");
    }
    if (is_unresolved_metadata(record.extract_reference)) {
        fail("source extract reference is absent or unresolved");
    }
    if (is_unresolved_metadata(record.verified_by)) {
        fail("verifier identity is absent or unresolved");
    }
    if (is_unresolved_metadata(record.verification_procedures)) {
        fail("verification procedures are absent or unresolved");
    }
    if (is_unresolved_metadata(record.approved_by)) {
        fail("approval identity is absent or unresolved");
    }
    if (record.conflict_status == ConflictStatus::NotAssessed ||
        record.conflict_status == ConflictStatus::Unresolved) {
        fail("reviewer conflict status is not cleared");
    }
    return qualifies;
}

[[nodiscard]] bool is_unresolved_metadata(std::string_view value) {
    std::string upper(value);
    std::transform(
        upper.begin(), upper.end(), upper.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    const auto contains_marker = [&upper](std::string_view marker) {
        std::size_t position = 0U;
        while ((position = upper.find(marker, position)) !=
               std::string::npos) {
            const std::size_t end = position + marker.size();
            const bool left_boundary =
                position == 0U ||
                std::isalnum(
                    static_cast<unsigned char>(upper[position - 1U])) == 0;
            const bool right_boundary =
                end == upper.size() ||
                std::isalnum(static_cast<unsigned char>(upper[end])) == 0;
            if (left_boundary && right_boundary) {
                return true;
            }
            position = end;
        }
        return false;
    };
    return contains_marker("NONE") || contains_marker("UNKNOWN") ||
        contains_marker("TBD") || contains_marker("PENDING") ||
        contains_marker("UNRESOLVED") ||
        upper.find("NOT_ESTABLISHED") != std::string::npos ||
        upper.find("NOT ESTABLISHED") != std::string::npos ||
        upper.find("NOT_PUBLICLY_EVIDENCED") != std::string::npos ||
        upper.find("NOT PUBLICLY EVIDENCED") != std::string::npos;
}

void add_metadata_reasons(
    const DossierMetadata& metadata,
    std::string_view requirement_id,
    std::vector<std::string>& reasons) {
    const auto require_resolved = [&reasons](
                                      std::string_view field,
                                      std::string_view value) {
        if (is_unresolved_metadata(value)) {
            add_unique_reason(
                reasons,
                "dossier field " + std::string(field) +
                    " remains explicitly unresolved");
        }
    };

    if (requirement_id == "REF-LEGAL-OPERATOR") {
        require_resolved(
            "project.operator_legal_name", metadata.operator_legal_name);
    } else if (requirement_id == "REF-FINANCING-OBLIGOR-AUTHORITY") {
        require_resolved(
            "project.financing_obligor", metadata.financing_obligor);
        require_resolved(
            "project.authorized_controller", metadata.authorized_controller);
    } else if (requirement_id == "REF-FACILITY-SITE") {
        require_resolved("project.facility_name", metadata.facility_name);
        require_resolved("project.site_city", metadata.site_city);
        require_resolved("project.site_country", metadata.site_country);
    } else if (requirement_id == "REF-ASSET-PERIMETER") {
        require_resolved("project.asset_perimeter", metadata.asset_perimeter);
    } else if (requirement_id == "REF-PRODUCT-SPEC") {
        require_resolved("project.product", metadata.product);
        require_resolved("project.species", metadata.species);
    } else if (requirement_id == "REF-PROCESS-BOUNDARY") {
        require_resolved("project.process_scope", metadata.process_scope);
    } else if (requirement_id == "REF-JURISDICTION") {
        require_resolved("project.jurisdiction", metadata.jurisdiction);
    } else if (requirement_id == "REF-GOVERNING-LAW") {
        require_resolved("project.governing_law", metadata.governing_law);
    } else if (requirement_id == "REF-USE-OF-FUNDS") {
        require_resolved(
            "project.financing_use_of_funds",
            metadata.financing_use_of_funds);
        require_resolved(
            "project.reporting_currency", metadata.reporting_currency);
    } else if (requirement_id == "REF-REGULATORY-FACILITY-SCOPE") {
        require_resolved("project.facility_scope", metadata.facility_scope);
    } else if (requirement_id == "COM-EXECUTED-OFFTAKE") {
        require_resolved(
            "project.buyer_or_channel", metadata.buyer_or_channel);
    } else if (requirement_id == kClaimPopulationFrameRequirementId) {
        if (metadata.subject_kind !=
            DossierSubjectKind::ClaimPopulation) {
            add_unique_reason(reasons,
                "claim-population requirement requires a claim-population dossier subject");
        }
        require_resolved("population.authority_legal_name",
            metadata.population_authority_legal_name);
        require_resolved("population.program_or_book_id",
            metadata.population_program_or_book_id);
        require_resolved(
            "population.scope", metadata.population_scope);
        require_resolved("population.reporting_currency",
            metadata.population_reporting_currency);
        require_resolved("dossier.id", metadata.id);
        require_resolved("dossier.owner", metadata.owner);
    }
}

constexpr std::size_t kMaximumPreparedEvidenceRecords = 100000U;

struct PreparedEvidenceGateUse {
    std::unordered_map<std::string, const EvidenceRecord*> records_by_id;
    std::unordered_map<std::string, std::vector<const EvidenceRecord*>>
        records_by_requirement;
    std::unordered_map<std::string, std::size_t> assessment_positions;
    std::vector<EvidenceRecordGateUseAssessment> record_assessments;
};

[[nodiscard]] PreparedEvidenceGateUse prepare_evidence_gate_use(
    const EvidenceDossier& dossier,
    std::string_view evaluation_date) {
    if (dossier.records.size() > kMaximumPreparedEvidenceRecords) {
        throw std::invalid_argument(
            "evidence dossier exceeds the prepared assessment record limit");
    }

    PreparedEvidenceGateUse prepared;
    prepared.records_by_id.reserve(dossier.records.size());
    prepared.records_by_requirement.reserve(kRequirements.size());
    prepared.assessment_positions.reserve(dossier.records.size());
    prepared.record_assessments.reserve(dossier.records.size());

    for (const EvidenceRecord& record : dossier.records) {
        prepared.records_by_id.emplace(record.record_id, &record);
        prepared.records_by_requirement[record.requirement_id].push_back(
            &record);
    }
    for (const EvidenceRecord& record : dossier.records) {
        const RequirementDefinition* requirement =
            find_requirement(record.requirement_id);
        if (requirement == nullptr) {
            invariant_error("record has an unknown requirement ID");
        }
        EvidenceRecordGateUseAssessment assessment;
        assessment.record_id = record.record_id;
        assessment.requirement_id = record.requirement_id;
        assessment.record_qualifies = record_qualifies(
            dossier, record, *requirement, evaluation_date,
            assessment.reasons);
        prepared.assessment_positions.emplace(
            assessment.record_id, prepared.record_assessments.size());
        prepared.record_assessments.push_back(std::move(assessment));
    }
    return prepared;
}

[[nodiscard]] const EvidenceRecordGateUseAssessment*
find_record_assessment(
    const PreparedEvidenceGateUse& prepared,
    std::string_view record_id) noexcept {
    const auto found = prepared.assessment_positions.find(
        std::string(record_id));
    if (found == prepared.assessment_positions.end()) {
        return nullptr;
    }
    return &prepared.record_assessments[found->second];
}

[[nodiscard]] bool adverse_record_is_unresolved(
    const PreparedEvidenceGateUse& prepared,
    const EvidenceRecord& record) {
    if (!is_contrary(record)) {
        return false;
    }
    if (record.resolution_status == ResolutionStatus::Open) {
        return true;
    }
    const auto resolution = prepared.records_by_id.find(record.resolved_by);
    if (resolution == prepared.records_by_id.end()) {
        return true;
    }
    const EvidenceRecordGateUseAssessment* assessment =
        find_record_assessment(prepared, resolution->second->record_id);
    return assessment == nullptr || !assessment->record_qualifies;
}

[[nodiscard]] RequirementAssessment assess_requirement(
    const EvidenceDossier& dossier,
    const RequirementDefinition& requirement,
    const PreparedEvidenceGateUse& prepared) {
    RequirementAssessment result;
    result.requirement_id = requirement.id;
    result.title = requirement.title;

    bool found_record = false;
    bool found_gap = false;
    bool found_partial = false;
    bool found_primary_support = false;
    bool found_additional_support =
        requirement.additional_required_sources == SourceMask{0U};
    const auto grouped = prepared.records_by_requirement.find(
        std::string(requirement.id));
    if (grouped != prepared.records_by_requirement.end()) {
      for (const EvidenceRecord* record_pointer : grouped->second) {
        const EvidenceRecord& record = *record_pointer;
        found_record = true;
        if (record.assertion_status == AssertionStatus::Gap) {
            found_gap = true;
            result.reasons.push_back(
                record.record_id + ": manifest records an explicit gap");
        }
        if (record.assertion_status == AssertionStatus::Partial) {
            found_partial = true;
            result.reasons.push_back(
                record.record_id +
                    ": partial evidence cannot satisfy the full requirement");
        }
        if (is_contrary(record)) {
            result.adverse_evidence_present = true;
            const bool unresolved =
                adverse_record_is_unresolved(prepared, record);
            if (unresolved) {
                result.unresolved_adverse_evidence_present = true;
                if (record.resolution_status == ResolutionStatus::Open) {
                    result.reasons.push_back(
                        record.record_id +
                            ": contrary evidence remains open");
                } else {
                    result.reasons.push_back(
                        record.record_id + ": resolution target " +
                            record.resolved_by + " does not qualify");
                }
            }
        }
        if (record.assertion_status == AssertionStatus::Supports) {
            const EvidenceRecordGateUseAssessment* assessment =
                find_record_assessment(prepared, record.record_id);
            if (assessment == nullptr) {
                invariant_error("prepared record assessment is missing");
            }
            if (assessment->record_qualifies) {
                const SourceMask source = source_bit(record.source_class);
                found_primary_support =
                    found_primary_support ||
                    (requirement.accepted_sources & source) != SourceMask{0U};
                found_additional_support =
                    found_additional_support ||
                    (requirement.additional_required_sources & source) !=
                        SourceMask{0U};
            } else {
                for (const std::string& reason : assessment->reasons) {
                    result.reasons.push_back(reason);
                }
            }
        }
      }
    }

    if (!found_record) {
        add_unique_reason(result.reasons, "no manifest record");
    }
    if (!found_primary_support) {
        add_unique_reason(
            result.reasons,
            "no qualifying record from the primary source group");
    }
    if (!found_additional_support) {
        add_unique_reason(
            result.reasons,
            "no qualifying record from the required conjunctive source group");
    }
    static_cast<void>(found_gap);
    static_cast<void>(found_partial);
    const std::size_t reasons_before_subject_metadata =
        result.reasons.size();
    if (requirement.included_in_reference_gates &&
        dossier.metadata.subject_kind !=
            DossierSubjectKind::ReferenceProject) {
        add_unique_reason(result.reasons,
            "reference-project requirement requires a reference-project dossier subject");
    }
    add_metadata_reasons(
        dossier.metadata, requirement.id, result.reasons);
    const bool subject_metadata_resolved =
        result.reasons.size() == reasons_before_subject_metadata;
    result.passed = found_primary_support && found_additional_support &&
        !result.unresolved_adverse_evidence_present &&
        subject_metadata_resolved;
    if (result.passed) {
        result.reasons.clear();
    }
    return result;
}

[[nodiscard]] std::string read_bounded_evidence_file(
    const std::filesystem::path& path, std::uintmax_t maximum_bytes,
    std::string_view description) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        throw std::invalid_argument(
            std::string(description) + " is not a readable regular file: " +
            path.string());
    }
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size > maximum_bytes) {
        throw std::invalid_argument(
            std::string(description) + " exceeds its byte guardrail");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "could not open " + std::string(description) + ": " +
            path.string());
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size()) ||
        input.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error(
            std::string(description) + " changed or could not be read completely");
    }
    return bytes;
}

} // namespace

std::string sha256_bytes_lower_hex(std::string_view bytes) {
    Sha256State state;
    sha256_update(state, bytes.data(), bytes.size());
    return sha256_finish(state);
}

std::string sha256_file_lower_hex(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        throw std::invalid_argument(
            "SHA-256 target is not a readable regular file: " +
            path.string());
    }
    const std::optional<std::string> digest = sha256_file(path);
    if (!digest.has_value()) {
        throw std::invalid_argument(
            "could not read SHA-256 target: " + path.string());
    }
    return *digest;
}

namespace {

[[nodiscard]] EvidenceDossier load_evidence_dossier_snapshot(
    const std::filesystem::path& directory,
    const std::filesystem::path& dossier_label,
    const std::filesystem::path& manifest_label,
    std::string_view dossier_bytes,
    std::string_view manifest_bytes) {
    EvidenceDossier dossier;
    dossier.directory = directory;
    dossier.metadata = parse_metadata_bytes(dossier_label, dossier_bytes);
    dossier.records = parse_manifest_bytes(manifest_label, manifest_bytes);
    validate_dossier_invariants(dossier);
    validate_controlled_adverse_files(dossier);
    return dossier;
}

} // namespace

EvidenceDossier load_evidence_dossier(
    const std::filesystem::path& dossier_path,
    const std::filesystem::path& manifest_path) {
    std::error_code path_error;
    const std::filesystem::path dossier_directory =
        std::filesystem::weakly_canonical(
            std::filesystem::absolute(dossier_path).parent_path(), path_error);
    if (path_error) {
        throw std::invalid_argument("could not resolve dossier directory");
    }
    const std::filesystem::path manifest_directory =
        std::filesystem::weakly_canonical(
            std::filesystem::absolute(manifest_path).parent_path(), path_error);
    if (path_error) {
        throw std::invalid_argument("could not resolve manifest directory");
    }
    if (dossier_directory != manifest_directory) {
        throw std::invalid_argument(
            "dossier and manifest must be in the same directory");
    }

    const std::string dossier_bytes = read_bounded_evidence_file(
        dossier_path, kMaximumEvidenceDossierBytes, "evidence dossier");
    const std::string manifest_bytes = read_bounded_evidence_file(
        manifest_path, kMaximumEvidenceManifestBytes, "evidence manifest");
    return load_evidence_dossier_snapshot(dossier_directory,
        dossier_path, manifest_path, dossier_bytes, manifest_bytes);
}

EvidenceDossier load_evidence_dossier_bytes(
    const std::filesystem::path& canonical_directory,
    std::string_view dossier_bytes,
    std::string_view manifest_bytes) {
    std::error_code error;
    const std::filesystem::path directory = std::filesystem::canonical(
        canonical_directory, error);
    if (error || !std::filesystem::is_directory(directory, error) || error) {
        throw std::invalid_argument(
            "could not resolve evidence snapshot directory");
    }
    return load_evidence_dossier_snapshot(directory,
        directory / "dossier.cfg", directory / "evidence_manifest.tsv",
        dossier_bytes, manifest_bytes);
}

EvidenceAssessment assess_evidence_dossier(
    const EvidenceDossier& dossier,
    std::string_view evaluation_date) {
    EvidenceGateUseBatchAssessment batch =
        assess_evidence_gate_use_batch(dossier, evaluation_date);
    return std::move(batch.dossier_assessment);
}

EvidenceGateUseBatchAssessment assess_evidence_gate_use_batch(
    const EvidenceDossier& dossier,
    std::string_view evaluation_date) {
    if (dossier.records.size() > kMaximumPreparedEvidenceRecords) {
        throw std::invalid_argument(
            "evidence dossier exceeds the prepared assessment record limit");
    }
    validate_dossier_invariants(dossier);
    validate_controlled_adverse_files(dossier);
    if (!is_iso_date(evaluation_date)) {
        throw std::invalid_argument(
            "evaluation date must use YYYY-MM-DD");
    }
    if (dossier.metadata.as_of_date > evaluation_date) {
        throw std::invalid_argument(
            "dossier as-of date must not follow the evaluation date");
    }
    PreparedEvidenceGateUse prepared =
        prepare_evidence_gate_use(dossier, evaluation_date);
    std::vector<RequirementAssessment> requirement_assessments;
    requirement_assessments.reserve(kRequirements.size());
    for (const RequirementDefinition& requirement : kRequirements) {
        requirement_assessments.push_back(
            assess_requirement(dossier, requirement, prepared));
    }

    for (EvidenceRecordGateUseAssessment& record :
         prepared.record_assessments) {
        const RequirementDefinition* requirement =
            find_requirement(record.requirement_id);
        if (requirement == nullptr) {
            invariant_error("record has an unknown requirement ID");
        }
        const std::size_t requirement_index = static_cast<std::size_t>(
            requirement - kRequirements.data());
        const RequirementAssessment& assessment =
            requirement_assessments[requirement_index];
        record.requirement_passed = assessment.passed;
        if (!assessment.passed) {
            record.reasons.emplace_back("requirement assessment failed");
        }
    }

    EvidenceGateUseBatchAssessment result;
    EvidenceAssessment& dossier_assessment = result.dossier_assessment;
    dossier_assessment.evaluation_date = evaluation_date;
    constexpr std::array<GateKind, 4U> gate_order{{
        GateKind::ReferenceBoundary,
        GateKind::ModelCalibration,
        GateKind::CrossStructureDiligence,
        GateKind::AnimalImpact,
    }};
    for (const GateKind gate : gate_order) {
        GateAssessment gate_assessment;
        gate_assessment.gate = gate;
        for (std::size_t index = 0U; index < kRequirements.size(); ++index) {
            if (kRequirements[index].gate != gate ||
                !kRequirements[index].included_in_reference_gates) {
                continue;
            }
            ++gate_assessment.requirements_total;
            if (requirement_assessments[index].passed) {
                ++gate_assessment.requirements_met;
            }
            gate_assessment.requirements.push_back(
                std::move(requirement_assessments[index]));
        }
        gate_assessment.passed =
            gate_assessment.requirements_total > 0U &&
            gate_assessment.requirements_met ==
                gate_assessment.requirements_total;
        dossier_assessment.gates.push_back(std::move(gate_assessment));
    }
    for (std::size_t index = 0U; index < kRequirements.size(); ++index) {
        if (!kRequirements[index].included_in_reference_gates) {
            dossier_assessment.supplemental_requirements.push_back(
                std::move(requirement_assessments[index]));
        }
    }
    dossier_assessment.adverse_records =
        static_cast<std::size_t>(std::count_if(
        dossier.records.begin(), dossier.records.end(),
        [](const EvidenceRecord& record) {
            return record.adverse_evidence ||
                record.assertion_status == AssertionStatus::Contradicts ||
                record.assertion_status == AssertionStatus::Mixed;
        }));
    dossier_assessment.unresolved_adverse_records =
        static_cast<std::size_t>(std::count_if(
            dossier.records.begin(), dossier.records.end(),
            [&prepared](const EvidenceRecord& record) {
                return adverse_record_is_unresolved(prepared, record);
            }));

    if (dossier_assessment.gates[0].passed) {
        dossier_assessment.highest_allowed_use = AllowedUse::DefinedReference;
    }
    if (dossier_assessment.gates[0].passed &&
        dossier_assessment.gates[1].passed) {
        dossier_assessment.highest_allowed_use =
            AllowedUse::ControlledCalibration;
    }
    if (dossier_assessment.gates[0].passed &&
        dossier_assessment.gates[1].passed &&
        dossier_assessment.gates[2].passed) {
        dossier_assessment.highest_allowed_use =
            AllowedUse::CrossStructureDiligence;
    }
    if (all_gates_pass(dossier_assessment)) {
        dossier_assessment.highest_allowed_use = AllowedUse::ImpactReporting;
    }
    result.records = std::move(prepared.record_assessments);
    return result;
}

EvidenceRecordGateUseAssessment assess_evidence_record_gate_use(
    const EvidenceDossier& dossier,
    std::string_view record_id,
    std::string_view evaluation_date) {
    EvidenceGateUseBatchAssessment batch =
        assess_evidence_gate_use_batch(dossier, evaluation_date);
    const auto record = std::find_if(
        batch.records.begin(), batch.records.end(),
        [record_id](const EvidenceRecordGateUseAssessment& candidate) {
            return candidate.record_id == record_id;
        });
    if (record == batch.records.end()) {
        throw std::invalid_argument(
            "unknown evidence record ID: " + std::string(record_id));
    }
    return *record;
}

void print_evidence_report(
    std::ostream& output,
    const EvidenceDossier& dossier,
    const EvidenceAssessment& supplied_assessment) {
    // Re-assess rather than trusting a possibly stale result paired with a
    // later-mutated dossier. Reports are an enforcing disclosure boundary.
    const EvidenceAssessment assessment = assess_evidence_dossier(
        dossier, supplied_assessment.evaluation_date);
    output << "============================================================\n";
    if (dossier.metadata.subject_kind ==
        DossierSubjectKind::ClaimPopulation) {
        output
            << "CLAIM-POPULATION EVIDENCE READINESS — NOT A CREDIT OPINION\n"
            << "============================================================\n"
            << "Dossier: " << dossier.metadata.id << '\n'
            << "Authority: "
            << dossier.metadata.population_authority_legal_name << '\n'
            << "Program or book: "
            << dossier.metadata.population_program_or_book_id << '\n'
            << "Scope: " << dossier.metadata.population_scope << '\n'
            << "Reporting currency: "
            << dossier.metadata.population_reporting_currency << '\n';
    } else {
        output
            << "REFERENCE-PROJECT EVIDENCE READINESS — NOT A CREDIT OPINION\n"
            << "============================================================\n"
            << "Dossier: " << dossier.metadata.id << '\n'
            << "Facility: " << dossier.metadata.facility_name << " / "
            << dossier.metadata.site_city << ", "
            << dossier.metadata.site_country << '\n';
    }
    output
        << "As of: " << dossier.metadata.as_of_date << '\n'
        << "Evaluated: " << assessment.evaluation_date << '\n'
        << "Status: " << to_string(dossier.metadata.status) << '\n';
    if (dossier.metadata.subject_kind ==
        DossierSubjectKind::ClaimPopulation) {
        output
            << "Reference-project permissions: NOT APPLICABLE / NOT GRANTED\n";
    } else {
        output
            << "Highest allowed use: "
            << to_string(assessment.highest_allowed_use) << '\n';
    }
    output
        << "Execution readiness: NOT ASSESSED BY THIS GATE\n"
        << "Adverse or contrary records retained: "
        << assessment.adverse_records << '\n'
        << "Open adverse or contrary records: "
        << assessment.unresolved_adverse_records << "\n\n";

    if (dossier.metadata.subject_kind ==
        DossierSubjectKind::ClaimPopulation) {
        output << "Supplemental claim-population requirements\n";
        for (const RequirementAssessment& requirement :
             assessment.supplemental_requirements) {
            output << (requirement.passed ? "PASS  " : "FAIL  ")
                   << requirement.requirement_id << ": "
                   << requirement.title << '\n';
            for (const std::string& reason : requirement.reasons) {
                output << "      " << reason << '\n';
            }
        }
        output << '\n';
    } else {
        for (const GateAssessment& gate : assessment.gates) {
            output << (gate.passed ? "PASS  " : "FAIL  ")
                   << to_string(gate.gate) << " ("
                   << gate.requirements_met << '/' << gate.requirements_total
                   << ")\n";
            for (const RequirementAssessment& requirement :
                 gate.requirements) {
                if (requirement.passed) {
                    continue;
                }
                output << "  - " << requirement.requirement_id << ": "
                       << requirement.title;
                if (requirement.unresolved_adverse_evidence_present) {
                    output << " [UNRESOLVED ADVERSE EVIDENCE]";
                } else if (requirement.adverse_evidence_present) {
                    output << " [RESOLVED ADVERSE HISTORY]";
                }
                output << '\n';
                for (const std::string& reason : requirement.reasons) {
                    output << "      " << reason << '\n';
                }
            }
            output << '\n';
        }
    }

    output
        << "Interpretation boundary\n"
        << "  This deterministic gate checks manifest completeness, stated\n"
        << "  provenance, retained-copy confinement and SHA-256, review and\n"
        << "  approval fields, conflict status, conjunctive source groups, and\n"
        << "  hard-coded minima. It does not authenticate signatures or source\n"
        << "  truth, reproduce technical diligence, give legal advice, establish\n"
        << "  bankability,\n"
        << "  or prove financing additionality, displacement, or animal impact.\n";
    if (dossier.metadata.subject_kind ==
        DossierSubjectKind::ClaimPopulation) {
        output
            << "  A population-profile pass admits this declared frame to a\n"
            << "  separately bound analytical workflow only. It grants no project\n"
            << "  diligence, calibration, pricing, or investment authority.\n";
    } else {
        output
            << "  A pass authorizes the named analytical use only.\n";
    }
    output
        << "  This is not an investment recommendation, rating, valuation, or\n"
        << "  offering document.\n";
}

bool all_gates_pass(const EvidenceAssessment& assessment) noexcept {
    return assessment.gates.size() == 4U &&
        std::all_of(
            assessment.gates.begin(), assessment.gates.end(),
            [](const GateAssessment& gate) { return gate.passed; });
}

bool claim_population_frame_passes(
    const EvidenceAssessment& assessment) noexcept {
    const auto matching = std::count_if(
        assessment.supplemental_requirements.begin(),
        assessment.supplemental_requirements.end(),
        [](const RequirementAssessment& requirement) {
            return requirement.requirement_id ==
                kClaimPopulationFrameRequirementId;
        });
    if (matching != 1) {
        return false;
    }
    const auto requirement = std::find_if(
        assessment.supplemental_requirements.begin(),
        assessment.supplemental_requirements.end(),
        [](const RequirementAssessment& candidate) {
            return candidate.requirement_id ==
                kClaimPopulationFrameRequirementId;
        });
    return requirement != assessment.supplemental_requirements.end() &&
        requirement->passed;
}

std::string_view to_string(DossierStatus value) noexcept {
    switch (value) {
    case DossierStatus::PublicResearchOnly:
        return "public-research-only";
    case DossierStatus::ControlledDiligence:
        return "controlled-diligence";
    }
    return "unknown";
}

std::string_view to_string(DossierSubjectKind value) noexcept {
    switch (value) {
    case DossierSubjectKind::ReferenceProject:
        return "reference-project";
    case DossierSubjectKind::ClaimPopulation:
        return "claim-population";
    }
    return "unknown";
}

std::string_view to_string(GateKind value) noexcept {
    switch (value) {
    case GateKind::ReferenceBoundary:
        return "reference boundary";
    case GateKind::ModelCalibration:
        return "model calibration";
    case GateKind::CrossStructureDiligence:
        return "cross-structure diligence checklist";
    case GateKind::AnimalImpact:
        return "animal-impact claims";
    }
    return "unknown";
}

std::string_view to_string(AllowedUse value) noexcept {
    switch (value) {
    case AllowedUse::PublicResearch:
        return "PUBLIC RESEARCH / QUESTION FORMATION ONLY";
    case AllowedUse::DefinedReference:
        return "DEFINED REFERENCE / DATA ACQUISITION";
    case AllowedUse::ControlledCalibration:
        return "CONTROLLED MODEL CALIBRATION";
    case AllowedUse::CrossStructureDiligence:
        return "CROSS-STRUCTURE DILIGENCE CHECKLIST COMPLETE";
    case AllowedUse::ImpactReporting:
        return "IMPACT REPORTING ELIGIBILITY REVIEW";
    }
    return "UNKNOWN";
}

} // namespace naturalehia::cellular_finance
