// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/probability_polytope_tail_attribution.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr long double kPublishedProbabilityTolerance = 1.0e-10L;

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

[[nodiscard]] double to_double(long double value) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            "probability-polytope tail attribution exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] long double probability_tolerance() noexcept {
    return 512.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon());
}

[[nodiscard]] long double money_tolerance(long double scale,
    long double tail_probability, std::size_t term_count) noexcept {
    const long double roundoff = 2048.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        std::max(1.0L, std::abs(scale)) *
        static_cast<long double>(std::max<std::size_t>(1U, term_count)) /
        std::max(tail_probability, 1.0e-12L);
    return 1.0e-8L + roundoff;
}

[[nodiscard]] long double path_money_tolerance(long double scale,
    std::size_t term_count) noexcept {
    return 1.0e-9L + 64.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        std::max(1.0L, std::abs(scale)) *
        static_cast<long double>(std::max<std::size_t>(1U, term_count));
}

void require_finite_nonnegative(double value, std::string_view label) {
    if (!std::isfinite(value) || value < 0.0) {
        throw std::invalid_argument(
            std::string(label) + " must be finite and non-negative");
    }
}

[[nodiscard]] std::unordered_map<std::string, std::size_t>
canonical_projection_taxonomy(
    const PortfolioSummary& portfolio,
    const ProbabilityPolytopeUpperExpectedShortfallProjection& projection) {
    if (projection.scenario_probabilities.size() !=
        portfolio.scenarios.size()) {
        throw std::invalid_argument(
            "pool-loss ES projection scenario taxonomy has the wrong size");
    }
    std::unordered_map<std::string, std::size_t> scenario_indices;
    scenario_indices.reserve(projection.scenario_probabilities.size());
    std::string previous_id;
    for (std::size_t index = 0U;
         index < projection.scenario_probabilities.size(); ++index) {
        const ProbabilityPolytopeScenario& projected =
            projection.scenario_probabilities[index];
        const JointScenarioResult& fixed = portfolio.scenarios[index];
        if (projected.scenario_id.empty() ||
            (!previous_id.empty() && projected.scenario_id <= previous_id) ||
            projected.scenario_id != fixed.scenario_id ||
            !scenario_indices.emplace(projected.scenario_id, index).second) {
            throw std::invalid_argument(
                "pool-loss ES projection scenario taxonomy is not the fixed portfolio taxonomy in canonical order");
        }
        previous_id = projected.scenario_id;
        if (!std::isfinite(projected.lower_weight) ||
            !std::isfinite(projected.central_weight) ||
            !std::isfinite(projected.upper_weight) ||
            projected.lower_weight < 0.0 || projected.upper_weight > 1.0 ||
            projected.lower_weight > projected.central_weight ||
            projected.central_weight > projected.upper_weight) {
            throw std::invalid_argument(
                "pool-loss ES projection has invalid scenario probabilities");
        }
        const long double central_error = std::abs(
            static_cast<long double>(projected.central_weight) -
            static_cast<long double>(fixed.normalized_weight));
        if (central_error > probability_tolerance()) {
            throw std::invalid_argument(
                "pool-loss ES projection central measure does not match the fixed portfolio");
        }
    }
    return scenario_indices;
}

void validate_event_taxonomy(
    const ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    const std::unordered_map<std::string, std::size_t>& scenario_indices) {
    std::unordered_set<std::string> event_ids;
    event_ids.reserve(projection.events.size());
    std::string previous_event_id;
    for (const ProbabilityEventConstraint& event : projection.events) {
        if (event.event_id.empty() ||
            (!previous_event_id.empty() &&
                event.event_id <= previous_event_id) ||
            !event_ids.emplace(event.event_id).second ||
            !std::isfinite(event.lower_probability) ||
            !std::isfinite(event.upper_probability) ||
            event.lower_probability < 0.0 ||
            event.upper_probability > 1.0 ||
            event.lower_probability > event.upper_probability ||
            event.scenario_ids.empty()) {
            throw std::invalid_argument(
                "pool-loss ES projection has an invalid event taxonomy");
        }
        previous_event_id = event.event_id;
        std::string previous_member;
        for (const std::string& member : event.scenario_ids) {
            if (scenario_indices.find(member) == scenario_indices.end() ||
                (!previous_member.empty() && member <= previous_member)) {
                throw std::invalid_argument(
                    "pool-loss ES projection event membership is not canonical or complete");
            }
            previous_member = member;
        }
    }
}

[[nodiscard]] long double validate_full_measure(
    const std::vector<double>& weights,
    const ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    const std::unordered_map<std::string, std::size_t>& scenario_indices,
    std::string_view column_name) {
    if (weights.size() != projection.scenario_probabilities.size()) {
        throw std::invalid_argument(std::string(column_name) +
            " full probability witness has the wrong scenario count");
    }
    CompensatedSum total;
    long double maximum_violation = 0.0L;
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        const long double weight = static_cast<long double>(weights[index]);
        const ProbabilityPolytopeScenario& bounds =
            projection.scenario_probabilities[index];
        if (!std::isfinite(weight)) {
            throw std::invalid_argument(std::string(column_name) +
                " full probability witness is non-finite");
        }
        total.add(weight);
        maximum_violation = std::max(maximum_violation,
            std::max(0.0L,
                static_cast<long double>(bounds.lower_weight) - weight));
        maximum_violation = std::max(maximum_violation,
            std::max(0.0L,
                weight - static_cast<long double>(bounds.upper_weight)));
    }
    maximum_violation = std::max(
        maximum_violation, std::abs(total.value() - 1.0L));
    for (const ProbabilityEventConstraint& event : projection.events) {
        CompensatedSum event_probability;
        for (const std::string& member : event.scenario_ids) {
            event_probability.add(static_cast<long double>(
                weights[scenario_indices.at(member)]));
        }
        maximum_violation = std::max(maximum_violation,
            std::max(0.0L,
                static_cast<long double>(event.lower_probability) -
                    event_probability.value()));
        maximum_violation = std::max(maximum_violation,
            std::max(0.0L,
                event_probability.value() -
                    static_cast<long double>(event.upper_probability)));
    }
    if (maximum_violation > kPublishedProbabilityTolerance) {
        throw std::invalid_argument(std::string(column_name) +
            " full probability witness violates the probability polytope");
    }
    return maximum_violation;
}

struct LossBlock {
    double loss{0.0};
    std::vector<std::size_t> indices{};
};

[[nodiscard]] std::vector<LossBlock> descending_loss_blocks(
    const std::vector<double>& pool_losses) {
    std::vector<std::size_t> order(pool_losses.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(),
        [&pool_losses](std::size_t first, std::size_t second) {
            if (pool_losses[first] != pool_losses[second]) {
                return pool_losses[first] > pool_losses[second];
            }
            return first < second;
        });
    std::vector<LossBlock> blocks;
    for (const std::size_t index : order) {
        if (blocks.empty() || blocks.back().loss != pool_losses[index]) {
            blocks.push_back(LossBlock{pool_losses[index], {}});
        }
        blocks.back().indices.push_back(index);
    }
    return blocks;
}

// Validates, but never replaces, the core projector's canonical tail masses.
// The one fractional equal-loss boundary block must be pro rata to full mass.
[[nodiscard]] long double validate_tail_mass(
    const std::vector<double>& full_weights,
    const std::vector<double>& tail_masses,
    const std::vector<double>& pool_losses,
    const std::vector<LossBlock>& loss_blocks,
    long double tail_probability, std::string_view column_name) {
    if (tail_masses.size() != full_weights.size() ||
        tail_masses.size() != pool_losses.size()) {
        throw std::invalid_argument(std::string(column_name) +
            " tail-mass witness has the wrong scenario count");
    }
    CompensatedSum mass_sum;
    long double maximum_violation = 0.0L;
    for (std::size_t index = 0U; index < tail_masses.size(); ++index) {
        const long double full =
            static_cast<long double>(full_weights[index]);
        const long double tail =
            static_cast<long double>(tail_masses[index]);
        if (!std::isfinite(tail)) {
            throw std::invalid_argument(std::string(column_name) +
                " tail-mass witness is non-finite");
        }
        mass_sum.add(tail);
        maximum_violation = std::max(
            maximum_violation, std::max(0.0L, -tail));
        maximum_violation = std::max(
            maximum_violation, std::max(0.0L, tail - full));
    }
    maximum_violation = std::max(maximum_violation,
        std::abs(mass_sum.value() - tail_probability));
    if (maximum_violation > kPublishedProbabilityTolerance) {
        throw std::invalid_argument(std::string(column_name) +
            " tail-mass witness violates its full measure or requested mass");
    }

    bool boundary_seen = false;
    bool tail_complete = false;
    const long double canonical_tolerance = probability_tolerance();
    for (const LossBlock& block : loss_blocks) {
        CompensatedSum full_sum;
        CompensatedSum tail_sum;
        for (const std::size_t index : block.indices) {
            full_sum.add(static_cast<long double>(full_weights[index]));
            tail_sum.add(static_cast<long double>(tail_masses[index]));
        }
        const long double full = full_sum.value();
        const long double tail = tail_sum.value();
        if (full <= canonical_tolerance) {
            continue;
        }
        const bool empty = std::abs(tail) <= canonical_tolerance;
        const bool complete = std::abs(tail - full) <= canonical_tolerance;
        if (!boundary_seen && !tail_complete && complete) {
            continue;
        }
        if (!boundary_seen && !tail_complete && !empty) {
            boundary_seen = true;
            tail_complete = true;
            const long double fraction = tail / full;
            for (const std::size_t index : block.indices) {
                const long double expected = fraction *
                    static_cast<long double>(full_weights[index]);
                if (std::abs(
                        static_cast<long double>(tail_masses[index]) -
                        expected) > canonical_tolerance) {
                    throw std::invalid_argument(std::string(column_name) +
                        " tied aggregate-loss boundary is not pro rata");
                }
            }
            continue;
        }
        if (empty) {
            tail_complete = true;
            continue;
        }
        throw std::invalid_argument(std::string(column_name) +
            " tail masses do not select the upper aggregate-loss tail");
    }
    return maximum_violation;
}

[[nodiscard]] double tail_expected_shortfall(
    const std::vector<double>& values,
    const std::vector<double>& tail_masses,
    long double tail_probability) {
    CompensatedSum numerator;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        numerator.add(static_cast<long double>(values[index]) *
            static_cast<long double>(tail_masses[index]));
    }
    return to_double(numerator.value() / tail_probability);
}

void reconcile_es(double calculated, double published,
    long double tail_probability, std::size_t scenario_count,
    std::string_view column_name) {
    if (!std::isfinite(published)) {
        throw std::invalid_argument(std::string(column_name) +
            " pool ES is non-finite");
    }
    const long double error = std::abs(
        static_cast<long double>(calculated) -
        static_cast<long double>(published));
    const long double scale = std::max(
        std::abs(static_cast<long double>(calculated)),
        std::abs(static_cast<long double>(published)));
    if (error > money_tolerance(scale, tail_probability, scenario_count)) {
        throw std::invalid_argument(std::string(column_name) +
            " tail masses do not reproduce aggregate resolved-principal-loss ES");
    }
}

void validate_projection_audits(
    const ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    long double value_scale) {
    const std::array<double, 17U> residuals{
        projection.minimum.maximum_constraint_violation,
        projection.minimum.maximum_tail_mass_violation,
        projection.minimum.objective_reconciliation_error,
        projection.minimum.threshold_formula_reconciliation_error,
        projection.minimum.optimality_residual,
        projection.central_objective_reconciliation_error,
        projection.maximum.maximum_constraint_violation,
        projection.maximum.maximum_tail_mass_violation,
        projection.maximum.objective_reconciliation_error,
        projection.maximum.threshold_formula_reconciliation_error,
        projection.maximum.optimality_residual,
        projection.maximum_endpoint_constraint_violation,
        projection.maximum_endpoint_tail_mass_violation,
        projection.maximum_endpoint_objective_reconciliation_error,
        projection.maximum_endpoint_threshold_formula_reconciliation_error,
        projection.maximum_endpoint_optimality_residual,
        projection.maximum_threshold_enumeration_optimality_residual};
    for (const double residual : residuals) {
        require_finite_nonnegative(residual,
            "pool-loss ES projection audit residual");
    }
    const double actual_maximum_constraint_residual = std::max({
        projection.minimum.maximum_constraint_violation,
        projection.maximum.maximum_constraint_violation,
        projection.maximum_endpoint_constraint_violation});
    const double actual_maximum_tail_residual = std::max({
        projection.minimum.maximum_tail_mass_violation,
        projection.maximum.maximum_tail_mass_violation,
        projection.maximum_endpoint_tail_mass_violation});
    if (actual_maximum_constraint_residual >
            static_cast<double>(kPublishedProbabilityTolerance) ||
        actual_maximum_tail_residual >
            static_cast<double>(kPublishedProbabilityTolerance)) {
        throw std::invalid_argument(
            "pool-loss ES projection reports a material probability residual");
    }
    const long double audit_tolerance = money_tolerance(value_scale,
        static_cast<long double>(projection.tail_probability),
        projection.scenario_probabilities.size());
    const double actual_maximum_objective_residual = std::max({
        projection.minimum.objective_reconciliation_error,
        projection.central_objective_reconciliation_error,
        projection.maximum.objective_reconciliation_error,
        projection.maximum_endpoint_objective_reconciliation_error});
    const double actual_maximum_threshold_residual = std::max({
        projection.minimum.threshold_formula_reconciliation_error,
        projection.maximum.threshold_formula_reconciliation_error,
        projection.maximum_endpoint_threshold_formula_reconciliation_error});
    const double actual_maximum_optimality_residual = std::max({
        projection.minimum.optimality_residual,
        projection.maximum.optimality_residual,
        projection.maximum_endpoint_optimality_residual,
        projection.maximum_threshold_enumeration_optimality_residual});
    if (static_cast<long double>(actual_maximum_objective_residual) >
            audit_tolerance ||
        static_cast<long double>(actual_maximum_threshold_residual) >
            audit_tolerance ||
        static_cast<long double>(actual_maximum_optimality_residual) >
            audit_tolerance) {
        throw std::invalid_argument(
            "pool-loss ES projection reports a material objective or optimality residual");
    }
}

struct AttributionColumn {
    const std::vector<double>* masses{nullptr};
    double pool_es{0.0};
};

} // namespace

ProbabilityPolytopePoolLossTailAttribution
attribute_probability_polytope_pool_loss_tail(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeUpperExpectedShortfallProjection&
        pool_loss_projection) {
    if (!std::isfinite(pool_loss_projection.tail_probability) ||
        pool_loss_projection.tail_probability <= 0.0 ||
        pool_loss_projection.tail_probability > 1.0) {
        throw std::invalid_argument(
            "pool-loss ES tail probability must be finite and in (0, 1]");
    }
    const bool threshold_enumeration_required =
        !pool_loss_projection.events.empty() &&
        pool_loss_projection.tail_probability != 1.0;
    if ((threshold_enumeration_required &&
            pool_loss_projection.distinct_thresholds_examined == 0U) ||
        !std::isfinite(pool_loss_projection.minimum_selected_threshold)) {
        throw std::invalid_argument(
            "pool-loss ES projection has no audited minimum threshold search");
    }

    const PortfolioSummary fixed = evaluate_portfolio(portfolio);
    const auto scenario_indices = canonical_projection_taxonomy(
        fixed, pool_loss_projection);
    validate_event_taxonomy(pool_loss_projection, scenario_indices);

    std::vector<std::string> project_ids;
    project_ids.reserve(fixed.projects.size());
    std::unordered_set<std::string> project_id_set;
    project_id_set.reserve(fixed.projects.size());
    for (const ProjectPortfolioSummary& project : fixed.projects) {
        if (project.project_id.empty() ||
            !project_id_set.emplace(project.project_id).second) {
            throw std::logic_error(
                "fixed portfolio project taxonomy is invalid");
        }
        project_ids.push_back(project.project_id);
    }
    std::sort(project_ids.begin(), project_ids.end());

    std::vector<double> pool_losses;
    pool_losses.reserve(fixed.scenarios.size());
    std::vector<std::vector<double>> project_losses(
        project_ids.size(), std::vector<double>(fixed.scenarios.size(), 0.0));
    double maximum_pathwise_error = 0.0;
    long double maximum_loss_scale = 0.0L;
    for (std::size_t scenario_index = 0U;
         scenario_index < fixed.scenarios.size(); ++scenario_index) {
        const JointScenarioResult& scenario = fixed.scenarios[scenario_index];
        if (scenario.projects.size() != project_ids.size() ||
            !std::isfinite(scenario.principal_loss_million) ||
            scenario.principal_loss_million < 0.0) {
            throw std::logic_error(
                "fixed portfolio loss taxonomy is incomplete");
        }
        std::unordered_map<std::string, double> loss_by_project;
        loss_by_project.reserve(scenario.projects.size());
        CompensatedSum project_loss_sum;
        for (const ProjectPathResult& project : scenario.projects) {
            if (!std::isfinite(project.principal_loss_million) ||
                project.principal_loss_million < 0.0 ||
                !loss_by_project.emplace(project.project_id,
                    project.principal_loss_million).second) {
                throw std::logic_error(
                    "fixed portfolio project loss taxonomy is invalid");
            }
            project_loss_sum.add(static_cast<long double>(
                project.principal_loss_million));
        }
        if (loss_by_project.size() != project_ids.size()) {
            throw std::logic_error(
                "fixed portfolio project loss taxonomy is incomplete");
        }
        for (std::size_t project_index = 0U;
             project_index < project_ids.size(); ++project_index) {
            const auto matching = loss_by_project.find(
                project_ids[project_index]);
            if (matching == loss_by_project.end()) {
                throw std::logic_error(
                    "fixed portfolio scenario omits a project loss");
            }
            project_losses[project_index][scenario_index] = matching->second;
        }
        const long double path_error = std::abs(
            project_loss_sum.value() -
            static_cast<long double>(scenario.principal_loss_million));
        maximum_pathwise_error = std::max(
            maximum_pathwise_error, to_double(path_error));
        maximum_loss_scale = std::max(maximum_loss_scale,
            std::max(std::abs(project_loss_sum.value()),
                std::abs(static_cast<long double>(
                    scenario.principal_loss_million))));
        if (path_error > path_money_tolerance(maximum_loss_scale,
                project_ids.size())) {
            throw std::logic_error(
                "fixed portfolio pool loss does not equal the sum of project losses");
        }
        pool_losses.push_back(scenario.principal_loss_million);
    }

    const long double tail_probability =
        static_cast<long double>(pool_loss_projection.tail_probability);
    std::vector<double> central_weights;
    central_weights.reserve(
        pool_loss_projection.scenario_probabilities.size());
    for (const ProbabilityPolytopeScenario& scenario :
         pool_loss_projection.scenario_probabilities) {
        central_weights.push_back(scenario.central_weight);
    }
    (void)validate_full_measure(pool_loss_projection.minimum.scenario_weights,
        pool_loss_projection, scenario_indices, "minimum");
    (void)validate_full_measure(central_weights, pool_loss_projection,
        scenario_indices, "central");
    (void)validate_full_measure(pool_loss_projection.maximum.scenario_weights,
        pool_loss_projection, scenario_indices, "maximum");

    const std::vector<LossBlock> loss_blocks =
        descending_loss_blocks(pool_losses);
    (void)validate_tail_mass(pool_loss_projection.minimum.scenario_weights,
        pool_loss_projection.minimum.tail_mass_weights, pool_losses,
        loss_blocks, tail_probability, "minimum");
    (void)validate_tail_mass(central_weights,
        pool_loss_projection.central_tail_mass_weights, pool_losses,
        loss_blocks, tail_probability, "central");
    (void)validate_tail_mass(pool_loss_projection.maximum.scenario_weights,
        pool_loss_projection.maximum.tail_mass_weights, pool_losses,
        loss_blocks, tail_probability, "maximum");

    const double calculated_minimum = tail_expected_shortfall(pool_losses,
        pool_loss_projection.minimum.tail_mass_weights, tail_probability);
    const double calculated_central = tail_expected_shortfall(pool_losses,
        pool_loss_projection.central_tail_mass_weights, tail_probability);
    const double calculated_maximum = tail_expected_shortfall(pool_losses,
        pool_loss_projection.maximum.tail_mass_weights, tail_probability);
    reconcile_es(calculated_minimum, pool_loss_projection.minimum.value,
        tail_probability, pool_losses.size(), "minimum");
    reconcile_es(calculated_central, pool_loss_projection.central,
        tail_probability, pool_losses.size(), "central");
    reconcile_es(calculated_maximum, pool_loss_projection.maximum.value,
        tail_probability, pool_losses.size(), "maximum");

    const long double range_tolerance = money_tolerance(maximum_loss_scale,
        tail_probability, pool_losses.size());
    if (static_cast<long double>(pool_loss_projection.minimum.value) >
            static_cast<long double>(pool_loss_projection.central) +
                range_tolerance ||
        static_cast<long double>(pool_loss_projection.central) >
            static_cast<long double>(pool_loss_projection.maximum.value) +
                range_tolerance) {
        throw std::invalid_argument(
            "pool-loss ES projection range does not contain its central value");
    }
    validate_projection_audits(pool_loss_projection, maximum_loss_scale);

    ProbabilityPolytopePoolLossTailAttribution result;
    result.tail_probability = pool_loss_projection.tail_probability;
    result.scenario_ids.reserve(
        pool_loss_projection.scenario_probabilities.size());
    for (const ProbabilityPolytopeScenario& scenario :
         pool_loss_projection.scenario_probabilities) {
        result.scenario_ids.push_back(scenario.scenario_id);
    }
    result.minimum_pool_es_tail_mass_weights =
        pool_loss_projection.minimum.tail_mass_weights;
    result.central_tail_mass_weights =
        pool_loss_projection.central_tail_mass_weights;
    result.maximum_pool_es_tail_mass_weights =
        pool_loss_projection.maximum.tail_mass_weights;
    result.minimum_pool_es_million = pool_loss_projection.minimum.value;
    result.central_pool_es_million = pool_loss_projection.central;
    result.maximum_pool_es_million = pool_loss_projection.maximum.value;
    result.maximum_pathwise_pool_loss_reconciliation_error_million =
        maximum_pathwise_error;

    const std::array<AttributionColumn, 3U> columns{
        AttributionColumn{
            &pool_loss_projection.minimum.tail_mass_weights,
            pool_loss_projection.minimum.value},
        AttributionColumn{
            &pool_loss_projection.central_tail_mass_weights,
            pool_loss_projection.central},
        AttributionColumn{
            &pool_loss_projection.maximum.tail_mass_weights,
            pool_loss_projection.maximum.value}};
    std::vector<std::array<double, 3U>> contributions(project_ids.size());
    for (std::size_t column = 0U; column < columns.size(); ++column) {
        CompensatedSum contribution_sum;
        for (std::size_t project_index = 0U;
             project_index < project_ids.size(); ++project_index) {
            const double contribution = tail_expected_shortfall(
                project_losses[project_index], *columns[column].masses,
                tail_probability);
            contributions[project_index][column] = contribution;
            contribution_sum.add(static_cast<long double>(contribution));
        }
        const long double error = std::abs(contribution_sum.value() -
            static_cast<long double>(columns[column].pool_es));
        result.maximum_contribution_to_pool_es_reconciliation_error_million =
            std::max(
                result.maximum_contribution_to_pool_es_reconciliation_error_million,
                to_double(error));
        if (error > money_tolerance(columns[column].pool_es,
                tail_probability, project_ids.size())) {
            throw std::logic_error(
                "common-tail project contributions do not reconcile to pool ES");
        }
    }

    result.projects.reserve(project_ids.size());
    for (std::size_t index = 0U; index < project_ids.size(); ++index) {
        result.projects.push_back(
            ProbabilityPolytopeProjectPoolLossTailContribution{
                project_ids[index], contributions[index][0],
                contributions[index][1], contributions[index][2]});
    }
    return result;
}

} // namespace naturalehia::cellular_finance
