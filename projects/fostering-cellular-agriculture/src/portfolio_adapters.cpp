// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/portfolio_adapters.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumScenarioCashSources = 256U;
constexpr std::size_t kPortfolioCashSourceCount = 8U;
constexpr std::string_view kSponsorFeeSuffix{".sponsor-fee"};
constexpr std::string_view kRecoverySuffix{".recovery"};
constexpr double kAbsoluteTolerance = 1.0e-10;

class CompensatedSum {
public:
    void add(long double value) noexcept {
        const long double next = sum_ + value;
        if (std::abs(sum_) >= std::abs(value)) {
            correction_ += (sum_ - next) + value;
        } else {
            correction_ += (value - next) + sum_;
        }
        sum_ = next;
    }

    [[nodiscard]] long double value() const noexcept {
        return sum_ + correction_;
    }

private:
    long double sum_{0.0L};
    long double correction_{0.0L};
};

[[nodiscard]] long double tolerance_for(
    long double first, long double second) noexcept {
    const long double scale =
        std::max({1.0L, std::abs(first), std::abs(second)});
    return static_cast<long double>(kAbsoluteTolerance) +
        64.0L * static_cast<long double>(
            std::numeric_limits<double>::epsilon()) * scale;
}

[[nodiscard]] bool nearly_equal(
    long double first, long double second) noexcept {
    return std::abs(first - second) <= tolerance_for(first, second);
}

[[nodiscard]] double to_double(long double value) {
    const double converted = static_cast<double>(value);
    if (!std::isfinite(converted)) {
        throw std::overflow_error(
            "staged-capital adapter aggregation exceeded numeric range");
    }
    return converted;
}

void require_reconciliation(long double actual, long double expected,
    std::string_view description) {
    if (!nearly_equal(actual, expected)) {
        throw std::logic_error(
            std::string(description) + " failed to reconcile");
    }
}

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool is_safe_portfolio_identifier(
    std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_project_id_with_source_suffix_capacity(
    std::string_view project_id) {
    if (!is_safe_portfolio_identifier(project_id)) {
        throw std::invalid_argument(
            "adapter project id must be a safe portfolio identifier");
    }
    const std::size_t longest_suffix =
        std::max(kSponsorFeeSuffix.size(), kRecoverySuffix.size());
    if (project_id.size() > kMaximumIdentifierLength - longest_suffix) {
        throw std::invalid_argument(
            "adapter project id is too long for transparent cash-source suffixes");
    }
}

[[nodiscard]] bool is_supported_completion_source(
    PortfolioCashSource source) noexcept {
    switch (source) {
    case PortfolioCashSource::Commercial:
    case PortfolioCashSource::LicensingRoyalty:
    case PortfolioCashSource::ExitSale:
    case PortfolioCashSource::Refinancing:
    case PortfolioCashSource::ExplicitSupport:
        return true;
    case PortfolioCashSource::Recovery:
    case PortfolioCashSource::SponsorFee:
    case PortfolioCashSource::FinancingFee:
        return false;
    }
    return false;
}

using CompletionAllocationMap = std::unordered_map<std::string,
    std::vector<StagedCompletionSourceAllocation>>;

[[nodiscard]] CompletionAllocationMap validate_completion_allocations(
    const StagedCapitalSummary& staged_summary,
    const std::vector<StagedCompletionSourceAllocation>& allocations,
    std::string_view sponsor_fee_source_id,
    std::string_view recovery_source_id) {
    if (staged_summary.cases.size() >
            std::numeric_limits<std::size_t>::max() /
                kMaximumScenarioCashSources ||
        allocations.size() >
            staged_summary.cases.size() * kMaximumScenarioCashSources) {
        throw std::invalid_argument(
            "completion-source allocation count exceeds the portfolio resource guardrail");
    }

    CompletionAllocationMap by_case;
    by_case.reserve(staged_summary.cases.size());
    std::unordered_map<std::string, PortfolioCashSource> id_taxonomies;
    id_taxonomies.reserve(allocations.size());
    for (const StagedCompletionSourceAllocation& allocation : allocations) {
        if (!is_supported_completion_source(allocation.source)) {
            throw std::invalid_argument(
                "completion cash source must classify completion proceeds and cannot be recovery, sponsor-fee, or financing-fee");
        }
        if (!std::isfinite(allocation.amount_million) ||
            !(allocation.amount_million > 0.0)) {
            throw std::invalid_argument(
                "completion-source allocation amount must be finite and strictly positive");
        }
        if (!is_safe_portfolio_identifier(allocation.cash_source_id)) {
            throw std::invalid_argument(
                "completion allocation cash source id must be a safe portfolio identifier");
        }
        if (allocation.cash_source_id == sponsor_fee_source_id ||
            allocation.cash_source_id == recovery_source_id) {
            throw std::invalid_argument(
                "completion allocation cash source id collides with a generated source taxonomy");
        }
        const auto [taxonomy, inserted_taxonomy] = id_taxonomies.emplace(
            allocation.cash_source_id, allocation.source);
        if (!inserted_taxonomy && taxonomy->second != allocation.source) {
            throw std::invalid_argument(
                "cash source id changes taxonomy across staged cases: " +
                allocation.cash_source_id);
        }
        const auto matching = std::find_if(staged_summary.cases.begin(),
            staged_summary.cases.end(),
            [&allocation](const StagedCapitalPathResult& path) {
                return path.case_id == allocation.case_id;
            });
        const bool allocation_required =
            matching != staged_summary.cases.end() &&
            matching->outcome == StagedCapitalOutcome::Completed &&
            matching->provider_nominal_recovery_million > 0.0;
        if (!allocation_required) {
            throw std::invalid_argument(
                "extraneous completion-source allocation for case: " +
                allocation.case_id);
        }

        std::vector<StagedCompletionSourceAllocation>& case_allocations =
            by_case[allocation.case_id];
        if (case_allocations.size() >= kMaximumScenarioCashSources) {
            throw std::invalid_argument(
                "completion-source allocation count exceeds the per-scenario guardrail");
        }
        if (std::any_of(case_allocations.begin(), case_allocations.end(),
                [&allocation](
                    const StagedCompletionSourceAllocation& existing) {
                    return existing.cash_source_id ==
                        allocation.cash_source_id;
                })) {
            throw std::invalid_argument(
                "duplicate completion case and cash-source-id allocation: " +
                allocation.case_id + "/" +
                allocation.cash_source_id);
        }
        case_allocations.push_back(allocation);
    }

    for (const StagedCapitalPathResult& path : staged_summary.cases) {
        const bool allocation_required =
            path.outcome == StagedCapitalOutcome::Completed &&
            path.provider_nominal_recovery_million > 0.0;
        const auto matching = by_case.find(path.case_id);
        if (allocation_required && matching == by_case.end()) {
            throw std::invalid_argument(
                "missing completion-source allocations for case: " +
                path.case_id);
        }
        if (!allocation_required) {
            continue;
        }

        CompensatedSum allocated;
        for (const StagedCompletionSourceAllocation& allocation :
             matching->second) {
            allocated.add(
                static_cast<long double>(allocation.amount_million));
        }
        const long double repayment = static_cast<long double>(
            path.provider_nominal_recovery_million);
        if (!nearly_equal(allocated.value(), repayment)) {
            const std::string direction =
                allocated.value() > repayment ? "over-allocates" :
                                                "under-allocates";
            throw std::invalid_argument(
                "completion-source allocations " + direction +
                " provider repayment for case: " + path.case_id);
        }
        std::sort(matching->second.begin(), matching->second.end(),
            [](const StagedCompletionSourceAllocation& first,
                const StagedCompletionSourceAllocation& second) {
                if (first.cash_source_id != second.cash_source_id) {
                    return first.cash_source_id < second.cash_source_id;
                }
                return static_cast<unsigned int>(first.source) <
                    static_cast<unsigned int>(second.source);
            });
    }
    return by_case;
}

[[nodiscard]] long double posting_sum(
    const CapitalCashPosting& posting) noexcept {
    CompensatedSum total;
    total.add(static_cast<long double>(posting.sponsor_million));
    total.add(static_cast<long double>(posting.project_unrestricted_million));
    total.add(static_cast<long double>(posting.provider_million));
    total.add(static_cast<long double>(posting.protected_reserve_million));
    total.add(static_cast<long double>(posting.external_million));
    return total.value();
}

[[nodiscard]] long double posting_scale(
    const CapitalCashPosting& posting) noexcept {
    return std::max({1.0L,
        std::abs(static_cast<long double>(posting.sponsor_million)),
        std::abs(static_cast<long double>(
            posting.project_unrestricted_million)),
        std::abs(static_cast<long double>(posting.provider_million)),
        std::abs(static_cast<long double>(posting.protected_reserve_million)),
        std::abs(static_cast<long double>(posting.external_million))});
}

void require_zero(double value, std::string_view description) {
    if (!nearly_equal(static_cast<long double>(value), 0.0L)) {
        throw std::logic_error(
            std::string(description) + " has an unexpected nonzero posting");
    }
}

void require_opposite(double first, double second,
    std::string_view description) {
    if (!nearly_equal(static_cast<long double>(first),
            -static_cast<long double>(second))) {
        throw std::logic_error(
            std::string(description) + " has inconsistent cash accounts");
    }
}

[[nodiscard]] double discount_factor(
    double annual_rate, std::size_t month) {
    if (annual_rate == 0.0 || month == 0U) {
        return 1.0;
    }
    return std::exp(-std::log1p(annual_rate) *
        static_cast<double>(month) / 12.0);
}

struct SourceAccumulator {
    PortfolioCashSource kind{PortfolioCashSource::Commercial};
    std::vector<MonthlyAmount> cash_available{};
};

void add_receipt(ProjectJointPath& project_path,
    std::unordered_map<std::string, SourceAccumulator>& source_accumulators,
    std::string source_id, PortfolioCashSource kind, std::size_t month,
    double amount, double principal_component) {
    project_path.investor_receipts.push_back(InvestorReceipt{
        month, source_id, amount, principal_component});
    const auto [source, inserted] = source_accumulators.try_emplace(
        std::move(source_id), SourceAccumulator{kind, {}});
    if (!inserted && source->second.kind != kind) {
        throw std::logic_error(
            "adapter attempted to reuse a cash-source id across taxonomies");
    }
    source->second.cash_available.push_back(MonthlyAmount{month, amount});
}

void require_expected_draw_posting(const CapitalCashPosting& posting) {
    if (!(posting.provider_million < 0.0)) {
        throw std::logic_error(
            "provider draw must be a negative provider posting");
    }
    require_zero(posting.sponsor_million, "provider draw sponsor account");
    require_opposite(posting.project_unrestricted_million,
        posting.provider_million, "provider draw");
    require_zero(posting.protected_reserve_million,
        "provider draw protected-reserve account");
    require_zero(posting.external_million,
        "provider draw external account");
}

void require_expected_fee_posting(const CapitalCashPosting& posting,
    std::string_view description) {
    if (!(posting.provider_million > 0.0)) {
        throw std::logic_error(
            std::string(description) +
            " must be a positive provider posting");
    }
    require_opposite(posting.sponsor_million, posting.provider_million,
        description);
    require_zero(posting.project_unrestricted_million,
        std::string(description) + " project account");
    require_zero(posting.protected_reserve_million,
        std::string(description) + " protected-reserve account");
    require_zero(posting.external_million,
        std::string(description) + " external account");
}

void require_expected_repayment_posting(
    const CapitalCashPosting& posting) {
    if (!(posting.provider_million > 0.0)) {
        throw std::logic_error(
            "provider repayment must be a positive provider posting");
    }
    require_zero(posting.sponsor_million,
        "provider repayment sponsor account");
    require_opposite(posting.project_unrestricted_million,
        posting.provider_million, "provider repayment");
    require_zero(posting.protected_reserve_million,
        "provider repayment protected-reserve account");
    require_zero(posting.external_million,
        "provider repayment external account");
}

void append_sources(JointScenario& scenario,
    std::unordered_map<std::string, SourceAccumulator>& accumulators) {
    scenario.cash_sources.reserve(accumulators.size());
    for (auto& [id, accumulator] : accumulators) {
        std::sort(accumulator.cash_available.begin(),
            accumulator.cash_available.end(),
            [](const MonthlyAmount& first, const MonthlyAmount& second) {
                if (first.month != second.month) {
                    return first.month < second.month;
                }
                return first.amount_million < second.amount_million;
            });
        scenario.cash_sources.push_back(ScenarioCashSource{
            id, accumulator.kind, std::move(accumulator.cash_available)});
    }
    std::sort(scenario.cash_sources.begin(), scenario.cash_sources.end(),
        [](const ScenarioCashSource& first,
            const ScenarioCashSource& second) {
            return first.id < second.id;
        });
}

void reconcile_adapted_summary(const StagedCapitalSummary& staged_summary,
    const PortfolioConfig& adapted,
    const PortfolioSummary& portfolio_summary) {
    if (staged_summary.cases.size() != portfolio_summary.scenarios.size()) {
        throw std::logic_error(
            "adapted portfolio scenario count failed to reconcile");
    }

    for (const StagedCapitalPathResult& staged_path : staged_summary.cases) {
        const auto matching = std::find_if(portfolio_summary.scenarios.begin(),
            portfolio_summary.scenarios.end(),
            [&staged_path](const JointScenarioResult& candidate) {
                return candidate.scenario_id == staged_path.case_id;
            });
        const auto configured = std::find_if(
            adapted.joint_scenarios.begin(), adapted.joint_scenarios.end(),
            [&staged_path](const JointScenario& candidate) {
                return candidate.id == staged_path.case_id;
            });
        if (matching == portfolio_summary.scenarios.end() ||
            configured == adapted.joint_scenarios.end() ||
            matching->projects.size() != 1U ||
            configured->project_paths.size() != 1U) {
            throw std::logic_error(
                "adapted portfolio path identity failed to reconcile");
        }
        require_reconciliation(configured->weight, staged_path.weight,
            "adapted configured scenario weight");
        require_reconciliation(matching->declared_weight,
            staged_path.weight, "adapted evaluated scenario weight");
        require_reconciliation(matching->total_draws_million,
            staged_path.total_provider_draws_million,
            "adapted path provider draws");
        require_reconciliation(matching->principal_loss_million,
            staged_path.provider_principal_loss_million,
            "adapted path provider principal loss");
        require_reconciliation(matching->outstanding_principal_million, 0.0L,
            "adapted resolved path outstanding principal");
        require_reconciliation(matching->npv_million,
            staged_path.provider_npv_after_upfront_fee_million,
            "adapted path provider NPV");

        const std::size_t month_count = matching->monthly_cash_flows.size();
        std::vector<CompensatedSum> staged_provider_cash(month_count);
        for (const CapitalCashLedgerEntry& entry : staged_path.cash_ledger) {
            if (entry.month >= month_count) {
                throw std::logic_error(
                    "adapted horizon omitted a staged provider cash month");
            }
            staged_provider_cash[entry.month].add(
                static_cast<long double>(entry.posting.provider_million));
        }
        for (std::size_t month = 0U; month < month_count; ++month) {
            require_reconciliation(
                matching->monthly_cash_flows[month].net_cash_flow_million,
                staged_provider_cash[month].value(),
                "adapted monthly provider net cash");
        }

        // Only dated net cash is identical. Portfolio liquidity deliberately
        // funds same-month draws before releasing receipts, so its gross peak
        // need can exceed the staged ledger's posting-order cash low point.
        using SparseMonthlySums =
            std::unordered_map<std::size_t, CompensatedSum>;
        std::unordered_map<std::string, SparseMonthlySums> source_budgets;
        std::unordered_map<std::string, SparseMonthlySums> source_receipts;
        std::unordered_map<std::string, PortfolioCashSource> source_kinds;
        source_budgets.reserve(configured->cash_sources.size());
        source_receipts.reserve(configured->cash_sources.size());
        source_kinds.reserve(configured->cash_sources.size());
        for (const ScenarioCashSource& source : configured->cash_sources) {
            source_budgets.emplace(source.id, SparseMonthlySums{});
            source_receipts.emplace(source.id, SparseMonthlySums{});
            source_kinds.emplace(source.id, source.kind);
            for (const MonthlyAmount& availability :
                 source.cash_available) {
                if (availability.month >= month_count) {
                    throw std::logic_error(
                        "adapted source budget lies outside the horizon");
                }
                source_budgets.at(source.id)[availability.month].add(
                    static_cast<long double>(availability.amount_million));
            }
        }

        std::array<CompensatedSum, kPortfolioCashSourceCount>
            source_nominal{};
        std::array<CompensatedSum, kPortfolioCashSourceCount> source_pv{};
        for (const ProjectJointPath& path : configured->project_paths) {
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                if (receipt.month >= month_count ||
                    !source_receipts.contains(receipt.cash_source_id) ||
                    !source_kinds.contains(receipt.cash_source_id)) {
                    throw std::logic_error(
                        "adapted receipt has no in-horizon identified source budget");
                }
                const long double amount =
                    static_cast<long double>(receipt.amount_million);
                source_receipts.at(receipt.cash_source_id)[receipt.month].add(
                    amount);
                const std::size_t source_index = static_cast<std::size_t>(
                    source_kinds.at(receipt.cash_source_id));
                if (source_index >= kPortfolioCashSourceCount) {
                    throw std::logic_error(
                        "adapted receipt source is outside the portfolio taxonomy");
                }
                source_nominal[source_index].add(amount);
                source_pv[source_index].add(amount *
                    static_cast<long double>(discount_factor(
                        adapted.annual_physical_hurdle_rate,
                        receipt.month)));
            }
        }
        for (const ScenarioCashSource& source : configured->cash_sources) {
            const SparseMonthlySums& budgets =
                source_budgets.at(source.id);
            const SparseMonthlySums& receipts =
                source_receipts.at(source.id);
            for (const auto& [month, budget] : budgets) {
                const auto receipt = receipts.find(month);
                require_reconciliation(budget.value(),
                    receipt == receipts.end() ? 0.0L
                                              : receipt->second.value(),
                    "adapted identified source budget");
            }
            for (const auto& [month, receipt] : receipts) {
                if (!budgets.contains(month)) {
                    require_reconciliation(0.0L, receipt.value(),
                        "adapted identified source budget");
                }
            }
        }
        if (matching->return_sources.size() !=
            kPortfolioCashSourceCount) {
            throw std::logic_error(
                "adapted portfolio source-total taxonomy is incomplete");
        }
        for (const ReturnSourceTotal& total : matching->return_sources) {
            const std::size_t source_index =
                static_cast<std::size_t>(total.source);
            if (source_index >= kPortfolioCashSourceCount) {
                throw std::logic_error(
                    "adapted portfolio source total is outside the taxonomy");
            }
            require_reconciliation(total.nominal_million,
                source_nominal[source_index].value(),
                "adapted nominal return-source total");
            require_reconciliation(total.present_value_million,
                source_pv[source_index].value(),
                "adapted present-value return-source total");
        }
    }

    CompensatedSum weight_sum;
    CompensatedSum actual_path_npv;
    for (const StagedCapitalPathResult& path : staged_summary.cases) {
        weight_sum.add(static_cast<long double>(path.weight));
        actual_path_npv.add(static_cast<long double>(path.weight) *
            static_cast<long double>(
                path.provider_npv_after_upfront_fee_million));
    }
    const long double normalized_actual_path_npv =
        actual_path_npv.value() / weight_sum.value();

    std::array<CompensatedSum, kPortfolioCashSourceCount>
        expected_source_nominal{};
    std::array<CompensatedSum, kPortfolioCashSourceCount>
        expected_source_pv{};
    for (const JointScenario& scenario : adapted.joint_scenarios) {
        const long double normalized_weight =
            static_cast<long double>(scenario.weight) / weight_sum.value();
        std::unordered_map<std::string, PortfolioCashSource> source_kinds;
        source_kinds.reserve(scenario.cash_sources.size());
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            source_kinds.emplace(source.id, source.kind);
        }
        for (const ProjectJointPath& path : scenario.project_paths) {
            for (const InvestorReceipt& receipt : path.investor_receipts) {
                const auto source = source_kinds.find(
                    receipt.cash_source_id);
                if (source == source_kinds.end()) {
                    throw std::logic_error(
                        "adapted expected source receipt has no taxonomy");
                }
                const std::size_t source_index =
                    static_cast<std::size_t>(source->second);
                if (source_index >= kPortfolioCashSourceCount) {
                    throw std::logic_error(
                        "adapted expected source input is outside the taxonomy");
                }
                const long double weighted_amount = normalized_weight *
                    static_cast<long double>(receipt.amount_million);
                expected_source_nominal[source_index].add(weighted_amount);
                expected_source_pv[source_index].add(weighted_amount *
                    static_cast<long double>(discount_factor(
                        adapted.annual_physical_hurdle_rate,
                        receipt.month)));
            }
        }
    }
    if (portfolio_summary.expected_return_sources.size() !=
        kPortfolioCashSourceCount) {
        throw std::logic_error(
            "adapted expected return-source taxonomy is incomplete");
    }
    for (const ReturnSourceTotal& total :
         portfolio_summary.expected_return_sources) {
        const std::size_t source_index =
            static_cast<std::size_t>(total.source);
        if (source_index >= kPortfolioCashSourceCount) {
            throw std::logic_error(
                "adapted expected return source is outside the taxonomy");
        }
        require_reconciliation(total.nominal_million,
            expected_source_nominal[source_index].value(),
            "adapted expected nominal return-source total");
        require_reconciliation(total.present_value_million,
            expected_source_pv[source_index].value(),
            "adapted expected present-value return-source total");
    }

    // The staged summary's charged-fee aggregate is deliberately not used:
    // it is an all-provider-performs fee-replay sensitivity. The portfolio
    // adapter instead preserves and weights the actual configured paths,
    // including provider nonperformance.
    require_reconciliation(portfolio_summary.npv_million.mean,
        normalized_actual_path_npv, "adapted expected actual-path NPV");
    require_reconciliation(portfolio_summary.principal_loss_million.mean,
        staged_summary.expected_provider_principal_loss_million,
        "adapted expected provider principal loss");
    require_reconciliation(
        portfolio_summary.configured_scenario_weight_sum,
        staged_summary.configured_case_weight_sum,
        "adapted configured weight sum");
}

} // namespace

PortfolioConfig adapt_staged_capital_to_portfolio(
    const StagedCapitalConfig& staged, std::string project_id,
    ProjectStage stage,
    const std::vector<StagedCompletionSourceAllocation>&
        completion_source_allocations) {
    validate_staged_capital_config(staged);
    require_project_id_with_source_suffix_capacity(project_id);

    const std::string sponsor_fee_source_id =
        project_id + std::string(kSponsorFeeSuffix);
    const std::string recovery_source_id =
        project_id + std::string(kRecoverySuffix);

    const StagedCapitalSummary staged_summary =
        evaluate_staged_capital_cases(staged);
    const CompletionAllocationMap classified_completion_sources =
        validate_completion_allocations(
            staged_summary, completion_source_allocations,
            sponsor_fee_source_id, recovery_source_id);

    PortfolioConfig adapted;
    adapted.scenario_label = staged.scenario_label;
    adapted.source_note = staged.source_note;
    adapted.currency_label = staged.currency_label;
    adapted.monetary_basis = staged.monetary_basis;
    adapted.synthetic_inputs = staged.synthetic_inputs;
    adapted.horizon_months = 1U;
    adapted.annual_physical_hurdle_rate =
        staged.terms.provider_hurdle_rate;
    adapted.projects.push_back(PortfolioProject{
        project_id, stage, staged.terms.provider_commitment_million});
    adapted.joint_scenarios.reserve(staged_summary.cases.size());

    for (const StagedCapitalPathResult& staged_path : staged_summary.cases) {
        JointScenario scenario;
        scenario.id = staged_path.case_id;
        scenario.weight = staged_path.weight;

        ProjectJointPath project_path;
        project_path.project_id = project_id;
        project_path.resolution = ProjectPathResolution::Resolved;
        std::unordered_map<std::string, SourceAccumulator>
            source_accumulators;

        CompensatedSum translated_draws;
        CompensatedSum translated_fees;
        CompensatedSum translated_repayment;
        CompensatedSum translated_terminal_receipts;
        CompensatedSum translated_npv;
        long double outstanding_principal = 0.0L;
        std::size_t prior_month = 0U;
        bool first_entry = true;

        adapted.horizon_months = std::max({adapted.horizon_months,
            staged_path.outcome_month, staged_path.recovery_month});
        for (const CapitalCashLedgerEntry& entry : staged_path.cash_ledger) {
            if (!first_entry && entry.month < prior_month) {
                throw std::logic_error(
                    "staged-capital ledger is not chronological");
            }
            first_entry = false;
            prior_month = entry.month;
            adapted.horizon_months =
                std::max(adapted.horizon_months, entry.month);

            const long double balance = posting_sum(entry.posting);
            if (std::abs(balance) >
                tolerance_for(posting_scale(entry.posting), 0.0L)) {
                throw std::logic_error(
                    "staged-capital ledger contains an unbalanced cash posting");
            }

            const double provider_cash = entry.posting.provider_million;
            translated_npv.add(static_cast<long double>(provider_cash) *
                static_cast<long double>(discount_factor(
                    staged.terms.provider_hurdle_rate, entry.month)));
            if (provider_cash == 0.0) {
                continue;
            }

            switch (entry.kind) {
            case CapitalCashFlowKind::ProviderDraw: {
                require_expected_draw_posting(entry.posting);
                const double amount = -provider_cash;
                project_path.capital_draws.push_back(
                    MonthlyAmount{entry.month, amount});
                translated_draws.add(static_cast<long double>(amount));
                outstanding_principal += static_cast<long double>(amount);
                break;
            }
            case CapitalCashFlowKind::UpfrontFee:
                if (entry.month != 0U) {
                    throw std::logic_error(
                        "staged-capital upfront fee is not at close");
                }
                [[fallthrough]];
            case CapitalCashFlowKind::CommitmentFee:
                require_expected_fee_posting(entry.posting,
                    entry.kind == CapitalCashFlowKind::UpfrontFee
                        ? "upfront fee"
                        : "commitment fee");
                add_receipt(project_path, source_accumulators,
                    sponsor_fee_source_id, PortfolioCashSource::SponsorFee,
                    entry.month, provider_cash, 0.0);
                translated_fees.add(
                    static_cast<long double>(provider_cash));
                break;
            case CapitalCashFlowKind::ProviderRepayment: {
                require_expected_repayment_posting(entry.posting);
                if (entry.month != staged_path.recovery_month) {
                    throw std::logic_error(
                        "provider repayment is not at the staged path recovery month");
                }
                const bool completed = staged_path.outcome ==
                    StagedCapitalOutcome::Completed;
                if (completed) {
                    const auto& allocations =
                        classified_completion_sources.at(
                            staged_path.case_id);
                    CompensatedSum allocation_total;
                    for (const StagedCompletionSourceAllocation& allocation :
                         allocations) {
                        allocation_total.add(static_cast<long double>(
                            allocation.amount_million));
                    }
                    const long double total_allocated =
                        allocation_total.value();
                    const long double principal_total = std::min(
                        total_allocated, outstanding_principal);
                    const long double principal_ratio =
                        principal_total / total_allocated;
                    std::vector<long double> principal_allocations;
                    principal_allocations.reserve(allocations.size());
                    CompensatedSum assigned_principal;
                    for (const StagedCompletionSourceAllocation& allocation :
                         allocations) {
                        const long double principal =
                            static_cast<long double>(
                                allocation.amount_million) *
                            principal_ratio;
                        principal_allocations.push_back(principal);
                        assigned_principal.add(principal);
                    }
                    const auto largest_allocation = std::max_element(
                        allocations.begin(), allocations.end(),
                        [](const StagedCompletionSourceAllocation& first,
                            const StagedCompletionSourceAllocation& second) {
                            if (first.amount_million !=
                                second.amount_million) {
                                return first.amount_million <
                                    second.amount_million;
                            }
                            return first.cash_source_id >
                                second.cash_source_id;
                        });
                    const std::size_t residual_index =
                        static_cast<std::size_t>(std::distance(
                            allocations.begin(), largest_allocation));
                    principal_allocations[residual_index] +=
                        principal_total - assigned_principal.value();
                    const long double residual_limit =
                        static_cast<long double>(
                            allocations[residual_index].amount_million);
                    const long double residual_tolerance = tolerance_for(
                        principal_allocations[residual_index],
                        residual_limit);
                    if (principal_allocations[residual_index] <
                            -residual_tolerance ||
                        principal_allocations[residual_index] >
                            residual_limit + residual_tolerance) {
                        throw std::logic_error(
                            "pro-rata completion principal allocation exceeded its receipt");
                    }
                    principal_allocations[residual_index] = std::clamp(
                        principal_allocations[residual_index], 0.0L,
                        residual_limit);
                    for (std::size_t index = 0U;
                         index < allocations.size(); ++index) {
                        const StagedCompletionSourceAllocation& allocation =
                            allocations[index];
                        add_receipt(project_path, source_accumulators,
                            allocation.cash_source_id,
                            allocation.source, entry.month,
                            allocation.amount_million,
                            to_double(principal_allocations[index]));
                        translated_terminal_receipts.add(
                            static_cast<long double>(
                                allocation.amount_million));
                    }
                    outstanding_principal = std::max(
                        0.0L, outstanding_principal - principal_total);
                } else {
                    const long double principal = std::min(
                        static_cast<long double>(provider_cash),
                        outstanding_principal);
                    outstanding_principal = std::max(
                        0.0L, outstanding_principal - principal);
                    add_receipt(project_path, source_accumulators,
                        recovery_source_id, PortfolioCashSource::Recovery,
                        entry.month, provider_cash, to_double(principal));
                    translated_terminal_receipts.add(
                        static_cast<long double>(provider_cash));
                }
                translated_repayment.add(
                    static_cast<long double>(provider_cash));
                break;
            }
            default:
                throw std::logic_error(
                    "unsupported nonzero provider posting in staged-capital ledger");
            }
        }

        require_reconciliation(translated_draws.value(),
            staged_path.total_provider_draws_million,
            "translated provider draws");
        require_reconciliation(translated_fees.value(),
            static_cast<long double>(staged.terms.upfront_fee_million) +
                static_cast<long double>(
                    staged_path.total_commitment_fees_million),
            "translated provider fees");
        require_reconciliation(translated_repayment.value(),
            staged_path.provider_nominal_recovery_million,
            "translated provider repayment");
        require_reconciliation(translated_terminal_receipts.value(),
            staged_path.provider_nominal_recovery_million,
            "allocated terminal provider receipts");
        require_reconciliation(outstanding_principal,
            staged_path.provider_principal_loss_million,
            "translated provider principal loss");
        require_reconciliation(translated_npv.value(),
            staged_path.provider_npv_after_upfront_fee_million,
            "translated provider NPV");

        std::sort(project_path.capital_draws.begin(),
            project_path.capital_draws.end(),
            [](const MonthlyAmount& first, const MonthlyAmount& second) {
                if (first.month != second.month) {
                    return first.month < second.month;
                }
                return first.amount_million < second.amount_million;
            });
        std::sort(project_path.investor_receipts.begin(),
            project_path.investor_receipts.end(),
            [](const InvestorReceipt& first, const InvestorReceipt& second) {
                if (first.month != second.month) {
                    return first.month < second.month;
                }
                if (first.cash_source_id != second.cash_source_id) {
                    return first.cash_source_id < second.cash_source_id;
                }
                return first.amount_million < second.amount_million;
            });
        append_sources(scenario, source_accumulators);
        scenario.project_paths.push_back(std::move(project_path));
        adapted.joint_scenarios.push_back(std::move(scenario));
    }

    validate_portfolio_config(adapted);
    const PortfolioSummary portfolio_summary = evaluate_portfolio(adapted);
    reconcile_adapted_summary(staged_summary, adapted, portfolio_summary);
    return adapted;
}

} // namespace naturalehia::cellular_finance
