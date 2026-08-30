// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/claim_ledger_portfolio_adapter.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kClaimLedgerJointPortfolioVersion{"0.1.0"};

// The authoritative multi-claim entry point accepts loader-verified package
// roots and re-runs the one-claim adapter for every asset. The assembler never
// accepts a bare PortfolioConfig as evidence of a verified claim.
struct ClaimLedgerJointPortfolioAssetInput {
    ClaimLedgerPackage package{};
    ClaimLedgerPortfolioAdapterTerms adapter_terms{};
};

struct ClaimLedgerJointPortfolioSelection {
    std::string portfolio_project_id{};
    std::string marginal_scenario_id{};
};

// One complete state of the portfolio. Its probability and project-state
// selections are supplied explicitly; no Cartesian product, independence
// assumption, copula, resampling or probability calibration is performed.
struct ClaimLedgerJointPortfolioScenario {
    std::string scenario_id{};
    double physical_probability{0.0};
    // Identifies the retained analyst method, stress, cohort or other basis
    // for this supplied joint weight. It is lineage, not evidence promotion.
    std::string probability_basis_id{};
    std::vector<std::string> factor_tags{};
    std::vector<ClaimLedgerJointPortfolioSelection> selections{};
};

struct ClaimLedgerJointPortfolioTerms {
    std::string scenario_label{
        "Explicit joint states for verified claim-ledger assets"};
    std::string source_note{
        "Analyst-declared joint coupling; not empirical calibration or fair value"};
    std::vector<ClaimLedgerJointPortfolioScenario> joint_scenarios{};
};

struct ClaimLedgerJointScenarioLineage {
    std::string joint_scenario_id{};
    double declared_physical_probability{0.0};
    // Weight after normalization by the assembler's compensated declared
    // total, before Portfolio performs its authoritative final normalization.
    double configured_physical_probability{0.0};
    // Authoritative weight actually used in PortfolioSummary.
    double physical_probability{0.0};
    std::string probability_basis_id{};
    // Coupling-level declarations remain separate from the effective union so
    // overlap with a marginal factor cannot erase their provenance.
    std::vector<std::string> declared_factor_tags{};
    // Canonical union of declared joint and selected marginal factors.
    std::vector<std::string> factor_tags{};
};

struct ClaimLedgerJointSelectionLineage {
    std::string joint_scenario_id{};
    double joint_configured_physical_probability{0.0};
    // Authoritative evaluated Portfolio weight.
    double joint_physical_probability{0.0};
    std::string portfolio_project_id{};
    std::string marginal_scenario_id{};
    double claim_ledger_marginal_probability{0.0};
    double marginal_probability{0.0};
    std::size_t probability_known_at_period{0U};
    std::string probability_source_record_id{};
    std::vector<std::string> marginal_factor_tags{};
    std::string package_id{};
    std::string claim_config_sha256{};
    std::string economic_cluster_id{};
    std::string claim_id{};
};

struct ClaimLedgerJointMarginalReconciliation {
    std::string portfolio_project_id{};
    std::string marginal_scenario_id{};
    // Weight configured by the one-claim adapter before Portfolio performs
    // its authoritative final normalization.
    double configured_marginal_probability{0.0};
    // Authoritative normalized weight from the one-claim PortfolioSummary.
    double marginal_probability{0.0};
    double probability_from_joint_states{0.0};
    double reconciliation_error{0.0};
};

struct ClaimLedgerJointProjectReconciliation {
    std::string portfolio_project_id{};
    double marginal_expected_draws_million{0.0};
    double joint_expected_draws_million{0.0};
    double marginal_expected_receipts_million{0.0};
    double joint_expected_receipts_million{0.0};
    double marginal_expected_principal_loss_million{0.0};
    double joint_expected_principal_loss_million{0.0};
    double marginal_expected_npv_million{0.0};
    double joint_expected_npv_million{0.0};
    double maximum_reconciliation_error_million{0.0};
};

// These rows retain the exact selected one-claim lineage beneath each joint
// state. The outer identifiers prevent a marginal scenario label from being
// mistaken for a joint scenario label.
struct ClaimLedgerJointCashLineage {
    std::string joint_scenario_id{};
    std::string package_id{};
    std::string claim_config_sha256{};
    std::string marginal_scenario_id{};
    ClaimLedgerPortfolioCashLineage marginal_cash{};
};

struct ClaimLedgerJointCashBudgetLineage {
    std::string joint_scenario_id{};
    std::string package_id{};
    std::string claim_config_sha256{};
    std::string marginal_scenario_id{};
    ClaimLedgerPortfolioCashBudgetLineage marginal_budget{};
};

struct ClaimLedgerJointPortfolioResult {
    PortfolioConfig portfolio{};
    PortfolioSummary portfolio_summary{};
    // Complete one-project adapter outputs remain available for an audit of
    // the marginal distributions and their package-level cash lineage.
    std::vector<ClaimLedgerPortfolioAdapterResult> marginal_assets{};
    std::vector<ClaimLedgerPortfolioPackageLineage> package_lineage{};
    std::vector<ClaimLedgerJointScenarioLineage> joint_scenario_lineage{};
    std::vector<ClaimLedgerJointSelectionLineage> selection_lineage{};
    std::vector<ClaimLedgerJointMarginalReconciliation>
        marginal_reconciliations{};
    std::vector<ClaimLedgerJointProjectReconciliation>
        project_reconciliations{};
    std::vector<ClaimLedgerJointCashLineage> cash_lineage{};
    std::vector<ClaimLedgerJointCashBudgetLineage> cash_budget_lineage{};
    double maximum_marginal_probability_reconciliation_error{0.0};
    double maximum_project_reconciliation_error_million{0.0};
};

// Re-verifies every package, adapts each claim at its frozen decision cut and
// assembles only the supplied complete joint states. Every supplied marginal
// probability must be reproduced by those states. Cash-source identifiers,
// package roots and economic-cluster identifiers must be unique across
// assets; shared-source and same-cluster netting are deliberately unsupported
// in v0.1 and fail closed rather than double counting value.
[[nodiscard]] ClaimLedgerJointPortfolioResult
assemble_claim_ledger_joint_portfolio(
    const std::vector<ClaimLedgerJointPortfolioAssetInput>& assets,
    const ClaimLedgerJointPortfolioTerms& terms);

} // namespace naturalehia::cellular_finance
