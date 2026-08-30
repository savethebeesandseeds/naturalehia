// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/probability_polytope.hpp>
#include <naturalehia/cellular_finance/probability_polytope_config.hpp>
#include <naturalehia/cellular_finance/probability_polytope_tail_attribution.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr
        << "usage: " << program
        << " <portfolio.cfg> <probability-envelope.cfg> [--print-normalized]\n"
        << "       " << program
        << " --event-polytope <portfolio.cfg> <polytope.cfg> "
           "[--print-normalized]\n";
}

void print_range_row(std::string_view label,
    const cf::AmbiguityMetricRange& range, std::string_view unit,
    double scale = 1.0) {
    std::cout << "  " << label << " | "
              << range.minimum.value * scale << " | "
              << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_witness(std::string_view label, std::string_view endpoint_label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::PortfolioAmbiguitySummary& summary, double scale = 1.0) {
    if (endpoint.scenario_weights.size() !=
        summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "probability-envelope witness has the wrong scenario count");
    }
    std::cout << "  " << label << " | " << endpoint_label << " | "
              << endpoint.value * scale << " | ";
    for (std::size_t index = 0U;
         index < summary.scenario_probability_bounds.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout << summary.scenario_probability_bounds[index].scenario_id
                  << '=' << endpoint.scenario_weights[index];
    }
    std::cout << '\n';
}

void print_witness_pair(std::string_view label,
    const cf::AmbiguityMetricRange& range,
    const cf::PortfolioAmbiguitySummary& summary, double scale = 1.0) {
    print_witness(label, "minimum", range.minimum, summary, scale);
    print_witness(label, "maximum", range.maximum, summary, scale);
}

void print_tail_mass_vector(std::string_view label,
    const std::vector<double>& masses,
    const cf::PortfolioAmbiguitySummary& summary) {
    if (masses.size() != summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "pool-tail attribution has the wrong scenario count");
    }
    std::cout << "  " << label << " | ";
    for (std::size_t index = 0U; index < masses.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout << summary.scenario_probability_bounds[index].scenario_id
                  << '=' << masses[index];
    }
    std::cout << '\n';
}

void print_tail_attribution(std::string_view label,
    const cf::PoolLossTailAttribution& attribution,
    const cf::PortfolioAmbiguitySummary& summary,
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
        attribution.minimum_pool_es_tail_mass_weights, summary);
    print_tail_mass_vector("declared central measure",
        attribution.central_tail_mass_weights, summary);
    print_tail_mass_vector("maximum-pool-ES measure",
        attribution.maximum_pool_es_tail_mass_weights, summary);
    std::cout
        << "  These are additive attributions under three shared measures, "
           "not independently optimized project ranges.\n\n";
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::PortfolioAmbiguityConfig& ambiguity,
    const cf::PortfolioAmbiguitySummary& summary) {
    const std::string& currency = portfolio.currency_label;
    std::cout << std::fixed << std::setprecision(6);
    std::cout
        << "SYNTHETIC PHYSICAL-PROBABILITY ENVELOPE ANALYSIS\n"
        << "Not a forecast, fair value, market price, rating, term sheet, "
           "offering document, or investment recommendation.\n\n"
        << "Analysis basis\n"
        << "  portfolio: " << portfolio.scenario_label << '\n'
        << "  probability envelope: " << ambiguity.scenario_label << '\n'
        << "  portfolio source note: " << portfolio.source_note << '\n'
        << "  envelope source note: " << ambiguity.source_note << '\n'
        << "  portfolio model version: " << portfolio.model_version << '\n'
        << "  envelope model version: " << ambiguity.model_version << '\n'
        << "  measure: physical P sensitivity over fixed joint cash paths\n"
        << "  monetary basis: " << portfolio.monetary_basis << '\n'
        << "  central envelope weight sum before normalization: "
        << std::defaultfloat
        << std::setprecision(std::numeric_limits<double>::max_digits10)
        << summary.configured_central_weight_sum << '\n'
        << "  lower-bound sum: " << summary.lower_bound_sum << '\n'
        << "  upper-bound sum: " << summary.upper_bound_sum << '\n'
        << std::fixed << std::setprecision(6) << '\n';

    std::cout << "Probability bounds\n"
              << "  scenario | lower | central | upper\n";
    for (const cf::ScenarioProbabilityBounds& bounds :
         summary.scenario_probability_bounds) {
        std::cout << "  " << bounds.scenario_id << " | "
                  << bounds.lower_weight * 100.0 << "% | "
                  << bounds.central_weight * 100.0 << "% | "
                  << bounds.upper_weight * 100.0 << "%\n";
    }
    std::cout << '\n';

    std::cout << "Exact componentwise financial ranges\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_range_row("expected project draws",
        summary.expected_total_draws_million, currency + " million");
    print_range_row("expected investor receipts",
        summary.expected_total_receipts_million, currency + " million");
    print_range_row("expected pool costs",
        summary.expected_total_pool_costs_million, currency + " million");
    print_range_row("expected outstanding principal at horizon",
        summary.expected_outstanding_principal_million,
        currency + " million");
    print_range_row("expected realized principal loss",
        summary.expected_principal_loss_million, currency + " million");
    print_range_row("probability of any realized principal impairment",
        summary.principal_impairment_probability, "percent", 100.0);
    print_range_row("expected NPV at declared physical-P hurdle",
        summary.expected_npv_million, currency + " million");
    print_range_row("probability of negative NPV",
        summary.negative_npv_probability, "percent", 100.0);
    print_range_row("principal-loss ES95",
        summary.principal_loss_expected_shortfall_95_million,
        currency + " million");
    print_range_row("principal-loss ES99",
        summary.principal_loss_expected_shortfall_99_million,
        currency + " million");
    print_range_row("NPV-shortfall ES95",
        summary.npv_shortfall_expected_shortfall_95_million,
        currency + " million");
    print_range_row("NPV-shortfall ES99",
        summary.npv_shortfall_expected_shortfall_99_million,
        currency + " million");
    print_range_row("expected peak same-month project draw",
        summary.expected_peak_same_month_draw_million,
        currency + " million");
    print_range_row("expected peak same-month gross funding need",
        summary.expected_peak_same_month_funding_need_million,
        currency + " million");
    print_range_row("expected peak cumulative net outlay",
        summary.expected_peak_cumulative_net_outlay_million,
        currency + " million");
    std::cout << '\n';

    std::cout << "Underlying project financial ranges\n"
              << "  Project NPV is before shared pool costs; outstanding "
                 "principal is exposure, not realized loss.\n";
    for (const cf::ProjectAmbiguitySummary& project : summary.projects) {
        std::cout << "  project: " << project.project_id << '\n'
                  << "  metric | minimum | central | maximum | unit\n";
        print_range_row("expected draws",
            project.expected_total_draws_million, currency + " million");
        print_range_row("expected receipts",
            project.expected_total_receipts_million, currency + " million");
        print_range_row("expected outstanding principal",
            project.expected_outstanding_principal_million,
            currency + " million");
        print_range_row("expected realized principal loss",
            project.expected_realized_principal_loss_million,
            currency + " million");
        print_range_row("expected NPV before pool costs",
            project.expected_npv_before_pool_costs_million,
            currency + " million");
        print_range_row("principal impairment probability",
            project.principal_impairment_probability, "percent", 100.0);
        print_range_row("negative-NPV probability before pool costs",
            project.negative_npv_probability, "percent", 100.0);
    }
    std::cout << '\n';

    print_tail_attribution("Common-witness pool-loss ES95 attribution",
        summary.principal_loss_tail_attribution_95, summary, currency);
    print_tail_attribution("Common-witness pool-loss ES99 attribution",
        summary.principal_loss_tail_attribution_99, summary, currency);

    std::cout
        << "Expected receipts by declared external source\n"
        << "  source | nominal minimum | nominal central | nominal maximum | "
           "PV minimum | PV central | PV maximum | unit\n";
    for (const cf::AmbiguityReturnSourceTotal& source :
         summary.expected_return_sources) {
        std::cout << "  " << cf::to_string(source.source) << " | "
                  << source.nominal_million.minimum.value << " | "
                  << source.nominal_million.central << " | "
                  << source.nominal_million.maximum.value << " | "
                  << source.present_value_million.minimum.value << " | "
                  << source.present_value_million.central << " | "
                  << source.present_value_million.maximum.value << " | "
                  << currency << " million\n";
    }
    std::cout
        << "  Source endpoints are componentwise. Do not add independently "
           "optimized minima or maxima.\n\n";

    std::cout << "Endpoint witness ledger\n"
              << "  metric | endpoint | value | scenario probability vector\n";
    print_witness_pair("expected project draws",
        summary.expected_total_draws_million, summary);
    print_witness_pair("expected investor receipts",
        summary.expected_total_receipts_million, summary);
    print_witness_pair("expected pool costs",
        summary.expected_total_pool_costs_million, summary);
    print_witness_pair("expected outstanding principal",
        summary.expected_outstanding_principal_million, summary);
    print_witness_pair("expected realized principal loss",
        summary.expected_principal_loss_million, summary);
    print_witness_pair("principal impairment probability",
        summary.principal_impairment_probability, summary, 100.0);
    print_witness_pair("expected NPV", summary.expected_npv_million, summary);
    print_witness_pair("negative NPV probability",
        summary.negative_npv_probability, summary, 100.0);
    print_witness_pair("principal-loss ES95",
        summary.principal_loss_expected_shortfall_95_million, summary);
    print_witness_pair("principal-loss ES99",
        summary.principal_loss_expected_shortfall_99_million, summary);
    print_witness_pair("NPV-shortfall ES95",
        summary.npv_shortfall_expected_shortfall_95_million, summary);
    print_witness_pair("NPV-shortfall ES99",
        summary.npv_shortfall_expected_shortfall_99_million, summary);
    print_witness_pair("peak same-month draw",
        summary.expected_peak_same_month_draw_million, summary);
    print_witness_pair("peak gross funding need",
        summary.expected_peak_same_month_funding_need_million, summary);
    print_witness_pair("peak cumulative outlay",
        summary.expected_peak_cumulative_net_outlay_million, summary);
    for (const cf::AmbiguityReturnSourceTotal& source :
         summary.expected_return_sources) {
        const std::string nominal_label =
            std::string(cf::to_string(source.source)) + " nominal receipts";
        const std::string present_value_label =
            std::string(cf::to_string(source.source)) + " PV receipts";
        print_witness_pair(
            nominal_label, source.nominal_million, summary);
        print_witness_pair(
            present_value_label, source.present_value_million, summary);
    }
    for (const cf::ProjectAmbiguitySummary& project : summary.projects) {
        const std::string prefix = "project " + project.project_id + ' ';
        print_witness_pair(prefix + "expected draws",
            project.expected_total_draws_million, summary);
        print_witness_pair(prefix + "expected receipts",
            project.expected_total_receipts_million, summary);
        print_witness_pair(prefix + "expected outstanding principal",
            project.expected_outstanding_principal_million, summary);
        print_witness_pair(prefix + "expected realized principal loss",
            project.expected_realized_principal_loss_million, summary);
        print_witness_pair(prefix + "expected NPV before pool costs",
            project.expected_npv_before_pool_costs_million, summary);
        print_witness_pair(prefix + "principal impairment probability",
            project.principal_impairment_probability, summary, 100.0);
        print_witness_pair(prefix + "negative-NPV probability",
            project.negative_npv_probability, summary, 100.0);
    }
    std::cout << '\n';

    std::cout << "Reconciliation controls\n"
              << "  maximum endpoint probability error: "
              << summary.maximum_endpoint_probability_error << '\n'
              << "  maximum central metric reconciliation error: "
              << summary.maximum_central_metric_reconciliation_error << ' '
              << currency << " million or raw probability units\n"
              << "  ES95 maximum tail-mass reconciliation error: "
              << summary.principal_loss_tail_attribution_95
                     .maximum_tail_mass_reconciliation_error
              << '\n'
              << "  ES95 maximum project-contribution reconciliation error: "
              << summary.principal_loss_tail_attribution_95
                     .maximum_project_contribution_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  ES99 maximum tail-mass reconciliation error: "
              << summary.principal_loss_tail_attribution_99
                     .maximum_tail_mass_reconciliation_error
              << '\n'
              << "  ES99 maximum project-contribution reconciliation error: "
              << summary.principal_loss_tail_attribution_99
                     .maximum_project_contribution_reconciliation_error_million
              << ' ' << currency << " million\n\n";

    std::cout
        << "Interpretation boundary\n"
        << "  The scenarios and all dated cash paths remain fixed; only their "
           "physical probabilities vary inside the declared bounds.\n"
        << "  Every endpoint has its own feasible witness. Endpoints from "
           "different rows need not be jointly attainable and must not be "
           "combined into a synthetic best or worst portfolio.\n"
        << "  Within an exact tied-objective block, endpoint probability "
           "capacity is filled pro rata; fractional pool-tail ties are also "
           "allocated pro rata. The selected attribution is auditable but is "
           "not claimed to be the only mathematically optimal attribution.\n"
        << "  ES endpoints are exact for this interval-probability, sum-to-one "
           "set and the fixed scenario losses. Richer dependence or moment "
           "constraints require a different optimizer.\n"
        << "  Central NPV uses the declared physical-P hurdle. Neither the "
           "central value nor its range is risk-neutral value, fair value, or "
           "an investable quote.\n"
        << "  These inputs are synthetic. Real use requires the documented "
           "calibration population, evidence, cohort-transfer analysis, and "
           "validation record.\n";
}

struct PolytopeReportMetric {
    std::string label{};
    std::string unit{};
    double display_scale{1.0};
    cf::ProbabilityPolytopeMetricProjection projection{};
};

struct PolytopeTailReportMetric {
    std::string label{};
    std::string unit{};
    cf::ProbabilityPolytopeUpperExpectedShortfallProjection projection{};
};

template <typename ScenarioValue>
cf::ProbabilityPolytopeMetricProjection project_scenario_values(
    const cf::ProbabilityPolytopeProjector& projector,
    const cf::PortfolioSummary& fixed_paths, ScenarioValue scenario_value) {
    std::vector<cf::ProbabilityPolytopeScenarioValue> values;
    values.reserve(fixed_paths.scenarios.size());
    for (const cf::JointScenarioResult& scenario : fixed_paths.scenarios) {
        values.push_back(
            cf::ProbabilityPolytopeScenarioValue{
                scenario.scenario_id, scenario_value(scenario)});
    }
    return projector.project_expectation(values);
}

template <typename ScenarioValue>
cf::ProbabilityPolytopeUpperExpectedShortfallProjection
project_tail_scenario_values(
    const cf::ProbabilityPolytopeProjector& projector,
    const cf::PortfolioSummary& fixed_paths, ScenarioValue scenario_value,
    double tail_probability) {
    std::vector<cf::ProbabilityPolytopeScenarioValue> values;
    values.reserve(fixed_paths.scenarios.size());
    for (const cf::JointScenarioResult& scenario : fixed_paths.scenarios) {
        values.push_back(
            cf::ProbabilityPolytopeScenarioValue{
                scenario.scenario_id, scenario_value(scenario)});
    }
    return projector.project_upper_expected_shortfall(
        values, tail_probability);
}

template <typename ScenarioValue>
void add_polytope_metric(std::vector<PolytopeReportMetric>& metrics,
    std::string label, std::string unit, double display_scale,
    const cf::ProbabilityPolytopeProjector& projector,
    const cf::PortfolioSummary& fixed_paths, ScenarioValue scenario_value) {
    metrics.push_back(PolytopeReportMetric{std::move(label), std::move(unit),
        display_scale,
        project_scenario_values(projector, fixed_paths, scenario_value)});
}

const cf::ProjectPathResult& find_project_path(
    const cf::JointScenarioResult& scenario, std::string_view project_id) {
    const auto found = std::find_if(scenario.projects.begin(),
        scenario.projects.end(),
        [project_id](const cf::ProjectPathResult& project) {
            return project.project_id == project_id;
        });
    if (found == scenario.projects.end()) {
        throw std::logic_error("evaluated scenario " + scenario.scenario_id +
            " is missing project " + std::string(project_id));
    }
    return *found;
}

double central_event_probability(
    const cf::ProbabilityEventConstraint& event,
    const std::vector<cf::ProbabilityPolytopeScenario>& scenarios) {
    double probability = 0.0;
    for (const std::string& member : event.scenario_ids) {
        const auto found = std::find_if(scenarios.begin(), scenarios.end(),
            [&member](const cf::ProbabilityPolytopeScenario& scenario) {
                return scenario.scenario_id == member;
            });
        if (found == scenarios.end()) {
            throw std::logic_error(
                "event " + event.event_id + " has an unknown scenario member");
        }
        probability += found->central_weight;
    }
    return probability;
}

void print_polytope_range_row(const PolytopeReportMetric& metric) {
    const cf::ProbabilityPolytopeMetricRange& range =
        metric.projection.expectation;
    std::cout << "  " << metric.label << " | "
              << range.minimum.value * metric.display_scale << " | "
              << range.central * metric.display_scale << " | "
              << range.maximum.value * metric.display_scale << " | "
              << metric.unit << '\n';
}

void print_polytope_tail_range_row(const PolytopeTailReportMetric& metric) {
    const auto& projection = metric.projection;
    std::cout << "  " << metric.label << " | "
              << projection.minimum.value << " | " << projection.central
              << " | " << projection.maximum.value << " | "
              << metric.unit << '\n';
}

void print_polytope_witness(const PolytopeReportMetric& metric,
    std::string_view endpoint_label,
    const cf::ProbabilityPolytopeEndpoint& endpoint) {
    if (endpoint.scenario_weights.size() !=
        metric.projection.scenario_probabilities.size()) {
        throw std::logic_error(
            "event-polytope witness has the wrong scenario count");
    }

    std::cout << "  " << metric.label << " | " << endpoint_label << " | "
              << endpoint.value * metric.display_scale << " | ";
    for (std::size_t index = 0U;
         index < metric.projection.scenario_probabilities.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout
            << metric.projection.scenario_probabilities[index].scenario_id
            << '=' << endpoint.scenario_weights[index];
    }
    std::cout << " | " << std::scientific
              << endpoint.maximum_constraint_violation << " | "
              << endpoint.objective_reconciliation_error << " | "
              << endpoint.optimality_residual << std::fixed << '\n';
}

void print_polytope_witness_pair(const PolytopeReportMetric& metric) {
    print_polytope_witness(
        metric, "minimum", metric.projection.expectation.minimum);
    print_polytope_witness(
        metric, "maximum", metric.projection.expectation.maximum);
}

void print_polytope_tail_witness(const PolytopeTailReportMetric& metric,
    std::string_view endpoint_label,
    const cf::ProbabilityPolytopeUpperExpectedShortfallEndpoint& endpoint) {
    if (endpoint.scenario_weights.size() !=
            metric.projection.scenario_probabilities.size() ||
        endpoint.tail_mass_weights.size() !=
            metric.projection.scenario_probabilities.size()) {
        throw std::logic_error(
            "event-polytope tail witness has the wrong scenario count");
    }
    std::cout << "  " << metric.label << " | " << endpoint_label << " | "
              << endpoint.value << " | p: ";
    for (std::size_t index = 0U;
         index < metric.projection.scenario_probabilities.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout
            << metric.projection.scenario_probabilities[index].scenario_id
            << '=' << endpoint.scenario_weights[index];
    }
    std::cout << " | tail: ";
    for (std::size_t index = 0U;
         index < metric.projection.scenario_probabilities.size(); ++index) {
        if (index != 0U) {
            std::cout << "; ";
        }
        std::cout
            << metric.projection.scenario_probabilities[index].scenario_id
            << '=' << endpoint.tail_mass_weights[index];
    }
    std::cout << " | " << std::scientific
              << endpoint.maximum_constraint_violation << " | "
              << endpoint.maximum_tail_mass_violation << " | "
              << endpoint.objective_reconciliation_error << " | "
              << endpoint.threshold_formula_reconciliation_error << " | "
              << endpoint.optimality_residual << std::fixed << '\n';
}

void print_polytope_tail_witness_pair(
    const PolytopeTailReportMetric& metric) {
    print_polytope_tail_witness(
        metric, "minimum", metric.projection.minimum);
    print_polytope_tail_witness(
        metric, "maximum", metric.projection.maximum);
}

void print_polytope_tail_attribution(std::string_view label,
    const cf::ProbabilityPolytopePoolLossTailAttribution& attribution,
    std::string_view currency) {
    std::cout << label << '\n'
              << "  project | at minimum pool-ES witness | at declared "
                 "central measure | at maximum pool-ES witness | unit\n";
    for (const auto& project : attribution.projects) {
        std::cout << "  " << project.project_id << " | "
                  << project.at_minimum_pool_es_witness_million << " | "
                  << project.at_central_measure_million << " | "
                  << project.at_maximum_pool_es_witness_million << " | "
                  << currency << " million\n";
    }
    const auto print_masses = [&](std::string_view column,
                                  const std::vector<double>& masses) {
        if (masses.size() != attribution.scenario_ids.size()) {
            throw std::logic_error(
                "event-polytope attribution has the wrong scenario count");
        }
        std::cout << "  " << column << " tail mass | ";
        for (std::size_t index = 0U; index < masses.size(); ++index) {
            if (index != 0U) {
                std::cout << "; ";
            }
            std::cout << attribution.scenario_ids[index] << '='
                      << masses[index];
        }
        std::cout << '\n';
    };
    print_masses("minimum", attribution.minimum_pool_es_tail_mass_weights);
    print_masses("central", attribution.central_tail_mass_weights);
    print_masses("maximum", attribution.maximum_pool_es_tail_mass_weights);
    std::cout << "  maximum pathwise loss-additivity error: "
              << std::scientific
              << attribution
                     .maximum_pathwise_pool_loss_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum contribution reconciliation error: "
              << attribution
                     .maximum_contribution_to_pool_es_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  witness boundary: " << attribution.witness_disclosure
              << std::fixed << "\n\n";
}

std::vector<PolytopeReportMetric> make_pool_polytope_metrics(
    const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeProjector& projector,
    const cf::PortfolioSummary& fixed_paths) {
    const std::string money_unit = portfolio.currency_label + " million";
    std::vector<PolytopeReportMetric> metrics;
    metrics.reserve(11U);
    add_polytope_metric(metrics, "pool expected draws", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.total_draws_million;
        });
    add_polytope_metric(metrics, "pool expected receipts", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.total_receipts_million;
        });
    add_polytope_metric(metrics, "pool expected costs", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.total_pool_costs_million;
        });
    add_polytope_metric(metrics, "pool outstanding exposure at horizon",
        money_unit, 1.0, projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.outstanding_principal_million;
        });
    add_polytope_metric(metrics,
        "pool expected resolved principal loss at horizon", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.principal_loss_million;
        });
    add_polytope_metric(metrics, "pool impairment probability", "percent",
        100.0, projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.principal_loss_million > 0.0 ? 1.0 : 0.0;
        });
    add_polytope_metric(metrics,
        "pool expected NPV at declared physical-P hurdle", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.npv_million;
        });
    add_polytope_metric(metrics, "pool negative-NPV probability", "percent",
        100.0, projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.npv_million < 0.0 ? 1.0 : 0.0;
        });
    add_polytope_metric(metrics,
        "pool expected peak same-month project draw", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.peak_same_month_draw_million;
        });
    add_polytope_metric(metrics,
        "pool expected peak same-month gross funding need", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.peak_same_month_funding_need_million;
        });
    add_polytope_metric(metrics,
        "pool expected peak cumulative net outlay", money_unit, 1.0,
        projector, fixed_paths,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.peak_cumulative_net_outlay_million;
        });
    return metrics;
}

std::vector<PolytopeReportMetric> make_project_polytope_metrics(
    const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeProjector& projector,
    const cf::PortfolioSummary& fixed_paths) {
    const std::string money_unit = portfolio.currency_label + " million";
    std::vector<std::string> project_ids;
    project_ids.reserve(portfolio.projects.size());
    for (const cf::PortfolioProject& project : portfolio.projects) {
        project_ids.push_back(project.id);
    }
    std::sort(project_ids.begin(), project_ids.end());

    std::vector<PolytopeReportMetric> metrics;
    metrics.reserve(project_ids.size() * 7U);
    for (const std::string& project_id : project_ids) {
        const std::string prefix = "project " + project_id + ' ';
        add_polytope_metric(metrics, prefix + "expected draws", money_unit,
            1.0, projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                    .total_draws_million;
            });
        add_polytope_metric(metrics, prefix + "expected receipts", money_unit,
            1.0, projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                    .total_receipts_million;
            });
        add_polytope_metric(metrics,
            prefix + "outstanding exposure at horizon", money_unit, 1.0,
            projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                    .outstanding_principal_million;
            });
        add_polytope_metric(metrics,
            prefix + "expected resolved principal loss at horizon", money_unit,
            1.0, projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                    .principal_loss_million;
            });
        add_polytope_metric(metrics, prefix + "impairment probability",
            "percent", 100.0, projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                               .principal_loss_million > 0.0
                    ? 1.0
                    : 0.0;
            });
        add_polytope_metric(metrics,
            prefix + "expected NPV before pool costs", money_unit, 1.0,
            projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                    .npv_before_pool_costs_million;
            });
        add_polytope_metric(metrics,
            prefix + "negative-NPV probability before pool costs", "percent",
            100.0, projector, fixed_paths,
            [&project_id](const cf::JointScenarioResult& scenario) {
                return find_project_path(scenario, project_id)
                               .npv_before_pool_costs_million < 0.0
                    ? 1.0
                    : 0.0;
            });
    }
    return metrics;
}

void print_event_polytope_report(const cf::PortfolioConfig& portfolio,
    const cf::ProbabilityPolytopeConfig& probability_polytope,
    const cf::ProbabilityPolytopeProjector& projector,
    const cf::PortfolioSummary& fixed_paths) {
    const std::vector<PolytopeReportMetric> pool_metrics =
        make_pool_polytope_metrics(portfolio, projector, fixed_paths);
    const std::vector<PolytopeReportMetric> project_metrics =
        make_project_polytope_metrics(portfolio, projector, fixed_paths);
    const std::string money_unit = portfolio.currency_label + " million";
    const std::array<PolytopeTailReportMetric, 4U> tail_metrics{
        PolytopeTailReportMetric{
            "pool resolved principal loss ES95", money_unit,
            project_tail_scenario_values(projector, fixed_paths,
                [](const cf::JointScenarioResult& scenario) {
                    return scenario.principal_loss_million;
                },
                0.05)},
        PolytopeTailReportMetric{
            "pool resolved principal loss ES99", money_unit,
            project_tail_scenario_values(projector, fixed_paths,
                [](const cf::JointScenarioResult& scenario) {
                    return scenario.principal_loss_million;
                },
                0.01)},
        PolytopeTailReportMetric{
            "pool NPV shortfall ES95", money_unit,
            project_tail_scenario_values(projector, fixed_paths,
                [](const cf::JointScenarioResult& scenario) {
                    return std::max(0.0, -scenario.npv_million);
                },
                0.05)},
        PolytopeTailReportMetric{
            "pool NPV shortfall ES99", money_unit,
            project_tail_scenario_values(projector, fixed_paths,
                [](const cf::JointScenarioResult& scenario) {
                    return std::max(0.0, -scenario.npv_million);
                },
                0.01)}};
    const cf::ProbabilityPolytopePoolLossTailAttribution attribution_95 =
        cf::attribute_probability_polytope_pool_loss_tail(
            portfolio, tail_metrics[0].projection);
    const cf::ProbabilityPolytopePoolLossTailAttribution attribution_99 =
        cf::attribute_probability_polytope_pool_loss_tail(
            portfolio, tail_metrics[1].projection);
    if (pool_metrics.empty()) {
        throw std::logic_error("event-polytope report has no pool metrics");
    }
    const cf::ProbabilityPolytopeMetricProjection& taxonomy =
        pool_metrics.front().projection;

    std::cout << std::fixed << std::setprecision(6);
    std::cout
        << "SYNTHETIC EVENT-PROBABILITY POLYTOPE ANALYSIS\n"
        << "Not a forecast, calibration, fair value, market price, rating, "
           "term sheet, offering document, or investment recommendation.\n\n"
        << "Analysis basis\n"
        << "  portfolio: " << portfolio.scenario_label << '\n'
        << "  event-probability polytope: "
        << probability_polytope.scenario_label << '\n'
        << "  portfolio source note: " << portfolio.source_note << '\n'
        << "  polytope source note: " << probability_polytope.source_note
        << '\n'
        << "  portfolio model version: " << portfolio.model_version << '\n'
        << "  polytope model version: " << probability_polytope.model_version
        << '\n'
        << "  measure: physical P sensitivity over fixed joint cash paths\n"
        << "  solver scope: audited floating-point linear and upper-tail "
           "fixed-path risk\n"
        << "  monetary basis: " << portfolio.monetary_basis << '\n'
        << "  calibrated_execution_authorized=false\n\n";

    std::cout << "Scenario probability box\n"
              << "  scenario | lower | central | upper\n";
    for (const cf::ProbabilityPolytopeScenario& scenario :
         taxonomy.scenario_probabilities) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.lower_weight * 100.0 << "% | "
                  << scenario.central_weight * 100.0 << "% | "
                  << scenario.upper_weight * 100.0 << "%\n";
    }
    std::cout << '\n';

    std::cout
        << "Named event constraints\n"
        << "  event | definition | lower | central | upper | explicit members\n";
    for (const cf::ProbabilityEventConstraint& event : taxonomy.events) {
        std::cout << "  " << event.event_id << " | " << event.definition
                  << " | " << event.lower_probability * 100.0 << "% | "
                  << central_event_probability(
                         event, taxonomy.scenario_probabilities) *
                         100.0
                  << "% | " << event.upper_probability * 100.0 << "% | ";
        for (std::size_t index = 0U; index < event.scenario_ids.size();
             ++index) {
            if (index != 0U) {
                std::cout << "; ";
            }
            std::cout << event.scenario_ids[index];
        }
        std::cout << '\n';
    }
    std::cout << '\n';

    std::cout << "Audited floating-point linear fixed-path pool ranges\n"
              << "  metric | minimum | central | maximum | unit\n";
    for (const PolytopeReportMetric& metric : pool_metrics) {
        print_polytope_range_row(metric);
    }
    std::cout << '\n';

    std::cout
        << "Audited floating-point linear fixed-path project ranges\n"
        << "  Project NPV is before shared pool costs; outstanding principal "
           "is exposure, not resolved loss.\n"
        << "  metric | minimum | central | maximum | unit\n";
    for (const PolytopeReportMetric& metric : project_metrics) {
        print_polytope_range_row(metric);
    }
    std::cout << '\n';

    std::cout
        << "Audited floating-point fixed-path upper-tail ranges\n"
        << "  Tail mass is 5% for ES95 and 1% for ES99.\n"
        << "  metric | minimum | central | maximum | unit\n";
    for (const PolytopeTailReportMetric& metric : tail_metrics) {
        print_polytope_tail_range_row(metric);
    }
    std::cout << '\n';

    print_polytope_tail_attribution(
        "Common aggregate-loss ES95 project attribution", attribution_95,
        portfolio.currency_label);
    print_polytope_tail_attribution(
        "Common aggregate-loss ES99 project attribution", attribution_99,
        portfolio.currency_label);

    std::cout
        << "Endpoint witness ledger\n"
        << "  metric | endpoint | value | scenario probability vector | "
           "maximum constraint violation | objective reconciliation error | "
           "simplex reduced-cost residual\n";
    for (const PolytopeReportMetric& metric : pool_metrics) {
        print_polytope_witness_pair(metric);
    }
    for (const PolytopeReportMetric& metric : project_metrics) {
        print_polytope_witness_pair(metric);
    }
    std::cout << '\n';

    std::cout
        << "Upper-tail endpoint witness ledger\n"
        << "  metric | endpoint | value | full probability vector | "
           "fractional tail-mass vector | maximum constraint violation | "
           "maximum tail-mass violation | objective reconciliation error | "
           "threshold-formula reconciliation error | simplex reduced-cost "
           "residual\n";
    for (const PolytopeTailReportMetric& metric : tail_metrics) {
        print_polytope_tail_witness_pair(metric);
    }
    std::cout << '\n';

    double maximum_constraint_violation = 0.0;
    double maximum_objective_reconciliation_error = 0.0;
    double maximum_optimality_residual = 0.0;
    double maximum_tail_mass_violation = 0.0;
    double maximum_threshold_formula_error = 0.0;
    double maximum_threshold_enumeration_optimality_residual = 0.0;
    const auto collect_residuals = [&](const PolytopeReportMetric& metric) {
        maximum_constraint_violation =
            std::max(maximum_constraint_violation,
                metric.projection.maximum_endpoint_constraint_violation);
        maximum_objective_reconciliation_error =
            std::max(maximum_objective_reconciliation_error,
                metric.projection
                    .maximum_endpoint_objective_reconciliation_error);
        maximum_optimality_residual =
            std::max(maximum_optimality_residual,
                metric.projection.maximum_endpoint_optimality_residual);
    };
    for (const PolytopeReportMetric& metric : pool_metrics) {
        collect_residuals(metric);
    }
    for (const PolytopeReportMetric& metric : project_metrics) {
        collect_residuals(metric);
    }
    for (const PolytopeTailReportMetric& metric : tail_metrics) {
        maximum_constraint_violation = std::max(
            maximum_constraint_violation,
            metric.projection.maximum_endpoint_constraint_violation);
        maximum_tail_mass_violation = std::max(maximum_tail_mass_violation,
            metric.projection.maximum_endpoint_tail_mass_violation);
        maximum_objective_reconciliation_error = std::max(
            maximum_objective_reconciliation_error,
            metric.projection
                .maximum_endpoint_objective_reconciliation_error);
        maximum_threshold_formula_error = std::max(
            maximum_threshold_formula_error,
            metric.projection
                .maximum_endpoint_threshold_formula_reconciliation_error);
        maximum_optimality_residual = std::max(maximum_optimality_residual,
            metric.projection.maximum_endpoint_optimality_residual);
        maximum_threshold_enumeration_optimality_residual = std::max(
            maximum_threshold_enumeration_optimality_residual,
            metric.projection
                .maximum_threshold_enumeration_optimality_residual);
    }

    std::cout << "Solver audit controls\n"
              << "  maximum endpoint constraint violation: "
              << std::scientific << maximum_constraint_violation << '\n'
              << "  maximum endpoint objective reconciliation error: "
              << maximum_objective_reconciliation_error
              << " (monetary millions or raw probability units)\n"
              << "  maximum endpoint simplex reduced-cost residual: "
              << maximum_optimality_residual << '\n'
              << "  maximum tail-mass violation: "
              << maximum_tail_mass_violation << '\n'
              << "  maximum tail threshold-formula reconciliation error: "
              << maximum_threshold_formula_error << '\n'
              << "  maximum minimum-threshold enumeration reduced-cost "
                 "residual: "
              << maximum_threshold_enumeration_optimality_residual
              << std::fixed << "\n\n";

    std::cout
        << "Interpretation and completeness boundary\n"
        << "  Every metric endpoint has a probability witness that passes the "
           "disclosed numerical feasibility tolerance; endpoints from "
           "different rows need not be jointly attainable.\n"
        << "  A chosen witness may be nonunique; deterministic solver choices "
           "do not establish mathematical or economic uniqueness.\n"
        << "  This is physical-P sensitivity, not a price, fair value, "
           "risk-neutral value, or investable quote.\n"
        << "  ES95/99 use their own full probability and fractional-tail "
           "witnesses; they are not inferred from expectation endpoints.\n"
        << "  The same event candidate set can enter the fully funded stack "
           "through naturalehia-capital-stack --event-polytope, where every "
           "tranche tail and WAL is re-solved on its own fixed payoff path.\n"
        << "  Event membership is the explicit declared scenario set; it is "
           "not inferred from factor tags, project labels, or outcomes.\n"
        << "  calibrated_execution_authorized=false\n";
}

} // namespace

int main(int argc, char* argv[]) {
    const bool event_polytope_mode =
        argc >= 2 && std::string_view(argv[1]) == "--event-polytope";
    if (event_polytope_mode) {
        if (argc < 4 || argc > 5 ||
            std::string_view(argv[2]).starts_with("--") ||
            std::string_view(argv[3]).starts_with("--")) {
            print_usage(argv[0]);
            return 2;
        }
        const bool print_normalized = argc == 5;
        if (print_normalized &&
            std::string_view(argv[4]) != "--print-normalized") {
            print_usage(argv[0]);
            return 2;
        }

        try {
            const cf::PortfolioConfig portfolio =
                cf::load_portfolio_config(std::filesystem::path(argv[2]));
            const cf::ProbabilityPolytopeConfig probability_polytope =
                cf::load_probability_polytope_config(
                    std::filesystem::path(argv[3]));
            const cf::PortfolioSummary fixed_paths =
                cf::evaluate_portfolio(portfolio);
            const cf::ProbabilityPolytopeProjector projector(
                portfolio, probability_polytope);
            print_event_polytope_report(
                portfolio, probability_polytope, projector, fixed_paths);

            if (print_normalized) {
                std::cout << "\nNormalized portfolio configuration\n";
                std::cout << std::defaultfloat
                          << std::setprecision(
                                 std::numeric_limits<double>::max_digits10);
                cf::print_normalized_portfolio_config(std::cout, portfolio);
                std::cout
                    << "\nNormalized event-probability-polytope "
                       "configuration\n";
                cf::print_normalized_probability_polytope_config(
                    std::cout, probability_polytope);
            }
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "event-probability-polytope analysis failed: "
                      << error.what() << '\n';
            return 1;
        }
    }

    if (argc < 3 || argc > 4 ||
        std::string_view(argv[1]).starts_with("--") ||
        std::string_view(argv[2]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 4;
    if (print_normalized &&
        std::string_view(argv[3]) != "--print-normalized") {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const cf::PortfolioConfig portfolio =
            cf::load_portfolio_config(std::filesystem::path(argv[1]));
        const cf::PortfolioAmbiguityConfig ambiguity =
            cf::load_portfolio_ambiguity_config(
                std::filesystem::path(argv[2]));
        const cf::PortfolioAmbiguitySummary summary =
            cf::evaluate_portfolio_ambiguity(portfolio, ambiguity);
        print_report(portfolio, ambiguity, summary);

        if (print_normalized) {
            std::cout << "\nNormalized portfolio configuration\n";
            std::cout << std::defaultfloat
                      << std::setprecision(
                             std::numeric_limits<double>::max_digits10);
            cf::print_normalized_portfolio_config(std::cout, portfolio);
            std::cout << "\nNormalized probability-envelope configuration\n";
            cf::print_normalized_portfolio_ambiguity_config(
                std::cout, ambiguity);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "probability-envelope analysis failed: " << error.what()
                  << '\n';
        return 1;
    }
}
