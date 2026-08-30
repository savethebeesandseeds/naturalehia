// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/probability_polytope.hpp>

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumScenariosWithEvents = 512U;
constexpr std::size_t kMaximumEvents = 256U;
constexpr std::size_t kMaximumEventMemberships = 65'536U;
constexpr std::size_t kMaximumDenseTableauCells = 2'000'000U;
constexpr std::size_t kMaximumSimplexPivots = 100'000U;
constexpr std::size_t kMaximumExpectedShortfallAggregatePivots = 2'000'000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr long double kMinimumEventTailProbability = 1.0e-6L;
constexpr long double kWeightSumTolerance = 1.0e-12L;
constexpr long double kPublishedConstraintTolerance = 1.0e-10L;
constexpr long double kSimplexTolerance =
    128.0L * std::numeric_limits<long double>::epsilon();

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

struct CanonicalEvent {
    ProbabilityEventConstraint published{};
    std::vector<std::size_t> member_indices{};
};

struct CanonicalPolytope {
    std::vector<std::string> scenario_ids{};
    std::vector<long double> lower{};
    std::vector<long double> central{};
    std::vector<long double> upper{};
    std::vector<ProbabilityPolytopeScenario> published_scenarios{};
    std::vector<CanonicalEvent> events{};
    long double lower_sum{0.0L};
    long double central_sum{0.0L};
    long double upper_sum{0.0L};
};

struct SimplexResult {
    std::vector<long double> variables{};
    long double objective{0.0L};
    long double reduced_cost_violation{0.0L};
    std::size_t pivot_count{0U};
};

struct ScaledObjective {
    std::vector<long double> coefficients{};
    long double offset{0.0L};
    long double scale{1.0L};
};

struct FractionalTailAllocation {
    std::vector<double> masses{};
    long double expected_shortfall{0.0L};
    double boundary_value{0.0};
};

struct ThresholdMinimumSolution {
    SimplexResult simplex{};
    long double value{0.0L};
    long double threshold_scaled{0.0L};
    double threshold{0.0};
    long double optimality_residual{0.0L};
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
            "probability-polytope aggregation exceeded numeric range");
    }
    return converted;
}

[[nodiscard]] long double stable_sum(
    const std::vector<long double>& values) noexcept {
    CompensatedSum sum;
    for (const long double value : values) {
        sum.add(value);
    }
    return sum.value();
}

[[nodiscard]] long double match_tolerance(
    long double first, long double second) noexcept {
    return 64.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        std::max({1.0L, std::abs(first), std::abs(second)});
}

[[nodiscard]] long double objective_tolerance(long double scale) noexcept {
    return 1.0e-9L +
        256.0L *
            static_cast<long double>(std::numeric_limits<double>::epsilon()) *
            std::max(1.0L, std::abs(scale));
}

[[nodiscard]] PortfolioAmbiguityConfig make_box_ambiguity(
    const ProbabilityPolytopeConfig& probability_polytope) {
    PortfolioAmbiguityConfig ambiguity;
    ambiguity.model_version = std::string(kPortfolioAmbiguityModelVersion);
    ambiguity.scenario_label = probability_polytope.scenario_label;
    ambiguity.source_note = probability_polytope.source_note;
    ambiguity.synthetic_inputs = probability_polytope.synthetic_inputs;
    ambiguity.scenario_probabilities.reserve(
        probability_polytope.scenario_probabilities.size());
    for (const ProbabilityPolytopeScenario& scenario :
         probability_polytope.scenario_probabilities) {
        ambiguity.scenario_probabilities.push_back(ScenarioProbabilityBounds{
            scenario.scenario_id, scenario.lower_weight,
            scenario.central_weight, scenario.upper_weight});
    }
    return ambiguity;
}

[[nodiscard]] std::size_t constraint_row_count(
    std::size_t scenarios, std::size_t events) {
    if (scenarios >
            (std::numeric_limits<std::size_t>::max() - 2U) / 2U ||
        events >
            (std::numeric_limits<std::size_t>::max() - 2U * scenarios - 2U) /
                2U) {
        throw std::runtime_error(
            "probability-polytope tableau dimensions exceed the resource guard");
    }
    return 2U * scenarios + 2U * events + 2U;
}

[[nodiscard]] std::size_t lifted_tail_constraint_row_count(
    std::size_t scenarios, std::size_t events) {
    if (scenarios >
            (std::numeric_limits<std::size_t>::max() - 4U) / 2U ||
        events >
            (std::numeric_limits<std::size_t>::max() - 2U * scenarios - 4U) /
                2U) {
        throw std::runtime_error(
            "probability-polytope lifted tableau dimensions exceed the resource guard");
    }
    return 2U * scenarios + 2U * events + 4U;
}

[[nodiscard]] std::size_t checked_tableau_cell_count(
    std::size_t variables, std::size_t constraint_rows) {
    if (variables > std::numeric_limits<std::size_t>::max() - 2U ||
        constraint_rows > std::numeric_limits<std::size_t>::max() - 2U) {
        throw std::runtime_error(
            "probability-polytope tableau dimensions exceed the resource guard");
    }
    const std::size_t rows = constraint_rows + 2U;
    const std::size_t columns = variables + 2U;
    if (rows > kMaximumDenseTableauCells / columns) {
        throw std::runtime_error(
            "probability-polytope dense tableau exceeds 2000000 long-double cells");
    }
    return rows * columns;
}

void require_tableau_resource_bound(
    std::size_t scenarios, std::size_t events) {
    (void)checked_tableau_cell_count(
        scenarios, constraint_row_count(scenarios, events));
}

[[nodiscard]] CanonicalPolytope canonicalize_polytope(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope) {
    validate_portfolio_config(portfolio);
    if (probability_polytope.model_version !=
        kProbabilityPolytopeModelVersion) {
        throw std::invalid_argument(
            "probability-polytope model_version does not match this engine");
    }
    if (!probability_polytope.synthetic_inputs) {
        throw std::invalid_argument(
            "probability-polytope v0.2 accepts synthetic candidate inputs only");
    }
    require_safe_text(probability_polytope.scenario_label,
        "probability-polytope scenario_label");
    require_safe_text(probability_polytope.source_note,
        "probability-polytope source_note");
    if (probability_polytope.scenario_probabilities.empty()) {
        throw std::invalid_argument(
            "probability-polytope scenarios must be non-empty");
    }
    if (probability_polytope.scenario_probabilities.size() !=
        portfolio.joint_scenarios.size()) {
        throw std::invalid_argument(
            "probability-polytope scenarios must name every portfolio scenario exactly once");
    }

    const bool has_events = !probability_polytope.events.empty();
    if (has_events && probability_polytope.scenario_probabilities.size() >
            kMaximumScenariosWithEvents) {
        throw std::runtime_error(
            "event-constrained probability polytope exceeds the 512-scenario solver guard");
    }
    if (probability_polytope.events.size() > kMaximumEvents) {
        throw std::runtime_error(
            "event-constrained probability polytope exceeds the 256-event solver guard");
    }
    if (has_events) {
        require_tableau_resource_bound(
            probability_polytope.scenario_probabilities.size(),
            probability_polytope.events.size());
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

    std::unordered_map<std::string, const ProbabilityPolytopeScenario*>
        configured_by_id;
    configured_by_id.reserve(
        probability_polytope.scenario_probabilities.size());
    for (const ProbabilityPolytopeScenario& configured :
         probability_polytope.scenario_probabilities) {
        if (!is_safe_identifier(configured.scenario_id)) {
            throw std::invalid_argument(
                "probability-polytope scenario id must be a safe identifier");
        }
        if (!configured_by_id.emplace(
                configured.scenario_id, &configured).second) {
            throw std::invalid_argument(
                "probability-polytope scenario ids must be unique");
        }
        if (!std::isfinite(configured.lower_weight) ||
            !std::isfinite(configured.central_weight) ||
            !std::isfinite(configured.upper_weight)) {
            throw std::invalid_argument(
                "probability-polytope scenario probabilities must be finite");
        }
        if (configured.lower_weight < 0.0 ||
            configured.central_weight < 0.0 ||
            configured.upper_weight < 0.0 ||
            configured.lower_weight > 1.0 ||
            configured.central_weight > 1.0 ||
            configured.upper_weight > 1.0) {
            throw std::invalid_argument(
                "probability-polytope scenario probabilities must be in [0, 1]");
        }
        if (configured.lower_weight > configured.central_weight ||
            configured.central_weight > configured.upper_weight) {
            throw std::invalid_argument(
                "probability-polytope scenario bounds do not contain their central weight");
        }
    }

    CompensatedSum configured_central_sum;
    CompensatedSum portfolio_central_sum;
    CompensatedSum lower_sum;
    CompensatedSum upper_sum;
    for (const JointScenario* scenario : ordered_portfolio_scenarios) {
        const auto matching = configured_by_id.find(scenario->id);
        if (matching == configured_by_id.end()) {
            throw std::invalid_argument(
                "probability-polytope scenarios omit a portfolio scenario");
        }
        configured_central_sum.add(static_cast<long double>(
            matching->second->central_weight));
        portfolio_central_sum.add(static_cast<long double>(scenario->weight));
        lower_sum.add(
            static_cast<long double>(matching->second->lower_weight));
        upper_sum.add(
            static_cast<long double>(matching->second->upper_weight));
    }
    const long double configured_sum = configured_central_sum.value();
    if (std::abs(configured_sum - 1.0L) > kWeightSumTolerance) {
        throw std::invalid_argument(
            "probability-polytope central weights must sum to one within tolerance");
    }
    if (lower_sum.value() > 1.0L) {
        throw std::invalid_argument(
            "probability-polytope lower weights leave no feasible probability measure");
    }
    if (upper_sum.value() < 1.0L) {
        throw std::invalid_argument(
            "probability-polytope upper weights leave no feasible probability measure");
    }

    CanonicalPolytope canonical;
    canonical.lower_sum = lower_sum.value();
    canonical.upper_sum = upper_sum.value();
    canonical.scenario_ids.reserve(ordered_portfolio_scenarios.size());
    canonical.lower.reserve(ordered_portfolio_scenarios.size());
    canonical.central.reserve(ordered_portfolio_scenarios.size());
    canonical.upper.reserve(ordered_portfolio_scenarios.size());
    canonical.published_scenarios.reserve(
        ordered_portfolio_scenarios.size());

    const long double portfolio_sum = portfolio_central_sum.value();
    for (const JointScenario* scenario : ordered_portfolio_scenarios) {
        const ProbabilityPolytopeScenario& configured =
            *configured_by_id.at(scenario->id);
        const long double normalized_configured =
            static_cast<long double>(configured.central_weight) /
            configured_sum;
        const long double normalized_portfolio =
            static_cast<long double>(scenario->weight) / portfolio_sum;
        const long double lower =
            static_cast<long double>(configured.lower_weight);
        const long double upper =
            static_cast<long double>(configured.upper_weight);
        if (normalized_configured <
                lower - match_tolerance(normalized_configured, lower) ||
            normalized_configured >
                upper + match_tolerance(normalized_configured, upper) ||
            normalized_portfolio <
                lower - match_tolerance(normalized_portfolio, lower) ||
            normalized_portfolio >
                upper + match_tolerance(normalized_portfolio, upper)) {
            throw std::invalid_argument(
                "probability-polytope normalized central measure lies outside scenario bounds");
        }
        if (std::abs(normalized_configured - normalized_portfolio) >
            match_tolerance(normalized_configured, normalized_portfolio)) {
            throw std::invalid_argument(
                "probability-polytope central weights do not match the portfolio measure");
        }
        canonical.scenario_ids.push_back(scenario->id);
        canonical.lower.push_back(lower);
        canonical.central.push_back(normalized_portfolio);
        canonical.upper.push_back(upper);
        canonical.published_scenarios.push_back(
            ProbabilityPolytopeScenario{scenario->id,
                configured.lower_weight, to_double(normalized_portfolio),
                configured.upper_weight});
    }
    canonical.central_sum = stable_sum(canonical.central);

    std::unordered_map<std::string, std::size_t> scenario_index;
    scenario_index.reserve(canonical.scenario_ids.size());
    for (std::size_t index = 0U; index < canonical.scenario_ids.size();
         ++index) {
        scenario_index.emplace(canonical.scenario_ids[index], index);
    }

    std::unordered_set<std::string> event_ids;
    event_ids.reserve(probability_polytope.events.size());
    std::set<std::vector<std::string>> event_taxonomies;
    std::size_t aggregate_memberships = 0U;
    canonical.events.reserve(probability_polytope.events.size());
    for (const ProbabilityEventConstraint& configured_event :
         probability_polytope.events) {
        if (!is_safe_identifier(configured_event.event_id)) {
            throw std::invalid_argument(
                "probability event id must be a safe identifier");
        }
        if (!event_ids.insert(configured_event.event_id).second) {
            throw std::invalid_argument(
                "probability event ids must be unique");
        }
        require_safe_text(
            configured_event.definition, "probability event definition");
        if (!std::isfinite(configured_event.lower_probability) ||
            !std::isfinite(configured_event.upper_probability)) {
            throw std::invalid_argument(
                "probability event bounds must be finite");
        }
        if (configured_event.lower_probability < 0.0 ||
            configured_event.upper_probability > 1.0 ||
            configured_event.lower_probability >
                configured_event.upper_probability) {
            throw std::invalid_argument(
                "probability event bounds must satisfy 0 <= lower <= upper <= 1");
        }
        if (configured_event.scenario_ids.empty()) {
            throw std::invalid_argument(
                "probability event membership must be non-empty");
        }
        if (configured_event.scenario_ids.size() >
            kMaximumEventMemberships - aggregate_memberships) {
            throw std::runtime_error(
                "probability event memberships exceed the 65536-entry solver guard");
        }
        aggregate_memberships += configured_event.scenario_ids.size();

        CanonicalEvent event;
        event.published = configured_event;
        std::sort(event.published.scenario_ids.begin(),
            event.published.scenario_ids.end());
        if (std::adjacent_find(event.published.scenario_ids.begin(),
                event.published.scenario_ids.end()) !=
            event.published.scenario_ids.end()) {
            throw std::invalid_argument(
                "probability event scenario members must be unique");
        }
        event.member_indices.reserve(event.published.scenario_ids.size());
        for (const std::string& member : event.published.scenario_ids) {
            if (!is_safe_identifier(member)) {
                throw std::invalid_argument(
                    "probability event member must be a safe scenario identifier");
            }
            const auto matching = scenario_index.find(member);
            if (matching == scenario_index.end()) {
                throw std::invalid_argument(
                    "probability event names an unknown portfolio scenario");
            }
            event.member_indices.push_back(matching->second);
        }
        if (event.member_indices.size() == canonical.scenario_ids.size()) {
            throw std::invalid_argument(
                "probability event must be a proper subset of the scenario taxonomy");
        }
        if (!event_taxonomies.insert(event.published.scenario_ids).second) {
            throw std::invalid_argument(
                "probability events must not duplicate a membership set");
        }

        CompensatedSum event_central_sum;
        for (const std::size_t member : event.member_indices) {
            event_central_sum.add(canonical.central[member]);
        }
        const long double central_probability =
            event_central_sum.value() / canonical.central_sum;
        const long double lower = static_cast<long double>(
            configured_event.lower_probability);
        const long double upper = static_cast<long double>(
            configured_event.upper_probability);
        if (central_probability < lower -
                match_tolerance(central_probability, lower) ||
            central_probability > upper +
                match_tolerance(central_probability, upper)) {
            throw std::invalid_argument(
                "probability event bounds do not contain the normalized central measure");
        }
        canonical.events.push_back(std::move(event));
    }
    std::sort(canonical.events.begin(), canonical.events.end(),
        [](const CanonicalEvent& first, const CanonicalEvent& second) {
            return first.published.event_id < second.published.event_id;
        });
    return canonical;
}

[[nodiscard]] std::vector<double> canonical_objective_values(
    const std::vector<ProbabilityPolytopeScenarioValue>& scenario_values,
    const CanonicalPolytope& canonical) {
    if (scenario_values.size() != canonical.scenario_ids.size()) {
        throw std::invalid_argument(
            "probability-polytope objective must name every scenario exactly once");
    }
    std::unordered_map<std::string, double> value_by_id;
    value_by_id.reserve(scenario_values.size());
    for (const ProbabilityPolytopeScenarioValue& scenario : scenario_values) {
        if (!is_safe_identifier(scenario.scenario_id)) {
            throw std::invalid_argument(
                "probability-polytope objective scenario id must be a safe identifier");
        }
        if (!std::isfinite(scenario.value)) {
            throw std::invalid_argument(
                "probability-polytope objective values must be finite");
        }
        if (!value_by_id.emplace(scenario.scenario_id, scenario.value).second) {
            throw std::invalid_argument(
                "probability-polytope objective scenario ids must be unique");
        }
    }
    std::vector<double> ordered_values;
    ordered_values.reserve(canonical.scenario_ids.size());
    for (const std::string& scenario_id : canonical.scenario_ids) {
        const auto matching = value_by_id.find(scenario_id);
        if (matching == value_by_id.end()) {
            throw std::invalid_argument(
                "probability-polytope objective scenario ids do not match the portfolio taxonomy");
        }
        ordered_values.push_back(matching->second);
    }
    return ordered_values;
}

[[nodiscard]] ScaledObjective scale_objective(
    const std::vector<double>& values) {
    if (values.empty()) {
        throw std::logic_error(
            "probability-polytope objective is unexpectedly empty");
    }
    const auto [minimum, maximum] =
        std::minmax_element(values.begin(), values.end());
    ScaledObjective result;
    // Halving before addition avoids overflowing when the finite endpoints
    // have opposite signs near the numeric limits.
    result.offset = static_cast<long double>(*minimum) / 2.0L +
        static_cast<long double>(*maximum) / 2.0L;
    long double maximum_deviation = 0.0L;
    result.coefficients.reserve(values.size());
    for (const double value : values) {
        const long double deviation =
            static_cast<long double>(value) - result.offset;
        maximum_deviation =
            std::max(maximum_deviation, std::abs(deviation));
        result.coefficients.push_back(deviation);
    }
    if (maximum_deviation > 0.0L) {
        result.scale = maximum_deviation;
        for (long double& coefficient : result.coefficients) {
            coefficient /= result.scale;
        }
    }
    return result;
}

class TwoPhaseSimplex {
public:
    TwoPhaseSimplex(const CanonicalPolytope& canonical,
        const std::vector<long double>& objective,
        std::size_t pivot_limit = kMaximumSimplexPivots)
        : TwoPhaseSimplex(canonical.scenario_ids.size(),
              constraint_row_count(
                  canonical.scenario_ids.size(), canonical.events.size()),
              objective, pivot_limit) {
        std::size_t row = 0U;
        for (std::size_t scenario = 0U;
             scenario < canonical.scenario_ids.size();
             ++scenario) {
            cell(row, scenario) = 1.0L;
            cell(row, right_hand_side_column()) = canonical.upper[scenario];
            ++row;
            cell(row, scenario) = -1.0L;
            cell(row, right_hand_side_column()) = -canonical.lower[scenario];
            ++row;
        }
        for (const CanonicalEvent& event : canonical.events) {
            for (const std::size_t member : event.member_indices) {
                cell(row, member) = 1.0L;
            }
            cell(row, right_hand_side_column()) =
                static_cast<long double>(event.published.upper_probability);
            ++row;
            for (const std::size_t member : event.member_indices) {
                cell(row, member) = -1.0L;
            }
            cell(row, right_hand_side_column()) =
                -static_cast<long double>(event.published.lower_probability);
            ++row;
        }
        for (std::size_t scenario = 0U;
             scenario < canonical.scenario_ids.size();
             ++scenario) {
            cell(row, scenario) = 1.0L;
        }
        cell(row, right_hand_side_column()) = 1.0L;
        ++row;
        for (std::size_t scenario = 0U;
             scenario < canonical.scenario_ids.size();
             ++scenario) {
            cell(row, scenario) = -1.0L;
        }
        cell(row, right_hand_side_column()) = -1.0L;
        ++row;
        if (row != constraint_rows_) {
            throw std::logic_error(
                "probability-polytope simplex constraint assembly mismatch");
        }
    }

    // Lifted upper-tail maximum with p=y+z. The first K variables are y and
    // the next K are z; every scenario and event constraint applies to p.
    TwoPhaseSimplex(const CanonicalPolytope& canonical,
        const std::vector<long double>& tail_objective,
        long double tail_probability, std::size_t pivot_limit)
        : TwoPhaseSimplex(2U * canonical.scenario_ids.size(),
              lifted_tail_constraint_row_count(
                  canonical.scenario_ids.size(), canonical.events.size()),
              make_lifted_objective(tail_objective), pivot_limit) {
        const std::size_t scenarios = canonical.scenario_ids.size();
        if (tail_objective.size() != scenarios ||
            tail_probability <= 0.0L || tail_probability >= 1.0L) {
            throw std::logic_error(
                "probability-polytope lifted tail request is invalid");
        }

        std::size_t row = 0U;
        for (std::size_t scenario = 0U; scenario < scenarios; ++scenario) {
            cell(row, scenario) = 1.0L;
            cell(row, scenarios + scenario) = 1.0L;
            cell(row, right_hand_side_column()) = canonical.upper[scenario];
            ++row;
            cell(row, scenario) = -1.0L;
            cell(row, scenarios + scenario) = -1.0L;
            cell(row, right_hand_side_column()) = -canonical.lower[scenario];
            ++row;
        }
        for (const CanonicalEvent& event : canonical.events) {
            for (const std::size_t member : event.member_indices) {
                cell(row, member) = 1.0L;
                cell(row, scenarios + member) = 1.0L;
            }
            cell(row, right_hand_side_column()) =
                static_cast<long double>(event.published.upper_probability);
            ++row;
            for (const std::size_t member : event.member_indices) {
                cell(row, member) = -1.0L;
                cell(row, scenarios + member) = -1.0L;
            }
            cell(row, right_hand_side_column()) =
                -static_cast<long double>(event.published.lower_probability);
            ++row;
        }
        for (std::size_t scenario = 0U; scenario < scenarios; ++scenario) {
            cell(row, scenario) = 1.0L;
        }
        cell(row, right_hand_side_column()) = tail_probability;
        ++row;
        for (std::size_t scenario = 0U; scenario < scenarios; ++scenario) {
            cell(row, scenario) = -1.0L;
        }
        cell(row, right_hand_side_column()) = -tail_probability;
        ++row;
        for (std::size_t scenario = 0U; scenario < scenarios; ++scenario) {
            cell(row, scenarios + scenario) = 1.0L;
        }
        cell(row, right_hand_side_column()) = 1.0L - tail_probability;
        ++row;
        for (std::size_t scenario = 0U; scenario < scenarios; ++scenario) {
            cell(row, scenarios + scenario) = -1.0L;
        }
        cell(row, right_hand_side_column()) =
            -(1.0L - tail_probability);
        ++row;
        if (row != constraint_rows_) {
            throw std::logic_error(
                "probability-polytope lifted constraint assembly mismatch");
        }
    }

    [[nodiscard]] SimplexResult solve() {
        std::size_t leaving = 0U;
        for (std::size_t row = 1U; row < constraint_rows_; ++row) {
            const long double candidate =
                cell(row, right_hand_side_column());
            const long double current =
                cell(leaving, right_hand_side_column());
            if (candidate < current - comparison_tolerance(candidate, current) ||
                (std::abs(candidate - current) <=
                        comparison_tolerance(candidate, current) &&
                    basis_[row] < basis_[leaving])) {
                leaving = row;
            }
        }

        if (cell(leaving, right_hand_side_column()) <
            -kSimplexTolerance) {
            pivot(leaving, artificial_column());
            if (!run_simplex(Phase::One)) {
                throw std::runtime_error(
                    "probability-polytope phase-one simplex became unbounded");
            }
            const long double phase_one_value =
                cell(phase_one_objective_row(), right_hand_side_column());
            if (phase_one_value < -kPublishedConstraintTolerance) {
                throw std::invalid_argument(
                    "probability-polytope constraints have no feasible measure");
            }
            if (phase_one_value > kPublishedConstraintTolerance) {
                throw std::runtime_error(
                    "probability-polytope phase-one simplex has an invalid residual");
            }

            for (std::size_t row = 0U; row < constraint_rows_; ++row) {
                if (basis_[row] != -1) {
                    continue;
                }
                std::optional<std::size_t> entering;
                for (std::size_t column = 0U;
                     column <= variable_count_; ++column) {
                    if (nonbasis_[column] < 0 ||
                        std::abs(cell(row, column)) <= kSimplexTolerance) {
                        continue;
                    }
                    if (!entering ||
                        nonbasis_[column] < nonbasis_[*entering]) {
                        entering = column;
                    }
                }
                if (entering) {
                    pivot(row, *entering);
                }
                break;
            }
        }

        if (!run_simplex(Phase::Two)) {
            throw std::runtime_error(
                "bounded probability-polytope simplex reported an unbounded objective");
        }

        SimplexResult result;
        result.variables.assign(variable_count_, 0.0L);
        for (std::size_t row = 0U; row < constraint_rows_; ++row) {
            if (basis_[row] >= 0 &&
                static_cast<std::size_t>(basis_[row]) < variable_count_) {
                long double value = cell(row, right_hand_side_column());
                if (value < 0.0L &&
                    value >= -kPublishedConstraintTolerance) {
                    value = 0.0L;
                }
                result.variables[static_cast<std::size_t>(basis_[row])] =
                    value;
            }
        }
        result.objective =
            cell(phase_two_objective_row(), right_hand_side_column());
        for (std::size_t column = 0U; column <= variable_count_; ++column) {
            if (nonbasis_[column] >= 0) {
                result.reduced_cost_violation = std::max(
                    result.reduced_cost_violation,
                    std::max(0.0L,
                        -cell(phase_two_objective_row(), column)));
            }
        }
        result.pivot_count = pivot_count_;
        return result;
    }

private:
    enum class Phase : unsigned char { One, Two };

    TwoPhaseSimplex(std::size_t variable_count,
        std::size_t constraint_rows,
        const std::vector<long double>& objective,
        std::size_t pivot_limit)
        : constraint_rows_(constraint_rows), variable_count_(variable_count),
          tableau_rows_(constraint_rows_ + 2U),
          tableau_columns_(variable_count_ + 2U),
          basis_(constraint_rows_), nonbasis_(variable_count_ + 1U),
          tableau_(checked_tableau_cell_count(
              variable_count_, constraint_rows_), 0.0L),
          pivot_limit_(std::min(pivot_limit, kMaximumSimplexPivots)) {
        if (objective.size() != variable_count_ || pivot_limit_ == 0U) {
            throw std::runtime_error(
                "probability-polytope simplex objective or pivot resource is invalid");
        }
        for (std::size_t constraint = 0U;
             constraint < constraint_rows_; ++constraint) {
            basis_[constraint] = static_cast<std::ptrdiff_t>(
                variable_count_ + constraint);
            cell(constraint, artificial_column()) = -1.0L;
        }
        for (std::size_t variable = 0U; variable < variable_count_;
             ++variable) {
            nonbasis_[variable] = static_cast<std::ptrdiff_t>(variable);
            cell(phase_two_objective_row(), variable) = -objective[variable];
        }
        nonbasis_[artificial_column()] = -1;
        cell(phase_one_objective_row(), artificial_column()) = 1.0L;
    }

    [[nodiscard]] static std::vector<long double> make_lifted_objective(
        const std::vector<long double>& tail_objective) {
        std::vector<long double> objective(2U * tail_objective.size(), 0.0L);
        std::copy(tail_objective.begin(), tail_objective.end(),
            objective.begin());
        return objective;
    }

    [[nodiscard]] std::size_t artificial_column() const noexcept {
        return variable_count_;
    }

    [[nodiscard]] std::size_t right_hand_side_column() const noexcept {
        return variable_count_ + 1U;
    }

    [[nodiscard]] std::size_t phase_two_objective_row() const noexcept {
        return constraint_rows_;
    }

    [[nodiscard]] std::size_t phase_one_objective_row() const noexcept {
        return constraint_rows_ + 1U;
    }

    [[nodiscard]] long double& cell(
        std::size_t row, std::size_t column) noexcept {
        return tableau_[row * tableau_columns_ + column];
    }

    [[nodiscard]] const long double& cell(
        std::size_t row, std::size_t column) const noexcept {
        return tableau_[row * tableau_columns_ + column];
    }

    [[nodiscard]] static long double comparison_tolerance(
        long double first, long double second) noexcept {
        return kSimplexTolerance *
            std::max({1.0L, std::abs(first), std::abs(second)});
    }

    void pivot(std::size_t pivot_row, std::size_t pivot_column) {
        if (pivot_count_ >= pivot_limit_) {
            throw std::runtime_error(
                "probability-polytope simplex exceeded its pivot resource guard");
        }
        ++pivot_count_;
        const long double pivot_value = cell(pivot_row, pivot_column);
        if (!std::isfinite(pivot_value) ||
            std::abs(pivot_value) <= kSimplexTolerance) {
            throw std::runtime_error(
                "probability-polytope simplex encountered a singular pivot");
        }
        const long double inverse = 1.0L / pivot_value;
        for (std::size_t row = 0U; row < tableau_rows_; ++row) {
            if (row == pivot_row) {
                continue;
            }
            const long double row_multiplier =
                cell(row, pivot_column) * inverse;
            for (std::size_t column = 0U; column < tableau_columns_;
                 ++column) {
                if (column == pivot_column) {
                    continue;
                }
                const long double updated = cell(row, column) -
                    cell(pivot_row, column) * row_multiplier;
                if (!std::isfinite(updated)) {
                    throw std::runtime_error(
                        "probability-polytope simplex exceeded numeric range");
                }
                cell(row, column) = updated;
            }
        }
        for (std::size_t column = 0U; column < tableau_columns_; ++column) {
            if (column != pivot_column) {
                cell(pivot_row, column) *= inverse;
            }
        }
        for (std::size_t row = 0U; row < tableau_rows_; ++row) {
            if (row != pivot_row) {
                cell(row, pivot_column) *= -inverse;
            }
        }
        cell(pivot_row, pivot_column) = inverse;
        std::swap(basis_[pivot_row], nonbasis_[pivot_column]);
    }

    [[nodiscard]] bool run_simplex(Phase phase) {
        const std::size_t objective_row = phase == Phase::One
            ? phase_one_objective_row()
            : phase_two_objective_row();
        while (true) {
            std::optional<std::size_t> entering;
            for (std::size_t column = 0U; column <= variable_count_;
                 ++column) {
                if (nonbasis_[column] < 0 ||
                    cell(objective_row, column) >= -kSimplexTolerance) {
                    continue;
                }
                if (!entering ||
                    nonbasis_[column] < nonbasis_[*entering]) {
                    entering = column;
                }
            }
            if (!entering) {
                return true;
            }

            std::optional<std::size_t> leaving;
            long double leaving_ratio = 0.0L;
            for (std::size_t row = 0U; row < constraint_rows_; ++row) {
                const long double coefficient = cell(row, *entering);
                if (coefficient <= kSimplexTolerance) {
                    continue;
                }
                const long double ratio =
                    cell(row, right_hand_side_column()) / coefficient;
                if (!leaving ||
                    ratio < leaving_ratio -
                        comparison_tolerance(ratio, leaving_ratio) ||
                    (std::abs(ratio - leaving_ratio) <=
                            comparison_tolerance(ratio, leaving_ratio) &&
                        basis_[row] < basis_[*leaving])) {
                    leaving = row;
                    leaving_ratio = ratio;
                }
            }
            if (!leaving) {
                return false;
            }
            pivot(*leaving, *entering);
        }
    }

    std::size_t constraint_rows_{0U};
    std::size_t variable_count_{0U};
    std::size_t tableau_rows_{0U};
    std::size_t tableau_columns_{0U};
    std::vector<std::ptrdiff_t> basis_{};
    std::vector<std::ptrdiff_t> nonbasis_{};
    std::vector<long double> tableau_{};
    std::size_t pivot_count_{0U};
    std::size_t pivot_limit_{kMaximumSimplexPivots};
};

[[nodiscard]] long double direct_objective(
    const std::vector<double>& values,
    const std::vector<double>& weights) {
    if (values.size() != weights.size()) {
        throw std::logic_error(
            "probability-polytope endpoint objective dimension mismatch");
    }
    CompensatedSum total;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        total.add(static_cast<long double>(values[index]) *
            static_cast<long double>(weights[index]));
    }
    return total.value();
}

[[nodiscard]] long double central_objective(
    const std::vector<double>& values,
    const CanonicalPolytope& canonical) {
    CompensatedSum numerator;
    for (std::size_t index = 0U; index < values.size(); ++index) {
        numerator.add(static_cast<long double>(values[index]) *
            canonical.central[index]);
    }
    return numerator.value() / canonical.central_sum;
}

[[nodiscard]] long double maximum_constraint_violation(
    const std::vector<double>& weights,
    const CanonicalPolytope& canonical) {
    if (weights.size() != canonical.scenario_ids.size()) {
        throw std::logic_error(
            "probability-polytope endpoint witness dimension mismatch");
    }
    CompensatedSum total_probability;
    long double violation = 0.0L;
    for (std::size_t scenario = 0U; scenario < weights.size(); ++scenario) {
        const long double weight =
            static_cast<long double>(weights[scenario]);
        if (!std::isfinite(weight)) {
            throw std::logic_error(
                "probability-polytope endpoint witness is non-finite");
        }
        total_probability.add(weight);
        violation = std::max(violation, std::max(0.0L, -weight));
        violation = std::max(
            violation, std::max(0.0L, canonical.lower[scenario] - weight));
        violation = std::max(
            violation, std::max(0.0L, weight - canonical.upper[scenario]));
    }
    violation =
        std::max(violation, std::abs(total_probability.value() - 1.0L));
    for (const CanonicalEvent& event : canonical.events) {
        CompensatedSum event_probability;
        for (const std::size_t member : event.member_indices) {
            event_probability.add(
                static_cast<long double>(weights[member]));
        }
        const long double probability = event_probability.value();
        violation = std::max(violation,
            std::max(0.0L,
                static_cast<long double>(event.published.lower_probability) -
                    probability));
        violation = std::max(violation,
            std::max(0.0L, probability -
                static_cast<long double>(
                    event.published.upper_probability)));
    }
    return violation;
}

[[nodiscard]] std::vector<double> published_central_weights(
    const CanonicalPolytope& canonical) {
    std::vector<double> weights;
    weights.reserve(canonical.published_scenarios.size());
    for (const ProbabilityPolytopeScenario& scenario :
         canonical.published_scenarios) {
        weights.push_back(scenario.central_weight);
    }
    return weights;
}

[[nodiscard]] long double direct_tail_expected_shortfall(
    const std::vector<double>& values,
    const std::vector<double>& tail_masses,
    long double tail_probability) {
    if (values.size() != tail_masses.size() ||
        tail_probability <= 0.0L) {
        throw std::logic_error(
            "probability-polytope tail objective dimension mismatch");
    }
    CompensatedSum numerator;
    for (std::size_t scenario = 0U; scenario < values.size(); ++scenario) {
        numerator.add(static_cast<long double>(values[scenario]) *
            static_cast<long double>(tail_masses[scenario]));
    }
    return numerator.value() / tail_probability;
}

[[nodiscard]] FractionalTailAllocation allocate_fractional_upper_tail(
    const std::vector<double>& values, const std::vector<double>& weights,
    long double tail_probability) {
    if (values.empty() || values.size() != weights.size() ||
        tail_probability <= 0.0L || tail_probability > 1.0L) {
        throw std::logic_error(
            "probability-polytope fractional-tail request is invalid");
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

    std::vector<long double> masses(values.size(), 0.0L);
    long double remaining = tail_probability;
    bool found_boundary = false;
    double boundary = values[order.front()];
    for (std::size_t begin = 0U;
         begin < order.size() && remaining > 0.0L;) {
        std::size_t end = begin + 1U;
        while (end < order.size() &&
            values[order[end]] == values[order[begin]]) {
            ++end;
        }
        CompensatedSum block_probability;
        for (std::size_t position = begin; position < end; ++position) {
            const long double weight =
                static_cast<long double>(weights[order[position]]);
            if (!std::isfinite(weight) ||
                weight < -kPublishedConstraintTolerance) {
                throw std::logic_error(
                    "probability-polytope tail measure has an invalid weight");
            }
            block_probability.add(std::max(0.0L, weight));
        }
        const long double block = block_probability.value();
        const long double included = std::min(remaining, block);
        const long double fraction = block > 0.0L ? included / block : 0.0L;
        for (std::size_t position = begin; position < end; ++position) {
            const std::size_t scenario = order[position];
            masses[scenario] = fraction * std::max(0.0L,
                static_cast<long double>(weights[scenario]));
        }
        if (included > 0.0L) {
            boundary = values[order[begin]];
            found_boundary = true;
        }
        remaining -= included;
        if (remaining < 0.0L &&
            remaining >= -kPublishedConstraintTolerance) {
            remaining = 0.0L;
        }
        begin = end;
    }
    if (remaining > kPublishedConstraintTolerance || !found_boundary) {
        throw std::logic_error(
            "probability-polytope full measure cannot supply the requested tail mass");
    }

    FractionalTailAllocation result;
    result.boundary_value = boundary;
    result.masses.reserve(masses.size());
    for (const long double mass : masses) {
        result.masses.push_back(to_double(mass));
    }
    result.expected_shortfall = direct_tail_expected_shortfall(
        values, result.masses, tail_probability);
    return result;
}

[[nodiscard]] long double maximum_tail_mass_violation(
    const std::vector<double>& tail_masses,
    const std::vector<double>& full_weights,
    long double tail_probability) {
    if (tail_masses.size() != full_weights.size()) {
        throw std::logic_error(
            "probability-polytope tail witness dimension mismatch");
    }
    CompensatedSum total_mass;
    long double violation = 0.0L;
    for (std::size_t scenario = 0U; scenario < tail_masses.size();
         ++scenario) {
        const long double tail =
            static_cast<long double>(tail_masses[scenario]);
        const long double full =
            static_cast<long double>(full_weights[scenario]);
        if (!std::isfinite(tail) || !std::isfinite(full)) {
            throw std::logic_error(
                "probability-polytope tail witness is non-finite");
        }
        total_mass.add(tail);
        violation = std::max(violation, std::max(0.0L, -tail));
        violation = std::max(violation, std::max(0.0L, tail - full));
    }
    return std::max(
        violation, std::abs(total_mass.value() - tail_probability));
}

[[nodiscard]] long double expected_shortfall_tolerance(
    long double scale, long double tail_probability) noexcept {
    return 1.0e-8L + 1024.0L *
        static_cast<long double>(std::numeric_limits<double>::epsilon()) *
        std::max(1.0L, std::abs(scale)) /
        tail_probability;
}

[[nodiscard]] long double threshold_formula_value(
    const ScaledObjective& scaled, long double threshold_scaled,
    const std::vector<double>& full_weights,
    long double tail_probability) {
    if (scaled.coefficients.size() != full_weights.size()) {
        throw std::logic_error(
            "probability-polytope threshold formula dimension mismatch");
    }
    CompensatedSum hinge_expectation;
    for (std::size_t scenario = 0U;
         scenario < full_weights.size(); ++scenario) {
        const long double hinge = std::max(
            0.0L, scaled.coefficients[scenario] - threshold_scaled);
        hinge_expectation.add(
            static_cast<long double>(full_weights[scenario]) * hinge);
    }
    return scaled.offset + scaled.scale *
        (threshold_scaled + hinge_expectation.value() / tail_probability);
}

void verify_tail_endpoint(
    const ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint,
    long double value_scale, long double tail_probability) {
    if (static_cast<long double>(endpoint.maximum_constraint_violation) >
            kPublishedConstraintTolerance ||
        static_cast<long double>(endpoint.maximum_tail_mass_violation) >
            kPublishedConstraintTolerance) {
        throw std::logic_error(
            "published probability-polytope expected-shortfall witness violates a constraint");
    }
    const long double tolerance =
        expected_shortfall_tolerance(value_scale, tail_probability);
    if (static_cast<long double>(endpoint.objective_reconciliation_error) >
            tolerance ||
        static_cast<long double>(
            endpoint.threshold_formula_reconciliation_error) > tolerance ||
        static_cast<long double>(endpoint.optimality_residual) > tolerance) {
        throw std::logic_error(
            "published probability-polytope expected-shortfall endpoint failed an audit");
    }
}

[[nodiscard]] long double box_optimum(
    const std::vector<double>& values, const CanonicalPolytope& canonical,
    bool maximize) {
    std::vector<long double> weights = canonical.lower;
    long double residual = 1.0L - canonical.lower_sum;
    std::vector<std::size_t> order(values.size());
    std::iota(order.begin(), order.end(), 0U);
    std::sort(order.begin(), order.end(),
        [&values, maximize](std::size_t first, std::size_t second) {
            if (values[first] != values[second]) {
                return maximize ? values[first] > values[second]
                                : values[first] < values[second];
            }
            return first < second;
        });
    for (const std::size_t scenario : order) {
        if (residual <= 0.0L) {
            break;
        }
        const long double allocation = std::min(
            residual, canonical.upper[scenario] - weights[scenario]);
        weights[scenario] += allocation;
        residual -= allocation;
    }
    CompensatedSum result;
    for (std::size_t scenario = 0U; scenario < values.size(); ++scenario) {
        result.add(weights[scenario] *
            static_cast<long double>(values[scenario]));
    }
    return result.value();
}

void verify_endpoint(const ProbabilityPolytopeEndpoint& endpoint,
    const std::vector<double>& values, long double objective_scale) {
    if (static_cast<long double>(endpoint.maximum_constraint_violation) >
        kPublishedConstraintTolerance) {
        throw std::logic_error(
            "published probability-polytope endpoint violates a constraint");
    }
    const long double tolerance = objective_tolerance(objective_scale);
    if (static_cast<long double>(endpoint.objective_reconciliation_error) >
        tolerance) {
        throw std::logic_error(
            "published probability-polytope endpoint does not reproduce its objective");
    }
    if (static_cast<long double>(endpoint.optimality_residual) > tolerance) {
        throw std::logic_error(
            "published probability-polytope endpoint failed its simplex reduced-cost audit");
    }
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw std::logic_error(
                "published probability-polytope endpoint has a non-finite objective");
        }
    }
}

[[nodiscard]] ProbabilityPolytopeEndpoint make_box_endpoint(
    const AmbiguityEndpoint& endpoint, const std::vector<double>& values,
    const CanonicalPolytope& canonical, bool maximize) {
    ProbabilityPolytopeEndpoint result;
    result.value = endpoint.value;
    result.scenario_weights = endpoint.scenario_weights;
    const long double recomputed =
        direct_objective(values, result.scenario_weights);
    result.maximum_constraint_violation = to_double(
        maximum_constraint_violation(result.scenario_weights, canonical));
    result.objective_reconciliation_error = to_double(std::abs(
        recomputed - static_cast<long double>(result.value)));
    result.optimality_residual = to_double(std::abs(
        recomputed - box_optimum(values, canonical, maximize)));
    const auto [minimum, maximum] =
        std::minmax_element(values.begin(), values.end());
    const long double scale = std::max(
        std::abs(static_cast<long double>(*minimum)),
        std::abs(static_cast<long double>(*maximum)));
    verify_endpoint(result, values, scale);
    return result;
}

[[nodiscard]] ProbabilityPolytopeEndpoint make_simplex_endpoint(
    const std::vector<double>& values, const CanonicalPolytope& canonical,
    const ScaledObjective& scaled, bool maximize) {
    std::vector<long double> simplex_objective = scaled.coefficients;
    if (!maximize) {
        for (long double& coefficient : simplex_objective) {
            coefficient = -coefficient;
        }
    }
    TwoPhaseSimplex solver(canonical, simplex_objective);
    const SimplexResult solved = solver.solve();

    ProbabilityPolytopeEndpoint endpoint;
    endpoint.scenario_weights.reserve(solved.variables.size());
    for (const long double weight : solved.variables) {
        endpoint.scenario_weights.push_back(to_double(weight));
    }
    const long double simplex_value = maximize
        ? scaled.offset + scaled.scale * solved.objective
        : scaled.offset - scaled.scale * solved.objective;
    endpoint.value = to_double(simplex_value);
    const long double recomputed =
        direct_objective(values, endpoint.scenario_weights);
    endpoint.maximum_constraint_violation = to_double(
        maximum_constraint_violation(endpoint.scenario_weights, canonical));
    endpoint.objective_reconciliation_error = to_double(std::abs(
        recomputed - static_cast<long double>(endpoint.value)));
    endpoint.optimality_residual = to_double(
        scaled.scale * solved.reduced_cost_violation);
    verify_endpoint(endpoint, values,
        std::max(std::abs(scaled.offset), scaled.scale));
    return endpoint;
}

void require_range_contains_central(
    const ProbabilityPolytopeMetricRange& range) {
    const long double scale = std::max({
        std::abs(static_cast<long double>(range.minimum.value)),
        std::abs(static_cast<long double>(range.central)),
        std::abs(static_cast<long double>(range.maximum.value))});
    const long double tolerance = objective_tolerance(scale);
    if (static_cast<long double>(range.minimum.value) >
            static_cast<long double>(range.maximum.value) + tolerance ||
        static_cast<long double>(range.central) <
            static_cast<long double>(range.minimum.value) - tolerance ||
        static_cast<long double>(range.central) >
            static_cast<long double>(range.maximum.value) + tolerance) {
        throw std::logic_error(
            "probability-polytope range does not contain its feasible central value");
    }
}

[[nodiscard]] std::vector<ProbabilityEventConstraint> published_events(
    const CanonicalPolytope& canonical) {
    std::vector<ProbabilityEventConstraint> events;
    events.reserve(canonical.events.size());
    for (const CanonicalEvent& event : canonical.events) {
        events.push_back(event.published);
    }
    return events;
}

void update_projection_audits(ProbabilityPolytopeMetricProjection& projection) {
    projection.maximum_endpoint_constraint_violation = std::max(
        projection.expectation.minimum.maximum_constraint_violation,
        projection.expectation.maximum.maximum_constraint_violation);
    projection.maximum_endpoint_objective_reconciliation_error = std::max(
        projection.expectation.minimum.objective_reconciliation_error,
        projection.expectation.maximum.objective_reconciliation_error);
    projection.maximum_endpoint_optimality_residual = std::max(
        projection.expectation.minimum.optimality_residual,
        projection.expectation.maximum.optimality_residual);
}

[[nodiscard]] long double scenario_value_scale(
    const std::vector<double>& values) noexcept {
    long double scale = 0.0L;
    for (const double value : values) {
        scale = std::max(
            scale, std::abs(static_cast<long double>(value)));
    }
    return scale;
}

[[nodiscard]] std::vector<double> publish_simplex_variables(
    const std::vector<long double>& variables) {
    std::vector<double> published;
    published.reserve(variables.size());
    for (const long double variable : variables) {
        published.push_back(to_double(variable));
    }
    return published;
}

[[nodiscard]] std::vector<double> publish_lifted_full_measure(
    const SimplexResult& solved, std::size_t scenario_count) {
    if (solved.variables.size() != 2U * scenario_count) {
        throw std::logic_error(
            "probability-polytope lifted solution dimension mismatch");
    }
    std::vector<double> weights;
    weights.reserve(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        weights.push_back(to_double(solved.variables[scenario] +
            solved.variables[scenario_count + scenario]));
    }
    return weights;
}

[[nodiscard]] std::vector<double> publish_lifted_tail_mass(
    const SimplexResult& solved, std::size_t scenario_count) {
    if (solved.variables.size() != 2U * scenario_count) {
        throw std::logic_error(
            "probability-polytope lifted tail dimension mismatch");
    }
    std::vector<double> masses;
    masses.reserve(scenario_count);
    for (std::size_t scenario = 0U; scenario < scenario_count; ++scenario) {
        masses.push_back(to_double(solved.variables[scenario]));
    }
    return masses;
}

[[nodiscard]] long double scaled_value_for(
    const std::vector<double>& values, const ScaledObjective& scaled,
    double value) {
    for (std::size_t scenario = 0U; scenario < values.size(); ++scenario) {
        if (values[scenario] == value) {
            return scaled.coefficients[scenario];
        }
    }
    throw std::logic_error(
        "probability-polytope selected threshold is not a scenario value");
}

[[nodiscard]] ProbabilityPolytopeUpperExpectedShortfallEndpoint
make_audited_tail_endpoint(double value, std::vector<double> full_weights,
    std::vector<double> tail_masses, const std::vector<double>& values,
    const CanonicalPolytope& canonical, long double tail_probability,
    long double additional_objective_error,
    long double threshold_formula_error,
    long double optimality_residual) {
    ProbabilityPolytopeUpperExpectedShortfallEndpoint endpoint;
    endpoint.value = value;
    endpoint.scenario_weights = std::move(full_weights);
    endpoint.tail_mass_weights = std::move(tail_masses);
    endpoint.maximum_constraint_violation = to_double(
        maximum_constraint_violation(endpoint.scenario_weights, canonical));
    endpoint.maximum_tail_mass_violation = to_double(
        maximum_tail_mass_violation(endpoint.tail_mass_weights,
            endpoint.scenario_weights, tail_probability));
    const long double direct = direct_tail_expected_shortfall(
        values, endpoint.tail_mass_weights, tail_probability);
    endpoint.objective_reconciliation_error = to_double(std::max(
        std::abs(direct - static_cast<long double>(endpoint.value)),
        additional_objective_error));
    endpoint.threshold_formula_reconciliation_error =
        to_double(threshold_formula_error);
    endpoint.optimality_residual = to_double(optimality_residual);
    verify_tail_endpoint(endpoint,
        scenario_value_scale(values), tail_probability);
    return endpoint;
}

[[nodiscard]] ProbabilityPolytopeUpperExpectedShortfallEndpoint
make_box_tail_endpoint(const AmbiguityEndpoint& delegated,
    const std::vector<double>& values, const CanonicalPolytope& canonical,
    const ScaledObjective& scaled, long double tail_probability,
    bool minimum) {
    const FractionalTailAllocation tail = allocate_fractional_upper_tail(
        values, delegated.scenario_weights, tail_probability);
    long double threshold_error = 0.0L;
    if (minimum) {
        const long double threshold_scaled = scaled_value_for(
            values, scaled, tail.boundary_value);
        const long double threshold_value = threshold_formula_value(
            scaled, threshold_scaled, delegated.scenario_weights,
            tail_probability);
        threshold_error = std::abs(threshold_value -
            static_cast<long double>(delegated.value));
    }
    return make_audited_tail_endpoint(delegated.value,
        delegated.scenario_weights, tail.masses, values, canonical,
        tail_probability, 0.0L, threshold_error, 0.0L);
}

void update_expected_shortfall_projection_audits(
    ProbabilityPolytopeUpperExpectedShortfallProjection& projection,
    const CanonicalPolytope& canonical, long double value_scale) {
    const std::vector<double> central_weights =
        published_central_weights(canonical);
    const double central_constraint_violation = to_double(
        maximum_constraint_violation(central_weights, canonical));
    const double central_tail_violation = to_double(
        maximum_tail_mass_violation(projection.central_tail_mass_weights,
            central_weights,
            static_cast<long double>(projection.tail_probability)));
    projection.maximum_endpoint_constraint_violation = std::max({
        projection.minimum.maximum_constraint_violation,
        central_constraint_violation,
        projection.maximum.maximum_constraint_violation});
    projection.maximum_endpoint_tail_mass_violation = std::max({
        projection.minimum.maximum_tail_mass_violation,
        central_tail_violation,
        projection.maximum.maximum_tail_mass_violation});
    projection.maximum_endpoint_objective_reconciliation_error = std::max({
        projection.minimum.objective_reconciliation_error,
        projection.central_objective_reconciliation_error,
        projection.maximum.objective_reconciliation_error});
    projection.maximum_endpoint_threshold_formula_reconciliation_error =
        std::max(
            projection.minimum.threshold_formula_reconciliation_error,
            projection.maximum.threshold_formula_reconciliation_error);
    projection.maximum_endpoint_optimality_residual = std::max(
        projection.minimum.optimality_residual,
        projection.maximum.optimality_residual);
    const long double audit_tolerance = expected_shortfall_tolerance(
        value_scale, static_cast<long double>(projection.tail_probability));
    if (static_cast<long double>(
            projection.maximum_endpoint_constraint_violation) >
            kPublishedConstraintTolerance ||
        static_cast<long double>(
            projection.maximum_endpoint_tail_mass_violation) >
            kPublishedConstraintTolerance ||
        static_cast<long double>(
            projection.maximum_endpoint_objective_reconciliation_error) >
            audit_tolerance ||
        static_cast<long double>(projection
            .maximum_endpoint_threshold_formula_reconciliation_error) >
            audit_tolerance ||
        static_cast<long double>(
            projection.maximum_endpoint_optimality_residual) >
            audit_tolerance ||
        static_cast<long double>(projection
            .maximum_threshold_enumeration_optimality_residual) >
            audit_tolerance) {
        throw std::logic_error(
            "probability-polytope expected-shortfall projection failed an aggregate audit");
    }
}

[[nodiscard]] std::size_t remaining_expected_shortfall_pivot_limit(
    std::size_t used_pivots) {
    if (used_pivots >= kMaximumExpectedShortfallAggregatePivots) {
        throw std::runtime_error(
            "probability-polytope expected-shortfall projection exceeded its aggregate pivot guard");
    }
    return std::min(kMaximumSimplexPivots,
        kMaximumExpectedShortfallAggregatePivots - used_pivots);
}

} // namespace

struct ProbabilityPolytopeProjector::State {
    State(CanonicalPolytope canonical_polytope,
        const PortfolioConfig& portfolio,
        const ProbabilityPolytopeConfig& configured_polytope)
        : canonical(std::move(canonical_polytope)) {
        if (canonical.events.empty()) {
            box_projector.emplace(
                portfolio, make_box_ambiguity(configured_polytope));
        }
    }

    CanonicalPolytope canonical{};
    std::optional<PortfolioAmbiguityProjector> box_projector{};
};

ProbabilityPolytopeProjector::ProbabilityPolytopeProjector(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope)
    : state_(std::make_shared<const State>(
          canonicalize_polytope(portfolio, probability_polytope), portfolio,
          probability_polytope)) {}

ProbabilityPolytopeMetricProjection
ProbabilityPolytopeProjector::project_expectation(
    const std::vector<ProbabilityPolytopeScenarioValue>& scenario_values)
    const {
    if (!state_) {
        throw std::logic_error(
            "probability-polytope projector has no prepared state");
    }
    const CanonicalPolytope& canonical = state_->canonical;
    const std::vector<double> ordered_values =
        canonical_objective_values(scenario_values, canonical);

    ProbabilityPolytopeMetricProjection projection;
    projection.scenario_probabilities = canonical.published_scenarios;
    projection.events = published_events(canonical);

    if (state_->box_projector) {
        std::vector<AmbiguityScenarioMetricValue> ambiguity_values;
        ambiguity_values.reserve(scenario_values.size());
        for (const ProbabilityPolytopeScenarioValue& scenario :
             scenario_values) {
            ambiguity_values.push_back(AmbiguityScenarioMetricValue{
                scenario.scenario_id, scenario.value});
        }
        const AmbiguityMetricProjection box_projection =
            state_->box_projector->project_expectation(ambiguity_values);
        projection.expectation.minimum = make_box_endpoint(
            box_projection.expectation.minimum, ordered_values, canonical,
            false);
        projection.expectation.central = box_projection.expectation.central;
        projection.expectation.maximum = make_box_endpoint(
            box_projection.expectation.maximum, ordered_values, canonical,
            true);
    } else {
        const ScaledObjective scaled = scale_objective(ordered_values);
        projection.expectation.minimum = make_simplex_endpoint(
            ordered_values, canonical, scaled, false);
        projection.expectation.central =
            to_double(central_objective(ordered_values, canonical));
        projection.expectation.maximum = make_simplex_endpoint(
            ordered_values, canonical, scaled, true);
    }
    require_range_contains_central(projection.expectation);
    update_projection_audits(projection);
    return projection;
}

ProbabilityPolytopeUpperExpectedShortfallProjection
ProbabilityPolytopeProjector::project_upper_expected_shortfall(
    const std::vector<ProbabilityPolytopeScenarioValue>& scenario_values,
    double tail_probability) const {
    if (!state_) {
        throw std::logic_error(
            "probability-polytope projector has no prepared state");
    }
    if (!std::isfinite(tail_probability) || tail_probability <= 0.0 ||
        tail_probability > 1.0) {
        throw std::invalid_argument(
            "probability-polytope upper-tail probability must be finite and in (0, 1]");
    }
    const CanonicalPolytope& canonical = state_->canonical;
    const long double requested_tail =
        static_cast<long double>(tail_probability);
    if (!canonical.events.empty() &&
        requested_tail < kMinimumEventTailProbability) {
        throw std::invalid_argument(
            "event-constrained probability-polytope tails must be at least 1e-6");
    }
    const std::vector<double> ordered_values =
        canonical_objective_values(scenario_values, canonical);
    const ScaledObjective scaled = scale_objective(ordered_values);
    const std::vector<double> central_weights =
        published_central_weights(canonical);

    ProbabilityPolytopeUpperExpectedShortfallProjection projection;
    projection.scenario_probabilities = canonical.published_scenarios;
    projection.events = published_events(canonical);
    projection.tail_probability = tail_probability;

    // At tau=1, upper expected shortfall is ordinary expectation. Reuse the
    // already-audited linear endpoint witnesses and avoid a degenerate lifted
    // z-mass equality at zero.
    if (tail_probability == 1.0) {
        const ProbabilityPolytopeMetricProjection expectation =
            project_expectation(scenario_values);
        const auto minimum_value =
            std::min_element(ordered_values.begin(), ordered_values.end());
        const long double threshold_scaled = scaled_value_for(
            ordered_values, scaled, *minimum_value);
        const long double minimum_threshold_formula = threshold_formula_value(
            scaled, threshold_scaled,
            expectation.expectation.minimum.scenario_weights,
            requested_tail);
        projection.minimum = make_audited_tail_endpoint(
            expectation.expectation.minimum.value,
            expectation.expectation.minimum.scenario_weights,
            expectation.expectation.minimum.scenario_weights,
            ordered_values, canonical, requested_tail,
            static_cast<long double>(expectation.expectation.minimum
                .objective_reconciliation_error),
            std::abs(minimum_threshold_formula -
                static_cast<long double>(
                    expectation.expectation.minimum.value)),
            static_cast<long double>(
                expectation.expectation.minimum.optimality_residual));
        projection.maximum = make_audited_tail_endpoint(
            expectation.expectation.maximum.value,
            expectation.expectation.maximum.scenario_weights,
            expectation.expectation.maximum.scenario_weights,
            ordered_values, canonical, requested_tail,
            static_cast<long double>(expectation.expectation.maximum
                .objective_reconciliation_error),
            0.0L,
            static_cast<long double>(
                expectation.expectation.maximum.optimality_residual));
        projection.central = expectation.expectation.central;
        projection.central_tail_mass_weights = central_weights;
        projection.central_objective_reconciliation_error = to_double(
            std::abs(direct_tail_expected_shortfall(ordered_values,
                         projection.central_tail_mass_weights,
                         requested_tail) -
                static_cast<long double>(projection.central)));
        projection.minimum_selected_threshold = *minimum_value;
        projection.distinct_thresholds_examined = 0U;
        update_expected_shortfall_projection_audits(
            projection, canonical, scenario_value_scale(ordered_values));
        return projection;
    }

    // With no named events, preserve the exact v0.1 endpoint values and full
    // witnesses. Tail masses are independently canonicalized for this richer
    // v0.2 result instead of exposing the v0.1 internal allocation.
    if (state_->box_projector) {
        std::vector<AmbiguityScenarioMetricValue> ambiguity_values;
        ambiguity_values.reserve(scenario_values.size());
        for (const ProbabilityPolytopeScenarioValue& scenario :
             scenario_values) {
            ambiguity_values.push_back(AmbiguityScenarioMetricValue{
                scenario.scenario_id, scenario.value});
        }
        const AmbiguityUpperExpectedShortfallProjection delegated =
            state_->box_projector->project_upper_expected_shortfall(
                ambiguity_values, tail_probability);
        projection.minimum = make_box_tail_endpoint(
            delegated.upper_expected_shortfall.minimum, ordered_values,
            canonical, scaled, requested_tail, true);
        projection.maximum = make_box_tail_endpoint(
            delegated.upper_expected_shortfall.maximum, ordered_values,
            canonical, scaled, requested_tail, false);
        projection.central = delegated.upper_expected_shortfall.central;
        const FractionalTailAllocation central_tail =
            allocate_fractional_upper_tail(
                ordered_values, central_weights, requested_tail);
        projection.central_tail_mass_weights = central_tail.masses;
        projection.central_objective_reconciliation_error = to_double(
            std::abs(central_tail.expected_shortfall -
                static_cast<long double>(projection.central)));
        projection.minimum_selected_threshold =
            allocate_fractional_upper_tail(ordered_values,
                projection.minimum.scenario_weights, requested_tail)
                .boundary_value;
        projection.distinct_thresholds_examined = 0U;
        update_expected_shortfall_projection_audits(
            projection, canonical, scenario_value_scale(ordered_values));
        return projection;
    }

    const bool constant_objective = std::all_of(
        ordered_values.begin() + 1, ordered_values.end(),
        [&ordered_values](double value) {
            return value == ordered_values.front();
        });
    if (constant_objective) {
        const FractionalTailAllocation tail = allocate_fractional_upper_tail(
            ordered_values, central_weights, requested_tail);
        const double value = ordered_values.front();
        const long double threshold_formula = threshold_formula_value(
            scaled, scaled.coefficients.front(), central_weights,
            requested_tail);
        projection.minimum = make_audited_tail_endpoint(value,
            central_weights, tail.masses, ordered_values, canonical,
            requested_tail, 0.0L,
            std::abs(threshold_formula - static_cast<long double>(value)),
            0.0L);
        projection.maximum = make_audited_tail_endpoint(value,
            central_weights, tail.masses, ordered_values, canonical,
            requested_tail, 0.0L, 0.0L, 0.0L);
        projection.central = value;
        projection.central_tail_mass_weights = tail.masses;
        projection.central_objective_reconciliation_error = to_double(
            std::abs(tail.expected_shortfall -
                static_cast<long double>(value)));
        projection.minimum_selected_threshold = value;
        projection.distinct_thresholds_examined = 1U;
        update_expected_shortfall_projection_audits(
            projection, canonical, scenario_value_scale(ordered_values));
        return projection;
    }

    std::vector<std::size_t> threshold_indices(ordered_values.size());
    std::iota(threshold_indices.begin(), threshold_indices.end(), 0U);
    std::sort(threshold_indices.begin(), threshold_indices.end(),
        [&ordered_values](std::size_t first, std::size_t second) {
            if (ordered_values[first] != ordered_values[second]) {
                return ordered_values[first] < ordered_values[second];
            }
            return first < second;
        });
    threshold_indices.erase(std::unique(threshold_indices.begin(),
        threshold_indices.end(),
        [&ordered_values](std::size_t first, std::size_t second) {
            return ordered_values[first] == ordered_values[second];
        }), threshold_indices.end());
    if (threshold_indices.size() > kMaximumScenariosWithEvents) {
        throw std::runtime_error(
            "probability-polytope expected-shortfall threshold count exceeds 512");
    }

    std::optional<ThresholdMinimumSolution> selected_minimum;
    std::size_t used_pivots = 0U;
    long double maximum_threshold_optimality_residual = 0.0L;
    for (const std::size_t threshold_index : threshold_indices) {
        const long double threshold_scaled =
            scaled.coefficients[threshold_index];
        std::vector<long double> minimization_objective;
        minimization_objective.reserve(ordered_values.size());
        for (const long double coefficient : scaled.coefficients) {
            // The simplex maximizes, hence the negative hinge objective.
            minimization_objective.push_back(
                -std::max(0.0L, coefficient - threshold_scaled));
        }
        TwoPhaseSimplex solver(canonical, minimization_objective,
            remaining_expected_shortfall_pivot_limit(used_pivots));
        SimplexResult solved = solver.solve();
        if (solved.pivot_count >
            kMaximumExpectedShortfallAggregatePivots - used_pivots) {
            throw std::runtime_error(
                "probability-polytope expected-shortfall projection exceeded its aggregate pivot guard");
        }
        used_pivots += solved.pivot_count;
        const long double candidate = scaled.offset + scaled.scale *
            (threshold_scaled - solved.objective / requested_tail);
        const long double optimality = scaled.scale *
            solved.reduced_cost_violation / requested_tail;
        maximum_threshold_optimality_residual = std::max(
            maximum_threshold_optimality_residual, optimality);
        if (!selected_minimum || candidate < selected_minimum->value) {
            selected_minimum = ThresholdMinimumSolution{std::move(solved),
                candidate, threshold_scaled,
                ordered_values[threshold_index], optimality};
        }
    }
    if (!selected_minimum) {
        throw std::logic_error(
            "probability-polytope minimum tail enumeration produced no threshold");
    }
    projection.distinct_thresholds_examined = threshold_indices.size();
    projection.maximum_threshold_enumeration_optimality_residual =
        to_double(maximum_threshold_optimality_residual);

    std::vector<double> minimum_weights =
        publish_simplex_variables(selected_minimum->simplex.variables);
    const FractionalTailAllocation minimum_tail =
        allocate_fractional_upper_tail(
            ordered_values, minimum_weights, requested_tail);
    const long double selected_formula = threshold_formula_value(scaled,
        selected_minimum->threshold_scaled, minimum_weights, requested_tail);
    projection.minimum = make_audited_tail_endpoint(
        to_double(selected_minimum->value), std::move(minimum_weights),
        minimum_tail.masses, ordered_values, canonical, requested_tail,
        std::abs(selected_formula - selected_minimum->value),
        std::abs(selected_formula - selected_minimum->value),
        selected_minimum->optimality_residual);
    projection.minimum_selected_threshold = selected_minimum->threshold;

    TwoPhaseSimplex maximum_solver(canonical, scaled.coefficients,
        requested_tail,
        remaining_expected_shortfall_pivot_limit(used_pivots));
    const SimplexResult maximum_solved = maximum_solver.solve();
    if (maximum_solved.pivot_count >
        kMaximumExpectedShortfallAggregatePivots - used_pivots) {
        throw std::runtime_error(
            "probability-polytope expected-shortfall projection exceeded its aggregate pivot guard");
    }
    used_pivots += maximum_solved.pivot_count;
    (void)used_pivots;
    std::vector<double> maximum_weights = publish_lifted_full_measure(
        maximum_solved, ordered_values.size());
    const std::vector<double> raw_maximum_tail = publish_lifted_tail_mass(
        maximum_solved, ordered_values.size());
    const FractionalTailAllocation maximum_tail =
        allocate_fractional_upper_tail(
            ordered_values, maximum_weights, requested_tail);
    const long double lifted_value = scaled.offset +
        scaled.scale * maximum_solved.objective / requested_tail;
    const long double raw_tail_value = direct_tail_expected_shortfall(
        ordered_values, raw_maximum_tail, requested_tail);
    projection.maximum = make_audited_tail_endpoint(
        to_double(lifted_value), std::move(maximum_weights),
        maximum_tail.masses, ordered_values, canonical, requested_tail,
        std::abs(raw_tail_value - lifted_value), 0.0L,
        scaled.scale * maximum_solved.reduced_cost_violation /
            requested_tail);
    projection.maximum.maximum_tail_mass_violation = std::max(
        projection.maximum.maximum_tail_mass_violation,
        to_double(maximum_tail_mass_violation(raw_maximum_tail,
            projection.maximum.scenario_weights, requested_tail)));
    verify_tail_endpoint(projection.maximum,
        scenario_value_scale(ordered_values), requested_tail);

    const FractionalTailAllocation central_tail =
        allocate_fractional_upper_tail(
            ordered_values, central_weights, requested_tail);
    projection.central = to_double(central_tail.expected_shortfall);
    projection.central_tail_mass_weights = central_tail.masses;
    projection.central_objective_reconciliation_error = to_double(std::abs(
        direct_tail_expected_shortfall(ordered_values,
            projection.central_tail_mass_weights, requested_tail) -
        static_cast<long double>(projection.central)));

    const long double range_tolerance = expected_shortfall_tolerance(
        scenario_value_scale(ordered_values), requested_tail);
    if (static_cast<long double>(projection.minimum.value) >
            static_cast<long double>(projection.maximum.value) +
                range_tolerance ||
        static_cast<long double>(projection.central) <
            static_cast<long double>(projection.minimum.value) -
                range_tolerance ||
        static_cast<long double>(projection.central) >
            static_cast<long double>(projection.maximum.value) +
                range_tolerance ||
        maximum_threshold_optimality_residual > range_tolerance) {
        throw std::logic_error(
            "probability-polytope expected-shortfall range failed its global audit");
    }
    update_expected_shortfall_projection_audits(
        projection, canonical, scenario_value_scale(ordered_values));
    return projection;
}

void validate_probability_polytope_config(const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope) {
    const CanonicalPolytope canonical =
        canonicalize_polytope(portfolio, probability_polytope);
    if (canonical.events.empty()) {
        validate_portfolio_ambiguity_config(
            portfolio, make_box_ambiguity(probability_polytope));
    }
}

} // namespace naturalehia::cellular_finance
