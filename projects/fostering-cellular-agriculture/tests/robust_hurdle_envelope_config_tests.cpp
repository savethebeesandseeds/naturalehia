// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_hurdle_envelope_config.hpp>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

int failures = 0;

void check(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename Function>
void expect_invalid_argument(Function&& function, std::string_view message) {
    try {
        std::forward<Function>(function)();
    } catch (const std::invalid_argument&) {
        return;
    } catch (...) {
        // Reported below.
    }
    check(false, message);
}

template <typename Function>
void expect_runtime_error(Function&& function, std::string_view message) {
    try {
        std::forward<Function>(function)();
    } catch (const std::runtime_error&) {
        return;
    } catch (...) {
        // Reported below.
    }
    check(false, message);
}

void replace_once(std::string& text, std::string_view from,
    std::string_view to) {
    const std::size_t position = text.find(from);
    if (position == std::string::npos) {
        throw std::runtime_error("test replacement source is absent");
    }
    text.replace(position, from.size(), to);
}

void erase_line(std::string& text, std::string_view prefix) {
    const std::size_t begin = text.find(prefix);
    if (begin == std::string::npos) {
        throw std::runtime_error("test line prefix is absent");
    }
    const std::size_t end = text.find('\n', begin);
    text.erase(begin,
        end == std::string::npos ? std::string::npos : end - begin + 1U);
}

cf::RobustHurdleMarketObservationConfig make_settled_observation(
    std::string record_id, std::string cluster_id) {
    cf::RobustHurdleMarketObservationConfig observation;
    observation.record_id = std::move(record_id);
    observation.economic_observation_cluster_id = std::move(cluster_id);
    observation.status = cf::RobustHurdleObservationStatus::
        SettledOrderlyArmsLengthTransaction;
    observation.transaction_type =
        cf::RobustHurdleObservationTransactionType::PrimaryIssuance;
    observation.observation_date = "2026-08-01";
    observation.execution_date = "2026-08-01";
    observation.settlement_date = "2026-08-02";
    observation.source_reference = "Executed and settled transaction record";
    observation.evidence_record_id = "TXN-EVIDENCE-001";
    observation.settlement_evidence_record_id = "SETTLEMENT-EVIDENCE-001";
    observation.orderly_arms_length_evidence_record_id =
        "ORDERLY-ARMS-LENGTH-EVIDENCE-001";
    observation.transaction_market_anchor_id = "MARKET-ANCHOR-001";
    observation.transaction_execution_is_evidenced = true;
    observation.buyer_cash_payment_is_evidenced = true;
    observation.transaction_settlement_is_evidenced = true;
    observation.orderly_transaction_is_evidenced = true;
    observation.arms_length_transaction_is_evidenced = true;
    observation.forced_or_distressed_transaction = false;
    observation.related_party_transaction = false;
    observation.settled_claim_quantity_million = 10.0;
    observation.observed_market_claim_id = "target-market-claim";
    observation.normalized_term_result_id = "target-term-result";
    observation.claim_relation =
        cf::RobustHurdleObservationClaimRelation::SameTargetClaim;
    observation.currency_label = "EUR";
    observation.monetary_basis = "2026 nominal EUR million";
    observation.return_basis = cf::RobustHurdleObservationReturnBasis::
        AnnualEffectiveAllInBuyerCashDiscountRateOnPhysicalExpectedFullClaimCashFlows;
    observation.annual_effective_hurdle_lower = 0.08;
    observation.annual_effective_hurdle_upper = 0.12;
    observation.return_normalization_result_id = "RETURN-NORMALIZATION-001";
    observation.observed_gross_buyer_price_million = 5.0;
    observation.buyer_direct_cost_million = 0.1;
    observation.observed_gross_buyer_price_is_evidenced = true;
    observation.buyer_direct_cost_is_evidenced = true;
    observation.buyer_direct_cost_evidence_record_id = "BUYER-COST-001";
    observation.quoted_bid_gross_buyer_price_million = 0.0;
    observation.quoted_ask_gross_buyer_price_million = 0.0;
    observation.quoted_bid_buyer_direct_cost_million = 0.0;
    observation.quoted_ask_buyer_direct_cost_million = 0.0;
    observation.quoted_bid_claim_quantity_million = 0.0;
    observation.quoted_ask_claim_quantity_million = 0.0;
    observation.quoted_bid_timestamp_utc = "none";
    observation.quoted_ask_timestamp_utc = "none";
    observation.quoted_bid_currency_label = "UNSPECIFIED";
    observation.quoted_ask_currency_label = "UNSPECIFIED";
    observation.quoted_bid_and_ask_are_executable = false;
    observation.quoted_bid_and_ask_are_evidenced = false;
    observation.quote_evidence_record_id = "none";
    observation.quote_valid_until_utc = "none";
    observation
        .discounted_expected_cash_is_strictly_decreasing_over_rate_interval =
        false;
    observation.quote_rate_preimage_maps_ask_to_lower_and_bid_to_upper =
        false;
    observation.settled_price_preimage_is_single_connected_interval = true;
    observation.expected_cash_operator = cf::RobustHurdleExpectedCashOperator::
        RobustMinimumOverDeclaredProbabilitySet;
    observation.full_dated_scenario_cash_input_sha256 =
        std::string(64U, 'a');
    observation.probability_input_sha256 = std::string(64U, 'b');
    observation.expected_cash_calculation_run_sha256 = std::string(64U, 'c');
    observation.expected_cash_reconstruction_identity_is_evidenced = true;
    observation.maximum_expected_cash_reconstruction_residual_million = 0.0;
    observation.physical_expected_full_claim_cash_includes_loss_and_timing =
        true;
    observation.normalized_to_full_claim_at_observation_date = true;
    observation.fixed_probability_to_robust_bridge_is_present = false;
    observation.fixed_probability_to_robust_bridge_lower_log_delta = 0.0;
    observation.fixed_probability_to_robust_bridge_upper_log_delta = 0.0;
    observation.fixed_probability_to_robust_bridge_method_id = "none";
    observation.fixed_probability_to_robust_bridge_evidence_record_id =
        "none";
    observation.target_reference_price_relation =
        cf::RobustIssuePriceHurdleReferenceRelation::Independent;
    observation.side_rights_or_non_cash_consideration_present = false;
    observation.side_rights_or_non_cash_consideration_note = "none";
    observation.comparability_treatments.fill(
        cf::RobustHurdleComparabilityTreatment::Matched);
    observation.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::ComponentwiseBoxOuterHull;
    observation.jointly_feasible_total_lower_log_gross_return_delta = 0.0;
    observation.jointly_feasible_total_upper_log_gross_return_delta = 0.0;
    observation.joint_adjustment_set_method_id = "none";
    observation.joint_adjustment_set_evidence_record_id = "none";
    observation.expected_loss_recovery_and_timing_are_excluded_from_adjustments =
        true;
    observation.source_note = "Settled same-claim test observation";
    return observation;
}

cf::RobustHurdleMarketObservationConfig make_adjusted_observation() {
    cf::RobustHurdleMarketObservationConfig observation =
        make_settled_observation("z-record", "z-cluster");
    observation.observed_market_claim_id = "comparable-market-claim";
    observation.normalized_term_result_id = "comparable-term-result";
    observation.claim_relation =
        cf::RobustHurdleObservationClaimRelation::ComparableClaim;
    observation.comparability_treatments[cf::robust_hurdle_axis_index(
        cf::RobustHurdleComparabilityAxis::LiquidityAndTransferability)] =
        cf::RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;
    observation.comparability_treatments[cf::robust_hurdle_axis_index(
        cf::RobustHurdleComparabilityAxis::
            SystematicCovarianceConcentrationAndResidualModelRiskPremium)] =
        cf::RobustHurdleComparabilityTreatment::
            BoundedLogGrossReturnAdjustment;

    cf::RobustHurdleLogGrossReturnAdjustmentConfig z_adjustment;
    z_adjustment.adjustment_id = "z-liquidity";
    z_adjustment.axis =
        cf::RobustHurdleComparabilityAxis::LiquidityAndTransferability;
    z_adjustment.lower_log_gross_return_delta = 0.01;
    z_adjustment.upper_log_gross_return_delta = 0.02;
    z_adjustment.method_id = "LIQUIDITY-METHOD-001";
    z_adjustment.source_reference = "Bounded liquidity adjustment evidence";
    z_adjustment.evidence_record_id = "LIQUIDITY-EVIDENCE-001";
    z_adjustment.source_note = "Liquidity interval";

    cf::RobustHurdleLogGrossReturnAdjustmentConfig a_adjustment;
    a_adjustment.adjustment_id = "a-systematic-risk";
    a_adjustment.axis = cf::RobustHurdleComparabilityAxis::
        SystematicCovarianceConcentrationAndResidualModelRiskPremium;
    a_adjustment.lower_log_gross_return_delta = -0.01;
    a_adjustment.upper_log_gross_return_delta = 0.01;
    a_adjustment.method_id = "SYSTEMATIC-RISK-METHOD-001";
    a_adjustment.source_reference = "Bounded systematic-risk adjustment";
    a_adjustment.evidence_record_id = "SYSTEMATIC-RISK-EVIDENCE-001";
    a_adjustment.source_note = "Systematic risk interval";
    observation.adjustments = {z_adjustment, a_adjustment};
    observation.adjustment_set_basis =
        cf::RobustHurdleAdjustmentSetBasis::JointlyFeasibleTotalInterval;
    observation.jointly_feasible_total_lower_log_gross_return_delta = 0.0;
    observation.jointly_feasible_total_upper_log_gross_return_delta = 0.03;
    observation.joint_adjustment_set_method_id = "JOINT-SET-METHOD-001";
    observation.joint_adjustment_set_evidence_record_id =
        "JOINT-SET-EVIDENCE-001";
    observation.source_note = "Adjusted comparable test observation";
    return observation;
}

cf::RobustHurdleEnvelopeConfig make_config() {
    cf::RobustHurdleEnvelopeConfig config;
    config.analysis_id = "hurdle-envelope-test";
    config.as_of_date = "2026-08-30";
    config.source_note = "Strict parser and normalized-writer fixture";
    config.synthetic_inputs = false;
    config.universe_manifest_id = "UNIVERSE-MANIFEST-001";
    config.inclusion_rule_id = "INCLUSION-RULE-001";
    config.deduplication_manifest_id = "DEDUPLICATION-MANIFEST-001";
    config.source_clustering_rule_id = "SOURCE-CLUSTERING-RULE-001";
    config.observation_universe_is_frozen = true;
    config.inclusion_rule_is_predeclared = true;
    config.deduplication_rule_is_predeclared = true;
    config.all_in_scope_economic_observation_clusters_are_included = true;
    config.same_dealer_same_window_quotes_are_clustered = true;
    config.target_market_claim_id = "target-market-claim";
    config.target_normalized_term_result_id = "target-term-result";
    config.target_currency_label = "EUR";
    config.target_monetary_basis = "2026 nominal EUR million";
    config.target_claim_quantity_million = 10.0;
    config.annual_effective_domain_lower = 0.0;
    config.annual_effective_domain_upper = 10.0;
    config.maximum_contaminated_clusters = 0U;
    config.minimum_consensus_cluster_coverage = 3U;
    config.observations.push_back(make_adjusted_observation());
    config.observations.push_back(
        make_settled_observation("a-record", "a-cluster"));
    return config;
}

std::string normalized(const cf::RobustHurdleEnvelopeConfig& config) {
    std::ostringstream output;
    cf::print_normalized_robust_hurdle_envelope_config(output, config);
    return output.str();
}

cf::RobustHurdleEnvelopeConfig parse(std::string_view text) {
    std::istringstream input{std::string(text)};
    return cf::parse_robust_hurdle_envelope_config(input);
}

void test_round_trip_and_canonical_order() {
    const std::string first = normalized(make_config());
    const cf::RobustHurdleEnvelopeConfig parsed = parse(first);
    check(parsed.observations.size() == 2U &&
            parsed.observations[0].record_id == "a-record" &&
            parsed.observations[1].record_id == "z-record",
        "observations are canonicalized by record id");
    check(parsed.observations[1].adjustments.size() == 2U &&
            parsed.observations[1].adjustments[0].adjustment_id ==
                "a-systematic-risk" &&
            parsed.observations[1].adjustments[1].adjustment_id ==
                "z-liquidity",
        "adjustments are canonicalized by adjustment id");
    check(parsed.observations[0].transaction_execution_is_evidenced &&
            parsed.observations[0].buyer_cash_payment_is_evidenced &&
            parsed.observations[0].transaction_settlement_is_evidenced &&
            parsed.observations[0].orderly_transaction_is_evidenced &&
            parsed.observations[0].arms_length_transaction_is_evidenced &&
            parsed.observations[0].execution_date == "2026-08-01" &&
            parsed.observations[0].settlement_date == "2026-08-02",
        "settled-transaction evidence fields round trip");
    check(parsed.observations[0].quoted_bid_timestamp_utc == "none" &&
            !parsed.observations[0].quoted_bid_and_ask_are_executable,
        "explicit executable-quote fields round trip on a non-quote record");
    check(normalized(parsed) == first,
        "normalized output is deterministic and reloadable");

    std::string placeholder_hash = first;
    replace_once(placeholder_hash,
        "observation.1.probability_input_sha256=" + std::string(64U, 'b'),
        "observation.1.probability_input_sha256=none");
    check(parse(placeholder_hash).observations[0].probability_input_sha256 ==
            "none",
        "placeholder reconstruction hashes remain representable for core ineligibility classification");
}

void test_closed_schema_and_scalar_parsing() {
    const std::string valid = normalized(make_config());

    std::string unknown = valid + "hurdle_envelope.future_field=x\n";
    expect_invalid_argument([&] { (void)parse(unknown); },
        "unknown fixed keys are rejected");

    std::string duplicate = valid +
        "hurdle_envelope.analysis_id=duplicate\n";
    expect_invalid_argument([&] { (void)parse(duplicate); },
        "duplicate keys are rejected");

    std::string missing = valid;
    erase_line(missing, "hurdle_envelope.source_clustering_rule_id=");
    expect_invalid_argument([&] { (void)parse(missing); },
        "missing fixed keys are rejected");

    std::string bad_bool = valid;
    replace_once(bad_bool,
        "hurdle_envelope.observation_universe_is_frozen=true",
        "hurdle_envelope.observation_universe_is_frozen=yes");
    expect_invalid_argument([&] { (void)parse(bad_bool); },
        "booleans accept only true or false");

    std::string bad_enum = valid;
    replace_once(bad_enum,
        "observation.1.status=settled_orderly_arms_length_transaction",
        "observation.1.status=settled-ish");
    expect_invalid_argument([&] { (void)parse(bad_enum); },
        "invalid observation enum text is rejected");

    std::string bad_treatment = valid;
    replace_once(bad_treatment,
        "observation.1.comparability.contractual_cashflow_rights=matched",
        "observation.1.comparability.contractual_cashflow_rights=close_enough");
    expect_invalid_argument([&] { (void)parse(bad_treatment); },
        "invalid comparability treatment text is rejected");

    std::string bad_axis = valid;
    replace_once(bad_axis,
        "observation.2.adjustment.1.axis=systematic_covariance_concentration_and_residual_model_risk_premium",
        "observation.2.adjustment.1.axis=unknown_axis");
    expect_invalid_argument([&] { (void)parse(bad_axis); },
        "invalid adjustment axis text is rejected");

    std::string non_finite = valid;
    replace_once(non_finite,
        "hurdle_envelope.annual_effective_domain_upper=10",
        "hurdle_envelope.annual_effective_domain_upper=nan");
    expect_invalid_argument([&] { (void)parse(non_finite); },
        "non-finite decimal values are rejected");

    std::string impossible_date = valid;
    replace_once(impossible_date,
        "hurdle_envelope.as_of_date=2026-08-30",
        "hurdle_envelope.as_of_date=2026-02-30");
    expect_invalid_argument([&] { (void)parse(impossible_date); },
        "impossible calendar dates are rejected through core validation");
}

void test_dynamic_bounds_and_exact_counts() {
    const std::string valid = normalized(make_config());

    std::string too_many_observations = valid;
    replace_once(too_many_observations, "observation.count=2",
        "observation.count=129");
    expect_invalid_argument([&] { (void)parse(too_many_observations); },
        "observation count is bounded at 128");

    std::string too_many_adjustments = valid;
    replace_once(too_many_adjustments,
        "observation.2.adjustment.count=2",
        "observation.2.adjustment.count=9");
    expect_invalid_argument([&] { (void)parse(too_many_adjustments); },
        "per-observation adjustment count is bounded at eight");

    std::string extra_observation = valid + "observation.3.id=extra\n";
    expect_invalid_argument([&] { (void)parse(extra_observation); },
        "records beyond the declared observation count are rejected");

    std::string extra_adjustment = valid +
        "observation.1.adjustment.1.id=extra\n";
    expect_invalid_argument([&] { (void)parse(extra_adjustment); },
        "adjustments beyond the declared count are rejected");

    std::string out_of_bound = valid + "observation.129.id=extra\n";
    expect_invalid_argument([&] { (void)parse(out_of_bound); },
        "out-of-bound observation indices are rejected as unknown keys");

    std::string unknown_adjustment_field = valid;
    replace_once(unknown_adjustment_field,
        "observation.2.adjustment.1.source_note=Systematic risk interval",
        "observation.2.adjustment.1.future_note=Systematic risk interval");
    expect_invalid_argument([&] { (void)parse(unknown_adjustment_field); },
        "unknown dynamic adjustment fields are rejected");
}

void test_text_and_stream_guardrails() {
    const std::string valid = normalized(make_config());

    std::string unsafe_id = valid;
    replace_once(unsafe_id,
        "hurdle_envelope.analysis_id=hurdle-envelope-test",
        "hurdle_envelope.analysis_id=../../unsafe");
    expect_invalid_argument([&] { (void)parse(unsafe_id); },
        "unsafe identifiers are rejected");

    std::string padded = valid;
    replace_once(padded,
        "hurdle_envelope.source_note=Strict parser and normalized-writer fixture",
        "hurdle_envelope.source_note= Strict parser and normalized-writer fixture");
    expect_invalid_argument([&] { (void)parse(padded); },
        "surrounding text whitespace is rejected");

    std::string embedded_bom = valid;
    embedded_bom += std::string("# embedded ") +
        std::string("\xEF\xBB\xBF", 3U) + " marker\n";
    expect_invalid_argument([&] { (void)parse(embedded_bom); },
        "embedded BOMs are rejected");

    std::string long_line(4'097U, 'x');
    expect_invalid_argument([&] { (void)parse(long_line); },
        "individual lines are bounded at 4096 bytes");

    std::istringstream failed_input(valid);
    failed_input.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            (void)cf::parse_robust_hurdle_envelope_config(failed_input);
        },
        "non-EOF input stream failures are reported");

    cf::RobustHurdleEnvelopeConfig injected = make_config();
    injected.source_note = "line one\nhurdle_envelope.analysis_id=injected";
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_hurdle_envelope_config(
                output, injected);
        },
        "normalized output rejects control-character schema injection");

    injected = make_config();
    injected.annual_effective_domain_upper =
        std::numeric_limits<double>::quiet_NaN();
    expect_invalid_argument(
        [&] {
            std::ostringstream output;
            cf::print_normalized_robust_hurdle_envelope_config(
                output, injected);
        },
        "normalized output rejects non-finite in-memory values");

    std::ostringstream failed_output;
    failed_output.setstate(std::ios::badbit);
    expect_runtime_error(
        [&] {
            cf::print_normalized_robust_hurdle_envelope_config(
                failed_output, make_config());
        },
        "normalized writer reports output stream failures");
}

void test_file_size_and_load_guardrails() {
    const std::filesystem::path oversized =
        std::filesystem::temp_directory_path() /
        "robust-hurdle-envelope-config-oversized.tmp";
    {
        std::ofstream output(oversized, std::ios::binary | std::ios::trunc);
        check(static_cast<bool>(output),
            "oversized-loader fixture opens for writing");
    }
    std::filesystem::resize_file(oversized,
        static_cast<std::uintmax_t>(16U * 1024U * 1024U) + 1U);
    expect_invalid_argument(
        [&] { (void)cf::load_robust_hurdle_envelope_config(oversized); },
        "loader rejects files above 16 MiB before parsing");
    std::error_code remove_error;
    (void)std::filesystem::remove(oversized, remove_error);

    const std::filesystem::path missing =
        std::filesystem::temp_directory_path() /
        "robust-hurdle-envelope-config-does-not-exist.tmp";
    expect_runtime_error(
        [&] { (void)cf::load_robust_hurdle_envelope_config(missing); },
        "loader distinguishes missing files from invalid configurations");
}

void test_canonical_three_comparable_fixture() {
    const std::filesystem::path path =
        "scenarios/market-priority-cap-v0.1-synthetic/hurdle-evidence.cfg";
    const cf::RobustHurdleEnvelopeConfig config =
        cf::load_robust_hurdle_envelope_config(path);
    const cf::RobustHurdleEnvelopeSummary summary =
        cf::evaluate_robust_hurdle_envelope(config);
    check(config.synthetic_inputs && config.observations.size() == 3U &&
            config.maximum_contaminated_clusters == 1U,
        "canonical fixture retains three synthetic clusters and k=1");
    check(summary.selected_evidence_tier ==
            cf::RobustHurdleEvidenceTier::SettledComparable &&
            summary.required_cluster_coverage == 2U &&
            summary.selected_identified_set.has_value(),
        "canonical fixture selects the settled-comparable tier with q=2");
    const auto& components = summary.selected_identified_set->components;
    check(components.size() == 2U && components[0].lower == 0.09 &&
            components[0].upper == 0.10 && components[1].lower == 0.11 &&
            components[1].upper == 0.12,
        "canonical fixture reproduces S1=[0.09,0.10] union [0.11,0.12]");
    check(summary.synthetic_inputs && summary.mechanical_candidate_set_only &&
            !summary.empirical_hurdle_evidence_release_authorized,
        "canonical fixture cannot authorize empirical evidence release");
}

} // namespace

int main() {
    test_round_trip_and_canonical_order();
    test_closed_schema_and_scalar_parsing();
    test_dynamic_bounds_and_exact_counts();
    test_text_and_stream_guardrails();
    test_file_size_and_load_guardrails();
    test_canonical_three_comparable_fixture();

    if (failures != 0) {
        std::cerr << failures
                  << " robust hurdle-envelope config test(s) failed\n";
        return 1;
    }
    std::cout << "robust hurdle-envelope config tests passed\n";
    return 0;
}
