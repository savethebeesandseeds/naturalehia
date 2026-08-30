// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kPortfolioAmbiguityModelVersion{"0.1.0"};

// Componentwise probability bounds over one already-declared joint scenario.
// The central weight is an explicit audit checksum: after normalization it must
// match the same scenario's weight in PortfolioConfig.
struct ScenarioProbabilityBounds {
    std::string scenario_id{};
    double lower_weight{0.0};
    double central_weight{0.0};
    double upper_weight{0.0};
};

struct PortfolioAmbiguityConfig {
    std::string model_version{kPortfolioAmbiguityModelVersion};
    std::string scenario_label{
        "unnamed synthetic probability-ambiguity analysis"};
    std::string source_note{
        "Unvalidated synthetic probability bounds for robustness testing"};
    bool synthetic_inputs{true};
    std::vector<ScenarioProbabilityBounds> scenario_probabilities{};
};

// scenario_weights uses the associated summary or projection's
// scenario_probability_bounds order, which is deterministic ascending
// scenario-id order.
struct AmbiguityEndpoint {
    double value{0.0};
    std::vector<double> scenario_weights{};
};

// These are scalar projection bounds. Each minimum and maximum contains its
// own feasible probability witness; endpoints of different metrics need not be
// attainable under one common probability measure.
struct AmbiguityMetricRange {
    AmbiguityEndpoint minimum{};
    double central{0.0};
    AmbiguityEndpoint maximum{};
};

// One scalar objective value keyed to an already-declared joint scenario.
// Values may be signed, but must be finite. Keying prevents a caller from
// silently projecting a vector in a different scenario order.
struct AmbiguityScenarioMetricValue {
    std::string scenario_id{};
    double value{0.0};
};

// A self-describing linear-expectation projection. Endpoint weights use the
// order of scenario_probability_bounds, which is ascending scenario-id order.
struct AmbiguityMetricProjection {
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    AmbiguityMetricRange expectation{};
    double maximum_endpoint_probability_error{0.0};
};

// A self-describing projection of the fractional upper-tail average. The
// tail_probability is the mass averaged (0.05 means the worst five percent),
// not a confidence level. Endpoint weights are complete feasible probability
// measures in ascending scenario-id order; they are not tail-allocation
// weights. For downside NPV, callers should supply max(0, -NPV), since the
// upper tail of raw NPV is an upside measure.
struct AmbiguityUpperExpectedShortfallProjection {
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    double tail_probability{0.0};
    AmbiguityMetricRange upper_expected_shortfall{};
    double maximum_endpoint_probability_error{0.0};
};

// Prepared, immutable capped-simplex projector. Construction performs the
// complete portfolio/envelope validation and central-measure checksum once;
// its projection methods may then be called repeatedly for different keyed
// scenario value vectors without rebuilding project cash paths.
class PortfolioAmbiguityProjector {
public:
    PortfolioAmbiguityProjector(const PortfolioConfig& portfolio,
        const PortfolioAmbiguityConfig& ambiguity);

    [[nodiscard]] AmbiguityMetricProjection project_expectation(
        const std::vector<AmbiguityScenarioMetricValue>& scenario_values)
        const;

    // Bounds the average of the largest tail_probability fraction of supplied
    // scenario values. This is a nonlinear tail functional, not a linear
    // expectation. Fractional scenario atoms at the tail boundary are handled
    // exactly under each endpoint probability witness.
    [[nodiscard]] AmbiguityUpperExpectedShortfallProjection
    project_upper_expected_shortfall(
        const std::vector<AmbiguityScenarioMetricValue>& scenario_values,
        double tail_probability) const;

private:
    struct State;
    std::shared_ptr<const State> state_{};
};

struct AmbiguityReturnSourceTotal {
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    AmbiguityMetricRange nominal_million{};
    AmbiguityMetricRange present_value_million{};
};

// Componentwise project ranges under the same probability ambiguity set as
// the pool. Every scalar minimum and maximum has its own complete probability
// witness; endpoints from different projects or metrics generally cannot be
// combined into one probability measure. Project NPV excludes shared pool
// costs because version 0.1 has no declared project-level cost-allocation rule.
struct ProjectAmbiguitySummary {
    std::string project_id{};
    AmbiguityMetricRange expected_total_draws_million{};
    AmbiguityMetricRange expected_total_receipts_million{};
    AmbiguityMetricRange expected_outstanding_principal_million{};
    AmbiguityMetricRange expected_realized_principal_loss_million{};
    AmbiguityMetricRange expected_npv_before_pool_costs_million{};
    AmbiguityMetricRange principal_impairment_probability{};
    AmbiguityMetricRange negative_npv_probability{};
};

// One project's additive share of the pool-loss tail at three common measures.
// These are not independently optimized project endpoints. In each column all
// projects use the same full probability witness and the same fractional pool
// tail, so their contributions reconcile exactly to that column's pool ES.
struct ProjectPoolLossTailContribution {
    std::string project_id{};
    double at_minimum_pool_es_measure_million{0.0};
    double at_central_measure_million{0.0};
    double at_maximum_pool_es_measure_million{0.0};
};

// The minimum and maximum full probability witnesses are reused from the
// corresponding principal-loss ES range. The central full probability measure
// is scenario_probability_bounds.central_weight. These vectors therefore store
// only tail probability masses (not full probability measures), in the same
// ascending scenario-id order. Exact aggregate-loss ties at the fractional
// boundary are included pro rata to their probability mass. When an endpoint
// probability measure only partly fills an exact equal-loss block, the block
// is filled pro rata to each atom's remaining upper-minus-lower capacity. These
// rules select one permutation- and name-invariant canonical result. A reported
// column is attribution under that selected common witness, not a claim that
// the optimizing probability measure or project attribution is mathematically
// unique.
struct PoolLossTailAttribution {
    double tail_probability{0.0};
    std::vector<double> minimum_pool_es_tail_mass_weights{};
    std::vector<double> central_tail_mass_weights{};
    std::vector<double> maximum_pool_es_tail_mass_weights{};
    std::vector<ProjectPoolLossTailContribution> projects{};
    double maximum_tail_mass_reconciliation_error{0.0};
    double maximum_project_contribution_reconciliation_error_million{0.0};
};

struct PortfolioAmbiguitySummary {
    // The ordinary engine is evaluated once under the declared central measure.
    // Its scenario cash paths are never rebuilt or modified for an endpoint.
    PortfolioSummary central_portfolio{};

    // Sorted by scenario_id. central_weight is normalized; lower and upper are
    // component bounds and are not themselves probability distributions.
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    double configured_central_weight_sum{0.0};
    double lower_bound_sum{0.0};
    double upper_bound_sum{0.0};

    std::vector<ProjectAmbiguitySummary> projects{};

    AmbiguityMetricRange expected_total_draws_million{};
    AmbiguityMetricRange expected_total_receipts_million{};
    AmbiguityMetricRange expected_total_pool_costs_million{};
    AmbiguityMetricRange expected_outstanding_principal_million{};
    AmbiguityMetricRange expected_principal_loss_million{};
    AmbiguityMetricRange expected_npv_million{};
    AmbiguityMetricRange principal_impairment_probability{};
    AmbiguityMetricRange negative_npv_probability{};

    AmbiguityMetricRange expected_peak_same_month_draw_million{};
    AmbiguityMetricRange expected_peak_same_month_funding_need_million{};
    AmbiguityMetricRange expected_peak_cumulative_net_outlay_million{};

    // One entry for every PortfolioCashSource. Bounds are componentwise; the
    // maximum of every source category generally is not one feasible source mix.
    std::vector<AmbiguityReturnSourceTotal> expected_return_sources{};

    // Exact bounds under the interval-probability ambiguity set. NPV downside
    // is measured on max(0, -NPV), not on the upper tail of raw NPV.
    AmbiguityMetricRange principal_loss_expected_shortfall_95_million{};
    AmbiguityMetricRange principal_loss_expected_shortfall_99_million{};
    AmbiguityMetricRange npv_shortfall_expected_shortfall_95_million{};
    AmbiguityMetricRange npv_shortfall_expected_shortfall_99_million{};

    // Additive project attribution under common minimum-pool-ES, central, and
    // maximum-pool-ES measures. These do not allocate unresolved exposure.
    PoolLossTailAttribution principal_loss_tail_attribution_95{};
    PoolLossTailAttribution principal_loss_tail_attribution_99{};

    // Maximum absolute sum-to-one or interval-bound residual across every
    // published endpoint witness after conversion to double.
    double maximum_endpoint_probability_error{0.0};
    // Maximum difference between a central metric recomputed from scenario
    // values and the corresponding ordinary PortfolioSummary metric.
    double maximum_central_metric_reconciliation_error{0.0};
};

void validate_portfolio_ambiguity_config(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity);

// Evaluates only the supplied finite joint scenarios. Probability ambiguity
// reweights fixed scenario results; it does not change project cash, construct
// scenarios, resample, or infer independence.
[[nodiscard]] PortfolioAmbiguitySummary evaluate_portfolio_ambiguity(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity);

} // namespace naturalehia::cellular_finance
