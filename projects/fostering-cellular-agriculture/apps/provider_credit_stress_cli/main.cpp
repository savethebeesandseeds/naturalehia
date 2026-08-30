// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/pooled_loss_protection_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/provider_credit_stress.hpp>
#include <naturalehia/cellular_finance/provider_credit_stress_config.hpp>
#include <naturalehia/cellular_finance/provider_price_ladder_config.hpp>
#include <naturalehia/cellular_finance/success_participation_config.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cf = naturalehia::cellular_finance;

namespace {

void print_usage(std::string_view program) {
    std::cerr
        << "usage: " << program
        << " <portfolio.cfg> <probability-envelope.cfg> "
           "<success-participation.cfg> <loss-protection.cfg> "
           "<provider-price.cfg> <provider-credit.cfg> "
           "[--print-normalized]\n";
}

void print_range(std::string_view label,
    const cf::AmbiguityMetricRange& range, std::string_view unit,
    double scale = 1.0) {
    std::cout << "  " << label << " | "
              << range.minimum.value * scale << " | "
              << range.central * scale << " | "
              << range.maximum.value * scale << " | " << unit << '\n';
}

void print_distribution(std::string_view label,
    const cf::PortfolioDistributionSummary& distribution,
    std::string_view unit) {
    std::cout << "  " << label << " | " << distribution.mean << " | "
              << distribution.standard_deviation << " | "
              << distribution.p50 << " | " << distribution.p95 << " | "
              << distribution.p99 << " | " << distribution.maximum << " | "
              << distribution.expected_shortfall_95 << " | "
              << distribution.expected_shortfall_99 << " | " << unit
              << '\n';
}

void print_witness(std::string_view label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::ProviderCreditStressSummary& summary) {
    if (endpoint.scenario_weights.size() !=
        summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "provider-credit witness has the wrong scenario count");
    }
    std::cout << "  " << label << " | " << endpoint.value << " | ";
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

void print_optional_amount(std::string_view label,
    const std::optional<double>& amount, std::string_view currency) {
    std::cout << "  " << label << ": ";
    if (amount.has_value()) {
        std::cout << *amount << ' ' << currency << " million\n";
    } else {
        std::cout << "none\n";
    }
}

void print_optional_ratio(std::string_view label,
    const std::optional<double>& ratio) {
    std::cout << "  " << label << ": ";
    if (ratio.has_value()) {
        std::cout << *ratio << '\n';
    } else {
        std::cout << "undefined\n";
    }
}

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::ProviderCreditStressConfig& credit,
    const cf::ProviderCreditStressSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    const cf::ProviderCreditExposureBoundary& exposure = summary.exposure;
    const cf::ProviderCreditCentralRisk& central = summary.central;
    const cf::ProviderCreditRobustRisk& robust = summary.robust;
    const cf::ProviderCreditSupportEconomics& support = summary.support;

    std::cout << std::fixed << std::setprecision(6)
              << "SYNTHETIC PROVIDER COUNTERPARTY-CREDIT STRESS\n"
              << "Fixed conditional physical-P provider states only; not "
                 "CVA, fair value, a market quote, a rating, legal proof, "
                 "capital validation, or an offering.\n\n"
              << "Contract and price boundary\n"
              << "  provider id (protection/credit exact match): "
              << summary.provider_id << '\n'
              << "  selected coverage fraction g: "
              << summary.selected_coverage_fraction << '\n'
              << "  claim settlement month: " << summary.settlement_month
              << '\n'
              << "  gross contractual claim changed by credit stress: "
              << (summary.gross_contractual_claim_is_changed ? "yes" : "no")
              << '\n'
              << "  provider price reduced for default: "
              << (summary.provider_price_is_reduced_for_default ? "yes"
                                                                 : "no")
              << '\n'
              << "  conditional provider-state weights independently "
                 "optimized: no\n"
              << "  provider default measure: physical stress probability\n"
              << "  provider price basis: full performance in every "
                 "pricing scenario\n\n";

    std::cout << "Exposure and pledged-collateral boundary\n"
              << "  contractual maximum exposure: "
              << exposure.contractual_maximum_exposure_million << ' '
              << currency << " million\n"
              << "  modeled maximum claim: "
              << exposure.modeled_maximum_claim_million << ' ' << currency
              << " million\n"
              << "  price-ladder collateral base explicitly pledged: "
              << exposure.pledged_collateral_base_million << ' ' << currency
              << " million\n"
              << "  pledged collateral grown to settlement: "
              << exposure.pledged_collateral_at_settlement_million << ' '
              << currency << " million\n"
              << "  contractual maximum unsecured exposure: "
              << exposure.contractual_maximum_unsecured_exposure_million
              << ' ' << currency << " million\n"
              << "  modeled maximum unsecured exposure: "
              << exposure.modeled_maximum_unsecured_exposure_million << ' '
              << currency << " million\n"
              << "  Pledge and retained yield are input assertions, not "
                 "facts inferred from the price ladder.\n\n";

    std::cout << "Robust physical-probability ranges\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_range("provider default probability",
        robust.provider_default_probability, "percent", 100.0);
    print_range("positive claim and provider default probability",
        robust.positive_claim_and_provider_default_probability, "percent",
        100.0);
    print_range("expected contractual claim in default atoms",
        robust.expected_contractual_claim_at_default_million,
        std::string(currency) + " million");
    print_range("expected direct provider payment",
        robust.expected_direct_provider_payment_million,
        std::string(currency) + " million");
    print_range("expected collateral applied",
        robust.expected_collateral_applied_million,
        std::string(currency) + " million");
    print_range("expected unsecured exposure in default atoms (E[U*1_D])",
        robust.expected_unsecured_exposure_at_default_million,
        std::string(currency) + " million");
    print_range("expected delayed unsecured recovery",
        robust.expected_delayed_unsecured_recovery_million,
        std::string(currency) + " million");
    print_range("expected ultimate unpaid claim",
        robust.expected_ultimate_unpaid_claim_million,
        std::string(currency) + " million");
    print_range("expected full claim PV",
        robust.expected_full_claim_present_value_million,
        std::string(currency) + " million");
    print_range("expected support received PV",
        robust.expected_actual_support_received_present_value_million,
        std::string(currency) + " million");
    print_range("expected counterparty credit loss PV",
        robust.expected_investor_credit_loss_present_value_million,
        std::string(currency) + " million");
    print_range("investor NPV before premium",
        robust.investor_expected_npv_before_premium_million,
        std::string(currency) + " million");
    print_range("hypothetical investor NPV if paying unchanged provider price",
        robust
            .investor_expected_npv_after_unchanged_full_performance_price_million,
        std::string(currency) + " million");
    std::cout << '\n';

    std::cout << "Central physical counterparty-risk distribution\n"
              << "  metric | mean | standard deviation | p50 | p95 | p99 | "
                 "maximum | expected shortfall 95% | expected shortfall 99% "
                 "| unit\n";
    print_distribution("counterparty credit loss PV",
        central.investor_credit_loss_present_value_million,
        std::string(currency) + " million");
    print_distribution("ultimate unpaid claim",
        central.ultimate_unpaid_claim_million,
        std::string(currency) + " million");
    print_distribution("unsecured exposure in default atoms (U*1_D)",
        central.unsecured_exposure_at_default_million,
        std::string(currency) + " million");
    std::cout << '\n';

    std::cout << "Wrong-way risk and claim delivery\n"
              << "  central provider default probability: "
              << central.provider_default_probability * 100.0 << " percent\n"
              << "  central positive-claim-and-default probability: "
              << central.positive_claim_and_provider_default_probability *
                     100.0
              << " percent\n";
    print_optional_ratio("claim-weighted provider default rate",
        central.claim_weighted_provider_default_rate);
    std::cout << "  claim/default covariance: "
              << central.contractual_claim_provider_default_covariance_million
              << ' ' << currency << " million\n";
    print_optional_ratio("claim/default correlation",
        central.contractual_claim_provider_default_correlation);
    print_optional_amount("expected contractual claim given default",
        central.expected_contractual_claim_given_provider_default_million,
        currency);
    print_optional_amount("expected unsecured exposure given default",
        central.expected_unsecured_exposure_given_provider_default_million,
        currency);
    print_optional_ratio("claim-at-default / unconditional-mean multiplier",
        central.claim_at_default_severity_multiplier);
    print_optional_ratio("central claim-PV delivery ratio",
        robust.central_claim_present_value_delivery_ratio);
    if (robust.robust_minimum_claim_present_value_delivery_ratio.has_value()) {
        print_witness("robust minimum claim-PV delivery ratio",
            *robust.robust_minimum_claim_present_value_delivery_ratio,
            summary);
    } else {
        std::cout << "  robust minimum claim-PV delivery ratio: undefined\n";
    }
    std::cout << '\n';

    std::cout << "Unchanged price and explicit support\n"
              << "  unchanged full-performance provider all-in floor: "
              << support.unchanged_full_performance_provider_price_million
              << ' ' << currency << " million\n"
              << "  provider price change caused by default stress: "
              << support.full_performance_provider_price_change_million << ' '
              << currency << " million\n"
              << "  stressed investor signed premium headroom: "
              << support.stressed_investor_signed_premium_headroom_million
              << ' ' << currency << " million\n";
    print_optional_amount("stressed investor maximum non-negative premium",
        support.stressed_investor_maximum_nonnegative_premium_million,
        currency);
    std::cout << "  provider premium support required: "
              << support.stressed_provider_premium_support_required_million
              << ' ' << currency << " million\n"
              << "  investor target restoration required: "
              << support.stressed_investor_target_restoration_required_million
              << ' ' << currency << " million\n"
              << "  base full-performance all-in support gap: "
              << support.base_full_performance_all_in_support_gap_million
              << ' ' << currency << " million\n"
              << "  incremental counterparty-credit support gap: "
              << support.incremental_counterparty_credit_support_gap_million
              << ' ' << currency << " million\n"
              << "  stressed total all-in support gap: "
              << support.stressed_all_in_support_gap_million << ' '
              << currency << " million\n\n";

    std::cout << "Scenario/provider-state settlement ledger\n"
              << "  scenario/state | conditional weight | central joint "
                 "weight | performs | claim | direct payment | collateral "
                 "realization fraction | collateral applied | collateral PV "
                 "| unsecured EAD | recovery fraction | delayed recovery | "
                 "recovery month | recovery PV | ultimate unpaid | received "
                 "PV | credit loss PV | investor NPV before "
                 "premium | unit\n";
    for (const cf::ProviderCreditScenarioResult& scenario :
         summary.scenarios) {
        for (const cf::ProviderCreditOutcomeResult& outcome :
             scenario.outcomes) {
            std::cout << "  " << scenario.scenario_id << '/'
                      << outcome.outcome_id << " | "
                      << outcome.conditional_weight << " | "
                      << outcome.expanded_central_weight << " | "
                      << (outcome.provider_performs ? "yes" : "no") << " | "
                      << outcome.gross_contractual_claim_million << " | "
                      << outcome.direct_provider_payment_at_settlement_million
                      << " | " << outcome.collateral_realization_fraction
                      << " | "
                      << outcome.collateral_applied_at_settlement_million
                      << " | "
                      << outcome.collateral_received_present_value_million
                      << " | "
                      << outcome.unsecured_exposure_at_default_million << " | "
                      << outcome.unsecured_recovery_fraction << " | "
                      << outcome.delayed_unsecured_recovery_million << " | "
                      << outcome.unsecured_recovery_month << " | "
                      << outcome.unsecured_recovery_present_value_million
                      << " | "
                      << outcome.ultimate_unpaid_claim_million << " | "
                      << outcome.actual_support_received_present_value_million
                      << " | "
                      << outcome.investor_credit_loss_present_value_million
                      << " | "
                      << outcome.investor_npv_before_premium_million << " | "
                      << currency << " million\n";
        }
    }
    std::cout << '\n';

    std::cout << "Binding probability witnesses\n"
              << "  metric | endpoint value | scenario probability vector\n";
    print_witness("maximum expected counterparty credit loss PV",
        robust.expected_investor_credit_loss_present_value_million.maximum,
        summary);
    print_witness("minimum expected support received PV",
        robust.expected_actual_support_received_present_value_million.minimum,
        summary);
    std::cout << '\n';

    std::cout << "Reconciliation controls\n"
              << "  maximum gross project-loss change: "
              << summary.maximum_gross_project_loss_change_million << ' '
              << currency << " million\n"
              << "  maximum conditional-weight sum error: "
              << summary.maximum_conditional_weight_sum_error << '\n'
              << "  expanded central probability sum error: "
              << summary.expanded_central_probability_sum_error << '\n'
              << "  maximum default-waterfall error: "
              << summary.maximum_default_waterfall_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum credit-loss error: "
              << summary.maximum_credit_loss_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum conditional-collapse error: "
              << summary.maximum_conditional_collapse_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum central probability-projection error: "
              << summary
                     .maximum_central_probability_projection_reconciliation_error
              << '\n'
              << "  maximum central monetary-projection error: "
              << summary
                     .maximum_central_monetary_projection_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  support-gap decomposition error: "
              << summary.support_gap_decomposition_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum probability-witness reconciliation error: "
              << summary.maximum_probability_witness_reconciliation_error
              << '\n'
              << "  maximum monetary-witness reconciliation error: "
              << summary.maximum_monetary_witness_reconciliation_error_million
              << ' '
              << currency << " million\n"
              << "  robust delivery-ratio objective residual: "
              << robust
                     .robust_minimum_delivery_ratio_objective_residual_million
              << ' ' << currency << " million\n"
              << "  maximum endpoint probability error: "
              << summary.maximum_endpoint_probability_error << "\n\n";

    std::cout
        << "Interpretation boundary\n"
        << "  " << summary.provider_credit_model_limitation << '\n'
        << "  Conditional provider states are averaged within each original "
           "project scenario before ambiguity projection.\n"
        << "  Provider failure changes only claim delivery; it does not "
           "reprice the provider, reduce gross project loss, or create DVA.\n"
        << "  Collateral is applied before delayed unsecured recovery, and "
           "each transfer is capped by the unchanged contractual claim.\n"
        << "  Required support is disclosed explicitly; no such support is "
           "assumed to exist merely because the model computes it.\n"
        << "  Synthetic input label: " << credit.scenario_label << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 7 || argc > 8 ||
        std::string_view(argv[1]).starts_with("--") ||
        std::string_view(argv[2]).starts_with("--") ||
        std::string_view(argv[3]).starts_with("--") ||
        std::string_view(argv[4]).starts_with("--") ||
        std::string_view(argv[5]).starts_with("--") ||
        std::string_view(argv[6]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 8;
    if (print_normalized &&
        std::string_view(argv[7]) != "--print-normalized") {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const cf::PortfolioConfig portfolio =
            cf::load_portfolio_config(std::filesystem::path(argv[1]));
        const cf::PortfolioAmbiguityConfig ambiguity =
            cf::load_portfolio_ambiguity_config(
                std::filesystem::path(argv[2]));
        const cf::SuccessParticipationConfig participation =
            cf::load_success_participation_config(
                std::filesystem::path(argv[3]));
        const cf::PooledLossProtectionConfig protection =
            cf::load_pooled_loss_protection_config(
                std::filesystem::path(argv[4]));
        const cf::ProviderPriceLadderConfig pricing =
            cf::load_provider_price_ladder_config(
                std::filesystem::path(argv[5]));
        const cf::ProviderCreditStressConfig credit =
            cf::load_provider_credit_stress_config(
                std::filesystem::path(argv[6]));

        const cf::ProviderCreditStressSummary summary =
            cf::solve_provider_credit_stress(portfolio, ambiguity,
                participation, protection, pricing, credit);
        print_report(portfolio, credit, summary);

        if (print_normalized) {
            std::cout << "\nNormalized portfolio configuration\n";
            cf::print_normalized_portfolio_config(std::cout, portfolio);
            std::cout
                << "\nNormalized probability-envelope configuration\n";
            cf::print_normalized_portfolio_ambiguity_config(
                std::cout, ambiguity);
            std::cout
                << "\nNormalized success-participation configuration\n";
            cf::print_normalized_success_participation_config(
                std::cout, participation);
            std::cout
                << "\nNormalized pooled-loss-protection configuration\n";
            cf::print_normalized_pooled_loss_protection_config(
                std::cout, protection);
            std::cout
                << "\nNormalized provider price-ladder configuration\n";
            cf::print_normalized_provider_price_ladder_config(
                std::cout, pricing);
            std::cout
                << "\nNormalized provider counterparty-credit "
                   "configuration\n";
            cf::print_normalized_provider_credit_stress_config(
                std::cout, credit);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "provider counterparty-credit stress failed: "
                  << error.what() << '\n';
        return 1;
    }
}
