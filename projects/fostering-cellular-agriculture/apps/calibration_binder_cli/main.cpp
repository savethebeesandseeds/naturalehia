// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/calibration_binder.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program
              << " <binder.cfg> [--evaluation-date YYYY-MM-DD] "
                 "[--print-normalized]\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 5 ||
        std::string_view(argv[1]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }

    std::string evaluation_date;
    bool print_normalized = false;
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--print-normalized" && !print_normalized) {
            print_normalized = true;
            continue;
        }
        if (argument == "--evaluation-date" && evaluation_date.empty() &&
            index + 1 < argc) {
            evaluation_date = argv[++index];
            continue;
        }
        print_usage(argv[0]);
        return 2;
    }

    try {
        const cf::CalibrationBinder binder = cf::load_calibration_binder(
            std::filesystem::path(argv[1]), evaluation_date);
        cf::print_calibration_binder_report(std::cout, binder);
        if (print_normalized) {
            std::cout << "\nNormalized binder configuration\n";
            cf::print_normalized_calibration_binder_config(
                std::cout, binder.config);
            std::cout << "\nNormalized portfolio configuration\n";
            cf::print_normalized_portfolio_config(
                std::cout, binder.portfolio);
            std::cout << "\nNormalized probability-envelope configuration\n";
            cf::print_normalized_portfolio_ambiguity_config(
                std::cout, binder.ambiguity);
            std::cout << "\nNormalized calibration lineage\n";
            cf::print_normalized_calibration_lineage(
                std::cout, binder.lineage);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "calibration binder review failed: " << error.what()
                  << '\n';
        return 1;
    }
}
