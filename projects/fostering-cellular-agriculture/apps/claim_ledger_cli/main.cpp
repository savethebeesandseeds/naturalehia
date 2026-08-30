// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_package.hpp>

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

enum class Requirement {
    None,
    ExpectedReturn,
    RatePreimage,
    ObservationAdmission,
};

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program
              << " <claim.cfg> [--require-expected-return|"
                 "--require-rate-preimage|--require-observation-admission]\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        print_usage(argc > 0 ? argv[0] : "naturalehia-claim-ledger");
        return 2;
    }

    Requirement requirement = Requirement::None;
    if (argc == 3) {
        const std::string_view option{argv[2]};
        if (option == "--require-expected-return") {
            requirement = Requirement::ExpectedReturn;
        } else if (option == "--require-rate-preimage") {
            requirement = Requirement::RatePreimage;
        } else if (option == "--require-observation-admission") {
            requirement = Requirement::ObservationAdmission;
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    try {
        const cf::ClaimLedgerPackage package =
            cf::load_claim_ledger_package(std::filesystem::path{argv[1]});
        cf::print_claim_ledger_package_report(std::cout, package);
        std::cout.flush();
        if (!std::cout) {
            throw std::runtime_error(
                "claim-ledger report output could not be written completely");
        }

        bool requirement_met = true;
        switch (requirement) {
        case Requirement::None:
            break;
        case Requirement::ExpectedReturn:
            requirement_met = package.expected_return_admissible;
            break;
        case Requirement::RatePreimage:
            requirement_met = package.evaluation.has_value() &&
                package.evaluation->readiness.rate_preimage_ready;
            break;
        case Requirement::ObservationAdmission:
            requirement_met = package.observation_admissible;
            break;
        }
        if (!requirement_met) {
            std::cerr << "claim-ledger requirement is not met\n";
            return 3;
        }
    } catch (const std::exception& error) {
        std::cerr << "claim-ledger error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
