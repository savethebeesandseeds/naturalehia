#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include <naturalehia/fauna/source.hpp>

namespace naturalehia::fauna {

struct SyntheticConfig {
    SourceId source{1U};
    FrameId frame{1U};
    TaxonId individual_taxon{1U};
    TaxonId colony_taxon{2U};

    std::uint64_t seed{1U};
    std::size_t individual_count{12U};
    std::size_t colony_count{3U};
    std::size_t steps{60U};

    TimePoint start_time{};
    std::int64_t tick_ns{1'000'000'000LL};
    double area_size_m{10'000.0};
    double maximum_individual_speed_mps{8.0};
    double position_noise_stddev_m{5.0};
    double population_noise_stddev{50.0};
    double missed_detection_probability{0.05};
    double tag_visibility_probability{0.80};

    double colony_population_mean{20'000.0};
    double colony_population_trend_stddev_per_second{0.25};
};

// Kept as a descriptive alias for callers that prefer the longer form.
using SyntheticSourceConfig = SyntheticConfig;

class SyntheticSource final : public ObservationSource {
  public:
    explicit SyntheticSource(SyntheticConfig config = {});
    ~SyntheticSource() override;

    SyntheticSource(SyntheticSource&&) noexcept;
    SyntheticSource& operator=(SyntheticSource&&) noexcept;

    SyntheticSource(const SyntheticSource&) = delete;
    SyntheticSource& operator=(const SyntheticSource&) = delete;

    [[nodiscard]] SourceId id() const noexcept override;
    ReadResult read(std::span<Observation> output) override;

    [[nodiscard]] const SyntheticConfig& config() const noexcept;
    [[nodiscard]] TimePoint current_time() const noexcept;
    [[nodiscard]] std::size_t completed_steps() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace naturalehia::fauna
