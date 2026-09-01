// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/evidence_gate.hpp>
#include <naturalehia/cellular_finance/partial_credit_claim_loss_cohort_config.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace cf = naturalehia::cellular_finance;

static_assert(!std::is_aggregate_v<
    cf::PartialCreditClaimLossCohortPackage>);

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename Function>
void expect_invalid(Function&& function, std::string_view message) {
    try {
        std::invoke(std::forward<Function>(function));
        check(false, message);
    } catch (const std::invalid_argument&) {
        check(true, message);
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << message << " (wrong exception: "
                  << error.what() << ")\n";
        ++failures;
    }
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not read test file");
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("could not write test file");
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) throw std::runtime_error("could not finish test file");
}

void replace_once(std::string& text, std::string_view old_value,
                  std::string_view new_value) {
    const std::size_t position = text.find(old_value);
    if (position == std::string::npos) {
        throw std::runtime_error("test replacement target not found");
    }
    text.replace(position, old_value.size(), new_value);
}

void replace_key(std::string& text, std::string_view key,
                 std::string_view value) {
    const std::string prefix = std::string(key) + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test key not found");
    }
    const std::size_t begin = position + prefix.size();
    const std::size_t end = text.find('\n', begin);
    text.replace(begin,
        end == std::string::npos ? std::string::npos : end - begin, value);
}

void remove_key(std::string& text, std::string_view key) {
    const std::string prefix = std::string(key) + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test key not found");
    }
    const std::size_t end = text.find('\n', position);
    text.erase(position,
        end == std::string::npos ? std::string::npos : end - position + 1U);
}

struct TempTree {
    std::filesystem::path root{};
    ~TempTree() noexcept {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
};

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

[[nodiscard]] std::string evidence_row(
    std::string_view id, std::string_view source_class,
    std::string_view retained, std::string_view uri) {
    std::ostringstream output;
    output << id << '\t' << cf::kClaimPopulationFrameRequirementId
           << "\tsupports\t" << source_class
           << "\tV3\texact\t2027-01-01\t2027-01-02\t2028-01-01\t"
              "synthetic-owner\t"
           << uri << '\t' << retained
           << "\tba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
              "f20015ad\tsynthetic-v1\tsection-1\tcontrolled\tfalse\t"
              "not-applicable\tNONE\tNONE\tNONE\tNONE\tgate\t"
              "Synthetic Verifier\t2027-01-02\t"
              "synthetic population register review\tSynthetic Approver\t"
              "none-disclosed\tsynthetic test evidence only\n";
    return output.str();
}

[[nodiscard]] std::string non_supporting_retained_evidence_row() {
    return
        "POP-CONTEXT\tFIN-CLAIM-POPULATION-FRAME\tpartial\t"
        "operator-disclosure\tV1\texact\t2027-01-01\t2027-01-02\t"
        "2028-01-01\tsynthetic-owner\tcontrolled://synthetic/context\t"
        "retained/context.txt\t"
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61"
        "f20015ad\tsynthetic-v1\tsection-1\tcontrolled\tfalse\t"
        "not-applicable\tNONE\tNONE\tNONE\tNONE\tquestion-only\t"
        "Synthetic Verifier\t2027-01-02\t"
        "synthetic contextual review\tSynthetic Approver\t"
        "none-disclosed\tnon-supporting retained-copy regression only\n";
}

[[nodiscard]] std::string manifest(bool independent = true) {
    std::string result = manifest_header();
    result += evidence_row("POP-AUTHORITY", "capital-provider-record",
        "retained/population.txt", "controlled://synthetic/population");
    if (independent) {
        result += evidence_row("POP-INDEPENDENT", "independent-report",
            "retained/review.txt", "controlled://synthetic/review");
    }
    return result;
}

[[nodiscard]] std::string dossier() {
    return
        "dossier.schema_version=0.3.0\n"
        "dossier.id=synthetic-cohort-dossier\n"
        "dossier.as_of_date=2027-02-01\n"
        "dossier.status=controlled-diligence\n"
        "dossier.subject_kind=claim-population\n"
        "dossier.owner=synthetic-owner\n"
        "population.authority_legal_name=Synthetic Program Authority\n"
        "population.program_or_book_id=synthetic-partial-credit-book\n"
        "population.scope=Complete synthetic protected-claim register\n"
        "population.reporting_currency=TEST\n"
        "governance.negative_evidence_preserved=true\n"
        "governance.public_claims_not_model_calibration=true\n"
        "governance.no_bankability_claim_without_gate=true\n"
        "governance.no_animal_impact_claim_without_gate=true\n";
}

[[nodiscard]] std::string methods(
    std::string_view cited_records = "POP-AUTHORITY,POP-INDEPENDENT") {
    struct MethodRow {
        std::string_view id;
        std::string_view purpose;
    };
    constexpr MethodRow rows[] = {
        {"population-v1", "population"},
        {"sampling-unit-v1", "sampling-unit"},
        {"cluster-v1", "cluster"},
        {"term-stratum-v1", "term-stratum"},
        {"horizon-v1", "horizon"},
        {"loss-v1", "loss"},
        {"resolution-v1", "resolution"},
        {"censoring-v1", "censoring"},
        {"denominator-v1", "denominator"},
        {"monetary-basis-v1", "monetary-basis"},
        {"amount-bound-v1", "amount-bound"},
        {"metric-v1", "metric"},
    };
    std::ostringstream output;
    for (const MethodRow& row : rows) {
        const std::string prefix = "method." + std::string(row.id) + '.';
        output << prefix << "purpose=" << row.purpose << '\n'
               << prefix << "version=1.0.0\n"
               << prefix << "implementation_id="
               << cf::kPartialCreditClaimLossCohortMechanicalImplementationId
               << '\n'
               << prefix << "effective_date=2026-01-01\n"
               << prefix
               << "definition=Deterministic synthetic fixture rule only\n"
               << prefix << "inputs=claim_cfg_path\n"
               << prefix << "output=mechanical_result\n"
               << prefix << "evidence_record_ids=" << cited_records << '\n'
               << prefix << "evidence_requirement_ids="
               << cf::kClaimPopulationFrameRequirementId << '\n';
    }
    return output.str();
}

[[nodiscard]] std::string observations(
    std::string_view claim_hash,
    std::string_view cited_records = "POP-AUTHORITY,POP-INDEPENDENT") {
    std::ostringstream output;
    output
        << "observation_id\teconomic_cluster_id\teligible_date\t"
           "horizon_end_date\tdisposition\ttrigger_status\ttrigger_date\t"
           "classification_date\tresolution_date\texclusion_rule_id\t"
           "claim_cfg_path\tclaim_config_sha256\trealized_scenario_id\t"
           "provider_claim_id\tpopulation_evidence_record_ids\t"
           "population_requirement_ids\tclassification_evidence_record_ids\t"
           "classification_requirement_ids\n"
        << "observation-1\tone-claim-provider-supported-synthetic-cluster\t"
           "2026-01-01\t2027-01-01\tresolved\tnot-triggered\tNONE\t"
           "2027-01-01\t2027-01-01\tNONE\tclaims/one/claim.cfg\t"
        << claim_hash
        << "\tperforming-maturity\tsynthetic-provider-claim\t"
        << cited_records << '\t'
        << cf::kClaimPopulationFrameRequirementId << '\t' << cited_records
        << '\t' << cf::kClaimPopulationFrameRequirementId << '\n';
    return output.str();
}

[[nodiscard]] std::string cohort_config(
    const std::filesystem::path& directory) {
    std::ostringstream output;
    output
        << "schema_version=partial-credit-claim-loss-cohort-binder-v0.1\n"
           "cohort_id=synthetic-partial-credit-book\n"
           "as_of_date=2027-02-01\n"
           "frame_start_date=2026-01-01\n"
           "frame_end_date=2026-01-01\n"
           "population_definition=population-v1\n"
           "source_note=Synthetic parser and hash regression only\n"
           "sampling_unit_definition=sampling-unit-v1\n"
           "economic_cluster_definition=cluster-v1\n"
           "protection_term_stratum_definition=term-stratum-v1\n"
           "outcome_horizon_definition=horizon-v1\n"
           "loss_definition=loss-v1\n"
           "resolution_definition=resolution-v1\n"
           "censoring_definition=censoring-v1\n"
           "denominator_definition=denominator-v1\n"
           "currency_label=TEST\n"
           "monetary_basis=nominal synthetic millions at 2026-01-01\n"
           "monetary_basis_definition=monetary-basis-v1\n"
           "population_frame_count=1\n"
           "candidate_only=true\n"
           "observations_path=observations.tsv\n"
           "observations_sha256="
        << cf::sha256_file_lower_hex(directory / "observations.tsv") << '\n'
        << "methods_path=methods.cfg\nmethods_sha256="
        << cf::sha256_file_lower_hex(directory / "methods.cfg") << '\n'
        << "dossier_path=dossier.cfg\ndossier_sha256="
        << cf::sha256_file_lower_hex(directory / "dossier.cfg") << '\n'
        << "evidence_manifest_path=evidence_manifest.tsv\n"
           "evidence_manifest_sha256="
        << cf::sha256_file_lower_hex(directory / "evidence_manifest.tsv")
        << '\n';
    return output.str();
}

[[nodiscard]] std::filesystem::path make_valid_package(
    const std::filesystem::path& fixture_config,
    const std::filesystem::path& workspace, std::string_view name) {
    const std::filesystem::path root = workspace / name;
    std::filesystem::create_directories(root / "claims");
    std::filesystem::copy(fixture_config.parent_path(), root / "claims" / "one",
        std::filesystem::copy_options::recursive);
    std::filesystem::create_directories(root / "retained");
    write_text(root / "retained" / "population.txt", "abc");
    write_text(root / "retained" / "review.txt", "abc");
    write_text(root / "dossier.cfg", dossier());
    write_text(root / "evidence_manifest.tsv", manifest());
    write_text(root / "methods.cfg", methods());
    write_text(root / "observations.tsv", observations(
        cf::sha256_file_lower_hex(root / "claims" / "one" / "claim.cfg")));
    write_text(root / "cohort.cfg", cohort_config(root));
    return root;
}

[[nodiscard]] std::filesystem::path copy_package(
    const std::filesystem::path& source,
    const std::filesystem::path& workspace, std::string_view name) {
    const std::filesystem::path target = workspace / name;
    std::filesystem::copy(source, target,
        std::filesystem::copy_options::recursive);
    return target;
}

void rehash(std::filesystem::path root, std::string_view key,
            std::string_view file) {
    std::string config = read_text(root / "cohort.cfg");
    replace_key(config, key, cf::sha256_file_lower_hex(
        root / std::filesystem::path(file)));
    write_text(root / "cohort.cfg", config);
}

void test_valid(const std::filesystem::path& root) {
    const cf::PartialCreditClaimLossCohortPackage package =
        cf::load_partial_credit_claim_loss_cohort_package(root);
    check(package.five_file_integrity_verified() &&
            package.population_frame_evidence_passed() &&
            package.candidate_package_valid() &&
            !package.empirical_realized_cash_admissible() &&
            package.methods.size() == 12U &&
            package.observations.size() == 1U &&
            package.admission_blockers.size() == 2U,
        "valid five-file package is hash-bound but remains empirically inadmissible");
    const cf::PartialCreditClaimLossCohortEvaluation evaluation =
        cf::evaluate_partial_credit_claim_loss_cohort(package);
    check(evaluation.five_file_integrity_verified &&
            evaluation.population_frame_evidence_passed &&
            evaluation.candidate_package_valid &&
            !evaluation.empirical_realized_cash_admissible &&
            !evaluation.calibrated_execution_authorized &&
            !evaluation.portfolio_export_authorized,
        "evaluation retains structural provenance without granting probability or Portfolio authority");

    cf::PartialCreditClaimLossCohortPackage caller_constructed;
    caller_constructed.config = package.config;
    caller_constructed.observations = package.observations;
    caller_constructed.admission_blockers.clear();
    const cf::PartialCreditClaimLossCohortEvaluation unsealed_evaluation =
        cf::evaluate_partial_credit_claim_loss_cohort(caller_constructed);
    check(!caller_constructed.five_file_integrity_verified() &&
            !caller_constructed.population_frame_evidence_passed() &&
            !caller_constructed.candidate_package_valid() &&
            !unsealed_evaluation.five_file_integrity_verified &&
            !unsealed_evaluation.population_frame_evidence_passed &&
            !unsealed_evaluation.candidate_package_valid &&
            unsealed_evaluation.frame_count == 1U,
        "caller-constructed mechanical input cannot forge loader provenance");

    cf::PartialCreditClaimLossCohortPackage mutated = package;
    mutated.config.cohort_id = "caller-mutated-cohort";
    mutated.observations.clear();
    mutated.admission_blockers.clear();
    const cf::PartialCreditClaimLossCohortEvaluation sealed_evaluation =
        cf::evaluate_partial_credit_claim_loss_cohort(mutated);
    check(sealed_evaluation.five_file_integrity_verified &&
            sealed_evaluation.population_frame_evidence_passed &&
            sealed_evaluation.candidate_package_valid &&
            sealed_evaluation.frame_count == 1U &&
            sealed_evaluation.observations.size() == 1U &&
            sealed_evaluation.observations.front().observation_id ==
                "observation-1",
        "sealed evaluation reloads the binder instead of trusting caller-mutated parsed fields");
}

void test_adversaries(const std::filesystem::path& valid,
                      const std::filesystem::path& workspace) {
    {
        const auto root = copy_package(valid, workspace, "observation-drift");
        write_text(root / "observations.tsv",
            read_text(root / "observations.tsv") + "\n");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "observations.tsv hash drift is rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "methods-drift");
        write_text(root / "methods.cfg", read_text(root / "methods.cfg") + "\n");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "methods.cfg hash drift is rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "dossier-drift");
        write_text(root / "dossier.cfg", read_text(root / "dossier.cfg") + "\n");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "dossier.cfg hash drift is rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "manifest-drift");
        write_text(root / "evidence_manifest.tsv",
            read_text(root / "evidence_manifest.tsv") + "\n");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "evidence_manifest.tsv hash drift is rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "duplicate-config");
        write_text(root / "cohort.cfg",
            read_text(root / "cohort.cfg") + "cohort_id=duplicate\n");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "duplicate cohort.cfg keys are rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "unknown-config");
        write_text(root / "cohort.cfg",
            read_text(root / "cohort.cfg") + "unknown.field=value\n");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "unknown cohort.cfg keys are rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "missing-config");
        std::string config = read_text(root / "cohort.cfg");
        remove_key(config, "monetary_basis_definition");
        write_text(root / "cohort.cfg", config);
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "missing required cohort.cfg keys are rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "crlf-config");
        std::string config = read_text(root / "cohort.cfg");
        replace_once(config, "\n", "\r\n");
        write_text(root / "cohort.cfg", config);
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "non-LF cohort.cfg is rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "bad-header");
        std::string rows = read_text(root / "observations.tsv");
        replace_once(rows, "observation_id", "unknown_id");
        write_text(root / "observations.tsv", rows);
        rehash(root, "observations_sha256", "observations.tsv");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "closed observations.tsv header is enforced after a valid rehash");
    }
    {
        const auto root = copy_package(valid, workspace, "claim-escape");
        std::string rows = read_text(root / "observations.tsv");
        replace_once(rows, "claims/one/claim.cfg", "../claim.cfg");
        write_text(root / "observations.tsv", rows);
        rehash(root, "observations_sha256", "observations.tsv");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "escaping Claim Ledger paths are rejected after a valid rehash");
    }
    {
        const auto root = copy_package(valid, workspace, "unknown-disposition");
        std::string rows = read_text(root / "observations.tsv");
        replace_once(rows, "\tresolved\t", "\tclosed\t");
        write_text(root / "observations.tsv", rows);
        rehash(root, "observations_sha256", "observations.tsv");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "unknown observation enums are rejected after a valid rehash");
    }
    {
        const auto root = copy_package(valid, workspace, "claim-hash");
        std::string rows = read_text(root / "observations.tsv");
        const std::string actual = cf::sha256_file_lower_hex(
            root / "claims" / "one" / "claim.cfg");
        replace_once(rows, actual, std::string(64U, '0'));
        write_text(root / "observations.tsv", rows);
        rehash(root, "observations_sha256", "observations.tsv");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "nested Claim Ledger root hash mismatch is rejected");
    }
    {
        const auto root = copy_package(valid, workspace, "unsupported-method");
        std::string value = read_text(root / "methods.cfg");
        replace_once(value,
            cf::kPartialCreditClaimLossCohortMechanicalImplementationId,
            "unsupported-kernel");
        write_text(root / "methods.cfg", value);
        rehash(root, "methods_sha256", "methods.cfg");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "unsupported compiled method identifiers fail closed");
    }
    {
        const auto root = copy_package(valid, workspace, "citation-mismatch");
        std::string rows = read_text(root / "observations.tsv");
        replace_once(rows, cf::kClaimPopulationFrameRequirementId,
            "UNKNOWN-REQUIREMENT");
        write_text(root / "observations.tsv", rows);
        rehash(root, "observations_sha256", "observations.tsv");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "evidence and requirement citations must match");
    }
    {
        const auto root = copy_package(valid, workspace, "population-gap");
        write_text(root / "evidence_manifest.tsv", manifest(false));
        write_text(root / "methods.cfg", methods("POP-AUTHORITY"));
        const std::string claim_hash = cf::sha256_file_lower_hex(
            root / "claims" / "one" / "claim.cfg");
        write_text(root / "observations.tsv",
            observations(claim_hash, "POP-AUTHORITY"));
        std::string config = read_text(root / "cohort.cfg");
        replace_key(config, "evidence_manifest_sha256",
            cf::sha256_file_lower_hex(root / "evidence_manifest.tsv"));
        replace_key(config, "methods_sha256",
            cf::sha256_file_lower_hex(root / "methods.cfg"));
        replace_key(config, "observations_sha256",
            cf::sha256_file_lower_hex(root / "observations.tsv"));
        write_text(root / "cohort.cfg", config);
        const auto package =
            cf::load_partial_credit_claim_loss_cohort_package(root);
        check(package.five_file_integrity_verified() &&
                !package.population_frame_evidence_passed() &&
                !package.candidate_package_valid() &&
                !package.empirical_realized_cash_admissible(),
            "a missing independent population source remains a visible admission failure, not a probability input");
    }
    {
        const auto root = copy_package(
            valid, workspace, "retained-evidence-mismatch");
        write_text(root / "retained" / "population.txt", "changed");
        expect_invalid([&] {
            static_cast<void>(
                cf::load_partial_credit_claim_loss_cohort_package(root));
        }, "a named retained evidence copy must match its manifest hash");
    }
    {
        const auto root = copy_package(
            valid, workspace, "retained-evidence-post-load-drift");
        const cf::PartialCreditClaimLossCohortPackage loaded =
            cf::load_partial_credit_claim_loss_cohort_package(root);
        write_text(root / "retained" / "population.txt", "changed");
        expect_invalid([&] {
            static_cast<void>(
                cf::evaluate_partial_credit_claim_loss_cohort(loaded));
        }, "sealed evaluation rejects retained evidence drift instead of copying cached gate authority");
    }
    {
        const auto root = copy_package(
            valid, workspace, "non-supporting-retained-evidence-drift");
        write_text(root / "retained" / "context.txt", "abc");
        write_text(root / "evidence_manifest.tsv",
            read_text(root / "evidence_manifest.tsv") +
                non_supporting_retained_evidence_row());
        rehash(root, "evidence_manifest_sha256", "evidence_manifest.tsv");
        const cf::PartialCreditClaimLossCohortPackage loaded =
            cf::load_partial_credit_claim_loss_cohort_package(root);
        write_text(root / "retained" / "context.txt", "changed");
        expect_invalid([&] {
            static_cast<void>(
                cf::evaluate_partial_credit_claim_loss_cohort(loaded));
        }, "sealed evaluation rechecks non-supporting retained evidence records too");
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: partial_credit_claim_loss_cohort_config_tests "
                     "<claim.cfg>\n";
        return 2;
    }
    try {
        const auto nonce = std::chrono::steady_clock::now()
                               .time_since_epoch().count();
        TempTree temp{std::filesystem::temp_directory_path() /
            ("naturalehia-partial-credit-cohort-config-tests-" +
             std::to_string(nonce))};
        std::filesystem::create_directories(temp.root);
        const std::filesystem::path valid = make_valid_package(
            std::filesystem::absolute(argv[1]), temp.root, "valid");
        test_valid(valid);
        test_adversaries(valid, temp.root);
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        ++failures;
    }
    if (failures != 0) {
        std::cerr << failures
                  << " partial-credit cohort config test(s) failed\n";
        return 1;
    }
    std::cout << "partial-credit cohort config tests passed\n";
    return 0;
}
