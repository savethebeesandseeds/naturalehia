// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_package.hpp>
#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

[[nodiscard]] bool near(
    double first, double second, double tolerance = 1.0e-9) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] bool exact_value(
    const cf::ClaimLedgerValue& value, double expected) {
    return value.status == cf::ClaimLedgerValueStatus::Known &&
        value.lower.has_value() && value.upper.has_value() &&
        near(*value.lower, expected) && near(*value.upper, expected);
}

[[nodiscard]] bool has_blocker(
    const cf::ClaimLedgerPackage& package, std::string_view fragment) {
    return std::any_of(package.blockers.begin(), package.blockers.end(),
        [fragment](const std::string& blocker) {
            return blocker.find(fragment) != std::string::npos;
        });
}

template <typename Function>
void expect_invalid_argument(Function&& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

template <typename Function>
void expect_invalid_argument_containing(Function&& operation,
    std::string_view diagnostic, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument& error) {
        check(std::string_view(error.what()).find(diagnostic) !=
                std::string_view::npos,
            std::string(message) + " (stable diagnostic)");
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not read test file: " + path.string());
    }
    std::ostringstream output;
    output << input.rdbuf();
    if (input.bad()) {
        throw std::runtime_error("could not read test file completely");
    }
    return output.str();
}

void write_text(
    const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not write test file: " + path.string());
    }
    output.write(contents.data(),
        static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("could not write test file completely");
    }
}

void replace_once(std::string& text, std::string_view before,
    std::string_view after) {
    const std::size_t position = text.find(before);
    if (position == std::string::npos ||
        text.find(before, position + before.size()) != std::string::npos) {
        throw std::runtime_error(
            "test replacement target must occur exactly once");
    }
    text.replace(position, before.size(), after);
}

void replace_all(std::string& text, std::string_view before,
    std::string_view after) {
    if (before.empty()) {
        throw std::runtime_error("test replacement target cannot be empty");
    }
    std::size_t position = 0U;
    while ((position = text.find(before, position)) != std::string::npos) {
        text.replace(position, before.size(), after);
        position += after.size();
    }
}

void remove_unique_line_containing(
    std::string& text, std::string_view needle) {
    const std::size_t match = text.find(needle);
    if (match == std::string::npos ||
        text.find(needle, match + needle.size()) != std::string::npos) {
        throw std::runtime_error(
            "test row-removal target must occur exactly once");
    }
    const std::size_t line_start =
        match == 0U ? 0U : text.rfind('\n', match - 1U) + 1U;
    const std::size_t newline = text.find('\n', match);
    const std::size_t line_end =
        newline == std::string::npos ? text.size() : newline + 1U;
    text.erase(line_start, line_end - line_start);
}

[[nodiscard]] std::vector<std::string_view> split_tabs(
    std::string_view row) {
    std::vector<std::string_view> result;
    std::size_t start = 0U;
    while (true) {
        const std::size_t tab = row.find('\t', start);
        result.push_back(row.substr(start,
            tab == std::string_view::npos ? row.size() - start
                                          : tab - start));
        if (tab == std::string_view::npos) break;
        start = tab + 1U;
    }
    return result;
}

[[nodiscard]] std::string scenario_closing_rows(
    std::string_view common_entries, std::string_view scenario_id,
    std::string_view prefix, std::size_t period) {
    const std::size_t header_end = common_entries.find('\n');
    if (header_end == std::string_view::npos) {
        throw std::runtime_error("common closing fixture has no header");
    }
    std::ostringstream output;
    std::size_t position = header_end + 1U;
    while (position < common_entries.size()) {
        const std::size_t newline = common_entries.find('\n', position);
        const std::size_t end = newline == std::string_view::npos
            ? common_entries.size()
            : newline;
        std::string_view row = common_entries.substr(position, end - position);
        if (!row.empty() && row.back() == '\r') row.remove_suffix(1U);
        if (!row.empty()) {
            const auto fields = split_tabs(row);
            if (fields.size() != 12U) {
                throw std::runtime_error(
                    "common closing fixture row has the wrong width");
            }
            output << scenario_id << '\t' << prefix << '-' << fields[0U]
                   << '\t' << prefix << '-' << fields[1U] << '\t' << prefix
                   << "-closing\t" << fields[3U] << '\t' << period << '\t'
                   << fields[5U] << '\t' << fields[6U] << '\t' << fields[7U]
                   << '\t' << fields[8U] << '\t' << fields[9U] << '\t'
                   << fields[10U] << '\t' << fields[11U] << '\n';
        }
        if (newline == std::string_view::npos) break;
        position = newline + 1U;
    }
    return output.str();
}

void replace_first_data_cell(std::string& text,
    std::string_view column_name, std::string_view after) {
    const std::size_t header_end = text.find('\n');
    if (header_end == std::string::npos) {
        throw std::runtime_error("test TSV has no data header");
    }
    std::string_view header(text.data(), header_end);
    if (!header.empty() && header.back() == '\r') header.remove_suffix(1U);
    const std::vector<std::string_view> columns = split_tabs(header);
    const auto column = std::find(columns.begin(), columns.end(), column_name);
    if (column == columns.end()) {
        throw std::runtime_error("test TSV column is missing");
    }
    const std::size_t column_index =
        static_cast<std::size_t>(std::distance(columns.begin(), column));
    const std::size_t row_start = header_end + 1U;
    const std::size_t newline = text.find('\n', row_start);
    const std::size_t raw_row_end =
        newline == std::string::npos ? text.size() : newline;
    std::size_t row_end = raw_row_end;
    if (row_end > row_start && text[row_end - 1U] == '\r') --row_end;
    std::string_view row(text.data() + row_start, row_end - row_start);
    const std::vector<std::string_view> fields = split_tabs(row);
    if (column_index >= fields.size()) {
        throw std::runtime_error("test TSV data row is too narrow");
    }
    const std::size_t cell_start =
        static_cast<std::size_t>(fields[column_index].data() - text.data());
    text.replace(cell_start, fields[column_index].size(), after);
}

void refresh_bound_hash(const std::filesystem::path& directory,
    std::string_view stem, std::string_view filename) {
    const std::filesystem::path file = directory / filename;
    const std::string digest = cf::sha256_file_lower_hex(file);
    const std::filesystem::path config_path = directory / "claim.cfg";
    std::string config = read_text(config_path);
    const std::string prefix = "file." + std::string(stem) + ".sha256=";
    const std::size_t first = config.find(prefix);
    if (first == std::string::npos) {
        throw std::runtime_error("bound hash key is missing in test fixture");
    }
    const std::size_t value_start = first + prefix.size();
    const std::size_t line_end = config.find('\n', value_start);
    config.replace(value_start,
        (line_end == std::string::npos ? config.size() : line_end) -
            value_start,
        digest);
    write_text(config_path, config);
}

constexpr std::string_view kTestSourceManifestHeader =
    "source_id\trecord_date\taccess_date\tevidence_class\tprovenance_tag\t"
    "distribution_channel\toriginating_record\tsource_uri\tretention_status\t"
    "retained_copy\tretained_sha256\tbytes\tclaim_scope\tlimitations\n";

void bind_parent_manifest(const std::filesystem::path& fixture,
    std::string_view manifest_bytes) {
    const std::filesystem::path manifest =
        fixture.parent_path() / "source_manifest.tsv";
    write_text(manifest, manifest_bytes);
    const std::string digest = cf::sha256_file_lower_hex(manifest);
    std::string config = read_text(fixture / "claim.cfg");
    const std::string prefix = "source_manifest.sha256=";
    const std::size_t value_start = config.find(prefix);
    if (value_start == std::string::npos) {
        throw std::runtime_error("source manifest hash key is missing");
    }
    const std::size_t start = value_start + prefix.size();
    const std::size_t line_end = config.find('\n', start);
    config.replace(start,
        (line_end == std::string::npos ? config.size() : line_end) - start,
        digest);
    write_text(fixture / "claim.cfg", config);
}

void write_retained_test_manifest(const std::filesystem::path& fixture,
    std::string_view source_record_id,
    std::string_view record_date = "2026-01-01",
    bool duplicate = false,
    std::string_view retained_path = "retained_sources/test-source.txt",
    std::string_view evidence_class = "A",
    std::string_view provenance_tag = "CTR",
    std::string_view access_date = "2026-08-30",
    std::string_view retained_bytes =
        "controlled test evidence retained for the decision\n") {
    const std::filesystem::path retained_directory =
        fixture.parent_path() / "retained_sources";
    std::filesystem::create_directories(retained_directory);
    const std::filesystem::path retained_file =
        retained_directory / "test-source.txt";
    write_text(retained_file, retained_bytes);
    const std::string retained_digest =
        cf::sha256_file_lower_hex(retained_file);
    const auto row = [&]() {
        std::ostringstream output;
        output << source_record_id << '\t' << record_date
               << '\t' << access_date << '\t' << evidence_class << '\t'
               << provenance_tag << "\tcontrolled-data-room\t"
                  "executed-test-record\turn:test:controlled-record\tRETAINED\t"
               << retained_path << '\t' << retained_digest << '\t'
               << retained_bytes.size()
               << "\tcomplete controlled test fixture\ttest-only evidence\n";
        return output.str();
    }();
    std::string manifest{kTestSourceManifestHeader};
    manifest += row;
    if (duplicate) manifest += row;
    bind_parent_manifest(fixture, manifest);
}

void copy_fixture(const std::filesystem::path& source,
    const std::filesystem::path& destination) {
    std::filesystem::copy(source, destination,
        std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
}

void make_controlled_fixture(const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::string_view source_record_id) {
    copy_fixture(source, destination);
    std::string config = read_text(destination / "claim.cfg");
    replace_once(config, "package.status=synthetic-complete",
        "package.status=controlled-candidate");
    replace_once(config,
        "package.economic_cluster_boundary_input_status=stress",
        "package.economic_cluster_boundary_input_status=derived");
    replace_all(config, "_input_status=stress", "_input_status=derived");
    replace_all(config, "SYNTHETIC", source_record_id);
    write_text(destination / "claim.cfg", config);

    constexpr std::array<std::pair<std::string_view, std::string_view>, 7U>
        bound_files{{
            {"terms", "terms.tsv"},
            {"common_entries", "common_entries.tsv"},
            {"scenarios", "scenarios.tsv"},
            {"scenario_entries", "scenario_entries.tsv"},
            {"provider_claims", "provider_claims.tsv"},
            {"covenant_events", "covenant_events.tsv"},
            {"conversion_context", "conversion_context.tsv"},
        }};
    for (const auto& [stem, filename] : bound_files) {
        std::string bytes = read_text(destination / filename);
        replace_all(bytes, "\tstress\t", "\testimated\t");
        if (filename == "common_entries.tsv") {
            replace_all(bytes, "\tcontractual\t", "\tobserved\t");
        }
        replace_all(bytes, "SYNTHETIC", source_record_id);
        write_text(destination / filename, bytes);
        refresh_bound_hash(destination, stem, filename);
    }
    if (source_record_id == "NO_PUBLIC_SOURCE") {
        bind_parent_manifest(destination, kTestSourceManifestHeader);
    } else {
        write_retained_test_manifest(destination, source_record_id);
    }
}

void test_complete_synthetic(const std::filesystem::path& claim_path) {
    const cf::ClaimLedgerPackageWithPathEvidence loaded =
        cf::load_claim_ledger_package_with_full_path_evidence(
            claim_path, "failure-with-provider");
    const cf::ClaimLedgerPackage& package = loaded.package;
    check(package.package_integrity && package.core_config_ready &&
            package.core_config.has_value() && package.evaluation.has_value(),
        "the SHA-256-bound synthetic package constructs and evaluates the core");
    check(package.config.package_status ==
                cf::ClaimLedgerPackageStatus::SyntheticComplete &&
            package.config.economic_cluster_boundary_status ==
                cf::ClaimLedgerEconomicClusterBoundaryStatus::Defined &&
            !package.expected_return_admissible &&
            !package.observation_admissible,
        "synthetic mechanics are never promoted into an admissible expected return or market observation");
    const auto& failure_evidence = loaded.full_path;
    check(failure_evidence.claim_config_sha256 ==
                package.claim_config_sha256 &&
            failure_evidence.package_id ==
                package.config.package_id &&
            failure_evidence.claim_id ==
                package.config.claim_id &&
            failure_evidence.cash_path_status.input_status ==
                cf::ClaimLedgerInputStatus::Derived &&
            failure_evidence.cash_path_status.source.source_record_id ==
                "SYNTHETIC" &&
            !failure_evidence.cash_path_status.source.record_date.has_value() &&
            !failure_evidence.cash_path_status.source.retained_copy_verified &&
            failure_evidence.full_evaluation_scenario_index <
                package.full_evaluation->scenarios.size() &&
            package.full_evaluation->scenarios[
                failure_evidence.full_evaluation_scenario_index]
                    .scenario_id == failure_evidence.scenario_id,
        "synthetic full-path provenance is retained and tied to one authoritative evaluated scenario without becoming evidence");
    check(std::is_sorted(failure_evidence.selected_entries.begin(),
              failure_evidence.selected_entries.end(),
              [](const auto& left, const auto& right) {
                  return left.entry.economic_fact_id <
                      right.entry.economic_fact_id;
              }) &&
            std::adjacent_find(failure_evidence.selected_entries.begin(),
                failure_evidence.selected_entries.end(),
                [](const auto& left, const auto& right) {
                    return left.entry.economic_fact_id ==
                        right.entry.economic_fact_id;
                }) == failure_evidence.selected_entries.end(),
        "full-path evidence retains one canonical latest version per economic fact");
    expect_invalid_argument([&] {
        (void)cf::load_claim_ledger_package_with_full_path_evidence(
            claim_path, "unknown-scenario");
    }, "an unknown full-path evidence scenario fails closed");
    expect_invalid_argument_containing([&] {
        (void)cf::load_claim_ledger_package_with_full_path_evidence(
            claim_path.parent_path() /
                "__scenario-validation-must-precede-package-io__" /
                "claim.cfg",
            std::string(129U, 'a'));
    }, "safe bounded identifier",
        "an unsafe requested scenario is rejected before package I/O");
    check(package.row_counts.terms == 4U &&
            package.row_counts.common_entries == 10U &&
            package.row_counts.scenarios == 2U &&
            package.row_counts.scenario_entries == 9U &&
            package.row_counts.provider_claims == 1U &&
            package.row_counts.covenant_events == 1U &&
            package.row_counts.conversion_context == 0U,
        "every bound table is parsed with the expected exact row count");
    const cf::ClaimLedgerSummary& summary = *package.evaluation;
    check(summary.readiness.expected_cash_ready &&
            summary.readiness.npv_ready &&
            summary.readiness.rate_preimage_ready &&
            summary.readiness.provider_claim_ready &&
            exact_value(summary.expected_npv_million,
                0.190909090909091) &&
            exact_value(summary.expected_principal_loss_million, 0.8) &&
            exact_value(summary.annual_effective_rate_preimage,
                0.123595505617978),
        "package evaluation matches the independent one-year hand calculation");

    std::ostringstream report;
    cf::print_claim_ledger_package_report(report, package);
    const std::string output = report.str();
    check(output.find("PROJECT CLAIM LEDGER v0.1 PACKAGE REVIEW") !=
                std::string::npos &&
            output.find("package_integrity=verified") != std::string::npos &&
            output.find("expected_return_admissible=false") !=
                std::string::npos &&
            output.find("npv_admissible=false") != std::string::npos &&
            output.find("Analysis basis") != std::string::npos &&
            output.find("currency_label=TEST") != std::string::npos &&
            output.find("monetary_basis=nominal synthetic millions") !=
                std::string::npos &&
            output.find("maturity_date=2027-01-01") !=
                std::string::npos &&
            output.find("annual_effective_discount_rate=0.1") !=
                std::string::npos &&
            output.find("Expected investor cash-flow schedule") !=
                std::string::npos &&
            output.find("0|-") != std::string::npos &&
            output.find("12|10") != std::string::npos &&
            output.find("Bound snapshots") != std::string::npos &&
            output.find("claim_config=claim.cfg|") != std::string::npos &&
            output.find("source_manifest_sha256=NOT_APPLICABLE") !=
                std::string::npos &&
            output.find("mechanical_expected_cash_ready=true") !=
                std::string::npos &&
            output.find("mechanical_npv_ready=true") != std::string::npos &&
            output.find("mechanical_rate_preimage_ready=true") !=
                std::string::npos &&
            output.find("observation_admissible=false") !=
                std::string::npos,
        "the deterministic report distinguishes mechanics readiness from observation admission");

    cf::ClaimLedgerPackage mutated = package;
    mutated.config.currency_label = "FORGED";
    std::ostringstream mutation_report;
    cf::print_claim_ledger_package_report(mutation_report, mutated);
    check(mutation_report.str().find("currency_label=TEST") !=
                std::string::npos &&
            mutation_report.str().find("currency_label=FORGED") ==
                std::string::npos,
        "the public report API reloads the verified root snapshot instead of trusting mutable caller fields");

    cf::ClaimLedgerPackage forged;
    forged.package_integrity = true;
    bool forged_rejected = false;
    try {
        std::ostringstream forged_report;
        cf::print_claim_ledger_package_report(forged_report, forged);
    } catch (const std::logic_error&) {
        forged_rejected = true;
    }
    check(forged_rejected,
        "a caller cannot manufacture a verified package review by setting the public integrity boolean");
}

void test_incomplete_public(const std::filesystem::path& liberation_path,
    const std::filesystem::path& solar_path) {
    const cf::ClaimLedgerPackage liberation =
        cf::load_claim_ledger_package(liberation_path);
    check(liberation.package_integrity && !liberation.core_config_ready &&
            !liberation.core_config.has_value() &&
            !liberation.evaluation.has_value() &&
            !liberation.expected_return_admissible &&
            !liberation.observation_admissible &&
            liberation.config.contractual_face_amount_million.value.status ==
                cf::ClaimLedgerValueStatus::Unknown &&
            !liberation.config.contractual_face_amount_million.value.lower
                 .has_value(),
        "Liberation remains a valid incomplete package without converting UNKNOWN face to zero");
    check(has_blocker(liberation, "economic cluster boundary is unresolved") &&
            has_blocker(liberation, "timeline.settlement_date is UNKNOWN") &&
            has_blocker(liberation,
                "probability is not exact"),
        "Liberation exposes cluster, settlement and probability blockers");

    const cf::ClaimLedgerPackage solar =
        cf::load_claim_ledger_package(solar_path);
    check(solar.package_integrity && !solar.core_config_ready &&
            !solar.evaluation.has_value() &&
            !solar.expected_return_admissible &&
            !solar.observation_admissible &&
            solar.row_counts.provider_claims == 2U &&
            solar.row_counts.covenant_events == 6U,
        "Solar retains both named provider observations and dated covenant history without evaluation");
    check(has_blocker(solar,
              "provider claim finnvera-guarantee-terms-UNKNOWN") &&
            has_blocker(solar,
                "provider claim ekf-guarantee-terms-UNKNOWN") &&
            has_blocker(solar, "timeline.maturity_date is UNKNOWN") &&
            has_blocker(solar,
                "common_entries.tsv entry required-buyer-price"),
        "Solar exposes guarantee, maturity and settlement-price blockers");
}

void test_package_adversaries(const std::filesystem::path& source_claim,
    const std::filesystem::path& temporary_root) {
    const std::filesystem::path source = source_claim.parent_path();

    std::filesystem::path fixture = temporary_root / "hash-mismatch";
    copy_fixture(source, fixture);
    write_text(fixture / "terms.tsv",
        read_text(fixture / "terms.tsv") + "\n");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a changed bound byte invalidates the package hash");

    fixture = temporary_root / "unknown-key";
    copy_fixture(source, fixture);
    write_text(fixture / "claim.cfg",
        read_text(fixture / "claim.cfg") + "unknown.key=value\n");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package_config(
                fixture / "claim.cfg");
        },
        "the closed claim.cfg schema rejects unknown keys");

    fixture = temporary_root / "path-traversal";
    copy_fixture(source, fixture);
    std::string config = read_text(fixture / "claim.cfg");
    replace_once(config, "file.terms.path=terms.tsv",
        "file.terms.path=../terms.tsv");
    write_text(fixture / "claim.cfg", config);
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package_config(
                fixture / "claim.cfg");
        },
        "bound-file path traversal is rejected before opening a file");

    fixture = temporary_root / "bad-source-marker";
    copy_fixture(source, fixture);
    std::string terms = read_text(fixture / "terms.tsv");
    const std::string marker = "\tSYNTHETIC\t";
    const std::size_t marker_position = terms.find(marker);
    if (marker_position == std::string::npos) {
        throw std::runtime_error("synthetic source marker is missing");
    }
    terms.replace(marker_position, marker.size(), "\tUNLISTED-SOURCE\t");
    write_text(fixture / "terms.tsv", terms);
    refresh_bound_hash(fixture, "terms", "terms.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "synthetic packages accept only the exact SYNTHETIC absence marker");

    fixture = temporary_root / "synthetic-absence-source";
    copy_fixture(source, fixture);
    terms = read_text(fixture / "terms.tsv");
    terms.replace(marker_position, marker.size(), "\tNO_PUBLIC_SOURCE\t");
    write_text(fixture / "terms.tsv", terms);
    refresh_bound_hash(fixture, "terms", "terms.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "synthetic-complete packages cannot use a public-evidence absence marker");

    fixture = temporary_root / "malformed-typed-number";
    copy_fixture(source, fixture);
    std::string common_entries =
        read_text(fixture / "common_entries.tsv");
    replace_first_data_cell(
        common_entries, "lower_value", "not-a-number");
    write_text(fixture / "common_entries.tsv", common_entries);
    refresh_bound_hash(fixture, "common_entries", "common_entries.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a hash-consistent malformed typed number is rejected");

    fixture = temporary_root / "not-applicable-monetary-entry";
    copy_fixture(source, fixture);
    common_entries = read_text(fixture / "common_entries.tsv");
    replace_first_data_cell(
        common_entries, "value_status", "not-applicable");
    replace_first_data_cell(
        common_entries, "lower_value", "NOT_APPLICABLE");
    replace_first_data_cell(
        common_entries, "upper_value", "NOT_APPLICABLE");
    write_text(fixture / "common_entries.tsv", common_entries);
    refresh_bound_hash(fixture, "common_entries", "common_entries.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a hash-consistent not-applicable monetary entry is rejected");

    fixture = temporary_root / "selector-work-product-guard";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string guarded_scenarios = read_text(fixture / "scenarios.tsv");
    std::ostringstream additional_scenarios;
    for (std::size_t index = 0U; index < 4'094U; ++index) {
        additional_scenarios << "resource-scenario-" << index
                             << "\tknown\t0\t0\t0\testimated\tTEST-SOURCE\t"
                                "complete-resolved\t0\tderived\tTEST-SOURCE\n";
    }
    guarded_scenarios += additional_scenarios.str();
    write_text(fixture / "scenarios.tsv", guarded_scenarios);
    refresh_bound_hash(fixture, "scenarios", "scenarios.tsv");
    std::string guarded_entries =
        read_text(fixture / "scenario_entries.tsv");
    std::ostringstream additional_entries;
    for (std::size_t index = 0U; index < 470U; ++index) {
        additional_entries
            << "resource-scenario-0\tresource-entry-" << index
            << "\tresource-fact-" << index << "\tresource-event-" << index
            << "\tinterest-accrual\t1\t0\tknown\t0\t0\testimated\t"
               "TEST-SOURCE\tNONE\n";
    }
    guarded_entries += additional_entries.str();
    write_text(fixture / "scenario_entries.tsv", guarded_entries);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "scenario-by-entry decision-version selection is rejected before a legal package can force billions of visits");

    fixture = temporary_root / "bounded-blocker-report";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string many_unknown_entries =
        read_text(fixture / "scenario_entries.tsv");
    std::ostringstream unknown_rows;
    for (std::size_t index = 0U; index < 2'000U; ++index) {
        unknown_rows
            << "performing-maturity\tunknown-entry-" << index
            << "\tunknown-fact-" << index
            << "\tNONE\tinterest-accrual\tUNKNOWN\tUNKNOWN\tunknown\t"
               "UNKNOWN\tUNKNOWN\tunknown\tTEST-SOURCE\tNONE\n";
    }
    many_unknown_entries += unknown_rows.str();
    write_text(fixture / "scenario_entries.tsv", many_unknown_entries);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage bounded_blockers =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    std::ostringstream bounded_blocker_report;
    cf::print_claim_ledger_package_report(
        bounded_blocker_report, bounded_blockers);
    check(bounded_blockers.blockers.size() <= 1'024U &&
            std::any_of(bounded_blockers.blockers.begin(),
                bounded_blockers.blockers.end(),
                [](const std::string& blocker) {
                    return blocker.starts_with(
                        "additional blockers omitted after report cap:");
                }) &&
            has_blocker(bounded_blockers,
                "mechanical expected-cash ledger is not ready") &&
            bounded_blocker_report.str().size() < 2U * 1024U * 1024U,
        "millions of potential row blockers collapse to a bounded sample with an explicit omitted count and bounded report");

    fixture = temporary_root / "bounded-late-provider-blockers";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string many_late_providers =
        read_text(fixture / "provider_claims.tsv");
    std::ostringstream late_provider_rows;
    for (std::size_t index = 0U; index < 1'100U; ++index) {
        late_provider_rows
            << "late-provider-claim-" << index << "\tlate-provider-"
            << index
            << "\t1\tknown\t1\t1\tknown\t1\t1\tknown\t0\t0\tknown\t0\t0\t"
               "known\t0\t0\ttrue\tfalse\ttrue\ttrue\ttrue\t"
               "principal-first\testimated\tTEST-SOURCE\n";
    }
    many_late_providers += late_provider_rows.str();
    write_text(fixture / "provider_claims.tsv", many_late_providers);
    refresh_bound_hash(fixture, "provider_claims", "provider_claims.tsv");
    std::string many_late_provider_cash =
        read_text(fixture / "scenario_entries.tsv");
    std::ostringstream late_provider_cash_rows;
    for (std::size_t index = 0U; index < 1'100U; ++index) {
        late_provider_cash_rows
            << "failure-with-provider\tlate-provider-cash-" << index
            << "\tlate-provider-cash-fact-" << index
            << "\tlate-provider-payment-" << index
            << "\tguarantee-principal-cash\t12\t0\tknown\t0\t0\t"
               "estimated\tTEST-SOURCE\tlate-provider-claim-"
            << index << '\n';
    }
    many_late_provider_cash += late_provider_cash_rows.str();
    write_text(fixture / "scenario_entries.tsv", many_late_provider_cash);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage bounded_late_providers =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(bounded_late_providers.package_integrity &&
            !bounded_late_providers.core_config_ready &&
            bounded_late_providers.blockers.size() <= 1'024U &&
            has_blocker(bounded_late_providers,
                "additional blockers omitted after report cap:") &&
            has_blocker(bounded_late_providers,
                "decision-cut core is blocked by UNKNOWN entry timing"),
        "thousands of decision-cut guarantee rows with later provider terms remain an honest bounded incomplete report instead of exhausting the critical-blocker reserve");

    fixture = temporary_root / "incomplete-cash-path";
    copy_fixture(source, fixture);
    std::string scenarios = read_text(fixture / "scenarios.tsv");
    replace_first_data_cell(scenarios, "cash_path_status", "incomplete");
    write_text(fixture / "scenarios.tsv", scenarios);
    refresh_bound_hash(fixture, "scenarios", "scenarios.tsv");
    const cf::ClaimLedgerPackage incomplete =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    std::ostringstream incomplete_report;
    cf::print_claim_ledger_package_report(incomplete_report, incomplete);
    const std::string incomplete_output = incomplete_report.str();
    check(incomplete.evaluation.has_value() &&
            !incomplete.evaluation->readiness.expected_cash_ready &&
            !incomplete.expected_return_admissible &&
            !incomplete.observation_admissible &&
            incomplete_output.find(
                "expected_buyer_cash_outflow_t0_million=UNKNOWN") !=
                std::string::npos &&
            incomplete_output.find(
                "expected_terminal_receipts_million=UNKNOWN") !=
                std::string::npos &&
            incomplete_output.find(
                "expected_total_receipts_million=UNKNOWN") !=
                std::string::npos &&
            incomplete_output.find(
                "expected_provider_claim_generated_million=UNKNOWN") !=
                std::string::npos,
        "an explicit incomplete cash-path assertion blocks expected-return admission");

    fixture = temporary_root / "calendar-horizon-mismatch";
    copy_fixture(source, fixture);
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "timeline.horizon_date=2027-01-01",
        "timeline.horizon_date=2027-02-01");
    write_text(fixture / "claim.cfg", config);
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package_config(
                fixture / "claim.cfg");
        },
        "a horizon date inconsistent with the monthly period origin is rejected");

    fixture = temporary_root / "negative-package-discount-rate";
    copy_fixture(source, fixture);
    config = read_text(fixture / "claim.cfg");
    replace_once(config,
        "valuation.annual_effective_discount_rate_lower=0.10",
        "valuation.annual_effective_discount_rate_lower=-0.5");
    replace_once(config,
        "valuation.annual_effective_discount_rate_upper=0.10",
        "valuation.annual_effective_discount_rate_upper=-0.5");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackageConfig negative_rate =
        cf::load_claim_ledger_package_config(fixture / "claim.cfg");
    check(exact_value(negative_rate.annual_effective_discount_rate.value,
              -0.5),
        "the package schema preserves annual effective rates between minus one and zero");

    fixture = temporary_root / "tiny-bounded-discount-rate";
    copy_fixture(source, fixture);
    config = read_text(fixture / "claim.cfg");
    replace_once(config,
        "valuation.annual_effective_discount_rate_status=known",
        "valuation.annual_effective_discount_rate_status=bounded");
    replace_once(config,
        "valuation.annual_effective_discount_rate_lower=0.10",
        "valuation.annual_effective_discount_rate_lower=0.0000001");
    replace_once(config,
        "valuation.annual_effective_discount_rate_upper=0.10",
        "valuation.annual_effective_discount_rate_upper=0.0000002");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackage tiny_rate =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    std::ostringstream tiny_rate_report;
    cf::print_claim_ledger_package_report(tiny_rate_report, tiny_rate);
    check(tiny_rate_report.str().find(
              "annual_effective_discount_rate=BOUNDED[1e-07,2e-07]") !=
            std::string::npos,
        "report formatting preserves tiny nonzero uncertainty instead of rounding both endpoints to zero");

    fixture = temporary_root / "controlled-evidenced-candidate";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    const cf::ClaimLedgerPackage controlled =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(controlled.evaluation.has_value() &&
            controlled.evaluation->readiness.rate_preimage_ready &&
            controlled.expected_return_admissible &&
            controlled.npv_admissible &&
            controlled.observation_admissible,
        "an evidenced controlled candidate with a settlement-observed common closing passes the return, NPV, and observation boundaries");

    fixture = temporary_root / "scenario-primary-close-period-mismatch";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    common_entries = read_text(fixture / "common_entries.tsv");
    const std::size_t common_header_end = common_entries.find('\n');
    if (common_header_end == std::string::npos) {
        throw std::runtime_error("common entries header is missing");
    }
    std::string scenario_closes =
        read_text(fixture / "scenario_entries.tsv");
    scenario_closes += scenario_closing_rows(common_entries,
        "performing-maturity", "performing-primary", 0U);
    scenario_closes += scenario_closing_rows(common_entries,
        "failure-with-provider", "failure-primary", 1U);
    write_text(fixture / "common_entries.tsv",
        common_entries.substr(0U, common_header_end + 1U));
    write_text(fixture / "scenario_entries.tsv", scenario_closes);
    refresh_bound_hash(fixture, "common_entries", "common_entries.tsv");
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage mismatched_primary_close =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(mismatched_primary_close.evaluation.has_value() &&
            mismatched_primary_close.evaluation->readiness.expected_cash_ready &&
            mismatched_primary_close.evaluation->readiness.rate_preimage_ready &&
            !mismatched_primary_close.expected_return_admissible &&
            has_blocker(mismatched_primary_close,
                "at least one scenario path's earliest selected buyer-price"),
        "one scenario's period-zero close cannot mask another scenario's primary funding in a later monthly bucket");

    fixture = temporary_root / "scenario-specific-anchor-funding";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    common_entries = read_text(fixture / "common_entries.tsv");
    std::string scenario_anchor_funding =
        read_text(fixture / "scenario_entries.tsv");
    scenario_anchor_funding += scenario_closing_rows(common_entries,
        "performing-maturity", "additional-anchor", 0U);
    scenario_anchor_funding +=
        "performing-maturity\tadditional-principal-due\t"
        "additional-principal-due\tadditional-maturity\tprincipal-due\t12\t0\t"
        "known\t10\t10\testimated\tTEST-SOURCE\tNONE\n"
        "performing-maturity\tadditional-principal-cash\t"
        "additional-principal-cash\tadditional-maturity\tprincipal-cash\t12\t0\t"
        "known\t10\t10\testimated\tTEST-SOURCE\tNONE\n";
    write_text(fixture / "scenario_entries.tsv", scenario_anchor_funding);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "claim.contractual_face_amount_lower_million=10",
        "claim.contractual_face_amount_lower_million=20");
    replace_once(config, "claim.contractual_face_amount_upper_million=10",
        "claim.contractual_face_amount_upper_million=20");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackage path_dependent_anchor =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(path_dependent_anchor.expected_return_admissible &&
            path_dependent_anchor.npv_admissible &&
            !path_dependent_anchor.observation_admissible &&
            has_blocker(path_dependent_anchor,
                "no scenario-specific funding in its anchor bucket"),
        "scenario-dependent same-month funding can be projected but cannot be promoted to one observed common closing");

    fixture = temporary_root / "provider-known-after-guarantee-cash";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string provider_claims =
        read_text(fixture / "provider_claims.tsv");
    replace_first_data_cell(provider_claims, "known_at_period", "1");
    write_text(fixture / "provider_claims.tsv", provider_claims);
    refresh_bound_hash(fixture, "provider_claims", "provider_claims.tsv");
    const cf::ClaimLedgerPackage late_provider_terms =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(!late_provider_terms.core_config_ready &&
            !late_provider_terms.evaluation.has_value() &&
            !late_provider_terms.expected_return_admissible &&
            has_blocker(late_provider_terms,
                "guarantee cash references provider terms unavailable at the decision period"),
        "decision-known guarantee cash with only post-decision provider terms loads as explicit evidentiary incompleteness instead of a malformed package");

    fixture = temporary_root / "estimated-closing-price";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    common_entries = read_text(fixture / "common_entries.tsv");
    replace_first_data_cell(common_entries, "input_status", "estimated");
    write_text(fixture / "common_entries.tsv", common_entries);
    refresh_bound_hash(fixture, "common_entries", "common_entries.tsv");
    const cf::ClaimLedgerPackage estimated_closing_price =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(estimated_closing_price.expected_return_admissible &&
            estimated_closing_price.npv_admissible &&
            !estimated_closing_price.observation_admissible &&
            has_blocker(estimated_closing_price,
                "observed buyer cash facts"),
        "an evidenced price estimate may support a projected return but cannot be labelled an observed closing");

    fixture = temporary_root / "later-closing-price-correction";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    common_entries = read_text(fixture / "common_entries.tsv");
    common_entries +=
        "closing-buyer-price-correction\tclosing-buyer-price\t"
        "closing-2026-01-01\tbuyer-price\t0\t1\tknown\t8\t8\t"
        "observed\tTEST-SOURCE\tNONE\n";
    write_text(fixture / "common_entries.tsv", common_entries);
    refresh_bound_hash(fixture, "common_entries", "common_entries.tsv");
    const cf::ClaimLedgerPackage corrected_closing_price =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(corrected_closing_price.expected_return_admissible &&
            corrected_closing_price.npv_admissible &&
            !corrected_closing_price.observation_admissible &&
            !corrected_closing_price.full_path_evaluation_available,
        "a later retained correction leaves the frozen ex-ante return intact but removes stale market-observation status");

    fixture = temporary_root / "later-principal-cash-actual";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string scenario_actuals =
        read_text(fixture / "scenario_entries.tsv");
    scenario_actuals +=
        "performing-maturity\tsuccess-principal-cash-actual\t"
        "success-principal-cash\tmaturity-success\tprincipal-cash\t12\t1\t"
        "known\t9\t9\tobserved\tTEST-SOURCE\tNONE\n";
    write_text(fixture / "scenario_entries.tsv", scenario_actuals);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackageWithPathEvidence
        later_principal_actual_with_evidence =
            cf::load_claim_ledger_package_with_full_path_evidence(
                fixture / "claim.cfg", "performing-maturity");
    const cf::ClaimLedgerPackage& later_principal_actual =
        later_principal_actual_with_evidence.package;
    check(later_principal_actual.expected_return_admissible &&
            later_principal_actual.full_path_evaluation_available &&
            later_principal_actual.evaluation.has_value() &&
            later_principal_actual.full_evaluation.has_value() &&
            exact_value(later_principal_actual.evaluation->scenarios[0U]
                            .decision_path.periods[12U]
                            .principal_cash_million,
                10.0) &&
            exact_value(later_principal_actual.full_evaluation->scenarios[0U]
                            .decision_path.periods[12U]
                            .principal_cash_million,
                10.0) &&
            exact_value(later_principal_actual.full_evaluation->scenarios[0U]
                            .full_path.periods[12U]
                            .principal_cash_million,
                9.0),
        "later actuals replace one economic fact exactly once in the separate full path without changing the admitted decision path");
    const cf::ClaimLedgerPathEvidenceSnapshot& actual_evidence =
        later_principal_actual_with_evidence.full_path;
    const auto selected_actual = std::find_if(
        actual_evidence.selected_entries.begin(),
        actual_evidence.selected_entries.end(), [](const auto& evidence) {
            return evidence.entry.economic_fact_id ==
                "success-principal-cash";
        });
    const std::size_t selected_actual_count = static_cast<std::size_t>(
        std::count_if(actual_evidence.selected_entries.begin(),
            actual_evidence.selected_entries.end(), [](const auto& evidence) {
                return evidence.entry.economic_fact_id ==
                    "success-principal-cash";
            }));
    check(selected_actual != actual_evidence.selected_entries.end() &&
            selected_actual_count == 1U &&
            selected_actual->entry.entry_id ==
                "success-principal-cash-actual" &&
            selected_actual->scenario_id.has_value() &&
            *selected_actual->scenario_id == "performing-maturity" &&
            selected_actual->entry.known_at_period == 1U &&
            selected_actual->input_status ==
                cf::ClaimLedgerInputStatus::Observed &&
            selected_actual->source.source_record_id == "TEST-SOURCE" &&
            selected_actual->source.record_date == "2026-01-01" &&
            selected_actual->source.retained_copy_verified &&
            actual_evidence.full_evaluation_scenario_index <
                later_principal_actual.full_evaluation->scenarios.size() &&
            exact_value(later_principal_actual.full_evaluation->scenarios[
                            actual_evidence.full_evaluation_scenario_index]
                            .full_path.periods[12U]
                            .principal_cash_million,
                9.0),
        "the one-scenario evidence snapshot retains the authoritative latest actual, status, source date, and evaluated-path binding");
    const auto selected_common = std::find_if(
        actual_evidence.selected_entries.begin(),
        actual_evidence.selected_entries.end(), [](const auto& evidence) {
            return evidence.entry.economic_fact_id ==
                "closing-buyer-price";
        });
    const cf::ClaimLedgerPackageWithPathEvidence failure_path_evidence =
        cf::load_claim_ledger_package_with_full_path_evidence(
            fixture / "claim.cfg", "failure-with-provider");
    check(selected_common != actual_evidence.selected_entries.end() &&
            !selected_common->scenario_id.has_value() &&
            std::all_of(actual_evidence.selected_entries.begin(),
                actual_evidence.selected_entries.end(),
                [](const auto& evidence) {
                    return !evidence.scenario_id.has_value() ||
                        *evidence.scenario_id == "performing-maturity";
                }) &&
            std::all_of(failure_path_evidence.full_path.selected_entries.begin(),
                failure_path_evidence.full_path.selected_entries.end(),
                [](const auto& evidence) {
                    return !evidence.scenario_id.has_value() ||
                        *evidence.scenario_id == "failure-with-provider";
                }) &&
            std::none_of(
                failure_path_evidence.full_path.selected_entries.begin(),
                failure_path_evidence.full_path.selected_entries.end(),
                [](const auto& evidence) {
                    return evidence.entry.entry_id ==
                        "success-principal-cash-actual";
                }),
        "selected-entry scope distinguishes common facts and prevents cross-scenario actual leakage");
    check(actual_evidence.contractual_face_source.record_date ==
                "2026-01-01" &&
            actual_evidence.contractual_face_source.retained_copy_verified &&
            actual_evidence.cash_path_status.source.record_date ==
                "2026-01-01" &&
            actual_evidence.cash_path_status.source.retained_copy_verified &&
            !actual_evidence.provider_terms.empty() &&
            actual_evidence.provider_terms.front()
                .source.retained_copy_verified &&
            !actual_evidence.covenant_events.empty() &&
            std::all_of(actual_evidence.covenant_events.begin(),
                actual_evidence.covenant_events.end(),
                [](const auto& evidence) {
                    return evidence.source.record_date == "2026-01-01" &&
                        evidence.source.retained_copy_verified;
                }) &&
            std::none_of(actual_evidence.selected_entries.begin(),
                actual_evidence.selected_entries.end(),
                [](const auto& evidence) {
                    return evidence.entry.entry_id ==
                        "failure-principal-writeoff";
                }),
        "scalar, scenario-status, provider, and selected-scenario covenant provenance remains hash-bound without cross-scenario leakage");

    fixture = temporary_root / "later-only-conversion-metadata";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "claim.conversion_unit_label=not-applicable",
        "claim.conversion_unit_label=synthetic-equity-unit");
    replace_once(config, "claim.conversion_unit_basis=not-applicable",
        "claim.conversion_unit_basis=one synthetic unit per recorded conversion unit");
    write_text(fixture / "claim.cfg", config);
    scenario_actuals = read_text(fixture / "scenario_entries.tsv");
    scenario_actuals +=
        "performing-maturity\tlater-zero-conversion-principal\t"
        "later-zero-conversion-principal\tlater-zero-conversion\t"
        "conversion-principal-extinguishment\t12\t1\tknown\t0\t0\t"
        "observed\tTEST-SOURCE\tNONE\n"
        "performing-maturity\tlater-zero-conversion-units\t"
        "later-zero-conversion-units\tlater-zero-conversion\t"
        "conversion-units\t12\t1\tknown\t0\t0\tobserved\t"
        "TEST-SOURCE\tNONE\n";
    write_text(fixture / "scenario_entries.tsv", scenario_actuals);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage later_only_conversion =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(later_only_conversion.evaluation.has_value() &&
            later_only_conversion.expected_return_admissible &&
            later_only_conversion.full_path_evaluation_available &&
            later_only_conversion.core_config.has_value() &&
            later_only_conversion.full_core_config.has_value() &&
            later_only_conversion.core_config->conversion_unit_label ==
                "not-applicable" &&
            later_only_conversion.full_core_config->conversion_unit_label ==
                "synthetic-equity-unit",
        "later-only conversion metadata remains in the full backtest state without invalidating or rewriting the decision-time ledger");

    fixture = temporary_root / "later-known-opening-balance";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "claim.opening_principal_lower_million=0",
        "claim.opening_principal_lower_million=9");
    replace_once(config, "claim.opening_principal_upper_million=0",
        "claim.opening_principal_upper_million=9");
    replace_once(config, "claim.opening_principal_known_at_period=0",
        "claim.opening_principal_known_at_period=1");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackage later_opening_balance =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(later_opening_balance.evaluation.has_value() &&
            later_opening_balance.core_config.has_value() &&
            later_opening_balance.core_config->opening_principal_million
                    .status == cf::ClaimLedgerValueStatus::Unknown &&
            !later_opening_balance.expected_return_admissible &&
            !later_opening_balance.full_path_evaluation_available,
        "a post-decision opening balance is represented as decision-time incompleteness even when the retained full state is internally inconsistent");

    fixture = temporary_root / "later-new-primary-closing-fact";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    common_entries = read_text(fixture / "common_entries.tsv");
    common_entries +=
        "late-discovered-buyer-cost\tlate-discovered-buyer-cost\t"
        "closing-2026-01-01\tbuyer-direct-cost\t1\t1\tknown\t0.1\t"
        "0.1\tobserved\tTEST-SOURCE\tNONE\n";
    write_text(fixture / "common_entries.tsv", common_entries);
    refresh_bound_hash(fixture, "common_entries", "common_entries.tsv");
    const cf::ClaimLedgerPackage expanded_primary_close =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(expanded_primary_close.expected_return_admissible &&
            expanded_primary_close.npv_admissible &&
            !expanded_primary_close.observation_admissible,
        "a later-discovered economic fact assigned to the primary closing removes stale observation status even when its row is dated to a later model period");

    fixture = temporary_root / "pre-settlement-closing-source";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(
        fixture, "TEST-SOURCE", "2025-12-31");
    const cf::ClaimLedgerPackage pre_settlement_closing =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(pre_settlement_closing.expected_return_admissible &&
            pre_settlement_closing.npv_admissible &&
            !pre_settlement_closing.observation_admissible &&
            has_blocker(pre_settlement_closing,
                "settlement-dated transaction evidence"),
        "a pre-settlement contract may evidence an ex-ante return but cannot prove that the recorded closing amounts settled");

    fixture = temporary_root / "non-transaction-closing-source";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2026-01-01",
        false, "retained_sources/test-source.txt", "A", "PR-M");
    const cf::ClaimLedgerPackage non_transaction_closing =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(non_transaction_closing.expected_return_admissible &&
            non_transaction_closing.npv_admissible &&
            !non_transaction_closing.observation_admissible,
        "market-derived evidence may support projections but cannot prove actual settlement cash");

    fixture = temporary_root / "principal-due-after-maturity";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "timeline.horizon_date=2027-01-01",
        "timeline.horizon_date=2028-01-01");
    replace_once(config, "timeline.horizon_period=12",
        "timeline.horizon_period=24");
    write_text(fixture / "claim.cfg", config);
    std::string delayed_scenario_entries =
        read_text(fixture / "scenario_entries.tsv");
    replace_all(delayed_scenario_entries, "\t12\t0\t", "\t24\t0\t");
    write_text(fixture / "scenario_entries.tsv", delayed_scenario_entries);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage delayed_due =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(delayed_due.evaluation.has_value() &&
            delayed_due.evaluation->readiness.expected_cash_ready &&
            delayed_due.evaluation->readiness.rate_preimage_ready &&
            !delayed_due.expected_return_admissible &&
            !delayed_due.npv_admissible &&
            has_blocker(delayed_due,
                "surviving obligations must be due from the maturity bucket"),
        "a mechanically resolved cash path cannot postpone contractual principal due beyond the stated maturity");

    fixture = temporary_root / "maturity-prepayment-without-due";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string undued_maturity =
        read_text(fixture / "scenario_entries.tsv");
    remove_unique_line_containing(
        undued_maturity, "\tsuccess-principal-due\t");
    write_text(fixture / "scenario_entries.tsv", undued_maturity);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage maturity_prepayment =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(maturity_prepayment.evaluation.has_value() &&
            maturity_prepayment.evaluation->readiness.expected_cash_ready &&
            !maturity_prepayment.expected_return_admissible &&
            has_blocker(maturity_prepayment,
                "surviving obligations must be due from the maturity bucket"),
        "same-period principal cash cannot hide that the contractual balance was never made due at maturity");

    fixture = temporary_root / "maturity-writeoff-without-due";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    undued_maturity = read_text(fixture / "scenario_entries.tsv");
    remove_unique_line_containing(
        undued_maturity, "\tsuccess-principal-due\t");
    replace_once(undued_maturity,
        "success-principal-cash\tsuccess-principal-cash\t"
        "maturity-success\tprincipal-cash",
        "success-principal-writeoff\tsuccess-principal-writeoff\t"
        "maturity-success\tprincipal-writeoff");
    write_text(fixture / "scenario_entries.tsv", undued_maturity);
    refresh_bound_hash(
        fixture, "scenario_entries", "scenario_entries.tsv");
    const cf::ClaimLedgerPackage maturity_writeoff =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(maturity_writeoff.evaluation.has_value() &&
            maturity_writeoff.evaluation->readiness.expected_cash_ready &&
            !maturity_writeoff.expected_return_admissible,
        "a maturity-bucket writeoff cannot suppress the missing due event and its associated default trigger");

    fixture = temporary_root / "stress-opening-balance";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "claim.opening_principal_input_status=derived",
        "claim.opening_principal_input_status=stress");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackage stress_opening =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(stress_opening.evaluation.has_value() &&
            stress_opening.evaluation->readiness.rate_preimage_ready &&
            !stress_opening.expected_return_admissible &&
            !stress_opening.npv_admissible,
        "a stress-classified opening balance cannot enter an admitted expected return");

    fixture = temporary_root / "stress-discount-rate";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    config = read_text(fixture / "claim.cfg");
    replace_once(config,
        "valuation.annual_effective_discount_rate_input_status=derived",
        "valuation.annual_effective_discount_rate_input_status=stress");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackage stress_discount =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(stress_discount.expected_return_admissible &&
            stress_discount.evaluation.has_value() &&
            stress_discount.evaluation->readiness.npv_ready &&
            !stress_discount.npv_admissible,
        "an expected-cash rate preimage remains admissible while a stress discount rate keeps NPV non-admissible");

    fixture = temporary_root / "unknown-contractual-maturity";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    config = read_text(fixture / "claim.cfg");
    replace_once(config, "timeline.maturity_date_status=known",
        "timeline.maturity_date_status=unknown");
    replace_once(config, "timeline.maturity_date=2027-01-01",
        "timeline.maturity_date=UNKNOWN");
    write_text(fixture / "claim.cfg", config);
    const cf::ClaimLedgerPackage unknown_maturity =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(unknown_maturity.evaluation.has_value() &&
            unknown_maturity.evaluation->readiness.rate_preimage_ready &&
            !unknown_maturity.expected_return_admissible &&
            !unknown_maturity.npv_admissible,
        "a mechanically complete cash path cannot replace an unknown contractual maturity in admitted instrument terms");

    fixture = temporary_root / "estimated-completeness-candidate";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    scenarios = read_text(fixture / "scenarios.tsv");
    replace_first_data_cell(
        scenarios, "cash_path_input_status", "estimated");
    write_text(fixture / "scenarios.tsv", scenarios);
    refresh_bound_hash(fixture, "scenarios", "scenarios.tsv");
    const cf::ClaimLedgerPackage estimated_completeness =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(estimated_completeness.evaluation.has_value() &&
            estimated_completeness.evaluation->readiness.rate_preimage_ready &&
            !estimated_completeness.expected_return_admissible &&
            !estimated_completeness.observation_admissible &&
            has_blocker(estimated_completeness,
                "no admissible completeness attestation"),
        "an estimated assertion cannot attest that a scenario cash path is complete");

    fixture = temporary_root / "duplicate-manifest-source-id";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(
        fixture, "TEST-SOURCE", "2025-12-31", true);
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "duplicate source IDs in the parent manifest are rejected");

    fixture = temporary_root / "duplicate-retained-evidence-path";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    std::string manifest =
        read_text(fixture.parent_path() / "source_manifest.tsv");
    const std::size_t row_start = manifest.find('\n') + 1U;
    std::string duplicate_path_row = manifest.substr(row_start);
    replace_once(duplicate_path_row, "TEST-SOURCE\t", "TEST-SOURCE-2\t");
    manifest += duplicate_path_row;
    bind_parent_manifest(fixture, manifest);
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "one retained file cannot be re-hashed under two different manifest source IDs");

    fixture = temporary_root / "empty-retained-evidence";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2026-01-01",
        false, "retained_sources/test-source.txt", "A", "CTR",
        "2026-08-30", "");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "an empty retained file cannot launder absence into underwriting evidence");

    fixture = temporary_root / "blank-retained-evidence";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2026-01-01",
        false, "retained_sources/test-source.txt", "A", "CTR",
        "2026-08-30", "\xEF\xBB\xBF \r\n\t");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "BOM and whitespace alone cannot become retained underwriting evidence");

    fixture = temporary_root / "absence-only-controlled-candidate";
    make_controlled_fixture(source, fixture, "NO_PUBLIC_SOURCE");
    const cf::ClaimLedgerPackageWithPathEvidence absence_only_with_evidence =
        cf::load_claim_ledger_package_with_full_path_evidence(
            fixture / "claim.cfg", "failure-with-provider");
    const cf::ClaimLedgerPackage& absence_only =
        absence_only_with_evidence.package;
    check(absence_only.core_config_ready &&
            absence_only.evaluation.has_value() &&
            absence_only.evaluation->readiness.rate_preimage_ready &&
            !absence_only.expected_return_admissible &&
            !absence_only.observation_admissible,
        "mechanically complete controlled candidates with absence-only provenance cannot be admitted");
    check(!absence_only_with_evidence.full_path.selected_entries.empty() &&
            std::all_of(
                absence_only_with_evidence.full_path.selected_entries.begin(),
                absence_only_with_evidence.full_path.selected_entries.end(),
                [](const auto& evidence) {
                    return evidence.source.source_record_id ==
                            "NO_PUBLIC_SOURCE" &&
                        !evidence.source.record_date.has_value() &&
                        !evidence.source.evidence_class.has_value() &&
                        !evidence.source.provenance_tag.has_value() &&
                        !evidence.source.retained_copy_verified;
                }),
        "the reserved public-source absence marker remains representable in a full-path snapshot without invented metadata");

    fixture = temporary_root / "missing-ordinary-source-metadata";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    bind_parent_manifest(fixture, kTestSourceManifestHeader);
    expect_invalid_argument_containing([&fixture] {
        (void)cf::load_claim_ledger_package_with_full_path_evidence(
            fixture / "claim.cfg", "failure-with-provider");
    }, "does not resolve in parent source_manifest.tsv",
        "an ordinary source cannot reach a full-path snapshot without manifest metadata");

    fixture = temporary_root / "laundered-absence-marker";
    make_controlled_fixture(source, fixture, "NO_PUBLIC_SOURCE");
    write_retained_test_manifest(
        fixture, "NO_PUBLIC_SOURCE", "2025-12-31");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a retained row cannot launder an absence marker into evidence");

    fixture = temporary_root / "post-decision-source";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2026-01-02");
    const cf::ClaimLedgerPackage hindsight =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(hindsight.evaluation.has_value() &&
            hindsight.evaluation->readiness.rate_preimage_ready &&
            !hindsight.expected_return_admissible &&
            !hindsight.npv_admissible &&
            !hindsight.observation_admissible &&
            has_blocker(hindsight, "dated no later than the decision date"),
        "a retained publication dated after the decision cannot support an ex-ante return");

    fixture = temporary_root / "claim-only-source";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2025-12-31",
        false, "retained_sources/test-source.txt", "C", "CLM");
    const cf::ClaimLedgerPackage claim_only =
        cf::load_claim_ledger_package(fixture / "claim.cfg");
    check(claim_only.evaluation.has_value() &&
            claim_only.evaluation->readiness.rate_preimage_ready &&
            !claim_only.expected_return_admissible &&
            !claim_only.npv_admissible &&
            !claim_only.observation_admissible,
        "retained sponsor-claim bytes remain attributable evidence but cannot become a base expected return");

    fixture = temporary_root / "impossible-source-chronology";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2026-01-01",
        false, "retained_sources/test-source.txt", "A", "CTR",
        "2025-12-31");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a source cannot be accessed before its stated record date");

    constexpr std::array<std::string_view, 5U> substantive_manifest_fields{{
        "distribution_channel", "originating_record", "source_uri",
        "claim_scope", "limitations",
    }};
    for (const std::string_view field : substantive_manifest_fields) {
        fixture = temporary_root /
            ("blank-manifest-" + std::string(field));
        make_controlled_fixture(source, fixture, "TEST-SOURCE");
        std::string blank_manifest =
            read_text(fixture.parent_path() / "source_manifest.tsv");
        replace_first_data_cell(blank_manifest, field, " ");
        bind_parent_manifest(fixture, blank_manifest);
        expect_invalid_argument(
            [&fixture] {
                (void)cf::load_claim_ledger_package(
                    fixture / "claim.cfg");
            },
            "required source-manifest metadata cannot be blank or whitespace-only");
    }

    fixture = temporary_root / "nonportable-retained-path";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2025-12-31",
        false, "retained_sources/../retained_sources/test-source.txt");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a canonicalizable but non-portable retained-evidence path is rejected");

    fixture = temporary_root / "self-referential-retained-path";
    make_controlled_fixture(source, fixture, "TEST-SOURCE");
    write_retained_test_manifest(fixture, "TEST-SOURCE", "2025-12-31",
        false, "self-referential-retained-path/common_entries.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a claim-ledger artifact cannot serve as its own retained external evidence");

    fixture = temporary_root / "bad-header";
    copy_fixture(source, fixture);
    scenarios = read_text(fixture / "scenarios.tsv");
    replace_once(scenarios, "scenario_id\tprobability_status",
        "scenario\tprobability_status");
    write_text(fixture / "scenarios.tsv", scenarios);
    refresh_bound_hash(fixture, "scenarios", "scenarios.tsv");
    expect_invalid_argument(
        [&fixture] {
            (void)cf::load_claim_ledger_package(fixture / "claim.cfg");
        },
        "a hash-consistent but semantically different TSV header is rejected");
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: claim_ledger_package_tests "
                     "<synthetic-claim.cfg> <liberation-claim.cfg> "
                     "<solar-claim.cfg>\n";
        return 2;
    }
    const std::filesystem::path temporary_root =
        std::filesystem::current_path() / "claim-ledger-package-tests.tmp";
    try {
        std::filesystem::remove_all(temporary_root);
        std::filesystem::create_directories(temporary_root);
        test_complete_synthetic(argv[1]);
        test_incomplete_public(argv[2], argv[3]);
        test_package_adversaries(argv[1], temporary_root);
        std::filesystem::remove_all(temporary_root);
    } catch (const std::exception& error) {
        std::filesystem::remove_all(temporary_root);
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        ++failures;
    }

    if (failures != 0) {
        std::cerr << failures << " claim-ledger-package test(s) failed\n";
        return 1;
    }
    std::cout << "all claim-ledger-package tests passed\n";
    return 0;
}
