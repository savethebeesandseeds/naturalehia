// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack_probability_polytope.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kWalMinimumExpectedPrincipalCashMillion = 1.0e-10;
constexpr std::size_t kMaximumWalIterations = 128U;
constexpr double kWalAbsoluteObjectiveTolerance = 1.0e-9;
constexpr double kWalAbsoluteRatioTolerance = 1.0e-12;

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
};

[[nodiscard]] double checked_double(
    long double value, std::string_view description) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            std::string(description) + " is outside the finite double range");
    }
    return converted;
}

// The v0.1 capital-stack evaluator is used only as the validated deterministic
// cash-path ledger. Its private ambiguity bounds deliberately carry no
// financial meaning: [0,1] is guaranteed aggregate-feasible even when
// configured central doubles sum just above or below one. The central weights
// are copied from the portfolio, so the v0.1 checksum normalizes the same raw
// values on both sides. None of the private evaluator's risk ranges, endpoint
// witnesses, WAL results, or target conclusions are consumed by v0.2.
[[nodiscard]] PortfolioAmbiguityConfig make_private_ledger_ambiguity(
    const PortfolioConfig& portfolio) {
    std::vector<const JointScenario*> ordered;
    ordered.reserve(portfolio.joint_scenarios.size());
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        ordered.push_back(&scenario);
    }
    std::sort(ordered.begin(), ordered.end(),
        [](const JointScenario* first, const JointScenario* second) {
            return first->id < second->id;
        });

    PortfolioAmbiguityConfig locked;
    locked.scenario_label =
        "private stack cash-path ledger measure";
    locked.source_note =
        "Internal broad bounds used only to evaluate deterministic stack ledgers";
    locked.scenario_probabilities.reserve(ordered.size());
    for (std::size_t index = 0U; index < ordered.size(); ++index) {
        locked.scenario_probabilities.push_back(ScenarioProbabilityBounds{
            ordered[index]->id, 0.0, ordered[index]->weight, 1.0});
    }
    return locked;
}

template <typename Selector>
[[nodiscard]] ProbabilityPolytopeMetricProjection project_metric(
    const ProbabilityPolytopeProjector& projector,
    const std::vector<CapitalStackScenarioResult>& scenarios,
    Selector selector) {
    std::vector<ProbabilityPolytopeScenarioValue> values;
    values.reserve(scenarios.size());
    for (const CapitalStackScenarioResult& scenario : scenarios) {
        const double value = selector(scenario);
        if (!std::isfinite(value)) {
            throw std::logic_error(
                "capital-stack probability-polytope projection contains a non-finite value");
        }
        values.push_back(
            ProbabilityPolytopeScenarioValue{scenario.scenario_id, value});
    }
    return projector.project_expectation(values);
}

template <typename Selector>
[[nodiscard]] ProbabilityPolytopeUpperExpectedShortfallProjection project_es(
    const ProbabilityPolytopeProjector& projector,
    const std::vector<CapitalStackScenarioResult>& scenarios,
    Selector selector, double tail_probability) {
    std::vector<ProbabilityPolytopeScenarioValue> values;
    values.reserve(scenarios.size());
    for (const CapitalStackScenarioResult& scenario : scenarios) {
        const double value = selector(scenario);
        if (!std::isfinite(value) || value < 0.0) {
            throw std::logic_error(
                "capital-stack probability-polytope tail projection requires finite non-negative values");
        }
        values.push_back(
            ProbabilityPolytopeScenarioValue{scenario.scenario_id, value});
    }
    return projector.project_upper_expected_shortfall(
        values, tail_probability);
}

void update_linear_audits(
    const ProbabilityPolytopeMetricProjection& projection,
    CapitalStackProbabilityPolytopeSummary& summary) {
    summary.maximum_probability_constraint_violation = std::max(
        summary.maximum_probability_constraint_violation,
        projection.maximum_endpoint_constraint_violation);
    summary.maximum_objective_reconciliation_error = std::max(
        summary.maximum_objective_reconciliation_error,
        projection.maximum_endpoint_objective_reconciliation_error);
    summary.maximum_reduced_cost_optimality_residual = std::max(
        summary.maximum_reduced_cost_optimality_residual,
        projection.maximum_endpoint_optimality_residual);
}

void update_tail_audits(
    const ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    CapitalStackProbabilityPolytopeSummary& summary) {
    summary.maximum_probability_constraint_violation = std::max(
        summary.maximum_probability_constraint_violation,
        projection.maximum_endpoint_constraint_violation);
    summary.maximum_tail_mass_violation = std::max(
        summary.maximum_tail_mass_violation,
        projection.maximum_endpoint_tail_mass_violation);
    const double tail_objective_error = std::max(
        projection.maximum_endpoint_objective_reconciliation_error,
        projection.central_objective_reconciliation_error);
    summary.maximum_tail_objective_reconciliation_error = std::max(
        summary.maximum_tail_objective_reconciliation_error,
        tail_objective_error);
    summary.maximum_objective_reconciliation_error = std::max(
        summary.maximum_objective_reconciliation_error,
        tail_objective_error);
    summary.maximum_tail_threshold_formula_reconciliation_error = std::max(
        summary.maximum_tail_threshold_formula_reconciliation_error,
        projection
            .maximum_endpoint_threshold_formula_reconciliation_error);
    summary.maximum_reduced_cost_optimality_residual = std::max(
        summary.maximum_reduced_cost_optimality_residual,
        projection.maximum_endpoint_optimality_residual);
    summary.maximum_tail_threshold_enumeration_optimality_residual = std::max(
        summary.maximum_tail_threshold_enumeration_optimality_residual,
        projection.maximum_threshold_enumeration_optimality_residual);
}

[[nodiscard]] const CapitalStackScenarioResult& find_scenario(
    const std::unordered_map<std::string, const CapitalStackScenarioResult*>&
        by_id,
    const std::string& scenario_id) {
    const auto matching = by_id.find(scenario_id);
    if (matching == by_id.end()) {
        throw std::logic_error(
            "capital-stack probability-polytope witness lost a scenario");
    }
    return *matching->second;
}

struct WalWitnessComponents {
    double summary_numerator_million_years{0.0};
    double summary_denominator_million{0.0};
    double monthly_numerator_million_years{0.0};
    double monthly_denominator_million{0.0};
};

[[nodiscard]] WalWitnessComponents wal_witness_components(
    const std::vector<double>& weights,
    const std::vector<ProbabilityPolytopeScenario>& scenario_probabilities,
    const std::unordered_map<std::string, const CapitalStackScenarioResult*>&
        scenarios,
    std::size_t tranche_index) {
    if (weights.size() != scenario_probabilities.size()) {
        throw std::logic_error(
            "capital-stack probability-polytope WAL witness has the wrong dimension");
    }
    CompensatedSum summary_numerator;
    CompensatedSum summary_denominator;
    CompensatedSum monthly_numerator;
    CompensatedSum monthly_denominator;
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        const CapitalStackScenarioResult& scenario = find_scenario(
            scenarios, scenario_probabilities[index].scenario_id);
        if (tranche_index >= scenario.tranches.size()) {
            throw std::logic_error(
                "capital-stack probability-polytope WAL lost a tranche");
        }
        const CapitalStackTrancheScenarioResult& tranche =
            scenario.tranches[tranche_index];
        const long double weight = static_cast<long double>(weights[index]);
        summary_numerator.add(weight * static_cast<long double>(
            tranche.principal_cash_time_million_years));
        summary_denominator.add(weight * static_cast<long double>(
            tranche.principal_cash_distribution_million));
        for (const CapitalStackMonthlyTrancheCashFlow& cash :
             tranche.monthly_cash_flows) {
            const long double principal = static_cast<long double>(
                cash.principal_cash_distribution_million);
            monthly_denominator.add(weight * principal);
            monthly_numerator.add(weight * principal *
                static_cast<long double>(cash.month) / 12.0L);
        }
    }
    return WalWitnessComponents{
        checked_double(summary_numerator.value(),
            "capital-stack WAL summary numerator"),
        checked_double(summary_denominator.value(),
            "capital-stack WAL summary denominator"),
        checked_double(monthly_numerator.value(),
            "capital-stack WAL monthly numerator"),
        checked_double(monthly_denominator.value(),
            "capital-stack WAL monthly denominator")};
}

[[nodiscard]] std::vector<double> central_weights(
    const std::vector<ProbabilityPolytopeScenario>& scenarios) {
    std::vector<double> result;
    result.reserve(scenarios.size());
    for (const ProbabilityPolytopeScenario& scenario : scenarios) {
        result.push_back(scenario.central_weight);
    }
    return result;
}

[[nodiscard]] double wal_tolerance(double scale) noexcept {
    return kWalAbsoluteObjectiveTolerance +
        128.0 * std::numeric_limits<double>::epsilon() *
            std::max(1.0, std::abs(scale));
}

struct WalSolveResult {
    std::optional<CapitalStackProbabilityPolytopeWalRange> range{};
};

[[nodiscard]] WalSolveResult solve_principal_wal(
    const ProbabilityPolytopeProjector& projector,
    const std::vector<CapitalStackScenarioResult>& scenario_results,
    const std::vector<ProbabilityPolytopeScenario>& scenario_probabilities,
    std::size_t tranche_index, double maximum_years,
    CapitalStackProbabilityPolytopeSummary& summary) {
    const auto denominator = project_metric(projector, scenario_results,
        [tranche_index](const CapitalStackScenarioResult& scenario) {
            return scenario.tranches[tranche_index]
                .principal_cash_distribution_million;
        });
    update_linear_audits(denominator, summary);
    if (denominator.expectation.minimum.value <=
        kWalMinimumExpectedPrincipalCashMillion) {
        return {};
    }
    const auto numerator = project_metric(projector, scenario_results,
        [tranche_index](const CapitalStackScenarioResult& scenario) {
            return scenario.tranches[tranche_index]
                .principal_cash_time_million_years;
        });
    update_linear_audits(numerator, summary);

    std::unordered_map<std::string, const CapitalStackScenarioResult*>
        scenarios_by_id;
    scenarios_by_id.reserve(scenario_results.size());
    for (const CapitalStackScenarioResult& scenario : scenario_results) {
        scenarios_by_id.emplace(scenario.scenario_id, &scenario);
    }

    const auto objective = [&](double ratio) {
        const auto projection = project_metric(projector, scenario_results,
            [tranche_index, ratio](
                const CapitalStackScenarioResult& scenario) {
                const CapitalStackTrancheScenarioResult& tranche =
                    scenario.tranches[tranche_index];
                return checked_double(
                    static_cast<long double>(
                        tranche.principal_cash_time_million_years) -
                        static_cast<long double>(ratio) *
                            static_cast<long double>(
                                tranche.principal_cash_distribution_million),
                    "capital-stack probability-polytope WAL root objective");
            });
        update_linear_audits(projection, summary);
        return projection;
    };

    const auto make_endpoint = [&](bool maximize) {
        double ratio = maximize ? 0.0 : maximum_years;
        ProbabilityPolytopeMetricProjection final_projection;
        const ProbabilityPolytopeEndpoint* final_endpoint = nullptr;
        WalWitnessComponents final_components;
        double next_ratio = ratio;
        bool converged = false;
        for (std::size_t iteration = 0U;
             iteration < kMaximumWalIterations; ++iteration) {
            final_projection = objective(ratio);
            final_endpoint = maximize
                ? &final_projection.expectation.maximum
                : &final_projection.expectation.minimum;
            final_components = wal_witness_components(
                final_endpoint->scenario_weights, scenario_probabilities,
                scenarios_by_id, tranche_index);
            if (final_components.summary_denominator_million <= 0.0) {
                throw std::logic_error(
                    "capital-stack probability-polytope WAL witness has no principal cash");
            }
            next_ratio = final_components.summary_numerator_million_years /
                final_components.summary_denominator_million;
            if (!std::isfinite(next_ratio) || next_ratio < -1.0e-12 ||
                next_ratio > maximum_years + 1.0e-12) {
                throw std::logic_error(
                    "capital-stack probability-polytope WAL is outside the analysis horizon");
            }
            const double objective_scale =
                denominator.expectation.maximum.value * maximum_years;
            if (std::abs(final_endpoint->value) <=
                    wal_tolerance(objective_scale) &&
                std::abs(next_ratio - ratio) <=
                    kWalAbsoluteRatioTolerance +
                        128.0 * std::numeric_limits<double>::epsilon() *
                            std::max({1.0, std::abs(next_ratio),
                                std::abs(ratio)})) {
                converged = true;
                break;
            }
            ratio = std::clamp(next_ratio, 0.0, maximum_years);
        }
        if (!converged || final_endpoint == nullptr) {
            throw std::runtime_error(
                "capital-stack probability-polytope WAL root solver did not converge");
        }

        CapitalStackProbabilityPolytopeWalEndpoint result;
        result.value_years = std::clamp(
            next_ratio, 0.0, maximum_years);
        result.scenario_weights = final_endpoint->scenario_weights;
        result.numerator_million_years =
            final_components.summary_numerator_million_years;
        result.denominator_million =
            final_components.summary_denominator_million;
        result.root_ratio_input_years = ratio;
        result.root_objective_value_million_years = final_endpoint->value;
        result.numerator_reconciliation_error_million_years = std::abs(
            final_components.summary_numerator_million_years -
            final_components.monthly_numerator_million_years);
        result.denominator_reconciliation_error_million = std::abs(
            final_components.summary_denominator_million -
            final_components.monthly_denominator_million);
        const double monthly_ratio =
            final_components.monthly_numerator_million_years /
            final_components.monthly_denominator_million;
        result.ratio_reconciliation_error_years =
            std::abs(result.value_years - monthly_ratio);
        const double direct_root = checked_double(
            static_cast<long double>(
                final_components.summary_numerator_million_years) -
                static_cast<long double>(ratio) *
                    static_cast<long double>(
                        final_components.summary_denominator_million),
            "capital-stack probability-polytope WAL direct root objective");
        result.root_objective_reconciliation_error_million_years =
            std::abs(result.root_objective_value_million_years - direct_root);
        result.root_objective_absolute_residual_million_years =
            std::abs(result.root_objective_value_million_years);
        result.maximum_constraint_violation =
            final_endpoint->maximum_constraint_violation;
        result.objective_reconciliation_error =
            final_endpoint->objective_reconciliation_error;
        result.optimality_residual = final_endpoint->optimality_residual;
        return result;
    };

    CapitalStackProbabilityPolytopeWalRange range;
    range.minimum = make_endpoint(false);
    range.maximum = make_endpoint(true);
    range.central_numerator_million_years = numerator.expectation.central;
    range.central_denominator_million = denominator.expectation.central;
    range.central_years = range.central_numerator_million_years /
        range.central_denominator_million;
    const WalWitnessComponents central_components = wal_witness_components(
        central_weights(scenario_probabilities), scenario_probabilities,
        scenarios_by_id, tranche_index);
    range.central_ratio_reconciliation_error_years = std::abs(
        range.central_years -
        central_components.monthly_numerator_million_years /
            central_components.monthly_denominator_million);
    if (range.minimum.value_years > range.central_years + 1.0e-9 ||
        range.central_years > range.maximum.value_years + 1.0e-9) {
        throw std::logic_error(
            "capital-stack probability-polytope central WAL lies outside its endpoint range");
    }

    const auto update_wal_endpoint = [&](const auto& endpoint) {
        summary.maximum_wal_numerator_reconciliation_error_million_years =
            std::max(
                summary
                    .maximum_wal_numerator_reconciliation_error_million_years,
                endpoint.numerator_reconciliation_error_million_years);
        summary.maximum_wal_denominator_reconciliation_error_million =
            std::max(
                summary.maximum_wal_denominator_reconciliation_error_million,
                endpoint.denominator_reconciliation_error_million);
        summary.maximum_wal_ratio_reconciliation_error_years = std::max(
            summary.maximum_wal_ratio_reconciliation_error_years,
            endpoint.ratio_reconciliation_error_years);
        summary
            .maximum_wal_root_objective_reconciliation_error_million_years =
            std::max(
                summary
                    .maximum_wal_root_objective_reconciliation_error_million_years,
                endpoint
                    .root_objective_reconciliation_error_million_years);
        summary
            .maximum_wal_root_objective_absolute_residual_million_years =
            std::max(
                summary
                    .maximum_wal_root_objective_absolute_residual_million_years,
                endpoint.root_objective_absolute_residual_million_years);
    };
    update_wal_endpoint(range.minimum);
    update_wal_endpoint(range.maximum);
    summary.maximum_wal_numerator_reconciliation_error_million_years =
        std::max(
            summary.maximum_wal_numerator_reconciliation_error_million_years,
            std::abs(central_components.summary_numerator_million_years -
                central_components.monthly_numerator_million_years));
    summary.maximum_wal_denominator_reconciliation_error_million = std::max(
        summary.maximum_wal_denominator_reconciliation_error_million,
        std::abs(central_components.summary_denominator_million -
            central_components.monthly_denominator_million));
    summary.maximum_wal_ratio_reconciliation_error_years = std::max(
        summary.maximum_wal_ratio_reconciliation_error_years,
        range.central_ratio_reconciliation_error_years);

    const double wal_scale = std::max(1.0,
        denominator.expectation.maximum.value * maximum_years);
    if (summary.maximum_wal_numerator_reconciliation_error_million_years >
            wal_tolerance(wal_scale) ||
        summary.maximum_wal_denominator_reconciliation_error_million >
            wal_tolerance(denominator.expectation.maximum.value) ||
        summary.maximum_wal_ratio_reconciliation_error_years >
            wal_tolerance(maximum_years) ||
        summary
                .maximum_wal_root_objective_reconciliation_error_million_years >
            wal_tolerance(wal_scale) ||
        summary
                .maximum_wal_root_objective_absolute_residual_million_years >
            wal_tolerance(wal_scale)) {
        throw std::logic_error(
            "capital-stack probability-polytope WAL audit exceeded tolerance");
    }

    WalSolveResult result;
    result.range = std::move(range);
    return result;
}

void copy_path_controls(const CapitalStackSummary& source,
    CapitalStackProbabilityPolytopeSummary& destination) {
    destination.gross_project_principal_loss_is_changed =
        source.gross_project_principal_loss_is_changed;
    destination.project_cash_is_changed_by_tranching =
        source.project_cash_is_changed_by_tranching;
    destination.fair_value_or_market_price_is_estimated =
        source.fair_value_or_market_price_is_estimated;
    destination.legal_enforceability_is_validated =
        source.legal_enforceability_is_validated;
    destination.ratings_or_regulatory_capital_are_validated =
        source.ratings_or_regulatory_capital_are_validated;

    destination.maximum_commitment_identity_error_million =
        source.maximum_commitment_identity_error_million;
    destination.maximum_reserve_roll_forward_error_million =
        source.maximum_reserve_roll_forward_error_million;
    destination.maximum_reserve_shortfall_million =
        source.maximum_reserve_shortfall_million;
    destination.maximum_subscription_reconciliation_error_million =
        source.maximum_subscription_reconciliation_error_million;
    destination.maximum_pool_cost_call_reconciliation_error_million =
        source.maximum_pool_cost_call_reconciliation_error_million;
    destination.maximum_principal_distribution_reconciliation_error_million =
        source.maximum_principal_distribution_reconciliation_error_million;
    destination
        .maximum_nonprincipal_distribution_reconciliation_error_million =
        source.maximum_nonprincipal_distribution_reconciliation_error_million;
    destination.maximum_priority_nonprincipal_cap_violation_million =
        source.maximum_priority_nonprincipal_cap_violation_million;
    destination.maximum_realized_loss_reconciliation_error_million =
        source.maximum_realized_loss_reconciliation_error_million;
    destination.maximum_unresolved_exposure_reconciliation_error_million =
        source.maximum_unresolved_exposure_reconciliation_error_million;
    destination.maximum_nominal_net_cash_reconciliation_error_million =
        source.maximum_nominal_net_cash_reconciliation_error_million;
    destination.maximum_stack_npv_reconciliation_error_million =
        source.maximum_stack_npv_reconciliation_error_million;
}

} // namespace

void validate_capital_stack_probability_polytope(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack) {
    const PortfolioConfig selected = apply_success_participation_fraction(
        portfolio, participation,
        stack.underlying_success_participation_fraction);
    validate_probability_polytope_config(selected, probability_polytope);
    const PortfolioAmbiguityConfig locked =
        make_private_ledger_ambiguity(selected);
    validate_capital_stack_config(
        portfolio, locked, participation, stack);
}

CapitalStackProbabilityPolytopeSummary
evaluate_capital_stack_probability_polytope(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack) {
    const PortfolioConfig selected = apply_success_participation_fraction(
        portfolio, participation,
        stack.underlying_success_participation_fraction);
    const ProbabilityPolytopeProjector projector(
        selected, probability_polytope);
    const PortfolioAmbiguityConfig locked =
        make_private_ledger_ambiguity(selected);
    const CapitalStackSummary deterministic = evaluate_capital_stack(
        portfolio, locked, participation, stack);

    CapitalStackProbabilityPolytopeSummary summary;
    summary.underlying_success_participation_fraction =
        stack.underlying_success_participation_fraction;
    summary.underlying_target_worst_expected_npv_million =
        participation.target_worst_expected_npv_million;
    summary.aggregate_commitment_million =
        deterministic.aggregate_commitment_million;
    summary.scenarios = deterministic.scenarios;
    summary.model_limitation =
        "Synthetic fixed-path physical-probability analysis using an audited "
        "floating-point event-polytope solver. No symbolic or independent "
        "dual certificate, empirical calibration, fair value, market spread, "
        "rating, legal enforceability, tax, reserve-custody analysis, or "
        "regulatory-capital conclusion is established.";
    copy_path_controls(deterministic, summary);

    const auto underlying_npv = project_metric(projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.underlying_on_demand_npv_million;
        });
    update_linear_audits(underlying_npv, summary);
    summary.scenario_probabilities = underlying_npv.scenario_probabilities;
    summary.events = underlying_npv.events;
    summary.expected_underlying_on_demand_npv_million =
        underlying_npv.expectation;
    summary.selected_underlying_success_participation_meets_target =
        underlying_npv.expectation.minimum.value >=
        summary.underlying_target_worst_expected_npv_million;
    summary.selected_underlying_target_gap_million = std::max(0.0,
        summary.underlying_target_worst_expected_npv_million -
            underlying_npv.expectation.minimum.value);

    const auto full_stack_npv = project_metric(projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.fully_funded_stack_npv_at_pool_hurdle_million;
        });
    update_linear_audits(full_stack_npv, summary);
    summary.expected_fully_funded_stack_npv_at_pool_hurdle_million =
        full_stack_npv.expectation;
    const auto prefunding_drag = project_metric(projector, summary.scenarios,
        [](const CapitalStackScenarioResult& scenario) {
            return scenario.prefunding_drag_npv_million;
        });
    update_linear_audits(prefunding_drag, summary);
    summary.expected_prefunding_drag_npv_million =
        prefunding_drag.expectation;

    summary.tranches.reserve(stack.tranches.size());
    for (std::size_t index = 0U; index < stack.tranches.size(); ++index) {
        const CapitalStackTrancheConfig& term = stack.tranches[index];
        CapitalStackProbabilityPolytopeTrancheSummary tranche;
        tranche.tranche_id = term.id;
        tranche.attachment_million = term.attachment_million;
        tranche.detachment_million = term.detachment_million;
        tranche.notional_million =
            term.detachment_million - term.attachment_million;
        tranche.priority_nonprincipal_cap_million =
            term.priority_nonprincipal_cap_million;
        tranche.annual_physical_hurdle_rate =
            term.annual_physical_hurdle_rate;
        tranche.is_first_loss_residual = term.is_first_loss_residual;

        const auto project = [&](auto selector) {
            return project_metric(projector, summary.scenarios,
                [index, selector](const CapitalStackScenarioResult& scenario) {
                    return selector(scenario.tranches[index]);
                });
        };
        const auto assign = [&](ProbabilityPolytopeMetricRange& destination,
                                const auto& projection) {
            destination = projection.expectation;
            update_linear_audits(projection, summary);
        };

        assign(tranche.expected_contributions_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.total_contributions_million;
            }));
        assign(
            tranche
                .expected_underlying_principal_cash_distribution_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.underlying_principal_cash_distribution_million;
            }));
        assign(tranche.expected_unused_reserve_principal_return_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.unused_reserve_principal_return_million;
            }));
        assign(tranche.expected_principal_cash_distribution_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.principal_cash_distribution_million;
            }));
        assign(tranche.expected_nonprincipal_cash_distribution_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.nonprincipal_cash_distribution_million;
            }));
        assign(tranche.expected_total_distributions_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.total_distributions_million;
            }));
        assign(tranche.expected_realized_principal_loss_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.realized_principal_loss_million;
            }));
        assign(tranche.expected_realized_principal_loss_fraction,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.realized_principal_loss_million /
                    value.notional_million;
            }));
        assign(tranche.expected_unresolved_principal_exposure_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.unresolved_principal_exposure_million;
            }));
        assign(tranche.expected_principal_cash_shortfall_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.principal_cash_shortfall_million;
            }));
        assign(tranche.expected_npv_at_tranche_hurdle_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.npv_at_tranche_hurdle_million;
            }));
        assign(tranche.expected_all_in_cash_shortfall_million,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.all_in_cash_shortfall_million;
            }));
        assign(tranche.expected_scenario_cash_multiple,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.cash_multiple;
            }));
        assign(tranche.expected_scenario_net_return_fraction,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.net_return_fraction;
            }));
        assign(tranche.principal_impairment_probability,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.realized_principal_loss_million > 0.0 ? 1.0 : 0.0;
            }));
        assign(tranche.principal_exhaustion_probability,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.realized_principal_loss_million >=
                        value.notional_million
                    ? 1.0
                    : 0.0;
            }));
        assign(tranche.negative_npv_probability,
            project([](const CapitalStackTrancheScenarioResult& value) {
                return value.npv_at_tranche_hurdle_million < 0.0 ? 1.0 : 0.0;
            }));

        tranche.principal_loss_expected_shortfall_95_million = project_es(
            projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return scenario.tranches[index]
                    .realized_principal_loss_million;
            },
            0.05);
        update_tail_audits(
            tranche.principal_loss_expected_shortfall_95_million, summary);
        tranche.principal_loss_expected_shortfall_99_million = project_es(
            projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return scenario.tranches[index]
                    .realized_principal_loss_million;
            },
            0.01);
        update_tail_audits(
            tranche.principal_loss_expected_shortfall_99_million, summary);
        tranche.npv_shortfall_expected_shortfall_95_million = project_es(
            projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return std::max(0.0,
                    -scenario.tranches[index]
                         .npv_at_tranche_hurdle_million);
            },
            0.05);
        update_tail_audits(
            tranche.npv_shortfall_expected_shortfall_95_million, summary);
        tranche.npv_shortfall_expected_shortfall_99_million = project_es(
            projector, summary.scenarios,
            [index](const CapitalStackScenarioResult& scenario) {
                return std::max(0.0,
                    -scenario.tranches[index]
                         .npv_at_tranche_hurdle_million);
            },
            0.01);
        update_tail_audits(
            tranche.npv_shortfall_expected_shortfall_99_million, summary);

        const WalSolveResult wal = solve_principal_wal(projector,
            summary.scenarios, summary.scenario_probabilities, index,
            static_cast<double>(selected.horizon_months) / 12.0, summary);
        tranche.principal_cash_weighted_average_life_years = wal.range;
        tranche.central_expected_npv_meets_hurdle =
            tranche.expected_npv_at_tranche_hurdle_million.central >= 0.0;
        tranche.robust_expected_npv_meets_hurdle =
            tranche.expected_npv_at_tranche_hurdle_million.minimum.value >=
            0.0;
        summary.tranches.push_back(std::move(tranche));
    }

    return summary;
}

} // namespace naturalehia::cellular_finance
