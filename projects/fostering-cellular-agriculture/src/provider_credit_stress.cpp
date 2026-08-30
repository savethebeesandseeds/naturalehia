// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/provider_credit_stress.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kProbabilityTolerance = 1.0e-12;
constexpr double kReconciliationTolerance = 1.0e-9;
constexpr std::size_t kRatioBisectionIterations = 128U;

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

struct WeightedValue {
    double value{0.0};
    double weight{0.0};
};

struct CollapsedScenarioMetrics {
    std::string scenario_id{};
    double provider_default_probability{0.0};
    double positive_claim_and_default_probability{0.0};
    double contractual_claim_at_default_million{0.0};
    double direct_provider_payment_million{0.0};
    double collateral_applied_million{0.0};
    double delayed_unsecured_recovery_million{0.0};
    double ultimate_unpaid_claim_million{0.0};
    double unsecured_exposure_at_default_million{0.0};
    double full_claim_present_value_million{0.0};
    double actual_support_present_value_million{0.0};
    double investor_credit_loss_present_value_million{0.0};
    double investor_npv_before_premium_million{0.0};
    double investor_npv_after_price_million{0.0};
};

[[nodiscard]] double checked_double(
    long double value, std::string_view description) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            std::string(description) + " exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] bool close_enough(double first, double second,
    double tolerance = kReconciliationTolerance) noexcept {
    if (!std::isfinite(first) || !std::isfinite(second)) {
        return false;
    }
    const double scale = std::max({1.0, std::abs(first), std::abs(second)});
    return std::abs(first - second) <= tolerance * scale;
}

[[nodiscard]] double safe_accumulation_factor(double annual_rate,
    std::size_t month, std::string_view description) {
    const double result = std::pow(1.0 + annual_rate,
        static_cast<double>(month) / 12.0);
    if (!std::isfinite(result) || result <= 0.0) {
        throw std::overflow_error(
            std::string(description) + " accumulation factor is invalid");
    }
    return result;
}

[[nodiscard]] std::size_t checked_recovery_month(
    std::size_t settlement_month, std::size_t delay_months) {
    if (delay_months >
        std::numeric_limits<std::size_t>::max() - settlement_month) {
        throw std::overflow_error(
            "provider credit recovery month exceeded numeric range");
    }
    return settlement_month + delay_months;
}

[[nodiscard]] const JointScenarioResult& find_scenario(
    const PortfolioSummary& summary, std::string_view scenario_id) {
    const auto matching = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [scenario_id](const auto& scenario) {
            return scenario.scenario_id == scenario_id;
        });
    if (matching == summary.scenarios.end()) {
        throw std::logic_error(
            "provider credit stress lost a validated portfolio scenario");
    }
    return *matching;
}

[[nodiscard]] std::unordered_map<std::string,
    const ProviderCreditScenarioConfig*>
validate_and_index_credit_scenarios(const PortfolioConfig& portfolio,
    const ProviderCreditStressConfig& credit,
    double& maximum_weight_sum_error) {
    validate_provider_credit_stress_config(credit);
    if (credit.scenarios.size() != portfolio.joint_scenarios.size()) {
        throw std::invalid_argument(
            "provider credit stress must name every portfolio scenario exactly once");
    }

    std::unordered_map<std::string, const ProviderCreditScenarioConfig*>
        indexed;
    indexed.reserve(credit.scenarios.size());
    for (const ProviderCreditScenarioConfig& scenario : credit.scenarios) {
        if (!indexed.emplace(scenario.scenario_id, &scenario).second) {
            throw std::invalid_argument(
                "provider credit stress scenario ids must be unique");
        }
        CompensatedSum conditional_sum;
        for (const ProviderCreditOutcomeConfig& outcome : scenario.outcomes) {
            conditional_sum.add(
                static_cast<long double>(outcome.conditional_weight));
            if (!credit.price_ladder_collateral_is_pledged_to_investor &&
                outcome.collateral_realization_fraction != 0.0) {
                throw std::invalid_argument(
                    "unpledged price-ladder collateral cannot have a positive investor realization fraction");
            }
        }
        const double sum = checked_double(conditional_sum.value(),
            "provider credit conditional probability sum");
        maximum_weight_sum_error = std::max(
            maximum_weight_sum_error, std::abs(sum - 1.0));
        if (std::abs(sum - 1.0) > kProbabilityTolerance) {
            throw std::invalid_argument(
                "provider credit conditional outcome weights must sum to one");
        }
    }
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        if (indexed.find(scenario.id) == indexed.end()) {
            throw std::invalid_argument(
                "provider credit stress scenario ids do not match the portfolio");
        }
    }

    if (credit.price_ladder_collateral_is_pledged_to_investor !=
        credit.collateral_yield_remains_in_pledged_account) {
        throw std::invalid_argument(
            "provider credit pledged-collateral and retained-yield assertions must agree");
    }
    return indexed;
}

[[nodiscard]] double weighted_quantile(
    const std::vector<WeightedValue>& values, double probability) {
    std::vector<WeightedValue> ordered = values;
    std::sort(ordered.begin(), ordered.end(),
        [](const WeightedValue& first, const WeightedValue& second) {
            if (first.value != second.value) {
                return first.value < second.value;
            }
            return first.weight < second.weight;
        });
    CompensatedSum total_weight;
    for (const WeightedValue& item : ordered) {
        total_weight.add(static_cast<long double>(item.weight));
    }
    const long double target = static_cast<long double>(probability) *
        total_weight.value();
    CompensatedSum cumulative;
    for (const WeightedValue& item : ordered) {
        cumulative.add(static_cast<long double>(item.weight));
        if (cumulative.value() >= target) {
            return item.value;
        }
    }
    return ordered.back().value;
}

[[nodiscard]] long double upper_expected_shortfall(
    const std::vector<double>& values, const std::vector<double>& weights,
    double tail_probability) {
    if (values.empty() || values.size() != weights.size()) {
        throw std::logic_error(
            "provider credit central tail distribution is invalid");
    }
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(), [&values](std::size_t first,
                                           std::size_t second) {
        if (values[first] != values[second]) {
            return values[first] > values[second];
        }
        return first < second;
    });
    CompensatedSum total_weight;
    for (const double weight : weights) {
        total_weight.add(static_cast<long double>(weight));
    }
    const long double requested =
        static_cast<long double>(tail_probability) * total_weight.value();
    if (requested <= 0.0L) {
        throw std::logic_error(
            "provider credit central tail distribution has no mass");
    }
    long double remaining = requested;
    CompensatedSum total;
    for (const std::size_t index : order) {
        if (remaining <= 0.0L) {
            break;
        }
        const long double included = std::min(remaining,
            static_cast<long double>(weights[index]));
        total.add(included * static_cast<long double>(values[index]));
        remaining -= included;
    }
    if (remaining > static_cast<long double>(kProbabilityTolerance)) {
        throw std::logic_error(
            "provider credit central tail distribution has insufficient mass");
    }
    return total.value() / requested;
}

[[nodiscard]] PortfolioDistributionSummary summarize_distribution(
    const std::vector<double>& values, const std::vector<double>& weights) {
    if (values.empty() || values.size() != weights.size()) {
        throw std::logic_error(
            "provider credit central distribution is invalid");
    }
    std::vector<WeightedValue> weighted;
    weighted.reserve(values.size());
    CompensatedSum weight_sum;
    CompensatedSum shifted_value_sum;
    const long double value_origin =
        static_cast<long double>(values.front());
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (!std::isfinite(values[index]) ||
            !std::isfinite(weights[index]) || weights[index] < 0.0) {
            throw std::logic_error(
                "provider credit central distribution contains an invalid atom");
        }
        weighted.push_back(WeightedValue{values[index], weights[index]});
        weight_sum.add(static_cast<long double>(weights[index]));
        shifted_value_sum.add(static_cast<long double>(weights[index]) *
            (static_cast<long double>(values[index]) - value_origin));
    }
    if (weight_sum.value() <= 0.0L) {
        throw std::logic_error(
            "provider credit central distribution has no probability mass");
    }
    const long double mean_shift =
        shifted_value_sum.value() / weight_sum.value();
    const long double mean = value_origin + mean_shift;
    CompensatedSum variance;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        const long double difference =
            (static_cast<long double>(values[index]) - value_origin) -
            mean_shift;
        variance.add(static_cast<long double>(weights[index]) *
            difference * difference);
    }

    PortfolioDistributionSummary result;
    result.mean = checked_double(mean,
        "provider credit central distribution mean");
    result.standard_deviation = std::sqrt(std::max(0.0,
        checked_double(variance.value() / weight_sum.value(),
            "provider credit central distribution variance")));
    result.p50 = weighted_quantile(weighted, 0.50);
    result.p95 = weighted_quantile(weighted, 0.95);
    result.p99 = weighted_quantile(weighted, 0.99);
    result.maximum = *std::max_element(values.begin(), values.end());
    result.expected_shortfall_95 = checked_double(
        upper_expected_shortfall(values, weights, 0.05),
        "provider credit central expected shortfall 95");
    result.expected_shortfall_99 = checked_double(
        upper_expected_shortfall(values, weights, 0.01),
        "provider credit central expected shortfall 99");
    return result;
}

[[nodiscard]] std::vector<AmbiguityScenarioMetricValue> keyed_values(
    const std::vector<CollapsedScenarioMetrics>& scenarios,
    const std::vector<double>& values) {
    if (scenarios.size() != values.size()) {
        throw std::logic_error(
            "provider credit collapsed objective has the wrong scenario count");
    }
    std::vector<AmbiguityScenarioMetricValue> result;
    result.reserve(scenarios.size());
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        result.push_back(AmbiguityScenarioMetricValue{
            scenarios[index].scenario_id, values[index]});
    }
    return result;
}

[[nodiscard]] double endpoint_expectation_error(
    const AmbiguityMetricProjection& projection,
    const std::vector<AmbiguityScenarioMetricValue>& values) {
    std::unordered_map<std::string, double> by_id;
    by_id.reserve(values.size());
    for (const AmbiguityScenarioMetricValue& value : values) {
        by_id.emplace(value.scenario_id, value.value);
    }
    const auto error = [&](const AmbiguityEndpoint& endpoint) {
        if (endpoint.scenario_weights.size() !=
            projection.scenario_probability_bounds.size()) {
            throw std::logic_error(
                "provider credit ambiguity witness has the wrong size");
        }
        CompensatedSum sum;
        for (std::size_t index = 0U;
             index < projection.scenario_probability_bounds.size(); ++index) {
            const auto matching = by_id.find(
                projection.scenario_probability_bounds[index].scenario_id);
            if (matching == by_id.end()) {
                throw std::logic_error(
                    "provider credit ambiguity witness lost a scenario");
            }
            sum.add(static_cast<long double>(endpoint.scenario_weights[index]) *
                static_cast<long double>(matching->second));
        }
        return std::abs(checked_double(sum.value(),
                            "provider credit ambiguity witness expectation") -
            endpoint.value);
    };
    return std::max(error(projection.expectation.minimum),
        error(projection.expectation.maximum));
}

[[nodiscard]] bool same_probability_bounds(
    const std::vector<ScenarioProbabilityBounds>& first,
    const std::vector<ScenarioProbabilityBounds>& second) noexcept {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0U; index < first.size(); ++index) {
        if (first[index].scenario_id != second[index].scenario_id ||
            first[index].lower_weight != second[index].lower_weight ||
            first[index].central_weight != second[index].central_weight ||
            first[index].upper_weight != second[index].upper_weight) {
            return false;
        }
    }
    return true;
}

class ProjectionBook {
public:
    enum class MetricUnit : unsigned char {
        Probability,
        Million,
    };

    ProjectionBook(const PortfolioAmbiguityProjector& projector,
        const std::vector<CollapsedScenarioMetrics>& scenarios,
        ProviderCreditStressSummary& summary)
        : projector_(projector), scenarios_(scenarios), summary_(summary) {}

    [[nodiscard]] AmbiguityMetricRange project(
        const std::vector<double>& values, MetricUnit unit) {
        const std::vector<AmbiguityScenarioMetricValue> keyed =
            keyed_values(scenarios_, values);
        const AmbiguityMetricProjection projection =
            projector_.project_expectation(keyed);
        summary_.maximum_endpoint_probability_error = std::max(
            summary_.maximum_endpoint_probability_error,
            projection.maximum_endpoint_probability_error);
        const double witness_error =
            endpoint_expectation_error(projection, keyed);
        if (unit == MetricUnit::Probability) {
            summary_.maximum_probability_witness_reconciliation_error =
                std::max(
                    summary_.maximum_probability_witness_reconciliation_error,
                    witness_error);
        } else {
            summary_.maximum_monetary_witness_reconciliation_error_million =
                std::max(summary_
                             .maximum_monetary_witness_reconciliation_error_million,
                    witness_error);
        }
        if (summary_.scenario_probability_bounds.empty()) {
            summary_.scenario_probability_bounds =
                projection.scenario_probability_bounds;
        } else if (!same_probability_bounds(
                       summary_.scenario_probability_bounds,
                       projection.scenario_probability_bounds)) {
            throw std::logic_error(
                "provider credit ambiguity projections changed scenario bounds");
        }
        return projection.expectation;
    }

private:
    const PortfolioAmbiguityProjector& projector_;
    const std::vector<CollapsedScenarioMetrics>& scenarios_;
    ProviderCreditStressSummary& summary_;
};

template <typename Accessor>
[[nodiscard]] std::vector<double> collapsed_values(
    const std::vector<CollapsedScenarioMetrics>& scenarios,
    Accessor accessor) {
    std::vector<double> result;
    result.reserve(scenarios.size());
    for (const CollapsedScenarioMetrics& scenario : scenarios) {
        result.push_back(accessor(scenario));
    }
    return result;
}

[[nodiscard]] double weighted_value_for_endpoint(
    const AmbiguityEndpoint& endpoint,
    const std::vector<ScenarioProbabilityBounds>& bounds,
    const std::vector<CollapsedScenarioMetrics>& scenarios,
    const std::vector<double>& values) {
    if (endpoint.scenario_weights.size() != bounds.size() ||
        scenarios.size() != values.size()) {
        throw std::logic_error(
            "provider credit delivery-ratio witness has the wrong size");
    }
    std::unordered_map<std::string, double> by_id;
    by_id.reserve(scenarios.size());
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        by_id.emplace(scenarios[index].scenario_id, values[index]);
    }
    CompensatedSum sum;
    for (std::size_t index = 0U; index < bounds.size(); ++index) {
        const auto matching = by_id.find(bounds[index].scenario_id);
        if (matching == by_id.end()) {
            throw std::logic_error(
                "provider credit delivery-ratio witness lost a scenario");
        }
        sum.add(static_cast<long double>(endpoint.scenario_weights[index]) *
            static_cast<long double>(matching->second));
    }
    return checked_double(sum.value(),
        "provider credit delivery-ratio witness expectation");
}

void solve_delivery_ratio(ProjectionBook& book,
    const std::vector<CollapsedScenarioMetrics>& scenarios,
    const AmbiguityMetricRange& expected_full_claim,
    const AmbiguityMetricRange& expected_actual_support,
    ProviderCreditStressSummary& summary) {
    if (expected_full_claim.central > 0.0) {
        summary.robust.central_claim_present_value_delivery_ratio =
            expected_actual_support.central / expected_full_claim.central;
    }
    if (expected_full_claim.maximum.value <= 0.0) {
        return;
    }

    const std::vector<double> full = collapsed_values(scenarios,
        [](const CollapsedScenarioMetrics& scenario) {
            return scenario.full_claim_present_value_million;
        });
    const std::vector<double> actual = collapsed_values(scenarios,
        [](const CollapsedScenarioMetrics& scenario) {
            return scenario.actual_support_present_value_million;
        });

    double lower = 0.0;
    double upper = 1.0;
    std::optional<AmbiguityEndpoint> last_failing_witness;
    for (std::size_t iteration = 0U;
         iteration < kRatioBisectionIterations; ++iteration) {
        const double midpoint = std::midpoint(lower, upper);
        if (midpoint == lower || midpoint == upper) {
            break;
        }
        std::vector<double> objective;
        objective.reserve(scenarios.size());
        for (std::size_t index = 0U; index < scenarios.size(); ++index) {
            objective.push_back(checked_double(
                static_cast<long double>(actual[index]) -
                    static_cast<long double>(midpoint) *
                        static_cast<long double>(full[index]),
                "provider credit delivery-ratio objective"));
        }
        const AmbiguityMetricRange projected = book.project(
            objective, ProjectionBook::MetricUnit::Million);
        if (projected.minimum.value < 0.0) {
            upper = midpoint;
            last_failing_witness = projected.minimum;
        } else {
            lower = midpoint;
        }
    }

    AmbiguityEndpoint witness;
    double ratio = lower;
    if (last_failing_witness.has_value()) {
        witness = *last_failing_witness;
        const double witness_full = weighted_value_for_endpoint(witness,
            summary.scenario_probability_bounds, scenarios, full);
        const double witness_actual = weighted_value_for_endpoint(witness,
            summary.scenario_probability_bounds, scenarios, actual);
        if (witness_full <= 0.0) {
            throw std::logic_error(
                "provider credit minimum delivery-ratio witness has no claim PV");
        }
        ratio = witness_actual / witness_full;
    } else {
        // No failing point below one means every positive-denominator measure
        // delivers the full claim PV. Use a feasible maximum-claim witness so
        // the published ratio is not paired with a zero-denominator atom.
        witness = expected_full_claim.maximum;
        const double witness_full = weighted_value_for_endpoint(witness,
            summary.scenario_probability_bounds, scenarios, full);
        if (witness_full <= 0.0) {
            throw std::logic_error(
                "provider credit full-delivery witness has no claim PV");
        }
        const double witness_actual = weighted_value_for_endpoint(witness,
            summary.scenario_probability_bounds, scenarios, actual);
        ratio = witness_actual / witness_full;
    }
    if (!std::isfinite(ratio) || ratio < 0.0 || ratio > 1.0 + 1.0e-12) {
        throw std::logic_error(
            "provider credit robust delivery ratio is outside [0, 1]");
    }
    ratio = std::clamp(ratio, 0.0, 1.0);

    std::vector<double> final_objective;
    final_objective.reserve(scenarios.size());
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        final_objective.push_back(checked_double(
            static_cast<long double>(actual[index]) -
                static_cast<long double>(ratio) *
                    static_cast<long double>(full[index]),
            "provider credit final delivery-ratio objective"));
    }
    const AmbiguityMetricRange certification = book.project(
        final_objective, ProjectionBook::MetricUnit::Million);
    const double witness_residual = weighted_value_for_endpoint(witness,
        summary.scenario_probability_bounds, scenarios, final_objective);
    const double scale = std::max({1.0, expected_full_claim.maximum.value,
        expected_actual_support.maximum.value});
    if (certification.minimum.value < -kReconciliationTolerance * scale ||
        std::abs(witness_residual) > kReconciliationTolerance * scale) {
        throw std::logic_error(
            "provider credit robust delivery ratio did not retain a binding feasible witness");
    }
    witness.value = ratio;
    summary.robust.robust_minimum_claim_present_value_delivery_ratio =
        std::move(witness);
    summary.robust.robust_minimum_delivery_ratio_objective_residual_million =
        std::max(std::abs(certification.minimum.value),
            std::abs(witness_residual));
}

[[nodiscard]] double range_value_error(
    const AmbiguityMetricRange& first, const AmbiguityMetricRange& second) {
    return std::max({
        std::abs(first.minimum.value - second.minimum.value),
        std::abs(first.central - second.central),
        std::abs(first.maximum.value - second.maximum.value),
    });
}

[[nodiscard]] std::optional<double> conservative_premium_ceiling(
    double worst_npv, double target) {
    const double signed_headroom = worst_npv - target;
    if (signed_headroom < 0.0) {
        return std::nullopt;
    }
    double ceiling = signed_headroom;
    while (worst_npv + (-ceiling) < target) {
        const double next = std::nextafter(ceiling, 0.0);
        if (next == ceiling) {
            throw std::logic_error(
                "provider credit premium ceiling could not be certified");
        }
        ceiling = next;
    }
    return ceiling;
}

} // namespace

ProviderCreditStressSummary solve_provider_credit_stress(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection,
    const ProviderPriceLadderConfig& pricing,
    const ProviderCreditStressConfig& credit) {
    ProviderCreditStressSummary result;
    const auto credit_by_scenario = validate_and_index_credit_scenarios(
        portfolio, credit, result.maximum_conditional_weight_sum_error);
    if (credit.provider_id != protection.provider_id) {
        throw std::invalid_argument(
            "provider credit provider_id must exactly match the pooled-loss-protection provider_id");
    }
    result.provider_id = credit.provider_id;

    result.full_performance_price_ladder = solve_provider_price_ladder(
        portfolio, ambiguity, participation, protection, pricing);
    const ProviderPriceLadderSummary& price =
        result.full_performance_price_ladder;
    result.selected_coverage_fraction =
        price.selected_protection_point.coverage_fraction;
    result.settlement_month = protection.settlement_month;

    const PortfolioSummary gross_portfolio = evaluate_portfolio(portfolio);
    const PortfolioConfig underlying_config =
        apply_success_participation_fraction(portfolio, participation,
            protection.underlying_success_participation_fraction);
    const PortfolioSummary underlying_portfolio =
        evaluate_portfolio(underlying_config);
    const PortfolioAmbiguityProjector projector(portfolio, ambiguity);

    const double investor_settlement_accumulation = safe_accumulation_factor(
        portfolio.annual_physical_hurdle_rate,
        protection.settlement_month,
        "provider credit investor settlement");
    const double pledged_collateral_base =
        credit.price_ladder_collateral_is_pledged_to_investor
        ? price.costs.collateral_base_million
        : 0.0;
    const double collateral_accumulation = safe_accumulation_factor(
        pricing.collateral_annual_effective_yield_rate,
        protection.settlement_month,
        "provider credit pledged collateral");
    const double pledged_collateral_at_settlement = checked_double(
        static_cast<long double>(pledged_collateral_base) *
            static_cast<long double>(collateral_accumulation),
        "provider credit pledged collateral at settlement");
    const double unchanged_price = price.costs.provider_all_in_floor_million;

    result.exposure.contractual_maximum_exposure_million =
        price.costs.contractual_maximum_exposure_million;
    result.exposure.modeled_maximum_claim_million =
        price.costs.modeled_maximum_claim_million;
    result.exposure.pledged_collateral_base_million =
        pledged_collateral_base;
    result.exposure.pledged_collateral_at_settlement_million =
        pledged_collateral_at_settlement;

    std::vector<CollapsedScenarioMetrics> collapsed;
    collapsed.reserve(gross_portfolio.scenarios.size());
    std::vector<double> expanded_weights;
    std::vector<double> expanded_claims;
    std::vector<double> expanded_full_claim_pv;
    std::vector<double> expanded_actual_support_pv;
    std::vector<double> expanded_unsecured_exposure;
    std::vector<double> expanded_ultimate_unpaid;
    std::vector<double> expanded_credit_loss_pv;
    std::vector<double> expanded_default_indicator;
    std::vector<double> expanded_positive_claim_indicator;

    CompensatedSum expanded_weight_sum;
    for (const JointScenarioResult& gross_scenario :
         gross_portfolio.scenarios) {
        const JointScenarioResult& underlying_scenario = find_scenario(
            underlying_portfolio, gross_scenario.scenario_id);
        result.maximum_gross_project_loss_change_million = std::max(
            result.maximum_gross_project_loss_change_million,
            std::abs(gross_scenario.principal_loss_million -
                underlying_scenario.principal_loss_million));
        if (!close_enough(gross_scenario.principal_loss_million,
                underlying_scenario.principal_loss_million)) {
            throw std::logic_error(
                "provider credit underlying participation changed gross project loss");
        }
        const ProviderCreditScenarioConfig& credit_scenario =
            *credit_by_scenario.at(gross_scenario.scenario_id);

        ProviderCreditScenarioResult published;
        published.scenario_id = gross_scenario.scenario_id;
        published.central_portfolio_weight =
            gross_scenario.normalized_weight;
        published.underlying_investor_npv_million =
            underlying_scenario.npv_million;
        published.gross_project_principal_loss_million =
            gross_scenario.principal_loss_million;
        published.gross_contractual_claim_million = checked_double(
            static_cast<long double>(result.selected_coverage_fraction) *
                static_cast<long double>(
                    gross_scenario.principal_loss_million),
            "provider credit contractual claim");
        published.full_claim_present_value_million =
            published.gross_contractual_claim_million /
            investor_settlement_accumulation;

        CompensatedSum raw_conditional_sum;
        for (const ProviderCreditOutcomeConfig& outcome :
             credit_scenario.outcomes) {
            raw_conditional_sum.add(
                static_cast<long double>(outcome.conditional_weight));
        }
        const long double conditional_denominator =
            raw_conditional_sum.value();
        if (conditional_denominator <= 0.0L) {
            throw std::logic_error(
                "provider credit scenario has no conditional probability mass");
        }

        CompensatedSum collapsed_default;
        CompensatedSum collapsed_direct;
        CompensatedSum collapsed_collateral;
        CompensatedSum collapsed_recovery;
        CompensatedSum collapsed_unpaid;
        CompensatedSum collapsed_actual_pv;
        CompensatedSum collapsed_credit_loss_pv;
        CompensatedSum collapsed_unsecured;
        CompensatedSum collapsed_investor_npv;

        published.outcomes.reserve(credit_scenario.outcomes.size());
        for (const ProviderCreditOutcomeConfig& configured_outcome :
             credit_scenario.outcomes) {
            if (!configured_outcome.provider_performs) {
                result.provider_default_risk_is_modeled = true;
            }
            const double theta = checked_double(
                static_cast<long double>(
                    configured_outcome.conditional_weight) /
                    conditional_denominator,
                "provider credit normalized conditional weight");
            ProviderCreditOutcomeResult outcome;
            outcome.outcome_id = configured_outcome.outcome_id;
            outcome.conditional_weight = theta;
            outcome.expanded_central_weight = checked_double(
                static_cast<long double>(
                    gross_scenario.normalized_weight) *
                    static_cast<long double>(theta),
                "provider credit expanded central weight");
            outcome.provider_performs = configured_outcome.provider_performs;
            outcome.settlement_month = protection.settlement_month;
            outcome.gross_contractual_claim_million =
                published.gross_contractual_claim_million;
            outcome.full_claim_present_value_million =
                published.full_claim_present_value_million;
            outcome.pledged_collateral_available_at_settlement_million =
                pledged_collateral_at_settlement;
            outcome.collateral_realization_fraction =
                configured_outcome.collateral_realization_fraction;
            outcome.unsecured_recovery_fraction =
                configured_outcome.unsecured_recovery_fraction;

            if (configured_outcome.provider_performs) {
                outcome.unsecured_recovery_month =
                    protection.settlement_month;
                outcome.direct_provider_payment_at_settlement_million =
                    published.gross_contractual_claim_million;
            } else {
                outcome.unsecured_recovery_month = checked_recovery_month(
                    protection.settlement_month,
                    configured_outcome.unsecured_recovery_delay_months);
                const double realized_collateral = checked_double(
                    static_cast<long double>(
                        configured_outcome.collateral_realization_fraction) *
                        static_cast<long double>(
                            pledged_collateral_at_settlement),
                    "provider credit realized collateral");
                outcome.collateral_applied_at_settlement_million = std::min(
                    published.gross_contractual_claim_million,
                    realized_collateral);
                outcome.unsecured_exposure_at_default_million = checked_double(
                    static_cast<long double>(
                        published.gross_contractual_claim_million) -
                        static_cast<long double>(
                            outcome.collateral_applied_at_settlement_million),
                    "provider credit unsecured exposure");
                outcome.delayed_unsecured_recovery_million = checked_double(
                    static_cast<long double>(
                        configured_outcome.unsecured_recovery_fraction) *
                        static_cast<long double>(
                            outcome.unsecured_exposure_at_default_million),
                    "provider credit delayed unsecured recovery");
                outcome.ultimate_unpaid_claim_million = checked_double(
                    static_cast<long double>(
                        outcome.unsecured_exposure_at_default_million) -
                        static_cast<long double>(
                            outcome.delayed_unsecured_recovery_million),
                    "provider credit ultimate unpaid claim");
            }

            outcome.actual_support_received_nominal_million = checked_double(
                static_cast<long double>(
                    outcome.direct_provider_payment_at_settlement_million) +
                    static_cast<long double>(
                        outcome.collateral_applied_at_settlement_million) +
                    static_cast<long double>(
                        outcome.delayed_unsecured_recovery_million),
                "provider credit actual nominal support");
            outcome.collateral_received_present_value_million =
                outcome.collateral_applied_at_settlement_million /
                investor_settlement_accumulation;
            const double recovery_accumulation = safe_accumulation_factor(
                portfolio.annual_physical_hurdle_rate,
                outcome.unsecured_recovery_month,
                "provider credit delayed recovery");
            outcome.unsecured_recovery_present_value_million =
                outcome.delayed_unsecured_recovery_million /
                recovery_accumulation;
            const double direct_present_value =
                outcome.direct_provider_payment_at_settlement_million /
                investor_settlement_accumulation;
            outcome.actual_support_received_present_value_million =
                checked_double(static_cast<long double>(direct_present_value) +
                        static_cast<long double>(
                            outcome.collateral_received_present_value_million) +
                        static_cast<long double>(
                            outcome.unsecured_recovery_present_value_million),
                    "provider credit actual support present value");
            outcome.investor_credit_loss_present_value_million =
                checked_double(static_cast<long double>(
                                   outcome.full_claim_present_value_million) -
                        static_cast<long double>(
                            outcome.actual_support_received_present_value_million),
                    "provider credit investor credit loss present value");
            if (outcome.investor_credit_loss_present_value_million < 0.0 &&
                close_enough(
                    outcome.investor_credit_loss_present_value_million,
                    0.0)) {
                outcome.investor_credit_loss_present_value_million = 0.0;
            }
            if (outcome.investor_credit_loss_present_value_million < 0.0) {
                throw std::logic_error(
                    "provider credit delivery exceeded the full claim PV");
            }
            outcome.investor_npv_before_premium_million = checked_double(
                static_cast<long double>(
                    underlying_scenario.npv_million) +
                    static_cast<long double>(
                        outcome.actual_support_received_present_value_million),
                "provider credit investor NPV before premium");
            outcome.investor_npv_after_unchanged_full_performance_price_million =
                checked_double(static_cast<long double>(
                                   outcome.investor_npv_before_premium_million) -
                        static_cast<long double>(unchanged_price),
                    "provider credit investor NPV after unchanged price");

            const double waterfall_error = std::abs(
                published.gross_contractual_claim_million -
                (outcome.direct_provider_payment_at_settlement_million +
                    outcome.collateral_applied_at_settlement_million +
                    outcome.delayed_unsecured_recovery_million +
                    outcome.ultimate_unpaid_claim_million));
            result.maximum_default_waterfall_reconciliation_error_million =
                std::max(
                    result.maximum_default_waterfall_reconciliation_error_million,
                    waterfall_error);
            const double credit_loss_error = std::abs(
                outcome.full_claim_present_value_million -
                (outcome.actual_support_received_present_value_million +
                    outcome.investor_credit_loss_present_value_million));
            result.maximum_credit_loss_reconciliation_error_million =
                std::max(
                    result.maximum_credit_loss_reconciliation_error_million,
                    credit_loss_error);

            const long double conditional_weight =
                static_cast<long double>(theta);
            if (!configured_outcome.provider_performs) {
                collapsed_default.add(conditional_weight);
            }
            collapsed_direct.add(conditional_weight *
                static_cast<long double>(
                    outcome.direct_provider_payment_at_settlement_million));
            collapsed_collateral.add(conditional_weight *
                static_cast<long double>(
                    outcome.collateral_applied_at_settlement_million));
            collapsed_recovery.add(conditional_weight *
                static_cast<long double>(
                    outcome.delayed_unsecured_recovery_million));
            collapsed_unpaid.add(conditional_weight *
                static_cast<long double>(
                    outcome.ultimate_unpaid_claim_million));
            collapsed_actual_pv.add(conditional_weight *
                static_cast<long double>(
                    outcome.actual_support_received_present_value_million));
            collapsed_credit_loss_pv.add(conditional_weight *
                static_cast<long double>(
                    outcome.investor_credit_loss_present_value_million));
            collapsed_unsecured.add(conditional_weight *
                static_cast<long double>(
                    outcome.unsecured_exposure_at_default_million));
            collapsed_investor_npv.add(conditional_weight *
                static_cast<long double>(
                    outcome.investor_npv_before_premium_million));

            expanded_weight_sum.add(static_cast<long double>(
                outcome.expanded_central_weight));
            expanded_weights.push_back(outcome.expanded_central_weight);
            expanded_claims.push_back(
                outcome.gross_contractual_claim_million);
            expanded_full_claim_pv.push_back(
                outcome.full_claim_present_value_million);
            expanded_actual_support_pv.push_back(
                outcome.actual_support_received_present_value_million);
            expanded_unsecured_exposure.push_back(
                outcome.unsecured_exposure_at_default_million);
            expanded_ultimate_unpaid.push_back(
                outcome.ultimate_unpaid_claim_million);
            expanded_credit_loss_pv.push_back(
                outcome.investor_credit_loss_present_value_million);
            expanded_default_indicator.push_back(
                configured_outcome.provider_performs ? 0.0 : 1.0);
            expanded_positive_claim_indicator.push_back(
                published.gross_contractual_claim_million > 0.0 ? 1.0 : 0.0);

            result.exposure.modeled_maximum_unsecured_exposure_million =
                std::max(
                    result.exposure.modeled_maximum_unsecured_exposure_million,
                    outcome.unsecured_exposure_at_default_million);
            if (!configured_outcome.provider_performs) {
                const double contractual_collateral = std::min(
                    result.exposure.contractual_maximum_exposure_million,
                    configured_outcome.collateral_realization_fraction *
                        pledged_collateral_at_settlement);
                result.exposure
                    .contractual_maximum_unsecured_exposure_million =
                    std::max(result.exposure
                                 .contractual_maximum_unsecured_exposure_million,
                        result.exposure.contractual_maximum_exposure_million -
                            contractual_collateral);
            }
            published.outcomes.push_back(std::move(outcome));
        }

        published.conditional_provider_default_probability =
            checked_double(collapsed_default.value(),
                "provider credit conditional default probability");
        published.conditional_expected_direct_provider_payment_million =
            checked_double(collapsed_direct.value(),
                "provider credit conditional direct payment");
        published.conditional_expected_collateral_applied_million =
            checked_double(collapsed_collateral.value(),
                "provider credit conditional collateral");
        published.conditional_expected_delayed_unsecured_recovery_million =
            checked_double(collapsed_recovery.value(),
                "provider credit conditional delayed recovery");
        published.conditional_expected_ultimate_unpaid_claim_million =
            checked_double(collapsed_unpaid.value(),
                "provider credit conditional ultimate unpaid claim");
        published.conditional_expected_actual_support_present_value_million =
            checked_double(collapsed_actual_pv.value(),
                "provider credit conditional actual support PV");
        published.conditional_expected_credit_loss_present_value_million =
            checked_double(collapsed_credit_loss_pv.value(),
                "provider credit conditional credit loss PV");
        published.conditional_expected_unsecured_exposure_at_default_million =
            checked_double(collapsed_unsecured.value(),
                "provider credit conditional unsecured exposure");
        published.conditional_expected_investor_npv_before_premium_million =
            checked_double(collapsed_investor_npv.value(),
                "provider credit conditional investor NPV");

        CollapsedScenarioMetrics metrics;
        metrics.scenario_id = published.scenario_id;
        metrics.provider_default_probability =
            published.conditional_provider_default_probability;
        metrics.positive_claim_and_default_probability =
            published.gross_contractual_claim_million > 0.0
            ? published.conditional_provider_default_probability
            : 0.0;
        metrics.contractual_claim_at_default_million = checked_double(
            static_cast<long double>(
                published.gross_contractual_claim_million) *
                static_cast<long double>(
                    published.conditional_provider_default_probability),
            "provider credit conditional claim at default");
        metrics.direct_provider_payment_million =
            published.conditional_expected_direct_provider_payment_million;
        metrics.collateral_applied_million =
            published.conditional_expected_collateral_applied_million;
        metrics.delayed_unsecured_recovery_million =
            published
                .conditional_expected_delayed_unsecured_recovery_million;
        metrics.ultimate_unpaid_claim_million =
            published.conditional_expected_ultimate_unpaid_claim_million;
        metrics.unsecured_exposure_at_default_million =
            published
                .conditional_expected_unsecured_exposure_at_default_million;
        metrics.full_claim_present_value_million =
            published.full_claim_present_value_million;
        metrics.actual_support_present_value_million =
            published
                .conditional_expected_actual_support_present_value_million;
        metrics.investor_credit_loss_present_value_million =
            published.conditional_expected_credit_loss_present_value_million;
        metrics.investor_npv_before_premium_million =
            published.conditional_expected_investor_npv_before_premium_million;
        metrics.investor_npv_after_price_million = checked_double(
            static_cast<long double>(
                metrics.investor_npv_before_premium_million) -
                static_cast<long double>(unchanged_price),
            "provider credit conditional investor NPV after price");

        const double collapsed_credit_identity_error = std::abs(
            metrics.full_claim_present_value_million -
            (metrics.actual_support_present_value_million +
                metrics.investor_credit_loss_present_value_million));
        result.maximum_conditional_collapse_reconciliation_error_million =
            std::max(
                result.maximum_conditional_collapse_reconciliation_error_million,
                collapsed_credit_identity_error);
        collapsed.push_back(std::move(metrics));
        result.scenarios.push_back(std::move(published));
    }

    result.expanded_central_probability_sum_error = std::abs(
        checked_double(expanded_weight_sum.value(),
            "provider credit expanded central probability sum") -
        1.0);
    if (result.expanded_central_probability_sum_error >
        kProbabilityTolerance) {
        throw std::logic_error(
            "provider credit expanded central probabilities do not sum to one");
    }

    result.central.gross_contractual_claim_million =
        summarize_distribution(expanded_claims, expanded_weights);
    result.central.full_claim_present_value_million =
        summarize_distribution(expanded_full_claim_pv, expanded_weights);
    result.central.actual_support_received_present_value_million =
        summarize_distribution(expanded_actual_support_pv, expanded_weights);
    result.central.unsecured_exposure_at_default_million =
        summarize_distribution(expanded_unsecured_exposure, expanded_weights);
    result.central.ultimate_unpaid_claim_million =
        summarize_distribution(expanded_ultimate_unpaid, expanded_weights);
    result.central.investor_credit_loss_present_value_million =
        summarize_distribution(expanded_credit_loss_pv, expanded_weights);

    CompensatedSum default_probability;
    CompensatedSum positive_claim_probability;
    CompensatedSum joint_positive_claim_default_probability;
    CompensatedSum expected_claim_at_default;
    CompensatedSum expected_claim_deviation;
    const long double claim_origin =
        static_cast<long double>(expanded_claims.front());
    for (std::size_t index = 0U; index < expanded_weights.size(); ++index) {
        const long double weight =
            static_cast<long double>(expanded_weights[index]);
        default_probability.add(weight * static_cast<long double>(
            expanded_default_indicator[index]));
        positive_claim_probability.add(weight * static_cast<long double>(
            expanded_positive_claim_indicator[index]));
        joint_positive_claim_default_probability.add(weight *
            static_cast<long double>(expanded_default_indicator[index]) *
            static_cast<long double>(
                expanded_positive_claim_indicator[index]));
        expected_claim_at_default.add(weight *
            static_cast<long double>(expanded_claims[index]) *
            static_cast<long double>(expanded_default_indicator[index]));
        expected_claim_deviation.add(weight *
            (static_cast<long double>(expanded_claims[index]) -
                claim_origin));
    }
    const long double total_probability = expanded_weight_sum.value();
    if (total_probability <= 0.0L) {
        throw std::logic_error(
            "provider credit wrong-way diagnostics have no probability mass");
    }
    const long double mean_default_long =
        default_probability.value() / total_probability;
    const long double mean_claim_deviation =
        expected_claim_deviation.value() / total_probability;
    result.central.provider_default_probability = checked_double(
        mean_default_long,
        "provider credit central default probability");
    result.central.positive_claim_probability = checked_double(
        positive_claim_probability.value() / total_probability,
        "provider credit central positive claim probability");
    result.central.positive_claim_and_provider_default_probability =
        checked_double(joint_positive_claim_default_probability.value() /
                total_probability,
            "provider credit central joint claim and default probability");
    const double central_claim_at_default = checked_double(
        expected_claim_at_default.value() / total_probability,
        "provider credit central claim at default");
    if (result.central.positive_claim_probability > 0.0) {
        result.central.provider_default_probability_given_positive_claim =
            result.central
                .positive_claim_and_provider_default_probability /
            result.central.positive_claim_probability;
    }
    if (result.central.provider_default_probability > 0.0) {
        result.central.positive_claim_probability_given_provider_default =
            result.central
                .positive_claim_and_provider_default_probability /
            result.central.provider_default_probability;
        result.central.expected_contractual_claim_given_provider_default_million =
            central_claim_at_default /
            result.central.provider_default_probability;
        result.central.expected_unsecured_exposure_given_provider_default_million =
            result.central.unsecured_exposure_at_default_million.mean /
            result.central.provider_default_probability;
    }
    if (result.central.gross_contractual_claim_million.mean > 0.0) {
        result.central.claim_weighted_provider_default_rate =
            central_claim_at_default /
            result.central.gross_contractual_claim_million.mean;
        if (result.central
                .expected_contractual_claim_given_provider_default_million
                .has_value()) {
            result.central.claim_at_default_severity_multiplier =
                *result.central
                     .expected_contractual_claim_given_provider_default_million /
                result.central.gross_contractual_claim_million.mean;
        }
    }
    // Center first and use compensated arithmetic. Computing
    // E[X D] - E[X]E[D]
    // from rounded million-scale doubles is catastrophically unstable when
    // claim severity is almost constant.
    CompensatedSum centered_covariance;
    CompensatedSum centered_claim_variance;
    CompensatedSum centered_default_variance;
    for (std::size_t index = 0U; index < expanded_weights.size(); ++index) {
        const long double normalized_weight =
            static_cast<long double>(expanded_weights[index]) /
            total_probability;
        const long double claim_difference =
            (static_cast<long double>(expanded_claims[index]) -
                claim_origin) -
            mean_claim_deviation;
        const long double default_difference =
            static_cast<long double>(expanded_default_indicator[index]) -
            mean_default_long;
        centered_covariance.add(normalized_weight * claim_difference *
            default_difference);
        centered_claim_variance.add(normalized_weight * claim_difference *
            claim_difference);
        centered_default_variance.add(normalized_weight * default_difference *
            default_difference);
    }
    result.central.contractual_claim_provider_default_covariance_million =
        checked_double(centered_covariance.value(),
            "provider credit centered claim-default covariance");
    if (centered_claim_variance.value() > 0.0L &&
        centered_default_variance.value() > 0.0L) {
        long double correlation = centered_covariance.value() /
            std::sqrt(centered_claim_variance.value() *
                centered_default_variance.value());
        constexpr long double correlation_roundoff_tolerance =
            256.0L * static_cast<long double>(
                         std::numeric_limits<double>::epsilon());
        if (correlation < -1.0L - correlation_roundoff_tolerance ||
            correlation > 1.0L + correlation_roundoff_tolerance) {
            throw std::logic_error(
                "provider credit centered claim-default correlation exceeded [-1, 1]");
        }
        correlation = std::clamp(correlation, -1.0L, 1.0L);
        result.central.contractual_claim_provider_default_correlation =
            checked_double(correlation,
                "provider credit centered claim-default correlation");
    }

    ProjectionBook projections(projector, collapsed, result);
    result.robust.provider_default_probability = projections.project(
        collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.provider_default_probability;
            }),
        ProjectionBook::MetricUnit::Probability);
    result.robust.positive_claim_and_provider_default_probability =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.positive_claim_and_default_probability;
            }),
            ProjectionBook::MetricUnit::Probability);
    result.robust.expected_contractual_claim_at_default_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.contractual_claim_at_default_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_direct_provider_payment_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.direct_provider_payment_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_collateral_applied_million = projections.project(
        collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.collateral_applied_million;
            }),
        ProjectionBook::MetricUnit::Million);
    result.robust.expected_delayed_unsecured_recovery_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.delayed_unsecured_recovery_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_ultimate_unpaid_claim_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.ultimate_unpaid_claim_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_unsecured_exposure_at_default_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.unsecured_exposure_at_default_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_full_claim_present_value_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.full_claim_present_value_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_actual_support_received_present_value_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.actual_support_present_value_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.expected_investor_credit_loss_present_value_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.investor_credit_loss_present_value_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust.investor_expected_npv_before_premium_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.investor_npv_before_premium_million;
            }),
            ProjectionBook::MetricUnit::Million);
    result.robust
        .investor_expected_npv_after_unchanged_full_performance_price_million =
        projections.project(collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.investor_npv_after_price_million;
            }),
            ProjectionBook::MetricUnit::Million);

    result.maximum_central_probability_projection_reconciliation_error =
        std::max(
            std::abs(result.central.provider_default_probability -
                result.robust.provider_default_probability.central),
            std::abs(result.central
                         .positive_claim_and_provider_default_probability -
                result.robust
                    .positive_claim_and_provider_default_probability.central));
    result.maximum_central_monetary_projection_reconciliation_error_million =
        std::max({
            std::abs(result.central.full_claim_present_value_million.mean -
                result.robust.expected_full_claim_present_value_million.central),
            std::abs(
                result.central.actual_support_received_present_value_million
                    .mean -
                result.robust
                    .expected_actual_support_received_present_value_million
                    .central),
            std::abs(result.central
                         .investor_credit_loss_present_value_million.mean -
                result.robust
                    .expected_investor_credit_loss_present_value_million
                    .central),
            std::abs(result.central.unsecured_exposure_at_default_million.mean -
                result.robust
                    .expected_unsecured_exposure_at_default_million.central),
            std::abs(result.central.ultimate_unpaid_claim_million.mean -
                result.robust.expected_ultimate_unpaid_claim_million.central),
        });

    const std::vector<double> full_performance_investor_values =
        collapsed_values(collapsed,
            [](const CollapsedScenarioMetrics& scenario) {
                return scenario.investor_npv_before_premium_million +
                    scenario.investor_credit_loss_present_value_million;
            });
    const AmbiguityMetricRange full_performance_investor =
        projections.project(full_performance_investor_values,
            ProjectionBook::MetricUnit::Million);
    const double full_performance_reconciliation = range_value_error(
        full_performance_investor,
        price.selected_protection_point
            .investor_expected_npv_before_premium_million);
    result.maximum_conditional_collapse_reconciliation_error_million =
        std::max(
            result.maximum_conditional_collapse_reconciliation_error_million,
            full_performance_reconciliation);
    if (!close_enough(full_performance_reconciliation, 0.0)) {
        throw std::logic_error(
            "provider credit full-performance NPV does not reconcile to the selected priced protection point");
    }

    solve_delivery_ratio(projections, collapsed,
        result.robust.expected_full_claim_present_value_million,
        result.robust.expected_actual_support_received_present_value_million,
        result);

    result.support.unchanged_full_performance_provider_price_million =
        unchanged_price;
    result.support.full_performance_provider_price_change_million = 0.0;
    const double target = price.selected_protection_point
                              .investor_target_worst_expected_npv_million;
    const double stressed_worst = result.robust
                                      .investor_expected_npv_before_premium_million
                                      .minimum.value;
    result.support.stressed_investor_signed_premium_headroom_million =
        stressed_worst - target;
    result.support.stressed_investor_maximum_nonnegative_premium_million =
        conservative_premium_ceiling(stressed_worst, target);
    const double payable = result.support
                               .stressed_investor_maximum_nonnegative_premium_million
                               .value_or(0.0);
    result.support.stressed_provider_premium_support_required_million =
        std::max(0.0, unchanged_price - payable);
    result.support.stressed_investor_target_restoration_required_million =
        std::max(0.0, target - stressed_worst);
    result.support.stressed_all_in_support_gap_million = checked_double(
        static_cast<long double>(result.support
                                     .stressed_provider_premium_support_required_million) +
            static_cast<long double>(result.support
                                         .stressed_investor_target_restoration_required_million),
        "provider credit stressed all-in support gap");
    result.support.base_full_performance_all_in_support_gap_million =
        price.all_in_support_gap_million;
    const double incremental =
        result.support.stressed_all_in_support_gap_million -
        result.support.base_full_performance_all_in_support_gap_million;
    if (incremental < 0.0 && !close_enough(incremental, 0.0)) {
        throw std::logic_error(
            "provider credit stress reduced the full-performance support gap");
    }
    result.support.incremental_counterparty_credit_support_gap_million =
        std::max(0.0, incremental);
    result.support_gap_decomposition_reconciliation_error_million = std::abs(
        result.support.stressed_all_in_support_gap_million -
        (result.support.base_full_performance_all_in_support_gap_million +
            result.support.incremental_counterparty_credit_support_gap_million));

    result.provider_credit_model_limitation =
        "Physical-measure settlement stress only. Provider performance atoms "
        "are fixed conditional on existing portfolio scenarios. The engine "
        "does not estimate market CVA or fair value, infer default timing, "
        "validate collateral perfection or legal enforceability, model "
        "close-out netting or margin calls, or validate provider liquidity, "
        "capital adequacy, ratings, tax, or recovery evidence.";
    result.maximum_endpoint_probability_error = std::max(
        result.maximum_endpoint_probability_error,
        price.maximum_endpoint_probability_error);
    return result;
}

} // namespace naturalehia::cellular_finance
