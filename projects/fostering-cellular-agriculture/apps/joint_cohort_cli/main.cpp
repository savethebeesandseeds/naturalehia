// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/joint_cohort.hpp>
#include <naturalehia/cellular_finance/joint_cohort_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr << "usage: " << program
              << " <cohort.cfg> [--print-normalized]\n"
              << "candidate_only=true; calibrated_execution_authorized=false\n";
}

void print_optional(const std::optional<double>& value) {
    if (value.has_value()) {
        std::cout << *value;
    } else {
        std::cout << "NA";
    }
}

void print_witness(
    std::string_view label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::JointCohortResult& result) {
    if (!result.generated_probability_envelope.has_value() ||
        endpoint.scenario_weights.size() !=
            result.generated_probability_envelope->scenario_probabilities.size()) {
        throw std::logic_error(
            "joint-cohort endpoint witness has the wrong scenario count");
    }
    std::cout << "    " << label << " witness: ";
    for (std::size_t index = 0U;
         index < endpoint.scenario_weights.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout
            << result.generated_probability_envelope
                   ->scenario_probabilities[index].scenario_id
            << '=' << endpoint.scenario_weights[index];
    }
    std::cout << '\n';
}

void print_projection(
    std::string_view label,
    const cf::AmbiguityMetricRange& range,
    const cf::JointCohortResult& result) {
    std::cout << "  " << label << " | "
              << range.minimum.value * 100.0 << "% | "
              << range.central * 100.0 << "% | "
              << range.maximum.value * 100.0 << "%\n";
    print_witness("minimum", range.minimum, result);
    print_witness("maximum", range.maximum, result);
}

void print_range_witnesses(
    std::string_view label,
    const cf::AmbiguityMetricRange& range,
    const cf::JointCohortResult& result) {
    print_witness(std::string(label) + " minimum", range.minimum, result);
    print_witness(std::string(label) + " maximum", range.maximum, result);
}

void print_tail_mass_vector(std::string_view label,
    const std::vector<double>& masses,
    const cf::JointCohortResult& result) {
    if (!result.generated_probability_envelope.has_value() ||
        masses.size() != result.generated_probability_envelope
                             ->scenario_probabilities.size()) {
        throw std::logic_error(
            "joint-cohort tail attribution has the wrong scenario count");
    }
    std::cout << "  " << label << " | ";
    for (std::size_t index = 0U; index < masses.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout
            << result.generated_probability_envelope
                   ->scenario_probabilities[index].scenario_id
            << '=' << masses[index];
    }
    std::cout << '\n';
}

void print_tail_attribution(std::string_view label,
    const cf::PoolLossTailAttribution& attribution,
    const cf::JointCohortResult& result,
    std::string_view currency) {
    std::cout << label << "\n"
              << "  project | at minimum-pool-ES measure | at declared "
                 "central measure | at maximum-pool-ES measure | unit\n";
    for (const cf::ProjectPoolLossTailContribution& project :
         attribution.projects) {
        std::cout << "  " << project.project_id << " | "
                  << project.at_minimum_pool_es_measure_million << " | "
                  << project.at_central_measure_million << " | "
                  << project.at_maximum_pool_es_measure_million << " | "
                  << currency << " million\n";
    }
    std::cout << "  common fractional tail mass by scenario (sums to "
              << attribution.tail_probability << ")\n";
    print_tail_mass_vector("minimum-pool-ES measure",
        attribution.minimum_pool_es_tail_mass_weights, result);
    print_tail_mass_vector("declared central measure",
        attribution.central_tail_mass_weights, result);
    print_tail_mass_vector("maximum-pool-ES measure",
        attribution.maximum_pool_es_tail_mass_weights, result);
    std::cout
        << "  These are additive attributions under three shared measures, "
           "not independently optimized project ranges.\n\n";
}

void print_interpretation_boundary() {
    std::cout
        << "\nInterpretation boundary\n"
        << "  The portfolio center remains an analyst-declared physical-P "
           "reference/hypothesis. In incomplete cohorts it is not replaced by "
           "a matured-only estimate.\n"
        << "  Exclusion rules are asserted frozen and outcome-blind; this tool "
           "does not validate the truth of those assertions.\n"
        << "  Every financial endpoint has its own feasible probability "
           "witness. Endpoints must not be combined across metrics.\n"
        << "  Exact tied-objective capacity and fractional pool-tail ties are "
           "allocated pro rata. Common-tail project contributions are one "
           "auditable additive attribution, not separate project maxima.\n"
        << "  calibrated_execution_authorized=false\n"
        << "  This synthetic candidate does not authorize calibration, pricing, "
           "a rating, an offering, or an investment recommendation.\n";
}

void print_report(
    const cf::JointCohortPackage& package,
    const cf::JointCohortResult& result) {
    const cf::JointCohortAnalysisConfig& config = package.config.analysis;
    double total_commitment = 0.0;
    for (const cf::PortfolioProject& project : package.portfolio.projects) {
        total_commitment += project.commitment_million;
    }
    std::cout << std::fixed << std::setprecision(6);
    std::cout
        << "JOINT-COHORT PROBABILITY ENVELOPE v0.1 - SYNTHETIC CANDIDATE\n"
        << "calibrated_execution_authorized=false\n"
        << "candidate_only=" << (result.candidate_only ? "true" : "false")
        << "\n"
        << "generated_envelope_synthetic="
        << (result.generated_envelope_synthetic ? "true" : "false")
        << "\n\n"
        << "Bound package\n"
        << "  cohort_id: " << config.id << '\n'
        << "  as_of_date: " << config.as_of_date << '\n'
        << "  population frame count: " << config.population_frame_count
        << '\n'
        << "  portfolio SHA-256: " << package.config.portfolio_file.sha256
        << '\n'
        << "  raw ledger SHA-256: " << package.config.ledger_file.sha256
        << '\n'
        << "  source note: " << config.source_note << '\n'
        << "  population: " << config.population_definition << '\n'
        << "  sampling unit: " << config.sampling_unit_definition << '\n'
        << "  outcome mapping: " << config.outcome_mapping_definition << '\n'
        << "  horizon: " << config.horizon_definition << '\n'
        << "  scenario taxonomy frozen: "
        << config.scenario_taxonomy_frozen_date << "\n\n"
        << "Portfolio declaration\n"
        << "  currency: " << package.portfolio.currency_label << '\n'
        << "  monetary basis: " << package.portfolio.monetary_basis << '\n'
        << "  portfolio horizon: " << package.portfolio.horizon_months
        << " months\n"
        << "  annual declared hurdle: "
        << package.portfolio.annual_physical_hurdle_rate * 100.0 << "%\n"
        << "  total commitment: " << total_commitment << ' '
        << package.portfolio.currency_label << " million\n";
    for (const cf::PortfolioProject& project : package.portfolio.projects) {
        std::cout << "  project " << project.id << " commitment: "
                  << project.commitment_million << ' '
                  << package.portfolio.currency_label << " million | stage="
                  << cf::to_string(project.stage) << '\n';
    }
    std::cout
        << '\n'
        << "Statistical declaration\n"
        << "  probability measure: " << config.probability_measure << '\n'
        << "  sampling assumption token: " << config.sampling_assumption
        << '\n'
        << "  independently and identically distributed complete joint-unit "
           "draws are required\n"
        << "  interval method: " << config.interval_method << '\n'
        << "  confidence level: " << result.confidence_level << '\n'
        << "  this confidence level governs the probability-parameter outer "
           "set; ES95/ES99 instead average the worst 5%/1% cash-outcome tails\n"
        << "  output: conservative nonasymptotic simultaneous outer "
           "confidence set; not exact intervals and not calibration\n"
        << "  unknown included rows remain in N and are compatible with every "
           "scenario; no independent-censoring claim is made\n\n"
        << "Raw-row counts\n"
        << "  raw observations: " << result.raw_observation_count << '\n'
        << "  included denominator N: "
        << result.included_observation_count << '\n'
        << "  matured: " << result.matured_count << '\n'
        << "  not yet matured: " << result.not_yet_matured_count << '\n'
        << "  unresolved due: " << result.unresolved_count << '\n'
        << "  unknown U: " << result.unknown_count << '\n'
        << "  excluded outside N: " << result.excluded_count << '\n'
        << "  included cluster IDs unique: "
        << (result.included_cluster_ids_unique ? "true" : "false")
        << "\n\n";

    std::cout << "Exclusion disclosure - assertions mechanically checked, not independently validated\n";
    for (const cf::JointCohortExclusionRuleDisclosure& rule :
         result.exclusion_rule_disclosures) {
        std::cout << "  rule " << rule.rule_id
                  << " | frozen=" << rule.frozen_date
                  << " | outcome_blind_asserted="
                  << (rule.outcome_blind_asserted ? "true" : "false")
                  << " | excluded=" << rule.excluded_observation_count
                  << " | " << rule.statement << '\n';
    }
    for (const cf::JointCohortExcludedObservationDisclosure& observation :
         result.excluded_observations) {
        std::cout << "  excluded observation " << observation.observation_id
                  << " | cluster=" << observation.cluster_id
                  << " | rule=" << observation.exclusion_rule_id << '\n';
    }
    std::cout << '\n';

    if (!result.primary_outer_set_available) {
        std::cout << "Primary outer set: BLOCKED\n"
                  << "  " << result.block_reason << "\n\n";
        std::cout << "Descriptive compatible counts - no confidence bounds\n"
                  << "  scenario | matured d_i | compatible min | compatible max | portfolio reference\n";
        for (const cf::JointCohortScenarioEnvelope& scenario :
             result.scenario_envelopes) {
            std::cout << "  " << scenario.scenario_id << " | "
                      << scenario.matured_count << " | "
                      << scenario.compatible_minimum_count << " | "
                      << scenario.compatible_maximum_count << " | "
                      << scenario.portfolio_reference_weight << '\n';
        }
        print_interpretation_boundary();
        return;
    }

    std::cout << "Primary conservative simultaneous outer set\n"
              << "  Hoeffding-Bonferroni epsilon: "
              << result.hoeffding_bonferroni_epsilon << '\n'
              << "  scenario | d_i | c_i | lower | portfolio reference/hypothesis | upper | descriptive empirical frequency | Goodman lower | Goodman upper\n";
    for (const cf::JointCohortScenarioEnvelope& scenario :
         result.scenario_envelopes) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.compatible_minimum_count << " | "
                  << scenario.compatible_maximum_count << " | "
                  << scenario.primary_lower_weight << " | "
                  << scenario.portfolio_reference_weight << " | "
                  << scenario.primary_upper_weight << " | ";
        print_optional(scenario.descriptive_empirical_frequency);
        std::cout << " | ";
        print_optional(scenario.goodman_lower_weight);
        std::cout << " | ";
        print_optional(scenario.goodman_upper_weight);
        std::cout << '\n';
    }
    if (result.goodman_diagnostic_available) {
        std::cout << "  Goodman 1965 simultaneous score challenger is a "
                     "complete-cohort large-sample diagnostic only; it never "
                     "drives the financial envelope.\n";
        if (result.goodman_sparse_cell_warning) {
            std::cout << "  Goodman sparse/zero-cell warning: at least one "
                         "matured scenario count is below five.\n";
        }
    } else {
        std::cout << "  Goodman diagnostic unavailable: it requires complete "
                     "outcomes and K>1.\n";
    }
    std::cout << "  portfolio reference lies inside every primary bound: "
              << (result.portfolio_reference_within_primary_bounds
                      ? "true"
                      : "false")
              << "\n\n";

    if (!result.financial_ranges_available ||
        !result.financial_ranges.has_value()) {
        std::cout << "Financial range evaluation: BLOCKED\n"
                  << "  " << result.block_reason << "\n";
        print_interpretation_boundary();
        return;
    }

    const cf::PortfolioAmbiguitySummary& financial =
        *result.financial_ranges;
    const std::string& currency = package.portfolio.currency_label;
    const auto print_range = [&currency](
                                 std::string_view label,
                                 const cf::AmbiguityMetricRange& range,
                                 std::string_view unit,
                                 double scale = 1.0) {
        std::cout << "  " << label << " | "
                  << range.minimum.value * scale << " | "
                  << range.central * scale << " | "
                  << range.maximum.value * scale << " | "
                  << (unit.empty() ? currency : std::string(unit)) << '\n';
    };
    std::cout << "Exact financial ranges under the generated outer set\n"
              << "  metric | minimum | declared central | maximum | unit\n";
    print_range("expected project draws",
        financial.expected_total_draws_million, currency + " million");
    print_range("expected investor receipts",
        financial.expected_total_receipts_million, currency + " million");
    print_range("expected pool costs",
        financial.expected_total_pool_costs_million, currency + " million");
    print_range("expected outstanding principal at horizon",
        financial.expected_outstanding_principal_million,
        currency + " million");
    print_range("expected terminal principal loss",
        financial.expected_principal_loss_million,
        currency + " million");
    print_range("probability of principal impairment",
        financial.principal_impairment_probability, "percent", 100.0);
    print_range("NPV using the declared hurdle and physical-P scenario weights",
        financial.expected_npv_million, currency + " million");
    print_range("probability of negative NPV",
        financial.negative_npv_probability, "percent", 100.0);
    print_range("principal-loss ES95",
        financial.principal_loss_expected_shortfall_95_million,
        currency + " million");
    print_range("principal-loss ES99",
        financial.principal_loss_expected_shortfall_99_million,
        currency + " million");
    print_range("NPV-shortfall ES95",
        financial.npv_shortfall_expected_shortfall_95_million,
        currency + " million");
    print_range("NPV-shortfall ES99",
        financial.npv_shortfall_expected_shortfall_99_million,
        currency + " million");
    print_range("expected peak same-month project draw",
        financial.expected_peak_same_month_draw_million,
        currency + " million");
    print_range("expected peak same-month gross funding need",
        financial.expected_peak_same_month_funding_need_million,
        currency + " million");
    print_range("expected peak cumulative net outlay",
        financial.expected_peak_cumulative_net_outlay_million,
        currency + " million");
    std::cout << "  Liquidity rows are expectations of each scenario's "
                 "pathwise peak; they are not a worst-path reserve or "
                 "commitment requirement.\n";
    std::cout << '\n';

    std::cout << "Underlying project financial ranges\n"
              << "  Project NPV is before shared pool costs; outstanding "
                 "principal is exposure, not realized loss.\n";
    for (const cf::ProjectAmbiguitySummary& project : financial.projects) {
        std::cout << "  project: " << project.project_id << '\n'
                  << "  metric | minimum | declared central | maximum | unit\n";
        print_range("expected draws", project.expected_total_draws_million,
            currency + " million");
        print_range("expected receipts",
            project.expected_total_receipts_million,
            currency + " million");
        print_range("expected outstanding principal",
            project.expected_outstanding_principal_million,
            currency + " million");
        print_range("expected realized principal loss",
            project.expected_realized_principal_loss_million,
            currency + " million");
        print_range("expected NPV before pool costs",
            project.expected_npv_before_pool_costs_million,
            currency + " million");
        print_range("principal impairment probability",
            project.principal_impairment_probability, "percent", 100.0);
        print_range("negative-NPV probability before pool costs",
            project.negative_npv_probability, "percent", 100.0);
    }
    std::cout << '\n';

    print_tail_attribution("Common-witness pool-loss ES95 attribution",
        financial.principal_loss_tail_attribution_95, result, currency);
    print_tail_attribution("Common-witness pool-loss ES99 attribution",
        financial.principal_loss_tail_attribution_99, result, currency);

    std::cout
        << "Expected receipts by declared external source\n"
        << "  source | nominal minimum | nominal central | nominal maximum | "
           "PV minimum | PV central | PV maximum | unit\n";
    for (const cf::AmbiguityReturnSourceTotal& source :
         financial.expected_return_sources) {
        std::cout << "  " << cf::to_string(source.source) << " | "
                  << source.nominal_million.minimum.value << " | "
                  << source.nominal_million.central << " | "
                  << source.nominal_million.maximum.value << " | "
                  << source.present_value_million.minimum.value << " | "
                  << source.present_value_million.central << " | "
                  << source.present_value_million.maximum.value << " | "
                  << currency << " million\n";
    }
    std::cout << "  Source endpoints are componentwise and must not be added "
                 "across independently optimized extrema.\n\n";

    std::cout << "Financial endpoint probability witnesses\n";
    print_range_witnesses("expected project draws",
        financial.expected_total_draws_million, result);
    print_range_witnesses("expected investor receipts",
        financial.expected_total_receipts_million, result);
    print_range_witnesses("expected pool costs",
        financial.expected_total_pool_costs_million, result);
    print_range_witnesses("expected outstanding principal",
        financial.expected_outstanding_principal_million, result);
    print_range_witnesses("expected terminal principal loss",
        financial.expected_principal_loss_million, result);
    print_range_witnesses("principal impairment probability",
        financial.principal_impairment_probability, result);
    print_range_witnesses(
        "NPV using the declared hurdle and physical-P scenario weights",
        financial.expected_npv_million, result);
    print_range_witnesses("negative NPV probability",
        financial.negative_npv_probability, result);
    print_range_witnesses("principal-loss ES95",
        financial.principal_loss_expected_shortfall_95_million, result);
    print_range_witnesses("principal-loss ES99",
        financial.principal_loss_expected_shortfall_99_million, result);
    print_range_witnesses("NPV-shortfall ES95",
        financial.npv_shortfall_expected_shortfall_95_million, result);
    print_range_witnesses("NPV-shortfall ES99",
        financial.npv_shortfall_expected_shortfall_99_million, result);
    print_range_witnesses("peak same-month project draw",
        financial.expected_peak_same_month_draw_million, result);
    print_range_witnesses("peak gross funding need",
        financial.expected_peak_same_month_funding_need_million, result);
    print_range_witnesses("peak cumulative net outlay",
        financial.expected_peak_cumulative_net_outlay_million, result);
    for (const cf::AmbiguityReturnSourceTotal& source :
         financial.expected_return_sources) {
        const std::string source_label(cf::to_string(source.source));
        print_range_witnesses(source_label + " nominal receipts",
            source.nominal_million, result);
        print_range_witnesses(source_label + " PV receipts",
            source.present_value_million, result);
    }
    for (const cf::ProjectAmbiguitySummary& project : financial.projects) {
        const std::string prefix = "project " + project.project_id + ' ';
        print_range_witnesses(prefix + "expected draws",
            project.expected_total_draws_million, result);
        print_range_witnesses(prefix + "expected receipts",
            project.expected_total_receipts_million, result);
        print_range_witnesses(prefix + "expected outstanding principal",
            project.expected_outstanding_principal_million, result);
        print_range_witnesses(prefix + "expected realized principal loss",
            project.expected_realized_principal_loss_million, result);
        print_range_witnesses(prefix + "expected NPV before pool costs",
            project.expected_npv_before_pool_costs_million, result);
        print_range_witnesses(prefix + "principal impairment probability",
            project.principal_impairment_probability, result);
        print_range_witnesses(prefix + "negative-NPV probability",
            project.negative_npv_probability, result);
    }
    std::cout << '\n';

    std::cout << "Probability and reconciliation controls\n"
              << "  configured central weight sum: "
              << financial.configured_central_weight_sum << '\n'
              << "  component lower-bound sum: "
              << financial.lower_bound_sum << '\n'
              << "  component upper-bound sum: "
              << financial.upper_bound_sum << '\n'
              << "  maximum endpoint probability error: "
              << financial.maximum_endpoint_probability_error << '\n'
              << "  maximum central metric reconciliation error: "
              << financial.maximum_central_metric_reconciliation_error
              << "\n"
              << "  ES95 maximum tail-mass reconciliation error: "
              << financial.principal_loss_tail_attribution_95
                     .maximum_tail_mass_reconciliation_error
              << '\n'
              << "  ES95 maximum project-contribution reconciliation error: "
              << financial.principal_loss_tail_attribution_95
                     .maximum_project_contribution_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  ES99 maximum tail-mass reconciliation error: "
              << financial.principal_loss_tail_attribution_99
                     .maximum_tail_mass_reconciliation_error
              << '\n'
              << "  ES99 maximum project-contribution reconciliation error: "
              << financial.principal_loss_tail_attribution_99
                     .maximum_project_contribution_reconciliation_error_million
              << ' ' << currency << " million\n\n";

    std::cout << "Joint impairment projections\n"
              << "  project impairment means scenario "
                 "principal_loss_million > 0\n"
              << "  Individual project impairment appears in the project "
                 "financial ranges above.\n"
              << "  metric | minimum | declared central | maximum\n";
    if (result.pair_impairment_projections_available) {
        for (const cf::JointCohortPairImpairmentProjection& pair :
             result.pair_impairment_probabilities) {
            print_projection(
                "pair " + pair.first_project_id + "+" +
                    pair.second_project_id + " simultaneous impairment",
                pair.simultaneous_impairment_probability, result);
        }
    } else {
        std::cout << "  pair projections omitted: "
                  << result.pair_impairment_projection_block_reason << '\n';
    }
    if (result.any_project_impairment_probability.has_value()) {
        print_projection("any-project impairment",
            *result.any_project_impairment_probability, result);
    }
    if (result.all_projects_impairment_probability.has_value()) {
        print_projection("all-project impairment",
            *result.all_projects_impairment_probability, result);
    }

    print_interpretation_boundary();
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3 ||
        std::string_view(argv[1]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 3;
    if (print_normalized &&
        std::string_view(argv[2]) != "--print-normalized") {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const cf::JointCohortPackage package =
            cf::load_joint_cohort_package(
                std::filesystem::path(argv[1]));
        const cf::JointCohortResult result = cf::evaluate_joint_cohort(
            package.config.analysis, package.portfolio,
            package.observations);
        print_report(package, result);
        if (print_normalized) {
            std::cout
                << "\nNormalized semantic renderings\n"
                << "These audit-friendly renderings are not a directly "
                   "reloadable hash-consistent cohort package. The cohort "
                   "configuration retains original raw-file hashes; comments "
                   "are removed and ledger rows may be sorted. Recompute the "
                   "portfolio and ledger SHA-256 values before rebinding the "
                   "rendered files as a new package.\n"
                << "\nNormalized joint-cohort configuration (retains original "
                   "raw-file hashes)\n";
            std::cout << std::defaultfloat
                      << std::setprecision(
                             std::numeric_limits<double>::max_digits10);
            cf::print_normalized_joint_cohort_config(
                std::cout, package.config);
            std::cout << "\nNormalized portfolio configuration\n";
            cf::print_normalized_portfolio_config(
                std::cout, package.portfolio);
            if (result.generated_probability_envelope.has_value()) {
                std::cout
                    << "\nNormalized generated probability-envelope configuration\n";
                cf::print_normalized_portfolio_ambiguity_config(
                    std::cout, *result.generated_probability_envelope);
            }
            std::cout
                << "\nNormalized semantic rendering of authoritative ledger rows\n";
            cf::print_normalized_joint_cohort_ledger(
                std::cout, package.observations);
        }
        return result.primary_outer_set_available &&
                result.financial_ranges_available
            ? 0
            : 3;
    } catch (const std::exception& error) {
        std::cerr << "joint-cohort envelope failed: " << error.what() << '\n'
                  << "candidate_only=true; "
                     "calibrated_execution_authorized=false\n";
        return 1;
    }
}
