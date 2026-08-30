// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace naturalehia::cellular_finance {

inline constexpr std::string_view kProviderCreditStressModelVersion{"0.1.0"};

// One provider-performance atom conditional on an already named portfolio
// scenario. The conditional weights are physical-measure stress inputs, not
// risk-neutral probabilities or independently optimized ambiguity variables.
struct ProviderCreditOutcomeConfig {
    std::string outcome_id{};
    double conditional_weight{0.0};
    bool provider_performs{true};
    double collateral_realization_fraction{0.0};
    double unsecured_recovery_fraction{0.0};
    std::size_t unsecured_recovery_delay_months{0U};
};

struct ProviderCreditScenarioConfig {
    std::string scenario_id{};
    std::vector<ProviderCreditOutcomeConfig> outcomes{};
};

// A deliberately narrow physical-measure counterparty-credit overlay. The
// assertions are analyst declarations required to prevent provider failure
// from being mistaken for cheaper protection, a change to the gross claim, or
// a market CVA/fair-value estimate. This type is defined with the parser so
// the core stress engine can include it without duplicating input structures.
struct ProviderCreditStressConfig {
    std::string model_version{kProviderCreditStressModelVersion};
    std::string scenario_label{
        "unnamed synthetic provider counterparty-credit stress"};
    std::string source_note{
        "Unvalidated synthetic provider-credit assumptions for mechanics testing"};
    // Must identify the same counterparty as the pooled-loss-protection leg.
    // Exact cross-input matching is enforced by the core stress engine.
    std::string provider_id{"unnamed-provider"};
    bool synthetic_inputs{true};

    bool gross_contractual_claim_remains_unchanged{false};
    bool provider_price_remains_full_performance_and_unchanged{false};
    bool conditional_provider_state_weights_are_fixed_physical{false};
    // Both fields are mandatory declarations. If the pricing-model collateral
    // is not legally pledged for this stress, both remain false and every
    // realization fraction must be zero: priced carry alone creates no
    // investor collateral benefit. A declared pledge requires retained yield.
    bool price_ladder_collateral_is_pledged_to_investor{false};
    bool collateral_yield_remains_in_pledged_account{false};
    bool collateral_applies_before_unsecured_recovery{false};
    bool provider_default_occurs_at_claim_settlement{false};
    bool provider_default_is_physical_stress_not_pricing_measure{false};

    bool legal_enforceability_is_validated{false};
    bool market_cva_or_fair_value_is_claimed{false};

    std::vector<ProviderCreditScenarioConfig> scenarios{};
};

// Intrinsic validation covers assertions, bounded values, unique safe IDs,
// and per-scenario conditional probability sums. Matching the scenario IDs
// exactly to a PortfolioConfig is necessarily a core-engine validation step.
void validate_provider_credit_stress_config(
    const ProviderCreditStressConfig& config);

// Parses the closed v0.1 key=value schema from an existing stream. Blank
// lines and full-line comments beginning with '#' are ignored. Unknown,
// missing, and duplicate keys are errors. Accepted conditional weights are
// normalized within each portfolio scenario before this function returns.
[[nodiscard]] ProviderCreditStressConfig parse_provider_credit_stress_config(
    std::istream& input);

[[nodiscard]] ProviderCreditStressConfig load_provider_credit_stress_config(
    const std::filesystem::path& path);

// Emits every v0.1 field in deterministic, reloadable order. Conditional
// weights are emitted normalized without mutating the caller's object.
void print_normalized_provider_credit_stress_config(
    std::ostream& output, const ProviderCreditStressConfig& config);

} // namespace naturalehia::cellular_finance
