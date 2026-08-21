#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace naturalehia::fauna::detail {

inline constexpr std::size_t kUnassignedTrack = std::numeric_limits<std::size_t>::max();

struct AssociationEdge {
    std::size_t observation_index{};
    std::size_t track_index{};
    double cost{};
};

// Finds a maximum-cardinality matching and, among all such matchings, one with minimum cost.
// Invalid edges are rejected with std::invalid_argument. Repeated observation/track pairs are
// canonicalized to their lowest supplied cost.
[[nodiscard]] std::vector<std::size_t> solve_association(std::size_t observation_count,
                                                         std::size_t track_count,
                                                         std::span<const AssociationEdge> edges);

} // namespace naturalehia::fauna::detail
