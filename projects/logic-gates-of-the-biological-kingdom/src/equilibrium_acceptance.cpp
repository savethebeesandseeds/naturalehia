// SPDX-License-Identifier: MIT

#include <naturalehia/protein_logic/equilibrium_acceptance.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace naturalehia::protein_logic {
namespace {

void validate_probability(double value, std::string_view name) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        throw std::invalid_argument(std::string{name} + " must be finite and within [0, 1]");
    }
}

void validate_regions(const GlobalAssessment& assessment) {
    for (std::size_t index = 0; index < assessment.regions.size(); ++index) {
        const RegionBounds& region = assessment.regions[index];
        const XorRegion expected_region = kCanonicalXorRegionOrder[index];
        if (region.region != expected_region ||
            region.expected_level != expected_xor_level(expected_region)) {
            throw std::invalid_argument(
                      "assessment regions must use canonical XOR order and expected levels");
        }
        validate_probability(region.minimum.active_probability, "regional minimum");
        validate_probability(region.maximum.active_probability, "regional maximum");
        if (region.minimum.active_probability > region.maximum.active_probability) {
            throw std::invalid_argument(
                      "regional minimum probability must not exceed its maximum");
        }
    }
}

void validate_optional_probability(
    const std::optional<double>& value, std::string_view name) {
    if (value.has_value()) {
        validate_probability(*value, name);
    }
}

void validate_criteria(const SteadyStateXorCriteria& criteria) {
    if (criteria.threshold.has_value() &&
        (*criteria.threshold <= 0.0 || *criteria.threshold >= 1.0 ||
        !std::isfinite(*criteria.threshold))) {
        throw std::invalid_argument("acceptance threshold must be finite and within (0, 1)");
    }
    validate_optional_probability(criteria.minimum_separation, "minimum separation");
    validate_optional_probability(
        criteria.maximum_intended_off_activity, "maximum intended OFF activity");
    validate_optional_probability(criteria.maximum_single_high_floor_imbalance,
        "maximum single-high floor imbalance");
}

CriterionOutcome lower_bound_outcome(double value, const std::optional<double>& lower) {
    if (!lower.has_value()) {
        return CriterionOutcome::NotAssessed;
    }
    return value - *lower > kAssessmentDecisionTolerance ? CriterionOutcome::Pass
                                                          : CriterionOutcome::Fail;
}

CriterionOutcome upper_bound_outcome(double value, const std::optional<double>& upper) {
    if (!upper.has_value()) {
        return CriterionOutcome::NotAssessed;
    }
    return *upper - value > kAssessmentDecisionTolerance ? CriterionOutcome::Pass
                                                          : CriterionOutcome::Fail;
}

CriterionOutcome aggregate_outcomes(const std::array<CriterionOutcome, 4>& outcomes) {
    if (std::ranges::find(outcomes, CriterionOutcome::Fail) != outcomes.end()) {
        return CriterionOutcome::Fail;
    }
    if (std::ranges::find(outcomes, CriterionOutcome::NotAssessed) != outcomes.end()) {
        return CriterionOutcome::NotAssessed;
    }
    return CriterionOutcome::Pass;
}

} // namespace

SteadyStateXorMetrics compute_steady_state_xor_metrics(
    const GlobalAssessment& assessment) {
    validate_regions(assessment);

    const RegionBounds& low_low = assessment.regions[0];
    const RegionBounds& high_low = assessment.regions[1];
    const RegionBounds& low_high = assessment.regions[2];
    const RegionBounds& high_high = assessment.regions[3];

    SteadyStateXorMetrics metrics{};
    metrics.basal_off_activity_ceiling = low_low.maximum.active_probability;
    metrics.input_a_only_on_floor = high_low.minimum.active_probability;
    metrics.input_a_only_on_ceiling = high_low.maximum.active_probability;
    metrics.input_b_only_on_floor = low_high.minimum.active_probability;
    metrics.input_b_only_on_ceiling = low_high.maximum.active_probability;
    metrics.joint_high_off_activity_ceiling = high_high.maximum.active_probability;
    metrics.on_floor =
        std::min(metrics.input_a_only_on_floor, metrics.input_b_only_on_floor);
    metrics.intended_off_activity_ceiling = std::max(
        metrics.basal_off_activity_ceiling, metrics.joint_high_off_activity_ceiling);
    metrics.separation = metrics.on_floor - metrics.intended_off_activity_ceiling;
    metrics.single_high_floor_imbalance =
        std::abs(metrics.input_a_only_on_floor - metrics.input_b_only_on_floor);
    metrics.single_high_response_gap_upper_bound =
        std::max(metrics.input_a_only_on_ceiling - metrics.input_b_only_on_floor,
            metrics.input_b_only_on_ceiling - metrics.input_a_only_on_floor);
    return metrics;
}

SteadyStateXorAcceptance assess_steady_state_xor(
    const GlobalAssessment& assessment, const SteadyStateXorCriteria& criteria) {
    validate_criteria(criteria);

    SteadyStateXorAcceptance result{};
    result.metrics = compute_steady_state_xor_metrics(assessment);
    result.criteria = criteria;

    if (criteria.threshold.has_value()) {
        result.on_threshold_margin = result.metrics.on_floor - *criteria.threshold;
        result.off_threshold_margin =
            *criteria.threshold - result.metrics.intended_off_activity_ceiling;
        result.threshold_margin =
            std::min(*result.on_threshold_margin, *result.off_threshold_margin);
        result.threshold_outcome =
            *result.threshold_margin > result.decision_tolerance ? CriterionOutcome::Pass
                                                                 : CriterionOutcome::Fail;
    }
    if (criteria.minimum_separation.has_value()) {
        result.separation_clearance =
            result.metrics.separation - *criteria.minimum_separation;
    }
    if (criteria.maximum_intended_off_activity.has_value()) {
        result.intended_off_activity_clearance =
            *criteria.maximum_intended_off_activity -
            result.metrics.intended_off_activity_ceiling;
    }
    if (criteria.maximum_single_high_floor_imbalance.has_value()) {
        result.single_high_floor_balance_clearance =
            *criteria.maximum_single_high_floor_imbalance -
            result.metrics.single_high_floor_imbalance;
    }

    result.separation_outcome =
        lower_bound_outcome(result.metrics.separation, criteria.minimum_separation);
    result.intended_off_activity_outcome = upper_bound_outcome(
        result.metrics.intended_off_activity_ceiling,
        criteria.maximum_intended_off_activity);
    result.single_high_floor_balance_outcome = upper_bound_outcome(
        result.metrics.single_high_floor_imbalance,
        criteria.maximum_single_high_floor_imbalance);
    result.overall_outcome = aggregate_outcomes({{
                result.threshold_outcome,
                result.separation_outcome,
                result.intended_off_activity_outcome,
                result.single_high_floor_balance_outcome,
            }});
    return result;
}

} // namespace naturalehia::protein_logic
