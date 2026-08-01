#include <naturalehia/engine.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace naturalehia {
namespace {

constexpr double kNanosecondsPerSecond = 1'000'000'000.0;

struct AliasKey {
    std::uint32_t source{};
    std::uint64_t external{};

    bool operator==(const AliasKey&) const = default;
};

struct AliasKeyHash {
    [[nodiscard]] std::size_t operator()(const AliasKey& key) const noexcept {
        const auto first = static_cast<std::size_t>(key.source);
        const auto second = static_cast<std::size_t>(key.external ^ (key.external >> 32U));
        return first ^
               (second + static_cast<std::size_t>(0x9e3779b9U) + (first << 6U) + (first >> 2U));
    }
};

[[nodiscard]] bool finite(double value) noexcept { return std::isfinite(value) != 0; }

[[nodiscard]] bool finite(Vec3 value) noexcept {
    return finite(value.x) && finite(value.y) && finite(value.z);
}

[[nodiscard]] bool safe_to_square(double value) noexcept {
    // sqrt(DBL_MAX / 2): leaves room to add two independently supplied variances.
    constexpr double kMaximumStandardDeviation = 9.480751908109176e153;
    return finite(value) && std::abs(value) <= kMaximumStandardDeviation;
}

[[nodiscard]] bool finite(const AxisState& axis) noexcept {
    return finite(axis.position_m) && finite(axis.velocity_mps) && finite(axis.position_variance) &&
           axis.position_variance >= 0.0 && finite(axis.position_velocity_covariance) &&
           finite(axis.velocity_variance) && axis.velocity_variance >= 0.0;
}

[[nodiscard]] bool valid_observation(const Observation& observation) noexcept {
    if (observation.source.value == 0U || observation.sequence == 0U ||
        observation.frame.value == 0U || observation.taxon.value == 0U) {
        return false;
    }

    if (const auto* detection = std::get_if<IndividualDetection>(&observation.measurement)) {
        return finite(detection->position_m) && safe_to_square(detection->position_stddev_m) &&
               detection->position_stddev_m > 0.0;
    }

    const auto& survey = std::get<ColonySurvey>(observation.measurement);
    return finite(survey.centroid_m) && safe_to_square(survey.position_stddev_m) &&
           survey.position_stddev_m > 0.0 && finite(survey.estimated_population) &&
           survey.estimated_population >= 0.0 && safe_to_square(survey.population_stddev) &&
           survey.population_stddev > 0.0;
}

[[nodiscard]] Vec3 measurement_position(const Observation& observation) noexcept {
    if (const auto* detection = std::get_if<IndividualDetection>(&observation.measurement)) {
        return detection->position_m;
    }
    return std::get<ColonySurvey>(observation.measurement).centroid_m;
}

[[nodiscard]] double measurement_position_stddev(const Observation& observation) noexcept {
    if (const auto* detection = std::get_if<IndividualDetection>(&observation.measurement)) {
        return detection->position_stddev_m;
    }
    return std::get<ColonySurvey>(observation.measurement).position_stddev_m;
}

[[nodiscard]] std::array<AxisState, 3>& axes_of(EstimatedState& state) noexcept {
    if (auto* individual = std::get_if<IndividualState>(&state)) {
        return individual->axes;
    }
    return std::get<ColonyState>(state).centroid_axes;
}

[[nodiscard]] const std::array<AxisState, 3>& axes_of(const EstimatedState& state) noexcept {
    if (const auto* individual = std::get_if<IndividualState>(&state)) {
        return individual->axes;
    }
    return std::get<ColonyState>(state).centroid_axes;
}

[[nodiscard]] const std::array<AxisState, 3>& axes_of(const Track& track) noexcept {
    return axes_of(track.state);
}

[[nodiscard]] double elapsed_seconds(TimePoint from, TimePoint to) noexcept {
    if (to >= from) {
        const auto difference = static_cast<std::uint64_t>(to.nanoseconds) -
                                static_cast<std::uint64_t>(from.nanoseconds);
        return static_cast<double>(difference) / kNanosecondsPerSecond;
    }
    const auto difference =
        static_cast<std::uint64_t>(from.nanoseconds) - static_cast<std::uint64_t>(to.nanoseconds);
    return -static_cast<double>(difference) / kNanosecondsPerSecond;
}

[[nodiscard]] bool predict_axis(AxisState& axis, double elapsed,
                                double acceleration_variance) noexcept {
    if (elapsed <= 0.0) {
        return finite(axis);
    }

    const double elapsed2 = elapsed * elapsed;
    const double elapsed3 = elapsed2 * elapsed;
    const double elapsed4 = elapsed2 * elapsed2;
    const double old_position_variance = axis.position_variance;
    const double old_covariance = axis.position_velocity_covariance;
    const double old_velocity_variance = axis.velocity_variance;

    AxisState predicted = axis;
    predicted.position_m += predicted.velocity_mps * elapsed;
    predicted.position_variance = old_position_variance + 2.0 * elapsed * old_covariance +
                                  elapsed2 * old_velocity_variance +
                                  0.25 * acceleration_variance * elapsed4;
    predicted.position_velocity_covariance =
        old_covariance + elapsed * old_velocity_variance + 0.5 * acceleration_variance * elapsed3;
    predicted.velocity_variance = old_velocity_variance + acceleration_variance * elapsed2;
    if (!finite(predicted)) {
        return false;
    }
    axis = predicted;
    return true;
}

[[nodiscard]] bool update_axis(AxisState& axis, double measurement,
                               double measurement_variance) noexcept {
    const double innovation_variance = axis.position_variance + measurement_variance;
    if (!(innovation_variance > 0.0) || !finite(innovation_variance)) {
        return false;
    }

    const double gain_position = axis.position_variance / innovation_variance;
    const double gain_velocity = axis.position_velocity_covariance / innovation_variance;
    const double innovation = measurement - axis.position_m;
    if (!finite(innovation)) {
        return false;
    }

    const double old_position_variance = axis.position_variance;
    const double old_covariance = axis.position_velocity_covariance;
    const double old_velocity_variance = axis.velocity_variance;
    const double one_minus_gain = 1.0 - gain_position;

    AxisState updated = axis;
    updated.position_m += gain_position * innovation;
    updated.velocity_mps += gain_velocity * innovation;

    // Joseph-form covariance update keeps the small fixed-size filter positive.
    updated.position_variance = one_minus_gain * one_minus_gain * old_position_variance +
                                gain_position * gain_position * measurement_variance;
    updated.position_velocity_covariance =
        one_minus_gain * (old_covariance - gain_velocity * old_position_variance) +
        gain_position * gain_velocity * measurement_variance;
    updated.velocity_variance = gain_velocity * gain_velocity * old_position_variance -
                                2.0 * gain_velocity * old_covariance + old_velocity_variance +
                                gain_velocity * gain_velocity * measurement_variance;

    updated.position_variance = std::max(0.0, updated.position_variance);
    updated.velocity_variance = std::max(0.0, updated.velocity_variance);
    if (!finite(updated)) {
        return false;
    }
    axis = updated;
    return true;
}

[[nodiscard]] std::array<AxisState, 3> initial_axes(Vec3 position, double position_stddev,
                                                    double velocity_stddev) noexcept {
    const double position_variance = position_stddev * position_stddev;
    const double velocity_variance = velocity_stddev * velocity_stddev;
    return {{{position.x, 0.0, position_variance, 0.0, velocity_variance},
             {position.y, 0.0, position_variance, 0.0, velocity_variance},
             {position.z, 0.0, position_variance, 0.0, velocity_variance}}};
}

[[nodiscard]] bool whitespace_only(std::string_view value) noexcept {
    if (value.empty()) {
        return true;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
               byte == 0U;
    });
}

[[nodiscard]] std::string automatic_name(EntityKind kind, EntityId id) {
    std::array<char, 32> digits{};
    const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), id.value);
    if (result.ec != std::errc{}) {
        throw std::logic_error("failed to format an entity ID");
    }

    const auto digit_count = static_cast<std::size_t>(result.ptr - digits.data());
    std::string output = kind == EntityKind::individual ? "animal-" : "colony-";
    if (digit_count < 6U) {
        output.append(6U - digit_count, '0');
    }
    output.append(digits.data(), digit_count);
    return output;
}

[[nodiscard]] bool compatible(const Track& track, const Observation& observation) noexcept {
    return track.kind == kind_of(observation.measurement) && track.taxon == observation.taxon &&
           track.frame == observation.frame;
}

struct Candidate {
    std::size_t index{};
    double score{};
};

} // namespace

struct TrackingEngine::Impl {
    explicit Impl(EngineConfig supplied_config) : config(supplied_config) {
        const bool valid =
            finite(config.association_gate_sigma) && config.association_gate_sigma > 0.0 &&
            finite(config.ambiguity_margin_sigma) && config.ambiguity_margin_sigma >= 0.0 &&
            config.confirmations_required > 0U && config.stale_after_ns > 0 &&
            safe_to_square(config.individual_acceleration_stddev_mps2) &&
            config.individual_acceleration_stddev_mps2 >= 0.0 &&
            safe_to_square(config.colony_drift_stddev_mps2) &&
            config.colony_drift_stddev_mps2 >= 0.0 &&
            safe_to_square(config.population_process_stddev_per_second) &&
            config.population_process_stddev_per_second >= 0.0 &&
            safe_to_square(config.initial_velocity_stddev_mps) &&
            config.initial_velocity_stddev_mps > 0.0;
        if (!valid) {
            throw std::invalid_argument("invalid Naturalehia engine configuration");
        }
    }

    [[nodiscard]] double acceleration_variance(EntityKind kind) const noexcept {
        const double standard_deviation = kind == EntityKind::individual
                                              ? config.individual_acceleration_stddev_mps2
                                              : config.colony_drift_stddev_mps2;
        return standard_deviation * standard_deviation;
    }

    struct SourceGroup {
        bool initialized{false};
        TimePoint timestamp{};
        std::vector<EntityId> entities;
    };

    [[nodiscard]] static bool used_in_group(const SourceGroup& group, EntityId id) noexcept {
        return std::find(group.entities.begin(), group.entities.end(), id) != group.entities.end();
    }

    [[nodiscard]] SourceGroup& begin_group(SourceId source, TimePoint timestamp) {
        SourceGroup& group = groups_by_source[source.value];
        if (!group.initialized || timestamp != group.timestamp) {
            group.timestamp = timestamp;
            group.initialized = true;
            group.entities.clear();
        }
        return group;
    }

    [[nodiscard]] double association_score(const Track& track,
                                           const Observation& observation) const noexcept {
        const double elapsed = elapsed_seconds(track.last_observed_at, observation.observed_at);
        if (elapsed < 0.0 || !finite(elapsed)) {
            return std::numeric_limits<double>::infinity();
        }

        const auto& axes = axes_of(track);
        const Vec3 measured = measurement_position(observation);
        const std::array<double, 3> values{measured.x, measured.y, measured.z};
        const double measurement_variance =
            measurement_position_stddev(observation) * measurement_position_stddev(observation);
        double squared_score = 0.0;

        for (std::size_t dimension = 0; dimension < axes.size(); ++dimension) {
            AxisState predicted = axes[dimension];
            if (!predict_axis(predicted, elapsed, acceleration_variance(track.kind))) {
                return std::numeric_limits<double>::infinity();
            }
            const double total_variance = predicted.position_variance + measurement_variance;
            if (!(total_variance > 0.0) || !finite(total_variance) ||
                !finite(predicted.position_m)) {
                return std::numeric_limits<double>::infinity();
            }
            const double residual = values[dimension] - predicted.position_m;
            const double normalized_residual = residual / std::sqrt(total_variance);
            if (!finite(normalized_residual)) {
                return std::numeric_limits<double>::infinity();
            }
            squared_score += normalized_residual * normalized_residual;
            if (!finite(squared_score)) {
                return std::numeric_limits<double>::infinity();
            }
        }

        return std::sqrt(squared_score);
    }

    [[nodiscard]] Track make_track(const Observation& observation) {
        Track track;
        track.id = EntityId{next_entity_id++};
        track.kind = kind_of(observation.measurement);
        const std::string base_name = automatic_name(track.kind, track.id);
        track.display_name = base_name;
        std::uint64_t suffix = 2U;
        while (names.contains(track.display_name)) {
            track.display_name = base_name + "-" + std::to_string(suffix++);
        }
        track.taxon = observation.taxon;
        track.frame = observation.frame;
        track.phase =
            config.confirmations_required <= 1U ? TrackPhase::confirmed : TrackPhase::tentative;
        track.created_at = observation.observed_at;
        track.last_observed_at = observation.observed_at;
        track.observation_count = 1U;

        if (const auto* detection = std::get_if<IndividualDetection>(&observation.measurement)) {
            track.state =
                IndividualState{initial_axes(detection->position_m, detection->position_stddev_m,
                                             config.initial_velocity_stddev_mps)};
        } else {
            const auto& survey = std::get<ColonySurvey>(observation.measurement);
            track.state = ColonyState{
                initial_axes(survey.centroid_m, survey.position_stddev_m,
                             config.initial_velocity_stddev_mps),
                std::max(0.0, survey.estimated_population),
                survey.population_stddev * survey.population_stddev,
            };
        }
        return track;
    }

    [[nodiscard]] bool update_track(Track& track, const Observation& observation) noexcept {
        const double elapsed = elapsed_seconds(track.last_observed_at, observation.observed_at);
        EstimatedState updated_state = track.state;
        auto& axes = axes_of(updated_state);
        const double process_variance = acceleration_variance(track.kind);
        for (auto& axis : axes) {
            if (!predict_axis(axis, elapsed, process_variance)) {
                return false;
            }
        }

        const Vec3 measured = measurement_position(observation);
        const std::array<double, 3> values{measured.x, measured.y, measured.z};
        const double position_stddev = measurement_position_stddev(observation);
        const double position_variance = position_stddev * position_stddev;
        for (std::size_t dimension = 0; dimension < axes.size(); ++dimension) {
            if (!update_axis(axes[dimension], values[dimension], position_variance)) {
                return false;
            }
        }

        if (auto* colony = std::get_if<ColonyState>(&updated_state)) {
            const auto& survey = std::get<ColonySurvey>(observation.measurement);
            const double process_stddev = config.population_process_stddev_per_second;
            const double predicted_population_variance =
                colony->population_variance +
                process_stddev * process_stddev * std::max(0.0, elapsed);
            const double measurement_variance = survey.population_stddev * survey.population_stddev;
            const double innovation_variance = predicted_population_variance + measurement_variance;
            const double innovation = survey.estimated_population - colony->estimated_population;
            if (!(predicted_population_variance >= 0.0) || !(innovation_variance > 0.0) ||
                !finite(predicted_population_variance) || !finite(innovation_variance) ||
                !finite(innovation)) {
                return false;
            }
            const double gain = predicted_population_variance / innovation_variance;
            const double estimated_population = colony->estimated_population + gain * innovation;
            const double population_variance = (1.0 - gain) * predicted_population_variance;
            if (!finite(estimated_population) || !finite(population_variance)) {
                return false;
            }
            colony->estimated_population = std::max(0.0, estimated_population);
            colony->population_variance = std::max(0.0, population_variance);
        }

        track.state = std::move(updated_state);
        track.last_observed_at = observation.observed_at;
        if (track.observation_count < std::numeric_limits<std::uint32_t>::max()) {
            ++track.observation_count;
        }
        track.phase = track.observation_count >= config.confirmations_required
                          ? TrackPhase::confirmed
                          : TrackPhase::tentative;
        return true;
    }

    EngineConfig config;
    std::vector<Track> tracks;
    std::unordered_map<std::uint64_t, std::size_t> entity_indices;
    std::unordered_map<AliasKey, EntityId, AliasKeyHash> aliases;
    std::unordered_map<std::uint32_t, std::uint64_t> last_sequence_by_source;
    std::unordered_map<std::uint32_t, TimePoint> last_timestamp_by_source;
    std::unordered_map<std::uint32_t, SourceGroup> groups_by_source;
    std::unordered_map<std::string, EntityId> names;
    std::optional<TimePoint> watermark;
    std::uint64_t next_entity_id{1U};
};

TrackingEngine::TrackingEngine(EngineConfig config) : impl_(std::make_unique<Impl>(config)) {}

TrackingEngine::~TrackingEngine() = default;
TrackingEngine::TrackingEngine(TrackingEngine&&) noexcept = default;
TrackingEngine& TrackingEngine::operator=(TrackingEngine&&) noexcept = default;

BatchReport TrackingEngine::ingest(std::span<const Observation> observations) {
    BatchReport report;
    if (observations.empty()) {
        return report;
    }

    std::vector<std::size_t> order(observations.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](std::size_t left, std::size_t right) {
        const Observation& first = observations[left];
        const Observation& second = observations[right];
        if (first.observed_at != second.observed_at) {
            return first.observed_at < second.observed_at;
        }
        if (first.source != second.source) {
            return first.source < second.source;
        }
        if (first.sequence != second.sequence) {
            return first.sequence < second.sequence;
        }
        return left < right;
    });

    struct PreparedObservation {
        const Observation* observation{};
        std::optional<AliasKey> alias;
        bool handled_as_known_alias{false};
    };

    std::size_t group_begin = 0U;
    while (group_begin < order.size()) {
        const Observation& first = observations[order[group_begin]];
        std::size_t group_end = group_begin + 1U;
        while (group_end < order.size()) {
            const Observation& candidate = observations[order[group_end]];
            if (candidate.source != first.source || candidate.observed_at != first.observed_at) {
                break;
            }
            ++group_end;
        }

        std::vector<PreparedObservation> prepared;
        prepared.reserve(group_end - group_begin);
        for (std::size_t position = group_begin; position < group_end; ++position) {
            const Observation& observation = observations[order[position]];
            if (!valid_observation(observation)) {
                ++report.invalid;
                continue;
            }

            const auto sequence_iterator =
                impl_->last_sequence_by_source.find(observation.source.value);
            const auto timestamp_iterator =
                impl_->last_timestamp_by_source.find(observation.source.value);
            if ((sequence_iterator != impl_->last_sequence_by_source.end() &&
                 observation.sequence <= sequence_iterator->second) ||
                (timestamp_iterator != impl_->last_timestamp_by_source.end() &&
                 observation.observed_at < timestamp_iterator->second)) {
                ++report.duplicate_or_late;
                continue;
            }

            // A valid source envelope is consumed even if its evidence cannot be associated.
            impl_->last_sequence_by_source[observation.source.value] = observation.sequence;
            impl_->last_timestamp_by_source[observation.source.value] = observation.observed_at;
            if (impl_->watermark.has_value() && observation.observed_at < *impl_->watermark) {
                ++report.duplicate_or_late;
                continue;
            }

            const std::optional<AliasKey> alias =
                observation.external_entity_id.has_value()
                    ? std::optional<AliasKey>{AliasKey{observation.source.value,
                                                       *observation.external_entity_id}}
                    : std::nullopt;
            prepared.push_back({&observation, alias, false});
        }

        if (prepared.empty()) {
            group_begin = group_end;
            continue;
        }

        auto& source_group = impl_->begin_group(first.source, first.observed_at);

        // Reserve tracks with known hard aliases before proximity association. This prevents
        // an earlier untagged detection in the same source frame from stealing that identity.
        for (PreparedObservation& item : prepared) {
            if (!item.alias.has_value()) {
                continue;
            }
            const auto alias_iterator = impl_->aliases.find(*item.alias);
            if (alias_iterator == impl_->aliases.end()) {
                continue;
            }
            item.handled_as_known_alias = true;

            const auto entity_iterator = impl_->entity_indices.find(alias_iterator->second.value);
            if (entity_iterator == impl_->entity_indices.end()) {
                ++report.alias_conflicts;
                continue;
            }

            Track& track = impl_->tracks[entity_iterator->second];
            if (!compatible(track, *item.observation)) {
                ++report.alias_conflicts;
                continue;
            }
            if (Impl::used_in_group(source_group, track.id)) {
                ++report.ambiguous;
                continue;
            }
            if (item.observation->observed_at < track.last_observed_at) {
                ++report.duplicate_or_late;
                continue;
            }
            if (!impl_->update_track(track, *item.observation)) {
                ++report.invalid;
                continue;
            }

            source_group.entities.push_back(track.id);
            ++report.accepted;
            ++report.matched_by_alias;
        }

        struct AssociationEdge {
            std::size_t observation_index{};
            std::size_t track_index{};
            double score{};
        };

        const std::size_t unassigned = std::numeric_limits<std::size_t>::max();
        std::vector<std::size_t> assignments(prepared.size(), unassigned);
        std::vector<bool> blocked(prepared.size(), false);
        std::vector<AssociationEdge> edges;
        std::unordered_map<AliasKey, std::size_t, AliasKeyHash> new_aliases_in_group;

        for (std::size_t item_index = 0; item_index < prepared.size(); ++item_index) {
            const PreparedObservation& item = prepared[item_index];
            if (item.handled_as_known_alias) {
                blocked[item_index] = true;
                continue;
            }
            if (item.alias.has_value()) {
                const auto [unused, inserted] =
                    new_aliases_in_group.emplace(*item.alias, item_index);
                static_cast<void>(unused);
                if (!inserted) {
                    blocked[item_index] = true;
                    ++report.ambiguous;
                    continue;
                }
            }

            const Observation& observation = *item.observation;
            std::vector<Candidate> candidates;
            candidates.reserve(impl_->tracks.size());
            for (std::size_t track_index = 0; track_index < impl_->tracks.size(); ++track_index) {
                const Track& track = impl_->tracks[track_index];
                if (!compatible(track, observation) ||
                    Impl::used_in_group(source_group, track.id) ||
                    observation.observed_at < track.last_observed_at) {
                    continue;
                }
                const double score = impl_->association_score(track, observation);
                if (score <= impl_->config.association_gate_sigma) {
                    candidates.push_back({track_index, score});
                }
            }

            std::sort(candidates.begin(), candidates.end(),
                      [&](const Candidate& left, const Candidate& right) {
                          if (left.score != right.score) {
                              return left.score < right.score;
                          }
                          return impl_->tracks[left.index].id < impl_->tracks[right.index].id;
                      });
            if (candidates.size() > 1U &&
                candidates[1].score - candidates[0].score <= impl_->config.ambiguity_margin_sigma) {
                blocked[item_index] = true;
                ++report.ambiguous;
                continue;
            }
            for (const Candidate& candidate : candidates) {
                edges.push_back({item_index, candidate.index, candidate.score});
            }
        }

        std::sort(
            edges.begin(), edges.end(),
            [&](const AssociationEdge& left, const AssociationEdge& right) {
                if (left.score != right.score) {
                    return left.score < right.score;
                }
                const auto left_sequence = prepared[left.observation_index].observation->sequence;
                const auto right_sequence = prepared[right.observation_index].observation->sequence;
                if (left_sequence != right_sequence) {
                    return left_sequence < right_sequence;
                }
                return impl_->tracks[left.track_index].id < impl_->tracks[right.track_index].id;
            });

        std::vector<bool> assigned_tracks(impl_->tracks.size(), false);
        for (const AssociationEdge& edge : edges) {
            if (assignments[edge.observation_index] == unassigned &&
                !assigned_tracks[edge.track_index]) {
                assignments[edge.observation_index] = edge.track_index;
                assigned_tracks[edge.track_index] = true;
            }
        }

        for (std::size_t item_index = 0; item_index < prepared.size(); ++item_index) {
            const PreparedObservation& item = prepared[item_index];
            if (blocked[item_index]) {
                continue;
            }
            const Observation& observation = *item.observation;
            if (assignments[item_index] != unassigned) {
                Track& track = impl_->tracks[assignments[item_index]];
                if (!impl_->update_track(track, observation)) {
                    ++report.invalid;
                    continue;
                }
                if (item.alias.has_value()) {
                    impl_->aliases.emplace(*item.alias, track.id);
                }
                source_group.entities.push_back(track.id);
                ++report.accepted;
                ++report.matched_by_distance;
                continue;
            }

            Track track = impl_->make_track(observation);
            const EntityId id = track.id;
            const std::size_t index = impl_->tracks.size();
            impl_->names.emplace(track.display_name, id);
            impl_->tracks.push_back(std::move(track));
            impl_->entity_indices.emplace(id.value, index);
            if (item.alias.has_value()) {
                impl_->aliases.emplace(*item.alias, id);
            }
            source_group.entities.push_back(id);
            ++report.accepted;
            ++report.created;
        }

        group_begin = group_end;
    }

    return report;
}

void TrackingEngine::advance_to(TimePoint now) noexcept {
    if (impl_->watermark.has_value() && now < *impl_->watermark) {
        return;
    }
    impl_->watermark = now;
    for (Track& track : impl_->tracks) {
        if (now <= track.last_observed_at) {
            continue;
        }
        const auto elapsed = static_cast<std::uint64_t>(now.nanoseconds) -
                             static_cast<std::uint64_t>(track.last_observed_at.nanoseconds);
        if (elapsed >= static_cast<std::uint64_t>(impl_->config.stale_after_ns)) {
            track.phase = TrackPhase::stale;
        }
    }
}

std::span<const Track> TrackingEngine::tracks() const noexcept { return impl_->tracks; }

const Track* TrackingEngine::find(EntityId id) const noexcept {
    const auto iterator = impl_->entity_indices.find(id.value);
    if (iterator == impl_->entity_indices.end()) {
        return nullptr;
    }
    return &impl_->tracks[iterator->second];
}

RenameResult TrackingEngine::rename(EntityId id, std::string new_name) {
    const auto entity_iterator = impl_->entity_indices.find(id.value);
    if (entity_iterator == impl_->entity_indices.end()) {
        return RenameResult::not_found;
    }
    if (whitespace_only(new_name)) {
        return RenameResult::empty_name;
    }

    Track& track = impl_->tracks[entity_iterator->second];
    if (track.display_name == new_name) {
        return RenameResult::renamed;
    }
    if (impl_->names.contains(new_name)) {
        return RenameResult::name_in_use;
    }

    impl_->names.erase(track.display_name);
    track.display_name = std::move(new_name);
    impl_->names.emplace(track.display_name, track.id);
    return RenameResult::renamed;
}

const char* to_string(EntityKind kind) noexcept {
    switch (kind) {
    case EntityKind::individual:
        return "individual";
    case EntityKind::colony:
        return "colony";
    }
    return "unknown";
}

const char* to_string(TrackPhase phase) noexcept {
    switch (phase) {
    case TrackPhase::tentative:
        return "tentative";
    case TrackPhase::confirmed:
        return "confirmed";
    case TrackPhase::stale:
        return "stale";
    }
    return "unknown";
}

} // namespace naturalehia
