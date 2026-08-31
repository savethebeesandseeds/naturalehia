// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/model.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using naturalehia::protein_logic::LogicalLevel;
using naturalehia::protein_logic::ModelParameters;

struct CommandLineOptions {
    ModelParameters parameters{-4.0, 8.0, 8.0, -16.0};
    double threshold{0.5};
    bool show_help{false};
};

void print_usage(std::ostream& output) {
    output << "Usage: naturalehia-protein-logic [options]\n\n"
           << "Evaluate a phenomenological two-input XOR specification. This tool\n"
           << "does not predict a protein sequence, structure, or biological function.\n\n"
           << "Options:\n"
           << "  --baseline NUMBER   Baseline contribution to active-state log odds\n"
           << "  --input-a NUMBER    Input A contribution to active-state log odds\n"
           << "  --input-b NUMBER    Input B contribution to active-state log odds\n"
           << "  --joint NUMBER      Additional contribution when both inputs are present\n"
           << "  --threshold NUMBER  ON threshold in the open interval (0, 1)\n"
           << "  -h, --help          Show this help\n";
}

std::optional<double> parse_number(std::string_view text) {
    const std::string owned_text{text};
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(owned_text.c_str(), &end);

    if (end == owned_text.c_str() || *end != '\0' || errno == ERANGE || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

bool parse_options(int argc, char* argv[], CommandLineOptions& options, std::string& error) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "-h" || argument == "--help") {
            options.show_help = true;
            continue;
        }

        double* destination = nullptr;
        if (argument == "--baseline") {
            destination = &options.parameters.baseline_log_odds;
        } else if (argument == "--input-a") {
            destination = &options.parameters.input_a_log_odds;
        } else if (argument == "--input-b") {
            destination = &options.parameters.input_b_log_odds;
        } else if (argument == "--joint") {
            destination = &options.parameters.joint_log_odds;
        } else if (argument == "--threshold") {
            destination = &options.threshold;
        } else {
            error = "unknown option: " + std::string{argument};
            return false;
        }

        if (index + 1 >= argc) {
            error = "missing value for " + std::string{argument};
            return false;
        }

        const std::string_view value_text{argv[++index]};
        const std::optional<double> value = parse_number(value_text);
        if (!value.has_value()) {
            error = "invalid finite number for " + std::string{argument} + ": ";
            error.append(value_text);
            return false;
        }
        *destination = *value;
    }
    return true;
}

constexpr std::string_view level_name(LogicalLevel level) noexcept {
    return level == LogicalLevel::On ? "ON" : "OFF";
}

} // namespace

int main(int argc, char* argv[]) {
    CommandLineOptions options{};
    std::string error;
    if (!parse_options(argc, argv, options, error)) {
        std::cerr << "error: " << error << "\n\n";
        print_usage(std::cerr);
        return 2;
    }

    if (options.show_help) {
        print_usage(std::cout);
        return 0;
    }

    try {
        const auto assessment =
            naturalehia::protein_logic::assess_xor(options.parameters, options.threshold);

        std::cout << "model\tphenomenological-log-odds-v1\n"
                  << "input_a\tinput_b\tlog_odds\tactive_probability\texpected\tobserved\n"
                  << std::fixed << std::setprecision(6);

        for (const auto& state : assessment.states) {
            std::cout << (state.inputs.input_a ? 1 : 0) << '\t' << (state.inputs.input_b ? 1 : 0)
                      << '\t' << state.log_odds << '\t' << state.active_probability << '\t'
                      << level_name(state.expected_xor_level) << '\t'
                      << level_name(state.observed_level) << '\n';
        }

        std::cout << "threshold\t" << assessment.threshold << '\n'
                  << "off_ceiling\t" << assessment.off_ceiling << '\n'
                  << "on_floor\t" << assessment.on_floor << '\n'
                  << "separation_margin\t" << assessment.separation_margin << '\n'
                  << "endpoint_specification_match\t"
                  << (assessment.passes_threshold ? "true" : "false") << '\n'
                  << "scope\tmathematical specification; not biological validation\n";
    } catch (const std::exception& exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 2;
    }

    return 0;
}
