// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/claim_ledger.hpp>
#include <naturalehia/cellular_finance/claim_ledger_package.hpp>
#include <naturalehia/cellular_finance/portfolio.hpp>

#include <optional>
#include <string>
#include <vector>

namespace naturalehia::cellular_finance {

// One allocation maps an exact decision-cut cash-receipt entry to an
// identified external source budget. A missing scenario_id means the entry is
// common to every path on which that economic fact remains selected. Multiple
// rows may split one entry, but their amounts must exhaust it exactly.
struct ClaimLedgerReceiptSourceAllocation {
    std::optional<std::string> scenario_id{};
    std::string entry_id{};
    std::string cash_source_id{};
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    double amount_million{0.0};
};

struct ClaimLedgerScenarioFactorSet {
    std::string scenario_id{};
    // These are explicit analyst declarations. The adapter never derives a
    // common factor from a project name, outcome, or source record.
    std::vector<std::string> factor_tags{};
};

// One externally supplied scenario cash budget. Allocations consume these
// rows; the adapter never manufactures availability from the receipt it is
// supposed to constrain. A missing scenario_id means the same dated budget is
// available in every scenario. Provider fields are required for cash sourced
// from a claim-ledger guarantee.
struct ClaimLedgerScenarioCashBudget {
    std::optional<std::string> scenario_id{};
    std::string cash_source_id{};
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    std::size_t month{0U};
    double amount_million{0.0};
    std::string counterparty_id{};
    std::optional<std::string> provider_claim_id{};
};

struct ClaimLedgerPortfolioAdapterTerms {
    std::string project_id{};
    ProjectStage stage{ProjectStage::Research};
    // A physical-measure hurdle sensitivity for the portfolio projection. It
    // is not inferred from the claim coupon or called fair value.
    double annual_physical_hurdle_rate{0.0};
    std::vector<ClaimLedgerReceiptSourceAllocation>
        receipt_source_allocations{};
    std::vector<ClaimLedgerScenarioCashBudget> cash_source_budgets{};
    std::vector<ClaimLedgerScenarioFactorSet> scenario_factor_sets{};
};

enum class ClaimLedgerPortfolioAdmissionBasis : unsigned char {
    SyntheticMechanics,
    ControlledExpectedReturn,
};

enum class ClaimLedgerPortfolioSourceScope : unsigned char {
    DecisionCut,
};

enum class ClaimLedgerPortfolioOutputKind : unsigned char {
    InvestorOutlay,
    PrincipalMovement,
    InvestorReceipt,
};

struct ClaimLedgerPortfolioPackageLineage {
    std::string claim_ledger_model_version{};
    std::string package_id{};
    std::string claim_config_sha256{};
    ClaimLedgerPackageStatus package_status{
        ClaimLedgerPackageStatus::RetainedPublicIncomplete};
    ClaimLedgerPortfolioAdmissionBasis admission_basis{
        ClaimLedgerPortfolioAdmissionBasis::SyntheticMechanics};
    ClaimLedgerPortfolioSourceScope source_scope{
        ClaimLedgerPortfolioSourceScope::DecisionCut};
    std::string economic_cluster_id{};
    ClaimLedgerEconomicClusterBoundaryStatus
        economic_cluster_boundary_status{
            ClaimLedgerEconomicClusterBoundaryStatus::Unresolved};
    std::string claim_id{};
    std::string claim_project_id{};
    std::string portfolio_project_id{};
    std::size_t decision_period{0U};
    std::size_t horizon_period{0U};
    // Calendar and information-axis identity. These fields are retained so a
    // multi-claim assembler cannot treat equal period numbers from different
    // origins or observation cuts as contemporaneous cash.
    std::string period_unit_label{};
    std::size_t periods_per_year{0U};
    std::string period_origin_date{};
    std::string decision_date{};
    std::string horizon_date{};
};

struct ClaimLedgerPortfolioCashLineage {
    std::string portfolio_scenario_id{};
    // Null identifies a selected common entry, not missing lineage.
    std::optional<std::string> claim_scenario_id{};
    std::string entry_id{};
    std::string economic_fact_id{};
    std::string event_group_id{};
    ClaimLedgerEntryKind entry_kind{ClaimLedgerEntryKind::BuyerPrice};
    std::size_t known_at_period{0U};
    std::string source_record_id{};
    std::string provider_claim_id{};
    ClaimLedgerPortfolioOutputKind output_kind{
        ClaimLedgerPortfolioOutputKind::InvestorOutlay};
    std::string portfolio_project_id{};
    std::optional<std::string> cash_source_id{};
    std::size_t month{0U};
    double amount_million{0.0};
    std::optional<double> principal_component_million{};
    // Populated for receipts from the validated external budget identity.
    // Empty for investor outlays and non-cash principal movements.
    std::string cash_budget_counterparty_id{};
    std::optional<std::string> cash_budget_provider_claim_id{};
};

// Exact external budget rows retained after scenario-scope expansion. A null
// declared_claim_scenario_id means that the supplied budget was common; the
// portfolio_scenario_id identifies each path in which that common row applies.
struct ClaimLedgerPortfolioCashBudgetLineage {
    std::string portfolio_scenario_id{};
    std::optional<std::string> declared_claim_scenario_id{};
    std::string cash_source_id{};
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    std::size_t month{0U};
    double amount_million{0.0};
    std::string counterparty_id{};
    std::optional<std::string> provider_claim_id{};
};

struct ClaimLedgerPortfolioPathBridge {
    std::string scenario_id{};
    // Exact upstream weight before adapter normalization.
    double claim_ledger_physical_probability{0.0};
    // Probability used by Portfolio after normalization to a unit sum.
    double physical_probability{0.0};
    // Buyer price and direct cost are project-attributed investor outlays, not
    // contractual principal. Direct cost affects NPV without becoming
    // protected notional.
    double buyer_price_cash_outflow_million{0.0};
    double buyer_direct_cost_million{0.0};
    double investor_receipts_million{0.0};
    double contractual_principal_loss_million{0.0};
    double portfolio_principal_loss_million{0.0};
    // An undiscounted cash-return diagnostic, not a principal loss or fair
    // value. It includes direct costs and all investor cash receipts.
    double nominal_investor_cash_shortfall_million{0.0};
    double maximum_monthly_cash_reconciliation_error_million{0.0};
    std::size_t probability_known_at_period{0U};
    std::string probability_source_record_id{};
    std::size_t cash_path_status_known_at_period{0U};
    std::string cash_path_status_source_record_id{};
    std::vector<std::string> decision_entry_ids{};
};

struct ClaimLedgerPortfolioAdapterResult {
    PortfolioConfig portfolio{};
    PortfolioSummary portfolio_summary{};
    std::vector<ClaimLedgerPortfolioPathBridge> paths{};
    std::optional<ClaimLedgerPortfolioPackageLineage> package_lineage{};
    std::vector<ClaimLedgerPortfolioCashLineage> cash_lineage{};
    std::vector<ClaimLedgerPortfolioCashBudgetLineage> cash_budget_lineage{};
    double expected_contractual_principal_loss_million{0.0};
    double expected_nominal_investor_cash_shortfall_million{0.0};
    double maximum_monthly_cash_reconciliation_error_million{0.0};
};

// Re-evaluates the supplied ledger and accepts only an exact, complete
// decision-cut expected-cash state. It emits a one-project synthetic
// PortfolioConfig because the portfolio remains a contract-mechanics engine,
// not an empirical-admission layer. Buyer price and buyer direct cost are
// classified investor outlays; every positive receipt must exhaust an
// explicit source allocation. Contractual additions, cash reductions,
// conversions and writeoffs populate a separate principal ledger, so issue
// price and contractual loss can never be conflated.
[[nodiscard]] ClaimLedgerPortfolioAdapterResult
adapt_claim_ledger_to_portfolio(const ClaimLedgerConfig& ledger,
    const ClaimLedgerPortfolioAdapterTerms& terms);

// Authoritative package boundary. The supplied root snapshot is reloaded and
// hash-compared before use. Retained-public-incomplete packages are rejected;
// controlled candidates require expected-return admission. Only the verified
// decision-cut core is adapted, never retained full/backtest state.
[[nodiscard]] ClaimLedgerPortfolioAdapterResult
adapt_claim_ledger_package_to_portfolio(const ClaimLedgerPackage& package,
    const ClaimLedgerPortfolioAdapterTerms& terms);

} // namespace naturalehia::cellular_finance
