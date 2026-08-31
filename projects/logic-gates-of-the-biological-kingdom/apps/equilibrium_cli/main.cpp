// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/equilibrium_acceptance.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using naturalehia::protein_logic::AxisSpacing;
using naturalehia::protein_logic::ClosedInterval;
using naturalehia::protein_logic::EquilibriumParameterBox;
using naturalehia::protein_logic::EquilibriumParameters;
using naturalehia::protein_logic::InputState;
using naturalehia::protein_logic::LogicalLevel;
using naturalehia::protein_logic::MacrostateParameterBox;
using naturalehia::protein_logic::OperatingWindows;
using naturalehia::protein_logic::SteadyStateXorAcceptance;
using naturalehia::protein_logic::SteadyStateXorCriteria;
using naturalehia::protein_logic::SteadyStateXorMetrics;
using naturalehia::protein_logic::XorRegion;

constexpr std::size_t kMaximumSurfacePoints = 1'000'000;

struct CommonOptions {
    EquilibriumParameters parameters{
        -4.0,
        {1.0, 1.0, 10.0},
        {0.01, 0.01, 0.001},
    };
    std::string concentration_unit{"normalized"};
    bool show_help{false};
};

struct AuditCriteriaOptions {
    std::optional<std::string> label{};
    SteadyStateXorCriteria criteria{};
};

struct RegionOptions : CommonOptions {
    OperatingWindows windows{
        {{0.0, 0.01}, {3.0, 10.0}},
        {{0.0, 0.01}, {3.0, 10.0}},
    };
    double threshold{0.5};
    double required_margin{0.0};
    double apo_log_radius{0.0};
    double positive_relative_radius{0.0};
    bool independent_null{false};
    AuditCriteriaOptions audit_criteria{};
};

struct SurfaceOptions : CommonOptions {
    ClosedInterval input_a_bounds{0.0, 10.0};
    ClosedInterval input_b_bounds{0.0, 10.0};
    std::size_t input_a_points{21};
    std::size_t input_b_points{21};
    AxisSpacing input_a_spacing{AxisSpacing::Linear};
    AxisSpacing input_b_spacing{AxisSpacing::Linear};
};

void print_parameter_usage(std::ostream& output) {
    output << "  --apo-log NUMBER    Apo log(Z_ON/Z_OFF)\n"
           << "  --off-kd-a NUMBER   OFF-state dissociation scale for input A\n"
           << "  --off-kd-b NUMBER   OFF-state dissociation scale for input B\n"
           << "  --off-omega NUMBER  OFF-state double-binding factor\n"
           << "  --on-kd-a NUMBER    ON-state dissociation scale for input A\n"
           << "  --on-kd-b NUMBER    ON-state dissociation scale for input B\n"
           << "  --on-omega NUMBER   ON-state double-binding factor\n"
           << "  --unit LABEL        Concentration-unit label written to TSV output\n";
}

void print_region_usage(std::ostream& output) {
    output << "Usage: naturalehia-protein-logic-equilibrium region [options]\n\n"
           << "Bound complete XOR concentration windows in a two-state equilibrium\n"
           << "model. Defaults are an illustrative dimensionless regression fixture,\n"
           << "not estimated biological parameters.\n\n"
           << "Equilibrium parameters:\n";
    print_parameter_usage(output);
    output << "\nOperating windows:\n"
           << "  --a-low-min NUMBER  --a-low-max NUMBER\n"
           << "  --a-high-min NUMBER --a-high-max NUMBER\n"
           << "  --b-low-min NUMBER  --b-low-max NUMBER\n"
           << "  --b-high-min NUMBER --b-high-max NUMBER\n\n"
           << "Assessment options:\n"
           << "  --threshold NUMBER          Fixed ON threshold in (0, 1)\n"
           << "  --required-separation NUMBER  Separation that must be exceeded\n"
           << "  --apo-log-radius NUMBER     Independent additive stress radius\n"
           << "  --positive-radius NUMBER    Relative stress radius in [0, 1)\n"
           << "  --independent-null          Force both omega values to one\n"
           << "  -h, --help                  Show this help\n";
}

void print_surface_usage(std::ostream& output) {
    output << "Usage: naturalehia-protein-logic-equilibrium surface [options]\n\n"
           << "Emit a deterministic two-input response grid for inspection. A sampled\n"
           << "surface is not proof of behavior between grid points.\n\n"
           << "Equilibrium parameters:\n";
    print_parameter_usage(output);
    output << "\nAxis options:\n"
           << "  --a-min NUMBER      --a-max NUMBER      --a-points INTEGER\n"
           << "  --b-min NUMBER      --b-max NUMBER      --b-points INTEGER\n"
           << "  --a-spacing VALUE   linear or log\n"
           << "  --b-spacing VALUE   linear or log\n"
           << "  -h, --help          Show this help\n";
}

void print_coupling_audit_usage(std::ostream& output) {
    output << "Usage: naturalehia-protein-logic-equilibrium coupling-audit [options]\n\n"
           << "Audit the declared state-specific double-binding model against a\n"
           << "nested ablation that changes only both omega intervals to one. This\n"
           << "is not statistical model selection or evidence for a mechanism.\n\n"
           << "Equilibrium parameters, operating windows, and stress controls:\n";
    print_parameter_usage(output);
    output << "\nOperating windows:\n"
           << "  --a-low-min NUMBER  --a-low-max NUMBER\n"
           << "  --a-high-min NUMBER --a-high-max NUMBER\n"
           << "  --b-low-min NUMBER  --b-low-max NUMBER\n"
           << "  --b-high-min NUMBER --b-high-max NUMBER\n"
           << "  --apo-log-radius NUMBER     Independent additive stress radius\n"
           << "  --positive-radius NUMBER    Relative stress radius in [0, 1)\n\n"
           << "Optional acceptance protocol (supply all five or none):\n"
           << "  --criteria-label LABEL\n"
           << "  --criteria-threshold NUMBER\n"
           << "  --criteria-min-separation NUMBER\n"
           << "  --criteria-max-intended-off NUMBER\n"
           << "  --criteria-max-floor-imbalance NUMBER\n"
           << "  -h, --help                  Show this help\n";
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

std::optional<std::size_t> parse_size(std::string_view text) {
    if (text.empty() || text.front() == '-') {
        return std::nullopt;
    }
    const std::string owned_text{text};
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(owned_text.c_str(), &end, 10);
    if (end == owned_text.c_str() || *end != '\0' || errno == ERANGE ||
        value > std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(value);
}

bool valid_label(std::string_view text) {
    if (text.empty() || text.front() == '=' || text.front() == '+' ||
        text.front() == '-' || text.front() == '@') {
        return false;
    }
    for (const char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 0x20U || byte == 0x7fU) {
            return false;
        }
    }
    return true;
}

double* parameter_destination(std::string_view argument, EquilibriumParameters& parameters) {
    if (argument == "--apo-log") {
        return &parameters.apo_log_on_off;
    }
    if (argument == "--off-kd-a") {
        return &parameters.off.dissociation_a;
    }
    if (argument == "--off-kd-b") {
        return &parameters.off.dissociation_b;
    }
    if (argument == "--off-omega") {
        return &parameters.off.omega;
    }
    if (argument == "--on-kd-a") {
        return &parameters.on.dissociation_a;
    }
    if (argument == "--on-kd-b") {
        return &parameters.on.dissociation_b;
    }
    if (argument == "--on-omega") {
        return &parameters.on.omega;
    }
    return nullptr;
}

bool read_number(int argc, char* argv[], int& index, std::string_view argument,
    double& destination, std::string& error) {
    if (index + 1 >= argc) {
        error = "missing value for " + std::string{argument};
        return false;
    }
    const std::string_view text{argv[++index]};
    const std::optional<double> value = parse_number(text);
    if (!value.has_value()) {
        error = "invalid finite number for " + std::string{argument} + ": " + std::string{text};
        return false;
    }
    destination = *value;
    return true;
}

bool read_size(int argc, char* argv[], int& index, std::string_view argument,
    std::size_t& destination, std::string& error) {
    if (index + 1 >= argc) {
        error = "missing value for " + std::string{argument};
        return false;
    }
    const std::string_view text{argv[++index]};
    const std::optional<std::size_t> value = parse_size(text);
    if (!value.has_value()) {
        error = "invalid non-negative integer for " + std::string{argument} + ": " +
        std::string{text};
        return false;
    }
    destination = *value;
    return true;
}

bool read_unit(int argc, char* argv[], int& index, std::string& destination,
    std::string& error) {
    if (index + 1 >= argc) {
        error = "missing value for --unit";
        return false;
    }
    const std::string_view value{argv[++index]};
    if (!valid_label(value)) {
        error = "unit label must be nonempty, contain no ASCII control characters, and "
                "not begin with =, +, -, or @";
        return false;
    }
    destination = std::string{value};
    return true;
}

bool read_label(int argc, char* argv[], int& index, std::string_view argument,
    std::optional<std::string>& destination, std::string& error) {
    if (index + 1 >= argc) {
        error = "missing value for " + std::string{argument};
        return false;
    }
    const std::string_view value{argv[++index]};
    if (!valid_label(value)) {
        error = std::string{argument} +
        " must be nonempty, contain no ASCII control characters, and not begin with =, +, "
        "-, or @";
        return false;
    }
    destination = std::string{value};
    return true;
}

bool read_optional_number(int argc, char* argv[], int& index, std::string_view argument,
    std::optional<double>& destination, std::string& error) {
    double value = 0.0;
    if (!read_number(argc, argv, index, argument, value, error)) {
        return false;
    }
    destination = value;
    return true;
}

bool parse_region_options(
    int argc, char* argv[], int first, RegionOptions& options, std::string& error,
    std::string_view command_name = "region") {
    for (int index = first; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "-h" || argument == "--help") {
            options.show_help = true;
            continue;
        }
        if (argument == "--independent-null") {
            options.independent_null = true;
            continue;
        }
        if (argument == "--unit") {
            if (!read_unit(argc, argv, index, options.concentration_unit, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--criteria-label") {
            if (!read_label(argc, argv, index, argument, options.audit_criteria.label, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--criteria-threshold" ||
            argument == "--criteria-min-separation" ||
            argument == "--criteria-max-intended-off" ||
            argument == "--criteria-max-floor-imbalance") {
            std::optional<double>* destination = nullptr;
            if (argument == "--criteria-threshold") {
                destination = &options.audit_criteria.criteria.threshold;
            } else if (argument == "--criteria-min-separation") {
                destination = &options.audit_criteria.criteria.minimum_separation;
            } else if (argument == "--criteria-max-intended-off") {
                destination = &options.audit_criteria.criteria.maximum_intended_off_activity;
            } else {
                destination =
                    &options.audit_criteria.criteria.maximum_single_high_floor_imbalance;
            }
            if (!read_optional_number(argc, argv, index, argument, *destination, error)) {
                return false;
            }
            continue;
        }

        double* destination = parameter_destination(argument, options.parameters);
        if (destination == nullptr) {
            if (argument == "--a-low-min") {
                destination = &options.windows.input_a.low.lower;
            } else if (argument == "--a-low-max") {
                destination = &options.windows.input_a.low.upper;
            } else if (argument == "--a-high-min") {
                destination = &options.windows.input_a.high.lower;
            } else if (argument == "--a-high-max") {
                destination = &options.windows.input_a.high.upper;
            } else if (argument == "--b-low-min") {
                destination = &options.windows.input_b.low.lower;
            } else if (argument == "--b-low-max") {
                destination = &options.windows.input_b.low.upper;
            } else if (argument == "--b-high-min") {
                destination = &options.windows.input_b.high.lower;
            } else if (argument == "--b-high-max") {
                destination = &options.windows.input_b.high.upper;
            } else if (argument == "--threshold") {
                if (command_name != "region") {
                    error = "coupling-audit uses --criteria-threshold, not --threshold";
                    return false;
                }
                destination = &options.threshold;
            } else if (argument == "--required-separation") {
                if (command_name != "region") {
                    error = "coupling-audit uses --criteria-min-separation, not "
                            "--required-separation";
                    return false;
                }
                destination = &options.required_margin;
            } else if (argument == "--apo-log-radius") {
                destination = &options.apo_log_radius;
            } else if (argument == "--positive-radius") {
                destination = &options.positive_relative_radius;
            } else {
                error = "unknown " + std::string{command_name} +
                " option: " + std::string{argument};
                return false;
            }
        }
        if (!read_number(argc, argv, index, argument, *destination, error)) {
            return false;
        }
    }
    return true;
}

std::optional<AxisSpacing> parse_spacing(std::string_view text) {
    if (text == "linear") {
        return AxisSpacing::Linear;
    }
    if (text == "log" || text == "logarithmic") {
        return AxisSpacing::Logarithmic;
    }
    return std::nullopt;
}

bool parse_surface_options(
    int argc, char* argv[], int first, SurfaceOptions& options, std::string& error) {
    for (int index = first; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "-h" || argument == "--help") {
            options.show_help = true;
            continue;
        }
        if (argument == "--unit") {
            if (!read_unit(argc, argv, index, options.concentration_unit, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--a-points" || argument == "--b-points") {
            std::size_t& destination =
                argument == "--a-points" ? options.input_a_points : options.input_b_points;
            if (!read_size(argc, argv, index, argument, destination, error)) {
                return false;
            }
            continue;
        }
        if (argument == "--a-spacing" || argument == "--b-spacing") {
            if (index + 1 >= argc) {
                error = "missing value for " + std::string{argument};
                return false;
            }
            const std::string_view text{argv[++index]};
            const std::optional<AxisSpacing> spacing = parse_spacing(text);
            if (!spacing.has_value()) {
                error = "spacing must be 'linear' or 'log': " + std::string{text};
                return false;
            }
            if (argument == "--a-spacing") {
                options.input_a_spacing = *spacing;
            } else {
                options.input_b_spacing = *spacing;
            }
            continue;
        }

        double* destination = parameter_destination(argument, options.parameters);
        if (destination == nullptr) {
            if (argument == "--a-min") {
                destination = &options.input_a_bounds.lower;
            } else if (argument == "--a-max") {
                destination = &options.input_a_bounds.upper;
            } else if (argument == "--b-min") {
                destination = &options.input_b_bounds.lower;
            } else if (argument == "--b-max") {
                destination = &options.input_b_bounds.upper;
            } else {
                error = "unknown surface option: " + std::string{argument};
                return false;
            }
        }
        if (!read_number(argc, argv, index, argument, *destination, error)) {
            return false;
        }
    }
    return true;
}

ClosedInterval additive_interval(double center, double radius) {
    if (!std::isfinite(radius) || radius < 0.0) {
        throw std::invalid_argument("apo log radius must be finite and non-negative");
    }
    const ClosedInterval result{center - radius, center + radius};
    if (!std::isfinite(result.lower) || !std::isfinite(result.upper)) {
        throw std::invalid_argument("apo log stress interval cannot be represented");
    }
    return result;
}

ClosedInterval relative_interval(double center, double radius) {
    if (!std::isfinite(radius) || radius < 0.0 || radius >= 1.0) {
        throw std::invalid_argument("positive relative radius must be in [0, 1)");
    }
    const ClosedInterval result{center * (1.0 - radius), center * (1.0 + radius)};
    if (!std::isfinite(result.lower) || !std::isfinite(result.upper)) {
        throw std::invalid_argument("positive parameter stress interval cannot be represented");
    }
    return result;
}

EquilibriumParameterBox make_parameter_box(const RegionOptions& options) {
    const auto macrostate_box = [&](const auto& macrostate) {
            return MacrostateParameterBox{
            relative_interval(macrostate.dissociation_a, options.positive_relative_radius),
            relative_interval(macrostate.dissociation_b, options.positive_relative_radius),
            relative_interval(macrostate.omega, options.positive_relative_radius),
            };
        };

    EquilibriumParameterBox result{
        additive_interval(options.parameters.apo_log_on_off, options.apo_log_radius),
        macrostate_box(options.parameters.off),
        macrostate_box(options.parameters.on),
    };
    if (options.independent_null) {
        result.off.omega = {1.0, 1.0};
        result.on.omega = {1.0, 1.0};
    }
    return result;
}

bool any_audit_criteria(const AuditCriteriaOptions& options) {
    return options.label.has_value() || options.criteria.threshold.has_value() ||
           options.criteria.minimum_separation.has_value() ||
           options.criteria.maximum_intended_off_activity.has_value() ||
           options.criteria.maximum_single_high_floor_imbalance.has_value();
}

bool complete_audit_criteria(const AuditCriteriaOptions& options) {
    return options.label.has_value() && options.criteria.threshold.has_value() &&
           options.criteria.minimum_separation.has_value() &&
           options.criteria.maximum_intended_off_activity.has_value() &&
           options.criteria.maximum_single_high_floor_imbalance.has_value();
}

std::optional<SteadyStateXorCriteria> declared_audit_criteria(
    const AuditCriteriaOptions& options) {
    if (!any_audit_criteria(options)) {
        return std::nullopt;
    }
    if (!complete_audit_criteria(options)) {
        throw std::invalid_argument(
                  "coupling-audit acceptance requires all five criteria options or none");
    }
    return options.criteria;
}

constexpr InputState logical_inputs(XorRegion region) noexcept {
    switch (region) {
    case XorRegion::LowLow:
        return {false, false};
    case XorRegion::HighLow:
        return {true, false};
    case XorRegion::LowHigh:
        return {false, true};
    case XorRegion::HighHigh:
        return {true, true};
    }
    return {};
}

void print_interval(std::string_view name, ClosedInterval interval) {
    std::cout << "parameter_bound\t" << name << '\t' << interval.lower << '\t'
              << interval.upper << '\n';
}

void print_parameter(std::string_view name, double value) {
    std::cout << "parameter\t" << name << '\t' << value << '\n';
}

void print_operating_window(
    std::string_view input, std::string_view level, ClosedInterval interval) {
    std::cout << "operating_window\t" << input << '\t' << level << '\t' << interval.lower
              << '\t' << interval.upper << '\n';
}

constexpr std::string_view spacing_name(AxisSpacing spacing) noexcept {
    return spacing == AxisSpacing::Logarithmic ? "log" : "linear";
}

void print_metrics(std::string_view variant, const SteadyStateXorMetrics& metrics) {
    std::cout << "metric\t" << variant << '\t' << metrics.basal_off_activity_ceiling << '\t'
              << metrics.input_a_only_on_floor << '\t'
              << metrics.input_a_only_on_ceiling << '\t'
              << metrics.input_b_only_on_floor << '\t'
              << metrics.input_b_only_on_ceiling << '\t'
              << metrics.joint_high_off_activity_ceiling << '\t' << metrics.on_floor << '\t'
              << metrics.intended_off_activity_ceiling << '\t' << metrics.separation << '\t'
              << metrics.single_high_floor_imbalance << '\t'
              << metrics.single_high_response_gap_upper_bound << '\n';
}

void print_acceptance(
    std::string_view variant, const SteadyStateXorAcceptance& acceptance) {
    std::cout << "acceptance\t" << variant << '\t' << *acceptance.on_threshold_margin << '\t'
              << *acceptance.off_threshold_margin << '\t' << *acceptance.threshold_margin << '\t'
              << *acceptance.separation_clearance << '\t'
              << *acceptance.intended_off_activity_clearance << '\t'
              << *acceptance.single_high_floor_balance_clearance << '\t'
              << naturalehia::protein_logic::criterion_outcome_name(
        acceptance.threshold_outcome)
              << '\t'
              << naturalehia::protein_logic::criterion_outcome_name(
        acceptance.separation_outcome)
              << '\t'
              << naturalehia::protein_logic::criterion_outcome_name(
        acceptance.intended_off_activity_outcome)
              << '\t'
              << naturalehia::protein_logic::criterion_outcome_name(
        acceptance.single_high_floor_balance_outcome)
              << '\t'
              << naturalehia::protein_logic::criterion_outcome_name(
        acceptance.overall_outcome)
              << '\n';
}

int run_region(int argc, char* argv[], int first) {
    RegionOptions options{};
    std::string error;
    if (!parse_region_options(argc, argv, first, options, error)) {
        std::cerr << "error: " << error << "\n\n";
        print_region_usage(std::cerr);
        return 2;
    }
    if (options.show_help) {
        print_region_usage(std::cout);
        return 0;
    }
    if (any_audit_criteria(options.audit_criteria)) {
        throw std::invalid_argument(
                  "acceptance criteria are evaluated only by coupling-audit");
    }

    if (options.independent_null) {
        options.parameters.off.omega = 1.0;
        options.parameters.on.omega = 1.0;
    }
    const EquilibriumParameterBox parameter_box = make_parameter_box(options);
    const bool independent_binding = parameter_box.off.omega.lower == 1.0 &&
        parameter_box.off.omega.upper == 1.0 && parameter_box.on.omega.lower == 1.0 &&
        parameter_box.on.omega.upper == 1.0;
    const auto assessment = naturalehia::protein_logic::assess_parameter_box(parameter_box,
            options.windows, options.threshold, options.required_margin);

    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10)
              << "model\ttwo-state-equilibrium-binding-v1\n"
              << "mechanism\t"
              << (independent_binding ? "independent-binding-null" : "coupled-binding")
              << '\n'
              << "concentration_unit\t" << options.concentration_unit << '\n'
              << "uncertainty_semantics\tindependent closed stress intervals; not confidence\n";

    print_interval("apo_log_on_off", parameter_box.apo_log_on_off);
    print_interval("off_kd_a", parameter_box.off.dissociation_a);
    print_interval("off_kd_b", parameter_box.off.dissociation_b);
    print_interval("off_omega", parameter_box.off.omega);
    print_interval("on_kd_a", parameter_box.on.dissociation_a);
    print_interval("on_kd_b", parameter_box.on.dissociation_b);
    print_interval("on_omega", parameter_box.on.omega);

    print_operating_window("input_a", "low", options.windows.input_a.low);
    print_operating_window("input_a", "high", options.windows.input_a.high);
    print_operating_window("input_b", "low", options.windows.input_b.low);
    print_operating_window("input_b", "high", options.windows.input_b.high);

    std::cout << "region\tinput_a\tinput_b\texpected\tprobability_lower\tprobability_upper"
                 "\tlower_at_a\tlower_at_b\tupper_at_a\tupper_at_b\n";
    for (const auto& region : assessment.global.regions) {
        const InputState inputs = logical_inputs(region.region);
        std::cout << "region\t" << (inputs.input_a ? 1 : 0) << '\t'
                  << (inputs.input_b ? 1 : 0) << '\t'
                  << (region.expected_level == LogicalLevel::On ? "ON" : "OFF") << '\t'
                  << region.minimum.active_probability << '\t'
                  << region.maximum.active_probability << '\t'
                  << region.minimum.concentrations.input_a << '\t'
                  << region.minimum.concentrations.input_b << '\t'
                  << region.maximum.concentrations.input_a << '\t'
                  << region.maximum.concentrations.input_b << '\n';
    }

    const auto& global = assessment.global;
    std::cout << "threshold\t" << global.threshold << '\n'
              << "required_separation\t" << global.required_margin << '\n'
              << "off_ceiling\t" << global.off_ceiling << '\n'
              << "on_floor\t" << global.on_floor << '\n'
              << "separation_margin\t" << global.separation_margin << '\n'
              << "threshold_margin\t" << global.threshold_margin << '\n'
              << "decision_tolerance\t" << global.decision_tolerance << '\n'
              << "numerical_semantics\tbinary64-operational-v1\n"
              << "numerical_certification\tnot_certified\n"
              << "passes_threshold\t" << (global.passes_threshold ? "true" : "false")
              << '\n'
              << "passes_required_separation\t"
              << (global.passes_required_margin ? "true" : "false") << '\n'
              << "bounds_scope\tanalytic extrema for declared rectangles and independent"
        " parameter box; binary64 values\n"
              << "not_modeled\tkinetics, reset, path dependence, folding, aggregation,"
        " production, biological function\n";
    return 0;
}

int run_coupling_audit(int argc, char* argv[], int first) {
    RegionOptions options{};
    std::string error;
    if (!parse_region_options(argc, argv, first, options, error, "coupling-audit")) {
        std::cerr << "error: " << error << "\n\n";
        print_coupling_audit_usage(std::cerr);
        return 2;
    }
    if (options.show_help) {
        print_coupling_audit_usage(std::cout);
        return 0;
    }
    if (options.independent_null) {
        throw std::invalid_argument(
                  "coupling-audit constructs its own independent-binding null");
    }

    const std::optional<SteadyStateXorCriteria> criteria =
        declared_audit_criteria(options.audit_criteria);
    const EquilibriumParameterBox declared_box = make_parameter_box(options);
    if (declared_box.off.omega.lower == 1.0 && declared_box.off.omega.upper == 1.0 &&
        declared_box.on.omega.lower == 1.0 && declared_box.on.omega.upper == 1.0) {
        throw std::invalid_argument(
                  "coupling-audit requires at least one declared omega interval to differ "
                  "from [1, 1]; otherwise no ablation occurs");
    }
    EquilibriumParameterBox independent_box = declared_box;
    independent_box.off.omega = {1.0, 1.0};
    independent_box.on.omega = {1.0, 1.0};

    const auto declared_assessment = naturalehia::protein_logic::assess_parameter_box(
        declared_box, options.windows);
    const auto independent_assessment = naturalehia::protein_logic::assess_parameter_box(
        independent_box, options.windows);
    const auto declared_acceptance = naturalehia::protein_logic::assess_steady_state_xor(
        declared_assessment.global, criteria.value_or(SteadyStateXorCriteria{}));
    const auto independent_acceptance = naturalehia::protein_logic::assess_steady_state_xor(
        independent_assessment.global, criteria.value_or(SteadyStateXorCriteria{}));

    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10)
              << "audit\tpaired-omega-ablation-v1\n"
              << "declared_variant\tstate-specific-double-binding\n"
              << "nested_null\tindependent-binding\n"
              << "concentration_unit\t" << options.concentration_unit << '\n'
              << "stress_semantics\tindependent closed intervals; not confidence\n"
              << "audit_scope\tsame apo and dissociation boxes; null forces only both"
        " omega intervals to one; no refitting\n"
              << "interpretation\tdeterministic coupling-term ablation; not statistical"
        " model selection or biological validation\n";

    print_interval("apo_log_on_off", declared_box.apo_log_on_off);
    print_interval("off_kd_a", declared_box.off.dissociation_a);
    print_interval("off_kd_b", declared_box.off.dissociation_b);
    print_interval("declared_off_omega", declared_box.off.omega);
    print_interval("on_kd_a", declared_box.on.dissociation_a);
    print_interval("on_kd_b", declared_box.on.dissociation_b);
    print_interval("declared_on_omega", declared_box.on.omega);
    print_interval("null_off_omega", independent_box.off.omega);
    print_interval("null_on_omega", independent_box.on.omega);

    print_operating_window("input_a", "low", options.windows.input_a.low);
    print_operating_window("input_a", "high", options.windows.input_a.high);
    print_operating_window("input_b", "low", options.windows.input_b.low);
    print_operating_window("input_b", "high", options.windows.input_b.high);

    std::cout << "metric\tvariant\tbasal_off_activity_ceiling\tinput_a_only_on_floor"
                 "\tinput_a_only_on_ceiling\tinput_b_only_on_floor"
                 "\tinput_b_only_on_ceiling\tjoint_high_off_activity_ceiling"
                 "\ton_floor\tintended_off_activity_ceiling\tseparation"
                 "\tsingle_high_floor_imbalance"
                 "\tsingle_high_response_gap_upper_bound\n";
    print_metrics("declared", declared_acceptance.metrics);
    print_metrics("independent_null", independent_acceptance.metrics);
    std::cout << "decision_tolerance\t" << declared_acceptance.decision_tolerance << '\n'
              << "numerical_semantics\tbinary64-operational-v1\n"
              << "numerical_certification\tnot_certified\n";

    if (!criteria.has_value()) {
        std::cout << "acceptance_status\tnot_assessed\n"
                  << "acceptance_reason\tno complete caller-declared criteria supplied\n";
    } else {
        std::cout << "acceptance_status\tassessed\n"
                  << "criteria_label\t" << *options.audit_criteria.label << '\n'
                  << "criteria_provenance\tcaller-declared label and values; the CLI"
            " infers no empirical basis\n"
                  << "criterion\tthreshold\t" << *criteria->threshold << '\n'
                  << "criterion\tminimum_separation\t" << *criteria->minimum_separation
                  << '\n'
                  << "criterion\tmaximum_intended_off_activity\t"
                  << *criteria->maximum_intended_off_activity << '\n'
                  << "criterion\tmaximum_single_high_floor_imbalance\t"
                  << *criteria->maximum_single_high_floor_imbalance << '\n'
                  << "acceptance\tvariant\ton_threshold_margin\toff_threshold_margin"
            "\tthreshold_margin\tseparation_clearance"
            "\tintended_off_activity_clearance\tfloor_balance_clearance"
            "\tthreshold\tseparation\tintended_off_activity"
            "\tsingle_high_floor_balance\toverall\n";
        print_acceptance("declared", declared_acceptance);
        print_acceptance("independent_null", independent_acceptance);
        std::cout << "overall_outcome\tdeclared\t"
                  << naturalehia::protein_logic::criterion_outcome_name(
            declared_acceptance.overall_outcome)
                  << '\n'
                  << "overall_outcome\tindependent_null\t"
                  << naturalehia::protein_logic::criterion_outcome_name(
            independent_acceptance.overall_outcome)
                  << '\n';
    }

    std::cout << "evidence_status\tcross_talk\tnot_assessed\toutside model scope\n"
              << "evidence_status\tresponse_time\tnot_assessed\tequilibrium has no time\n"
              << "evidence_status\treset\tnot_assessed\trequires kinetics and rechallenge\n"
              << "bounds_scope\tanalytic extrema for the declared bilinear model;"
        " binary64 values\n";
    return 0;
}

int run_surface(int argc, char* argv[], int first) {
    SurfaceOptions options{};
    std::string error;
    if (!parse_surface_options(argc, argv, first, options, error)) {
        std::cerr << "error: " << error << "\n\n";
        print_surface_usage(std::cerr);
        return 2;
    }
    if (options.show_help) {
        print_surface_usage(std::cout);
        return 0;
    }

    if ((options.input_a_points == 1 &&
        options.input_a_bounds.lower != options.input_a_bounds.upper) ||
        (options.input_b_points == 1 &&
        options.input_b_bounds.lower != options.input_b_bounds.upper)) {
        throw std::invalid_argument(
                  "a singleton CLI axis requires equal minimum and maximum values");
    }

    const std::size_t point_count = naturalehia::protein_logic::checked_surface_size(
        options.input_a_points, options.input_b_points);
    if (point_count > kMaximumSurfacePoints) {
        throw std::invalid_argument("surface exceeds the CLI limit of 1000000 points");
    }
    const auto surface = naturalehia::protein_logic::sample_surface(options.parameters,
            options.input_a_bounds, options.input_a_points, options.input_a_spacing,
            options.input_b_bounds, options.input_b_points, options.input_b_spacing);

    std::cout << std::setprecision(std::numeric_limits<double>::max_digits10)
              << "model\ttwo-state-equilibrium-binding-v1\n"
              << "assessment\tsampled-response-surface\n"
              << "concentration_unit\t" << options.concentration_unit << '\n'
              << "surface_layout\tinput-a-fastest\n";
    print_parameter("apo_log_on_off", options.parameters.apo_log_on_off);
    print_parameter("off_kd_a", options.parameters.off.dissociation_a);
    print_parameter("off_kd_b", options.parameters.off.dissociation_b);
    print_parameter("off_omega", options.parameters.off.omega);
    print_parameter("on_kd_a", options.parameters.on.dissociation_a);
    print_parameter("on_kd_b", options.parameters.on.dissociation_b);
    print_parameter("on_omega", options.parameters.on.omega);
    std::cout << "axis\tinput_a\t" << options.input_a_bounds.lower << '\t'
              << options.input_a_bounds.upper << '\t' << options.input_a_points << '\t'
              << spacing_name(options.input_a_spacing) << '\n'
              << "axis\tinput_b\t" << options.input_b_bounds.lower << '\t'
              << options.input_b_bounds.upper << '\t' << options.input_b_points << '\t'
              << spacing_name(options.input_b_spacing) << '\n'
              << "input_a\tinput_b\tactive_probability\n";
    for (std::size_t input_b_index = 0; input_b_index < surface.input_b_axis.size();
        ++input_b_index) {
        for (std::size_t input_a_index = 0; input_a_index < surface.input_a_axis.size();
            ++input_a_index) {
            std::cout << surface.input_a_axis[input_a_index] << '\t'
                      << surface.input_b_axis[input_b_index] << '\t'
                      << surface.at(input_a_index, input_b_index) << '\n';
        }
    }
    std::cout << "scope\tsampled equilibrium-model surface; not biological validation\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc <= 1 || std::string_view{argv[1]} == "region") {
            const int first = argc <= 1 ? 1 : 2;
            return run_region(argc, argv, first);
        }
        if (std::string_view{argv[1]} == "surface") {
            return run_surface(argc, argv, 2);
        }
        if (std::string_view{argv[1]} == "coupling-audit") {
            return run_coupling_audit(argc, argv, 2);
        }
        if (std::string_view{argv[1]} == "-h" || std::string_view{argv[1]} == "--help") {
            print_region_usage(std::cout);
            std::cout << '\n';
            print_surface_usage(std::cout);
            std::cout << '\n';
            print_coupling_audit_usage(std::cout);
            return 0;
        }

        std::cerr << "error: unknown command: " << argv[1] << "\n\n";
        print_region_usage(std::cerr);
        return 2;
    } catch (const std::exception& exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 2;
    }
}
