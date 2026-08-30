// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_probability_polytope.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] bool near(
    double first, double second, double tolerance = 1.0e-8) {
    return std::abs(first - second) <=
        tolerance * std::max({1.0, std::abs(first), std::abs(second)});
}

[[nodiscard]] cf::ScenarioCashSource make_source(std::string id,
    cf::PortfolioCashSource kind, std::size_t month, double amount) {
    cf::ScenarioCashSource source;
    source.id = std::move(id);
    source.kind = kind;
    source.cash_available.push_back(cf::MonthlyAmount{month, amount});
    return source;
}

[[nodiscard]] cf::ProjectJointPath make_path(std::string project_id,
    std::size_t receipt_month, std::string source_id, double receipt,
    double principal) {
    cf::ProjectJointPath path;
    path.project_id = std::move(project_id);
    path.resolution = cf::ProjectPathResolution::Resolved;
    path.capital_draws.push_back(cf::MonthlyAmount{0U, 10.0});
    path.investor_receipts.push_back(cf::InvestorReceipt{receipt_month,
        std::move(source_id), receipt, principal});
    return path;
}

[[nodiscard]] cf::PortfolioConfig four_state_portfolio() {
    cf::PortfolioConfig portfolio;
    portfolio.scenario_label = "four-state event-polytope stack hand table";
    portfolio.source_note = "synthetic unit-test cash paths only";
    portfolio.currency_label = "TEST";
    portfolio.monetary_basis = "constant test units at close";
    portfolio.horizon_months = 24U;
    portfolio.projects = {
        {"culture-platform", cf::ProjectStage::Research, 10.0},
        {"bioprocess-scaleup", cf::ProjectStage::Pilot, 10.0},
    };

    cf::JointScenario success;
    success.id = "common-success";
    success.weight = 0.62;
    success.pool_costs = {{0U, 0.2}};
    success.cash_sources = {
        make_source("culture-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
        make_source("scaleup-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
    };
    success.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 13.0, 10.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 13.0,
            10.0),
    };

    cf::JointScenario culture_loss;
    culture_loss.id = "culture-loss-scaleup-success";
    culture_loss.weight = 0.18;
    culture_loss.pool_costs = {{0U, 0.2}};
    culture_loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
    };
    culture_loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 24U, "scaleup-commercial", 13.0,
            10.0),
    };

    cf::JointScenario scaleup_loss;
    scaleup_loss.id = "culture-success-scaleup-loss";
    scaleup_loss.weight = 0.18;
    scaleup_loss.pool_costs = {{0U, 0.2}};
    scaleup_loss.cash_sources = {
        make_source("culture-commercial", cf::PortfolioCashSource::Commercial,
            24U, 13.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    scaleup_loss.project_paths = {
        make_path("culture-platform", 24U, "culture-commercial", 13.0, 10.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0, 2.0),
    };

    cf::JointScenario common_loss;
    common_loss.id = "common-loss";
    common_loss.weight = 0.02;
    common_loss.pool_costs = {{0U, 0.2}};
    common_loss.cash_sources = {
        make_source("culture-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
        make_source("scaleup-recovery", cf::PortfolioCashSource::Recovery,
            12U, 2.0),
    };
    common_loss.project_paths = {
        make_path("culture-platform", 12U, "culture-recovery", 2.0, 2.0),
        make_path("bioprocess-scaleup", 12U, "scaleup-recovery", 2.0, 2.0),
    };
    portfolio.joint_scenarios = {
        scaleup_loss, common_loss, success, culture_loss};
    return portfolio;
}

[[nodiscard]] cf::PortfolioAmbiguityConfig component_envelope() {
    cf::PortfolioAmbiguityConfig ambiguity;
    ambiguity.scenario_label = "four-state synthetic component envelope";
    ambiguity.source_note = "invented probability bounds for unit tests";
    ambiguity.scenario_probabilities = {
        {"common-success", 0.50, 0.62, 0.70},
        {"culture-loss-scaleup-success", 0.10, 0.18, 0.25},
        {"culture-success-scaleup-loss", 0.10, 0.18, 0.25},
        {"common-loss", 0.01, 0.02, 0.10},
    };
    return ambiguity;
}

[[nodiscard]] cf::ProbabilityPolytopeConfig component_polytope() {
    const cf::PortfolioAmbiguityConfig ambiguity = component_envelope();
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "four-state event-free component polytope";
    polytope.source_note = "v0.1 delegation equivalence fixture";
    for (const cf::ScenarioProbabilityBounds& bounds :
         ambiguity.scenario_probabilities) {
        polytope.scenario_probabilities.push_back(
            cf::ProbabilityPolytopeScenario{bounds.scenario_id,
                bounds.lower_weight, bounds.central_weight,
                bounds.upper_weight});
    }
    return polytope;
}

[[nodiscard]] cf::ProbabilityPolytopeConfig event_polytope() {
    cf::ProbabilityPolytopeConfig polytope;
    polytope.scenario_label = "overlapping event constraint hand table";
    polytope.source_note = "invented probability events for unit tests";
    polytope.scenario_probabilities = {
        {"common-success", 0.0, 0.62, 1.0},
        {"culture-loss-scaleup-success", 0.0, 0.18, 1.0},
        {"culture-success-scaleup-loss", 0.0, 0.18, 1.0},
        {"common-loss", 0.0, 0.02, 1.0},
    };
    polytope.events = {
        {"culture-platform-impairment", "Culture platform is impaired",
            0.12, 0.30,
            {"culture-loss-scaleup-success", "common-loss"}},
        {"scaleup-impairment", "Scaleup project is impaired", 0.12, 0.30,
            {"culture-success-scaleup-loss", "common-loss"}},
        {"common-process-shock", "Both projects are impaired", 0.01, 0.10,
            {"common-loss"}},
        {"any-project-impairment", "At least one project is impaired", 0.30,
            0.50,
            {"culture-loss-scaleup-success",
                "culture-success-scaleup-loss", "common-loss"}},
    };
    return polytope;
}

[[nodiscard]] cf::SuccessParticipationConfig participation_terms() {
    cf::SuccessParticipationConfig terms;
    terms.scenario_label = "synthetic selected success-payoff terms";
    terms.source_note =
        "unit-test assertion that selected excess cash is scalable";
    terms.selected_nonprincipal_cash_is_contractually_scalable = true;
    terms.scalable_source_kinds = {cf::PortfolioCashSource::Commercial};
    return terms;
}

[[nodiscard]] cf::CapitalStackConfig stack_terms(double participation = 1.0) {
    cf::CapitalStackConfig terms;
    terms.scenario_label = "three-tranche synthetic hand terms";
    terms.source_note = "invented priority caps for unit tests";
    terms.aggregate_commitment_is_fully_funded_at_par_at_month_zero = true;
    terms.subscription_reserve_is_zero_yield_and_lossless = true;
    terms.undrawn_commitment_cancels_and_returns_only_at_horizon = true;
    terms.pool_costs_are_additional_pro_rata_calls = true;
    terms.principal_cash_is_paid_most_senior_first = true;
    terms.nonprincipal_cash_is_paid_to_caps_then_residual = true;
    terms.tranching_does_not_change_project_cash_or_gross_loss = true;
    terms.underlying_success_participation_fraction = participation;
    terms.tranches = {
        {"first-loss-residual", 0.0, 4.0, 0.0, 0.0, true},
        {"intermediate", 4.0, 10.0, 2.0, 0.0, false},
        {"senior", 10.0, 20.0, 1.0, 0.0, false},
    };
    return terms;
}

void check_range_matches(const cf::ProbabilityPolytopeMetricRange& current,
    const cf::AmbiguityMetricRange& prior, std::string_view message) {
    check(near(current.minimum.value, prior.minimum.value) &&
            near(current.central, prior.central) &&
            near(current.maximum.value, prior.maximum.value),
        message);
}

void check_es_matches(
    const cf::ProbabilityPolytopeUpperExpectedShortfallProjection& current,
    const cf::AmbiguityMetricRange& prior, std::string_view message) {
    check(near(current.minimum.value, prior.minimum.value) &&
            near(current.central, prior.central) &&
            near(current.maximum.value, prior.maximum.value),
        message);
}

void test_event_free_matches_v01_for_every_stack_metric() {
    const cf::PortfolioConfig portfolio = four_state_portfolio();
    const cf::SuccessParticipationConfig participation = participation_terms();
    const cf::CapitalStackConfig terms = stack_terms();
    const cf::CapitalStackSummary prior = cf::evaluate_capital_stack(
        portfolio, component_envelope(), participation, terms);
    const cf::CapitalStackProbabilityPolytopeSummary current =
        cf::evaluate_capital_stack_probability_polytope(
            portfolio, component_polytope(), participation, terms);

    check(current.events.empty() && current.tranches.size() ==
            prior.tranches.size(),
        "event-free bridge preserves the stack and declares no events");
    check_range_matches(current.expected_underlying_on_demand_npv_million,
        prior.expected_underlying_on_demand_npv_million,
        "event-free underlying NPV matches v0.1");
    check_range_matches(
        current.expected_fully_funded_stack_npv_at_pool_hurdle_million,
        prior.expected_fully_funded_stack_npv_at_pool_hurdle_million,
        "event-free fully-funded NPV matches v0.1");
    check_range_matches(current.expected_prefunding_drag_npv_million,
        prior.expected_prefunding_drag_npv_million,
        "event-free prefunding drag matches v0.1");

    for (std::size_t index = 0U; index < current.tranches.size(); ++index) {
        const auto& now = current.tranches[index];
        const auto& old = prior.tranches[index];
        check_range_matches(now.expected_contributions_million,
            old.expected_contributions_million,
            "event-free contributions match v0.1");
        check_range_matches(
            now.expected_underlying_principal_cash_distribution_million,
            old.expected_underlying_principal_cash_distribution_million,
            "event-free underlying principal cash matches v0.1");
        check_range_matches(
            now.expected_unused_reserve_principal_return_million,
            old.expected_unused_reserve_principal_return_million,
            "event-free reserve return matches v0.1");
        check_range_matches(now.expected_principal_cash_distribution_million,
            old.expected_principal_cash_distribution_million,
            "event-free principal cash matches v0.1");
        check_range_matches(
            now.expected_nonprincipal_cash_distribution_million,
            old.expected_nonprincipal_cash_distribution_million,
            "event-free nonprincipal cash matches v0.1");
        check_range_matches(now.expected_total_distributions_million,
            old.expected_total_distributions_million,
            "event-free distributions match v0.1");
        check_range_matches(now.expected_realized_principal_loss_million,
            old.expected_realized_principal_loss_million,
            "event-free realized loss matches v0.1");
        check_range_matches(now.expected_realized_principal_loss_fraction,
            old.expected_realized_principal_loss_fraction,
            "event-free loss fraction matches v0.1");
        check_range_matches(
            now.expected_unresolved_principal_exposure_million,
            old.expected_unresolved_principal_exposure_million,
            "event-free unresolved exposure matches v0.1");
        check_range_matches(now.expected_principal_cash_shortfall_million,
            old.expected_principal_cash_shortfall_million,
            "event-free principal shortfall matches v0.1");
        check_range_matches(now.expected_npv_at_tranche_hurdle_million,
            old.expected_npv_at_tranche_hurdle_million,
            "event-free tranche NPV matches v0.1");
        check_range_matches(now.expected_all_in_cash_shortfall_million,
            old.expected_all_in_cash_shortfall_million,
            "event-free all-in shortfall matches v0.1");
        check_range_matches(now.expected_scenario_cash_multiple,
            old.expected_scenario_cash_multiple,
            "event-free scenario cash multiple matches v0.1");
        check_range_matches(now.expected_scenario_net_return_fraction,
            old.expected_scenario_net_return_fraction,
            "event-free scenario return matches v0.1");
        check_range_matches(now.principal_impairment_probability,
            old.principal_impairment_probability,
            "event-free impairment probability matches v0.1");
        check_range_matches(now.principal_exhaustion_probability,
            old.principal_exhaustion_probability,
            "event-free exhaustion probability matches v0.1");
        check_range_matches(now.negative_npv_probability,
            old.negative_npv_probability,
            "event-free negative-NPV probability matches v0.1");
        check_es_matches(now.principal_loss_expected_shortfall_95_million,
            old.principal_loss_expected_shortfall_95_million,
            "event-free loss ES95 matches v0.1");
        check_es_matches(now.principal_loss_expected_shortfall_99_million,
            old.principal_loss_expected_shortfall_99_million,
            "event-free loss ES99 matches v0.1");
        check_es_matches(now.npv_shortfall_expected_shortfall_95_million,
            old.npv_shortfall_expected_shortfall_95_million,
            "event-free NPV ES95 matches v0.1");
        check_es_matches(now.npv_shortfall_expected_shortfall_99_million,
            old.npv_shortfall_expected_shortfall_99_million,
            "event-free NPV ES99 matches v0.1");
        check(now.principal_cash_weighted_average_life_years.has_value() ==
                old.principal_cash_weighted_average_life_years.has_value(),
            "event-free WAL availability matches v0.1");
        if (now.principal_cash_weighted_average_life_years &&
            old.principal_cash_weighted_average_life_years) {
            check(near(now.principal_cash_weighted_average_life_years->minimum
                           .value_years,
                      old.principal_cash_weighted_average_life_years->minimum
                          .value) &&
                    near(now.principal_cash_weighted_average_life_years
                             ->central_years,
                        old.principal_cash_weighted_average_life_years
                            ->central) &&
                    near(now.principal_cash_weighted_average_life_years->maximum
                             .value_years,
                        old.principal_cash_weighted_average_life_years->maximum
                            .value),
                "event-free common-witness WAL matches v0.1");
        }
    }
}

void test_overlapping_events_hand_table_and_tail_risk() {
    cf::SuccessParticipationConfig participation = participation_terms();
    participation.target_worst_expected_npv_million = 1.0;
    const cf::CapitalStackProbabilityPolytopeSummary summary =
        cf::evaluate_capital_stack_probability_polytope(four_state_portfolio(),
            event_polytope(), participation, stack_terms());
    const auto& junior = summary.tranches[0];
    const auto& mezzanine = summary.tranches[1];
    const auto& senior = summary.tranches[2];

    check(near(summary.expected_underlying_on_demand_npv_million.minimum.value,
              -0.8) &&
            near(summary.expected_underlying_on_demand_npv_million.central,
                1.4) &&
            near(summary.expected_underlying_on_demand_npv_million.maximum.value,
                2.39) &&
            !summary.selected_underlying_success_participation_meets_target &&
            near(summary.selected_underlying_target_gap_million, 1.8),
        "event-polytope selected underlying target is recomputed from its own worst endpoint");
    check(near(junior.expected_realized_principal_loss_million.minimum.value,
              1.2) &&
            near(junior.expected_realized_principal_loss_million.central,
                1.52) &&
            near(junior.expected_realized_principal_loss_million.maximum.value,
                2.0) &&
            near(mezzanine.expected_npv_at_tranche_hurdle_million.minimum.value,
                -0.46) &&
            near(mezzanine.expected_npv_at_tranche_hurdle_million.central,
                0.34) &&
            near(mezzanine.expected_npv_at_tranche_hurdle_million.maximum.value,
                0.70) &&
            near(senior.expected_realized_principal_loss_million.minimum.value,
                0.06) &&
            near(senior.expected_realized_principal_loss_million.maximum.value,
                0.60),
        "overlapping events produce the hand-calculated tranche linear ranges");
    check(near(mezzanine.principal_loss_expected_shortfall_95_million.minimum.value,
              4.4) &&
            near(mezzanine.principal_loss_expected_shortfall_95_million.central,
                4.8) &&
            near(mezzanine.principal_loss_expected_shortfall_95_million.maximum.value,
                6.0) &&
            near(senior.principal_loss_expected_shortfall_95_million.minimum.value,
                1.2) &&
            near(senior.principal_loss_expected_shortfall_95_million.central,
                2.4) &&
            near(senior.principal_loss_expected_shortfall_95_million.maximum.value,
                6.0) &&
            near(senior.principal_loss_expected_shortfall_99_million.minimum.value,
                6.0),
        "event-constrained principal-loss ES95 and ES99 match the hand tails");
    check(near(junior.npv_shortfall_expected_shortfall_95_million.minimum.value,
              4.04) &&
            near(mezzanine.npv_shortfall_expected_shortfall_95_million.minimum.value,
                2.86) &&
            near(mezzanine.npv_shortfall_expected_shortfall_95_million.central,
                3.66) &&
            near(mezzanine.npv_shortfall_expected_shortfall_95_million.maximum.value,
                6.06) &&
            near(senior.npv_shortfall_expected_shortfall_95_million.minimum.value,
                1.22) &&
            near(senior.npv_shortfall_expected_shortfall_95_million.maximum.value,
                6.1),
        "event-constrained NPV shortfall ES applies max zero minus NPV");
    check(senior.principal_cash_weighted_average_life_years.has_value() &&
            near(senior.principal_cash_weighted_average_life_years->minimum
                     .value_years,
                1.8723404255319149) &&
            near(senior.principal_cash_weighted_average_life_years
                     ->central_years,
                18.96 / 9.88) &&
            near(senior.principal_cash_weighted_average_life_years->maximum
                     .value_years,
                1.937625754527163),
        "event-constrained WAL uses one probability witness for numerator and denominator");
    check(summary.events.size() == 4U &&
            summary.scenario_probabilities.size() == 4U &&
            summary.maximum_probability_constraint_violation < 1.0e-8 &&
            summary.maximum_objective_reconciliation_error < 1.0e-8 &&
            summary.maximum_tail_mass_violation < 1.0e-8 &&
            summary.maximum_wal_root_objective_absolute_residual_million_years <
                1.0e-8,
        "event, witness, tail, and WAL audit residuals are published and small");
}

void test_non_unit_participation_is_held_fixed() {
    const double participation_fraction = 0.37;
    const cf::CapitalStackProbabilityPolytopeSummary summary =
        cf::evaluate_capital_stack_probability_polytope(four_state_portfolio(),
            event_polytope(), participation_terms(),
            stack_terms(participation_fraction));
    const auto& junior = summary.tranches.front();
    check(near(summary.underlying_success_participation_fraction,
              participation_fraction) &&
            near(summary.expected_underlying_on_demand_npv_million.minimum.value,
                -3.446) &&
            near(summary.expected_underlying_on_demand_npv_million.central,
                -1.624) &&
            near(summary.expected_underlying_on_demand_npv_million.maximum.value,
                -0.8041) &&
            !summary.selected_underlying_success_participation_meets_target &&
            near(summary.selected_underlying_target_gap_million, 3.446),
        "non-unit q is projected without being re-solved to meet the target");
    check(near(junior.expected_total_distributions_million.minimum.value, 2.0) &&
            near(junior.expected_total_distributions_million.central, 2.48) &&
            near(junior.expected_total_distributions_million.maximum.value,
                2.8) &&
            near(junior.expected_npv_at_tranche_hurdle_million.minimum.value,
                -2.04) &&
            near(junior.expected_npv_at_tranche_hurdle_million.central,
                -1.56) &&
            near(junior.expected_npv_at_tranche_hurdle_million.maximum.value,
                -1.24),
        "non-unit q changes only the fixed cash paths before event projection");
}

void test_wal_is_unavailable_when_robust_denominator_can_vanish() {
    cf::ProbabilityPolytopeConfig polytope = event_polytope();
    polytope.events.clear();
    const cf::CapitalStackProbabilityPolytopeSummary summary =
        cf::evaluate_capital_stack_probability_polytope(four_state_portfolio(),
            polytope, participation_terms(), stack_terms());
    check(!summary.tranches.front()
               .principal_cash_weighted_average_life_years.has_value() &&
            summary.tranches.back()
                .principal_cash_weighted_average_life_years.has_value(),
        "WAL is withheld when one feasible measure eliminates a tranche's expected principal cash");
}

void test_permutation_invariance() {
    const cf::CapitalStackProbabilityPolytopeSummary ordered =
        cf::evaluate_capital_stack_probability_polytope(four_state_portfolio(),
            event_polytope(), participation_terms(), stack_terms());
    cf::PortfolioConfig portfolio = four_state_portfolio();
    std::reverse(
        portfolio.joint_scenarios.begin(), portfolio.joint_scenarios.end());
    cf::ProbabilityPolytopeConfig polytope = event_polytope();
    std::reverse(polytope.scenario_probabilities.begin(),
        polytope.scenario_probabilities.end());
    std::reverse(polytope.events.begin(), polytope.events.end());
    for (cf::ProbabilityEventConstraint& event : polytope.events) {
        std::reverse(event.scenario_ids.begin(), event.scenario_ids.end());
    }
    const cf::CapitalStackProbabilityPolytopeSummary permuted =
        cf::evaluate_capital_stack_probability_polytope(
            portfolio, polytope, participation_terms(), stack_terms());
    check(near(ordered.expected_underlying_on_demand_npv_million.minimum.value,
              permuted.expected_underlying_on_demand_npv_million.minimum.value) &&
            near(ordered.tranches[2]
                     .expected_realized_principal_loss_million.maximum.value,
                permuted.tranches[2]
                    .expected_realized_principal_loss_million.maximum.value) &&
            near(ordered.tranches[1]
                     .principal_loss_expected_shortfall_95_million.minimum.value,
                permuted.tranches[1]
                    .principal_loss_expected_shortfall_95_million.minimum.value) &&
            ordered.scenario_probabilities.front().scenario_id ==
                permuted.scenario_probabilities.front().scenario_id &&
            ordered.events.front().event_id == permuted.events.front().event_id,
        "scenario, event, and event-member input permutations preserve canonical results");
    check(ordered.tranches[2].principal_cash_weighted_average_life_years &&
            permuted.tranches[2].principal_cash_weighted_average_life_years &&
            near(ordered.tranches[2]
                     .principal_cash_weighted_average_life_years->minimum
                     .value_years,
                permuted.tranches[2]
                    .principal_cash_weighted_average_life_years->minimum
                    .value_years),
        "WAL endpoint value is permutation invariant");
}

void test_near_unit_sum_private_ledger_regression() {
    cf::PortfolioConfig portfolio = four_state_portfolio();
    for (cf::JointScenario& scenario : portfolio.joint_scenarios) {
        if (scenario.id == "common-success") {
            scenario.weight += 5.0e-13;
        }
    }
    cf::ProbabilityPolytopeConfig polytope = event_polytope();
    for (cf::ProbabilityPolytopeScenario& scenario :
         polytope.scenario_probabilities) {
        if (scenario.scenario_id == "common-success") {
            scenario.central_weight += 5.0e-13;
        }
    }
    const cf::CapitalStackProbabilityPolytopeSummary summary =
        cf::evaluate_capital_stack_probability_polytope(
            portfolio, polytope, participation_terms(), stack_terms());
    double central_sum = 0.0;
    for (const cf::ProbabilityPolytopeScenario& scenario :
         summary.scenario_probabilities) {
        central_sum += scenario.central_weight;
    }
    check(near(central_sum, 1.0, 1.0e-12) &&
            summary.scenarios.size() == 4U,
        "near-unit configured sums do not make the private ledger ambiguity falsely infeasible");
}

} // namespace

int main() {
    test_event_free_matches_v01_for_every_stack_metric();
    test_overlapping_events_hand_table_and_tail_risk();
    test_non_unit_participation_is_held_fixed();
    test_wal_is_unavailable_when_robust_denominator_can_vanish();
    test_permutation_invariance();
    test_near_unit_sum_private_ledger_regression();
    if (failures != 0) {
        std::cerr << failures
                  << " capital-stack probability-polytope test(s) failed\n";
        return 1;
    }
    std::cout << "capital-stack probability-polytope tests passed\n";
    return 0;
}
