// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/joint_cohort.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumObservations = 100000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumDefinitionLength = 1024U;
constexpr std::size_t kMaximumSourceNoteLength = 2048U;
constexpr std::size_t kMaximumCitationIds = 64U;
constexpr std::size_t kMaximumPairProjectionEndpointWeights = 100000U;

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
};

[[noreturn]] void invalid(std::string message) {
    throw std::invalid_argument("joint cohort: " + std::move(message));
}

[[nodiscard]] bool is_safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength) {
        return false;
    }
    return std::all_of(
        value.begin(), value.end(), [](unsigned char character) {
            return (character >= 'A' && character <= 'Z') ||
                (character >= 'a' && character <= 'z') ||
                (character >= '0' && character <= '9') || character == '-' ||
                character == '_' || character == '.' || character == ':';
        });
}

void require_safe_identifier(
    std::string_view value,
    std::string_view field) {
    if (!is_safe_identifier(value) || value == "NONE") {
        invalid(std::string(field) + " must be a safe non-NONE identifier");
    }
}

void require_single_line_text(
    std::string_view value,
    std::string_view field,
    std::size_t maximum_length) {
    if (value.empty() || value.size() > maximum_length ||
        value.front() == ' ' || value.back() == ' ' ||
        value.find('=') != std::string_view::npos ||
        std::any_of(value.begin(), value.end(), [](unsigned char character) {
            return character < 0x20U || character == 0x7fU;
        })) {
        invalid(std::string(field) +
                " must be bounded nonempty single-line key-value-safe text");
    }
}

[[nodiscard]] bool is_leap_year(int year) noexcept {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

[[nodiscard]] int parse_decimal(std::string_view value) noexcept {
    int result = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return -1;
        }
        result = result * 10 + (character - '0');
    }
    return result;
}

void validate_id_list(
    const std::vector<std::string>& values,
    std::string_view field) {
    if (values.size() > kMaximumCitationIds) {
        invalid(std::string(field) + " exceeds the citation count limit");
    }
    std::unordered_set<std::string> unique;
    unique.reserve(values.size());
    for (const std::string& value : values) {
        require_safe_identifier(value, field);
        if (!unique.insert(value).second) {
            invalid(std::string(field) + " contains a duplicate ID");
        }
    }
}

[[nodiscard]] double outward_lower_double(long double value) noexcept {
    double result = static_cast<double>(value);
    if (static_cast<long double>(result) > value) {
        result = std::nextafter(result, 0.0);
    }
    return result;
}

[[nodiscard]] double outward_upper_double(long double value) noexcept {
    double result = static_cast<double>(value);
    if (static_cast<long double>(result) < value) {
        result = std::nextafter(result, 1.0);
    }
    return result;
}

[[nodiscard]] long double goodman_critical_value(
    long double family_alpha,
    std::size_t category_count) {
    const long double target =
        family_alpha / static_cast<long double>(category_count);
    long double lower = 0.0L;
    long double upper = 1.0L;
    const auto upper_tail = [](long double value) {
        return std::erfc(std::sqrt(value / 2.0L));
    };
    while (upper_tail(upper) > target && upper < 1048576.0L) {
        upper *= 2.0L;
    }
    if (upper_tail(upper) > target) {
        throw std::logic_error(
            "Goodman diagnostic critical-value bracket failed");
    }
    for (std::size_t iteration = 0U; iteration < 200U; ++iteration) {
        const long double midpoint = (lower + upper) / 2.0L;
        if (upper_tail(midpoint) > target) {
            lower = midpoint;
        } else {
            upper = midpoint;
        }
    }
    return (lower + upper) / 2.0L;
}

[[nodiscard]] std::pair<double, double> goodman_interval(
    std::size_t count,
    std::size_t total,
    long double critical) {
    const long double n = static_cast<long double>(total);
    const long double x = static_cast<long double>(count);
    const long double proportion = x / n;
    const long double root = std::sqrt(
        critical *
        (critical + 4.0L * x * (1.0L - proportion)));
    const long double denominator = 2.0L * (n + critical);
    const long double lower =
        std::clamp((critical + 2.0L * x - root) / denominator,
                   0.0L, 1.0L);
    const long double upper =
        std::clamp((critical + 2.0L * x + root) / denominator,
                   0.0L, 1.0L);
    return {outward_lower_double(lower), outward_upper_double(upper)};
}

[[nodiscard]] std::vector<AmbiguityScenarioMetricValue> indicator_values(
    const PortfolioSummary& central,
    const std::vector<std::string>& selected_projects,
    bool require_all) {
    std::vector<AmbiguityScenarioMetricValue> values;
    values.reserve(central.scenarios.size());
    for (const JointScenarioResult& scenario : central.scenarios) {
        std::size_t impaired = 0U;
        for (const std::string& project_id : selected_projects) {
            const auto project = std::find_if(
                scenario.projects.begin(), scenario.projects.end(),
                [&project_id](const ProjectPathResult& candidate) {
                    return candidate.project_id == project_id;
                });
            if (project == scenario.projects.end()) {
                throw std::logic_error(
                    "joint cohort projection lost a portfolio project");
            }
            if (project->principal_loss_million > 0.0) {
                ++impaired;
            }
        }
        const bool event = require_all
            ? impaired == selected_projects.size()
            : impaired > 0U;
        values.push_back(
            AmbiguityScenarioMetricValue{scenario.scenario_id,
                                         event ? 1.0 : 0.0});
    }
    return values;
}

void add_project_impairment_projections(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const PortfolioAmbiguitySummary& financial,
    JointCohortResult& result) {
    const PortfolioSummary& central = financial.central_portfolio;
    std::vector<std::string> project_ids;
    project_ids.reserve(portfolio.projects.size());
    for (const PortfolioProject& project : portfolio.projects) {
        project_ids.push_back(project.id);
    }
    std::sort(project_ids.begin(), project_ids.end());

    const std::size_t project_count = project_ids.size();
    const std::size_t pair_count = project_count > 1U
        ? project_count * (project_count - 1U) / 2U
        : 0U;
    const std::size_t scenario_count = portfolio.joint_scenarios.size();

    const PortfolioAmbiguityProjector projector(portfolio, ambiguity);
    result.project_impairment_probabilities.reserve(project_count);
    for (const std::string& project_id : project_ids) {
        const auto bounded = std::find_if(financial.projects.begin(),
            financial.projects.end(), [&project_id](const auto& project) {
                return project.project_id == project_id;
            });
        if (bounded == financial.projects.end()) {
            throw std::logic_error(
                "joint cohort financial ranges lost a portfolio project");
        }
        result.project_impairment_probabilities.push_back(
            JointCohortProjectImpairmentProjection{
                project_id, bounded->principal_impairment_probability});
    }
    const bool pair_work_within_limit =
        scenario_count == 0U || pair_count <=
            kMaximumPairProjectionEndpointWeights /
                (2U * scenario_count);
    if (pair_work_within_limit) {
        result.pair_impairment_probabilities.reserve(pair_count);
        for (std::size_t first = 0U; first < project_count; ++first) {
            for (std::size_t second = first + 1U;
                 second < project_count; ++second) {
                const AmbiguityMetricProjection projection =
                    projector.project_expectation(indicator_values(
                        central,
                        {project_ids[first], project_ids[second]}, true));
                result.pair_impairment_probabilities.push_back(
                    JointCohortPairImpairmentProjection{
                        project_ids[first], project_ids[second],
                        projection.expectation});
            }
        }
        result.pair_impairment_projections_available = true;
    } else {
        result.pair_impairment_projections_available = false;
        result.pair_impairment_projection_block_reason =
            "pair impairment projections omitted because endpoint witness work exceeds 100000 weights";
    }
    result.any_project_impairment_probability =
        projector.project_expectation(
            indicator_values(central, project_ids, false)).expectation;
    result.all_projects_impairment_probability =
        projector.project_expectation(
            indicator_values(central, project_ids, true)).expectation;
    result.project_impairment_projections_available = true;
}

} // namespace

std::string_view to_string(
    JointCohortObservationStatus status) noexcept {
    switch (status) {
    case JointCohortObservationStatus::Matured:
        return "matured";
    case JointCohortObservationStatus::NotYetMatured:
        return "not-yet-matured";
    case JointCohortObservationStatus::Unresolved:
        return "unresolved";
    case JointCohortObservationStatus::Excluded:
        return "excluded";
    }
    return "unknown";
}

bool is_joint_cohort_iso_date(std::string_view value) noexcept {
    if (value.size() != 10U || value[4U] != '-' || value[7U] != '-') {
        return false;
    }
    const int year = parse_decimal(value.substr(0U, 4U));
    const int month = parse_decimal(value.substr(5U, 2U));
    const int day = parse_decimal(value.substr(8U, 2U));
    if (year < 1 || month < 1 || month > 12 || day < 1) {
        return false;
    }
    constexpr int days_per_month[12]{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int maximum_day = days_per_month[month - 1];
    if (month == 2 && is_leap_year(year)) {
        maximum_day = 29;
    }
    return day <= maximum_day;
}

void validate_joint_cohort_analysis_config(
    const JointCohortAnalysisConfig& config) {
    if (config.version != kJointCohortVersion) {
        invalid("version must be 0.1.0");
    }
    require_safe_identifier(config.id, "id");
    if (!is_joint_cohort_iso_date(config.as_of_date)) {
        invalid("as_of_date must be a calendar date in YYYY-MM-DD");
    }
    require_single_line_text(
        config.source_note, "source_note", kMaximumSourceNoteLength);
    require_single_line_text(config.population_definition,
        "population_definition", kMaximumDefinitionLength);
    require_single_line_text(config.sampling_unit_definition,
        "sampling_unit_definition", kMaximumDefinitionLength);
    require_single_line_text(config.outcome_mapping_definition,
        "outcome_mapping_definition", kMaximumDefinitionLength);
    require_single_line_text(config.horizon_definition,
        "horizon_definition", kMaximumDefinitionLength);
    if (!is_joint_cohort_iso_date(
            config.scenario_taxonomy_frozen_date) ||
        config.scenario_taxonomy_frozen_date > config.as_of_date) {
        invalid("scenario_taxonomy_frozen_date must be a valid date no later than as_of_date");
    }
    if (config.population_frame_count == 0U ||
        config.population_frame_count > kMaximumObservations) {
        invalid("population_frame_count must be between 1 and 100000");
    }
    if (!config.candidate_only) {
        invalid("candidate_only must be true");
    }
    if (!config.synthetic_inputs) {
        invalid("synthetic_inputs must be true in v0.1");
    }
    if (config.probability_measure != kJointCohortProbabilityMeasure) {
        invalid("probability_measure must be physical-P");
    }
    if (config.sampling_assumption != kJointCohortSamplingAssumption) {
        invalid("sampling_assumption must be iid-complete-joint-state-candidate");
    }
    if (config.interval_method != kJointCohortIntervalMethod) {
        invalid("interval_method must be hoeffding-bonferroni-outer-v0.1");
    }
    if (!std::isfinite(config.confidence_level) ||
        config.confidence_level <= 0.0 || config.confidence_level >= 1.0) {
        invalid("confidence_level must be finite and in (0, 1)");
    }
    if (config.exclusion_rules.size() > 1024U) {
        invalid("exclusion rule count must not exceed 1024");
    }

    std::unordered_set<std::string> rule_ids;
    rule_ids.reserve(config.exclusion_rules.size());
    for (const JointCohortExclusionRule& rule : config.exclusion_rules) {
        require_safe_identifier(rule.id, "exclusion rule id");
        if (!rule_ids.insert(rule.id).second) {
            invalid("exclusion rule IDs must be unique");
        }
        if (!is_joint_cohort_iso_date(rule.frozen_date) ||
            rule.frozen_date > config.as_of_date) {
            invalid("exclusion rule frozen_date must be a valid date no later than as_of_date");
        }
        if (!rule.outcome_blind_asserted) {
            invalid("every exclusion rule must assert outcome_blind=true");
        }
        require_single_line_text(
            rule.statement, "exclusion rule statement",
            kMaximumDefinitionLength);
    }
}

void validate_joint_cohort_ledger_syntax(
    const std::vector<JointCohortObservation>& observations) {
    if (observations.empty() || observations.size() > kMaximumObservations) {
        invalid("raw ledger must contain between 1 and 100000 rows");
    }
    std::unordered_set<std::string> observation_ids;
    observation_ids.reserve(observations.size());
    for (const JointCohortObservation& observation : observations) {
        require_safe_identifier(
            observation.observation_id, "observation_id");
        require_safe_identifier(observation.cluster_id, "cluster_id");
        if (!observation_ids.insert(observation.observation_id).second) {
            invalid("observation_id values must be unique");
        }
        if (!is_joint_cohort_iso_date(observation.eligible_date) ||
            !is_joint_cohort_iso_date(observation.horizon_end_date) ||
            observation.horizon_end_date < observation.eligible_date) {
            invalid("eligible_date and horizon_end_date must be ordered calendar dates");
        }
        if (to_string(observation.status) == "unknown") {
            invalid("observation status enum value is invalid");
        }
        validate_id_list(
            observation.evidence_record_ids, "evidence_record_ids");
        validate_id_list(
            observation.requirement_ids, "requirement_ids");
        if (observation.evidence_record_ids.empty() !=
            observation.requirement_ids.empty()) {
            invalid("evidence and requirement IDs must both be NONE or both be present");
        }

        switch (observation.status) {
        case JointCohortObservationStatus::Matured:
            require_safe_identifier(
                observation.scenario_id, "matured scenario_id");
            if (!is_joint_cohort_iso_date(
                    observation.classification_date) ||
                observation.classification_date <
                    observation.horizon_end_date ||
                observation.exclusion_rule_id != "NONE") {
                invalid("matured rows require an outcome, a classification date on or after horizon, and exclusion_rule_id=NONE");
            }
            break;
        case JointCohortObservationStatus::NotYetMatured:
        case JointCohortObservationStatus::Unresolved:
            if (observation.scenario_id != "NONE" ||
                observation.classification_date != "NONE" ||
                observation.exclusion_rule_id != "NONE") {
                invalid("unknown-outcome rows require scenario, classification, and exclusion fields NONE");
            }
            break;
        case JointCohortObservationStatus::Excluded:
            if (observation.scenario_id != "NONE" ||
                observation.classification_date != "NONE") {
                invalid("excluded rows require scenario_id and classification_date NONE");
            }
            require_safe_identifier(
                observation.exclusion_rule_id, "exclusion_rule_id");
            break;
        }
    }
}

JointCohortResult evaluate_joint_cohort(
    const JointCohortAnalysisConfig& config,
    const PortfolioConfig& portfolio,
    const std::vector<JointCohortObservation>& observations) {
    validate_joint_cohort_analysis_config(config);
    validate_joint_cohort_ledger_syntax(observations);
    if (config.population_frame_count != observations.size()) {
        invalid("population_frame_count must equal the authoritative raw ledger row count");
    }
    validate_portfolio_config(portfolio);
    if (!portfolio.synthetic_inputs) {
        invalid("v0.1 requires a synthetic portfolio");
    }

    std::vector<const JointScenario*> scenarios;
    scenarios.reserve(portfolio.joint_scenarios.size());
    std::unordered_map<std::string, std::size_t> scenario_positions;
    scenario_positions.reserve(portfolio.joint_scenarios.size());
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        scenarios.push_back(&scenario);
    }
    std::sort(scenarios.begin(), scenarios.end(),
        [](const JointScenario* left, const JointScenario* right) {
            return left->id < right->id;
        });
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        scenario_positions.emplace(scenarios[index]->id, index);
    }

    std::unordered_map<std::string, const JointCohortExclusionRule*> rules;
    rules.reserve(config.exclusion_rules.size());
    for (const JointCohortExclusionRule& rule : config.exclusion_rules) {
        rules.emplace(rule.id, &rule);
    }

    JointCohortResult result;
    result.candidate_only = config.candidate_only;
    result.generated_envelope_synthetic = true;
    result.calibrated_execution_authorized = false;
    result.raw_observation_count = observations.size();
    result.confidence_level = config.confidence_level;
    const long double requested_family_alpha =
        std::nextafter(
            1.0L - static_cast<long double>(config.confidence_level), 0.0L);
    result.family_alpha = outward_lower_double(requested_family_alpha);

    std::vector<std::size_t> matured_by_scenario(scenarios.size(), 0U);
    std::unordered_map<std::string, std::size_t> included_cluster_counts;
    included_cluster_counts.reserve(observations.size());
    std::unordered_map<std::string, std::size_t> excluded_by_rule;
    excluded_by_rule.reserve(rules.size());

    for (const JointCohortObservation& observation : observations) {
        if (observation.eligible_date > config.as_of_date) {
            invalid("eligible_date must not follow as_of_date");
        }
        if (observation.status != JointCohortObservationStatus::Excluded &&
            config.scenario_taxonomy_frozen_date >
                observation.eligible_date) {
            invalid("scenario taxonomy must be frozen on or before every included eligible_date");
        }
        switch (observation.status) {
        case JointCohortObservationStatus::Matured: {
            if (observation.classification_date > config.as_of_date) {
                invalid("matured classification_date must not follow as_of_date");
            }
            const auto scenario =
                scenario_positions.find(observation.scenario_id);
            if (scenario == scenario_positions.end()) {
                invalid("matured scenario_id is not an exact portfolio scenario");
            }
            ++matured_by_scenario[scenario->second];
            ++result.matured_count;
            ++result.included_observation_count;
            ++included_cluster_counts[observation.cluster_id];
            break;
        }
        case JointCohortObservationStatus::NotYetMatured:
            if (observation.horizon_end_date <= config.as_of_date) {
                invalid("not-yet-matured rows require horizon_end_date after as_of_date");
            }
            ++result.not_yet_matured_count;
            ++result.included_observation_count;
            ++included_cluster_counts[observation.cluster_id];
            break;
        case JointCohortObservationStatus::Unresolved:
            if (observation.horizon_end_date > config.as_of_date) {
                invalid("unresolved rows require a due horizon no later than as_of_date");
            }
            ++result.unresolved_count;
            ++result.included_observation_count;
            ++included_cluster_counts[observation.cluster_id];
            break;
        case JointCohortObservationStatus::Excluded: {
            const auto rule = rules.find(observation.exclusion_rule_id);
            if (rule == rules.end() ||
                !rule->second->outcome_blind_asserted ||
                rule->second->frozen_date > observation.eligible_date) {
                invalid("excluded rows require a configured outcome-blind rule frozen on or before eligible_date");
            }
            ++result.excluded_count;
            ++excluded_by_rule[observation.exclusion_rule_id];
            result.excluded_observations.push_back(
                JointCohortExcludedObservationDisclosure{
                    observation.observation_id, observation.cluster_id,
                    observation.exclusion_rule_id});
            break;
        }
        }
    }
    result.unknown_count =
        result.not_yet_matured_count + result.unresolved_count;
    std::sort(result.excluded_observations.begin(),
        result.excluded_observations.end(),
        [](const JointCohortExcludedObservationDisclosure& left,
           const JointCohortExcludedObservationDisclosure& right) {
            return left.observation_id < right.observation_id;
        });
    for (const JointCohortExclusionRule& rule : config.exclusion_rules) {
        result.exclusion_rule_disclosures.push_back(
            JointCohortExclusionRuleDisclosure{
                rule.id, rule.frozen_date, rule.outcome_blind_asserted,
                rule.statement, excluded_by_rule[rule.id]});
    }
    std::sort(result.exclusion_rule_disclosures.begin(),
        result.exclusion_rule_disclosures.end(),
        [](const JointCohortExclusionRuleDisclosure& left,
           const JointCohortExclusionRuleDisclosure& right) {
            return left.rule_id < right.rule_id;
        });

    CompensatedSum portfolio_weight_sum;
    for (const JointScenario* scenario : scenarios) {
        portfolio_weight_sum.add(static_cast<long double>(scenario->weight));
    }
    std::vector<double> configured_central_weights;
    configured_central_weights.reserve(scenarios.size());
    CompensatedSum configured_central_sum;
    for (const JointScenario* scenario : scenarios) {
        const double normalized = static_cast<double>(
            static_cast<long double>(scenario->weight) /
            portfolio_weight_sum.value());
        configured_central_weights.push_back(normalized);
        configured_central_sum.add(static_cast<long double>(normalized));
    }
    result.scenario_envelopes.reserve(scenarios.size());
    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        JointCohortScenarioEnvelope descriptive;
        descriptive.scenario_id = scenarios[index]->id;
        descriptive.matured_count = matured_by_scenario[index];
        descriptive.compatible_minimum_count = matured_by_scenario[index];
        descriptive.compatible_maximum_count =
            matured_by_scenario[index] + result.unknown_count;
        descriptive.portfolio_reference_weight = static_cast<double>(
            static_cast<long double>(scenarios[index]->weight) /
            portfolio_weight_sum.value());
        result.scenario_envelopes.push_back(std::move(descriptive));
    }

    const auto repeated_cluster = std::find_if(
        included_cluster_counts.begin(), included_cluster_counts.end(),
        [](const auto& item) { return item.second > 1U; });
    result.included_cluster_ids_unique =
        repeated_cluster == included_cluster_counts.end();
    if (!result.included_cluster_ids_unique) {
        result.block_reason =
            "repeated non-excluded cluster_id blocks the asserted IID envelope: " +
            repeated_cluster->first;
        return result;
    }
    if (result.included_observation_count == 0U) {
        result.block_reason =
            "no non-excluded observations remain in the denominator";
        return result;
    }

    const long double alpha = requested_family_alpha;
    const long double category_count =
        static_cast<long double>(scenarios.size());
    const long double denominator =
        2.0L * static_cast<long double>(result.included_observation_count);
    const long double epsilon = std::sqrt(
        (std::log(2.0L) + std::log(category_count) - std::log(alpha)) /
        denominator);
    if (!std::isfinite(epsilon)) {
        throw std::logic_error(
            "joint cohort Hoeffding-Bonferroni radius is not finite");
    }
    result.hoeffding_bonferroni_epsilon =
        outward_upper_double(epsilon);
    result.primary_outer_set_available = true;

    const bool complete = result.unknown_count == 0U;
    long double goodman_critical = 0.0L;
    if (complete && scenarios.size() > 1U) {
        goodman_critical = goodman_critical_value(alpha, scenarios.size());
        result.goodman_diagnostic_available = true;
        result.goodman_sparse_cell_warning = std::any_of(
            matured_by_scenario.begin(), matured_by_scenario.end(),
            [](std::size_t count) { return count < 5U; });
        result.goodman_chi_square_critical_value =
            outward_upper_double(goodman_critical);
    }

    result.portfolio_reference_within_primary_bounds = true;
    PortfolioAmbiguityConfig generated;
    generated.model_version = std::string(kPortfolioAmbiguityModelVersion);
    generated.scenario_label =
        "Synthetic joint-cohort conservative simultaneous outer set";
    generated.source_note =
        "Generated from supplied synthetic raw cohort rows under an asserted IID complete-joint-state candidate assumption";
    generated.synthetic_inputs = true;
    generated.scenario_probabilities.reserve(scenarios.size());

    for (std::size_t index = 0U; index < scenarios.size(); ++index) {
        const std::size_t minimum_count = matured_by_scenario[index];
        const std::size_t maximum_count =
            minimum_count + result.unknown_count;
        const long double n = static_cast<long double>(
            result.included_observation_count);
        long double lower = std::max(
            0.0L, static_cast<long double>(minimum_count) / n - epsilon);
        long double upper = std::min(
            1.0L, static_cast<long double>(maximum_count) / n + epsilon);
        if (epsilon >= 1.0L) {
            lower = 0.0L;
            upper = 1.0L;
        }
        const double published_lower = outward_lower_double(lower);
        const long double normalized_portfolio_reference =
            static_cast<long double>(scenarios[index]->weight) /
            portfolio_weight_sum.value();
        const long double normalized_configured_reference =
            static_cast<long double>(configured_central_weights[index]) /
            configured_central_sum.value();
        const double published_upper = outward_upper_double(upper);
        if (normalized_portfolio_reference <
                static_cast<long double>(published_lower) ||
            normalized_portfolio_reference >
                static_cast<long double>(published_upper) ||
            normalized_configured_reference <
                static_cast<long double>(published_lower) ||
            normalized_configured_reference >
                static_cast<long double>(published_upper)) {
            result.portfolio_reference_within_primary_bounds = false;
        }

        JointCohortScenarioEnvelope& published =
            result.scenario_envelopes[index];
        published.primary_lower_weight = published_lower;
        published.primary_upper_weight = published_upper;
        if (complete) {
            published.descriptive_empirical_frequency = static_cast<double>(
                static_cast<long double>(minimum_count) / n);
            if (result.goodman_diagnostic_available) {
                const auto goodman = goodman_interval(
                    minimum_count, result.included_observation_count,
                    goodman_critical);
                published.goodman_lower_weight = goodman.first;
                published.goodman_upper_weight = goodman.second;
            }
        }
        generated.scenario_probabilities.push_back(
            ScenarioProbabilityBounds{
                scenarios[index]->id, published_lower,
                configured_central_weights[index], published_upper});
    }

    if (!result.portfolio_reference_within_primary_bounds) {
        result.block_reason =
            "declared portfolio reference weights fall outside the conservative primary outer set";
        return result;
    }

    validate_portfolio_ambiguity_config(portfolio, generated);
    PortfolioAmbiguitySummary financial =
        evaluate_portfolio_ambiguity(portfolio, generated);
    add_project_impairment_projections(
        portfolio, generated, financial, result);
    result.generated_probability_envelope = generated;
    result.financial_ranges = std::move(financial);
    result.financial_ranges_available = true;
    return result;
}

} // namespace naturalehia::cellular_finance
