// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/robust_market_priority_cap.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kRobustIssuePriceSupportModelVersion{
    "0.1.0"};
inline constexpr std::size_t kRobustIssuePriceSupportMaximumHurdleCases{256U};
inline constexpr std::size_t
    kRobustIssuePriceSupportMaximumStructuralWorkUnits{4'000'000U};

enum class RobustIssuePriceReferenceStatus {
    InternalCandidate,
    NonbindingIndication,
    BindingUnsettledSubscription,
    ExecutedUnsettledPrimary,
    SettledPrimary,
    SettledSecondary,
};

enum class RobustIssuePriceSupportCapacityStatus {
    SyntheticCandidate,
    NonbindingIndication,
    ContractuallyCommitted,
    FundedOrEscrowed,
    SettledToIssue,
};

enum class RobustIssuePriceHurdleSourceType {
    SameClaimMarketObservation,
    ComparableMarketObservation,
    ModelAdjustedComparable,
    InvestorTarget,
    PolicyTarget,
    SyntheticSensitivity,
};

enum class RobustIssuePriceHurdleReferenceRelation {
    Independent,
    ModelImpliedFromReferencePrice,
    Unresolved,
};

enum class RobustIssuePriceSupportCaseStatus {
    HurdleNotIndependentOfReferencePrice,
    NoNonnegativeInvestorPrice,
    InvestorAndIssuerRequirementsDoNotOverlap,
    FinanceablePriceWindow,
};

enum class RobustIssuePriceSupportStatus {
    PriorityCapSelectionUnavailable,
    NoFinanceableWindow,
    FinanceableWindowFound,
};

[[nodiscard]] std::string_view to_string(
    RobustIssuePriceReferenceStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustIssuePriceSupportCapacityStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustIssuePriceHurdleSourceType source) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustIssuePriceHurdleReferenceRelation relation) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustIssuePriceSupportCaseStatus status) noexcept;
[[nodiscard]] std::string_view to_string(
    RobustIssuePriceSupportStatus status) noexcept;

// One externally supplied price record. The engine evaluates this price as a
// hypothetical primary issue in every status, but only a settled-primary
// record with evidenced funding and settlement can establish observed issue
// cash. A settled-secondary price never enters the project reserve.
struct RobustIssuePriceReferenceConfig {
    std::string record_id{"unnamed-reference-price"};
    RobustIssuePriceReferenceStatus status{
        RobustIssuePriceReferenceStatus::InternalCandidate};
    std::string market_claim_id{"market-priority"};
    std::string normalized_term_result_id{"unnamed-fixed-term-result"};
    bool secondary_price_normalized_to_full_month_zero_claim{false};

    double gross_issue_price_million{0.0};
    double claim_quantity_million{0.0};
    std::string quantity_basis{"full contractual market principal"};
    std::string price_basis{"gross buyer cash at month zero"};
    std::string currency_label{"UNSPECIFIED"};
    std::string monetary_basis{"unspecified monetary basis"};

    std::string execution_date{"none"};
    std::string settlement_date{"none"};
    double issuer_cost_million{0.0};
    double buyer_direct_cost_million{0.0};
    bool side_rights_or_non_cash_consideration_present{false};
    std::string side_rights_or_non_cash_consideration_note{"none"};

    std::string source_reference{"unvalidated synthetic reference"};
    std::string evidence_record_id{"none"};
    bool buyer_cash_payment_evidenced{false};
    bool settlement_evidenced{false};
    bool subscription_reserve_deposit_evidenced{false};
    bool issuer_cost_payment_evidenced{false};
    std::string issue_use_evidence_record_id{"none"};
};

// G is maximum declared conditional non-repayable support capacity for this
// issue. Version 0.1 rejects any repayment, participation, security, or
// recovery right: such a right is a separate claim and belongs in the
// complete waterfall.
struct RobustIssuePriceSupportCapacityConfig {
    std::string support_id{"unnamed-issue-price-support"};
    RobustIssuePriceSupportCapacityStatus status{
        RobustIssuePriceSupportCapacityStatus::SyntheticCandidate};
    double maximum_support_million{0.0};
    double settled_support_million{0.0};
    bool funding_evidenced{false};
    bool settlement_evidenced{false};
    std::string as_of_date{"none"};
    std::string source_reference{"unvalidated synthetic support capacity"};
    std::string evidence_record_id{"none"};
    std::string source_note{
        "No provider authority, budget, or cash is established"};

    bool support_is_non_repayable{false};
    bool support_receives_no_repayment_participation_security_or_recovery_rights{
        false};
    bool support_is_not_project_revenue{false};
    bool support_does_not_pay_future_pool_costs_or_cover_project_losses{false};
};

struct RobustIssuePriceHurdleCaseConfig {
    std::string case_id{"unnamed-investor-hurdle"};
    double annual_effective_hurdle_rate{0.0};
    RobustIssuePriceHurdleSourceType source_type{
        RobustIssuePriceHurdleSourceType::SyntheticSensitivity};
    RobustIssuePriceHurdleReferenceRelation reference_price_relation{
        RobustIssuePriceHurdleReferenceRelation::Unresolved};
    std::string as_of_date{"none"};
    std::string source_reference{"unvalidated synthetic hurdle"};
    std::string evidence_record_id{"none"};
    std::string source_note{
        "Externally supplied decision sensitivity; not market calibration"};
};

struct RobustIssuePriceSupportConfig {
    std::string model_version{kRobustIssuePriceSupportModelVersion};
    std::string scenario_label{
        "unnamed synthetic robust issue-price support term"};
    std::string source_note{
        "Unvalidated synthetic issue price, support, and hurdle inputs"};
    bool synthetic_inputs{true};

    // Required transaction assertions. P+S=M+F, exactly M enters the reserve,
    // F is paid as issuer cost at month zero, and C remains buyer-direct.
    bool market_claim_principal_is_fully_funded_at_issue{false};
    bool issue_support_and_price_fund_only_principal_and_issuer_costs{false};
    bool buyer_direct_cost_stays_outside_subscription_reserve{false};
    bool support_changes_no_claim_right_or_project_cash{false};
    bool physical_probability_polytope_is_unchanged{false};
    bool fair_value_or_market_price_is_estimated{false};

    RobustIssuePriceReferenceConfig reference_price{};
    RobustIssuePriceSupportCapacityConfig support{};
    std::vector<RobustIssuePriceHurdleCaseConfig> hurdle_cases{};
};

struct RobustIssuePriceSupportReferenceMetrics {
    ProbabilityPolytopeMetricRange expected_investor_contributions_million{};
    ProbabilityPolytopeMetricRange expected_distributions_million{};
    ProbabilityPolytopeMetricRange investor_npv_million{};
    std::optional<ProbabilityPolytopeMetricRange>
        expected_scenario_npv_margin_fraction{};
    std::optional<ProbabilityPolytopeMetricRange>
        expected_scenario_cash_multiple{};
    std::optional<ProbabilityPolytopeMetricRange>
        expected_scenario_net_return_fraction{};
    ProbabilityPolytopeMetricRange negative_npv_probability{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        npv_shortfall_es95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        npv_shortfall_es99_million{};

    double robust_investor_npv_million{0.0};
    std::optional<double> robust_expected_scenario_npv_margin_fraction{};
    bool investor_term_adequate{false};
    bool modeled_full_funding_adequate{false};
    bool modeled_joint_term_adequate{false};

    double required_issue_support_million{0.0};
    double observed_settled_support_million{0.0};
    double support_capacity_margin_million{0.0};
    double unused_support_capacity_million{0.0};
    double support_capacity_shortfall_million{0.0};
    double modeled_required_issue_sources_million{0.0};
    double issue_uses_million{0.0};
    double modeled_amount_entering_subscription_reserve_million{0.0};
    double modeled_issuer_cost_paid_million{0.0};
    double buyer_direct_cost_outside_reserve_million{0.0};
    double issue_funding_identity_error_million{0.0};

    double observed_primary_buyer_cash_million{0.0};
    double observed_issue_support_cash_million{0.0};
    std::optional<double> observed_issue_sources_million{};
    std::optional<double> observed_issue_funding_identity_error_million{};
    std::optional<double>
        observed_amount_entering_subscription_reserve_million{};
    std::optional<double> observed_issuer_cost_paid_million{};

    bool reference_is_settled_primary{false};
    bool reference_is_settled_secondary{false};
    bool observed_primary_price_cash_completed{false};
    bool observed_support_cash_completed{false};
    bool observed_issue_sources_settled_and_reconciled{false};
    bool observed_primary_funding_completed{false};
};

struct RobustIssuePriceSupportPrincipalRiskMetrics {
    double contractual_market_notional_million{0.0};
    ProbabilityPolytopeMetricRange
        expected_principal_cash_distribution_million{};
    ProbabilityPolytopeMetricRange expected_principal_loss_fraction{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        principal_loss_es95_million{};
    ProbabilityPolytopeUpperExpectedShortfallProjection
        principal_loss_es99_million{};
    double worst_principal_loss_es95_fraction{0.0};
    double worst_principal_loss_es99_fraction{0.0};
    ProbabilityPolytopeMetricRange principal_impairment_probability{};
    std::optional<CapitalStackProbabilityPolytopeWalRange>
        principal_cash_wal_years{};
};

struct RobustIssuePriceSupportCaseAudit {
    bool physical_probability_polytope_is_unchanged{false};
    bool market_contractual_cash_is_unchanged{false};
    bool sparse_market_monthly_ledger_reconciles{false};
    bool market_principal_risk_is_unchanged{false};
    bool market_principal_wal_is_unchanged{false};
    bool junior_cash_and_own_hurdle_npv_are_unchanged{false};
    bool raw_price_ceiling_zero_npv_reconciles{false};
    bool reference_price_npv_shift_reconciles{false};
    bool issue_funding_identity_reconciles{false};

    double maximum_contractual_cash_change_million{0.0};
    double maximum_sparse_monthly_ledger_error_million{0.0};
    double maximum_principal_risk_change{0.0};
    double maximum_market_wal_change_years{0.0};
    double maximum_junior_change_million{0.0};
    double raw_price_ceiling_zero_npv_error_million{0.0};
    double maximum_reference_price_npv_shift_error_million{0.0};
    double issue_funding_identity_error_million{0.0};

    RobustMarketPriorityCapCandidateAudit numerical_audit{};
};

struct RobustIssuePriceSupportCaseResult {
    std::string case_id{};
    double annual_effective_hurdle_rate{0.0};
    RobustIssuePriceHurdleSourceType hurdle_source_type{
        RobustIssuePriceHurdleSourceType::SyntheticSensitivity};
    RobustIssuePriceHurdleReferenceRelation
        hurdle_reference_price_relation{
            RobustIssuePriceHurdleReferenceRelation::Unresolved};
    std::string hurdle_as_of_date{};
    std::string hurdle_source_reference{};
    std::string hurdle_evidence_record_id{};
    std::string hurdle_source_note{};

    ProbabilityPolytopeMetricRange market_par_npv_million{};
    double raw_robust_investor_price_ceiling_million{0.0};
    double raw_central_investor_price_boundary_million{0.0};
    double raw_maximum_investor_price_boundary_million{0.0};
    double admissible_investor_price_ceiling_million{0.0};
    double issuer_funding_floor_million{0.0};
    bool modeled_financeable_price_window_exists{false};
    std::optional<double> financeable_price_window_lower_million{};
    std::optional<double> financeable_price_window_upper_million{};
    bool modeled_overlap_exists_without_support{false};
    bool documented_support_commitment_covers_overlap{false};
    bool funded_support_capacity_covers_overlap{false};
    bool funded_support_covered_price_window_exists{false};
    // Not applicable when the raw robust price ceiling is negative: no
    // amount of principal-only support can make a non-negative buyer price
    // satisfy that hurdle while the fixed future claim cash remains intact.
    std::optional<double> minimum_support_capacity_for_overlap_million{};
    std::optional<double> support_shortfall_million{};
    RobustIssuePriceSupportCaseStatus status{
        RobustIssuePriceSupportCaseStatus::
            InvestorAndIssuerRequirementsDoNotOverlap};

    bool reference_price_numerically_eligible{false};
    std::string reference_price_numerical_block_reason{};
    std::optional<RobustIssuePriceSupportReferenceMetrics> reference_price{};
    RobustIssuePriceSupportPrincipalRiskMetrics principal_risk{};
    RobustIssuePriceSupportCaseAudit audit{};
};

struct RobustIssuePriceSupportSummary {
    double fixed_underlying_success_participation_fraction{0.0};
    double fixed_junior_first_loss_million{0.0};
    double aggregate_commitment_and_stack_detachment_million{0.0};
    double fixed_market_notional_million{0.0};
    std::optional<double>
        selected_market_priority_nonprincipal_cap_million{};
    std::optional<std::size_t> selected_priority_cap_candidate_index{};
    bool selected_priority_cap_is_balanced{false};
    RobustMarketPriorityCapStatus upstream_priority_cap_status{
        RobustMarketPriorityCapStatus::NoTestedMarketAdequateCap};
    RobustIssuePriceSupportStatus status{
        RobustIssuePriceSupportStatus::PriorityCapSelectionUnavailable};

    double reference_gross_issue_price_million{0.0};
    double issuer_cost_million{0.0};
    double buyer_direct_cost_million{0.0};
    double maximum_issue_support_million{0.0};
    double settled_issue_support_million{0.0};
    double issuer_funding_floor_million{0.0};
    RobustIssuePriceReferenceStatus reference_price_status{
        RobustIssuePriceReferenceStatus::InternalCandidate};
    RobustIssuePriceSupportCapacityStatus support_capacity_status{
        RobustIssuePriceSupportCapacityStatus::SyntheticCandidate};

    std::size_t portfolio_cash_record_count{0U};
    std::size_t portfolio_auxiliary_record_count{0U};
    std::size_t portfolio_record_count{0U};
    std::size_t upstream_priority_cap_work_units{0U};
    std::size_t hurdle_stack_work_units{0U};
    std::size_t reference_projection_work_units{0U};
    std::size_t scenario_month_audit_work_units{0U};
    std::size_t structural_work_units{0U};
    std::size_t structural_work_unit_limit{
        kRobustIssuePriceSupportMaximumStructuralWorkUnits};

    std::vector<RobustIssuePriceSupportCaseResult> hurdle_cases{};
    std::vector<std::size_t> financeable_hurdle_case_indices{};
    std::vector<std::size_t>
        funded_support_covered_hurdle_case_indices{};
    std::vector<std::size_t> no_nonnegative_price_hurdle_case_indices{};
    std::optional<std::size_t> literal_zero_hurdle_case_index{};

    bool base_stack_was_not_mutated{false};
    bool only_market_hurdle_changed_across_cases{false};
    bool all_contractual_cash_and_principal_risk_invariants_hold{false};

    bool market_hurdle_is_discovered_or_empirically_calibrated{false};
    bool fair_value_or_accounting_value_is_estimated{false};
    bool market_consistent_discount_curve_or_pricing_measure_is_used{false};
    bool bid_offer_executable_price_spread_or_rating_is_produced{false};
    bool investor_demand_suitability_or_placement_is_established{false};
    bool support_provider_authority_or_budget_is_established{false};
    bool support_counterparty_or_performance_risk_is_modeled{false};
    bool legal_enforceability_tax_or_regulation_is_established{false};
    bool capital_mobilization_or_financing_additionality_is_proven{false};
    bool animal_product_displacement_or_welfare_impact_is_proven{false};
    std::string model_limitation{};
};

// Intrinsic validation is exposed for the strict companion parser. Cross-input
// validation additionally checks the fixed claim, currency/basis, selected
// cap prerequisites, reference price, support, and combined work bound.
void validate_robust_issue_price_support_config(
    const RobustIssuePriceSupportConfig& config);

void validate_robust_issue_price_support_config(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& priority_cap,
    const RobustIssuePriceSupportConfig& issue_price);

// Runs the finite priority-cap term, selects B from its status, rebuilds that
// exact two-claim stack, and evaluates every independently sourced hurdle.
// Physical probabilities and contractual paths remain fixed. P, F, C, and G
// are deterministic month-zero issuance inputs; support receives no rights.
[[nodiscard]] RobustIssuePriceSupportSummary
evaluate_robust_issue_price_support(
    const PortfolioConfig& portfolio,
    const ProbabilityPolytopeConfig& probability_polytope,
    const SuccessParticipationConfig& participation,
    const CapitalStackConfig& base_stack,
    const RobustMarketPriorityCapConfig& priority_cap,
    const RobustIssuePriceSupportConfig& issue_price);

} // namespace naturalehia::cellular_finance
