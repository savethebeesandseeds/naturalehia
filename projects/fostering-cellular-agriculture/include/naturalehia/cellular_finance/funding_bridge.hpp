// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/capital_stack.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kFundingBridgeModelVersion{"0.1.0"};

// Contractual capacity, a policy request, and the provider's later settlement
// outcome are separate. Only settled cash enters the controlled-cash ledger.
struct FundingProvider {
    std::string id{};
    std::string economic_group_id{};
    // Conservative simultaneous capacity: cumulative settled funding still
    // exposed plus outstanding contingent obligations may not exceed it.
    double declared_capacity_million{0.0};
    bool identity_evidenced{false};
    bool affiliation_evidenced{false};
    bool capacity_evidenced{false};
    std::string source_record_id{};
};

struct CallableCapitalFacility {
    std::string id{};
    std::string provider_id{};
    std::string source_record_id{};
    double commitment_million{0.0};
    std::size_t availability_start_month{0U};
    // Last month in which a notice may be issued. A valid notice may settle
    // after this month when its fixed lag remains inside the model horizon.
    std::size_t contractual_expiry_month{0U};
    std::size_t settlement_lag_months{0U};
    std::string permitted_purpose{};
    double annual_commitment_fee_rate{0.0};
    double liquidity_reserve_fraction{0.0};
    double annual_liquidity_hurdle_rate{0.0};
    double annual_reserve_yield_rate{0.0};
};

struct WarehouseFacility {
    std::string id{};
    std::string provider_id{};
    std::string source_record_id{};
    double committed_limit_million{0.0};
    std::size_t availability_start_month{0U};
    // Last request month. Every requested advance must still settle no later
    // than legal_maturity_month.
    std::size_t availability_end_month{0U};
    std::size_t legal_maturity_month{0U};
    std::size_t settlement_lag_months{0U};
    std::string permitted_purpose{};
    double collateral_advance_rate{0.0};
    double annual_interest_rate{0.0};
    double annual_undrawn_fee_rate{0.0};
    double advance_fee_rate{0.0};
    double upfront_fee_rate{0.0};
};

struct CapitalCallRequest {
    std::string id{};
    std::string facility_id{};
    std::size_t notice_month{0U};
    double requested_million{0.0};
};

struct WarehouseDrawRequest {
    std::string id{};
    std::string facility_id{};
    std::size_t request_month{0U};
    double requested_million{0.0};
};

enum class FundingRequestKind : unsigned char {
    CallableCapital,
    WarehouseDraw,
};

// Result row for one policy request. All same-month call and warehouse rows
// share one atomic acceptance decision under the v0.1 monthly convention.
struct FundingProviderRequestResult {
    std::size_t action_month{0U};
    FundingRequestKind kind{FundingRequestKind::CallableCapital};
    std::string provider_id{};
    std::string facility_id{};
    std::string request_id{};
    std::string facility_source_record_id{};
    double requested_million{0.0};
    bool accepted{false};
};

enum class FundingSettlementStatus : unsigned char {
    SettledInFull,
    FinalPartialSettlement,
    Failed,
};

// A final provider outcome references exactly one prior request. A partial or
// failed remainder becomes provider non-performance; it is never silently
// restored to available capacity.
struct FundingSettlementOutcome {
    std::string request_id{};
    std::size_t settlement_month{0U};
    FundingSettlementStatus status{FundingSettlementStatus::SettledInFull};
    double actual_cash_million{0.0};
    std::string source_record_id{};
};

// Immutable result row for one mandatory final provider outcome. This ledger
// retains fully settled, partial, and failed outcomes; aggregate cash/default
// fields are not a substitute for request-level provenance.
struct FundingProviderSettlementResult {
    std::size_t settlement_month{0U};
    FundingSettlementStatus status{FundingSettlementStatus::SettledInFull};
    std::string provider_id{};
    std::string facility_id{};
    std::string request_id{};
    std::string source_record_id{};
    double requested_million{0.0};
    double actual_cash_million{0.0};
    double missing_cash_million{0.0};
};

enum class SupplementalFundingPurpose : unsigned char {
    // Restricted to buyer-direct costs, pool costs, and facility charges.
    CostSupport,
    // Qualifying, settled, subordinated capital that replenishes protection.
    ProtectionReplenishment,
    // Gross settled refinancing cash. The replacement liability remains
    // outside the bridge and therefore suppresses its investor-NPV outputs.
    SettledTakeout,
};

struct SupplementalFundingReceipt {
    std::string id{};
    std::string provider_id{};
    std::size_t month{0U};
    SupplementalFundingPurpose purpose{SupplementalFundingPurpose::CostSupport};
    double actual_cash_million{0.0};
    std::string source_record_id{};
};

enum class EligibleBasisMovementKind : unsigned char {
    EligibleAddition,
    PrincipalBasisReturn,
    Disposition,
    Writeoff,
    EligibilityRemoval,
};

// Cost basis is not inferred from contractual principal. Additions must be
// supported by paid acquisition/primary-funding uses; every submitted
// reduction is a separate aggregate movement. Synthetic v0.1 cannot prove
// that the submitter omitted no required reduction.
struct EligibleBasisMovement {
    std::string id{};
    std::size_t month{0U};
    EligibleBasisMovementKind kind{EligibleBasisMovementKind::EligibleAddition};
    double amount_million{0.0};
    std::string reference_id{};
    std::string source_record_id{};
};

// Protection absorption is not inferred from contractual principal or from
// an eligibility change. It is a separate evidenced economic event.
struct ProtectionAbsorption {
    std::string id{};
    std::size_t month{0U};
    double amount_million{0.0};
    std::string reference_id{};
    std::string source_record_id{};
};

struct ProtectionRelease {
    std::string id{};
    std::size_t month{0U};
    double amount_million{0.0};
    std::string source_record_id{};
};

struct FundingBridgeScenarioPerformance {
    std::string scenario_id{};
    std::vector<CapitalCallRequest> capital_call_requests{};
    std::vector<FundingSettlementOutcome> capital_call_outcomes{};
    std::vector<WarehouseDrawRequest> warehouse_draw_requests{};
    std::vector<FundingSettlementOutcome> warehouse_draw_outcomes{};
    std::vector<SupplementalFundingReceipt> supplemental_receipts{};
    std::vector<EligibleBasisMovement> eligible_basis_movements{};
    std::vector<ProtectionAbsorption> protection_absorptions{};
    std::vector<ProtectionRelease> protection_releases{};
};

struct FundingBridgeConfig {
    std::string model_version{kFundingBridgeModelVersion};
    std::string scenario_label{
        "unnamed synthetic callable-capital and warehouse bridge"};
    std::string source_note{
        "Unvalidated synthetic funding-performance inputs for mechanics testing"};
    bool synthetic_inputs{true};

    // Settled subordinated cash at month zero. It must equal the frozen v0.2
    // capital stack's first-loss-residual notional.
    double funded_at_close_cash_million{0.0};
    std::string funded_at_close_provider_id{};
    std::string funded_at_close_source_record_id{};
    std::vector<FundingProvider> providers{};
    CallableCapitalFacility callable_facility{};
    WarehouseFacility warehouse_facility{};

    // Explicit accounting assertions. They make the intended economic
    // boundary machine-checkable; they do not establish enforceability.
    bool uncalled_commitment_is_not_cash_or_loss_absorption{false};
    bool acquisition_and_primary_funding_uses_precede_same_month_receipts{
        false};
    bool warehouse_is_external_temporary_debt{false};
    bool warehouse_proceeds_cannot_fund_interest_fees_or_costs{false};
    bool project_receipts_sweep_warehouse_principal_before_investor_cash{
        false};
    bool policy_uses_observed_history_only{false};
    bool no_dynamic_tranche_allocation_or_pricing_is_claimed{false};

    // Must match the PortfolioConfig scenario ids exactly. Requests are
    // validated for nonanticipativity across identical observed histories.
    std::vector<FundingBridgeScenarioPerformance> scenario_performance{};
};

enum class FundingBridgeFailureKind : unsigned char {
    None,
    CallableFacilityUnavailable,
    WarehouseFacilityUnavailable,
    SimultaneousFacilityRequestFailure,
    ProviderCapacityExceeded,
    WarehouseLimitExceeded,
    WarehouseBorrowingBaseDeficit,
    FundedProtectionDeficit,
    NonAssetCostShortfall,
    FundingUseShortfall,
    ProtectionReleaseShortfall,
    WarehouseMaturityUnpaid,
};

enum class FundingBridgeFailurePhase : unsigned char {
    None,
    ContractAndProviderTest,
    NonAssetCosts,
    AssetFundingUse,
    EligibilityAndProtectionTest,
    LegalMaturity,
};

struct FundingBridgeFailureRecord {
    FundingBridgeFailureKind kind{FundingBridgeFailureKind::None};
    FundingBridgeFailurePhase phase{FundingBridgeFailurePhase::None};
    std::size_t month{0U};
    std::string provider_id{};
    std::string facility_id{};
    std::string request_id{};
    std::string source_record_id{};
    double amount_due_million{0.0};
    double amount_available_million{0.0};
    double shortfall_million{0.0};
    // For an atomic rejected closing, this is the whole rejected transaction;
    // shortfall_million remains only the binding restricted-source gap.
    double rejected_transaction_million{0.0};
    double eligible_purpose_cash_available_million{0.0};
    std::vector<std::string> causal_source_record_ids{};
    struct SettlementShortfall {
        std::size_t settlement_month{0U};
        std::string provider_id{};
        std::string facility_id{};
        std::string request_id{};
        std::string source_record_id{};
        double missing_cash_million{0.0};
    };
    // Every provider settlement shortfall observed through the failure month.
    // Prior-month rows are disclosure context, not automatic but-for
    // attribution of the later failure.
    std::vector<SettlementShortfall> settlement_shortfalls{};
    struct ProviderCapacityBreach {
        std::string provider_id{};
        std::string source_record_id{};
        double capacity_used_million{0.0};
        double declared_capacity_million{0.0};
        double excess_million{0.0};
    };
    // All simultaneous legal-provider capacity breaches at t*. A scalar
    // provider headline is populated only when this vector has one row.
    std::vector<ProviderCapacityBreach> provider_capacity_breaches{};
    std::string explanation{};
};

enum class WarehouseAdvanceTestPhase : unsigned char {
    Request,
    Settlement,
};

struct WarehouseAdvanceTestResult {
    std::string request_id{};
    WarehouseAdvanceTestPhase phase{WarehouseAdvanceTestPhase::Request};
    double requested_million{0.0};
    double settled_million{0.0};
    // At request phase this includes funded principal, prior unresolved
    // requests, burned/defaulted capacity, and the new request. A request is
    // contingent rather than funded debt, so F/BB tests become applicable
    // only when cash settles.
    double prospective_funded_and_contingent_dependency_million{0.0};
    double principal_after_settlement_million{0.0};
    double eligible_basis_million{0.0};
    double funded_protection_million{0.0};
    double required_funded_protection_million{0.0};
    double borrowing_base_million{0.0};
    double borrowing_base_headroom_million{0.0};
    double protection_headroom_million{0.0};
    bool facility_limit_passed{false};
    bool funded_protection_test_applicable{false};
    bool funded_protection_test_passed{false};
    bool borrowing_base_test_applicable{false};
    bool borrowing_base_test_passed{false};
    bool passed{false};
};

struct FundingBridgeMonthlyResult {
    std::size_t month{0U};
    bool processed{false};

    // The opening and closing source-use cash buckets are mutually exclusive.
    // controlled_cash is the sum of qualifying-protection and callable cash;
    // it does not include cost support or warehouse proceeds.
    double opening_qualifying_protection_cash_million{0.0};
    double opening_callable_cash_million{0.0};
    double opening_controlled_cash_million{0.0};
    double opening_cost_support_cash_million{0.0};
    double opening_warehouse_proceeds_cash_million{0.0};
    double opening_takeout_cash_million{0.0};
    double opening_project_principal_cash_million{0.0};
    double opening_project_nonprincipal_cash_million{0.0};
    double funded_at_close_receipt_million{0.0};
    double capital_call_requested_million{0.0};
    double capital_call_settled_million{0.0};
    double callable_available_undrawn_million{0.0};
    double callable_called_unsettled_million{0.0};
    double callable_defaulted_million{0.0};
    double callable_expired_uncalled_million{0.0};
    double callable_commitment_fee_due_million{0.0};
    double callable_liquidity_opportunity_cost_million{0.0};

    double warehouse_draw_requested_million{0.0};
    double warehouse_draw_called_unsettled_million{0.0};
    double warehouse_draw_defaulted_million{0.0};
    double warehouse_advance_settled_million{0.0};
    double warehouse_opening_principal_million{0.0};
    double warehouse_opening_unpaid_charges_million{0.0};
    double warehouse_interest_due_million{0.0};
    double warehouse_undrawn_fee_due_million{0.0};
    double warehouse_advance_fee_due_million{0.0};
    double warehouse_upfront_fee_due_million{0.0};
    double warehouse_charges_paid_million{0.0};
    double warehouse_charges_unpaid_million{0.0};
    double warehouse_closing_unpaid_charges_million{0.0};
    double warehouse_principal_repayment_million{0.0};
    double warehouse_closing_principal_million{0.0};
    double warehouse_past_due_principal_million{0.0};
    double warehouse_principal_repaid_from_call_settlement_million{0.0};
    double warehouse_principal_repaid_from_takeout_million{0.0};
    double warehouse_principal_repaid_from_project_principal_million{0.0};
    double warehouse_principal_repaid_from_project_nonprincipal_million{0.0};
    double warehouse_principal_repaid_from_unused_warehouse_cash_million{0.0};
    double warehouse_principal_repaid_from_callable_cash_million{0.0};
    double warehouse_principal_repaid_from_protection_cash_million{0.0};
    std::vector<WarehouseAdvanceTestResult> warehouse_advance_tests{};

    double cost_support_receipts_million{0.0};
    double protection_replenishment_receipts_million{0.0};
    double settled_takeout_receipts_million{0.0};
    double buyer_direct_cost_due_million{0.0};
    double pool_cost_due_million{0.0};
    double total_nonasset_cost_due_million{0.0};
    double total_nonasset_cost_paid_million{0.0};
    double total_nonasset_cost_unpaid_million{0.0};
    double nonasset_cost_liquidity_gap_million{0.0};

    double acquisition_and_primary_funding_due_million{0.0};
    double acquisition_and_primary_funding_paid_million{0.0};
    double acquisition_and_primary_funding_unpaid_million{0.0};
    double acquisition_and_primary_funding_liquidity_gap_million{0.0};
    double asset_uses_paid_from_qualifying_protection_cash_million{0.0};
    double asset_uses_paid_from_callable_cash_million{0.0};
    double asset_uses_paid_from_warehouse_proceeds_million{0.0};
    double declared_counterfactual_project_receipts_million{0.0};
    double declared_counterfactual_project_principal_receipts_million{0.0};
    double actual_project_receipts_million{0.0};
    double actual_project_principal_receipts_million{0.0};
    double actual_project_nonprincipal_receipts_million{0.0};
    double investor_distribution_million{0.0};
    double protection_release_million{0.0};

    double eligible_basis_opening_million{0.0};
    double eligible_basis_additions_million{0.0};
    double eligible_basis_principal_returns_million{0.0};
    double eligible_basis_dispositions_million{0.0};
    double eligible_basis_writeoffs_million{0.0};
    double eligible_basis_removals_million{0.0};
    double eligible_basis_closing_million{0.0};
    double funded_protection_opening_million{0.0};
    double qualifying_protection_invested_basis_opening_million{0.0};
    double retained_protection_supporting_asset_basis_opening_million{0.0};
    double funded_protection_paid_million{0.0};
    double funded_protection_released_million{0.0};
    double funded_protection_absorbed_million{0.0};
    double funded_protection_closing_million{0.0};
    double qualifying_protection_invested_basis_closing_million{0.0};
    double retained_protection_supporting_asset_basis_closing_million{0.0};
    double protection_cash_reclassified_from_project_receipts_million{0.0};
    double eligible_basis_writeoff_not_absorbed_by_protection_million{0.0};
    double required_funded_protection_million{0.0};
    double funded_protection_headroom_million{0.0};
    double warehouse_borrowing_base_before_reductions_million{0.0};
    double warehouse_borrowing_base_after_reductions_million{0.0};
    double warehouse_borrowing_base_headroom_million{0.0};

    double closing_qualifying_protection_cash_million{0.0};
    double closing_callable_cash_million{0.0};
    double closing_controlled_cash_million{0.0};
    double closing_cost_support_cash_million{0.0};
    double closing_warehouse_proceeds_cash_million{0.0};
    double closing_takeout_cash_million{0.0};
    double closing_project_principal_cash_million{0.0};
    double closing_project_nonprincipal_cash_million{0.0};
    double total_actual_cash_sources_million{0.0};
    double total_actual_cash_uses_million{0.0};
    double cash_reconciliation_error_million{0.0};
    double warehouse_reconciliation_error_million{0.0};
    double warehouse_charge_reconciliation_error_million{0.0};
    double eligible_basis_reconciliation_error_million{0.0};
    double funded_protection_reconciliation_error_million{0.0};
    // Computed from the complete canonical observed history. Missing request
    // rows at a month are treated as explicit zero decisions. The engine
    // compares the full history strings; this id is disclosure only.
    std::string computed_information_set_id{};
    std::optional<double> investor_net_cash_flow_million{};
};

struct FundingProviderScenarioResult {
    std::string provider_id{};
    std::string economic_group_id{};
    double declared_capacity_million{0.0};
    double settled_initial_and_supplemental_capital_million{0.0};
    double settled_callable_capital_million{0.0};
    double ending_available_callable_commitment_million{0.0};
    double ending_called_unsettled_callable_million{0.0};
    double callable_defaulted_million{0.0};
    double expired_uncalled_callable_million{0.0};
    double peak_warehouse_funded_ead_million{0.0};
    double ending_warehouse_funded_ead_million{0.0};
    double cumulative_settled_warehouse_advances_million{0.0};
    double ending_warehouse_contingent_commitment_million{0.0};
    double ending_called_unsettled_warehouse_million{0.0};
    double warehouse_draw_defaulted_million{0.0};
    double settled_takeout_cash_million{0.0};
    double capacity_used_million{0.0};
    double capacity_headroom_million{0.0};
    bool provider_nonperformance_observed{false};
    double callable_default_share_of_request{0.0};
    double warehouse_default_share_of_request{0.0};
};

struct FundingEconomicGroupScenarioResult {
    std::string economic_group_id{};
    // Gross settled funding intentionally counts each external settlement,
    // including refinancing cash, and is not an endpoint exposure measure.
    double cumulative_gross_settled_funding_million{0.0};
    double ending_contingent_funding_dependency_million{0.0};
    double cumulative_gross_settled_funding_share{0.0};
    double ending_contingent_funding_dependency_share{0.0};
};

struct FundingBridgeScenarioResult {
    std::string scenario_id{};
    bool feasible{false};
    FundingBridgeFailureKind failure_kind{FundingBridgeFailureKind::None};
    FundingBridgeFailurePhase failure_phase{FundingBridgeFailurePhase::None};
    std::optional<std::size_t> first_infeasible_month{};
    std::optional<FundingBridgeFailureRecord> failure{};
    std::vector<FundingBridgeMonthlyResult> months{};
    std::vector<FundingProviderScenarioResult> providers{};
    std::vector<FundingEconomicGroupScenarioResult> economic_groups{};
    // One row for every callable and warehouse policy request processed
    // before the scenario stops, including rows rejected by the atomic
    // same-month request phase.
    std::vector<FundingProviderRequestResult> provider_requests{};
    // One dated row for every final call/draw outcome processed before the
    // scenario stops, including settlements in full.
    std::vector<FundingProviderSettlementResult> provider_settlements{};
    // Complete dated request/outcome lineage for every provider shortfall
    // observed before scenario processing stops, including feasible paths
    // preserved by explicit replacement cash.
    std::vector<FundingBridgeFailureRecord::SettlementShortfall>
        provider_settlement_shortfalls{};

    double total_call_requested_million{0.0};
    double total_actual_call_receipts_million{0.0};
    double ending_available_callable_commitment_million{0.0};
    double ending_called_unsettled_callable_million{0.0};
    double callable_defaulted_million{0.0};
    double expired_uncalled_callable_million{0.0};
    double total_warehouse_draw_requested_million{0.0};
    double total_warehouse_advances_million{0.0};
    double warehouse_draw_defaulted_million{0.0};
    double ending_called_unsettled_warehouse_million{0.0};
    double total_warehouse_principal_repayments_million{0.0};
    double total_warehouse_interest_million{0.0};
    double total_warehouse_fees_million{0.0};
    double total_callable_commitment_fees_million{0.0};
    double ending_warehouse_principal_million{0.0};
    double ending_warehouse_unpaid_charges_million{0.0};
    double ending_warehouse_funded_ead_million{0.0};
    double peak_warehouse_funded_ead_million{0.0};
    std::size_t warehouse_exposure_months{0U};
    double total_cost_support_receipts_million{0.0};
    double total_protection_replenishment_receipts_million{0.0};
    double total_settled_takeout_receipts_million{0.0};
    double total_acquisition_and_primary_funding_uses_million{0.0};
    double total_buyer_direct_cost_uses_million{0.0};
    double total_pool_cost_uses_million{0.0};
    double total_project_receipts_million{0.0};
    double first_funding_shortfall_million{0.0};
    double total_funding_shortfall_million{0.0};
    double ending_controlled_cash_return_million{0.0};
    double ending_eligible_basis_million{0.0};
    double ending_funded_protection_million{0.0};
    double minimum_funded_protection_headroom_million{0.0};
    double minimum_warehouse_borrowing_base_headroom_million{0.0};
    double callable_liquidity_opportunity_cost_pv_million{0.0};
    double cumulative_gross_settled_funding_source_hhi{0.0};
    double ending_economic_group_contingent_funding_dependency_hhi{0.0};
    double maximum_cash_reconciliation_error_million{0.0};
    double maximum_warehouse_reconciliation_error_million{0.0};
    double maximum_warehouse_charge_reconciliation_error_million{0.0};
    double maximum_eligible_basis_reconciliation_error_million{0.0};
    double maximum_funded_protection_reconciliation_error_million{0.0};

    // Successful-path outputs are unavailable after any failure. Cash NPV is
    // also unavailable when takeout debt has settled because that replacement
    // liability and its later cash flows are outside this bridge.
    std::optional<double> investor_cash_npv_million{};
    std::optional<double> fully_prefunded_baseline_npv_million{};
    std::optional<double> cash_npv_change_vs_fully_prefunded_million{};
    std::optional<double> economic_npv_change_after_callable_liquidity_cost_million{};
    std::optional<double> fully_prefunded_prefunding_drag_million{};
    std::optional<double> preserved_project_receipts_million{};
    std::optional<double> preserved_project_principal_loss_million{};
    std::optional<bool> project_cash_is_preserved{};
    std::optional<bool> gross_project_principal_loss_is_preserved{};
};

struct FundingBridgeSummary {
    std::string model_version{};
    std::string scenario_label{};
    std::string source_note{};
    bool synthetic_inputs{true};
    double funded_protection_target_million{0.0};
    CallableCapitalFacility callable_facility{};
    WarehouseFacility warehouse_facility{};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    std::vector<FundingBridgeScenarioResult> scenarios{};

    std::optional<AmbiguityMetricRange> expected_investor_cash_npv_million{};
    std::optional<AmbiguityMetricRange>
        expected_cash_npv_change_vs_fully_prefunded_million{};
    std::optional<AmbiguityMetricRange>
        expected_economic_npv_change_after_callable_liquidity_cost_million{};
    AmbiguityMetricRange funding_failure_probability{};
    AmbiguityMetricRange expected_funding_shortfall_million{};
    AmbiguityMetricRange funding_shortfall_expected_shortfall_95_million{};
    AmbiguityMetricRange funding_shortfall_expected_shortfall_99_million{};
    AmbiguityMetricRange expected_ending_warehouse_funded_ead_million{};
    AmbiguityMetricRange
        ending_warehouse_funded_ead_expected_shortfall_95_million{};
    AmbiguityMetricRange
        ending_warehouse_funded_ead_expected_shortfall_99_million{};
    AmbiguityMetricRange expected_peak_warehouse_funded_ead_million{};
    AmbiguityMetricRange expected_callable_provider_default_million{};

    // This validates the v0.1 synthetic convention that all prior-month path
    // facts are observed. Live known-at provenance is outside v0.1.
    bool monthly_path_nonanticipativity_validated{false};
    bool uncalled_capital_is_counted_as_cash_or_loss_absorption{false};
    bool warehouse_is_counted_as_permanent_asset_capital{false};
    bool warehouse_loss_or_recovery_is_inferred{false};
    bool project_cash_is_changed_on_feasible_paths{false};
    bool gross_project_principal_loss_is_changed_on_feasible_paths{false};
    bool dynamic_tranche_allocation_is_available{false};
    bool fair_value_or_market_price_is_estimated{false};
    bool probabilities_are_calibrated{false};
    bool legal_enforceability_is_validated{false};
    std::string model_limitation{};

    double maximum_cash_reconciliation_error_million{0.0};
    double maximum_warehouse_reconciliation_error_million{0.0};
    double maximum_warehouse_charge_reconciliation_error_million{0.0};
    double maximum_eligible_basis_reconciliation_error_million{0.0};
    double maximum_funded_protection_reconciliation_error_million{0.0};
};

[[nodiscard]] std::string_view to_string(
    FundingSettlementStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    SupplementalFundingPurpose purpose) noexcept;
[[nodiscard]] std::string_view to_string(
    EligibleBasisMovementKind kind) noexcept;
[[nodiscard]] std::string_view to_string(
    FundingBridgeFailureKind kind) noexcept;
[[nodiscard]] std::string_view to_string(
    FundingBridgeFailurePhase phase) noexcept;

void validate_funding_bridge_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack, const FundingBridgeConfig& bridge);

// Evaluates a supplied, non-optimizing funding policy against the frozen
// project paths. It performs no scenario generation, probability calibration,
// fair valuation, legal analysis, or dynamic tranche waterfall.
[[nodiscard]] FundingBridgeSummary evaluate_funding_bridge(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& stack, const FundingBridgeConfig& bridge);

} // namespace naturalehia::cellular_finance
