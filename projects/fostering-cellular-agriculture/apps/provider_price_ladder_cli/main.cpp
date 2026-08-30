// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/pooled_loss_protection_config.hpp>
#include <naturalehia/cellular_finance/portfolio_ambiguity_config.hpp>
#include <naturalehia/cellular_finance/portfolio_config.hpp>
#include <naturalehia/cellular_finance/provider_price_ladder.hpp>
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
           "<provider-price.cfg> [--print-normalized]\n";
}

void print_range(std::string_view label,
    const cf::AmbiguityMetricRange& range, std::string_view currency) {
    std::cout << "  " << label << " | " << range.minimum.value << " | "
              << range.central << " | " << range.maximum.value << " | "
              << currency << " million\n";
}

void print_witness(std::string_view label,
    const cf::AmbiguityEndpoint& endpoint,
    const cf::ProviderPriceLadderSummary& summary) {
    if (endpoint.scenario_weights.size() !=
        summary.scenario_probability_bounds.size()) {
        throw std::logic_error(
            "provider price-ladder witness has the wrong scenario count");
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

void print_report(const cf::PortfolioConfig& portfolio,
    const cf::PooledLossProtectionConfig& protection,
    const cf::ProviderPriceLadderConfig& pricing,
    const cf::ProviderPriceLadderSummary& summary) {
    const std::string_view currency = portfolio.currency_label;
    const cf::PooledLossProtectionRobustPoint& point =
        summary.selected_protection_point;
    const cf::ProviderPriceLadderCostBreakdown& costs = summary.costs;

    std::cout << std::fixed << std::setprecision(6)
              << "SYNTHETIC PROVIDER PRICE-LADDER SENSITIVITY\n"
              << "Physical-probability premium adequacy only; not fair "
                 "value, a market quote, a rating, capital validation, or "
                 "an offering.\n\n"
              << "Selected protection point\n"
              << "  provider id: " << protection.provider_id << '\n'
              << "  selection mode: "
              << cf::to_string(summary.coverage_selection) << '\n'
              << "  protection solver status: "
              << cf::to_string(summary.protection_solve_status) << '\n'
              << "  selected coverage fraction g: "
              << point.coverage_fraction << '\n'
              << "  investor target met before premium: "
              << (summary.selected_point_meets_investor_target_before_premium
                          ? "yes"
                          : "no")
              << '\n'
              << "  settlement month: " << protection.settlement_month
              << '\n'
              << "  cost basis: contractual maximum exposure\n"
              << "  provider performance: assumed in every scenario\n\n";

    std::cout << "Exposure—not price\n"
              << "  configured monetary support cap: "
              << point.provider_risk.legal_support_cap_million << ' '
              << currency << " million\n"
              << "  contractual maximum exposure: "
              << costs.contractual_maximum_exposure_million << ' '
              << currency << " million\n"
              << "  modeled maximum claim: "
              << costs.modeled_maximum_claim_million << ' ' << currency
              << " million\n"
              << "  collateral principal base: "
              << costs.collateral_base_million << ' ' << currency
              << " million\n"
              << "  allocated risk-capital base: "
              << costs.risk_capital_base_million << ' ' << currency
              << " million\n"
              << "  Collateral principal and allocated capital stock are "
                 "not premium expenses.\n\n";

    const double claim_and_variable_expense =
        costs.claim_only_robust_floor_million +
        costs.variable_claim_expense_at_robust_endpoint_million;
    std::cout << "Provider robust upfront price ladder\n"
              << "  robust expected claim PV: "
              << costs.claim_only_robust_floor_million << ' ' << currency
              << " million\n"
              << "  variable claim expense: "
              << costs.variable_claim_expense_at_robust_endpoint_million
              << ' ' << currency << " million\n"
              << "  claim plus variable expense subtotal: "
              << claim_and_variable_expense << ' ' << currency
              << " million\n"
              << "  collateral funding carry PV: "
              << costs.collateral_funding_carry_present_value_million << ' '
              << currency << " million\n"
              << "  incremental economic-capital charge PV: "
              << costs.risk_capital_charge_present_value_million << ' '
              << currency << " million\n"
              << "  fixed upfront expense: "
              << costs.fixed_expense_upfront_million << ' ' << currency
              << " million\n"
              << "  provider robust cost-recovery floor: "
              << costs.provider_cost_recovery_floor_million << ' '
              << currency << " million\n"
              << "  target underwriting profit: "
              << costs.target_profit_upfront_million << ' ' << currency
              << " million\n"
              << "  provider robust all-in floor: "
              << costs.provider_all_in_floor_million << ' ' << currency
              << " million\n\n";

    std::cout << "Physical expected provider requirement range\n"
              << "  metric | minimum | central | maximum | unit\n";
    print_range("cost recovery",
        summary.provider_cost_recovery_requirement_million, currency);
    print_range("all-in revenue requirement",
        summary.provider_all_in_revenue_requirement_million, currency);
    std::cout << '\n';

    std::cout << "Investor comparison and explicit support\n"
              << "  status: " << cf::to_string(summary.status) << '\n'
              << "  investor signed premium headroom: "
              << summary.investor_signed_premium_headroom_million << ' '
              << currency << " million\n";
    print_optional_amount("investor maximum non-negative premium",
        summary.investor_maximum_nonnegative_premium_million, currency);
    print_optional_amount("robust price interval lower bound",
        summary.robust_price_interval_lower_bound_million, currency);
    print_optional_amount("robust price interval upper bound",
        summary.robust_price_interval_upper_bound_million, currency);
    std::cout << "  cost-recovery support gap: "
              << summary.cost_recovery_support_gap_million << ' '
              << currency << " million\n"
              << "  provider premium support required: "
              << summary.provider_premium_support_required_million << ' '
              << currency << " million\n"
              << "  investor target restoration required: "
              << summary.investor_target_restoration_required_million << ' '
              << currency << " million\n"
              << "  total all-in support gap: "
              << summary.all_in_support_gap_million << ' ' << currency
              << " million\n"
              << "  robust all-in bilateral interval exists: "
              << (summary.robust_all_in_nonnegative_premium_interval_exists
                          ? "yes"
                          : "no")
              << "\n\n";

    std::cout << "Binding probability witnesses\n"
              << "  metric | endpoint value | scenario probability vector\n";
    print_witness("investor minimum NPV before premium",
        point.investor_expected_npv_before_premium_million.minimum,
        summary);
    print_witness("provider maximum all-in requirement",
        summary.provider_all_in_revenue_requirement_million.maximum,
        summary);
    std::cout << '\n';

    std::cout << "Reconciliation controls\n"
              << "  selected coverage error: "
              << summary.selected_coverage_reconciliation_error << '\n'
              << "  maximum price-ladder sum error: "
              << summary.maximum_cost_ladder_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  support-gap decomposition error: "
              << summary
                     .support_gap_decomposition_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum transformed-range error: "
              << summary
                     .maximum_transformed_range_reconciliation_error_million
              << ' ' << currency << " million\n"
              << "  maximum endpoint probability error: "
              << summary.maximum_endpoint_probability_error << "\n\n";

    std::cout
        << "Interpretation boundary\n"
        << "  " << summary.provider_price_model_limitation << '\n'
        << "  The collateral carry and economic-capital charge are "
           "independently supplied, incremental, nonduplicative pricing "
           "allowances.\n"
        << "  Their bases are exposure-sizing bases, not debt/equity "
           "funding shares; capital does not reduce collateral in v0.1.\n"
        << "  Supplied net rates must already reflect any capital funding, "
           "premium float, investment income, collateral reuse, and "
           "treasury offsets.\n"
        << "  Allocated capital stock is not charged as an expense; the "
           "model does not establish regulatory or economic capital "
           "sufficiency.\n"
        << "  Target profit is separate from claim cost, expenses, funding "
           "carry, and capital charge.\n"
        << "  A positive gap is required catalytic support, not hidden in "
           "probabilities or called diversification.\n"
        << "  Synthetic input label: " << pricing.scenario_label << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 6 || argc > 7 ||
        std::string_view(argv[1]).starts_with("--") ||
        std::string_view(argv[2]).starts_with("--") ||
        std::string_view(argv[3]).starts_with("--") ||
        std::string_view(argv[4]).starts_with("--") ||
        std::string_view(argv[5]).starts_with("--")) {
        print_usage(argv[0]);
        return 2;
    }
    const bool print_normalized = argc == 7;
    if (print_normalized &&
        std::string_view(argv[6]) != "--print-normalized") {
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

        const cf::ProviderPriceLadderSummary summary =
            cf::solve_provider_price_ladder(portfolio, ambiguity,
                participation, protection, pricing);
        print_report(portfolio, protection, pricing, summary);

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
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "provider price-ladder analysis failed: "
                  << error.what() << '\n';
        return 1;
    }
}
