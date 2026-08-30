# Robust Capital-Mobilization Frontier v0.1 — Verification Record

Status: implemented deterministic synthetic verification, 2026-08-30.

## Verified result

The finite-grid frontier reproduces its disclosed four-state hand model and
reports the intended tested set:

```text
tested q-by-A candidates       20
feasible candidates             7
nondominated feasible points    6
minimum tested feasible q       25/28
```

The strict frontier parser, normalized four-file replay, report contract, and
exit taxonomy passed in both Debug and Release. The full strict MSVC project
suite passed `51/51` tests in each configuration at frontier closure and
`54/54` after the priority-cap module shared its internal work-count helper.

An independent oracle enumerated the fixture polytope's 11 vertices and found
zero discrepancies over 228 candidate terms, 5,472 endpoint witnesses, and
62,816 assertions. This is strong floating-point and software evidence for the
tested synthetic domain. It is not a proof of economic calibration, a price,
or evidence that any investor committed capital.

The implementation now enforces the combined structural-work bound

```text
C * [S * (S + E + 1)
     + R
     + N * S * (H + 1)
     + 2 * S * (H + 1)] <= 4,000,000
```

Here `C`, `N`, `S`, `E`, and `H` are candidate, project, scenario, event, and
horizon-month counts. `R` counts all cash-availability, draw, receipt,
pool-cost, cash-source, and factor-tag records, plus each configured portfolio
loss layer applied to each scenario. Overflow-safe integer arithmetic runs
before evaluating a candidate. The fixture reports
`R=36`, 720 probability-projection units, and 8,720 cash-path units, or 9,440
combined. A 64-scenario, 1,024-candidate regression fails because its
probability component alone is 4,259,840. A separate one-scenario, 2,400-month
regression has only 2,048 probability units but 9,843,712 cash-path units and
also fails closed. A one-scenario, 3,909-record regression separately rejects
4,107,264 combined units. This proxy bounds the principal repeated work of the
grid; it is not an economic limit or a run-time guarantee.

## What was verified

The implementation under test is the
[frontier core](../src/robust_capital_mobilization_frontier.cpp), its
[public interface](../include/naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp),
the [closed-schema term parser](../src/robust_capital_mobilization_frontier_config.cpp),
and the
[human-readable command-line report](../apps/capital_mobilization_frontier_cli/main.cpp).
The economic definitions are in the
[frontier term](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_1.md).

For a fixed aggregate project commitment `K`, version 0.1 varies only:

```text
q = contingent share of already-declared scalable non-principal success cash
A = funded junior first-loss amount
M = K - A = funded market-claim principal notional
```

Every tested `(q,A)` creates exactly two contiguous, fully funded claims:

```text
funded junior loss-absorbing claim   [0,A]
market-facing priority claim         [A,K]
```

The two subscription notionals sum to `K`. Principal cash pays the market
claim first; principal loss attaches junior first. The market claim receives
selected non-principal cash only up to one fixed lifetime cap, after which the
junior claim receives the residual. Changing `q` or `A` does not change project
draws, recovery, gross loss, scenario definitions, or probability bounds.

Pool costs are not financed inside the displayed notionals. They are
**additional pro-rata calls** to both claimholders. Consequently, market
notional `M` is not total investor cash contributed. The report separately
publishes expected contributions and distributions, and NPV includes these
cost calls. Principal-loss and NPV-shortfall fractions use funded principal
notional `M`, not all-in cash contributed.

## Exact synthetic fixture

The reloadable fixture is in
[`scenarios/capital-mobilization-frontier-v0.1-synthetic`](../scenarios/capital-mobilization-frontier-v0.1-synthetic/).
All amounts below are invented `DEMO` millions.

There are two projects, each with commitment 10, so `K=20`. Both draw fully at
month zero. The pool also calls 0.2 of cost pro rata to the claim notionals. A
successful project pays 10 of principal plus `4q` of eligible non-principal
cash at month 24. A failed project recovers 2 of principal at month 12. The
pool and both claims use zero physical-measure hurdles in this fixture.

| Joint state | Central probability | Pool NPV |
|---|---:|---:|
| Common success | 0.62 | `-0.2 + 8q` |
| Culture loss / scale-up success | 0.18 | `-8.2 + 4q` |
| Culture success / scale-up loss | 0.18 | `-8.2 + 4q` |
| Common loss | 0.02 | `-16.2` |

The event candidate set constrains:

```text
common-loss probability                  0.01 <= c <= 0.10
each individual-project impairment       0.12 <= c+x, c+y <= 0.30
any-project impairment                   0.30 <= c+x+y <= 0.50
all four state probabilities             non-negative and summing to one
```

The polytope has 11 independently enumerated vertices. One adverse witness
places 10% on common loss, 40% across the two single-loss states, and 50% on
common success. It gives the aggregate lower endpoint:

```text
robust aggregate NPV(q) = -5 + 5.6q
robust aggregate NPV >= 0  iff  q >= 25/28
```

## Two-claim hand reconciliation

The market claim has a fixed lifetime non-principal priority cap of 1. For
`q>=0.25` and `8<=A<=16`, at least one successful project fills that cap in
every state except common loss. Because the market investor contributes
notional `M=20-A` plus an additional cost call equal to 1% of `M` (its pro-rata
share of the 0.2 pool cost), its scenario NPVs are:

```text
any state except common loss:  1 - 0.01M = 0.8 + 0.01A
common loss:                   4 - 1.01M = 1.01A - 16.2
```

The common-loss state is the lower outcome over this range. At its admitted
10% upper probability:

```text
robust market NPV(A) = 0.9(0.8 + 0.01A)
                       + 0.1(1.01A - 16.2)
                     = 0.11A - 0.90

robust market NPV >= 0  iff  A >= 90/11
```

There exists a feasible probability witness that minimizes aggregate, market,
and junior NPV simultaneously in this fixture. Additivity therefore gives:

```text
robust junior NPV(q,A) = -4.1 + 5.6q - 0.11A

zero-target catalytic concession
    = max(0, 4.1 - 5.6q + 0.11A)
```

This does not require the implementation to select identical witnesses for
all three endpoints; a linear objective may have more than one minimizing
vertex. Every reported endpoint retains and reconciles its own witness.

When `A<16`, only common loss writes down market principal. Loss amount in
that state is `16-A`, while market notional is `20-A`. Because common loss may
occupy 10% of the candidate probability and all of either tail:

```text
worst expected principal-loss fraction
    = 0.10(16-A)/(20-A)

worst principal-loss ES95 fraction
    = worst principal-loss ES99 fraction
    = (16-A)/(20-A)

worst principal-impairment probability = 10% for A<16; 0% at A=16
```

The common-loss market NPV is negative throughout this range, hence:

```text
worst negative-NPV probability = 10%

worst NPV-shortfall ES95 amount
    = worst NPV-shortfall ES99 amount
    = 16.2 - 1.01A
```

The fixture's market expected-principal-cash and WAL identities were also
checked. If `e` is any-project impairment probability and `c` is common-loss
probability, then:

```text
E[Q] = M - (M-4)c

WAL = [2M - 2e - 2(M-3)c] / [M - (M-4)c]
```

Each ratio endpoint uses one common probability measure for its numerator and
denominator. The engine does not divide separately optimized endpoints.

## Hand boundary table

The declared mandate requires robust aggregate NPV and robust market NPV
margin to be non-negative; expected principal loss no greater than 5%; both
principal-loss tails no greater than 50%; impairment and negative-NPV
probabilities no greater than 10%; both NPV-shortfall tails no greater than
51%; WAL no greater than two years; `A<=14`; and catalytic concession no
greater than 0.42.

Under those constraints, the loss limits imply `A>=12`. The concession limit
adds the tested boundary:

```text
q >= max(25/28, (368 + 11A)/560)
```

The analytical points below reconcile the implementation:

| `A` | `M` | `q` | Robust pool NPV | Robust market NPV | Robust junior NPV | Concession | Worst expected principal loss | Principal-loss ES95 = ES99 | Worst negative-NPV probability | NPV-shortfall ES95 = ES99 (amount / % of `M`) |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `90/11` | `130/11` | `25/28` | 0.00 | 0.00 | 0.00 | 0.00 | 6.615% | 66.154% | 10% | 7.936 / 67.154% of `M` |
| 12 | 8 | `25/28` | 0.00 | 0.42 | -0.42 | 0.42 | 5.000% | 50.000% | 10% | 4.080 / 51.000% of `M` |
| 14 | 6 | `261/280` | 0.22 | 0.64 | -0.42 | 0.42 | 3.333% | 33.333% | 10% | 2.060 / 34.333% of `M` |
| 16 | 4 | `34/35` | 0.44 | 0.86 | -0.42 | 0.42 | 0.000% | 0.000% | 10% | 0.040 / 1.000% of `M` |

The first row fails the declared loss limits. The next two rows are feasible
boundary points. The last row fails the declared `A<=14` mandate even though
it eliminates modeled market principal loss.

The `A=16` case is an important anti-overstatement check. In common loss the
market claim receives its entire 4.00 principal, but the investor also pays a
0.04 pro-rata pool-cost call. Its NPV in that state is therefore `-0.04`.
There is no modeled principal write-down, yet negative-NPV probability may
still reach 10% and NPV-shortfall ES95/99 remains 0.04, or 1% of market
notional. The claim is not risk-free or capital guaranteed.

## Complete fixture frontier

The evaluator sorts the deliberately unsorted input grids into:

```text
q = 0.25, 25/28, 261/280, 271/280, 34/35
A = 90/11, 12, 14, 16
```

Their Cartesian product contains 20 candidates. In q-major, A-minor order,
the feasible indices are:

```text
5, 9, 10, 13, 14, 17, 18
```

The nondominated feasible indices are:

```text
5, 9, 10, 13, 14, 18
```

Candidate 17, `(q,A)=(34/35,12)`, is dominated because candidate 13 uses the
same `A`, already reduces catalytic concession to zero, has the same market
risk at that `A`, and requires less success participation, `q=271/280`. No
weighted score selects among the remaining trade-offs. The minimum tested
feasible participation is `25/28`, and the least tested feasible first loss at
that q is 12.

These are statements about the declared grid only. The engine neither
interpolates nor claims a continuous or global optimum.

## Independent numerical oracle

A one-time independent audit used deterministic seed
`0xF20A17C0FFEE1234`. It did not use the production simplex, expected-shortfall
optimizer, WAL ratio solver, candidate evaluator, or dominance function as its
oracle. It independently:

- enumerated the four-state event polytope's 11 vertices;
- constructed project cash and the two-claim waterfall directly;
- evaluated terms below and above the `q=0.25` non-principal-cap regime and
  across the first-loss boundaries;
- checked randomized q/A grids, declared constraint subsets, feasibility,
  least-A selection, minimum feasible q, and nondominance; and
- challenged invalid and near-boundary inputs.

The audit covered:

| Audit item | Count or maximum |
|---|---:|
| Independently enumerated fixture vertices | 11 |
| Candidate terms | 228 |
| Directly checked endpoint witnesses | 5,472 |
| Value, witness, feasibility, selection, and boundary assertions | 62,816 |
| Discrepancies | 0 |
| Maximum absolute scalar difference | `5.8207660913467407e-10` |
| Maximum direct endpoint-witness objective reconciliation difference | `6.0396132539608516e-14` |

The maximum scalar difference occurred in robust market NPV margin with a
near-minimum permitted market notional. Division by that small denominator
amplifies an otherwise small money difference; it remained within the
scale-aware comparison. No witness failed probability feasibility or direct
objective reconciliation.

After the exposure fields and complete record-aware structural-work guard were
added, and again after the priority-cap module extracted their overflow-safe
calculation into a shared internal helper, the retained oracle was freshly
rebuilt against the current production library in strict Debug and Release.
Every total and both maximum errors above remained exactly unchanged. A
separate direct guard harness independently
calculated all public work fields and tested horizon, record-cardinality,
factor-tag, and scenario-expanded loss-layer cases:

```text
canonical records: cash / auxiliary / R                 28 / 8 / 36  matched
canonical work: probability / cash path / total    288 / 3,488 / 3,776  matched
20-cell work: probability / cash path / total      720 / 8,720 / 9,440  matched
H=239 combined work                                   3,989,504  accepted
H=240 combined work                                   4,005,888  rejected
R=3,804 combined work                                 3,999,744  accepted
R=3,805 combined work                                 4,000,768  rejected
128 layers * 4 scenarios combined work                4,366,336  rejected
published limit                                        4,000,000
guard discrepancies                                             0
```

The horizon boundary used 1,024 candidates, two projects, four scenarios, and
no events. The record boundary used 1,024 candidates and one scenario; its
probability component was only 2,048 in both cases. The loss-layer test also
confirmed that each configured layer is counted once per scenario, rather than
only once for the portfolio. The former probability-only guard would have
accepted every over-limit example in this paragraph.

This oracle is independent implementation evidence, not a symbolic proof. It
is deliberately low-dimensional: the probability vertex enumeration uses the
four-state synthetic fixture, and the randomized terms do not establish
behavior for every larger portfolio or event topology. It does not test
empirical probabilities, investor behavior, legal cash-flow rights, pricing,
tax, accounting, regulation, or operational data. On MSVC, `long double` does
not provide wider precision than `double`. The retained independent oracle
does not separately audit the two newly surfaced contribution/distribution
report fields; their 8.08 and 8.50/8.90/8.95 hand values and endpoint witnesses
are covered by the production core and CLI regressions.

## Parser, report, and replay controls

The [parser tests](../tests/robust_capital_mobilization_frontier_config_tests.cpp)
verify a closed, reloadable schema. Unknown, duplicate, and missing keys;
non-finite values; duplicate grid cells; invalid `none` spellings; unsafe
identifiers; surrounding whitespace; embedded BOMs; overlong lines; stream
failures; control-character injection; oversized grids; and sub-unit claim
amounts fail closed. A single leading UTF-8 BOM is accepted. Grid values are
canonicalized into ascending order, while the exact lowercase literal `none`
preserves an intentionally omitted mandate.

The
[CLI regression](../tests/check_capital_mobilization_frontier_cli.cmake)
checks the analysis basis, all fixed terms, the reported 20-candidate count,
representative candidate rows, all twelve mandate decisions at the boundary
point, feasible and nondominated selections, separate endpoint witnesses, tail
mass, WAL common-measure witnesses, audit residuals, source notes, and
interpretation boundaries. A frontier with no feasible candidate remains a
valid economic result and exits successfully.

With `--print-normalized`, the CLI prints all four complete inputs:

1. portfolio cash-path configuration;
2. event-probability-polytope configuration;
3. success-participation configuration; and
4. frontier terms and mandate configuration.

The regression extracts those renderings, reloads them, reruns the frontier,
and requires byte-stable normalized frontier output. This verifies semantic
round-trip stability; it does not turn synthetic inputs into calibrated data.

The command-line exit taxonomy is explicit:

| Exit code | Meaning |
|---:|---|
| 0 | analysis completed, including the valid case of no feasible cells |
| 1 | command grammar or option error |
| 2 | file loading or single-file parsing/configuration error |
| 3 | cross-input validation or analysis error |

Every failure path tested by the CLI retains
`calibrated_execution_authorized=false`.

## Numerical and resource boundaries

Mandate-pass and nondominance comparisons use the same disclosed tolerance:

```text
tol(a,b) = 1e-10
         + 256 * epsilon * max(1, abs(a), abs(b))
```

Accordingly, a value within `tol` of a threshold may pass, and two values
within `tol` are not treated as strictly different for dominance. This is a
floating-point comparison rule, not an economic materiality threshold.

The implemented frontier controls also require:

- at most 1,024 declared `(q,A)` candidates;
- unique, finite grid values;
- `q` in `[0,1]`; and
- both generated claim notionals, `A` and `K-A`, to be at least `1e-6` in a
  million-unit configuration—one base currency unit.

The one-unit floor prevents numerically meaningless generated claims. It is
not a recommended minimum investment size.

The combined work bound is implemented and reported. With `C` candidates, `N`
projects, `S` scenarios, `E` events, horizon month `H`, and `R` portfolio
records, it is:

```text
C * [S * (S + E + 1)
     + R
     + N * S * (H + 1)
     + 2 * S * (H + 1)] <= 4,000,000
```

The [frontier core](../src/robust_capital_mobilization_frontier.cpp) calculates
the probability-projection, portfolio-record, project-scenario-month, and
two-claim scenario-month terms with overflow-safe integer comparisons after
validating the probability set and before generating any candidate waterfall.
The summary and CLI publish the cash and auxiliary constituents of `R`, both
work subtotals, their combined value, and the 4,000,000-unit limit. Core
regressions verify probability-heavy, low-state long-horizon, and low-state
high-record rejections. The last two are cases the former probability-only
proxy would have missed. This is a deterministic structural proxy; it neither
predicts elapsed time nor replaces the lower-level tableau, pivot, and
scenario-work guards.

## Strict build and test record

The project was compiled as C++20 with MSVC under `/W4 /WX /permissive-`.
Debug and Release were built and tested independently. The frontier-specific
tests are:

- `cellular_finance_robust_capital_mobilization_frontier_tests`;
- `cellular_finance_robust_capital_mobilization_frontier_config_tests`; and
- `cf_frontier_cli_synthetic`.

The frontier closure record and the current shared-helper retention record are:

| Record | Configuration | Strict build | CTest result | Elapsed test time |
|---|---|:---:|---:|---:|
| Frontier closure | Debug | passed | 51/51 | 17.52 s |
| Frontier closure | Release | passed | 51/51 | 9.97 s |
| Current shared-helper retention | Debug | passed | 54/54 | 20.67 s |
| Current shared-helper retention | Release | passed | 54/54 | 10.20 s |

The first attempt used a long disposable build directory and hit Windows'
260-character path limit in an MSBuild tracking-log (`.tlog`) path. That
environmental path failure is not counted as a source build or test result.
The same source was then built in a short, isolated disposable directory; the
strict Debug and Release results above are from that passing build.

## Interpretation boundaries

All reported probabilities and expectations are under the supplied physical
candidate set. In this record:

- expected principal loss means modeled physical expected principal
  write-down; it is not IFRS 9 expected credit loss, Basel regulatory expected
  loss, an accounting impairment conclusion, or legal default;
- principal impairment probability means the probability of a modeled
  principal write-down, not an accounting or contractual classification;
- NPV means expected physical cash discounted at a declared hurdle, not fair
  value, a quoted price, expected investor return, risk-neutral valuation, or
  an accounting measurement;
- ES95 and ES99 are physical deal-horizon tail averages over the stated
  candidate set, not Basel's regulatory market-risk expected-shortfall
  measure;
- catalytic NPV concession is a modeled gap to one declared hurdle-based
  target, not automatically a subsidy, grant element, or budgetary cost;
- `q` is contingent success-cash participation, not a coupon, yield, equity
  ownership percentage, or promised payment; and
- a feasible or nondominated tested cell is not a security price, rating,
  suitability finding, offer, recommendation, investor commitment, or proof
  of actual capital mobilization.

The junior layer is funded loss-absorbing capital in this model, not an
unfunded guarantee or insurance policy. The market layer is senior only within
the modeled waterfall; this does not establish a debt characterization,
security interest, enforceability, bankruptcy remoteness, or regulatory
priority.

## Residual limitations

Version 0.1 remains a synthetic structural research tool. It does not model or
validate reserve custody and yield, subscription default, fees, taxes,
liquidity, secondary trading, coupons, scheduled amortization, external
guarantees, replenishment, legal remedies, accounting classification,
regulatory capital, rating methodology, market spread, risk premium,
risk-neutral probabilities, empirical calibration, investor demand, or a
causal mobilization counterfactual.

The verified fixture is intentionally small and transparent. Moving toward a
real instrument requires evidence-bound project cash interfaces and event
bounds, market-reference pricing, legal terms, identified capital providers,
and documented investor decisions. None of those later tasks may be inferred
from the passing arithmetic in this record.
