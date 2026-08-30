// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/staged_capital.hpp>
#include <naturalehia/cellular_finance/staged_capital_config.hpp>

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(const char* program) {
    std::cerr << "usage: " << program
              << " <staged-capital.cfg> [--print-normalized]\n";
}

void print_money_distribution(std::string_view label,
    const cf::WeightedDistributionSummary& value,
    std::string_view currency) {
    std::cout << "  " << label << " (" << currency << " million)\n"
              << "    mean=" << value.mean << " p50=" << value.p50
              << " p95=" << value.p95 << " p99=" << value.p99
              << " max=" << value.maximum << '\n';
}

void print_percentage(std::string_view label, double value) {
    std::cout << "  " << label << ": " << value * 100.0 << "%\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 3;
    if (print_normalized &&
        std::string_view(argv[2]) != "--print-normalized") {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const cf::StagedCapitalConfig config =
            cf::load_staged_capital_config(std::filesystem::path(argv[1]));
        const cf::StagedCapitalSummary summary =
            cf::evaluate_staged_capital_cases(config);

        std::cout << std::fixed << std::setprecision(6);
        std::cout
            << "SYNTHETIC FINANCIAL-ENGINEERING ANALYSIS\n"
            << "MILESTONE-GATED COMMITTED CAPITAL FACILITY v"
            << cf::kStagedCapitalModelVersion << "\n"
            << "Not a term sheet, commitment, forecast, fair value, market "
               "price, or recommendation.\n"
            << "Certifications and case weights are explicit synthetic "
               "inputs; the model does not certify evidence.\n\n";

        std::cout << "Scenario\n"
                  << "  label: " << config.scenario_label << '\n'
                  << "  source boundary: " << config.source_note << '\n'
                  << "  monetary basis: " << config.currency_label << ", "
                  << config.monetary_basis << '\n'
                  << "  explicit weighted cases: " << config.cases.size()
                  << '\n'
                  << "  declared case-weight sum before near-one normalization: "
                  << std::defaultfloat
                  << std::setprecision(
                         std::numeric_limits<double>::max_digits10)
                  << summary.configured_case_weight_sum
                  << std::fixed << std::setprecision(6)
                  << "\n\n";

        std::cout << "Contract terms\n"
                  << "  provider commitment: "
                  << config.terms.provider_commitment_million << ' '
                  << config.currency_label << " million\n"
                  << "  sponsor construction commitment: "
                  << config.terms.sponsor_construction_commitment_million
                  << ' ' << config.currency_label << " million\n"
                  << "  provider eligible-cost share: "
                  << config.terms.provider_cost_share * 100.0 << "%\n"
                  << "  annual capped PIK rate: "
                  << config.terms.annual_pik_rate * 100.0 << "%\n"
                  << "  funded-claim cap: "
                  << config.terms.claim_cap_multiple
                  << "x cumulative provider principal\n"
                  << "  annual fee on modeled committed undrawn availability: "
                  << config.terms.annual_commitment_fee_rate * 100.0
                  << "%\n"
                  << "  protected workout reserve: "
                  << config.terms.protected_workout_reserve_million << ' '
                  << config.currency_label << " million\n\n";

        std::cout << "Weighted physical-measure case outcomes\n";
        print_percentage("completion", summary.completion_weight);
        print_percentage(
            "final milestone failure", summary.milestone_failure_weight);
        print_percentage("cost-to-complete funding failure",
            summary.cost_to_complete_failure_weight);
        print_percentage("sponsor funding failure",
            summary.sponsor_funding_failure_weight);
        print_percentage("provider funding failure",
            summary.provider_funding_failure_weight);
        print_percentage("any provider draw", summary.provider_draw_weight);
        print_percentage(
            "unrecovered funded provider principal",
            summary.provider_principal_loss_weight);
        print_percentage(
            "provider claim writeoff", summary.provider_claim_writeoff_weight);
        print_percentage("protected reserve shortfall at stop",
            summary.protected_reserve_shortfall_at_stop_weight);
        print_percentage("final unresolved safety shortfall after recovery",
            summary.safety_funding_shortfall_weight);
        print_percentage("expected commitment utilization",
            summary.expected_commitment_utilization);
        std::cout << '\n';

        std::cout << "Exposure and loss\n";
        print_money_distribution("provider draws",
            summary.provider_draws_million, config.currency_label);
        print_money_distribution("peak provider net cash outlay",
            summary.peak_provider_net_cash_outlay_million,
            config.currency_label);
        print_money_distribution("unrecovered funded provider principal",
            summary.provider_principal_loss_million, config.currency_label);
        print_money_distribution("provider PIK-bearing claim writeoff",
            summary.provider_claim_writeoff_million,
            config.currency_label);
        std::cout << "  unrecovered-principal expected shortfall 95: "
                  << summary.provider_principal_loss_million
                         .expected_shortfall_95
                  << ' ' << config.currency_label << " million\n"
                  << "  unrecovered-principal expected shortfall 99: "
                  << summary.provider_principal_loss_million
                         .expected_shortfall_99
                  << ' ' << config.currency_label << " million\n"
                  << "  expected nominal provider terminal receipt: "
                  << summary.expected_provider_nominal_recovery_million << ' '
                  << config.currency_label << " million\n"
                  << "  expected provider terminal-receipt PV at declared hurdle: "
                  << summary.expected_provider_recovery_pv_million << ' '
                  << config.currency_label << " million\n";
        if (summary.conditional_provider_principal_loss_million.has_value()) {
            std::cout << "  conditional unrecovered funded principal: "
                      << *summary.conditional_provider_principal_loss_million
                      << ' ' << config.currency_label << " million\n";
        } else {
            std::cout
                << "  conditional unrecovered funded principal: not applicable\n";
        }
        if (summary.conditional_provider_claim_writeoff_million.has_value()) {
            std::cout << "  conditional claim writeoff: "
                      << *summary.conditional_provider_claim_writeoff_million
                      << ' ' << config.currency_label << " million\n";
        } else {
            std::cout << "  conditional claim writeoff: not applicable\n";
        }
        std::cout << '\n';

        std::cout << "Project and sponsor funding consequences\n";
        print_money_distribution("total sponsor cash call",
            summary.sponsor_total_cash_call_million, config.currency_label);
        print_money_distribution("stranded eligible spend",
            summary.stranded_spend_million, config.currency_label);
        print_money_distribution("explicit funding gap",
            summary.funding_gap_million, config.currency_label);
        print_money_distribution("protected reserve shortfall at stop",
            summary.protected_reserve_shortfall_at_stop_million,
            config.currency_label);
        print_money_distribution("final unresolved safety shortfall",
            summary.safety_funding_shortfall_million,
            config.currency_label);
        std::cout << '\n';

        std::cout << "Provider fee-adequacy sensitivity\n"
                  << "  physical-case weight replayed with provider "
                     "performance held true: "
                  << summary.fee_sensitivity_included_weight * 100.0
                  << "%\n"
                  << "  declared provider hurdle: "
                  << config.terms.provider_hurdle_rate * 100.0 << "%\n"
                  << "  expected provider NPV before upfront fee: "
                  << summary
                         .expected_provider_npv_before_upfront_fee_million
                  << ' ' << config.currency_label << " million\n"
                  << "  physical-P zero-NPV upfront fee: "
                  << summary
                         .physical_measure_break_even_upfront_fee_million
                  << ' ' << config.currency_label << " million\n"
                  << "  charged upfront fee: "
                  << summary.charged_upfront_fee_million << ' '
                  << config.currency_label << " million\n"
                  << "  fee adequacy gap (charged minus break-even): "
                  << summary.upfront_fee_adequacy_gap_million << ' '
                  << config.currency_label << " million\n"
                  << "  expected provider NPV after charged upfront fee: "
                  << summary
                         .expected_provider_npv_after_charged_upfront_fee_million
                  << ' ' << config.currency_label << " million\n"
                  << "  A negative break-even fee is reported as a rebate; "
                     "it is not floored.\n"
                  << "  This is a declared-case physical-P zero-NPV "
                     "sensitivity, not risk-neutral valuation.\n"
                  << "  Paired all-provider-performs fee cases:\n"
                  << "    case | weight | outcome | outcome month | terminal "
                     "settlement month | draw | terminal receipt | provider NPV "
                     "before fee\n";
        for (const auto& fee_case : summary.fee_basis_cases) {
            std::cout << "    " << fee_case.case_id << " | "
                      << fee_case.weight << " | "
                      << cf::to_string(
                             fee_case.all_provider_performs_outcome)
                      << " | " << fee_case.outcome_month << " | "
                      << fee_case.recovery_month << " | "
                      << fee_case.provider_draws_million << " | "
                      << fee_case.provider_nominal_recovery_million << " | "
                      << fee_case.provider_npv_before_upfront_fee_million
                      << '\n';
        }
        std::cout << '\n';

        std::cout << "Case ledger outcomes\n"
                  << "  case | weight | outcome | outcome month | terminal "
                     "settlement month | draw | claim | terminal receipt | "
                     "unrecovered principal | claim writeoff | funding gap | "
                     "reserve gap | final safety gap\n";
        for (const auto& path : summary.cases) {
            std::cout << "  " << path.case_id << " | " << path.weight
                      << " | " << cf::to_string(path.outcome) << " | "
                      << path.outcome_month << " | "
                      << path.recovery_month << " | "
                      << path.total_provider_draws_million << " | "
                      << path.provider_claim_at_exit_million << " | "
                      << path.provider_nominal_recovery_million << " | "
                      << path.provider_principal_loss_million << " | "
                      << path.provider_claim_writeoff_million << " | "
                      << path.funding_gap_million << " | "
                      << path.protected_reserve_shortfall_at_stop_million
                      << " | "
                      << path.safety_funding_shortfall_million << '\n';
        }

        std::cout << "\nPathwise controls\n"
                  << "  maximum cash-entry imbalance: "
                  << summary.maximum_cash_entry_imbalance_million << ' '
                  << config.currency_label << " million\n"
                  << "  maximum commitment/claim memo imbalance: "
                  << summary.maximum_memo_rollforward_imbalance_million << ' '
                  << config.currency_label << " million\n"
                  << "  protected reserve is excluded from provider "
                     "recovery in every case.\n";

        if (print_normalized) {
            std::cout << "\nNormalized configuration\n";
            cf::print_normalized_staged_capital_config(std::cout, config);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "staged-capital analysis failed: " << error.what()
                  << '\n';
        return 1;
    }
}
