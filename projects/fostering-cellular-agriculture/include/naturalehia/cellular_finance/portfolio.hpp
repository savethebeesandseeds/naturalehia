// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kPortfolioLegacyModelVersion{"0.1.0"};
inline constexpr std::string_view kPortfolioModelVersion{"0.2.0"};

enum class ProjectStage : unsigned char {
    Research,
    Pilot,
    Demonstration,
    FirstIndustrial,
    RepeatProduction,
};

// These labels identify the external source of cash available for an investor
// receipt. They do not assert that the source exists, is creditworthy, or is
// legally enforceable. In particular, refinancing is liquidity from a new
// financing claim; it is not operating value created by the underlying asset.
enum class PortfolioCashSource : unsigned char {
    Commercial,
    LicensingRoyalty,
    ExitSale,
    Recovery,
    Refinancing,
    ExplicitSupport,
    SponsorFee,
    // Fee cash paid to the claim investor when sponsor identity is not
    // established (for example, a settlement-withheld financing fee).
    FinancingFee,
};

enum class ProjectPathResolution : unsigned char {
    // The instrument has terminated by the analysis horizon. Funded principal
    // not returned by then is a realized principal loss on this path.
    Resolved,
    // The instrument remains legally/economically outstanding at the horizon.
    // Unreturned principal is exposure, not a modeled impairment or recovery.
    Continuing,
};

// Legacy portfolio rows use one cash draw as both investor outflow and
// principal creation. A normalized contractual claim instead supplies its
// principal roll-forward separately, so issue price, OID/premium and
// capitalized amounts cannot create a false loss or recovery.
enum class PrincipalAccountingMode : unsigned char {
    DrawEqualsPrincipalLegacy,
    ExplicitContractualLedger,
};

enum class PrincipalMovementKind : unsigned char {
    FundedPrincipalAddition,
    CapitalizedFeeAddition,
    CapitalizedInterestAddition,
    ConversionExtinguishment,
    Writeoff,
};

enum class InvestorOutlayPurpose : unsigned char {
    PrimaryProjectFunding,
    ClaimPurchasePrice,
    BuyerDirectCost,
};

struct PortfolioProject {
    std::string id{};
    ProjectStage stage{ProjectStage::Research};
    // Maximum investor funding or acquisition-price cash outflow. In legacy
    // mode this is also the principal limit.
    double commitment_million{0.0};
    PrincipalAccountingMode principal_accounting_mode{
        PrincipalAccountingMode::DrawEqualsPrincipalLegacy};
    // Required and positive only in explicit-contractual-ledger mode. It is
    // the maximum contractual principal balance and the loss-reference
    // notional used by layers and proportional protection.
    double principal_limit_million{0.0};
    // Contractual principal already outstanding at the analysis origin.
    double opening_principal_million{0.0};
};

struct MonthlyAmount {
    std::size_t month{0U};
    double amount_million{0.0};
};

struct InvestorReceipt {
    std::size_t month{0U};
    // References one identified scenario-level cash budget. Multiple projects
    // may draw from it, but their cumulative receipts share its finite amount.
    std::string cash_source_id{};
    double amount_million{0.0};
    // The portion of amount_million that retires funded principal. This is a
    // memo classification; it does not create additional cash.
    double principal_component_million{0.0};
};

struct PrincipalMovement {
    std::size_t month{0U};
    PrincipalMovementKind kind{
        PrincipalMovementKind::FundedPrincipalAddition};
    // A non-negative memo balance movement. It is never investor cash.
    double amount_million{0.0};
};

struct InvestorOutlay {
    std::size_t month{0U};
    InvestorOutlayPurpose purpose{
        InvestorOutlayPurpose::PrimaryProjectFunding};
    double amount_million{0.0};
};

struct ProjectJointPath {
    std::string project_id{};
    ProjectPathResolution resolution{ProjectPathResolution::Resolved};
    // Legacy cash funding field. Explicit-contractual-ledger paths instead
    // use classified investor_outlays and must leave this empty.
    std::vector<MonthlyAmount> capital_draws{};
    std::vector<InvestorReceipt> investor_receipts{};
    // Empty and prohibited in legacy mode. In explicit mode these movements,
    // opening principal and principal cash receipts form an independent
    // contractual balance ledger. Monthly ordering is additions, principal
    // cash, conversion extinguishment, then writeoff.
    std::vector<PrincipalMovement> principal_movements{};
    std::vector<InvestorOutlay> investor_outlays{};
};

struct ScenarioCashSource {
    std::string id{};
    PortfolioCashSource kind{PortfolioCashSource::Commercial};
    // Gross external cash budget available to all project instruments in this
    // joint scenario. Unused availability is not an investor receipt.
    std::vector<MonthlyAmount> cash_available{};
};

struct JointScenario {
    std::string id{};
    // Analyst-declared physical-measure weight. Accepted near-one totals are
    // normalized for every aggregate calculation and remain visible in output.
    double weight{0.0};
    // Each configured project must appear exactly once. This is the declared
    // dependence model; no independence assumption or resampling is added.
    std::vector<ProjectJointPath> project_paths{};
    std::vector<ScenarioCashSource> cash_sources{};
    std::vector<MonthlyAmount> pool_costs{};
    std::vector<std::string> factor_tags{};
};

struct LossLayer {
    std::string id{};
    double attachment_million{0.0};
    double detachment_million{0.0};
};

struct PortfolioConfig {
    std::string model_version{kPortfolioModelVersion};
    std::string scenario_label{"unnamed synthetic joint-scenario analysis"};
    std::string source_note{
        "Unvalidated synthetic assumptions for contract-mechanics testing"};
    std::string currency_label{"DEMO"};
    std::string monetary_basis{"unspecified-synthetic"};
    bool synthetic_inputs{true};
    std::size_t horizon_months{120U};
    // A physical-measure sensitivity supplied by the analyst. The resulting
    // NPV discounts dated investor cash only; it assigns no terminal value to
    // continuing exposure unless an explicit dated receipt is supplied. It is
    // not fair value, a market quote, or an arbitrage-free price.
    double annual_physical_hurdle_rate{0.0};
    std::vector<PortfolioProject> projects{};
    std::vector<JointScenario> joint_scenarios{};
    // Optional. If present, layers must form one contiguous partition from
    // zero through aggregate project commitment. Packaging allocates modeled
    // loss; it cannot create project cash or reduce aggregate loss.
    std::vector<LossLayer> loss_layers{};
};

struct PortfolioDistributionSummary {
    double mean{0.0};
    double standard_deviation{0.0};
    double p50{0.0};
    double p95{0.0};
    double p99{0.0};
    double maximum{0.0};
    // Upper-tail averages of the reported metric. They are severity measures
    // for non-negative exposure/loss series. For NPV, use the NPV-shortfall
    // distribution and negative-NPV probability for downside interpretation.
    double expected_shortfall_95{0.0};
    double expected_shortfall_99{0.0};
};

struct ReturnSourceTotal {
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    double nominal_million{0.0};
    double present_value_million{0.0};
};

struct MonthlyInvestorCashFlow {
    std::size_t month{0U};
    double capital_draws_million{0.0};
    // Equal to capital_draws_million for legacy paths; the preferred v0.2
    // name also includes classified purchase price and direct-cost outlays.
    double investor_outlays_million{0.0};
    double investor_receipts_million{0.0};
    double pool_costs_million{0.0};
    // Gross liquidity required before this month's receipts are released.
    double funding_need_million{0.0};
    double net_cash_flow_million{0.0};
    // End-of-month balance after this month's receipts; the interim gross
    // funding peak is retained separately on JointScenarioResult.
    double cumulative_net_cash_flow_million{0.0};
};

struct ProjectPathResult {
    std::string project_id{};
    ProjectPathResolution resolution{ProjectPathResolution::Resolved};
    double total_draws_million{0.0};
    double total_investor_outlays_million{0.0};
    double total_receipts_million{0.0};
    double opening_principal_million{0.0};
    double principal_added_million{0.0};
    double principal_returned_million{0.0};
    double principal_converted_million{0.0};
    double outstanding_principal_million{0.0};
    double principal_loss_million{0.0};
    // Direct project instrument cash flows only; pool costs are reported and
    // deducted at pool level rather than silently allocated among projects.
    double npv_before_pool_costs_million{0.0};
    std::vector<ReturnSourceTotal> return_sources{};
};

struct LayerPathResult {
    std::string layer_id{};
    double principal_loss_million{0.0};
};

struct JointScenarioResult {
    std::string scenario_id{};
    double declared_weight{0.0};
    double normalized_weight{0.0};
    std::vector<std::string> factor_tags{};
    std::vector<ProjectPathResult> projects{};
    std::vector<MonthlyInvestorCashFlow> monthly_cash_flows{};
    std::vector<ReturnSourceTotal> return_sources{};
    std::vector<LayerPathResult> layers{};
    double total_draws_million{0.0};
    double total_investor_outlays_million{0.0};
    double total_receipts_million{0.0};
    double total_pool_costs_million{0.0};
    double peak_same_month_draw_million{0.0};
    double peak_same_month_funding_need_million{0.0};
    // Assumes each month's draws and pool costs settle before that month's
    // receipts, preventing same-month netting from hiding gross liquidity.
    double peak_cumulative_net_outlay_million{0.0};
    double principal_returned_million{0.0};
    double opening_principal_million{0.0};
    double principal_added_million{0.0};
    double principal_converted_million{0.0};
    double outstanding_principal_million{0.0};
    double principal_loss_million{0.0};
    double npv_million{0.0};
};

struct ProjectPortfolioSummary {
    std::string project_id{};
    PortfolioDistributionSummary total_draws_million{};
    PortfolioDistributionSummary total_receipts_million{};
    PortfolioDistributionSummary outstanding_principal_million{};
    PortfolioDistributionSummary principal_loss_million{};
    PortfolioDistributionSummary npv_before_pool_costs_million{};
    PortfolioDistributionSummary npv_shortfall_before_pool_costs_million{};
    double expected_draws_million{0.0};
    double expected_receipts_million{0.0};
    double expected_outstanding_principal_million{0.0};
    double expected_principal_loss_million{0.0};
    double expected_npv_before_pool_costs_million{0.0};
    double principal_impairment_probability{0.0};
    double negative_npv_probability{0.0};
    // Contributions use the same worst aggregate-loss tail as pool ES. Their
    // sum therefore reconciles exactly to aggregate pool ES.
    double pool_loss_tail_contribution_es95_million{0.0};
    double pool_loss_tail_contribution_es99_million{0.0};
};

struct PairwiseLossCorrelation {
    std::string first_project_id{};
    std::string second_project_id{};
    // Absent if either project's loss is constant across the declared measure.
    std::optional<double> correlation{};
};

struct LayerPortfolioSummary {
    std::string layer_id{};
    double attachment_million{0.0};
    double detachment_million{0.0};
    PortfolioDistributionSummary principal_loss_million{};
    double expected_loss_million{0.0};
    double impairment_probability{0.0};
    double exhaustion_probability{0.0};
};

struct PortfolioSummary {
    std::vector<JointScenarioResult> scenarios{};
    std::vector<ProjectPortfolioSummary> projects{};
    std::vector<PairwiseLossCorrelation> pairwise_loss_correlations{};
    std::vector<LayerPortfolioSummary> layers{};
    std::vector<ReturnSourceTotal> expected_return_sources{};

    // Raw declared sum before accepted near-one normalization.
    double configured_scenario_weight_sum{0.0};
    PortfolioDistributionSummary total_draws_million{};
    PortfolioDistributionSummary peak_same_month_draw_million{};
    PortfolioDistributionSummary peak_same_month_funding_need_million{};
    PortfolioDistributionSummary peak_cumulative_net_outlay_million{};
    PortfolioDistributionSummary outstanding_principal_million{};
    PortfolioDistributionSummary principal_loss_million{};
    PortfolioDistributionSummary npv_million{};
    PortfolioDistributionSummary npv_shortfall_million{};
    double principal_impairment_probability{0.0};
    double negative_npv_probability{0.0};

    double sum_standalone_es95_million{0.0};
    double sum_standalone_es99_million{0.0};
    double diversification_benefit_es95_million{0.0};
    double diversification_benefit_es99_million{0.0};
    // One minus pool ES divided by the sum of standalone ES: zero means no
    // measured diversification benefit and larger values mean more benefit.
    // Absent when every standalone ES is zero.
    std::optional<double> diversification_ratio_es95{};
    std::optional<double> diversification_ratio_es99{};

    double maximum_cash_reconciliation_error_million{0.0};
    double maximum_layer_reconciliation_error_million{0.0};
};

[[nodiscard]] std::string_view to_string(ProjectStage stage) noexcept;
[[nodiscard]] std::string_view to_string(
    ProjectPathResolution resolution) noexcept;
[[nodiscard]] std::string_view to_string(
    PrincipalAccountingMode mode) noexcept;
[[nodiscard]] std::string_view to_string(
    PrincipalMovementKind kind) noexcept;
[[nodiscard]] std::string_view to_string(
    InvestorOutlayPurpose purpose) noexcept;
[[nodiscard]] std::string_view to_string(
    PortfolioCashSource source) noexcept;

// Returns the legal principal notional for loss allocation. Legacy projects
// use commitment; explicit projects use their separate principal limit.
[[nodiscard]] double portfolio_reference_principal_limit(
    const PortfolioProject& project);
[[nodiscard]] double portfolio_aggregate_reference_principal(
    const PortfolioConfig& config);

void validate_portfolio_config(const PortfolioConfig& config);

// Evaluates the exact supplied joint distribution. It performs no simulation,
// independence inference, probability calibration, or scenario generation.
[[nodiscard]] PortfolioSummary evaluate_portfolio(
    const PortfolioConfig& config);

} // namespace naturalehia::cellular_finance
