# Event-Probability Polytope v0.2 Verification

**Verification date:** 2026-08-30  
**Input status:** synthetic candidate constraints only  
**Linear checkpoint status:** implemented, independently audited, and passing  
**Tail and capital-stack status:** implemented, independently challenged, and passing

## Claim under test

The v0.2 projector intersects the existing scenario-level probability box and
unit-mass constraint with explicit lower and upper bounds on named sets of the
same fixed joint cash-flow scenarios. For a finite linear scenario metric it
returns audited floating-point minimum and maximum values, the declared central
value, and a complete endpoint probability vector in canonical scenario order.
For a finite loss metric it also returns event-constrained upper ES endpoints
with full-measure and fractional-tail witnesses. Those same fixed paths and
candidate probabilities can be routed through the funded capital stack to
re-project tranche expectations, tails, target results, and common-measure
principal weighted-average life.

This record does not call the result an exact or independently certified LP
optimum. The implementation checks primal constraints, directly recomputes the
objective, and inspects the final simplex tableau's reduced-cost condition. It
does not reconstruct a separate dual witness or publish a primal-dual gap.

The first linear checkpoint intentionally withheld event-constrained ES95,
ES99, and capital-stack results. The completed v0.2 integration now supplies
the separate tail optimizers and common-witness weighted-average-life
calculation that those outputs require; it does not substitute expectation
endpoints for them.

## Build environment

| Item | Verified value |
|---|---|
| Platform | Windows x64 |
| Generator | Visual Studio 17 2022 |
| Compiler | MSVC 19.44.35227.0 |
| Windows SDK | 10.0.26100.0 |
| CMake | 4.3.3 |
| Language | C++20, extensions disabled |
| Compile policy | `/W4 /WX /permissive-` |
| Configurations | Debug and Release |

The linear integration added three registered tests to the prior 42-test
inventory. That historical checkpoint suite was:

| Configuration | Result | Tests | CTest wall time |
|---|---:|---:|---:|
| Debug | passed | 45/45 | 25.47 s |
| Release | passed | 45/45 | 9.17 s |

The earlier 42/42 records remain historical v0.1 checkpoints; they are not
silently rewritten as if the event-polytope code existed then.

The completed tail, common-tail attribution, capital-stack bridge, and CLI
integration added three more registered tests. A fresh strict full build and
suite run produced:

| Configuration | Result | Tests | CTest wall time |
|---|---:|---:|---:|
| Debug | passed | 48/48 | 19.30 s |
| Release | passed | 48/48 | 12.57 s |

Both configurations compiled the full project with warnings treated as
errors. The 45/45 table remains above as an honest historical checkpoint, not
as the current completion count.

## Implemented controls

The core test covers:

- a hand-solved four-atom system with overlapping marginal and common-shock
  events;
- complete minimum and maximum witnesses and direct objective reconciliation;
- invariant values under scenario, event, member, and objective input
  permutations;
- signed and constant objectives and degenerate optimal faces;
- exact event-free delegation to the v0.1 box-simplex values and selected
  witnesses;
- singleton-event equivalence to component probability bounds;
- a 10,000-scenario event-free regression;
- invalid central weights, incomplete or duplicate taxonomies, non-finite
  values, unsafe or duplicate event definitions, unknown members, full-set
  events, duplicated membership sets, and incoherent event bounds; and
- the event-mode scenario and dense-tableau resource guards.

The strict configuration test covers unknown, missing, and duplicate keys;
canonical finite decimals and booleans; UTF-8 BOM placement; bounded text,
line, file, count, and membership sizes; canonical sorting; caller stream-state
restoration; and byte-stable print-load-print replay. The normalized output is
a reloadable semantic configuration, not an evidence hash binding.

The tail controls cover ES95, ES99, `tau=1`, signed and constant values, exact
ties, scenario permutations, the event-free delegate, full and fractional-tail
witness reconciliation, minimum-threshold enumeration, lifted maximum-ES
solutions, tail resource guards, and project contributions that add back to
the pool tail under the same witness. The capital-stack controls preserve a
fixed participation share and fixed waterfall paths, then independently
re-project pool and tranche linear metrics, four loss or shortfall tails,
robust target results, and common-measure WAL. They also cover an unavailable
ratio denominator and ratio endpoint witness reconciliation.

The probability CLI regression covers v0.2 success, grammar-error, and
analysis-error exits; required pool and project linear rows; both pool tails;
common-tail attribution; named events; complete full and tail witnesses;
residual fields; normalization replay; and the synthetic and physical-measure
warnings. The capital-stack CLI regression covers the corresponding event
mode while preserving both the legacy positional mode and direct-cohort mode.

## Hand-reconciled synthetic fixture

Let `c` be the common-loss probability, `x` culture-only impairment, `y`
scale-up-only impairment, and `s` common success. The fixture declares:

```text
0.01 <= c <= 0.10
0.12 <= c + x <= 0.30
0.12 <= c + y <= 0.30
0.30 <= c + x + y <= 0.50
c + x + y + s = 1
```

The central measure is `(c,x,y,s)=(0.02,0.18,0.18,0.62)`. Component scenario
bounds are deliberately broad, so the event slabs—not hidden atom bounds—drive
the informative ranges.

Each single-project impairment loses 8; common loss loses 16. Therefore pool
expected loss is

```text
8(x + y) + 16c = 8(c + x + y) + 8c.
```

The lower endpoint combines 30% any-project impairment with 1% common loss,
giving `8(0.30)+8(0.01)=2.48`. The upper endpoint combines 50% any-project
impairment with 10% common loss, giving `8(0.50)+8(0.10)=4.80`. The central
value is `3.20`.

The complete CLI table reconciled as follows:

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Pool receipts | 19.40 | 21.60 | 22.59 |
| Pool resolved principal loss | 2.48 | 3.20 | 4.80 |
| Pool impairment probability | 30% | 38% | 50% |
| Pool NPV at declared physical hurdle | -0.80 | 1.40 | 2.39 |
| Each project impairment probability | 12% | 20% | 30% |
| Each project resolved principal loss | 0.96 | 1.60 | 2.40 |
| Each project NPV before pool costs | -0.30 | 0.80 | 1.68 |

Draws, pool costs, and the three reported liquidity metrics are constant across
these four fixed paths. Probability constraints therefore cannot change them.

Pool loss values are `0,8,8,16`. At tail mass `0.05`, any full measure with
common-loss probability `c<0.05` has:

```text
ES95 = [16c + 8(0.05-c)] / 0.05 = 8 + 160c.
```

The admissible `0.01<=c<=0.10` therefore gives `9.60` at the minimum,
`11.20` at the declared central `c=0.02`, and `16.00` at the maximum. At tail
mass `0.01`, every feasible measure has at least enough common-loss mass to
fill the tail, so ES99 is `16.00` throughout.

Pool NPV shortfall values are `0,5.2,5.2,16.2`. The corresponding audited
tail table is:

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Pool resolved-principal-loss ES95 | 9.60 | 11.20 | 16.00 |
| Pool resolved-principal-loss ES99 | 16.00 | 16.00 | 16.00 |
| Pool NPV-shortfall ES95 | 7.40 | 9.60 | 16.20 |
| Pool NPV-shortfall ES99 | 16.20 | 16.20 | 16.20 |

At the central ES95 tail, common loss contributes mass `0.02` and the two
equal single-loss states each contribute `0.015`. Each project therefore
contributes `5.60` and the two rows add to pool ES `11.20`. At ES99 each
project contributes `8.00`, adding to `16.00`. The selected minimum-ES95 full
measure is non-unique and can yield asymmetric project contributions; the
report retains that one common witness and does not call either row a separate
project minimum.

Across the full fixture report, the largest published endpoint constraint
violation was `8.326673e-17`, the largest direct-objective reconciliation error
was `9.992007e-16`, and the largest reported final-tableau reduced-cost
violation was zero.

The event layer is not cosmetic. The earlier component-only fixture permits
each project's impairment probability to range from 11% to 35%. Explicit
project-impairment event bounds narrow that range to 12%–30% without changing a
cash path. The corresponding comparison for either project is:

| Project metric | Component-only v0.1 | With explicit events v0.2 |
|---|---:|---:|
| Impairment probability | 11%–35% | 12%–30% |
| Expected resolved principal loss | 0.88–2.80 | 0.96–2.40 |
| Expected receipts | 9.15–11.79 | 9.70–11.68 |
| Expected NPV before pool costs | -0.85–1.79 | -0.30–1.68 |

This tightening comes from declared marginal information. It is not a claim
that the narrower event interval is empirically calibrated.

## Independent solver audit

A separate temporary vertex-enumeration harness compared the simplex endpoints
against independently enumerated low-dimensional feasible polytopes. It matched
both objectives in 750 deterministic random cases with three to six atoms.
The cases included overlapping events, exact event equalities, variable
component bounds, signed objectives, degeneracy, and a fully fixed measure.
The temporary harness and build products were removed after the audit; this was
an independent one-time review, not a hidden runtime dependency.

No material simplex, equality-handling, or endpoint error was found. The
single artificial-variable cleanup used by the two-phase tableau was also
reviewed and found consistent with that tableau construction.

## Independent expected-shortfall audit

A second, reproducible harness challenged the nonlinear tail implementation
without using the production simplex as its oracle. With deterministic seed
`0x5EEDC0FFEE1234AB`, it generated 80 feasible probability polytopes with
three to six states and evaluated four tail masses, producing 320 randomized
projections. The cases contained 213 overlapping event constraints, including
16 exact event equalities; 15 nonconstant tied objectives; five constant
objectives; signed values; and ten fixed component-bound faces.

For minimum ES the harness independently enumerated vertices of the base
probability polytope and evaluated discrete upper tails directly. For maximum
ES it independently enumerated vertices of a lifted ES hypograph. It also
checked the synthetic four-state fixture at tail masses `0.05` and `0.01`.

The independent values matched 322 projections as follows:

| Quantity | Largest absolute difference |
|---|---:|
| Minimum ES | `3.33e-14` |
| Central ES | `7.11e-15` |
| Maximum ES | `7.82e-14` |

All 966 endpoint and central witnesses independently passed component/event
feasibility, unit mass, `0<=y<=p`, requested tail mass, direct ES
reconciliation, and pro-rata allocation within exact-value ties. The harness
compiled under `/W4 /WX /permissive-`. Its retained source and build recipe
are outside the runtime project in the task's reproducible audit-artifact
directory; the production code has no dependency on them.

This remains a low-dimensional floating-point audit, not a symbolic proof or
dual certificate. Exhaustive six-state random cases were limited to two named
events for tractable vertex enumeration, and MSVC evaluated `long double` at
ordinary double precision.

## Capital-stack hand reconciliation

The event candidate set was then routed through the existing fixed waterfall:
first-loss residual `[0,4]`, intermediate `[4,10]` with a `2` non-principal
cap, and senior `[10,20]` with a `1` cap. The success-participation share was
held at `q=1`; project cash paths and gross pool loss were unchanged.

Let `a` be any-project impairment probability and `c` common-loss probability.
The principal-loss equations are `4a`, `4a+2c`, and `6c` for the three
tranches. Their event-constrained results were:

| Tranche metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| First-loss expected principal loss | 1.20 | 1.52 | 2.00 |
| First-loss expected NPV | -0.54 | 0.30 | 0.86 |
| Intermediate expected principal loss | 1.22 | 1.56 | 2.20 |
| Intermediate expected NPV | -0.46 | 0.34 | 0.70 |
| Senior expected principal loss | 0.06 | 0.12 | 0.60 |
| Senior expected NPV | 0.20 | 0.76 | 0.83 |

The independently hand-derived tail results were:

| Tranche metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| Intermediate principal-loss ES95 | 4.40 | 4.80 | 6.00 |
| Intermediate principal-loss ES99 | 6.00 | 6.00 | 6.00 |
| Intermediate NPV-shortfall ES95 | 2.86 | 3.66 | 6.06 |
| Intermediate NPV-shortfall ES99 | 6.06 | 6.06 | 6.06 |
| Senior principal-loss ES95 | 1.20 | 2.40 | 6.00 |
| Senior principal-loss ES99 | 6.00 | 6.00 | 6.00 |
| Senior NPV-shortfall ES95 | 1.22 | 2.44 | 6.10 |
| Senior NPV-shortfall ES99 | 6.10 | 6.10 | 6.10 |

Senior principal cash has denominator `D=10-6c` and cash-time numerator
`N=20-2a-14c`. Optimizing `N/D` under one probability measure gives
`1.8723404255 / 1.9190283401 / 1.9376257545` years. The implementation retains
the endpoint probability vector and directly reconciles `N`, `D`, the monthly
cash ledger, the ratio, and the final `N-rD` root. It never divides a minimum
numerator by a separately selected maximum denominator.

A separate regression held `q=0.37` rather than silently re-solving it. The
underlying NPV range became `-3.446 / -1.624 / -0.8041`; against a zero target
the reported robust gap was `3.446`. Another regression withheld WAL when one
feasible probability measure eliminated a tranche's material expected
principal cash.

## Independent capital-stack and ratio audit

A third independent harness enumerated the hand polytope's 11 vertices with
long-double Gaussian elimination. It did not use the production simplex or
the production Dinkelbach ratio solver. It reproduced 54 linear ranges, 12
ES95/ES99 ranges, the underlying target and gap, and all three tranche WAL
ranges.

For senior WAL the independently selected minimum witness was
`(c,x,y,s)=(0.10,0.20,0.20,0.50)`, giving `1.8723404255` years. The maximum
witness was `(0.01,0.18,0.11,0.70)`, giving `1.9376257545` years. Dividing
separately optimized numerator and denominator endpoints would instead create
the false wider range `1.77062`–`2.04894`; the implementation never reports
that range.

The Release randomized challenge then covered:

- 1,000 four-state event polytopes;
- 6,000 independently checked WAL endpoints;
- 11,641 independently enumerated vertices; and
- 171,170 value, witness, feasibility, and boundary checks.

The largest reported-value difference was `1.87e-14`; the largest witness
difference was `1.78e-15`. Fixed `q=0.37`, event-free projection,
zero-denominator WAL suppression, positive-denominator availability, and the
private-ledger data-flow boundary also passed. Source review confirmed that
the private v0.1 ambiguity object supplies deterministic paths and accounting
controls only; every published financial range, tail, target conclusion, and
WAL result is recomputed by the v0.2 projector.

The independent tolerance was `2e-8` relative for reported values and
`2e-11` for vertex feasibility. This remains floating-point evidence over
synthetic four-state cases, not a symbolic proof, dual certificate,
high-dimensional guarantee, calibration result, or valuation validation.

## Corrections made during verification

The first strict parser accepted a central-weight sum within its documented
`1e-12` tolerance, while the projector compared the normalized central value
to a component bound without tolerance. A near-unit-sum configuration could
therefore print and reload but fail during projection. The core, the v0.1
event-free delegate, and both strict configuration paths now share a scaled
`64 * double epsilon` central-bound tolerance, with event and event-free
regressions.

Public wording initially called the event LP result “certified.” That was too
strong without an independently reconstructed dual witness and gap. Reports
and documents now say “audited floating-point.” Related language now states
that overlapping event membership defines the co-occurrence taxonomy and
restricts, but does not identify, dependence; numerical feasibility is not an
exact proof; and a deterministic witness is not necessarily unique or
cross-platform byte-identical.

## Numerical and resource boundary

The central weights must sum to one within `1e-12`. Central-to-bound and
central-to-portfolio comparisons use `64 * double epsilon` times the relevant
scale. A published endpoint is rejected if its maximum scenario, event, or
unit-mass violation exceeds `1e-10`. Objective and final-tableau reduced-cost
audits use `1e-9 + 256 * double epsilon * max(1, objective scale)`.

Event mode accepts at most 512 scenarios, 256 named events, 65,536 aggregate
event memberships, 2,000,000 dense long-double tableau cells, and 100,000
pivots per solve. A limit failure throws; no partial or “best so far” endpoint
is published. On the verified MSVC platform, `long double` should not be read
as a promise of precision beyond ordinary `double`.

Event-constrained ES requires tail mass of at least `1e-6`, examines at most
512 distinct exact scenario-value thresholds, and permits at most 2,000,000
pivots across the threshold and lifted solves while retaining the 100,000
per-solve limit. Tail objective, threshold, and reduced-cost audits use
`1e-8 + 1024 * double epsilon * max(1,value scale) / tau`; published
probability and tail-mass violations remain bounded by `1e-10`.

The WAL bridge reports no ratio unless the minimum feasible expected principal
cash exceeds `1e-10` million. Its common-witness root iteration is limited to
128 projections and checks both an absolute objective tolerance of `1e-9`
million-years plus scale-aware roundoff and a `1e-12`-year ratio tolerance.
Failure to converge or reconcile throws instead of returning a ratio.

These tolerances mean an accepted central vector and endpoint are numerical
witnesses. They are not symbolic proofs of an exact real-number polytope.

## Residual limitations

- Every event member is an explicitly declared joint scenario ID. The engine
  does not infer membership from a project name, factor tag, loss, or cash flow.
- The listed joint scenarios remain a finite candidate payoff family. Event
  bounds do not prove that omitted states are irrelevant.
- Separate marginal records do not identify joint dependence, and multiple
  intervals are not a simultaneous confidence region unless their construction
  establishes that coverage.
- The fixture is invented and always reports
  `calibrated_execution_authorized=false`.
- The central physical measure is a reference hypothesis, not a market pricing
  measure. No result is fair value, a quote, a rating, or an investment
  recommendation.
- Linear expectation endpoints cannot be substituted for ES, tranche-tail, or
  weighted-average-life endpoints. Those metrics now have separate optimizers
  and witnesses, but none is a price, quote, rating, or empirical calibration.

The financial interpretation and equations are in
[Event-Probability Polytope v0.2](EVENT_PROBABILITY_POLYTOPE_V0_2.md).
