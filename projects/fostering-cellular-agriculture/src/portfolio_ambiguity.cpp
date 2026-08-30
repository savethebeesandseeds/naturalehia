// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

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

constexpr std::size_t kCashSourceCount = 8U;
constexpr std::size_t kMaximumScenarios = 10'000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
// 15 scalar/tail ranges plus seven source categories with nominal and PV.
constexpr std::size_t kFixedPublishedMetricRangeCount = 29U;
constexpr std::size_t kProjectPublishedMetricRangeCount = 7U;
// At most 8,000,000 doubles (about 61 MiB) may be retained solely as endpoint
// and tail-mass witnesses. The guard is evaluated from the accepted dynamic
// project and scenario dimensions before any published witness allocation.
constexpr std::size_t kMaximumPublishedWitnessWeights = 8'000'000U;
constexpr std::size_t kPublishedTailMassVectorCount = 6U;
constexpr double kWeightSumTolerance = 1.0e-12;

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

struct CanonicalAmbiguity {
    std::vector<std::string> scenario_ids{};
    std::vector<long double> lower{};
    std::vector<long double> central{};
    std::vector<long double> upper{};
    std::vector<ScenarioProbabilityBounds> published_bounds{};
    long double configured_central_sum{0.0L};
    long double lower_sum{0.0L};
    long double upper_sum{0.0L};
};

struct ExtremeMeasure {
    std::vector<long double> weights{};
    long double value{0.0L};
};

struct TailMassAllocation {
    std::vector<long double> masses{};
    long double expected_shortfall{0.0L};
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
            "portfolio ambiguity aggregation exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] double reconciliation_tolerance(double scale) noexcept {
    return 1.0e-9 + 32.0 * std::numeric_limits<double>::epsilon() *
        std::max(1.0, std::abs(scale));
}

[[nodiscard]] long double stable_sum(
    const std::vector<long double>& values) noexcept {
    CompensatedSum sum;
    for (const long double value : values) {
        sum.add(value);
    }
    return sum.value();
}

[[nodiscard]] long double weighted_average(
    const std::vector<double>& values,
    const std::vector<long double>& weights) {
    if (values.empty() || values.size() != weights.size()) {
        throw std::logic_error(
            "invalid internal portfolio ambiguity distribution");
    }
    CompensatedSum numerator;
    CompensatedSum denominator;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (!std::isfinite(values[index])) {
            throw std::logic_error(
                "portfolio ambiguity objective is non-finite");
        }
        numerator.add(weights[index] *
            static_cast<long double>(values[index]));
        denominator.add(weights[index]);
    }
    if (denominator.value() <= 0.0L) {
        throw std::logic_error(
            "portfolio ambiguity probability measure has no mass");
    }
    return numerator.value() / denominator.value();
}

[[nodiscard]] long double central_match_tolerance(
    long double first, long double second) noexcept {
    return 64.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        std::max({1.0L, std::abs(first), std::abs(second)});
}

[[nodiscard]] CanonicalAmbiguity canonicalize_ambiguity(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity) {
    validate_portfolio_config(portfolio);
    if (ambiguity.model_version != kPortfolioAmbiguityModelVersion) {
        throw std::invalid_argument(
            "portfolio ambiguity model_version does not match this engine");
    }
    if (!ambiguity.synthetic_inputs) {
        throw std::invalid_argument(
            "portfolio ambiguity v0.1 accepts synthetic inputs only");
    }
    require_safe_text(ambiguity.scenario_label, "ambiguity scenario_label");
    require_safe_text(ambiguity.source_note, "ambiguity source_note");
    if (ambiguity.scenario_probabilities.empty() ||
        ambiguity.scenario_probabilities.size() > kMaximumScenarios) {
        throw std::invalid_argument(
            "ambiguity scenario probabilities must be non-empty and bounded");
    }
    if (ambiguity.scenario_probabilities.size() !=
        portfolio.joint_scenarios.size()) {
        throw std::invalid_argument(
            "ambiguity probabilities must name every portfolio scenario exactly once");
    }
    const std::size_t project_range_count =
        kProjectPublishedMetricRangeCount * portfolio.projects.size();
    if (project_range_count >
        std::numeric_limits<std::size_t>::max() -
            kFixedPublishedMetricRangeCount) {
        throw std::invalid_argument(
            "ambiguity endpoint witness count exceeds the resource bound");
    }
    const std::size_t metric_range_count =
        kFixedPublishedMetricRangeCount + project_range_count;
    if (metric_range_count >
        (std::numeric_limits<std::size_t>::max() -
            kPublishedTailMassVectorCount) /
            2U) {
        throw std::invalid_argument(
            "ambiguity endpoint witness count exceeds the resource bound");
    }
    const std::size_t witness_vectors =
        2U * metric_range_count + kPublishedTailMassVectorCount;
    if (ambiguity.scenario_probabilities.size() >
        kMaximumPublishedWitnessWeights / witness_vectors) {
        throw std::invalid_argument(
            "ambiguity endpoint witness output exceeds the resource bound");
    }

    std::vector<const JointScenario*> ordered_portfolio_scenarios;
    ordered_portfolio_scenarios.reserve(portfolio.joint_scenarios.size());
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        ordered_portfolio_scenarios.push_back(&scenario);
    }
    std::sort(ordered_portfolio_scenarios.begin(),
        ordered_portfolio_scenarios.end(),
        [](const JointScenario* first, const JointScenario* second) {
            return first->id < second->id;
        });

    std::unordered_map<std::string, const ScenarioProbabilityBounds*>
        ambiguity_by_id;
    ambiguity_by_id.reserve(ambiguity.scenario_probabilities.size());
    for (const ScenarioProbabilityBounds& bounds :
         ambiguity.scenario_probabilities) {
        if (!is_safe_identifier(bounds.scenario_id)) {
            throw std::invalid_argument(
                "ambiguity scenario id must be a safe identifier");
        }
        if (!ambiguity_by_id.emplace(bounds.scenario_id, &bounds).second) {
            throw std::invalid_argument(
                "ambiguity scenario ids must be unique");
        }
        if (!std::isfinite(bounds.lower_weight) ||
            !std::isfinite(bounds.central_weight) ||
            !std::isfinite(bounds.upper_weight)) {
            throw std::invalid_argument(
                "ambiguity probability bounds must be finite");
        }
        if (bounds.lower_weight < 0.0 || bounds.lower_weight > 1.0 ||
            bounds.upper_weight < 0.0 || bounds.upper_weight > 1.0 ||
            bounds.central_weight < 0.0) {
            throw std::invalid_argument(
                "ambiguity probabilities are outside their supported ranges");
        }
        if (bounds.lower_weight > bounds.upper_weight) {
            throw std::invalid_argument(
                "ambiguity lower weight exceeds upper weight");
        }
    }

    CompensatedSum configured_central_sum;
    CompensatedSum portfolio_central_sum;
    CompensatedSum lower_sum;
    CompensatedSum upper_sum;
    for (const JointScenario* scenario : ordered_portfolio_scenarios) {
        const auto matching = ambiguity_by_id.find(scenario->id);
        if (matching == ambiguity_by_id.end()) {
            throw std::invalid_argument(
                "ambiguity probabilities omit a portfolio scenario");
        }
        configured_central_sum.add(static_cast<long double>(
            matching->second->central_weight));
        portfolio_central_sum.add(
            static_cast<long double>(scenario->weight));
        lower_sum.add(static_cast<long double>(
            matching->second->lower_weight));
        upper_sum.add(static_cast<long double>(
            matching->second->upper_weight));
    }
    const long double configured_sum = configured_central_sum.value();
    if (std::abs(configured_sum - 1.0L) >
        static_cast<long double>(kWeightSumTolerance)) {
        throw std::invalid_argument(
            "ambiguity central weights must sum to one within tolerance");
    }
    if (lower_sum.value() > 1.0L) {
        throw std::invalid_argument(
            "ambiguity lower weights leave no feasible probability measure");
    }
    if (upper_sum.value() < 1.0L) {
        throw std::invalid_argument(
            "ambiguity upper weights leave no feasible probability measure");
    }

    CanonicalAmbiguity canonical;
    canonical.configured_central_sum = configured_sum;
    canonical.lower_sum = lower_sum.value();
    canonical.upper_sum = upper_sum.value();
    canonical.scenario_ids.reserve(ordered_portfolio_scenarios.size());
    canonical.lower.reserve(ordered_portfolio_scenarios.size());
    canonical.central.reserve(ordered_portfolio_scenarios.size());
    canonical.upper.reserve(ordered_portfolio_scenarios.size());
    canonical.published_bounds.reserve(ordered_portfolio_scenarios.size());

    const long double portfolio_sum = portfolio_central_sum.value();
    for (const JointScenario* scenario : ordered_portfolio_scenarios) {
        const ScenarioProbabilityBounds& configured =
            *ambiguity_by_id.at(scenario->id);
        const long double normalized_central =
            static_cast<long double>(configured.central_weight) /
            configured_sum;
        const long double normalized_portfolio =
            static_cast<long double>(scenario->weight) / portfolio_sum;
        const long double lower =
            static_cast<long double>(configured.lower_weight);
        const long double upper =
            static_cast<long double>(configured.upper_weight);
        if (normalized_central <
                lower - central_match_tolerance(normalized_central, lower) ||
            normalized_central >
                upper + central_match_tolerance(normalized_central, upper)) {
            throw std::invalid_argument(
                "normalized ambiguity central weight lies outside its bounds");
        }
        if (normalized_portfolio <
                lower - central_match_tolerance(normalized_portfolio, lower) ||
            normalized_portfolio >
                upper + central_match_tolerance(normalized_portfolio, upper)) {
            throw std::invalid_argument(
                "normalized portfolio central weight lies outside ambiguity bounds");
        }
        if (std::abs(normalized_central - normalized_portfolio) >
            central_match_tolerance(
                normalized_central, normalized_portfolio)) {
            throw std::invalid_argument(
                "ambiguity central weights do not match the portfolio measure");
        }
        canonical.scenario_ids.push_back(scenario->id);
        canonical.lower.push_back(lower);
        // Use the ordinary portfolio's normalized measure after the explicit
        // checksum above, guaranteeing one canonical central interpretation.
        canonical.central.push_back(normalized_portfolio);
        canonical.upper.push_back(upper);
        canonical.published_bounds.push_back(ScenarioProbabilityBounds{
            scenario->id, configured.lower_weight,
            to_double(normalized_portfolio), configured.upper_weight});
    }
    return canonical;
}

[[nodiscard]] ExtremeMeasure extreme_expectation(
    const std::vector<double>& values,
    const CanonicalAmbiguity& ambiguity, bool maximize) {
    if (values.size() != ambiguity.scenario_ids.size()) {
        throw std::logic_error(
            "portfolio ambiguity objective has the wrong scenario count");
    }
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw std::logic_error(
                "portfolio ambiguity objective is non-finite");
        }
    }

    ExtremeMeasure result;
    result.weights = ambiguity.lower;
    long double residual = 1.0L - ambiguity.lower_sum;
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(),
        [&values, maximize](std::size_t first,
            std::size_t second) {
            if (values[first] != values[second]) {
                return maximize ? values[first] > values[second]
                                : values[first] < values[second];
            }
            return first < second;
        });

    // Equal objective values form one economically indistinguishable block.
    // Allocate a partial block pro rata to remaining probability capacity so
    // an endpoint witness cannot change merely because an atom is renamed.
    for (std::size_t begin = 0U;
         begin < order.size() && residual > 0.0L;) {
        std::size_t end = begin + 1U;
        while (end < order.size() &&
               values[order[end]] == values[order[begin]]) {
            ++end;
        }
        CompensatedSum block_capacity_sum;
        for (std::size_t position = begin; position < end; ++position) {
            const std::size_t index = order[position];
            block_capacity_sum.add(
                ambiguity.upper[index] - result.weights[index]);
        }
        const long double block_capacity = block_capacity_sum.value();
        if (block_capacity <= residual) {
            for (std::size_t position = begin; position < end; ++position) {
                const std::size_t index = order[position];
                result.weights[index] = ambiguity.upper[index];
            }
            residual -= block_capacity;
        } else if (block_capacity > 0.0L) {
            const long double fraction = residual / block_capacity;
            for (std::size_t position = begin; position < end; ++position) {
                const std::size_t index = order[position];
                const long double capacity =
                    ambiguity.upper[index] - result.weights[index];
                result.weights[index] += fraction * capacity;
            }
            residual = 0.0L;
        }
        begin = end;
    }
    // Sequential capacity subtraction can leave a positive machine residual
    // even when the compensated mass of the bounded witness is exactly one.
    // Validate the constructed measure itself instead of treating that scratch
    // residual as economic infeasibility.
    const long double mass_error =
        std::abs(stable_sum(result.weights) - 1.0L);
    if (mass_error > static_cast<long double>(kWeightSumTolerance)) {
        throw std::logic_error(
            "validated ambiguity set could not allocate unit probability mass");
    }
    result.value = weighted_average(values, result.weights);
    return result;
}

[[nodiscard]] TailMassAllocation allocate_upper_tail(
    const std::vector<double>& values,
    const std::vector<long double>& weights,
    const std::vector<std::string>& scenario_ids,
    long double tail_probability) {
    if (values.empty() || values.size() != weights.size() ||
        values.size() != scenario_ids.size() ||
        tail_probability <= 0.0L || tail_probability > 1.0L) {
        throw std::logic_error(
            "invalid internal ambiguity expected-shortfall request");
    }
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(),
        [&values](std::size_t first, std::size_t second) {
            if (values[first] != values[second]) {
                return values[first] > values[second];
            }
            return first < second;
        });

    const long double requested = tail_probability * stable_sum(weights);
    long double remaining = requested;
    CompensatedSum tail_total;
    TailMassAllocation result;
    result.masses.assign(values.size(), 0.0L);
    // Fractional boundary mass is spread pro rata over every atom with the
    // same aggregate loss. This preserves additive project attribution under
    // scenario reordering or renaming.
    for (std::size_t begin = 0U;
         begin < order.size() && remaining > 0.0L;) {
        std::size_t end = begin + 1U;
        while (end < order.size() &&
               values[order[end]] == values[order[begin]]) {
            ++end;
        }
        CompensatedSum block_weight_sum;
        for (std::size_t position = begin; position < end; ++position) {
            block_weight_sum.add(weights[order[position]]);
        }
        const long double block_weight = block_weight_sum.value();
        const long double included = std::min(remaining, block_weight);
        const long double fraction = block_weight > 0.0L
            ? included / block_weight
            : 0.0L;
        for (std::size_t position = begin; position < end; ++position) {
            const std::size_t index = order[position];
            result.masses[index] = fraction * weights[index];
            tail_total.add(result.masses[index] *
                static_cast<long double>(values[index]));
        }
        remaining -= included;
        begin = end;
    }
    if (remaining > static_cast<long double>(kWeightSumTolerance)) {
        throw std::logic_error(
            "ambiguity expected-shortfall tail has insufficient mass");
    }
    result.expected_shortfall = tail_total.value() / requested;
    return result;
}

[[nodiscard]] long double upper_expected_shortfall(
    const std::vector<double>& values,
    const std::vector<long double>& weights,
    const std::vector<std::string>& scenario_ids,
    long double tail_probability) {
    return allocate_upper_tail(values, weights, scenario_ids,
        tail_probability).expected_shortfall;
}

void update_probability_error(double& maximum_error,
    const std::vector<double>& weights,
    const CanonicalAmbiguity& ambiguity) {
    if (weights.size() != ambiguity.lower.size()) {
        throw std::logic_error(
            "ambiguity endpoint witness has the wrong scenario count");
    }
    CompensatedSum weight_sum;
    long double error = 0.0L;
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        const long double weight = static_cast<long double>(weights[index]);
        weight_sum.add(weight);
        error = std::max(error,
            std::max(0.0L, ambiguity.lower[index] - weight));
        error = std::max(error,
            std::max(0.0L, weight - ambiguity.upper[index]));
    }
    error = std::max(error, std::abs(weight_sum.value() - 1.0L));
    maximum_error = std::max(maximum_error, to_double(error));
    if (error > static_cast<long double>(kWeightSumTolerance)) {
        throw std::logic_error(
            "ambiguity endpoint probability reconciliation failed");
    }
}

[[nodiscard]] AmbiguityEndpoint make_endpoint(
    const std::vector<long double>& weights, long double value,
    const CanonicalAmbiguity& ambiguity, double& maximum_probability_error) {
    AmbiguityEndpoint endpoint;
    endpoint.value = to_double(value);
    endpoint.scenario_weights.reserve(weights.size());
    for (const long double weight : weights) {
        endpoint.scenario_weights.push_back(to_double(weight));
    }
    update_probability_error(maximum_probability_error,
        endpoint.scenario_weights, ambiguity);
    return endpoint;
}

void reconcile_central(double calculated, double published,
    double& maximum_error) {
    const double error = std::abs(calculated - published);
    maximum_error = std::max(maximum_error, error);
    if (!std::isfinite(error) ||
        error > reconciliation_tolerance(
            std::max(std::abs(calculated), std::abs(published)))) {
        throw std::logic_error(
            "ambiguity central metric does not reconcile to portfolio summary");
    }
}

void require_central_within_range(const AmbiguityMetricRange& range) {
    const double scale = std::max({std::abs(range.minimum.value),
        std::abs(range.central), std::abs(range.maximum.value)});
    const double tolerance = reconciliation_tolerance(scale);
    if (range.minimum.value > range.maximum.value + tolerance ||
        range.central < range.minimum.value - tolerance ||
        range.central > range.maximum.value + tolerance) {
        throw std::logic_error(
            "ambiguity metric range does not contain its feasible central value");
    }
}

[[nodiscard]] AmbiguityMetricRange bound_expectation(
    const std::vector<double>& values, double published_central,
    const CanonicalAmbiguity& ambiguity, double& maximum_probability_error,
    double& maximum_central_error) {
    const ExtremeMeasure minimum =
        extreme_expectation(values, ambiguity, false);
    const ExtremeMeasure maximum =
        extreme_expectation(values, ambiguity, true);
    const double calculated_central =
        to_double(weighted_average(values, ambiguity.central));
    reconcile_central(
        calculated_central, published_central, maximum_central_error);

    AmbiguityMetricRange range;
    range.minimum = make_endpoint(minimum.weights, minimum.value,
        ambiguity, maximum_probability_error);
    range.central = published_central;
    range.maximum = make_endpoint(maximum.weights, maximum.value,
        ambiguity, maximum_probability_error);
    require_central_within_range(range);
    return range;
}

[[nodiscard]] AmbiguityMetricRange bound_expected_shortfall(
    const std::vector<double>& values, double published_central,
    long double tail_probability, const CanonicalAmbiguity& ambiguity,
    double& maximum_probability_error, double& maximum_central_error) {
    // The low-value and high-value greedy measures are respectively the
    // first-order-stochastically minimal and maximal members of the bounded
    // simplex. Expected shortfall is monotone under that order, so evaluating
    // the ordinary fractional tail at these witnesses gives exact endpoints.
    const ExtremeMeasure minimum_measure =
        extreme_expectation(values, ambiguity, false);
    const ExtremeMeasure maximum_measure =
        extreme_expectation(values, ambiguity, true);
    const long double minimum = upper_expected_shortfall(values,
        minimum_measure.weights, ambiguity.scenario_ids, tail_probability);
    const long double maximum = upper_expected_shortfall(values,
        maximum_measure.weights, ambiguity.scenario_ids, tail_probability);
    const double calculated_central = to_double(upper_expected_shortfall(
        values, ambiguity.central, ambiguity.scenario_ids,
        tail_probability));
    reconcile_central(
        calculated_central, published_central, maximum_central_error);

    AmbiguityMetricRange range;
    range.minimum = make_endpoint(minimum_measure.weights, minimum,
        ambiguity, maximum_probability_error);
    range.central = published_central;
    range.maximum = make_endpoint(maximum_measure.weights, maximum,
        ambiguity, maximum_probability_error);
    require_central_within_range(range);
    return range;
}

template <typename Accessor>
[[nodiscard]] std::vector<double> scenario_values(
    const PortfolioSummary& summary, Accessor accessor) {
    std::vector<double> values;
    values.reserve(summary.scenarios.size());
    for (const JointScenarioResult& scenario : summary.scenarios) {
        values.push_back(accessor(scenario));
    }
    return values;
}

[[nodiscard]] std::vector<double> canonical_metric_values(
    const std::vector<AmbiguityScenarioMetricValue>& scenario_values,
    const CanonicalAmbiguity& ambiguity) {
    if (scenario_values.size() != ambiguity.scenario_ids.size()) {
        throw std::invalid_argument(
            "ambiguity metric values must name every portfolio scenario exactly once");
    }

    std::unordered_map<std::string, double> value_by_id;
    value_by_id.reserve(scenario_values.size());
    for (const AmbiguityScenarioMetricValue& scenario : scenario_values) {
        if (!is_safe_identifier(scenario.scenario_id)) {
            throw std::invalid_argument(
                "ambiguity metric scenario id must be a safe identifier");
        }
        if (!std::isfinite(scenario.value)) {
            throw std::invalid_argument(
                "ambiguity metric scenario values must be finite");
        }
        if (!value_by_id.emplace(scenario.scenario_id, scenario.value).second) {
            throw std::invalid_argument(
                "ambiguity metric scenario ids must be unique");
        }
    }

    std::vector<double> ordered_values;
    ordered_values.reserve(ambiguity.scenario_ids.size());
    for (const std::string& scenario_id : ambiguity.scenario_ids) {
        const auto matching = value_by_id.find(scenario_id);
        if (matching == value_by_id.end()) {
            throw std::invalid_argument(
                "ambiguity metric values do not match the portfolio scenario ids");
        }
        ordered_values.push_back(matching->second);
    }
    return ordered_values;
}

[[nodiscard]] std::vector<long double> endpoint_weights(
    const AmbiguityEndpoint& endpoint, std::size_t expected_size) {
    if (endpoint.scenario_weights.size() != expected_size) {
        throw std::logic_error(
            "pool-tail attribution endpoint has the wrong scenario count");
    }
    std::vector<long double> weights;
    weights.reserve(expected_size);
    for (const double weight : endpoint.scenario_weights) {
        if (!std::isfinite(weight) || weight < 0.0) {
            throw std::logic_error(
                "pool-tail attribution endpoint has an invalid probability");
        }
        weights.push_back(static_cast<long double>(weight));
    }
    return weights;
}

struct PublishedTailColumn {
    std::vector<double> masses{};
    std::vector<double> project_contributions{};
    double expected_shortfall{0.0};
};

[[nodiscard]] PublishedTailColumn publish_tail_column(
    const PortfolioSummary& portfolio, const std::vector<double>& pool_losses,
    const CanonicalAmbiguity& ambiguity,
    const std::vector<long double>& probability_weights,
    long double tail_probability, double published_expected_shortfall,
    double& maximum_tail_mass_error,
    double& maximum_contribution_error) {
    const TailMassAllocation allocation = allocate_upper_tail(pool_losses,
        probability_weights, ambiguity.scenario_ids, tail_probability);
    const double calculated_es = to_double(allocation.expected_shortfall);
    const double es_error = std::abs(calculated_es - published_expected_shortfall);
    const double es_tolerance = reconciliation_tolerance(
        std::max(std::abs(calculated_es),
            std::abs(published_expected_shortfall)));
    if (!std::isfinite(es_error) || es_error > es_tolerance) {
        throw std::logic_error(
            "pool-tail attribution does not reproduce published pool ES");
    }

    PublishedTailColumn result;
    result.expected_shortfall = published_expected_shortfall;
    result.masses.reserve(allocation.masses.size());
    CompensatedSum mass_sum;
    long double tail_error = 0.0L;
    for (std::size_t index = 0U; index < allocation.masses.size(); ++index) {
        const long double mass = allocation.masses[index];
        mass_sum.add(mass);
        tail_error = std::max(tail_error, std::max(0.0L, -mass));
        tail_error = std::max(tail_error,
            std::max(0.0L, mass - probability_weights[index]));
        result.masses.push_back(to_double(mass));
    }
    tail_error = std::max(tail_error,
        std::abs(mass_sum.value() - tail_probability));
    maximum_tail_mass_error = std::max(
        maximum_tail_mass_error, to_double(tail_error));
    if (tail_error > static_cast<long double>(kWeightSumTolerance)) {
        throw std::logic_error(
            "pool-tail attribution mass does not reconcile");
    }

    result.project_contributions.reserve(portfolio.projects.size());
    CompensatedSum contribution_sum;
    for (std::size_t project_index = 0U;
         project_index < portfolio.projects.size(); ++project_index) {
        CompensatedSum contribution;
        for (std::size_t scenario_index = 0U;
             scenario_index < portfolio.scenarios.size(); ++scenario_index) {
            if (portfolio.scenarios[scenario_index].projects.size() !=
                portfolio.projects.size()) {
                throw std::logic_error(
                    "pool-tail attribution project taxonomy is incomplete");
            }
            contribution.add(allocation.masses[scenario_index] *
                static_cast<long double>(portfolio.scenarios[scenario_index]
                    .projects[project_index].principal_loss_million));
        }
        const double published = to_double(
            contribution.value() / tail_probability);
        result.project_contributions.push_back(published);
        contribution_sum.add(static_cast<long double>(published));
    }
    const double contribution_error = std::abs(
        to_double(contribution_sum.value()) - published_expected_shortfall);
    maximum_contribution_error = std::max(
        maximum_contribution_error, contribution_error);
    if (!std::isfinite(contribution_error) ||
        contribution_error > reconciliation_tolerance(
            published_expected_shortfall)) {
        throw std::logic_error(
            "project pool-tail contributions do not reconcile to pool ES");
    }
    return result;
}

[[nodiscard]] PoolLossTailAttribution make_pool_loss_tail_attribution(
    const PortfolioSummary& portfolio, const std::vector<double>& pool_losses,
    const CanonicalAmbiguity& ambiguity, const AmbiguityMetricRange& es_range,
    long double tail_probability) {
    PoolLossTailAttribution result;
    result.tail_probability = to_double(tail_probability);
    const std::vector<long double> minimum_weights = endpoint_weights(
        es_range.minimum, ambiguity.scenario_ids.size());
    const std::vector<long double> maximum_weights = endpoint_weights(
        es_range.maximum, ambiguity.scenario_ids.size());
    const PublishedTailColumn minimum = publish_tail_column(portfolio,
        pool_losses, ambiguity, minimum_weights, tail_probability,
        es_range.minimum.value,
        result.maximum_tail_mass_reconciliation_error,
        result.maximum_project_contribution_reconciliation_error_million);
    const PublishedTailColumn central = publish_tail_column(portfolio,
        pool_losses, ambiguity, ambiguity.central, tail_probability,
        es_range.central, result.maximum_tail_mass_reconciliation_error,
        result.maximum_project_contribution_reconciliation_error_million);
    const PublishedTailColumn maximum = publish_tail_column(portfolio,
        pool_losses, ambiguity, maximum_weights, tail_probability,
        es_range.maximum.value,
        result.maximum_tail_mass_reconciliation_error,
        result.maximum_project_contribution_reconciliation_error_million);

    result.minimum_pool_es_tail_mass_weights = minimum.masses;
    result.central_tail_mass_weights = central.masses;
    result.maximum_pool_es_tail_mass_weights = maximum.masses;
    result.projects.reserve(portfolio.projects.size());
    for (std::size_t project_index = 0U;
         project_index < portfolio.projects.size(); ++project_index) {
        const ProjectPortfolioSummary& project =
            portfolio.projects[project_index];
        result.projects.push_back(ProjectPoolLossTailContribution{
            project.project_id,
            minimum.project_contributions[project_index],
            central.project_contributions[project_index],
            maximum.project_contributions[project_index]});
    }
    return result;
}

} // namespace

struct PortfolioAmbiguityProjector::State {
    explicit State(CanonicalAmbiguity canonical_ambiguity)
        : canonical(std::move(canonical_ambiguity)) {}

    CanonicalAmbiguity canonical{};
};

PortfolioAmbiguityProjector::PortfolioAmbiguityProjector(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity)
    : state_(std::make_shared<const State>(
          canonicalize_ambiguity(portfolio, ambiguity))) {}

AmbiguityMetricProjection PortfolioAmbiguityProjector::project_expectation(
    const std::vector<AmbiguityScenarioMetricValue>& scenario_values) const {
    if (!state_) {
        throw std::logic_error(
            "portfolio ambiguity projector has no prepared state");
    }
    const CanonicalAmbiguity& canonical = state_->canonical;
    const std::vector<double> ordered_values =
        canonical_metric_values(scenario_values, canonical);
    const double central =
        to_double(weighted_average(ordered_values, canonical.central));

    AmbiguityMetricProjection projection;
    projection.scenario_probability_bounds = canonical.published_bounds;
    double unused_central_error = 0.0;
    projection.expectation = bound_expectation(ordered_values, central,
        canonical, projection.maximum_endpoint_probability_error,
        unused_central_error);
    return projection;
}

AmbiguityUpperExpectedShortfallProjection
PortfolioAmbiguityProjector::project_upper_expected_shortfall(
    const std::vector<AmbiguityScenarioMetricValue>& scenario_values,
    double tail_probability) const {
    if (!state_) {
        throw std::logic_error(
            "portfolio ambiguity projector has no prepared state");
    }
    if (!std::isfinite(tail_probability) || tail_probability <= 0.0 ||
        tail_probability > 1.0) {
        throw std::invalid_argument(
            "ambiguity expected-shortfall tail probability must be finite and in (0, 1]");
    }

    const CanonicalAmbiguity& canonical = state_->canonical;
    const std::vector<double> ordered_values =
        canonical_metric_values(scenario_values, canonical);
    const long double requested_tail_probability =
        static_cast<long double>(tail_probability);
    const double central = to_double(upper_expected_shortfall(ordered_values,
        canonical.central, canonical.scenario_ids,
        requested_tail_probability));

    AmbiguityUpperExpectedShortfallProjection projection;
    projection.scenario_probability_bounds = canonical.published_bounds;
    projection.tail_probability = tail_probability;
    double unused_central_error = 0.0;
    projection.upper_expected_shortfall = bound_expected_shortfall(
        ordered_values, central, requested_tail_probability, canonical,
        projection.maximum_endpoint_probability_error,
        unused_central_error);
    return projection;
}

void validate_portfolio_ambiguity_config(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity) {
    (void)canonicalize_ambiguity(portfolio, ambiguity);
}

PortfolioAmbiguitySummary evaluate_portfolio_ambiguity(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity) {
    const CanonicalAmbiguity canonical =
        canonicalize_ambiguity(portfolio, ambiguity);
    PortfolioSummary central = evaluate_portfolio(portfolio);
    if (central.scenarios.size() != canonical.scenario_ids.size()) {
        throw std::logic_error(
            "portfolio ambiguity lost a validated scenario");
    }
    for (std::size_t index = 0U; index < central.scenarios.size(); ++index) {
        if (central.scenarios[index].scenario_id !=
            canonical.scenario_ids[index]) {
            throw std::logic_error(
                "portfolio ambiguity scenario order is inconsistent");
        }
        if (central.scenarios[index].return_sources.size() !=
            kCashSourceCount) {
            throw std::logic_error(
                "portfolio ambiguity source taxonomy is incomplete");
        }
    }
    if (central.expected_return_sources.size() != kCashSourceCount) {
        throw std::logic_error(
            "portfolio ambiguity expected source taxonomy is incomplete");
    }

    PortfolioAmbiguitySummary result;
    result.scenario_probability_bounds = canonical.published_bounds;
    result.configured_central_weight_sum =
        to_double(canonical.configured_central_sum);
    result.lower_bound_sum = to_double(canonical.lower_sum);
    result.upper_bound_sum = to_double(canonical.upper_sum);

    const std::vector<double> losses = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.principal_loss_million;
        });
    const std::vector<double> total_draws = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.total_draws_million;
        });
    const std::vector<double> total_receipts = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.total_receipts_million;
        });
    const std::vector<double> total_pool_costs = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.total_pool_costs_million;
        });
    const std::vector<double> outstanding_principal = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.outstanding_principal_million;
        });
    const std::vector<double> npvs = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.npv_million;
        });
    const std::vector<double> impairment_events = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.principal_loss_million > 0.0 ? 1.0 : 0.0;
        });
    const std::vector<double> negative_npv_events = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.npv_million < 0.0 ? 1.0 : 0.0;
        });
    const std::vector<double> peak_draws = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.peak_same_month_draw_million;
        });
    const std::vector<double> peak_funding_needs = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.peak_same_month_funding_need_million;
        });
    const std::vector<double> peak_outlays = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return scenario.peak_cumulative_net_outlay_million;
        });
    const std::vector<double> npv_shortfalls = scenario_values(central,
        [](const JointScenarioResult& scenario) {
            return std::max(0.0, -scenario.npv_million);
        });
    const double central_total_receipts =
        to_double(weighted_average(total_receipts, canonical.central));
    const double central_total_pool_costs =
        to_double(weighted_average(total_pool_costs, canonical.central));
    CompensatedSum central_source_total;
    for (const ReturnSourceTotal& source : central.expected_return_sources) {
        central_source_total.add(
            static_cast<long double>(source.nominal_million));
    }
    reconcile_central(to_double(central_source_total.value()),
        central_total_receipts,
        result.maximum_central_metric_reconciliation_error);

    result.expected_total_draws_million = bound_expectation(total_draws,
        central.total_draws_million.mean, canonical,
        result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_total_receipts_million = bound_expectation(total_receipts,
        central_total_receipts,
        canonical, result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_total_pool_costs_million = bound_expectation(
        total_pool_costs, central_total_pool_costs,
        canonical, result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_outstanding_principal_million = bound_expectation(
        outstanding_principal, central.outstanding_principal_million.mean,
        canonical, result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_principal_loss_million = bound_expectation(losses,
        central.principal_loss_million.mean, canonical,
        result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_npv_million = bound_expectation(npvs,
        central.npv_million.mean, canonical,
        result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.principal_impairment_probability = bound_expectation(
        impairment_events, central.principal_impairment_probability,
        canonical, result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.negative_npv_probability = bound_expectation(
        negative_npv_events, central.negative_npv_probability,
        canonical, result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_peak_same_month_draw_million = bound_expectation(
        peak_draws, central.peak_same_month_draw_million.mean, canonical,
        result.maximum_endpoint_probability_error,
        result.maximum_central_metric_reconciliation_error);
    result.expected_peak_same_month_funding_need_million =
        bound_expectation(peak_funding_needs,
            central.peak_same_month_funding_need_million.mean, canonical,
            result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
    result.expected_peak_cumulative_net_outlay_million =
        bound_expectation(peak_outlays,
            central.peak_cumulative_net_outlay_million.mean, canonical,
            result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);

    result.projects.reserve(central.projects.size());
    for (std::size_t project_index = 0U;
         project_index < central.projects.size(); ++project_index) {
        const ProjectPortfolioSummary& central_project =
            central.projects[project_index];
        std::vector<double> project_draws;
        std::vector<double> project_receipts;
        std::vector<double> project_outstanding;
        std::vector<double> project_losses;
        std::vector<double> project_npvs;
        std::vector<double> project_impairment_events;
        std::vector<double> project_negative_npv_events;
        project_draws.reserve(central.scenarios.size());
        project_receipts.reserve(central.scenarios.size());
        project_outstanding.reserve(central.scenarios.size());
        project_losses.reserve(central.scenarios.size());
        project_npvs.reserve(central.scenarios.size());
        project_impairment_events.reserve(central.scenarios.size());
        project_negative_npv_events.reserve(central.scenarios.size());
        for (const JointScenarioResult& scenario : central.scenarios) {
            if (scenario.projects.size() != central.projects.size()) {
                throw std::logic_error(
                    "portfolio ambiguity project taxonomy is incomplete");
            }
            const ProjectPathResult& project =
                scenario.projects[project_index];
            if (project.project_id != central_project.project_id) {
                throw std::logic_error(
                    "portfolio ambiguity project order is inconsistent");
            }
            project_draws.push_back(project.total_draws_million);
            project_receipts.push_back(project.total_receipts_million);
            project_outstanding.push_back(
                project.outstanding_principal_million);
            project_losses.push_back(project.principal_loss_million);
            project_npvs.push_back(project.npv_before_pool_costs_million);
            project_impairment_events.push_back(
                project.principal_loss_million > 0.0 ? 1.0 : 0.0);
            project_negative_npv_events.push_back(
                project.npv_before_pool_costs_million < 0.0 ? 1.0 : 0.0);
        }

        ProjectAmbiguitySummary bounded;
        bounded.project_id = central_project.project_id;
        bounded.expected_total_draws_million = bound_expectation(
            project_draws, central_project.expected_draws_million, canonical,
            result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded.expected_total_receipts_million = bound_expectation(
            project_receipts, central_project.expected_receipts_million,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded.expected_outstanding_principal_million = bound_expectation(
            project_outstanding,
            central_project.expected_outstanding_principal_million,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded.expected_realized_principal_loss_million = bound_expectation(
            project_losses, central_project.expected_principal_loss_million,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded.expected_npv_before_pool_costs_million = bound_expectation(
            project_npvs, central_project.expected_npv_before_pool_costs_million,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded.principal_impairment_probability = bound_expectation(
            project_impairment_events,
            central_project.principal_impairment_probability, canonical,
            result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded.negative_npv_probability = bound_expectation(
            project_negative_npv_events,
            central_project.negative_npv_probability, canonical,
            result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        result.projects.push_back(std::move(bounded));
    }

    result.expected_return_sources.reserve(kCashSourceCount);
    for (std::size_t source = 0U; source < kCashSourceCount; ++source) {
        const PortfolioCashSource source_kind =
            static_cast<PortfolioCashSource>(source);
        if (central.expected_return_sources[source].source != source_kind) {
            throw std::logic_error(
                "portfolio ambiguity expected source order is inconsistent");
        }
        const std::vector<double> nominal = scenario_values(central,
            [source, source_kind](const JointScenarioResult& scenario) {
                if (scenario.return_sources[source].source != source_kind) {
                    throw std::logic_error(
                        "portfolio ambiguity scenario source order is inconsistent");
                }
                return scenario.return_sources[source].nominal_million;
            });
        const std::vector<double> present_value = scenario_values(central,
            [source](const JointScenarioResult& scenario) {
                return scenario.return_sources[source].present_value_million;
            });
        AmbiguityReturnSourceTotal bounded_source;
        bounded_source.source = source_kind;
        bounded_source.nominal_million = bound_expectation(nominal,
            central.expected_return_sources[source].nominal_million,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        bounded_source.present_value_million = bound_expectation(present_value,
            central.expected_return_sources[source].present_value_million,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
        result.expected_return_sources.push_back(std::move(bounded_source));
    }

    result.principal_loss_expected_shortfall_95_million =
        bound_expected_shortfall(losses,
            central.principal_loss_million.expected_shortfall_95, 0.05L,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
    result.principal_loss_expected_shortfall_99_million =
        bound_expected_shortfall(losses,
            central.principal_loss_million.expected_shortfall_99, 0.01L,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
    result.npv_shortfall_expected_shortfall_95_million =
        bound_expected_shortfall(npv_shortfalls,
            central.npv_shortfall_million.expected_shortfall_95, 0.05L,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);
    result.npv_shortfall_expected_shortfall_99_million =
        bound_expected_shortfall(npv_shortfalls,
            central.npv_shortfall_million.expected_shortfall_99, 0.01L,
            canonical, result.maximum_endpoint_probability_error,
            result.maximum_central_metric_reconciliation_error);

    result.principal_loss_tail_attribution_95 =
        make_pool_loss_tail_attribution(central, losses, canonical,
            result.principal_loss_expected_shortfall_95_million, 0.05L);
    result.principal_loss_tail_attribution_99 =
        make_pool_loss_tail_attribution(central, losses, canonical,
            result.principal_loss_expected_shortfall_99_million, 0.01L);

    result.central_portfolio = std::move(central);
    return result;
}

} // namespace naturalehia::cellular_finance
