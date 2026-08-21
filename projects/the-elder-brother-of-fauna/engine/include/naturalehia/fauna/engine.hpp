#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include <naturalehia/fauna/model.hpp>

namespace naturalehia::fauna {

struct EngineConfig {
    double association_gate_sigma{4.0};
    double ambiguity_margin_sigma{0.25};
    std::uint32_t confirmations_required{3};
    std::int64_t stale_after_ns{30'000'000'000LL};
    double individual_acceleration_stddev_mps2{1.5};
    double colony_drift_stddev_mps2{0.05};
    double population_process_stddev_per_second{2.0};
    double initial_velocity_stddev_mps{10.0};
};

struct BatchReport {
    std::size_t accepted{};
    std::size_t matched_by_alias{};
    std::size_t matched_by_distance{};
    std::size_t created{};
    std::size_t ambiguous{};
    std::size_t invalid{};
    std::size_t duplicate_or_late{};
    std::size_t alias_conflicts{};
};

enum class RenameResult : std::uint8_t {
    renamed,
    not_found,
    empty_name,
    name_in_use,
};

class TrackingEngine {
  public:
    explicit TrackingEngine(EngineConfig config = {});
    ~TrackingEngine();

    TrackingEngine(TrackingEngine&&) noexcept;
    TrackingEngine& operator=(TrackingEngine&&) noexcept;

    TrackingEngine(const TrackingEngine&) = delete;
    TrackingEngine& operator=(const TrackingEngine&) = delete;

    [[nodiscard]] BatchReport ingest(std::span<const Observation> observations);
    // Moves the lifecycle watermark forward. Calls that move backwards are ignored, and later
    // ingest calls reject observations older than this watermark.
    void advance_to(TimePoint now) noexcept;

    // Views and pointers remain valid only until the next mutating engine call.
    [[nodiscard]] std::span<const Track> tracks() const noexcept;
    [[nodiscard]] const Track* find(EntityId id) const noexcept;
    [[nodiscard]] RenameResult rename(EntityId id, std::string new_name);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace naturalehia::fauna
