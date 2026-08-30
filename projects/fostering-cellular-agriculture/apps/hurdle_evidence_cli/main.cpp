// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/robust_hurdle_envelope.hpp>
#include <naturalehia/cellular_finance/robust_hurdle_envelope_config.hpp>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program
              << " <hurdle-envelope.cfg> [--print-normalized]\n"
              << "calibrated_execution_authorized=false\n";
}

[[nodiscard]] std::string_view bool_text(bool value) noexcept {
    return value ? "true" : "false";
}

[[nodiscard]] double display_value(double value) noexcept {
    return std::abs(value) < 0.5e-6 ? 0.0 : value;
}

void print_index_list(const std::vector<std::size_t>& indices) {
    if (indices.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t position = 0U; position < indices.size(); ++position) {
        if (position != 0U) {
            std::cout << ',';
        }
        std::cout << indices[position];
    }
}

void print_identifier_list(const std::vector<std::string>& identifiers) {
    if (identifiers.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t position = 0U; position < identifiers.size();
         ++position) {
        if (position != 0U) {
            std::cout << ',';
        }
        std::cout << identifiers[position];
    }
}

void print_cluster_list(const cf::RobustHurdleEnvelopeSummary& summary,
    const std::vector<std::size_t>& indices) {
    if (indices.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t position = 0U; position < indices.size(); ++position) {
        const std::size_t index = indices[position];
        if (index >= summary.observations.size()) {
            throw std::logic_error(
                "hurdle-envelope tier contains an invalid observation index");
        }
        if (position != 0U) {
            std::cout << ',';
        }
        std::cout << summary.observations[index]
                         .economic_observation_cluster_id;
    }
}

void print_rate_interval(double lower, double upper) {
    std::cout << '[' << display_value(lower * 100.0) << "%, "
              << display_value(upper * 100.0) << "%]";
}

void print_optional_rate_interval(const std::optional<double>& lower,
    const std::optional<double>& upper) {
    if (!lower.has_value() || !upper.has_value()) {
        std::cout << "not available";
        return;
    }
    print_rate_interval(*lower, *upper);
}

void print_closed_interval(const cf::RobustHurdleClosedInterval& interval,
    std::string_view indent, std::size_t component_index) {
    std::cout << indent << "component " << component_index << " | ";
    print_rate_interval(interval.lower, interval.upper);
    std::cout << " | endpoint coverage="
              << interval.lower_endpoint_coverage << '/'
              << interval.upper_endpoint_coverage
              << " | lower boundary witnesses=";
    print_identifier_list(interval.lower_boundary_witness_cluster_ids);
    std::cout << " | upper boundary witnesses=";
    print_identifier_list(interval.upper_boundary_witness_cluster_ids);
    std::cout << '\n';
}

void print_components(
    const std::vector<cf::RobustHurdleClosedInterval>& components,
    std::string_view indent) {
    if (components.empty()) {
        std::cout << indent << "components: empty set\n";
        return;
    }
    for (std::size_t index = 0U; index < components.size(); ++index) {
        print_closed_interval(components[index], indent, index + 1U);
    }
}

void print_identified_set(const cf::RobustHurdleIdentifiedSet& identified,
    std::size_t set_index, std::string_view indent) {
    std::cout << indent << "S_" << set_index
              << " | allowed discordant clusters="
              << identified.maximum_contaminated_clusters
              << " | required coverage="
              << identified.required_cluster_coverage
              << " | maximum attained coverage="
              << identified.maximum_attained_coverage
              << " | disjoint components=" << identified.components.size()
              << '\n';
    print_components(identified.components, "      ");
}

void print_input_and_selection(
    const cf::RobustHurdleEnvelopeConfig& config,
    const cf::RobustHurdleEnvelopeSummary& summary) {
    std::cout
        << "Input scope and primary selection\n"
        << "  model version: " << config.model_version << '\n'
        << "  analysis id: " << config.analysis_id << '\n'
        << "  analysis as-of date: " << config.as_of_date << '\n'
        << "  classification: "
        << (config.synthetic_inputs
                ? "synthetic candidate"
                : "transaction-conditioned/model-conditioned rate set")
        << '\n'
        << "  synthetic_inputs=" << bool_text(config.synthetic_inputs)
        << '\n'
        << "  target market claim id: " << config.target_market_claim_id
        << '\n'
        << "  target normalized term/result id: "
        << config.target_normalized_term_result_id << '\n'
        << "  target claim quantity: "
        << config.target_claim_quantity_million << ' '
        << config.target_currency_label << " million\n"
        << "  target monetary basis: " << config.target_monetary_basis
        << '\n'
        << "  declared annual-effective domain: ";
    print_rate_interval(config.annual_effective_domain_lower,
        config.annual_effective_domain_upper);
    std::cout
        << '\n'
        << "  predeclared maximum discordant clusters k: "
        << config.maximum_contaminated_clusters << '\n'
        << "  consensus-label minimum coverage: "
        << config.minimum_consensus_cluster_coverage << '\n'
        << "  overall status: " << cf::to_string(summary.status) << '\n'
        << "  identification basis: "
        << cf::to_string(summary.identification_basis) << '\n'
        << "  selected evidence tier: ";
    if (summary.selected_evidence_tier.has_value()) {
        std::cout << cf::to_string(*summary.selected_evidence_tier) << '\n';
    } else {
        std::cout << "none\n";
    }
    std::cout << "  selected transaction-conditioned/model-conditioned "
                 "rate set: ";
    if (!summary.selected_identified_set.has_value()) {
        std::cout << "not identified\n\n";
        return;
    }
    const auto& selected = *summary.selected_identified_set;
    std::cout << "S_" << selected.maximum_contaminated_clusters
              << " with " << selected.components.size()
              << " disjoint component(s)\n";
    print_identified_set(selected,
        selected.maximum_contaminated_clusters, "    ");
    std::cout << '\n';
}

void print_universe_controls(const cf::RobustHurdleEnvelopeConfig& config) {
    std::cout
        << "Observation-universe controls\n"
        << "  universe manifest id: " << config.universe_manifest_id << '\n'
        << "  inclusion rule id: " << config.inclusion_rule_id << '\n'
        << "  deduplication manifest id: "
        << config.deduplication_manifest_id << '\n'
        << "  source-clustering rule id: "
        << config.source_clustering_rule_id << '\n'
        << "  observation universe is frozen: "
        << bool_text(config.observation_universe_is_frozen) << '\n'
        << "  inclusion rule is predeclared: "
        << bool_text(config.inclusion_rule_is_predeclared) << '\n'
        << "  deduplication rule is predeclared: "
        << bool_text(config.deduplication_rule_is_predeclared) << '\n'
        << "  all in-scope economic clusters are included: "
        << bool_text(
               config.all_in_scope_economic_observation_clusters_are_included)
        << '\n'
        << "  same-dealer/same-window quotes are clustered: "
        << bool_text(config.same_dealer_same_window_quotes_are_clustered)
        << "\n\n";
}

void print_observations(const cf::RobustHurdleEnvelopeSummary& summary) {
    std::cout << "Individual observation classification and mapping\n";
    if (summary.observations.empty()) {
        std::cout << "  none\n\n";
        return;
    }
    for (std::size_t index = 0U; index < summary.observations.size();
         ++index) {
        const auto& observation = summary.observations[index];
        std::cout
            << "  Observation " << index + 1U << " | summary index="
            << index << " | record=" << observation.record_id
            << " | cluster=" << observation.economic_observation_cluster_id
            << '\n'
            << "    eligibility: "
            << cf::to_string(observation.eligibility) << '\n'
            << "    evidence tier: ";
        if (observation.evidence_tier.has_value()) {
            std::cout << cf::to_string(*observation.evidence_tier);
        } else {
            std::cout << "none";
        }
        std::cout << "\n    ineligibility reasons: ";
        if (observation.ineligibility_reasons.empty()) {
            std::cout << "none\n";
        } else {
            for (std::size_t reason_index = 0U;
                 reason_index < observation.ineligibility_reasons.size();
                 ++reason_index) {
                if (reason_index != 0U) {
                    std::cout << ',';
                }
                std::cout << cf::to_string(
                    observation.ineligibility_reasons[reason_index]);
            }
            std::cout << '\n';
        }
        std::cout
            << "    source transaction-conditioned/model-conditioned rate "
               "interval: ";
        print_rate_interval(observation.source_interval_lower,
            observation.source_interval_upper);
        std::cout
            << "\n    componentwise adjustment sum, log gross-return "
               "units: ["
            << display_value(
                   observation.summed_lower_log_gross_return_adjustment)
            << ", "
            << display_value(
                   observation.summed_upper_log_gross_return_adjustment)
            << "]\n"
            << "    applied jointly feasible adjustment, log gross-return "
               "units: ["
            << display_value(
                   observation.applied_lower_log_gross_return_adjustment)
            << ", "
            << display_value(
                   observation.applied_upper_log_gross_return_adjustment)
            << "]\n"
            << "    mapped interval before domain clipping: ";
        print_optional_rate_interval(observation.mapped_interval_lower,
            observation.mapped_interval_upper);
        std::cout << "\n    mapped interval status: "
                  << cf::to_string(observation.mapped_interval_status)
                  << "\n    domain-clipped H_" << index + 1U << ": ";
        if (observation.clipped_interval.has_value()) {
            print_rate_interval(observation.clipped_interval->lower,
                observation.clipped_interval->upper);
            std::cout << '\n';
        } else {
            std::cout << "empty set\n";
        }
        std::cout << std::scientific
                  << "    maximum normalization identity error: "
                  << observation.maximum_normalization_identity_error
                  << std::fixed << "\n\n";
    }
}

void print_tier(const cf::RobustHurdleEnvelopeSummary& summary,
    const cf::RobustHurdleEvidenceTierResult& tier,
    bool selected) {
    std::cout << "  Tier: " << cf::to_string(tier.tier)
              << (selected ? " [selected]" : " [challenger]") << '\n'
              << "    status: " << cf::to_string(tier.status) << '\n'
              << "    eligible clusters n: " << tier.eligible_cluster_count
              << '\n'
              << "    selected required coverage q: "
              << tier.required_cluster_coverage << '\n'
              << "    declared label quorum: " << tier.declared_quorum
              << '\n'
              << "    comparable-consensus label threshold met: "
              << bool_text(tier.comparable_consensus_label_threshold_met)
              << "\n    eligible observation indices: ";
    print_index_list(tier.eligible_observation_indices);
    std::cout << "\n    eligible cluster ids: ";
    print_cluster_list(summary, tier.eligible_observation_indices);
    std::cout << "\n    informative observation indices: ";
    print_index_list(tier.informative_observation_indices);
    std::cout << "\n    binding observation indices: ";
    print_index_list(tier.binding_observation_indices);
    std::cout << '\n';

    if (tier.eligible_interval_hull_diagnostic.has_value()) {
        std::cout
            << "    eligible-interval hull (outer diagnostic only; gaps "
               "remain excluded): ";
        print_rate_interval(tier.eligible_interval_hull_diagnostic->lower,
            tier.eligible_interval_hull_diagnostic->upper);
        std::cout << '\n';
    } else {
        std::cout << "    eligible-interval hull diagnostic: not available\n";
    }

    std::cout << "    S_0 through S_k\n";
    if (tier.identified_sets_s0_through_sk.empty()) {
        std::cout << "      not computed for this tier status\n";
    } else {
        for (std::size_t set_index = 0U;
             set_index < tier.identified_sets_s0_through_sk.size();
             ++set_index) {
            print_identified_set(tier.identified_sets_s0_through_sk[set_index],
                set_index, "      ");
        }
    }

    std::cout << "    Leave-one-cluster-out diagnostics\n";
    if (tier.leave_one_cluster_out.empty()) {
        std::cout << "      not computed for this tier status\n\n";
        return;
    }
    for (const auto& sensitivity : tier.leave_one_cluster_out) {
        std::cout
            << "      omit summary index="
            << sensitivity.omitted_eligible_observation_index
            << " | cluster="
            << sensitivity.omitted_economic_observation_cluster_id
            << " | contamination budget remains admissible="
            << bool_text(
                   sensitivity.declared_contamination_budget_remains_admissible)
            << " | remaining n="
            << sensitivity.remaining_eligible_cluster_count
            << " | required coverage="
            << sensitivity.required_cluster_coverage << '\n';
        if (sensitivity
                .declared_contamination_budget_remains_admissible) {
            print_components(sensitivity.components, "        ");
        } else {
            std::cout << "        components: not computed\n";
        }
    }
    std::cout << '\n';
}

void print_tiers(const cf::RobustHurdleEnvelopeSummary& summary) {
    std::cout
        << "Tier-separated identification and stability diagnostics\n"
        << "  The highest eligible tier controls even when its selected set "
           "is empty; lower tiers are challenger diagnostics only.\n";
    for (const auto& tier : summary.evidence_tiers) {
        const bool selected = summary.selected_evidence_tier.has_value() &&
            *summary.selected_evidence_tier == tier.tier;
        print_tier(summary, tier, selected);
    }
}

void print_work_and_audits(
    const cf::RobustHurdleEnvelopeSummary& summary) {
    std::cout
        << "Finite work, counts, and set audits\n"
        << "  input observations: " << summary.input_observation_count
        << '\n'
        << "  financially eligible clusters: "
        << summary.eligible_cluster_count << '\n'
        << "  financially ineligible clusters: "
        << summary.financially_ineligible_cluster_count << '\n'
        << "  eligible clusters mapped to an empty domain intersection: "
        << summary.eligible_empty_interval_cluster_count << '\n'
        << "  selected required coverage: "
        << summary.required_cluster_coverage << '\n'
        << "  comparability-axis work units: "
        << summary.work.comparability_axis_work_units << '\n'
        << "  adjustment-normalization work units: "
        << summary.work.adjustment_normalization_work_units << '\n'
        << "  identified-set event work units: "
        << summary.work.identified_set_event_work_units << '\n'
        << "  leave-one-cluster-out event work units: "
        << summary.work.leave_one_cluster_out_event_work_units << '\n'
        << "  structural work: " << summary.work.structural_work_units
        << " / " << summary.work.structural_work_unit_limit << '\n'
        << std::scientific
        << "  maximum normalization identity error: "
        << summary.maximum_normalization_identity_error << std::fixed
        << '\n'
        << "  every identified set is closed, disjoint, and canonical: "
        << bool_text(
               summary.all_identified_sets_are_closed_disjoint_and_canonical)
        << '\n'
        << "  every selected component meets its coverage threshold: "
        << bool_text(
               summary.every_selected_component_meets_its_coverage_threshold)
        << '\n'
        << "  every selected gap is below its coverage threshold: "
        << bool_text(summary.every_selected_gap_is_below_its_coverage_threshold)
        << "\n\n";
}

void print_claim_boundaries(const cf::RobustHurdleEnvelopeConfig& config,
    const cf::RobustHurdleEnvelopeSummary& summary) {
    std::cout
        << "Claims boundary\n"
        << "  Result wording: "
        << (config.synthetic_inputs
                ? "synthetic candidate transaction-conditioned/"
                  "model-conditioned rate set"
                : "transaction-conditioned/model-conditioned rate set")
        << ".\n"
        << "  Prices are evidence inputs; the rate set additionally depends "
           "on declared expected-cash reconstructions and normalization "
           "intervals.\n"
        << "  Empty eligible mappings and gaps are retained as falsifying "
           "boundaries, not averaged away.\n"
        << "  No fair-value, market-price, benchmark, investor-demand, "
           "placement, accounting, legal, tax, or recommendation claim is "
           "made.\n"
        << "arithmetic_averaging_or_observation_weighting_is_used="
        << bool_text(
               summary.arithmetic_averaging_or_observation_weighting_is_used)
        << '\n'
        << "statistical_confidence_or_coverage_is_estimated="
        << bool_text(summary.statistical_confidence_or_coverage_is_estimated)
        << '\n'
        << "observation_independence_is_established="
        << bool_text(summary.observation_independence_is_established) << '\n'
        << "market_hurdle_is_point_identified="
        << bool_text(summary.market_hurdle_is_point_identified) << '\n'
        << "selected_rate_set_is_singleton="
        << bool_text(summary.selected_rate_set_is_singleton) << '\n'
        << "buyer_hurdle_or_investor_beliefs_are_observed="
        << bool_text(summary.buyer_hurdle_or_investor_beliefs_are_observed)
        << '\n'
        << "annual_expected_holding_period_return_is_inferred="
        << bool_text(summary.annual_expected_holding_period_return_is_inferred)
        << '\n'
        << "synthetic_inputs=" << bool_text(summary.synthetic_inputs) << '\n'
        << "empirical_hurdle_evidence_release_authorized="
        << bool_text(summary.empirical_hurdle_evidence_release_authorized)
        << '\n'
        << "source_identifiers_or_assertion_booleans_authenticate_documents="
        << bool_text(
               summary.source_identifiers_or_assertion_booleans_authenticate_documents)
        << '\n'
        << "mechanical_candidate_set_only="
        << bool_text(summary.mechanical_candidate_set_only) << '\n'
        << "target_reference_price_is_used_to_infer_a_hurdle="
        << bool_text(summary.target_reference_price_is_used_to_infer_a_hurdle)
        << '\n'
        << "fair_value_market_price_demand_or_placement_is_established="
        << bool_text(
               summary.fair_value_market_price_demand_or_placement_is_established)
        << '\n'
        << "  Core model limitation: " << summary.model_limitation << '\n'
        << "  Source note: " << config.source_note << '\n'
        << "calibrated_execution_authorized=false\n";
}

void print_report(const cf::RobustHurdleEnvelopeConfig& config,
    const cf::RobustHurdleEnvelopeSummary& summary) {
    std::cout << std::fixed << std::setprecision(6)
              << (config.synthetic_inputs ? "SYNTHETIC CANDIDATE " : "")
              << "ROBUST HURDLE EVIDENCE ENVELOPE\n"
              << "Finite transaction-conditioned/model-conditioned rate "
                 "set identification; no averaging across evidence tiers "
                 "or disjoint components.\n\n";
    print_input_and_selection(config, summary);
    print_universe_controls(config);
    print_observations(summary);
    print_tiers(summary);
    print_work_and_audits(summary);
    print_claim_boundaries(config, summary);
}

void print_normalized_input(
    const cf::RobustHurdleEnvelopeConfig& config) {
    std::cout << "\nNormalized robust-hurdle-envelope configuration\n";
    cf::print_normalized_robust_hurdle_envelope_config(std::cout, config);
}

} // namespace

int main(int argc, char** argv) {
    if ((argc != 2 && argc != 3) ||
        (argc == 3 &&
            std::string_view(argv[2]) != "--print-normalized")) {
        print_usage(argc > 0 ? std::string_view(argv[0])
                             : std::string_view("hurdle-evidence"));
        return 1;
    }
    const bool print_normalized = argc == 3;

    cf::RobustHurdleEnvelopeConfig config;
    try {
        config = cf::load_robust_hurdle_envelope_config(
            std::filesystem::path(argv[1]));
    } catch (const std::exception& error) {
        std::cerr << "hurdle-evidence input/configuration failed: "
                  << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 2;
    }

    try {
        const cf::RobustHurdleEnvelopeSummary summary =
            cf::evaluate_robust_hurdle_envelope(config);
        print_report(config, summary);
        if (print_normalized) {
            print_normalized_input(config);
        }
        std::cout.flush();
        if (!std::cout) {
            throw std::runtime_error(
                "failed while writing hurdle-evidence report");
        }
    } catch (const std::exception& error) {
        std::cerr << "hurdle-evidence analysis failed: " << error.what()
                  << "\ncalibrated_execution_authorized=false\n";
        return 3;
    }
    return 0;
}
