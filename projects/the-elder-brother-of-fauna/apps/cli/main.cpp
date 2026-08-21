#include <naturalehia/fauna/engine.hpp>
#include <naturalehia/fauna/synthetic_source.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace {

struct Options {
    naturalehia::fauna::SyntheticConfig source{};
    std::size_t batch_size{256U};
};

void print_help(std::ostream& output) {
    output << "The Elder Brother of Fauna: synthetic tracking demo\n\n"
           << "Usage: naturalehia-fauna [options]\n\n"
           << "Options:\n"
           << "  --steps N        Number of simulated time steps (default: 60)\n"
           << "  --individuals N  Number of individually tracked animals (default: 12)\n"
           << "  --colonies N     Number of tracked colonies (default: 3)\n"
           << "  --seed N         Deterministic random seed (default: 1)\n"
           << "  --batch-size N   Maximum observations read at once (default: 256)\n"
           << "  --help            Show this help text\n";
}

[[nodiscard]] std::uint64_t parse_unsigned(std::string_view text, std::string_view option) {
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (text.empty() || result.ec != std::errc{} || result.ptr != end) {
        throw std::invalid_argument(std::string(option) + " expects a non-negative integer");
    }
    return value;
}

[[nodiscard]] std::size_t parse_size(std::string_view text, std::string_view option) {
    const std::uint64_t value = parse_unsigned(text, option);
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
            print_help(std::cout);
            throw std::runtime_error("help requested");
        }

        if (index + 1 >= argc) {
            throw std::invalid_argument(std::string(argument) + " requires a value");
        }
        const std::string_view value{argv[++index]};
        if (argument == "--steps") {
            options.source.steps = parse_size(value, argument);
        } else if (argument == "--individuals") {
            options.source.individual_count = parse_size(value, argument);
        } else if (argument == "--colonies") {
            options.source.colony_count = parse_size(value, argument);
        } else if (argument == "--seed") {
            options.source.seed = parse_unsigned(value, argument);
        } else if (argument == "--batch-size") {
            options.batch_size = parse_size(value, argument);
            if (options.batch_size == 0U) {
                throw std::invalid_argument("--batch-size must be greater than zero");
            }
        } else {
            throw std::invalid_argument("unknown option: " + std::string(argument));
        }
    }
    return options;
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

void print_tracks(std::span<const naturalehia::fauna::Track> tracks) {
    std::cout << "\nFinal tracks (" << tracks.size() << ")\n";
    std::cout << std::left << std::setw(8) << "ID" << std::setw(16) << "NAME" << std::setw(12)
              << "KIND" << std::setw(12) << "PHASE" << std::setw(8) << "TAXON" << std::setw(8)
              << "OBS" << std::setw(30) << "POSITION M"
              << "ESTIMATE\n";
    std::cout << std::string(112U, '-') << '\n';

    for (const auto& track : tracks) {
        naturalehia::fauna::Vec3 position{};
        naturalehia::fauna::Vec3 velocity{};
        double population{};
        const bool individual =
            std::holds_alternative<naturalehia::fauna::IndividualState>(track.state);
        if (individual) {
            const auto& state = std::get<naturalehia::fauna::IndividualState>(track.state);
            position = naturalehia::fauna::position_of(state);
            velocity = naturalehia::fauna::velocity_of(state);
        } else {
            const auto& state = std::get<naturalehia::fauna::ColonyState>(track.state);
            position = naturalehia::fauna::centroid_of(state);
            population = state.estimated_population;
        }

        std::cout << std::left << std::setw(8) << track.id.value << std::setw(16)
                  << track.display_name << std::setw(12)
                  << naturalehia::fauna::to_string(track.kind) << std::setw(12)
                  << naturalehia::fauna::to_string(track.phase) << std::setw(8) << track.taxon.value
                  << std::setw(8) << track.observation_count;

        std::cout << std::fixed << std::setprecision(1) << '(' << position.x << ", " << position.y
                  << ", " << position.z << ')';
        if (individual) {
            std::cout << "   velocity=(" << velocity.x << ", " << velocity.y << ", " << velocity.z
                      << ") m/s";
        } else {
            std::cout << "   population=" << population;
        }
        std::cout << '\n';
    }
}

void print_report(const naturalehia::fauna::BatchReport& report, std::size_t emitted,
                  std::size_t batches) {
    std::cout << "\nProcessing report\n"
              << "  emitted observations : " << emitted << '\n'
              << "  source batches       : " << batches << '\n'
              << "  accepted             : " << report.accepted << '\n'
              << "  new tracks           : " << report.created << '\n'
              << "  matched by alias     : " << report.matched_by_alias << '\n'
              << "  matched by distance  : " << report.matched_by_distance << '\n'
              << "  ambiguous            : " << report.ambiguous << '\n'
              << "  invalid              : " << report.invalid << '\n'
              << "  duplicate or late    : " << report.duplicate_or_late << '\n'
              << "  alias conflicts      : " << report.alias_conflicts << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    Options options;
    try {
        options = parse_options(argc, argv);
    } catch (const std::runtime_error& error) {
        if (std::string_view{error.what()} == "help requested") {
            return 0;
        }
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << "\nRun with --help for usage.\n";
        return 2;
    }

    try {
        naturalehia::fauna::SyntheticSource source(options.source);
        naturalehia::fauna::TrackingEngine engine;
        std::vector<naturalehia::fauna::Observation> buffer(options.batch_size);
        naturalehia::fauna::BatchReport total_report;
        std::size_t emitted{};
        std::size_t batches{};

        for (;;) {
            const naturalehia::fauna::ReadResult result = source.read(buffer);
            if (result.count > buffer.size()) {
                throw std::runtime_error("source returned more observations than the buffer holds");
            }
            if (result.status == naturalehia::fauna::ReadStatus::error) {
                throw std::runtime_error(result.message.empty() ? "synthetic source failed"
                                                                : result.message);
            }
            if (result.status == naturalehia::fauna::ReadStatus::would_block) {
                if (result.count != 0U) {
                    throw std::runtime_error("source returned data with would-block status");
                }
                continue;
            }

            if (result.count != 0U) {
                const auto observations =
                    std::span<const naturalehia::fauna::Observation>{buffer.data(), result.count};
                add_report(total_report, engine.ingest(observations));
                emitted += result.count;
                ++batches;
                engine.advance_to(observations.back().observed_at);
            } else if (result.status == naturalehia::fauna::ReadStatus::data) {
                throw std::runtime_error("synthetic source returned an empty data batch");
            }

            if (result.status == naturalehia::fauna::ReadStatus::end) {
                break;
            }
        }

        engine.advance_to(source.current_time());
        print_tracks(engine.tracks());
        print_report(total_report, emitted, batches);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
