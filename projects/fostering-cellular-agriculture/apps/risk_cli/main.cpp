// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/config.hpp>
#include <naturalehia/cellular_finance/model.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

using naturalehia::cellular_finance::ComparisonSummary;
using naturalehia::cellular_finance::DistributionSummary;
using naturalehia::cellular_finance::InstrumentTerms;
using naturalehia::cellular_finance::SimulationConfig;
using naturalehia::cellular_finance::SimulationSummary;

struct Options {
    std::filesystem::path config_path{};
    std::optional<std::size_t> trials{};
    std::optional<std::uint64_t> seed{};
    bool print_config{false};
};

void print_help(std::ostream& output) {
    output
        << "Fostering Cellular Agriculture: transparent facility-risk model\n\n"
        << "Usage:\n"
        << "  naturalehia-cellular-finance SCENARIO.cfg [options]\n\n"
        << "Options:\n"
        << "  --trials N      Override the declared Monte Carlo trial count\n"
        << "  --seed N        Override the declared deterministic seed\n"
        << "  --print-config  Print the normalized, complete input set\n"
        << "  --help          Show this help text\n\n"
        << "Version 0.1 accepts synthetic illustrations only. It does not\n"
        << "produce investment, legal, engineering, food-safety, or policy\n"
        << "conclusions.\n";
}

[[nodiscard]] std::uint64_t parse_unsigned(
    std::string_view text, std::string_view option) {
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (text.empty() || result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument(
            std::string(option) + " expects a non-negative integer");
    }
    return value;
}

[[nodiscard]] std::size_t parse_size(
    std::string_view text, std::string_view option) {
    const std::uint64_t value = parse_unsigned(text, option);
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        if (value >
            static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw std::invalid_argument(
                std::string(option) + " is too large on this platform");
        }
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] Options parse_options(int argc, char* argv[]) {
    Options options;
    if (argc <= 1) {
        throw std::invalid_argument("a scenario configuration is required");
    }

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help") {
            print_help(std::cout);
            throw std::runtime_error("help requested");
        }
        if (argument == "--print-config") {
            options.print_config = true;
            continue;
        }
        if (argument == "--trials" || argument == "--seed") {
            if (index + 1 >= argc) {
                throw std::invalid_argument(
                    std::string(argument) + " requires a value");
            }
            const std::string_view value{argv[++index]};
            if (argument == "--trials") {
                options.trials = parse_size(value, argument);
            } else {
                options.seed = parse_unsigned(value, argument);
            }
            continue;
        }
        if (!argument.empty() && argument.front() == '-') {
            throw std::invalid_argument(
                "unknown option: " + std::string(argument));
        }
        if (!options.config_path.empty()) {
            throw std::invalid_argument(
                "only one scenario configuration may be supplied");
        }
        options.config_path = std::filesystem::path(argument);
    }
    if (options.config_path.empty()) {
        throw std::invalid_argument("a scenario configuration is required");
    }
    return options;
}

void print_distribution_row(
    std::string_view label,
    const DistributionSummary& baseline,
    const DistributionSummary& structured) {
    std::cout << std::left << std::setw(39) << label
              << std::right << std::setw(15) << baseline.mean
              << std::setw(15) << structured.mean
              << std::setw(15) << structured.mean - baseline.mean << '\n';
}

void print_probability_row(
    std::string_view label, double baseline, double structured) {
    constexpr double percentage = 100.0;
    std::cout << std::left << std::setw(39) << label
              << std::right << std::setw(14) << baseline * percentage << '%'
              << std::setw(14) << structured * percentage << '%'
              << std::setw(14)
              << (structured - baseline) * percentage << " pp\n";
}

void print_instrument_terms(
    const InstrumentTerms& terms, std::string_view currency_label) {
    std::cout
        << "\nStructured-case contract terms\n"
        << "  physical offtake share          "
        << terms.offtake_fraction * 100.0 << "% at "
        << terms.offtake_price_per_kg << ' ' << currency_label << "/kg\n"
        << "  price-support form/share        "
        << naturalehia::cellular_finance::to_string(
               terms.price_support_kind)
        << " / " << terms.price_support_fraction * 100.0 << "%\n"
        << "  price-support strike            "
        << terms.price_support_strike_per_kg << ' ' << currency_label
        << "/kg\n"
        << "  annual/lifetime absolute caps   "
        << terms.price_support_annual_cap_million << " / "
        << terms.price_support_lifetime_cap_million << ' ' << currency_label
        << " million\n"
        << "  completion-delay trigger        "
        << terms.completion_delay_trigger_years << " years\n"
        << "  delay payout rate / cap          "
        << terms.completion_payout_per_delay_year_million << ' '
        << currency_label << " million per delay-year / "
        << terms.completion_delay_cover_cap_million << ' ' << currency_label
        << " million\n"
        << "  upfront project fee              "
        << terms.upfront_fee_million << ' ' << currency_label << " million\n";
}

void print_zero_factor_checkpoint(const SimulationConfig& config) {
    const auto& facility = config.facility;
    const auto& risk = config.risk;
    const auto& terms = config.instrument;
    const double attempted_output =
        facility.annual_nameplate_output_million_kg *
        facility.steady_state_utilization;
    const double qualified_output = attempted_output *
        (1.0 - risk.annual_contamination_probability *
                   risk.contamination_output_loss_fraction);
    const double base_cfads =
        qualified_output * facility.base_spot_price_per_kg -
        attempted_output * facility.base_variable_cost_per_kg -
        facility.base_fixed_opex_million;
    const double break_even_spot = qualified_output > 0.0
        ? (attempted_output * facility.base_variable_cost_per_kg +
              facility.base_fixed_opex_million) /
            qualified_output
        : 0.0;
    const double offtake_repricing = qualified_output *
        terms.offtake_fraction *
        (terms.offtake_price_per_kg - facility.base_spot_price_per_kg);

    double price_support_settlement = 0.0;
    if (terms.price_support_kind !=
            naturalehia::cellular_finance::PriceSupportKind::None &&
        terms.price_support_fraction > 0.0) {
        double raw_settlement = qualified_output *
            terms.price_support_fraction *
            (terms.price_support_strike_per_kg -
             facility.base_spot_price_per_kg);
        if (terms.price_support_kind ==
            naturalehia::cellular_finance::PriceSupportKind::OneWayFloor) {
            raw_settlement = std::max(0.0, raw_settlement);
        }
        const double cap = std::min(
            terms.price_support_annual_cap_million,
            terms.price_support_lifetime_cap_million);
        price_support_settlement = std::clamp(raw_settlement, -cap, cap);
    }

    std::cout
        << "\nZero-factor probability-weighted steady-state checkpoint\n"
        << "  qualified output                " << qualified_output
        << " million kg/year\n"
        << "  unsupported CFADS               " << base_cfads
        << ' ' << config.currency_label << " million/year\n"
        << "  break-even spot price           " << break_even_spot
        << ' ' << config.currency_label << "/qualified kg\n"
        << "  structured CFADS before debt    "
        << base_cfads + offtake_repricing + price_support_settlement
        << ' ' << config.currency_label << " million/year\n"
        << "  base capex / annual nameplate   "
        << facility.base_capex_million /
               facility.annual_nameplate_output_million_kg
        << ' ' << config.currency_label
        << " per (kg/year) of nameplate capacity\n"
        << "  convention: full ramp, mean yield, base prices/costs, and\n"
        << "  zero-factor contamination loss; not a forecast or MC mean.\n";
}

void print_distribution_detail(
    std::string_view title, const DistributionSummary& summary,
    bool include_zero_threshold_shortfall) {
    std::cout << "\n" << title << "\n"
              << "  mean                         " << summary.mean << '\n'
              << "  standard deviation           " << summary.standard_deviation << '\n'
              << "  5th / 50th / 95th percentile "
              << summary.p05 << " / " << summary.p50 << " / "
              << summary.p95 << '\n';
    if (include_zero_threshold_shortfall) {
        std::cout << "  zero-threshold shortfall VaR 95% "
                  << summary.shortfall_value_at_risk_95 << '\n'
                  << "  zero-threshold expected shortfall "
                  << summary.shortfall_expected_shortfall_95 << '\n';
    }
}

void print_summary(
    const SimulationConfig& config, const ComparisonSummary& result) {
    const SimulationSummary& baseline = result.without_instrument;
    const SimulationSummary& structured = result.with_instrument;

    std::cout << std::fixed << std::setprecision(4);
    std::cout
        << "============================================================\n"
        << "SYNTHETIC ILLUSTRATION — NOT AN INVESTMENT CONCLUSION\n"
        << "============================================================\n"
        << "Model: annual reference v"
        << naturalehia::cellular_finance::kModelVersion << '\n'
        << "Scenario: " << config.scenario_label << '\n'
        << "Source note: " << config.source_note << '\n'
        << "Trials: " << config.trials << "\nSeed: " << config.seed << '\n'
        << "Paired paths: yes; both cases receive identical random draws\n"
        << "Monetary unit: " << config.currency_label
        << " millions; basis: " << config.monetary_basis << "\n";

    print_instrument_terms(config.instrument, config.currency_label);
    print_zero_factor_checkpoint(config);

    std::cout << '\n' << std::left << std::setw(39) << "Mean metric"
        << std::right << std::setw(15) << "Unprotected"
        << std::setw(15) << "Structured"
        << std::setw(15) << "Difference" << '\n'
        << std::string(84U, '-') << '\n';

    print_distribution_row(
        "Project NPV before instruments",
        baseline.project_npv_before_instruments_million,
        structured.project_npv_before_instruments_million);
    print_distribution_row(
        "Project NPV after instruments",
        baseline.project_npv_after_instruments_million,
        structured.project_npv_after_instruments_million);
    print_distribution_row(
        "Sponsor-equity cash-flow PV",
        baseline.sponsor_npv_after_financing_million,
        structured.sponsor_npv_after_financing_million);
    print_probability_row(
        "Probability project NPV is negative",
        baseline.probability_project_npv_negative,
        structured.probability_project_npv_negative);
    print_probability_row(
        "Probability sponsor NPV is negative",
        baseline.probability_sponsor_npv_negative,
        structured.probability_sponsor_npv_negative);
    print_probability_row(
        "Debt payment default within horizon",
        baseline.debt_default_probability,
        structured.debt_default_probability);

    std::cout << "\nPaired attribution\n"
              << "  mean project-NPV transfer       "
              << result.paired_project_npv_change_million.mean << '\n'
              << "  mean sponsor-NPV change         "
              << result.paired_sponsor_npv_change_million.mean << '\n'
              << "  net receipts from counterparties "
              << result.paired_instrument_transfer_pv_million.mean << '\n'
              << "  signed net transfers after prior default PV "
              << structured
                     .expected_instrument_net_receipts_after_default_pv_million
              << '\n'
              << "  fixed-offtake repricing PV       "
              << structured.expected_offtake_repricing_pv_million << '\n'
              << "  price-support settlement PV      "
              << structured.expected_price_support_net_settlement_pv_million
              << '\n'
              << "  completion-delay cover payout PV "
              << structured
                     .expected_completion_delay_cover_payout_pv_million
              << '\n'
              << "  upfront fee paid                 "
              << structured.expected_upfront_fee_million << '\n'
              << "  gross positive support payout PV "
              << structured.expected_positive_support_payout_pv_million
              << '\n'
              << "  unconditional lender EL PV, base "
              << baseline.unconditional_expected_debt_loss_pv_million << '\n'
              << "  unconditional lender EL PV, structured "
              << structured.unconditional_expected_debt_loss_pv_million
              << '\n'
              << "  unconditional lender EL at default dates, base "
              << baseline
                     .unconditional_expected_debt_loss_at_default_date_million
              << '\n'
              << "  unconditional lender EL at default dates, structured "
              << structured
                     .unconditional_expected_debt_loss_at_default_date_million
              << '\n'
              << "\nPaired threshold transitions (path counts)\n"
              << "  project NPV negative -> nonnegative "
              << result.project_npv_negative_to_nonnegative_count << '\n'
              << "  project NPV nonnegative -> negative "
              << result.project_npv_nonnegative_to_negative_count << '\n'
              << "  sponsor PV negative -> nonnegative "
              << result.sponsor_npv_negative_to_nonnegative_count << '\n'
              << "  sponsor PV nonnegative -> negative "
              << result.sponsor_npv_nonnegative_to_negative_count << '\n'
              << "  within-horizon payment defaults avoided "
              << result.debt_default_avoided_within_horizon_count << '\n'
              << "  within-horizon payment defaults introduced "
              << result.debt_default_introduced_within_horizon_count << '\n'
              << "  defaults delayed, both cases default "
              << result.debt_default_delayed_count << '\n'
              << "  defaults accelerated, both cases default "
              << result.debt_default_accelerated_count << '\n'
              << "  defaults at same time, both cases default "
              << result.debt_default_same_timing_count << '\n';

    std::cout << "\nConditional lender-loss severity\n";
    if (baseline.mean_debt_loss_given_default_at_default_date_million
            .has_value()) {
        std::cout << "  mean loss given default at default date, base "
                  << *baseline
                          .mean_debt_loss_given_default_at_default_date_million
                  << '\n';
    } else {
        std::cout <<
            "  mean loss given default at default date, base not applicable\n";
    }
    if (structured.mean_debt_loss_given_default_at_default_date_million
            .has_value()) {
        std::cout << "  mean loss given default at default date, structured "
                  << *structured
                          .mean_debt_loss_given_default_at_default_date_million
                  << '\n';
    } else {
        std::cout << "  mean loss given default at default date, structured "
                     "not applicable\n";
    }

    if (baseline.minimum_dscr.has_value() &&
        structured.minimum_dscr.has_value()) {
        std::cout << "\nDebt-service diagnostics\n"
                  << "  paths with debt service, base    "
                  << baseline.fraction_paths_with_debt_service * 100.0
                  << "%\n"
                  << "  paths with debt service, structured "
                  << structured.fraction_paths_with_debt_service * 100.0
                  << "%\n"
                  << "  terminal sponsor balloon frequency, base "
                  << baseline.probability_terminal_debt_outstanding * 100.0
                  << "%\n"
                  << "  terminal sponsor balloon frequency, structured "
                  << structured.probability_terminal_debt_outstanding * 100.0
                  << "%\n"
                  << "  unconditional terminal balloon, base "
                  << baseline.expected_terminal_debt_balance_million << '\n'
                  << "  unconditional terminal balloon, structured "
                  << structured.expected_terminal_debt_balance_million << '\n'
                  << "  mean minimum DSCR, base          "
                  << baseline.minimum_dscr->mean << '\n'
                  << "  mean minimum DSCR, structured    "
                  << structured.minimum_dscr->mean << '\n';
    }

    if (baseline.debt_default_timing_years_after_close.has_value() ||
        structured.debt_default_timing_years_after_close.has_value()) {
        std::cout << "\nConditional default-timing diagnostics\n";
        if (baseline.debt_default_timing_years_after_close.has_value()) {
            std::cout << "  mean timing after close, base    "
                      << baseline.debt_default_timing_years_after_close->mean
                      << " years\n";
        } else {
            std::cout << "  mean timing after close, base    no defaults\n";
        }
        if (structured.debt_default_timing_years_after_close.has_value()) {
            std::cout << "  mean timing after close, structured "
                      << structured
                             .debt_default_timing_years_after_close->mean
                      << " years\n";
        } else {
            std::cout <<
                "  mean timing after close, structured no defaults\n";
        }
        if (result.paired_default_timing_change_years.has_value()) {
            std::cout << "  paired mean timing change, both cases default "
                      << result.paired_default_timing_change_years->mean
                      << " years\n"
                      << "  positive means later in the structured case; "
                         "marginal means above use different defaulting "
                         "populations.\n";
        }
    }

    print_distribution_detail(
        "Structured sponsor-equity cash-flow PV distribution (million)",
        structured.sponsor_npv_after_financing_million, true);
    print_distribution_detail(
        "Actual capex distribution (million)",
        structured.actual_capex_million, false);
    print_distribution_detail(
        "Commercial-operation timing after financial close (years)",
        structured.commercial_operation_timing_years_after_close, false);

    std::cout
        << "\nInterpretation boundary\n"
        << "  Instrument receipts are transfers from counterparties; the\n"
        << "  model does not label them as newly created social value. It\n"
        << "  holds physical paths fixed, so it cannot establish financing\n"
        << "  additionality, adoption, displacement, or animal-welfare impact.\n"
        << "  Default means the first uncured scheduled senior-debt payment\n"
        << "  shortfall within the horizon. Any terminal balance is assumed\n"
        << "  paid by the sponsor and is reported separately. Positive support\n"
        << "  payout excludes physical-offtake repricing. The engine\n"
        << "  excludes taxes, inflation, terminal asset value, refinancing,\n"
        << "  counterparty default, legal classification, and behavior change.\n"
        << "  Sponsor cash flows use the project discount rate; this is not a\n"
        << "  separately calibrated equity hurdle rate. Delay cover is treated\n"
        << "  as unrestricted project cash and may create incentive conflicts.\n"
        << "  No output is suitable for pricing or offering a real instrument.\n";
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
        SimulationConfig config =
            naturalehia::cellular_finance::load_config(options.config_path);
        if (options.trials.has_value()) {
            config.trials = *options.trials;
        }
        if (options.seed.has_value()) {
            config.seed = *options.seed;
        }
        naturalehia::cellular_finance::validate_config(config);
        if (options.print_config) {
            std::cout << "Normalized configuration\n"
                      << "------------------------\n";
            naturalehia::cellular_finance::print_normalized_config(
                std::cout, config);
            std::cout << '\n';
        }

        const ComparisonSummary result =
            naturalehia::cellular_finance::run_paired_simulation(config);
        print_summary(config, result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
