// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/pooled_loss_protection.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr std::size_t kBisectionIterations = 96U;
constexpr double kMaximumAnnualHurdle = 10.0;

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

struct ProjectBasis {
    std::string project_id{};
    double gross_loss_million{0.0};
};

struct ScenarioBasis {
    std::string scenario_id{};
    double underlying_npv_million{0.0};
    double gross_loss_million{0.0};
    std::vector<ProjectBasis> projects{};
};

[[nodiscard]] bool is_ascii_alpha_numeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alpha_numeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alpha_numeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_identifier(
    std::string_view value, std::string_view description) {
    if (!is_safe_identifier(value)) {
        throw std::invalid_argument(
            std::string(description) + " must be a safe identifier");
    }
}

void require_safe_text(std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

[[nodiscard]] double to_double(long double value) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            "pooled loss protection aggregation exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] const JointScenarioResult& find_scenario(
    const PortfolioSummary& summary, std::string_view scenario_id) {
    const auto matching = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [scenario_id](const auto& scenario) {
            return scenario.scenario_id == scenario_id;
        });
    if (matching == summary.scenarios.end()) {
        throw std::logic_error(
            "pooled loss protection lost a validated scenario");
    }
    return *matching;
}

[[nodiscard]] const ProjectPathResult& find_project(
    const JointScenarioResult& scenario, std::string_view project_id) {
    const auto matching = std::find_if(scenario.projects.begin(),
        scenario.projects.end(), [project_id](const auto& project) {
            return project.project_id == project_id;
        });
    if (matching == scenario.projects.end()) {
        throw std::logic_error(
            "pooled loss protection lost a validated project path");
    }
    return *matching;
}

[[nodiscard]] std::vector<AmbiguityScenarioMetricValue> keyed_values(
    const std::vector<ScenarioBasis>& basis,
    const std::vector<double>& values) {
    if (basis.size() != values.size()) {
        throw std::logic_error(
            "pooled loss protection metric has the wrong scenario count");
    }
    std::vector<AmbiguityScenarioMetricValue> result;
    result.reserve(basis.size());
    for (std::size_t index = 0U; index < basis.size(); ++index) {
        result.push_back(
            AmbiguityScenarioMetricValue{basis[index].scenario_id,
                values[index]});
    }
    return result;
}

[[nodiscard]] double endpoint_expectation_error(
    const AmbiguityMetricProjection& projection,
    const std::vector<AmbiguityScenarioMetricValue>& values) {
    std::unordered_map<std::string, double> values_by_id;
    values_by_id.reserve(values.size());
    for (const AmbiguityScenarioMetricValue& value : values) {
        values_by_id.emplace(value.scenario_id, value.value);
    }
    const auto reconcile = [&](const AmbiguityEndpoint& endpoint) {
        if (endpoint.scenario_weights.size() !=
            projection.scenario_probability_bounds.size()) {
            throw std::logic_error(
                "pooled loss protection expectation witness has the wrong size");
        }
        CompensatedSum sum;
        for (std::size_t index = 0U;
             index < projection.scenario_probability_bounds.size(); ++index) {
            const auto matching = values_by_id.find(
                projection.scenario_probability_bounds[index].scenario_id);
            if (matching == values_by_id.end()) {
                throw std::logic_error(
                    "pooled loss protection expectation witness lost a scenario");
            }
            sum.add(static_cast<long double>(
                        endpoint.scenario_weights[index]) *
                static_cast<long double>(matching->second));
        }
        return std::abs(to_double(sum.value()) - endpoint.value);
    };
    return std::max(reconcile(projection.expectation.minimum),
        reconcile(projection.expectation.maximum));
}

[[nodiscard]] long double upper_expected_shortfall(
    const std::vector<double>& values, const std::vector<double>& weights,
    double tail_probability) {
    if (values.size() != weights.size() || values.empty()) {
        throw std::logic_error(
            "pooled loss protection tail witness has the wrong size");
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

    CompensatedSum weight_sum;
    for (const double weight : weights) {
        weight_sum.add(static_cast<long double>(weight));
    }
    const long double requested =
        static_cast<long double>(tail_probability) * weight_sum.value();
    if (requested <= 0.0L) {
        throw std::logic_error(
            "pooled loss protection tail witness has no probability mass");
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
    if (remaining > 1.0e-12L) {
        throw std::logic_error(
            "pooled loss protection tail witness has insufficient mass");
    }
    return total.value() / requested;
}

[[nodiscard]] double endpoint_tail_error(
    const AmbiguityUpperExpectedShortfallProjection& projection,
    const std::vector<AmbiguityScenarioMetricValue>& keyed) {
    std::unordered_map<std::string, double> values_by_id;
    values_by_id.reserve(keyed.size());
    for (const auto& value : keyed) {
        values_by_id.emplace(value.scenario_id, value.value);
    }
    std::vector<double> values;
    values.reserve(projection.scenario_probability_bounds.size());
    for (const auto& bounds : projection.scenario_probability_bounds) {
        const auto matching = values_by_id.find(bounds.scenario_id);
        if (matching == values_by_id.end()) {
            throw std::logic_error(
                "pooled loss protection tail witness lost a scenario");
        }
        values.push_back(matching->second);
    }
    const auto reconcile = [&](const AmbiguityEndpoint& endpoint) {
        const double calculated = to_double(upper_expected_shortfall(values,
            endpoint.scenario_weights, projection.tail_probability));
        return std::abs(calculated - endpoint.value);
    };
    return std::max(
        reconcile(projection.upper_expected_shortfall.minimum),
        reconcile(projection.upper_expected_shortfall.maximum));
}

struct WeightedValue {
    double value{0.0};
    double weight{0.0};
};

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
    CompensatedSum weight_sum;
    for (const WeightedValue& item : ordered) {
        weight_sum.add(static_cast<long double>(item.weight));
    }
    const long double target =
        static_cast<long double>(probability) * weight_sum.value();
    CompensatedSum cumulative;
    for (const WeightedValue& item : ordered) {
        cumulative.add(static_cast<long double>(item.weight));
        if (cumulative.value() >= target) {
            return item.value;
        }
    }
    return ordered.back().value;
}

[[nodiscard]] PortfolioDistributionSummary summarize_distribution(
    const std::vector<double>& values, const std::vector<double>& weights) {
    if (values.empty() || values.size() != weights.size()) {
        throw std::logic_error(
            "pooled loss protection central distribution is invalid");
    }
    std::vector<WeightedValue> weighted;
    weighted.reserve(values.size());
    CompensatedSum weight_sum;
    CompensatedSum value_sum;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        weighted.push_back(WeightedValue{values[index], weights[index]});
        weight_sum.add(static_cast<long double>(weights[index]));
        value_sum.add(static_cast<long double>(weights[index]) *
            static_cast<long double>(values[index]));
    }
    const long double total_weight = weight_sum.value();
    if (total_weight <= 0.0L) {
        throw std::logic_error(
            "pooled loss protection central distribution has no mass");
    }
    const long double mean = value_sum.value() / total_weight;
    CompensatedSum variance;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        const long double difference =
            static_cast<long double>(values[index]) - mean;
        variance.add(static_cast<long double>(weights[index]) *
            difference * difference);
    }

    PortfolioDistributionSummary result;
    result.mean = to_double(mean);
    result.standard_deviation = std::sqrt(std::max(0.0,
        to_double(variance.value() / total_weight)));
    result.p50 = weighted_quantile(weighted, 0.50);
    result.p95 = weighted_quantile(weighted, 0.95);
    result.p99 = weighted_quantile(weighted, 0.99);
    result.maximum = *std::max_element(values.begin(), values.end());
    result.expected_shortfall_95 = to_double(
        upper_expected_shortfall(values, weights, 0.05));
    result.expected_shortfall_99 = to_double(
        upper_expected_shortfall(values, weights, 0.01));
    return result;
}

[[nodiscard]] double safe_discount_factor(double annual_rate,
    std::size_t month) {
    const double factor = std::pow(1.0 + annual_rate,
        static_cast<double>(month) / 12.0);
    if (!std::isfinite(factor) || factor <= 0.0) {
        throw std::overflow_error(
            "pooled loss protection discount factor is invalid");
    }
    return factor;
}

void validate_terms(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection) {
    validate_success_participation_config(
        portfolio, ambiguity, participation);
    if (protection.model_version != kPooledLossProtectionModelVersion) {
        throw std::invalid_argument(
            "pooled loss protection model_version does not match this engine");
    }
    if (!protection.synthetic_inputs) {
        throw std::invalid_argument(
            "pooled loss protection v0.1 accepts synthetic inputs only");
    }
    require_safe_text(
        protection.scenario_label, "pooled loss protection scenario_label");
    require_safe_text(
        protection.source_note, "pooled loss protection source_note");
    require_safe_identifier(
        protection.provider_id, "pooled loss protection provider_id");
    if (!protection
             .portfolio_principal_loss_is_contractual_reference_amount) {
        throw std::invalid_argument(
            "pooled loss protection requires an explicit terminal-loss reference assertion");
    }
    if (!protection.gross_project_loss_remains_visible) {
        throw std::invalid_argument(
            "pooled loss protection requires an explicit gross-loss visibility assertion");
    }
    if (!protection
             .support_is_assumed_fully_funded_and_performing_in_all_scenarios) {
        throw std::invalid_argument(
            "pooled loss protection requires an explicit fully-funded performance assumption");
    }
    if (!protection.premium_is_upfront_at_month_zero) {
        throw std::invalid_argument(
            "pooled loss protection requires an explicit month-zero premium assertion");
    }
    if (!std::isfinite(
            protection.underlying_success_participation_fraction) ||
        protection.underlying_success_participation_fraction < 0.0 ||
        protection.underlying_success_participation_fraction > 1.0) {
        throw std::invalid_argument(
            "pooled loss protection underlying participation fraction must be finite and within [0, 1]");
    }
    if (protection.settlement_month != portfolio.horizon_months) {
        throw std::invalid_argument(
            "pooled loss protection settlement_month must equal the terminal portfolio horizon");
    }
    if (!std::isfinite(protection.support_cap_million) ||
        protection.support_cap_million < 0.0) {
        throw std::invalid_argument(
            "pooled loss protection support cap must be finite and non-negative");
    }
    const double reference_principal =
        portfolio_aggregate_reference_principal(portfolio);
    if (protection.support_cap_million > reference_principal) {
        throw std::invalid_argument(
            "pooled loss protection support cap exceeds aggregate reference principal");
    }
    if (!std::isfinite(protection.provider_annual_physical_hurdle_rate) ||
        protection.provider_annual_physical_hurdle_rate < 0.0 ||
        protection.provider_annual_physical_hurdle_rate >
            kMaximumAnnualHurdle) {
        throw std::invalid_argument(
            "pooled loss protection provider hurdle is outside the supported range");
    }
    if (!portfolio.loss_layers.empty()) {
        throw std::invalid_argument(
            "pooled loss protection v0.1 requires an untranched portfolio with empty loss_layers");
    }
}

[[nodiscard]] double maximum_supported_fraction(
    double support_cap_million,
    double aggregate_reference_principal_million) {
    const long double exact = static_cast<long double>(support_cap_million) /
        static_cast<long double>(aggregate_reference_principal_million);
    double result = std::min(1.0, static_cast<double>(exact));
    while (static_cast<long double>(result) *
            static_cast<long double>(aggregate_reference_principal_million) >
        static_cast<long double>(support_cap_million)) {
        result = std::nextafter(result, 0.0);
    }
    return std::max(0.0, result);
}

struct ProjectionControls {
    double maximum_witness_error{0.0};
    double maximum_probability_error{0.0};
};

[[nodiscard]] AmbiguityMetricProjection project_expectation(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<ScenarioBasis>& basis,
    const std::vector<double>& values, ProjectionControls& controls) {
    const std::vector<AmbiguityScenarioMetricValue> keyed =
        keyed_values(basis, values);
    const AmbiguityMetricProjection projection =
        projector.project_expectation(keyed);
    controls.maximum_witness_error = std::max(
        controls.maximum_witness_error,
        endpoint_expectation_error(projection, keyed));
    controls.maximum_probability_error = std::max(
        controls.maximum_probability_error,
        projection.maximum_endpoint_probability_error);
    return projection;
}

[[nodiscard]] AmbiguityUpperExpectedShortfallProjection
project_expected_shortfall(const PortfolioAmbiguityProjector& projector,
    const std::vector<ScenarioBasis>& basis,
    const std::vector<double>& values, double tail_probability,
    ProjectionControls& controls) {
    const std::vector<AmbiguityScenarioMetricValue> keyed =
        keyed_values(basis, values);
    const AmbiguityUpperExpectedShortfallProjection projection =
        projector.project_upper_expected_shortfall(keyed, tail_probability);
    controls.maximum_witness_error = std::max(
        controls.maximum_witness_error,
        endpoint_tail_error(projection, keyed));
    controls.maximum_probability_error = std::max(
        controls.maximum_probability_error,
        projection.maximum_endpoint_probability_error);
    return projection;
}

[[nodiscard]] std::vector<double> investor_npvs(
    const std::vector<ScenarioBasis>& basis, double coverage_fraction,
    double investor_discount_factor) {
    std::vector<double> result;
    result.reserve(basis.size());
    for (const ScenarioBasis& scenario : basis) {
        const double nominal_claim =
            coverage_fraction * scenario.gross_loss_million;
        const double claim_present_value =
            nominal_claim / investor_discount_factor;
        const double combined =
            scenario.underlying_npv_million + claim_present_value;
        if (!std::isfinite(claim_present_value) ||
            !std::isfinite(combined)) {
            throw std::overflow_error(
                "pooled loss protection investor NPV exceeded numeric range");
        }
        result.push_back(combined);
    }
    return result;
}

[[nodiscard]] AmbiguityMetricProjection project_investor_npv(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<ScenarioBasis>& basis, double coverage_fraction,
    double investor_discount_factor, ProjectionControls& controls) {
    return project_expectation(projector, basis,
        investor_npvs(basis, coverage_fraction, investor_discount_factor),
        controls);
}

[[nodiscard]] PooledLossProtectionRobustPoint make_point(
    const PortfolioAmbiguityProjector& projector,
    const std::vector<ScenarioBasis>& basis,
    double coverage_fraction, double support_cap_million,
    double aggregate_reference_principal_million,
    double target_million, double investor_discount_factor,
    double provider_discount_factor, ProjectionControls& controls) {
    std::vector<double> claims;
    std::vector<double> provider_present_values;
    std::vector<double> claim_indicators;
    claims.reserve(basis.size());
    provider_present_values.reserve(basis.size());
    claim_indicators.reserve(basis.size());
    double maximum_claim = 0.0;
    for (const ScenarioBasis& scenario : basis) {
        const double claim = coverage_fraction * scenario.gross_loss_million;
        if (!std::isfinite(claim)) {
            throw std::overflow_error(
                "pooled loss protection claim exceeded numeric range");
        }
        claims.push_back(claim);
        provider_present_values.push_back(claim / provider_discount_factor);
        claim_indicators.push_back(claim > 0.0 ? 1.0 : 0.0);
        maximum_claim = std::max(maximum_claim, claim);
    }

    const AmbiguityMetricProjection investor_projection =
        project_investor_npv(projector, basis, coverage_fraction,
            investor_discount_factor, controls);
    const AmbiguityMetricProjection nominal_projection =
        project_expectation(projector, basis, claims, controls);
    const AmbiguityMetricProjection provider_pv_projection =
        project_expectation(
            projector, basis, provider_present_values, controls);
    const AmbiguityMetricProjection probability_projection =
        project_expectation(projector, basis, claim_indicators, controls);
    const AmbiguityUpperExpectedShortfallProjection es95_projection =
        project_expected_shortfall(
            projector, basis, claims, 0.05, controls);
    const AmbiguityUpperExpectedShortfallProjection es99_projection =
        project_expected_shortfall(
            projector, basis, claims, 0.01, controls);
    const AmbiguityUpperExpectedShortfallProjection provider_es95_projection =
        project_expected_shortfall(projector, basis,
            provider_present_values, 0.05, controls);
    const AmbiguityUpperExpectedShortfallProjection provider_es99_projection =
        project_expected_shortfall(projector, basis,
            provider_present_values, 0.01, controls);

    std::vector<double> central_weights;
    central_weights.reserve(basis.size());
    if (nominal_projection.scenario_probability_bounds.size() !=
        basis.size()) {
        throw std::logic_error(
            "pooled loss protection central distribution has the wrong scenario count");
    }
    for (std::size_t index = 0U; index < basis.size(); ++index) {
        if (nominal_projection.scenario_probability_bounds[index]
                .scenario_id != basis[index].scenario_id) {
            throw std::logic_error(
                "pooled loss protection central distribution lost scenario order");
        }
        central_weights.push_back(
            nominal_projection.scenario_probability_bounds[index]
                .central_weight);
    }

    PooledLossProtectionRobustPoint result;
    result.coverage_fraction = coverage_fraction;
    result.investor_target_worst_expected_npv_million = target_million;
    result.investor_expected_npv_before_premium_million =
        investor_projection.expectation;
    const double worst_investor_npv =
        investor_projection.expectation.minimum.value;
    result.investor_signed_premium_headroom_million = to_double(
        static_cast<long double>(worst_investor_npv) -
        static_cast<long double>(target_million));
    if (result.investor_signed_premium_headroom_million >= 0.0) {
        double conservative_ceiling =
            result.investor_signed_premium_headroom_million;
        while (worst_investor_npv + (-conservative_ceiling) <
            target_million) {
            const double next = std::nextafter(conservative_ceiling, 0.0);
            if (next == conservative_ceiling) {
                throw std::logic_error(
                    "pooled loss protection could not certify an investor premium ceiling");
            }
            conservative_ceiling = next;
        }
        result.investor_maximum_nonnegative_premium_million =
            conservative_ceiling;
    }
    result.provider_minimum_robust_break_even_premium_million =
        provider_pv_projection.expectation.maximum.value;
    const double effective_investor_ceiling =
        result.investor_maximum_nonnegative_premium_million.value_or(
            result.investor_signed_premium_headroom_million);
    result.premium_feasibility_gap_million = std::max(0.0,
        to_double(static_cast<long double>(
                      result.provider_minimum_robust_break_even_premium_million) -
            static_cast<long double>(effective_investor_ceiling)));
    result.robust_nonnegative_premium_interval_exists =
        result.investor_maximum_nonnegative_premium_million.has_value() &&
        result.provider_minimum_robust_break_even_premium_million <=
            *result.investor_maximum_nonnegative_premium_million;

    PooledLossProtectionProviderRisk& risk = result.provider_risk;
    risk.legal_support_cap_million = support_cap_million;
    risk.contractual_maximum_exposure_million =
        coverage_fraction * aggregate_reference_principal_million;
    risk.modeled_maximum_claim_million = maximum_claim;
    risk.uncommitted_legal_cap_capacity_million =
        support_cap_million - risk.contractual_maximum_exposure_million;
    if (support_cap_million > 0.0) {
        risk.maximum_cap_utilization =
            risk.contractual_maximum_exposure_million /
            support_cap_million;
    }
    risk.central_claim_nominal_million =
        summarize_distribution(claims, central_weights);
    risk.expected_claim_nominal_million = nominal_projection.expectation;
    risk.expected_claim_present_value_million =
        provider_pv_projection.expectation;
    risk.claim_probability = probability_projection.expectation;
    risk.claim_expected_shortfall_95_nominal_million =
        es95_projection.upper_expected_shortfall;
    risk.claim_expected_shortfall_99_nominal_million =
        es99_projection.upper_expected_shortfall;
    risk.claim_expected_shortfall_95_present_value_million =
        provider_es95_projection.upper_expected_shortfall;
    risk.claim_expected_shortfall_99_present_value_million =
        provider_es99_projection.upper_expected_shortfall;

    result.maximum_endpoint_probability_error = std::max({
        investor_projection.maximum_endpoint_probability_error,
        nominal_projection.maximum_endpoint_probability_error,
        provider_pv_projection.maximum_endpoint_probability_error,
        probability_projection.maximum_endpoint_probability_error,
        es95_projection.maximum_endpoint_probability_error,
        es99_projection.maximum_endpoint_probability_error,
        provider_es95_projection.maximum_endpoint_probability_error,
        provider_es99_projection.maximum_endpoint_probability_error});
    return result;
}

[[nodiscard]] AmbiguityEndpoint shifted_endpoint(
    const AmbiguityEndpoint& endpoint, double shift) {
    AmbiguityEndpoint result = endpoint;
    result.value += shift;
    if (!std::isfinite(result.value)) {
        throw std::overflow_error(
            "pooled loss protection premium translation exceeded numeric range");
    }
    return result;
}

[[nodiscard]] AmbiguityMetricRange shifted_range(
    const AmbiguityMetricRange& range, double shift) {
    AmbiguityMetricRange result;
    result.minimum = shifted_endpoint(range.minimum, shift);
    result.central = range.central + shift;
    result.maximum = shifted_endpoint(range.maximum, shift);
    if (!std::isfinite(result.central)) {
        throw std::overflow_error(
            "pooled loss protection premium translation exceeded numeric range");
    }
    return result;
}

} // namespace

std::string_view to_string(
    PooledLossProtectionSolveStatus status) noexcept {
    switch (status) {
    case PooledLossProtectionSolveStatus::AlreadyMeetsInvestorTargetAtZero:
        return "already-meets-investor-target-at-zero";
    case PooledLossProtectionSolveStatus::CertifiedInteriorBracket:
        return "certified-interior-bracket";
    case PooledLossProtectionSolveStatus::CertifiedSupportCapBoundaryBracket:
        return "certified-support-cap-boundary-bracket";
    case PooledLossProtectionSolveStatus::FullCoverageRequired:
        return "full-coverage-required";
    case PooledLossProtectionSolveStatus::NoGrossReferenceLoss:
        return "no-gross-reference-loss";
    case PooledLossProtectionSolveStatus::NoSupportCapacity:
        return "no-support-capacity";
    case PooledLossProtectionSolveStatus::
        UnattainableAtMaximumSupportedCoverage:
        return "unattainable-at-maximum-supported-coverage";
    }
    return "unknown";
}

void validate_pooled_loss_protection_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection) {
    validate_terms(portfolio, ambiguity, participation, protection);
}

PooledLossProtectionRobustPoint
evaluate_pooled_loss_protection_coverage(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection,
    double coverage_fraction) {
    validate_terms(portfolio, ambiguity, participation, protection);
    if (!std::isfinite(coverage_fraction) || coverage_fraction < 0.0 ||
        coverage_fraction > 1.0) {
        throw std::invalid_argument(
            "pooled loss protection coverage fraction must be finite and within [0, 1]");
    }

    const double aggregate_reference_principal =
        portfolio_aggregate_reference_principal(portfolio);
    const double maximum_coverage = maximum_supported_fraction(
        protection.support_cap_million, aggregate_reference_principal);
    if (coverage_fraction > maximum_coverage) {
        throw std::invalid_argument(
            "pooled loss protection coverage fraction exceeds the reference-principal support-cap domain");
    }

    const PortfolioSummary gross_portfolio = evaluate_portfolio(portfolio);
    const PortfolioConfig underlying_config =
        apply_success_participation_fraction(portfolio, participation,
            protection.underlying_success_participation_fraction);
    const PortfolioSummary underlying_portfolio =
        evaluate_portfolio(underlying_config);

    std::vector<ScenarioBasis> basis;
    basis.reserve(gross_portfolio.scenarios.size());
    for (const JointScenarioResult& gross_scenario :
         gross_portfolio.scenarios) {
        const JointScenarioResult& underlying_scenario =
            find_scenario(underlying_portfolio, gross_scenario.scenario_id);
        ScenarioBasis scenario;
        scenario.scenario_id = gross_scenario.scenario_id;
        scenario.underlying_npv_million = underlying_scenario.npv_million;
        scenario.gross_loss_million = gross_scenario.principal_loss_million;
        scenario.projects.reserve(gross_scenario.projects.size());
        for (const ProjectPathResult& gross_project :
             gross_scenario.projects) {
            const ProjectPathResult& underlying_project = find_project(
                underlying_scenario, gross_project.project_id);
            if (gross_project.principal_loss_million !=
                underlying_project.principal_loss_million) {
                throw std::logic_error(
                    "pooled loss protection participation changed gross project loss");
            }
            scenario.projects.push_back(ProjectBasis{
                gross_project.project_id,
                gross_project.principal_loss_million});
        }
        basis.push_back(std::move(scenario));
    }

    const PortfolioAmbiguityProjector projector(portfolio, ambiguity);
    const double investor_discount_factor = safe_discount_factor(
        portfolio.annual_physical_hurdle_rate,
        protection.settlement_month);
    const double provider_discount_factor = safe_discount_factor(
        protection.provider_annual_physical_hurdle_rate,
        protection.settlement_month);
    ProjectionControls controls;
    return make_point(projector, basis, coverage_fraction,
        protection.support_cap_million, aggregate_reference_principal,
        participation.target_worst_expected_npv_million,
        investor_discount_factor, provider_discount_factor, controls);
}

PooledLossProtectionSummary solve_pooled_loss_protection(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection) {
    validate_terms(portfolio, ambiguity, participation, protection);
    const PortfolioAmbiguityProjector projector(portfolio, ambiguity);
    const PortfolioSummary gross_portfolio = evaluate_portfolio(portfolio);
    const PortfolioConfig underlying_config =
        apply_success_participation_fraction(portfolio, participation,
            protection.underlying_success_participation_fraction);
    const PortfolioSummary underlying_portfolio =
        evaluate_portfolio(underlying_config);

    std::vector<ScenarioBasis> basis;
    basis.reserve(gross_portfolio.scenarios.size());
    double maximum_loss = 0.0;
    double maximum_loss_change = 0.0;
    for (const JointScenarioResult& gross_scenario :
         gross_portfolio.scenarios) {
        const JointScenarioResult& underlying_scenario =
            find_scenario(underlying_portfolio, gross_scenario.scenario_id);
        maximum_loss_change = std::max(maximum_loss_change,
            std::abs(gross_scenario.principal_loss_million -
                underlying_scenario.principal_loss_million));

        ScenarioBasis scenario;
        scenario.scenario_id = gross_scenario.scenario_id;
        scenario.underlying_npv_million = underlying_scenario.npv_million;
        scenario.gross_loss_million =
            gross_scenario.principal_loss_million;
        maximum_loss = std::max(maximum_loss, scenario.gross_loss_million);
        scenario.projects.reserve(gross_scenario.projects.size());
        for (const ProjectPathResult& gross_project :
             gross_scenario.projects) {
            const ProjectPathResult& underlying_project = find_project(
                underlying_scenario, gross_project.project_id);
            maximum_loss_change = std::max(maximum_loss_change,
                std::abs(gross_project.principal_loss_million -
                    underlying_project.principal_loss_million));
            scenario.projects.push_back(ProjectBasis{
                gross_project.project_id,
                gross_project.principal_loss_million});
        }
        basis.push_back(std::move(scenario));
    }

    const double aggregate_reference_principal =
        portfolio_aggregate_reference_principal(portfolio);
    const double maximum_coverage = maximum_supported_fraction(
        protection.support_cap_million, aggregate_reference_principal);
    const double investor_discount_factor = safe_discount_factor(
        portfolio.annual_physical_hurdle_rate,
        protection.settlement_month);
    const double provider_discount_factor = safe_discount_factor(
        protection.provider_annual_physical_hurdle_rate,
        protection.settlement_month);
    const double target =
        participation.target_worst_expected_npv_million;

    ProjectionControls controls;
    const AmbiguityMetricProjection zero_investor = project_investor_npv(
        projector, basis, 0.0, investor_discount_factor, controls);
    const AmbiguityMetricProjection maximum_investor = project_investor_npv(
        projector, basis, maximum_coverage, investor_discount_factor,
        controls);
    const double worst_zero = zero_investor.expectation.minimum.value;
    const double worst_maximum = maximum_investor.expectation.minimum.value;

    PooledLossProtectionSummary result;
    result.underlying_success_participation_fraction =
        protection.underlying_success_participation_fraction;
    result.investor_target_worst_expected_npv_million = target;
    result.legal_support_cap_million = protection.support_cap_million;
    result.aggregate_reference_principal_million =
        aggregate_reference_principal;
    result.aggregate_covered_commitment_million =
        aggregate_reference_principal;
    result.maximum_supported_coverage_fraction = maximum_coverage;
    result.modeled_full_coverage_maximum_claim_million = maximum_loss;
    result.maximum_underlying_loss_change_million = maximum_loss_change;
    result.provider_model_limitation =
        "External support is assumed fully funded and paid in every scenario. "
        "Provider default, legal enforceability, collateral and funding cost, "
        "capital, expenses, tax, exclusions, subrogation, payment delay, and "
        "fair value are not modeled.";

    if (worst_zero >= target) {
        result.status = PooledLossProtectionSolveStatus::
            AlreadyMeetsInvestorTargetAtZero;
        result.exact_minimum_coverage_fraction = 0.0;
        result.investor_target_passing_coverage_fraction_upper_bound = 0.0;
        result.reported_coverage_fraction = 0.0;
    } else if (maximum_loss <= 0.0) {
        result.status =
            PooledLossProtectionSolveStatus::NoGrossReferenceLoss;
        result.reported_coverage_fraction = 0.0;
    } else if (maximum_coverage <= 0.0) {
        result.status = PooledLossProtectionSolveStatus::NoSupportCapacity;
        result.failing_coverage_fraction_lower_bound = 0.0;
        result.reported_coverage_fraction = 0.0;
    } else if (worst_maximum < target) {
        result.status = PooledLossProtectionSolveStatus::
            UnattainableAtMaximumSupportedCoverage;
        result.failing_coverage_fraction_lower_bound = maximum_coverage;
        result.reported_coverage_fraction = maximum_coverage;
    } else {
        double lower = 0.0;
        double upper = maximum_coverage;
        for (std::size_t iteration = 0U;
             iteration < kBisectionIterations; ++iteration) {
            const double midpoint = std::midpoint(lower, upper);
            if (midpoint == lower || midpoint == upper) {
                break;
            }
            const AmbiguityMetricProjection midpoint_projection =
                project_investor_npv(projector, basis, midpoint,
                    investor_discount_factor, controls);
            if (midpoint_projection.expectation.minimum.value >= target) {
                upper = midpoint;
            } else {
                lower = midpoint;
            }
        }
        result.failing_coverage_fraction_lower_bound = lower;
        result.investor_target_passing_coverage_fraction_upper_bound = upper;
        result.reported_coverage_fraction = upper;
        if (upper == 1.0 && worst_maximum == target) {
            result.status =
                PooledLossProtectionSolveStatus::FullCoverageRequired;
            result.exact_minimum_coverage_fraction = 1.0;
        } else if (upper == maximum_coverage && maximum_coverage < 1.0) {
            result.status = PooledLossProtectionSolveStatus::
                CertifiedSupportCapBoundaryBracket;
        } else {
            result.status = PooledLossProtectionSolveStatus::
                CertifiedInteriorBracket;
        }
    }
    result.investor_target_gap_at_maximum_supported_coverage_million =
        std::max(0.0, target - worst_maximum);

    result.zero = make_point(projector, basis, 0.0,
        protection.support_cap_million, aggregate_reference_principal, target,
        investor_discount_factor, provider_discount_factor, controls);
    result.maximum_supported = make_point(projector, basis,
        maximum_coverage,
        protection.support_cap_million, aggregate_reference_principal, target,
        investor_discount_factor, provider_discount_factor, controls);
    result.reported = make_point(projector, basis,
        result.reported_coverage_fraction,
        protection.support_cap_million, aggregate_reference_principal, target,
        investor_discount_factor, provider_discount_factor, controls);
    result.maximum_support_cap_violation_million = std::max(0.0,
        result.maximum_supported.provider_risk
                .contractual_maximum_exposure_million -
            protection.support_cap_million);
    result.scenario_probability_bounds =
        zero_investor.scenario_probability_bounds;

    result.scenarios.reserve(basis.size());
    for (const ScenarioBasis& scenario : basis) {
        const double claim = result.reported_coverage_fraction *
            scenario.gross_loss_million;
        CompensatedSum project_claim_sum;
        PooledLossProtectionScenarioResult published;
        published.scenario_id = scenario.scenario_id;
        published.underlying_npv_million = scenario.underlying_npv_million;
        published.gross_principal_loss_million =
            scenario.gross_loss_million;
        published.protection_claim_million = claim;
        published.investor_external_support_cash_million = claim;
        published.provider_external_support_cash_million = -claim;
        published.claim_present_value_to_investor_million =
            claim / investor_discount_factor;
        published.claim_present_value_to_provider_million =
            claim / provider_discount_factor;
        published.investor_npv_before_premium_million =
            scenario.underlying_npv_million +
            published.claim_present_value_to_investor_million;
        published.residual_unprotected_loss_million =
            scenario.gross_loss_million - claim;
        published.legal_support_cap_million =
            protection.support_cap_million;
        const double contractual_maximum_exposure =
            result.reported_coverage_fraction *
            aggregate_reference_principal;
        published.remaining_contractual_claim_headroom_million =
            contractual_maximum_exposure - claim;
        published.uncommitted_legal_cap_capacity_million =
            protection.support_cap_million -
            contractual_maximum_exposure;
        published.projects.reserve(scenario.projects.size());
        for (const ProjectBasis& project : scenario.projects) {
            const double project_claim =
                result.reported_coverage_fraction *
                project.gross_loss_million;
            project_claim_sum.add(
                static_cast<long double>(project_claim));
            published.projects.push_back(PooledLossProtectionProjectResult{
                project.project_id, project.gross_loss_million,
                project_claim, project_claim, -project_claim,
                project.gross_loss_million - project_claim});
        }
        const double summed_project_claim =
            to_double(project_claim_sum.value());
        result.maximum_project_claim_reconciliation_error_million =
            std::max(
                result.maximum_project_claim_reconciliation_error_million,
                std::abs(summed_project_claim - claim));
        result.maximum_two_party_settlement_cash_reconciliation_error_million =
            std::max(
                result
                    .maximum_two_party_settlement_cash_reconciliation_error_million,
                std::abs(
                    published.investor_external_support_cash_million +
                    published.provider_external_support_cash_million));
        result.maximum_support_cap_violation_million = std::max(
            result.maximum_support_cap_violation_million,
            std::max({0.0, claim - protection.support_cap_million,
                result.reported.provider_risk
                        .contractual_maximum_exposure_million -
                    protection.support_cap_million}));
        const double reconstructed = scenario.underlying_npv_million +
            published.claim_present_value_to_investor_million;
        result.maximum_combined_npv_reconstruction_error_million =
            std::max(
                result.maximum_combined_npv_reconstruction_error_million,
                std::abs(reconstructed -
                    published.investor_npv_before_premium_million));
        result.scenarios.push_back(std::move(published));
    }
    result.maximum_witness_reconciliation_error_million =
        controls.maximum_witness_error;
    result.maximum_endpoint_probability_error = std::max({
        controls.maximum_probability_error,
        result.zero.maximum_endpoint_probability_error,
        result.reported.maximum_endpoint_probability_error,
        result.maximum_supported.maximum_endpoint_probability_error});
    return result;
}

PooledLossProtectionPremiumEvaluation
evaluate_pooled_loss_protection_upfront_premium(
    const PooledLossProtectionRobustPoint& point,
    double upfront_premium_million) {
    if (!std::isfinite(upfront_premium_million) ||
        upfront_premium_million < 0.0) {
        throw std::invalid_argument(
            "pooled loss protection upfront premium must be finite and non-negative");
    }
    PooledLossProtectionPremiumEvaluation result;
    result.upfront_premium_million = upfront_premium_million;
    result.investor_month_zero_premium_cash_million =
        -upfront_premium_million;
    result.provider_month_zero_premium_cash_million =
        upfront_premium_million;
    result.investor_expected_npv_after_premium_million = shifted_range(
        point.investor_expected_npv_before_premium_million,
        -upfront_premium_million);

    const AmbiguityMetricRange& claims =
        point.provider_risk.expected_claim_present_value_million;
    result.provider_expected_npv_after_premium_million.minimum =
        claims.maximum;
    result.provider_expected_npv_after_premium_million.minimum.value =
        upfront_premium_million - claims.maximum.value;
    result.provider_expected_npv_after_premium_million.central =
        upfront_premium_million - claims.central;
    result.provider_expected_npv_after_premium_million.maximum =
        claims.minimum;
    result.provider_expected_npv_after_premium_million.maximum.value =
        upfront_premium_million - claims.minimum.value;
    if (!std::isfinite(
            result.provider_expected_npv_after_premium_million.minimum.value) ||
        !std::isfinite(
            result.provider_expected_npv_after_premium_million.central) ||
        !std::isfinite(
            result.provider_expected_npv_after_premium_million.maximum.value)) {
        throw std::overflow_error(
            "pooled loss protection provider premium translation exceeded numeric range");
    }

    result.investor_target_gap_after_premium_million = std::max(0.0,
        point.investor_target_worst_expected_npv_million -
            result.investor_expected_npv_after_premium_million.minimum.value);
    result.provider_break_even_gap_after_premium_million = std::max(0.0,
        -result.provider_expected_npv_after_premium_million.minimum.value);
    result.investor_target_is_met =
        result.investor_expected_npv_after_premium_million.minimum.value >=
        point.investor_target_worst_expected_npv_million;
    result.provider_break_even_is_met =
        result.provider_expected_npv_after_premium_million.minimum.value >=
        0.0;
    result.premium_is_within_robust_interval =
        result.investor_target_is_met && result.provider_break_even_is_met;
    result.month_zero_premium_cash_reconciliation_error_million = std::abs(
        result.investor_month_zero_premium_cash_million +
        result.provider_month_zero_premium_cash_million);
    return result;
}

} // namespace naturalehia::cellular_finance
