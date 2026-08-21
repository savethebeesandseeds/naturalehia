#include <naturalehia/fauna/engine.hpp>
#include <naturalehia/fauna/synthetic_source.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

using naturalehia::fauna::BatchReport;
using naturalehia::fauna::ColonyState;
using naturalehia::fauna::ColonySurvey;
using naturalehia::fauna::EngineConfig;
using naturalehia::fauna::EntityId;
using naturalehia::fauna::EntityKind;
using naturalehia::fauna::FrameId;
using naturalehia::fauna::IndividualDetection;
using naturalehia::fauna::IndividualState;
using naturalehia::fauna::Observation;
using naturalehia::fauna::RenameResult;
using naturalehia::fauna::SourceId;
using naturalehia::fauna::TaxonId;
using naturalehia::fauna::TimePoint;
using naturalehia::fauna::TrackingEngine;
using naturalehia::fauna::TrackPhase;
using naturalehia::fauna::Vec3;

constexpr std::int64_t second = 1'000'000'000LL;

class TestFailure final : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

void require_impl(bool condition, std::string_view expression, int line) {
    if (condition) {
        return;
    }

    std::ostringstream message;
    message << "line " << line << ": requirement failed: " << expression;
    throw TestFailure(message.str());
}

#define REQUIRE(expression) require_impl(static_cast<bool>(expression), #expression, __LINE__)

Observation individual(std::uint32_t source, std::uint64_t sequence, std::int64_t time_ns,
                       Vec3 position, double position_stddev = 1.0,
                       std::optional<std::uint64_t> external_id = std::nullopt,
                       std::uint32_t taxon = 1, std::uint32_t frame = 1) {
    Observation observation{};
    observation.source = SourceId{source};
    observation.sequence = sequence;
    observation.observed_at = TimePoint{time_ns};
    observation.frame = FrameId{frame};
    observation.taxon = TaxonId{taxon};
    observation.external_entity_id = external_id;
    observation.measurement = IndividualDetection{position, position_stddev};
    return observation;
}

Observation colony(std::uint32_t source, std::uint64_t sequence, std::int64_t time_ns,
                   Vec3 centroid, double population, double position_stddev = 1.0,
                   double population_stddev = 2.0,
                   std::optional<std::uint64_t> external_id = std::nullopt,
                   std::uint32_t taxon = 10, std::uint32_t frame = 1) {
    Observation observation{};
    observation.source = SourceId{source};
    observation.sequence = sequence;
    observation.observed_at = TimePoint{time_ns};
    observation.frame = FrameId{frame};
    observation.taxon = TaxonId{taxon};
    observation.external_entity_id = external_id;
    observation.measurement = ColonySurvey{
        centroid,
        position_stddev,
        population,
        population_stddev,
    };
    return observation;
}

BatchReport ingest_one(TrackingEngine& engine, const Observation& observation) {
    return engine.ingest(std::span<const Observation>{&observation, 1});
}

const naturalehia::fauna::Track& only_track(const TrackingEngine& engine) {
    const auto tracks = engine.tracks();
    REQUIRE(tracks.size() == 1);
    return tracks.front();
}

void automatic_creation_and_naming() {
    TrackingEngine engine;

    const auto animal_report = ingest_one(engine, individual(1, 1, 0, {1.0, 2.0, 3.0}));
    REQUIRE(animal_report.accepted == 1);
    REQUIRE(animal_report.created == 1);

    const auto colony_report = ingest_one(engine, colony(2, 1, 0, {20.0, 30.0, 0.0}, 500.0));
    REQUIRE(colony_report.accepted == 1);
    REQUIRE(colony_report.created == 1);

    const auto tracks = engine.tracks();
    REQUIRE(tracks.size() == 2);
    REQUIRE(tracks[0].id != tracks[1].id);
    REQUIRE(!tracks[0].display_name.empty());
    REQUIRE(!tracks[1].display_name.empty());
    REQUIRE(tracks[0].display_name != tracks[1].display_name);
    REQUIRE(tracks[0].kind == EntityKind::individual);
    REQUIRE(tracks[1].kind == EntityKind::colony);
}

void source_alias_is_stable() {
    TrackingEngine engine;
    const auto first = individual(3, 1, 0, {0.0, 0.0, 0.0}, 0.2, 77);
    REQUIRE(ingest_one(engine, first).created == 1);
    const EntityId id = only_track(engine).id;

    const auto far_away = individual(3, 2, second, {500.0, -200.0, 10.0}, 0.2, 77);
    const auto report = ingest_one(engine, far_away);
    REQUIRE(report.accepted == 1);
    REQUIRE(report.matched_by_alias == 1);
    REQUIRE(report.created == 0);
    REQUIRE(engine.tracks().size() == 1);
    REQUIRE(only_track(engine).id == id);
    REQUIRE(only_track(engine).observation_count == 2);
}

void nearby_observations_associate_by_distance() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, individual(4, 1, 0, {0.0, 0.0, 0.0}, 1.0)).created == 1);
    const EntityId id = only_track(engine).id;

    const auto report = ingest_one(engine, individual(4, 2, second, {0.4, -0.1, 0.2}, 1.0));
    REQUIRE(report.accepted == 1);
    REQUIRE(report.matched_by_distance == 1);
    REQUIRE(report.created == 0);
    REQUIRE(engine.tracks().size() == 1);
    REQUIRE(only_track(engine).id == id);
    REQUIRE(only_track(engine).observation_count == 2);
}

void out_of_gate_observation_creates_track() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, individual(5, 1, 0, {0.0, 0.0, 0.0}, 0.1)).created == 1);

    const auto report = ingest_one(engine, individual(5, 2, second, {10'000.0, 0.0, 0.0}, 0.1));
    REQUIRE(report.accepted == 1);
    REQUIRE(report.created == 1);
    REQUIRE(report.matched_by_distance == 0);
    REQUIRE(engine.tracks().size() == 2);
}

void ambiguous_observation_is_not_assigned() {
    EngineConfig config{};
    config.ambiguity_margin_sigma = 0.5;
    TrackingEngine engine{config};

    REQUIRE(ingest_one(engine, individual(10, 1, 0, {-10.0, 0.0, 0.0}, 0.01, 101)).created == 1);
    REQUIRE(ingest_one(engine, individual(11, 1, 0, {10.0, 0.0, 0.0}, 0.01, 202)).created == 1);
    REQUIRE(engine.tracks().size() == 2);

    const auto report = ingest_one(engine, individual(12, 1, second, {0.0, 0.0, 0.0}, 100.0));
    REQUIRE(report.ambiguous == 1);
    REQUIRE(report.created == 0);
    REQUIRE(report.matched_by_distance == 0);
    REQUIRE(engine.tracks().size() == 2);
    REQUIRE(engine.tracks()[0].observation_count == 1);
    REQUIRE(engine.tracks()[1].observation_count == 1);
}

void invalid_and_duplicate_sequences_are_rejected() {
    TrackingEngine engine;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto invalid = individual(20, 1, 0, {nan, 0.0, 0.0});
    const auto invalid_report = ingest_one(engine, invalid);
    REQUIRE(invalid_report.invalid == 1);
    REQUIRE(invalid_report.accepted == 0);
    REQUIRE(engine.tracks().empty());

    const auto valid = individual(21, 1, 0, {1.0, 1.0, 1.0});
    REQUIRE(ingest_one(engine, valid).accepted == 1);
    const auto duplicate_report = ingest_one(engine, valid);
    REQUIRE(duplicate_report.duplicate_or_late == 1);
    REQUIRE(duplicate_report.accepted == 0);
    REQUIRE(engine.tracks().size() == 1);
    REQUIRE(only_track(engine).observation_count == 1);
}

void individual_filter_learns_constant_motion() {
    EngineConfig config{};
    config.individual_acceleration_stddev_mps2 = 0.1;
    TrackingEngine engine{config};

    for (std::uint64_t step = 0; step <= 12; ++step) {
        const double x = static_cast<double>(step);
        const auto observation = individual(30, step + 1, static_cast<std::int64_t>(step) * second,
                                            {x, 2.0, -1.0}, 0.15, 3001);
        REQUIRE(ingest_one(engine, observation).accepted == 1);
    }

    const auto& state = std::get<IndividualState>(only_track(engine).state);
    const Vec3 position = naturalehia::fauna::position_of(state);
    const Vec3 velocity = naturalehia::fauna::velocity_of(state);
    REQUIRE(std::isfinite(position.x));
    REQUIRE(std::isfinite(velocity.x));
    REQUIRE(std::abs(position.x - 12.0) < 0.75);
    REQUIRE(velocity.x > 0.5);
    REQUIRE(velocity.x < 1.5);
    REQUIRE(std::abs(position.y - 2.0) < 0.25);
    REQUIRE(std::abs(position.z + 1.0) < 0.25);
}

void colony_population_stays_nonnegative_and_updates() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, colony(40, 1, 0, {5.0, 6.0, 0.0}, 10.0, 0.5, 1.0, 4001)).accepted ==
            1);
    const double initial = std::get<ColonyState>(only_track(engine).state).estimated_population;

    REQUIRE(
        ingest_one(engine, colony(40, 2, second, {5.1, 5.9, 0.0}, 30.0, 0.5, 1.0, 4001)).accepted ==
        1);
    const auto& state = std::get<ColonyState>(only_track(engine).state);
    REQUIRE(std::isfinite(state.estimated_population));
    REQUIRE(state.estimated_population >= 0.0);
    REQUIRE(state.estimated_population > initial);
    REQUIRE(state.estimated_population <= 30.0);
    REQUIRE(state.population_variance >= 0.0);
}

void rename_enforces_nonempty_unique_names() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, individual(50, 1, 0, {0.0, 0.0, 0.0}, 0.1, 1, 1)).created == 1);
    REQUIRE(ingest_one(engine, individual(51, 1, 0, {0.0, 0.0, 0.0}, 0.1, 2, 2)).created == 1);
    REQUIRE(engine.tracks().size() == 2);

    const EntityId first = engine.tracks()[0].id;
    const EntityId second_id = engine.tracks()[1].id;
    const std::string second_original_name = engine.tracks()[1].display_name;

    REQUIRE(engine.rename(first, "Ada") == RenameResult::renamed);
    REQUIRE(engine.find(first) != nullptr);
    REQUIRE(engine.find(first)->display_name == "Ada");
    REQUIRE(engine.rename(second_id, "Ada") == RenameResult::name_in_use);
    REQUIRE(engine.find(second_id)->display_name == second_original_name);
    REQUIRE(engine.rename(first, "") == RenameResult::empty_name);
    REQUIRE(engine.rename(EntityId{999'999}, "Ghost") == RenameResult::not_found);
}

void automatic_names_do_not_collide_with_renames() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, individual(52, 1, 0, {0.0, 0.0, 0.0}, 0.1, 1, 1)).created == 1);
    const EntityId first = only_track(engine).id;
    REQUIRE(engine.rename(first, "animal-000002") == RenameResult::renamed);

    REQUIRE(ingest_one(engine, individual(53, 1, 0, {100.0, 0.0, 0.0}, 0.1, 2, 1)).created == 1);
    REQUIRE(engine.tracks().size() == 2);
    REQUIRE(engine.tracks()[0].display_name != engine.tracks()[1].display_name);
    REQUIRE(engine.tracks()[1].display_name == "animal-000002-2");
}

void track_lifecycle_confirms_stales_and_recovers() {
    EngineConfig config{};
    config.confirmations_required = 3;
    config.stale_after_ns = 5 * second;
    TrackingEngine engine{config};

    REQUIRE(ingest_one(engine, individual(60, 1, 0, {0.0, 0.0, 0.0}, 0.5, 6001)).accepted == 1);
    REQUIRE(only_track(engine).phase == TrackPhase::tentative);

    REQUIRE(ingest_one(engine, individual(60, 2, second, {0.1, 0.0, 0.0}, 0.5, 6001)).accepted ==
            1);
    REQUIRE(only_track(engine).phase == TrackPhase::tentative);

    REQUIRE(
        ingest_one(engine, individual(60, 3, 2 * second, {0.2, 0.0, 0.0}, 0.5, 6001)).accepted ==
        1);
    REQUIRE(only_track(engine).phase == TrackPhase::confirmed);

    engine.advance_to(TimePoint{9 * second});
    REQUIRE(only_track(engine).phase == TrackPhase::stale);

    const auto recovery =
        ingest_one(engine, individual(60, 4, 10 * second, {1.0, 0.0, 0.0}, 0.5, 6001));
    REQUIRE(recovery.accepted == 1);
    REQUIRE(recovery.matched_by_alias == 1);
    REQUIRE(only_track(engine).phase == TrackPhase::confirmed);
}

void known_aliases_are_reserved_before_distance_matching() {
    EngineConfig config{};
    config.initial_velocity_stddev_mps = 0.1;
    config.individual_acceleration_stddev_mps2 = 0.0;
    TrackingEngine engine{config};

    const std::array initial{
        individual(70, 1, 0, {0.0, 0.0, 0.0}, 0.1, 701),
        individual(70, 2, 0, {100.0, 0.0, 0.0}, 0.1, 702),
    };
    REQUIRE(engine.ingest(initial).created == 2);
    const EntityId aliased_id = engine.tracks()[0].id;

    const std::array next_frame{
        individual(70, 3, second, {0.1, 0.0, 0.0}, 0.1),
        individual(70, 4, second, {0.2, 0.0, 0.0}, 0.1, 701),
    };
    const auto report = engine.ingest(next_frame);
    REQUIRE(report.matched_by_alias == 1);
    REQUIRE(report.created == 1);
    REQUIRE(report.ambiguous == 0);
    REQUIRE(engine.find(aliased_id)->observation_count == 2);
}

void distance_assignment_is_group_wide() {
    EngineConfig config{};
    config.initial_velocity_stddev_mps = 0.1;
    config.individual_acceleration_stddev_mps2 = 0.0;
    TrackingEngine engine{config};

    const std::array initial{
        individual(75, 1, 0, {0.0, 0.0, 0.0}, 0.1, 751),
        individual(75, 2, 0, {0.8, 0.0, 0.0}, 0.1, 752),
    };
    REQUIRE(engine.ingest(initial).created == 2);

    // Sequential matching would give the first observation track A and fragment the second.
    // Frame-wide assignment instead yields the lower-total-innovation A<-0.0, B<-0.35 result.
    const std::array next_frame{
        individual(75, 3, second, {0.35, 0.0, 0.0}, 0.1),
        individual(75, 4, second, {0.0, 0.0, 0.0}, 0.1),
    };
    const auto report = engine.ingest(next_frame);
    REQUIRE(report.accepted == 2);
    REQUIRE(report.matched_by_distance == 2);
    REQUIRE(report.created == 0);
    REQUIRE(report.ambiguous == 0);
    REQUIRE(engine.tracks().size() == 2);
}

void global_assignment_prioritizes_match_count_over_local_cost() {
    EngineConfig config{};
    config.initial_velocity_stddev_mps = 0.1;
    config.individual_acceleration_stddev_mps2 = 0.0;
    TrackingEngine engine{config};
    TrackingEngine permuted_engine{config};

    const std::array initial{
        individual(76, 1, 0, {0.0, 0.0, 0.0}, 0.1, 761),
        individual(76, 2, 0, {1.0, 0.0, 0.0}, 0.1, 762),
    };
    REQUIRE(engine.ingest(initial).created == 2);
    const std::array permuted_initial{initial[1], initial[0]};
    REQUIRE(permuted_engine.ingest(permuted_initial).created == 2);

    // The first observation is locally cheaper for track A but can also use B. The second can
    // only use A. Edge-greedy selection takes A for the first observation and fragments the
    // second; a maximum-cardinality assignment uses B for the first and A for the second.
    const std::array next_frame{
        individual(76, 3, second, {0.4, 0.0, 0.0}, 0.1),
        individual(76, 4, second, {-0.5, 0.0, 0.0}, 0.1),
    };
    const auto report = engine.ingest(next_frame);
    const std::array permuted_next_frame{next_frame[1], next_frame[0]};
    const auto permuted_report = permuted_engine.ingest(permuted_next_frame);
    REQUIRE(report.accepted == 2);
    REQUIRE(report.matched_by_distance == 2);
    REQUIRE(report.created == 0);
    REQUIRE(report.ambiguous == 0);
    REQUIRE(engine.tracks().size() == 2);
    REQUIRE(engine.tracks()[0].observation_count == 2);
    REQUIRE(engine.tracks()[1].observation_count == 2);

    REQUIRE(permuted_report.accepted == report.accepted);
    REQUIRE(permuted_report.matched_by_alias == report.matched_by_alias);
    REQUIRE(permuted_report.matched_by_distance == report.matched_by_distance);
    REQUIRE(permuted_report.created == report.created);
    REQUIRE(permuted_report.ambiguous == report.ambiguous);
    REQUIRE(permuted_report.invalid == report.invalid);
    REQUIRE(permuted_report.duplicate_or_late == report.duplicate_or_late);
    REQUIRE(permuted_report.alias_conflicts == report.alias_conflicts);

    const auto tracks = engine.tracks();
    const auto permuted_tracks = permuted_engine.tracks();
    REQUIRE(permuted_tracks.size() == tracks.size());
    for (std::size_t index = 0; index < tracks.size(); ++index) {
        REQUIRE(permuted_tracks[index].id == tracks[index].id);
        REQUIRE(permuted_tracks[index].last_observed_at == tracks[index].last_observed_at);
        REQUIRE(permuted_tracks[index].observation_count == tracks[index].observation_count);
        const Vec3 position =
            naturalehia::fauna::position_of(std::get<IndividualState>(tracks[index].state));
        const Vec3 permuted_position = naturalehia::fauna::position_of(
            std::get<IndividualState>(permuted_tracks[index].state));
        REQUIRE(permuted_position.x == position.x);
        REQUIRE(permuted_position.y == position.y);
        REQUIRE(permuted_position.z == position.z);
    }
}

void source_groups_survive_interleaved_calls() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, individual(80, 1, 0, {0.0, 0.0, 0.0}, 0.2, 801)).created == 1);
    const EntityId tracked = only_track(engine).id;

    REQUIRE(ingest_one(engine, individual(80, 2, second, {0.1, 0.0, 0.0}, 0.2)).accepted == 1);
    REQUIRE(ingest_one(engine, individual(81, 1, second, {50.0, 0.0, 0.0}, 0.2, 811, 99)).created ==
            1);
    const auto duplicate_frame =
        ingest_one(engine, individual(80, 3, second, {0.2, 0.0, 0.0}, 0.2, 801));
    REQUIRE(duplicate_frame.accepted == 0);
    REQUIRE(duplicate_frame.ambiguous == 1);
    REQUIRE(engine.find(tracked)->observation_count == 2);
}

void watermark_and_alias_conflicts_are_enforced() {
    TrackingEngine engine;
    REQUIRE(ingest_one(engine, individual(90, 1, 0, {0.0, 0.0, 0.0}, 0.2, 901)).created == 1);
    const EntityId tracked = only_track(engine).id;

    const auto conflict =
        ingest_one(engine, individual(90, 2, second, {0.0, 0.0, 0.0}, 0.2, 901, 2));
    REQUIRE(conflict.accepted == 0);
    REQUIRE(conflict.alias_conflicts == 1);
    REQUIRE(engine.find(tracked)->observation_count == 1);

    engine.advance_to(TimePoint{100 * second});
    REQUIRE(engine.find(tracked)->phase == TrackPhase::stale);
    const auto too_late =
        ingest_one(engine, individual(90, 3, 2 * second, {0.0, 0.0, 0.0}, 0.2, 901));
    REQUIRE(too_late.accepted == 0);
    REQUIRE(too_late.duplicate_or_late == 1);
    REQUIRE(engine.find(tracked)->phase == TrackPhase::stale);

    engine.advance_to(TimePoint{50 * second});
    REQUIRE(engine.find(tracked)->phase == TrackPhase::stale);
    const auto recovery =
        ingest_one(engine, individual(90, 4, 101 * second, {1.0, 0.0, 0.0}, 0.2, 901));
    REQUIRE(recovery.matched_by_alias == 1);
    REQUIRE(engine.find(tracked)->phase == TrackPhase::tentative);
}

void unsafe_numeric_inputs_are_rejected() {
    TrackingEngine engine;
    const auto unsafe = individual(100, 1, 0, {0.0, 0.0, 0.0}, std::numeric_limits<double>::max());
    const auto report = ingest_one(engine, unsafe);
    REQUIRE(report.invalid == 1);
    REQUIRE(engine.tracks().empty());

    EngineConfig invalid_config{};
    invalid_config.initial_velocity_stddev_mps = std::numeric_limits<double>::max();
    bool engine_threw = false;
    try {
        TrackingEngine invalid_engine{invalid_config};
    } catch (const std::invalid_argument&) {
        engine_threw = true;
    }
    REQUIRE(engine_threw);
}

void nanosecond_deltas_remain_exact_at_large_timestamps() {
    constexpr std::int64_t large_timestamp = std::int64_t{1} << 54;
    EngineConfig config{};
    config.individual_acceleration_stddev_mps2 = 0.0;
    TrackingEngine engine{config};
    REQUIRE(ingest_one(engine, individual(105, 1, large_timestamp, {0.0, 0.0, 0.0}, 0.001, 1051))
                .created == 1);
    REQUIRE(
        ingest_one(engine, individual(105, 2, large_timestamp + 1, {1.0, 0.0, 0.0}, 0.001, 1051))
            .accepted == 1);
    const auto& state = std::get<IndividualState>(only_track(engine).state);
    REQUIRE(naturalehia::fauna::velocity_of(state).x > 0.01);

    config.stale_after_ns = 1;
    TrackingEngine lifecycle_engine{config};
    REQUIRE(ingest_one(lifecycle_engine,
                       individual(106, 1, large_timestamp, {0.0, 0.0, 0.0}, 0.1, 1061))
                .accepted == 1);
    lifecycle_engine.advance_to(TimePoint{large_timestamp + 1});
    REQUIRE(only_track(lifecycle_engine).phase == TrackPhase::stale);
}

std::vector<Observation> collect_synthetic(naturalehia::fauna::SyntheticSource& source) {
    std::vector<Observation> collected;
    std::vector<Observation> buffer(2);
    for (;;) {
        const auto result = source.read(buffer);
        REQUIRE(result.count <= buffer.size());
        REQUIRE(result.status != naturalehia::fauna::ReadStatus::error);
        REQUIRE(result.status != naturalehia::fauna::ReadStatus::would_block);
        collected.insert(collected.end(), buffer.begin(),
                         buffer.begin() + static_cast<std::ptrdiff_t>(result.count));
        if (result.status == naturalehia::fauna::ReadStatus::end) {
            break;
        }
    }
    return collected;
}

void synthetic_source_is_bounded_and_deterministic() {
    naturalehia::fauna::SyntheticConfig config{};
    config.seed = 987'654'321U;
    config.individual_count = 2;
    config.colony_count = 1;
    config.steps = 4;
    config.area_size_m = 1'000.0;
    config.position_noise_stddev_m = 1.5;
    config.population_noise_stddev = 5.0;
    config.missed_detection_probability = 0.0;
    config.tag_visibility_probability = 0.5;

    naturalehia::fauna::SyntheticSource first{config};
    naturalehia::fauna::SyntheticSource second_source{config};
    const auto first_run = collect_synthetic(first);
    const auto second_run = collect_synthetic(second_source);

    REQUIRE(first_run.size() == 12);
    REQUIRE(second_run.size() == first_run.size());
    TimePoint ordered_timestamp = first_run.front().observed_at;
    bool saw_untagged = false;
    for (std::size_t index = 0; index < first_run.size(); ++index) {
        const auto& left = first_run[index];
        const auto& right = second_run[index];
        if (left.observed_at != ordered_timestamp) {
            ordered_timestamp = left.observed_at;
            saw_untagged = false;
        }
        if (left.external_entity_id.has_value()) {
            REQUIRE(!saw_untagged);
        } else {
            saw_untagged = true;
        }
        REQUIRE(left.source == right.source);
        REQUIRE(left.sequence == index + 1U);
        REQUIRE(left.sequence == right.sequence);
        REQUIRE(left.observed_at == right.observed_at);
        REQUIRE(left.frame == right.frame);
        REQUIRE(left.taxon == right.taxon);
        REQUIRE(left.external_entity_id == right.external_entity_id);
        REQUIRE(left.measurement.index() == right.measurement.index());

        if (const auto* left_detection = std::get_if<IndividualDetection>(&left.measurement)) {
            const auto& right_detection = std::get<IndividualDetection>(right.measurement);
            REQUIRE(left_detection->position_m.x == right_detection.position_m.x);
            REQUIRE(left_detection->position_m.y == right_detection.position_m.y);
            REQUIRE(left_detection->position_m.z == right_detection.position_m.z);
        } else {
            const auto& left_survey = std::get<ColonySurvey>(left.measurement);
            const auto& right_survey = std::get<ColonySurvey>(right.measurement);
            REQUIRE(left_survey.centroid_m.x == right_survey.centroid_m.x);
            REQUIRE(left_survey.centroid_m.y == right_survey.centroid_m.y);
            REQUIRE(left_survey.centroid_m.z == right_survey.centroid_m.z);
            REQUIRE(left_survey.estimated_population == right_survey.estimated_population);
        }
    }

    REQUIRE(first.completed_steps() == config.steps);
    REQUIRE(first.current_time().nanoseconds ==
            config.start_time.nanoseconds +
                static_cast<std::int64_t>(config.steps - 1U) * config.tick_ns);
}

void synthetic_batches_integrate_and_extremes_fail_cleanly() {
    naturalehia::fauna::SyntheticConfig config{};
    config.seed = 123U;
    config.individual_count = 5;
    config.colony_count = 2;
    config.steps = 4;
    config.missed_detection_probability = 0.0;
    config.tag_visibility_probability = 1.0;
    naturalehia::fauna::SyntheticSource source{config};
    TrackingEngine engine;
    std::array<Observation, 2> buffer{};
    std::size_t accepted = 0U;
    for (;;) {
        const auto result = source.read(buffer);
        REQUIRE(result.status != naturalehia::fauna::ReadStatus::error);
        if (result.count != 0U) {
            accepted +=
                engine.ingest(std::span<const Observation>{buffer.data(), result.count}).accepted;
        }
        if (result.status == naturalehia::fauna::ReadStatus::end) {
            break;
        }
    }
    REQUIRE(accepted == 28U);
    REQUIRE(engine.tracks().size() == 7U);

    auto all_missed_config = config;
    all_missed_config.steps = 2;
    all_missed_config.missed_detection_probability = 1.0;
    naturalehia::fauna::SyntheticSource all_missed{all_missed_config};
    REQUIRE(all_missed.read(buffer).status == naturalehia::fauna::ReadStatus::would_block);
    REQUIRE(all_missed.read(buffer).status == naturalehia::fauna::ReadStatus::end);

    auto unsafe_config = config;
    unsafe_config.steps = 1;
    unsafe_config.maximum_individual_speed_mps = std::numeric_limits<double>::max();
    unsafe_config.tick_ns = std::numeric_limits<std::int64_t>::max();
    bool source_threw = false;
    try {
        naturalehia::fauna::SyntheticSource invalid_source{unsafe_config};
    } catch (const std::invalid_argument&) {
        source_threw = true;
    }
    REQUIRE(source_threw);
}

struct TestCase {
    std::string_view name;
    void (*run)();
};

} // namespace

int main() {
    const TestCase tests[]{
        {"automatic creation and naming", automatic_creation_and_naming},
        {"source alias stability", source_alias_is_stable},
        {"distance association", nearby_observations_associate_by_distance},
        {"out-of-gate creation", out_of_gate_observation_creates_track},
        {"ambiguous rejection", ambiguous_observation_is_not_assigned},
        {"invalid and duplicate rejection", invalid_and_duplicate_sequences_are_rejected},
        {"individual Kalman movement", individual_filter_learns_constant_motion},
        {"colony population update", colony_population_stays_nonnegative_and_updates},
        {"rename uniqueness", rename_enforces_nonempty_unique_names},
        {"automatic name collision avoidance", automatic_names_do_not_collide_with_renames},
        {"track lifecycle", track_lifecycle_confirms_stales_and_recovers},
        {"known alias priority", known_aliases_are_reserved_before_distance_matching},
        {"group-wide distance assignment", distance_assignment_is_group_wide},
        {"global assignment match cardinality",
         global_assignment_prioritizes_match_count_over_local_cost},
        {"interleaved source groups", source_groups_survive_interleaved_calls},
        {"watermark and alias conflicts", watermark_and_alias_conflicts_are_enforced},
        {"unsafe numeric input", unsafe_numeric_inputs_are_rejected},
        {"exact large-timestamp deltas", nanosecond_deltas_remain_exact_at_large_timestamps},
        {"deterministic synthetic source", synthetic_source_is_bounded_and_deterministic},
        {"synthetic integration and bounds", synthetic_batches_integrate_and_extremes_fail_cleanly},
    };

    std::size_t failures = 0;
    for (const auto& test : tests) {
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

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All " << std::size(tests) << " tests passed\n";
    return 0;
}
