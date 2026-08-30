# Robust Success-Participation Term v0.1

## Purpose

This module asks one narrow financial-engineering question:

> What is the smallest contractual share of already-declared success cash that
> makes the pool meet a stated NPV target under every probability mix admitted
> by its declared probability envelope?

It produces a term result, not a fund-management rule. It does not decide who
runs projects, select facilities, invent enterprise value, or allocate an
existing loss among tranches. Its output is either a supported participation
fraction or an explicit finding that the modeled cash rights are insufficient.

Version 0.1 is deliberately synthetic-only. It is a transparent contract and
falsification kernel to be calibrated later from real project evidence.

## Contract definition

For each investor receipt `r`, let:

```text
A_r = configured investor cash receipt
P_r = portion of that receipt classified as returned principal
Y_r = A_r - P_r
```

`Y_r` is eligible only when the receipt's declared external source kind is
selected in the term input. Version 0.1 permits only:

- commercial cash;
- licensing or royalty cash; and
- sale or exit cash.

At contractual fraction `q`:

```text
A_r(q) = P_r + q Y_r       for a selected receipt
A_r(q) = A_r               for every unselected receipt
0 <= q <= 1
```

Thus `q=0` switches off selected non-principal participation but leaves all
other investor cash unchanged. `q=1` exactly reconstructs the declared input
cash schedule. Draws, principal classifications, pool costs, scenario
probabilities, failure recoveries, loss layers, and source budgets never change.

This definition is intentionally narrower than “share all upside.” A scenario's
unused cash-source capacity is not an investor receipt and is not evidence of a
legal claim. The strict term file must separately assert that the selected
non-principal receipts are contractually scalable. That assertion is visible
model input, not proof that the right exists or can be transferred or enforced.

## Robust value equation

For joint scenario `s`, the dated-cash calculation gives:

```text
NPV_s(q) = B_s + q U_s
```

where `B_s` is scenario NPV with selected participation off and `U_s` is the
present value of the selected configured participation at `q=1`.

Let the probability envelope be:

```text
P = { p : sum_s p_s = 1 and lower_s <= p_s <= upper_s }
```

The conservative expected NPV at a candidate term is:

```text
g(q) = min over p in P of sum_s p_s NPV_s(q)
```

The required contractual term for target `T` is the smallest `q` in `[0,1]`
for which `g(q) >= T`. Because selected payoffs are non-negative, `g(q)` is
non-decreasing. It is a concave, piecewise-linear lower envelope because the
binding adverse probability witness may change as `q` changes.

The engine therefore projects the complete scenario NPV at every candidate
`q`. It never adds the separately minimized base NPV and participation payoff;
those two minima may come from different probability measures. An interior
solution is reported as a certified failing/passing bracket rather than a
falsely exact floating-point root. Exact results are reserved for literal
`q=0` and `q=1` boundaries.

## Possible results

The solver distinguishes five economically different outcomes:

1. The target is already met with selected participation off.
2. An interior failing/feasible fraction bracket is found.
3. Exactly the full configured participation is required.
4. No selected non-principal payoff exists to solve for.
5. Even full configured participation does not meet the robust target.

The fifth result is a financial-engineering result, not a software failure. It
prevents an attractive central case from being presented as a robust asset and
prevents a mathematically required rate above 100% from being silently clamped
to the maximum contractual rate.

## Hand-calculated synthetic result

The two-project fixture funds `10 + 10` at month zero, pays `0.2` of pool cost,
returns `2` on each failed project, and includes `3` of commercial
non-principal participation for each successful project. Its physical hurdle
is zero, so nominal and present-value participation are equal.

| Joint scenario | `NPV(q=0)` | Full-q selected payoff | `NPV(q=1)` |
|---|---:|---:|---:|
| common loss | -16.20 | 0.00 | -16.20 |
| common success | -0.20 | 6.00 | 5.80 |
| culture loss / scale-up success | -8.20 | 3.00 | -5.20 |
| culture success / scale-up loss | -8.20 | 3.00 | -5.20 |

Under the declared probability bounds:

| Expected NPV | Minimum | Central | Maximum |
|---|---:|---:|---:|
| `q=0` | -5.00 | -3.40 | -2.68 |
| `q=1` | -0.80 | 1.40 | 2.39 |

The robust lower endpoint is `-5.00 + 4.20q` throughout this particular
fixture. A zero-NPV target would require `q = 25/21`, or about `1.190476`.
Because the honest contractual domain ends at one, no feasible fraction exists;
at full participation the robust shortfall remains `0.80 DEMO million`.

The central probability mix alone crosses zero near `q = 17/24`, or
`0.708333`. That is useful context but not the conservative contract answer.
Expected principal loss remains `2.48 / 3.20 / 4.80` and impairment probability
remains `30% / 38% / 50%` across the minimum, central, and maximum probability
results for every `q`, because participation changes return, not principal loss.

In human terms: the modeled pool contains real success sharing, and its central
case is positive, but the full existing right is still too small to compensate
investors under the admitted adverse probability mix. Repackaging the same cash
or dividing its loss into layers cannot cure that gap.

## What could change the result honestly

A later design can cross the robust threshold only by changing something real
and evidenced, for example:

- a lower purchase price, lower pool cost, or lower funded draw;
- stronger milestone stopping that reduces cash spent before failure;
- a larger legally granted share of evidenced commercial, royalty, or exit cash;
- better recovery supported by enforceable collateral or contracts;
- independently supported narrower probability bounds after stronger evidence;
- an explicit risk-bearing guarantee, advance purchase, price support, or
  concessionary payment whose provider, cap, conditions, and cost are modeled.

These alternatives are not interchangeable. Narrowing uncertainty requires
evidence, while support introduces a new obligor and counterparty exposure.
Loss tranching changes which investor bears the same loss; it does not improve
the untranched economics.

## Strict input and reproducibility

The companion file is closed `key=value` schema version `0.1.0`. It requires:

```text
participation.model_version=0.1.0
participation.label=...
participation.source_note=...
participation.synthetic_inputs=true
participation.selected_nonprincipal_cash_is_contractually_scalable=true
participation.target_robust_npv_million=...
eligible_source.count=...
eligible_source.N.kind=commercial|licensing_royalty|exit_sale
```

Unknown, missing, and duplicate keys fail closed. Selected source kinds must be
unique. Numbers must be finite. Normalized output is deterministic and can be
loaded again without changing the input values.

Run the implemented analysis on Windows after building:

```powershell
.\build\dev\Debug\naturalehia-success-participation.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg `
  .\scenarios\two-project-success-participation-synthetic.cfg
```

Add `--print-normalized` to append all three complete normalized inputs.

The report includes q=0, q=1, and reported-term NPV ranges; endpoint probability
witnesses; nominal and present-value participation by source and scenario; the
central-only threshold as explicitly non-conservative context; principal-loss
invariance; source-capacity checks; and cash, witness, and probability
reconciliations.

## Current boundary and next research

This is physical-probability NPV at an analyst-declared hurdle. It is not fair
value, risk-neutral pricing, an observable market price, a security rating, or
an offer. It omits taxes, fees beyond configured pool costs, legal friction,
counterparty default, dilution, priority disputes, liquidity premiums, capital
treatment, and secondary-market execution.

Before a real term is proposed, each selected receipt needs a legal cash-flow
chain from payer to project to instrument, payment timing and seniority,
transfer and enforcement analysis, counterparty credit, costs, and evidence
that its scenario amount is obtainable. Joint outcomes and probability bounds
need cohort-based calibration under the project's
[calibration standard](PORTFOLIO_CALIBRATION_STANDARD_V0_1.md). Only after the
untranched cash right passes this falsification test should investor-specific
pricing and loss-bearing layers be researched.
