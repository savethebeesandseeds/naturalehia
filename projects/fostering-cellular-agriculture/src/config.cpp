// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/config.hpp>

#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::array<std::string_view, 46U> kExpectedKeys{{
    "scenario.model_version",
    "scenario.label",
    "scenario.source_note",
    "scenario.currency_label",
    "scenario.monetary_basis",
    "scenario.synthetic_inputs",
    "simulation.trials",
    "simulation.seed",
    "facility.analysis_years",
    "facility.planned_construction_years",
    "facility.base_capex_million",
    "facility.annual_nameplate_output_million_kg",
    "facility.steady_state_utilization",
    "facility.ramp_at_commercial_operation",
    "facility.annual_ramp_increment",
    "facility.base_spot_price_per_kg",
    "facility.base_variable_cost_per_kg",
    "facility.base_fixed_opex_million",
    "facility.project_discount_rate",
    "risk.capex_log_sigma",
    "risk.construction_duration_log_sigma",
    "risk.utilization_logit_sigma",
    "risk.biological_yield_log_sigma",
    "risk.output_price_log_sigma",
    "risk.variable_cost_log_sigma",
    "risk.fixed_opex_log_sigma",
    "risk.annual_contamination_probability",
    "risk.contamination_logit_sigma",
    "risk.contamination_output_loss_fraction",
    "risk.persistent_factor_loading",
    "debt.debt_fraction_of_base_capex",
    "debt.annual_interest_rate",
    "debt.tenor_years",
    "debt.recovery_fraction_after_default",
    "debt.assume_terminal_balance_paid_by_sponsor",
    "instrument.offtake_fraction",
    "instrument.offtake_price_per_kg",
    "instrument.price_support_kind",
    "instrument.price_support_fraction",
    "instrument.price_support_strike_per_kg",
    "instrument.price_support_annual_cap_million",
    "instrument.price_support_lifetime_cap_million",
    "instrument.completion_delay_trigger_years",
    "instrument.completion_payout_per_delay_year_million",
    "instrument.completion_delay_cover_cap_million",
    "instrument.upfront_fee_million",
}};

[[nodiscard]] std::string_view trim_view(std::string_view value) noexcept {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1U);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1U);
    }
    return value;
}

[[noreturn]] void parse_error(
    std::size_t line, std::string_view message) {
    throw std::invalid_argument(
        "configuration line " + std::to_string(line) + ": " +
        std::string(message));
}

[[nodiscard]] double parse_double(
    std::string_view value, std::size_t line) {
    double result{};
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end) {
        parse_error(line, "expected a decimal number");
    }
    return result;
}

[[nodiscard]] std::uint64_t parse_unsigned(
    std::string_view value, std::size_t line) {
    std::uint64_t result{};
    const char* const begin = value.data();
    const char* const end = begin + value.size();
    const auto conversion = std::from_chars(begin, end, result);
    if (value.empty() || conversion.ec != std::errc{} ||
        conversion.ptr != end) {
        parse_error(line, "expected a non-negative integer");
    }
    return result;
}

[[nodiscard]] std::size_t parse_size(
    std::string_view value, std::size_t line) {
    const std::uint64_t parsed = parse_unsigned(value, line);
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        if (parsed >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            parse_error(line, "integer is too large for this platform");
        }
    }
    return static_cast<std::size_t>(parsed);
}

[[nodiscard]] bool parse_bool(
    std::string_view value, std::size_t line) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    parse_error(line, "expected true or false");
}

[[nodiscard]] PriceSupportKind parse_support_kind(
    std::string_view value, std::size_t line) {
    if (value == "none") {
        return PriceSupportKind::None;
    }
    if (value == "one-way-floor") {
        return PriceSupportKind::OneWayFloor;
    }
    if (value == "two-way-difference") {
        return PriceSupportKind::TwoWayDifference;
    }
    parse_error(
        line,
        "expected none, one-way-floor, or two-way-difference");
}

void assign_value(SimulationConfig& config,
    std::string_view key, std::string_view value, std::size_t line) {
    if (key == "scenario.model_version") {
        config.model_version = std::string(value);
    } else if (key == "scenario.label") {
        config.scenario_label = std::string(value);
    } else if (key == "scenario.source_note") {
        config.source_note = std::string(value);
    } else if (key == "scenario.currency_label") {
        config.currency_label = std::string(value);
    } else if (key == "scenario.monetary_basis") {
        config.monetary_basis = std::string(value);
    } else if (key == "scenario.synthetic_inputs") {
        config.synthetic_inputs = parse_bool(value, line);
    } else if (key == "simulation.trials") {
        config.trials = parse_size(value, line);
    } else if (key == "simulation.seed") {
        config.seed = parse_unsigned(value, line);
    } else if (key == "facility.analysis_years") {
        config.facility.analysis_years = parse_size(value, line);
    } else if (key == "facility.planned_construction_years") {
        config.facility.planned_construction_years = parse_double(value, line);
    } else if (key == "facility.base_capex_million") {
        config.facility.base_capex_million = parse_double(value, line);
    } else if (key == "facility.annual_nameplate_output_million_kg") {
        config.facility.annual_nameplate_output_million_kg =
            parse_double(value, line);
    } else if (key == "facility.steady_state_utilization") {
        config.facility.steady_state_utilization = parse_double(value, line);
    } else if (key == "facility.ramp_at_commercial_operation") {
        config.facility.ramp_at_commercial_operation = parse_double(value, line);
    } else if (key == "facility.annual_ramp_increment") {
        config.facility.annual_ramp_increment = parse_double(value, line);
    } else if (key == "facility.base_spot_price_per_kg") {
        config.facility.base_spot_price_per_kg = parse_double(value, line);
    } else if (key == "facility.base_variable_cost_per_kg") {
        config.facility.base_variable_cost_per_kg = parse_double(value, line);
    } else if (key == "facility.base_fixed_opex_million") {
        config.facility.base_fixed_opex_million = parse_double(value, line);
    } else if (key == "facility.project_discount_rate") {
        config.facility.project_discount_rate = parse_double(value, line);
    } else if (key == "risk.capex_log_sigma") {
        config.risk.capex_log_sigma = parse_double(value, line);
    } else if (key == "risk.construction_duration_log_sigma") {
        config.risk.construction_duration_log_sigma = parse_double(value, line);
    } else if (key == "risk.utilization_logit_sigma") {
        config.risk.utilization_logit_sigma = parse_double(value, line);
    } else if (key == "risk.biological_yield_log_sigma") {
        config.risk.biological_yield_log_sigma = parse_double(value, line);
    } else if (key == "risk.output_price_log_sigma") {
        config.risk.output_price_log_sigma = parse_double(value, line);
    } else if (key == "risk.variable_cost_log_sigma") {
        config.risk.variable_cost_log_sigma = parse_double(value, line);
    } else if (key == "risk.fixed_opex_log_sigma") {
        config.risk.fixed_opex_log_sigma = parse_double(value, line);
    } else if (key == "risk.annual_contamination_probability") {
        config.risk.annual_contamination_probability = parse_double(value, line);
    } else if (key == "risk.contamination_logit_sigma") {
        config.risk.contamination_logit_sigma = parse_double(value, line);
    } else if (key == "risk.contamination_output_loss_fraction") {
        config.risk.contamination_output_loss_fraction = parse_double(value, line);
    } else if (key == "risk.persistent_factor_loading") {
        config.risk.persistent_factor_loading = parse_double(value, line);
    } else if (key == "debt.debt_fraction_of_base_capex") {
        config.debt.debt_fraction_of_base_capex = parse_double(value, line);
    } else if (key == "debt.annual_interest_rate") {
        config.debt.annual_interest_rate = parse_double(value, line);
    } else if (key == "debt.tenor_years") {
        config.debt.tenor_years = parse_size(value, line);
    } else if (key == "debt.recovery_fraction_after_default") {
        config.debt.recovery_fraction_after_default = parse_double(value, line);
    } else if (key == "debt.assume_terminal_balance_paid_by_sponsor") {
        config.debt.assume_terminal_balance_paid_by_sponsor =
            parse_bool(value, line);
    } else if (key == "instrument.offtake_fraction") {
        config.instrument.offtake_fraction = parse_double(value, line);
    } else if (key == "instrument.offtake_price_per_kg") {
        config.instrument.offtake_price_per_kg = parse_double(value, line);
    } else if (key == "instrument.price_support_kind") {
        config.instrument.price_support_kind = parse_support_kind(value, line);
    } else if (key == "instrument.price_support_fraction") {
        config.instrument.price_support_fraction = parse_double(value, line);
    } else if (key == "instrument.price_support_strike_per_kg") {
        config.instrument.price_support_strike_per_kg = parse_double(value, line);
    } else if (key == "instrument.price_support_annual_cap_million") {
        config.instrument.price_support_annual_cap_million =
            parse_double(value, line);
    } else if (key == "instrument.price_support_lifetime_cap_million") {
        config.instrument.price_support_lifetime_cap_million =
            parse_double(value, line);
    } else if (key == "instrument.completion_delay_trigger_years") {
        config.instrument.completion_delay_trigger_years =
            parse_double(value, line);
    } else if (key == "instrument.completion_payout_per_delay_year_million") {
        config.instrument.completion_payout_per_delay_year_million =
            parse_double(value, line);
    } else if (key == "instrument.completion_delay_cover_cap_million") {
        config.instrument.completion_delay_cover_cap_million =
            parse_double(value, line);
    } else if (key == "instrument.upfront_fee_million") {
        config.instrument.upfront_fee_million = parse_double(value, line);
    } else {
        parse_error(line, "unknown key: " + std::string(key));
    }
}

} // namespace

SimulationConfig load_config(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "could not open configuration file: " + path.string());
    }

    SimulationConfig config;
    std::unordered_set<std::string> seen;
    std::string line_text;
    std::size_t line_number = 0U;
    while (std::getline(input, line_text)) {
        ++line_number;
        std::string_view line{line_text};
        if (line_number == 1U && line.size() >= 3U &&
            static_cast<unsigned char>(line[0]) == 0xEFU &&
            static_cast<unsigned char>(line[1]) == 0xBBU &&
            static_cast<unsigned char>(line[2]) == 0xBFU) {
            line.remove_prefix(3U);
        }
        line = trim_view(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            parse_error(line_number, "expected key=value");
        }
        const std::string_view key = trim_view(line.substr(0U, equals));
        const std::string_view value = trim_view(line.substr(equals + 1U));
        if (key.empty() || value.empty()) {
            parse_error(line_number, "key and value must not be empty");
        }
        if (!seen.insert(std::string(key)).second) {
            parse_error(line_number, "duplicate key: " + std::string(key));
        }
        assign_value(config, key, value, line_number);
    }
    if (!input.eof()) {
        throw std::runtime_error(
            "failed while reading configuration file: " + path.string());
    }

    for (const std::string_view key : kExpectedKeys) {
        if (!seen.contains(std::string(key))) {
            throw std::invalid_argument(
                "configuration is missing required key: " + std::string(key));
        }
    }
    validate_config(config);
    return config;
}

void print_normalized_config(
    std::ostream& output, const SimulationConfig& config) {
    validate_config(config);
    output << std::setprecision(17) << std::boolalpha;
    output << "scenario.model_version=" << config.model_version << '\n';
    output << "scenario.label=" << config.scenario_label << '\n';
    output << "scenario.source_note=" << config.source_note << '\n';
    output << "scenario.currency_label=" << config.currency_label << '\n';
    output << "scenario.monetary_basis=" << config.monetary_basis << '\n';
    output << "scenario.synthetic_inputs=" << config.synthetic_inputs << '\n';
    output << "simulation.trials=" << config.trials << '\n';
    output << "simulation.seed=" << config.seed << '\n';
    output << "facility.analysis_years=" <<
        config.facility.analysis_years << '\n';
    output << "facility.planned_construction_years=" <<
        config.facility.planned_construction_years << '\n';
    output << "facility.base_capex_million=" <<
        config.facility.base_capex_million << '\n';
    output << "facility.annual_nameplate_output_million_kg=" <<
        config.facility.annual_nameplate_output_million_kg << '\n';
    output << "facility.steady_state_utilization=" <<
        config.facility.steady_state_utilization << '\n';
    output << "facility.ramp_at_commercial_operation=" <<
        config.facility.ramp_at_commercial_operation << '\n';
    output << "facility.annual_ramp_increment=" <<
        config.facility.annual_ramp_increment << '\n';
    output << "facility.base_spot_price_per_kg=" <<
        config.facility.base_spot_price_per_kg << '\n';
    output << "facility.base_variable_cost_per_kg=" <<
        config.facility.base_variable_cost_per_kg << '\n';
    output << "facility.base_fixed_opex_million=" <<
        config.facility.base_fixed_opex_million << '\n';
    output << "facility.project_discount_rate=" <<
        config.facility.project_discount_rate << '\n';
    output << "risk.capex_log_sigma=" << config.risk.capex_log_sigma << '\n';
    output << "risk.construction_duration_log_sigma=" <<
        config.risk.construction_duration_log_sigma << '\n';
    output << "risk.utilization_logit_sigma=" <<
        config.risk.utilization_logit_sigma << '\n';
    output << "risk.biological_yield_log_sigma=" <<
        config.risk.biological_yield_log_sigma << '\n';
    output << "risk.output_price_log_sigma=" <<
        config.risk.output_price_log_sigma << '\n';
    output << "risk.variable_cost_log_sigma=" <<
        config.risk.variable_cost_log_sigma << '\n';
    output << "risk.fixed_opex_log_sigma=" <<
        config.risk.fixed_opex_log_sigma << '\n';
    output << "risk.annual_contamination_probability=" <<
        config.risk.annual_contamination_probability << '\n';
    output << "risk.contamination_logit_sigma=" <<
        config.risk.contamination_logit_sigma << '\n';
    output << "risk.contamination_output_loss_fraction=" <<
        config.risk.contamination_output_loss_fraction << '\n';
    output << "risk.persistent_factor_loading=" <<
        config.risk.persistent_factor_loading << '\n';
    output << "debt.debt_fraction_of_base_capex=" <<
        config.debt.debt_fraction_of_base_capex << '\n';
    output << "debt.annual_interest_rate=" <<
        config.debt.annual_interest_rate << '\n';
    output << "debt.tenor_years=" << config.debt.tenor_years << '\n';
    output << "debt.recovery_fraction_after_default=" <<
        config.debt.recovery_fraction_after_default << '\n';
    output << "debt.assume_terminal_balance_paid_by_sponsor=" <<
        config.debt.assume_terminal_balance_paid_by_sponsor << '\n';
    output << "instrument.offtake_fraction=" <<
        config.instrument.offtake_fraction << '\n';
    output << "instrument.offtake_price_per_kg=" <<
        config.instrument.offtake_price_per_kg << '\n';
    output << "instrument.price_support_kind=" <<
        to_string(config.instrument.price_support_kind) << '\n';
    output << "instrument.price_support_fraction=" <<
        config.instrument.price_support_fraction << '\n';
    output << "instrument.price_support_strike_per_kg=" <<
        config.instrument.price_support_strike_per_kg << '\n';
    output << "instrument.price_support_annual_cap_million=" <<
        config.instrument.price_support_annual_cap_million << '\n';
    output << "instrument.price_support_lifetime_cap_million=" <<
        config.instrument.price_support_lifetime_cap_million << '\n';
    output << "instrument.completion_delay_trigger_years=" <<
        config.instrument.completion_delay_trigger_years << '\n';
    output << "instrument.completion_payout_per_delay_year_million=" <<
        config.instrument.completion_payout_per_delay_year_million << '\n';
    output << "instrument.completion_delay_cover_cap_million=" <<
        config.instrument.completion_delay_cover_cap_million << '\n';
    output << "instrument.upfront_fee_million=" <<
        config.instrument.upfront_fee_million << '\n';
}

} // namespace naturalehia::cellular_finance
