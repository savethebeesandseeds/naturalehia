// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kModelVersion{"0.1.0"};

enum class PriceSupportKind : unsigned char {
    None,
    OneWayFloor,
    TwoWayDifference,
};

struct FacilityAssumptions {
    std::size_t analysis_years{12U};
    double planned_construction_years{2.0};
    double base_capex_million{250.0};
    double annual_nameplate_output_million_kg{10.0};
    // Zero-factor conditional utilization. With nonzero logit dispersion this
    // is not generally the unconditional Monte Carlo mean.
    double steady_state_utilization{0.72};
    // Ramp multiplier at commercial operation; the annual increment is
    // applied to operating age measured from that point.
    double ramp_at_commercial_operation{0.45};
    double annual_ramp_increment{0.25};
    double base_spot_price_per_kg{14.0};
    double base_variable_cost_per_kg{9.0};
    double base_fixed_opex_million{12.0};
    double project_discount_rate{0.12};
};

// Log sigma values describe multiplicative dispersion. Utilization uses a
// logit-normal transform so that every sampled value remains in (0, 1).
struct RiskAssumptions {
    double capex_log_sigma{0.20};
    double construction_duration_log_sigma{0.20};
    double utilization_logit_sigma{0.30};
    double biological_yield_log_sigma{0.15};
    double output_price_log_sigma{0.18};
    double variable_cost_log_sigma{0.15};
    double fixed_opex_log_sigma{0.10};
    // Zero-factor full-year conditional probability. With nonzero logit
    // dispersion this is not generally the unconditional event frequency.
    double annual_contamination_probability{0.08};
    double contamination_logit_sigma{0.75};
    double contamination_output_loss_fraction{0.35};
    double persistent_factor_loading{0.35};
};

struct DebtTerms {
    double debt_fraction_of_base_capex{0.35};
    double annual_interest_rate{0.09};
    std::size_t tenor_years{7U};
    double recovery_fraction_after_default{0.35};
    // Version 0.1 treats any balance left at the analysis horizon as an
    // explicit sponsor-paid balloon. It reports that amount separately.
    bool assume_terminal_balance_paid_by_sponsor{true};
};

struct InstrumentTerms {
    // Physical offtake is valued as the difference between the contracted and
    // simulated spot price on the contracted share of qualified output.
    double offtake_fraction{0.0};
    double offtake_price_per_kg{0.0};

    PriceSupportKind price_support_kind{PriceSupportKind::None};
    double price_support_fraction{0.0};
    double price_support_strike_per_kg{0.0};
    // For a two-way contract, both provider payments and project repayments
    // consume these absolute-settlement caps. They are not one-way payout caps.
    double price_support_annual_cap_million{0.0};
    double price_support_lifetime_cap_million{0.0};

    double completion_delay_trigger_years{0.0};
    double completion_payout_per_delay_year_million{0.0};
    // This is a modeled payout contract, not a claim that a legally effective
    // guarantee or a creditworthy guarantor exists.
    double completion_delay_cover_cap_million{0.0};

    // Paid by the project at time zero. This is a transfer to the instrument
    // provider and therefore reduces net receipts to the project.
    double upfront_fee_million{0.0};
};

struct SimulationConfig {
    std::string model_version{kModelVersion};
    std::string scenario_label{"unnamed synthetic illustration"};
    std::string source_note{
        "Unvalidated synthetic assumptions for software testing only"};
    std::string currency_label{"DEMO"};
    std::string monetary_basis{"unspecified-synthetic"};
    bool synthetic_inputs{true};
    std::size_t trials{10'000U};
    std::uint64_t seed{20'260'827ULL};
    FacilityAssumptions facility{};
    RiskAssumptions risk{};
    DebtTerms debt{};
    InstrumentTerms instrument{};
};

struct DistributionSummary {
    double mean{0.0};
    double standard_deviation{0.0};
    double p05{0.0};
    double p50{0.0};
    double p95{0.0};
    // Loss is max(0, -value), so both measures are non-negative and relative
    // to a zero-NPV reference rather than to an arbitrary expected value.
    double shortfall_value_at_risk_95{0.0};
    double shortfall_expected_shortfall_95{0.0};
};

struct SimulationSummary {
    std::size_t trials{0U};
    DistributionSummary project_npv_before_instruments_million{};
    DistributionSummary project_npv_after_instruments_million{};
    DistributionSummary sponsor_npv_after_financing_million{};
    DistributionSummary actual_capex_million{};
    DistributionSummary commercial_operation_timing_years_after_close{};
    DistributionSummary total_qualified_output_million_kg{};
    std::optional<DistributionSummary> minimum_dscr{};
    double probability_project_npv_negative{0.0};
    double probability_sponsor_npv_negative{0.0};
    // Default and loss are measured only within the declared analysis horizon.
    double debt_default_probability{0.0};
    // Conditional on a modeled payment default; absent when no sampled path
    // defaults within the horizon.
    std::optional<DistributionSummary>
        debt_default_timing_years_after_close{};
    // Unconditional expected loss averages zero-loss non-default paths. The
    // conditional mean is severity among defaulted paths, measured at each
    // path's default date; it is absent when no path defaults.
    double unconditional_expected_debt_loss_at_default_date_million{0.0};
    double unconditional_expected_debt_loss_pv_million{0.0};
    std::optional<double>
        mean_debt_loss_given_default_at_default_date_million{};
    double fraction_paths_with_debt_service{0.0};
    double probability_terminal_debt_outstanding{0.0};
    double expected_terminal_debt_balance_million{0.0};
    double expected_instrument_net_receipts_pv_million{0.0};
    // Transfers in periods after a prior-period debt default. The default-year
    // transfer itself is included in the payment-capacity test, not here.
    double expected_instrument_net_receipts_after_default_pv_million{0.0};
    double expected_offtake_repricing_pv_million{0.0};
    double expected_price_support_net_settlement_pv_million{0.0};
    double expected_completion_delay_cover_payout_pv_million{0.0};
    double expected_upfront_fee_million{0.0};
    // Gross positive price-support and completion-cover payments. Physical
    // offtake repricing is deliberately excluded because it is a sale term.
    double expected_positive_support_payout_pv_million{0.0};
};

struct ComparisonSummary {
    SimulationSummary without_instrument{};
    SimulationSummary with_instrument{};
    DistributionSummary paired_project_npv_change_million{};
    DistributionSummary paired_sponsor_npv_change_million{};
    DistributionSummary paired_instrument_transfer_pv_million{};
    double paired_default_probability_change{0.0};
    // Conditional on paths that default in both cases. Positive values mean
    // the structured-case default occurs later.
    std::optional<DistributionSummary> paired_default_timing_change_years{};
    std::size_t project_npv_negative_to_nonnegative_count{0U};
    std::size_t project_npv_nonnegative_to_negative_count{0U};
    std::size_t sponsor_npv_negative_to_nonnegative_count{0U};
    std::size_t sponsor_npv_nonnegative_to_negative_count{0U};
    std::size_t debt_default_avoided_within_horizon_count{0U};
    std::size_t debt_default_introduced_within_horizon_count{0U};
    std::size_t debt_default_delayed_count{0U};
    std::size_t debt_default_accelerated_count{0U};
    std::size_t debt_default_same_timing_count{0U};
};

[[nodiscard]] std::string_view to_string(PriceSupportKind kind) noexcept;

// Version 0.1 deliberately rejects scenarios presented as validated. The
// engine is ready for transparent synthetic sensitivity work; a later data
// governance protocol must be completed before empirical claims are enabled.
void validate_config(const SimulationConfig& config);

// Generates each exogenous facility path once and evaluates both the empty and
// configured instrument on that same path. This paired design prevents an
// instrument from appearing helpful because it received luckier random draws.
[[nodiscard]] ComparisonSummary run_paired_simulation(
    const SimulationConfig& config);

} // namespace naturalehia::cellular_finance
