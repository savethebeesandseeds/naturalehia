#include "association.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using naturalehia::fauna::detail::AssociationEdge;
using naturalehia::fauna::detail::kUnassignedTrack;
using naturalehia::fauna::detail::solve_association;

[[noreturn]] void fail(std::string_view expression, std::string_view file, int line) {
    throw std::runtime_error(std::string{file} + ":" + std::to_string(line) +
                             ": requirement failed: " + std::string{expression});
}

#define REQUIRE(expression)                                                                        \
    do {                                                                                           \
        if (!(expression)) {                                                                       \
            fail(#expression, __FILE__, __LINE__);                                                 \
        }                                                                                          \
    } while (false)

template <typename Exception, typename Function> void require_throws(Function&& function) {
    bool threw_expected = false;
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        threw_expected = true;
    }
    REQUIRE(threw_expected);
}

struct Objective {
    std::size_t cardinality{};
    double cost{};
};

[[nodiscard]] bool better(Objective candidate, Objective incumbent) noexcept {
    return candidate.cardinality > incumbent.cardinality ||
           (candidate.cardinality == incumbent.cardinality && candidate.cost < incumbent.cost);
}

[[nodiscard]] std::vector<double> canonical_costs(std::size_t observation_count,
                                                  std::size_t track_count,
                                                  std::span<const AssociationEdge> edges) {
    const double forbidden = std::numeric_limits<double>::infinity();
    std::vector<double> costs(observation_count * track_count, forbidden);
    for (const AssociationEdge& edge : edges) {
        REQUIRE(edge.observation_index < observation_count);
        REQUIRE(edge.track_index < track_count);
        const std::size_t index = edge.observation_index * track_count + edge.track_index;
        costs[index] = std::min(costs[index], edge.cost);
    }
    return costs;
}

[[nodiscard]] Objective objective_of(std::span<const std::size_t> assignment,
                                     std::size_t observation_count, std::size_t track_count,
                                     std::span<const AssociationEdge> edges) {
    REQUIRE(assignment.size() == observation_count);
    const std::vector<double> costs = canonical_costs(observation_count, track_count, edges);
    std::vector<bool> used_tracks(track_count, false);
    Objective objective{};
    for (std::size_t observation = 0; observation < observation_count; ++observation) {
        const std::size_t track = assignment[observation];
        if (track == kUnassignedTrack) {
            continue;
        }
        REQUIRE(track < track_count);
        REQUIRE(!used_tracks[track]);
        used_tracks[track] = true;
        const double cost = costs[observation * track_count + track];
        REQUIRE(std::isfinite(cost));
        ++objective.cardinality;
        objective.cost += cost;
    }
    return objective;
}

void search_oracle(std::size_t observation, std::size_t observation_count, std::size_t track_count,
                   std::span<const double> costs, std::vector<bool>& used_tracks, Objective current,
                   Objective& best) {
    if (observation == observation_count) {
        if (better(current, best)) {
            best = current;
        }
        return;
    }

    search_oracle(observation + 1U, observation_count, track_count, costs, used_tracks, current,
                  best);
    for (std::size_t track = 0; track < track_count; ++track) {
        const double cost = costs[observation * track_count + track];
        if (used_tracks[track] || !std::isfinite(cost)) {
            continue;
        }
        used_tracks[track] = true;
        search_oracle(observation + 1U, observation_count, track_count, costs, used_tracks,
                      Objective{current.cardinality + 1U, current.cost + cost}, best);
        used_tracks[track] = false;
    }
}

[[nodiscard]] Objective brute_force_objective(std::size_t observation_count,
                                              std::size_t track_count,
                                              std::span<const AssociationEdge> edges) {
    const std::vector<double> costs = canonical_costs(observation_count, track_count, edges);
    std::vector<bool> used_tracks(track_count, false);
    Objective best{};
    search_oracle(0U, observation_count, track_count, costs, used_tracks, Objective{}, best);
    return best;
}

void maximum_cardinality_precedes_cost() {
    const std::vector<AssociationEdge> edges{
        {0U, 0U, 1.0},
        {0U, 1U, 100.0},
        {1U, 0U, 2.0},
    };

    const auto assignment = solve_association(2U, 2U, edges);
    REQUIRE(assignment == std::vector<std::size_t>({1U, 0U}));
    const Objective objective = objective_of(assignment, 2U, 2U, edges);
    REQUIRE(objective.cardinality == 2U);
    REQUIRE(objective.cost == 102.0);
}

void minimum_cost_breaks_equal_cardinality() {
    const std::vector<AssociationEdge> edges{
        {0U, 0U, 5.0},
        {0U, 1U, 1.0},
        {1U, 0U, 2.0},
        {1U, 1U, 4.0},
    };

    const auto assignment = solve_association(2U, 2U, edges);
    REQUIRE(assignment == std::vector<std::size_t>({1U, 0U}));
    const Objective objective = objective_of(assignment, 2U, 2U, edges);
    REQUIRE(objective.cardinality == 2U);
    REQUIRE(objective.cost == 3.0);
}

void edge_input_order_does_not_change_results() {
    const std::vector<AssociationEdge> edges{
        {0U, 0U, 8.0}, {0U, 1U, 2.0}, {1U, 0U, 3.0}, {1U, 2U, 7.0}, {2U, 1U, 4.0}, {2U, 2U, 1.0},
    };
    const auto expected = solve_association(3U, 3U, edges);

    std::vector<AssociationEdge> reversed = edges;
    std::reverse(reversed.begin(), reversed.end());
    REQUIRE(solve_association(3U, 3U, reversed) == expected);

    std::mt19937 random{0xA550C1A7U};
    for (std::size_t iteration = 0; iteration < 32U; ++iteration) {
        std::shuffle(reversed.begin(), reversed.end(), random);
        REQUIRE(solve_association(3U, 3U, reversed) == expected);
    }
}

void ties_are_stable_without_prescribing_a_particular_optimum() {
    std::vector<AssociationEdge> edges{
        {0U, 0U, 1.0}, {0U, 1U, 1.0}, {0U, 2U, 1.0}, {1U, 0U, 1.0}, {1U, 1U, 1.0},
        {1U, 2U, 1.0}, {2U, 0U, 1.0}, {2U, 1U, 1.0}, {2U, 2U, 1.0},
    };
    const auto baseline = solve_association(3U, 3U, edges);
    const Objective objective = objective_of(baseline, 3U, 3U, edges);
    REQUIRE(objective.cardinality == 3U);
    REQUIRE(objective.cost == 3.0);
    REQUIRE(solve_association(3U, 3U, edges) == baseline);

    std::mt19937 random{0x71E5U};
    for (std::size_t iteration = 0; iteration < 64U; ++iteration) {
        std::shuffle(edges.begin(), edges.end(), random);
        REQUIRE(solve_association(3U, 3U, edges) == baseline);
    }
}

void disconnected_forbidden_and_empty_inputs() {
    const std::vector<AssociationEdge> disconnected{
        {0U, 1U, 2.5},
        {2U, 2U, 1.0},
    };
    REQUIRE(solve_association(3U, 3U, disconnected) ==
            std::vector<std::size_t>({1U, kUnassignedTrack, 2U}));

    REQUIRE(solve_association(0U, 3U, {}).empty());
    REQUIRE(solve_association(3U, 0U, {}) == std::vector<std::size_t>(3U, kUnassignedTrack));
    REQUIRE(solve_association(2U, 2U, {}) == std::vector<std::size_t>(2U, kUnassignedTrack));
}

void duplicate_edges_keep_the_lowest_cost() {
    const std::vector<AssociationEdge> edges{
        {0U, 0U, 100.0}, {0U, 0U, 1.0}, {0U, 0U, 30.0}, {0U, 1U, 5.0}, {1U, 0U, 5.0}, {1U, 1U, 5.0},
    };
    const auto assignment = solve_association(2U, 2U, edges);
    REQUIRE(assignment == std::vector<std::size_t>({0U, 1U}));
    const Objective objective = objective_of(assignment, 2U, 2U, edges);
    REQUIRE(objective.cardinality == 2U);
    REQUIRE(objective.cost == 6.0);
}

void malformed_edges_are_rejected() {
    require_throws<std::invalid_argument>([] {
        const std::vector<AssociationEdge> edges{{1U, 0U, 0.0}};
        (void)solve_association(1U, 1U, edges);
    });
    require_throws<std::invalid_argument>([] {
        const std::vector<AssociationEdge> edges{{0U, 1U, 0.0}};
        (void)solve_association(1U, 1U, edges);
    });
    require_throws<std::invalid_argument>([] {
        const std::vector<AssociationEdge> edges{{0U, 0U, -1.0}};
        (void)solve_association(1U, 1U, edges);
    });
    require_throws<std::invalid_argument>([] {
        const std::vector<AssociationEdge> edges{
            {0U, 0U, std::numeric_limits<double>::quiet_NaN()}};
        (void)solve_association(1U, 1U, edges);
    });
    require_throws<std::invalid_argument>([] {
        const std::vector<AssociationEdge> edges{{0U, 0U, std::numeric_limits<double>::infinity()}};
        (void)solve_association(1U, 1U, edges);
    });
}

void seeded_random_graphs_match_brute_force_oracle() {
    std::mt19937 random{0xC0110A7EU};
    std::uniform_int_distribution<std::size_t> size_distribution{0U, 5U};
    std::bernoulli_distribution include_edge{0.58};
    std::uniform_int_distribution<int> cost_distribution{0, 25};

    for (std::size_t iteration = 0; iteration < 400U; ++iteration) {
        const std::size_t observation_count = size_distribution(random);
        const std::size_t track_count = size_distribution(random);
        std::vector<AssociationEdge> edges;
        for (std::size_t observation = 0; observation < observation_count; ++observation) {
            for (std::size_t track = 0; track < track_count; ++track) {
                if (include_edge(random)) {
                    edges.push_back(AssociationEdge{
                        observation,
                        track,
                        static_cast<double>(cost_distribution(random)),
                    });
                }
            }
        }

        const auto assignment = solve_association(observation_count, track_count, edges);
        const Objective actual = objective_of(assignment, observation_count, track_count, edges);
        const Objective expected = brute_force_objective(observation_count, track_count, edges);
        REQUIRE(actual.cardinality == expected.cardinality);
        REQUIRE(actual.cost == expected.cost);

        std::shuffle(edges.begin(), edges.end(), random);
        REQUIRE(solve_association(observation_count, track_count, edges) == assignment);
    }
}

struct TestCase {
    std::string_view name;
    void (*run)();
};

} // namespace

int main() {
    const TestCase tests[]{
        {"maximum cardinality precedes cost", maximum_cardinality_precedes_cost},
        {"minimum cost breaks equal cardinality", minimum_cost_breaks_equal_cardinality},
        {"edge input order invariance", edge_input_order_does_not_change_results},
        {"stable ties", ties_are_stable_without_prescribing_a_particular_optimum},
        {"disconnected, forbidden, and empty inputs", disconnected_forbidden_and_empty_inputs},
        {"duplicate edge canonicalization", duplicate_edges_keep_the_lowest_cost},
        {"malformed edge rejection", malformed_edges_are_rejected},
        {"seeded brute-force oracle", seeded_random_graphs_match_brute_force_oracle},
    };

    std::size_t failures = 0U;
    for (const TestCase& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    if (failures != 0U) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All " << std::size(tests) << " association tests passed\n";
    return 0;
}
