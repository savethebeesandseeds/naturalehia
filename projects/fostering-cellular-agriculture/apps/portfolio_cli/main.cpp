// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(const char* program) {
    std::cerr << "usage: " << program
              << " <portfolio.cfg> [--print-normalized]\n";
}

[[nodiscard]] double aggregate_commitment(const cf::PortfolioConfig& config) {
    long double total = 0.0L;
    for (const cf::PortfolioProject& project : config.projects) {
        total += static_cast<long double>(project.commitment_million);
    }
    return static_cast<double>(total);
}

[[nodiscard]] const cf::PortfolioProject& find_project(
    const cf::PortfolioConfig& config, std::string_view project_id) {
    const auto match = std::find_if(config.projects.begin(),
        config.projects.end(), [project_id](const cf::PortfolioProject& project) {
            return project.id == project_id;
        });
    if (match == config.projects.end()) {
        throw std::logic_error("portfolio result names an unknown project");
    }
    return *match;
}

void print_probability(std::string_view label, double probability) {
    std::cout << "  " << label << ": " << probability * 100.0 << "%\n";
}

void print_money_distribution(std::string_view label,
    const cf::PortfolioDistributionSummary& distribution,
    std::string_view currency, bool include_expected_shortfall = false) {
    std::cout << "  " << label << " (" << currency << " million)\n"
              << "    mean=" << distribution.mean
              << " p50=" << distribution.p50
              << " p95=" << distribution.p95
              << " p99=" << distribution.p99
              << " max=" << distribution.maximum << '\n';
    if (include_expected_shortfall) {
        std::cout << "    ES95=" << distribution.expected_shortfall_95
                  << " ES99=" << distribution.expected_shortfall_99 << '\n';
    }
}

void print_ratio(std::string_view label, const std::optional<double>& ratio) {
    std::cout << label;
    if (ratio.has_value()) {
        std::cout << *ratio * 100.0 << '%';
    } else {
        std::cout << "undefined";
    }
}

void print_factor_tags(const std::vector<std::string>& tags) {
    if (tags.empty()) {
        std::cout << "none";
        return;
    }
    for (std::size_t index = 0U; index < tags.size(); ++index) {
        if (index != 0U) {
            std::cout << ',';
        }
        std::cout << tags[index];
    }
}

void print_report(
    const cf::PortfolioConfig& config, const cf::PortfolioSummary& summary) {
    const std::string_view currency = config.currency_label;

    std::cout << std::fixed << std::setprecision(6)
              << "SYNTHETIC PARTICIPATION-POOL ANALYSIS\n"
              << "MULTI-PROJECT JOINT-SCENARIO ENGINE v"
              << cf::kPortfolioModelVersion << "\n"
              << "Not a price, fair value, term sheet, offering document, "
                 "investment recommendation, or claim of validated performance.\n"
              << "The engine evaluates only the declared synthetic joint "
                 "scenarios; it does not infer independence or calibrate "
                 "market probabilities.\n\n";

    std::cout << "Scenario and analysis basis\n"
              << "  label: " << config.scenario_label << '\n'
              << "  source boundary: " << config.source_note << '\n'
              << "  monetary basis: " << config.currency_label << ", "
              << config.monetary_basis << '\n'
              << "  horizon: " << config.horizon_months << " months\n"
              << "  projects: " << config.projects.size() << '\n'
              << "  declared joint scenarios: "
              << config.joint_scenarios.size() << '\n'
              << "  annual physical-P hurdle: "
              << config.annual_physical_hurdle_rate * 100.0 << "%\n"
              << "  declared scenario-weight sum before near-one normalization: "
              << std::defaultfloat
              << std::setprecision(std::numeric_limits<double>::max_digits10)
              << summary.configured_scenario_weight_sum
              << std::fixed << std::setprecision(6) << "\n\n";

    std::cout << "Portfolio economics\n"
              << "  portfolio commitment: " << aggregate_commitment(config)
              << ' ' << currency << " million\n"
              << "  expected draws: " << summary.total_draws_million.mean
              << ' ' << currency << " million\n"
              << "  expected outstanding principal at horizon: "
              << summary.outstanding_principal_million.mean << ' ' << currency
              << " million\n"
              << "  expected realized principal loss: "
              << summary.principal_loss_million.mean << ' ' << currency
              << " million\n";
    print_probability("probability of any realized principal impairment",
        summary.principal_impairment_probability);
    std::cout << "  pool principal-loss ES95: "
              << summary.principal_loss_million.expected_shortfall_95 << ' '
              << currency << " million\n"
              << "  pool principal-loss ES99: "
              << summary.principal_loss_million.expected_shortfall_99 << ' '
              << currency << " million\n"
              << "  expected NPV at declared physical-P hurdle: "
              << summary.npv_million.mean << ' ' << currency << " million\n";
    print_probability(
        "probability of negative NPV", summary.negative_npv_probability);
    std::cout << '\n';

    std::cout << "Loss and NPV distributions\n";
    print_money_distribution("realized principal loss",
        summary.principal_loss_million, currency, true);
    print_money_distribution(
        "outstanding principal at horizon",
        summary.outstanding_principal_million, currency, true);
    print_money_distribution("NPV", summary.npv_million, currency);
    print_money_distribution(
        "non-negative NPV shortfall", summary.npv_shortfall_million, currency,
        true);
    std::cout << '\n';

    std::cout << "Liquidity distributions\n";
    print_money_distribution(
        "same-month project draws", summary.peak_same_month_draw_million,
        currency);
    print_money_distribution("same-month gross funding need before receipts",
        summary.peak_same_month_funding_need_million, currency);
    print_money_distribution("cumulative gross-before-receipt net outlay",
        summary.peak_cumulative_net_outlay_million, currency);
    std::cout << '\n';

    std::cout << "Expected investor receipts by declared external source\n"
              << "  source | nominal receipt | present value at declared hurdle\n";
    for (const cf::ReturnSourceTotal& source :
        summary.expected_return_sources) {
        std::cout << "  " << cf::to_string(source.source) << " | "
                  << source.nominal_million << " | "
                  << source.present_value_million << '\n';
    }
    std::cout << "  Principal components are memo classifications inside these "
                 "receipts, not additional cash.\n\n";

    std::cout << "Tail diversification\n"
              << "  confidence | sum standalone ES | pool ES | benefit | ratio\n"
              << "  ES95 | " << summary.sum_standalone_es95_million << " | "
              << summary.principal_loss_million.expected_shortfall_95 << " | "
              << summary.diversification_benefit_es95_million << " | ";
    print_ratio("", summary.diversification_ratio_es95);
    std::cout << '\n'
              << "  ES99 | " << summary.sum_standalone_es99_million << " | "
              << summary.principal_loss_million.expected_shortfall_99 << " | "
              << summary.diversification_benefit_es99_million << " | ";
    print_ratio("", summary.diversification_ratio_es99);
    std::cout << "\n  Pooling changes the declared tail distribution; it does not "
                 "reduce expected loss or create project cash.\n\n";

    std::cout << "Project marginals\n"
              << "  project | stage | commitment | expected draws | expected "
                 "receipts | expected outstanding | expected realized loss | impairment | expected "
                 "NPV before pool costs | negative NPV before pool costs | "
                 "standalone ES95 | pool-tail contribution ES95\n";
    for (const cf::ProjectPortfolioSummary& project : summary.projects) {
        const cf::PortfolioProject& definition =
            find_project(config, project.project_id);
        std::cout << "  " << project.project_id << " | "
                  << cf::to_string(definition.stage) << " | "
                  << definition.commitment_million << " | "
                  << project.expected_draws_million << " | "
                  << project.expected_receipts_million << " | "
                  << project.expected_outstanding_principal_million << " | "
                  << project.expected_principal_loss_million << " | "
                  << project.principal_impairment_probability * 100.0 << "% | "
                  << project.expected_npv_before_pool_costs_million << " | "
                  << project.negative_npv_probability * 100.0 << "% | "
                  << project.principal_loss_million.expected_shortfall_95
                  << " | "
                  << project.pool_loss_tail_contribution_es95_million << '\n';
    }
    std::cout << '\n';

    std::cout << "Pairwise realized-loss correlations\n"
              << "  first project | second project | correlation\n";
    if (summary.pairwise_loss_correlations.empty()) {
        std::cout << "  none: fewer than two projects\n";
    }
    for (const cf::PairwiseLossCorrelation& correlation :
        summary.pairwise_loss_correlations) {
        std::cout << "  " << correlation.first_project_id << " | "
                  << correlation.second_project_id << " | ";
        if (correlation.correlation.has_value()) {
            std::cout << *correlation.correlation;
        } else {
            std::cout << "undefined (at least one loss series is constant)";
        }
        std::cout << '\n';
    }
    std::cout << '\n';

    std::cout << "Optional loss layers\n";
    if (summary.layers.empty()) {
        std::cout << "  none configured\n";
    } else {
        std::cout << "  layer | attachment | detachment | expected loss | "
                     "impairment | exhaustion | ES95 | ES99\n";
        for (const cf::LayerPortfolioSummary& layer : summary.layers) {
            std::cout << "  " << layer.layer_id << " | "
                      << layer.attachment_million << " | "
                      << layer.detachment_million << " | "
                      << layer.expected_loss_million << " | "
                      << layer.impairment_probability * 100.0 << "% | "
                      << layer.exhaustion_probability * 100.0 << "% | "
                      << layer.principal_loss_million.expected_shortfall_95
                      << " | "
                      << layer.principal_loss_million.expected_shortfall_99
                      << '\n';
        }
    }
    std::cout << '\n';

    std::cout << "Declared joint-scenario ledger\n"
              << "  scenario | declared weight | normalized weight | draws | "
                 "receipts | pool costs | outstanding | realized loss | NPV | "
                 "peak gross funding need | peak cumulative outlay | factors\n";
    for (const cf::JointScenarioResult& scenario : summary.scenarios) {
        std::cout << "  " << scenario.scenario_id << " | "
                  << scenario.declared_weight << " | "
                  << scenario.normalized_weight << " | "
                  << scenario.total_draws_million << " | "
                  << scenario.total_receipts_million << " | "
                  << scenario.total_pool_costs_million << " | "
                  << scenario.outstanding_principal_million << " | "
                  << scenario.principal_loss_million << " | "
                  << scenario.npv_million << " | "
                  << scenario.peak_same_month_funding_need_million << " | "
                  << scenario.peak_cumulative_net_outlay_million << " | ";
        print_factor_tags(scenario.factor_tags);
        std::cout << '\n'
                  << "    project | resolution | draws | receipts | principal "
                     "returned | outstanding | realized loss | NPV before pool "
                     "costs\n";
        for (const cf::ProjectPathResult& project : scenario.projects) {
            std::cout << "    " << project.project_id << " | "
                      << cf::to_string(project.resolution) << " | "
                      << project.total_draws_million << " | "
                      << project.total_receipts_million << " | "
                      << project.principal_returned_million << " | "
                      << project.outstanding_principal_million << " | "
                      << project.principal_loss_million << " | "
                      << project.npv_before_pool_costs_million << '\n';
        }
    }

    std::cout << "\nReconciliation controls\n"
              << "  maximum cash reconciliation error: "
              << summary.maximum_cash_reconciliation_error_million << ' '
              << currency << " million\n"
              << "  maximum loss-layer reconciliation error: "
              << summary.maximum_layer_reconciliation_error_million << ' '
              << currency << " million\n\n";

    std::cout << "Interpretation boundary\n"
              << "  Expected NPV is a physical-P sensitivity using analyst-declared "
                 "joint weights and hurdle. It is not risk-neutral value, fair "
                 "value, a market price, or an investable quote.\n"
              << "  Outstanding principal belongs to continuing paths and is exposure "
                 "at the horizon, not realized loss. Realized principal loss is "
                 "reported only for paths declared resolved.\n"
              << "  External-source labels preserve where receipts are assumed to "
                 "come from; they do not establish collectability, additionality, "
                 "or value creation. Refinancing is new financing liquidity.\n"
              << "  Correlation and diversification come only from the supplied joint "
                 "scenarios. Packaging and loss layers redistribute modeled loss; "
                 "they do not improve project performance or create cash.\n"
              << "  This synthetic analysis is not an offering and is not suitable "
                 "for pricing or marketing a real financial instrument.\n";
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
        const cf::PortfolioConfig config =
            cf::load_portfolio_config(std::filesystem::path(argv[1]));
        const cf::PortfolioSummary summary = cf::evaluate_portfolio(config);
        print_report(config, summary);

        if (print_normalized) {
            std::cout << "\nNormalized configuration\n";
            std::cout << std::defaultfloat
                      << std::setprecision(
                             std::numeric_limits<double>::max_digits10);
            cf::print_normalized_portfolio_config(std::cout, config);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "participation-pool analysis failed: " << error.what()
                  << '\n';
        return 1;
    }
}
