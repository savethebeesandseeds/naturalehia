// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio.hpp>

#include <cstddef>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace naturalehia::cellular_finance::detail {

struct RobustTwoClaimGridWorkCounts {
    std::size_t portfolio_cash_records{0U};
    std::size_t portfolio_auxiliary_records{0U};
    std::size_t portfolio_records{0U};
    std::size_t probability_projection{0U};
    std::size_t cash_path{0U};
    std::size_t total{0U};
};

[[nodiscard]] inline std::size_t checked_grid_product(
    std::initializer_list<std::size_t> factors, std::size_t limit,
    std::string_view limit_message) {
    std::size_t product = 1U;
    for (const std::size_t factor : factors) {
        if (factor != 0U && product > limit / factor) {
            throw std::invalid_argument(std::string(limit_message));
        }
        product *= factor;
    }
    return product;
}

[[nodiscard]] inline std::size_t checked_grid_sum(
    std::size_t first, std::size_t second, std::size_t limit,
    std::string_view limit_message) {
    if (first > limit || second > limit - first) {
        throw std::invalid_argument(std::string(limit_message));
    }
    return first + second;
}

[[nodiscard]] inline RobustTwoClaimGridWorkCounts
count_robust_two_claim_portfolio_records(const PortfolioConfig& portfolio,
    std::size_t limit, std::string_view limit_message) {
    RobustTwoClaimGridWorkCounts result;
    result.portfolio_auxiliary_records = checked_grid_product(
        {portfolio.loss_layers.size(), portfolio.joint_scenarios.size()},
        limit, limit_message);
    for (const JointScenario& scenario : portfolio.joint_scenarios) {
        result.portfolio_auxiliary_records = checked_grid_sum(
            result.portfolio_auxiliary_records,
            scenario.cash_sources.size(), limit, limit_message);
        result.portfolio_auxiliary_records = checked_grid_sum(
            result.portfolio_auxiliary_records,
            scenario.factor_tags.size(), limit, limit_message);
        result.portfolio_cash_records = checked_grid_sum(
            result.portfolio_cash_records, scenario.pool_costs.size(), limit,
            limit_message);
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            result.portfolio_cash_records = checked_grid_sum(
                result.portfolio_cash_records,
                source.cash_available.size(), limit, limit_message);
        }
        for (const ProjectJointPath& path : scenario.project_paths) {
            result.portfolio_cash_records = checked_grid_sum(
                result.portfolio_cash_records, path.capital_draws.size(),
                limit, limit_message);
            result.portfolio_cash_records = checked_grid_sum(
                result.portfolio_cash_records,
                path.investor_receipts.size(), limit, limit_message);
        }
    }
    result.portfolio_records = checked_grid_sum(
        result.portfolio_cash_records, result.portfolio_auxiliary_records,
        limit, limit_message);
    return result;
}

[[nodiscard]] inline RobustTwoClaimGridWorkCounts
checked_robust_two_claim_grid_work(const PortfolioConfig& portfolio,
    std::size_t candidate_count, std::size_t scenario_count,
    std::size_t event_count, std::size_t horizon_months, std::size_t limit,
    std::string_view limit_message) {
    if (scenario_count == 0U) {
        throw std::invalid_argument(
            "two-claim grid structural work requires at least one scenario");
    }
    if (event_count >= std::numeric_limits<std::size_t>::max() -
            scenario_count) {
        throw std::invalid_argument(
            "two-claim grid scenario and event counts overflow the structural-work calculation");
    }
    const std::size_t structural_width =
        scenario_count + event_count + 1U;
    if (horizon_months == std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument(
            "two-claim grid horizon overflows the structural-work calculation");
    }
    const std::size_t month_count = horizon_months + 1U;

    RobustTwoClaimGridWorkCounts result =
        count_robust_two_claim_portfolio_records(
            portfolio, limit, limit_message);
    result.probability_projection = checked_grid_product(
        {candidate_count, scenario_count, structural_width}, limit,
        limit_message);
    const std::size_t portfolio_record_work = checked_grid_product(
        {candidate_count, result.portfolio_records}, limit, limit_message);
    const std::size_t project_path_work = checked_grid_product(
        {candidate_count, portfolio.projects.size(), scenario_count,
            month_count},
        limit, limit_message);
    const std::size_t two_claim_waterfall_work = checked_grid_product(
        {candidate_count, scenario_count, 2U, month_count}, limit,
        limit_message);
    result.cash_path = checked_grid_sum(portfolio_record_work,
        checked_grid_sum(project_path_work, two_claim_waterfall_work, limit,
            limit_message),
        limit, limit_message);
    result.total = checked_grid_sum(result.probability_projection,
        result.cash_path, limit, limit_message);
    return result;
}

} // namespace naturalehia::cellular_finance::detail
