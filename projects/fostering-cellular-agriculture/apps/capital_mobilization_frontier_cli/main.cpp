// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/probability_polytope_config.hpp>
#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp>
#include <naturalehia/cellular_finance/robust_capital_mobilization_frontier_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr
        << "usage: " << program
        << " <portfolio.cfg> <event-polytope.cfg> "
           "<success-participation.cfg> <frontier-v0.1.cfg> "
           "[--print-normalized]\n"
        << "   or: " << program
        << " <portfolio.cfg> <event-polytope.cfg> "
           "<success-participation.cfg> <base-capital-stack-v0.2.cfg> "
           "<frontier-v0.2.cfg> [--print-normalized]\n"
        << "calibrated_execution_authorized=false\n";
}

[[nodiscard]] std::string_view pass_text(
    const std::optional<bool>& value) noexcept {
    if (!value.has_value()) {
        return "not-declared";
    }
    return *value ? "pass" : "fail";
}

void print_optional_term(std::string_view label,
    const std::optional<double>& value, std::string_view unit) {
    std::cout << "  " << label << ": ";
    if (value.has_value()) {
        std::cout << *value;
        if (!unit.empty()) {
            std::cout << ' ' << unit;
        }
    } else {
        std::cout << "none (not declared)";
    }
    std::cout << '\n';
}

void print_grid(const std::vector<double>& values) {
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (index != 0U) {
            std::cout << ", ";
        }
        std::cout << values[index];
    }
}

[[nodiscard]] bool contains_index(
    const std::vector<std::size_t>& values, std::size_t target) {
    return std::find(values.begin(), values.end(), target) != values.end();
}

[[nodiscard]] std::string_view cash_source_text(
    cf::PortfolioCashSource source) {
    switch (source) {
    case cf::PortfolioCashSource::Commercial:
        return "commercial";
    case cf::PortfolioCashSource::LicensingRoyalty:
        return "licensing_royalty";
    case cf::PortfolioCashSource::ExitSale:
        return "exit_sale";
    case cf::PortfolioCashSource::Recovery:
        return "recovery";
    case cf::PortfolioCashSource::Refinancing:
        return "refinancing";
    case cf::PortfolioCashSource::ExplicitSupport:
        return "explicit_support";
    case cf::PortfolioCashSource::SponsorFee:
        return "sponsor_fee";
    case cf::PortfolioCashSource::FinancingFee:
        return "financing_fee";
    }
    throw std::logic_error("frontier report encountered an unknown cash source");
}

void print_analysis_basis(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation) {
    std::cout << "Analysis basis\n"
              << "  portfolio label: " << portfolio.scenario_label << '\n'
              << "  portfolio source note: " << portfolio.source_note << '\n'
              << "  currency label: " << portfolio.currency_label << '\n'
              << "  monetary basis: " << portfolio.monetary_basis << '\n'
              << "  horizon: " << portfolio.horizon_months << " months\n"
              << "  pool annual physical-measure hurdle: "
              << portfolio.annual_physical_hurdle_rate * 100.0
              << " percent\n"
              << "  complete project-cash scenario count: "
              << portfolio.joint_scenarios.size() << '\n'
              << "  event-polytope label: " << polytope.scenario_label << '\n'
              << "  event-polytope source note: " << polytope.source_note
              << "\n  scenario probability bounds\n";
    for (const auto& scenario : polytope.scenario_probabilities) {
        std::cout << "    " << scenario.scenario_id << " | lower="
                  << scenario.lower_weight * 100.0 << "% | central="
                  << scenario.central_weight * 100.0 << "% | upper="
                  << scenario.upper_weight * 100.0 << "%\n";
    }
    std::cout << "  named event probability bounds\n";
    for (const auto& event : polytope.events) {
        std::cout << "    " << event.event_id << " | " << event.definition
                  << " | lower=" << event.lower_probability * 100.0
                  << "% | upper=" << event.upper_probability * 100.0
                  << "% | members=";
        for (std::size_t index = 0U; index < event.scenario_ids.size();
             ++index) {
            if (index != 0U) {
                std::cout << ',';
            }
            std::cout << event.scenario_ids[index];
        }
        std::cout << '\n';
    }
    std::cout << "  participation label: " << participation.scenario_label
              << '\n'
              << "  participation source note: " << participation.source_note
              << "\n  scalable non-principal cash-source kinds: ";
    for (std::size_t index = 0U;
         index < participation.scalable_source_kinds.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << cash_source_text(
            participation.scalable_source_kinds[index]);
    }
    std::cout
        << "\n  legacy participation-file robust NPV target: "
        << participation.target_worst_expected_npv_million << ' '
        << portfolio.currency_label
        << " million (does not bind this frontier unless repeated as a "
           "frontier mandate)\n\n";
}

void print_index_list(const std::vector<std::size_t>& values) {
    if (values.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t position = 0U; position < values.size(); ++position) {
        if (position != 0U) {
            std::cout << ',';
        }
        std::cout << values[position];
    }
}

void print_candidate_constraint_results(
    const cf::RobustCapitalMobilizationConstraintPasses& passes,
    bool uses_q) {
    std::cout << "  declared constraint results\n"
              << "    minimum robust aggregate NPV: "
              << pass_text(passes.robust_aggregate_npv) << '\n'
              << "    minimum market robust NPV margin: "
              << pass_text(passes.market_robust_npv_margin) << '\n'
              << (uses_q
                      ? "    maximum market expected issued-principal cash shortfall Q: "
                      : "    maximum market expected principal loss: ")
              << pass_text(passes.market_expected_loss_fraction) << '\n'
              << (uses_q
                      ? "    maximum market issued-principal cash-shortfall Q ES95: "
                      : "    maximum market principal-loss ES95: ")
              << pass_text(passes.market_principal_loss_es95_fraction)
              << '\n'
              << (uses_q
                      ? "    maximum market issued-principal cash-shortfall Q ES99: "
                      : "    maximum market principal-loss ES99: ")
              << pass_text(passes.market_principal_loss_es99_fraction)
              << '\n'
              << (uses_q
                      ? "    maximum market Pr[Q>0]: "
                      : "    maximum market principal impairment probability: ")
              << pass_text(passes.market_principal_impairment_probability)
              << '\n'
              << "    maximum market negative-NPV probability: "
              << pass_text(passes.market_negative_npv_probability) << '\n'
              << "    maximum market NPV-shortfall ES95: "
              << pass_text(passes.market_npv_shortfall_es95_fraction)
              << '\n'
              << "    maximum market NPV-shortfall ES99: "
              << pass_text(passes.market_npv_shortfall_es99_fraction)
              << '\n'
              << "    maximum market WAL: "
              << pass_text(passes.market_wal) << '\n'
              << (uses_q ? "    maximum junior issued principal A: "
                         : "    maximum catalytic first loss: ")
              << pass_text(passes.catalytic_first_loss) << '\n'
              << (uses_q ? "    maximum junior NPV concession: "
                         : "    maximum catalytic NPV concession: ")
              << pass_text(passes.catalytic_npv_concession) << '\n';
}

void print_candidate(std::size_t index,
    const cf::RobustCapitalMobilizationFrontierCandidate& candidate,
    const cf::RobustCapitalMobilizationFrontierSummary& summary,
    std::string_view currency) {
    std::cout << "Candidate " << index << " | q="
              << candidate.participation_fraction << " | A="
              << candidate.catalytic_first_loss_million
              << " | market notional=" << candidate.market_notional_million
              << " | feasible="
              << (candidate.all_declared_constraints_pass ? "yes" : "no")
              << " | nondominated="
              << (contains_index(
                      summary.nondominated_feasible_candidate_indices, index)
                         ? "yes"
                         : "no")
              << '\n'
              << "  aggregate fully funded NPV | minimum (robust)="
              << candidate.robust_aggregate_npv_million << " | central="
              << candidate.aggregate_fully_funded_npv_million.central
              << " | maximum="
              << candidate.aggregate_fully_funded_npv_million.maximum.value
              << ' ' << currency << " million\n"
              << (candidate.principal_risk_uses_issued_principal_cash_shortfall_q
                      ? "  junior NPV | minimum (robust)="
                      : "  catalytic NPV | minimum (robust)=")
              << candidate.robust_catalytic_npv_million << " | central="
              << candidate.catalytic_npv_million.central << " | maximum="
              << candidate.catalytic_npv_million.maximum.value << ' '
              << currency << " million\n"
              << "  market NPV | minimum (robust)="
              << candidate.robust_market_npv_million << " | central="
              << candidate.market_npv_million.central << " | maximum="
              << candidate.market_npv_million.maximum.value << ' '
              << currency << " million\n"
              << "  market robust NPV margin: "
              << candidate.robust_market_npv_margin_fraction * 100.0
              << " percent of market notional\n"
              << "  expected market contributions | minimum="
              << candidate.market_expected_contributions_million.minimum.value
              << " | central="
              << candidate.market_expected_contributions_million.central
              << " | maximum="
              << candidate.market_expected_contributions_million.maximum.value
              << ' ' << currency << " million\n"
              << "  expected market total distributions | minimum="
              << candidate.market_expected_total_distributions_million.minimum
                     .value
              << " | central="
              << candidate.market_expected_total_distributions_million.central
              << " | maximum="
              << candidate.market_expected_total_distributions_million.maximum
                     .value
              << ' ' << currency << " million\n"
              << "  expected market principal cash | minimum="
              << candidate.market_expected_principal_cash_distribution_million
                     .minimum.value
              << " | central="
              << candidate.market_expected_principal_cash_distribution_million
                     .central
              << " | maximum="
              << candidate.market_expected_principal_cash_distribution_million
                     .maximum.value
              << ' ' << currency << " million\n"
              << (candidate.principal_risk_uses_issued_principal_cash_shortfall_q
                      ? "  worst expected market issued-principal cash shortfall Q: "
                      : "  worst expected market principal loss: ")
              << candidate.worst_market_expected_loss_fraction * 100.0
              << " percent of market notional | "
              << candidate.worst_market_expected_loss_fraction *
                     candidate.market_notional_million
              << ' ' << currency << " million\n"
              << (candidate.principal_risk_uses_issued_principal_cash_shortfall_q
                      ? "  worst market issued-principal cash-shortfall Q ES95: "
                      : "  worst market principal-loss ES95: ")
              << candidate.worst_market_principal_loss_es95_fraction * 100.0
              << " percent of market notional | "
              << candidate.market_principal_loss_es95_million.maximum.value
              << ' ' << currency << " million\n"
              << (candidate.principal_risk_uses_issued_principal_cash_shortfall_q
                      ? "  worst market issued-principal cash-shortfall Q ES99: "
                      : "  worst market principal-loss ES99: ")
              << candidate.worst_market_principal_loss_es99_fraction * 100.0
              << " percent of market notional | "
              << candidate.market_principal_loss_es99_million.maximum.value
              << ' ' << currency << " million\n"
              << (candidate.principal_risk_uses_issued_principal_cash_shortfall_q
                      ? "  worst market Pr[Q>0]: "
                      : "  worst market principal impairment probability: ")
              << candidate.worst_market_principal_impairment_probability *
                     100.0
              << " percent\n"
              << "  worst market negative-NPV probability: "
              << candidate.worst_market_negative_npv_probability * 100.0
              << " percent\n"
              << "  worst market NPV-shortfall ES95: "
              << candidate.worst_market_npv_shortfall_es95_fraction * 100.0
              << " percent of market notional | "
              << candidate.market_npv_shortfall_es95_million.maximum.value
              << ' ' << currency << " million\n"
              << "  worst market NPV-shortfall ES99: "
              << candidate.worst_market_npv_shortfall_es99_fraction * 100.0
              << " percent of market notional | "
              << candidate.market_npv_shortfall_es99_million.maximum.value
              << ' ' << currency << " million\n";
    if (candidate.market_principal_cash_wal_years.has_value()) {
        const auto& wal = *candidate.market_principal_cash_wal_years;
        std::cout << "  market principal-cash WAL | minimum="
                  << wal.minimum.value_years << " | central="
                  << wal.central_years << " | maximum="
                  << wal.maximum.value_years << " years\n";
    } else {
        std::cout << "  market principal-cash WAL: unavailable\n";
    }
    std::cout << (candidate.principal_risk_uses_issued_principal_cash_shortfall_q
                      ? "  junior NPV concession: "
                      : "  catalytic NPV concession: ")
              << candidate.catalytic_npv_concession_million << ' '
              << currency << " million\n";
    print_candidate_constraint_results(candidate.constraint_passes,
        candidate.principal_risk_uses_issued_principal_cash_shortfall_q);
    std::cout << '\n';
}

void print_probability_vector(const std::vector<double>& weights,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    if (weights.size() != scenarios.size()) {
        throw std::logic_error(
            "frontier endpoint witness has the wrong scenario dimension");
    }
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        if (index != 0U) {
            std::cout << ';';
        }
        std::cout << scenarios[index].scenario_id << '=' << weights[index];
    }
}

void print_linear_witness(std::size_t candidate_index,
    std::string_view metric, std::string_view endpoint_name,
    const cf::ProbabilityPolytopeEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  candidate " << candidate_index << " | " << metric
              << " | " << endpoint_name << " | value=" << endpoint.value
              << " | own p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << '\n';
}

void print_tail_witness(std::size_t candidate_index,
    std::string_view metric,
    const cf::ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios,
    std::string_view currency) {
    std::cout << "  candidate " << candidate_index << " | " << metric
              << " | maximum | value=" << endpoint.value << ' ' << currency
              << " million | own p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << " | own tail mass: ";
    print_probability_vector(endpoint.tail_mass_weights, scenarios);
    std::cout << '\n';
}

void print_wal_witness(std::size_t candidate_index,
    const cf::CapitalStackProbabilityPolytopeWalEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  candidate " << candidate_index
              << " | market principal-cash WAL | maximum | value="
              << endpoint.value_years << " | own common-measure p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << '\n';
}

void print_candidate_witnesses(std::size_t index,
    const cf::RobustCapitalMobilizationFrontierCandidate& candidate,
    std::string_view currency) {
    const bool uses_q =
        candidate.principal_risk_uses_issued_principal_cash_shortfall_q;
    const auto& scenarios =
        candidate.market_principal_loss_es95_million.scenario_probabilities;
    print_linear_witness(index, "aggregate fully funded NPV", "minimum",
        candidate.aggregate_fully_funded_npv_million.minimum, scenarios);
    print_linear_witness(index, uses_q ? "junior NPV" : "catalytic NPV", "minimum",
        candidate.catalytic_npv_million.minimum, scenarios);
    print_linear_witness(index, "market NPV", "minimum",
        candidate.market_npv_million.minimum, scenarios);
    print_linear_witness(index, "market expected contributions", "minimum",
        candidate.market_expected_contributions_million.minimum, scenarios);
    print_linear_witness(index, "market expected contributions", "maximum",
        candidate.market_expected_contributions_million.maximum, scenarios);
    print_linear_witness(index, "market expected total distributions",
        "minimum",
        candidate.market_expected_total_distributions_million.minimum,
        scenarios);
    print_linear_witness(index, "market expected total distributions",
        "maximum",
        candidate.market_expected_total_distributions_million.maximum,
        scenarios);
    print_linear_witness(index,
        uses_q ? "market expected issued-principal cash shortfall Q fraction"
               : "market expected principal loss fraction",
        "maximum", candidate.market_expected_loss_fraction.maximum,
        scenarios);
    print_linear_witness(index, "market expected principal cash", "minimum",
        candidate.market_expected_principal_cash_distribution_million.minimum,
        scenarios);
    print_linear_witness(index, "market expected principal cash", "maximum",
        candidate.market_expected_principal_cash_distribution_million.maximum,
        scenarios);
    print_tail_witness(index,
        uses_q ? "market issued-principal cash-shortfall Q ES95"
               : "market principal-loss ES95",
        candidate.market_principal_loss_es95_million.maximum, scenarios,
        currency);
    print_tail_witness(index,
        uses_q ? "market issued-principal cash-shortfall Q ES99"
               : "market principal-loss ES99",
        candidate.market_principal_loss_es99_million.maximum, scenarios,
        currency);
    print_linear_witness(index,
        uses_q ? "market Pr[Q>0]"
               : "market principal impairment probability",
        "maximum", candidate.market_principal_impairment_probability.maximum,
        scenarios);
    print_linear_witness(index, "market negative-NPV probability", "maximum",
        candidate.market_negative_npv_probability.maximum, scenarios);
    print_tail_witness(index, "market NPV-shortfall ES95",
        candidate.market_npv_shortfall_es95_million.maximum, scenarios,
        currency);
    print_tail_witness(index, "market NPV-shortfall ES99",
        candidate.market_npv_shortfall_es99_million.maximum, scenarios,
        currency);
    if (candidate.market_principal_cash_wal_years.has_value()) {
        print_wal_witness(index,
            candidate.market_principal_cash_wal_years->maximum, scenarios);
    }
}

struct AuditMaxima {
    double stack{0.0};
    double probability{0.0};
    double objective{0.0};
    double reduced_cost{0.0};
    double tail_mass{0.0};
    double tail_objective{0.0};
    double wal_ratio{0.0};
    double wal_root{0.0};
};

[[nodiscard]] AuditMaxima aggregate_audits(
    const cf::RobustCapitalMobilizationFrontierSummary& summary) {
    AuditMaxima maxima;
    for (const auto& candidate : summary.candidates) {
        maxima.stack = std::max(maxima.stack,
            candidate.audit.maximum_stack_accounting_error_million);
        maxima.probability = std::max(maxima.probability,
            candidate.audit.maximum_probability_constraint_violation);
        maxima.objective = std::max(maxima.objective,
            candidate.audit.maximum_objective_reconciliation_error);
        maxima.reduced_cost = std::max(maxima.reduced_cost,
            candidate.audit.maximum_reduced_cost_optimality_residual);
        maxima.tail_mass = std::max(maxima.tail_mass,
            candidate.audit.maximum_tail_mass_violation);
        maxima.tail_objective = std::max(maxima.tail_objective,
            candidate.audit.maximum_tail_objective_reconciliation_error);
        maxima.wal_ratio = std::max(maxima.wal_ratio,
            candidate.audit.maximum_wal_ratio_reconciliation_error_years);
        maxima.wal_root = std::max(maxima.wal_root,
            candidate.audit
                .maximum_wal_root_objective_absolute_residual_million_years);
    }
    return maxima;
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::RobustCapitalMobilizationFrontierConfig& terms,
    const cf::RobustCapitalMobilizationFrontierSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    const bool uses_q =
        summary.principal_risk_uses_issued_principal_cash_shortfall_q;
    std::cout << std::fixed << std::setprecision(6)
              << "SYNTHETIC ROBUST CAPITAL-MOBILIZATION FRONTIER\n"
              << "Finite physical-probability mandate test only; not a price, "
                 "rating, offer, or recommendation.\n\n"
              ;
    print_analysis_basis(portfolio, polytope, participation);
    std::cout << "Fixed instrument terms\n"
              << "  label: " << terms.scenario_label << '\n';
    if (uses_q) {
        std::cout
              << "  frontier model version: " << summary.model_version << '\n'
              << "  capital-stack model version: "
              << summary.capital_stack_model_version << '\n'
              << "  aggregate project outlay limit: "
              << summary.aggregate_project_outlay_limit_million << ' '
              << currency << " million\n"
              << "  aggregate contractual asset-principal limit: "
              << summary.aggregate_contractual_asset_principal_limit_million
              << ' ' << currency << " million\n"
              << "  funded reserve and issued-principal stack detachment K: "
              << summary.funded_reserve_and_stack_detachment_million << ' '
              << currency << " million\n"
              << "  junior claim: " << terms.catalytic_claim_id
              << " | fully funded issued-principal cash-shortfall layer [0,A]; not causal asset-loss attribution\n";
    } else {
        std::cout << "  aggregate commitment and stack detachment K: "
                  << summary.aggregate_commitment_and_stack_detachment_million
                  << ' ' << currency << " million\n"
                  << "  catalytic claim: " << terms.catalytic_claim_id
                  << " | fully funded junior loss-absorbing layer [0,A]\n";
    }
    std::cout << "  market claim: " << terms.market_claim_id
              << " | fully funded priority layer [A,K]\n"
              << "  market notional M=K-A: funded principal notional; it "
                 "excludes additional pro-rata pool-cost calls\n"
              << "  q: contingent participation in declared scalable success "
                 "cash, not a coupon, yield, or ownership percentage\n"
              << "  market lifetime priority non-principal cap: "
              << terms.market_priority_nonprincipal_cap_million << ' '
              << currency << " million\n"
              << (uses_q ? "  junior annual physical-measure hurdle: "
                         : "  catalytic annual physical-measure hurdle: ")
              << terms.catalytic_annual_physical_hurdle_rate * 100.0
              << " percent\n"
              << "  market annual physical-measure hurdle: "
              << terms.market_annual_physical_hurdle_rate * 100.0
              << " percent\n"
              << (uses_q ? "  junior target NPV: "
                         : "  catalytic target NPV: ")
              << terms.catalytic_target_npv_million << ' ' << currency
              << " million\n"
              << "  tested participation grid q: ";
    print_grid(summary.evaluated_participation_fraction_grid);
    std::cout << (uses_q
            ? "\n  tested junior issued-principal grid A: "
            : "\n  tested catalytic first-loss grid A: ");
    print_grid(summary.evaluated_catalytic_first_loss_million_grid);
    std::cout << ' ' << currency << " million\n"
              << "  tested candidate count: " << summary.candidates.size()
              << " (maximum 1,024)\n"
              << "  structural work units: "
              << summary.structural_work_units << " / "
              << summary.structural_work_unit_limit
              << " combined deterministic bound\n"
              << "    portfolio records R: "
              << summary.portfolio_record_count << " (cash="
              << summary.portfolio_cash_record_count << ", auxiliary="
              << summary.portfolio_auxiliary_record_count << ")\n"
              << "    probability projection: "
              << summary.probability_projection_work_units
              << " = candidates * scenarios * (scenarios + events + 1)\n"
              << "    cash-path rebuild: "
              << summary.cash_path_work_units
              << " = candidates * (R + projects * scenarios * "
                 "(horizon + 1) + 2 * scenarios * (horizon + 1))\n\n"
              << "Declared mandates\n";

    const auto& limits = terms.constraints;
    print_optional_term("minimum robust aggregate NPV",
        limits.minimum_robust_aggregate_npv_million,
        std::string(currency) + " million");
    print_optional_term("minimum market robust NPV margin",
        limits.minimum_market_robust_npv_margin_fraction,
        "fraction of market notional");
    print_optional_term(uses_q
            ? "maximum market expected issued-principal cash shortfall Q"
            : "maximum market expected principal loss",
        limits.maximum_market_expected_loss_fraction,
        "fraction of market notional");
    print_optional_term(uses_q
            ? "maximum market issued-principal cash-shortfall Q ES95"
            : "maximum market principal-loss ES95",
        limits.maximum_market_principal_loss_es95_fraction,
        "fraction of market notional");
    print_optional_term(uses_q
            ? "maximum market issued-principal cash-shortfall Q ES99"
            : "maximum market principal-loss ES99",
        limits.maximum_market_principal_loss_es99_fraction,
        "fraction of market notional");
    print_optional_term(uses_q
            ? "maximum market Pr[Q>0]"
            : "maximum market principal impairment probability",
        limits.maximum_market_principal_impairment_probability, "probability");
    print_optional_term("maximum market negative-NPV probability",
        limits.maximum_market_negative_npv_probability, "probability");
    print_optional_term("maximum market NPV-shortfall ES95",
        limits.maximum_market_npv_shortfall_es95_fraction,
        "fraction of market notional");
    print_optional_term("maximum market NPV-shortfall ES99",
        limits.maximum_market_npv_shortfall_es99_fraction,
        "fraction of market notional");
    print_optional_term("maximum market WAL",
        limits.maximum_market_wal_years, "years");
    print_optional_term(uses_q ? "maximum junior issued principal A"
                               : "maximum catalytic first loss",
        limits.maximum_catalytic_first_loss_million,
        std::string(currency) + " million");
    print_optional_term(uses_q ? "maximum junior NPV concession"
                               : "maximum catalytic NPV concession",
        limits.maximum_catalytic_npv_concession_million,
        std::string(currency) + " million");
    std::cout << "  declared mandate count: "
              << summary.declared_constraint_count << "\n\n"
              << "Candidate ledger\n";

    for (std::size_t index = 0U; index < summary.candidates.size(); ++index) {
        print_candidate(index, summary.candidates[index], summary, currency);
    }

    std::cout << "Frontier selections\n  feasible candidate indices: ";
    print_index_list(summary.feasible_candidate_indices);
    std::cout << "\n  nondominated feasible candidate indices: ";
    print_index_list(summary.nondominated_feasible_candidate_indices);
    std::cout << "\n  minimum tested feasible q: ";
    if (summary.minimum_tested_feasible_participation_fraction.has_value()) {
        std::cout << *summary.minimum_tested_feasible_participation_fraction;
    } else {
        std::cout << "none";
    }
    std::cout << (uses_q
            ? "\n  least tested feasible junior issued principal A by q\n"
            : "\n  least tested feasible A by q\n");
    if (summary.least_first_loss_feasible_by_participation.empty()) {
        std::cout << "    none\n";
    } else {
        for (const auto& point :
             summary.least_first_loss_feasible_by_participation) {
            const auto& candidate = summary.candidates[point.candidate_index];
            std::cout << "    q=" << point.participation_fraction
                      << " | candidate=" << point.candidate_index
                      << " | A=" << candidate.catalytic_first_loss_million
                      << ' ' << currency << " million\n";
        }
    }

    std::cout
        << "\nNondominated endpoint witness ledger\n"
        << "  Each row retains that metric's own separately optimized measure. "
           "Different p or tail vectors are not one combined stress.\n";
    if (summary.nondominated_feasible_candidate_indices.empty()) {
        std::cout << "  none\n";
    } else {
        for (const std::size_t index :
             summary.nondominated_feasible_candidate_indices) {
            print_candidate_witnesses(
                index, summary.candidates[index], currency);
        }
    }

    const AuditMaxima audit = aggregate_audits(summary);
    std::cout << "\nAggregate numerical audit maxima across all candidates\n"
              << std::scientific
              << "  stack accounting error: " << audit.stack << ' '
              << currency << " million\n"
              << "  probability-constraint violation: "
              << audit.probability << '\n'
              << "  objective reconciliation error: " << audit.objective
              << '\n'
              << "  reduced-cost optimality residual: "
              << audit.reduced_cost << '\n'
              << "  tail-mass violation: " << audit.tail_mass << '\n'
              << "  tail objective reconciliation error: "
              << audit.tail_objective << '\n'
              << "  WAL ratio reconciliation error: " << audit.wal_ratio
              << " years\n"
              << "  WAL root objective absolute residual: "
              << audit.wal_root << ' ' << currency << " million-years\n"
              << std::fixed
              << "\nInterpretation boundary\n"
              << "  This is an explicitly enumerated finite grid, not a "
                 "continuous optimum or proof that untested terms fail.\n"
              << "  Different metrics can have different adverse probability "
                 "witnesses; their endpoints must not be assembled into one "
                 "forecast or scenario.\n"
              << (uses_q
                    ? "  Fully funded junior capital is an investor claim whose [0,A] layer absorbs issued-principal cash shortfall Q in the modeled liability waterfall. It is not causal attribution of asset loss, a guarantee, insurance policy, or debt characterization; it does not create project value.\n"
                    : "  Fully funded junior loss-absorbing capital is an investor claim in this model, not a guarantee, insurance policy, or a debt characterization. It redistributes modeled cash and loss; it does not create project value.\n")
              << (uses_q
                    ? "  Pool and buyer-direct costs are additional pro-rata calls included in contributions and NPV. Every Q and NPV-shortfall fraction uses market issued principal M, not all-in contributions.\n"
                    : "  Pool costs are additional pro-rata calls included in reported contributions and NPV. Every principal-loss and NPV-shortfall fraction uses funded principal notional M, not all-in contributions.\n")
              << (uses_q
                    ? "  Issued-principal cash shortfall Q is not contractual asset loss L, outstanding asset principal, IFRS 9 ECL, Basel regulatory EL, accounting impairment, recovery, or legal default.\n"
                    : "  Physical expected principal loss is not IFRS 9 ECL, Basel regulatory EL, accounting impairment, or legal default.\n")
              << "  Core model limitation: " << summary.model_limitation
              << '\n'
              << "  No fair value, market price, spread, rating, legal "
                 "enforceability, regulatory capital result, investor demand, "
                 "or actual capital mobilization is established.\n"
              << "  Portfolio source note: " << portfolio.source_note << '\n'
              << "  Event-polytope source note: " << polytope.source_note
              << '\n'
              << "  Participation source note: "
              << participation.source_note << '\n'
              << "  Frontier-terms source note: " << terms.source_note << '\n'
              << "weighted_score_or_continuous_optimum_is_claimed="
              << (summary.weighted_score_or_continuous_optimum_is_claimed
                         ? "true"
                         : "false")
              << '\n'
              << "fair_value_or_market_price_is_estimated="
              << (summary.fair_value_or_market_price_is_estimated ? "true"
                                                                  : "false")
              << '\n'
              << "capital_mobilization_is_established="
              << (summary.capital_mobilization_is_established ? "true"
                                                              : "false")
              << '\n'
              << "calibrated_execution_authorized=false\n";
}

void print_normalized_inputs(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const std::optional<cf::CapitalStackConfig>& base_stack,
    const cf::RobustCapitalMobilizationFrontierConfig& frontier) {
    std::cout << "\nNormalized portfolio configuration\n";
    cf::print_normalized_portfolio_config(std::cout, portfolio);
    std::cout << "\nNormalized event-probability-polytope configuration\n";
    cf::print_normalized_probability_polytope_config(std::cout, polytope);
    std::cout << "\nNormalized success-participation configuration\n";
    cf::print_normalized_success_participation_config(
        std::cout, participation);
    if (base_stack.has_value()) {
        std::cout << "\nNormalized base-capital-stack configuration\n";
        cf::print_normalized_capital_stack_config(std::cout, *base_stack);
    }
    std::cout << "\nNormalized capital-mobilization-frontier configuration\n";
    cf::print_normalized_robust_capital_mobilization_frontier_config(
        std::cout, frontier);
}

} // namespace

int main(int argc, char** argv) {
    const bool legacy_form = argc == 5 ||
        (argc == 6 && std::string_view(argv[5]) == "--print-normalized");
    const bool v02_form =
        (argc == 6 && !std::string_view(argv[5]).starts_with("--")) ||
        (argc == 7 && std::string_view(argv[6]) == "--print-normalized");
    if (!legacy_form && !v02_form) {
        print_usage(argc > 0 ? std::string_view(argv[0])
                             : std::string_view("frontier"));
        return 1;
    }
    const bool print_normalized =
        (legacy_form && argc == 6) || (v02_form && argc == 7);

    cf::PortfolioConfig portfolio;
    cf::ProbabilityPolytopeConfig polytope;
    cf::SuccessParticipationConfig participation;
    std::optional<cf::CapitalStackConfig> base_stack;
    cf::RobustCapitalMobilizationFrontierConfig frontier;
    try {
        portfolio = cf::load_portfolio_config(std::filesystem::path(argv[1]));
        polytope = cf::load_probability_polytope_config(
            std::filesystem::path(argv[2]));
        participation = cf::load_success_participation_config(
            std::filesystem::path(argv[3]));
        if (v02_form) {
            base_stack = cf::load_capital_stack_config(
                std::filesystem::path(argv[4]));
            frontier = cf::load_robust_capital_mobilization_frontier_config(
                std::filesystem::path(argv[5]));
        } else {
            frontier = cf::load_robust_capital_mobilization_frontier_config(
                std::filesystem::path(argv[4]));
        }
    } catch (const std::exception& error) {
        std::cerr << "capital-mobilization-frontier input/configuration failed: "
                  << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 2;
    }

    try {
        const cf::RobustCapitalMobilizationFrontierSummary summary = v02_form
            ? cf::evaluate_robust_capital_mobilization_frontier(portfolio,
                  polytope, participation, *base_stack, frontier)
            : cf::evaluate_robust_capital_mobilization_frontier(
                  portfolio, polytope, participation, frontier);
        print_report(portfolio, polytope, participation, frontier, summary);
        if (print_normalized) {
            print_normalized_inputs(portfolio, polytope, participation,
                base_stack, frontier);
        }
    } catch (const std::exception& error) {
        std::cerr << "capital-mobilization-frontier analysis failed: "
                  << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 3;
    }
    return 0;
}
