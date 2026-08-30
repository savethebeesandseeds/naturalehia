// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto suffix = std::chrono::steady_clock::now()
            .time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("naturalehia-evidence-gate-" + std::to_string(suffix));
        std::filesystem::create_directory(path_);
    }

    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void write_text(
    const std::filesystem::path& path,
    std::string_view content) {
    std::ofstream output(path, std::ios::binary);
    output << content;
    if (!output) {
        throw std::runtime_error("failed to write test fixture");
    }
}

void replace_all(
    std::string& value,
    std::string_view from,
    std::string_view to) {
    std::size_t position = 0U;
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }
}

[[nodiscard]] std::string controlled_dossier() {
    return
        "dossier.schema_version=0.2.0\n"
        "dossier.id=synthetic-complete-dossier\n"
        "dossier.as_of_date=2026-08-27\n"
        "dossier.status=controlled-diligence\n"
        "dossier.owner=synthetic-test-owner\n"
        "project.operator_legal_name=Synthetic Operator LLC\n"
        "project.financing_obligor=Synthetic Project SPV LLC\n"
        "project.authorized_controller=Synthetic Authorized Controller\n"
        "project.facility_name=Synthetic Reference Facility\n"
        "project.site_city=Test City\n"
        "project.site_country=Test Country\n"
        "project.jurisdiction=Test Jurisdiction\n"
        "project.governing_law=Test Governing Law\n"
        "project.product=Synthetic cultivated product\n"
        "project.species=Synthetic species\n"
        "project.process_scope=Defined synthetic process\n"
        "project.facility_scope=Defined synthetic facility\n"
        "project.asset_perimeter=Defined synthetic asset perimeter\n"
        "project.intended_use=Evidence-gate unit test only\n"
        "project.financing_use_of_funds=Synthetic test construction only\n"
        "project.buyer_or_channel=Synthetic Buyer\n"
        "project.reporting_currency=TEST\n"
        "governance.negative_evidence_preserved=true\n"
        "governance.public_claims_not_model_calibration=true\n"
        "governance.no_bankability_claim_without_gate=true\n"
        "governance.no_animal_impact_claim_without_gate=true\n";
}

[[nodiscard]] const std::vector<std::pair<std::string_view,
                                          std::string_view>>&
requirements() {
    static const std::vector<std::pair<std::string_view, std::string_view>>
        values{
            {"REF-LEGAL-OPERATOR", "regulator"},
            {"REF-FINANCING-OBLIGOR-AUTHORITY", "public-filing"},
            {"REF-FINANCING-OBLIGOR-AUTHORITY", "legal-opinion"},
            {"REF-FACILITY-SITE", "regulator"},
            {"REF-ASSET-PERIMETER", "engineering-record"},
            {"REF-ASSET-PERIMETER", "financial-record"},
            {"REF-PRODUCT-SPEC", "engineering-record"},
            {"REF-PRODUCT-SPEC", "regulator"},
            {"REF-PROCESS-BOUNDARY", "engineering-record"},
            {"REF-PROCESS-BOUNDARY", "regulator"},
            {"REF-JURISDICTION", "regulator"},
            {"REF-GOVERNING-LAW", "legal-opinion"},
            {"REF-USE-OF-FUNDS", "financial-record"},
            {"REF-REGULATORY-FACILITY-SCOPE", "regulator"},
            {"REF-IP-RIGHTS-CONTINUITY", "court-record"},
            {"REF-IP-RIGHTS-CONTINUITY", "legal-opinion"},
            {"TEC-COMPLETE-RUN-HISTORY", "operational-record"},
            {"TEC-COMPLETE-RUN-HISTORY", "independent-report"},
            {"TEC-FAILED-RUNS", "operational-record"},
            {"TEC-FAILED-RUNS", "independent-report"},
            {"TEC-MASS-BALANCE", "operational-record"},
            {"TEC-MASS-BALANCE", "independent-report"},
            {"TEC-UTILITY-COST", "operational-record"},
            {"TEC-UTILITY-COST", "independent-report"},
            {"TEC-INDEPENDENT-REVIEW", "independent-report"},
            {"CON-DESIGN-BASIS", "engineering-record"},
            {"CON-CAPEX-QUOTES", "engineering-record"},
            {"CON-CAPEX-QUOTES", "financial-record"},
            {"CON-SCHEDULE", "engineering-record"},
            {"CON-CONTINGENCY", "engineering-record"},
            {"CON-CONTINGENCY", "independent-report"},
            {"FIN-OPERATING-LEDGER", "financial-record"},
            {"TEC-CRITICAL-SUPPLY", "executed-contract"},
            {"TEC-CRITICAL-SUPPLY", "independent-report"},
            {"CON-UTILITY-CAPACITY", "executed-contract"},
            {"CON-UTILITY-CAPACITY", "engineering-record"},
            {"TEC-OM-RESILIENCE", "operational-record"},
            {"TEC-OM-RESILIENCE", "independent-report"},
            {"FIN-INSURANCE", "executed-contract"},
            {"FIN-INSURANCE", "independent-report"},
            {"CON-DECOMMISSIONING", "engineering-record"},
            {"CON-DECOMMISSIONING", "financial-record"},
            {"REG-PRODUCT-SCOPE", "regulator"},
            {"REG-OPEN-CONDITIONS", "regulator"},
            {"FIN-FINANCING-FAILURE-DIAGNOSIS", "financial-record"},
            {"FIN-FINANCING-FAILURE-DIAGNOSIS", "independent-report"},
            {"FIN-CAPITAL-ALTERNATIVES", "capital-provider-record"},
            {"FIN-CAPITAL-ALTERNATIVES", "independent-report"},
            {"COM-EXECUTED-OFFTAKE", "executed-contract"},
            {"COM-ACCEPTANCE", "executed-contract"},
            {"COM-ACCEPTANCE", "operational-record"},
            {"COM-COUNTERPARTY-CREDIT", "financial-record"},
            {"FIN-SOURCES-USES", "financial-record"},
            {"FIN-TERM-SHEET", "financial-record"},
            {"FIN-TAX-ANALYSIS", "tax-analysis"},
            {"FIN-ACCOUNTING-ANALYSIS", "accounting-analysis"},
            {"FIN-LEGAL-ANALYSIS", "legal-opinion"},
            {"CON-COMPLETION-TEST", "executed-contract"},
            {"CON-COMPLETION-TEST", "engineering-record"},
            {"FIN-CLAIMS-RECOVERY", "court-record"},
            {"FIN-CLAIMS-RECOVERY", "legal-opinion"},
            {"FIN-RECOVERY-VALUATION", "financial-record"},
            {"FIN-RECOVERY-VALUATION", "independent-report"},
            {"FIN-SPONSOR-EQUITY", "executed-contract"},
            {"FIN-SPONSOR-EQUITY", "financial-record"},
            {"INS-MILESTONE-MECHANICS", "executed-contract"},
            {"INS-MILESTONE-MECHANICS", "engineering-record"},
            {"INS-PRICE-INDEX-GOVERNANCE", "market-data-record"},
            {"INS-PRICE-INDEX-GOVERNANCE", "independent-report"},
            {"INS-PRICE-SETTLEMENT-MECHANICS", "executed-contract"},
            {"INS-PRICE-SETTLEMENT-MECHANICS", "financial-record"},
            {"INS-SUPPORT-PROVIDER-CREDIT", "financial-record"},
            {"INS-SUPPORT-PROVIDER-CREDIT", "legal-opinion"},
            {"INS-SUPPORT-CLAIMS-PROTOCOL", "executed-contract"},
            {"INS-SUPPORT-CLAIMS-PROTOCOL", "legal-opinion"},
            {"INS-COLLATERAL-CLOSEOUT", "executed-contract"},
            {"INS-COLLATERAL-CLOSEOUT", "legal-opinion"},
            {"IMP-FINANCING-ADDITIONALITY", "financial-record"},
            {"IMP-FINANCING-ADDITIONALITY", "independent-report"},
            {"IMP-DISPLACEMENT-METHOD", "independent-report"},
            {"IMP-DISPLACEMENT-METHOD", "operational-record"},
            {"IMP-ANIMAL-INPUT-BASELINE", "operational-record"},
            {"IMP-ANIMAL-INPUT-BASELINE", "independent-report"},
            {"IMP-OBSERVED-FINANCING", "financial-record"},
            {"IMP-OBSERVED-FINANCING", "capital-provider-record"},
            {"IMP-OBSERVED-OUTPUT-SALES", "operational-record"},
            {"IMP-OBSERVED-OUTPUT-SALES", "financial-record"},
            {"IMP-BUYER-SUBSTITUTION", "counterparty-record"},
            {"IMP-BUYER-SUBSTITUTION", "financial-record"},
            {"IMP-ATTRIBUTION-RESULT", "counterparty-record"},
            {"IMP-ATTRIBUTION-RESULT", "independent-report"},
            {"IMP-OUTCOME-ASSURANCE", "independent-report"},
            {"IMP-WELFARE-CONVERSION", "operational-record"},
            {"IMP-WELFARE-CONVERSION", "independent-report"},
        };
    return values;
}

[[nodiscard]] std::string manifest_header() {
    return
        "record_id\trequirement_id\tassertion_status\tsource_class\t"
        "verification\tapplicability\tsource_date\taccess_date\t"
        "next_review_date\trecord_owner\tsource_uri\tretained_copy\t"
        "retained_sha256\tdocument_version\textract_reference\t"
        "confidentiality\tadverse_evidence\tresolution_status\tresolved_by\t"
        "resolution_date\tresolution_authority\tresolution_basis\t"
        "decision_use\tverified_by\tverification_date\t"
        "verification_procedures\tapproved_by\tconflict_status\t"
        "limitations\n";
}

[[nodiscard]] std::string complete_manifest() {
    std::ostringstream output;
    output << manifest_header();
    std::size_t index = 0U;
    for (const auto& [requirement, source_class] : requirements()) {
        output << "SYN-" << ++index << '\t' << requirement
               << "\tsupports\t" << source_class
               << "\tV3\texact\t2026-01-01\t2026-01-02\t2027-01-01\t"
                  "test-owner\tcontrolled://synthetic/"
               << index
               << "\tretained/source.txt\t"
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
                  "f20015ad\tsynthetic-v1\tsection-1\tcontrolled\tfalse\t"
                  "not-applicable\tNONE\tNONE\tNONE\tNONE\tgate\t"
                  "Synthetic Verifier\t2026-01-02\t"
                  "synthetic completeness and provenance review\t"
                  "Synthetic Approver\tnone-disclosed\t"
                  "synthetic test evidence only\n";
    }
    return output.str();
}

[[nodiscard]] std::string claim_population_dossier() {
    return
        "dossier.schema_version=0.3.0\n"
        "dossier.id=synthetic-claim-population-dossier\n"
        "dossier.as_of_date=2026-08-30\n"
        "dossier.status=controlled-diligence\n"
        "dossier.subject_kind=claim-population\n"
        "dossier.owner=synthetic-test-owner\n"
        "population.authority_legal_name=Synthetic Program Authority\n"
        "population.program_or_book_id=synthetic-partial-credit-book\n"
        "population.scope=Complete issued-or-at-risk protected claim register\n"
        "population.reporting_currency=TEST\n"
        "governance.negative_evidence_preserved=true\n"
        "governance.public_claims_not_model_calibration=true\n"
        "governance.no_bankability_claim_without_gate=true\n"
        "governance.no_animal_impact_claim_without_gate=true\n";
}

[[nodiscard]] std::string claim_population_support_row(
    std::string_view record_id,
    std::string_view source_class,
    std::string_view retained_copy,
    std::string_view source_uri) {
    std::ostringstream output;
    output
        << record_id << '\t' << cf::kClaimPopulationFrameRequirementId
        << "\tsupports\t" << source_class
        << "\tV3\texact\t2026-08-01\t2026-08-02\t2027-08-01\t"
           "synthetic-test-owner\t"
        << source_uri << '\t' << retained_copy
        << "\tba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
           "f20015ad\tsynthetic-v1\tsection-1\tcontrolled\tfalse\t"
           "not-applicable\tNONE\tNONE\tNONE\tNONE\tgate\t"
           "Synthetic Verifier\t2026-08-02\t"
           "synthetic population completeness and provenance review\t"
           "Synthetic Approver\tnone-disclosed\t"
           "synthetic test evidence only\n";
    return output.str();
}

[[nodiscard]] std::string claim_population_manifest(
    bool include_independent_review = true) {
    std::string manifest = manifest_header();
    manifest += claim_population_support_row(
        "POP-AUTHORITY", "capital-provider-record",
        "retained/population.txt", "controlled://synthetic/population");
    if (include_independent_review) {
        manifest += claim_population_support_row(
            "POP-INDEPENDENT", "independent-report",
            "retained/review.txt", "controlled://synthetic/review");
    }
    return manifest;
}

struct Fixture {
    TemporaryDirectory directory;
    std::filesystem::path dossier;
    std::filesystem::path manifest;

    Fixture()
        : dossier(directory.path() / "dossier.cfg"),
          manifest(directory.path() / "evidence_manifest.tsv") {
        std::filesystem::create_directory(directory.path() / "retained");
        write_text(
            directory.path() / "retained" / "source.txt",
            "abc");
        write_text(dossier, controlled_dossier());
        write_text(manifest, complete_manifest());
    }
};

[[nodiscard]] bool throws_invalid(
    const std::filesystem::path& dossier,
    const std::filesystem::path& manifest) {
    try {
        static_cast<void>(cf::load_evidence_dossier(dossier, manifest));
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

[[nodiscard]] bool throws_assessment(
    const cf::EvidenceDossier& dossier,
    std::string_view evaluation_date = "2026-08-27") {
    try {
        static_cast<void>(
            cf::assess_evidence_dossier(dossier, evaluation_date));
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

[[nodiscard]] bool throws_invalid_bytes(
    const std::filesystem::path& directory,
    std::string_view dossier_bytes,
    std::string_view manifest_bytes) {
    try {
        static_cast<void>(cf::load_evidence_dossier_bytes(
            directory, dossier_bytes, manifest_bytes));
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

void test_complete_controlled_dossier_passes() {
    Fixture fixture;
    const cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    const cf::EvidenceGateUseBatchAssessment batch =
        cf::assess_evidence_gate_use_batch(dossier, "2026-08-27");
    const cf::EvidenceAssessment& assessment = batch.dossier_assessment;
    check(cf::all_gates_pass(assessment),
          "complete controlled synthetic dossier should pass all gates");
    check(assessment.highest_allowed_use == cf::AllowedUse::ImpactReporting,
          "complete dossier should reach impact-reporting use");
    check(assessment.gates.size() == 4U,
          "assessment should keep four separate gates");
    const cf::EvidenceRecordGateUseAssessment record_assessment =
        cf::assess_evidence_record_gate_use(
            dossier, "SYN-1", "2026-08-27");
    const auto batch_record = std::find_if(
        batch.records.begin(), batch.records.end(),
        [](const cf::EvidenceRecordGateUseAssessment& candidate) {
            return candidate.record_id == "SYN-1";
        });
    check(batch.records.size() == dossier.records.size(),
          "batch helper must assess every manifest record exactly once");
    check(batch_record != batch.records.end(),
          "batch helper must expose the controlled positive record");
    check(record_assessment.record_qualifies,
          "public evidence helper should reuse controlled record qualification");
    check(record_assessment.requirement_passed,
          "public evidence helper should report the complete requirement pass");
    check(record_assessment.requirement_id == "REF-LEGAL-OPERATOR",
          "public evidence helper should preserve the cited requirement ID");
    if (batch_record != batch.records.end()) {
        check(batch_record->record_qualifies ==
                  record_assessment.record_qualifies &&
                  batch_record->requirement_passed ==
                      record_assessment.requirement_passed &&
                  batch_record->requirement_id ==
                      record_assessment.requirement_id &&
                  batch_record->reasons == record_assessment.reasons,
              "batch and single-record helpers must agree for controlled evidence");
    }

    std::size_t requirement_total = 0U;
    std::unordered_set<std::string> requirement_ids;
    for (const cf::GateAssessment& gate : assessment.gates) {
        requirement_total += gate.requirements_total;
        for (const cf::RequirementAssessment& requirement :
             gate.requirements) {
            requirement_ids.insert(requirement.requirement_id);
        }
        switch (gate.gate) {
        case cf::GateKind::ReferenceBoundary:
            check(gate.requirements_total == 11U,
                  "reference boundary should contain 11 requirements");
            break;
        case cf::GateKind::ModelCalibration:
            check(gate.requirements_total == 15U,
                  "model calibration should contain 15 requirements");
            break;
        case cf::GateKind::CrossStructureDiligence:
            check(gate.requirements_total == 22U,
                  "cross-structure diligence should contain 22 requirements");
            break;
        case cf::GateKind::AnimalImpact:
            check(gate.requirements_total == 9U,
                  "animal impact should contain 9 requirements");
            break;
        }
    }
    check(requirement_total == 57U,
          "compiled evidence gate should contain 57 requirements");
    check(requirement_ids.size() == 57U,
          "compiled requirement IDs should be unique");
}

void test_claim_population_requirement_is_supplemental_and_conjunctive() {
    TemporaryDirectory directory;
    std::filesystem::create_directory(directory.path() / "retained");
    write_text(directory.path() / "retained" / "population.txt", "abc");
    write_text(directory.path() / "retained" / "review.txt", "abc");
    write_text(directory.path() / "retained" / "source.txt", "abc");

    cf::EvidenceDossier dossier = cf::load_evidence_dossier_bytes(
        directory.path(), claim_population_dossier(),
        claim_population_manifest());
    check(dossier.metadata.subject_kind ==
              cf::DossierSubjectKind::ClaimPopulation,
          "claim-population schema should retain its subject kind");
    check(dossier.metadata.population_program_or_book_id ==
              "synthetic-partial-credit-book",
          "claim-population schema should retain the bound program ID");

    const cf::EvidenceGateUseBatchAssessment batch =
        cf::assess_evidence_gate_use_batch(dossier, "2026-08-30");
    check(batch.dossier_assessment.gates.size() == 4U,
          "supplemental population evidence must not add a project gate");
    constexpr std::array<std::size_t, 4U> expected_totals{{
        11U, 15U, 22U, 9U,
    }};
    for (std::size_t index = 0U;
         index < batch.dossier_assessment.gates.size(); ++index) {
        check(batch.dossier_assessment.gates[index].requirements_total ==
                  expected_totals[index],
              "supplemental population evidence must preserve project-gate totals");
    }
    check(batch.dossier_assessment.highest_allowed_use ==
              cf::AllowedUse::PublicResearch,
          "population evidence alone must not promote project analytical use");
    check(!cf::all_gates_pass(batch.dossier_assessment),
          "population evidence alone must not pass unrelated project gates");
    check(batch.dossier_assessment.supplemental_requirements.size() == 1U,
          "population assessment should expose one supplemental requirement");
    check(cf::claim_population_frame_passes(batch.dossier_assessment),
          "subject-aware population helper should recognize the passing supplemental profile");
    if (batch.dossier_assessment.supplemental_requirements.size() == 1U) {
        const cf::RequirementAssessment& population =
            batch.dossier_assessment.supplemental_requirements.front();
        check(population.requirement_id ==
                  cf::kClaimPopulationFrameRequirementId &&
                  population.passed,
              "population authority plus independent review should pass the exposed supplemental requirement");
    }

    std::string hybrid_manifest = complete_manifest();
    hybrid_manifest += claim_population_support_row(
        "POP-AUTHORITY-HYBRID", "capital-provider-record",
        "retained/population.txt", "controlled://synthetic/population");
    hybrid_manifest += claim_population_support_row(
        "POP-INDEPENDENT-HYBRID", "independent-report",
        "retained/review.txt", "controlled://synthetic/review");
    const cf::EvidenceGateUseBatchAssessment hybrid =
        cf::assess_evidence_gate_use_batch(
            cf::load_evidence_dossier_bytes(
                directory.path(), claim_population_dossier(),
                hybrid_manifest),
            "2026-08-30");
    check(std::none_of(hybrid.dossier_assessment.gates.begin(),
              hybrid.dossier_assessment.gates.end(),
              [](const cf::GateAssessment& gate) { return gate.passed; }),
          "a claim-population dossier must not pass project gates even with every project record");
    const auto hybrid_population = std::find_if(
        hybrid.records.begin(), hybrid.records.end(),
        [](const cf::EvidenceRecordGateUseAssessment& record) {
            return record.record_id == "POP-AUTHORITY-HYBRID";
        });
    check(hybrid_population != hybrid.records.end() &&
              hybrid_population->record_qualifies &&
              hybrid_population->requirement_passed,
          "subject isolation must not prevent the supplemental population requirement from passing");

    for (const std::string_view record_id :
         {std::string_view{"POP-AUTHORITY"},
          std::string_view{"POP-INDEPENDENT"}}) {
        const auto record = std::find_if(
            batch.records.begin(), batch.records.end(),
            [record_id](const cf::EvidenceRecordGateUseAssessment& candidate) {
                return candidate.record_id == record_id;
            });
        check(record != batch.records.end(),
              "population batch should expose every cited witness");
        if (record != batch.records.end()) {
            check(record->record_qualifies,
                  "each controlled population witness should qualify");
            check(record->requirement_passed,
                  "authority plus independent review should pass the population requirement");
            check(record->requirement_id ==
                      cf::kClaimPopulationFrameRequirementId,
                  "population witness should retain the compiled requirement ID");
        }
    }

    dossier.records.erase(std::remove_if(
        dossier.records.begin(), dossier.records.end(),
        [](const cf::EvidenceRecord& record) {
            return record.record_id == "POP-INDEPENDENT";
        }), dossier.records.end());
    const cf::EvidenceRecordGateUseAssessment authority_only =
        cf::assess_evidence_record_gate_use(
            dossier, "POP-AUTHORITY", "2026-08-30");
    check(authority_only.record_qualifies,
          "authority record may qualify individually without independent review");
    check(!authority_only.requirement_passed,
          "authority record alone must not establish population completeness");
    const cf::EvidenceAssessment authority_only_assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-30");
    check(authority_only_assessment.supplemental_requirements.size() == 1U &&
              !authority_only_assessment.supplemental_requirements.front().passed,
          "supplemental assessment should disclose the missing independent witness");
    check(!cf::claim_population_frame_passes(authority_only_assessment),
          "subject-aware population helper should reject an incomplete source conjunction");

    dossier = cf::load_evidence_dossier_bytes(
        directory.path(), claim_population_dossier(),
        claim_population_manifest());
    dossier.metadata.owner = "NONE";
    const cf::EvidenceAssessment unresolved_owner =
        cf::assess_evidence_dossier(dossier, "2026-08-30");
    check(!cf::claim_population_frame_passes(unresolved_owner),
          "an unresolved population-dossier owner must block population admission");

    std::ostringstream report;
    cf::print_evidence_report(
        report, cf::load_evidence_dossier_bytes(
                    directory.path(), claim_population_dossier(),
                    claim_population_manifest()),
        batch.dossier_assessment);
    check(report.str().find("CLAIM-POPULATION EVIDENCE READINESS") !=
              std::string::npos,
          "population report should name its actual analytical subject");
    check(report.str().find(
              "PASS  FIN-CLAIM-POPULATION-FRAME") != std::string::npos,
          "population report should disclose the supplemental requirement outcome");
    check(report.str().find("Reference-project permissions: NOT APPLICABLE") !=
              std::string::npos,
          "population report should not imply project-gate authorization");
    check(report.str().find(
              "Scope: Complete issued-or-at-risk protected claim register") !=
              std::string::npos,
          "population report should disclose the claimed frame scope");

    cf::EvidenceDossier stale_dossier = cf::load_evidence_dossier_bytes(
        directory.path(), claim_population_dossier(),
        claim_population_manifest());
    const cf::EvidenceAssessment stale_pass =
        cf::assess_evidence_dossier(stale_dossier, "2026-08-30");
    stale_dossier.records.erase(std::remove_if(
        stale_dossier.records.begin(), stale_dossier.records.end(),
        [](const cf::EvidenceRecord& record) {
            return record.record_id == "POP-INDEPENDENT";
        }), stale_dossier.records.end());
    std::ostringstream stale_report;
    cf::print_evidence_report(stale_report, stale_dossier, stale_pass);
    check(stale_report.str().find(
              "FAIL  FIN-CLAIM-POPULATION-FRAME") != std::string::npos &&
              stale_report.str().find(
                  "PASS  FIN-CLAIM-POPULATION-FRAME") == std::string::npos,
          "reporting must re-assess the current dossier instead of trusting a stale pass result");
}

void test_claim_population_snapshot_and_schema_fail_closed() {
    TemporaryDirectory directory;
    std::filesystem::create_directory(directory.path() / "retained");
    write_text(directory.path() / "retained" / "population.txt", "abc");
    write_text(directory.path() / "retained" / "review.txt", "abc");
    const std::filesystem::path dossier_path =
        directory.path() / "dossier.cfg";
    const std::filesystem::path manifest_path =
        directory.path() / "evidence_manifest.tsv";
    const std::string dossier_bytes = claim_population_dossier();
    const std::string manifest_bytes = claim_population_manifest();
    write_text(dossier_path, dossier_bytes);
    write_text(manifest_path, manifest_bytes);

    write_text(dossier_path, "not=a-valid-dossier\n");
    write_text(manifest_path, "not-a-valid-manifest\n");
    const cf::EvidenceDossier captured =
        cf::load_evidence_dossier_bytes(
            directory.path(), dossier_bytes, manifest_bytes);
    check(captured.metadata.id == "synthetic-claim-population-dossier" &&
              captured.records.size() == 2U,
          "byte loader must parse the supplied immutable snapshots, not later live files");
    check(throws_invalid(dossier_path, manifest_path),
          "path loader should observe and reject later invalid live files");

    std::string wrong_subject = dossier_bytes;
    replace_all(wrong_subject,
        "dossier.subject_kind=claim-population",
        "dossier.subject_kind=reference-project");
    check(throws_invalid_bytes(
              directory.path(), wrong_subject, manifest_bytes),
          "claim-population schema must reject a reference-project subject");

    std::string oversized_row = manifest_header();
    oversized_row.append(128U * 1024U + 1U, 'x');
    oversized_row.push_back('\n');
    check(throws_invalid_bytes(
              directory.path(), dossier_bytes, oversized_row),
          "manifest rows beyond the byte guardrail must fail closed");

    cf::EvidenceDossier mutated = captured;
    mutated.metadata.facility_name = "forbidden cross-schema facility";
    check(throws_assessment(mutated, "2026-08-30"),
          "claim-population dossiers must reject reference-project metadata");

    Fixture reference_fixture;
    mutated = cf::load_evidence_dossier(
        reference_fixture.dossier, reference_fixture.manifest);
    mutated.metadata.population_scope = "forbidden population scope";
    check(throws_assessment(mutated),
          "reference-project dossiers must reject claim-population metadata");
}

void test_public_status_fails_closed() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.metadata.status = cf::DossierStatus::PublicResearchOnly;
    dossier.records.front().decision_use = cf::DecisionUse::QuestionOnly;
    const cf::EvidenceGateUseBatchAssessment batch =
        cf::assess_evidence_gate_use_batch(dossier, "2026-08-27");
    const cf::EvidenceAssessment& assessment = batch.dossier_assessment;
    check(!cf::all_gates_pass(assessment),
          "public-research status should fail even with complete metadata");
    check(assessment.highest_allowed_use == cf::AllowedUse::PublicResearch,
          "public dossier should remain question formation only");

    const cf::EvidenceRecordGateUseAssessment single =
        cf::assess_evidence_record_gate_use(
            dossier, "SYN-1", "2026-08-27");
    const auto batch_record = std::find_if(
        batch.records.begin(), batch.records.end(),
        [](const cf::EvidenceRecordGateUseAssessment& candidate) {
            return candidate.record_id == "SYN-1";
        });
    check(!single.record_qualifies && !single.requirement_passed,
          "public question-only evidence must fail controlled gate use");
    check(batch_record != batch.records.end(),
          "batch helper must expose the public question-only record");
    if (batch_record != batch.records.end()) {
        check(batch_record->record_qualifies == single.record_qualifies &&
                  batch_record->requirement_passed ==
                      single.requirement_passed &&
                  batch_record->requirement_id == single.requirement_id &&
                  batch_record->reasons == single.reasons,
              "batch and single-record helpers must agree for public question-only evidence");
    }

    std::ostringstream report;
    cf::print_evidence_report(report, dossier, assessment);
    check(report.str().find("PUBLIC RESEARCH / QUESTION FORMATION ONLY") !=
              std::string::npos,
          "report should state the public-research ceiling explicitly");
}

void test_batch_reason_volume_is_linear() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    const cf::EvidenceRecord seed = dossier.records.front();
    constexpr std::size_t record_count = 2048U;
    dossier.records.clear();
    dossier.records.reserve(record_count);
    for (std::size_t index = 0U; index < record_count; ++index) {
        cf::EvidenceRecord record = seed;
        record.record_id = "BULK-PARTIAL-" + std::to_string(index);
        record.assertion_status = cf::AssertionStatus::Partial;
        dossier.records.push_back(std::move(record));
    }

    const cf::EvidenceGateUseBatchAssessment batch =
        cf::assess_evidence_gate_use_batch(dossier, "2026-08-27");
    check(batch.records.size() == record_count,
          "batch must return one result per concentrated manifest record");
    std::size_t total_record_reasons = 0U;
    std::size_t maximum_record_reasons = 0U;
    for (const cf::EvidenceRecordGateUseAssessment& record : batch.records) {
        check(!record.record_qualifies && !record.requirement_passed,
              "partial concentrated records and their requirement must fail");
        total_record_reasons += record.reasons.size();
        maximum_record_reasons =
            std::max(maximum_record_reasons, record.reasons.size());
    }
    check(maximum_record_reasons <= 2U,
          "each failed record must retain only bounded record-level reasons");
    check(total_record_reasons <= record_count * 2U,
          "aggregate record-level reason volume must remain linear");

    const cf::RequirementAssessment* concentrated_requirement = nullptr;
    for (const cf::GateAssessment& gate :
         batch.dossier_assessment.gates) {
        for (const cf::RequirementAssessment& requirement :
             gate.requirements) {
            if (requirement.requirement_id == "REF-LEGAL-OPERATOR") {
                concentrated_requirement = &requirement;
            }
        }
    }
    check(concentrated_requirement != nullptr,
          "concentrated failed requirement must remain in dossier results");
    if (concentrated_requirement != nullptr) {
        check(!concentrated_requirement->passed,
              "concentrated partial evidence must not pass its requirement");
        check(concentrated_requirement->reasons.size() >= record_count &&
                  concentrated_requirement->reasons.size() <=
                      record_count + 2U,
              "requirement diagnostics must be stored once with linear volume");
    }

    const cf::EvidenceRecordGateUseAssessment single =
        cf::assess_evidence_record_gate_use(
            dossier, "BULK-PARTIAL-0", "2026-08-27");
    const auto batch_record = std::find_if(
        batch.records.begin(), batch.records.end(),
        [](const cf::EvidenceRecordGateUseAssessment& candidate) {
            return candidate.record_id == "BULK-PARTIAL-0";
        });
    check(batch_record != batch.records.end(),
          "concentrated batch must expose the compared record");
    if (batch_record != batch.records.end()) {
        check(batch_record->record_qualifies == single.record_qualifies &&
                  batch_record->requirement_passed ==
                      single.requirement_passed &&
                  batch_record->requirement_id == single.requirement_id &&
                  batch_record->reasons == single.reasons,
              "batch and single outcomes must agree under concentration");
    }
}

void test_adverse_evidence_blocks_requirement() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    cf::EvidenceRecord adverse = dossier.records.front();
    adverse.record_id = "SYN-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Mixed;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Open;
    adverse.resolved_by = "NONE";
    adverse.resolution_date = "NONE";
    adverse.resolution_authority = "NONE";
    adverse.resolution_basis = "OPEN - unresolved synthetic conflict";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));

    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().passed,
          "unresolved adverse evidence should block its gate");
    check(assessment.adverse_records == 1U,
          "adverse record should remain visible in the assessment");
    check(assessment.gates.front().requirements.front()
              .adverse_evidence_present,
          "affected requirement should be marked adverse");
    check(assessment.unresolved_adverse_records == 1U,
          "open adverse record should be counted as unresolved");

    dossier.records.back().retained_copy = "retained/missing-adverse.txt";
    check(throws_assessment(dossier),
          "controlled adverse history must retain a hash-matching source");
}

void test_resolved_adverse_history_is_preserved() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    cf::EvidenceRecord resolution = dossier.records.front();
    resolution.record_id = "SYN-RESOLUTION";
    resolution.source_date = "2026-01-03";
    resolution.access_date = "2026-01-04";
    resolution.verification_date = "2026-01-04";
    cf::EvidenceRecord adverse = dossier.records.front();
    adverse.record_id = "SYN-RESOLVED-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Contradicts;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Resolved;
    adverse.resolved_by = resolution.record_id;
    adverse.resolution_date = "2026-01-05";
    adverse.resolution_authority = "Synthetic Resolution Reviewer";
    adverse.resolution_basis =
        "SYN-RESOLVED-ADVERSE addressed by updated synthetic evidence";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));
    dossier.records.push_back(std::move(resolution));

    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(cf::all_gates_pass(assessment),
          "qualifying linked evidence should resolve an adverse record");
    check(assessment.adverse_records == 1U,
          "resolved adverse history must remain counted");
    check(assessment.unresolved_adverse_records == 0U,
          "resolved adverse history must not remain open");
}

void test_stale_or_missing_copy_fails() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().next_review_date = "2025-12-31";
    dossier.records[1].retained_copy = "retained/missing.txt";
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().passed,
          "stale evidence and missing retained copies should fail closed");
    check(assessment.gates.front().requirements_met == 9U,
          "two failed reference records should reduce the met count");
}

void test_unknown_and_duplicate_records_are_rejected() {
    Fixture fixture;
    const std::string unknown =
        manifest_header() +
        "X\tUNKNOWN-REQ\tgap\tnone\tV0\tillustrative\tNONE\t"
        "2026-08-27\t2027-01-01\towner\tNONE\tNONE\tNONE\tNONE\tNONE\t"
        "public\tfalse\tnot-applicable\tNONE\tNONE\tNONE\tNONE\t"
        "question-only\tNONE\tNONE\tNONE\tNONE\tnot-assessed\t"
        "intentional test gap\n";
    write_text(fixture.manifest, unknown);
    check(throws_invalid(fixture.dossier, fixture.manifest),
          "unknown requirement IDs should be rejected");

    const std::string row =
        "DUP\tREF-LEGAL-OPERATOR\tgap\tnone\tV0\tillustrative\tNONE\t"
        "2026-08-27\t2027-01-01\towner\tNONE\tNONE\tNONE\tNONE\tNONE\t"
        "public\tfalse\tnot-applicable\tNONE\tNONE\tNONE\tNONE\t"
        "question-only\tNONE\tNONE\tNONE\tNONE\tnot-assessed\t"
        "intentional test gap\n";
    write_text(fixture.manifest, manifest_header() + row + row);
    check(throws_invalid(fixture.dossier, fixture.manifest),
          "duplicate record IDs should be rejected");
}

void test_conjunctive_source_group_is_required() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    const auto record = std::find_if(
        dossier.records.begin(), dossier.records.end(),
        [](const cf::EvidenceRecord& candidate) {
            return candidate.requirement_id ==
                       "REF-FINANCING-OBLIGOR-AUTHORITY" &&
                candidate.source_class == cf::SourceClass::LegalOpinion;
        });
    check(record != dossier.records.end(),
          "synthetic fixture should contain conjunctive legal evidence");
    if (record != dossier.records.end()) {
        dossier.records.erase(record);
    }
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().passed,
          "one source group must not satisfy a conjunctive requirement");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    const auto primary = std::find_if(
        dossier.records.begin(), dossier.records.end(),
        [](const cf::EvidenceRecord& candidate) {
            return candidate.requirement_id ==
                       "REF-FINANCING-OBLIGOR-AUTHORITY" &&
                candidate.source_class == cf::SourceClass::PublicFiling;
        });
    check(primary != dossier.records.end(),
          "synthetic fixture should contain primary authority evidence");
    if (primary != dossier.records.end()) {
        dossier.records.erase(primary);
    }
    check(!cf::assess_evidence_dossier(dossier, "2026-08-27")
               .gates.front().passed,
          "the conjunctive group alone must not satisfy a requirement");
}

void test_unresolved_metadata_blocks_promotion() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.metadata.operator_legal_name =
        "NOT_ESTABLISHED - historical announcement only";
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().passed,
          "explicitly unresolved metadata must block a gate pass");
    check(!assessment.gates.front().requirements.front().passed,
          "operator requirement should expose unresolved metadata");
}

void test_assessor_revalidates_mutated_dossier() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.metadata.schema_version = "999.0";
    check(throws_assessment(dossier),
          "assessor must reject a mutated schema version");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.metadata.negative_evidence_preserved = false;
    check(throws_assessment(dossier),
          "assessor must revalidate governance commitments");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().retained_copy =
        fixture.directory.path() / "retained" / "source.txt";
    check(throws_assessment(dossier),
          "assessor must reject an absolute retained-copy path");
}

void test_date_controls_and_crlf_portability() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().source_date = "UNDATED";
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().requirements.front().passed,
          "an undated source must not qualify for gate use");
    check(throws_assessment(dossier, "2026-08-26"),
          "evaluation date must not precede dossier as-of date");

    std::string future_source = complete_manifest();
    const std::string dated = "2026-01-01\t2026-01-02";
    future_source.replace(
        future_source.find(dated), dated.size(),
        "2026-01-03\t2026-01-02");
    write_text(fixture.manifest, future_source);
    check(throws_invalid(fixture.dossier, fixture.manifest),
          "source date after access date must be rejected");

    std::string crlf = complete_manifest();
    std::size_t position = 0U;
    while ((position = crlf.find('\n', position)) != std::string::npos) {
        crlf.replace(position, 1U, "\r\n");
        position += 2U;
    }
    write_text(fixture.manifest, crlf);
    const cf::EvidenceDossier crlf_dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    check(cf::all_gates_pass(
              cf::assess_evidence_dossier(crlf_dossier, "2026-08-27")),
          "CRLF manifests should load portably");
}

void test_hash_mismatch_fails_closed() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().retained_sha256 =
        "aa7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
        "f20015ad";
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().requirements.front().passed,
          "a retained-copy hash mismatch must fail closed");
}

void test_sha256_known_vectors() {
    constexpr std::string_view abc_hash =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
        "f20015ad";

    Fixture empty_fixture;
    write_text(
        empty_fixture.directory.path() / "retained" / "source.txt", "");
    std::string empty_manifest = complete_manifest();
    replace_all(
        empty_manifest, abc_hash,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b"
        "7852b855");
    write_text(empty_fixture.manifest, empty_manifest);
    const cf::EvidenceDossier empty_dossier = cf::load_evidence_dossier(
        empty_fixture.dossier, empty_fixture.manifest);
    check(cf::all_gates_pass(
              cf::assess_evidence_dossier(empty_dossier, "2026-08-27")),
          "SHA-256 empty-file vector should verify");

    Fixture boundary_fixture;
    write_text(
        boundary_fixture.directory.path() / "retained" / "source.txt",
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq");
    std::string boundary_manifest = complete_manifest();
    replace_all(
        boundary_manifest, abc_hash,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd"
        "419db06c1");
    write_text(boundary_fixture.manifest, boundary_manifest);
    const cf::EvidenceDossier boundary_dossier =
        cf::load_evidence_dossier(
            boundary_fixture.dossier, boundary_fixture.manifest);
    check(cf::all_gates_pass(cf::assess_evidence_dossier(
              boundary_dossier, "2026-08-27")),
          "SHA-256 56-byte padding-boundary vector should verify");

    Fixture streaming_fixture;
    write_text(
        streaming_fixture.directory.path() / "retained" / "source.txt",
        std::string(1'000'000U, 'a'));
    std::string streaming_manifest = complete_manifest();
    replace_all(
        streaming_manifest, abc_hash,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39cc"
        "c7112cd0");
    write_text(streaming_fixture.manifest, streaming_manifest);
    const cf::EvidenceDossier streaming_dossier =
        cf::load_evidence_dossier(
            streaming_fixture.dossier, streaming_fixture.manifest);
    check(cf::all_gates_pass(cf::assess_evidence_dossier(
              streaming_dossier, "2026-08-27")),
          "SHA-256 streaming vector should verify");
}

void test_unresolved_record_provenance_fails_closed() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().record_owner =
        "UNKNOWN - awaiting verification";
    check(!cf::assess_evidence_dossier(dossier, "2026-08-27")
               .gates.front().requirements.front().passed,
          "UNKNOWN record ownership must not qualify");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().verification_procedures = "TBD_EXTERNAL_REVIEW";
    check(!cf::assess_evidence_dossier(dossier, "2026-08-27")
               .gates.front().requirements.front().passed,
          "TBD verification procedures must not qualify");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().document_version = "PENDING";
    check(!cf::assess_evidence_dossier(dossier, "2026-08-27")
               .gates.front().requirements.front().passed,
          "PENDING document versions must not qualify");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().approved_by = "UNRESOLVED";
    check(!cf::assess_evidence_dossier(dossier, "2026-08-27")
               .gates.front().requirements.front().passed,
          "UNRESOLVED approval identities must not qualify");
}

void test_resolution_chronology_and_effectiveness() {
    Fixture fixture;
    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    cf::EvidenceRecord adverse = dossier.records.front();
    adverse.record_id = "SYN-OLDER-TARGET-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Mixed;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Resolved;
    adverse.resolved_by = dossier.records.front().record_id;
    adverse.resolution_date = "2026-01-03";
    adverse.resolution_authority = "Synthetic Resolution Reviewer";
    adverse.resolution_basis =
        "SYN-OLDER-TARGET-ADVERSE incorrectly uses older support";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));
    check(throws_assessment(dossier),
          "older support must not resolve later adverse evidence");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    cf::EvidenceRecord resolution = dossier.records.front();
    resolution.record_id = "SYN-STALE-RESOLUTION";
    resolution.source_date = "2026-01-03";
    resolution.access_date = "2026-01-04";
    resolution.verification_date = "2026-01-04";
    resolution.next_review_date = "2026-01-05";
    adverse = dossier.records.front();
    adverse.record_id = "SYN-STALE-TARGET-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Contradicts;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Resolved;
    adverse.resolved_by = resolution.record_id;
    adverse.resolution_date = "2026-01-05";
    adverse.resolution_authority = "Synthetic Resolution Reviewer";
    adverse.resolution_basis =
        "SYN-STALE-TARGET-ADVERSE addressed by a stale review";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));
    dossier.records.push_back(std::move(resolution));
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().passed,
          "a stale resolution target must not clear adverse evidence");
    check(assessment.unresolved_adverse_records == 1U,
          "ineffective resolved status must be counted as unresolved");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    const auto unrelated = std::find_if(
        dossier.records.begin(), dossier.records.end(),
        [](const cf::EvidenceRecord& record) {
            return record.requirement_id == "REF-FACILITY-SITE";
        });
    check(unrelated != dossier.records.end(),
          "fixture should contain an unrelated support record");
    if (unrelated != dossier.records.end()) {
        adverse = dossier.records.front();
        adverse.record_id = "SYN-CROSS-REQUIREMENT-ADVERSE";
        adverse.assertion_status = cf::AssertionStatus::Mixed;
        adverse.adverse_evidence = true;
        adverse.resolution_status = cf::ResolutionStatus::Resolved;
        adverse.resolved_by = unrelated->record_id;
        adverse.resolution_date = "2026-01-05";
        adverse.resolution_authority = "Synthetic Resolution Reviewer";
        adverse.resolution_basis =
            "SYN-CROSS-REQUIREMENT-ADVERSE uses unrelated support";
        adverse.decision_use = cf::DecisionUse::Corroboration;
        dossier.records.push_back(std::move(adverse));
        check(throws_assessment(dossier),
              "cross-requirement resolution links must be rejected");
    }

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    resolution = dossier.records.front();
    resolution.record_id = "SYN-FUTURE-RESOLUTION";
    resolution.source_date = "2026-01-03";
    resolution.access_date = "2026-01-04";
    resolution.verification_date = "2026-01-04";
    adverse = dossier.records.front();
    adverse.record_id = "SYN-FUTURE-DATED-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Contradicts;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Resolved;
    adverse.resolved_by = resolution.record_id;
    adverse.resolution_date = "2099-01-01";
    adverse.resolution_authority = "Synthetic Resolution Reviewer";
    adverse.resolution_basis =
        "SYN-FUTURE-DATED-ADVERSE cannot be resolved in the future";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));
    dossier.records.push_back(std::move(resolution));
    check(throws_assessment(dossier),
          "resolution date after the dossier as-of date must be rejected");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    resolution = dossier.records.front();
    resolution.record_id = "SYN-PREMATURE-RESOLUTION";
    resolution.source_date = "2026-01-03";
    resolution.access_date = "2026-01-04";
    resolution.verification_date = "2026-01-05";
    adverse = dossier.records.front();
    adverse.record_id = "SYN-PREMATURE-DATED-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Mixed;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Resolved;
    adverse.resolved_by = resolution.record_id;
    adverse.resolution_date = "2026-01-04";
    adverse.resolution_authority = "Synthetic Resolution Reviewer";
    adverse.resolution_basis =
        "SYN-PREMATURE-DATED-ADVERSE predates completed verification";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));
    dossier.records.push_back(std::move(resolution));
    check(throws_assessment(dossier),
          "resolution date before target verification must be rejected");

    dossier = cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    resolution = dossier.records.front();
    resolution.record_id = "SYN-UNRESOLVED-AUTHORITY";
    resolution.source_date = "2026-01-03";
    resolution.access_date = "2026-01-04";
    resolution.verification_date = "2026-01-04";
    adverse = dossier.records.front();
    adverse.record_id = "SYN-UNKNOWN-AUTHORITY-ADVERSE";
    adverse.assertion_status = cf::AssertionStatus::Contradicts;
    adverse.adverse_evidence = true;
    adverse.resolution_status = cf::ResolutionStatus::Resolved;
    adverse.resolved_by = resolution.record_id;
    adverse.resolution_date = "2026-01-05";
    adverse.resolution_authority = "UNKNOWN - awaiting verification";
    adverse.resolution_basis =
        "SYN-UNKNOWN-AUTHORITY-ADVERSE lacks a resolution authority";
    adverse.decision_use = cf::DecisionUse::Corroboration;
    dossier.records.push_back(std::move(adverse));
    dossier.records.push_back(std::move(resolution));
    check(throws_assessment(dossier),
          "unresolved resolution authority must be rejected");
}

void test_symlink_escape_does_not_qualify() {
    Fixture fixture;
    TemporaryDirectory outside;
    write_text(outside.path() / "outside.txt", "outside dossier\n");
    const std::filesystem::path link =
        fixture.directory.path() / "retained" / "escape.txt";
    std::error_code error;
    std::filesystem::create_symlink(
        outside.path() / "outside.txt", link, error);
    if (error) {
        return;
    }

    cf::EvidenceDossier dossier =
        cf::load_evidence_dossier(fixture.dossier, fixture.manifest);
    dossier.records.front().retained_copy = "retained/escape.txt";
    const cf::EvidenceAssessment assessment =
        cf::assess_evidence_dossier(dossier, "2026-08-27");
    check(!assessment.gates.front().requirements.front().passed,
          "a symlink outside the dossier must not qualify");
}

void test_false_governance_commitment_is_rejected() {
    Fixture fixture;
    std::string dossier = controlled_dossier();
    const std::string expected =
        "governance.negative_evidence_preserved=true";
    dossier.replace(
        dossier.find(expected), expected.size(),
        "governance.negative_evidence_preserved=false");
    write_text(fixture.dossier, dossier);
    check(throws_invalid(fixture.dossier, fixture.manifest),
          "governance commitments should not be optional");
}

} // namespace

int main() {
    try {
        test_complete_controlled_dossier_passes();
        test_claim_population_requirement_is_supplemental_and_conjunctive();
        test_claim_population_snapshot_and_schema_fail_closed();
        test_public_status_fails_closed();
        test_batch_reason_volume_is_linear();
        test_adverse_evidence_blocks_requirement();
        test_resolved_adverse_history_is_preserved();
        test_stale_or_missing_copy_fails();
        test_unknown_and_duplicate_records_are_rejected();
        test_false_governance_commitment_is_rejected();
        test_conjunctive_source_group_is_required();
        test_unresolved_metadata_blocks_promotion();
        test_assessor_revalidates_mutated_dossier();
        test_date_controls_and_crlf_portability();
        test_hash_mismatch_fails_closed();
        test_sha256_known_vectors();
        test_unresolved_record_provenance_fails_closed();
        test_resolution_chronology_and_effectiveness();
        test_symlink_escape_does_not_qualify();
    } catch (const std::exception& error) {
        std::cerr << "UNEXPECTED EXCEPTION: " << error.what() << '\n';
        return 1;
    }
    if (failures != 0) {
        std::cerr << failures << " evidence-gate test(s) failed\n";
        return 1;
    }
    std::cout << "All evidence-gate tests passed\n";
    return 0;
}
