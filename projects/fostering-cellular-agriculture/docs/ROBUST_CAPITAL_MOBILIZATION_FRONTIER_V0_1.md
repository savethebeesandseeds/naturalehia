# Robust Capital-Mobilization Frontier v0.1

## Grid-based market-claim feasibility frontier

Status: synthetic financial-engineering term. Version 0.1 is a finite-grid
decision aid, not a transaction, valuation, or claim that capital has been
mobilized.

## Purpose

This term asks a concrete structuring question:

> For a fixed pool of project cash paths and a fixed candidate set of physical
> probabilities, which tested combinations of success participation and
> funded junior first-loss capital satisfy a stated market-investor mandate?

The answer is a set of feasible tested terms, not a weighted score and not one
supposedly optimal security. The calculation keeps the underlying projects,
gross cash generation, losses, event definitions, and probability bounds
unchanged. It changes only two disclosed economic terms and rebuilds the cash
waterfall and risk report for every tested pair.

The frontier connects the common
[Project Financial Interface](PROJECT_FINANCIAL_INTERFACE_V0_1.md), the
[event-probability candidate set](EVENT_PROBABILITY_POLYTOPE_V0_2.md), and the
[fully funded capital stack](CAPITAL_STACK_TERM_V0_1.md). The contractual
meaning of success cash is defined in the
[Success-Participation Term](SUCCESS_PARTICIPATION_TERM_V0_1.md).

## Two tested terms, one fixed structure

Let:

```text
K = aggregate project commitment and top stack detachment, excluding pool costs
q = fraction of selected, already-declared non-principal success cash
A = funded junior first-loss amount
M = K - A = market-claim notional
```

For every declared pair `(q,A)`, version 0.1 creates exactly two contiguous
claims:

```text
funded junior loss-absorbing claim   [0,A]
market-facing priority claim         [A,K]
```

Both claims subscribe their complete notionals at par at month zero. Together
they subscribe `K` into the project-commitment reserve; there is no gap or
hidden third owner in that commitment funding. Declared pool costs remain
additional pro-rata cash calls. They can make each investor's total cash
contributions exceed its claim notional. “Fully funded” therefore describes
the `K` commitment reserve, not a cap on all-in investor cash exposure. Project
draws use the reserve, principal cash pays the market claim first, realized
principal loss attaches to the junior claim first, and selected non-principal
cash pays the market claim only up to one fixed lifetime priority cap before
any residual goes junior.

`q` scales only eligible non-principal receipts already present in the project
interface. It does not scale principal, recovery, commitment, draw, cost, loss,
or probability. It is a contingent success-cash participation fraction, not a
coupon, yield, promised return, or ownership percentage. `A` reallocates the
fixed pool loss and cash priority; it does not reduce gross project loss or
create cash. The market priority cap and both claims' annual physical hurdle
rates are fixed across the grid. They are not optimized until a favorable
result appears.

The complete declared `q` grid crossed with the complete declared `A` grid may
contain at most 1,024 pairs. The evaluator sorts and tests every pair. It does
not interpolate between them. Let `C` be the candidate count, `N` the project
count, `S` the scenario count, `E` the event count, `H` the horizon month, and
`R` the portfolio-record count defined below. The evaluator rejects a combined
structural work count above 4,000,000:

```text
C * [S * (S + E + 1)
     + R
     + N * S * (H + 1)
     + 2 * S * (H + 1)] <= 4,000,000
```

The first term bounds repeated probability-projection structure, the second
counts every configured cash-availability, draw, receipt, pool-cost,
cash-source, and factor-tag record plus every configured portfolio loss layer
applied to every scenario, the third bounds repeated project cash paths, and
the fourth bounds monthly waterfall work for the exactly two generated claims.
The reported total also discloses `R` and the probability and cash-path
subtotals. This is a deterministic compute guard, not an economic limit or a
guarantee of run time.

## Robust metrics and endpoint witnesses

Let `P` be the event-probability polytope and let `X_s(q,A)` be a scenario
metric after rebuilding the participation cash and two-claim waterfall. A
favorable robust endpoint is generally:

```text
minimum over p in P of sum_s p_s X_s(q,A)
```

An adverse robust endpoint is generally the corresponding maximum. The report
retains the complete probability witness for every endpoint. Minimum market
NPV, maximum expected principal loss, maximum impairment probability, maximum
negative-NPV probability, maximum tail loss, and maximum WAL can bind at
different feasible probability measures. Those endpoints must never be
assembled into one invented stress scenario.

For each pair the report includes:

- minimum, central, and maximum fully funded aggregate NPV at the pool hurdle;
- minimum, central, and maximum NPV for each claim at its own hurdle;
- market expected total cash contributions, principal cash, and total cash
  distributions;
- market expected principal loss;
- market principal impairment and negative-NPV probabilities;
- market principal-loss ES95 and ES99;
- market NPV-shortfall ES95 and ES99; and
- market principal cash-weighted average life, when its denominator remains
  positive over the whole candidate set.

NPV here is a physical-measure sensitivity at a declared hurdle. “Robust” means
only that the stated inequality holds for every `p` admitted by the supplied
candidate set. It does not mean the event bounds are empirically correct.

## Principal-loss tails are not NPV-shortfall tails

For market notional `M`, scenario principal loss is the realized write-down of
that funded principal. Scenario NPV shortfall is:

```text
S_s = max(0, -NPV_s at the market claim's declared hurdle)
```

For tail mass `tau`, upper expected shortfall is the average loss in the worst
`tau` probability mass under one feasible full probability witness and one
associated fractional tail witness:

```text
ES_tau(p,L) = (1/tau) max_y sum_s L_s y_s
0 <= y_s <= p_s
sum_s y_s = tau
```

ES95 uses `tau=0.05`; ES99 uses `tau=0.01`. The frontier reports principal-loss
ES and NPV-shortfall ES separately, both in money and as a fraction of fixed
market notional. Those fractions use `M`, not total contributions; additional
pool-cost calls remain visible in the contribution and NPV rows. A claim can
return all principal yet have negative NPV because of pool costs, timing, or an
unmet return hurdle. Conversely, principal loss cannot be relabelled as a mere
return shortfall.

“Expected principal loss” here means the physical-probability expectation of
the modelled deal-horizon principal write-down. It is not IFRS 9 expected
credit loss, Basel regulatory expected loss, an accounting impairment
classification, or a legal default determination.

The term “expected shortfall” follows the general tail-average convention, but
these physical ES95/ES99 sensitivities are not the Basel market-risk capital
measure. The [Basel Framework](https://www.bis.org/baselframework/BaselFramework.pdf)
uses a 97.5th-percentile, one-tailed ES for its internal-model market-risk
standard and supplies its own regulatory calibration.

## Expected principal cash and WAL

For scenario `s`, define:

```text
Q_s = total market-claim principal cash
T_s = sum_t (t/12) * market principal cash_s,t

Expected principal cash(p) = sum_s p_s Q_s
WAL(p) = sum_s p_s T_s / sum_s p_s Q_s
```

Expected principal cash is reported beside WAL so that a duration figure is
not mistaken for a repayment amount. Each WAL endpoint uses one common
probability measure for its numerator and denominator; it is not the ratio of
separately optimized quantities. WAL is unavailable if any feasible measure
can reduce expected principal cash to the numerical denominator floor. Lost
principal is never treated as a maturity-date payment merely to manufacture a
duration.

## Declared mandate and feasibility

An institution may declare any subset of these independent constraints:

```text
minimum robust aggregate NPV
minimum robust market NPV / market notional
maximum market expected principal-loss fraction
maximum market principal-loss ES95 fraction
maximum market principal-loss ES99 fraction
maximum market principal-impairment probability
maximum market negative-NPV probability
maximum market NPV-shortfall ES95 fraction
maximum market NPV-shortfall ES99 fraction
maximum market WAL
maximum funded junior first-loss amount A
maximum catalytic NPV concession
```

An omitted constraint does not receive a market convention and does not bind.
A candidate is **feasible** only when it passes every declared constraint.
This is modeled mandate feasibility, not suitability for a particular investor
and not evidence of demand.

For every tested `q`, the report identifies the least **tested** `A` that is
feasible. It is absent when that row of the grid contains no feasible `A`.
Likewise, the minimum tested feasible `q` is only a grid result. Neither is a
continuous minimum, a recommended term, or evidence that nearby untested
values work.

Among feasible candidates, the frontier also reports the nondominated set.
Candidate one dominates candidate two only when it is no worse, within the
published numerical tolerance, in every minimized dimension and strictly
better in at least one. Those dimensions are `q`, `A`, catalytic NPV
concession, market expected principal loss, both principal-loss tails,
impairment, negative-NPV probability, both NPV-shortfall tails, and WAL. An
available WAL is preferred to an unavailable one. Return requirements remain
explicit constraints; the engine does not reward taking additional project
success cash after the stated mandate is met. No weighted composite score is
used.

For an actual value `x` and a declared boundary `b`, the pass and dominance
comparison tolerance is:

```text
1e-10 + 256 * machine epsilon * max(1, abs(x), abs(b))
```

A result inside that floating-point band can pass a displayed boundary. This
is a numerical comparison rule, not an economic materiality allowance.

## Catalytic NPV concession

Let `T_C` be the junior provider's declared NPV target and let
`NPV_C^min(q,A)` be its separately projected minimum expected NPV. The reported
benchmark gap is:

```text
C(q,A) = max(0, T_C - NPV_C^min(q,A))
```

This makes a modeled economic sacrifice visible instead of calling first-loss
support free diversification. It is not automatically a grant, subsidy,
budgetary cost, or accounting fair value. A transaction-level concession
would require an identified provider, reference price, funding cost, taxes,
fees, legal terms, and an evidenced counterfactual.

The distinction follows the direction of IFC's 2025 discussion of first-loss
guarantees, subordinated loans, junior equity, crowding-in, and minimum
concessionality: concessional support should be no greater than needed to
attract commercial capital and should be measured against an appropriate
reference price ([IFC, *The Role of Blended Finance in an Evolving Global
Context*](https://www.ifc.org/content/dam/ifc/doc/2025/role-of-blended-finance-in-an-evolving-global-context.pdf)).
This frontier supplies neither that market reference price nor proof of
crowding-in.

## Exact synthetic hand fixture

All cash amounts below are invented `DEMO` millions. The fixture has two
10-unit projects, `K=20`, full month-zero draw, a `0.2` pool cost, and a
zero-percent physical hurdle. A successful project returns 10 of principal
plus `4q` of selected non-principal cash at month 24. A failed project returns
2 of principal at month 12.

| Joint state | Central probability | Pool NPV |
|---|---:|---:|
| Common success | 0.62 | `-0.2 + 8q` |
| Culture loss / scale-up success | 0.18 | `-8.2 + 4q` |
| Culture success / scale-up loss | 0.18 | `-8.2 + 4q` |
| Common loss | 0.02 | `-16.2` |

The event polytope requires common loss between 1% and 10%, each project's
impairment between 12% and 30%, and any impairment between 30% and 50%. The
binding adverse aggregate witness assigns 10% to common loss, 40% across the
single-loss states, and 50% to common success. Therefore:

```text
robust aggregate NPV(q) = -5 + 5.6q
robust aggregate NPV >= 0  iff  q >= 25/28
```

Give the market claim a fixed non-principal priority cap of 1. For
`q >= 0.25` and `8 <= A <= 16`, that cap is filled whenever at least one
project succeeds. With `M=20-A`, market NPV is:

```text
market contribution in every state:  M + 0.01M = 1.01M
market distribution outside common loss: M + 1
market distribution in common loss:      4

non-common-loss state:  1 - 0.01M = 0.8 + 0.01A
common-loss state:      4 - 1.01M = 1.01A - 16.2

robust market NPV(A) = 0.9(0.8 + 0.01A)
                       + 0.1(1.01A - 16.2)
                     = 0.11A - 0.90
```

Thus robust market NPV is non-negative at `A >= 90/11`. In this fixture the
aggregate/junior adverse witness is also one feasible market-NPV minimizing
witness, so the following algebra reconciles all three. The market minimum is
non-unique, and the implementation may retain a different market witness with
the same value:

```text
robust junior NPV(q,A) = -4.1 + 5.6q - 0.11A
zero-target concession = max(0, 4.1 - 5.6q + 0.11A)
```

Only common loss impairs the market claim when `A < 16`; its severity is
`16-A`. Consequently:

```text
worst expected principal-loss fraction = 0.10(16-A) / (20-A)
worst principal-loss ES95 fraction      = (16-A) / (20-A)
worst principal-loss ES99 fraction      = (16-A) / (20-A)
worst impairment probability            = 10% for A<16; 0% at A=16
```

The common-loss state can occupy all of either tail because its admitted upper
probability is 10%. Its market NPV remains negative throughout this range, so:

```text
worst negative-NPV probability = 10%
worst NPV-shortfall ES95/ES99  = 16.2 - 1.01A
```

The following are exact analytical boundary points for the hand fixture. They
are valid as tested results only when the displayed values occur in the
declared grid.

| `A` | `M` | displayed `q` | Robust aggregate NPV | Robust market NPV | Robust junior NPV | Junior concession | Structural reading |
|---:|---:|---:|---:|---:|---:|---:|---|
| `90/11` | `130/11` | `25/28` | 0.00 | 0.00 | 0.00 | 0.00 | Market-NPV boundary; fails 5% expected-loss and 50% principal-tail limits |
| 12 | 8 | `25/28` | 0.00 | 0.42 | -0.42 | 0.42 | Boundary for those loss limits |
| 14 | 6 | `261/280` | 0.22 | 0.64 | -0.42 | 0.42 | More junior capital and more project success participation |
| 16 | 4 | `34/35` | 0.44 | 0.86 | -0.42 | 0.42 | No market principal loss, but not zero NPV risk; exceeds an `A<=14` mandate |

Market loss and return-shortfall measures remain visibly different:

| `A` | Worst expected principal loss | Principal-loss ES95 = ES99 | Worst impairment probability | Worst negative-NPV probability | NPV-shortfall ES95 = ES99 (amount / % of `M`) |
|---:|---:|---:|---:|---:|---:|
| `90/11` | 6.615% | 66.154% | 10% | 10% | 7.936 / 67.154% of `M` |
| 12 | 5.000% | 50.000% | 10% | 10% | 4.080 / 51.000% of `M` |
| 14 | 3.333% | 33.333% | 10% | 10% | 2.060 / 34.333% of `M` |
| 16 | 0.000% | 0.000% | 0% | 10% | 0.040 / 1.000% of `M` |

At `A=16`, the common-loss state returns the market claim's entire 4.00 of
principal. Its investor nevertheless paid 4.00 at par plus a 0.04 pro-rata
share of pool cost, leaving NPV `-0.04`. The claim has no modeled principal
write-down in that state, but its negative-NPV probability can still reach
10%. “No principal loss” is therefore not “risk-free” or “capital guaranteed.”

Expected principal cash and WAL are reported together:

| `A` | `M` | Expected principal cash min / central / max | WAL min / central / max (years) |
|---:|---:|---:|---:|
| `90/11` | `130/11` | 11.036 / 11.662 / 11.740 | 1.891 / 1.931 / 1.947 |
| 12 | 8 | 7.600 / 7.920 / 7.960 | 1.842 / 1.899 / 1.922 |
| 14 | 6 | 5.800 / 5.960 / 5.980 | 1.793 / 1.866 / 1.896 |
| 16 | 4 | 4.000 / 4.000 / 4.000 | 1.700 / 1.800 / 1.845 |

For this fixture, if `e` is any-impairment probability and `c` is common-loss
probability, then `0.30<=e<=0.50`, `0.01<=c<=0.10`, and:

```text
E[Q] = M - (M-4)c
WAL  = [2M - 2e - 2(M-3)c] / [M - (M-4)c]
```

The displayed WAL minimum uses `(e,c)=(0.50,0.10)` and the maximum uses
`(0.30,0.01)`. These closed forms are an audit of this four-state fixture, not
a shortcut used for general portfolios.

## What “fully funded” does and does not mean

The junior layer is funded cash subscribed into the modeled reserve. It is not
an unfunded promise to reimburse loss and therefore is not a guarantee. The
market claim is a funded priority claim with contingent success cash. Priority
does not by itself make it a loan, bond, deposit, or other debt instrument;
those labels require contractual payment obligations, maturity, remedies,
legal characterization, and applicable regulation. Full funding establishes
only the model's cash capacity under its assumed lossless reserve. It does not
establish custody, bankruptcy remoteness, enforceability, or a guaranteed
return.

## Mobilization, concessionality, and valuation boundaries

The frontier deliberately sets all three claims below to false:

```text
fair value or market price estimated          false
continuous optimum or weighted score claimed false
actual capital mobilization established       false
```

The OECD's reporting methodology treats causality as a key condition for
mobilized private finance: private financiers would not have invested in the
absence of the official intervention, with pro-rated attribution used to avoid
double counting ([OECD methodology](https://www.oecd.org/en/data/dashboards/mobilisation-of-private-finance-for-development/methodology.html)).
A modeled feasible cell shows no investor commitment and tests no such
counterfactual. It must not be reported as finance mobilized.

Likewise, expected NPV at a physical hurdle is not fair value. IFRS 13 defines
fair value as an exit price in an orderly transaction between market
participants at the measurement date and requires market-participant
assumptions, including assumptions about risk
([IFRS Foundation, IFRS 13](https://www.ifrs.org/issued-standards/list-of-standards/ifrs-13-fair-value-measurement/)).
Version 0.1 supplies no market price, risk-neutral measure, liquidity premium,
spread, rating, accounting classification, tax treatment, regulatory capital
charge, legal opinion, or investor demand evidence.

The frontier is useful before those later steps because it says exactly what a
candidate market claim would have to survive and how much modeled junior
capital and success cash accompany it. It is honest only while those terms,
risks, witnesses, and limits remain visible.

## Implementation map

- [C++ frontier interface](../include/naturalehia/cellular_finance/robust_capital_mobilization_frontier.hpp)
- [C++ evaluator](../src/robust_capital_mobilization_frontier.cpp)
- [strict frontier configuration](../include/naturalehia/cellular_finance/robust_capital_mobilization_frontier_config.hpp)
- [synthetic four-state fixture](../scenarios/capital-mobilization-frontier-v0.1-synthetic/frontier.cfg)
- [verification record](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_VERIFICATION_V0_1.md)
