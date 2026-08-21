#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace naturalehia::fauna {

struct EntityId {
    std::uint64_t value{};
    auto operator<=>(const EntityId&) const = default;
};

struct SourceId {
    std::uint32_t value{};
    auto operator<=>(const SourceId&) const = default;
};

struct TaxonId {
    std::uint32_t value{};
    auto operator<=>(const TaxonId&) const = default;
};

struct FrameId {
    std::uint32_t value{};
    auto operator<=>(const FrameId&) const = default;
};

struct TimePoint {
    std::int64_t nanoseconds{};
    auto operator<=>(const TimePoint&) const = default;
};

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

enum class EntityKind : std::uint8_t {
    individual,
    colony,
};

enum class TrackPhase : std::uint8_t {
    tentative,
    confirmed,
    stale,
};

struct IndividualDetection {
    Vec3 position_m{};
    double position_stddev_m{1.0};
};

struct ColonySurvey {
    Vec3 centroid_m{};
    double position_stddev_m{1.0};
    double estimated_population{};
    double population_stddev{1.0};
};

using Measurement = std::variant<IndividualDetection, ColonySurvey>;

struct Observation {
    SourceId source{};
    std::uint64_t sequence{};
    TimePoint observed_at{};
    FrameId frame{};
    TaxonId taxon{};
    std::optional<std::uint64_t> external_entity_id{};
    Measurement measurement{};
};

struct AxisState {
    double position_m{};
    double velocity_mps{};
    double position_variance{};
    double position_velocity_covariance{};
    double velocity_variance{};
};

struct IndividualState {
    std::array<AxisState, 3> axes{};
};

struct ColonyState {
    std::array<AxisState, 3> centroid_axes{};
    double estimated_population{};
    double population_variance{};
};

using EstimatedState = std::variant<IndividualState, ColonyState>;

struct Track {
    EntityId id{};
    std::string display_name{};
    EntityKind kind{EntityKind::individual};
    TaxonId taxon{};
    FrameId frame{};
    TrackPhase phase{TrackPhase::tentative};
    TimePoint created_at{};
    TimePoint last_observed_at{};
    std::uint32_t observation_count{};
    // The estimate is evaluated at last_observed_at. advance_to() updates lifecycle age only.
    EstimatedState state{};
};

[[nodiscard]] constexpr EntityKind kind_of(const Measurement& measurement) noexcept {
    return std::holds_alternative<IndividualDetection>(measurement) ? EntityKind::individual
                                                                    : EntityKind::colony;
}

[[nodiscard]] constexpr Vec3 position_of(const IndividualState& state) noexcept {
    return {state.axes[0].position_m, state.axes[1].position_m, state.axes[2].position_m};
}

[[nodiscard]] constexpr Vec3 velocity_of(const IndividualState& state) noexcept {
    return {state.axes[0].velocity_mps, state.axes[1].velocity_mps, state.axes[2].velocity_mps};
}

[[nodiscard]] constexpr Vec3 centroid_of(const ColonyState& state) noexcept {
    return {
        state.centroid_axes[0].position_m,
        state.centroid_axes[1].position_m,
        state.centroid_axes[2].position_m,
    };
}

[[nodiscard]] const char* to_string(EntityKind kind) noexcept;
[[nodiscard]] const char* to_string(TrackPhase phase) noexcept;

} // namespace naturalehia::fauna
