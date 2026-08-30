// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/evidence_gate.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

struct Options {
    std::filesystem::path dossier_path;
    std::filesystem::path manifest_path;
    std::string evaluation_date;
    bool report_only{};
};

[[nodiscard]] bool valid_iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4U] != '-' || value[7U] != '-') {
        return false;
    }
    const auto digit = [value](std::size_t index) {
        return value[index] >= '0' && value[index] <= '9';
    };
    constexpr std::array<std::size_t, 8U> digit_positions{{
        0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U,
    }};
    if (!std::all_of(
            digit_positions.begin(), digit_positions.end(), digit)) {
        return false;
    }
    const int year_value =
        (value[0U] - '0') * 1000 + (value[1U] - '0') * 100 +
        (value[2U] - '0') * 10 + (value[3U] - '0');
    if (year_value < 1900) {
        return false;
    }
    const unsigned int month_value = static_cast<unsigned int>(
        (value[5U] - '0') * 10 + (value[6U] - '0'));
    const unsigned int day_value = static_cast<unsigned int>(
        (value[8U] - '0') * 10 + (value[9U] - '0'));
    return std::chrono::year_month_day{
               std::chrono::year{year_value},
               std::chrono::month{month_value},
               std::chrono::day{day_value}}
        .ok();
}

[[nodiscard]] std::string current_utc_date() {
    const auto day = std::chrono::floor<std::chrono::days>(
        std::chrono::system_clock::now());
    const std::chrono::year_month_day date{day};
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4)
           << static_cast<int>(date.year()) << '-'
           << std::setw(2) << static_cast<unsigned int>(date.month()) << '-'
           << std::setw(2) << static_cast<unsigned int>(date.day());
    return output.str();
}

void print_help(std::ostream& output) {
    output
        << "Usage: naturalehia-evidence-gate <dossier.cfg> "
           "<evidence_manifest.tsv> [--evaluation-date YYYY-MM-DD] "
           "[--report-only]\n\n"
        << "Exit status:\n"
        << "  0  all four project gates passed, the claim-population\n"
        << "     requirement passed, or --report-only was supplied\n"
        << "  1  dossier or manifest is invalid\n"
        << "  2  command-line usage error\n"
        << "  3  valid dossier, but one or more gates failed\n\n"
        << "--report-only changes only the process exit status. It does not\n"
        << "change, suppress, or promote the reported gate result. The default\n"
        << "evaluation date is the current UTC date. An explicit date supports\n"
        << "reproducible historical review and is printed in the report.\n";
}

[[nodiscard]] Options parse_options(int argc, char* argv[]) {
    Options options;
    options.evaluation_date = current_utc_date();
    bool evaluation_date_supplied = false;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help") {
            print_help(std::cout);
            throw std::runtime_error("help requested");
        }
        if (argument == "--report-only") {
            options.report_only = true;
            continue;
        }
        if (argument == "--evaluation-date") {
            if (evaluation_date_supplied || index + 1 >= argc) {
                throw std::invalid_argument(
                    "--evaluation-date requires one value");
            }
            const std::string_view supplied_date{argv[index + 1]};
            if (!valid_iso_date(supplied_date)) {
                throw std::invalid_argument(
                    "--evaluation-date requires a valid YYYY-MM-DD value");
            }
            ++index;
            options.evaluation_date = supplied_date;
            evaluation_date_supplied = true;
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            throw std::invalid_argument(
                "unknown option: " + std::string(argument));
        }
        if (options.dossier_path.empty()) {
            options.dossier_path = std::filesystem::path(argument);
        } else if (options.manifest_path.empty()) {
            options.manifest_path = std::filesystem::path(argument);
        } else {
            throw std::invalid_argument(
                "exactly one dossier and one manifest may be supplied");
        }
    }
    if (options.dossier_path.empty() || options.manifest_path.empty()) {
        throw std::invalid_argument(
            "a dossier and evidence manifest are required");
    }
    return options;
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::runtime_error& error) {
        if (std::string_view{error.what()} == "help requested") {
            return 0;
        }
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what()
                  << "\nRun with --help for usage.\n";
        return 2;
    }

    try {
        const cf::EvidenceDossier dossier = cf::load_evidence_dossier(
            options.dossier_path, options.manifest_path);
        const cf::EvidenceAssessment assessment =
            cf::assess_evidence_dossier(
                dossier, options.evaluation_date);
        if (options.report_only) {
            std::cout
                << "NON-ENFORCING REPORT MODE — EXIT STATUS IS SUPPRESSED\n"
                << "The printed gate result is unchanged.\n\n";
        }
        cf::print_evidence_report(std::cout, dossier, assessment);
        const bool subject_profile_passed =
            dossier.metadata.subject_kind ==
                    cf::DossierSubjectKind::ClaimPopulation
                ? cf::claim_population_frame_passes(assessment)
                : cf::all_gates_pass(assessment);
        if (options.report_only || subject_profile_passed) {
            return 0;
        }
        return 3;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
