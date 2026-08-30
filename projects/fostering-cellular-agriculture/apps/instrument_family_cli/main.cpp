// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/capital_stack.hpp>
#include <naturalehia/cellular_finance/capital_stack_config.hpp>
#include <naturalehia/cellular_finance/pooled_loss_protection.hpp>
#include <naturalehia/cellular_finance/pooled_loss_protection_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/provider_credit_stress.hpp>
#include <naturalehia/cellular_finance/provider_credit_stress_config.hpp>
#include <naturalehia/cellular_finance/provider_price_ladder.hpp>
#include <naturalehia/cellular_finance/provider_price_ladder_config.hpp>
#include <naturalehia/cellular_finance/success_participation.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cf = naturalehia::cellular_finance;

namespace {

inline constexpr std::size_t kRequiredClaimCount{10U};
inline constexpr std::size_t kInstrumentVariantCount{2U};
inline constexpr double kRequiredProtectionFraction{0.30};
inline constexpr double kComparisonTolerance{1.0e-12};

static_assert(kInstrumentVariantCount <= 2U);

inline constexpr std::array<std::string_view, 5U> kRequiredRiskFactors{
    "biological-process",
    "scale-up-commissioning",
    "supplier-media",
    "regulatory-qualification",
    "buyer-product-acceptance",
};
inline constexpr std::string_view kNeutralRiskTag{
    "no-declared-adverse-factor"};

struct LoadedInputs {
    cf::PortfolioConfig portfolio{};
    cf::PortfolioAmbiguityConfig ambiguity{};
    cf::SuccessParticipationConfig participation{};
    cf::CapitalStackConfig stack{};
    cf::PooledLossProtectionConfig protection{};
    cf::ProviderPriceLadderConfig pricing{};
    cf::ProviderCreditStressConfig credit{};
};

struct EvaluatedFamily {
    cf::PortfolioAmbiguitySummary core{};
    cf::CapitalStackSummary funded{};
    cf::PooledLossProtectionSummary protection{};
    cf::ProviderCreditStressSummary credit{};
    cf::AmbiguityMetricRange core_all_in_contributions{};
    cf::AmbiguityMetricRange core_principal_cash{};
    cf::AmbiguityMetricRange core_nonprincipal_cash{};
    cf::AmbiguityMetricRange core_nominal_net_cash{};
    cf::AmbiguityMetricRange supported_cash{};
    cf::AmbiguityMetricRange supported_total_receipts{};
    cf::AmbiguityMetricRange supported_nominal_net_cash{};
    cf::AmbiguityMetricRange supported_residual_loss{};
    cf::AmbiguityMetricRange supported_residual_loss_es95{};
    cf::AmbiguityMetricRange supported_residual_loss_es99{};
    cf::AmbiguityMetricRange supported_npv_shortfall{};
    cf::AmbiguityMetricRange supported_npv_shortfall_es95{};
    cf::AmbiguityMetricRange supported_npv_shortfall_es99{};
};

struct DependenceSummary {
    std::size_t total_pairs{0U};
    std::size_t defined_pairs{0U};
    double minimum{0.0};
    double mean{0.0};
    double maximum{0.0};
    std::string minimum_pair{};
    std::string maximum_pair{};
};

void print_usage(std::string_view program) {
    std::cerr
        << "usage: " << program
        << " <portfolio.cfg> <ambiguity.cfg> <success-participation.cfg> "
           "<capital-stack.cfg> <loss-protection.cfg> <provider-price.cfg> "
           "<provider-credit.cfg>\n";
}

[[nodiscard]] bool scenario_has_factor(
    const cf::JointScenarioResult& scenario, std::string_view factor) {
    return std::any_of(scenario.factor_tags.begin(), scenario.factor_tags.end(),
        [factor](const std::string& tag) {
            return std::string_view(tag) == factor;
        });
}

template <typename Scenario, typename ValueFunction>
[[nodiscard]] std::vector<cf::AmbiguityScenarioMetricValue>
make_scenario_values(
    const std::vector<Scenario>& scenarios, ValueFunction value_function) {
    std::vector<cf::AmbiguityScenarioMetricValue> values;
    values.reserve(scenarios.size());
    for (const Scenario& scenario : scenarios) {
        values.push_back(cf::AmbiguityScenarioMetricValue{
            scenario.scenario_id, value_function(scenario)});
    }
    return values;
}

[[nodiscard]] const cf::ProjectAmbiguitySummary& find_project_range(
    const cf::PortfolioAmbiguitySummary& summary,
    std::string_view project_id) {
    const auto match = std::find_if(summary.projects.begin(),
        summary.projects.end(), [project_id](const auto& project) {
            return project.project_id == project_id;
        });
    if (match == summary.projects.end()) {
        throw std::logic_error(
            "instrument-family result omits a configured project range");
    }
    return *match;
}

[[nodiscard]] const cf::ProjectPortfolioSummary& find_central_project(
    const cf::PortfolioAmbiguitySummary& summary,
    std::string_view project_id) {
    const auto match = std::find_if(summary.central_portfolio.projects.begin(),
        summary.central_portfolio.projects.end(),
        [project_id](const auto& project) {
            return project.project_id == project_id;
        });
    if (match == summary.central_portfolio.projects.end()) {
        throw std::logic_error(
            "instrument-family result omits a configured central project");
    }
    return *match;
}

[[nodiscard]] const cf::JointScenarioResult& find_core_scenario(
    const cf::PortfolioAmbiguitySummary& summary,
    std::string_view scenario_id) {
    const auto match = std::find_if(summary.central_portfolio.scenarios.begin(),
        summary.central_portfolio.scenarios.end(),
        [scenario_id](const cf::JointScenarioResult& scenario) {
            return scenario.scenario_id == scenario_id;
        });
    if (match == summary.central_portfolio.scenarios.end()) {
        throw std::logic_error(
            "protection result names an unknown core scenario");
    }
    return *match;
}

[[nodiscard]] DependenceSummary summarize_dependence(
    const cf::PortfolioSummary& portfolio) {
    DependenceSummary result;
    result.total_pairs = portfolio.pairwise_loss_correlations.size();
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    long double sum = 0.0L;

    for (const cf::PairwiseLossCorrelation& pair :
         portfolio.pairwise_loss_correlations) {
        if (!pair.correlation.has_value()) {
            continue;
        }
        const double value = *pair.correlation;
        ++result.defined_pairs;
        sum += static_cast<long double>(value);
        if (value < minimum) {
            minimum = value;
            result.minimum_pair =
                pair.first_project_id + " / " + pair.second_project_id;
        }
        if (value > maximum) {
            maximum = value;
            result.maximum_pair =
                pair.first_project_id + " / " + pair.second_project_id;
        }
    }

    if (result.defined_pairs != 0U) {
        result.minimum = minimum;
        result.maximum = maximum;
        result.mean = static_cast<double>(
            sum / static_cast<long double>(result.defined_pairs));
    }
    return result;
}

[[nodiscard]] double maximum_stack_reconciliation_error(
    const cf::CapitalStackSummary& summary) {
    return std::max({summary.maximum_commitment_identity_error_million,
        summary.maximum_reserve_roll_forward_error_million,
        summary.maximum_reserve_shortfall_million,
        summary.maximum_subscription_reconciliation_error_million,
        summary.maximum_pool_cost_call_reconciliation_error_million,
        summary.maximum_principal_distribution_reconciliation_error_million,
        summary.maximum_nonprincipal_distribution_reconciliation_error_million,
        summary.maximum_priority_nonprincipal_cap_violation_million,
        summary.maximum_realized_loss_reconciliation_error_million,
        summary.maximum_unresolved_exposure_reconciliation_error_million,
        summary.maximum_nominal_net_cash_reconciliation_error_million,
        summary.maximum_stack_npv_reconciliation_error_million});
}

[[nodiscard]] double maximum_protection_reconciliation_error(
    const cf::PooledLossProtectionSummary& summary) {
    return std::max({summary.maximum_underlying_loss_change_million,
        summary.maximum_project_claim_reconciliation_error_million,
        summary.maximum_two_party_settlement_cash_reconciliation_error_million,
        summary.maximum_support_cap_violation_million,
        summary.maximum_combined_npv_reconstruction_error_million,
        summary.maximum_witness_reconciliation_error_million});
}

[[nodiscard]] double maximum_price_reconciliation_error(
    const cf::ProviderPriceLadderSummary& summary) {
    return std::max({summary.maximum_cost_ladder_reconciliation_error_million,
        summary.support_gap_decomposition_reconciliation_error_million,
        summary.maximum_transformed_range_reconciliation_error_million});
}

[[nodiscard]] double maximum_credit_reconciliation_error(
    const cf::ProviderCreditStressSummary& summary) {
    return std::max({summary.maximum_gross_project_loss_change_million,
        summary.maximum_default_waterfall_reconciliation_error_million,
        summary.maximum_credit_loss_reconciliation_error_million,
        summary.maximum_conditional_collapse_reconciliation_error_million,
        summary.maximum_central_monetary_projection_reconciliation_error_million,
        summary.support_gap_decomposition_reconciliation_error_million,
        summary.maximum_monetary_witness_reconciliation_error_million});
}

void require_synthetic_inputs(const LoadedInputs& inputs) {
    if (!inputs.portfolio.synthetic_inputs ||
        !inputs.ambiguity.synthetic_inputs ||
        !inputs.participation.synthetic_inputs ||
        !inputs.stack.synthetic_inputs ||
        !inputs.protection.synthetic_inputs ||
        !inputs.pricing.synthetic_inputs || !inputs.credit.synthetic_inputs) {
        throw std::invalid_argument(
            "this comparison reporter requires every input to be explicitly "
            "marked synthetic");
    }
}

void validate_family_inputs(const LoadedInputs& inputs) {
    require_synthetic_inputs(inputs);
    if (inputs.portfolio.projects.size() != kRequiredClaimCount) {
        throw std::invalid_argument(
            "instrument family requires exactly ten project claims");
    }
    if (inputs.stack.tranches.size() != 2U) {
        throw std::invalid_argument(
            "funded variant requires exactly two tranches");
    }
    if (inputs.stack.underlying_success_participation_fraction != 1.0 ||
        inputs.protection.underlying_success_participation_fraction != 1.0) {
        throw std::invalid_argument(
            "both variants must retain the core claim's q=1 cash rights");
    }
    if (inputs.pricing.coverage_selection !=
            cf::ProviderPriceCoverageSelection::ExplicitCoverageFraction ||
        !inputs.pricing.explicit_coverage_fraction.has_value() ||
        *inputs.pricing.explicit_coverage_fraction !=
            kRequiredProtectionFraction) {
        throw std::invalid_argument(
            "protection variant requires an explicit 30 percent coverage "
            "fraction");
    }

    for (const cf::JointScenario& scenario :
         inputs.portfolio.joint_scenarios) {
        for (const std::string& tag : scenario.factor_tags) {
            const bool is_required_adverse =
                std::find(kRequiredRiskFactors.begin(),
                    kRequiredRiskFactors.end(), std::string_view(tag)) !=
                kRequiredRiskFactors.end();
            if (!is_required_adverse &&
                std::string_view(tag) != kNeutralRiskTag) {
                throw std::invalid_argument(
                    "portfolio contains an unrecognized shared-risk tag: " +
                    tag);
            }
        }
        for (const cf::ScenarioCashSource& source : scenario.cash_sources) {
            if (source.kind != cf::PortfolioCashSource::ExplicitSupport) {
                continue;
            }
            const bool has_support = std::any_of(source.cash_available.begin(),
                source.cash_available.end(), [](const cf::MonthlyAmount& cash) {
                    return cash.amount_million != 0.0;
                });
            if (has_support) {
                throw std::invalid_argument(
                    "unsupported core cannot contain explicit-support cash");
            }
        }
    }

    for (const std::string_view factor : kRequiredRiskFactors) {
        const bool present = std::any_of(inputs.portfolio.joint_scenarios.begin(),
            inputs.portfolio.joint_scenarios.end(),
            [factor](const cf::JointScenario& scenario) {
                return std::any_of(scenario.factor_tags.begin(),
                    scenario.factor_tags.end(),
                    [factor](const std::string& tag) {
                        return std::string_view(tag) == factor;
                    });
            });
        if (!present) {
            throw std::invalid_argument(
                "portfolio omits required shared-risk factor: " +
                std::string(factor));
        }
    }
}

[[nodiscard]] LoadedInputs load_inputs(char* argv[]) {
    LoadedInputs inputs;
    inputs.portfolio =
        cf::load_portfolio_config(std::filesystem::path(argv[1]));
    inputs.ambiguity =
        cf::load_portfolio_ambiguity_config(std::filesystem::path(argv[2]));
    inputs.participation =
        cf::load_success_participation_config(std::filesystem::path(argv[3]));
    inputs.stack =
        cf::load_capital_stack_config(std::filesystem::path(argv[4]));
    inputs.protection = cf::load_pooled_loss_protection_config(
        std::filesystem::path(argv[5]));
    inputs.pricing =
        cf::load_provider_price_ladder_config(std::filesystem::path(argv[6]));
    inputs.credit =
        cf::load_provider_credit_stress_config(std::filesystem::path(argv[7]));
    return inputs;
}

[[nodiscard]] EvaluatedFamily evaluate_family(const LoadedInputs& inputs) {
    EvaluatedFamily result;
    result.core =
        cf::evaluate_portfolio_ambiguity(inputs.portfolio, inputs.ambiguity);
    result.funded = cf::evaluate_capital_stack(inputs.portfolio,
        inputs.ambiguity, inputs.participation, inputs.stack);
    result.protection = cf::solve_pooled_loss_protection(inputs.portfolio,
        inputs.ambiguity, inputs.participation, inputs.protection);
    result.credit = cf::solve_provider_credit_stress(inputs.portfolio,
        inputs.ambiguity, inputs.participation, inputs.protection,
        inputs.pricing, inputs.credit);

    if (result.protection.reported_coverage_fraction !=
            kRequiredProtectionFraction ||
        result.credit.selected_coverage_fraction !=
            kRequiredProtectionFraction) {
        throw std::invalid_argument(
            "the supplied cap does not support the required exact 30 percent "
            "comparison point");
    }

    const cf::PortfolioAmbiguityProjector projector(
        inputs.portfolio, inputs.ambiguity);
    const auto core_contributions = make_scenario_values(
        result.core.central_portfolio.scenarios,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.total_investor_outlays_million +
                   scenario.total_pool_costs_million;
        });
    result.core_all_in_contributions =
        projector.project_expectation(core_contributions).expectation;

    const auto core_principal_cash = make_scenario_values(
        result.core.central_portfolio.scenarios,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.principal_returned_million;
        });
    result.core_principal_cash =
        projector.project_expectation(core_principal_cash).expectation;

    const auto core_nonprincipal_cash = make_scenario_values(
        result.core.central_portfolio.scenarios,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.total_receipts_million -
                   scenario.principal_returned_million;
        });
    result.core_nonprincipal_cash =
        projector.project_expectation(core_nonprincipal_cash).expectation;

    const auto core_net_cash = make_scenario_values(
        result.core.central_portfolio.scenarios,
        [](const cf::JointScenarioResult& scenario) {
            return scenario.total_receipts_million -
                   scenario.total_investor_outlays_million -
                   scenario.total_pool_costs_million;
        });
    result.core_nominal_net_cash =
        projector.project_expectation(core_net_cash).expectation;

    const auto supported_cash = make_scenario_values(
        result.protection.scenarios,
        [](const cf::PooledLossProtectionScenarioResult& scenario) {
            return scenario.investor_external_support_cash_million;
        });
    result.supported_cash =
        projector.project_expectation(supported_cash).expectation;

    const auto supported_total_receipts = make_scenario_values(
        result.protection.scenarios,
        [&result](const cf::PooledLossProtectionScenarioResult& scenario) {
            const cf::JointScenarioResult& core_scenario =
                find_core_scenario(result.core, scenario.scenario_id);
            return core_scenario.total_receipts_million +
                   scenario.investor_external_support_cash_million;
        });
    result.supported_total_receipts =
        projector.project_expectation(supported_total_receipts).expectation;

    const auto supported_nominal_net_cash = make_scenario_values(
        result.protection.scenarios,
        [&result](const cf::PooledLossProtectionScenarioResult& scenario) {
            const cf::JointScenarioResult& core_scenario =
                find_core_scenario(result.core, scenario.scenario_id);
            return core_scenario.total_receipts_million +
                   scenario.investor_external_support_cash_million -
                   core_scenario.total_investor_outlays_million -
                   core_scenario.total_pool_costs_million;
        });
    result.supported_nominal_net_cash =
        projector.project_expectation(supported_nominal_net_cash).expectation;

    const auto residual_loss = make_scenario_values(
        result.protection.scenarios,
        [](const cf::PooledLossProtectionScenarioResult& scenario) {
            return scenario.residual_unprotected_loss_million;
        });
    result.supported_residual_loss =
        projector.project_expectation(residual_loss).expectation;
    result.supported_residual_loss_es95 =
        projector.project_upper_expected_shortfall(residual_loss, 0.05)
            .upper_expected_shortfall;
    result.supported_residual_loss_es99 =
        projector.project_upper_expected_shortfall(residual_loss, 0.01)
            .upper_expected_shortfall;

    const auto supported_npv_shortfall = make_scenario_values(
        result.protection.scenarios,
        [](const cf::PooledLossProtectionScenarioResult& scenario) {
            return std::max(
                0.0, -scenario.investor_npv_before_premium_million);
        });
    result.supported_npv_shortfall =
        projector.project_expectation(supported_npv_shortfall).expectation;
    result.supported_npv_shortfall_es95 =
        projector
            .project_upper_expected_shortfall(supported_npv_shortfall, 0.05)
            .upper_expected_shortfall;
    result.supported_npv_shortfall_es99 =
        projector
            .project_upper_expected_shortfall(supported_npv_shortfall, 0.01)
            .upper_expected_shortfall;
    return result;
}

void print_range(std::string_view label, const cf::AmbiguityMetricRange& range,
    std::string_view unit, double scale = 1.0) {
    std::cout << "  " << label << " | " << range.minimum.value * scale
              << " | " << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_compact_range(
    const cf::AmbiguityMetricRange& range, double scale = 1.0) {
    std::cout << range.minimum.value * scale << '/' << range.central * scale
              << '/' << range.maximum.value * scale;
}

void print_optional_money(std::string_view label,
    const std::optional<double>& value, std::string_view currency) {
    std::cout << "  " << label << ": ";
    if (value.has_value()) {
        std::cout << *value << ' ' << currency << " million\n";
    } else {
        std::cout << "none\n";
    }
}

void print_core_receipt_sources(const cf::PortfolioAmbiguitySummary& core,
    std::string_view currency) {
    std::cout << "\nCore receipt sources (nominal expectation ranges)\n"
              << "  source | minimum | central | maximum | unit\n";
    for (const cf::AmbiguityReturnSourceTotal& source :
         core.expected_return_sources) {
        if (std::abs(source.nominal_million.minimum.value) <=
                kComparisonTolerance &&
            std::abs(source.nominal_million.central) <=
                kComparisonTolerance &&
            std::abs(source.nominal_million.maximum.value) <=
                kComparisonTolerance) {
            continue;
        }
        print_range(cf::to_string(source.source), source.nominal_million,
            std::string(currency) + " million");
    }
    std::cout << "  explicit external support in the unsupported core: "
                 "zero in every declared path\n"
              << "  Source kinds and principal/non-principal classification "
                 "are separate views of the same cash; do not add them.\n";
}

void print_project_concentration(const LoadedInputs& inputs,
    const cf::PortfolioAmbiguitySummary& core) {
    const double aggregate_reference_principal =
        cf::portfolio_aggregate_reference_principal(inputs.portfolio);
    std::cout
        << "\nTen-claim concentration table\n"
        << "  project | stage | reference principal | principal share | "
           "central expected loss | maximum expected loss | central pool-loss "
           "ES95 contribution\n";
    for (const cf::PortfolioProject& project : inputs.portfolio.projects) {
        const cf::ProjectAmbiguitySummary& range =
            find_project_range(core, project.id);
        const cf::ProjectPortfolioSummary& central =
            find_central_project(core, project.id);
        const double principal =
            cf::portfolio_reference_principal_limit(project);
        const double share =
            aggregate_reference_principal == 0.0
                ? 0.0
                : 100.0 * principal / aggregate_reference_principal;
        std::cout << "  " << project.id << " | "
                  << cf::to_string(project.stage) << " | " << principal
                  << " | " << share << "% | "
                  << range.expected_realized_principal_loss_million.central
                  << " | "
                  << range.expected_realized_principal_loss_million.maximum
                         .value
                  << " | "
                  << central.pool_loss_tail_contribution_es95_million << '\n';
    }
}

void print_dependence_and_diversification(
    const cf::PortfolioAmbiguitySummary& core) {
    const DependenceSummary dependence =
        summarize_dependence(core.central_portfolio);
    std::cout << "\nDependence and diversification at the declared central "
                 "physical measure\n"
              << "  pairwise loss correlations defined: "
              << dependence.defined_pairs << " of " << dependence.total_pairs
              << '\n';
    if (dependence.defined_pairs == 0U) {
        std::cout << "  correlation minimum / mean / maximum: undefined\n";
    } else {
        std::cout << "  correlation minimum / mean / maximum: "
                  << dependence.minimum << " / " << dependence.mean << " / "
                  << dependence.maximum << '\n'
                  << "  minimum-correlation pair: "
                  << dependence.minimum_pair << '\n'
                  << "  maximum-correlation pair: "
                  << dependence.maximum_pair << '\n';
    }

    const cf::PortfolioSummary& portfolio = core.central_portfolio;
    std::cout << "  tail | sum standalone ES | pool ES | diversification "
                 "benefit | ratio\n"
              << "  ES95 | " << portfolio.sum_standalone_es95_million
              << " | "
              << portfolio.principal_loss_million.expected_shortfall_95
              << " | " << portfolio.diversification_benefit_es95_million
              << " | ";
    if (portfolio.diversification_ratio_es95.has_value()) {
        std::cout << *portfolio.diversification_ratio_es95 * 100.0 << "%\n";
    } else {
        std::cout << "undefined\n";
    }
    std::cout << "  ES99 | " << portfolio.sum_standalone_es99_million << " | "
              << portfolio.principal_loss_million.expected_shortfall_99
              << " | " << portfolio.diversification_benefit_es99_million
              << " | ";
    if (portfolio.diversification_ratio_es99.has_value()) {
        std::cout << *portfolio.diversification_ratio_es99 * 100.0 << "%\n";
    } else {
        std::cout << "undefined\n";
    }
    std::cout << "  These statistics come only from the disclosed joint paths; "
                 "no independence assumption is introduced.\n";
}

void print_factor_ranges(const LoadedInputs& inputs,
    const cf::PortfolioAmbiguitySummary& core) {
    const cf::PortfolioAmbiguityProjector projector(
        inputs.portfolio, inputs.ambiguity);
    std::cout
        << "\nShared-risk event ranges\n"
        << "  factor | probability minimum | central | maximum | "
           "unconditional loss contribution minimum | central | maximum\n";
    for (const std::string_view factor : kRequiredRiskFactors) {
        const auto event_values = make_scenario_values(
            core.central_portfolio.scenarios,
            [factor](const cf::JointScenarioResult& scenario) {
                return scenario_has_factor(scenario, factor) ? 1.0 : 0.0;
            });
        const auto loss_values = make_scenario_values(
            core.central_portfolio.scenarios,
            [factor](const cf::JointScenarioResult& scenario) {
                return scenario_has_factor(scenario, factor)
                           ? scenario.principal_loss_million
                           : 0.0;
            });
        const cf::AmbiguityMetricRange probability =
            projector.project_expectation(event_values).expectation;
        const cf::AmbiguityMetricRange loss =
            projector.project_expectation(loss_values).expectation;
        std::cout << "  " << factor << " | "
                  << probability.minimum.value * 100.0 << " | "
                  << probability.central * 100.0 << " | "
                  << probability.maximum.value * 100.0 << " | "
                  << loss.minimum.value << " | " << loss.central << " | "
                  << loss.maximum.value << '\n';
    }
    std::cout << "  Loss contribution is unconditional E[pool loss x event]; "
                 "it is not loss conditional on the factor.\n";
}

void print_tranches(const cf::CapitalStackSummary& funded,
    std::string_view currency) {
    std::cout
        << "\nVARIANT 1 - FULLY FUNDED FIRST-LOSS / MARKET-PRIORITY CLAIM\n"
        << "  month-zero par subscription: "
        << funded.aggregate_commitment_million << ' ' << currency
        << " million\n"
        << "  table monetary fields: " << currency << " million\n"
        << "  underlying project cash changed: "
        << (funded.project_cash_is_changed_by_tranching ? "yes" : "no")
        << '\n'
        << "  gross project principal loss changed: "
        << (funded.gross_project_principal_loss_is_changed ? "yes" : "no")
        << '\n'
        << "  q=1 meets the declared robust pool NPV target: "
        << (funded.selected_underlying_success_participation_meets_target
                ? "yes"
                : "no")
        << '\n'
        << "  q=1 robust pool target shortfall: "
        << funded.selected_underlying_target_gap_million << ' ' << currency
        << " million\n";
    print_range("prefunding drag at the pool hurdle",
        funded.expected_prefunding_drag_npv_million,
        std::string(currency) + " million");
    std::cout
        << "  tranche | role | attach | detach | notional | central "
           "contributions | central distributions | central loss | "
           "impairment min/central/max (%) | NPV min/central/max | principal "
           "cash WAL min/central/max (years)\n";
    for (const cf::CapitalStackTrancheSummary& tranche : funded.tranches) {
        std::cout << "  " << tranche.tranche_id << " | "
                  << (tranche.is_first_loss_residual ? "first-loss residual"
                                                     : "priority")
                  << " | " << tranche.attachment_million << " | "
                  << tranche.detachment_million << " | "
                  << tranche.notional_million << " | "
                  << tranche.expected_contributions_million.central << " | "
                  << tranche.expected_total_distributions_million.central
                  << " | "
                  << tranche.expected_realized_principal_loss_million.central
                  << " | ";
        print_compact_range(tranche.principal_impairment_probability, 100.0);
        std::cout << " | ";
        print_compact_range(tranche.expected_npv_at_tranche_hurdle_million);
        std::cout << " | ";
        if (tranche.principal_cash_weighted_average_life_years.has_value()) {
            print_compact_range(
                *tranche.principal_cash_weighted_average_life_years);
        } else {
            std::cout << "undefined";
        }
        std::cout << '\n'
                  << "    distribution sources min/central/max - underlying "
                     "principal cash: ";
        print_compact_range(
            tranche.expected_underlying_principal_cash_distribution_million);
        std::cout << " million; non-principal success cash: ";
        print_compact_range(
            tranche.expected_nonprincipal_cash_distribution_million);
        std::cout << " million; returned unused funded reserve: ";
        print_compact_range(
            tranche.expected_unused_reserve_principal_return_million);
        std::cout << " million\n"
                  << "    total distributions min/central/max: ";
        print_compact_range(tranche.expected_total_distributions_million);
        std::cout << " million; realized principal loss min/central/max: ";
        print_compact_range(
            tranche.expected_realized_principal_loss_million);
        std::cout << " million\n"
                  << "    principal-loss ES95 min/central/max: ";
        print_compact_range(
            tranche.principal_loss_expected_shortfall_95_million);
        std::cout << " million; ES99: ";
        print_compact_range(
            tranche.principal_loss_expected_shortfall_99_million);
        std::cout << " million; unresolved exposure min/central/max: ";
        print_compact_range(
            tranche.expected_unresolved_principal_exposure_million);
        std::cout << " million\n"
                  << "    negative-NPV probability min/central/max: ";
        print_compact_range(tranche.negative_npv_probability, 100.0);
        std::cout << "% ; NPV-shortfall ES95 min/central/max: ";
        print_compact_range(
            tranche.npv_shortfall_expected_shortfall_95_million);
        std::cout << " million; ES99: ";
        print_compact_range(
            tranche.npv_shortfall_expected_shortfall_99_million);
        std::cout << " million\n";
    }
    std::cout
        << "  The first-loss subscription is funded cash. The waterfall "
           "redistributes actual pool cash and loss; it creates neither.\n"
        << "  Project/factor concentration, dependence, and gross-project "
           "diversification are unchanged from the core.\n"
        << "  Contingent provider commitment, payout, premium capacity, and "
           "provider floor are not applicable; the first-loss claim is "
           "funded investor capital.\n"
        << "  Each range endpoint has its own feasible witness; component "
           "endpoints must not be added to reconstruct a total endpoint.\n";
}

void print_protection(const LoadedInputs& inputs,
    const EvaluatedFamily& family) {
    const cf::PooledLossProtectionRobustPoint& point =
        family.credit.full_performance_price_ladder.selected_protection_point;
    const cf::PooledLossProtectionProviderRisk& provider = point.provider_risk;
    const cf::ProviderPriceLadderSummary& price =
        family.credit.full_performance_price_ladder;
    const cf::ProviderCreditStressSummary& credit = family.credit;
    const std::string_view currency = inputs.portfolio.currency_label;

    std::cout
        << "\nVARIANT 2 - THIRTY-PERCENT FAILURE-CONTINGENT PARTIAL CREDIT\n"
        << "  coverage fraction: " << point.coverage_fraction * 100.0 << "%\n"
        << "  settlement month: " << inputs.protection.settlement_month << '\n'
        << "  provider: " << inputs.protection.provider_id << '\n'
        << "  provider annual physical-P hurdle for claim-cost PV: "
        << inputs.protection.provider_annual_physical_hurdle_rate * 100.0
        << "%\n"
        << "  coverage solver status: "
        << cf::to_string(family.protection.status) << '\n'
        << "  investor target gap at 30% before premium: "
        << std::max(0.0,
               point.investor_target_worst_expected_npv_million -
                   point.investor_expected_npv_before_premium_million.minimum
                       .value)
        << ' ' << currency << " million\n"
        << "  contractual maximum provider exposure: "
        << provider.contractual_maximum_exposure_million << ' ' << currency
        << " million\n"
        << "  modeled maximum claim: "
        << provider.modeled_maximum_claim_million << ' ' << currency
        << " million\n"
        << "  Full-provider-performance economics before investor premium\n"
        << "  metric | minimum | central | maximum | unit\n";
    print_range("full-performance external support cash",
        family.supported_cash, std::string(currency) + " million");
    print_range("total investor cash receipts including external support",
        family.supported_total_receipts,
        std::string(currency) + " million");
    print_range("nominal net investor cash including external support",
        family.supported_nominal_net_cash,
        std::string(currency) + " million");
    print_range("residual unprotected principal loss",
        family.supported_residual_loss,
        std::string(currency) + " million");
    print_range("residual principal-loss ES95",
        family.supported_residual_loss_es95,
        std::string(currency) + " million");
    print_range("residual principal-loss ES99",
        family.supported_residual_loss_es99,
        std::string(currency) + " million");
    print_range("outstanding principal at horizon (unchanged)",
        family.core.expected_outstanding_principal_million,
        std::string(currency) + " million");
    print_range("residual principal impairment probability (unchanged)",
        family.core.principal_impairment_probability, "percent", 100.0);
    print_range("investor NPV before premium",
        point.investor_expected_npv_before_premium_million,
        std::string(currency) + " million");
    print_range("expected supported NPV shortfall",
        family.supported_npv_shortfall,
        std::string(currency) + " million");
    print_range("supported NPV-shortfall ES95",
        family.supported_npv_shortfall_es95,
        std::string(currency) + " million");
    print_range("supported NPV-shortfall ES99",
        family.supported_npv_shortfall_es99,
        std::string(currency) + " million");
    print_range("peak gross funding need before settlement (unchanged)",
        family.core.expected_peak_same_month_funding_need_million,
        std::string(currency) + " million");
    std::cout
        << "  all-in underlying investor contributions: unchanged from core\n"
        << "  outstanding principal and gross impairment probability: "
           "unchanged from core\n"
        << "  draw liquidity before support settlement: unchanged "
           "from core\n"
        << "  protected-claim WAL: unavailable; external support settles at "
        << static_cast<double>(inputs.protection.settlement_month) / 12.0
        << " years in every claim-paying path\n";

    std::cout
        << "\n  Unsupported / full-provider-performance comparison\n"
        << "  measure | unsupported minimum/central/maximum | supported "
           "minimum/central/maximum\n"
        << "  all-in investor contributions | ";
    print_compact_range(family.core_all_in_contributions);
    std::cout << " | ";
    print_compact_range(family.core_all_in_contributions);
    std::cout << '\n' << "  total investor cash receipts | ";
    print_compact_range(family.core.expected_total_receipts_million);
    std::cout << " | ";
    print_compact_range(family.supported_total_receipts);
    std::cout << '\n' << "  NPV before premium | ";
    print_compact_range(family.core.expected_npv_million);
    std::cout << " | ";
    print_compact_range(point.investor_expected_npv_before_premium_million);
    std::cout << '\n' << "  gross / residual expected principal loss | ";
    print_compact_range(family.core.expected_principal_loss_million);
    std::cout << " | ";
    print_compact_range(family.supported_residual_loss);
    std::cout << '\n' << "  NPV-shortfall ES95 | ";
    print_compact_range(
        family.core.npv_shortfall_expected_shortfall_95_million);
    std::cout << " | ";
    print_compact_range(family.supported_npv_shortfall_es95);
    std::cout << "\n  Supported values assume full provider performance and "
                 "are before any investor premium.\n";

    std::cout << "\n  Provider payout risk\n";
    print_range("expected nominal payout",
        provider.expected_claim_nominal_million,
        std::string(currency) + " million");
    print_range("nominal payout ES95",
        provider.claim_expected_shortfall_95_nominal_million,
        std::string(currency) + " million");
    print_range("nominal payout ES99",
        provider.claim_expected_shortfall_99_nominal_million,
        std::string(currency) + " million");
    print_range("positive-claim probability", provider.claim_probability,
        "percent", 100.0);

    std::cout
        << "\n  Premium, provider floor, and catalytic gap\n"
        << "  price-ladder status: " << cf::to_string(price.status) << '\n'
        << "  investor signed premium headroom: "
        << price.investor_signed_premium_headroom_million << ' ' << currency
        << " million\n";
    print_optional_money("investor maximum non-negative premium",
        price.investor_maximum_nonnegative_premium_million, currency);
    std::cout
        << "  claim-only robust break-even floor: "
        << price.costs.claim_only_robust_floor_million << ' ' << currency
        << " million\n"
        << "  claim-only premium feasibility gap: "
        << point.premium_feasibility_gap_million << ' ' << currency
        << " million\n"
        << "  provider cost-recovery floor: "
        << price.costs.provider_cost_recovery_floor_million << ' ' << currency
        << " million\n"
        << "  provider all-in floor: "
        << price.costs.provider_all_in_floor_million << ' ' << currency
        << " million (month-zero provider-hurdle PV)\n"
        << "  provider premium support required: "
        << price.provider_premium_support_required_million << ' ' << currency
        << " million\n"
        << "  investor target restoration required: "
        << price.investor_target_restoration_required_million << ' '
        << currency << " million\n"
        << "  total all-in catalytic gap: "
        << price.all_in_support_gap_million << ' ' << currency
        << " million\n";

    std::cout
        << "\n  Provider counterparty-credit stress\n"
        << "  contractual maximum unsecured exposure: "
        << credit.exposure.contractual_maximum_unsecured_exposure_million
        << ' ' << currency << " million\n"
        << "  modeled maximum unsecured exposure: "
        << credit.exposure.modeled_maximum_unsecured_exposure_million << ' '
        << currency << " million\n";
    print_range("provider default probability",
        credit.robust.provider_default_probability, "percent", 100.0);
    print_range("actual support received PV",
        credit.robust.expected_actual_support_received_present_value_million,
        std::string(currency) + " million");
    print_range("expected unsecured exposure in default atoms",
        credit.robust.expected_unsecured_exposure_at_default_million,
        std::string(currency) + " million");
    print_range("investor counterparty-credit loss PV",
        credit.robust.expected_investor_credit_loss_present_value_million,
        std::string(currency) + " million");
    print_range("credit-stressed investor NPV before premium",
        credit.robust.investor_expected_npv_before_premium_million,
        std::string(currency) + " million");
    std::cout << "  central counterparty-credit loss PV ES95 / ES99: "
              << credit.central.investor_credit_loss_present_value_million
                     .expected_shortfall_95
              << " / "
              << credit.central.investor_credit_loss_present_value_million
                     .expected_shortfall_99
              << ' ' << currency << " million\n"
              << "  Support-delivery and credit-loss PV use the portfolio "
                 "investor hurdle; provider floors use the separate provider "
                 "hurdle.\n";
    std::cout << "  central claim/default correlation: ";
    if (credit.central.contractual_claim_provider_default_correlation
            .has_value()) {
        std::cout
            << *credit.central.contractual_claim_provider_default_correlation
            << '\n';
    } else {
        std::cout << "undefined\n";
    }
    std::cout << "  central claim-PV delivery ratio: ";
    if (credit.robust.central_claim_present_value_delivery_ratio.has_value()) {
        std::cout << *credit.robust.central_claim_present_value_delivery_ratio
                  << '\n';
    } else {
        std::cout << "undefined\n";
    }
    std::cout << "  robust minimum claim-PV delivery ratio: ";
    if (credit.robust.robust_minimum_claim_present_value_delivery_ratio
            .has_value()) {
        std::cout
            << credit.robust
                   .robust_minimum_claim_present_value_delivery_ratio->value
            << '\n';
    } else {
        std::cout << "undefined\n";
    }
    std::cout
        << "  base full-performance all-in gap: "
        << credit.support.base_full_performance_all_in_support_gap_million
        << ' ' << currency << " million\n"
        << "  incremental counterparty-credit gap: "
        << credit.support.incremental_counterparty_credit_support_gap_million
        << ' ' << currency << " million\n"
        << "  stressed total all-in catalytic gap: "
        << credit.support.stressed_all_in_support_gap_million << ' '
        << currency << " million\n"
        << "  Gross project loss remains visible. Support is an external "
           "transfer; provider default changes delivery, not the claim or "
           "full-performance provider floor.\n";
}

void print_report(const LoadedInputs& inputs, const EvaluatedFamily& family) {
    const std::string_view currency = inputs.portfolio.currency_label;
    const cf::ProviderPriceLadderSummary& price =
        family.credit.full_performance_price_ladder;

    std::cout
        << std::fixed << std::setprecision(6)
        << "SYNTHETIC TEN-CLAIM CELLULAR-AGRICULTURE INSTRUMENT FAMILY\n"
        << "ONE CORE MULTI-PROJECT ASSET / TWO TRANSPARENT VARIANTS\n"
        << "Physical-P cash-flow mechanics only; not calibration, fair value, "
           "a market quote, rating, legal opinion, offer, or investment "
           "recommendation.\n\n"
        << "Family boundary\n"
        << "  project claims: " << inputs.portfolio.projects.size()
        << " (required exactly 10)\n"
        << "  declared joint scenarios: "
        << inputs.portfolio.joint_scenarios.size() << '\n'
        << "  shared adverse risk groups: " << kRequiredRiskFactors.size()
        << '\n'
        << "  instrument variants: " << kInstrumentVariantCount
        << " (authorized maximum 2)\n"
        << "  core: unsupported multi-project milestone participation claim\n"
        << "  variant 1: fully funded first-loss / market-priority waterfall\n"
        << "  variant 2: 30% failure-contingent partial credit with provider "
           "stress\n"
        << "  currency and basis: " << currency << ", "
        << inputs.portfolio.monetary_basis << '\n'
        << "  horizon: " << inputs.portfolio.horizon_months << " months\n"
        << "  annual physical-P hurdle: "
        << inputs.portfolio.annual_physical_hurdle_rate * 100.0 << "%\n"
        << "  contractual reference principal: "
        << cf::portfolio_aggregate_reference_principal(inputs.portfolio) << ' '
        << currency << " million\n"
        << "  success participation retained across the comparison: q=1\n";

    std::cout
        << "\nUNSUPPORTED CORE - NO EXTERNAL CREDIT ENHANCEMENT\n"
        << "  metric | minimum | central | maximum | unit\n";
    print_range("all-in investor cash contributed (outlays plus pool costs)",
        family.core_all_in_contributions,
        std::string(currency) + " million");
    print_range("principal cash receipts", family.core_principal_cash,
        std::string(currency) + " million");
    print_range("non-principal success cash", family.core_nonprincipal_cash,
        std::string(currency) + " million");
    print_range("investor receipts",
        family.core.expected_total_receipts_million,
        std::string(currency) + " million");
    print_range("nominal net investor cash", family.core_nominal_net_cash,
        std::string(currency) + " million");
    print_range("outstanding principal at horizon",
        family.core.expected_outstanding_principal_million,
        std::string(currency) + " million");
    print_range("realized principal loss",
        family.core.expected_principal_loss_million,
        std::string(currency) + " million");
    print_range("principal impairment probability",
        family.core.principal_impairment_probability, "percent", 100.0);
    print_range("NPV at the declared physical-P hurdle",
        family.core.expected_npv_million,
        std::string(currency) + " million");
    print_range("negative-NPV probability",
        family.core.negative_npv_probability, "percent", 100.0);
    print_range("principal-loss ES95",
        family.core.principal_loss_expected_shortfall_95_million,
        std::string(currency) + " million");
    print_range("principal-loss ES99",
        family.core.principal_loss_expected_shortfall_99_million,
        std::string(currency) + " million");
    print_range("NPV-shortfall ES95",
        family.core.npv_shortfall_expected_shortfall_95_million,
        std::string(currency) + " million");
    print_range("NPV-shortfall ES99",
        family.core.npv_shortfall_expected_shortfall_99_million,
        std::string(currency) + " million");
    print_range("peak same-month gross funding need",
        family.core.expected_peak_same_month_funding_need_million,
        std::string(currency) + " million");
    print_range("peak cumulative net outlay",
        family.core.expected_peak_cumulative_net_outlay_million,
        std::string(currency) + " million");
    std::cout << "  core weighted-average life: unavailable for the on-demand "
                 "claim; tranche principal-cash WAL is reported below\n"
              << "  provider commitment, payout, premium capacity, and "
                 "provider floor: not applicable to the unsupported core\n"
              << "  remaining robust NPV gap at q=1: "
              << family.funded.selected_underlying_target_gap_million << ' '
              << currency << " million\n";

    print_core_receipt_sources(family.core, currency);
    print_project_concentration(inputs, family.core);
    print_dependence_and_diversification(family.core);
    print_factor_ranges(inputs, family.core);
    print_tranches(family.funded, currency);
    print_protection(inputs, family);

    std::cout
        << "\nEngine reconciliation controls\n"
        << "  core maximum cash reconciliation error: "
        << family.core.central_portfolio
               .maximum_cash_reconciliation_error_million
        << ' ' << currency << " million\n"
        << "  funded-variant maximum monetary reconciliation error: "
        << maximum_stack_reconciliation_error(family.funded) << ' '
        << currency << " million\n"
        << "  protection maximum monetary reconciliation error: "
        << maximum_protection_reconciliation_error(family.protection) << ' '
        << currency << " million\n"
        << "  provider-price maximum monetary reconciliation error: "
        << maximum_price_reconciliation_error(price) << ' ' << currency
        << " million\n"
        << "  provider-credit maximum monetary reconciliation error: "
        << maximum_credit_reconciliation_error(family.credit) << ' '
        << currency << " million\n";

    std::cout
        << "\nSynthetic / evidenced / unknown\n"
        << "  Synthetic: every project path, probability bound, hurdle, "
           "recovery, waterfall term, provider cost, collateral term, and "
           "counterparty state in this fixture.\n"
        << "  Mechanically verified here: deterministic engine replay and "
           "zero reconciliation controls.\n"
        << "  Empirical project or provider evidence supplied by this "
           "fixture: none. It is not an empirical portfolio.\n"
        << "  Unknown: calibrated probabilities, forecast accuracy, market "
           "price, fair value, liquidity, investor demand, legal "
           "enforceability, provider authority and capacity, tax, rating, and "
           "regulatory-capital treatment.\n"
        << "  Each range endpoint may use a different feasible probability "
           "witness. Endpoints must not be combined into one forecast.\n"
        << "  Pooling, tranching, and protection do not improve projects or "
           "create operating cash. They disclose and allocate already-modeled "
           "cash, loss, liquidity, and counterparty exposure.\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 8) {
        print_usage(argv[0]);
        return 2;
    }
    for (int index = 1; index < argc; ++index) {
        if (std::string_view(argv[index]).starts_with("--")) {
            print_usage(argv[0]);
            return 2;
        }
    }

    try {
        const LoadedInputs inputs = load_inputs(argv);
        validate_family_inputs(inputs);
        const EvaluatedFamily family = evaluate_family(inputs);
        print_report(inputs, family);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "instrument-family comparison failed: " << error.what()
                  << '\n';
        return 1;
    }
}
