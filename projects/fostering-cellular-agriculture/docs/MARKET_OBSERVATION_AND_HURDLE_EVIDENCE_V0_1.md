# Market Observation and Hurdle Evidence Set v0.1

> **Implementation status:** implemented and synthetically verified. The C++20
> core, strict canonical configuration, CLI, and exact disjoint-set fixture are
> operational. The first two retained real public packages—Liberation Labs'
> October 2024 facility note and Solar Foods' May 2022 amortizing guaranteed
> facility—both fail the protocol. Solar adds audited repayment reconciliation,
> but neither package exposes the complete price, claim, expected cash,
> recovery, and comparison bridge. No real transaction has passed, so no
> empirical hurdle set is released.

## Purpose

This specification answers one narrow financial question:

> Which expected-cash discount-rate cases are not ruled out by evidence that
> is independent of the target claim's proposed issue price?

The answer is a set of model-conditioned rates consistent with declared price
observations and explicit comparability adjustments. A transaction observes a
price, not the buyer's private required return or beliefs. The rate also
depends on an independently declared physical cash model. It is not a point
estimate, an average of private transactions, a fair value, a benchmark, or
proof of investor demand.

The target is the fixed market claim already produced by the capital-stack,
priority-cap, and issue-price terms. Its dated state-contingent cash flows and
physical-probability set do not change here. The evidence set supplies possible
values of `h` to the
[Robust Issue-Price Support Term v0.1](ROBUST_ISSUE_PRICE_SUPPORT_TERM_V0_1.md):

```text
robust buyer price ceiling at h
    = M - buyer-direct cost
      + minimum physical expected NPV of the par claim at h
```

This closes a real gap in the instrument. The issue-price engine can already
say what price a specified hurdle permits; it cannot legitimately invent the
hurdle.

## The rate must be on the right economic basis

The implemented issue-price term discounts cash that is already contingent on
project loss, delay, and recovery and then takes physical-probability
expectations. Its hurdle is therefore an annual effective discount rate on
**physical expected cash after loss and timing**. Gross buyer price and
buyer-direct transaction costs remain separate, but both are investor cash
outflows in the all-in return test.

A coupon, promised yield, yield to maturity, or contractual IRR is not the same
quantity. A risky claim's promised yield commonly contains compensation for
cash that may never be paid. Applying that yield to cash flows that have
already been reduced for physical loss can count expected loss twice.

For observation `j`, let `P_j` be gross cash paid for the claim, `C_j` be
separately evidenced buyer-direct costs, and `CF_other,j,s,t` be every other
same-day or later claim call and distribution in state `s`. It excludes only
`P_j` and `C_j`. On its own observation date, the robust basis matching the
target engine reconstructs price through:

```text
0 = minimum over p in Q_j of sum_s p_s [
      -P_j - C_j
      + sum_t>=0 CF_other,j,s,t / (1+r_j)^(elapsed_months_t/12)
]
```

Calls are negative and distributions are positive. `P_j` and `C_j` are actual
observation-date cash or evidenced observation-date equivalents. If execution,
settlement, or cost payment occurs later, the external ledger must date that
cash inside the discounted sum or normalize the settlement lag explicitly; it
may not treat a later payment as time zero without an evidenced bridge.

`Q_j` is that claim's independently declared physical probability set, not the
target price or a risk-neutral probability reverse-engineered from it. A fixed
central distribution is a different convention and must remain labelled or be
bridged explicitly. If the price preimage has no root, multiple components, or
only roots outside the declared domain, the external record preserves that
result; v0.1 does not force it into one convenient interval.

An observation is eligible for the primary evidence set only when its rate
preimage is one exact connected interval on this basis. The external
calculation must retain the observed price, buyer cost, full dated scenario
cash—including every other month-zero call—probability-set and recovery
methods, source hashes, solver method, and NPV-reconstruction residual in an
immutable result record. A promised-cash yield can remain in the report as a
comparison, but it cannot silently become `h`.

Each observation is first solved on its own observation date and in its native
currency. Transfer to the target date and currency requires identified
benchmark curves and, where relevant, the actual FX-forward, cross-currency-
basis, and hedge cash flows. Spot conversion alone cannot turn one currency's
discount-equivalent rate into another's, and a stale trade is never
mechanically rolled forward as if no market information changed. Observations
after the target decision date are backtest evidence only; no date adjustment
may introduce hindsight into an ex-ante set.

Likewise, a rate solved from the target reference price and the target cash
flows merely reconciles that price. It is circular evidence for deciding
whether the same price is adequate and is excluded from the primary set. A
named investor's independently approved target may be tested directly in the
issue-price engine, but it remains an investor mandate, not a market-observation
consensus.

## Observation hierarchy

The evidence hierarchy is multidimensional rather than a single status label:

1. A settled, orderly, arm's-length transaction in the identical full claim,
   on the required price and return basis, is direct **price** evidence.
2. A settled comparable claim may enter only through a complete, bounded
   adjustment bridge.
3. A contemporaneous executable two-sided quote may enter only when its market,
   validity period, size, and transaction anchor are evidenced.
4. Executed but unsettled terms, one-sided quotes, nonbinding indications,
   model marks, investor targets, policy targets, and synthetic cases remain
   visible sensitivities but do not identify the primary set in v0.1.

A stale, forced, related-party, distressed, odd-size, or side-rights transaction
can be less useful than a current firm quote. Status never overrides economic
comparability.

An executable quote retains the full same-size, same-time, same-currency price
interval with `bid<=ask`; v0.1 never substitutes its midpoint. The external
normalization maps both sides through the complete rate preimage with
side-consistent buyer costs. Under a separately proved strictly decreasing PV
function, `[P_bid,P_ask]` maps in reverse order to
`[r(P_ask),r(P_bid)]`. A one-sided quote supplies only a half-bound and remains
a sensitivity in v0.1.

The hierarchy borrows evidence discipline from primary standards without
claiming to create a regulated benchmark or an accounting valuation. The
[final IOSCO benchmark principles](https://www.iosco.org/library/pubdocs/pdf/IOSCOPD415.pdf)
give priority to observable arm's-length transactions and require a clear
hierarchy when transaction data are insufficient. The consolidated
[EU Benchmark Regulation](https://eur-lex.europa.eu/legal-content/EN/TXT/PDF/?uri=CELEX:02016R1011-20260101)
requires representative, verifiable inputs and explicit rules for transaction
data and expert judgment. The official
[IFRS 13 overview](https://www.ifrs.org/issued-standards/list-of-standards/ifrs-13-fair-value-measurement/)
requires relevant observable inputs to be maximized and material unobservable
adjustments to remain visible. Those principles support this recordkeeping
discipline; they do not make this output fair value.

## One economic observation record

Each record represents one de-duplicated economic observation cluster, not one
vendor copy of the same transaction. It retains:

- stable observation, claim, cluster, source, evidence, and normalized-result
  identifiers;
- transaction or quote status, observation and settlement dates, size,
  currency, monetary unit, gross-versus-ex-cost basis, and return basis;
- evidence of settlement or quote executability, orderliness, arm's-length
  status, and any transaction-market anchor;
- gross price, separately evidenced buyer-direct cost, and the externally
  calculated annual-effective expected-cash rate preimage;
- complete dated scenario-cash, physical-probability-set, recovery, solver,
  source-hash, and reconstruction-residual record identifiers;
- its relation to the target price, which must be independent;
- full-claim normalization at the observation's own date, plus a separately
  evidenced date/regime bridge when the observation predates the decision;
- side rights, options, non-cash consideration, and unresolved terms; and
- every comparability treatment and its evidence source.

The frozen universe, lookback, inclusion rule, de-duplication manifest, and all
in-scope clusters—including excluded clusters and their reasons—travel with the
set. Repeating the same deal in several databases never increases the evidence
count. Correlated same-dealer or same-window quotes are one source cluster or
are limited by a source cap fixed before results; several screens cannot create
a pseudo-quorum.

## The adjustment bridge

Eight dimensions must each be either matched or adjusted:

```text
1. contractual cash-flow rights
2. priority, security, tail exposure, and residual recovery-risk premium
3. systematic, covariance, concentration, and residual-model risk premium
4. maturity, timing, and weighted-average life
5. currency, monetary basis, and hedge cash flows
6. liquidity and transfer restrictions
7. claim size and market depth
8. observation date and market regime
```

An unresolved dimension makes the record ineligible. An adjusted dimension
requires one separately sourced lower and upper adjustment; a blank cell is
not zero. Side rights or non-cash consideration must first be valued and
incorporated by an external, evidenced normalization. Version 0.1 otherwise
excludes the record.

Adjustments are additive in log gross-return space. For observation `j`, let
its external expected-cash rate interval be `[r_j-, r_j+]`, and let dimension
`d` have a declared signed adjustment interval `[a_jd-, a_jd+]`. Then:

```text
z_j- = log(1 + r_j-) + sum_d a_jd-
z_j+ = log(1 + r_j+) + sum_d a_jd+

H_j = [exp(z_j-) - 1, exp(z_j+) - 1] intersect declared domain D
```

Equivalently, each log adjustment multiplies the gross annual-return factor.
This keeps signed compounding coherent. The engine checks the identity but does
not estimate an adjustment, select its sign, infer a probability, convert a
spot currency amount into a hedge, or roll a stale observation forward.
Expected project loss and expected recovery are not rate adjustments on
dimensions 2 or 3: they must already be inside each claim's independently
constructed physical expected cash. Those dimensions bridge only residual
tail, covariance, concentration, recovery-uncertainty, and model-risk premia
left after the expected-cash basis. A required no-double-count reconciliation
keeps the same loss or recovery effect from entering both cash and rate.

Summing the component bounds is exact only if their Cartesian combinations are
jointly feasible. A primary record therefore needs evidence that the displayed
total adjustment interval is the exact connected projection of a jointly
feasible adjustment set. A componentwise box or hull remains an outer
sensitivity and cannot identify `H_j`; dependence among adjustments must not
manufacture attainable rates.

All input rate and adjustment bounds must be finite and ordered, every source
rate must exceed `-1`, the log sums and `expm1` transformation use checked
finite arithmetic, every transformed endpoint must remain strictly above
`-1`, and every transformed lower bound must remain no greater than its upper
bound. `D` is bounded and closed. A valid connected preimage
wholly outside `D` remains a counted
eligible observation with an empty `H_j`. That result is potentially
falsifying evidence; it is not relabeled as a data-quality exclusion merely
because it is inconvenient.

## Set identification without averaging

Evidence tiers are never pooled merely to create agreement. The implementation
calculates sets separately for settled identical-claim transactions, settled
comparables, and executable transaction-anchored quotes. The highest tier with
eligible records controls the primary result even when its set is empty; lower
tiers remain challenger sensitivities and cannot outvote better evidence.
If the predeclared `k` is inadmissible for that highest nonempty tier, the
primary result is insufficient evidence. A lower tier is not promoted merely
because it contains enough clusters for the same `k`.

Within one tier, let `n` be the number of financially eligible, de-duplicated
economic clusters. Choose an integer discordance budget `k` before seeing the
resulting intervals and set `q=n-k`, with `0<=k<n/2`. The domain `D` is a
finite closed interval inside `[0,10]`, matching the current downstream rate
guard. Define:

```text
S_k = { h in D : at least q of the n sets H_j contain h }
```

`S_k` is the exact closed union of all hurdle regions not ruled out by at least
`n-k` observations. It can be empty, a single point, one interval, or several
disjoint intervals. Touching closed intervals retain their common point.

This is set identification, not statistical confidence. It attaches no random
sampling interpretation to a private comparable set and gives no observation
a hidden weight. `k` is a disclosed robustness allowance for discordant
clusters, not permission to delete whichever result is least convenient.
`k<n/2` preserves strict-majority consistency. Count alone does not establish
informativeness: `S_k=D` is reported as uninformative, and the output identifies
which proper-subset observations actually bind each component. One settled
identical-claim observation is reported as a direct transaction-conditioned
case, not a population consensus. A nonempty proper subset with `q>=3` may be
given the declared comparable-consensus label only in a non-direct tier; it is
never a confidence interval.

The implementation publishes `S_0` through `S_k`, maximum overlap, all
included and excluded clusters, each `H_j`, and leave-one-cluster-out results.
If the chosen `S_k` is empty, the hurdle is **not identified**. The engine does
not enlarge adjustments, increase `k`, use the target price, or fall back to a
policy target.

### Why gaps must survive

Suppose three comparable clusters produce:

```text
H_1 = [8%, 10%]
H_2 = [9%, 12%]
H_3 = [11%, 14%]
```

With `k=1`, at least two clusters must agree. The exact result is:

```text
S_1 = [9%, 10%] union [11%, 12%]
```

The interval from 10% to 11% has only one supporter. Reporting the hull
`[9%,12%]` would fabricate evidence in that gap. A hull may appear only as a
clearly labelled outer sensitivity diagnostic and never as the identified set.

## Connection to issue-price engineering

Every connected component remains separate when passed downstream. The
current issue-price engine accepts at most 256 finite hurdle cases and also
requires a literal-zero arithmetic baseline. If zero is absent from the
evidence set, the adapter adds one 0% `synthetic_sensitivity` case with an
`unresolved` relation to the reference price. That baseline is not evidence,
is not part of any component, and cannot establish a financeable window.

The remaining cases use a grid frozen before price results are examined,
include every component endpoint, are de-duplicated, and retain independent
source and normalized-result provenance. Evidence-consistent cases plus the
baseline must fit the 256-case and existing work limits. Endpoint-only testing
establishes the whole component only if the claim's price boundary is
separately proved monotone over that component. Future costs can defeat a
casual monotonicity assumption.

For every tested evidence-consistent hurdle `h`, the existing engine then
reports:

```text
buyer price ceiling P*(h)
issuer funding floor after no-rights support
conditional overlap or quantified support gap
reference-price NPV, downside probability, ES95, and ES99
separate documentary, funded, settled-source, and use-side evidence
```

The hurdle set does not change project probabilities, project cash, claim
priority, first-loss funding, support capacity, or distributions. It changes
only which externally defensible discount-rate cases are allowed to test the
fixed asset.

## What evidence must be acquired next

Public announcements can establish that cellular-agriculture debt,
convertibles, guarantees, grants, impairments, and asset transfers exist. As
the [Public Calibration Evidence Snapshot v0.1](PUBLIC_CALIBRATION_EVIDENCE_V0_1.md)
shows, they generally omit the complete price, cash rights, priority, maturity,
fees, options, recovery, settlement, and realized cash needed for this
normalization.

The retained dossiers show two distinct boundaries. The
[Liberation Labs note](../reference_transactions/liberation-labs-facility-financing-2024-2025/PUBLIC_TRANSACTION_DOSSIER.md)
shows that 10% promised annual interest and a named maturity do not reconstruct
expected investor cash when conversion, security, recovery, costs, and
settlement remain incomplete. Its April and October issuances also cannot be
counted until their common note-series or add-on relationship is resolved. The
[Solar Foods facility](../reference_transactions/solar-foods-factory-01-facility-2022-2025/PUBLIC_TRANSACTION_DOSSIER.md)
adds a full draw, capitalized fee, floating-rate formula, amortization terms,
named guarantors, covenant history, and audited aggregate principal repayments
that arithmetically match the disclosed schedule. It still lacks the complete
lender price, claim-level ledger, guarantee economics, default and recovery
cash, and comparison bridge. More observed terms therefore improve the asset
record without automatically identifying a rate.

The next acquisition target is not another financing headline. It is a
controlled package containing executed terms, settlement evidence, full dated
investor cash rights, fees and side rights, loss and recovery evidence,
liquidity and transfer terms, currency-hedge cash flows, and the independent
expected-cash calculation. Declined or failed transactions matter too because
an observation universe containing only completed financings is selected on
success.

Until at least one package survives the eligibility rules, the honest output is
`no transaction-consistent discount-rate set identified`. Synthetic rates
remain useful for mechanics and reverse stress, but the project will not rename
them market evidence.

## Hard claims boundary

This v0.1 set must never be described as:

- a fair value, market value, clearing price, rating, yield curve, or benchmark;
- a statistically representative sample or confidence interval;
- proof that an investor will subscribe or that support will perform;
- a substitution of promised yield for the model-conditioned expected-cash
  discount-equivalent rate, or an inferred annual holding-period return;
- evidence that the target project's physical probabilities are calibrated;
- permission to average heterogeneous or disjoint observations; or
- a financing, legal, accounting, tax, or investment recommendation.

Its positive claim is narrower and useful: it makes every independent price
observation, model-conditioned rate derivation, adjustment, disagreement, and
unsupported region visible before a rate is allowed to influence the price and
support required by the instrument.
