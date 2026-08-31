# Capital Stack Asset-to-Liability Bridge v0.2

Status: implemented and verified synthetic accounting boundary, 2026-09-01.

## Purpose

Capital Stack v0.2 lets a Portfolio v0.2 claim with an
`explicit-contractual-ledger` enter a funded tranche waterfall without
collapsing distinct ledgers into one number. It keeps separate:

1. cash subscribed into the acquisition and primary-funding reserve;
2. dated cash used to acquire or fund the assets;
3. contractual principal on those assets; and
4. principal issued to the capital-stack investors.

Buyer-direct cost remains a fifth, non-principal cash item: an additional
dated call. Version 0.1 still rejects an explicit contractual-principal ledger.
Version 0.2 is a separate, closed accounting contract; it does not reinterpret
v0.1.

The bridge allocates actual cash and preserves contractual asset outcomes. It
does not estimate fair value, market price, spread, rating, legal
enforceability, reserve custody risk, tax, regulatory capital, or empirical
probabilities. It accepts synthetic inputs only.

## Versioned term boundary

In addition to the common stack assertions, the v0.2 term file requires these
four statements to be explicit and true:

```text
capital_stack.asset_acquisition_and_primary_funding_limit_is_fully_funded_at_par_at_month_zero=true
capital_stack.buyer_direct_costs_are_additional_pro_rata_calls=true
capital_stack.principal_base_cash_above_issued_principal_is_nonprincipal=true
capital_stack.principal_limit_capacity_difference_is_reported_without_valuation_claim=true
```

The legacy assertion that aggregate project commitment is funded as tranche
principal must be false in v0.2. A v0.1 term cannot contain v0.2 assertions,
and a v0.1 term still rejects any explicit-contractual-ledger project. The
programmatic default remains v0.1.

## Reserve, uses, and issued principal

For explicit project `i` and complete joint state `s`, let

```text
A_i,s = claim-purchase-price cash
      + primary-project-funding cash.
```

These are dated cash uses. Buyer-direct cost is excluded. The reserve basis for
an explicit project is its maximum declared use over all supplied states:

```text
R_i = max_s A_i,s.
```

A legacy-accounting project retained in a v0.2 portfolio instead contributes
its declared commitment as `R_i`. The aggregate funded reserve and issued
principal are

```text
R = sum_i R_i.
```

This is deliberately the sum of project-level maxima, not the maximum of one
state's aggregate use. It can therefore fund mutually exclusive project uses
without assuming that reserve allocated to one project is available to another.

Tranche attachment and detachment points form a contiguous partition from zero
through `R`, and investors subscribe `R` at par in month zero. The reserve is
assumed zero-yield and lossless. In state `s`, define

```text
A_s = sum_i A_i,s
U_s = max(0, R - A_s).
```

`U_s` is returned only at the horizon. It is investors' own unused reserve,
not asset revenue, contractual principal created, or profit.

Buyer-direct cost `C_s` and pool cost `G_s` remain outside reserve and issued
principal. Both are dated pro-rata calls by tranche notional. Neither creates
contractual asset principal or issued tranche principal. Accordingly,
`aggregate_project_outlay_limit_million` can exceed `R`.

## Contractual asset principal and the capacity diagnostic

The Portfolio explicit ledger remains authoritative for every asset path:

```text
opening contractual principal + principal additions
  = principal-component cash receipts
  + conversion extinguishment
  + writeoff
  + closing contractual principal.
```

Let `K` be the aggregate contractual-asset principal limit. The stack reports

```text
principal-limit capacity difference_s = K - A_s.
```

This is only a comparison between a declared contractual-principal limit and
state cash use. It can be positive or negative. It is not principal actually
created, an asset value, a market-price conclusion, or an accounting valuation.

## Exact L/O/Q boundary

Three observables answer three different questions in state `s`:

```text
L_s = contractual asset principal explicitly written off
O_s = contractual asset principal still outstanding on continuing paths
Q_s = issued principal not returned as principal cash by the horizon.
```

`L_s` and `O_s` come unchanged from the Portfolio contractual-principal
ledger. They are asset-side facts. They are not assigned to liability
tranches.

For the liability side, let `P_s` be actual underlying cash classified as
principal. Principal-base cash is

```text
B_s = P_s + U_s.
```

Only the amount needed to repay issued principal enters the senior-first
principal waterfall:

```text
D_s = min(R, B_s).
```

The implementation then computes the horizon shortfall once, at aggregate
scale. With

```text
tau(R) = 1e-9 + 16 * epsilon_double * max(1, abs(R)),
```

it uses

```text
z_s = max(0, R - D_s)
Q_s = 0 if z_s <= tau(R), otherwise z_s.
```

`Q_s` is an exact cash-ledger shortfall subject to that numerical boundary. It
does not identify why cash is absent, allocate causality between resolved and
continuing assets, assert an accounting conclusion, or assume any post-horizon
recovery for `O_s`.

For a tranche with attachment `a`, detachment `d`, and notional `n=d-a`, define

```text
layer(z; a,d) = min(max(z-a, 0), n).
```

Its v0.2 principal cash shortfall is

```text
q_s(a,d) = layer(Q_s; a,d).
```

Because principal cash is paid most senior first, shortfall occupies the
liability stack from attachment zero upward. Within the same reconciliation
tolerance, each tranche's principal cash equals `n-q_s(a,d)`. The v0.1 loss and
unresolved-exposure fields are not repurposed in v0.2; the v0.2 cash-shortfall
fields and their probability and tail metrics are the relevant liability
outputs. Every summary, tranche summary, and path-level tranche result also
sets `legacy_v01_loss_layering_metrics_are_applicable=false`; v0.1 sets the
same flag to `true`. Downstream investor screens must select the metric family
from the model version and these applicability flags, and must fail closed if
they disagree. A zero in a legacy placeholder is therefore never evidence
that a v0.2 claim has zero principal risk.

There is intentionally no equation linking `L_s + O_s` to `Q_s`. The asset
ledger can show a writeoff while issued principal is fully returned, or show no
writeoff while issued principal has a horizon cash shortfall.

## Surplus cash and the monthly source memo

Principal-base cash above unpaid issued principal remains actual cash:

```text
S_s = B_s - D_s = max(0, B_s - R).
```

`S_s` enters the non-principal waterfall together with actual underlying
non-principal cash. Priority tranches receive cash only up to their remaining
lifetime caps; the first-loss residual receives the remainder. Reclassification
changes liability treatment, not the amount or original source of cash.

The source memo is calculated monthly. If underlying principal cash `P_m` and
unused-reserve return `U_m` arrive in the same month, they have equal seniority.
For combined principal `B_m=P_m+U_m>0` and tranche principal allocation
`d_j,m`, the memo split is

```text
underlying-principal memo_j,m = d_j,m * P_m / B_m
unused-reserve memo_j,m       = d_j,m * U_m / B_m.
```

Any same-month principal surplus is split by the same proportions:

```text
contractual-principal surplus_m = surplus_m * P_m / B_m
unused-reserve surplus_m        = surplus_m * U_m / B_m.
```

When those two surplus components and underlying non-principal cash enter the
same non-principal waterfall, each tranche's receipt is again memo-split pro
rata across the simultaneous sources. This convention prevents processing
order from manufacturing source preference. It is a reporting convention only;
it does not change total cash or waterfall seniority.

## Cash and source identities

Every state must satisfy, within the declared reconciliation tolerance:

```text
reserve:                 R = A_s + U_s
investor-outlay split:   project outlays = A_s + C_s
principal-base cash:     P_s + U_s = D_s + S_s
issued principal:        R = D_s + Q_s
subscriptions:           sum tranche subscriptions = R
additional calls:        sum direct-cost calls = C_s
                         sum pool-cost calls = G_s
principal layers:        sum tranche principal cash = D_s
                         sum tranche shortfalls = Q_s
cash distributions:      sum tranche distributions = D_s + S_s + N_s
```

Here `N_s` is actual selected-`q` underlying non-principal cash. Consequently,

```text
stack net cash
  = D_s + S_s + N_s - R - C_s - G_s
  = P_s + N_s - A_s - C_s - G_s
  = underlying portfolio net cash.
```

The source memos also reconcile underlying principal to `P_s`, reserve cash to
`U_s`, and the two surplus components to `S_s`. The stack preserves `L_s` and
`O_s` independently and distributes no more cash than the underlying paths and
unused reserve supply. Outstanding principal, conversion units, unused
priority caps, and uncalled outlay capacity receive no invented cash value.

## Exact hand fixture: 8 funded, 10 contractual principal, 8 returned

All amounts are synthetic test units in millions. The resolved asset uses 8 of
cash, creates 10 of contractual principal, returns 8 of principal cash, and
writes off 2.

| Ledger item | Amount |
|---|---:|
| Contractual-principal limit `K` | 10 |
| Acquisition use `A` / reserve `R` | 8 |
| Principal-limit capacity difference `K-A` | 2 |
| Underlying principal cash `P` | 8 |
| Contractual asset loss `L` | 2 |
| Contractual asset outstanding `O` | 0 |
| Issued-principal distribution `D` | 8 |
| Issued-principal cash shortfall `Q` | 0 |
| Stack net cash and NPV at 0% | 0 |

With `[0,4]` first-loss and `[4,8]` priority tranches:

| Tranche | Notional | Principal cash | Principal cash shortfall |
|---|---:|---:|---:|
| First-loss residual | 4 | 4 | 0 |
| Priority | 4 | 4 | 0 |

The asset-side writeoff `L=2` and liability-side shortfall `Q=0` are both
correct. The bridge does not force the asset result onto the liability layers.

## Exact hand fixture: 12 funded, 10 contractual principal, 10 returned

The resolved asset uses 12 of cash, creates and returns all 10 of contractual
principal, and has no writeoff or closing balance.

| Ledger item | Amount |
|---|---:|
| Contractual-principal limit `K` | 10 |
| Acquisition use `A` / reserve `R` | 12 |
| Principal-limit capacity difference `K-A` | -2 |
| Underlying principal cash `P` | 10 |
| Contractual asset loss `L` | 0 |
| Contractual asset outstanding `O` | 0 |
| Issued-principal distribution `D` | 10 |
| Issued-principal cash shortfall `Q` | 2 |
| Stack net cash and NPV at 0% | -2 |

With `[0,4]` first-loss and `[4,12]` priority tranches:

| Tranche | Notional | Principal cash | Principal cash shortfall |
|---|---:|---:|---:|
| First-loss residual | 4 | 2 | 2 |
| Priority | 8 | 8 | 0 |

The asset-side result is `L=0, O=0`; the liability cash ledger nevertheless
ends with `Q=2`. The capacity difference remains a diagnostic and supplies no
valuation interpretation.

## Claim-ledger-to-stack acceptance

The required integration route is

```text
loader-verified ClaimLedgerPackage objects
  -> Claim-Ledger Portfolio Adapter v0.2
  -> Claim-Ledger Joint-Portfolio Assembler v0.1
  -> exact assembled PortfolioConfig v0.2
  -> Capital Stack v0.2.
```

The stack receives the assembler's exact Portfolio object. No project, path,
cash event, principal movement, probability, or source budget is manually
re-encoded. The controlling two-claim case retains 20 of contractual-principal
limit, 18 of reserve and issued principal, and 18.4 of total project outlay
limit including 0.4 of buyer-direct cost. Across
`both-perform / a-perform-b-fail / a-fail-b-perform / both-fail`, it reports

```text
L = 0 / 4 / 4 / 8
O = 0 / 0 / 0 / 0
Q = 0 / 2 / 2 / 6.
```

Tranche distributions equal actual assembled receipts and stack nominal cash
equals Portfolio nominal cash in every state. Package-root and joint-selection
lineage remain beside the result.

The same assembled explicit portfolio is still rejected by Capital Stack v0.1
at its named accounting boundary. Acceptance by v0.2 proves only that these
synthetic ledgers reconcile. It does not promote a package, joint probability,
capital structure, or tranche to calibrated, controlled, market-ready, or
investable status.

## Remaining boundary

The bridge assumes full month-zero funding, a lossless zero-yield reserve,
horizon-only reserve return, no investor capital-call default, and fixed
waterfall terms. It does not model warehouse finance, reserve-provider credit,
reinvestment, coupons, scheduled liability amortization, replenishment,
hedging, taxes, legal priority, bankruptcy remoteness, ratings, regulatory
capital, secondary trading, or market-consistent pricing.

Implementation is in
[`capital_stack.hpp`](../include/naturalehia/cellular_finance/capital_stack.hpp),
[`capital_stack.cpp`](../src/capital_stack.cpp), and the strict parser in
[`capital_stack_config.cpp`](../src/capital_stack_config.cpp). The software
record is
[`CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_VERIFICATION_V0_2.md`](CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_VERIFICATION_V0_2.md).
