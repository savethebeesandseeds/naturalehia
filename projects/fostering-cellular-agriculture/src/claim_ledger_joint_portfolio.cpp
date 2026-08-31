// SPDX-License-Identifier: MIT

#include <naturalehia/cellular_finance/claim_ledger_joint_portfolio.hpp>
#include <naturalehia/cellular_finance/detail/claim_ledger_joint_portfolio_resource_guard.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace naturalehia::cellular_finance {
namespace {

constexpr std::size_t kMaximumAssets = 128U;
constexpr std::size_t kMaximumJointScenarios = 10'000U;
constexpr std::size_t kMaximumAssetScenarioPairs = 500'000U;
constexpr std::size_t kMaximumExpandedLineageRows =
    detail::kMaximumExpandedJointLineageEntries;
constexpr std::size_t kMaximumCashRecords = 2'000'000U;
constexpr std::size_t kMaximumScenarioMonths = 2'000'000U;
constexpr std::size_t kMaximumProjectScenarioMonths = 2'000'000U;
constexpr std::size_t kMaximumScenarioCashSources = 256U;
constexpr std::size_t kMaximumFactorTags = 64U;
constexpr std::size_t kMaximumIdentifierLength = 128U;
constexpr std::size_t kMaximumTextLength = 1'024U;
constexpr double kProbabilityTolerance = 1.0e-12;
constexpr double kMoneyTolerance = 1.0e-10;
constexpr std::string_view kJointSourcePrefix{
    "Verified package roots are retained in joint assembler lineage; no independence inference or empirical calibration. "};

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
    long double first, long double second, double absolute) noexcept {
    const long double scale =
        std::max({1.0L, std::abs(first), std::abs(second)});
    return static_cast<long double>(absolute) +
        64.0L * static_cast<long double>(
            std::numeric_limits<double>::epsilon()) * scale;
}

[[nodiscard]] bool nearly_equal(long double first, long double second,
    double absolute) noexcept {
    return std::abs(first - second) <=
        tolerance_for(first, second, absolute);
}

// Marginal reconciliation must not erase a positive rare state merely because
// it falls below an absolute tolerance. This comparison scales to the two
// probabilities themselves and treats zero versus positive as unequal.
[[nodiscard]] bool probabilities_equal(
    long double first, long double second) noexcept {
    if (first == second) return true;
    const long double scale = std::max(std::abs(first), std::abs(second));
    if (!(scale > 0.0L)) return true;
    return std::abs(first - second) <=
        128.0L * static_cast<long double>(
            std::numeric_limits<double>::epsilon()) * scale;
}

[[nodiscard]] bool ascii_alphanumeric(char character) noexcept {
    return (character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z') ||
        (character >= '0' && character <= '9');
}

[[nodiscard]] bool safe_identifier(std::string_view value) noexcept {
    if (value.empty() || value.size() > kMaximumIdentifierLength ||
        !ascii_alphanumeric(value.front())) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return ascii_alphanumeric(character) || character == '-' ||
            character == '_' || character == '.';
    });
}

void require_safe_text(
    std::string_view value, std::string_view description) {
    if (value.empty() || value.size() > kMaximumTextLength) {
        throw std::invalid_argument(
            std::string(description) + " must be non-empty and bounded");
    }
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        if (character < 0x20U || character == 0x7FU) {
            throw std::invalid_argument(
                std::string(description) + " contains a control character");
        }
    }
}

[[nodiscard]] double checked_double(long double value) {
    const double result = static_cast<double>(value);
    if (!std::isfinite(result)) {
        throw std::overflow_error(
            "claim-ledger joint portfolio exceeded numeric range");
    }
    return result;
}

[[nodiscard]] std::vector<std::string> sorted_copy(
    const std::vector<std::string>& values) {
    std::vector<std::string> result = values;
    std::sort(result.begin(), result.end());
    return result;
}

[[nodiscard]] long double validate_joint_terms_preflight(
    std::size_t asset_count,
    const ClaimLedgerJointPortfolioTerms& terms) {
    require_safe_text(terms.scenario_label, "joint scenario_label");
    require_safe_text(terms.source_note, "joint source_note");
    if (terms.source_note.size() >
        kMaximumTextLength - kJointSourcePrefix.size()) {
        throw std::invalid_argument(
            "joint source_note plus audit prefix exceeds the text guardrail");
    }

    CompensatedSum declared_probability_sum;
    std::unordered_set<std::string> joint_ids;
    joint_ids.reserve(terms.joint_scenarios.size());
    for (const ClaimLedgerJointPortfolioScenario& declaration :
         terms.joint_scenarios) {
        if (!safe_identifier(declaration.scenario_id)) {
            throw std::invalid_argument(
                "joint scenario_id must be a safe identifier");
        }
        if (!joint_ids.emplace(declaration.scenario_id).second) {
            throw std::invalid_argument(
                "joint scenario identifiers must be unique");
        }
        if (!std::isfinite(declaration.physical_probability) ||
            declaration.physical_probability < 0.0) {
            throw std::invalid_argument(
                "joint physical probabilities must be finite and non-negative");
        }
        if (!safe_identifier(declaration.probability_basis_id)) {
            throw std::invalid_argument(
                "joint probability_basis_id must be a safe identifier");
        }
        if (declaration.factor_tags.size() > kMaximumFactorTags) {
            throw std::invalid_argument(
                "declared joint factor tag count exceeds the resource guardrail");
        }
        std::unordered_set<std::string> factor_tags;
        factor_tags.reserve(declaration.factor_tags.size());
        for (const std::string& factor : declaration.factor_tags) {
            if (!safe_identifier(factor)) {
                throw std::invalid_argument(
                    "declared joint factor tag must be a safe identifier");
            }
            if (!factor_tags.emplace(factor).second) {
                throw std::invalid_argument(
                    "declared joint factor tags must be unique");
            }
        }
        if (declaration.selections.size() != asset_count) {
            throw std::invalid_argument(
                "each joint scenario must select every claim asset exactly once");
        }
        std::unordered_set<std::string> selected_projects;
        selected_projects.reserve(declaration.selections.size());
        for (const ClaimLedgerJointPortfolioSelection& selection :
             declaration.selections) {
            if (!safe_identifier(selection.portfolio_project_id) ||
                !safe_identifier(selection.marginal_scenario_id)) {
                throw std::invalid_argument(
                    "joint selection identifiers must be safe identifiers");
            }
            if (!selected_projects.emplace(
                    selection.portfolio_project_id).second) {
                throw std::invalid_argument(
                    "joint scenario contains a duplicate project selection");
            }
        }
        declared_probability_sum.add(static_cast<long double>(
            declaration.physical_probability));
    }
    const long double declared_total = declared_probability_sum.value();
    if (!std::isfinite(declared_total) || !(declared_total > 0.0L) ||
        !nearly_equal(declared_total, 1.0L, kProbabilityTolerance)) {
        throw std::invalid_argument(
            "joint physical probabilities must sum to one within tolerance");
    }
    return declared_total;
}

void add_lineage_rows(std::size_t& count, std::size_t addition) {
    if (addition > kMaximumExpandedLineageRows - count) {
        throw std::invalid_argument(
            "expanded joint claim lineage exceeds the resource guardrail");
    }
    count += addition;
}

void add_cash_records(std::size_t& count, std::size_t addition) {
    if (addition > kMaximumCashRecords - count) {
        throw std::invalid_argument(
            "expanded joint cash records exceed the resource guardrail");
    }
    count += addition;
}

[[nodiscard]] const ProjectPortfolioSummary& project_summary(
    const PortfolioSummary& summary, std::string_view project_id) {
    const auto found = std::find_if(summary.projects.begin(),
        summary.projects.end(), [project_id](const auto& project) {
            return project.project_id == project_id;
        });
    if (found == summary.projects.end()) {
        throw std::logic_error(
            "joint portfolio evaluation lost a configured project");
    }
    return *found;
}

struct MarginalView {
    const JointScenario* scenario{};
    const ClaimLedgerPortfolioPathBridge* bridge{};
    std::vector<const ClaimLedgerPortfolioCashLineage*> cash_lineage{};
    std::vector<const ClaimLedgerPortfolioCashBudgetLineage*>
        cash_budget_lineage{};
};

struct AssetWork {
    const ClaimLedgerPortfolioAdapterResult* adapted{};
    const ClaimLedgerPortfolioPackageLineage* package{};
    const PortfolioProject* project{};
    std::unordered_map<std::string, MarginalView> marginals{};
};

struct ValidatedJointScenario {
    const ClaimLedgerJointPortfolioScenario* declaration{};
    double physical_probability{0.0};
    std::vector<const MarginalView*> selected_marginals{};
};

void require_adapter_shape(AssetWork& work) {
    const ClaimLedgerPortfolioAdapterResult& adapted = *work.adapted;
    if (!adapted.package_lineage.has_value()) {
        throw std::logic_error(
            "joint portfolio requires package-derived adapter lineage");
    }
    work.package = &*adapted.package_lineage;
    if (work.package->economic_cluster_boundary_status !=
        ClaimLedgerEconomicClusterBoundaryStatus::Defined) {
        throw std::logic_error(
            "joint portfolio received an unresolved economic-cluster boundary");
    }
    if (adapted.portfolio.projects.size() != 1U ||
        adapted.portfolio.joint_scenarios.empty() ||
        !adapted.portfolio.loss_layers.empty()) {
        throw std::logic_error(
            "joint portfolio requires an unpackaged one-project marginal adapter result");
    }
    work.project = &adapted.portfolio.projects.front();
    if (work.project->id != work.package->portfolio_project_id ||
        adapted.paths.size() != adapted.portfolio.joint_scenarios.size()) {
        throw std::logic_error(
            "one-project adapter lineage does not reconcile to its portfolio");
    }

    std::unordered_map<std::string,
        const ClaimLedgerPortfolioPathBridge*> bridges;
    bridges.reserve(adapted.paths.size());
    for (const ClaimLedgerPortfolioPathBridge& bridge : adapted.paths) {
        if (!bridges.emplace(bridge.scenario_id, &bridge).second) {
            throw std::logic_error(
                "one-project adapter contains duplicate bridge scenarios");
        }
    }

    work.marginals.reserve(adapted.portfolio.joint_scenarios.size());
    for (const JointScenario& scenario :
         adapted.portfolio.joint_scenarios) {
        if (scenario.project_paths.size() != 1U ||
            !scenario.pool_costs.empty()) {
            throw std::logic_error(
                "one-project adapter scenario changed its unpackaged shape");
        }
        const auto bridge = bridges.find(scenario.id);
        if (bridge == bridges.end() ||
            scenario.project_paths.front().project_id != work.project->id ||
            !probabilities_equal(scenario.weight,
                bridge->second->physical_probability)) {
            throw std::logic_error(
                "one-project adapter scenario and bridge do not reconcile");
        }
        if (!work.marginals.emplace(scenario.id,
                MarginalView{&scenario, bridge->second, {}, {}}).second) {
            throw std::logic_error(
                "one-project adapter contains duplicate marginal scenarios");
        }
    }
    if (work.marginals.size() != bridges.size()) {
        throw std::logic_error(
            "one-project adapter bridge names an unknown marginal scenario");
    }

    for (const ClaimLedgerPortfolioCashLineage& row :
         adapted.cash_lineage) {
        const auto marginal = work.marginals.find(
            row.portfolio_scenario_id);
        if (marginal == work.marginals.end() ||
            row.portfolio_project_id != work.project->id) {
            throw std::logic_error(
                "one-project cash lineage names an unknown marginal path");
        }
        marginal->second.cash_lineage.push_back(&row);
    }
    for (const ClaimLedgerPortfolioCashBudgetLineage& row :
         adapted.cash_budget_lineage) {
        const auto marginal = work.marginals.find(
            row.portfolio_scenario_id);
        if (marginal == work.marginals.end()) {
            throw std::logic_error(
                "one-project cash-budget lineage names an unknown marginal path");
        }
        marginal->second.cash_budget_lineage.push_back(&row);
    }
}

void require_common_asset_basis(const PortfolioConfig& marginal,
    const ClaimLedgerPortfolioPackageLineage& package,
    const PortfolioConfig& first_portfolio,
    ClaimLedgerPortfolioAdmissionBasis first_admission,
    const ClaimLedgerPortfolioPackageLineage& first_package) {
    if (marginal.model_version != first_portfolio.model_version ||
        marginal.currency_label != first_portfolio.currency_label ||
        marginal.monetary_basis != first_portfolio.monetary_basis ||
        marginal.horizon_months != first_portfolio.horizon_months ||
        !nearly_equal(marginal.annual_physical_hurdle_rate,
            first_portfolio.annual_physical_hurdle_rate,
            kProbabilityTolerance)) {
        throw std::invalid_argument(
            "joint claim assets require one model version, currency, monetary basis, horizon and hurdle");
    }
    if (package.admission_basis != first_admission) {
        throw std::invalid_argument(
            "joint claim assets require one homogeneous admission basis");
    }
    if (package.period_unit_label.empty() ||
        package.period_origin_date.empty() ||
        package.decision_date.empty() || package.horizon_date.empty() ||
        package.period_unit_label != first_package.period_unit_label ||
        package.periods_per_year != first_package.periods_per_year ||
        package.period_origin_date != first_package.period_origin_date ||
        package.decision_date != first_package.decision_date ||
        package.horizon_date != first_package.horizon_date ||
        package.decision_period != first_package.decision_period ||
        package.horizon_period != first_package.horizon_period) {
        throw std::invalid_argument(
            "joint claim assets require one common calendar and information axis");
    }
}

void accumulate_retained_marginal_resources(
    const ClaimLedgerPortfolioAdapterResult& adapted,
    detail::RetainedMarginalResourceUsage& usage) {
    for (const PortfolioProject& project : adapted.portfolio.projects) {
        (void)project;
        usage.add_project_scenario_pairs(
            adapted.portfolio.joint_scenarios.size());
    }
    for (const JointScenario& scenario :
         adapted.portfolio.joint_scenarios) {
        usage.add_cash_records(scenario.pool_costs.size());
        usage.add_lineage_rows(scenario.factor_tags.size());
        for (const ScenarioCashSource& source : scenario.cash_sources) {
            usage.add_cash_records(source.cash_available.size());
        }
        for (const ProjectJointPath& path : scenario.project_paths) {
            usage.add_cash_records(path.capital_draws.size());
            usage.add_cash_records(path.investor_outlays.size());
            usage.add_cash_records(path.investor_receipts.size());
            usage.add_cash_records(path.principal_movements.size());
        }
    }
    for (const JointScenarioResult& scenario :
         adapted.portfolio_summary.scenarios) {
        usage.add_scenario_month_rows(
            scenario.monthly_cash_flows.size());
        usage.add_cash_records(scenario.return_sources.size());
        usage.add_lineage_rows(scenario.factor_tags.size());
        for (const ProjectPathResult& project : scenario.projects) {
            usage.add_cash_records(project.return_sources.size());
        }
    }
    usage.add_lineage_rows(adapted.cash_lineage.size());
    usage.add_lineage_rows(adapted.cash_budget_lineage.size());
    for (const ClaimLedgerPortfolioPathBridge& path : adapted.paths) {
        usage.add_lineage_rows(path.decision_entry_ids.size());
    }
}

[[nodiscard]] double absolute_difference(double first, double second) {
    return std::abs(first - second);
}

} // namespace

ClaimLedgerJointPortfolioResult assemble_claim_ledger_joint_portfolio(
    const std::vector<ClaimLedgerJointPortfolioAssetInput>& assets,
    const ClaimLedgerJointPortfolioTerms& terms) {
    if (assets.size() < 2U || assets.size() > kMaximumAssets) {
        throw std::invalid_argument(
            "joint claim portfolio requires between two and 128 assets");
    }
    if (terms.joint_scenarios.empty() ||
        terms.joint_scenarios.size() > kMaximumJointScenarios ||
        assets.size() >
            kMaximumAssetScenarioPairs / terms.joint_scenarios.size()) {
        throw std::invalid_argument(
            "joint scenario dimensions exceed the resource guardrail");
    }
    const long double declared_total =
        validate_joint_terms_preflight(assets.size(), terms);

    ClaimLedgerJointPortfolioResult result;
    result.marginal_assets.reserve(assets.size());
    result.package_lineage.reserve(assets.size());
    detail::RetainedMarginalResourceUsage retained_usage;
    std::unordered_set<std::string> preflight_project_ids;
    std::unordered_set<std::string> preflight_package_ids;
    std::unordered_set<std::string> preflight_package_roots;
    std::unordered_set<std::string> preflight_economic_clusters;
    std::unordered_set<std::string> preflight_claim_ids;
    std::unordered_map<std::string, std::size_t>
        preflight_cash_source_owners;
    preflight_project_ids.reserve(assets.size());
    preflight_package_ids.reserve(assets.size());
    preflight_package_roots.reserve(assets.size());
    preflight_economic_clusters.reserve(assets.size());
    preflight_claim_ids.reserve(assets.size());

    const PortfolioConfig* preflight_first_portfolio = nullptr;
    const ClaimLedgerPortfolioPackageLineage* preflight_first_package =
        nullptr;
    ClaimLedgerPortfolioAdmissionBasis preflight_first_admission{
        ClaimLedgerPortfolioAdmissionBasis::SyntheticMechanics};
    for (std::size_t asset_index = 0U; asset_index < assets.size();
         ++asset_index) {
        const ClaimLedgerJointPortfolioAssetInput& asset =
            assets[asset_index];
        result.marginal_assets.push_back(
            adapt_claim_ledger_package_to_portfolio(
                asset.package, asset.adapter_terms));
        AssetWork fresh;
        fresh.adapted = &result.marginal_assets.back();
        require_adapter_shape(fresh);
        accumulate_retained_marginal_resources(*fresh.adapted,
            retained_usage);

        const PortfolioConfig& marginal = fresh.adapted->portfolio;
        const ClaimLedgerPortfolioPackageLineage& package =
            *fresh.package;
        if (preflight_first_portfolio == nullptr) {
            preflight_first_portfolio = &marginal;
            preflight_first_package = &package;
            preflight_first_admission = package.admission_basis;
        } else {
            require_common_asset_basis(marginal, package,
                *preflight_first_portfolio, preflight_first_admission,
                *preflight_first_package);
        }
        if (!preflight_project_ids.emplace(fresh.project->id).second ||
            !preflight_package_ids.emplace(package.package_id).second ||
            !preflight_package_roots.emplace(
                package.claim_config_sha256).second ||
            !preflight_economic_clusters.emplace(
                package.economic_cluster_id).second ||
            !preflight_claim_ids.emplace(package.claim_id).second) {
            throw std::invalid_argument(
                "joint claim assets must have unique project, package, root, economic-cluster and claim identities");
        }
        for (const JointScenario& marginal_scenario :
             marginal.joint_scenarios) {
            for (const ScenarioCashSource& source :
                 marginal_scenario.cash_sources) {
                const auto [owner, inserted] =
                    preflight_cash_source_owners.emplace(
                        source.id, asset_index);
                if (!inserted && owner->second != asset_index) {
                    throw std::invalid_argument(
                        "cash-source identifiers must be unique across joint assets; shared budgets are unsupported in v0.1");
                }
            }
        }
    }
    std::sort(result.marginal_assets.begin(),
        result.marginal_assets.end(), [](const auto& first,
            const auto& second) {
            return first.portfolio.projects.front().id <
                second.portfolio.projects.front().id;
        });

    std::vector<AssetWork> work(assets.size());
    std::unordered_map<std::string, std::size_t> project_indices;
    std::unordered_set<std::string> package_ids;
    std::unordered_set<std::string> package_roots;
    std::unordered_set<std::string> economic_clusters;
    std::unordered_set<std::string> claim_ids;
    std::unordered_map<std::string, std::size_t> cash_source_owners;
    project_indices.reserve(assets.size());
    package_ids.reserve(assets.size());
    package_roots.reserve(assets.size());
    economic_clusters.reserve(assets.size());
    claim_ids.reserve(assets.size());

    const PortfolioConfig& first_portfolio =
        result.marginal_assets.front().portfolio;
    const ClaimLedgerPortfolioAdmissionBasis first_admission =
        result.marginal_assets.front().package_lineage.has_value()
        ? result.marginal_assets.front().package_lineage->admission_basis
        : ClaimLedgerPortfolioAdmissionBasis::SyntheticMechanics;
    const ClaimLedgerPortfolioPackageLineage* first_package =
        result.marginal_assets.front().package_lineage.has_value()
        ? &*result.marginal_assets.front().package_lineage
        : nullptr;
    for (std::size_t asset_index = 0U; asset_index < assets.size();
         ++asset_index) {
        work[asset_index].adapted = &result.marginal_assets[asset_index];
        require_adapter_shape(work[asset_index]);
        const AssetWork& asset_work = work[asset_index];
        const PortfolioConfig& marginal = asset_work.adapted->portfolio;
        const ClaimLedgerPortfolioPackageLineage& package =
            *asset_work.package;

        if (first_package == nullptr) {
            throw std::logic_error(
                "joint portfolio lost first package lineage after sorting");
        }
        require_common_asset_basis(marginal, package, first_portfolio,
            first_admission, *first_package);
        if (!project_indices.emplace(
                asset_work.project->id, asset_index).second ||
            !package_ids.emplace(package.package_id).second ||
            !package_roots.emplace(package.claim_config_sha256).second ||
            !economic_clusters.emplace(package.economic_cluster_id).second ||
            !claim_ids.emplace(package.claim_id).second) {
            throw std::invalid_argument(
                "joint claim assets must have unique project, package, root, economic-cluster and claim identities");
        }
        result.package_lineage.push_back(package);

        for (const JointScenario& marginal_scenario :
             marginal.joint_scenarios) {
            for (const ScenarioCashSource& source :
                 marginal_scenario.cash_sources) {
                const auto [owner, inserted] = cash_source_owners.emplace(
                    source.id, asset_index);
                if (!inserted && owner->second != asset_index) {
                    throw std::invalid_argument(
                        "cash-source identifiers must be unique across joint assets; shared budgets are unsupported in v0.1");
                }
            }
        }
    }

    const std::size_t month_count = first_portfolio.horizon_months + 1U;
    if (month_count >
            kMaximumScenarioMonths / terms.joint_scenarios.size() ||
        month_count > kMaximumProjectScenarioMonths /
            (assets.size() * terms.joint_scenarios.size())) {
        throw std::invalid_argument(
            "joint scenario-month dimensions exceed the resource guardrail");
    }

    std::vector<std::vector<CompensatedSum>> marginal_probability_sums;
    marginal_probability_sums.reserve(work.size());
    for (const AssetWork& asset_work : work) {
        marginal_probability_sums.emplace_back(asset_work.marginals.size());
    }
    std::vector<std::unordered_map<std::string, std::size_t>>
        marginal_positions(work.size());
    for (std::size_t asset_index = 0U; asset_index < work.size();
         ++asset_index) {
        marginal_positions[asset_index].reserve(
            work[asset_index].marginals.size());
        std::size_t position = 0U;
        for (const JointScenario& scenario :
             work[asset_index].adapted->portfolio.joint_scenarios) {
            marginal_positions[asset_index].emplace(
                scenario.id, position++);
        }
    }

    std::vector<ValidatedJointScenario> validated;
    validated.reserve(terms.joint_scenarios.size());
    std::size_t expanded_cash_rows = 0U;
    std::size_t expanded_budget_rows = 0U;
    std::size_t expanded_cash_records = 0U;
    detail::ExpandedJointResourceUsage expanded_usage;
    expanded_usage.add_lineage_entries(result.package_lineage.size());
    for (const AssetWork& asset_work : work) {
        expanded_usage.add_lineage_entries(
            asset_work.marginals.size());
        expanded_usage.add_lineage_entries(1U);
    }
    for (const ClaimLedgerJointPortfolioScenario& declaration :
         terms.joint_scenarios) {
        if (declaration.selections.size() != assets.size()) {
            throw std::invalid_argument(
                "each joint scenario must select every claim asset exactly once");
        }
        ValidatedJointScenario row;
        row.declaration = &declaration;
        row.physical_probability = checked_double(
            static_cast<long double>(declaration.physical_probability) /
            declared_total);
        row.selected_marginals.resize(assets.size(), nullptr);
        for (const ClaimLedgerJointPortfolioSelection& selection :
             declaration.selections) {
            const auto project = project_indices.find(
                selection.portfolio_project_id);
            if (project == project_indices.end()) {
                throw std::invalid_argument(
                    "joint scenario selection names an unknown project");
            }
            if (row.selected_marginals[project->second] != nullptr) {
                throw std::invalid_argument(
                    "joint scenario contains a duplicate project selection");
            }
            const auto marginal = work[project->second].marginals.find(
                selection.marginal_scenario_id);
            if (marginal == work[project->second].marginals.end()) {
                throw std::invalid_argument(
                    "joint scenario selection names an unknown marginal scenario");
            }
            row.selected_marginals[project->second] = &marginal->second;
            const std::size_t marginal_position =
                marginal_positions[project->second].at(
                    selection.marginal_scenario_id);
            marginal_probability_sums[project->second]
                [marginal_position]
                    .add(static_cast<long double>(
                        row.physical_probability));
            add_lineage_rows(expanded_cash_rows,
                marginal->second.cash_lineage.size());
            add_lineage_rows(expanded_budget_rows,
                marginal->second.cash_budget_lineage.size());
            expanded_usage.add_lineage_entries(
                marginal->second.cash_lineage.size());
            expanded_usage.add_lineage_entries(
                marginal->second.cash_budget_lineage.size());
            expanded_usage.add_lineage_entries(1U);
            expanded_usage.add_lineage_entries(
                marginal->second.scenario->factor_tags.size());
            const ProjectJointPath& path =
                marginal->second.scenario->project_paths.front();
            add_cash_records(expanded_cash_records,
                path.capital_draws.size());
            add_cash_records(expanded_cash_records,
                path.investor_outlays.size());
            add_cash_records(expanded_cash_records,
                path.investor_receipts.size());
            add_cash_records(expanded_cash_records,
                path.principal_movements.size());
            for (const ScenarioCashSource& source :
                 marginal->second.scenario->cash_sources) {
                add_cash_records(expanded_cash_records,
                    source.cash_available.size());
            }
        }
        if (std::find(row.selected_marginals.begin(),
                row.selected_marginals.end(), nullptr) !=
            row.selected_marginals.end()) {
            throw std::invalid_argument(
                "joint scenario is missing a project selection");
        }
        std::unordered_set<std::string_view> effective_factor_tags;
        effective_factor_tags.reserve(kMaximumFactorTags);
        for (const std::string& factor : declaration.factor_tags) {
            effective_factor_tags.emplace(factor);
        }
        for (const MarginalView* selected : row.selected_marginals) {
            for (const std::string& factor :
                 selected->scenario->factor_tags) {
                effective_factor_tags.emplace(factor);
            }
        }
        if (effective_factor_tags.size() > kMaximumFactorTags) {
            throw std::invalid_argument(
                "joint factor union exceeds the resource guardrail");
        }
        expanded_usage.add_lineage_entries(1U);
        expanded_usage.add_lineage_entries(
            declaration.factor_tags.size());
        // The effective union is retained in PortfolioConfig, the assembler's
        // scenario lineage and PortfolioSummary. Count every copied string
        // instance before any of those result vectors is constructed.
        expanded_usage.add_lineage_entries(
            effective_factor_tags.size());
        expanded_usage.add_lineage_entries(
            effective_factor_tags.size());
        expanded_usage.add_lineage_entries(
            effective_factor_tags.size());
        validated.push_back(std::move(row));
    }

    std::sort(validated.begin(), validated.end(),
        [](const ValidatedJointScenario& first,
            const ValidatedJointScenario& second) {
            return first.declaration->scenario_id <
                second.declaration->scenario_id;
        });

    for (std::size_t asset_index = 0U; asset_index < work.size();
         ++asset_index) {
        std::size_t marginal_position = 0U;
        for (const JointScenario& marginal :
             work[asset_index].adapted->portfolio.joint_scenarios) {
            const MarginalView& marginal_view =
                work[asset_index].marginals.at(marginal.id);
            const double authoritative_marginal_probability =
                marginal_view.bridge->physical_probability;
            const double from_joint = checked_double(
                marginal_probability_sums[asset_index]
                    [marginal_position++]
                        .value());
            const double error = from_joint -
                authoritative_marginal_probability;
            result.maximum_marginal_probability_reconciliation_error =
                std::max(
                    result.maximum_marginal_probability_reconciliation_error,
                    std::abs(error));
            result.marginal_reconciliations.push_back(
                ClaimLedgerJointMarginalReconciliation{
                    work[asset_index].project->id, marginal.id,
                    marginal.weight, authoritative_marginal_probability,
                    from_joint, error});
            if (!probabilities_equal(from_joint,
                    authoritative_marginal_probability)) {
                throw std::invalid_argument(
                    "joint scenario probabilities do not reproduce every claim marginal");
            }
        }
    }

    PortfolioConfig& portfolio = result.portfolio;
    portfolio.model_version = first_portfolio.model_version;
    portfolio.scenario_label = terms.scenario_label;
    portfolio.source_note = std::string(kJointSourcePrefix) +
        terms.source_note;
    portfolio.currency_label = first_portfolio.currency_label;
    portfolio.monetary_basis = first_portfolio.monetary_basis;
    portfolio.synthetic_inputs = true;
    portfolio.horizon_months = first_portfolio.horizon_months;
    portfolio.annual_physical_hurdle_rate =
        first_portfolio.annual_physical_hurdle_rate;
    portfolio.projects.reserve(work.size());
    for (const AssetWork& asset_work : work) {
        portfolio.projects.push_back(*asset_work.project);
    }
    portfolio.joint_scenarios.reserve(validated.size());
    result.joint_scenario_lineage.reserve(validated.size());
    result.selection_lineage.reserve(assets.size() * validated.size());
    result.cash_lineage.reserve(expanded_cash_rows);
    result.cash_budget_lineage.reserve(expanded_budget_rows);

    for (const ValidatedJointScenario& row : validated) {
        const ClaimLedgerJointPortfolioScenario& declaration =
            *row.declaration;
        JointScenario joint;
        joint.id = declaration.scenario_id;
        joint.weight = row.physical_probability;
        joint.factor_tags = declaration.factor_tags;
        std::unordered_set<std::string> joint_factor_set(
            joint.factor_tags.begin(), joint.factor_tags.end());
        for (const MarginalView* selected : row.selected_marginals) {
            for (const std::string& factor :
                 selected->scenario->factor_tags) {
                if (joint_factor_set.emplace(factor).second) {
                    if (joint.factor_tags.size() == kMaximumFactorTags) {
                        throw std::invalid_argument(
                            "joint factor union exceeds the resource guardrail");
                    }
                    joint.factor_tags.push_back(factor);
                }
            }
        }
        std::sort(joint.factor_tags.begin(), joint.factor_tags.end());
        std::size_t joint_cash_source_count = 0U;
        for (const MarginalView* selected : row.selected_marginals) {
            const std::size_t addition =
                selected->scenario->cash_sources.size();
            if (addition >
                kMaximumScenarioCashSources - joint_cash_source_count) {
                throw std::invalid_argument(
                    "joint scenario cash-source count exceeds the resource guardrail");
            }
            joint_cash_source_count += addition;
        }
        joint.project_paths.reserve(work.size());
        result.joint_scenario_lineage.push_back(
            ClaimLedgerJointScenarioLineage{declaration.scenario_id,
                declaration.physical_probability,
                row.physical_probability,
                row.physical_probability,
                declaration.probability_basis_id,
                sorted_copy(declaration.factor_tags),
                joint.factor_tags});

        for (std::size_t asset_index = 0U; asset_index < work.size();
             ++asset_index) {
            const AssetWork& asset_work = work[asset_index];
            const MarginalView& marginal =
                *row.selected_marginals[asset_index];
            joint.project_paths.push_back(
                marginal.scenario->project_paths.front());
            joint.cash_sources.insert(joint.cash_sources.end(),
                marginal.scenario->cash_sources.begin(),
                marginal.scenario->cash_sources.end());
            result.selection_lineage.push_back(
                ClaimLedgerJointSelectionLineage{
                    declaration.scenario_id, row.physical_probability,
                    row.physical_probability,
                    asset_work.project->id, marginal.scenario->id,
                    marginal.bridge->claim_ledger_physical_probability,
                    marginal.bridge->physical_probability,
                    marginal.bridge->probability_known_at_period,
                    marginal.bridge->probability_source_record_id,
                    sorted_copy(marginal.scenario->factor_tags),
                    asset_work.package->package_id,
                    asset_work.package->claim_config_sha256,
                    asset_work.package->economic_cluster_id,
                    asset_work.package->claim_id});
            for (const ClaimLedgerPortfolioCashLineage* cash :
                 marginal.cash_lineage) {
                result.cash_lineage.push_back(
                    ClaimLedgerJointCashLineage{
                        declaration.scenario_id,
                        asset_work.package->package_id,
                        asset_work.package->claim_config_sha256,
                        marginal.scenario->id, *cash});
            }
            for (const ClaimLedgerPortfolioCashBudgetLineage* budget :
                 marginal.cash_budget_lineage) {
                result.cash_budget_lineage.push_back(
                    ClaimLedgerJointCashBudgetLineage{
                        declaration.scenario_id,
                        asset_work.package->package_id,
                        asset_work.package->claim_config_sha256,
                        marginal.scenario->id, *budget});
            }
        }
        portfolio.joint_scenarios.push_back(std::move(joint));
    }

    validate_portfolio_config(portfolio);
    result.portfolio_summary = evaluate_portfolio(portfolio);

    // Reconcile a second time against Portfolio's evaluated normalized
    // measure. This is the authoritative postcondition if its accepted
    // near-one input normalization ever differs by a final rounding step.
    std::unordered_map<std::string, double> evaluated_joint_weights;
    evaluated_joint_weights.reserve(result.portfolio_summary.scenarios.size());
    for (const JointScenarioResult& scenario :
         result.portfolio_summary.scenarios) {
        if (!evaluated_joint_weights.emplace(
                scenario.scenario_id, scenario.normalized_weight).second) {
            throw std::logic_error(
                "joint portfolio evaluation duplicated a scenario");
        }
    }
    for (ClaimLedgerJointScenarioLineage& lineage :
         result.joint_scenario_lineage) {
        const auto evaluated = evaluated_joint_weights.find(
            lineage.joint_scenario_id);
        if (evaluated == evaluated_joint_weights.end()) {
            throw std::logic_error(
                "joint portfolio evaluation lost scenario lineage");
        }
        lineage.physical_probability = evaluated->second;
    }
    for (ClaimLedgerJointSelectionLineage& lineage :
         result.selection_lineage) {
        const auto evaluated = evaluated_joint_weights.find(
            lineage.joint_scenario_id);
        if (evaluated == evaluated_joint_weights.end()) {
            throw std::logic_error(
                "joint portfolio evaluation lost selection lineage");
        }
        lineage.joint_physical_probability = evaluated->second;
    }
    std::vector<std::vector<CompensatedSum>> evaluated_marginal_sums;
    evaluated_marginal_sums.reserve(work.size());
    for (const AssetWork& asset_work : work) {
        evaluated_marginal_sums.emplace_back(asset_work.marginals.size());
    }
    for (const ValidatedJointScenario& row : validated) {
        const auto evaluated = evaluated_joint_weights.find(
            row.declaration->scenario_id);
        if (evaluated == evaluated_joint_weights.end()) {
            throw std::logic_error(
                "joint portfolio evaluation lost a declared scenario");
        }
        for (std::size_t asset_index = 0U; asset_index < work.size();
             ++asset_index) {
            const std::size_t position =
                marginal_positions[asset_index].at(
                    row.selected_marginals[asset_index]->scenario->id);
            evaluated_marginal_sums[asset_index][position].add(
                static_cast<long double>(evaluated->second));
        }
    }
    result.marginal_reconciliations.clear();
    result.maximum_marginal_probability_reconciliation_error = 0.0;
    for (std::size_t asset_index = 0U; asset_index < work.size();
         ++asset_index) {
        std::size_t marginal_position = 0U;
        for (const JointScenario& marginal :
             work[asset_index].adapted->portfolio.joint_scenarios) {
            const MarginalView& marginal_view =
                work[asset_index].marginals.at(marginal.id);
            const double authoritative_marginal_probability =
                marginal_view.bridge->physical_probability;
            const double from_joint = checked_double(
                evaluated_marginal_sums[asset_index]
                    [marginal_position++]
                        .value());
            const double error = from_joint -
                authoritative_marginal_probability;
            result.maximum_marginal_probability_reconciliation_error =
                std::max(
                    result.maximum_marginal_probability_reconciliation_error,
                    std::abs(error));
            result.marginal_reconciliations.push_back(
                ClaimLedgerJointMarginalReconciliation{
                    work[asset_index].project->id, marginal.id,
                    marginal.weight, authoritative_marginal_probability,
                    from_joint, error});
            if (!probabilities_equal(from_joint,
                    authoritative_marginal_probability)) {
                throw std::logic_error(
                    "evaluated joint measure changed a claim marginal");
            }
        }
    }

    result.project_reconciliations.reserve(work.size());
    for (const AssetWork& asset_work : work) {
        const ProjectPortfolioSummary& marginal =
            asset_work.adapted->portfolio_summary.projects.front();
        const ProjectPortfolioSummary& joint = project_summary(
            result.portfolio_summary, asset_work.project->id);
        const double maximum_error = std::max({
            absolute_difference(marginal.expected_draws_million,
                joint.expected_draws_million),
            absolute_difference(marginal.expected_receipts_million,
                joint.expected_receipts_million),
            absolute_difference(marginal.expected_principal_loss_million,
                joint.expected_principal_loss_million),
            absolute_difference(marginal.expected_npv_before_pool_costs_million,
                joint.expected_npv_before_pool_costs_million)});
        result.maximum_project_reconciliation_error_million =
            std::max(result.maximum_project_reconciliation_error_million,
                maximum_error);
        result.project_reconciliations.push_back(
            ClaimLedgerJointProjectReconciliation{
                asset_work.project->id,
                marginal.expected_draws_million,
                joint.expected_draws_million,
                marginal.expected_receipts_million,
                joint.expected_receipts_million,
                marginal.expected_principal_loss_million,
                joint.expected_principal_loss_million,
                marginal.expected_npv_before_pool_costs_million,
                joint.expected_npv_before_pool_costs_million,
                maximum_error});
        if (!nearly_equal(maximum_error, 0.0L, kMoneyTolerance)) {
            throw std::logic_error(
                "joint portfolio changed a claim's marginal financial result");
        }
    }
    return result;
}

} // namespace naturalehia::cellular_finance
