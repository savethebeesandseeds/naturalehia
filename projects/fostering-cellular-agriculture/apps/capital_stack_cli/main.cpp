// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack.hpp>
#include <naturalehia/cellular_finance/capital_stack_config.hpp>
#include <naturalehia/cellular_finance/capital_stack_probability_polytope.hpp>
#include <naturalehia/cellular_finance/joint_cohort_capital_stack.hpp>
#include <naturalehia/cellular_finance/joint_cohort_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/probability_polytope_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr << "usage:\n  " << program
              << " <portfolio.cfg> <probability-envelope.cfg> "
                 "<success-participation.cfg> <capital-stack.cfg> "
                 "[--print-normalized]\n  "
              << program
              << " --joint-cohort <cohort.cfg> "
                 "<success-participation.cfg> <capital-stack.cfg> "
                 "[--print-normalized]\n  "
              << program
              << " --event-polytope <portfolio.cfg> <polytope.cfg> "
                 "<success-participation.cfg> <capital-stack.cfg> "
                 "[--print-normalized]\n"
              << "calibrated_execution_authorized=false\n";
}

void print_range(std::string_view label,
    const cf::AmbiguityMetricRange& range, std::string_view unit,
    double scale = 1.0) {
    std::cout << "  " << label << " | " << range.minimum.value * scale
              << " | " << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_witness(std::string_view label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::CapitalStackSummary& summary) {
    if (endpoint.scenario_weights.size() !=
        summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "capital-stack witness has the wrong scenario count");
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

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::CapitalStackConfig& terms,
    const cf::CapitalStackSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    std::cout << std::fixed << std::setprecision(6)
              << "SYNTHETIC FULLY FUNDED CAPITAL STACK\n"
              << "Physical-scenario cash allocation only; not fair value, a "
                 "market quote, spread, rating, legal opinion, capital ruling, "
                 "offer, or recommendation.\n\n"
              << "Contract boundary\n"
              << "  aggregate project commitment subscribed at par in month "
                 "zero: "
              << summary.aggregate_commitment_million << ' ' << currency
              << " million\n"
              << "  selected underlying success participation q: "
              << summary.underlying_success_participation_fraction << '\n'
              << "  declared robust underlying NPV target: "
              << summary.underlying_target_worst_expected_npv_million << ' '
              << currency << " million\n"
              << "  selected q meets that target across every probability "
                 "mix feasible within the candidate set: "
              << (summary.selected_underlying_success_participation_meets_target
                         ? "yes"
                         : "no")
              << '\n'
              << "  selected q target shortfall: "
              << summary.selected_underlying_target_gap_million << ' '
              << currency << " million\n"
              << "  subscription reserve: zero-yield and assumed lossless\n"
              << "  unused commitment: returned only at horizon\n"
              << "  project draws: paid inside the prefunded reserve\n"
              << "  pool costs: extra pro-rata calls, not tranche principal\n"
              << "  principal cash priority: most senior first\n"
              << "  resolved principal loss at horizon priority: first-loss first\n"
              << "  non-principal cash: senior/intermediate lifetime caps, "
                 "then residual\n"
              << "  premium or discount to par: none\n\n";

    std::cout << "Pool economics and prefunding\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_range("underlying draw-as-needed expected NPV",
        summary.expected_underlying_on_demand_npv_million,
        std::string(currency) + " million");
    print_range("fully funded stack expected NPV at pool hurdle",
        summary.expected_fully_funded_stack_npv_at_pool_hurdle_million,
        std::string(currency) + " million");
    print_range("prefunding drag",
        summary.expected_prefunding_drag_npv_million,
        std::string(currency) + " million");
    std::cout << '\n';

    for (const cf::CapitalStackTrancheSummary& tranche : summary.tranches) {
        std::cout << "Tranche: " << tranche.tranche_id << '\n'
                  << "  role: "
                  << (tranche.is_first_loss_residual ? "first-loss residual"
                                                     : "priority tranche")
                  << '\n'
                  << "  attachment / detachment / notional: "
                  << tranche.attachment_million << " / "
                  << tranche.detachment_million << " / "
                  << tranche.notional_million << ' ' << currency
                  << " million\n"
                  << "  lifetime non-principal priority cap: "
                  << tranche.priority_nonprincipal_cap_million << ' '
                  << currency << " million\n"
                  << "  declared annual hurdle used with physical-P "
                     "scenario weights: "
                  << tranche.annual_physical_hurdle_rate * 100.0
                  << " percent annual\n"
                  << "  central expected NPV meets hurdle: "
                  << (tranche.central_expected_npv_meets_hurdle ? "yes" : "no")
                  << '\n'
                  << "  every probability mix feasible within the candidate "
                     "set meets expected-NPV hurdle: "
                  << (tranche.robust_expected_npv_meets_hurdle ? "yes" : "no")
                  << "\n  metric | minimum | central | maximum | unit\n";
        print_range("expected contributions",
            tranche.expected_contributions_million,
            std::string(currency) + " million");
        print_range("expected underlying principal cash",
            tranche.expected_underlying_principal_cash_distribution_million,
            std::string(currency) + " million");
        print_range("expected unused-reserve principal return",
            tranche.expected_unused_reserve_principal_return_million,
            std::string(currency) + " million");
        print_range("expected principal cash",
            tranche.expected_principal_cash_distribution_million,
            std::string(currency) + " million");
        print_range("expected non-principal cash",
            tranche.expected_nonprincipal_cash_distribution_million,
            std::string(currency) + " million");
        print_range("expected total distributions",
            tranche.expected_total_distributions_million,
            std::string(currency) + " million");
        print_range("expected resolved principal loss at horizon",
            tranche.expected_realized_principal_loss_million,
            std::string(currency) + " million");
        print_range("expected resolved principal loss rate at horizon",
            tranche.expected_realized_principal_loss_fraction,
            "percent of tranche notional", 100.0);
        print_range("expected unresolved principal exposure",
            tranche.expected_unresolved_principal_exposure_million,
            std::string(currency) + " million");
        print_range("expected all-in cash shortfall",
            tranche.expected_all_in_cash_shortfall_million,
            std::string(currency) + " million");
        print_range("expected NPV at tranche hurdle",
            tranche.expected_npv_at_tranche_hurdle_million,
            std::string(currency) + " million");
        print_range("expected scenario cash multiple",
            tranche.expected_scenario_cash_multiple, "times");
        print_range("expected pathwise net-return fraction",
            tranche.expected_scenario_net_return_fraction, "percent", 100.0);
        print_range("principal impairment probability",
            tranche.principal_impairment_probability, "percent", 100.0);
        print_range("principal exhaustion probability",
            tranche.principal_exhaustion_probability, "percent", 100.0);
        print_range("negative NPV probability",
            tranche.negative_npv_probability, "percent", 100.0);
        print_range("principal loss ES95",
            tranche.principal_loss_expected_shortfall_95_million,
            std::string(currency) + " million");
        print_range("principal loss ES99",
            tranche.principal_loss_expected_shortfall_99_million,
            std::string(currency) + " million");
        print_range("NPV shortfall ES95",
            tranche.npv_shortfall_expected_shortfall_95_million,
            std::string(currency) + " million");
        print_range("NPV shortfall ES99",
            tranche.npv_shortfall_expected_shortfall_99_million,
            std::string(currency) + " million");
        if (tranche.principal_cash_weighted_average_life_years.has_value()) {
            print_range("principal cash-weighted average life",
                *tranche.principal_cash_weighted_average_life_years,
                "years");
        } else {
            std::cout << "  principal cash-weighted average life | "
                         "unavailable | unavailable | unavailable | "
                         "some feasible candidate-set measure has no material expected principal cash at the numerical tolerance\n";
        }
        print_witness("minimum expected-NPV witness",
            tranche.expected_npv_at_tranche_hurdle_million.minimum, summary);
        std::cout << '\n';
    }

    std::cout << "Scenario allocation audit\n"
              << "  scenario | tranche | contributions | project principal | "
                 "reserve return | principal cash | non-principal cash | "
                 "distributions | resolved loss at horizon | "
                 "outstanding | NPV | cash multiple\n";
    for (const cf::CapitalStackScenarioResult& scenario : summary.scenarios) {
        for (const cf::CapitalStackTrancheScenarioResult& tranche :
             scenario.tranches) {
            std::cout << "  " << scenario.scenario_id << " | "
                      << tranche.tranche_id << " | "
                      << tranche.total_contributions_million << " | "
                      << tranche.underlying_principal_cash_distribution_million
                      << " | "
                      << tranche.unused_reserve_principal_return_million
                      << " | "
                      << tranche.principal_cash_distribution_million << " | "
                      << tranche.nonprincipal_cash_distribution_million
                      << " | " << tranche.total_distributions_million << " | "
                      << tranche.realized_principal_loss_million << " | "
                      << tranche.unresolved_principal_exposure_million << " | "
                      << tranche.npv_at_tranche_hurdle_million << " | "
                      << tranche.cash_multiple << '\n';
        }
    }
    std::cout << '\n';

    std::cout << "Reconciliation controls\n"
              << "  maximum commitment identity error: "
              << summary.maximum_commitment_identity_error_million << ' '
              << currency << " million\n"
              << "  maximum subscription error: "
              << summary.maximum_subscription_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum reserve roll-forward error: "
              << summary.maximum_reserve_roll_forward_error_million << ' '
              << currency << " million\n"
              << "  maximum reserve shortfall: "
              << summary.maximum_reserve_shortfall_million << ' ' << currency
              << " million\n"
              << "  maximum pool-cost call error: "
              << summary.maximum_pool_cost_call_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum principal distribution error: "
              << summary.maximum_principal_distribution_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum non-principal distribution error: "
              << summary.maximum_nonprincipal_distribution_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum priority non-principal cap violation: "
              << summary.maximum_priority_nonprincipal_cap_violation_million
              << ' ' << currency << " million\n"
              << "  maximum resolved-loss error: "
              << summary.maximum_realized_loss_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum unresolved exposure error: "
              << summary.maximum_unresolved_exposure_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum nominal net-cash error: "
              << summary.maximum_nominal_net_cash_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum stack NPV error: "
              << summary.maximum_stack_npv_reconciliation_error_million << ' '
              << currency << " million\n"
              << "  maximum WAL common-witness objective residual: "
              << summary.maximum_wal_ratio_objective_residual_million_years
              << ' ' << currency << " million-years\n"
              << "  maximum endpoint probability error: "
              << summary.maximum_endpoint_probability_error << "\n\n";

    std::cout << "Interpretation boundary\n"
              << "  Lower senior loss is redistribution, not value creation. "
                 "All tranche distributions reconcile to the fixed-q pool's "
                 "actual principal and non-principal cash plus return of the "
                 "investors' own unused reserve.\n"
              << "  Priority caps are allocation ceilings, not coupons, PIK, "
                 "guaranteed returns, or new project cash. Unused cap expires.\n"
              << "  Expected scenario cash multiple is E[pathwise multiple], "
                 "not a ratio assembled from unrelated robust endpoints and "
                 "not an annualized return.\n"
              << "  Expected pathwise net-return fraction is E[(cash / calls) "
                 "- 1]; it is not IRR, an annualized return, or an expected "
                 "market return.\n"
              << "  The zero-yield reserve is an explicit synthetic assumption. "
                 "Custody loss, reserve yield, legal enforceability, tax, "
                 "capital-call default, rating, and market price are outside "
                 "version 0.1.\n"
              << "  Source note: " << terms.source_note << '\n';
}

void print_polytope_range(std::string_view label,
    const cf::ProbabilityPolytopeMetricRange& range, std::string_view unit,
    double scale = 1.0) {
    std::cout << "  " << label << " | " << range.minimum.value * scale
              << " | " << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_polytope_tail_range(std::string_view label,
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& range,
    std::string_view unit) {
    std::cout << "  " << label << " | " << range.minimum.value << " | "
              << range.central << " | " << range.maximum.value << " | "
              << unit << '\n';
}

[[nodiscard]] double central_event_probability(
    const cf::ProbabilityEventConstraint& event,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    double total = 0.0;
    for (const std::string& member : event.scenario_ids) {
        bool found = false;
        for (const cf::ProbabilityPolytopeScenario& scenario : scenarios) {
            if (scenario.scenario_id == member) {
                total += scenario.central_weight;
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::logic_error(
                "event-polytope report lost an event member");
        }
    }
    return total;
}

void print_probability_vector(const std::vector<double>& weights,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    if (weights.size() != scenarios.size()) {
        throw std::logic_error(
            "event-polytope capital-stack witness has the wrong dimension");
    }
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout << scenarios[index].scenario_id << '=' << weights[index];
    }
}

void print_linear_endpoint(std::string_view label,
    std::string_view endpoint_name,
    const cf::ProbabilityPolytopeEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  " << label << " | " << endpoint_name << " | "
              << endpoint.value << " | p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << std::scientific
              << " | constraint=" << endpoint.maximum_constraint_violation
              << "; objective="
              << endpoint.objective_reconciliation_error
              << "; reduced_cost=" << endpoint.optimality_residual
              << std::fixed << '\n';
}

void print_tail_endpoint(std::string_view label,
    const cf::ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  " << label << " | maximum | " << endpoint.value
              << " | p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << " | tail: ";
    print_probability_vector(endpoint.tail_mass_weights, scenarios);
    std::cout << std::scientific
              << " | constraint=" << endpoint.maximum_constraint_violation
              << "; tail_mass=" << endpoint.maximum_tail_mass_violation
              << "; objective="
              << endpoint.objective_reconciliation_error
              << "; reduced_cost=" << endpoint.optimality_residual
              << std::fixed << '\n';
}

void print_wal_endpoint(std::string_view label, std::string_view endpoint_name,
    const cf::CapitalStackProbabilityPolytopeWalEndpoint& endpoint,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    std::cout << "  " << label << " | " << endpoint_name << " | "
              << endpoint.value_years << " | p: ";
    print_probability_vector(endpoint.scenario_weights, scenarios);
    std::cout << " | numerator=" << endpoint.numerator_million_years
              << "; denominator=" << endpoint.denominator_million
              << std::scientific
              << "; ratio_error="
              << endpoint.ratio_reconciliation_error_years
              << "; root_residual="
              << endpoint.root_objective_absolute_residual_million_years
              << std::fixed << '\n';
}

void print_event_polytope_stack_report(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& probability_polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& terms,
    const cf::CapitalStackProbabilityPolytopeSummary& summary) {
    const std::string money_unit = portfolio.currency_label + " million";
    std::cout << std::fixed << std::setprecision(6)
              << "SYNTHETIC EVENT-CONSTRAINED FULLY FUNDED CAPITAL STACK\n"
              << "Fixed cash-path allocation under a bounded physical-P "
                 "measure; not fair value, a market price, spread, rating, "
                 "offer, or recommendation.\n\n"
              << "Instrument and target basis\n"
              << "  aggregate commitment subscribed at par at month zero: "
              << summary.aggregate_commitment_million << ' '
              << money_unit << '\n'
              << "  fixed underlying success participation q: "
              << summary.underlying_success_participation_fraction << '\n'
              << "  declared robust underlying NPV target: "
              << summary.underlying_target_worst_expected_npv_million << ' '
              << money_unit << '\n'
              << "  fixed-q worst expected underlying NPV: "
              << summary.expected_underlying_on_demand_npv_million.minimum.value
              << ' ' << money_unit << '\n'
              << "  fixed q meets target under every feasible event measure: "
              << (summary.selected_underlying_success_participation_meets_target
                         ? "yes"
                         : "no")
              << '\n'
              << "  robust target gap: "
              << summary.selected_underlying_target_gap_million << ' '
              << money_unit << "\n\n";

    std::cout << "Event-probability basis\n"
              << "  portfolio: " << portfolio.scenario_label << '\n'
              << "  probability polytope: "
              << probability_polytope.scenario_label << '\n'
              << "  measure: physical P over fixed scenario cash paths\n"
              << "  event membership: explicit scenario sets, never inferred\n"
              << "  scenario | lower | central | upper\n";
    for (const cf::ProbabilityPolytopeScenario& scenario :
         summary.scenario_probabilities) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.lower_weight * 100.0 << "% | "
                  << scenario.central_weight * 100.0 << "% | "
                  << scenario.upper_weight * 100.0 << "%\n";
    }
    std::cout << "  event | definition | lower | central | upper | "
                 "explicit members\n";
    for (const cf::ProbabilityEventConstraint& event : summary.events) {
        std::cout << "  " << event.event_id << " | " << event.definition
                  << " | "
                  << event.lower_probability * 100.0 << "% | "
                  << central_event_probability(
                         event, summary.scenario_probabilities) *
                         100.0
                  << "% | " << event.upper_probability * 100.0 << "% | ";
        for (std::size_t index = 0U; index < event.scenario_ids.size();
             ++index) {
            if (index != 0U) {
                std::cout << "; ";
            }
            std::cout << event.scenario_ids[index];
        }
        std::cout << '\n';
    }
    std::cout << '\n';

    std::cout << "Pool economics and prefunding\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_polytope_range("underlying draw-as-needed expected NPV",
        summary.expected_underlying_on_demand_npv_million, money_unit);
    print_polytope_range("fully funded stack expected NPV at pool hurdle",
        summary.expected_fully_funded_stack_npv_at_pool_hurdle_million,
        money_unit);
    print_polytope_range("prefunding drag",
        summary.expected_prefunding_drag_npv_million, money_unit);
    std::cout << '\n';

    for (const cf::CapitalStackProbabilityPolytopeTrancheSummary& tranche :
         summary.tranches) {
        std::cout << "Tranche: " << tranche.tranche_id << '\n'
                  << "  attachment / detachment / notional: "
                  << tranche.attachment_million << " / "
                  << tranche.detachment_million << " / "
                  << tranche.notional_million << ' ' << money_unit << '\n'
                  << "  central expected NPV meets hurdle: "
                  << (tranche.central_expected_npv_meets_hurdle ? "yes" : "no")
                  << '\n'
                  << "  robust expected-NPV hurdle met under every feasible "
                     "event measure: "
                  << (tranche.robust_expected_npv_meets_hurdle ? "yes" : "no")
                  << "\n  metric | minimum | central | maximum | unit\n";
        print_polytope_range("expected resolved principal loss at horizon",
            tranche.expected_realized_principal_loss_million, money_unit);
        print_polytope_range("principal impairment probability",
            tranche.principal_impairment_probability, "percent", 100.0);
        print_polytope_range("principal exhaustion probability",
            tranche.principal_exhaustion_probability, "percent", 100.0);
        print_polytope_range("expected NPV at tranche hurdle",
            tranche.expected_npv_at_tranche_hurdle_million, money_unit);
        print_polytope_tail_range("principal loss ES95",
            tranche.principal_loss_expected_shortfall_95_million,
            money_unit);
        print_polytope_tail_range("principal loss ES99",
            tranche.principal_loss_expected_shortfall_99_million,
            money_unit);
        print_polytope_tail_range("NPV shortfall ES95",
            tranche.npv_shortfall_expected_shortfall_95_million,
            money_unit);
        print_polytope_tail_range("NPV shortfall ES99",
            tranche.npv_shortfall_expected_shortfall_99_million,
            money_unit);
        if (tranche.principal_cash_weighted_average_life_years.has_value()) {
            const cf::CapitalStackProbabilityPolytopeWalRange& wal =
                *tranche.principal_cash_weighted_average_life_years;
            std::cout << "  principal cash-weighted average life "
                         "(common-measure ratio) | "
                      << wal.minimum.value_years << " | "
                      << wal.central_years << " | "
                      << wal.maximum.value_years << " | years\n";
        } else {
            std::cout
                << "  principal cash-weighted average life "
                   "(common-measure ratio) | unavailable | unavailable | "
                   "unavailable | a feasible measure has no material "
                   "expected principal cash\n";
        }
        std::cout << '\n';
    }

    std::cout
        << "Selected endpoint witness ledger\n"
        << "  Each row retains one complete probability measure; tail rows "
           "also retain fractional tail mass.\n";
    print_linear_endpoint("pool underlying expected NPV", "minimum",
        summary.expected_underlying_on_demand_npv_million.minimum,
        summary.scenario_probabilities);
    for (const cf::CapitalStackProbabilityPolytopeTrancheSummary& tranche :
         summary.tranches) {
        print_linear_endpoint(tranche.tranche_id + " expected NPV", "minimum",
            tranche.expected_npv_at_tranche_hurdle_million.minimum,
            summary.scenario_probabilities);
        print_tail_endpoint(tranche.tranche_id + " principal loss ES95",
            tranche.principal_loss_expected_shortfall_95_million.maximum,
            summary.scenario_probabilities);
        if (tranche.principal_cash_weighted_average_life_years.has_value()) {
            const cf::CapitalStackProbabilityPolytopeWalRange& wal =
                *tranche.principal_cash_weighted_average_life_years;
            print_wal_endpoint(tranche.tranche_id + " WAL", "minimum",
                wal.minimum, summary.scenario_probabilities);
            print_wal_endpoint(tranche.tranche_id + " WAL", "maximum",
                wal.maximum, summary.scenario_probabilities);
        }
    }
    std::cout << '\n';

    std::cout << std::scientific
              << "Numerical and cash-ledger audits\n"
              << "  maximum probability-constraint violation: "
              << summary.maximum_probability_constraint_violation << '\n'
              << "  maximum objective reconciliation error: "
              << summary.maximum_objective_reconciliation_error << '\n'
              << "  maximum simplex reduced-cost residual: "
              << summary.maximum_reduced_cost_optimality_residual << '\n'
              << "  maximum tail-mass violation: "
              << summary.maximum_tail_mass_violation << '\n'
              << "  maximum tail objective reconciliation error: "
              << summary.maximum_tail_objective_reconciliation_error << '\n'
              << "  maximum tail threshold-formula reconciliation error: "
              << summary
                     .maximum_tail_threshold_formula_reconciliation_error
              << '\n'
              << "  maximum tail threshold-enumeration residual: "
              << summary
                     .maximum_tail_threshold_enumeration_optimality_residual
              << '\n'
              << "  maximum WAL ratio reconciliation error: "
              << summary.maximum_wal_ratio_reconciliation_error_years << '\n'
              << "  maximum WAL root absolute residual: "
              << summary
                     .maximum_wal_root_objective_absolute_residual_million_years
              << '\n'
              << "  maximum reserve shortfall: "
              << summary.maximum_reserve_shortfall_million << '\n'
              << "  maximum subscription reconciliation error: "
              << summary.maximum_subscription_reconciliation_error_million
              << '\n'
              << "  maximum principal-distribution reconciliation error: "
              << summary
                     .maximum_principal_distribution_reconciliation_error_million
              << '\n'
              << "  maximum stack-NPV reconciliation error: "
              << summary.maximum_stack_npv_reconciliation_error_million
              << std::fixed << "\n\n";

    std::cout
        << "Interpretation and market boundary\n"
        << "  This candidate set is synthetic and uncalibrated. It is not "
           "empirical evidence, a forecast, confidence region, or a "
           "risk-neutral probability model.\n"
        << "  fair_value_or_market_price_is_estimated="
        << (summary.fair_value_or_market_price_is_estimated ? "true" : "false")
        << '\n'
        << "  legal_enforceability_is_validated="
        << (summary.legal_enforceability_is_validated ? "true" : "false")
        << '\n'
        << "  ratings_or_regulatory_capital_are_validated="
        << (summary.ratings_or_regulatory_capital_are_validated ? "true"
                                                                : "false")
        << '\n'
        << "  project_cash_is_changed_by_tranching="
        << (summary.project_cash_is_changed_by_tranching ? "true" : "false")
        << '\n'
        << "  gross_project_principal_loss_is_changed="
        << (summary.gross_project_principal_loss_is_changed ? "true" : "false")
        << '\n'
        << "  The structure redistributes fixed pool cash and loss; it does "
           "not create project value. Physical-P expected NPV and ES are not "
           "promised investor returns.\n"
        << "  Separate row endpoints can require different feasible measures "
           "and must not be assembled into one scenario. A reported witness "
           "may be nonunique.\n"
        << "  Results are audited floating-point projections, not symbolic "
           "optima or independent dual-gap certificates. Reserve yield, "
           "custody loss, liquidity, tax, legal form, spreads, ratings, and "
           "regulatory capital remain outside the model.\n"
        << "  model limitation: " << summary.model_limitation << '\n'
        << "  portfolio source note: " << portfolio.source_note << '\n'
        << "  polytope source note: " << probability_polytope.source_note
        << '\n'
        << "  participation source note: "
        << participation.source_note << '\n'
        << "  capital-stack source note: " << terms.source_note << '\n'
        << "  calibrated_execution_authorized=false\n";
}

void print_normalized_event_polytope_stack_run(
    const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& probability_polytope,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& terms) {
    std::cout << std::defaultfloat
              << std::setprecision(std::numeric_limits<double>::max_digits10)
              << "\nNormalized portfolio configuration\n";
    cf::print_normalized_portfolio_config(std::cout, portfolio);
    std::cout << "\nNormalized event-probability-polytope configuration\n";
    cf::print_normalized_probability_polytope_config(
        std::cout, probability_polytope);
    std::cout << "\nNormalized success-participation configuration\n";
    cf::print_normalized_success_participation_config(
        std::cout, participation);
    std::cout << "\nNormalized capital-stack configuration\n";
    cf::print_normalized_capital_stack_config(std::cout, terms);
}

void print_joint_cohort_provenance(
    const cf::JointCohortPackage& package,
    const cf::JointCohortCapitalStackResult& result) {
    const cf::JointCohortAnalysisConfig& config = package.config.analysis;
    std::cout << std::fixed << std::setprecision(6)
              << "JOINT-COHORT CANDIDATE INPUT TO CAPITAL STACK\n"
              << "calibrated_execution_authorized=false\n"
              << "candidate_only="
              << (result.cohort.candidate_only ? "true" : "false") << "\n\n"
              << "Statistical conditionality\n"
              << "  This synthetic candidate is conditional on the declared "
                 "population, complete-joint-unit, IID sampling, mapping, "
                 "unknown-outcome, and exclusion assertions. This software "
                 "does not validate their truth or outcome-blind application.\n"
              << "  Included unknown rows remain in N and are compatible with "
                 "every joint state when constructing the outer set.\n"
              << "  The confidence level concerns coverage of the physical-"
                 "probability parameter set. ES95 and ES99 are loss-tail "
                 "statistics under one fixed probability; they are not "
                 "95% or 99% confidence statements.\n\n"
              << "Evidence and replay boundary\n"
              << "  cohort id: " << config.id << '\n'
              << "  as-of date: " << config.as_of_date << '\n'
              << "  probability measure: " << config.probability_measure
              << '\n'
              << "  interval method: " << config.interval_method << '\n'
              << "  confidence level: " << result.cohort.confidence_level
              << '\n'
              << "  bound portfolio SHA-256: "
              << package.config.portfolio_file.sha256 << '\n'
              << "  bound raw ledger SHA-256: "
              << package.config.ledger_file.sha256 << '\n'
              << "  included denominator N: "
              << result.cohort.included_observation_count << '\n'
              << "  matured / unknown / excluded: "
              << result.cohort.matured_count << " / "
              << result.cohort.unknown_count << " / "
              << result.cohort.excluded_count << '\n'
              << "  term files: strict normalized inputs outside the cohort "
                 "portfolio-and-ledger hash binding\n\n";

    if (!result.cohort.primary_outer_set_available) {
        std::cout << "Primary outer set: BLOCKED\n"
                  << "  " << result.block_reason << "\n\n";
        return;
    }
    std::cout << "Primary scenario-probability outer set\n"
              << "  scenario | lower | declared central | upper\n";
    for (const cf::JointCohortScenarioEnvelope& scenario :
         result.cohort.scenario_envelopes) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.primary_lower_weight << " | "
                  << scenario.portfolio_reference_weight << " | "
                  << scenario.primary_upper_weight << '\n';
    }
    std::cout << "  portfolio reference lies inside every primary bound: "
              << (result.cohort.portfolio_reference_within_primary_bounds
                         ? "true"
                         : "false")
              << "\n\n";
    if (result.selected_underlying_financial_ranges.has_value()) {
        const cf::PortfolioAmbiguitySummary& financial =
            *result.selected_underlying_financial_ranges;
        const std::string& currency = package.portfolio.currency_label;
        std::cout
            << "Underlying project risk at selected q before tranching\n"
            << "  project | expected draws min/central/max | expected receipts "
               "min/central/max | outstanding exposure min/central/max | "
               "resolved loss at horizon min/central/max | impairment "
               "probability min/central/max | "
               "negative NPV probability min/central/max | NPV before pool "
               "costs min/central/max\n";
        for (const cf::ProjectAmbiguitySummary& project :
             financial.projects) {
            const auto print_triplet = [](const cf::AmbiguityMetricRange& range,
                                           double scale = 1.0) {
                std::cout << range.minimum.value * scale << '/'
                          << range.central * scale << '/'
                          << range.maximum.value * scale;
            };
            std::cout << "  " << project.project_id << " | ";
            print_triplet(project.expected_total_draws_million);
            std::cout << " | ";
            print_triplet(project.expected_total_receipts_million);
            std::cout << " | ";
            print_triplet(project.expected_outstanding_principal_million);
            std::cout << " | ";
            print_triplet(project.expected_realized_principal_loss_million);
            std::cout << " | ";
            print_triplet(project.principal_impairment_probability, 100.0);
            std::cout << "% | ";
            print_triplet(project.negative_npv_probability, 100.0);
            std::cout << "% | ";
            print_triplet(project.expected_npv_before_pool_costs_million);
            std::cout << '\n';
        }
        std::cout << "  Monetary columns are " << currency
                  << " million; project NPV excludes shared pool costs.\n"
                  << "  Every scalar minimum and maximum has its own feasible "
                     "probability witness; extrema across projects or metrics "
                     "must not be added together.\n\n"
                  << "Common-witness maximum-pool-tail loss attribution\n"
                  << "  project | ES95 attribution | ES99 attribution | unit\n";
        if (financial.principal_loss_tail_attribution_95.projects.size() !=
            financial.principal_loss_tail_attribution_99.projects.size()) {
            throw std::logic_error(
                "cohort project tail attributions have inconsistent taxonomies");
        }
        for (std::size_t index = 0U;
             index <
                 financial.principal_loss_tail_attribution_95.projects.size();
             ++index) {
            const cf::ProjectPoolLossTailContribution& es95 =
                financial.principal_loss_tail_attribution_95.projects[index];
            const cf::ProjectPoolLossTailContribution& es99 =
                financial.principal_loss_tail_attribution_99.projects[index];
            if (es95.project_id != es99.project_id) {
                throw std::logic_error(
                    "cohort project tail attribution order is inconsistent");
            }
            std::cout << "  " << es95.project_id << " | "
                      << es95.at_maximum_pool_es_measure_million << " | "
                      << es99.at_maximum_pool_es_measure_million << " | "
                      << currency << " million\n";
        }
        const auto print_maximum_tail_audit = [&financial, &currency](
            std::string_view label,
            const cf::PoolLossTailAttribution& attribution,
            const cf::AmbiguityMetricRange& pool_es) {
            if (attribution.maximum_pool_es_tail_mass_weights.size() !=
                    financial.scenario_probability_bounds.size() ||
                pool_es.maximum.scenario_weights.size() !=
                    financial.scenario_probability_bounds.size()) {
                throw std::logic_error(
                    "cohort project tail attribution has the wrong scenario count");
            }
            std::cout << "  " << label << " tail probability="
                      << attribution.tail_probability
                      << "; maximum pool ES=" << pool_es.maximum.value
                      << ' ' << currency << " million"
                      << "; maximum probability measure: ";
            for (std::size_t index = 0U;
                 index < attribution.maximum_pool_es_tail_mass_weights.size();
                 ++index) {
                if (index != 0U) {
                    std::cout << "; ";
                }
                std::cout
                    << financial.scenario_probability_bounds[index].scenario_id
                    << '='
                    << pool_es.maximum.scenario_weights[index];
            }
            std::cout << "; fractional tail mass: ";
            for (std::size_t index = 0U;
                 index < attribution.maximum_pool_es_tail_mass_weights.size();
                 ++index) {
                if (index != 0U) {
                    std::cout << "; ";
                }
                std::cout
                    << financial.scenario_probability_bounds[index].scenario_id
                    << '='
                    << attribution.maximum_pool_es_tail_mass_weights[index];
            }
            std::cout << '\n';
        };
        print_maximum_tail_audit("ES95",
            financial.principal_loss_tail_attribution_95,
            financial.principal_loss_expected_shortfall_95_million);
        print_maximum_tail_audit("ES99",
            financial.principal_loss_tail_attribution_99,
            financial.principal_loss_expected_shortfall_99_million);
        std::cout
            << "  Project contributions reconcile to the shown pool ES total. "
               "All projects in a column use one shared adverse probability and "
               "fractional-tail witness; these are not separate project "
               "maxima. Exact ties use the engine's canonical pro-rata "
               "allocation; another ES-optimal attribution may also exist.\n\n";
    }
    if (!result.capital_stack.has_value()) {
        std::cout << "Capital-stack evaluation: BLOCKED\n"
                  << "  " << result.block_reason << "\n\n";
    }
}

void print_normalized_joint_cohort_run(
    const cf::JointCohortPackage& package,
    const cf::JointCohortCapitalStackResult& result,
    const cf::SuccessParticipationConfig& participation,
    const cf::CapitalStackConfig& terms) {
    std::cout << std::defaultfloat
              << std::setprecision(std::numeric_limits<double>::max_digits10)
              << "\nNormalized semantic renderings\n"
              << "These renderings are audit-friendly representations, not a "
                 "directly reloadable hash-consistent cohort package. Comments "
                 "are removed and ledger rows may be sorted; recompute the "
                 "portfolio and ledger SHA-256 values before using the rendered "
                 "files as a new package.\n"
              << "\nNormalized joint-cohort configuration (retains original "
                 "raw-file hashes)\n";
    cf::print_normalized_joint_cohort_config(std::cout, package.config);
    std::cout << "\nNormalized bound portfolio configuration\n";
    cf::print_normalized_portfolio_config(std::cout, package.portfolio);
    if (result.cohort.generated_probability_envelope.has_value()) {
        std::cout
            << "\nNormalized generated probability-envelope configuration\n";
        cf::print_normalized_portfolio_ambiguity_config(
            std::cout, *result.cohort.generated_probability_envelope);
    }
    std::cout
        << "\nNormalized semantic rendering of authoritative ledger rows\n";
    cf::print_normalized_joint_cohort_ledger(
        std::cout, package.observations);
    std::cout << "\nNormalized success-participation configuration\n";
    cf::print_normalized_success_participation_config(
        std::cout, participation);
    std::cout << "\nNormalized capital-stack configuration\n";
    cf::print_normalized_capital_stack_config(std::cout, terms);
}

} // namespace

int main(int argc, char* argv[]) {
    const bool event_polytope_mode =
        argc >= 2 && std::string_view(argv[1]) == "--event-polytope";
    if (event_polytope_mode) {
        if (argc < 6 || argc > 7 ||
            std::string_view(argv[2]).starts_with("--") ||
            std::string_view(argv[3]).starts_with("--") ||
            std::string_view(argv[4]).starts_with("--") ||
            std::string_view(argv[5]).starts_with("--")) {
            print_usage(argv[0]);
            return 2;
        }
        const bool print_normalized = argc == 7;
        if (print_normalized &&
            std::string_view(argv[6]) != "--print-normalized") {
            print_usage(argv[0]);
            return 2;
        }

        try {
            const cf::PortfolioConfig portfolio =
                cf::load_portfolio_config(std::filesystem::path(argv[2]));
            const cf::ProbabilityPolytopeConfig probability_polytope =
                cf::load_probability_polytope_config(
                    std::filesystem::path(argv[3]));
            const cf::SuccessParticipationConfig participation =
                cf::load_success_participation_config(
                    std::filesystem::path(argv[4]));
            const cf::CapitalStackConfig terms =
                cf::load_capital_stack_config(
                    std::filesystem::path(argv[5]));
            const cf::CapitalStackProbabilityPolytopeSummary summary =
                cf::evaluate_capital_stack_probability_polytope(portfolio,
                    probability_polytope, participation, terms);
            print_event_polytope_stack_report(portfolio,
                probability_polytope, participation, terms, summary);
            if (print_normalized) {
                print_normalized_event_polytope_stack_run(portfolio,
                    probability_polytope, participation, terms);
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr
                << "event-probability capital-stack analysis failed: "
                << error.what() << '\n'
                << "calibrated_execution_authorized=false\n";
            return 1;
        }
    }

    const bool joint_cohort_mode =
        argc >= 2 && std::string_view(argv[1]) == "--joint-cohort";
    if (argc < 5 || argc > 6 ||
        (!joint_cohort_mode &&
            (std::string_view(argv[1]).starts_with("--") ||
                std::string_view(argv[2]).starts_with("--") ||
                std::string_view(argv[3]).starts_with("--") ||
                std::string_view(argv[4]).starts_with("--"))) ||
        (joint_cohort_mode &&
            (std::string_view(argv[2]).starts_with("--") ||
                std::string_view(argv[3]).starts_with("--") ||
                std::string_view(argv[4]).starts_with("--")))) {
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
        if (joint_cohort_mode) {
            const cf::JointCohortPackage package =
                cf::load_joint_cohort_package(
                    std::filesystem::path(argv[2]));
            const cf::SuccessParticipationConfig participation =
                cf::load_success_participation_config(
                    std::filesystem::path(argv[3]));
            const cf::CapitalStackConfig terms =
                cf::load_capital_stack_config(
                    std::filesystem::path(argv[4]));
            const cf::JointCohortCapitalStackResult bridge =
                cf::evaluate_joint_cohort_capital_stack(
                    package.config.analysis, package.portfolio,
                    package.observations, participation, terms);
            print_joint_cohort_provenance(package, bridge);
            if (!bridge.capital_stack.has_value()) {
                if (print_normalized) {
                    print_normalized_joint_cohort_run(
                        package, bridge, participation, terms);
                }
                return 3;
            }
            print_report(package.portfolio, terms, *bridge.capital_stack);
            if (print_normalized) {
                print_normalized_joint_cohort_run(
                    package, bridge, participation, terms);
            }
            return 0;
        }

        const cf::PortfolioConfig portfolio =
            cf::load_portfolio_config(std::filesystem::path(argv[1]));
        const cf::PortfolioAmbiguityConfig ambiguity =
            cf::load_portfolio_ambiguity_config(
                std::filesystem::path(argv[2]));
        const cf::SuccessParticipationConfig participation =
            cf::load_success_participation_config(
                std::filesystem::path(argv[3]));
        const cf::CapitalStackConfig terms =
            cf::load_capital_stack_config(std::filesystem::path(argv[4]));
        const cf::CapitalStackSummary summary = cf::evaluate_capital_stack(
            portfolio, ambiguity, participation, terms);
        print_report(portfolio, terms, summary);

        if (print_normalized) {
            std::cout << "\nNormalized portfolio configuration\n";
            cf::print_normalized_portfolio_config(std::cout, portfolio);
            std::cout << "\nNormalized probability-envelope configuration\n";
            cf::print_normalized_portfolio_ambiguity_config(
                std::cout, ambiguity);
            std::cout << "\nNormalized success-participation configuration\n";
            cf::print_normalized_success_participation_config(
                std::cout, participation);
            std::cout << "\nNormalized capital-stack configuration\n";
            cf::print_normalized_capital_stack_config(std::cout, terms);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "capital-stack analysis failed: " << error.what() << '\n'
                  << "calibrated_execution_authorized=false\n";
        return 1;
    }
}
