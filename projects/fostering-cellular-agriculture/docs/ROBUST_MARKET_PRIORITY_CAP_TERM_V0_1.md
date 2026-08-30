# Robust Market Non-Principal Priority-Cap Term v0.1

## Status and purpose

Status: implemented deterministic synthetic research term. The C++ evaluator,
strict configuration, CLI, retained fixture, and
[verification record](ROBUST_MARKET_PRIORITY_CAP_VERIFICATION_V0_1.md) are
complete. No result in this document is a transaction or a market quote.

This term asks one narrow question after a pool and its funded first-loss
structure have been defined:

> What is the smallest **tested** lifetime priority cap on actual
> non-principal cash that lets the market claim satisfy its declared physical-
> measure mandate at par, and what does granting that priority remove from the
> junior claim?

The term is an adequacy sensitivity for one contractual cash-allocation lever.
It is not a “return cap”: it does not cap principal, total investor gain,
annual yield, or a promised coupon. It neither discovers a market hurdle nor
estimates what an investor would pay.

It follows the implemented
[fully funded capital stack](CAPITAL_STACK_TERM_V0_1.md) and
[robust capital-mobilization frontier](ROBUST_CAPITAL_MOBILIZATION_FRONTIER_V0_1.md).
The frontier identifies viable tested combinations of success participation
and first loss while holding the market priority cap fixed. This term holds one
such `q` and `A` structure fixed and tests the cap itself.

## One variable; everything else fixed

Let:

```text
K = aggregate project commitment and top stack detachment
q = fixed fraction of declared scalable success cash
A = fixed funded junior first-loss amount
M = K - A = fixed market principal notional
B = tested market lifetime priority cap on actual non-principal cash
```

For every `B`, retain exactly two claims:

```text
funded junior loss-absorbing residual claim   [0,A]
market-facing priority claim                  [A,K]
```

Both subscribe their complete principal notionals at par at month zero. Pool
costs remain additional pro-rata calls, so the market investor's all-in cash
contribution can exceed `M`. Principal cash remains senior-first, principal
loss remains junior-first, and unresolved principal remains exposure rather
than an invented recovery.

The following must not change across the cap grid:

- project paths, draws, receipts, sources, dates, costs, and recoveries;
- scenario and event definitions and every probability bound;
- `q`, `A`, `K`, `M`, claim IDs, hurdles, and junior NPV target;
- principal priority, principal loss, impairment, and principal WAL; and
- aggregate project cash and gross project loss.

Changing `B` only reallocates non-principal cash already admitted by fixed
`q`. It cannot make a failed project pay cash that does not exist.

## Lifetime cash-allocation rule

Let `z_s,t(q)` be the actual non-principal cash available to the stack in
scenario `s` and month `t` after applying fixed `q`. This includes unscaled
non-principal source kinds already belonging to the stack. For a tested cap
`B`:

```text
remaining_s,t(B)
    = max(0, B - sum_{u<t} market_nonprincipal_s,u(B))

market_nonprincipal_s,t(B)
    = min(z_s,t(q), remaining_s,t(B))

junior_nonprincipal_s,t(B)
    = z_s,t(q) - market_nonprincipal_s,t(B)
```

Unused cap expires. It does not accrue, become principal, survive as a
receivable, or create a claim on another scenario. At every month and in every
scenario:

```text
market cash flow_s,t(B) + junior cash flow_s,t(B)
    = fixed aggregate stack cash flow_s,t
```

Increasing `B` therefore transfers dated cash from junior to market. It does
not increase pool cash.

## NPV and the conservation boundary

At fixed market hurdle `h_M` and junior hurdle `h_J`:

```text
NPV_M,s(B) = sum_t market CF_s,t(B) / (1+h_M)^(t/12)
NPV_J,s(B) = sum_t junior CF_s,t(B) / (1+h_J)^(t/12)
```

At one common pool hurdle `h_P`, value allocation must reconcile pathwise:

```text
NPV_M,s(B;h_P) + NPV_J,s(B;h_P)
    = fixed aggregate stack NPV_s(h_P)
```

Market and junior NPVs calculated at different own hurdles must not be added
and called aggregate NPV. Likewise, a robust market minimum and robust junior
minimum may use different adverse probability witnesses. They must not be
subtracted as though they were one state.

For probability polytope `P`:

```text
robust market NPV(B)
    = min over p in P of sum_s p_s NPV_M,s(B)

robust market NPV margin(B)
    = robust market NPV(B) / M

robust junior NPV(B)
    = min over p in P of sum_s p_s NPV_J,s(B)

junior concession(B)
    = max(0, declared junior target - robust junior NPV(B))
```

The concession is a physical-NPV benchmark gap. It is not automatically a
grant, subsidy, fiscal cost, accounting value, or price.

## Fixed eligibility and cap-sensitive mandates

Changing non-principal priority cannot repair principal risk. The term must
first report whether the fixed structure passes its declared:

- minimum robust aggregate pool NPV;
- maximum market expected principal-loss fraction;
- maximum market principal-loss ES95 and ES99 fractions;
- maximum market principal-impairment probability;
- maximum market principal-cash WAL; and
- maximum funded junior first-loss amount `A`.

If a fixed test fails, the result is `fixed-structure-ineligible`. A larger
cap must not be presented as a cure.

The cap-sensitive market mandates are:

- minimum robust market NPV margin;
- maximum market negative-NPV probability;
- maximum market NPV-shortfall ES95 fraction; and
- maximum market NPV-shortfall ES99 fraction.

At least one must be declared. Otherwise selecting the smallest grid value is
mathematically trivial and is not an adequacy result.

For scenario shortfall `S_s(B)=max(0,-NPV_M,s(B))` and tail mass `tau`:

```text
maximum ES_tau(B)
    = max over p in P, 0<=y<=p, sum(y)=tau
        (1/tau) * sum_s S_s(B)y_s
```

Negative-NPV probability is separately optimized from the pathwise sign
indicator. It is not inferred from expected NPV. Every endpoint retains its
own complete probability witness; tail endpoints also retain fractional tail
mass `y`.

An optional maximum junior concession creates a second boundary. Market
requirements generally impose a lower cap; the junior limit imposes an upper
cap. Their overlap can be empty.

## Finite-grid selection

Version 0.1 tests a declared, finite, canonically sorted grid of caps. The grid
must contain literal zero, its declared contractual ceiling, and the reference
cap in the supplied base stack. Values must be finite, unique, and
non-negative. A positive cap smaller than one base currency unit is rejected;
zero remains the explicit no-priority baseline.

For every tested cap, report separately:

```text
market_adequate(B) = every fixed and cap-sensitive market mandate passes

balanced(B) = market_adequate(B)
              and the optional junior-concession limit passes
```

The primary market result is the first tested market-adequate cap. The report
also gives the first tested balanced cap, the immediately lower tested cap,
all passing caps, and the contractual-ceiling result. If the market minimum
already breaches the junior limit, the status is
`market-and-junior-requirements-do-not-overlap`; a higher cap cannot repair it.

The term does not interpolate. “Minimum” always means minimum **tested** cap,
not a continuous root or optimized contract.

## Candidate and summary report

For every cap, retain and print:

- market and junior expected non-principal cash ranges;
- expired market cap capacity;
- market and junior contribution and total-distribution ranges;
- market and junior NPV minimum, central value, maximum, and witnesses;
- robust market NPV margin and junior concession;
- market negative-NPV probability and NPV-shortfall ES95/99;
- the fixed principal-loss, impairment, principal-cash, and WAL metrics;
- every declared pass/fail decision; and
- probability, tail, cash-allocation, cap, and NPV reconciliation residuals.

Cash multiple and pathwise net-return fraction may be shown as physical-
probability sensitivities. The latter is an expectation of scenario-level
ratios, not an annualized return and not a ratio of separately optimized cash
endpoints.

The grid-level controls must establish, within the disclosed numerical
tolerance:

- market contributions, principal cash, principal risk, and WAL are invariant;
- market non-principal cash and pathwise market NPV are nondecreasing in `B`;
- junior non-principal cash and pathwise junior NPV are nonincreasing;
- negative-NPV probability and NPV-shortfall tails are nonincreasing, although
  they can be flat or jump at a pathwise zero;
- market cash gained equals junior cash surrendered at every date and state;
- aggregate cash and pool-hurdle NPV are invariant; and
- the supplied base-stack object was not mutated.

Candidate and monotonicity comparisons use an absolute tolerance of `1e-10`
in the reported million-unit basis plus
`256 * machine_epsilon * max(1, abs(first), abs(second))`. The tolerance is a
floating-point classification control, not economic materiality, and is far
below the one-base-currency-unit minimum positive contractual cap.

Adverse witnesses and marginal cap value may change as the cap crosses
scenario cash levels. Monotonicity does not justify combining their endpoints.

## Exact synthetic hand fixture

Reuse the frontier fixture with invented `DEMO` millions:

```text
q = 25/28       A = 12       K = 20       M = 8
market hurdle = junior hurdle = pool hurdle = 0
junior target NPV = 0
market contribution in every state = 8.08
junior contribution in every state = 12.12
```

Let:

```text
u = min(B, 50/7)   // common-success non-principal cash
v = min(B, 25/7)   // either single-success state's non-principal cash
```

Market distributions are 4 in common loss, `8+u` in common success, and
`8+v` in either single-loss state. Market path NPVs are therefore:

```text
common loss       -4.08
common success     u - 0.08
either single loss v - 0.08
```

Over the declared event polytope:

```text
robust market NPV = -0.48 + 0.50u + 0.40v
central market NPV = -0.16 + 0.62u + 0.36v
maximum market NPV = -0.12 + 0.70u + 0.29v

robust junior NPV = 0.48 - 0.50u - 0.40v
junior concession = max(0, -0.48 + 0.50u + 0.40v)
```

For `0<=B<=25/7`:

```text
robust market NPV = 0.9B - 0.48
```

The exact zero-NPV cap is `B=8/15`. The declared test grid is:

```text
0, 0.08, 0.50, 8/15, 1.00
```

| `B` | Robust market NPV | Worst negative-NPV probability | NPV-shortfall ES95/99 | Robust junior NPV | Junior concession | Market result |
|---:|---:|---:|---:|---:|---:|---|
| 0 | -0.480 | 100% | 4.08 / 51% of `M` | 0.480 | 0 | fail |
| 0.08 | -0.408 | 10% mathematically | 4.08 / 51% of `M` | 0.408 | 0 | fail |
| 0.50 | -0.030 | 10% | 4.08 / 51% of `M` | 0.030 | 0 | fail |
| `8/15` | 0 | 10% | 4.08 / 51% of `M` | 0 | 0 | selected |
| 1.00 | 0.420 | 10% | 4.08 / 51% of `M` | -0.420 | 0.420 | passes market; transfers more cash |

The `B=0.08` sign boundary requires an explicit money tolerance or a just-
below/just-above regression; a binary floating-point approximation must not
silently determine the published classification.

Every cap has the same fixed market principal results:

```text
worst expected principal loss       0.40 = 5% of M
principal-loss ES95 and ES99        4.00 = 50% of M
worst principal impairment          10%
maximum principal WAL               about 1.922111 years
```

Worst market NPV-shortfall ES remains 4.08 at every cap because common loss
contains no transferable non-principal cash and can fill either tail. The cap
can repair robust expected NPV; it cannot repair that no-cash failure state.

The existing cap of 1.00 gives the market 0.42 of robust NPV and pushes the
junior claim 0.42 below its target. The tested cap `8/15` places both claims at
their stated zero-NPV hurdles. This is the term's concrete purpose: identify
enough priority to clear the mandate without granting unnecessary project
upside.

## Resource boundary and implementation architecture

Use a separate evaluator rather than changing frontier v0.1. Its fourth input
is one validated two-claim `CapitalStackConfig`; the fifth is this cap term.
For each cap, copy the stack, change only the identified market tranche's
`priority_nonprincipal_cap_million`, and call the public event-polytope stack
evaluator. Never mutate the caller's base stack.

The complete grid is guarded before evaluating any cap. With `C` cap
candidates, `N` projects, `S` scenarios, `E` events, horizon month `H`, and
the frontier's complete portfolio-record count `R`:

```text
C * [S * (S + E + 1)
     + R
     + N * S * (H + 1)
     + 2 * S * (H + 1)] <= 4,000,000
```

`R` includes cash-availability, draw, receipt, pool-cost, cash-source, and
factor-tag entries, plus each configured portfolio loss layer applied to each
scenario. A small internal pure helper should be shared with the frontier so
both terms use the same overflow-safe definition while preserving frontier
v0.1's public API and exact diagnostics.

Implemented and retained units are:

- `robust_market_priority_cap.hpp/.cpp`;
- strict `robust_market_priority_cap_config.hpp/.cpp`;
- `market_priority_cap_cli/main.cpp`;
- core, parser, normalized-replay, CLI, and resource-bound tests; and
- a retained synthetic fixture and independent numerical audit.

The `B=1` candidate must exactly reproduce the corresponding implemented
frontier candidate, including endpoint values and witnesses.

## Falsification tests

The term fails or reports no adequate cap when:

- the fixed pool fails aggregate or principal-risk eligibility;
- no actual non-principal cash exists;
- the contractual ceiling cannot meet the market mandate;
- a return-shortfall limit lies below the no-cash-state floor;
- market and junior requirements have no overlap;
- a candidate passes only because `q`, `A`, a hurdle, path, or probability
  changed;
- cash exceeds the cap or unused cap becomes a receivable;
- dated market and junior allocations do not reconcile to pool cash;
- a higher cap is selected while a lower tested cap passes;
- caps above cash saturation manufacture different economics;
- unequal claim hurdles produce a false NPV-conservation claim;
- grid or input ordering changes canonical output;
- parser normalization is not byte-stable; or
- candidate, probability, horizon, record, or loss-layer work exceeds its
  fail-closed resource bound.

Tests must include the exact `8/15` hand point, omitted-root grids, below/at/
above cap saturation, multiple payment dates, positive and unequal hurdles,
`q=0`, no passing cap, no market/junior overlap, near-minimum market notional,
and an `A=16` case proving that no principal write-down is not zero return risk.

## What the term does not establish

The following booleans remain false:

```text
continuous minimum or optimized contract claimed       false
market hurdle solved or empirically calibrated          false
expected investor return or annualized yield estimated  false
fair value, issue price, or market spread estimated     false
investor demand or suitability established              false
legal form, enforceability, or regulatory treatment     false
capital mobilization or crowding-in established         false
```

A passing cap is a tested contractual allocation under supplied physical
probabilities. It is not a coupon, promise, price, rating, offer, or investor
commitment. The honest result can be that existing success cash cannot satisfy
the market mandate without an unacceptable junior concession.
