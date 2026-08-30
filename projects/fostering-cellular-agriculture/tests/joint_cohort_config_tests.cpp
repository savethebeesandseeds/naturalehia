// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/evidence_gate.hpp>
#include <naturalehia/cellular_finance/joint_cohort_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

class NonCanonicalPunctuation final : public std::numpunct<char> {
protected:
    [[nodiscard]] char do_decimal_point() const override { return ','; }
    [[nodiscard]] std::string do_truename() const override { return "yes"; }
    [[nodiscard]] std::string do_falsename() const override { return "no"; }
};

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_invalid(
    const std::function<void()>& operation, std::string_view message) {
    try {
        operation();
        check(false, message);
    } catch (const std::invalid_argument&) {
        // Expected.
    } catch (...) {
        check(false, message);
    }
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not read test file");
    }
    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void write_text(
    const std::filesystem::path& path, std::string_view text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not write test file");
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!output) {
        throw std::runtime_error("could not finish writing test file");
    }
}

void replace_once(
    std::string& text, std::string_view old_value,
    std::string_view new_value) {
    const std::size_t position = text.find(old_value);
    if (position == std::string::npos) {
        throw std::runtime_error("test replacement target not found");
    }
    text.replace(position, old_value.size(), new_value);
}

void replace_key(
    std::string& text, std::string_view key, std::string_view value) {
    const std::string prefix = std::string(key) + '=';
    const std::size_t position = text.find(prefix);
    if (position == std::string::npos) {
        throw std::runtime_error("test key not found");
    }
    const std::size_t begin = position + prefix.size();
    const std::size_t end = text.find('\n', begin);
    text.replace(begin,
        end == std::string::npos ? std::string::npos : end - begin,
        value);
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

[[nodiscard]] std::filesystem::path copy_fixture(
    const std::filesystem::path& fixture,
    const std::filesystem::path& workspace,
    std::string_view name) {
    const std::filesystem::path destination = workspace / name;
    std::filesystem::copy(fixture, destination,
        std::filesystem::copy_options::recursive);
    return destination;
}

void update_ledger_hash(const std::filesystem::path& directory) {
    std::string config = read_text(directory / "cohort.cfg");
    replace_key(config, "file.ledger.sha256",
        cf::sha256_file_lower_hex(directory / "cohort.tsv"));
    write_text(directory / "cohort.cfg", config);
}

void test_valid_roundtrip(
    const std::filesystem::path& fixture,
    const std::filesystem::path& workspace) {
    const cf::JointCohortPackage package = cf::load_joint_cohort_package(
        fixture / "cohort.cfg");
    check(package.observations.size() == 22U &&
            package.config.analysis.population_frame_count == 22U &&
            package.portfolio.joint_scenarios.size() == 4U,
        "strict package loader retains bound portfolio and every raw row");
    check(cf::sha256_bytes_lower_hex(read_text(fixture / "portfolio.cfg")) ==
                package.config.portfolio_file.sha256 &&
            cf::sha256_bytes_lower_hex(read_text(fixture / "cohort.tsv")) ==
                package.config.ledger_file.sha256,
        "in-memory SHA-256 helper binds the exact immutable bytes supplied to each parser");
    const cf::PortfolioConfig path_portfolio =
        cf::load_portfolio_config(fixture / "portfolio.cfg");
    const cf::PortfolioConfig bytes_portfolio = cf::load_portfolio_config_bytes(
        read_text(fixture / "portfolio.cfg"));
    std::ostringstream path_portfolio_output;
    std::ostringstream bytes_portfolio_output;
    cf::print_normalized_portfolio_config(
        path_portfolio_output, path_portfolio);
    cf::print_normalized_portfolio_config(
        bytes_portfolio_output, bytes_portfolio);
    check(path_portfolio_output.str() == bytes_portfolio_output.str(),
        "portfolio byte-snapshot parser is semantically identical to the path loader");

    std::ostringstream config_output;
    const std::locale caller_locale(
        std::locale::classic(), new NonCanonicalPunctuation);
    config_output.imbue(caller_locale);
    config_output << std::fixed << std::hex << std::showbase
                  << std::showpoint << std::showpos << std::uppercase
                  << std::setprecision(3);
    config_output.fill('#');
    config_output.width(37);
    const std::ios_base::fmtflags config_flags = config_output.flags();
    cf::print_normalized_joint_cohort_config(
        config_output, package.config);
    const std::string normalized_config = config_output.str();
    check(config_output.precision() == 3 &&
            config_output.flags() == config_flags &&
            config_output.width() == 37 && config_output.fill() == '#' &&
            config_output.getloc() == caller_locale &&
            normalized_config.find(
                "joint_cohort.confidence_level=0.94999999999999996\n") !=
                std::string::npos &&
            normalized_config.find("candidate_only=true\n") !=
                std::string::npos,
        "normalized config is canonical and restores caller stream state");

    std::ostringstream ledger_output;
    ledger_output.imbue(caller_locale);
    ledger_output << std::fixed << std::hex << std::showpos;
    ledger_output.fill('@');
    ledger_output.width(19);
    const std::ios_base::fmtflags ledger_flags = ledger_output.flags();
    cf::print_normalized_joint_cohort_ledger(
        ledger_output, package.observations);
    check(ledger_output.flags() == ledger_flags &&
            ledger_output.width() == 19 && ledger_output.fill() == '@' &&
            ledger_output.getloc() == caller_locale &&
            ledger_output.str().starts_with("observation_id\tcluster_id") &&
            ledger_output.str().find("\tnot-yet-matured\t") !=
                std::string::npos,
        "normalized ledger is canonical and restores caller stream state");

    const std::filesystem::path copy =
        copy_fixture(fixture, workspace, "roundtrip");
    write_text(copy / "normalized.cfg", normalized_config);
    const cf::JointCohortPackageConfig reloaded =
        cf::load_joint_cohort_config(copy / "normalized.cfg");
    check(reloaded.analysis.id == package.config.analysis.id &&
            reloaded.analysis.population_definition ==
                package.config.analysis.population_definition &&
            reloaded.analysis.confidence_level ==
                package.config.analysis.confidence_level,
        "normalized configuration reloads without semantic drift");

    const cf::JointCohortResult result = cf::evaluate_joint_cohort(
        package.config.analysis, package.portfolio, package.observations);
    check(result.generated_probability_envelope.has_value(),
        "valid fixture produces a replayable generated ambiguity envelope");
    if (result.generated_probability_envelope.has_value()) {
        std::ostringstream generated_output;
        cf::print_normalized_portfolio_ambiguity_config(
            generated_output, *result.generated_probability_envelope);
        const std::filesystem::path generated_path =
            copy / "generated-ambiguity.cfg";
        write_text(generated_path, generated_output.str());
        const cf::PortfolioAmbiguityConfig generated_reloaded =
            cf::load_portfolio_ambiguity_config(generated_path);
        std::ostringstream generated_reprinted;
        cf::print_normalized_portfolio_ambiguity_config(
            generated_reprinted, generated_reloaded);
        check(generated_reprinted.str() == generated_output.str(),
            "generated primary probability envelope round-trips through the independent ambiguity parser");
        const cf::PortfolioAmbiguitySummary replayed =
            cf::evaluate_portfolio_ambiguity(
                package.portfolio, generated_reloaded);
        check(result.financial_ranges.has_value() &&
                replayed.expected_total_draws_million.minimum.value ==
                    result.financial_ranges
                        ->expected_total_draws_million.minimum.value &&
                replayed.expected_total_draws_million.central ==
                    result.financial_ranges
                        ->expected_total_draws_million.central &&
                replayed.expected_total_draws_million.maximum.value ==
                    result.financial_ranges
                        ->expected_total_draws_million.maximum.value,
            "reloaded generated envelope independently reproduces its financial range");
    }

    cf::JointCohortPackageConfig unsafe_text = package.config;
    unsafe_text.analysis.source_note = "not=reloadable";
    expect_invalid([&] {
        std::ostringstream output;
        cf::print_normalized_joint_cohort_config(output, unsafe_text);
    }, "programmatic key-value text containing equals is rejected");

    cf::JointCohortPackageConfig same_path = package.config;
    same_path.ledger_file = same_path.portfolio_file;
    expect_invalid([&] {
        std::ostringstream output;
        cf::print_normalized_joint_cohort_config(output, same_path);
    }, "programmatic normalized config rejects indistinct bound paths");

    std::vector<cf::JointCohortObservation> invalid_enum =
        package.observations;
    invalid_enum.front().status =
        static_cast<cf::JointCohortObservationStatus>(255U);
    expect_invalid([&] {
        std::ostringstream output;
        cf::print_normalized_joint_cohort_ledger(output, invalid_enum);
    }, "normalized ledger rejects invalid programmatic status enums");
    check(cf::to_string(cf::JointCohortObservationStatus::Matured) ==
              "matured" &&
            cf::to_string(
                cf::JointCohortObservationStatus::NotYetMatured) ==
              "not-yet-matured" &&
            cf::to_string(cf::JointCohortObservationStatus::Unresolved) ==
              "unresolved" &&
            cf::to_string(cf::JointCohortObservationStatus::Excluded) ==
              "excluded",
        "all accepted status enum values have exact schema spellings");
}

void test_config_adversaries(
    const std::filesystem::path& fixture,
    const std::filesystem::path& workspace) {
    const auto expect_config_change = [&](std::string_view name,
                                          const auto& mutate,
                                          std::string_view message) {
        const std::filesystem::path copy =
            copy_fixture(fixture, workspace, name);
        std::string config = read_text(copy / "cohort.cfg");
        mutate(config);
        write_text(copy / "cohort.cfg", config);
        expect_invalid([&] {
            static_cast<void>(
                cf::load_joint_cohort_config(copy / "cohort.cfg"));
        }, message);
    };

    expect_config_change("unknown-key", [](std::string& value) {
        value += "unknown.field=value\n";
    }, "unknown config fields are rejected");
    expect_config_change("duplicate-key", [](std::string& value) {
        value += "joint_cohort.id=duplicate\n";
    }, "duplicate config fields are rejected");
    expect_config_change("missing-key", [](std::string& value) {
        remove_key(value, "joint_cohort.population_definition");
    }, "missing required config fields are rejected");
    expect_config_change("nonfinite", [](std::string& value) {
        replace_key(value, "joint_cohort.confidence_level", "nan");
    }, "nonfinite confidence is rejected");
    expect_config_change("false-candidate", [](std::string& value) {
        replace_key(value, "joint_cohort.candidate_only", "false");
    }, "candidate_only=false is rejected");
    expect_config_change("false-synthetic", [](std::string& value) {
        replace_key(value, "joint_cohort.synthetic_inputs", "false");
    }, "synthetic_inputs=false is explicitly rejected");
    expect_config_change("wrong-measure", [](std::string& value) {
        replace_key(value, "joint_cohort.probability_measure", "risk-neutral-Q");
    }, "non-physical probability measure is rejected");
    expect_config_change("wrong-sampling", [](std::string& value) {
        replace_key(value, "joint_cohort.sampling_assumption", "exchangeable");
    }, "non-IID sampling token is rejected");
    expect_config_change("wrong-method", [](std::string& value) {
        replace_key(value, "joint_cohort.interval_method", "wald");
    }, "unknown interval method is rejected");
    expect_config_change("unsafe-path", [](std::string& value) {
        replace_key(value, "file.portfolio.path", "../portfolio.cfg");
    }, "escaping relative paths are rejected");
    expect_config_change("control-path", [](std::string& value) {
        replace_key(value, "file.portfolio.path", "bad\tname.cfg");
    }, "control characters in paths are rejected");
    expect_config_change("same-path", [](std::string& value) {
        replace_key(value, "file.ledger.path", "portfolio.cfg");
    }, "identical declared bound paths are rejected");
    expect_config_change("bad-hash", [](std::string& value) {
        replace_key(value, "file.ledger.sha256",
            std::string(64U, 'A'));
    }, "non-lowercase hash is rejected");
    expect_config_change("bad-frame", [](std::string& value) {
        replace_key(value, "joint_cohort.population_frame_count", "0");
    }, "zero population frame is rejected");
    expect_config_change("unasserted-rule", [](std::string& value) {
        replace_key(value,
            "exclusion_rule.1.outcome_blind_asserted", "false");
    }, "unasserted outcome-blind rule is rejected");
}

void test_bound_and_ledger_adversaries(
    const std::filesystem::path& fixture,
    const std::filesystem::path& workspace) {
    {
        const std::filesystem::path copy =
            copy_fixture(fixture, workspace, "portfolio-drift");
        std::string portfolio = read_text(copy / "portfolio.cfg");
        portfolio += "# hash drift\n";
        write_text(copy / "portfolio.cfg", portfolio);
        expect_invalid([&] {
            static_cast<void>(
                cf::load_joint_cohort_package(copy / "cohort.cfg"));
        }, "portfolio hash drift is rejected");
    }
    {
        const std::filesystem::path copy =
            copy_fixture(fixture, workspace, "ledger-hash-drift");
        std::string ledger = read_text(copy / "cohort.tsv");
        ledger += "\n";
        write_text(copy / "cohort.tsv", ledger);
        expect_invalid([&] {
            static_cast<void>(
                cf::load_joint_cohort_package(copy / "cohort.cfg"));
        }, "raw ledger hash drift is rejected before parsing");
    }

    const auto expect_ledger_change = [&](std::string_view name,
                                          const auto& mutate,
                                          std::string_view message) {
        const std::filesystem::path copy =
            copy_fixture(fixture, workspace, name);
        std::string ledger = read_text(copy / "cohort.tsv");
        mutate(ledger);
        write_text(copy / "cohort.tsv", ledger);
        update_ledger_hash(copy);
        expect_invalid([&] {
            static_cast<void>(
                cf::load_joint_cohort_package(copy / "cohort.cfg"));
        }, message);
    };

    expect_ledger_change("unknown-status", [](std::string& value) {
        replace_once(value, "\tmatured\t", "\tfinal\t");
    }, "unknown ledger statuses are rejected after a valid rehash");
    expect_ledger_change("duplicate-observation", [](std::string& value) {
        replace_once(value, "obs-002", "obs-001");
    }, "duplicate observation IDs are rejected");
    expect_ledger_change("citation-mismatch", [](std::string& value) {
        replace_once(value, "\tNONE\tNONE\n", "\tEV-1\tNONE\n");
    }, "evidence and requirement list presence must match");
    expect_ledger_change("bad-header", [](std::string& value) {
        const std::size_t newline = value.find('\n');
        value.replace(0U, newline, std::string(8193U, 'x'));
    }, "ledger header line length is bounded before schema parsing");
    expect_ledger_change("truncated-frame", [](std::string& value) {
        const std::size_t last_newline = value.rfind('\n', value.size() - 2U);
        value.erase(last_newline + 1U);
    }, "population_frame_count detects a truncated raw ledger");
}

void test_resource_bounds(const std::filesystem::path& workspace) {
    const std::filesystem::path oversized_config =
        workspace / "oversized-config.cfg";
    write_text(oversized_config, std::string(1024U * 1024U + 1U, 'x'));
    expect_invalid([&] {
        static_cast<void>(cf::load_joint_cohort_config(oversized_config));
    }, "configuration byte cap is enforced before parsing");

    const std::filesystem::path long_config_line =
        workspace / "long-config-line.cfg";
    write_text(long_config_line, std::string(4097U, 'x') + "\n");
    expect_invalid([&] {
        static_cast<void>(cf::load_joint_cohort_config(long_config_line));
    }, "configuration line cap is enforced");

    const std::filesystem::path oversized_ledger =
        workspace / "oversized-ledger.tsv";
    write_text(oversized_ledger,
        std::string(32U * 1024U * 1024U + 1U, 'x'));
    expect_invalid([&] {
        static_cast<void>(cf::load_joint_cohort_ledger(oversized_ledger));
    }, "raw ledger byte cap is enforced before hashing or parsing");

    const std::filesystem::path too_many_rows =
        workspace / "too-many-ledger-rows.tsv";
    std::ofstream output(
        too_many_rows, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not create row-cap ledger");
    }
    output
        << "observation_id\tcluster_id\teligible_date\thorizon_end_date\tstatus\t"
           "scenario_id\tclassification_date\texclusion_rule_id\t"
           "evidence_record_ids\trequirement_ids\n";
    constexpr std::string_view repeated_row =
        "same-observation\tsame-cluster\t2025-02-01\t2027-02-01\t"
        "not-yet-matured\tNONE\tNONE\tNONE\tNONE\tNONE\n";
    for (std::size_t row = 0U; row < 100001U; ++row) {
        output << repeated_row;
    }
    output.close();
    if (!output) {
        throw std::runtime_error("could not finish row-cap ledger");
    }
    expect_invalid([&] {
        static_cast<void>(cf::load_joint_cohort_ledger(too_many_rows));
    }, "raw ledger row cap is enforced before aggregate validation");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: joint_cohort_config_tests <fixture-directory>\n";
        return 2;
    }
    try {
        const std::filesystem::path fixture =
            std::filesystem::absolute(argv[1]);
        const auto nonce = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        TempTree temp{std::filesystem::temp_directory_path() /
            ("naturalehia-joint-cohort-tests-" + std::to_string(nonce))};
        std::filesystem::create_directories(temp.root);
        test_valid_roundtrip(fixture, temp.root);
        test_config_adversaries(fixture, temp.root);
        test_bound_and_ledger_adversaries(fixture, temp.root);
        test_resource_bounds(temp.root);
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        ++failures;
    }
    if (failures != 0) {
        std::cerr << failures << " joint-cohort config test(s) failed\n";
        return 1;
    }
    std::cout << "joint-cohort config tests passed\n";
    return 0;
}
