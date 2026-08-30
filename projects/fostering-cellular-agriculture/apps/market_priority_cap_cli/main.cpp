// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/probability_polytope_config.hpp>
#include <naturalehia/cellular_finance/robust_market_priority_cap.hpp>
#include <naturalehia/cellular_finance/robust_market_priority_cap_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <algorithm>
#include <cmath>
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
           "<success-participation.cfg> <base-capital-stack.cfg> "
           "<market-priority-cap.cfg> [--print-normalized]\n"
        << "calibrated_execution_authorized=false\n";
}

[[nodiscard]] std::string_view bool_text(bool value) noexcept {
    return value ? "true" : "false";
}

[[nodiscard]] std::string_view yes_no(bool value) noexcept {
    return value ? "yes" : "no";
}

[[nodiscard]] double display_value(double value) noexcept {
    return std::abs(value) < 0.5e-6 ? 0.0 : value;
}

[[nodiscard]] std::string_view pass_text(
    const std::optional<bool>& value) noexcept {
    if (!value.has_value()) {
        return "not-declared";
    }
    return *value ? "pass" : "fail";
}

[[nodiscard]] bool contains_index(
    const std::vector<std::size_t>& indices, std::size_t target) {
    return std::find(indices.begin(), indices.end(), target) != indices.end();
}

[[nodiscard]] bool matches_index(
    const std::optional<std::size_t>& index, std::size_t target) noexcept {
    return index.has_value() && *index == target;
}

void print_optional_term(std::string_view label,
    const std::optional<double>& value, std::string_view unit) {
    std::cout << "  " << label << ": ";
    if (!value.has_value()) {
        std::cout << "none (not declared)\n";
        return;
    }
    std::cout << *value;
    if (!unit.empty()) {
        std::cout << ' ' << unit;
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

void print_index_list(const std::vector<std::size_t>& indices) {
    if (indices.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t position = 0U; position < indices.size(); ++position) {
        if (position != 0U) {
            std::cout << ',';
        }
        std::cout << indices[position];
    }
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
    throw std::logic_error(
        "priority-cap report encountered an unknown cash source");
}

void print_fixed_structure(const cf::PortfolioConfig& portfolio,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& terms,
    const cf::RobustMarketPriorityCapSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    std::cout
        << "Fixed structure and at-par meaning\n"
        << "  cap-term label: " << terms.scenario_label << '\n'
        << "  fixed underlying success participation q: "
        << summary.fixed_underlying_success_participation_fraction << '\n'
        << "  fixed junior first-loss capital A: "
        << summary.fixed_junior_first_loss_million << ' ' << currency
        << " million\n"
        << "  fixed aggregate commitment and stack detachment K: "
        << summary.aggregate_commitment_and_stack_detachment_million << ' '
        << currency << " million\n"
        << "  fixed market principal notional M=K-A: "
        << summary.fixed_market_notional_million << ' ' << currency
        << " million\n"
        << "  junior claim: " << terms.junior_claim_id
        << " | fully funded first-loss residual [0,A]\n"
        << "  market claim: " << terms.market_claim_id
        << " | fully funded priority principal layer [A,K]\n"
        << "  at par: M is subscribed as principal at month zero; this is a "
           "funding convention, not an estimated fair value or issue price\n"
        << "  additional cost calls: pool costs are separate pro-rata "
           "investor contributions, so total market contributions can exceed "
           "M without changing principal notional\n"
        << "  varied term B: the market claim's lifetime priority allocation "
           "cap on available non-principal pool cash; it is not a coupon, "
           "promised return, or new project cash\n"
        << "  cash-transfer identity: increasing B can only transfer modeled "
           "non-principal cash from the junior residual to the market claim\n"
        << "  base-reference B: "
        << summary.base_reference_market_priority_cap_million << ' '
        << currency << " million\n"
        << "  contractual ceiling for B: "
        << summary.contractual_ceiling_million << ' ' << currency
        << " million\n"
        << "  tested finite B grid: ";
    print_grid(summary.evaluated_market_priority_cap_million_grid);
    std::cout
        << ' ' << currency << " million\n"
        << "  fixed junior annual physical-measure hurdle: "
        << summary.fixed_junior_annual_physical_hurdle_rate * 100.0
        << " percent\n"
        << "  fixed market annual physical-measure hurdle: "
        << summary.fixed_market_annual_physical_hurdle_rate * 100.0
        << " percent\n"
        << "  junior target NPV used only for concession measurement: "
        << terms.junior_target_npv_million << ' ' << currency
        << " million\n"
        << "  base-stack label: " << base_stack.scenario_label << '\n'
        << "  base-stack source note: " << base_stack.source_note << '\n'
        << "  cap-term source note: " << terms.source_note << "\n\n";
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
              << "  project count: " << portfolio.projects.size() << '\n'
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
        << " million (does not solve q or B in this term)\n\n";
}

void print_work_and_eligibility(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::RobustMarketPriorityCapSummary& summary) {
    const bool fixed_eligible = !summary.candidates.empty() &&
                                summary.candidates.front()
                                    .fixed_structure_eligible;
    std::cout
        << "Finite-grid scope, raw records, and fixed eligibility\n"
        << "  tested candidate count C: " << summary.candidates.size()
        << " (maximum 1,024)\n"
        << "  scenarios S: " << portfolio.joint_scenarios.size() << '\n'
        << "  events E: " << polytope.events.size() << '\n'
        << "  projects N: " << portfolio.projects.size() << '\n'
        << "  horizon H: " << portfolio.horizon_months << " months\n"
        << "  raw portfolio cash records: "
        << summary.portfolio_cash_record_count << '\n'
        << "  raw portfolio auxiliary records: "
        << summary.portfolio_auxiliary_record_count << '\n'
        << "  raw portfolio records R: " << summary.portfolio_record_count
        << " = cash + auxiliary\n"
        << "  probability work: "
        << summary.probability_projection_work_units
        << " = C * S * (S + E + 1)\n"
        << "  cash-path work: " << summary.cash_path_work_units
        << " = C * (R + N * S * (H + 1) + 2 * S * (H + 1))\n"
        << "  combined structural work: " << summary.structural_work_units
        << " / " << summary.structural_work_unit_limit << '\n'
        << "  fixed-structure declared mandate count: "
        << summary.declared_fixed_structure_constraint_count << '\n'
        << "  cap-sensitive market declared mandate count: "
        << summary.declared_cap_sensitive_market_constraint_count << '\n'
        << "  junior concession mandate declared: "
        << yes_no(summary.junior_concession_constraint_is_declared) << '\n'
        << "  fixed structure eligible across tested B: "
        << yes_no(fixed_eligible)
        << " (B cannot cure a fixed-structure failure)\n\n";
}

void print_mandates(const cf::RobustMarketPriorityCapConfig& terms,
    std::string_view currency) {
    const auto& limits = terms.constraints;
    const std::string monetary_unit = std::string(currency) + " million";
    std::cout << "Declared mandates by economic role\n"
              << "  Fixed-structure eligibility mandates\n";
    print_optional_term("minimum robust aggregate NPV",
        limits.minimum_robust_aggregate_npv_million, monetary_unit);
    print_optional_term("maximum market expected principal loss",
        limits.maximum_market_expected_loss_fraction,
        "fraction of market notional M");
    print_optional_term("maximum market principal-loss ES95",
        limits.maximum_market_principal_loss_es95_fraction,
        "fraction of market notional M");
    print_optional_term("maximum market principal-loss ES99",
        limits.maximum_market_principal_loss_es99_fraction,
        "fraction of market notional M");
    print_optional_term("maximum market principal impairment probability",
        limits.maximum_market_principal_impairment_probability,
        "probability");
    print_optional_term("maximum market WAL",
        limits.maximum_market_wal_years, "years");
    print_optional_term("maximum junior first loss A",
        limits.maximum_catalytic_first_loss_million, monetary_unit);
    std::cout << "  Cap-sensitive market adequacy mandates\n";
    print_optional_term("minimum market robust NPV margin",
        limits.minimum_market_robust_npv_margin_fraction,
        "fraction of market notional M");
    print_optional_term("maximum market negative-NPV probability",
        limits.maximum_market_negative_npv_probability, "probability");
    print_optional_term("maximum market NPV-shortfall ES95",
        limits.maximum_market_npv_shortfall_es95_fraction,
        "fraction of market notional M");
    print_optional_term("maximum market NPV-shortfall ES99",
        limits.maximum_market_npv_shortfall_es99_fraction,
        "fraction of market notional M");
    std::cout << "  Junior concession mandate\n";
    print_optional_term("maximum junior NPV concession",
        limits.maximum_catalytic_npv_concession_million, monetary_unit);
    std::cout << '\n';
}

void print_range(std::string_view label,
    const cf::ProbabilityPolytopeMetricRange& range,
    std::string_view unit, double scale = 1.0) {
    std::cout << "  " << label << " | minimum="
              << range.minimum.value * scale << " | central="
              << range.central * scale << " | maximum="
              << range.maximum.value * scale;
    if (!unit.empty()) {
        std::cout << ' ' << unit;
    }
    std::cout << '\n';
}

void print_npv_range(std::string_view owner, double robust,
    const cf::ProbabilityPolytopeMetricRange& range,
    std::string_view currency) {
    std::cout << "  " << owner << " NPV | minimum (robust)="
              << display_value(robust) << " | central="
              << display_value(range.central) << " | maximum="
              << display_value(range.maximum.value) << ' ' << currency
              << " million\n";
}

void print_candidate_tags(std::size_t index,
    const cf::RobustMarketPriorityCapSummary& summary) {
    bool printed = false;
    const auto print_tag = [&printed](std::string_view tag) {
        if (printed) {
            std::cout << ',';
        }
        std::cout << tag;
        printed = true;
    };
    if (index == summary.base_reference_candidate_index) {
        print_tag("base-reference");
    }
    if (index == summary.contractual_ceiling_candidate_index) {
        print_tag("contractual-ceiling");
    }
    if (matches_index(
            summary.minimum_tested_market_adequate_candidate_index, index)) {
        print_tag("minimum-tested-market-adequate");
    }
    if (matches_index(
            summary.previous_tested_candidate_before_market_adequate_index,
            index)) {
        print_tag("previous-tested-before-market-adequate");
    }
    if (matches_index(summary.minimum_tested_balanced_candidate_index,
            index)) {
        print_tag("minimum-tested-balanced");
    }
    if (matches_index(summary.previous_tested_candidate_before_balanced_index,
            index)) {
        print_tag("previous-tested-before-balanced");
    }
    if (!printed) {
        std::cout << "ordinary-tested-point";
    }
}

void print_candidate_constraint_results(
    const cf::RobustMarketPriorityCapConstraintPasses& passes) {
    std::cout << "  declared mandate results\n"
              << "    minimum robust aggregate NPV: "
              << pass_text(passes.robust_aggregate_npv) << '\n'
              << "    minimum market robust NPV margin: "
              << pass_text(passes.market_robust_npv_margin) << '\n'
              << "    maximum market expected principal loss: "
              << pass_text(passes.market_expected_loss_fraction) << '\n'
              << "    maximum market principal-loss ES95: "
              << pass_text(passes.market_principal_loss_es95_fraction)
              << '\n'
              << "    maximum market principal-loss ES99: "
              << pass_text(passes.market_principal_loss_es99_fraction)
              << '\n'
              << "    maximum market principal impairment probability: "
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
              << "    maximum junior first loss A: "
              << pass_text(passes.catalytic_first_loss) << '\n'
              << "    maximum junior NPV concession: "
              << pass_text(passes.catalytic_npv_concession) << '\n';
}

void print_candidate_audit(
    const cf::RobustMarketPriorityCapCandidateAudit& audit,
    std::string_view currency) {
    std::cout
        << std::scientific
        << "  numerical audit residuals (absolute)\n"
        << "    commitment identity: "
        << audit.maximum_commitment_identity_error_million << ' ' << currency
        << " million\n"
        << "    reserve roll-forward: "
        << audit.maximum_reserve_roll_forward_error_million << ' ' << currency
        << " million\n"
        << "    reserve shortfall: "
        << audit.maximum_reserve_shortfall_million << ' ' << currency
        << " million\n"
        << "    subscription reconciliation: "
        << audit.maximum_subscription_reconciliation_error_million << ' '
        << currency << " million\n"
        << "    pool-cost call reconciliation: "
        << audit.maximum_pool_cost_call_reconciliation_error_million << ' '
        << currency << " million\n"
        << "    principal distribution reconciliation: "
        << audit.maximum_principal_distribution_reconciliation_error_million
        << ' ' << currency << " million\n"
        << "    non-principal distribution reconciliation: "
        << audit.maximum_nonprincipal_distribution_reconciliation_error_million
        << ' ' << currency << " million\n"
        << "    priority non-principal cap violation: "
        << audit.maximum_priority_nonprincipal_cap_violation_million << ' '
        << currency << " million\n"
        << "    realized loss reconciliation: "
        << audit.maximum_realized_loss_reconciliation_error_million << ' '
        << currency << " million\n"
        << "    unresolved exposure reconciliation: "
        << audit.maximum_unresolved_exposure_reconciliation_error_million
        << ' ' << currency << " million\n"
        << "    nominal net-cash reconciliation: "
        << audit.maximum_nominal_net_cash_reconciliation_error_million << ' '
        << currency << " million\n"
        << "    stack NPV reconciliation: "
        << audit.maximum_stack_npv_reconciliation_error_million << ' '
        << currency << " million\n"
        << "    probability-constraint violation: "
        << audit.maximum_probability_constraint_violation << '\n'
        << "    objective reconciliation: "
        << audit.maximum_objective_reconciliation_error << '\n'
        << "    reduced-cost optimality residual: "
        << audit.maximum_reduced_cost_optimality_residual << '\n'
        << "    tail-mass violation: "
        << audit.maximum_tail_mass_violation << '\n'
        << "    tail objective reconciliation: "
        << audit.maximum_tail_objective_reconciliation_error << '\n'
        << "    tail threshold-formula reconciliation: "
        << audit.maximum_tail_threshold_formula_reconciliation_error << '\n'
        << "    tail threshold-enumeration optimality residual: "
        << audit.maximum_tail_threshold_enumeration_optimality_residual
        << '\n'
        << "    WAL numerator reconciliation: "
        << audit.maximum_wal_numerator_reconciliation_error_million_years
        << ' ' << currency << " million-years\n"
        << "    WAL denominator reconciliation: "
        << audit.maximum_wal_denominator_reconciliation_error_million << ' '
        << currency << " million\n"
        << "    WAL ratio reconciliation: "
        << audit.maximum_wal_ratio_reconciliation_error_years << " years\n"
        << "    WAL root-objective reconciliation: "
        << audit.maximum_wal_root_objective_reconciliation_error_million_years
        << ' ' << currency << " million-years\n"
        << "    WAL root-objective absolute residual: "
        << audit.maximum_wal_root_objective_absolute_residual_million_years
        << ' ' << currency << " million-years\n"
        << std::fixed;
}

void print_candidate(std::size_t index,
    const cf::RobustMarketPriorityCapCandidate& candidate,
    const cf::RobustMarketPriorityCapSummary& summary,
    std::string_view currency) {
    const std::string monetary_unit = std::string(currency) + " million";
    std::cout << "Candidate " << index << " | B="
              << candidate.market_priority_nonprincipal_cap_million
              << " | fixed market notional M="
              << candidate.market_notional_million << " | tags=";
    print_candidate_tags(index, summary);
    std::cout << '\n';

    print_npv_range("aggregate fully funded",
        candidate.robust_aggregate_npv_million,
        candidate.aggregate_fully_funded_npv_million, currency);

    print_range("junior expected contributions",
        candidate.junior_expected_contributions_million, monetary_unit);
    print_range("junior expected non-principal distributions",
        candidate.junior_expected_nonprincipal_cash_distribution_million,
        monetary_unit);
    print_range("junior expected total distributions",
        candidate.junior_expected_total_distributions_million, monetary_unit);
    print_range("junior expected scenario cash multiple",
        candidate.junior_expected_scenario_cash_multiple, "multiple");
    print_range("junior expected scenario net return",
        candidate.junior_expected_scenario_net_return_fraction, "percent",
        100.0);
    print_npv_range("junior", candidate.robust_junior_npv_million,
        candidate.junior_npv_million, currency);
    std::cout << "  junior NPV concession=max(0,target-robust NPV): "
              << candidate.junior_npv_concession_million << ' ' << currency
              << " million\n";

    print_range("market expected contributions",
        candidate.market_expected_contributions_million, monetary_unit);
    print_range("market expected principal distributions",
        candidate.market_expected_principal_cash_distribution_million,
        monetary_unit);
    print_range("market expected non-principal distributions",
        candidate.market_expected_nonprincipal_cash_distribution_million,
        monetary_unit);
    print_range("market expected total distributions",
        candidate.market_expected_total_distributions_million, monetary_unit);
    print_range("market expected scenario cash multiple",
        candidate.market_expected_scenario_cash_multiple, "multiple");
    print_range("market expected scenario net return",
        candidate.market_expected_scenario_net_return_fraction, "percent",
        100.0);
    print_npv_range("market", candidate.robust_market_npv_million,
        candidate.market_npv_million, currency);
    std::cout << "  market robust NPV margin: "
              << candidate.robust_market_npv_margin_fraction * 100.0
              << " percent of M\n";
    print_range("market expired priority-cap capacity",
        candidate.market_expired_priority_cap_capacity_million,
        monetary_unit);

    print_range("market expected principal-loss fraction",
        candidate.market_expected_loss_fraction, "percent of M", 100.0);
    std::cout
        << "  worst expected market principal loss: "
        << candidate.worst_market_expected_loss_fraction * 100.0
        << " percent of M\n"
        << "  worst market principal-loss ES95: "
        << candidate.worst_market_principal_loss_es95_fraction * 100.0
        << " percent of M | "
        << candidate.market_principal_loss_es95_million.maximum.value << ' '
        << currency << " million\n"
        << "  worst market principal-loss ES99: "
        << candidate.worst_market_principal_loss_es99_fraction * 100.0
        << " percent of M | "
        << candidate.market_principal_loss_es99_million.maximum.value << ' '
        << currency << " million\n";
    print_range("market principal impairment probability",
        candidate.market_principal_impairment_probability, "percent",
        100.0);
    std::cout
        << "  worst market principal impairment probability: "
        << candidate.worst_market_principal_impairment_probability * 100.0
        << " percent\n";
    print_range("market negative-NPV probability",
        candidate.market_negative_npv_probability, "percent", 100.0);
    std::cout
        << "  worst market negative-NPV probability: "
        << candidate.worst_market_negative_npv_probability * 100.0
        << " percent\n"
        << "  worst market NPV-shortfall ES95: "
        << candidate.worst_market_npv_shortfall_es95_fraction * 100.0
        << " percent of M | "
        << candidate.market_npv_shortfall_es95_million.maximum.value << ' '
        << currency << " million\n"
        << "  worst market NPV-shortfall ES99: "
        << candidate.worst_market_npv_shortfall_es99_fraction * 100.0
        << " percent of M | "
        << candidate.market_npv_shortfall_es99_million.maximum.value << ' '
        << currency << " million\n";
    if (candidate.market_principal_cash_wal_years.has_value()) {
        const auto& wal = *candidate.market_principal_cash_wal_years;
        std::cout << "  market principal-cash WAL | minimum="
                  << wal.minimum.value_years << " | central="
                  << wal.central_years << " | maximum="
                  << wal.maximum.value_years << " years\n";
    } else {
        std::cout << "  market principal-cash WAL: unavailable\n";
    }
    std::cout
        << "  fixed-structure eligible: "
        << yes_no(candidate.fixed_structure_eligible) << '\n'
        << "  cap-sensitive market mandates pass: "
        << yes_no(candidate.cap_sensitive_market_mandates_pass) << '\n'
        << "  market adequate: " << yes_no(candidate.market_adequate) << '\n'
        << "  junior concession limit passes: "
        << yes_no(candidate.junior_concession_limit_passes) << '\n'
        << "  balanced (market adequate and junior limit passes): "
        << yes_no(candidate.balanced) << '\n';
    print_candidate_constraint_results(candidate.constraint_passes);
    print_candidate_audit(candidate.audit, currency);
    std::cout << '\n';
}

void print_probability_vector(const std::vector<double>& weights,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    if (weights.size() != scenarios.size()) {
        throw std::logic_error(
            "priority-cap endpoint witness has the wrong scenario dimension");
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

void print_range_witnesses(std::size_t candidate_index,
    std::string_view metric, const cf::ProbabilityPolytopeMetricRange& range,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    print_linear_witness(
        candidate_index, metric, "minimum", range.minimum, scenarios);
    print_linear_witness(
        candidate_index, metric, "maximum", range.maximum, scenarios);
}

void print_tail_witness(std::size_t candidate_index,
    std::string_view metric, std::string_view endpoint_name,
    const cf::ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios,
    std::string_view currency) {
    std::cout << "  candidate " << candidate_index << " | " << metric
              << " | " << endpoint_name << " | value=" << endpoint.value
              << ' ' << currency << " million | own p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << " | own tail mass: ";
    print_probability_vector(endpoint.tail_mass_weights, scenarios);
    std::cout << '\n';
}

void print_tail_witnesses(std::size_t candidate_index,
    std::string_view metric,
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios,
    std::string_view currency) {
    print_tail_witness(candidate_index, metric, "minimum",
        projection.minimum, scenarios, currency);
    print_tail_witness(candidate_index, metric, "maximum",
        projection.maximum, scenarios, currency);
}

void print_wal_witness(std::size_t candidate_index,
    std::string_view endpoint_name,
    const cf::CapitalStackProbabilityPolytopeWalEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  candidate " << candidate_index
              << " | market principal-cash WAL | " << endpoint_name
              << " | value=" << endpoint.value_years
              << " | numerator=" << endpoint.numerator_million_years
              << " | denominator=" << endpoint.denominator_million
              << " | own common-measure p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << '\n';
}

void print_candidate_witnesses(std::size_t index,
    const cf::RobustMarketPriorityCapCandidate& candidate,
    std::string_view currency) {
    const auto& scenarios =
        candidate.market_principal_loss_es95_million.scenario_probabilities;
    print_range_witnesses(index, "aggregate fully funded NPV",
        candidate.aggregate_fully_funded_npv_million, scenarios);
    print_range_witnesses(index, "junior expected contributions",
        candidate.junior_expected_contributions_million, scenarios);
    print_range_witnesses(index, "junior expected non-principal cash",
        candidate.junior_expected_nonprincipal_cash_distribution_million,
        scenarios);
    print_range_witnesses(index, "junior expected total distributions",
        candidate.junior_expected_total_distributions_million, scenarios);
    print_range_witnesses(index, "junior expected scenario cash multiple",
        candidate.junior_expected_scenario_cash_multiple, scenarios);
    print_range_witnesses(index, "junior expected scenario net return",
        candidate.junior_expected_scenario_net_return_fraction, scenarios);
    print_range_witnesses(
        index, "junior NPV", candidate.junior_npv_million, scenarios);
    print_range_witnesses(index, "market expected contributions",
        candidate.market_expected_contributions_million, scenarios);
    print_range_witnesses(index, "market expected principal cash",
        candidate.market_expected_principal_cash_distribution_million,
        scenarios);
    print_range_witnesses(index, "market expected non-principal cash",
        candidate.market_expected_nonprincipal_cash_distribution_million,
        scenarios);
    print_range_witnesses(index, "market expected total distributions",
        candidate.market_expected_total_distributions_million, scenarios);
    print_range_witnesses(index, "market expected scenario cash multiple",
        candidate.market_expected_scenario_cash_multiple, scenarios);
    print_range_witnesses(index, "market expected scenario net return",
        candidate.market_expected_scenario_net_return_fraction, scenarios);
    print_range_witnesses(
        index, "market NPV", candidate.market_npv_million, scenarios);
    print_range_witnesses(index, "market expired priority-cap capacity",
        candidate.market_expired_priority_cap_capacity_million, scenarios);
    print_range_witnesses(index, "market expected principal-loss fraction",
        candidate.market_expected_loss_fraction, scenarios);
    print_tail_witnesses(index, "market principal-loss ES95",
        candidate.market_principal_loss_es95_million, scenarios, currency);
    print_tail_witnesses(index, "market principal-loss ES99",
        candidate.market_principal_loss_es99_million, scenarios, currency);
    print_range_witnesses(index,
        "market principal impairment probability",
        candidate.market_principal_impairment_probability, scenarios);
    print_range_witnesses(index, "market negative-NPV probability",
        candidate.market_negative_npv_probability, scenarios);
    print_tail_witnesses(index, "market NPV-shortfall ES95",
        candidate.market_npv_shortfall_es95_million, scenarios, currency);
    print_tail_witnesses(index, "market NPV-shortfall ES99",
        candidate.market_npv_shortfall_es99_million, scenarios, currency);
    if (candidate.market_principal_cash_wal_years.has_value()) {
        print_wal_witness(index, "minimum",
            candidate.market_principal_cash_wal_years->minimum, scenarios);
        print_wal_witness(index, "maximum",
            candidate.market_principal_cash_wal_years->maximum, scenarios);
    }
}

void print_selected_index(std::string_view label,
    const std::optional<std::size_t>& index,
    const cf::RobustMarketPriorityCapSummary& summary,
    std::string_view currency) {
    std::cout << "  " << label << ": ";
    if (!index.has_value()) {
        std::cout << "none\n";
        return;
    }
    const auto& candidate = summary.candidates.at(*index);
    std::cout << "candidate " << *index << " | B="
              << candidate.market_priority_nonprincipal_cap_million << ' '
              << currency << " million\n";
}

void print_selections(const cf::RobustMarketPriorityCapSummary& summary,
    std::string_view currency) {
    std::cout << "Priority-cap selection and tested brackets\n"
              << "  status: " << cf::to_string(summary.status)
              << "\n  market-adequate candidate indices: ";
    print_index_list(summary.market_adequate_candidate_indices);
    std::cout << "\n  balanced candidate indices: ";
    print_index_list(summary.balanced_candidate_indices);
    std::cout << '\n';
    print_selected_index("minimum tested market-adequate B",
        summary.minimum_tested_market_adequate_candidate_index, summary,
        currency);
    print_selected_index("previous tested B before market adequacy",
        summary.previous_tested_candidate_before_market_adequate_index,
        summary, currency);
    print_selected_index("minimum tested balanced B",
        summary.minimum_tested_balanced_candidate_index, summary, currency);
    print_selected_index("previous tested B before balance",
        summary.previous_tested_candidate_before_balanced_index, summary,
        currency);
    std::cout << "  base-reference candidate: "
              << summary.base_reference_candidate_index << " | B="
              << summary.candidates.at(summary.base_reference_candidate_index)
                     .market_priority_nonprincipal_cap_million
              << ' ' << currency << " million\n"
              << "  contractual-ceiling candidate: "
              << summary.contractual_ceiling_candidate_index << " | B="
              << summary.candidates
                     .at(summary.contractual_ceiling_candidate_index)
                     .market_priority_nonprincipal_cap_million
              << ' ' << currency << " million\n"
              << "  minimum means only the lowest passing explicitly tested "
                 "B; the adjacent tested bracket is not interpolated into a "
                 "continuous contract solution\n\n";
}

void print_grid_audit(const cf::RobustMarketPriorityCapGridAudit& audit,
    std::string_view currency) {
    std::cout
        << "Across-grid structural, principal, and transfer audit\n"
        << "  base stack was not mutated: "
        << bool_text(audit.base_stack_was_not_mutated) << '\n'
        << "  market contributions invariant: "
        << bool_text(audit.market_contributions_are_invariant) << '\n'
        << "  market principal cash invariant: "
        << bool_text(audit.market_principal_cash_is_invariant) << '\n'
        << "  market principal risk invariant: "
        << bool_text(audit.market_principal_risk_is_invariant) << '\n'
        << "  market principal WAL invariant: "
        << bool_text(audit.market_principal_wal_is_invariant) << '\n'
        << "  market non-principal cash nondecreasing: "
        << bool_text(audit.market_nonprincipal_cash_is_nondecreasing) << '\n'
        << "  market path NPV nondecreasing: "
        << bool_text(audit.market_path_npv_is_nondecreasing) << '\n'
        << "  junior non-principal cash nonincreasing: "
        << bool_text(audit.junior_nonprincipal_cash_is_nonincreasing) << '\n'
        << "  junior path NPV nonincreasing: "
        << bool_text(audit.junior_path_npv_is_nonincreasing) << '\n'
        << "  market negative-NPV probability nonincreasing: "
        << bool_text(
               audit.market_negative_npv_probability_is_nonincreasing)
        << '\n'
        << "  market NPV-shortfall tails nonincreasing: "
        << bool_text(audit.market_npv_shortfall_tails_are_nonincreasing)
        << '\n'
        << "  market cash gained equals junior cash surrendered: "
        << bool_text(audit.market_cash_gained_equals_junior_cash_surrendered)
        << '\n'
        << "  aggregate cash invariant: "
        << bool_text(audit.aggregate_cash_is_invariant) << '\n'
        << "  pool-hurdle NPV invariant: "
        << bool_text(audit.pool_hurdle_npv_is_invariant) << '\n'
        << std::scientific
        << "  maximum market contribution change: "
        << audit.maximum_market_contribution_change_million << ' ' << currency
        << " million\n"
        << "  maximum market principal-cash change: "
        << audit.maximum_market_principal_cash_change_million << ' '
        << currency << " million\n"
        << "  maximum market principal-risk change: "
        << audit.maximum_market_principal_risk_change << '\n'
        << "  maximum market WAL change: "
        << audit.maximum_market_wal_change_years << " years\n"
        << "  maximum market non-principal monotonicity violation: "
        << audit.maximum_market_nonprincipal_monotonicity_violation_million
        << ' ' << currency << " million\n"
        << "  maximum market path-NPV monotonicity violation: "
        << audit.maximum_market_path_npv_monotonicity_violation_million << ' '
        << currency << " million\n"
        << "  maximum junior non-principal monotonicity violation: "
        << audit.maximum_junior_nonprincipal_monotonicity_violation_million
        << ' ' << currency << " million\n"
        << "  maximum junior path-NPV monotonicity violation: "
        << audit.maximum_junior_path_npv_monotonicity_violation_million << ' '
        << currency << " million\n"
        << "  maximum negative-NPV probability monotonicity violation: "
        << audit.maximum_negative_npv_probability_monotonicity_violation
        << '\n'
        << "  maximum NPV-shortfall-tail monotonicity violation: "
        << audit.maximum_npv_shortfall_tail_monotonicity_violation_million
        << ' ' << currency << " million\n"
        << "  maximum market/junior cash-transfer reconciliation error: "
        << audit.maximum_cash_transfer_reconciliation_error_million << ' '
        << currency << " million\n"
        << "  maximum aggregate-cash change: "
        << audit.maximum_aggregate_cash_change_million << ' ' << currency
        << " million\n"
        << "  maximum pool-hurdle NPV change: "
        << audit.maximum_pool_hurdle_npv_change_million << ' ' << currency
        << " million\n\n"
        << std::fixed;
}

void print_interpretation_boundary(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& terms,
    const cf::RobustMarketPriorityCapSummary& summary) {
    std::cout
        << "Interpretation boundary and false-claim ledger\n"
        << "  The result holds q, A, K, M, project cash, probability set, "
           "claim hurdles, and funded-principal ordering fixed. Only B varies.\n"
        << "  Contributions include at-par funded principal plus additional "
           "pro-rata pool-cost calls; returns shown are physical-measure path "
           "sensitivities, not promised or annualized investor returns.\n"
        << "  Each endpoint retains its own adverse measure. Different "
           "witness vectors are not one combined stress or forecast.\n"
        << "  Principal-loss and NPV-shortfall fractions use fixed funded "
           "market principal notional M, not all-in contributions.\n"
        << "  Principal-loss metrics and WAL are structurally invariant to B; "
           "B only reallocates available non-principal cash after the modeled "
           "cash exists.\n"
        << "  Physical expected principal loss is not IFRS 9 ECL, Basel "
           "regulatory EL, accounting impairment, or legal default.\n"
        << "  Core model limitation: " << summary.model_limitation << '\n'
        << "  Portfolio source note: " << portfolio.source_note << '\n'
        << "  Event-polytope source note: " << polytope.source_note << '\n'
        << "  Participation source note: " << participation.source_note
        << '\n'
        << "  Base-stack source note: " << base_stack.source_note << '\n'
        << "  Priority-cap terms source note: " << terms.source_note << '\n'
        << "continuous_minimum_or_optimized_contract_is_claimed="
        << bool_text(
               summary.continuous_minimum_or_optimized_contract_is_claimed)
        << '\n'
        << "market_hurdle_is_solved_or_empirically_calibrated="
        << bool_text(
               summary.market_hurdle_is_solved_or_empirically_calibrated)
        << '\n'
        << "expected_investor_return_or_annualized_yield_is_estimated="
        << bool_text(
               summary.expected_investor_return_or_annualized_yield_is_estimated)
        << '\n'
        << "fair_value_issue_price_or_market_spread_is_estimated="
        << bool_text(
               summary.fair_value_issue_price_or_market_spread_is_estimated)
        << '\n'
        << "investor_demand_or_suitability_is_established="
        << bool_text(summary.investor_demand_or_suitability_is_established)
        << '\n'
        << "legal_form_enforceability_or_regulatory_treatment_is_validated="
        << bool_text(summary
                         .legal_form_enforceability_or_regulatory_treatment_is_validated)
        << '\n'
        << "capital_mobilization_or_crowding_in_is_established="
        << bool_text(
               summary.capital_mobilization_or_crowding_in_is_established)
        << '\n'
        << "calibrated_execution_authorized=false\n";
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& terms,
    const cf::RobustMarketPriorityCapSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    std::cout
        << std::fixed << std::setprecision(6)
        << "SYNTHETIC ROBUST MARKET PRIORITY-CAP ADEQUACY TERM\n"
        << "Finite physical-probability cash-priority sensitivity only; not "
           "a price, coupon, rating, offer, or recommendation.\n\n";
    print_fixed_structure(portfolio, base_stack, terms, summary);
    print_analysis_basis(portfolio, polytope, participation);
    print_work_and_eligibility(portfolio, polytope, summary);
    print_mandates(terms, currency);

    std::cout << "Every tested priority-cap candidate\n";
    for (std::size_t index = 0U; index < summary.candidates.size(); ++index) {
        print_candidate(index, summary.candidates[index], summary, currency);
    }

    print_selections(summary, currency);

    std::cout
        << "Every-candidate endpoint witness ledger\n"
        << "  Each row retains that metric's own separately optimized "
           "probability measure and, for ES, its own tail mass. Different "
           "rows are not one combined stress.\n";
    for (std::size_t index = 0U; index < summary.candidates.size(); ++index) {
        print_candidate_witnesses(index, summary.candidates[index], currency);
    }
    std::cout << '\n';

    print_grid_audit(summary.grid_audit, currency);
    print_interpretation_boundary(portfolio, polytope, participation,
        base_stack, terms, summary);
}

void print_normalized_inputs(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& base_stack,
    const cf::RobustMarketPriorityCapConfig& terms) {
    std::cout << "\nNormalized portfolio configuration\n";
    cf::print_normalized_portfolio_config(std::cout, portfolio);
    std::cout << "\nNormalized event-probability-polytope configuration\n";
    cf::print_normalized_probability_polytope_config(std::cout, polytope);
    std::cout << "\nNormalized success-participation configuration\n";
    cf::print_normalized_success_participation_config(
        std::cout, participation);
    std::cout << "\nNormalized base-capital-stack configuration\n";
    cf::print_normalized_capital_stack_config(std::cout, base_stack);
    std::cout << "\nNormalized robust-market-priority-cap configuration\n";
    cf::print_normalized_robust_market_priority_cap_config(
        std::cout, terms);
}

} // namespace

int main(int argc, char** argv) {
    if ((argc != 6 && argc != 7) ||
        (argc == 7 &&
            std::string_view(argv[6]) != "--print-normalized")) {
        print_usage(argc > 0 ? std::string_view(argv[0])
                             : std::string_view("market-priority-cap"));
        return 1;
    }
    const bool print_normalized = argc == 7;

    cf::PortfolioConfig portfolio;
    cf::ProbabilityPolytopeConfig polytope;
    cf::SuccessParticipationConfig participation;
    cf::CapitalStackConfig base_stack;
    cf::RobustMarketPriorityCapConfig terms;
    try {
        portfolio = cf::load_portfolio_config(std::filesystem::path(argv[1]));
        polytope = cf::load_probability_polytope_config(
            std::filesystem::path(argv[2]));
        participation = cf::load_success_participation_config(
            std::filesystem::path(argv[3]));
        base_stack = cf::load_capital_stack_config(
            std::filesystem::path(argv[4]));
        terms = cf::load_robust_market_priority_cap_config(
            std::filesystem::path(argv[5]));
    } catch (const std::exception& error) {
        std::cerr << "market-priority-cap input/configuration failed: "
                  << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 2;
    }

    try {
        const cf::RobustMarketPriorityCapSummary summary =
            cf::evaluate_robust_market_priority_cap(
                portfolio, polytope, participation, base_stack, terms);
        print_report(portfolio, polytope, participation, base_stack, terms,
            summary);
        if (print_normalized) {
            print_normalized_inputs(
                portfolio, polytope, participation, base_stack, terms);
        }
        std::cout.flush();
        if (!std::cout) {
            throw std::runtime_error(
                "failed while writing market-priority-cap report");
        }
    } catch (const std::exception& error) {
        std::cerr << "market-priority-cap analysis failed: " << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 3;
    }
    return 0;
}
