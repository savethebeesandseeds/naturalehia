#include <naturalehia/synthetic_source.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace naturalehia {
namespace {

constexpr double kNanosecondsPerSecond = 1'000'000'000.0;
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kMinimumReportedStddev = 1.0e-6;
constexpr double kMaximumStandardDeviation = 9.480751908109176e153;
constexpr double kNormalSampleSafetyFactor = 64.0;

[[nodiscard]] bool finite(double value) noexcept { return std::isfinite(value) != 0; }

[[nodiscard]] bool safe_to_square(double value) noexcept {
    return finite(value) && std::abs(value) <= kMaximumStandardDeviation;
}

[[nodiscard]] std::uint64_t negative_magnitude(std::int64_t value) noexcept {
    // This form is defined for INT64_MIN, unlike a direct unary minus.
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] std::uint64_t positive_time_room(std::int64_t start) noexcept {
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if (start >= 0) {
        return static_cast<std::uint64_t>(maximum - start);
    }
    return static_cast<std::uint64_t>(maximum) + negative_magnitude(start);
}

void validate_config(const SyntheticConfig& config) {
    if (config.source.value == 0U || config.frame.value == 0U ||
        config.individual_taxon.value == 0U || config.colony_taxon.value == 0U) {
        throw std::invalid_argument("synthetic source, frame, and taxon IDs must be non-zero");
    }
    if (config.tick_ns <= 0) {
        throw std::invalid_argument("synthetic tick must be positive");
    }
    if (!finite(config.area_size_m) || config.area_size_m <= 0.0) {
        throw std::invalid_argument("synthetic area size must be finite and positive");
    }
    if (!finite(config.maximum_individual_speed_mps) || config.maximum_individual_speed_mps < 0.0) {
        throw std::invalid_argument("synthetic maximum speed must be finite and non-negative");
    }
    if (!safe_to_square(config.position_noise_stddev_m) || config.position_noise_stddev_m < 0.0 ||
        !safe_to_square(config.population_noise_stddev) || config.population_noise_stddev < 0.0) {
        throw std::invalid_argument("synthetic noise values must be finite and non-negative");
    }
    if (!finite(config.missed_detection_probability) || config.missed_detection_probability < 0.0 ||
        config.missed_detection_probability > 1.0 || !finite(config.tag_visibility_probability) ||
        config.tag_visibility_probability < 0.0 || config.tag_visibility_probability > 1.0) {
        throw std::invalid_argument("synthetic probabilities must be within [0, 1]");
    }
    if (!finite(config.colony_population_mean) || config.colony_population_mean < 0.0 ||
        !safe_to_square(config.colony_population_trend_stddev_per_second) ||
        config.colony_population_trend_stddev_per_second < 0.0) {
        throw std::invalid_argument(
            "synthetic colony population values must be finite and non-negative");
    }
    if (config.colony_count > std::numeric_limits<std::size_t>::max() - config.individual_count) {
        throw std::invalid_argument("synthetic entity count is too large");
    }

    const std::size_t entities_per_step = config.individual_count + config.colony_count;
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t)) {
        if (entities_per_step >
                static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max()) ||
            config.steps > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max())) {
            throw std::invalid_argument("synthetic source limits exceed 64-bit counters");
        }
    }
    const auto entity_count = static_cast<std::uint64_t>(entities_per_step);
    const auto step_count = static_cast<std::uint64_t>(config.steps);
    if (entity_count != 0U &&
        step_count > std::numeric_limits<std::uint64_t>::max() / entity_count) {
        throw std::invalid_argument("synthetic observation sequence would overflow");
    }

    if (config.steps > 1U) {
        const auto intervals = static_cast<std::uint64_t>(config.steps - 1U);
        const auto tick = static_cast<std::uint64_t>(config.tick_ns);
        if (intervals > positive_time_room(config.start_time.nanoseconds) / tick) {
            throw std::invalid_argument("synthetic timeline would overflow");
        }
    }

    const double elapsed_seconds = static_cast<double>(config.tick_ns) / kNanosecondsPerSecond;
    const double doubled_area = 2.0 * config.area_size_m;
    const double maximum_displacement = config.maximum_individual_speed_mps * elapsed_seconds;
    const double maximum_noisy_coordinate =
        config.area_size_m + kNormalSampleSafetyFactor * config.position_noise_stddev_m;
    const double maximum_population = 2.0 * config.colony_population_mean;
    const double maximum_noisy_population =
        maximum_population + kNormalSampleSafetyFactor * config.population_noise_stddev;
    const double maximum_population_step =
        maximum_population + kNormalSampleSafetyFactor *
                                 config.colony_population_trend_stddev_per_second * elapsed_seconds;
    if (!finite(doubled_area) || !finite(maximum_displacement) ||
        !finite(config.area_size_m + maximum_displacement) || !finite(maximum_noisy_coordinate) ||
        !finite(maximum_population) || !finite(maximum_noisy_population) ||
        !finite(maximum_population_step)) {
        throw std::invalid_argument("synthetic configuration would overflow numerical state");
    }
}

class DeterministicRandom {
  public:
    explicit DeterministicRandom(std::uint64_t seed) noexcept : state_(seed) {}

    [[nodiscard]] std::uint64_t next() noexcept {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31U);
    }

    [[nodiscard]] double unit() noexcept {
        constexpr double scale = 1.0 / 9'007'199'254'740'992.0;
        return static_cast<double>(next() >> 11U) * scale;
    }

    [[nodiscard]] double uniform(double lower, double upper) noexcept {
        return lower + (upper - lower) * unit();
    }

    [[nodiscard]] bool event(double probability) noexcept {
        if (probability <= 0.0) {
            return false;
        }
        if (probability >= 1.0) {
            return true;
        }
        return unit() < probability;
    }

    [[nodiscard]] double normal(double standard_deviation) noexcept {
        if (standard_deviation <= 0.0) {
            return 0.0;
        }
        const double first = std::max(unit(), std::numeric_limits<double>::min());
        const double second = unit();
        const double standard_normal =
            std::sqrt(-2.0 * std::log(first)) * std::cos(kTwoPi * second);
        return standard_deviation * standard_normal;
    }

  private:
    std::uint64_t state_;
};

struct IndividualTruth {
    Vec3 position_m{};
    Vec3 velocity_mps{};
    std::uint64_t external_id{};
};

struct ColonyTruth {
    Vec3 centroid_m{};
    double population{};
    double population_trend_per_second{};
    std::uint64_t external_id{};
};

void reflect_axis(double& position, double& velocity, double boundary) noexcept {
    const double unfolded = position;
    const double period = 2.0 * boundary;
    double folded = std::fmod(unfolded, period);
    if (folded < 0.0) {
        folded += period;
    }
    if (folded > boundary) {
        position = period - folded;
        velocity = -velocity;
    } else {
        position = folded;
    }
}

} // namespace

struct SyntheticSource::Impl {
    explicit Impl(SyntheticConfig supplied_config)
        : config(std::move(supplied_config)), random(config.seed), current(config.start_time) {
        validate_config(config);

        const std::size_t entity_count = config.individual_count + config.colony_count;
        individuals.reserve(config.individual_count);
        colonies.reserve(config.colony_count);
        pending.reserve(entity_count);

        for (std::size_t index = 0; index < config.individual_count; ++index) {
            const double angle = random.uniform(0.0, kTwoPi);
            const double speed = random.uniform(0.25 * config.maximum_individual_speed_mps,
                                                config.maximum_individual_speed_mps);
            individuals.push_back({
                {random.uniform(0.0, config.area_size_m), random.uniform(0.0, config.area_size_m),
                 0.0},
                {std::cos(angle) * speed, std::sin(angle) * speed, 0.0},
                static_cast<std::uint64_t>(index) + 1U,
            });
        }

        for (std::size_t index = 0; index < config.colony_count; ++index) {
            const double population_scale = random.uniform(0.75, 1.25);
            const double trend = random.normal(config.colony_population_trend_stddev_per_second);
            colonies.push_back({
                {random.uniform(0.0, config.area_size_m), random.uniform(0.0, config.area_size_m),
                 0.0},
                config.colony_population_mean * population_scale,
                trend,
                static_cast<std::uint64_t>(config.individual_count) +
                    static_cast<std::uint64_t>(index) + 1U,
            });
        }
    }

    void advance_truth() noexcept {
        const double elapsed = static_cast<double>(config.tick_ns) / kNanosecondsPerSecond;
        for (auto& individual : individuals) {
            individual.position_m.x += individual.velocity_mps.x * elapsed;
            individual.position_m.y += individual.velocity_mps.y * elapsed;
            reflect_axis(individual.position_m.x, individual.velocity_mps.x, config.area_size_m);
            reflect_axis(individual.position_m.y, individual.velocity_mps.y, config.area_size_m);
        }

        const double lower_population = 0.25 * config.colony_population_mean;
        const double upper_population = 2.0 * config.colony_population_mean;
        for (auto& colony : colonies) {
            colony.population += colony.population_trend_per_second * elapsed;
            if (colony.population < lower_population) {
                colony.population = lower_population;
                colony.population_trend_per_second = std::abs(colony.population_trend_per_second);
            } else if (colony.population > upper_population) {
                colony.population = upper_population;
                colony.population_trend_per_second = -std::abs(colony.population_trend_per_second);
            }
        }
    }

    [[nodiscard]] Vec3 noisy_position(Vec3 truth) noexcept {
        return {
            truth.x + random.normal(config.position_noise_stddev_m),
            truth.y + random.normal(config.position_noise_stddev_m),
            truth.z + random.normal(config.position_noise_stddev_m),
        };
    }

    void generate_step() {
        pending.clear();
        pending_offset = 0U;

        if (completed != 0U) {
            advance_truth();
            current.nanoseconds += config.tick_ns;
        }

        const double position_stddev =
            std::max(config.position_noise_stddev_m, kMinimumReportedStddev);
        const double population_stddev =
            std::max(config.population_noise_stddev, kMinimumReportedStddev);

        for (const auto& individual : individuals) {
            if (random.event(config.missed_detection_probability)) {
                continue;
            }

            Observation observation;
            observation.source = config.source;
            observation.observed_at = current;
            observation.frame = config.frame;
            observation.taxon = config.individual_taxon;
            if (random.event(config.tag_visibility_probability)) {
                observation.external_entity_id = individual.external_id;
            }
            observation.measurement =
                IndividualDetection{noisy_position(individual.position_m), position_stddev};
            pending.push_back(std::move(observation));
        }

        for (const auto& colony : colonies) {
            if (random.event(config.missed_detection_probability)) {
                continue;
            }

            Observation observation;
            observation.source = config.source;
            observation.observed_at = current;
            observation.frame = config.frame;
            observation.taxon = config.colony_taxon;
            if (random.event(config.tag_visibility_probability)) {
                observation.external_entity_id = colony.external_id;
            }
            observation.measurement = ColonySurvey{
                noisy_position(colony.centroid_m),
                position_stddev,
                std::max(0.0, colony.population + random.normal(config.population_noise_stddev)),
                population_stddev,
            };
            pending.push_back(std::move(observation));
        }

        // Stable identifiers carry stronger association evidence. Emit them first so the
        // ordering remains safe even when a caller's bounded buffer splits one time step.
        std::stable_partition(pending.begin(), pending.end(), [](const Observation& observation) {
            return observation.external_entity_id.has_value();
        });
        for (Observation& observation : pending) {
            observation.sequence = next_sequence++;
        }

        ++completed;
    }

    SyntheticConfig config;
    DeterministicRandom random;
    std::vector<IndividualTruth> individuals;
    std::vector<ColonyTruth> colonies;
    std::vector<Observation> pending;
    std::size_t pending_offset{};
    std::size_t completed{};
    std::uint64_t next_sequence{1U};
    TimePoint current{};
};

SyntheticSource::SyntheticSource(SyntheticConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

SyntheticSource::~SyntheticSource() = default;
SyntheticSource::SyntheticSource(SyntheticSource&&) noexcept = default;
SyntheticSource& SyntheticSource::operator=(SyntheticSource&&) noexcept = default;

SourceId SyntheticSource::id() const noexcept { return impl_->config.source; }

ReadResult SyntheticSource::read(std::span<Observation> output) {
    if (impl_->pending_offset >= impl_->pending.size() && impl_->completed >= impl_->config.steps) {
        return {0U, ReadStatus::end, {}};
    }
    if (output.empty()) {
        return {0U, ReadStatus::would_block, "output buffer has no capacity"};
    }

    if (impl_->pending_offset >= impl_->pending.size() && impl_->completed < impl_->config.steps) {
        impl_->generate_step();
    }

    if (impl_->pending_offset >= impl_->pending.size()) {
        return {0U,
                impl_->completed >= impl_->config.steps ? ReadStatus::end : ReadStatus::would_block,
                {}};
    }

    const std::size_t remaining = impl_->pending.size() - impl_->pending_offset;
    const std::size_t count = std::min(output.size(), remaining);
    std::copy_n(impl_->pending.begin() + static_cast<std::ptrdiff_t>(impl_->pending_offset), count,
                output.begin());
    impl_->pending_offset += count;
    return {count, ReadStatus::data, {}};
}

const SyntheticConfig& SyntheticSource::config() const noexcept { return impl_->config; }

TimePoint SyntheticSource::current_time() const noexcept { return impl_->current; }

std::size_t SyntheticSource::completed_steps() const noexcept { return impl_->completed; }

} // namespace naturalehia
