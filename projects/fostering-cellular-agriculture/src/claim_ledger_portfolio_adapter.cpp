// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_portfolio_adapter.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr double kAbsoluteTolerance = 1.0e-10;
constexpr std::size_t kMaximumAllocations = 1'000'000U;
constexpr std::size_t kMaximumCashBudgets = 1'000'000U;
constexpr std::size_t kMaximumIdentifierLength = 128U;

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

[[nodiscard]] double checked_double(long double value) {
    const double result = static_cast<double>(value);
    if (!std::isfinite(result)) {
        throw std::overflow_error(
            "claim-ledger portfolio adapter exceeded numeric range");
    }
    return result;
}

[[nodiscard]] bool is_ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !is_ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return is_ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

[[nodiscard]] double exact_value(
    const ClaimLedgerValue& value, std::string_view label) {
    if (value.status != ClaimLedgerValueStatus::Known ||
        !value.lower.has_value() || !value.upper.has_value() ||
        !nearly_equal(*value.lower, *value.upper)) {
        throw std::invalid_argument(
            std::string(label) + " must be an exact known value");
    }
    return *value.lower;
}

[[nodiscard]] bool receipt_kind(ClaimLedgerEntryKind kind) noexcept {
    switch (kind) {
    case ClaimLedgerEntryKind::CashFee:
    case ClaimLedgerEntryKind::PrincipalCash:
    case ClaimLedgerEntryKind::InterestCash:
    case ClaimLedgerEntryKind::RecoveryPrincipalCash:
    case ClaimLedgerEntryKind::RecoveryInterestCash:
    case ClaimLedgerEntryKind::GuaranteePrincipalCash:
    case ClaimLedgerEntryKind::GuaranteeInterestCash:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool principal_receipt_kind(
    ClaimLedgerEntryKind kind) noexcept {
    return kind == ClaimLedgerEntryKind::PrincipalCash ||
        kind == ClaimLedgerEntryKind::RecoveryPrincipalCash ||
        kind == ClaimLedgerEntryKind::GuaranteePrincipalCash;
}

[[nodiscard]] bool guarantee_receipt_kind(
    ClaimLedgerEntryKind kind) noexcept {
    return kind == ClaimLedgerEntryKind::GuaranteePrincipalCash ||
        kind == ClaimLedgerEntryKind::GuaranteeInterestCash;
}

[[nodiscard]] bool allowed_source(
    ClaimLedgerEntryKind kind, PortfolioCashSource source) noexcept {
    if (kind == ClaimLedgerEntryKind::CashFee) {
        return source == PortfolioCashSource::FinancingFee;
    }
    if (kind == ClaimLedgerEntryKind::RecoveryPrincipalCash ||
        kind == ClaimLedgerEntryKind::RecoveryInterestCash) {
        return source == PortfolioCashSource::Recovery;
    }
    if (kind == ClaimLedgerEntryKind::GuaranteePrincipalCash ||
        kind == ClaimLedgerEntryKind::GuaranteeInterestCash) {
        return source == PortfolioCashSource::ExplicitSupport;
    }
    return source == PortfolioCashSource::Commercial ||
        source == PortfolioCashSource::LicensingRoyalty ||
        source == PortfolioCashSource::ExitSale ||
        source == PortfolioCashSource::Refinancing ||
        source == PortfolioCashSource::ExplicitSupport;
}

struct SelectedEntry {
    const ClaimLedgerEntry* entry{};
    std::optional<std::string> scenario_id{};
};

void append_nonreceipt_lineage(
    std::vector<ClaimLedgerPortfolioCashLineage>& lineage,
    std::string_view portfolio_scenario_id,
    const SelectedEntry& selected, ClaimLedgerPortfolioOutputKind output_kind,
    std::string_view portfolio_project_id, double amount) {
    const ClaimLedgerEntry& entry = *selected.entry;
    lineage.push_back(ClaimLedgerPortfolioCashLineage{
        std::string(portfolio_scenario_id), selected.scenario_id,
        entry.entry_id, entry.economic_fact_id, entry.event_group_id,
        entry.kind, entry.known_at_period, entry.source_record_id,
        entry.provider_claim_id, output_kind,
        std::string(portfolio_project_id), std::nullopt, entry.period,
        amount, std::nullopt});
}

[[nodiscard]] std::vector<SelectedEntry> selected_entries(
    const ClaimLedgerConfig& ledger, const ClaimLedgerScenario& scenario) {
    std::unordered_map<std::string, SelectedEntry> latest;
    const auto consider = [&](const ClaimLedgerEntry& entry,
                              std::optional<std::string> scenario_id) {
        if (entry.known_at_period > ledger.decision_period) return;
        const auto found = latest.find(entry.economic_fact_id);
        if (found == latest.end() || entry.known_at_period >
                found->second.entry->known_at_period) {
            latest[entry.economic_fact_id] =
                SelectedEntry{&entry, std::move(scenario_id)};
        }
    };
    for (const ClaimLedgerEntry& entry : ledger.common_entries) {
        consider(entry, std::nullopt);
    }
    for (const ClaimLedgerEntry& entry : scenario.entries) {
        consider(entry, scenario.scenario_id);
    }
    std::vector<SelectedEntry> result;
    result.reserve(latest.size());
    for (const auto& [fact_id, selected] : latest) {
        static_cast<void>(fact_id);
        result.push_back(selected);
    }
    std::sort(result.begin(), result.end(),
        [](const SelectedEntry& first, const SelectedEntry& second) {
            if (first.entry->period != second.entry->period) {
                return first.entry->period < second.entry->period;
            }
            return first.entry->entry_id < second.entry->entry_id;
        });
    return result;
}

[[nodiscard]] std::string allocation_key(
    const std::optional<std::string>& scenario_id,
    std::string_view entry_id) {
    std::string result;
    result.reserve((scenario_id.has_value() ? scenario_id->size() : 0U) +
        entry_id.size() + 3U);
    // The leading scope tag makes a literal scenario id such as "COMMON"
    // categorically distinct from a null/common allocation scope.
    result.push_back(scenario_id.has_value() ? 'S' : 'C');
    result.push_back('\x1f');
    if (scenario_id.has_value()) {
        result.append(*scenario_id);
    }
    result.push_back('\x1f');
    result.append(entry_id);
    return result;
}

struct AllocationReference {
    const ClaimLedgerReceiptSourceAllocation* allocation{};
    std::size_t original_index{};
};

using AllocationIndex =
    std::unordered_map<std::string, std::vector<AllocationReference>>;

struct SourceAccumulator {
    PortfolioCashSource kind{PortfolioCashSource::Commercial};
    std::string counterparty_id{};
    std::optional<std::string> provider_claim_id{};
    std::vector<MonthlyAmount> cash_available{};
    long double allocated_receipts{};
};

using ProviderIdentityIndex = std::unordered_map<std::string, std::string>;

using CashBudgetPointers =
    std::vector<const ClaimLedgerScenarioCashBudget*>;

struct CashBudgetIndex {
    CashBudgetPointers common{};
    std::unordered_map<std::string, CashBudgetPointers> by_scenario{};
    std::size_t expanded_lineage_size{0U};
};

[[nodiscard]] ProviderIdentityIndex provider_identities(
    const ClaimLedgerConfig& ledger) {
    ProviderIdentityIndex result;
    result.reserve(ledger.provider_claims.size());
    for (const ClaimLedgerProviderClaim& provider : ledger.provider_claims) {
        result.emplace(provider.provider_claim_id, provider.provider_id);
    }
    return result;
}

[[nodiscard]] AllocationIndex validate_allocations(
    const ClaimLedgerConfig& ledger,
    const ClaimLedgerPortfolioAdapterTerms& terms,
    std::vector<bool>& used) {
    if (terms.receipt_source_allocations.size() > kMaximumAllocations) {
        throw std::invalid_argument(
            "claim-ledger receipt allocation count exceeds the resource guardrail");
    }
    std::unordered_set<std::string> scenario_ids;
    for (const ClaimLedgerScenario& scenario : ledger.scenarios) {
        scenario_ids.insert(scenario.scenario_id);
    }
    std::unordered_map<std::string, PortfolioCashSource> source_taxonomies;
    AllocationIndex index;
    used.assign(terms.receipt_source_allocations.size(), false);
    for (std::size_t position = 0U;
         position < terms.receipt_source_allocations.size(); ++position) {
        const auto& allocation = terms.receipt_source_allocations[position];
        if (allocation.scenario_id.has_value() &&
            !scenario_ids.contains(*allocation.scenario_id)) {
            throw std::invalid_argument(
                "receipt allocation names an unknown claim-ledger scenario");
        }
        if (!std::isfinite(allocation.amount_million) ||
            !(allocation.amount_million > 0.0)) {
            throw std::invalid_argument(
                "receipt allocation amount must be finite and positive");
        }
        const auto [taxonomy, inserted] = source_taxonomies.emplace(
            allocation.cash_source_id, allocation.source);
        if (!inserted && taxonomy->second != allocation.source) {
            throw std::invalid_argument(
                "one receipt cash-source id changes taxonomy across allocations");
        }
        index[allocation_key(
                  allocation.scenario_id, allocation.entry_id)]
            .push_back(AllocationReference{&allocation, position});
    }
    return index;
}

[[nodiscard]] CashBudgetIndex validate_cash_budgets(
    const ClaimLedgerConfig& ledger,
    const ClaimLedgerPortfolioAdapterTerms& terms,
    const ProviderIdentityIndex& providers) {
    if (terms.cash_source_budgets.size() > kMaximumCashBudgets) {
        throw std::invalid_argument(
            "claim-ledger cash-budget count exceeds the resource guardrail");
    }
    std::unordered_set<std::string> scenario_ids;
    for (const ClaimLedgerScenario& scenario : ledger.scenarios) {
        scenario_ids.insert(scenario.scenario_id);
    }
    struct Identity {
        PortfolioCashSource source{};
        std::string counterparty_id{};
        std::optional<std::string> provider_claim_id{};
    };
    std::unordered_map<std::string, Identity> identities;
    CashBudgetIndex index;
    index.by_scenario.reserve(ledger.scenarios.size());
    for (const ClaimLedgerScenarioCashBudget& budget :
         terms.cash_source_budgets) {
        if (budget.scenario_id.has_value() &&
            !scenario_ids.contains(*budget.scenario_id)) {
            throw std::invalid_argument(
                "cash budget names an unknown claim-ledger scenario");
        }
        if (!safe_identifier(budget.cash_source_id) ||
            !safe_identifier(budget.counterparty_id)) {
            throw std::invalid_argument(
                "cash budget source and counterparty must be safe identifiers");
        }
        if (budget.month > ledger.horizon_period) {
            throw std::invalid_argument(
                "cash budget falls outside the claim-ledger horizon");
        }
        if (!std::isfinite(budget.amount_million) ||
            !(budget.amount_million > 0.0)) {
            throw std::invalid_argument(
                "cash budget amount must be finite and positive");
        }
        if (budget.provider_claim_id.has_value()) {
            const auto found = providers.find(*budget.provider_claim_id);
            if (found == providers.end()) {
                throw std::invalid_argument(
                    "cash budget names an unknown guarantee provider claim");
            }
            if (budget.source != PortfolioCashSource::ExplicitSupport ||
                budget.counterparty_id != found->second) {
                throw std::invalid_argument(
                    "guarantee cash budget taxonomy and counterparty must match its provider claim");
            }
        }
        const Identity identity{budget.source, budget.counterparty_id,
            budget.provider_claim_id};
        const auto [found, inserted] = identities.emplace(
            budget.cash_source_id, identity);
        if (!inserted &&
            (found->second.source != identity.source ||
                found->second.counterparty_id != identity.counterparty_id ||
                found->second.provider_claim_id !=
                    identity.provider_claim_id)) {
            throw std::invalid_argument(
                "one cash-source id changes taxonomy, counterparty, or provider identity");
        }
        if (budget.scenario_id.has_value()) {
            index.by_scenario[*budget.scenario_id].push_back(&budget);
        } else {
            index.common.push_back(&budget);
        }
    }

    const std::size_t scenario_count = ledger.scenarios.size();
    const std::size_t scenario_specific_count =
        terms.cash_source_budgets.size() - index.common.size();
    if (scenario_specific_count > kMaximumCashBudgets ||
        (!index.common.empty() &&
            (scenario_count == 0U ||
                index.common.size() >
                    (kMaximumCashBudgets - scenario_specific_count) /
                        scenario_count))) {
        throw std::invalid_argument(
            "expanded claim-ledger cash-budget lineage exceeds the resource guardrail");
    }
    index.expanded_lineage_size = scenario_specific_count +
        index.common.size() * scenario_count;
    return index;
}

template <typename Operation>
void for_each_applicable_budget(const std::string& scenario_id,
    const CashBudgetIndex& index, Operation&& operation) {
    for (const ClaimLedgerScenarioCashBudget* budget : index.common) {
        operation(*budget);
    }
    const auto scenario_budgets = index.by_scenario.find(scenario_id);
    if (scenario_budgets == index.by_scenario.end()) {
        return;
    }
    for (const ClaimLedgerScenarioCashBudget* budget :
         scenario_budgets->second) {
        operation(*budget);
    }
}

void populate_cash_budgets(const std::string& scenario_id,
    const CashBudgetIndex& index,
    std::unordered_map<std::string, SourceAccumulator>& sources) {
    for_each_applicable_budget(scenario_id, index,
        [&](const ClaimLedgerScenarioCashBudget& budget) {
        const auto [source, inserted] = sources.try_emplace(
            budget.cash_source_id,
            SourceAccumulator{budget.source, budget.counterparty_id,
                budget.provider_claim_id, {}, 0.0L});
        if (!inserted &&
            (source->second.kind != budget.source ||
                source->second.counterparty_id != budget.counterparty_id ||
                source->second.provider_claim_id !=
                    budget.provider_claim_id)) {
            throw std::logic_error(
                "validated cash budget changed identity within a scenario");
        }
        source->second.cash_available.push_back(
            MonthlyAmount{budget.month, budget.amount_million});
        });
}

void append_cash_budget_lineage(const std::string& portfolio_scenario_id,
    const CashBudgetIndex& index,
    std::vector<ClaimLedgerPortfolioCashBudgetLineage>& lineage) {
    for_each_applicable_budget(portfolio_scenario_id, index,
        [&](const ClaimLedgerScenarioCashBudget& budget) {
        lineage.push_back(ClaimLedgerPortfolioCashBudgetLineage{
            portfolio_scenario_id, budget.scenario_id,
            budget.cash_source_id, budget.source, budget.month,
            budget.amount_million, budget.counterparty_id,
            budget.provider_claim_id});
        });
}

[[nodiscard]] std::unordered_map<std::string, std::vector<std::string>>
validate_factor_sets(const ClaimLedgerConfig& ledger,
    const ClaimLedgerPortfolioAdapterTerms& terms) {
    std::unordered_set<std::string> scenario_ids;
    for (const ClaimLedgerScenario& scenario : ledger.scenarios) {
        scenario_ids.insert(scenario.scenario_id);
    }
    std::unordered_map<std::string, std::vector<std::string>> result;
    for (const ClaimLedgerScenarioFactorSet& factors :
         terms.scenario_factor_sets) {
        if (!scenario_ids.contains(factors.scenario_id)) {
            throw std::invalid_argument(
                "factor set names an unknown claim-ledger scenario");
        }
        if (!result.emplace(factors.scenario_id, factors.factor_tags).second) {
            throw std::invalid_argument(
                "duplicate claim-ledger scenario factor set");
        }
    }
    return result;
}

struct ReceiptWork {
    const ClaimLedgerEntry* entry{};
    std::optional<std::string> claim_scenario_id{};
    double amount{};
    std::vector<AllocationReference> allocations{};
    long double principal_component{};
};

void append_receipt_allocations(ReceiptWork& receipt,
    ProjectJointPath& project_path,
    std::unordered_map<std::string, SourceAccumulator>& sources,
    std::vector<bool>& used, std::string_view portfolio_scenario_id,
    std::string_view portfolio_project_id,
    std::vector<ClaimLedgerPortfolioCashLineage>& lineage) {
    long double allocated = 0.0L;
    for (const AllocationReference& reference : receipt.allocations) {
        allocated += static_cast<long double>(
            reference.allocation->amount_million);
    }
    if (!nearly_equal(allocated,
            static_cast<long double>(receipt.amount))) {
        throw std::invalid_argument(
            "receipt source allocations do not exhaust entry: " +
            receipt.entry->entry_id);
    }
    long double assigned_principal = 0.0L;
    std::size_t residual_index = 0U;
    for (std::size_t index = 1U; index < receipt.allocations.size(); ++index) {
        const auto* current = receipt.allocations[index].allocation;
        const auto* residual = receipt.allocations[residual_index].allocation;
        if (current->amount_million > residual->amount_million ||
            (current->amount_million == residual->amount_million &&
                current->cash_source_id < residual->cash_source_id)) {
            residual_index = index;
        }
    }
    std::vector<long double> principal(receipt.allocations.size(), 0.0L);
    for (std::size_t index = 0U; index < receipt.allocations.size(); ++index) {
        principal[index] = static_cast<long double>(
            receipt.allocations[index].allocation->amount_million) /
            allocated * receipt.principal_component;
        assigned_principal += principal[index];
    }
    principal[residual_index] +=
        receipt.principal_component - assigned_principal;

    for (std::size_t index = 0U; index < receipt.allocations.size(); ++index) {
        const AllocationReference& reference = receipt.allocations[index];
        const auto& allocation = *reference.allocation;
        if (!allowed_source(receipt.entry->kind, allocation.source)) {
            throw std::invalid_argument(
                "receipt allocation uses a source taxonomy incompatible with entry: " +
                receipt.entry->entry_id);
        }
        const double principal_component = checked_double(std::clamp(
            principal[index], 0.0L,
            static_cast<long double>(allocation.amount_million)));
        project_path.investor_receipts.push_back(InvestorReceipt{
            receipt.entry->period, allocation.cash_source_id,
            allocation.amount_million, principal_component});
        const auto source = sources.find(allocation.cash_source_id);
        if (source == sources.end()) {
            throw std::invalid_argument(
                "receipt allocation references no externally supplied cash budget: " +
                allocation.cash_source_id);
        }
        if (source->second.kind != allocation.source) {
            throw std::invalid_argument(
                "receipt allocation taxonomy differs from its cash budget");
        }
        const bool guarantee_receipt =
            guarantee_receipt_kind(receipt.entry->kind);
        const bool provider_bound_budget =
            source->second.provider_claim_id.has_value();
        if (guarantee_receipt != provider_bound_budget ||
            (guarantee_receipt &&
                *source->second.provider_claim_id !=
                    receipt.entry->provider_claim_id)) {
            throw std::invalid_argument(
                "receipt and cash budget do not preserve the exact guarantee-provider boundary");
        }
        source->second.allocated_receipts +=
            static_cast<long double>(allocation.amount_million);
        used[reference.original_index] = true;
        lineage.push_back(ClaimLedgerPortfolioCashLineage{
            std::string(portfolio_scenario_id),
            receipt.claim_scenario_id, receipt.entry->entry_id,
            receipt.entry->economic_fact_id,
            receipt.entry->event_group_id, receipt.entry->kind,
            receipt.entry->known_at_period,
            receipt.entry->source_record_id,
            receipt.entry->provider_claim_id,
            ClaimLedgerPortfolioOutputKind::InvestorReceipt,
            std::string(portfolio_project_id), allocation.cash_source_id,
            receipt.entry->period, allocation.amount_million,
            principal_component, source->second.counterparty_id,
            source->second.provider_claim_id});
    }
}

void require_exact_budget_exhaustion(
    const std::unordered_map<std::string, SourceAccumulator>& sources) {
    for (const auto& [id, source] : sources) {
        long double available = 0.0L;
        for (const MonthlyAmount& cash : source.cash_available) {
            available += static_cast<long double>(cash.amount_million);
        }
        if (!nearly_equal(available, source.allocated_receipts)) {
            throw std::invalid_argument(
                "cash budget is not exhausted exactly by adapted receipts: " +
                id);
        }
    }
}

void append_sources(JointScenario& scenario,
    std::unordered_map<std::string, SourceAccumulator>& sources) {
    for (auto& [id, source] : sources) {
        std::sort(source.cash_available.begin(), source.cash_available.end(),
            [](const MonthlyAmount& first, const MonthlyAmount& second) {
                if (first.month != second.month) {
                    return first.month < second.month;
                }
                return first.amount_million < second.amount_million;
            });
        scenario.cash_sources.push_back(ScenarioCashSource{
            id, source.kind, std::move(source.cash_available)});
    }
    std::sort(scenario.cash_sources.begin(), scenario.cash_sources.end(),
        [](const ScenarioCashSource& first,
            const ScenarioCashSource& second) {
            return first.id < second.id;
        });
}

[[nodiscard]] const ClaimLedgerScenarioResult& ledger_result(
    const ClaimLedgerSummary& summary, std::string_view scenario_id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [scenario_id](const auto& scenario) {
            return scenario.scenario_id == scenario_id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error(
            "claim-ledger evaluation lost a configured scenario");
    }
    return *found;
}

[[nodiscard]] const JointScenarioResult& portfolio_result(
    const PortfolioSummary& summary, std::string_view scenario_id) {
    const auto found = std::find_if(summary.scenarios.begin(),
        summary.scenarios.end(), [scenario_id](const auto& scenario) {
            return scenario.scenario_id == scenario_id;
        });
    if (found == summary.scenarios.end()) {
        throw std::logic_error(
            "portfolio evaluation lost an adapted scenario");
    }
    return *found;
}

[[nodiscard]] bool lower_hex_sha256(std::string_view value) noexcept {
    return value.size() == 64U &&
        std::all_of(value.begin(), value.end(), [](char character) {
            return (character >= '0' && character <= '9') ||
                (character >= 'a' && character <= 'f');
        });
}

void require_decision_only_core(const ClaimLedgerConfig& core) {
    const std::size_t cut = core.decision_period;
    if (core.face_amount_known_at_period > cut ||
        core.opening_principal_known_at_period > cut ||
        core.opening_accrued_interest_known_at_period > cut) {
        throw std::logic_error(
            "verified claim package exposed a post-decision core scalar");
    }
    const auto require_entry = [cut](const ClaimLedgerEntry& entry) {
        if (entry.known_at_period > cut) {
            throw std::logic_error(
                "verified claim package exposed a post-decision core entry");
        }
    };
    const auto require_covenant = [cut](
                                      const ClaimLedgerCovenantEvent& event) {
        if (event.known_at_period > cut) {
            throw std::logic_error(
                "verified claim package exposed a post-decision covenant");
        }
    };
    for (const ClaimLedgerEntry& entry : core.common_entries) {
        require_entry(entry);
    }
    for (const ClaimLedgerCovenantEvent& event :
         core.common_covenant_events) {
        require_covenant(event);
    }
    for (const ClaimLedgerProviderClaim& provider : core.provider_claims) {
        if (provider.known_at_period > cut) {
            throw std::logic_error(
                "verified claim package exposed post-decision provider terms");
        }
    }
    for (const ClaimLedgerScenario& scenario : core.scenarios) {
        if (scenario.probability_known_at_period > cut ||
            scenario.cash_path_status_known_at_period > cut) {
            throw std::logic_error(
                "verified claim package exposed post-decision scenario metadata");
        }
        for (const ClaimLedgerEntry& entry : scenario.entries) {
            require_entry(entry);
        }
        for (const ClaimLedgerCovenantEvent& event :
             scenario.covenant_events) {
            require_covenant(event);
        }
    }
}

void require_verified_package_core(const ClaimLedgerPackage& package) {
    if (!package.package_integrity || !package.core_config_ready ||
        !package.core_config.has_value() || !package.evaluation.has_value() ||
        !package.evaluation->readiness.expected_cash_ready) {
        throw std::invalid_argument(
            "claim-ledger package lacks a verified ready decision core");
    }
    const ClaimLedgerConfig& core = *package.core_config;
    if (core.model_version != package.config.model_version ||
        core.ledger_id != package.config.package_id ||
        core.claim_id != package.config.claim_id ||
        core.project_id != package.config.project_id ||
        !package.config.decision_period.has_value() ||
        !package.config.horizon_period.has_value() ||
        core.decision_period != *package.config.decision_period ||
        core.horizon_period != *package.config.horizon_period ||
        core.currency_label != package.config.currency_label ||
        core.monetary_basis != package.config.monetary_basis) {
        throw std::logic_error(
            "verified claim-ledger package decision core identity is inconsistent");
    }
    require_decision_only_core(core);
}

} // namespace

ClaimLedgerPortfolioAdapterResult adapt_claim_ledger_to_portfolio(
    const ClaimLedgerConfig& ledger,
    const ClaimLedgerPortfolioAdapterTerms& terms) {
    const ClaimLedgerSummary claim_summary = evaluate_claim_ledger(ledger);
    if (!claim_summary.readiness.expected_cash_ready) {
        throw std::invalid_argument(
            "claim-ledger portfolio adapter requires an exact complete decision-cut expected-cash ledger");
    }
    if (ledger.horizon_period == 0U) {
        throw std::invalid_argument(
            "claim-ledger portfolio adapter requires a positive monthly horizon");
    }
    if (ledger.period_unit_label != "month" ||
        ledger.periods_per_year != 12U) {
        throw std::invalid_argument(
            "claim-ledger portfolio adapter requires monthly periods with 12 periods per year");
    }
    const double opening_principal = exact_value(
        ledger.opening_principal_million, "opening principal");
    const double opening_interest = exact_value(
        ledger.opening_accrued_interest_million,
        "opening accrued interest");
    if (!nearly_equal(opening_interest, 0.0L)) {
        throw std::invalid_argument(
            "claim-ledger portfolio adapter v0.2 requires zero opening accrued interest because accrued-interest exposure is not represented in Portfolio");
    }
    const double principal_limit = exact_value(
        ledger.contractual_face_amount_million,
        "contractual face amount");

    std::vector<bool> allocation_used;
    const AllocationIndex allocations =
        validate_allocations(ledger, terms, allocation_used);
    const ProviderIdentityIndex providers = provider_identities(ledger);
    const CashBudgetIndex cash_budgets =
        validate_cash_budgets(ledger, terms, providers);
    const auto factor_sets = validate_factor_sets(ledger, terms);

    CompensatedSum raw_probability_sum;
    for (const ClaimLedgerScenario& scenario : ledger.scenarios) {
        raw_probability_sum.add(static_cast<long double>(exact_value(
            scenario.physical_probability, "scenario probability")));
    }
    const long double raw_probability_total = raw_probability_sum.value();
    if (!std::isfinite(raw_probability_total) ||
        !(raw_probability_total > 0.0L)) {
        throw std::logic_error(
            "claim-ledger ready state has no positive finite probability measure");
    }

    ClaimLedgerPortfolioAdapterResult result;
    PortfolioConfig& portfolio = result.portfolio;
    portfolio.scenario_label =
        "Claim-ledger decision-cut projection: " + ledger.ledger_id;
    portfolio.source_note =
        "Mechanical claim-ledger adapter output; accepted claim probabilities are normalized to a unit-sum Portfolio measure and evidence is not promoted to empirical calibration";
    portfolio.currency_label = ledger.currency_label;
    portfolio.monetary_basis = ledger.monetary_basis;
    portfolio.synthetic_inputs = true;
    portfolio.horizon_months = ledger.horizon_period;
    portfolio.annual_physical_hurdle_rate =
        terms.annual_physical_hurdle_rate;
    portfolio.joint_scenarios.reserve(ledger.scenarios.size());
    result.cash_budget_lineage.reserve(
        cash_budgets.expanded_lineage_size);

    long double maximum_investor_outlay = 0.0L;
    for (const ClaimLedgerScenario& claim_scenario : ledger.scenarios) {
        JointScenario scenario;
        scenario.id = claim_scenario.scenario_id;
        const double raw_probability = exact_value(
            claim_scenario.physical_probability, "scenario probability");
        scenario.weight = checked_double(
            static_cast<long double>(raw_probability) /
            raw_probability_total);
        const auto factors = factor_sets.find(claim_scenario.scenario_id);
        if (factors != factor_sets.end()) {
            scenario.factor_tags = factors->second;
        }

        ProjectJointPath project_path;
        project_path.project_id = terms.project_id;
        project_path.resolution = ProjectPathResolution::Resolved;
        std::unordered_map<std::string, SourceAccumulator> sources;
        populate_cash_budgets(
            claim_scenario.scenario_id, cash_budgets, sources);
        append_cash_budget_lineage(claim_scenario.scenario_id, cash_budgets,
            result.cash_budget_lineage);
        std::vector<ReceiptWork> receipt_work;
        const std::vector<SelectedEntry> selected =
            selected_entries(ledger, claim_scenario);
        receipt_work.reserve(selected.size());

        long double path_buyer_price = 0.0L;
        long double path_direct_cost = 0.0L;
        for (const SelectedEntry& selected_entry : selected) {
            const ClaimLedgerEntry& entry = *selected_entry.entry;
            const double amount = exact_value(
                entry.value, "selected claim-ledger entry");
            if (entry.kind == ClaimLedgerEntryKind::BuyerPrice) {
                if (amount > 0.0) {
                    project_path.investor_outlays.push_back(InvestorOutlay{
                        entry.period,
                        InvestorOutlayPurpose::ClaimPurchasePrice, amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::InvestorOutlay,
                        terms.project_id, amount);
                    path_buyer_price += static_cast<long double>(amount);
                }
            } else if (entry.kind ==
                ClaimLedgerEntryKind::BuyerDirectCost) {
                if (amount > 0.0) {
                    project_path.investor_outlays.push_back(InvestorOutlay{
                        entry.period,
                        InvestorOutlayPurpose::BuyerDirectCost, amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::InvestorOutlay,
                        terms.project_id, amount);
                    path_direct_cost += static_cast<long double>(amount);
                }
            } else if (entry.kind ==
                ClaimLedgerEntryKind::FundedPrincipal) {
                if (amount > 0.0) {
                    project_path.principal_movements.push_back(
                        PrincipalMovement{entry.period,
                            PrincipalMovementKind::FundedPrincipalAddition,
                            amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::PrincipalMovement,
                        terms.project_id, amount);
                }
            } else if (entry.kind ==
                ClaimLedgerEntryKind::CapitalizedFee) {
                if (amount > 0.0) {
                    project_path.principal_movements.push_back(
                        PrincipalMovement{entry.period,
                            PrincipalMovementKind::CapitalizedFeeAddition,
                            amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::PrincipalMovement,
                        terms.project_id, amount);
                }
            } else if (entry.kind ==
                ClaimLedgerEntryKind::CapitalizedInterest) {
                if (amount > 0.0) {
                    project_path.principal_movements.push_back(
                        PrincipalMovement{entry.period,
                            PrincipalMovementKind::CapitalizedInterestAddition,
                            amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::PrincipalMovement,
                        terms.project_id, amount);
                }
            } else if (entry.kind ==
                ClaimLedgerEntryKind::ConversionPrincipalExtinguishment) {
                if (amount > 0.0) {
                    project_path.principal_movements.push_back(
                        PrincipalMovement{entry.period,
                            PrincipalMovementKind::ConversionExtinguishment,
                            amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::PrincipalMovement,
                        terms.project_id, amount);
                }
            } else if (entry.kind ==
                ClaimLedgerEntryKind::PrincipalWriteoff) {
                if (amount > 0.0) {
                    project_path.principal_movements.push_back(
                        PrincipalMovement{entry.period,
                            PrincipalMovementKind::Writeoff, amount});
                    append_nonreceipt_lineage(result.cash_lineage,
                        claim_scenario.scenario_id, selected_entry,
                        ClaimLedgerPortfolioOutputKind::PrincipalMovement,
                        terms.project_id, amount);
                }
            } else if (receipt_kind(entry.kind) && amount > 0.0) {
                const std::string key = allocation_key(
                    selected_entry.scenario_id, entry.entry_id);
                const auto found = allocations.find(key);
                if (found == allocations.end() || found->second.empty()) {
                    throw std::invalid_argument(
                        "positive claim-ledger receipt lacks an explicit source allocation: " +
                        entry.entry_id);
                }
                receipt_work.push_back(
                    ReceiptWork{&entry, selected_entry.scenario_id, amount,
                        found->second,
                        principal_receipt_kind(entry.kind)
                            ? static_cast<long double>(amount)
                            : 0.0L});
            }
        }
        if (!(path_buyer_price > 0.0L)) {
            throw std::invalid_argument(
                "each adapted claim-ledger path requires positive buyer-price cash cost");
        }
        maximum_investor_outlay = std::max(maximum_investor_outlay,
            path_buyer_price + path_direct_cost);

        for (ReceiptWork& receipt : receipt_work) {
            append_receipt_allocations(
                receipt, project_path, sources, allocation_used,
                claim_scenario.scenario_id, terms.project_id,
                result.cash_lineage);
        }
        require_exact_budget_exhaustion(sources);
        std::sort(project_path.investor_outlays.begin(),
            project_path.investor_outlays.end(),
            [](const InvestorOutlay& first, const InvestorOutlay& second) {
                if (first.month != second.month) {
                    return first.month < second.month;
                }
                if (first.purpose != second.purpose) {
                    return first.purpose < second.purpose;
                }
                return first.amount_million < second.amount_million;
            });
        std::sort(project_path.principal_movements.begin(),
            project_path.principal_movements.end(),
            [](const PrincipalMovement& first,
                const PrincipalMovement& second) {
                if (first.month != second.month) {
                    return first.month < second.month;
                }
                if (first.kind != second.kind) {
                    return first.kind < second.kind;
                }
                return first.amount_million < second.amount_million;
            });
        scenario.project_paths.push_back(std::move(project_path));
        append_sources(scenario, sources);
        portfolio.joint_scenarios.push_back(std::move(scenario));
    }

    if (result.cash_budget_lineage.size() !=
        cash_budgets.expanded_lineage_size) {
        throw std::logic_error(
            "expanded cash-budget lineage did not match its preflight cardinality");
    }

    std::sort(result.cash_budget_lineage.begin(),
        result.cash_budget_lineage.end(),
        [](const ClaimLedgerPortfolioCashBudgetLineage& first,
            const ClaimLedgerPortfolioCashBudgetLineage& second) {
            if (first.portfolio_scenario_id !=
                second.portfolio_scenario_id) {
                return first.portfolio_scenario_id <
                    second.portfolio_scenario_id;
            }
            if (first.cash_source_id != second.cash_source_id) {
                return first.cash_source_id < second.cash_source_id;
            }
            if (first.month != second.month) {
                return first.month < second.month;
            }
            if (first.declared_claim_scenario_id !=
                second.declared_claim_scenario_id) {
                return first.declared_claim_scenario_id <
                    second.declared_claim_scenario_id;
            }
            if (first.amount_million != second.amount_million) {
                return first.amount_million < second.amount_million;
            }
            if (first.counterparty_id != second.counterparty_id) {
                return first.counterparty_id < second.counterparty_id;
            }
            return first.provider_claim_id < second.provider_claim_id;
        });

    if (std::find(allocation_used.begin(), allocation_used.end(), false) !=
        allocation_used.end()) {
        throw std::invalid_argument(
            "one or more receipt source allocations do not reference a positive selected decision-cut entry");
    }
    PortfolioProject project;
    project.id = terms.project_id;
    project.stage = terms.stage;
    project.commitment_million = checked_double(maximum_investor_outlay);
    project.principal_accounting_mode =
        PrincipalAccountingMode::ExplicitContractualLedger;
    project.principal_limit_million = principal_limit;
    project.opening_principal_million = opening_principal;
    portfolio.projects.push_back(std::move(project));
    validate_portfolio_config(portfolio);
    result.portfolio_summary = evaluate_portfolio(portfolio);

    CompensatedSum expected_contractual_loss;
    CompensatedSum expected_nominal_cash_shortfall;
    CompensatedSum normalized_probability_sum;
    for (const ClaimLedgerScenario& scenario : ledger.scenarios) {
        const ClaimLedgerScenarioResult& claim_path =
            ledger_result(claim_summary, scenario.scenario_id);
        const JointScenarioResult& pool_path =
            portfolio_result(result.portfolio_summary, scenario.scenario_id);
        if (pool_path.projects.size() != 1U) {
            throw std::logic_error(
                "adapted one-claim portfolio did not retain one project path");
        }
        double maximum_cash_error = 0.0;
        if (claim_path.decision_path.periods.size() !=
            pool_path.monthly_cash_flows.size()) {
            throw std::logic_error(
                "adapted portfolio changed the claim-ledger monthly horizon");
        }
        for (std::size_t month = 0U;
             month < claim_path.decision_path.periods.size(); ++month) {
            const double claim_cash = exact_value(
                claim_path.decision_path.periods[month]
                    .investor_cashflow_million,
                "claim-ledger path investor cash");
            maximum_cash_error = std::max(maximum_cash_error,
                std::abs(claim_cash -
                    pool_path.monthly_cash_flows[month]
                        .net_cash_flow_million));
        }
        if (maximum_cash_error > tolerance_for(
                maximum_cash_error, 0.0L)) {
            throw std::logic_error(
                "adapted monthly investor cash failed to reconcile");
        }
        const double raw_probability = exact_value(
            scenario.physical_probability, "scenario probability");
        const double probability = pool_path.normalized_weight;
        const double contractual_loss = exact_value(
            claim_path.decision_path.principal_loss_million,
            "claim-ledger contractual principal loss");
        const double portfolio_loss =
            pool_path.projects.front().principal_loss_million;
        if (!nearly_equal(contractual_loss, portfolio_loss)) {
            throw std::logic_error(
                "adapted portfolio principal loss changed the contractual claim writeoff");
        }
        long double buyer_price = 0.0L;
        long double buyer_cost = 0.0L;
        for (const ClaimLedgerPeriodResult& period :
             claim_path.decision_path.periods) {
            buyer_price += static_cast<long double>(exact_value(
                period.buyer_price_million, "buyer price"));
            buyer_cost += static_cast<long double>(exact_value(
                period.buyer_direct_cost_million, "buyer direct cost"));
        }
        const double investor_receipts =
            pool_path.projects.front().total_receipts_million;
        const double nominal_cash_shortfall = std::max(0.0,
            checked_double(buyer_price + buyer_cost) - investor_receipts);
        ClaimLedgerPortfolioPathBridge bridge;
        bridge.scenario_id = scenario.scenario_id;
        bridge.claim_ledger_physical_probability = raw_probability;
        bridge.physical_probability = probability;
        bridge.buyer_price_cash_outflow_million =
            checked_double(buyer_price);
        bridge.buyer_direct_cost_million = checked_double(buyer_cost);
        bridge.investor_receipts_million = investor_receipts;
        bridge.contractual_principal_loss_million = contractual_loss;
        bridge.portfolio_principal_loss_million = portfolio_loss;
        bridge.nominal_investor_cash_shortfall_million =
            nominal_cash_shortfall;
        bridge.maximum_monthly_cash_reconciliation_error_million =
            maximum_cash_error;
        bridge.probability_known_at_period =
            scenario.probability_known_at_period;
        bridge.probability_source_record_id =
            scenario.probability_source_record_id;
        bridge.cash_path_status_known_at_period =
            scenario.cash_path_status_known_at_period;
        bridge.cash_path_status_source_record_id =
            scenario.cash_path_status_source_record_id;
        bridge.decision_entry_ids = claim_path.decision_entry_ids;
        result.paths.push_back(std::move(bridge));
        expected_contractual_loss.add(
            static_cast<long double>(probability) * contractual_loss);
        expected_nominal_cash_shortfall.add(
            static_cast<long double>(probability) * nominal_cash_shortfall);
        normalized_probability_sum.add(
            static_cast<long double>(probability));
        result.maximum_monthly_cash_reconciliation_error_million =
            std::max(result.maximum_monthly_cash_reconciliation_error_million,
                maximum_cash_error);
    }
    result.expected_contractual_principal_loss_million = checked_double(
        expected_contractual_loss.value() /
        normalized_probability_sum.value());
    result.expected_nominal_investor_cash_shortfall_million =
        checked_double(expected_nominal_cash_shortfall.value() /
            normalized_probability_sum.value());
    const double normalized_claim_expected_loss = checked_double(
        static_cast<long double>(exact_value(
            claim_summary.expected_principal_loss_million,
            "expected claim-ledger principal loss")) /
        raw_probability_total);
    if (!nearly_equal(result.expected_contractual_principal_loss_million,
            normalized_claim_expected_loss)) {
        throw std::logic_error(
            "expected contractual principal loss failed to reconcile");
    }
    if (!nearly_equal(result.expected_contractual_principal_loss_million,
            result.portfolio_summary.principal_loss_million.mean)) {
        throw std::logic_error(
            "portfolio expected principal loss failed to reconcile to the claim ledger");
    }
    return result;
}

ClaimLedgerPortfolioAdapterResult adapt_claim_ledger_package_to_portfolio(
    const ClaimLedgerPackage& supplied,
    const ClaimLedgerPortfolioAdapterTerms& terms) {
    if (!supplied.package_integrity || supplied.directory.empty() ||
        supplied.claim_config_filename != std::filesystem::path("claim.cfg") ||
        !lower_hex_sha256(supplied.claim_config_sha256)) {
        throw std::logic_error(
            "claim-ledger portfolio adapter requires a loader-verified root snapshot");
    }
    const ClaimLedgerPackage verified = load_claim_ledger_package(
        supplied.directory / supplied.claim_config_filename);
    if (verified.claim_config_sha256 != supplied.claim_config_sha256) {
        throw std::logic_error(
            "claim-ledger root snapshot changed after package verification");
    }
    require_verified_package_core(verified);
    if (verified.config.economic_cluster_boundary_status !=
        ClaimLedgerEconomicClusterBoundaryStatus::Defined) {
        throw std::invalid_argument(
            "claim-ledger portfolio admission requires a defined economic-cluster boundary");
    }

    ClaimLedgerPortfolioAdmissionBasis admission{};
    switch (verified.config.package_status) {
    case ClaimLedgerPackageStatus::SyntheticComplete:
        admission = ClaimLedgerPortfolioAdmissionBasis::SyntheticMechanics;
        break;
    case ClaimLedgerPackageStatus::ControlledCandidate:
        if (!verified.expected_return_admissible) {
            throw std::invalid_argument(
                "controlled claim-ledger package is not admitted for expected-return analysis");
        }
        admission =
            ClaimLedgerPortfolioAdmissionBasis::ControlledExpectedReturn;
        break;
    case ClaimLedgerPackageStatus::RetainedPublicIncomplete:
        throw std::invalid_argument(
            "retained-public-incomplete claim-ledger packages cannot enter a portfolio");
    }

    ClaimLedgerPortfolioAdapterResult result =
        adapt_claim_ledger_to_portfolio(*verified.core_config, terms);
    result.package_lineage = ClaimLedgerPortfolioPackageLineage{
        verified.config.model_version, verified.config.package_id,
        verified.claim_config_sha256, verified.config.package_status,
        admission, ClaimLedgerPortfolioSourceScope::DecisionCut,
        verified.config.economic_cluster_id,
        verified.config.economic_cluster_boundary_status,
        verified.config.claim_id, verified.config.project_id,
        terms.project_id, verified.core_config->decision_period,
        verified.core_config->horizon_period,
        verified.config.period_unit_label,
        verified.config.periods_per_year,
        verified.config.period_origin_date.value.value_or(""),
        verified.config.decision_date.value.value_or(""),
        verified.config.horizon_date.value.value_or("")};
    result.portfolio.source_note =
        "Mechanical decision-cut projection from claim package " +
        verified.config.package_id + " sha256=" +
        verified.claim_config_sha256 + " status=" +
        std::string(to_string(verified.config.package_status)) +
        "; analyst hurdle, source budgets, factors and portfolio coupling remain synthetic assumptions";
    validate_portfolio_config(result.portfolio);
    return result;
}

} // namespace naturalehia::cellular_finance
