// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/success_participation.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
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
           "<success-participation.cfg> [--print-normalized]\n";
}

void print_range_row(std::string_view label,
    const cf::AmbiguityMetricRange& range, std::string_view unit,
    double scale = 1.0) {
    std::cout << "  " << label << " | "
              << range.minimum.value * scale << " | "
              << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_source_kinds(const cf::SuccessParticipationConfig& participation) {
    for (std::size_t index = 0U;
         index < participation.scalable_source_kinds.size(); ++index) {
        if (index != 0U) {
            std::cout << ", ";
        }
        std::cout << cf::to_string(
            participation.scalable_source_kinds[index]);
    }
}

void print_witness(std::string_view label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::SuccessParticipationSummary& summary) {
    if (endpoint.scenario_weights.size() !=
        summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "success-participation witness has the wrong scenario count");
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

[[nodiscard]] std::optional<double> central_measure_threshold(
    const cf::SuccessParticipationSummary& summary) {
    const double target = summary.target_worst_expected_npv_million;
    const double q0 = summary.q0.expected_npv_million.central;
    const double q1 = summary.q1.expected_npv_million.central;
    if (q0 >= target) {
        return 0.0;
    }
    const double slope = q1 - q0;
    if (!(slope > 0.0) || q1 < target) {
        return std::nullopt;
    }
    const double fraction = (target - q0) / slope;
    if (!std::isfinite(fraction) || fraction < 0.0 || fraction > 1.0) {
        return std::nullopt;
    }
    return fraction;
}

void print_contract_result(const cf::SuccessParticipationSummary& summary,
    std::string_view currency) {
    const bool feasible_term =
        summary.status ==
            cf::SuccessParticipationSolveStatus::AlreadyMeetsTargetAtZero ||
        summary.status ==
            cf::SuccessParticipationSolveStatus::CertifiedInteriorBracket ||
        summary.status ==
            cf::SuccessParticipationSolveStatus::FullParticipationRequired;
    std::cout << "Robust contract-term result\n"
              << "  solver status: " << cf::to_string(summary.status) << '\n'
              << (feasible_term
                         ? "  reported contractual fraction q: "
                         : "  reported evaluation fraction q: ")
              << summary.reported_fraction << '\n';
    if (summary.exact_minimum_fraction.has_value()) {
        std::cout << "  exact minimum supported fraction: "
                  << *summary.exact_minimum_fraction << '\n';
    }
    if (summary.failing_fraction_lower_bound.has_value() &&
        summary.feasible_fraction_upper_bound.has_value()) {
        std::cout << "  certified failing lower fraction: "
                  << *summary.failing_fraction_lower_bound << '\n'
                  << "  certified feasible upper fraction: "
                  << *summary.feasible_fraction_upper_bound << '\n'
                  << "  certified bracket width: "
                  << *summary.feasible_fraction_upper_bound -
                         *summary.failing_fraction_lower_bound
                  << '\n';
    }

    switch (summary.status) {
    case cf::SuccessParticipationSolveStatus::AlreadyMeetsTargetAtZero:
        std::cout
            << "  decision: the target is already met with selected "
               "participation switched off\n";
        break;
    case cf::SuccessParticipationSolveStatus::CertifiedInteriorBracket:
        std::cout
            << "  decision: the reported upper endpoint is the smallest "
               "certified feasible term at solver precision\n";
        break;
    case cf::SuccessParticipationSolveStatus::FullParticipationRequired:
        std::cout
            << "  decision: only the configured full participation term "
               "meets the robust target\n";
        break;
    case cf::SuccessParticipationSolveStatus::NoSelectedParticipationCash:
        std::cout
            << "  decision: no selected non-principal payoff exists to "
               "solve for\n";
        break;
    case cf::SuccessParticipationSolveStatus::UnattainableAtFullParticipation:
        std::cout
            << "  robust break-even inside contractual domain: no\n"
            << "  robust NPV gap at maximum q: "
            << summary.target_gap_at_full_participation_million << ' '
            << currency << " million\n"
            << "  decision: reject this modeled term as economically "
               "insufficient at the robust target\n";
        break;
    }

    const std::optional<double> central_threshold =
        central_measure_threshold(summary);
    if (central_threshold.has_value()) {
        std::cout << "  central-measure threshold q (context only): "
                  << *central_threshold << '\n';
    } else {
        std::cout
            << "  central-measure threshold q (context only): not attained "
               "inside [0,1]\n";
    }
    std::cout << '\n';
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::PortfolioAmbiguityConfig& ambiguity,
    const cf::SuccessParticipationConfig& participation,
    const cf::SuccessParticipationSummary& summary,
    const cf::PortfolioAmbiguitySummary& configured_portfolio) {
    const std::string& currency = portfolio.currency_label;
    std::cout << std::fixed << std::setprecision(6);
    std::cout
        << "SYNTHETIC SUCCESS-PARTICIPATION TERM ANALYSIS\n"
        << "Not a forecast, fair value, market price, rating, term sheet, "
           "offering document, or investment recommendation.\n\n"
        << "Analysis basis\n"
        << "  portfolio: " << portfolio.scenario_label << '\n'
        << "  probability envelope: " << ambiguity.scenario_label << '\n'
        << "  participation term: " << participation.scenario_label << '\n'
        << "  term source note: " << participation.source_note << '\n'
        << "  measure: robust physical P NPV over fixed joint cash paths\n"
        << "  monetary basis: " << portfolio.monetary_basis << '\n'
        << "  contractual rate domain: 0.000000 to 1.000000\n"
        << "  selected scalable source kinds: ";
    print_source_kinds(participation);
    std::cout << '\n'
              << "  payoff rule: principal component + q * selected "
                 "configured non-principal receipt\n"
              << "  robust target NPV: "
              << participation.target_worst_expected_npv_million << ' '
              << currency << " million\n\n";

    std::cout << "Expected selected participation\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_range_row("full-q nominal payoff",
        summary.full_participation_nominal_million,
        currency + " million");
    print_range_row("full-q present-value payoff",
        summary.full_participation_present_value_million,
        currency + " million");
    std::cout << '\n';

    std::cout << "Expected NPV across the probability envelope\n"
              << "  term point | minimum | central | maximum | unit\n";
    print_range_row("q=0 selected participation off",
        summary.q0.expected_npv_million, currency + " million");
    print_range_row("q=1 configured full participation",
        summary.q1.expected_npv_million, currency + " million");
    print_range_row("reported q",
        summary.reported.expected_npv_million, currency + " million");
    std::cout << '\n';

    print_contract_result(summary, currency);

    std::cout << "Loss exposure (invariant in q)\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_range_row("expected realized principal loss",
        configured_portfolio.expected_principal_loss_million,
        currency + " million");
    print_range_row("probability of any realized principal impairment",
        configured_portfolio.principal_impairment_probability,
        "percent", 100.0);
    std::cout << '\n';

    std::cout
        << "Full-q payoff by selected external source\n"
        << "  source | nominal minimum | nominal central | nominal maximum | "
           "PV minimum | PV central | PV maximum | unit\n";
    for (const cf::SuccessParticipationSourceRange& source :
         summary.source_ranges) {
        std::cout << "  " << cf::to_string(source.source) << " | "
                  << source.full_participation_nominal_million.minimum.value
                  << " | "
                  << source.full_participation_nominal_million.central
                  << " | "
                  << source.full_participation_nominal_million.maximum.value
                  << " | "
                  << source.full_participation_present_value_million.minimum.value
                  << " | "
                  << source.full_participation_present_value_million.central
                  << " | "
                  << source.full_participation_present_value_million.maximum.value
                  << " | " << currency << " million\n";
    }
    std::cout
        << "  Source endpoints are componentwise. Do not add independently "
           "optimized minima or maxima.\n\n";

    std::cout
        << "Scenario payoff ledger\n"
        << "  scenario | q=0 NPV | full-q nominal payoff | full-q PV payoff | "
           "q=1 NPV | reported-q NPV | unit\n";
    for (const cf::SuccessParticipationScenarioResult& scenario :
         summary.scenarios) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.selected_participation_off_npv_million << " | "
                  << scenario.full_participation_nominal_million << " | "
                  << scenario.full_participation_present_value_million
                  << " | " << scenario.configured_q1_npv_million << " | "
                  << scenario.npv_at_reported_fraction_million << " | "
                  << currency << " million\n";
    }
    std::cout << '\n';

    std::cout << "Binding minimum-NPV probability witnesses\n"
              << "  term point | minimum NPV | scenario probability vector\n";
    print_witness("q=0", summary.q0.expected_npv_million.minimum, summary);
    print_witness("q=1", summary.q1.expected_npv_million.minimum, summary);
    print_witness("reported q",
        summary.reported.expected_npv_million.minimum, summary);
    std::cout << '\n';

    std::cout << "Reconciliation controls\n"
              << "  maximum q=1 cash reconstruction error: "
              << summary.maximum_q1_cash_reconstruction_error_million << ' '
              << currency << " million\n"
              << "  maximum principal-loss reconciliation error: "
              << summary.maximum_principal_loss_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum source-capacity violation: "
              << summary.maximum_source_capacity_violation_million << ' '
              << currency << " million\n"
              << "  maximum witness reconciliation error: "
              << summary.maximum_witness_reconciliation_error_million << ' '
              << currency << " million\n"
              << "  maximum endpoint probability error: "
              << summary.maximum_endpoint_probability_error << "\n\n";

    std::cout
        << "Interpretation boundary\n"
        << "  q=0 switches off only the selected non-principal participation; "
           "all unselected receipts remain unchanged.\n"
        << "  q=1 reconstructs the declared configured cash schedule. A rate "
           "above one is not available: it would require a new evidenced and "
           "enforceable cash right, followed by full revalidation.\n"
        << "  Unused scenario cash-source capacity is not an investor asset "
           "and is never converted into one by this solver.\n"
        << "  The robust minimum is recomputed on combined scenario NPV at "
           "every candidate q; separately optimized endpoints are never added.\n"
        << "  The central-measure threshold is context, not the conservative "
           "contract answer. Physical-P NPV is not risk-neutral value, fair "
           "value, a price, or an investable quote.\n"
        << "  These inputs are synthetic. Real use requires evidence for the "
           "cash right, probability calibration, transferability, enforcement, "
           "costs, counterparty performance, and investor pricing.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 4 || argc > 5 ||
        std::string_view(argv[1]).starts_with("--") ||
        std::string_view(argv[2]).starts_with("--") ||
        std::string_view(argv[3]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 5;
    if (print_normalized &&
        std::string_view(argv[4]) != "--print-normalized") {
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
        const cf::SuccessParticipationSummary summary =
            cf::solve_success_participation(
                portfolio, ambiguity, participation);
        const cf::PortfolioAmbiguitySummary configured_portfolio =
            cf::evaluate_portfolio_ambiguity(portfolio, ambiguity);
        print_report(portfolio, ambiguity, participation, summary,
            configured_portfolio);

        if (print_normalized) {
            std::cout << "\nNormalized portfolio configuration\n";
            cf::print_normalized_portfolio_config(std::cout, portfolio);
            std::cout << "\nNormalized probability-envelope configuration\n";
            cf::print_normalized_portfolio_ambiguity_config(
                std::cout, ambiguity);
            std::cout << "\nNormalized success-participation configuration\n";
            cf::print_normalized_success_participation_config(
                std::cout, participation);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "success-participation analysis failed: " << error.what()
                  << '\n';
        return 1;
    }
}
