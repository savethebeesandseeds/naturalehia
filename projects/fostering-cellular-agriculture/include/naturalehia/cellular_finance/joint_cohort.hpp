// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kJointCohortVersion{"0.1.0"};
inline constexpr std::string_view kJointCohortProbabilityMeasure{
    "physical-P"};
inline constexpr std::string_view kJointCohortSamplingAssumption{
    "iid-complete-joint-state-candidate"};
inline constexpr std::string_view kJointCohortIntervalMethod{
    "hoeffding-bonferroni-outer-v0.1"};

enum class JointCohortObservationStatus : unsigned char {
    Matured,
    NotYetMatured,
    Unresolved,
    Excluded,
};

struct JointCohortExclusionRule {
    std::string id{};
    std::string frozen_date{};
    bool outcome_blind_asserted{};
    std::string statement{};
};

struct JointCohortAnalysisConfig {
    std::string version{kJointCohortVersion};
    std::string id{};
    std::string as_of_date{};
    std::string source_note{};
    std::string population_definition{};
    std::string sampling_unit_definition{};
    std::string outcome_mapping_definition{};
    std::string horizon_definition{};
    std::string scenario_taxonomy_frozen_date{};
    std::size_t population_frame_count{};
    bool candidate_only{true};
    bool synthetic_inputs{true};
    std::string probability_measure{kJointCohortProbabilityMeasure};
    std::string sampling_assumption{kJointCohortSamplingAssumption};
    std::string interval_method{kJointCohortIntervalMethod};
    double confidence_level{0.95};
    std::vector<JointCohortExclusionRule> exclusion_rules{};
};

struct JointCohortObservation {
    std::string observation_id{};
    std::string cluster_id{};
    std::string eligible_date{};
    std::string horizon_end_date{};
    JointCohortObservationStatus status{
        JointCohortObservationStatus::NotYetMatured};
    std::string scenario_id{};
    std::string classification_date{};
    std::string exclusion_rule_id{};
    std::vector<std::string> evidence_record_ids{};
    std::vector<std::string> requirement_ids{};
};

struct JointCohortScenarioEnvelope {
    std::string scenario_id{};
    std::size_t matured_count{};
    std::size_t compatible_minimum_count{};
    std::size_t compatible_maximum_count{};
    double primary_lower_weight{};
    double portfolio_reference_weight{};
    double primary_upper_weight{};
    std::optional<double> descriptive_empirical_frequency{};
    std::optional<double> goodman_lower_weight{};
    std::optional<double> goodman_upper_weight{};
};

struct JointCohortExcludedObservationDisclosure {
    std::string observation_id{};
    std::string cluster_id{};
    std::string exclusion_rule_id{};
};

struct JointCohortExclusionRuleDisclosure {
    std::string rule_id{};
    std::string frozen_date{};
    bool outcome_blind_asserted{};
    std::string statement{};
    std::size_t excluded_observation_count{};
};

struct JointCohortProjectImpairmentProjection {
    std::string project_id{};
    AmbiguityMetricRange impairment_probability{};
};

struct JointCohortPairImpairmentProjection {
    std::string first_project_id{};
    std::string second_project_id{};
    AmbiguityMetricRange simultaneous_impairment_probability{};
};

struct JointCohortResult {
    bool candidate_only{true};
    bool generated_envelope_synthetic{true};
    bool calibrated_execution_authorized{};
    bool included_cluster_ids_unique{};
    bool primary_outer_set_available{};
    bool portfolio_reference_within_primary_bounds{};
    bool financial_ranges_available{};
    bool goodman_diagnostic_available{};
    bool goodman_sparse_cell_warning{};
    bool project_impairment_projections_available{};
    bool pair_impairment_projections_available{};
    std::string block_reason{};
    std::string pair_impairment_projection_block_reason{};

    std::size_t raw_observation_count{};
    std::size_t included_observation_count{};
    std::size_t matured_count{};
    std::size_t not_yet_matured_count{};
    std::size_t unresolved_count{};
    std::size_t excluded_count{};
    std::size_t unknown_count{};
    double confidence_level{};
    double family_alpha{};
    double hoeffding_bonferroni_epsilon{};
    std::optional<double> goodman_chi_square_critical_value{};

    std::vector<JointCohortScenarioEnvelope> scenario_envelopes{};
    std::vector<JointCohortExcludedObservationDisclosure>
        excluded_observations{};
    std::vector<JointCohortExclusionRuleDisclosure>
        exclusion_rule_disclosures{};

    std::optional<PortfolioAmbiguityConfig>
        generated_probability_envelope{};
    std::optional<PortfolioAmbiguitySummary> financial_ranges{};

    std::vector<JointCohortProjectImpairmentProjection>
        project_impairment_probabilities{};
    std::vector<JointCohortPairImpairmentProjection>
        pair_impairment_probabilities{};
    std::optional<AmbiguityMetricRange> any_project_impairment_probability{};
    std::optional<AmbiguityMetricRange> all_projects_impairment_probability{};
};

[[nodiscard]] std::string_view to_string(
    JointCohortObservationStatus status) noexcept;

// Strict calendar-date predicate shared by the raw-ledger parser and the
// programmatic validation path.
[[nodiscard]] bool is_joint_cohort_iso_date(
    std::string_view value) noexcept;

void validate_joint_cohort_analysis_config(
    const JointCohortAnalysisConfig& config);

// Validates lexical ledger invariants that do not depend on a portfolio or an
// as-of date. It does not aggregate or replace the authoritative rows.
void validate_joint_cohort_ledger_syntax(
    const std::vector<JointCohortObservation>& observations);

// Evaluates only the supplied authoritative observation rows. The primary
// result is a conservative nonasymptotic simultaneous outer confidence set
// under the explicitly asserted IID complete-joint-unit sampling assumption.
// It is never a calibration authorization.
[[nodiscard]] JointCohortResult evaluate_joint_cohort(
    const JointCohortAnalysisConfig& config,
    const PortfolioConfig& portfolio,
    const std::vector<JointCohortObservation>& observations);

} // namespace naturalehia::cellular_finance
