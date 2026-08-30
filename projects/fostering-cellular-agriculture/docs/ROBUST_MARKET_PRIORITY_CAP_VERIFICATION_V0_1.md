# Robust Market Non-Principal Priority-Cap v0.1 — Verification Record

Status: implemented deterministic synthetic verification, 2026-08-30.

## Verified result

The evaluator answers one finite contractual-design question while holding the
underlying pool and funded principal structure fixed:

> What is the smallest tested lifetime cap `B` on actual non-principal cash
> allocated first to the market claim that satisfies the declared market
> mandate, and what junior NPV concession accompanies it?

For the retained five-point hand fixture, the result is:

```text
tested B grid                         0, 0.08, 0.50, 8/15, 1.00
previous point before adequacy        0.50
minimum tested market-adequate B      8/15
minimum tested market-and-junior B    8/15
status                                minimum-tested-balanced-cap-found
```

This is a fixed-price, physical-measure adequacy sensitivity. It does not
interpolate between grid points, solve an investor hurdle, estimate fair
value, annualized yield, demand, legal form, or capital mobilization.

The strict core, parser, normalized five-file replay, command-line report, and
exit taxonomy pass in both Debug and Release. The full strict MSVC project
suite passes `54/54` tests in each configuration.

## Implementation under test

The implementation consists of the
[public core interface](../include/naturalehia/cellular_finance/robust_market_priority_cap.hpp),
[core evaluator](../src/robust_market_priority_cap.cpp),
[strict configuration interface](../include/naturalehia/cellular_finance/robust_market_priority_cap_config.hpp),
[closed-schema parser](../src/robust_market_priority_cap_config.cpp), and
[human-readable command-line report](../apps/market_priority_cap_cli/main.cpp).
The economic definition and limitations are in the
[term specification](ROBUST_MARKET_PRIORITY_CAP_TERM_V0_1.md).

Each cap candidate copies one validated two-claim base stack, changes only the
market claim's `priority_nonprincipal_cap_million`, and calls the public
capital-stack event-polytope evaluator exactly once. The caller's base stack is
not mutated. Across the grid, the engine audits that contributions, principal
cash, principal risk, principal weighted-average life, aggregate dated cash,
and pool-hurdle NPV remain fixed, while market non-principal cash and path NPV
do not decrease and junior non-principal cash and path NPV do not increase.

## Exact hand fixture

The retained fixture fixes:

```text
q = 25/28              fixed selected success-cash participation
A = 12                 funded junior first-loss principal
K = 20                 aggregate funded commitment
M = K - A = 8          funded market principal notional
market contribution    8.08 in every state and probability measure
junior contribution    12.12 in every state and probability measure
claim hurdles          0% for the exact hand reconciliation
junior NPV target      0
```

Let `u=min(B,50/7)` and `v=min(B,25/7)`. The four market-claim path NPVs are:

```text
common loss                         -4.08
common success                       u - 0.08
either single-project loss           v - 0.08
```

Direct evaluation of the declared event polytope gives:

```text
robust market NPV  = -0.48 + 0.50u + 0.40v
central market NPV = -0.16 + 0.62u + 0.36v
maximum market NPV = -0.12 + 0.70u + 0.29v
robust junior NPV  =  0.48 - 0.50u - 0.40v
junior concession  = max(0, -robust junior NPV)
```

Every tested cap is below both cash-saturation points, so `u=v=B` and the
robust market expression is `0.90B-0.48`. Its exact zero is `B=8/15`. The
retained fixture declares a maximum junior concession of `0.42`, so both
`B=8/15` and `B=1` are balanced even though the smaller tested cap is selected.

| Tested `B` | Robust market NPV | Robust junior NPV | Junior concession | Market adequate | Balanced |
|---:|---:|---:|---:|:---:|:---:|
| 0 | -0.48 | 0.48 | 0 | no | no |
| 0.08 | -0.408 | 0.408 | 0 | no | no |
| 0.50 | -0.03 | 0.03 | 0 | no | no |
| 8/15 | 0 | 0 | 0 | yes | yes |
| 1.00 | 0.42 | -0.42 | 0.42 | yes | yes |

The engine uses a disclosed money comparison tolerance at the `B=0.08` path
sign boundary. Tests place caps immediately below, at, and above that boundary
so binary representation does not choose the economic classification.

## Principal risk and return downside stay visible

The market claim's principal terms do not change with `B`. Every candidate
retains:

```text
market principal notional M                 8.00
market total contribution                   8.08
worst expected principal loss               0.40 = 5% of M
principal-loss ES95 and ES99                 4.00 = 50% of M
worst principal impairment probability      10%
maximum principal-cash WAL                   about 1.922111 years
worst NPV-shortfall ES95 and ES99            4.08 = 51% of M
```

The NPV-shortfall tail does not improve because the common-loss state has no
non-principal cash to reallocate and can fill either tail. A larger cap can
improve robust expected market NPV; it cannot repair a no-cash state or reduce
principal loss.

At `B=1`, the priority-cap evaluator reproduces the corresponding implemented
frontier candidate, including endpoint values and probability and tail
witnesses. This cross-engine comparison detects accidental changes in `q`,
`A`, the waterfall, the probability set, or the claim hurdles.

## Cash-transfer and grid audit

The canonical report confirms every across-grid invariant. Maximum observed
cash reconciliation residuals are:

```text
market gain minus junior surrender by date    1.110223e-16
aggregate dated cash change                    3.552714e-15
pool-hurdle NPV change                         0
market contribution change                    0
market principal-cash change                   0
market principal-risk change                   0
market principal-WAL change                    0
```

The first integration run exposed a real audit defect: the code assumed each
claim's monthly cash ledger had the same sparse record count at every cap. A
zero or newly active non-principal payment can legitimately add or remove a
sparse record. The corrected audit reconciles the canonical union of month
keys, treats an absent record as zero, and separately checks dated principal,
contribution, non-principal, and aggregate cash. The canonical fixture and
multi-date tests now exercise that path.

## Parser, report, and replay controls

The priority-cap file is a closed versioned `key=value` schema. It rejects
unknown, duplicate, missing, non-finite, unsafe, signed-negative-zero, and
incoherent values; more than 1,024 caps; an omitted literal zero, contractual
ceiling, or base-reference cap; duplicate cap values; sub-base-currency-unit
positive caps; mismatched claim IDs; a base stack other than exactly two
ordered funded claims; and a term with no cap-sensitive market mandate.

The parser reads incrementally under a fixed line-length bound, rejects an
unknown or out-of-range indexed key before storing it, and caps raw records at
the closed schema's 1,045 possible entries. Adversarial tests verify that a
multi-megabyte delimiter-free line and a flood of unknown keys are rejected
near their first invalid bytes rather than accumulated in memory.

Normalized output is deterministic, reloadable, canonically ordered, and
byte-stable after load/print/load/print. The CLI loads, validates, and can print
all five inputs: portfolio, event polytope, success participation, base stack,
and priority-cap terms. The golden regression checks the hand boundary, every
candidate, selection brackets, witnesses, risk invariants, numerical audits,
source notes, and false-claim ledger.

Exit codes are:

```text
0  finite-grid analysis completed, including an honest no-adequate-cap result
1  command grammar or usage failure
2  file loading or strict parsing failure
3  readable inputs fail cross-input financial validation or evaluation
```

`calibrated_execution_authorized=false` is emitted on every report path.
The CLI also flushes and validates standard output before returning success.
A deterministic broken-pipe regression proves that a truncated report is
reported as an analysis/output failure with exit code 3 rather than as a
successful complete report.

## Resource boundary

The complete grid is preflighted with overflow-safe integer arithmetic before
any candidate evaluation:

```text
C * [S * (S + E + 1)
     + R
     + N * S * (H + 1)
     + 2 * S * (H + 1)] <= 4,000,000
```

`C`, `N`, `S`, `E`, and `H` are the cap-candidate, project, scenario, event,
and horizon-month counts. `R` includes every cash-availability, draw, receipt,
pool-cost, cash-source, and factor-tag record, plus each configured loss layer
applied to each scenario. The helper is shared internally with the frontier;
its public API and exact diagnostic remain unchanged.

For the canonical priority-cap fixture:

```text
C=5, N=2, S=4, E=4, H=24, R=36
probability-projection work    180
cash-path work               2,180
combined work                2,360 / 4,000,000
```

Tests separately reject probability-heavy, long-horizon, high-record, and
scenario-expanded loss-layer cases with the exact fail-closed diagnostic.
This is a conservative structural-work proxy, not an economic limit or a
wall-clock guarantee.

## Falsification coverage

Core, parser, and report tests cover:

- the exact `8/15` root and a grid omitting that root;
- no market-adequate cap, fixed-structure ineligibility, and no overlap between
  market adequacy and the junior-concession limit;
- `q=0` and no transferable non-principal cash;
- caps below, at, and above global cash saturation;
- two non-principal payment dates and unequal positive claim hurdles;
- fixed `A=16` with no market principal write-down but nonzero return downside;
- a one-base-currency-unit market principal notional;
- base-stack immutability and exact `B=1` frontier equivalence;
- principal-risk, WAL, cash, and NPV monotonicity and invariance; and
- candidate, probability, horizon, record, and loss-layer resource failures.

## Build and retained independent checks

The project was compiled as C++20 with MSVC under `/EHsc /W4 /WX
/permissive-`. Debug and Release were built separately. All `54/54` CTest
targets passed in each configuration; the final measured complete runs were
13.75 seconds in Debug and 5.40 seconds in Release. The three priority-cap-specific
core, parser, and CLI tests also passed separately in both configurations.

An independent no-edit mathematical audit re-derived the fixture with exact
rational arithmetic, reconciled every endpoint, selection, risk measure, and
resource count, and found no numerical implementation defect. Its documentation
findings—an omitted fixed `A` eligibility constraint, stale implementation
status, and an undisclosed comparison tolerance—were corrected before this
record was finalized. A separate independent code review then identified and
closed the bounded-input and broken-output cases described above; its final
re-review reported no additional findings.

After the priority-cap module introduced the shared structural-work helper,
the retained independent frontier oracle was rebuilt against the current
Debug and Release libraries. Both runs remained identical: 228 candidates,
5,472 direct endpoint-witness checks, 62,816 assertions, maximum scalar
difference `5.8207660913467407e-10`, maximum witness reconciliation difference
`6.0396132539608516e-14`, and zero discrepancies. The separate direct resource
guard also retained zero discrepancies at its exact horizon, record, and
loss-layer boundaries.

## Interpretation boundary

All fixture inputs are invented `DEMO` data. A passing cap means only that one
tested contractual allocation clears supplied physical-measure limits under
the supplied synthetic probability set. It does not establish a market price,
expected investor return, annualized yield, demand, suitability, enforceable
cash rights, legal or regulatory treatment, accounting, tax, empirical
probability, reference-project financeability, capital mobilization, financing
additionality, cellular-agriculture output, or animal-welfare impact.

Before any decision use, the project still requires controlled project data,
enforceable cash-right analysis, empirical probability and dependence work,
independent model validation, pricing-measure and investor research, and the
separate financing-additionality and animal-product-displacement evidence
defined elsewhere in this project.
