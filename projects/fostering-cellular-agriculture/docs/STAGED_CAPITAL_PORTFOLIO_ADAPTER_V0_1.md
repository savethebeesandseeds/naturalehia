# Staged-Capital Portfolio Adapter v0.1

Status: implemented and mechanically verified, 2026-08-29. The staged-capital
engine, participation-pool interface, and provider-cash adapter are code.

## Purpose and boundary

The adapter translates the provider-facing cash claim produced by
`evaluate_staged_capital_cases` into the existing `PortfolioConfig` interface.
It preserves the staged cases, their declared physical weights, dated provider
cash, principal loss, and provider-hurdle NPV. It does not translate total
ProjectCo economics or sponsor economics into investor cash.

The adapter must use the actual `StagedCapitalSummary::cases`. It must not use
the provider-performance fee-replay cases, invent scenarios, resample cases, or
change their weights.

## Implemented surface

The public API exposes one explicit conversion:

```cpp
struct StagedCompletionSourceAllocation {
    std::string case_id{};
    std::string cash_source_id{};
    PortfolioCashSource source{PortfolioCashSource::Commercial};
    double amount_million{0.0};
};

[[nodiscard]] PortfolioConfig adapt_staged_capital_to_portfolio(
    const StagedCapitalConfig& staged, std::string project_id,
    ProjectStage stage,
    const std::vector<StagedCompletionSourceAllocation>&
        completion_source_allocations);
```

`project_id` and `stage` are caller inputs because the staged-capital
configuration contains neither common-interface field. A completed case with a
positive terminal provider repayment requires one or more allocations whose
strictly positive finite amounts exhaust that repayment. This permits, for
example, one case to repay partly from two buyers and partly from a licensing
payment. Under-allocation, over-allocation, a missing allocation, an allocation
for an ineligible case, or a duplicate `(case_id, cash_source_id)` is an error.
For completed cases, the allowed source kinds are `Commercial`,
`LicensingRoyalty`, `ExitSale`, `Refinancing`, or `ExplicitSupport`.
`SponsorFee` is reserved for fees and `Recovery` for non-completion paths.

`cash_source_id` is a caller-supplied safe identifier preserved verbatim. One
ID must retain one taxonomy across every case, while several IDs may share one
taxonomy. This preserves declared counterparty concentration for completion
cash. The ID and allocation are assertions to be evidenced; conversion does not
prove that a payer exists, that a contract is enforceable, or that cash will be
available. IDs generated for sponsor fees and recovery are reserved against
collision.

## Configuration mapping

| Staged-capital input or result | Common-interface target |
|---|---|
| Validated model version | `PortfolioConfig::model_version = kPortfolioModelVersion` |
| `scenario_label` | `PortfolioConfig::scenario_label` (`portfolio.label` when serialized) |
| `source_note` | `PortfolioConfig::source_note` |
| `currency_label` | `PortfolioConfig::currency_label` |
| `monetary_basis` | `PortfolioConfig::monetary_basis` |
| `synthetic_inputs` | `PortfolioConfig::synthetic_inputs`; v0.1 requires `true` |
| `terms.provider_hurdle_rate` | `annual_physical_hurdle_rate` |
| `terms.provider_commitment_million` | The single `PortfolioProject::commitment_million` |
| Adapter arguments | The single project's ID and stage |
| Staged terminal months | Portfolio horizon as defined below |

The horizon is the greatest `recovery_month`, `outcome_month`, or cash-ledger
month across the actual staged paths, with a minimum of one month. It is an
analysis boundary, not an added maturity or terminal value. The conversion
must fail rather than truncate if that horizon exceeds the portfolio engine's
resource limit.

Every `StagedCapitalPathResult` becomes one `JointScenario`:

- `id = case_id`;
- `weight = path.weight`, the original declared physical-case weight;
- exactly one `ProjectJointPath` for the configured project;
- `resolution = ProjectPathResolution::Resolved`;
- no factor tags are inferred;
- `pool_costs` is empty.

`PortfolioConfig::loss_layers` is empty. Fees and protected-reserve activity
must not be relabeled as pool costs.

## Provider-cash mapping

Only nonzero provider-account cash in `CapitalCashLedgerEntry` is adapted.
Entries with zero provider posting remain staged-model audit information.

| Ledger kind | Portfolio representation | Principal component |
|---|---|---:|
| `ProviderDraw` | `MonthlyAmount` in `capital_draws`, using the negated provider posting | Not applicable |
| `UpfrontFee` | `InvestorReceipt` from a `SponsorFee` source | 0 |
| `CommitmentFee` | `InvestorReceipt` from a `SponsorFee` source | 0 |
| `ProviderRepayment`, completed path | One or more `InvestorReceipt` records from the caller-allocated completion sources | Pro-rata share of the running cap described below |
| `ProviderRepayment`, any other outcome | `InvestorReceipt` from a `Recovery` source | Running cap described below |

The scenario uses `<project_id>.sponsor-fee` for sponsor-paid fees and
`<project_id>.recovery` for a non-completion repayment. Completed repayments use
the caller's allocation IDs and may therefore have several source records. Each
source's `cash_available` events equal its mapped investor receipts by month.
Thus the common cumulative source-budget test reconciles at equality; sponsor
residual cash or unused gross ProjectCo proceeds cannot become an investor
payoff accidentally.

The generated recovery ID is a project-level recovery bucket because the
staged input supplies only an aggregate `recovery_value_million`. It is not a
payer-level identity and cannot establish shared insurer, guarantor, collateral
buyer, or other recovery-counterparty concentration across projects.

`CompletionOrRecoveryProceeds` is an external-to-ProjectCo source posting, not
a provider receipt. `SponsorResidualDistribution`, protected-reserve funding,
workout use, reserve release, sponsor construction contributions, and eligible
construction use likewise do not map to provider cash. They remain visible in
the source staged result.

The adapter must reject any future or unknown ledger kind carrying nonzero
provider cash. Silent omission would break provider-cash conservation.

### Principal and PIK

Maintain running funded principal by month. A `ProviderDraw` increases it. For
the terminal `ProviderRepayment`, set:

```text
principal component = min(repayment cash, running funded principal)
running funded principal -= principal component
```

The full repayment is cash receipt, but only this capped component returns
principal. Any paid amount above funded principal is realized yield, including
settlement of accrued PIK. When a completion repayment has several source
allocations, the adapter divides the total principal component pro rata by cash
amount and assigns only a deterministic floating-point residual for exact
closure. That split is an accounting convention, not evidence of source-level
seniority or PIK provenance. `contractual_return_accrued_million` is a memo
entry, not cash, so PIK is never emitted as a receipt before payment. Claim
writeoff is also not principal loss. A repayment equal to funded principal can
therefore produce zero pool principal loss even when unpaid contractual PIK is
written off. On every resolved path:

```text
principal loss = provider draws - principal returned
               = provider_principal_loss_million.
```

## Why completion source is mandatory

The staged ledger deliberately combines completion and recovery provenance in
`CompletionOrRecoveryProceeds`. A completed case's `completion_value_million`
is only described as external completion cash. It does not say whether that
cash came from commercial operations, a licence or royalty, an asset or equity
sale, refinancing, or explicit outside support. The subsequent
`ProviderRepayment` is an internal ProjectCo-to-provider transfer and cannot
answer that question.

Guessing `Commercial` would overstate operating value; guessing `ExitSale` or
`Refinancing` would invent transaction structure. The caller must therefore
allocate each positive completed-case repayment among declared sources. The
adapter records those allocations but does not validate the payer,
enforceability, or economic substance. A non-completion terminal payment is
`Recovery` because the staged
input explicitly defines `recovery_value_million` as gross ProjectCo recovery
cash and the ledger already applies prior workout obligations.

## Hard reconciliation gates

Conversion succeeds only if all of the following hold within the staged
engine's scale-aware money tolerance and the portfolio engine's own input and
reconciliation tolerances:

1. `validate_staged_capital_config` and evaluation of every configured case
   succeed; cash postings balance and staged cash/memo accounts close.
2. Adapted case IDs and declared weights match the configured cases one for one;
   the raw weight sum is unchanged and remains within `1e-12` of one.
3. Each portfolio scenario's declared weight equals its matching staged-case
   weight, and the raw configured weight sums reconcile.
4. For every case and month:

   ```text
   portfolio receipts - portfolio draws
       = sum of staged provider-account cash postings.
   ```

5. Total portfolio draws equal `total_provider_draws_million`; sponsor-fee
   receipts equal provider postings for upfront and commitment fees; receipts
   mapped from `ProviderRepayment` equal
   `provider_nominal_recovery_million`.
6. Every receipt has one declared scenario source, and cumulative receipts from
   that source never exceed cumulative available cash. The adapter-generated
   budgets must reconcile exactly by month.
7. Principal returned never exceeds cash receipt or funded principal. Adapted
   principal loss equals `provider_principal_loss_million`, and adapted
   outstanding principal is zero because every staged path is resolved.
8. With no pool costs, each scenario's common-interface NPV equals
   `provider_npv_after_upfront_fee_million` at
   `terms.provider_hurdle_rate`.
9. The single-project pool's expected draws, principal loss, receipt-source
   nominal cash and PV by taxonomy, and NPV equal independent normalized
   physical-weight aggregates recomputed from the staged paths and allocations.
10. `validate_portfolio_config` and `evaluate_portfolio` succeed with zero cash
   reconciliation error beyond their reported tolerance. No gate is a warning.

## Dependence boundary

This output is a valid one-project joint distribution. It measures the staged
facility's marginal provider loss, cash return, timing, liquidity, and NPV on a
common interface. With one project it says nothing about cross-project
dependence, pairwise correlation, or diversification.

Separate single-project adapters must not be combined by multiplying their
case weights or assuming independence. A later portfolio constructor must
declare complete multi-project joint scenarios and joint physical weights,
while preserving each adapted marginal distribution. Common shocks and case
couplings must be explicit inputs; the adapter cannot infer them from similar
phase names or outcomes.

## Residual limitations

- This is physical-measure contract mechanics, not calibration, fair value,
  risk-neutral pricing, a market quote, or an investment recommendation.
- Completion-source IDs, taxonomies, and allocations are supplied, not
  evidenced. Source credit quality, legal enforceability, seniority,
  collateral, and realization costs are not added by conversion.
- The common result represents the provider claim only. It excludes sponsor
  residual distributions, protected-reserve economics, eligible project spend,
  and total ProjectCo value except where they produce actual provider cash.
- Paid PIK appears as yield; unpaid claim writeoff remains available only in the
  staged report. The common principal-loss metric intentionally does not measure
  lost contractual yield.
- `SponsorFee` identifies fee provenance but adds no separate sponsor-default
  model beyond the explicit staged path.
- The staged engine permits monetary inputs up to `1e9` million, while the
  common portfolio interface permits `1e6` million per amount. The portfolio
  horizon is 2,400 months, covering the staged engine's maximum 1,200-month
  scheduled path plus maximum 1,200-month delayed recovery. Any staged value
  outside the common engine's amount or aggregate-work limits is rejected,
  never scaled, split, clipped, or silently omitted.
- The staged engine measures peak provider outlay in exact ledger-entry order.
  The common pool conservatively settles draws before receipts within a month,
  so its peak funding need can be higher. Monthly net cash and NPV reconcile;
  within-month peak liquidity need is not asserted to be identical.
- Completion IDs preserve caller-declared payer identity, but recovery remains
  one project-level bucket. Neither label is counterparty credit evidence.
- The adapter creates no pool costs, loss layers, portfolio weights, dependence
  model, pricing kernel, or terminal value.
