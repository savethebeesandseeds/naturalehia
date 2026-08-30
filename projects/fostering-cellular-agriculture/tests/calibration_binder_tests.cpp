// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/calibration_binder.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

class FixtureCopy {
public:
    explicit FixtureCopy(const std::filesystem::path& source) {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        path_ = std::filesystem::temp_directory_path() /
            ("fca-calibration-binder-" + std::to_string(stamp));
        std::filesystem::copy(
            source, path_, std::filesystem::copy_options::recursive);
    }

    ~FixtureCopy() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    if (!input || !output) {
        throw std::runtime_error("failed to read test fixture");
    }
    return output.str();
}

void write_text(const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary);
    output << text;
    if (!output) {
        throw std::runtime_error("failed to write test fixture");
    }
}

void replace_once(
    std::string& value,
    std::string_view from,
    std::string_view to) {
    const std::size_t position = value.find(from);
    if (position == std::string::npos) {
        throw std::runtime_error("test replacement source is absent");
    }
    value.replace(position, from.size(), to);
}

void refresh_hash(
    const std::filesystem::path& directory,
    std::string_view file_name,
    std::string_view binder_field) {
    const std::filesystem::path binder_path = directory / "binder.cfg";
    std::string binder = read_text(binder_path);
    const std::string prefix = std::string(binder_field) + '=';
    const std::size_t start = binder.find(prefix);
    if (start == std::string::npos) {
        throw std::runtime_error("binder hash field is absent");
    }
    const std::size_t value_start = start + prefix.size();
    const std::size_t value_end = binder.find('\n', value_start);
    const std::string digest = cf::sha256_file_lower_hex(
        directory / std::filesystem::path(file_name));
    binder.replace(
        value_start,
        (value_end == std::string::npos ? binder.size() : value_end) -
            value_start,
        digest);
    write_text(binder_path, binder);
}

[[nodiscard]] bool binder_fails(const std::filesystem::path& directory) {
    try {
        static_cast<void>(
            cf::load_calibration_binder(directory / "binder.cfg"));
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

[[nodiscard]] std::string binder_failure_message(
    const std::filesystem::path& directory) {
    try {
        static_cast<void>(
            cf::load_calibration_binder(directory / "binder.cfg"));
        return {};
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
}

[[nodiscard]] bool normalized_lineage_fails(
    const std::vector<cf::CalibrationLineageRow>& rows) {
    try {
        std::ostringstream output;
        cf::print_normalized_calibration_lineage(output, rows);
        return false;
    } catch (const std::invalid_argument&) {
        return true;
    }
}

void mutate_lineage(
    const std::filesystem::path& directory,
    std::string content) {
    write_text(directory / "lineage.tsv", content);
    refresh_hash(directory, "lineage.tsv", "file.lineage.sha256");
}

void test_valid_fixture(const std::filesystem::path& source) {
    FixtureCopy fixture(source);
    const cf::CalibrationBinder binder = cf::load_calibration_binder(
        fixture.path() / "binder.cfg");
    check(binder.candidate_package_valid,
          "valid fixture should be a candidate package");
    check(!binder.calibrated_execution_authorized,
          "candidate package must never authorize calibrated execution");
    check(binder.material_target_paths.size() == 25U,
          "one-project fixture should have 25 material targets");
    check(binder.lineage.size() == binder.material_target_paths.size(),
          "lineage should cover every material target exactly once");
    check(binder.evidence_assessment.highest_allowed_use ==
              cf::AllowedUse::PublicResearch,
          "gap dossier should remain public-research-only");
    std::ostringstream report;
    cf::print_calibration_binder_report(report, binder);
    check(report.str().find("calibrated_execution_authorized=false") !=
              std::string::npos,
          "report should state the hard authorization boundary");
    check(report.str().find("project_id=synthetic-facility") !=
              std::string::npos &&
          report.str().find(
              "dossier_id=synthetic-binder-gap-dossier") !=
              std::string::npos,
          "report should state both bound identities");
}

void test_identity_binding(const std::filesystem::path& source) {
    {
        FixtureCopy fixture(source);
        std::string binder = read_text(fixture.path() / "binder.cfg");
        replace_once(binder, "binder.project_id=synthetic-facility",
                     "binder.project_id=different-project");
        write_text(fixture.path() / "binder.cfg", binder);
        check(binder_fails(fixture.path()),
              "binder project ID mismatch must fail");
    }
    {
        FixtureCopy fixture(source);
        std::string binder = read_text(fixture.path() / "binder.cfg");
        replace_once(
            binder, "binder.dossier_id=synthetic-binder-gap-dossier",
            "binder.dossier_id=different-dossier");
        write_text(fixture.path() / "binder.cfg", binder);
        check(binder_fails(fixture.path()),
              "binder dossier ID mismatch must fail");
    }
    {
        FixtureCopy fixture(source);
        const std::filesystem::path two_project =
            source.parent_path() /
            "two-project-participation-pool-synthetic.cfg";
        write_text(fixture.path() / "portfolio.cfg", read_text(two_project));
        refresh_hash(
            fixture.path(), "portfolio.cfg", "file.portfolio.sha256");
        check(binder_fails(fixture.path()),
              "binder v0.1 must reject a valid two-project portfolio");
    }
}

void test_hash_drift(const std::filesystem::path& source) {
    FixtureCopy fixture(source);
    std::string portfolio = read_text(fixture.path() / "portfolio.cfg");
    portfolio += "# raw hash drift\n";
    write_text(fixture.path() / "portfolio.cfg", portfolio);
    check(binder_fails(fixture.path()),
          "unacknowledged raw-file hash drift must fail");
}

void test_lineage_coverage(const std::filesystem::path& source) {
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(lineage,
                     "portfolio/scenario.1.weight\tprobability",
                     "ambiguity/scenario.1.central_weight\tprobability");
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "duplicate lineage target must fail");
    }
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        const std::size_t last = lineage.rfind("SYN-023\t");
        if (last == std::string::npos) {
            throw std::runtime_error("last lineage row is absent");
        }
        lineage.erase(last);
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "missing lineage target must fail");
    }
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(lineage,
                     "portfolio/scenario.1.weight\tprobability",
                     "portfolio/scenario.999.weight\tprobability");
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "orphan lineage target must fail");
    }
}

void test_citation_controls(const std::filesystem::path& source) {
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(lineage,
                     "invented-unit-path\tNONE\tNONE\t",
                     "invented-unit-path\tSYN-GAP-001\tFIN-TERM-SHEET\t");
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "citation requirement mismatch must fail");
    }
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(lineage,
                     "\tprobability\tsynthetic\tinvented-unit-path\t",
                     "\tprobability\tobserved\tinvented-unit-path\t");
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "observed status without citations must fail");
    }
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(
            lineage,
            "\tprobability\tsynthetic\tinvented-unit-path\tNONE\tNONE\t",
            "\tprobability\tobserved\tinvented-unit-path\tSYN-PUB-001\t"
            "REF-LEGAL-OPERATOR\t");
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "public question-only evidence must not qualify an observed input");
    }
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(lineage,
                     "\tprobability\tsynthetic\tinvented-unit-path\t",
                     "\tunknown-class\tsynthetic\tinvented-unit-path\t");
        mutate_lineage(fixture.path(), lineage);
        check(binder_fails(fixture.path()),
              "unknown lineage classes must fail");
    }
}

void test_target_class_compatibility(const std::filesystem::path& source) {
    FixtureCopy fixture(source);
    std::string lineage = read_text(fixture.path() / "lineage.tsv");
    replace_once(lineage,
                 "ambiguity/scenario.1.central_weight\tprobability",
                 "ambiguity/scenario.1.central_weight\tcapital");
    mutate_lineage(fixture.path(), lineage);
    check(binder_fails(fixture.path()),
          "probability weights must reject an arbitrary capital class");
}

void test_resource_bounds(const std::filesystem::path& source) {
    {
        FixtureCopy fixture(source);
        std::fstream portfolio(
            fixture.path() / "portfolio.cfg",
            std::ios::binary | std::ios::in | std::ios::out);
        portfolio.seekp(16 * 1024 * 1024);
        portfolio.put('x');
        portfolio.close();
        const std::string message = binder_failure_message(fixture.path());
        check(message.find("exceeds its byte cap") != std::string::npos,
              "artifact byte cap must be checked before hash drift");
    }
    {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        const std::size_t header_end = lineage.find('\n');
        const std::size_t first_row_end = lineage.find(
            '\n', header_end == std::string::npos ? 0U : header_end + 1U);
        if (first_row_end == std::string::npos) {
            throw std::runtime_error("first lineage row end is absent");
        }
        lineage.insert(first_row_end, 8'193U, 'x');
        mutate_lineage(fixture.path(), lineage);
        const std::string message = binder_failure_message(fixture.path());
        check(message.find("exceeds 8192 bytes") != std::string::npos,
              "lineage lines must have an explicit byte bound");
    }
}

void test_non_synthetic_rejection(const std::filesystem::path& source) {
    FixtureCopy fixture(source);
    std::string portfolio = read_text(fixture.path() / "portfolio.cfg");
    replace_once(portfolio, "portfolio.synthetic_inputs=true",
                 "portfolio.synthetic_inputs=false");
    write_text(fixture.path() / "portfolio.cfg", portfolio);
    refresh_hash(fixture.path(), "portfolio.cfg", "file.portfolio.sha256");
    check(binder_fails(fixture.path()),
          "synthetic_inputs=false must remain explicitly rejected");
}

void test_complete_lineage_vocabulary(const std::filesystem::path& source) {
    constexpr std::string_view uncited_statuses[]{
        "hypothesis", "stress", "synthetic", "policy",
    };
    for (const std::string_view status : uncited_statuses) {
        FixtureCopy fixture(source);
        std::string lineage = read_text(fixture.path() / "lineage.tsv");
        replace_once(
            lineage, "\tprobability\tsynthetic\tinvented-unit-path\t",
            "\tprobability\t" + std::string(status) +
                "\tinvented-unit-path\t");
        mutate_lineage(fixture.path(), lineage);
        check(!binder_fails(fixture.path()),
              "every uncited candidate input_status must parse successfully");
    }

    constexpr std::array<cf::CalibrationInputClass, 11U> classes{{
        cf::CalibrationInputClass::Capital,
        cf::CalibrationInputClass::Transition,
        cf::CalibrationInputClass::Probability,
        cf::CalibrationInputClass::Recovery,
        cf::CalibrationInputClass::Dependence,
        cf::CalibrationInputClass::QualifiedOutput,
        cf::CalibrationInputClass::CommercialCash,
        cf::CalibrationInputClass::SourceCredit,
        cf::CalibrationInputClass::Cost,
        cf::CalibrationInputClass::PolicyHurdle,
        cf::CalibrationInputClass::InstrumentTerm,
    }};
    constexpr std::array<std::string_view, 11U> class_names{{
        "capital", "transition", "probability", "recovery", "dependence",
        "qualified-output", "commercial-cash", "source-credit", "cost",
        "policy-hurdle", "instrument-term",
    }};
    constexpr std::array<std::string_view, 11U> compatible_targets{{
        "portfolio/project.1.commitment_million",
        "portfolio/project.1.stage",
        "portfolio/scenario.1.weight",
        "portfolio/scenario.1.project.1.resolution",
        "portfolio/scenario.1.id",
        "portfolio/scenario.1.project.1.receipt.1.amount_million",
        "portfolio/scenario.1.project.1.receipt.1.month",
        "portfolio/scenario.1.project.1.receipt.1.cash_source_id",
        "portfolio/scenario.1.pool_cost.1.amount_million",
        "portfolio/portfolio.annual_physical_hurdle_rate",
        "portfolio/portfolio.currency_label",
    }};
    for (std::size_t index = 0U; index < classes.size(); ++index) {
        check(cf::to_string(classes[index]) == class_names[index],
              "every input_class enum must have an exact normalized token");
        cf::CalibrationLineageRow row;
        row.input_id = "PROGRAMMATIC-CLASS-" + std::to_string(index);
        row.target_path = compatible_targets[index];
        row.input_class = classes[index];
        row.input_status = cf::CalibrationInputStatus::Synthetic;
        row.method_id = "programmatic-test";
        row.limitations = "Programmatic class normalization test only.";
        row.update_or_retire = "Retire after test.";
        check(!normalized_lineage_fails({row}),
              "every valid input_class must normalize on a compatible target");
    }

    constexpr std::array<cf::CalibrationInputStatus, 9U> statuses{{
        cf::CalibrationInputStatus::Observed,
        cf::CalibrationInputStatus::Contractual,
        cf::CalibrationInputStatus::Derived,
        cf::CalibrationInputStatus::Estimated,
        cf::CalibrationInputStatus::Transfer,
        cf::CalibrationInputStatus::Hypothesis,
        cf::CalibrationInputStatus::Stress,
        cf::CalibrationInputStatus::Synthetic,
        cf::CalibrationInputStatus::Policy,
    }};
    constexpr std::array<std::string_view, 9U> status_names{{
        "observed", "contractual", "derived", "estimated", "transfer",
        "hypothesis", "stress", "synthetic", "policy",
    }};
    for (std::size_t index = 0U; index < statuses.size(); ++index) {
        check(cf::to_string(statuses[index]) == status_names[index],
              "every input_status enum must have an exact normalized token");
        cf::CalibrationLineageRow row;
        row.input_id = "PROGRAMMATIC-STATUS-" + std::to_string(index);
        row.target_path = index < 5U
            ? "portfolio/project.1.commitment_million"
            : "portfolio/scenario.1.weight";
        row.input_class = index < 5U
            ? cf::CalibrationInputClass::Capital
            : cf::CalibrationInputClass::Probability;
        row.input_status = statuses[index];
        row.method_id = "programmatic-test";
        if (index < 5U) {
            row.evidence_record_ids = {"PROGRAMMATIC-EVIDENCE"};
            row.requirement_ids = {"PROGRAMMATIC-REQUIREMENT"};
        }
        row.limitations = "Programmatic status normalization test only.";
        row.update_or_retire = "Retire after test.";
        check(!normalized_lineage_fails({row}),
              "every valid input_status must normalize with required citations");
    }

    for (std::size_t index = 0U; index < 5U; ++index) {
        cf::CalibrationLineageRow row;
        row.input_id =
            "PROGRAMMATIC-PROBABILITY-STATUS-" + std::to_string(index);
        row.target_path = "portfolio/scenario.1.weight";
        row.input_class = cf::CalibrationInputClass::Probability;
        row.input_status = statuses[index];
        row.method_id = "programmatic-test";
        row.evidence_record_ids = {"PROGRAMMATIC-CONTROLLED-EVIDENCE"};
        row.requirement_ids = {"PROGRAMMATIC-PASSING-REQUIREMENT"};
        row.limitations =
            "No empirical population and method ledger is present.";
        row.update_or_retire = "Retire when v0.1 is superseded.";
        check(normalized_lineage_fails({row}),
              "evidence-backed probability statuses must fail in binder v0.1");
    }

    cf::CalibrationLineageRow invalid_row;
    invalid_row.input_id = "PROGRAMMATIC-INVALID";
    invalid_row.target_path = "portfolio/scenario.1.weight";
    invalid_row.input_class = cf::CalibrationInputClass::Probability;
    invalid_row.input_status = cf::CalibrationInputStatus::Synthetic;
    invalid_row.method_id = "programmatic-test";
    invalid_row.limitations = "Programmatic invalid-enum test only.";
    invalid_row.update_or_retire = "Retire after test.";
    invalid_row.input_class = static_cast<cf::CalibrationInputClass>(999);
    check(normalized_lineage_fails({invalid_row}),
          "invalid programmatic input_class enums must fail normalized output");
    invalid_row.input_class = cf::CalibrationInputClass::Probability;
    invalid_row.input_status = static_cast<cf::CalibrationInputStatus>(999);
    check(normalized_lineage_fails({invalid_row}),
          "invalid programmatic input_status enums must fail normalized output");
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            throw std::invalid_argument("fixture directory argument required");
        }
        const std::filesystem::path source(argv[1]);
        test_valid_fixture(source);
        test_identity_binding(source);
        test_hash_drift(source);
        test_lineage_coverage(source);
        test_citation_controls(source);
        test_target_class_compatibility(source);
        test_resource_bounds(source);
        test_non_synthetic_rejection(source);
        test_complete_lineage_vocabulary(source);
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }
    if (failures != 0) {
        std::cerr << failures << " calibration binder test(s) failed\n";
        return 1;
    }
    std::cout << "all calibration binder tests passed\n";
    return 0;
}
