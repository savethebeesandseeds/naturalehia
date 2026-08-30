// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_hurdle_envelope.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void expect_invalid(
    const std::function<void()>& action, const std::string& message) {
    try {
        action();
        check(false, message);
    } catch (const std::invalid_argument&) {
    }
}

[[nodiscard]] bool close(double first, double second) {
    return std::abs(first - second) <= 1.0e-10;
}

[[nodiscard]] std::string hash(char value) {
    return std::string(64U, value);
}

[[nodiscard]] cf::RobustHurdleMarketObservationConfig settled_same(
    std::string id, double lower, double upper) {
    cf::RobustHurdleMarketObservationConfig observation;
    observation.record_id = std::move(id);
    observation.economic_observation_cluster_id =
        observation.record_id + "-cluster";
    observation.status = cf::RobustHurdleObservationStatus::
        SettledOrderlyArmsLengthTransaction;
    observation.transaction_type =
        cf::RobustHurdleObservationTransactionType::PrimaryIssuance;
    observation.observation_date = "2026-01-15";
    observation.execution_date = "2026-01-15";
    observation.settlement_date = "2026-01-15";
    observation.source_reference = "Controlled transaction record";
    observation.evidence_record_id = observation.record_id + "-evidence";
    observation.settlement_evidence_record_id =
        observation.record_id + "-settlement";
    observation.orderly_arms_length_evidence_record_id =
        observation.record_id + "-orderly";
    observation.transaction_execution_is_evidenced = true;
    observation.buyer_cash_payment_is_evidenced = true;
    observation.transaction_settlement_is_evidenced = true;
    observation.orderly_transaction_is_evidenced = true;
    observation.arms_length_transaction_is_evidenced = true;
    observation.settled_claim_quantity_million = 100.0;
    observation.observed_market_claim_id = "market-priority";
    observation.normalized_term_result_id = "fixed-term-result";
    observation.claim_relation =
        cf::RobustHurdleObservationClaimRelation::SameTargetClaim;
    observation.currency_label = "EUR";
    observation.monetary_basis = "nominal EUR million at 2026-01-01";
    observation.return_basis = cf::RobustHurdleObservationReturnBasis::
        AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows;
    observation.annual_effective_hurdle_lower = lower;
    observation.annual_effective_hurdle_upper = upper;
    observation.return_normalization_result_id =
        observation.record_id + "-return";
    observation.observed_gross_buyer_price_million = 90.0;
    observation.buyer_direct_cost_million = 2.0;
    observation.observed_gross_buyer_price_is_evidenced = true;
    observation.buyer_direct_cost_is_evidenced = true;
    observation.buyer_direct_cost_evidence_record_id =
        observation.record_id + "-buyer-cost";
    observation.expected_cash_operator = cf::RobustHurdleExpectedCashOperator::
        RobustMinimumOverDeclaredProbabilitySet;
    observation.full_dated_scenario_cash_input_sha256 = hash('a');
    observation.probability_input_sha256 = hash('b');
    observation.expected_cash_calculation_run_sha256 = hash('c');
    observation.expected_cash_reconstruction_identity_is_evidenced = true;
    observation.physical_expected_full_claim_cash_includes_loss_and_timing =
        true;
    observation.normalized_to_full_claim_at_observation_date = true;
    observation.target_reference_price_relation =
        cf::RobustIssuePriceHurdleReferenceRelation::Independent;
    observation.settled_price_preimage_is_single_connected_interval = true;
    observation.comparability_treatments.fill(
        cf::RobustHurdleComparabilityTreatment::Matched);
    observation.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval;
    observation.source_note =
        "Synthetic mechanically complete settled observation";
    return observation;
}

[[nodiscard]] cf::RobustHurdleMarketObservationConfig settled_comparable(
    std::string id, double lower, double upper) {
    auto result = settled_same(std::move(id), lower, upper);
    result.observed_market_claim_id = result.record_id + "-claim";
    result.normalized_term_result_id = result.record_id + "-term";
    result.claim_relation =
        cf::RobustHurdleObservationClaimRelation::ComparableClaim;
    return result;
}

[[nodiscard]] cf::RobustHurdleMarketObservationConfig quote(
    std::string id, double lower, double upper) {
    auto result = settled_comparable(std::move(id), lower, upper);
    result.status = cf::RobustHurdleObservationStatus::ExecutableTwoSidedQuote;
    result.transaction_type =
        cf::RobustHurdleObservationTransactionType::TwoSidedMarketQuote;
    result.execution_date = "none";
    result.settlement_date = "none";
    result.settlement_evidence_record_id = "none";
    result.orderly_arms_length_evidence_record_id = "none";
    result.transaction_execution_is_evidenced = false;
    result.buyer_cash_payment_is_evidenced = false;
    result.transaction_settlement_is_evidenced = false;
    result.orderly_transaction_is_evidenced = false;
    result.arms_length_transaction_is_evidenced = false;
    result.settled_claim_quantity_million = 0.0;
    result.settled_price_preimage_is_single_connected_interval = false;
    result.observed_gross_buyer_price_million = 0.0;
    result.observed_gross_buyer_price_is_evidenced = false;
    result.transaction_market_anchor_id = result.record_id + "-anchor";
    result.quoted_bid_gross_buyer_price_million = 88.0;
    result.quoted_ask_gross_buyer_price_million = 90.0;
    result.quoted_bid_buyer_direct_cost_million = 1.0;
    result.quoted_ask_buyer_direct_cost_million = 1.0;
    result.quoted_bid_claim_quantity_million = 100.0;
    result.quoted_ask_claim_quantity_million = 100.0;
    result.quoted_bid_timestamp_utc = "2026-01-15T10:00:00Z";
    result.quoted_ask_timestamp_utc = "2026-01-15T10:00:00Z";
    result.quoted_bid_currency_label = "EUR";
    result.quoted_ask_currency_label = "EUR";
    result.quoted_bid_and_ask_are_executable = true;
    result.quoted_bid_and_ask_are_evidenced = true;
    result.quote_evidence_record_id = result.record_id + "-quote";
    result.quote_valid_until_utc = "2026-01-15T12:00:00Z";
    result.discounted_expected_cash_is_strictly_decreasing_over_rate_interval =
        true;
    result.quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper = true;
    return result;
}

[[nodiscard]] cf::RobustHurdleEnvelopeConfig base_config() {
    cf::RobustHurdleEnvelopeConfig config;
    config.analysis_id = "synthetic-hurdle-envelope";
    config.as_of_date = "2026-01-15";
    config.source_note = "Synthetic fixture for exact mechanics";
    config.synthetic_inputs = true;
    config.universe_manifest_id = "universe-v1";
    config.inclusion_rule_id = "inclusion-v1";
    config.deduplication_manifest_id = "dedup-v1";
    config.source_clustering_rule_id = "source-cluster-v1";
    config.observation_universe_is_frozen = true;
    config.inclusion_rule_is_predeclared = true;
    config.deduplication_rule_is_predeclared = true;
    config.all_in_scope_economic_observation_clusters_are_included = true;
    config.same_dealer_same_window_quotes_are_clustered = true;
    config.target_market_claim_id = "market-priority";
    config.target_normalized_term_result_id = "fixed-term-result";
    config.target_currency_label = "EUR";
    config.target_monetary_basis = "nominal EUR million at 2026-01-01";
    config.target_claim_quantity_million = 100.0;
    config.annual_effective_domain_lower = 0.0;
    config.annual_effective_domain_upper = 1.0;
    config.maximum_contaminated_clusters = 0U;
    config.minimum_consensus_cluster_coverage = 3U;
    config.observations = {settled_same("direct", 0.05, 0.10)};
    return config;
}

[[nodiscard]] const cf::RobustHurdleEvidenceTierResult& selected_tier(
    const cf::RobustHurdleEnvelopeSummary& summary) {
    return *std::find_if(summary.evidence_tiers.begin(),
        summary.evidence_tiers.end(), [&](const auto& tier) {
            return summary.selected_evidence_tier.has_value() &&
                tier.tier == *summary.selected_evidence_tier;
        });
}

void test_direct_transaction_conditioned_set() {
    const auto summary = cf::evaluate_robust_hurdle_envelope(base_config());
    check(summary.status == cf::RobustHurdleEnvelopeStatus::IdentifiedSetFound,
        "direct mechanically complete transaction produces a nonempty candidate set");
    check(summary.identification_basis ==
            cf::RobustHurdleEnvelopeIdentificationBasis::
                DirectSingleTransactionConditionedCase,
        "one settled identical transaction is labelled transaction-conditioned, not consensus");
    check(summary.selected_evidence_tier ==
            cf::RobustHurdleEvidenceTier::DirectSettledIdenticalClaim,
        "direct settled identical evidence is the highest tier");
    check(!selected_tier(summary).comparable_consensus_label_threshold_met,
        "a single direct transaction is never labelled comparable consensus");
    check(summary.selected_identified_set->components.size() == 1U &&
            close(summary.selected_identified_set->components[0].lower, 0.05) &&
            close(summary.selected_identified_set->components[0].upper, 0.10),
        "S_0 retains the exact supplied closed interval");
    check(summary.every_selected_component_meets_its_coverage_threshold &&
            summary.every_selected_gap_is_below_its_coverage_threshold,
        "coverage audit checks endpoints and adjacent cells");
    check(summary.synthetic_inputs && summary.mechanical_candidate_set_only &&
            !summary.empirical_hurdle_evidence_release_authorized &&
            !summary.source_identifiers_or_assertion_booleans_authenticate_documents &&
            !summary.buyer_hurdle_or_investor_beliefs_are_observed &&
            !summary.annual_expected_holding_period_return_is_inferred,
        "synthetic mechanics retain all evidence and interpretation boundaries");

    auto real_label_only = base_config();
    real_label_only.synthetic_inputs = false;
    const auto real_label_summary =
        cf::evaluate_robust_hurdle_envelope(real_label_only);
    check(!real_label_summary.synthetic_inputs &&
            real_label_summary.mechanical_candidate_set_only &&
            !real_label_summary.empirical_hurdle_evidence_release_authorized &&
            !real_label_summary
                 .source_identifiers_or_assertion_booleans_authenticate_documents,
        "a non-synthetic label never promotes mechanical output into authenticated empirical evidence");

    auto direct_multi = base_config();
    direct_multi.observations = {settled_same("direct-a", 0.05, 0.10),
        settled_same("direct-b", 0.06, 0.11),
        settled_same("direct-c", 0.07, 0.12)};
    const auto direct_multi_summary =
        cf::evaluate_robust_hurdle_envelope(direct_multi);
    check(direct_multi_summary.identification_basis ==
            cf::RobustHurdleEnvelopeIdentificationBasis::
                LimitedMultiObservationIdentification &&
            !selected_tier(direct_multi_summary)
                 .comparable_consensus_label_threshold_met,
        "multiple direct identical-claim transactions remain direct evidence, not comparable consensus");
}

void test_touching_intervals_preserve_singleton() {
    auto config = base_config();
    config.observations = {settled_comparable("left", 0.10, 0.20),
        settled_comparable("right", 0.20, 0.30)};
    const auto summary = cf::evaluate_robust_hurdle_envelope(config);
    check(summary.selected_evidence_tier ==
            cf::RobustHurdleEvidenceTier::SettledComparable &&
            summary.required_cluster_coverage == 2U,
        "two settled comparables form their own tier with q=2");
    check(summary.selected_rate_set_is_singleton &&
            summary.selected_identified_set->components.size() == 1U &&
            summary.selected_identified_set->components[0].lower == 0.20 &&
            summary.selected_identified_set->components[0].upper == 0.20,
        "closed touching intervals identify their shared endpoint exactly");
    check(summary.identification_basis ==
            cf::RobustHurdleEnvelopeIdentificationBasis::
                LimitedMultiObservationIdentification,
        "q=2 remains identified but is not labelled comparable consensus");
    check(!selected_tier(summary).comparable_consensus_label_threshold_met,
        "q=2 remains below the declared comparable-consensus label threshold");
}

void test_sk_is_exact_disjoint_union() {
    auto config = base_config();
    config.maximum_contaminated_clusters = 2U;
    config.observations = {
        settled_comparable("a-left", 0.10, 0.20),
        settled_comparable("b-left", 0.10, 0.20),
        settled_comparable("c-right", 0.40, 0.50),
        settled_comparable("d-right", 0.40, 0.50),
        settled_comparable("e-span", 0.10, 0.50)};
    const auto summary = cf::evaluate_robust_hurdle_envelope(config);
    const auto& tier = selected_tier(summary);
    check(tier.identified_sets_s0_through_sk.size() == 3U,
        "engine publishes S_0 through S_k");
    const auto& sk = tier.identified_sets_s0_through_sk[2].components;
    check(sk.size() == 2U && sk[0].lower == 0.10 &&
            sk[0].upper == 0.20 && sk[1].lower == 0.40 &&
            sk[1].upper == 0.50,
        "coverage q=3 yields an exact disjoint union without hull bridging");
    check(tier.eligible_interval_hull_diagnostic.has_value() &&
            tier.eligible_interval_hull_diagnostic->lower == 0.10 &&
            tier.eligible_interval_hull_diagnostic->upper == 0.50,
        "the broader hull is retained only as a separately labelled diagnostic");
    check(summary.identification_basis ==
            cf::RobustHurdleEnvelopeIdentificationBasis::
                ComparableConsensusThresholdMet,
        "q=3 receives only the declared consensus-threshold label");
    check(tier.comparable_consensus_label_threshold_met,
        "q=3 explicitly meets the declared comparable-consensus label threshold");
}

void test_evidence_tiers_do_not_pool_or_outvote() {
    auto config = base_config();
    config.observations = {settled_comparable("settled-a", 0.15, 0.25),
        settled_comparable("settled-b", 0.20, 0.30),
        quote("quote-a", 0.50, 0.60), quote("quote-b", 0.50, 0.60),
        quote("quote-c", 0.50, 0.60)};
    const auto summary = cf::evaluate_robust_hurdle_envelope(config);
    check(summary.selected_evidence_tier ==
            cf::RobustHurdleEvidenceTier::SettledComparable,
        "higher settled-comparable tier controls even when q=2 and quotes have q=3");
    check(summary.selected_identified_set->components.size() == 1U &&
            summary.selected_identified_set->components[0].lower == 0.20 &&
            summary.selected_identified_set->components[0].upper == 0.25,
        "lower quote tier never enters or outvotes the settled tier set");
    check(summary.evidence_tiers[2].identified_sets_s0_through_sk.back()
                .components[0]
                .lower == 0.50,
        "lower-tier quote result remains separately reported");
}

void test_inadmissible_higher_tier_does_not_fall_through() {
    auto config = base_config();
    config.maximum_contaminated_clusters = 1U;
    config.observations = {settled_same("direct", 0.05, 0.10),
        settled_comparable("comparable-a", 0.15, 0.25),
        settled_comparable("comparable-b", 0.20, 0.30),
        settled_comparable("comparable-c", 0.20, 0.35)};

    const auto summary = cf::evaluate_robust_hurdle_envelope(config);
    check(summary.selected_evidence_tier ==
            cf::RobustHurdleEvidenceTier::DirectSettledIdenticalClaim,
        "the first nonempty and highest-quality tier controls even when its contamination budget is inadmissible");
    check(!summary.selected_identified_set.has_value() &&
            summary.status == cf::RobustHurdleEnvelopeStatus::
                InsufficientEligibleClustersForDeclaredContaminationBudget,
        "an inadmissible higher tier reports insufficient evidence instead of falling through");
    check(summary.evidence_tiers[0].status ==
            cf::RobustHurdleEvidenceTierStatus::
                ContaminationBudgetIsInadmissible &&
            !summary.evidence_tiers[1].identified_sets_s0_through_sk.empty(),
        "lower-tier calculations remain challenger diagnostics only");
    check(summary.identification_basis ==
            cf::RobustHurdleEnvelopeIdentificationBasis::Unavailable &&
            !summary.selected_rate_set_is_singleton,
        "an insufficient selected tier carries no identification label or singleton claim");
}

void test_uninformative_and_empty_eligible_sets() {
    auto uninformative = base_config();
    uninformative.observations = {settled_same("direct", 0.0, 1.0)};
    const auto broad = cf::evaluate_robust_hurdle_envelope(uninformative);
    check(broad.status == cf::RobustHurdleEnvelopeStatus::
            UninformativeDeclaredDomainIsNotNarrowed,
        "S_k equal to D is explicitly uninformative, not identified");
    check(selected_tier(broad).informative_observation_indices.empty() &&
            broad.identification_basis ==
                cf::RobustHurdleEnvelopeIdentificationBasis::Unavailable &&
            !selected_tier(broad).comparable_consensus_label_threshold_met,
        "a full-domain cluster is neither informative nor an identification basis");

    auto three_full = base_config();
    three_full.observations = {settled_comparable("full-a", 0.0, 1.0),
        settled_comparable("full-b", 0.0, 1.0),
        settled_comparable("full-c", 0.0, 1.0)};
    const auto full_q3 = cf::evaluate_robust_hurdle_envelope(three_full);
    check(full_q3.status == cf::RobustHurdleEnvelopeStatus::
            UninformativeDeclaredDomainIsNotNarrowed &&
            full_q3.identification_basis ==
                cf::RobustHurdleEnvelopeIdentificationBasis::Unavailable &&
            !selected_tier(full_q3).comparable_consensus_label_threshold_met,
        "q=3 full-domain comparables remain uninformative, not consensus");

    auto three_empty = base_config();
    three_empty.observations = {settled_comparable("empty-a", 11.0, 12.0),
        settled_comparable("empty-b", 11.0, 12.0),
        settled_comparable("empty-c", 11.0, 12.0)};
    const auto empty_q3 = cf::evaluate_robust_hurdle_envelope(three_empty);
    check(empty_q3.status ==
            cf::RobustHurdleEnvelopeStatus::IdentifiedSetIsEmpty &&
            empty_q3.identification_basis ==
                cf::RobustHurdleEnvelopeIdentificationBasis::Unavailable &&
            !selected_tier(empty_q3).comparable_consensus_label_threshold_met,
        "q=3 empty comparable preimages falsify the domain but do not create consensus");

    auto mixed_empty = base_config();
    mixed_empty.maximum_contaminated_clusters = 1U;
    mixed_empty.observations = {
        settled_comparable("mixed-a", 0.10, 0.20),
        settled_comparable("mixed-b", 0.15, 0.25),
        settled_comparable("mixed-empty", 11.0, 12.0)};
    const auto mixed = cf::evaluate_robust_hurdle_envelope(mixed_empty);
    check(mixed.eligible_cluster_count == 3U &&
            mixed.eligible_empty_interval_cluster_count == 1U &&
            mixed.required_cluster_coverage == 2U &&
            mixed.selected_identified_set->components.size() == 1U &&
            mixed.selected_identified_set->components[0].lower == 0.15 &&
            mixed.selected_identified_set->components[0].upper == 0.20,
        "an empty eligible H_j remains in n and q while the nonempty intervals determine exact coverage");

    auto outside = base_config();
    auto shifted = settled_comparable("outside", 0.10, 0.20);
    const std::size_t liquidity = cf::robust_hurdle_axis_index(
        cf::RobustHurdleComparabilityAxis::LiquidityAndTransferability);
    shifted.comparability_treatments[liquidity] =
        cf::RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;
    cf::RobustHurdleLogGrossReturnAdjustmentConfig adjustment;
    adjustment.adjustment_id = "liquidity-shift";
    adjustment.axis =
        cf::RobustHurdleComparabilityAxis::LiquidityAndTransferability;
    adjustment.lower_log_gross_return_delta = 2.0;
    adjustment.upper_log_gross_return_delta = 2.0;
    adjustment.method_id = "liquidity-method";
    adjustment.source_reference = "Synthetic adjustment evidence";
    adjustment.evidence_record_id = "liquidity-evidence";
    adjustment.source_note = "Exact synthetic shift";
    shifted.adjustments = {adjustment};
    shifted.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval;
    shifted.jointly_feasible_total_lower_log_gross_return_delta = 2.0;
    shifted.jointly_feasible_total_upper_log_gross_return_delta = 2.0;
    shifted.joint_adjustment_set_method_id = "joint-method";
    shifted.joint_adjustment_set_evidence_record_id = "joint-evidence";
    shifted.expected_loss_recovery_and_timing_are_excluded_from_adjustments =
        true;
    outside.observations = {shifted};
    const auto falsifying = cf::evaluate_robust_hurdle_envelope(outside);
    check(falsifying.eligible_cluster_count == 1U &&
            falsifying.eligible_empty_interval_cluster_count == 1U &&
            falsifying.status ==
                cf::RobustHurdleEnvelopeStatus::IdentifiedSetIsEmpty,
        "a mapped interval wholly outside D remains eligible as an empty falsifying set");
    check(falsifying.identification_basis ==
            cf::RobustHurdleEnvelopeIdentificationBasis::Unavailable &&
            !selected_tier(falsifying)
                 .comparable_consensus_label_threshold_met,
        "an empty selected set has no identification or consensus label");

    auto directly_above = base_config();
    directly_above.observations = {settled_same("above", 11.0, 12.0)};
    const auto above = cf::evaluate_robust_hurdle_envelope(directly_above);
    check(above.eligible_cluster_count == 1U &&
            above.eligible_empty_interval_cluster_count == 1U &&
            above.status ==
                cf::RobustHurdleEnvelopeStatus::IdentifiedSetIsEmpty &&
            above.observations[0].mapped_interval_status ==
                cf::RobustHurdleMappedIntervalStatus::
                    EmptyAboveDeclaredDomain,
        "a valid source preimage wholly above D is an eligible empty falsifier, not rejected by a source-rate cap");

    auto directly_below = base_config();
    directly_below.observations = {settled_same("below", -0.90, -0.50)};
    const auto below = cf::evaluate_robust_hurdle_envelope(directly_below);
    check(below.eligible_cluster_count == 1U &&
            below.eligible_empty_interval_cluster_count == 1U &&
            below.status ==
                cf::RobustHurdleEnvelopeStatus::IdentifiedSetIsEmpty &&
            below.observations[0].mapped_interval_status ==
                cf::RobustHurdleMappedIntervalStatus::
                    EmptyBelowDeclaredDomain,
        "a finite source preimage above -100 percent but wholly below D is also an eligible empty falsifier");

    auto rounded_to_minus_one = base_config();
    auto extreme = settled_comparable("rounded-minus-one",
        std::nextafter(-1.0, 0.0), std::nextafter(-1.0, 0.0));
    const std::size_t extreme_liquidity = cf::robust_hurdle_axis_index(
        cf::RobustHurdleComparabilityAxis::LiquidityAndTransferability);
    extreme.comparability_treatments[extreme_liquidity] =
        cf::RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;
    cf::RobustHurdleLogGrossReturnAdjustmentConfig extreme_adjustment;
    extreme_adjustment.adjustment_id = "extreme-liquidity-shift";
    extreme_adjustment.axis =
        cf::RobustHurdleComparabilityAxis::LiquidityAndTransferability;
    extreme_adjustment.lower_log_gross_return_delta = -16.0;
    extreme_adjustment.upper_log_gross_return_delta = -16.0;
    extreme_adjustment.method_id = "extreme-method";
    extreme_adjustment.source_reference = "Adversarial numerical fixture";
    extreme_adjustment.evidence_record_id = "extreme-evidence";
    extreme_adjustment.source_note = "Forces expm1 rounding boundary";
    extreme.adjustments = {extreme_adjustment};
    extreme.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval;
    extreme.jointly_feasible_total_lower_log_gross_return_delta = -16.0;
    extreme.jointly_feasible_total_upper_log_gross_return_delta = -16.0;
    extreme.joint_adjustment_set_method_id = "extreme-joint-method";
    extreme.joint_adjustment_set_evidence_record_id =
        "extreme-joint-evidence";
    extreme.expected_loss_recovery_and_timing_are_excluded_from_adjustments =
        true;
    rounded_to_minus_one.observations = {extreme};
    expect_invalid(
        [&] { (void)cf::evaluate_robust_hurdle_envelope(rounded_to_minus_one); },
        "a transformed rate that rounds to exactly -1 must fail before poisoning the identity audit");
}

void test_ineligible_records_and_adjustment_guards() {
    auto config = base_config();
    auto mark = settled_comparable("mark", 0.10, 0.20);
    mark.status = cf::RobustHurdleObservationStatus::ModelMark;
    mark.execution_date = "none";
    mark.settlement_date = "none";
    mark.settlement_evidence_record_id = "none";
    mark.orderly_arms_length_evidence_record_id = "none";
    mark.transaction_execution_is_evidenced = false;
    mark.buyer_cash_payment_is_evidenced = false;
    mark.transaction_settlement_is_evidenced = false;
    mark.orderly_transaction_is_evidenced = false;
    mark.arms_length_transaction_is_evidenced = false;
    mark.settled_claim_quantity_million = 0.0;
    mark.settled_price_preimage_is_single_connected_interval = false;
    mark.return_basis =
        cf::RobustHurdleObservationReturnBasis::PromisedYieldToMaturity;
    config.observations = {mark};
    const auto report = cf::evaluate_robust_hurdle_envelope(config);
    check(report.status == cf::RobustHurdleEnvelopeStatus::
            NoEligibleEconomicObservationClusters &&
            report.observations[0].ineligibility_reasons.size() >= 2U,
        "model marks and promised YTM remain retained with explicit reasons");

    auto fixed = base_config();
    fixed.observations[0].expected_cash_operator =
        cf::RobustHurdleExpectedCashOperator::FixedDeclaredProbabilityVector;
    fixed.observations[0].fixed_probability_to_robust_bridge_is_present = true;
    fixed.observations[0].fixed_probability_to_robust_bridge_lower_log_delta =
        -0.01;
    fixed.observations[0].fixed_probability_to_robust_bridge_upper_log_delta =
        0.01;
    fixed.observations[0].fixed_probability_to_robust_bridge_method_id =
        "fixed-bridge";
    fixed.observations[0].fixed_probability_to_robust_bridge_evidence_record_id =
        "fixed-bridge-evidence";
    const auto fixed_report = cf::evaluate_robust_hurdle_envelope(fixed);
    check(fixed_report.eligible_cluster_count == 0U &&
            std::find(fixed_report.observations[0].ineligibility_reasons.begin(),
                fixed_report.observations[0].ineligibility_reasons.end(),
                cf::RobustHurdleObservationIneligibilityReason::
                    FixedProbabilityOperatorIsReportOnlyInV01) !=
                fixed_report.observations[0].ineligibility_reasons.end(),
        "fixed-p expected cash is report-only even when a bridge is declared");

    auto box = base_config();
    auto adjusted = settled_comparable("box", 0.10, 0.20);
    const std::size_t date_axis = cf::robust_hurdle_axis_index(
        cf::RobustHurdleComparabilityAxis::ObservationDateAndMarketRegime);
    adjusted.comparability_treatments[date_axis] =
        cf::RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;
    cf::RobustHurdleLogGrossReturnAdjustmentConfig date_adjustment;
    date_adjustment.adjustment_id = "date";
    date_adjustment.axis =
        cf::RobustHurdleComparabilityAxis::ObservationDateAndMarketRegime;
    date_adjustment.lower_log_gross_return_delta = -0.01;
    date_adjustment.upper_log_gross_return_delta = 0.01;
    date_adjustment.method_id = "date-method";
    date_adjustment.source_reference = "Date method evidence";
    date_adjustment.evidence_record_id = "date-evidence";
    date_adjustment.source_note = "Synthetic box";
    adjusted.adjustments = {date_adjustment};
    adjusted.expected_loss_recovery_and_timing_are_excluded_from_adjustments =
        true;
    adjusted.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull;
    adjusted.joint_adjustment_set_method_id = "joint-method";
    adjusted.joint_adjustment_set_evidence_record_id = "joint-evidence";
    box.observations = {adjusted};
    check(cf::evaluate_robust_hurdle_envelope(box).eligible_cluster_count == 0U,
        "a componentwise Cartesian outer hull is report-only, not primary evidence");
}

void test_quote_and_settlement_evidence_guards() {
    auto config = base_config();
    config.observations = {quote("quote", 0.20, 0.30)};
    const auto valid = cf::evaluate_robust_hurdle_envelope(config);
    check(valid.selected_evidence_tier ==
            cf::RobustHurdleEvidenceTier::ExecutableTwoSidedQuote,
        "a complete executable two-sided quote enters only the quote tier");

    auto broken_quote = config;
    broken_quote.observations[0]
        .quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper = false;
    check(cf::evaluate_robust_hurdle_envelope(broken_quote)
                .eligible_cluster_count == 0U,
        "quote interval is ineligible without side-correct preimage evidence");

    auto broken_settlement = base_config();
    broken_settlement.observations[0].arms_length_transaction_is_evidenced =
        false;
    broken_settlement.observations[0]
        .settled_price_preimage_is_single_connected_interval = false;
    const auto broken = cf::evaluate_robust_hurdle_envelope(broken_settlement);
    check(broken.eligible_cluster_count == 0U &&
            broken.observations[0].ineligibility_reasons.size() >= 2U,
        "settled status alone proves neither arm's-length execution nor a connected rate preimage");
}

void test_settlement_and_market_regime_dates() {
    auto future_settlement = base_config();
    future_settlement.observations[0].settlement_date = "2026-01-16";
    const auto future =
        cf::evaluate_robust_hurdle_envelope(future_settlement);
    check(future.eligible_cluster_count == 0U &&
            std::find(future.observations[0].ineligibility_reasons.begin(),
                future.observations[0].ineligibility_reasons.end(),
                cf::RobustHurdleObservationIneligibilityReason::
                    SettlementDateIsAfterAnalysisAsOfDate) !=
                future.observations[0].ineligibility_reasons.end(),
        "settlement after analysis as-of is retained but financially ineligible");

    auto stale_matched = base_config();
    stale_matched.as_of_date = "2026-01-16";
    const auto stale = cf::evaluate_robust_hurdle_envelope(stale_matched);
    check(stale.eligible_cluster_count == 0U &&
            std::find(stale.observations[0].ineligibility_reasons.begin(),
                stale.observations[0].ineligibility_reasons.end(),
                cf::RobustHurdleObservationIneligibilityReason::
                    MatchedObservationDateDoesNotEqualAnalysisAsOfDate) !=
                stale.observations[0].ineligibility_reasons.end(),
        "a prior-date record cannot declare the market-regime axis matched");

    auto bridged = base_config();
    bridged.as_of_date = "2026-01-16";
    auto comparable = settled_comparable("date-bridged", 0.10, 0.20);
    const std::size_t axis = cf::robust_hurdle_axis_index(
        cf::RobustHurdleComparabilityAxis::ObservationDateAndMarketRegime);
    comparable.comparability_treatments[axis] =
        cf::RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;
    cf::RobustHurdleLogGrossReturnAdjustmentConfig adjustment;
    adjustment.adjustment_id = "date-regime";
    adjustment.axis =
        cf::RobustHurdleComparabilityAxis::ObservationDateAndMarketRegime;
    adjustment.lower_log_gross_return_delta = 0.01;
    adjustment.upper_log_gross_return_delta = 0.02;
    adjustment.method_id = "date-regime-method";
    adjustment.source_reference = "Synthetic date-regime bridge";
    adjustment.evidence_record_id = "date-regime-evidence";
    adjustment.source_note = "Jointly feasible bridge";
    comparable.adjustments = {adjustment};
    comparable.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval;
    comparable.jointly_feasible_total_lower_log_gross_return_delta = 0.01;
    comparable.jointly_feasible_total_upper_log_gross_return_delta = 0.02;
    comparable.joint_adjustment_set_method_id = "joint-date-method";
    comparable.joint_adjustment_set_evidence_record_id =
        "joint-date-evidence";
    comparable.expected_loss_recovery_and_timing_are_excluded_from_adjustments =
        true;
    bridged.observations = {comparable};
    check(cf::evaluate_robust_hurdle_envelope(bridged).eligible_cluster_count ==
            1U,
        "a prior-date comparable can enter only through an evidenced bounded date/regime adjustment");
}

void test_validation_and_resource_bounds() {
    auto duplicate = base_config();
    duplicate.observations.push_back(duplicate.observations[0]);
    duplicate.observations[1].record_id = "other-record";
    expect_invalid(
        [&] { cf::validate_robust_hurdle_envelope_config(duplicate); },
        "duplicate economic cluster id is rejected");

    auto invalid_k = base_config();
    invalid_k.observations.push_back(settled_same("second", 0.05, 0.10));
    invalid_k.maximum_contaminated_clusters = 1U;
    expect_invalid(
        [&] { cf::validate_robust_hurdle_envelope_config(invalid_k); },
        "predeclared k must satisfy k<n/2 in the frozen universe");

    auto invalid_rate = base_config();
    invalid_rate.observations[0].annual_effective_hurdle_lower = -1.0;
    expect_invalid(
        [&] { cf::validate_robust_hurdle_envelope_config(invalid_rate); },
        "annual effective source rate must be greater than -1");

    auto invalid_enum = base_config();
    invalid_enum.observations[0].return_basis =
        static_cast<cf::RobustHurdleObservationReturnBasis>(999);
    expect_invalid(
        [&] { cf::validate_robust_hurdle_envelope_config(invalid_enum); },
        "invalid direct-API enum is rejected");

    auto maximum = base_config();
    maximum.observations.clear();
    maximum.maximum_contaminated_clusters = 63U;
    for (std::size_t index = 0U;
         index < cf::kRobustHurdleEnvelopeMaximumObservations; ++index) {
        auto observation = settled_comparable(
            "record-" + std::to_string(index), 0.10, 0.20);
        observation.economic_observation_cluster_id =
            "cluster-" + std::to_string(index);
        maximum.observations.push_back(std::move(observation));
    }
    const auto bounded = cf::evaluate_robust_hurdle_envelope(maximum);
    check(bounded.work.structural_work_units <=
            bounded.work.structural_work_unit_limit &&
            bounded.work.comparability_axis_work_units == 128U * 8U &&
            bounded.evidence_tiers[1].leave_one_cluster_out.size() == 128U,
        "maximum finite record universe and leave-one-out audit stay within the declared work bound");
}

void test_canonical_record_order() {
    auto first = base_config();
    first.observations = {settled_comparable("z-record", 0.10, 0.30),
        settled_comparable("a-record", 0.20, 0.40)};
    auto second = first;
    std::reverse(second.observations.begin(), second.observations.end());
    const auto left = cf::evaluate_robust_hurdle_envelope(first);
    const auto right = cf::evaluate_robust_hurdle_envelope(second);
    check(left.observations[0].record_id == "a-record" &&
            right.observations[0].record_id == "a-record" &&
            left.selected_identified_set->components[0].lower ==
                right.selected_identified_set->components[0].lower &&
            left.selected_identified_set->components[0].upper ==
                right.selected_identified_set->components[0].upper,
        "direct API canonicalizes records by stable record id before evaluation");
}

} // namespace

int main() {
    test_direct_transaction_conditioned_set();
    test_touching_intervals_preserve_singleton();
    test_sk_is_exact_disjoint_union();
    test_evidence_tiers_do_not_pool_or_outvote();
    test_inadmissible_higher_tier_does_not_fall_through();
    test_uninformative_and_empty_eligible_sets();
    test_ineligible_records_and_adjustment_guards();
    test_quote_and_settlement_evidence_guards();
    test_settlement_and_market_regime_dates();
    test_validation_and_resource_bounds();
    test_canonical_record_order();
    if (failures != 0) {
        std::cerr << failures << " robust hurdle-envelope test(s) failed\n";
        return 1;
    }
    std::cout << "robust hurdle-envelope tests passed\n";
    return 0;
}
