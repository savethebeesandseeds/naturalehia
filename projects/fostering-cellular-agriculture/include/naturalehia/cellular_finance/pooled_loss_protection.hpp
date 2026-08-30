// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>
#include <naturalehia/cellular_finance/success_participation.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kPooledLossProtectionModelVersion{"0.1.0"};

// Contract terms for one external, cash-settled protection leg over the
// untranched portfolio's terminal realized principal loss. Version 0.1 is a
// synthetic physical-measure analysis; the assertions make its deliberately
// narrow settlement convention visible and do not prove that support exists,
// is enforceable, or is creditworthy.
struct PooledLossProtectionConfig {
    std::string model_version{kPooledLossProtectionModelVersion};
    std::string scenario_label{
        "unnamed synthetic pooled-loss-protection analysis"};
    std::string source_note{
        "Unvalidated synthetic protection terms for mechanics testing"};
    std::string provider_id{"unnamed-provider"};
    bool synthetic_inputs{true};

    // These four assertions must be explicit and true in v0.1. The core
    // validator separately checks the portfolio-dependent conditions.
    bool portfolio_principal_loss_is_contractual_reference_amount{false};
    bool gross_project_loss_remains_visible{false};
    bool support_is_assumed_fully_funded_and_performing_in_all_scenarios{false};
    bool premium_is_upfront_at_month_zero{false};

    // Fixes the underlying participation contract at one already-supported
    // fraction. The protection analysis does not silently re-optimize it.
    double underlying_success_participation_fraction{0.0};
    std::size_t settlement_month{0U};
    double support_cap_million{0.0};
    // A provider-side physical-measure hurdle sensitivity. It is not a market
    // discount curve, fair value input, or risk-neutral pricing parameter.
    double provider_annual_physical_hurdle_rate{0.0};
};

struct PooledLossProtectionProjectResult {
    std::string project_id{};
    // The underlying engine's terminal realized principal loss remains gross
    // and unchanged. Every following amount is an overlay cash or memo field.
    double gross_principal_loss_million{0.0};
    double protection_claim_million{0.0};
    double investor_external_support_cash_million{0.0};
    double provider_external_support_cash_million{0.0};
    double residual_unprotected_loss_million{0.0};
};

struct PooledLossProtectionScenarioResult {
    std::string scenario_id{};
    double underlying_npv_million{0.0};
    double gross_principal_loss_million{0.0};
    double protection_claim_million{0.0};
    double investor_external_support_cash_million{0.0};
    double provider_external_support_cash_million{0.0};
    double claim_present_value_to_investor_million{0.0};
    double claim_present_value_to_provider_million{0.0};
    double investor_npv_before_premium_million{0.0};
    double residual_unprotected_loss_million{0.0};
    double legal_support_cap_million{0.0};
    // The percentage leg cannot claim more than coverage times aggregate
    // contractual reference principal even when the legal cap is larger.
    double remaining_contractual_claim_headroom_million{0.0};
    double uncommitted_legal_cap_capacity_million{0.0};
    std::vector<PooledLossProtectionProjectResult> projects{};
};

struct PooledLossProtectionProviderRisk {
    double legal_support_cap_million{0.0};
    // Coverage times the whole aggregate contractual reference principal,
    // including a full-loss path not present in the modeled scenario table.
    double contractual_maximum_exposure_million{0.0};
    double modeled_maximum_claim_million{0.0};
    double uncommitted_legal_cap_capacity_million{0.0};
    std::optional<double> maximum_cap_utilization{};
    PortfolioDistributionSummary central_claim_nominal_million{};
    AmbiguityMetricRange expected_claim_nominal_million{};
    AmbiguityMetricRange expected_claim_present_value_million{};
    AmbiguityMetricRange claim_probability{};
    AmbiguityMetricRange claim_expected_shortfall_95_nominal_million{};
    AmbiguityMetricRange claim_expected_shortfall_99_nominal_million{};
    AmbiguityMetricRange claim_expected_shortfall_95_present_value_million{};
    AmbiguityMetricRange claim_expected_shortfall_99_present_value_million{};
};

struct PooledLossProtectionRobustPoint {
    double coverage_fraction{0.0};
    double investor_target_worst_expected_npv_million{0.0};
    AmbiguityMetricRange investor_expected_npv_before_premium_million{};
    // A negative headroom means the investor target is not met even before a
    // non-negative premium. The optional ceiling is then deliberately absent.
    double investor_signed_premium_headroom_million{0.0};
    std::optional<double> investor_maximum_nonnegative_premium_million{};
    // Physical-measure claim-only break-even under the declared provider
    // hurdle. It excludes every cost identified by the summary boundary.
    double provider_minimum_robust_break_even_premium_million{0.0};
    double premium_feasibility_gap_million{0.0};
    bool robust_nonnegative_premium_interval_exists{false};
    PooledLossProtectionProviderRisk provider_risk{};
    double maximum_endpoint_probability_error{0.0};
};

enum class PooledLossProtectionSolveStatus : unsigned char {
    AlreadyMeetsInvestorTargetAtZero,
    CertifiedInteriorBracket,
    CertifiedSupportCapBoundaryBracket,
    FullCoverageRequired,
    NoGrossReferenceLoss,
    NoSupportCapacity,
    UnattainableAtMaximumSupportedCoverage,
};

struct PooledLossProtectionSummary {
    PooledLossProtectionSolveStatus status{
        PooledLossProtectionSolveStatus::NoGrossReferenceLoss};
    double underlying_success_participation_fraction{0.0};
    double investor_target_worst_expected_npv_million{0.0};
    double legal_support_cap_million{0.0};
    double aggregate_reference_principal_million{0.0};
    // Compatibility alias for v0.1 consumers. Equal to
    // aggregate_reference_principal_million; in explicit-ledger portfolios it
    // is not the investor cash commitment.
    double aggregate_covered_commitment_million{0.0};
    double maximum_supported_coverage_fraction{0.0};
    double modeled_full_coverage_maximum_claim_million{0.0};

    // As in the participation solver, only literal zero or one is published
    // as exact. Every non-boundary answer is a certified failing/passing
    // floating-point bracket whose upper endpoint is the reported term. Here
    // "passing" means only that the investor target is met before premium; it
    // does not mean that a bilateral premium interval exists or that the term
    // is investable.
    std::optional<double> exact_minimum_coverage_fraction{};
    std::optional<double> failing_coverage_fraction_lower_bound{};
    std::optional<double>
        investor_target_passing_coverage_fraction_upper_bound{};
    double reported_coverage_fraction{0.0};
    double investor_target_gap_at_maximum_supported_coverage_million{0.0};

    PooledLossProtectionRobustPoint zero{};
    PooledLossProtectionRobustPoint reported{};
    PooledLossProtectionRobustPoint maximum_supported{};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    std::vector<PooledLossProtectionScenarioResult> scenarios{};

    // These flags and the text are output facts about this engine boundary,
    // not optional warnings. Version 0.1 assumes performance rather than
    // modeling provider credit or proving an enforceable support source.
    bool provider_default_risk_is_modeled{false};
    bool provider_funding_and_collateral_costs_are_modeled{false};
    bool legal_enforceability_is_validated{false};
    std::string provider_model_limitation{};

    double maximum_underlying_loss_change_million{0.0};
    double maximum_project_claim_reconciliation_error_million{0.0};
    double maximum_two_party_settlement_cash_reconciliation_error_million{
        0.0};
    double maximum_support_cap_violation_million{0.0};
    double maximum_combined_npv_reconstruction_error_million{0.0};
    double maximum_witness_reconciliation_error_million{0.0};
    double maximum_endpoint_probability_error{0.0};
};

struct PooledLossProtectionPremiumEvaluation {
    double upfront_premium_million{0.0};
    double investor_month_zero_premium_cash_million{0.0};
    double provider_month_zero_premium_cash_million{0.0};
    AmbiguityMetricRange investor_expected_npv_after_premium_million{};
    AmbiguityMetricRange provider_expected_npv_after_premium_million{};
    double investor_target_gap_after_premium_million{0.0};
    double provider_break_even_gap_after_premium_million{0.0};
    bool investor_target_is_met{false};
    bool provider_break_even_is_met{false};
    bool premium_is_within_robust_interval{false};
    // This reconciles nominal month-zero transfers only. It is not an NPV
    // conservation statement; investor and provider hurdles may differ.
    double month_zero_premium_cash_reconciliation_error_million{0.0};
};

[[nodiscard]] std::string_view to_string(
    PooledLossProtectionSolveStatus status) noexcept;

void validate_pooled_loss_protection_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection);

// Conservatively brackets the smallest proportional coverage fraction that
// meets the investor target before premium and is supported by the legal cash
// cap. That threshold is not a claim of bilateral price feasibility. The
// external claim never mutates portfolio cash sources, receipts, principal
// components, loss layers, or scenario probabilities.
[[nodiscard]] PooledLossProtectionSummary solve_pooled_loss_protection(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection);

// Projects the complete protection economics at one exact caller-selected
// coverage fraction. The fraction must lie inside the same reference-principal
// support-cap domain used by solve_pooled_loss_protection. No endpoint is
// interpolated: every investor, provider, and tail objective is freshly
// projected so binding ambiguity witnesses may change.
[[nodiscard]] PooledLossProtectionRobustPoint
evaluate_pooled_loss_protection_coverage(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation,
    const PooledLossProtectionConfig& protection,
    double coverage_fraction);

// Applies one candidate non-negative month-zero premium to a previously
// projected point. It changes neither the claim nor any underlying project
// cash and preserves the endpoint probability witnesses.
[[nodiscard]] PooledLossProtectionPremiumEvaluation
evaluate_pooled_loss_protection_upfront_premium(
    const PooledLossProtectionRobustPoint& point,
    double upfront_premium_million);

} // namespace naturalehia::cellular_finance
