#include <naturalehia/fauna/engine.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

constexpr std::int64_t kInitialTimeNs = 1'000'000'000LL;
constexpr std::int64_t kFramePeriodNs = 100'000'000LL;
constexpr double kEntitySpacingM = 6.0;
constexpr double kMotionPerFrameM = 2.0;
constexpr double kPositionStddevM = 0.5;
constexpr double kMaximumTrajectoryErrorM = kEntitySpacingM / 2.0;

struct Options {
    std::size_t entities{256U};
    std::size_t frames{10U};
    std::size_t repetitions{5U};
    bool show_help{false};
};

struct RunResult {
    std::uint64_t elapsed_ns{};
    naturalehia::fauna::BatchReport report{};
    std::size_t final_tracks{};
};

void print_help(std::ostream& output) {
    output << "The Elder Brother of Fauna: end-to-end association benchmark\n\n"
           << "Usage: fauna_association_benchmark [options]\n\n"
           << "Options:\n"
           << "  --entities N      Parallel animal tracks per frame (default: 256)\n"
           << "  --frames N        Timed frames per repetition (default: 10)\n"
           << "  --repetitions N   Independent timed repetitions (default: 5)\n"
           << "  --help             Show this help text\n\n"
           << "Only TrackingEngine::ingest calls for untagged frames are timed. Track\n"
           << "initialization and deterministic workload generation are excluded.\n";
}

[[nodiscard]] std::uint64_t parse_unsigned(std::string_view text, std::string_view option) {
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (text.empty() || result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument(std::string(option) + " expects a positive integer");
    }
    return value;
}

[[nodiscard]] std::size_t parse_positive_size(std::string_view text, std::string_view option) {
    const std::uint64_t value = parse_unsigned(text, option);
    if (value == 0U) {
        throw std::invalid_argument(std::string(option) + " must be greater than zero");
    }
    if constexpr (sizeof(std::size_t) < sizeof(std::uint64_t)) {
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw std::invalid_argument(std::string(option) + " is too large on this platform");
        }
    }
    return static_cast<std::size_t>(value);
}

[[nodiscard]] Options parse_options(int argc, char* argv[]) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--help") {
            options.show_help = true;
            continue;
        }
        if (argument != "--entities" && argument != "--frames" && argument != "--repetitions") {
            throw std::invalid_argument("unknown option: " + std::string(argument));
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument(std::string(argument) + " requires a value");
        }

        const std::string_view value{argv[++index]};
        if (argument == "--entities") {
            options.entities = parse_positive_size(value, argument);
        } else if (argument == "--frames") {
            options.frames = parse_positive_size(value, argument);
        } else if (argument == "--repetitions") {
            options.repetitions = parse_positive_size(value, argument);
        }
    }
    return options;
}

[[nodiscard]] std::size_t checked_product(std::size_t left, std::size_t right,
                                          std::string_view description) {
    if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
        throw std::invalid_argument(std::string(description) + " is too large");
    }
    return left * right;
}

void validate_options(const Options& options) {
    static_cast<void>(
        checked_product(options.entities, options.frames, "the measured observation count"));

    if (options.frames >
        static_cast<std::size_t>((std::numeric_limits<std::int64_t>::max() - kInitialTimeNs) /
                                 kFramePeriodNs)) {
        throw std::invalid_argument("--frames exceeds the timestamp range");
    }

    const std::uint64_t entities = static_cast<std::uint64_t>(options.entities);
    const std::uint64_t frames = static_cast<std::uint64_t>(options.frames);
    if (frames == std::numeric_limits<std::uint64_t>::max() ||
        entities > std::numeric_limits<std::uint64_t>::max() / (frames + 1U)) {
        throw std::invalid_argument("--entities and --frames exceed the sequence range");
    }
}

void fill_frame(std::span<naturalehia::fauna::Observation> observations, std::size_t frame_index) {
    const std::uint64_t entity_count = static_cast<std::uint64_t>(observations.size());
    const std::uint64_t sequence_base = static_cast<std::uint64_t>(frame_index) * entity_count;
    const std::int64_t timestamp =
        kInitialTimeNs + static_cast<std::int64_t>(frame_index) * kFramePeriodNs;
    const double displacement = static_cast<double>(frame_index) * kMotionPerFrameM;

    for (std::size_t index = 0U; index < observations.size(); ++index) {
        const std::uint64_t identity = static_cast<std::uint64_t>(index) + 1U;
        const double position_x = static_cast<double>(index) * kEntitySpacingM + displacement;
        const std::optional<std::uint64_t> external_identity =
            frame_index == 0U ? std::optional<std::uint64_t>{identity} : std::nullopt;
        observations[index] = naturalehia::fauna::Observation{
            naturalehia::fauna::SourceId{1U},
            sequence_base + identity,
            naturalehia::fauna::TimePoint{timestamp},
            naturalehia::fauna::FrameId{1U},
            naturalehia::fauna::TaxonId{1U},
            external_identity,
            naturalehia::fauna::IndividualDetection{naturalehia::fauna::Vec3{position_x, 0.0, 0.0},
                                                    kPositionStddevM}};
    }
}

void add_report(naturalehia::fauna::BatchReport& total,
                const naturalehia::fauna::BatchReport& batch) noexcept {
    total.accepted += batch.accepted;
    total.matched_by_alias += batch.matched_by_alias;
    total.matched_by_distance += batch.matched_by_distance;
    total.created += batch.created;
    total.ambiguous += batch.ambiguous;
    total.invalid += batch.invalid;
    total.duplicate_or_late += batch.duplicate_or_late;
    total.alias_conflicts += batch.alias_conflicts;
}

void validate_seed(const naturalehia::fauna::BatchReport& report, std::size_t entities,
                   std::size_t final_tracks) {
    if (report.accepted != entities || report.created != entities || final_tracks != entities ||
        report.matched_by_alias != 0U || report.matched_by_distance != 0U ||
        report.ambiguous != 0U || report.invalid != 0U || report.duplicate_or_late != 0U ||
        report.alias_conflicts != 0U) {
        throw std::runtime_error("failed to initialize the deterministic benchmark workload");
    }
}

void validate_measurement(const RunResult& result, std::size_t expected_observations,
                          std::size_t entities) {
    const auto& report = result.report;
    if (report.accepted != expected_observations ||
        report.matched_by_distance != expected_observations || report.created != 0U ||
        report.matched_by_alias != 0U || report.ambiguous != 0U || report.invalid != 0U ||
        report.duplicate_or_late != 0U || report.alias_conflicts != 0U ||
        result.final_tracks != entities) {
        throw std::runtime_error(
            "benchmark workload did not produce one distance match per animal");
    }
}

void validate_identity_continuity(const naturalehia::fauna::TrackingEngine& engine,
                                  std::span<const naturalehia::fauna::EntityId> identities,
                                  std::size_t frame_index) {
    const double expected_displacement = static_cast<double>(frame_index) * kMotionPerFrameM;
    const auto expected_timestamp = naturalehia::fauna::TimePoint{
        kInitialTimeNs + static_cast<std::int64_t>(frame_index) * kFramePeriodNs};
    const std::size_t maximum_count = std::numeric_limits<std::uint32_t>::max();
    const std::uint32_t expected_count =
        static_cast<std::uint32_t>(std::min(frame_index + 1U, maximum_count));

    std::optional<double> reference_x;
    for (std::size_t index = 0U; index < identities.size(); ++index) {
        const naturalehia::fauna::Track* const track = engine.find(identities[index]);
        if (track == nullptr || track->last_observed_at != expected_timestamp ||
            track->observation_count != expected_count) {
            throw std::runtime_error("benchmark identity continuity metadata changed");
        }
        const auto* const state = std::get_if<naturalehia::fauna::IndividualState>(&track->state);
        if (state == nullptr) {
            throw std::runtime_error("benchmark identity changed entity kind");
        }

        const naturalehia::fauna::Vec3 position = naturalehia::fauna::position_of(*state);
        const double expected_x =
            static_cast<double>(index) * kEntitySpacingM + expected_displacement;
        if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
            !std::isfinite(position.z) ||
            std::abs(position.x - expected_x) >= kMaximumTrajectoryErrorM ||
            std::abs(position.y) > kPositionStddevM || std::abs(position.z) > kPositionStddevM) {
            throw std::runtime_error("benchmark identity left its expected trajectory");
        }

        if (!reference_x.has_value()) {
            reference_x = position.x;
            continue;
        }
        const double expected_offset = static_cast<double>(index) * kEntitySpacingM;
        const double tolerance =
            128.0 * std::numeric_limits<double>::epsilon() * (1.0 + std::abs(expected_offset));
        if (std::abs((position.x - *reference_x) - expected_offset) > tolerance) {
            throw std::runtime_error("benchmark identity-to-trajectory mapping changed");
        }
    }
}

[[nodiscard]] RunResult run_once(const Options& options) {
    naturalehia::fauna::TrackingEngine engine;
    std::vector<naturalehia::fauna::Observation> observations(options.entities);

    fill_frame(observations, 0U);
    const naturalehia::fauna::BatchReport seed_report = engine.ingest(observations);
    validate_seed(seed_report, options.entities, engine.tracks().size());
    std::vector<naturalehia::fauna::EntityId> identities;
    identities.reserve(options.entities);
    for (const naturalehia::fauna::Track& track : engine.tracks()) {
        identities.push_back(track.id);
    }

    RunResult result;
    for (std::size_t frame_offset = 0U; frame_offset < options.frames; ++frame_offset) {
        const std::size_t frame_index = frame_offset + 1U;
        fill_frame(observations, frame_index);
        const auto start = std::chrono::steady_clock::now();
        const naturalehia::fauna::BatchReport frame_report = engine.ingest(observations);
        const auto finish = std::chrono::steady_clock::now();
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
        if (elapsed < 0 || static_cast<std::uint64_t>(elapsed) >
                               std::numeric_limits<std::uint64_t>::max() - result.elapsed_ns) {
            throw std::runtime_error("benchmark timer range exceeded");
        }
        result.elapsed_ns += static_cast<std::uint64_t>(elapsed);
        add_report(result.report, frame_report);
        validate_identity_continuity(engine, identities, frame_index);
    }
    result.final_tracks = engine.tracks().size();

    const std::size_t expected_observations =
        checked_product(options.entities, options.frames, "the measured observation count");
    validate_measurement(result, expected_observations, options.entities);
    return result;
}

void print_result(const Options& options, std::vector<std::uint64_t> elapsed_samples,
                  const RunResult& representative) {
    std::sort(elapsed_samples.begin(), elapsed_samples.end());
    const std::uint64_t minimum = elapsed_samples.front();
    const std::uint64_t median = elapsed_samples[elapsed_samples.size() / 2U];
    const std::uint64_t maximum = elapsed_samples.back();
    const std::size_t observations =
        checked_product(options.entities, options.frames, "the measured observation count");
    const double observations_as_double = static_cast<double>(observations);
    const double median_as_double = static_cast<double>(median);
    const double ns_per_observation =
        median == 0U ? 0.0 : median_as_double / observations_as_double;
    const double observations_per_second =
        median == 0U ? 0.0 : observations_as_double * 1'000'000'000.0 / median_as_double;

    std::cout << std::fixed << std::setprecision(3) << "benchmark=fauna_association_v1"
              << " workload=parallel_individuals"
              << " scope=tracking_engine_ingest"
              << " entities=" << options.entities << " frames=" << options.frames
              << " repetitions=" << options.repetitions
              << " observations_per_repetition=" << observations << " elapsed_ns_min=" << minimum
              << " elapsed_ns_p50=" << median << " elapsed_ns_max=" << maximum
              << " ns_per_observation_p50=" << ns_per_observation
              << " observations_per_second_p50=" << observations_per_second
              << " matched_by_distance_per_repetition=" << representative.report.matched_by_distance
              << " final_tracks=" << representative.final_tracks << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Options options = parse_options(argc, argv);
        if (options.show_help) {
            print_help(std::cout);
            return 0;
        }
        validate_options(options);

        std::vector<std::uint64_t> elapsed_samples;
        elapsed_samples.reserve(options.repetitions);
        RunResult representative;
        for (std::size_t repetition = 0U; repetition < options.repetitions; ++repetition) {
            RunResult result = run_once(options);
            elapsed_samples.push_back(result.elapsed_ns);
            representative = result;
        }
        print_result(options, std::move(elapsed_samples), representative);
        return 0;
    } catch (const std::invalid_argument& error) {
        std::cerr << "error: " << error.what() << "\nRun with --help for usage.\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
