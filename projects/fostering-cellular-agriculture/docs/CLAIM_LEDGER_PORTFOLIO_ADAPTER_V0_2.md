# Claim-Ledger Portfolio Adapter v0.2

Status: implemented and mechanically verified, 2026-08-30. The claim-ledger
package boundary, explicit contractual-principal Portfolio mode, source-budget
adapter, package and event lineage, and reconciliation tests are code.

## Purpose and standard project hook

This adapter is the standard route by which a project with an identified
investor claim enters `PortfolioConfig`. The project first records that claim
as a hash-bound
[Project Claim Ledger v0.1](PROJECT_CLAIM_LEDGER_V0_1.md) package. The adapter
then translates only the verified decision-time claim state into the common
Portfolio interface:

```text
project evidence and terms
    -> ClaimLedgerPackage
    -> verified decision-cut ClaimLedgerConfig
    + explicit external cash budgets and allocations
    -> PortfolioConfig v0.2
```

A project should not hand-copy a claim's cash and principal into Portfolio.
Doing so would bypass settlement identities, version selection, provider-claim
controls, package admission, and immutable lineage. A project-specific model
that does not yet emit a claim-ledger package remains outside this standard
hook until an adapter preserves the same meanings and reconciliations.

The output is one project and one alternative scenario for every claim-ledger
scenario. It retains each accepted claim-ledger physical weight, converts the
weights once to a unit-sum Portfolio measure, and preserves dated investor cash.
It does not construct a multi-project dependence model, estimate probabilities,
or turn package evidence into fair value.

## Public boundary

The authoritative entry point accepts a loaded package:

```cpp
[[nodiscard]] ClaimLedgerPortfolioAdapterResult
adapt_claim_ledger_package_to_portfolio(
    const ClaimLedgerPackage& package,
    const ClaimLedgerPortfolioAdapterTerms& terms);
```

`ClaimLedgerPortfolioAdapterTerms` supplies the target Portfolio project ID and
stage, an analyst-declared annual physical hurdle, receipt-source allocations,
external scenario cash budgets, and scenario factor tags. These fields are not
inferred from a coupon, project name, economic outcome, or documentary source
record.

The lower-level overload taking `ClaimLedgerConfig` is a mechanical boundary
for internal construction and tests. It re-evaluates the ledger and applies the
cash, principal, budget, and reconciliation gates, but it cannot establish
package status or immutable package-root lineage. Projects should use the
package entry point.

## Package-root verification and admission

`ClaimLedgerPackage` is a public, copyable value, so its
`package_integrity=true` member is not trusted by itself. The adapter requires a
loader-verified root description, reloads the exact `claim.cfg`, verifies all
bound package bytes again, and compares the reloaded root SHA-256 with the
supplied `claim_config_sha256`. A changed root, changed bound file, forged
status, or caller-mutated core cannot enter the conversion.

The `claim.cfg` root binds the seven package TSV snapshots and, for controlled
packages, the retained source manifest. Package integrity proves confined,
hash-matching bytes, closed schemas, identifiers, and source resolution. It
does not prove source truth, legal enforceability, provider solvency, or market
value.

After reloading, admission is fail-closed:

| Package state | Adapter treatment |
|---|---|
| `synthetic-complete` | Accepted as `SyntheticMechanics` only when the verified decision core has exact, complete expected cash |
| `controlled-candidate` | Accepted as `ControlledExpectedReturn` only when package-level `expected_return_admissible=true` |
| `retained-public-incomplete` | Rejected |

Both accepted routes require:

- verified package integrity, `core_config_ready`, `core_config`, and core
  evaluation;
- mechanically exact, complete, resolved decision-cut expected cash;
- a defined economic-cluster boundary;
- exact identity agreement between package and core for model, package, claim,
  project, decision period, horizon, currency, and monetary basis; and
- no scalar, entry, covenant, provider term, probability, or cash-path status
  in the core whose `known_at_period` follows the decision period.

Controlled expected-return admission is the relevant evidence gate. Separate
`npv_admissible` and `observation_admissible` labels are not required because
the Portfolio hurdle is an independent analyst input and the adapter does not
claim that its output is a settlement-price observation.

## Decision cut, never the backtest path

The package loader retains two different states. `core_config` is the frozen
decision cut: later scalar values and scenario metadata are masked, and later
entries, provider terms, and covenants are excluded. `full_core_config` and
`full_evaluation` retain later state for backtest reconstruction when that
state is structurally available.

The adapter consumes only `core_config` and each scenario's `decision_path`.
It never reads the full/backtest pair, never requires
`full_path_evaluation_available`, and never lets a later actual replace an
ex-ante selected entry. Path lineage carries the exact `decision_entry_ids`
and the decision-time probability and completeness provenance so this boundary
is auditable.

## Probability-measure boundary

Claim Ledger and Portfolio have different numeric tolerances for a probability
total close to one. The adapter does not pass that implementation difference
through to the economic result. It first accepts only a Claim-Ledger-ready
decision measure, sums its raw physical weights with compensated arithmetic,
and divides every weight by that same positive finite total. Portfolio, all
adapter expectations, and all expected-loss reconciliation then use that one
unit-sum measure.

Each `ClaimLedgerPortfolioPathBridge` retains both values:

- `claim_ledger_physical_probability` is the raw accepted Claim Ledger weight;
- `physical_probability` is its normalized Portfolio weight.

Normalization only removes an accepted floating-point closure residual. It is
not calibration, smoothing, a change of scenario, or a risk-neutral measure.
The claim-ledger expectation is divided by the same raw total before it is
compared with the Portfolio expectation, so the adapter never mixes the two
measures.

## External source budgets

An investor receipt is an internal claim cash flow. It does not by itself prove
that the underlying project, buyer, guarantor, refinancing provider, or other
outside payer has enough cash. Every positive receipt therefore needs both:

1. one or more `ClaimLedgerReceiptSourceAllocation` rows that exhaust the
   selected claim entry; and
2. a separately declared `ClaimLedgerScenarioCashBudget` with the same
   `cash_source_id` and taxonomy.

One budget records:

| Field | Meaning |
|---|---|
| `scenario_id` | Optional claim scenario; absence means the same dated budget is declared in every alternative scenario |
| `cash_source_id` | Stable economic cash-budget identifier |
| `source` | Commercial, licensing/royalty, exit sale, recovery, refinancing, explicit support, or financing fee |
| `month`, `amount_million` | Dated gross amount available to the adapted receipt |
| `counterparty_id` | Declared economic payer identity |
| `provider_claim_id` | Required link for guarantee cash; absent for other sources |

Amounts are finite, positive, within the horizon, and bounded. One source ID
must keep one taxonomy, counterparty, and provider identity everywhere.
Allocations must exhaust each positive selected receipt exactly, and every
budget must in turn be exhausted exactly by adapted receipts. The one-claim
adapter therefore cannot manufacture availability from the receipt being
constrained or insert unused source headroom.

Common scope and scenario scope are different typed identities. In particular,
an absent `scenario_id` cannot collide with a real scenario literally named
`COMMON` or any other text. Returned budget lineage expands each common budget
once into every Portfolio path while retaining the null declared scope; a
scenario budget remains in its declared path. This makes both applicability and
exact exhaustion independently auditable.

Before path construction, the adapter indexes common and scenario-specific
budgets and computes the exact expanded lineage cardinality with overflow-safe
arithmetic. More than 1,000,000 expanded rows is rejected before allocation.
Each path then visits only its common slice and its own scenario slice; a valid
large input cannot force a full budget-table rescan for every scenario.

The source taxonomy is constrained by claim cash kind:

- investor `cash-fee` uses `FinancingFee`;
- recovery principal or interest cash uses `Recovery`;
- guarantee principal or interest cash uses `ExplicitSupport`; and
- ordinary principal or interest cash may use commercial, licensing/royalty,
  exit-sale, refinancing, or explicit-support budgets.

`source_record_id` and `cash_source_id` are deliberately different.
`source_record_id` identifies documentary package provenance.
`cash_source_id` identifies a finite economic cash budget. Neither may be
copied into the other as a substitute for analysis.

## Provider identity

Guarantee cash preserves the exact claim-ledger `provider_claim_id`. Its budget
must name that provider claim, use `ExplicitSupport`, and use the provider's
contractual `provider_id` as `counterparty_id`. A missing provider link, an
unknown provider claim, a different counterparty, or relabeling guarantee cash
as recovery is rejected.

The boundary is bidirectional: a provider-bound budget may fund only a
guarantee receipt, and every guarantee receipt must use its exact provider-bound
budget. Ordinary cash may still use an `ExplicitSupport` taxonomy, but it must
not carry a guarantee-provider claim.

This preserves payer concentration and prevents two provider claims from being
silently collapsed. It does not establish provider credit quality, available
capital, claim enforceability, exclusions, or collection. Those belong to the
provider-credit and legal evidence layers.

## Cash and contractual principal are separate ledgers

Portfolio v0.2 adds `ExplicitContractualLedger`. The claim adapter always uses
that mode. Investor cash and contractual principal are conserved independently.

### Investor cash

| Selected claim-ledger entry | Portfolio representation |
|---|---|
| `buyer-price` | `InvestorOutlay` with `ClaimPurchasePrice` |
| `buyer-direct-cost` | `InvestorOutlay` with `BuyerDirectCost` |
| `cash-fee` | `InvestorReceipt` from a `FinancingFee` budget; zero principal component |
| `principal-cash`, `recovery-principal-cash`, `guarantee-principal-cash` | Budgeted `InvestorReceipt`; the full cash amount is principal component |
| `interest-cash`, `recovery-interest-cash`, `guarantee-interest-cash` | Budgeted `InvestorReceipt`; zero principal component |

For every scenario and month:

```text
Portfolio net investor cash
    = investor receipts
      - claim purchase price
      - buyer direct cost
    = claim-ledger investor cash flow.
```

The adapter checks the equality path by path and month by month. Buyer direct
cost is project-attributed investor cash. It affects liquidity and NPV but does
not create contractual principal.

### Contractual principal

| Selected claim-ledger entry or scalar | Portfolio representation |
|---|---|
| `opening_principal_million` | `PortfolioProject::opening_principal_million` |
| `contractual_face_amount_million` | `PortfolioProject::principal_limit_million` |
| `funded-principal` | `FundedPrincipalAddition` |
| `capitalized-fee` | `CapitalizedFeeAddition` |
| `capitalized-interest` | `CapitalizedInterestAddition` |
| Principal cash kinds above | Receipt `principal_component_million` |
| `conversion-principal-extinguishment` | `ConversionExtinguishment` |
| `principal-writeoff` | `Writeoff`, and therefore Portfolio principal loss |

Monthly principal ordering is additions, principal cash, conversion
extinguishment, then writeoff. No reduction may exceed outstanding principal;
the balance may not exceed contractual face; and every adapted resolved path
must close at zero:

```text
opening principal
  + funded-principal additions
  + capitalized fees
  + capitalized interest
= principal cash
  + conversion extinguishment
  + principal writeoff
  + closing principal.
```

OID, original-issue premium, borrower proceeds, amounts due, interest accrual,
and conversion units remain settlement or claim-ledger controls. They are not
additional cash or principal movements. Guarantee cash is emitted once from
the selected guarantee-cash entry; provider evaluation does not create a
second receipt.

## Above-par purchase and no false loss

Purchase price is not contractual principal. Suppose an investor pays 11.0,
pays 0.2 direct cost, acquires contractual principal of 10.0, and later
receives the full 10.0 principal with no other cash:

```text
investor outlay                  = 11.2
contractual principal added      = 10.0
principal returned               = 10.0
contractual principal writeoff   = 0.0
Portfolio principal loss         = 0.0
nominal investor cash shortfall  = 1.2
```

The 1.2 economic shortfall is visible in cash return and NPV. It is not a false
principal loss. Conversely, a below-par purchase does not reduce a contractual
writeoff merely because the investor paid less than face.

`commitment_million` is the greatest pathwise buyer price plus buyer direct
cost: the maximum investor cash outlay. `principal_limit_million` is the
contractual face cap and the reference notional for Portfolio loss layers and
proportional principal-loss protection. The two numbers may differ.

## Output lineage

The adapter result preserves both package-level and generated-event lineage.

`ClaimLedgerPortfolioPackageLineage` records:

- model version, package ID, immutable `claim_config_sha256`, and package
  status;
- synthetic-mechanics or controlled-expected-return admission basis;
- the typed `DecisionCut` source scope;
- economic-cluster ID and boundary status;
- claim project, claim, and target Portfolio project IDs; and
- period unit, periods per year, period-origin, decision and horizon dates,
  and decision and horizon periods. The calendar fields prevent a later
  multi-claim assembler from treating equal period numbers from different
  information axes as contemporaneous cash.

Every generated investor outlay, principal movement, and investor receipt has
a `ClaimLedgerPortfolioCashLineage` row. It records the Portfolio scenario,
common-or-scenario claim scope, exact entry and economic-fact IDs, event group,
entry kind, `known_at_period`, documentary source, provider claim, output kind,
target project, month, amount, optional cash-source ID, optional principal
component, and the validated budget payer and provider identity. A split
receipt produces one lineage row per source allocation.

`ClaimLedgerPortfolioCashBudgetLineage` separately records every exact budget
row after path-scope expansion: Portfolio path, original null-or-scenario scope,
source ID and taxonomy, month, amount, counterparty, and provider claim. A
caller can therefore distinguish otherwise identical cash amounts funded by
different payers and can reconstruct which common budgets applied to which
paths.

Each path bridge additionally records raw accepted and normalized physical
probability, the decision-time probability source and timing,
cash-path-completeness source and timing, all selected decision entry IDs,
contractual and Portfolio principal loss, investor cash outlays and receipts,
nominal cash shortfall, and the maximum monthly cash reconciliation error.

The persistent `PortfolioConfig::source_note` carries package ID, root hash,
status, and the synthetic-assumption boundary. The structured lineage lives in
`ClaimLedgerPortfolioAdapterResult`; callers that serialize only
`PortfolioConfig` must retain the adapter result beside it for full audit
lineage.

## Economic-cluster and dependence boundary

The package must have a defined economic-cluster boundary, and the adapter
copies its `economic_cluster_id`. This prevents an assembler from treating two
records of the same financing as unrelated projects. The one-claim adapter
does not decide whether two claims in one economic cluster are distinct rights,
shared cash claims, or duplicates.

One adapted result is a valid one-project marginal scenario distribution. It
does not establish cross-project independence or diversification. Separate
claim packages must not be combined by multiplying marginal scenario weights.
The implemented
[Claim-Ledger Joint-Portfolio Assembler v0.1](CLAIM_LEDGER_JOINT_PORTFOLIO_ASSEMBLER_V0_1.md)
requires complete joint scenarios, common factors, and joint physical weights.
It re-verifies every package, rejects duplicate economic clusters and
cross-asset cash-source IDs, requires one common calendar, and proves that the
evaluated joint measure reproduces every one-claim marginal. Shared economic
rights and budgets are deliberately unsupported in v0.1; they fail closed
rather than being silently merged or counted twice.

## Hard reconciliation gates

Conversion succeeds only when all of the following hold:

1. The package root reloads with the same SHA-256 and exposes a verified,
   identity-consistent decision core.
2. Package status, controlled admission where applicable, and the defined
   economic-cluster boundary pass.
3. Core expected cash is exact and complete; every path is resolved and has a
   positive buyer-price cash outlay.
4. Periods are monthly with 12 periods per year, and the horizon is positive.
5. Opening principal and contractual face are exact; opening accrued interest
   is exactly zero.
6. Every positive receipt is exhausted by compatible positive source
   allocations, and every allocation is used.
7. Every allocation references an independently supplied budget with stable
   taxonomy, counterparty, provider identity, typed scenario scope, and exact
   exhaustion. Provider-bound budgets and guarantee receipts match in both
   directions. Expanded lineage passes its cardinality guardrail before path
   construction.
8. Investor cash reconciles to the claim ledger for every month and path.
9. Contractual principal reconciles independently; resolved paths close at
   zero and Portfolio writeoff loss equals claim-ledger principal loss.
10. Accepted raw Claim Ledger weights are normalized once; expected Portfolio
    principal loss equals the independently evaluated claim-ledger expected
    principal loss on that same normalized measure.
11. `validate_portfolio_config` and `evaluate_portfolio` complete without a
    reconciliation failure. No failed gate is downgraded to a warning.

## Current honest limitations

- The adapter is monthly only and requires exactly 12 periods per year. It
  does not convert daily, quarterly, irregular, or business-day cash timing.
- Opening contractual principal is supported. Opening accrued interest is not:
  it must be exactly zero because Portfolio has no separate accrued-interest
  exposure and loss ledger.
- Principal loss and protection use contractual principal. There is no
  cost-basis protection for above-par purchase price, buyer direct cost,
  transaction cost, or negative NPV. `nominal_investor_cash_shortfall` is an
  undiscounted diagnostic, not an insured loss or payable claim.
- Accrued-interest writeoff remains in the claim-ledger report; it is not
  relabeled as Portfolio principal loss.
- The adapter accepts mechanically complete resolved cash paths. It does not
  yet map a continuing claim with residual contractual exposure.
- External budgets, counterparty IDs, allocations, hurdle rate, factor tags,
  and eventual multi-project coupling are analyst declarations. Package
  admission does not evidence or calibrate them, so `synthetic_inputs` remains
  true even for an admitted controlled claim package.
- Exact budget exhaustion deliberately represents only the cash consumed by
  this one claim. Unused enterprise cash, guarantor capacity, collateral value,
  or refinancing headroom is not inferred.
- Provider identity is preserved, but provider default, claim disputes,
  collateral, collection delay beyond the ledger, and correlated provider
  credit stress remain outside this adapter.
- Non-cash conversion units do not become Portfolio receipts. A separate
  evidenced cash realization is required.
- Portfolio uses a conservative within-month liquidity convention in which
  outlays are funded before same-month receipts. Monthly net cash and NPV
  reconcile; observed intraday settlement netting is not claimed.
- This is physical-measure contract mechanics and controlled-package lineage,
  not probability calibration, risk-neutral pricing, fair value, a market
  quote, a rating, legal advice, or an investment recommendation.
