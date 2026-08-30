// SPDX-License-Identifier: MIT

#pragma once

#include <naturalehia/cellular_finance/portfolio_ambiguity.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kSuccessParticipationModelVersion{"0.1.0"};

// Version 0.1 scales only the already-declared non-principal part of receipts
// whose cash-source taxonomy is explicitly selected here. It does not infer
// residual enterprise value, consume unused source capacity, or alter any
// principal classification. The assertion is an auditable modeling statement,
// not evidence that the selected payoff exists or is legally enforceable.
struct SuccessParticipationConfig {
    std::string model_version{kSuccessParticipationModelVersion};
    std::string scenario_label{
        "unnamed synthetic success-participation terms analysis"};
    std::string source_note{
        "Unvalidated synthetic payoff terms for mechanics testing"};
    bool synthetic_inputs{true};
    bool selected_nonprincipal_cash_is_contractually_scalable{false};
    // Must be explicit, non-empty, unique, and limited to commercial,
    // licensing-royalty, and exit-sale cash. Unselected receipts remain fixed.
    std::vector<PortfolioCashSource> scalable_source_kinds{};
    // A physical-measure NPV target under the portfolio's declared hurdle.
    // It is not a fair-value, market-price, or risk-neutral pricing target.
    double target_worst_expected_npv_million{0.0};
};

struct SuccessParticipationSourceAmount {
    std::string cash_source_id{};
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    double full_participation_nominal_million{0.0};
    double full_participation_present_value_million{0.0};
};

struct SuccessParticipationScenarioResult {
    std::string scenario_id{};
    // q=0 switches off selected participation only. Non-selected receipts,
    // including any non-principal amount, remain exactly as configured.
    double selected_participation_off_npv_million{0.0};
    double configured_q1_npv_million{0.0};
    double full_participation_nominal_million{0.0};
    double full_participation_present_value_million{0.0};
    double npv_at_reported_fraction_million{0.0};
    std::vector<SuccessParticipationSourceAmount> sources{};
};

struct SuccessParticipationSourceRange {
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    AmbiguityMetricRange full_participation_nominal_million{};
    AmbiguityMetricRange full_participation_present_value_million{};
};

struct SuccessParticipationRobustPoint {
    double participation_fraction{0.0};
    AmbiguityMetricRange expected_npv_million{};
    double maximum_endpoint_probability_error{0.0};
};

enum class SuccessParticipationSolveStatus : unsigned char {
    AlreadyMeetsTargetAtZero,
    CertifiedInteriorBracket,
    FullParticipationRequired,
    NoSelectedParticipationCash,
    UnattainableAtFullParticipation,
};

struct SuccessParticipationSummary {
    SuccessParticipationSolveStatus status{
        SuccessParticipationSolveStatus::NoSelectedParticipationCash};
    double target_worst_expected_npv_million{0.0};

    // An exact minimum is published only at the literal q=0 or q=1 boundary.
    // An interior result instead publishes a failing lower fraction and a
    // feasible upper fraction. reported_fraction is the conservative upper
    // endpoint, not a falsely precise mathematical root.
    std::optional<double> exact_minimum_fraction{};
    std::optional<double> failing_fraction_lower_bound{};
    std::optional<double> feasible_fraction_upper_bound{};
    double reported_fraction{0.0};
    double target_gap_at_full_participation_million{0.0};

    SuccessParticipationRobustPoint q0{};
    SuccessParticipationRobustPoint q1{};
    SuccessParticipationRobustPoint reported{};

    AmbiguityMetricRange full_participation_nominal_million{};
    AmbiguityMetricRange full_participation_present_value_million{};
    std::vector<SuccessParticipationSourceRange> source_ranges{};
    std::vector<ScenarioProbabilityBounds> scenario_probability_bounds{};
    std::vector<SuccessParticipationScenarioResult> scenarios{};

    // Controls report computational identities only; zero error does not
    // validate probabilities, cash-flow forecasts, or legal enforceability.
    double maximum_q1_cash_reconstruction_error_million{0.0};
    double maximum_principal_loss_reconciliation_error_million{0.0};
    double maximum_source_capacity_violation_million{0.0};
    double maximum_witness_reconciliation_error_million{0.0};
    double maximum_endpoint_probability_error{0.0};
};

[[nodiscard]] std::string_view to_string(
    SuccessParticipationSolveStatus status) noexcept;

void validate_success_participation_config(const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation);

// Returns a copy whose selected receipt amount is
// principal_component + q * (configured_amount - principal_component).
// q=1 returns an economically and bitwise unchanged receipt schedule. This
// operation never changes principal components, draws, costs, source budgets,
// probabilities, resolutions, or loss layers.
[[nodiscard]] PortfolioConfig apply_success_participation_fraction(
    const PortfolioConfig& portfolio,
    const SuccessParticipationConfig& participation,
    double participation_fraction);

// Finds the smallest supported fraction conservatively. An interior result is
// a certified failing/passing bracket because floating-point output must not be
// presented as an exact contractual threshold.
[[nodiscard]] SuccessParticipationSummary solve_success_participation(
    const PortfolioConfig& portfolio,
    const PortfolioAmbiguityConfig& ambiguity,
    const SuccessParticipationConfig& participation);

} // namespace naturalehia::cellular_finance
