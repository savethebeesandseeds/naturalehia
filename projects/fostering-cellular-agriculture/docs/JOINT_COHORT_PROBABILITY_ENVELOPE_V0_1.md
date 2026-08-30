# Joint-Cohort Probability Envelope v0.1

**Status:** implemented as a synthetic, candidate-only C++20 reference model.
Passing the executable means that the supplied package is computationally
coherent; it never authorizes calibration, pricing, a rating, or execution.
The strict build, test, hand-reconciliation, and audit record is in
[Joint-Cohort Probability Envelope v0.1 Verification](JOINT_COHORT_PROBABILITY_ENVELOPE_VERIFICATION_V0_1.md).

## Purpose

This module is the first reproducible bridge from a row-level outcome cohort to
the physical-probability set used by the participation-pool risk engine. It is a
small-pool benchmark, not a claim that cellular-agriculture risk is already
calibrated and not the final architecture for hundreds of facilities.

The financial question is narrow and useful:

> Given a fixed table of complete joint pool outcomes, what probability bounds
> can a declared cohort support, and what ranges of investor exposure, loss,
> investor cash receipts, expected pathwise liquidity peaks, project-level
> risk, tail concentration, and NPV using the declared hurdle and physical-P
> scenario weights follow from those bounds?

The cohort module never changes a project's capital draw, recovery, payoff,
timing, or contract term. It changes no cash path. It constructs a candidate
outer set for the weights of paths that already exist, then sends that set to
the existing probability-envelope engine.

Version 0.1 is hard-coded as synthetic and candidate-only. It always leaves
`calibrated_execution_authorized=false`.

## The sampling unit is the whole joint pool

One included ledger row must represent one independent and identically
distributed repetition of the **same complete joint risk unit** represented by
the portfolio's scenario table.

If a pool contains facilities A, B, and C, one row describes the synchronized
outcome vector `(A outcome, B outcome, C outcome)`. Three unrelated rows, one
from each facility, are not three observations of that joint vector. They may
help estimate project marginals, but they do not reveal simultaneous failure or
common-shock probability.

This requirement prevents an attractive but false diversification result. It
also exposes the v0.1 scalability limit. With `m` binary facility outcomes, a
complete table can contain `2^m` joint atoms. Ten facilities imply 1,024 atoms;
fourteen imply 16,384, already above the current engine's 10,000-scenario cap.
Delay, continuing, recovery, and multiple-success states increase the count
faster. Version 0.1 is therefore a reference calculation for small, explicitly
defined pools.

## Why the authoritative input is a row ledger

The tool derives counts from a strict SHA-256-bound row ledger. It does not
accept analyst-supplied aggregate counts. The package loader reads each bound
portfolio and ledger artifact into a bounded immutable byte snapshot, checks
the declared SHA-256 over that snapshot, and parses those same bytes. It then
rechecks that neither resolved path nor file contents changed during the load.
A counts-only file could hide:

- duplicated or omitted sampling units;
- outcome-dependent exclusions;
- immature or unresolved units removed from the denominator;
- repeated observations from one declared cluster treated as independent;
- a changed mapping between observed outcomes and financial scenarios; or
- a different population frame from the one described to investors.

The package also binds the exact portfolio file. A cohort cannot silently be
applied to a different scenario taxonomy or different cash paths.

Hashes establish which bytes were used. They do not establish that a row is
true, that differently labelled units are comparable, that a classification is
correct, or that the asserted complete joint units are actually IID. Those are
evidence and validation questions outside file integrity.

Each row has a stable identity, cluster, eligibility date, outcome horizon,
status, scenario classification where known, exclusion-rule identifier where
applicable, and evidence/requirement identifiers. The four dispositions are:

- `matured`: the horizon has passed and the row maps to exactly one portfolio
  scenario;
- `not-yet-matured`: the horizon is after the cohort as-of date and the eventual
  scenario is unknown;
- `unresolved`: the horizon has passed but the row has not been classified; and
- `excluded`: the row is outside the denominator under the declared frozen
  eligibility rule.

An excluded row remains visible. Exclusion outside the denominator is
statistically defensible only if the rule was frozen before outcome inspection
and is outcome-blind. Version 0.1 can verify the declared rule identifier and
file integrity; it cannot prove the analyst's intent. If that condition is not
supportable, the honest treatment is to keep the row included and compatible
with every scenario.

Repeated cluster identifiers among included rows are an obvious contradiction
of the IID claim and block the statistical export. Distinct cluster labels do
not prove independence; they only remove one visible failure of the assumption.

## Reloadable package and process contract

The top-level `cohort.cfg` declares the cohort identity, as-of date, population
frame, complete-joint-unit definition, outcome mapping, horizon, taxonomy
freeze date, confidence level, hard candidate-only flags, exclusion rules, and
the relative path and SHA-256 of both bound artifacts. Unknown, duplicate,
missing, non-finite, unsafe, or incoherent fields are rejected.

The authoritative ledger has exactly these ten tab-separated columns:

```text
observation_id  cluster_id  eligible_date  horizon_end_date  status
scenario_id  classification_date  exclusion_rule_id
evidence_record_ids  requirement_ids
```

The executable is:

```powershell
.\build\dev\Debug\naturalehia-joint-cohort-envelope.exe `
  .\scenarios\joint-cohort-v0.1-synthetic\cohort.cfg `
  --print-normalized
```

Its exit contract is fail-closed: `0` means a candidate envelope and financial
range report were computed; `1` means malformed input or a runtime failure;
`2` means incorrect command-line use; and `3` means the package was readable
but its statistical or financial export was blocked. Exit `0` is still only a
mechanical result under synthetic assumptions.

## Primary finite-sample outer set

Let:

- `K` be the number of mutually exclusive and exhaustive joint scenarios;
- `N` be all included rows, including matured, not-yet-matured, and unresolved
  rows;
- `S_u` be the set of scenarios compatible with included row `u`;
- `d_i` be the number of rows for which `S_u = {i}`; and
- `c_i` be the number of rows for which `i` is in `S_u`.

The ledger implemented in v0.1 gives a matured row one compatible scenario and
gives every not-yet-matured or unresolved row all `K` scenarios. If `x_i` is the
unknown count after every included row is eventually resolved, then:

```text
d_i <= x_i <= c_i
```

For family confidence level `1 - alpha`, define:

```text
epsilon = sqrt(log(2*K/alpha) / (2*N))

lower_i = max(0, d_i/N - epsilon)
upper_i = min(1, c_i/N + epsilon)
```

Under independent and identically distributed draws of the declared complete
joint unit, a scenario taxonomy frozen before outcome inspection, and truthful
compatible sets containing the latent outcome, Hoeffding's inequality plus a
union bound gives simultaneous coverage of at least `1 - alpha`. The result is
a **conservative nonasymptotic simultaneous outer confidence set**, conditional
on those assumptions. It is not an exact interval for each atom and it is not a
sector calibration.

Unknown outcomes remain in `N` and can occupy every scenario. This means the
calculation does not need to assume that censoring is independent of outcome;
it pays for missing outcomes by widening the set. If `epsilon >= 1`, a `[0,1]`
bound is the honest result.

The primary component bounds are intersected with the probability simplex:

```text
sum_i probability_i = 1
lower_i <= probability_i <= upper_i
```

This box-simplex is conservative. It loses the row-level fact that one unknown
unit can eventually occupy only one scenario. The simplex restores total mass,
but not every coupling among unresolved rows. Reports therefore call it an
outer envelope.

The finite-sample inequality follows the bounded-independent-variable result in
[Hoeffding (1963)](https://doi.org/10.1080/01621459.1963.10500830).

## Declared central weights are not promoted

The portfolio's existing central weights remain a declared reference
hypothesis. The cohort tool checks that each reference weight lies within the
outer set. If it does not, the tool reports a conflict and refuses to emit an
executable ambiguity configuration. It never silently moves, smooths, or adds
pseudocounts to the reference measure.

When every included outcome is complete, `x_i/N` is reported as a descriptive
empirical frequency. A zero-count state remains zero in that descriptive
measure; its upper probability bound remains positive when the data are sparse.
No pseudocount is presented as an observation. Empirical frequency is not
automatically a transferable probability for a new project or pool.

## Goodman score challenger

For a complete cohort, the tool also calculates the simultaneous multinomial
score construction proposed by Goodman as a challenger diagnostic. For count
`x_i`, complete sample size `N`, and
`B = z_(1-alpha/(2K))^2`:

```text
goodman_lower_i =
  [B + 2*x_i - sqrt(B*(B + 4*x_i*(N-x_i)/N))]
  / [2*(N+B)]

goodman_upper_i =
  [B + 2*x_i + sqrt(B*(B + 4*x_i*(N-x_i)/N))]
  / [2*(N+B)]
```

Goodman's construction is large-sample/asymptotic. Sparse and zero-count joint
states are precisely where it should not be the sole release bound. Version
0.1 therefore publishes it only beside the finite-sample outer set and never
uses it to narrow the investor-risk calculation. See
[Goodman (1965)](https://www.stat.cmu.edu/technometrics/59-69/VOL-07-02/v0702247.pdf).

## Connection to investor risk

When the declared reference measure lies inside the generated set, the existing
capped-simplex projector computes exact endpoints, within this set and for the
fixed cash paths, for:

- expected capital draws, receipts, pool costs, and outstanding exposure;
- expected terminal principal loss and impairment probability;
- expected receipts by disclosed commercial, licensing, sale, recovery,
  refinancing, support, and fee sources;
- expected NPV using the declared hurdle and physical-P scenario weights, and
  the probability of negative NPV;
- the expectation across scenarios of each path's own peak same-month draw,
  gross funding need, and cumulative net outlay; and
- principal-loss and NPV-shortfall ES95 and ES99.

It can also project scenario indicators to show ranges for each project's
impairment, pairwise simultaneous impairment, any-project impairment, and
all-project impairment. Every scalar endpoint has its own feasible probability
witness. Endpoints from different rows need not occur under one common measure
and must not be assembled into a fictitious worst portfolio.

The generic financial export now reports, for every project, exact ranges for
expected draws, receipts, outstanding principal, realized loss, NPV before
shared pool costs, impairment, and negative NPV. Pool ES95 and ES99 are also
attributed to projects under the same minimum-pool-ES, central, and
maximum-pool-ES probability and fractional-tail witnesses. Contributions in a
column add to pool ES. They are not independently optimized project maxima.

Here, project impairment means that the fixed scenario cash path ends with
strictly positive project principal loss. The expected liquidity-peak rows are
not a worst-path reserve or contractual commitment requirement; those require a
separate capacity rule over the scenario set.

The family confidence level concerns coverage of the unknown physical
probability vector under the stated sampling assumptions. `ES95`, by contrast,
is the average loss in the worst 5% of a fixed cash-loss distribution. The two
uses of “95” answer different questions and are reported separately.

Pooling may change the concentration and timing of loss. It cannot reduce the
expected underlying project loss by arithmetic or create cash that no project
generates. A positive diversification diagnostic under the declared reference
weights is not proof that diversification survives every measure feasible
within the candidate set.

These ranges describe the underlying untranched participation-pool cash paths.
They are not a note or tranche price or return. Claim-level analysis must apply
the same candidate probability set to the selected waterfall, contractual
notional, timing, costs, and issue price.

## From the cohort to an investor claim

The existing capital-stack executable now performs that claim-level step
directly:

```powershell
.\build\dev\Debug\naturalehia-capital-stack.exe --joint-cohort `
  .\scenarios\joint-cohort-v0.1-synthetic\cohort.cfg `
  .\scenarios\joint-cohort-v0.1-synthetic\success-participation.cfg `
  .\scenarios\joint-cohort-v0.1-synthetic\capital-stack.cfg `
  --print-normalized
```

The loader supplies the same SHA-256-bound portfolio object to the cohort
calculation and the waterfall. The bridge accepts no second portfolio, manual
probability file, Goodman fallback, or participation re-solve. A readable
cohort blocked by repeated clusters or an incompatible declared center exits
`3` and emits no stack. Malformed or economically incompatible terms exit `1`;
syntax errors exit `2`; a computed candidate exits `0`. Every exit retains
`calibrated_execution_authorized=false`.

Only the portfolio and authoritative raw ledger are bound by `cohort.cfg`.
Participation and stack terms are strict normalized replay inputs outside that
hash binding. Printed cohort, portfolio, and ledger normalizations are semantic
audit renderings that retain the original raw-file hashes, so they must be
rehashed before rebinding as a new package. The report says so instead of
calling the whole run hash-bound.

The project table in this integrated report is recomputed at the fixed
participation fraction selected by the stack. In a non-unit `q` run, receipts
and NPV therefore describe the cash entering the waterfall; draws, exposure,
and resolved principal loss at horizon remain unchanged. This selected view is
kept separate from the cohort's original hash-bound portfolio view.

The synthetic result is deliberately adverse. Each 10-million project has
central expected loss 2, a candidate-set expected-loss range from 0 to about
8.062, and
impairment probability from 0% to about 80.620%. At the maximum-pool-tail
witness, each contributes 10 to both pool ES95 and ES99. The pool's declared
full success participation has central NPV `-1.0` and minimum expected NPV
about `-11.949`, so it misses a zero target. Seniority reallocates this risk:
the 10-million senior claim has central expected loss `0.20`, maximum expected
loss about `5.062`, and impairment probability from 0% to about 50.620%. Its
central NPV is `0.68`, but its minimum expected NPV is about `-4.668`. The
candidate therefore fails the robust attraction test even though priority
improves the central senior result.

## What v0.1 may and may not say

Version 0.1 may say:

> Under the declared IID complete-joint-unit assumption, frozen cohort frame,
> fixed scenario mapping, and conservative treatment of unknown outcomes, these
> are candidate outer bounds on the probabilities of the supplied joint states.
> The financial ranges show the sensitivity of the supplied cash paths to those
> bounds.

It may not say that:

- cellular-agriculture probabilities or expected returns are calibrated;
- separate facility histories establish joint dependence;
- differently named facilities are independent;
- a larger or differently composed pool inherits the result;
- probability evidence validates synthetic payoff, recovery, timing, or
  contract amounts;
- physical-hurdle NPV is price, fair value, a rating, or expected market return;
  or
- adding many facilities by itself makes an asset safer.

## The scalable next model

The v0.1 benchmark should remain reproducible even after a more scalable model
exists. The next architecture is a general probability polytope with event
constraints such as:

```text
event_lower_j
    <= sum(probability_i for scenario i in event_j)
    <= event_upper_j
```

Events can represent one project's impairment, pair co-impairment, a common
bioprocess shock, stage transitions, geography, platform families, or facility
groups. This permits project-level and factor evidence without pretending that
separate marginals directly observe every complete joint atom. A general linear
optimizer can then bound the same investor metrics without enumerating every
facility-state combination.

That v0.2 model is the many-facility destination. It should be built only after
the row-population, horizon, exclusion, mapping, and financial-reconciliation
chain in this benchmark is mechanically sound.
