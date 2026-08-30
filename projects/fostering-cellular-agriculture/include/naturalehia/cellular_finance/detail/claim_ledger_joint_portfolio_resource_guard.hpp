// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace naturalehia::cellular_finance::detail {

inline constexpr std::size_t
    kMaximumRetainedMarginalProjectScenarioPairs = 500'000U;
inline constexpr std::size_t
    kMaximumRetainedMarginalScenarioMonthRows = 2'000'000U;
inline constexpr std::size_t
    kMaximumRetainedMarginalCashRecords = 2'000'000U;
inline constexpr std::size_t
    kMaximumRetainedMarginalLineageRows = 2'000'000U;
inline constexpr std::size_t
    kMaximumExpandedJointLineageEntries = 2'000'000U;

// This small checked accumulator is kept in a detail header so the exact-limit
// and overflow behavior can be unit-tested without allocating millions of
// synthetic rows. It is not part of the financial-model input interface.
inline void add_bounded_resource(std::size_t& current,
    std::size_t addition, std::size_t limit,
    std::string_view description) {
    if (current > limit || addition > limit - current) {
        throw std::invalid_argument(
            std::string(description) + " exceeds the resource guardrail");
    }
    current += addition;
}

struct RetainedMarginalResourceUsage {
    std::size_t project_scenario_pairs{0U};
    std::size_t scenario_month_rows{0U};
    std::size_t cash_records{0U};
    std::size_t lineage_rows{0U};

    void add_project_scenario_pairs(std::size_t addition) {
        add_bounded_resource(project_scenario_pairs, addition,
            kMaximumRetainedMarginalProjectScenarioPairs,
            "retained marginal project-scenario pairs");
    }

    void add_scenario_month_rows(std::size_t addition) {
        add_bounded_resource(scenario_month_rows, addition,
            kMaximumRetainedMarginalScenarioMonthRows,
            "retained marginal scenario-month rows");
    }

    void add_cash_records(std::size_t addition) {
        add_bounded_resource(cash_records, addition,
            kMaximumRetainedMarginalCashRecords,
            "retained marginal cash records");
    }

    void add_lineage_rows(std::size_t addition) {
        add_bounded_resource(lineage_rows, addition,
            kMaximumRetainedMarginalLineageRows,
            "retained marginal lineage and decision-entry rows");
    }
};

struct ExpandedJointResourceUsage {
    std::size_t lineage_entries{0U};

    void add_lineage_entries(std::size_t addition) {
        add_bounded_resource(lineage_entries, addition,
            kMaximumExpandedJointLineageEntries,
            "expanded joint lineage rows and factor instances");
    }
};

} // namespace naturalehia::cellular_finance::detail
