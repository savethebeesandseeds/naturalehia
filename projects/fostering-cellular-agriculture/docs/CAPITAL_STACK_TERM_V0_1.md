# Fully Funded Capital Stack v0.1

Status: implemented synthetic financial-engineering term, 2026-08-29.

## Purpose

This term divides one declared and mechanically validated cellular-agriculture participation pool into
first-loss, intermediate, and senior investor positions. It answers a narrow
question: **can the same underlying project cash be allocated into claims with
meaningfully different loss, duration, and expected-return profiles without
inventing value?**

The answer is an allocation rule, not a market price. Version 0.1 calculates
physical-scenario cash, loss, exposure, NPV at declared hurdles, and exact
ranges over the existing probability envelope. It does not calculate a spread,
risk-neutral fair value, rating, legal form, regulatory capital treatment, or
offering price.

The underlying pool must remain untranched. The engine first fixes one declared
success-participation fraction `q`, reconstructs the complete pool cash paths,
and only then applies the stack. Tranching changes neither a project draw nor a
project receipt, gross principal loss, continuing exposure, pool cost, joint
scenario, or probability bound.

## Why the stack is fully funded

The existing loss coordinates are absolute amounts of aggregate commitment. A
first-loss layer of 4 million must contain 4 million of capital capable of
protecting the layers above it even if underlying project draws are staged. A
pro-rata capital-call structure would not necessarily do that: after a small
draw, the first-loss investor may have paid much less than 4 million while the
loss formula still assigns the entire first 4 million of pool loss to it.

Version 0.1 therefore uses the smallest coherent boundary:

- investors subscribe the complete aggregate project commitment at par in
  month zero;
- the vehicle holds those proceeds in a segregated reserve assumed lossless
  and zero-yield;
- actual milestone project draws consume the reserve;
- pool costs are additional pro-rata investor calls and never become tranche
  principal;
- undrawn project commitments irrevocably expire at the horizon; and
- the remaining reserve returns through the principal waterfall at the
  horizon.

This convention is intentionally capital-inefficient. Its cost appears as
prefunding drag whenever projects would otherwise draw later. A future
capital-call or warehouse design must supply equally real loss support before
it can replace this boundary.

## Path accounting

For one joint scenario, define:

```text
K = aggregate project commitment
D = actual project draws
P = cash receipts classified as principal return
L = realized principal loss on resolved claims
O = funded principal still outstanding on continuing claims
U = unused reserve returned at the horizon = K - D
```

The underlying project engine enforces:

```text
D = P + L + O
```

The fully funded stack therefore enforces:

```text
K = P + U + L + O
```

Reserve return `U` is the investors' own undeployed capital. It is neither
project revenue nor investment profit. Continuing amount `O` has no invented
terminal cash value and remains exposure, not realized loss.

The aggregate investor cash path is:

```text
CF_stack,t
    = -K, at month zero
      - pool costs_t
      + underlying investor receipts_t
      + U, at the horizon
```

Project draws occur inside the reserve ledger. Consequently the stack cannot
copy the pool's draw-as-needed NPV. It rebuilds every tranche cash flow and
reports the difference as prefunding drag.

## Principal loss and cash priority

Tranches form a contiguous partition from zero through `K`, listed from
first-loss to most senior. A tranche with attachment `A`, detachment `B`, and
notional `N = B - A` receives realized principal loss:

```text
Loss(A,B) = min(max(L - A, 0), N)
```

Each notional must be at least `1e-6` million—one base currency unit—and the
engine caps combined scenario-by-tranche-by-month work at two million cells.
These are numerical and resource boundaries, not proposed market lot sizes.

Actual principal-component receipts and unused reserve returned at the
horizon have equal priority. Their combined amount pays principal in the
reverse direction: senior first, then intermediate, then first-loss. When
both sources arrive at the horizon, each tranche's source attribution is pro
rata within that month's combined principal; implementation order cannot
create source preference. Unresolved exposure occupies the same stack from
the bottom after realized loss. For every tranche:

```text
Notional
    = principal cash returned
      + realized principal loss
      + unresolved principal exposure
```

This identity is more informative than a senior label. Attachment means the
amount of aggregate realized loss absorbed below a tranche before it is first
impaired. Detachment means the loss at which it is exhausted. Neither is a
rating, a replenishing threshold, or evidence that the pool is safe.

## Non-principal return waterfall

Principal and non-principal cash remain separate ledgers. After applying the
fixed `q`, actual non-principal pool receipts pay:

1. the senior tranche up to its remaining lifetime priority cap;
2. each intermediate tranche in reverse attachment order up to its remaining
   cap; and
3. every remaining amount to the first-loss residual.

A priority cap is an allocation ceiling on cash that actually exists. It is
not a coupon, accrued interest, payment in kind, guaranteed return, or new
claim against a project. Unused cap expires at the horizon. Non-principal cash
may improve an investor's total economic return, but it does not relabel or
reduce gross project principal loss.

Version 0.1 issues every tranche at par. A discount would leave the reserve
underfunded. A premium would create surplus cash requiring a named recipient
and use. A later secondary-price sensitivity may model a separate
buyer-to-seller transfer, but that price must not enter the project cash or
loss waterfall.

## Investor risk and return report

For every tranche and complete joint path, the engine reports:

- month-zero par subscription and additional pool-cost calls;
- underlying project principal cash and unused-reserve principal return;
- priority or residual non-principal cash;
- total distributions, signed net cash, cash multiple, and return fraction;
- resolved principal loss at horizon and unresolved principal exposure;
- all-in cash shortfall and NPV at the tranche's declared physical hurdle;
- principal impairment and exhaustion; and
- dated cash flows and principal cash-weighted average life.

Across the central probabilities and every measure feasible within the
declared candidate probability envelope, it
reports minimum, central, and maximum expected cash, loss, exposure, NPV,
scenario cash multiple, impairment, exhaustion, ES95, ES99, and NPV-shortfall
ES95/ES99. Every scalar endpoint retains its own feasible probability witness.
Endpoints from different rows are not one combined forecast.

The capital-stack engine does not assume that its declared `q` is economically
adequate. It reports the success-participation term's robust NPV target, tests
the selected `q` against the minimum expected underlying NPV, and publishes any
shortfall. A failing `q` remains analyzable so investors can see why the claim
does not work; it is not relabelled as a supported term and is never silently
re-solved inside the waterfall.

The reported cash multiple range is `E[pathwise cash multiple]`; it is not a
ratio of independently optimized cash endpoints and is not annualized. The
pathwise net-return fraction is `E[(cash / calls) - 1]`; it is not IRR, an
annualized return, or an expected market return. NPV is a physical-measure
sensitivity at the declared hurdle, not fair value.

Weighted-average life uses a common probability measure. For scenario `s`:

```text
Q_s = total principal cash paid to the tranche
T_s = sum_t (t / 12) * principal cash_s,t

WAL(p) = sum_s p_s T_s / sum_s p_s Q_s
```

The engine solves the exact minimum and maximum ratio over the bounded
probability simplex by repeatedly projecting `T - rQ`. It publishes a feasible
binding witness and a numerical certification residual. WAL is absent if any
measure feasible within the candidate set has expected principal cash at or below the
`1e-10`-million numerical tolerance. Lost principal
is never treated as a horizon payment merely to produce a duration.

## Synthetic hand fixture

All cash amounts in this fixture are synthetic `DEMO` millions unless stated
otherwise; percentages and years are labeled separately.

The fixture has two projects, each with a 10-million commitment, and three
tranches:

| Tranche | Attachment | Detachment | Notional | Non-principal rule |
|---|---:|---:|---:|---|
| First-loss residual | 0 | 4 | 4 | Receives remaining cash |
| Intermediate | 4 | 10 | 6 | Priority cap of 2 |
| Senior | 10 | 20 | 10 | Priority cap of 1 |

Every project draws fully in month zero, so this particular table has no
prefunding drag. Pool cost is 0.2. In common success, the pool receives 20 of
principal plus 6 of non-principal cash. In either single-project-loss state it
receives 12 of principal plus 3 of non-principal cash. In common loss it
receives only 4 of principal.

| State | First-loss distribution | Intermediate distribution | Senior distribution |
|---|---:|---:|---:|
| Common success | 7 | 8 | 11 |
| Either single loss | 0 | 4 | 11 |
| Common loss | 0 | 0 | 4 |

Under the central synthetic weights, expected tranche contributions and
distributions are:

| Tranche | Contribution | Expected distribution | Expected NPV at 0% |
|---|---:|---:|---:|
| First-loss residual | 4.04 | 4.34 | 0.30 |
| Intermediate | 6.06 | 6.40 | 0.34 |
| Senior | 10.10 | 10.86 | 0.76 |

The totals reconcile to the untranched pool: 20.20 of contributions, 21.60 of
expected distributions, and 1.40 of expected NPV. Packaging created no cash.

Probability uncertainty changes the conclusion by tranche. Worst expected NPV
is `-0.54` for first-loss, `-0.46` for intermediate, and `+0.20` for senior.
Thus all three look positive centrally, but only the senior term clears a zero
hurdle under every probability mix feasible within the candidate set. Senior impairment probability is
2% centrally and ranges from 1% to 10%; its loss ES99 is 6 of its 10 notional. The
model does not call that tranche riskless. Senior principal WAL is about 1.919
years centrally and ranges from 1.872 to 1.938 years under common witnesses.

The numbers are invented mechanics, not a claim about cellular-agriculture
returns, loss rates, dependence, or investor appetite.

## Direct cohort-to-claim bridge

The same executable now has a `--joint-cohort` mode. It loads one
SHA-256-bound cohort package, evaluates the raw ledger, constructs the primary
physical-probability outer set, and applies that exact generated set to the
capital stack using the same in-memory portfolio. It accepts no second
portfolio path, no manual probability fallback, and no Goodman diagnostic as a
waterfall input. A readable but statistically blocked cohort exits with status
`3` and produces no tranche analysis.

Before the waterfall, the report shows each project's exact expected draw,
receipt, outstanding-exposure, resolved-loss-at-horizon, impairment,
negative-NPV, and pre-pool-cost NPV ranges at the selected fixed `q`. Every
scalar endpoint has its own feasible probability witness, so project or metric
extrema cannot be summed. The report also attributes pool ES95 and ES99 loss to
projects under one shared adverse probability and fractional-tail witness,
printing both vectors and the pool total. Those contributions add to pool ES;
they are not independently maximized project charges.

In the candidate-only cohort fixture, each 10-million project has a central
expected loss of 2 and a candidate-set range from 0 to about 8.062. Each project's
impairment probability ranges from 0% to about 80.620%. Under the shared
maximum-pool-tail witness, each contributes 10 to both pool ES95 and ES99. The
20-million pool's declared `q=1` has central NPV of `-1.0` and minimum expected
NPV of about `-11.949`, so it misses its zero target. The senior 10-million
claim is positive centrally (`0.68`) but its expected NPV can fall to about
`-4.668`; its impairment probability ranges from 0% to about 50.620%. The
engineering therefore refuses to call the synthetic senior claim robustly
attractive merely because subordination improves its central result.

Only the portfolio and raw cohort ledger are bound by `cohort.cfg`. The strict
participation and stack term files are normalized replay inputs but are not
part of that hash binding. Printed cohort, portfolio, and ledger normalizations
are semantic audit renderings that retain the original raw-file hashes; they
must be rehashed before rebinding as a new package. The report states both
boundaries explicitly.

## Research and market precedents

The design follows a disciplined subset of established language while not
claiming an established cellular-agriculture asset class. Fernandez, Stein,
and Lo's proposed research-backed obligation pools biomedical development
rights and allocates cash through senior debt, junior debt, and equity; its
performance results are simulations, not issued-market evidence
([Nature Biotechnology](https://www.nature.com/articles/nbt.2374)). Hull, Lo,
and Stein show why small common-factor correlation can sharply weaken pooled
long-shot performance and state directly that tranching reallocates rather
than creates value
([author paper](https://www.rogermstein.com/wp-content/uploads/LongShots_12.pdf)).

One useful realized life-science analogue is the Global Health Investment
Fund: a pooled late-stage fund using equity and mezzanine instruments with a
partial Gates Foundation/Sida loss guarantee, not rated basic-research debt
([World Bank/IFC case study](https://documents1.worldbank.org/curated/en/959631487668386111/pdf/112812-WP-GHIF-PUBLIC.pdf)).
IFC's blended-finance principles require minimum concessionality,
additionality, crowding-in, commercial sustainability, and disclosure of the
subsidy rather than calling first-loss support free diversification
([IFC](https://www.ifc.org/en/what-we-do/sector-expertise/blended-finance/how-blended-finance-works)).

The practical implication is stage-dependent leverage. Shared science and
public evidence may require grants. Early technical and scale-up risk requires
equity or funded first-loss capital. Participating debt becomes plausible only
after validated milestones and attachable licensing, royalty, procurement, or
offtake cash. Senior debt requires those cash rights or a separately disclosed
creditworthy support provider. The detailed evidence/inference boundary is in
[Financial-Engineering Precedents v0.1](FINANCIAL_ENGINEERING_PRECEDENTS_V0_1.md).

## Falsification and next work

The stack fails as an investable idea when:

- the untranched pool has no credible success cash after costs and time;
- common biological, scale-up, supplier, buyer, regulatory, energy, or funding
  shocks erase the apparent diversification;
- priority caps exceed obtainable contractual non-principal rights;
- full funding produces unacceptable reserve drag;
- first-loss capital is described as protection but is not actually funded;
- reserve return is called project profit;
- a favorable senior expected result hides catastrophic exhaustion or an
  adverse probability witness; or
- a subsidy or guarantee is concealed inside an attachment label.

The next empirical step is not tranche optimization. It is to populate the
common project interface with enforceable cash rights, staged draws, recovery,
common-factor states, and defensible candidate probability bounds for real
research, pilot, and first-industrial candidates. Only then should investor
hurdles, price, legal bankruptcy remoteness, reserve custody, tax, ratings, and
regulatory treatment be researched for an actual transaction.

Implementation is in [`capital_stack.hpp`](../include/naturalehia/cellular_finance/capital_stack.hpp),
[`capital_stack.cpp`](../src/capital_stack.cpp), and the strict parser in
[`capital_stack_config.cpp`](../src/capital_stack_config.cpp). The thin
evidence bridge is in
[`joint_cohort_capital_stack.hpp`](../include/naturalehia/cellular_finance/joint_cohort_capital_stack.hpp)
and [`joint_cohort_capital_stack.cpp`](../src/joint_cohort_capital_stack.cpp).
The controlling
fixture is
[`two-project-capital-stack-synthetic.cfg`](../scenarios/two-project-capital-stack-synthetic.cfg),
and the verification record is
[`CAPITAL_STACK_VERIFICATION_V0_1.md`](CAPITAL_STACK_VERIFICATION_V0_1.md).
