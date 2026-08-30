# Event-Probability Polytope v0.2

## Purpose

Version 0.1 places a lower and upper probability on every complete joint
scenario. That is a useful benchmark, but real evidence often speaks about
events instead:

- one project's impairment rate;
- two projects failing together;
- a shared bioprocess, supplier, buyer, energy, or regulatory shock; or
- a platform, geography, stage, or facility group.

Version 0.2 adds those statements without converting separate marginal records
into fictitious observations of every joint outcome.

This is financial engineering over fixed cash paths. It does not estimate a
cellular-agriculture probability, validate event membership, create an omitted
scenario, or price a security.

## Candidate probability set

For supplied joint scenarios `i = 1,...,K`, let `p_i` be the unknown physical
probability. Existing component bounds remain:

```text
lower_i <= p_i <= upper_i
sum_i p_i = 1
```

Each named event `j` is an explicit subset of those exact scenario IDs. Its
declared candidate bound adds:

```text
event_lower_j
    <= sum(p_i for scenario i in event j)
    <= event_upper_j
```

Events may overlap or nest. A culture-platform impairment event and a scale-up
impairment event can both contain the common-loss scenario. That explicit
overlap defines the co-occurrence taxonomy; together with the probability
bounds it restricts, but does not identify, dependence. It is not inferred
from project names or assumed independence.

The portfolio's declared central physical measure must match the polytope
scenario table and satisfy every component and event bound. It is a reference
hypothesis and audit checksum, not an empirical estimate. Requiring it to pass
the disclosed numerical tolerances also supplies a visible numerical
feasibility witness for the declared candidate set; it is not an exact proof.

## Explicit event taxonomy

An event definition contains:

```text
event ID
plain-language definition
lower and upper probability
explicit member scenario IDs
```

The implementation does not expand free-form factor tags, project labels, cash
outcomes, or loss amounts into event membership. A data or calibration package
may eventually construct the explicit list, but that package must bind its
portfolio and evidence separately.

Events that are empty, contain the entire scenario set, repeat a member, name
an unknown scenario, or duplicate another event's membership set are rejected.
Tight or redundant bounds do not add evidence merely because they are present.

## Current computation

The v0.2 implementation projects finite linear scenario objectives:

```text
minimum or maximum of sum_i p_i * metric_i
subject to the component, event, and unit-mass constraints
```

Examples include expected draws, receipts, outstanding exposure, resolved
principal loss at horizon, impairment probability, and physical-hurdle NPV.
Each metric has its own endpoint probability witness that passes the disclosed
numerical feasibility tolerances. Extrema from different projects or rows
cannot be assembled into one probability measure.

It also projects upper expected shortfall for a declared tail mass `tau`. For
a fixed probability measure `p` and scenario loss `L`, fractional tail mass
`y` satisfies:

```text
0 <= y_i <= p_i
sum_i y_i = tau
ES_tau(p,L) = (1/tau) max_y sum_i L_i y_i
```

The maximum feasible ES is one lifted linear program. It writes `p=y+z`,
requires `sum(y)=tau` and `sum(z)=1-tau`, applies every component and event
bound to `y+z`, and maximizes the loss carried by `y`.

The minimum feasible ES uses the equivalent threshold formula:

```text
ES_tau(p,L) = min_v [v + (1/tau) sum_i p_i max(L_i-v,0)]
```

For finite scenario losses an optimum threshold is one of the distinct loss
values. The engine enumerates those values, minimizes the hinge expectation
over the probability polytope for each, and selects the smallest audited
result. It then recomputes the canonical upper tail of the selected full
measure. If an exact equal-loss block crosses the boundary, its fractional
tail mass is allocated pro rata; no scenario inside that tie is favored by
identifier or input order.

ES95 uses `tau=0.05`; ES99 uses `tau=0.01`. These are loss-tail averages, not
confidence probabilities supplied as `0.95` or `0.99`. Each minimum, central,
and maximum result retains the full probability measure and the common
fractional tail mass. Project contributions to pool loss are calculated under
those exact same tail masses and add back to pool ES. They are not separately
optimized project-tail bounds, and a selected optimizing measure can be
non-unique.

With no event constraints, the projector delegates endpoint values and full
measures to the v0.1 box-simplex engine and adds canonical tail-mass witnesses.
That preserves its fast large-scenario path and pro-rata treatment of exact
tied objectives. With events present, a bounded deterministic two-phase
simplex solves the linear subproblems in long-double arithmetic.

The event-constrained result is an audited floating-point solution, not a
symbolic result or an independently reconstructed primal-dual certificate.
Before publication the engine recomputes:

- unit probability mass;
- every scenario and event bound;
- the objective from the published witness; and
- the final-tableau simplex reduced-cost optimality residual.

For tail results it additionally recomputes `0<=y<=p`, total tail mass, ES
directly from `y`, the selected minimum-threshold formula, and the largest
reduced-cost residual across every threshold solve. These are floating-point
audits, not a separate dual solution or proof of a primal-dual gap.

It throws rather than publishing an iteration-limited, resource-limited, or
unaudited best-so-far answer. Stable pivot rules deterministically select one
optimal witness for a given implementation and build, but an optimal face can
contain others and floating behavior can vary by platform; witness uniqueness
or cross-platform byte identity is not claimed.

## Strict input and replay boundary

The strict `polytope.model_version=0.2.0` companion file declares the complete
scenario box and all event subsets. Unknown, duplicate, missing, non-finite,
unsafe, oversized, or internally inconsistent inputs fail closed. Normalized
output sorts scenarios, events, and event members and is reloadable as a
semantic candidate-set input.

The companion file contains no raw-evidence hash binding. A successful parse
or solve therefore means only that the supplied candidate set is coherent with
the supplied portfolio. It never means
`calibrated_execution_authorized=true`.

## What this improves—and what it does not

The event polytope can express marginal and shared-factor restrictions without
pretending that each restriction is a direct bound on one complete atom. It
also exposes which financial result is sensitive to which overlapping event
constraints.

The current payoff engine still requires a finite list of complete joint cash
scenarios. Event constraints improve the probability model; they do not yet
eliminate enumeration of those payoff paths, prove that omitted states are
irrelevant, or establish joint dependence from separate facility histories.
Multiple event intervals are not a simultaneous confidence set unless their
construction establishes that coverage.

## Capital-stack routing

The same candidate set can now enter the fully funded capital-stack evaluator.
The declared success-participation fraction `q` is held fixed. The existing
waterfall is used only to construct and reconcile deterministic scenario cash
paths; none of its private v0.1 probability ranges or target conclusions are
reused. Every pool and tranche expectation, impairment/exhaustion probability,
NPV range, loss ES95/ES99, NPV-shortfall ES95/ES99, robust target result, and
target gap is re-projected through the event polytope.

Principal cash-weighted average life is a ratio under one probability measure,
not a ratio of unrelated numerator and denominator endpoints:

```text
WAL(p) = sum_i p_i * principal_cash_time_i
         / sum_i p_i * principal_cash_i
```

The engine solves each ratio endpoint by repeated linear projections of
`numerator - r * denominator`, retains the final common probability witness,
and reconciles its numerator, denominator, monthly cash ledger, ratio, and root
residual. WAL is withheld if any feasible measure can reduce expected
principal cash to the declared numerical floor.

This routing makes the candidate physical-risk set usable for instrument
design; it does not make the declared terms valuable. In particular, the
module does not estimate a security price, spread, risk-neutral measure,
rating, regulatory capital charge, legal enforceability, or empirical event
probability. Those remain research, calibration, and market-validation work,
not management gates hidden inside the solver.
