// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/pooled_loss_protection.hpp>
#include <naturalehia/cellular_finance/pooled_loss_protection_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr
        << "usage: " << program
        << " <portfolio.cfg> <probability-envelope.cfg> "
           "<success-participation.cfg> <loss-protection.cfg> "
           "[--print-normalized]\n";
}

void print_range_row(std::string_view label,
    const cf::AmbiguityMetricRange& range, std::string_view unit,
    double scale = 1.0) {
    std::cout << "  " << label << " | "
              << range.minimum.value * scale << " | "
              << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_witness(std::string_view label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::PooledLossProtectionSummary& summary) {
    if (endpoint.scenario_weights.size() !=
        summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "pooled-loss-protection witness has the wrong scenario count");
    }
    std::cout << "  " << label << " | " << endpoint.value << " | ";
    for (std::size_t index = 0U;
         index < summary.scenario_probability_bounds.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout << summary.scenario_probability_bounds[index].scenario_id
                  << '=' << endpoint.scenario_weights[index];
    }
    std::cout << '\n';
}

void print_premium_evaluation(std::string_view label,
    const cf::PooledLossProtectionPremiumEvaluation& evaluation,
    std::string_view currency) {
    std::cout << "  " << label << " premium: "
              << evaluation.upfront_premium_million << ' ' << currency
              << " million\n"
              << "    investor robust NPV after premium: "
              << evaluation.investor_expected_npv_after_premium_million
                     .minimum.value
              << ' ' << currency << " million\n"
              << "    provider robust NPV after premium: "
              << evaluation.provider_expected_npv_after_premium_million
                     .minimum.value
              << ' ' << currency << " million\n"
              << "    investor target met: "
              << (evaluation.investor_target_is_met ? "yes" : "no")
              << "\n"
              << "    provider break-even met: "
              << (evaluation.provider_break_even_is_met ? "yes" : "no")
              << "\n";
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::PooledLossProtectionConfig& protection,
    const cf::PooledLossProtectionSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    const cf::PooledLossProtectionRobustPoint& point = summary.reported;
    std::cout << std::fixed << std::setprecision(6);

    std::cout
        << "SYNTHETIC POOLED PRINCIPAL-LOSS PROTECTION ANALYSIS\n"
        << "Not a forecast, fair value, market price, rating, term sheet, "
           "offering document, or investment recommendation.\n\n"
        << "Contract boundary\n"
        << "  provider id: " << protection.provider_id << '\n'
        << "  underlying success-participation fraction q: "
        << summary.underlying_success_participation_fraction << '\n'
        << "  reference amount: final resolved principal loss after "
           "in-horizon recovery\n"
        << "  settlement month: " << protection.settlement_month << '\n'
        << "  claim rule: coverage fraction g * gross pool principal loss\n"
        << "  gross project loss remains visible: yes\n"
        << "  continuing exposure is a covered realized loss: no\n"
        << "  premium: external investor-to-provider transfer at month zero\n"
        << "  external support performance: assumed fully funded and paid "
           "in every modeled scenario\n\n";

    std::cout << "Coverage result\n"
              << "  solver status: " << cf::to_string(summary.status) << '\n'
              << "  investor robust NPV target: "
              << summary.investor_target_worst_expected_npv_million << ' '
              << currency << " million\n"
              << "  aggregate contractual reference principal: "
              << summary.aggregate_reference_principal_million << ' '
              << currency << " million\n"
              << "  legal support cap: "
              << summary.legal_support_cap_million << ' ' << currency
              << " million\n"
              << "  maximum supported coverage fraction: "
              << summary.maximum_supported_coverage_fraction << '\n'
              << "  reported coverage fraction: "
              << summary.reported_coverage_fraction << '\n';
    if (summary.exact_minimum_coverage_fraction.has_value()) {
        std::cout << "  exact minimum coverage fraction: "
                  << *summary.exact_minimum_coverage_fraction << '\n';
    }
    if (summary.failing_coverage_fraction_lower_bound.has_value()) {
        std::cout << std::setprecision(17)
                  << "  certified failing lower coverage: "
                  << *summary.failing_coverage_fraction_lower_bound << '\n'
                  << std::setprecision(6);
    }
    if (summary.investor_target_passing_coverage_fraction_upper_bound
            .has_value()) {
        std::cout << std::setprecision(17)
                  << "  certified investor-target-passing upper coverage: "
                  << *summary
                          .investor_target_passing_coverage_fraction_upper_bound
                  << '\n' << std::setprecision(6);
    }
    std::cout << "  target gap at maximum supported coverage: "
              << summary
                     .investor_target_gap_at_maximum_supported_coverage_million
              << ' ' << currency << " million\n\n";

    std::cout
        << "Investor expected NPV before premium\n"
        << "  term point | minimum | central | maximum | unit\n";
    print_range_row("g=0 no protection",
        summary.zero.investor_expected_npv_before_premium_million,
        std::string(currency) + " million");
    print_range_row("reported g",
        point.investor_expected_npv_before_premium_million,
        std::string(currency) + " million");
    print_range_row("maximum supported g",
        summary.maximum_supported
            .investor_expected_npv_before_premium_million,
        std::string(currency) + " million");
    std::cout << '\n';

    std::cout << "Two-sided upfront premium test at reported coverage\n"
              << "  investor signed premium headroom: "
              << point.investor_signed_premium_headroom_million << ' '
              << currency << " million\n";
    if (point.investor_maximum_nonnegative_premium_million.has_value()) {
        std::cout << "  investor maximum non-negative premium: "
                  << *point.investor_maximum_nonnegative_premium_million
                  << ' ' << currency << " million\n";
    } else {
        std::cout << "  investor maximum non-negative premium: none\n";
    }
    std::cout << "  provider claim-only robust break-even floor: "
              << point.provider_minimum_robust_break_even_premium_million
              << ' ' << currency << " million\n"
              << "  premium feasibility gap: "
              << point.premium_feasibility_gap_million << ' ' << currency
              << " million\n"
              << "  robust non-negative bilateral price interval exists: "
              << (point.robust_nonnegative_premium_interval_exists ? "yes"
                                                                   : "no")
              << "\n";
    if (point.investor_maximum_nonnegative_premium_million.has_value()) {
        print_premium_evaluation("at investor ceiling",
            cf::evaluate_pooled_loss_protection_upfront_premium(point,
                *point.investor_maximum_nonnegative_premium_million),
            currency);
    }
    print_premium_evaluation("at provider floor",
        cf::evaluate_pooled_loss_protection_upfront_premium(point,
            point.provider_minimum_robust_break_even_premium_million),
        currency);
    std::cout << '\n';

    std::cout
        << "Provider payout and exposure at reported coverage\n"
        << "  metric | minimum | central | maximum | unit\n";
    print_range_row("expected nominal claim",
        point.provider_risk.expected_claim_nominal_million,
        std::string(currency) + " million");
    print_range_row("expected claim PV at provider hurdle",
        point.provider_risk.expected_claim_present_value_million,
        std::string(currency) + " million");
    print_range_row("probability of a positive claim",
        point.provider_risk.claim_probability, "percent", 100.0);
    print_range_row("nominal claim expected shortfall 95%",
        point.provider_risk.claim_expected_shortfall_95_nominal_million,
        std::string(currency) + " million");
    print_range_row("nominal claim expected shortfall 99%",
        point.provider_risk.claim_expected_shortfall_99_nominal_million,
        std::string(currency) + " million");
    std::cout << "  contractual maximum exposure: "
              << point.provider_risk.contractual_maximum_exposure_million
              << ' ' << currency << " million\n"
              << "  uncommitted legal cap capacity: "
              << point.provider_risk.uncommitted_legal_cap_capacity_million
              << ' ' << currency << " million\n"
              << "  largest claim in modeled scenarios: "
              << point.provider_risk.modeled_maximum_claim_million << ' '
              << currency << " million\n\n";

    std::cout
        << "Scenario and project protection ledger\n"
        << "  scenario/project | underlying NPV | gross loss | claim | "
           "residual loss | investor NPV before premium | unit\n";
    for (const cf::PooledLossProtectionScenarioResult& scenario :
         summary.scenarios) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.underlying_npv_million << " | "
                  << scenario.gross_principal_loss_million << " | "
                  << scenario.protection_claim_million << " | "
                  << scenario.residual_unprotected_loss_million << " | "
                  << scenario.investor_npv_before_premium_million << " | "
                  << currency << " million\n";
        for (const cf::PooledLossProtectionProjectResult& project :
             scenario.projects) {
            std::cout << "    " << project.project_id << " | - | "
                      << project.gross_principal_loss_million << " | "
                      << project.protection_claim_million << " | "
                      << project.residual_unprotected_loss_million
                      << " | - | " << currency << " million\n";
        }
    }
    std::cout << '\n';

    std::cout << "Binding probability witnesses\n"
              << "  metric | endpoint value | scenario probability vector\n";
    print_witness("investor minimum NPV before premium",
        point.investor_expected_npv_before_premium_million.minimum,
        summary);
    print_witness("provider maximum expected claim PV",
        point.provider_risk.expected_claim_present_value_million.maximum,
        summary);
    std::cout << '\n';

    std::cout << "Reconciliation controls\n"
              << "  maximum underlying gross-loss change: "
              << summary.maximum_underlying_loss_change_million << ' '
              << currency << " million\n"
              << "  maximum project-to-pool claim error: "
              << summary.maximum_project_claim_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum two-party settlement cash error: "
              << summary
                     .maximum_two_party_settlement_cash_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum support-cap violation: "
              << summary.maximum_support_cap_violation_million << ' '
              << currency << " million\n"
              << "  maximum combined-NPV reconstruction error: "
              << summary.maximum_combined_npv_reconstruction_error_million
              << ' ' << currency << " million\n"
              << "  maximum witness reconciliation error: "
              << summary.maximum_witness_reconciliation_error_million << ' '
              << currency << " million\n"
              << "  maximum endpoint probability error: "
              << summary.maximum_endpoint_probability_error << "\n\n";

    std::cout
        << "Interpretation boundary\n"
        << "  " << summary.provider_model_limitation << '\n'
        << "  The provider floor is a physical-P claim-only sensitivity, not "
           "fair value or a market quote.\n"
        << "  Gross project loss is not cured; reported protection is an "
           "external transfer and residual-loss memo.\n"
        << "  A positive premium gap is required catalytic support inside "
           "this narrow model before provider costs.\n"
        << "  Investor and provider endpoints may use different feasible "
           "probability witnesses and must not be combined as one forecast.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 5 || argc > 6 ||
        std::string_view(argv[1]).starts_with("--") ||
        std::string_view(argv[2]).starts_with("--") ||
        std::string_view(argv[3]).starts_with("--") ||
        std::string_view(argv[4]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 6;
    if (print_normalized &&
        std::string_view(argv[5]) != "--print-normalized") {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const cf::PortfolioConfig portfolio =
            cf::load_portfolio_config(std::filesystem::path(argv[1]));
        const cf::PortfolioAmbiguityConfig ambiguity =
            cf::load_portfolio_ambiguity_config(
                std::filesystem::path(argv[2]));
        const cf::SuccessParticipationConfig participation =
            cf::load_success_participation_config(
                std::filesystem::path(argv[3]));
        const cf::PooledLossProtectionConfig protection =
            cf::load_pooled_loss_protection_config(
                std::filesystem::path(argv[4]));
        const cf::PooledLossProtectionSummary summary =
            cf::solve_pooled_loss_protection(portfolio, ambiguity,
                participation, protection);
        print_report(portfolio, protection, summary);

        if (print_normalized) {
            std::cout << "\nNormalized portfolio configuration\n";
            cf::print_normalized_portfolio_config(std::cout, portfolio);
            std::cout << "\nNormalized probability-envelope configuration\n";
            cf::print_normalized_portfolio_ambiguity_config(
                std::cout, ambiguity);
            std::cout << "\nNormalized success-participation configuration\n";
            cf::print_normalized_success_participation_config(
                std::cout, participation);
            std::cout << "\nNormalized pooled-loss-protection configuration\n";
            cf::print_normalized_pooled_loss_protection_config(
                std::cout, protection);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "pooled-loss-protection analysis failed: "
                  << error.what() << '\n';
        return 1;
    }
}
