# Multi-Project Risk Engine: Extended Integration Target

Status: post-kernel integration roadmap, 2026-08-29. The executable v0.1
kernel is documented in
[`PARTICIPATION_POOL_ENGINE_V0_1.md`](PARTICIPATION_POOL_ENGINE_V0_1.md). The
kernel, strict parser, human-readable CLI, and first staged-capital adapter are
now implemented under the authoritative `portfolio.*` API and file names.
Calibrated inputs, remaining stage adapters, and richer payoff/source accounting
below remain extended integration targets rather than claims about the current
public API.

Any extension must preserve five invariants already enforced by the kernel:

1. resolved principal loss is separate from principal still outstanding at
   the analysis horizon;
2. cash budgets have scenario-level source identifiers and are shared across
   every project that references them, preventing a finite guarantee from
   being counted more than once;
3. gross funding need assumes draws and pool costs settle before same-month
   receipts;
4. quantiles and expected shortfall use the actual stably summed weight mass;
   and
5. published cash totals use deterministic compensated accumulation and tight,
   scale-aware reconciliation.

The illustrative structs below predate these audit corrections. They are
schematic integration notes wherever they omit the five fields above; the
implemented [`portfolio.hpp`](../include/naturalehia/cellular_finance/portfolio.hpp)
is authoritative for current semantics.

## Scope and current-code boundary

Build one untranched participation-pool engine over explicit finite **joint
scenarios**. Each joint scenario contains one complete instrument cash-flow
path for every project. The engine preserves the source and type of every
payoff, aggregates without changing project paths, and measures exposure,
loss, payoff, liquidity, dependence, and tail diversification.

Version 0.1 accepts synthetic inputs only. It does not form a Cartesian product
of marginal cases, infer independence, accept a correlation matrix, calibrate
probabilities, optimize weights, or create tranches.

This fits the existing code as follows:

- `StagedCapitalPathResult` is adaptable because it exposes a timestamped
  provider cash ledger, draws, terminal receipts, and principal loss.
- `ComparisonSummary` from the annual engine is not adaptable because
  `ExogenousPath` and `PathEvaluation` are private to `model.cpp`. Pooling annual
  summaries would discard dependence. A later change must expose annual
  pathwise cash flows and stable path IDs first.
- Parsing, audit text, synthetic-only enforcement, near-one weight
  normalization, numerical tolerance, and discrete expected-shortfall behavior
  should match `staged_capital.cpp` and `staged_capital_config.cpp`.

## Original integration sketch and current names

The following was the pre-implementation naming sketch. It is retained to make
the design history auditable, but the current files are `portfolio.hpp`,
`portfolio_config.hpp`, `portfolio.cpp`, `portfolio_config.cpp`,
`apps/portfolio_cli/main.cpp`, and their corresponding tests. New work should
extend those authoritative surfaces rather than create a parallel
`multi_project` engine.

Add:

```text
include/naturalehia/cellular_finance/multi_project.hpp
include/naturalehia/cellular_finance/multi_project_config.hpp
src/multi_project.cpp
src/multi_project_config.cpp
apps/multi_project_cli/main.cpp
tests/multi_project_tests.cpp
tests/check_multi_project_cli.cmake
scenarios/multi-project-pool-synthetic.cfg
```

In `CMakeLists.txt`, add both sources to the library, add
`naturalehia_multi_project_cli` with output name
`naturalehia-multi-project-risk`, and register the unit and CLI tests. Do not
refactor the staged-capital public API in this change.

## Exact public API

`multi_project.hpp`:

```cpp
inline constexpr std::string_view kMultiProjectRiskModelVersion{"0.1.0"};

enum class DevelopmentStage : unsigned char {
    Research, Pilot, Demonstration, FirstIndustrial, RepeatProduction,
};

enum class TerminalProjectState : unsigned char {
    Successful, Failed, Exited,
};

struct ProjectDefinition {
    std::string id{};
    DevelopmentStage development_stage{DevelopmentStage::Research};
    double pool_participation_fraction{1.0};
    double instrument_commitment_million{0.0};
    std::vector<std::string> common_factor_ids{};
};

struct ProjectCashPeriod {
    std::size_t month{0U};
    double investor_funding_million{0.0};

    // Named cash sources. Investor funding is deliberately excluded.
    double distributable_underlying_cash_million{0.0};
    double explicit_outside_support_cash_million{0.0};
    double sponsor_or_cofinancier_cash_million{0.0};
    double capital_refund_cash_million{0.0};
    double net_recovery_cash_million{0.0};

    // Actual cash receipts, never PIK or an accounting claim.
    double fixed_cash_income_million{0.0};
    double principal_or_capital_return_million{0.0};
    double revenue_participation_million{0.0};
    double royalty_or_licensing_million{0.0};
    double equity_linked_success_million{0.0};
    double demand_or_risk_transfer_million{0.0};
    double residual_proceeds_million{0.0};
    double noninvestor_distribution_or_use_million{0.0};
};

struct ProjectJointPathInput {
    std::string project_id{};
    TerminalProjectState terminal_state{TerminalProjectState::Failed};
    std::size_t terminal_month{0U};
    std::vector<ProjectCashPeriod> cash_periods{};
};

struct PoolCostPeriod {
    std::size_t month{0U};
    double cash_cost_million{0.0};
};

struct JointScenarioInput {
    std::string id{};
    double weight{0.0};
    std::vector<ProjectJointPathInput> project_paths{};
    std::vector<PoolCostPeriod> pool_costs{};
};

struct MultiProjectRiskConfig {
    std::string model_version{kMultiProjectRiskModelVersion};
    std::string scenario_label{"unnamed synthetic multi-project analysis"};
    std::string source_note{
        "Unvalidated synthetic assumptions for pool-mechanics testing"};
    std::string currency_label{"DEMO"};
    std::string monetary_basis{"unspecified-synthetic"};
    bool synthetic_inputs{true};
    std::size_t horizon_months{120U};
    double investor_hurdle_rate{0.0};
    std::vector<ProjectDefinition> projects{};
    std::vector<JointScenarioInput> joint_scenarios{};
};

struct PayoffTotals {
    double fixed_cash_income_million{0.0};
    double principal_or_capital_return_million{0.0};
    double revenue_participation_million{0.0};
    double royalty_or_licensing_million{0.0};
    double equity_linked_success_million{0.0};
    double demand_or_risk_transfer_million{0.0};
    double residual_proceeds_million{0.0};
};

struct CashSourceTotals {
    double distributable_underlying_cash_million{0.0};
    double explicit_outside_support_cash_million{0.0};
    double sponsor_or_cofinancier_cash_million{0.0};
    double capital_refund_cash_million{0.0};
    double net_recovery_cash_million{0.0};
};

struct WeightedRiskDistribution {
    double mean{0.0};
    double standard_deviation{0.0};
    double minimum{0.0};
    double p05{0.0};
    double p50{0.0};
    double p95{0.0};
    double p99{0.0};
    double maximum{0.0};
    double expected_shortfall_95{0.0};
    double expected_shortfall_99{0.0};
};

struct ProjectScenarioResult {
    std::string project_id{};
    TerminalProjectState terminal_state{TerminalProjectState::Failed};
    std::size_t terminal_month{0U};
    double held_fraction{0.0};
    double held_funding_million{0.0};
    PayoffTotals held_payoffs{};
    CashSourceTotals held_sources{};
    double held_capital_loss_million{0.0};
    double held_npv_million{0.0};
    double peak_held_net_cash_outlay_million{0.0};
    double maximum_source_cash_imbalance_million{0.0};
    double maximum_capital_rollforward_imbalance_million{0.0};
};

struct PoolCashPeriodResult {
    std::size_t month{0U};
    double project_funding_million{0.0};
    double investor_receipts_million{0.0};
    double pool_cost_million{0.0};
    double net_investor_cash_million{0.0};
};

struct JointScenarioResult {
    std::string id{};
    double declared_weight{0.0};
    double normalized_weight{0.0};
    std::vector<ProjectScenarioResult> projects{};
    std::vector<PoolCashPeriodResult> pool_cash_periods{};
    double pool_funding_million{0.0};
    PayoffTotals pool_payoffs{};
    CashSourceTotals held_sources{};
    double pool_cost_million{0.0};
    double pool_capital_loss_million{0.0};
    double pool_npv_million{0.0};
    std::optional<double> cash_multiple{};
    double peak_pool_net_cash_outlay_million{0.0};
    double maximum_single_month_draw_million{0.0};
};

struct ProjectMarginalSummary {
    std::string project_id{};
    double held_commitment_million{0.0};
    double expected_funding_million{0.0};
    PayoffTotals expected_payoffs{};
    double expected_capital_loss_million{0.0};
    double probability_capital_loss{0.0};
    std::optional<double> conditional_capital_loss_million{};
    double expected_npv_million{0.0};
    double probability_negative_npv{0.0};
    WeightedRiskDistribution capital_loss_million{};
    WeightedRiskDistribution npv_million{};
};

struct PairwiseDependenceSummary {
    std::string first_project_id{};
    std::string second_project_id{};
    std::optional<double> weighted_loss_correlation{};
    double simultaneous_loss_probability{0.0};
};

struct FactorConcentrationSummary {
    std::string factor_id{};
    double held_commitment_million{0.0};
    double expected_capital_loss_million{0.0};
};

struct MultiProjectRiskSummary {
    double configured_joint_weight_sum{0.0};
    std::vector<JointScenarioResult> joint_scenarios{};
    std::vector<ProjectMarginalSummary> project_marginals{};
    std::vector<PairwiseDependenceSummary> pairwise_dependence{};
    std::vector<FactorConcentrationSummary> factor_concentrations{};
    double held_commitment_million{0.0};
    double expected_funding_million{0.0};
    PayoffTotals expected_payoffs{};
    CashSourceTotals expected_held_sources{};
    double expected_pool_cost_million{0.0};
    double expected_capital_loss_million{0.0};
    double probability_any_capital_loss{0.0};
    std::optional<double> conditional_capital_loss_million{};
    double expected_npv_million{0.0};
    double probability_negative_npv{0.0};
    double cash_multiple_included_weight{0.0};
    std::optional<WeightedRiskDistribution> cash_multiple{};
    WeightedRiskDistribution pool_capital_loss_million{};
    WeightedRiskDistribution pool_npv_million{};
    WeightedRiskDistribution peak_pool_net_cash_outlay_million{};
    WeightedRiskDistribution maximum_single_month_draw_million{};
    double standalone_es95_sum_million{0.0};
    double pool_es95_million{0.0};
    double diversification_benefit_95_million{0.0};
    std::optional<double> diversification_ratio_95{};
    double standalone_es99_sum_million{0.0};
    double pool_es99_million{0.0};
    double diversification_benefit_99_million{0.0};
    std::optional<double> diversification_ratio_99{};
    double maximum_source_cash_imbalance_million{0.0};
    double maximum_capital_rollforward_imbalance_million{0.0};
    double maximum_pool_cash_aggregation_imbalance_million{0.0};
    double maximum_pool_loss_aggregation_imbalance_million{0.0};
    double expected_loss_reconciliation_imbalance_million{0.0};
};

[[nodiscard]] std::string_view to_string(DevelopmentStage) noexcept;
[[nodiscard]] std::string_view to_string(TerminalProjectState) noexcept;
void validate_multi_project_risk_config(const MultiProjectRiskConfig&);
[[nodiscard]] MultiProjectRiskSummary evaluate_multi_project_risk(
    const MultiProjectRiskConfig&);
```

`multi_project_config.hpp`:

```cpp
[[nodiscard]] MultiProjectRiskConfig load_multi_project_risk_config(
    const std::filesystem::path&);
void print_normalized_multi_project_risk_config(
    std::ostream&, const MultiProjectRiskConfig&);
```

## Mechanics and invariants

The input is scenario-first: every joint scenario contains every configured
project exactly once. `pool_participation_fraction` scales that project's
funding, receipts, commitment, and loss into the pool; it is not a probability.

For each unscaled project path and month:

```text
sources = underlying cash + outside support + sponsor/cofinancier cash
        + capital refund + net recovery

investor receipts = sum(the seven payoff legs)

closing available source cash
  = opening available source cash + sources
  - investor receipts - noninvestor distribution/use

closing unreturned capital
  = opening unreturned capital + investor funding
  - principal or capital return.
```

Available source cash and unreturned capital may never be negative. Source cash
must be zero at the terminal month. Terminal unreturned capital is project
capital loss. PIK and contractual claim writeoff are not cash loss. A capital
refund must be classified as principal/capital return, and cumulative refunds
cannot exceed cumulative funding; investor funding cannot pay investor yield.

For held fraction `a_i` and joint case `omega`:

```text
CF_pool,t(omega)
  = sum_i a_i * (receipts_i,t - funding_i,t) - pool_cost_t

Loss_pool(omega) = sum_i a_i * capital_loss_i(omega).
```

Expected loss must equal the sum of project marginal expected held losses.
Pooling does not lower expected loss. Tail diversification is:

```text
benefit_alpha = sum_i ES_alpha(a_i Loss_i) - ES_alpha(Loss_pool)
ratio_alpha   = 1 - ES_alpha(Loss_pool) / sum_i ES_alpha(a_i Loss_i).
```

The ratio is absent for a zero denominator. Report 95% and 99%. Do not floor
benefit; a materially negative value is an internal error because all losses
use the same normalized joint cases. Pairwise weighted loss correlation is an
output and is absent when either variance is zero.

Validation rejects: non-v0.1 or non-synthetic input; unsafe or duplicate IDs;
empty sets; participation outside `(0,1]`; non-positive commitment or case
weight; weights farther than `1e-12` from one; omitted/duplicate/extra projects
in a joint case; unsorted or duplicate cash months; events after terminal or
horizon; non-finite/negative amounts; cumulative funding above commitment;
capital return above outstanding capital; refunds above capital return or
funding; negative/terminal source balance; failed aggregation identities; and
resource excess. The implemented kernel currently permits at most 128 projects,
10,000 joint scenarios, a 2,400-month horizon, 500,000 project-scenario pairs,
2,000,000 cash records, 128-byte IDs, and `1e6` million per money field, with
additional aggregate month-work guards, scale-aware money tolerance,
long-double weight accumulation, and strict positive-event semantics.

## Staged-capital adapter

The implemented adapter converts the actual configured staged cases into a
one-project `PortfolioConfig`. It maps provider draws, sponsor-paid fees, and
terminal provider repayments; preserves dated provider net cash, physical
weights, principal loss, and actual-path NPV; and leaves common-factor tags empty
rather than inventing dependence. Completion repayment may be divided among
several explicit source categories whose amounts must reconcile exactly.

The exact API, source-allocation rules, PIK convention, reconciliation gates,
and residual boundaries are in
[`STAGED_CAPITAL_PORTFOLIO_ADAPTER_V0_1.md`](STAGED_CAPITAL_PORTFOLIO_ADAPTER_V0_1.md).
The separate all-provider-performs fee-replay sensitivity is never treated as a
pool asset.

## CLI

The implemented command is:

```text
naturalehia-participation-pool <portfolio.cfg> [--print-normalized]
```

Use the existing indexed strict-key style:
`project.count`, `project.N.*`, `joint_case.count`,
`joint_case.N.project.M.*`, and sparse
`joint_case.N.project.M.period.K.*`. Every source, payoff, and noninvestor-use
field is required, including explicit zeroes. `--print-normalized` emits the
complete reloadable config with classic locale and `max_digits10`.

Output sections: synthetic/non-valuation warning; analysis basis and raw weight
sum; project/stage/participation/commitment/factor table; expected funding,
payoff legs, source mix, costs, and NPV; capital-loss and liquidity tails;
ES95/ES99 diversification; pairwise dependence; all joint cases; and maximum
reconciliation imbalances.

A companion command is now implemented for probability uncertainty:

```text
naturalehia-probability-envelope <portfolio.cfg> <probability-envelope.cfg>
    [--print-normalized]
```

It preserves every configured cash path and computes exact min/central/max
ranges under named lower and upper physical-probability bounds. Every endpoint
includes its feasible probability witness. See
[`PROBABILITY_ENVELOPE_ENGINE_V0_1.md`](PROBABILITY_ENVELOPE_ENGINE_V0_1.md).

## Minimal high-value test matrix

| Test | Proof |
|---|---|
| Two-project hand table | Funding, seven payoff legs, held scaling, cost, NPV, and loss match arithmetic. |
| Same marginals, different joints | Expected and marginal losses match while pool p95/ES differ. |
| Comonotonic vs mutually exclusive loss | Diversification is exactly zero in the first and positive by hand calculation in the second. |
| Simultaneous draws | Same total funding, different draw alignment, different peak liquidity. |
| Source/capital conservation | Duplicate recovery, self-funded yield, excess return/refund, and terminal source cash are rejected. |
| Pool identities | Cash, loss, and expected loss reconcile exactly after participation scaling and pool costs. |
| Tail boundary | Just-below and exact 95% weights reproduce staged inverse-CDF/fractional-ES behavior. |
| Dependence diagnostics | Positive/negative cases and zero-variance `not applicable` match hand calculations. |
| Staged adapter | Completion, milestone failure, later provider failure, and recovery below PIK claim reproduce provider ledger and principal loss. |
| Strict parser/guardrails | Round-trip under non-classic locale; unknown, duplicate, missing, NaN, infinity, unsafe ID, weight, and resource failures. |
| CLI | Success output contains warnings, joint cases, tails, diversification, and zero controls; bad config exits 1, bad options exit 2. |

The implemented v0.1 kernel, parser, CLI, staged adapter, and probability
envelope meet this mechanics gate: the untranched pool reconciles pathwise, its
tail-risk change is traceable to declared joint outcomes, and uncertainty in
their weights is reported without rewriting cash. This does not complete
empirical calibration, the remaining stage-specific adapters, or investable
pricing.
