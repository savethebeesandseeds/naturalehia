#include "association.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace naturalehia::fauna::detail {
namespace {

struct ResidualEdge {
    std::size_t to{};
    std::size_t reverse_index{};
    bool has_capacity{};
    long double cost{};
};

struct Predecessor {
    std::size_t vertex{kUnassignedTrack};
    std::size_t edge_index{kUnassignedTrack};
};

using ResidualGraph = std::vector<std::vector<ResidualEdge>>;

[[nodiscard]] std::vector<AssociationEdge>
canonicalize_edges(std::size_t observation_count, std::size_t track_count,
                   std::span<const AssociationEdge> edges) {
    std::vector<AssociationEdge> canonical;
    canonical.reserve(edges.size());
    for (const AssociationEdge& edge : edges) {
        if (edge.observation_index >= observation_count || edge.track_index >= track_count) {
            throw std::invalid_argument("association edge endpoint is out of range");
        }
        if (std::isfinite(edge.cost) == 0 || edge.cost < 0.0) {
            throw std::invalid_argument("association edge cost must be finite and nonnegative");
        }

        AssociationEdge normalized = edge;
        if (normalized.cost == 0.0) {
            normalized.cost = 0.0; // Canonicalize negative zero.
        }
        canonical.push_back(normalized);
    }

    std::sort(canonical.begin(), canonical.end(),
              [](const AssociationEdge& left, const AssociationEdge& right) {
                  if (left.observation_index != right.observation_index) {
                      return left.observation_index < right.observation_index;
                  }
                  if (left.track_index != right.track_index) {
                      return left.track_index < right.track_index;
                  }
                  return left.cost < right.cost;
              });

    std::vector<AssociationEdge> unique;
    unique.reserve(canonical.size());
    for (const AssociationEdge& edge : canonical) {
        if (!unique.empty() && unique.back().observation_index == edge.observation_index &&
            unique.back().track_index == edge.track_index) {
            continue;
        }
        unique.push_back(edge);
    }
    return unique;
}

void add_unit_edge(ResidualGraph& graph, std::size_t from, std::size_t to, long double cost) {
    const std::size_t forward_index = graph[from].size();
    const std::size_t reverse_index = graph[to].size();
    graph[from].push_back(ResidualEdge{to, reverse_index, true, cost});
    graph[to].push_back(ResidualEdge{from, forward_index, false, -cost});
}

[[nodiscard]] bool find_shortest_augmenting_path(const ResidualGraph& graph, std::size_t source,
                                                 std::size_t sink,
                                                 std::vector<Predecessor>& predecessors) {
    const long double infinity = std::numeric_limits<long double>::infinity();
    std::vector<long double> distances(graph.size(), infinity);
    std::fill(predecessors.begin(), predecessors.end(), Predecessor{});
    distances[source] = 0.0L;

    // A shortest residual path has a simple representative because a minimum-cost flow has no
    // negative residual cycle. Bellman-Ford is used here because reverse assignment arcs have
    // negative cost after the first augmentation.
    for (std::size_t pass = 1U; pass < graph.size(); ++pass) {
        bool changed = false;
        for (std::size_t from = 0U; from < graph.size(); ++from) {
            if (distances[from] == infinity) {
                continue;
            }
            for (std::size_t edge_index = 0U; edge_index < graph[from].size(); ++edge_index) {
                const ResidualEdge& edge = graph[from][edge_index];
                if (!edge.has_capacity) {
                    continue;
                }
                const long double candidate = distances[from] + edge.cost;
                if (std::isfinite(candidate) == 0) {
                    throw std::overflow_error("association path cost overflowed");
                }
                if (candidate < distances[edge.to]) {
                    distances[edge.to] = candidate;
                    predecessors[edge.to] = Predecessor{from, edge_index};
                    changed = true;
                }
            }
        }
        if (!changed) {
            break;
        }
    }

    return predecessors[sink].vertex != kUnassignedTrack;
}

void augment_unit_flow(ResidualGraph& graph, std::size_t source, std::size_t sink,
                       const std::vector<Predecessor>& predecessors) {
    std::size_t current = sink;
    std::size_t path_length = 0U;
    while (current != source) {
        if (path_length++ >= graph.size()) {
            throw std::logic_error("association residual path contains a cycle");
        }

        const Predecessor predecessor = predecessors[current];
        if (predecessor.vertex == kUnassignedTrack || predecessor.vertex >= graph.size() ||
            predecessor.edge_index >= graph[predecessor.vertex].size()) {
            throw std::logic_error("association residual path is incomplete");
        }

        ResidualEdge& forward = graph[predecessor.vertex][predecessor.edge_index];
        if (!forward.has_capacity || forward.to != current ||
            forward.reverse_index >= graph[current].size()) {
            throw std::logic_error("association residual path is inconsistent");
        }
        ResidualEdge& reverse = graph[current][forward.reverse_index];
        forward.has_capacity = false;
        reverse.has_capacity = true;
        current = predecessor.vertex;
    }
}

} // namespace

std::vector<std::size_t> solve_association(std::size_t observation_count, std::size_t track_count,
                                           std::span<const AssociationEdge> edges) {
    constexpr std::size_t kTerminalCount = 2U;
    const std::size_t maximum_size = std::numeric_limits<std::size_t>::max();
    if (track_count > maximum_size - kTerminalCount ||
        observation_count > maximum_size - kTerminalCount - track_count) {
        throw std::length_error("association graph is too large");
    }

    const std::vector<AssociationEdge> canonical =
        canonicalize_edges(observation_count, track_count, edges);
    std::vector<std::size_t> assignments(observation_count, kUnassignedTrack);
    if (observation_count == 0U || track_count == 0U || canonical.empty()) {
        return assignments;
    }

    const std::size_t source = 0U;
    const std::size_t first_observation = 1U;
    const std::size_t first_track = first_observation + observation_count;
    const std::size_t sink = first_track + track_count;
    ResidualGraph graph(sink + 1U);

    // Unit capacities enforce at most one selected edge at each observation and track.
    for (std::size_t observation = 0U; observation < observation_count; ++observation) {
        add_unit_edge(graph, source, first_observation + observation, 0.0L);
    }
    for (const AssociationEdge& edge : canonical) {
        add_unit_edge(graph, first_observation + edge.observation_index,
                      first_track + edge.track_index, static_cast<long double>(edge.cost));
    }
    for (std::size_t track = 0U; track < track_count; ++track) {
        add_unit_edge(graph, first_track + track, sink, 0.0L);
    }

    std::vector<Predecessor> predecessors(graph.size());
    while (find_shortest_augmenting_path(graph, source, sink, predecessors)) {
        augment_unit_flow(graph, source, sink, predecessors);
    }

    for (std::size_t observation = 0U; observation < observation_count; ++observation) {
        const std::size_t vertex = first_observation + observation;
        for (const ResidualEdge& edge : graph[vertex]) {
            if (edge.to >= first_track && edge.to < sink && !edge.has_capacity) {
                assignments[observation] = edge.to - first_track;
                break;
            }
        }
    }
    return assignments;
}

} // namespace naturalehia::fauna::detail
