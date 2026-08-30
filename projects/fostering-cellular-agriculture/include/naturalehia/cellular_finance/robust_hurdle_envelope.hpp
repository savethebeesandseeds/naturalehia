// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_issue_price_support.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kRobustHurdleEnvelopeModelVersion{
    "0.1.0"};
inline constexpr std::size_t kRobustHurdleEnvelopeMaximumObservations{128U};
inline constexpr std::size_t
    kRobustHurdleEnvelopeComparabilityAxisCount{8U};
inline constexpr std::size_t
    kRobustHurdleEnvelopeMaximumAdjustmentsPerObservation{8U};
inline constexpr std::size_t
    kRobustHurdleEnvelopeMaximumStructuralWorkUnits{4'000'000U};

enum class RobustHurdleObservationStatus {
    SettledOrderlyArmsLengthTransaction,
    ExecutableTwoSidedQuote,
    NonbindingIndication,
    ModelMark,
};

enum class RobustHurdleObservationTransactionType {
    PrimaryIssuance,
    SecondaryTrade,
    TwoSidedMarketQuote,
};

// Only the first basis is economically eligible in v0.1. It is an external
// model-conditioned rate, not an observed buyer belief: all-in buyer cash
// P+C equals the declared operator over discounted physical-probability
// expected full-claim cash. Expected loss and timing are already embedded.
enum class RobustHurdleObservationReturnBasis {
    AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows,
    GrossPriceExBuyerCostDiscountRate,
    PromisedYieldToMaturity,
    CouponRate,
    InternalRateOfReturn,
    TargetPriceImpliedDiscountRate,
    OtherOrUnresolved,
};

enum class RobustHurdleObservationClaimRelation {
    SameTargetClaim,
    ComparableClaim,
};

enum class RobustHurdleComparabilityAxis {
    ContractualCashflowRights,
    SeniorityAndResidualTailRiskPremiumAfterExpectedCash,
    SystematicCovarianceConcentrationAndResidualModelRiskPremium,
    ContractualTermAndCashflowTiming,
    CurrencyAndMonetaryBasis,
    LiquidityAndTransferability,
    TransactionSize,
    ObservationDateAndMarketRegime,
};

enum class RobustHurdleExpectedCashOperator {
    RobustMinimumOverDeclaredProbabilitySet,
    FixedDeclaredProbabilityVector,
};

enum class RobustHurdleAdjustmentSetBasis {
    JointlyFeasibleTotalInterval,
    ComponentwiseBoxOuterHull,
};

enum class RobustHurdleEvidenceTier {
    DirectSettledIdenticalClaim,
    SettledComparable,
    ExecutableTwoSidedQuote,
};

enum class RobustHurdleEvidenceTierStatus {
    NoEligibleClusters,
    ContaminationBudgetIsInadmissible,
    UninformativeDeclaredDomainIsNotNarrowed,
    IdentifiedSetIsEmpty,
    IdentifiedSetFound,
};

enum class RobustHurdleComparabilityTreatment {
    Matched,
    BoundedLogGrossReturnAdjustment,
    Unresolved,
};

enum class RobustHurdleObservationEligibility {
    Ineligible,
    EligibleSameClaimMarketObservation,
    EligibleComparableMarketObservation,
    EligibleModelAdjustedComparable,
};

enum class RobustHurdleObservationIneligibilityReason {
    ObservationStatusIsNotEligible,
    ObservationDateIsAfterAnalysisAsOfDate,
    SettlementDateIsAfterAnalysisAsOfDate,
    SourceOrEvidenceIsMissing,
    TransactionMarketAnchorIsMissing,
    ReturnBasisIsNotPhysicalExpectedFullClaimDiscountRate,
    ExpectedFullClaimLossAndTimingAreNotIncluded,
    SettledTransactionEvidenceIsIncomplete,
    SettledOrderlyArmsLengthEvidenceIsIncomplete,
    SettledPricePreimageIsNotSingleConnectedInterval,
    FullClaimObservationDateNormalizationIsMissing,
    ReturnNormalizationResultIsMissing,
    PriceOrBuyerCostEvidenceIsMissing,
    ExecutableQuoteSidesAreIncompleteOrIncoherent,
    ExecutableQuoteRatePreimageIsNotEvidenced,
    ExpectedCashReconstructionMetadataIsMissing,
    ExpectedCashReconstructionDoesNotReconcile,
    FixedProbabilityOperatorIsReportOnlyInV01,
    ReferencePriceRelationIsNotIndependent,
    SideRightsOrNonCashConsiderationIsPresent,
    ClaimOrTermIdentityIsMissing,
    SameClaimIdentityDoesNotMatchTarget,
    SameClaimRequiresAllAxesMatchedAndNoAdjustments,
    ComparableAxisIsUnresolved,
    ComparableAdjustmentIsMissing,
    ComparableAdjustmentIsDuplicated,
    ComparableAdjustmentIsUnexpected,
    MatchedCurrencyOrMonetaryBasisDoesNotMatchTarget,
    MatchedTransactionSizeDoesNotMatchTarget,
    MatchedObservationDateDoesNotEqualAnalysisAsOfDate,
    AdjustmentProvenanceIsMissing,
    AdjustmentSetIsOnlyAComponentwiseBoxOuterHull,
    JointAdjustmentSetProvenanceIsMissing,
    JointAdjustmentTotalIsOutsideComponentBounds,
    ExpectedLossRecoveryOrTimingWouldBeDoubleCounted,
};

enum class RobustHurdleMappedIntervalStatus {
    NonemptyAfterDomainClipping,
    EmptyBelowDeclaredDomain,
    EmptyAboveDeclaredDomain,
};

enum class RobustHurdleEnvelopeStatus {
    NoEligibleEconomicObservationClusters,
    InsufficientEligibleClustersForDeclaredContaminationBudget,
    UninformativeDeclaredDomainIsNotNarrowed,
    IdentifiedSetIsEmpty,
    IdentifiedSetFound,
};

enum class RobustHurdleEnvelopeIdentificationBasis {
    Unavailable,
    DirectSingleTransactionConditionedCase,
    LimitedMultiObservationIdentification,
    ComparableConsensusThresholdMet,
};

[[nodiscard]] std::string_view to_string(
    RobustHurdleObservationStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleObservationTransactionType type) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleObservationReturnBasis basis) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleObservationClaimRelation relation) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleExpectedCashOperator value) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleAdjustmentSetBasis value) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleEvidenceTier value) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleEvidenceTierStatus value) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleComparabilityAxis axis) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleComparabilityTreatment treatment) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleObservationEligibility eligibility) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleObservationIneligibilityReason reason) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleMappedIntervalStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleEnvelopeStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustHurdleEnvelopeIdentificationBasis basis) noexcept;

[[nodiscard]] constexpr std::size_t robust_hurdle_axis_index(
    RobustHurdleComparabilityAxis axis) noexcept {
    return static_cast<std::size_t>(axis);
}

struct RobustHurdleLogGrossReturnAdjustmentConfig {
    std::string adjustment_id{"none"};
    RobustHurdleComparabilityAxis axis{
        RobustHurdleComparabilityAxis::ContractualCashflowRights};
    double lower_log_gross_return_delta{0.0};
    double upper_log_gross_return_delta{0.0};
    std::string method_id{"none"};
    std::string source_reference{"none"};
    std::string evidence_record_id{"none"};
    std::string source_note{"none"};
};

// One record represents one already-deduplicated economic-observation
// cluster. Raw transaction-to-return calculation happens outside this engine.
// The supplied interval must use the exact eligible return basis above.
struct RobustHurdleMarketObservationConfig {
    std::string record_id{"none"};
    std::string economic_observation_cluster_id{"none"};
    RobustHurdleObservationStatus status{
        RobustHurdleObservationStatus::ModelMark};
    RobustHurdleObservationTransactionType transaction_type{
        RobustHurdleObservationTransactionType::PrimaryIssuance};
    std::string observation_date{"none"};
    std::string execution_date{"none"};
    std::string settlement_date{"none"};
    std::string source_reference{"none"};
    std::string evidence_record_id{"none"};
    std::string settlement_evidence_record_id{"none"};
    std::string orderly_arms_length_evidence_record_id{"none"};
    std::string transaction_market_anchor_id{"none"};
    bool transaction_execution_is_evidenced{false};
    bool buyer_cash_payment_is_evidenced{false};
    bool transaction_settlement_is_evidenced{false};
    bool orderly_transaction_is_evidenced{false};
    bool arms_length_transaction_is_evidenced{false};
    bool forced_or_distressed_transaction{false};
    bool related_party_transaction{false};
    double settled_claim_quantity_million{0.0};

    std::string observed_market_claim_id{"none"};
    std::string normalized_term_result_id{"none"};
    RobustHurdleObservationClaimRelation claim_relation{
        RobustHurdleObservationClaimRelation::ComparableClaim};
    std::string currency_label{"UNSPECIFIED"};
    std::string monetary_basis{"unspecified monetary basis"};

    RobustHurdleObservationReturnBasis return_basis{
        RobustHurdleObservationReturnBasis::OtherOrUnresolved};
    double annual_effective_hurdle_lower{0.0};
    double annual_effective_hurdle_upper{0.0};
    std::string return_normalization_result_id{"none"};
    double observed_gross_buyer_price_million{0.0};
    double buyer_direct_cost_million{0.0};
    bool observed_gross_buyer_price_is_evidenced{false};
    bool buyer_direct_cost_is_evidenced{false};
    std::string buyer_direct_cost_evidence_record_id{"none"};

    double quoted_bid_gross_buyer_price_million{0.0};
    double quoted_ask_gross_buyer_price_million{0.0};
    double quoted_bid_buyer_direct_cost_million{0.0};
    double quoted_ask_buyer_direct_cost_million{0.0};
    double quoted_bid_claim_quantity_million{0.0};
    double quoted_ask_claim_quantity_million{0.0};
    std::string quoted_bid_timestamp_utc{"none"};
    std::string quoted_ask_timestamp_utc{"none"};
    std::string quoted_bid_currency_label{"UNSPECIFIED"};
    std::string quoted_ask_currency_label{"UNSPECIFIED"};
    bool quoted_bid_and_ask_are_executable{false};
    bool quoted_bid_and_ask_are_evidenced{false};
    std::string quote_evidence_record_id{"none"};
    std::string quote_valid_until_utc{"none"};
    bool discounted_expected_cash_is_strictly_decreasing_over_rate_interval{
        false};
    bool quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper{false};
    bool settled_price_preimage_is_single_connected_interval{false};

    RobustHurdleExpectedCashOperator expected_cash_operator{
        RobustHurdleExpectedCashOperator::FixedDeclaredProbabilityVector};
    std::string full_dated_scenario_cash_input_sha256{"none"};
    std::string probability_input_sha256{"none"};
    std::string expected_cash_calculation_run_sha256{"none"};
    bool expected_cash_reconstruction_identity_is_evidenced{false};
    double maximum_expected_cash_reconstruction_residual_million{0.0};
    bool physical_expected_full_claim_cash_includes_loss_and_timing{false};
    bool normalized_to_full_claim_at_observation_date{false};
    bool fixed_probability_to_robust_bridge_is_present{false};
    double fixed_probability_to_robust_bridge_lower_log_delta{0.0};
    double fixed_probability_to_robust_bridge_upper_log_delta{0.0};
    std::string fixed_probability_to_robust_bridge_method_id{"none"};
    std::string fixed_probability_to_robust_bridge_evidence_record_id{"none"};
    RobustIssuePriceHurdleReferenceRelation target_reference_price_relation{
        RobustIssuePriceHurdleReferenceRelation::Unresolved};

    bool side_rights_or_non_cash_consideration_present{false};
    std::string side_rights_or_non_cash_consideration_note{"none"};

    std::array<RobustHurdleComparabilityTreatment,
        kRobustHurdleEnvelopeComparabilityAxisCount>
        comparability_treatments{
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved,
            RobustHurdleComparabilityTreatment::Unresolved};
    std::vector<RobustHurdleLogGrossReturnAdjustmentConfig> adjustments{};
    RobustHurdleAdjustmentSetBasis adjustment_set_basis{
        RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull};
    double jointly_feasible_total_lower_log_gross_return_delta{0.0};
    double jointly_feasible_total_upper_log_gross_return_delta{0.0};
    std::string joint_adjustment_set_method_id{"none"};
    std::string joint_adjustment_set_evidence_record_id{"none"};
    bool expected_loss_recovery_and_timing_are_excluded_from_adjustments{
        false};
    std::string source_note{"none"};
};

struct RobustHurdleEnvelopeConfig {
    std::string model_version{kRobustHurdleEnvelopeModelVersion};
    std::string analysis_id{"none"};
    std::string as_of_date{"none"};
    std::string source_note{"none"};
    bool synthetic_inputs{true};

    std::string universe_manifest_id{"none"};
    std::string inclusion_rule_id{"none"};
    std::string deduplication_manifest_id{"none"};
    std::string source_clustering_rule_id{"none"};
    bool observation_universe_is_frozen{false};
    bool inclusion_rule_is_predeclared{false};
    bool deduplication_rule_is_predeclared{false};
    bool all_in_scope_economic_observation_clusters_are_included{false};
    bool same_dealer_same_window_quotes_are_clustered{false};

    std::string target_market_claim_id{"none"};
    std::string target_normalized_term_result_id{"none"};
    std::string target_currency_label{"UNSPECIFIED"};
    std::string target_monetary_basis{"unspecified monetary basis"};
    double target_claim_quantity_million{0.0};

    double annual_effective_domain_lower{0.0};
    double annual_effective_domain_upper{10.0};
    std::size_t maximum_contaminated_clusters{0U};
    std::size_t minimum_consensus_cluster_coverage{3U};
    std::vector<RobustHurdleMarketObservationConfig> observations{};
};

struct RobustHurdleClosedInterval {
    double lower{0.0};
    double upper{0.0};
    std::size_t lower_endpoint_coverage{0U};
    std::size_t upper_endpoint_coverage{0U};
    std::vector<std::string> lower_boundary_witness_cluster_ids{};
    std::vector<std::string> upper_boundary_witness_cluster_ids{};
};

struct RobustHurdleObservationResult {
    std::string record_id{};
    std::string economic_observation_cluster_id{};
    RobustHurdleObservationEligibility eligibility{
        RobustHurdleObservationEligibility::Ineligible};
    std::optional<RobustHurdleEvidenceTier> evidence_tier{};
    std::vector<RobustHurdleObservationIneligibilityReason>
        ineligibility_reasons{};

    double source_interval_lower{0.0};
    double source_interval_upper{0.0};
    double summed_lower_log_gross_return_adjustment{0.0};
    double summed_upper_log_gross_return_adjustment{0.0};
    double applied_lower_log_gross_return_adjustment{0.0};
    double applied_upper_log_gross_return_adjustment{0.0};
    std::optional<double> mapped_interval_lower{};
    std::optional<double> mapped_interval_upper{};
    RobustHurdleMappedIntervalStatus mapped_interval_status{
        RobustHurdleMappedIntervalStatus::NonemptyAfterDomainClipping};
    std::optional<RobustHurdleClosedInterval> clipped_interval{};
    double maximum_normalization_identity_error{0.0};
};

struct RobustHurdleIdentifiedSet {
    std::size_t maximum_contaminated_clusters{0U};
    std::size_t required_cluster_coverage{0U};
    std::size_t maximum_attained_coverage{0U};
    std::vector<RobustHurdleClosedInterval> components{};
};

struct RobustHurdleLeaveOneClusterOutResult {
    std::size_t omitted_eligible_observation_index{0U};
    std::string omitted_economic_observation_cluster_id{};
    bool declared_contamination_budget_remains_admissible{false};
    std::size_t remaining_eligible_cluster_count{0U};
    std::size_t required_cluster_coverage{0U};
    std::vector<RobustHurdleClosedInterval> components{};
};

struct RobustHurdleEvidenceTierResult {
    RobustHurdleEvidenceTier tier{
        RobustHurdleEvidenceTier::DirectSettledIdenticalClaim};
    RobustHurdleEvidenceTierStatus status{
        RobustHurdleEvidenceTierStatus::NoEligibleClusters};
    std::size_t eligible_cluster_count{0U};
    std::size_t required_cluster_coverage{0U};
    std::size_t declared_quorum{0U};
    bool comparable_consensus_label_threshold_met{false};
    std::vector<std::size_t> eligible_observation_indices{};
    std::vector<std::size_t> informative_observation_indices{};
    std::vector<std::size_t> binding_observation_indices{};
    std::vector<RobustHurdleIdentifiedSet> identified_sets_s0_through_sk{};
    std::vector<RobustHurdleLeaveOneClusterOutResult>
        leave_one_cluster_out{};
    std::optional<RobustHurdleClosedInterval>
        eligible_interval_hull_diagnostic{};
};

struct RobustHurdleEnvelopeWorkCounts {
    std::size_t comparability_axis_work_units{0U};
    std::size_t adjustment_normalization_work_units{0U};
    std::size_t identified_set_event_work_units{0U};
    std::size_t leave_one_cluster_out_event_work_units{0U};
    std::size_t structural_work_units{0U};
    std::size_t structural_work_unit_limit{
        kRobustHurdleEnvelopeMaximumStructuralWorkUnits};
};

struct RobustHurdleEnvelopeSummary {
    RobustHurdleEnvelopeStatus status{
        RobustHurdleEnvelopeStatus::NoEligibleEconomicObservationClusters};
    RobustHurdleEnvelopeIdentificationBasis identification_basis{
        RobustHurdleEnvelopeIdentificationBasis::Unavailable};

    std::size_t input_observation_count{0U};
    std::size_t eligible_cluster_count{0U};
    std::size_t financially_ineligible_cluster_count{0U};
    std::size_t eligible_empty_interval_cluster_count{0U};
    std::size_t maximum_contaminated_clusters{0U};
    std::size_t required_cluster_coverage{0U};

    std::vector<RobustHurdleObservationResult> observations{};
    std::vector<std::size_t> eligible_observation_indices{};
    std::vector<RobustHurdleEvidenceTierResult> evidence_tiers{};

    std::optional<RobustHurdleEvidenceTier> selected_evidence_tier{};
    std::optional<RobustHurdleIdentifiedSet> selected_identified_set{};

    RobustHurdleEnvelopeWorkCounts work{};
    double maximum_normalization_identity_error{0.0};
    bool all_identified_sets_are_closed_disjoint_and_canonical{false};
    bool every_selected_component_meets_its_coverage_threshold{false};
    bool every_selected_gap_is_below_its_coverage_threshold{false};

    bool arithmetic_averaging_or_observation_weighting_is_used{false};
    bool statistical_confidence_or_coverage_is_estimated{false};
    bool observation_independence_is_established{false};
    bool market_hurdle_is_point_identified{false};
    bool selected_rate_set_is_singleton{false};
    bool buyer_hurdle_or_investor_beliefs_are_observed{false};
    bool annual_expected_holding_period_return_is_inferred{false};
    bool synthetic_inputs{true};
    bool empirical_hurdle_evidence_release_authorized{false};
    bool source_identifiers_or_assertion_booleans_authenticate_documents{false};
    bool mechanical_candidate_set_only{true};
    bool target_reference_price_is_used_to_infer_a_hurdle{false};
    bool fair_value_market_price_demand_or_placement_is_established{false};
    std::string model_limitation{};
};

void validate_robust_hurdle_envelope_config(
    const RobustHurdleEnvelopeConfig& config);

// Finite, deterministic set identification. Financially ineligible records
// remain in the result. Eligible mapped intervals wholly outside the declared
// domain remain eligible as empty sets and therefore can falsify consensus.
[[nodiscard]] RobustHurdleEnvelopeSummary evaluate_robust_hurdle_envelope(
    const RobustHurdleEnvelopeConfig& config);

} // namespace naturalehia::cellular_finance
