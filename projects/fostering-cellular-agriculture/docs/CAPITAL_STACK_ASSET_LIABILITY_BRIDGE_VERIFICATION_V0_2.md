# Capital Stack Asset-to-Liability Bridge v0.2 — Verification Record

Status: complete strict WebAssembly suite passes, 70/70, 2026-09-01.

## Verified boundary

All 70 declared tests were rebuilt under the repository's strict C++20
Emscripten SDK 6.0.5 Release configuration, with warnings promoted to errors.
Every generated WebAssembly program and every CLI-wrapper invocation executed
through Node with exception support enabled. The complete run passed 70/70 in
92.03 seconds. The release evidence recorded here is the WebAssembly run; it
does not rely on a Windows-native result.

The following focused executables passed in the current verification run:

- `cellular_finance_capital_stack_tests`, including the legacy v0.1 boundary;
- `cellular_finance_capital_stack_config_tests`;
- `cellular_finance_capital_stack_v02_tests`;
- `cellular_finance_capital_stack_probability_polytope_tests`; and
- `cellular_finance_claim_ledger_capital_stack_v02_tests`.

Those five bridge-specific executables passed inside the complete 70/70 run.
The other 65 tests cover the surrounding portfolio, ambiguity, guarantee,
provider-credit, investor-screening, evidence, configuration, integration and
CLI surfaces. The Node-aware CTest harness is part of this boundary: it cannot
silently treat an unexecuted `.js` file as a passing CLI test.

The checks establish software arithmetic for synthetic inputs. They do not
validate project evidence, probabilities, contractual enforceability, reserve
custody, provider credit, fair value, market price, rating, tax, regulatory
capital, or investor suitability.

## L/O/Q oracle

For every v0.2 state, the tests treat these as separate observables:

```text
L = contractual asset principal written off
O = contractual asset principal outstanding on continuing paths
Q = canonical horizon cash shortfall against issued principal.
```

Only `Q` is layered across the liability tranches:

```text
tranche shortfall = layer(Q; attachment, detachment).
```

The tests do not require `Q=L+O`, do not assign `L` or `O` to a tranche, and do
not infer a post-horizon value for `O`. The legacy v0.1 tranche loss and
unresolved-exposure fields stay zero in v0.2; the cash-shortfall fields,
incidence measures, and cash-shortfall tail measures are the v0.2 outputs.

## Exact hand checks

All amounts are synthetic test units in millions. The hurdle is zero, so NPV
equals nominal net cash.

### 8 funded, 10 contractual principal, 8 returned

| Measure | Expected | Verified |
|---|---:|---:|
| Contractual-principal limit `K` | 10 | 10 |
| Acquisition use / issued principal `R` | 8 | 8 |
| Principal-limit capacity difference `K-A` | 2 | 2 |
| Underlying principal cash `P` | 8 | 8 |
| Contractual asset loss `L` | 2 | 2 |
| Contractual asset outstanding `O` | 0 | 0 |
| Issued-principal distribution `D` | 8 | 8 |
| Issued-principal cash shortfall `Q` | 0 | 0 |
| Stack NPV | 0 | 0 |

The `[0,4]` residual and `[4,8]` priority claims each receive 4 of principal
cash and have zero shortfall. The asset ledger nevertheless preserves `L=2`.

### 12 funded, 10 contractual principal, 10 returned

| Measure | Expected | Verified |
|---|---:|---:|
| Contractual-principal limit `K` | 10 | 10 |
| Acquisition use / issued principal `R` | 12 | 12 |
| Principal-limit capacity difference `K-A` | -2 | -2 |
| Underlying principal cash `P` | 10 | 10 |
| Contractual asset loss `L` | 0 | 0 |
| Contractual asset outstanding `O` | 0 | 0 |
| Issued-principal distribution `D` | 10 | 10 |
| Issued-principal cash shortfall `Q` | 2 | 2 |
| Stack NPV | -2 | -2 |

The `[4,12]` priority claim receives its full 8. The `[0,4]` residual receives
2 of principal cash and has a 2-unit shortfall. The asset-side result remains
`L=0, O=0`.

### 8 funded, 10 contractual principal, 10 returned

The asset returns 10 against 8 of issued principal. The tests require 8 to
enter the issued-principal waterfall, the remaining 2 to retain its
contractual-principal source memo while entering the non-principal waterfall,
and total distributions to remain 10. Thus `D=8`, `S=2`, `L=O=Q=0`, and stack
nominal net cash is 2.

### Buyer-direct cost

An asset with 8 of acquisition use and 0.5 of buyer-direct cost has `R=8`, a
project outlay limit of 8.5, and 0.5 of additional dated pro-rata calls. In the
two-equal-tranche fixture, each tranche is called for 0.25. The cost never
becomes reserve use, contractual asset principal, or issued principal. The
capacity diagnostic is zero because `K=8` and `A=8`.

### Continuing asset

A continuing path with `R=8`, principal cash of 3, and contractual principal
still outstanding of 7 reports

```text
L=0, O=7, D=3, Q=5.
```

The senior-first waterfall gives the `[4,8]` priority tranche 3 of principal
cash and a 1-unit shortfall; the `[0,4]` residual receives no principal cash and
has a 4-unit shortfall. The result makes no causal assignment between `O=7` and
`Q=5`.

## Project maxima and simultaneous-source controls

The staggered-use fixture has two explicit projects. Each has a maximum
acquisition use of 10, but only one project uses cash in either state. The test
requires

```text
R = max_s A_1,s + max_s A_2,s = 10 + 10 = 20,
```

not the 10-unit maximum aggregate state use. Each state therefore uses `A=10`
and returns `U=10` at the horizon.

In the simultaneous-surplus state, underlying principal cash `P=15` arrives in
the same month as `U=10`. The exact cash result is

```text
B=P+U=25, D=20, S=5.
```

The equal-seniority pro-rata source memo records 12 of the principal
distribution as underlying principal and 8 as reserve return. It records the
surplus as 3 from underlying principal and 2 from reserve. Total principal cash
and total surplus remain 20 and 5; processing order creates no source
preference.

The other state preserves `L=5, O=0, Q=5`. `Q` layers as 4 to the `[0,4]`
residual and 1 to the `[4,20]` priority tranche. All reserve, source, waterfall,
and nominal-cash reconciliations remain below `1e-9`.

## One aggregate numerical boundary

The high-scale test uses issued principal `R=1,000,000`, underlying principal
cash smaller by `4e-10`, and the minimum permitted junior notional `1e-6`. The
engine canonicalizes the aggregate residual once using

```text
tau(R) = 1e-9 + 16 * epsilon_double * max(1, abs(R)).
```

Because the residual is below `tau(R)`, aggregate `Q`, both tranche shortfall
layers, and both binary shortfall-event indicators are exactly zero. The small
contractual writeoff remains visible as asset-side `L`; it is not used as a
replacement for canonical `Q`. This verifies that scale and minimum tranche
size cannot create inconsistent shortfall classifications.

The WebAssembly suite also covers a separate legacy-ledger floating-point
boundary. Three funding draws of `2.45`, `2.45`, and `2.10` sum to a nominal
7-unit repayment with a binary residual of approximately
`4.4408920985006262e-16` under wasm32 arithmetic. For the legacy
draw-equals-principal ledger only, a returned-principal amount that is within
the declared money tolerance of total draws is canonicalized to those draws
before principal is closed. The path therefore has zero loss and no impairment
event. The same regression declares an explicit-ledger writeoff of one machine
epsilon and requires the engine to preserve it as a real loss and event. The
rule removes a computational artifact without erasing a contractual ledger
entry. In the complete run this restored the intended 16 / 37 / 60 synthetic
portfolio loss outcomes and their associated provider-claim probabilities.

## Event-polytope coverage

The singleton event-polytope bridge consumes the exact deterministic v0.2 path
and reproduces its `L`, `O`, `Q`, NPV, nominal cash, and tranche shortfall under
one audited probability witness.

The nontrivial two-state test constrains the `Q>0` state probability to
`[0.25,0.75]`. It verifies these physical-measure ranges:

```text
E[L] = 1.25 / 2.50 / 3.75
E[Q] = 1.25 / 2.50 / 3.75
```

The identical numerical ranges arise from this particular fixture; the engine
still projects `L` and `Q` as separate state variables. The residual tranche's
shortfall probability is `0.25 / 0.50 / 0.75` and its maximum ES95 shortfall is
4. The priority tranche has the same event probability and maximum ES95
shortfall of 1. Probability-constraint, objective-reconciliation, and tail-mass
violations remain below `1e-9`.

A second two-state fixture prevents that numerical coincidence from hiding a
selector error. In `(resolved-loss, continuing)` state order it supplies the
distinct vectors

```text
L = (2, 0), O = (0, 7), Q = (0, 5).
```

The continuing-state probability is constrained to `[0.25,0.75]`, with central
weight `0.50`. Independent optimization of the three selectors must produce

```text
E[L] = 0.50 / 1.00 / 1.50
E[O] = 1.75 / 3.50 / 5.25
E[Q] = 1.25 / 2.50 / 3.75.
```

The endpoint witnesses are retained and checked rather than reconstructed from
the reported scalars. This proves that the probability layer does not substitute
asset loss, continuing exposure, or liability cash shortfall for one another.

## Version and parser controls

The focused configuration tests establish that:

- v0.1 retains its closed key set and named rejection of explicit contractual
  principal ledgers;
- v0.1 rejects every v0.2-only asset-liability assertion;
- v0.2 rejects the legacy aggregate-commitment funding assertion;
- all four v0.2 accounting assertions are required and must be true;
- unknown, duplicate, missing, and version-incompatible keys fail closed; and
- normalized v0.1 and v0.2 files are deterministic and reloadable under their
  original versions.

The programmatic default remains v0.1, preventing an existing caller from
silently opting into the new accounting semantics.

## Downstream investor-screen controls

The robust market-priority-cap and issue-price-support screens select their
principal-risk family by Capital Stack version and applicability flags. A v0.1
stack continues to use the legacy realized-loss family. A v0.2 stack uses
issued-principal cash-shortfall `Q`: expected `Q` over market notional, `Q`
ES95, `Q` ES99, and `Pr[Q>0]`. Zero-filled legacy placeholders are therefore
not admissible evidence of zero v0.2 risk.

The synthetic v0.2 fixture reports a worst expected-`Q` fraction of 5%, `Q`
ES95 and ES99 fractions of 50%, and shortfall incidence of 10%. It remains
valid when every underlying project uses the legacy at-par project ledger,
because the Capital Stack v0.2 liability bridge still computes applicable
`Q` metrics. Tightening the expected-shortfall mandate to 4.9% rejects the
fixed structure. The issue-price screen reads the same risk family and holds it
invariant across hurdle-price cases; changing price support cannot manufacture
a reduction in contractual cash risk.

## Reconciliation coverage

The focused v0.2 tests require every reported residual to remain below `1e-9`
test units for:

- project-level reserve maxima, dated reserve use, horizon return, and reserve
  roll-forward;
- par subscriptions and non-negative reserve balance;
- buyer-direct-cost and pool-cost calls;
- underlying-principal, reserve-principal, and issued-principal allocation;
- contractual-principal and unused-reserve surplus source memos;
- underlying and total non-principal distributions;
- priority non-principal caps;
- aggregate `Q` and every `layer(Q)` tranche shortfall;
- exact preservation of contractual asset `L` and `O`;
- nominal investor cash and stack NPV; and
- probability feasibility, projected objectives, and tail-mass witnesses.

## Loader-to-assembler acceptance case

The dedicated integration executable passed under Emscripten and Node. It:

1. loads and re-verifies two hash-bound Claim Ledger packages;
2. adapts and assembles them into one explicit Portfolio v0.2 without manual
   re-encoding;
3. passes that exact Portfolio object into Capital Stack v0.2;
4. preserves 20 of contractual-principal limit, 18 of reserve and issued
   principal, and 18.4 of project outlay limit including 0.4 of direct cost;
5. reports state `L` as `0 / 4 / 4 / 8`, state `O` as `0 / 0 / 0 / 0`, and
   state `Q` as `0 / 2 / 2 / 6`;
6. reconciles every distribution to assembled receipt cash and every stack
   nominal result to Portfolio nominal cash;
7. retains package-root and joint-selection lineage beside the stack result;
   and
8. proves that v0.1 rejects the same assembled explicit portfolio at its named
   accounting boundary.

This acceptance case demonstrates that the accounting boundary consumes the
assembler's exact explicit Portfolio object. It passed as part of the complete
70/70 WebAssembly suite reported above.

## Honest interpretation

Passing these tests means that the synthetic reserve, asset-principal,
liability-principal, source-memo, and cash ledgers close under the declared
numerical tolerance. It does not show that any claim package has sufficient
evidence, that any joint probability is empirically calibrated, that a reserve
is legally or operationally secure, or that any tranche can be issued at the
modeled terms.

The economic contract and remaining limitations are in
[`CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_V0_2.md`](CAPITAL_STACK_ASSET_LIABILITY_BRIDGE_V0_2.md).
