# Physical-Probability Envelope Engine v0.1

## What it does

The probability-envelope engine re-evaluates the financial meaning of one
fixed participation-pool scenario table under declared candidate probability
ranges rather than one precise distribution.

For scenario `i`, the input declares:

```text
lower_i <= central_i <= upper_i
sum_i probability_i = 1
```

The engine then reports the exact minimum, central result, and exact maximum of
each supported financial metric over every probability vector satisfying those
bounds. It also prints the full probability vector attaining each endpoint.

The underlying project paths are evaluated once by the ordinary pool engine.
Extremal probabilities reweight those fixed results; they never change a draw,
receipt, recovery, source, resolution, factor tag, or loss.

Version 0.1 accepts synthetic inputs only. It is a physical-`P` robustness
calculation, not a forecast, fair value, risk-neutral price, rating, offer, or
investment recommendation.

## Why this is financial engineering

The pool's expected loss or NPV is not credible if it depends on an arbitrary
single probability estimate. This module makes parameter exposure visible:

```text
fixed contractual cash paths
        +
declared candidate probability set
        ->
exact range of expected cash, loss, NPV, liquidity, and tail severity
```

This lets an investor distinguish three statements:

1. the central scenario table has positive expected NPV;
2. every distribution feasible within the declared candidate set has positive
   expected NPV; or
3. some distributions feasible within that set produce negative expected NPV.

Only the second is robust within the stated set. Even then, a positive
physical-`P` NPV is not an investable price.

## Strict companion format

The envelope is a separate file so the existing strict portfolio format and
cash-conserving adapters remain unchanged. The complete v0.1 shape is:

```ini
ambiguity.model_version=0.1.0
ambiguity.label=...
ambiguity.source_note=...
ambiguity.synthetic_inputs=true

scenario.count=N
scenario.1.id=...
scenario.1.central_weight=...
scenario.1.lower_weight=...
scenario.1.upper_weight=...
...
```

Every portfolio scenario must occur exactly once. `central_weight` is an audit
checksum: after normalization it must equal the same scenario's ordinary
portfolio weight. It is not a second base case. Identifiers, text, line length,
file size, row count, and generated witness count are bounded.

The parser rejects unknown, duplicate, missing, malformed, non-finite, unsafe,
or out-of-range values. The core additionally rejects:

- a central total outside the accepted near-one tolerance;
- a central probability outside its interval;
- a scenario ID mismatch or incomplete ID bijection;
- a central measure that differs from the pool input; or
- an infeasible set where lower bounds exceed unit mass or upper bounds cannot
  supply unit mass.

Lower and upper columns are component bounds, not probability vectors. Their
sums need only satisfy:

```text
sum(lower) <= 1 <= sum(upper)
```

`--print-normalized` emits both complete reloadable inputs with deterministic
ordering and round-trip precision.

## Exact endpoint algorithm

For a scenario value vector `x`:

```text
E[x] = sum_i probability_i * x_i
```

To minimize it, the engine begins at every lower bound, sorts scenarios by
`x_i` ascending, and fills each remaining capacity `upper_i - lower_i` until
unit probability is reached. To maximize, it fills from the largest `x_i`.
If unit mass cuts through an exact equal-value block, the engine fills every
atom in that block in proportion to its remaining probability capacity. An
exchange argument shows that moving any feasible
probability mass from a higher value to an unfilled lower value cannot increase
the minimum; the reverse proves the maximum. Complexity is `O(N log N)` per
metric. The pro-rata block rule does not change the endpoint value; it prevents
an economically identical witness from changing merely because a scenario is
renamed or reordered.

The same low-to-high and high-to-low measures are respectively the
first-order-stochastically smallest and largest feasible distributions for a
fixed loss vector. The engine evaluates the ordinary fractional worst-tail
average on those witnesses, giving exact ES95 and ES99 bounds under this
box-plus-sum constraint.

The prepared projector exposes both operations for arbitrary finite values
keyed by scenario ID: linear expectation and upper expected shortfall at an
explicit tail probability. The tail projection returns its own fractional-tail
result and full endpoint witnesses; it is not mislabeled as an expectation.
This lets later contract modules project provider payout tails without
duplicating or weakening the probability optimization. Raw NPV must not be
passed to upper ES as though its high tail were downside; a caller must first
construct a non-negative NPV-shortfall value.

That proof does not automatically extend to ambiguity sets containing moment,
factor, copula, or cross-scenario coupling constraints. A future richer set
will require its own optimizer and verification.

## Reported ranges

Version 0.1 reports exact componentwise ranges for:

- expected project draws, investor receipts, pool costs, and outstanding
  principal;
- expected realized principal loss and impairment probability;
- expected physical-hurdle NPV and negative-NPV probability;
- expected peak same-month draw, gross funding need, and cumulative outlay;
- nominal and present-value receipts for each of the seven external-source
  categories;
- for every project, expected draws, receipts, outstanding principal, realized
  loss, NPV before shared pool costs, impairment probability, and negative-NPV
  probability;
- principal-loss ES95 and ES99; and
- non-negative NPV-shortfall ES95 and ES99.

Every scalar endpoint has its own witness. Different metrics' endpoints need
not be simultaneously attainable. In particular, independently optimized
source minima or maxima must not be added. Version 0.1 also does not subtract
independently optimized standalone and pool tail values to claim a “robust
diversification benefit”; that could combine incompatible witnesses.

Project tail concentration uses a different object. For one shared probability
measure `p` and tail mass `tau`, let `y_i` be the portion of scenario `i` inside
the highest aggregate-loss tail. Exact loss ties at the boundary receive the
same pro-rata fraction of their probability mass. For project `j`:

```text
tail contribution_j(p, tau)
    = (1 / tau) * sum_i y_i * project_loss_ij
```

The engine reports this attribution at the minimum-pool-ES witness, declared
central measure, and maximum-pool-ES witness for both 5% and 1% tails. Every
project in one column uses the same `p` and `y`, so contributions add exactly
to pool ES. These three columns are not minimum, central, and maximum of each
project's own contribution. No independently optimized project charges are
added.

## Hand-calculated synthetic fixture

The companion fixture uses the existing two-project cash paths:

| Joint state | Central | Bounds | Principal loss | NPV | Commercial receipt | Recovery receipt |
|---|---:|---:|---:|---:|---:|---:|
| both succeed | 62% | 50%–70% | 0 | 5.8 | 26 | 0 |
| culture loses, scale-up succeeds | 18% | 10%–25% | 8 | -5.2 | 13 | 2 |
| culture succeeds, scale-up loses | 18% | 10%–25% | 8 | -5.2 | 13 | 2 |
| both lose | 2% | 1%–10% | 16 | -16.2 | 0 | 4 |

All monetary amounts are invented `DEMO` millions. The lower-bound sum is
`0.71`; the upper-bound sum is `1.30`.

The exact results are:

| Metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| expected principal loss | 2.48 | 3.20 | 4.80 |
| expected investor receipts | 19.40 | 21.60 | 22.59 |
| expected pool costs | 0.20 | 0.20 | 0.20 |
| impairment probability | 30% | 38% | 50% |
| expected NPV | -0.80 | 1.40 | 2.39 |
| negative-NPV probability | 30% | 38% | 50% |
| principal-loss ES95 | 9.60 | 11.20 | 16.00 |
| principal-loss ES99 | 16.00 | 16.00 | 16.00 |
| NPV-shortfall ES95 | 7.40 | 9.60 | 16.20 |
| NPV-shortfall ES99 | 16.20 | 16.20 | 16.20 |
| expected commercial receipt | 18.20 | 20.80 | 21.97 |
| expected recovery receipt | 0.62 | 0.80 | 1.20 |

Each project has the following symmetric ranges in this particular fixture:

| Project metric | Minimum | Central | Maximum |
|---|---:|---:|---:|
| expected receipts | 9.15 | 10.80 | 11.79 |
| expected realized loss | 0.88 | 1.60 | 2.80 |
| impairment probability | 11% | 20% | 35% |
| expected NPV before pool costs | -0.85 | 0.80 | 1.79 |

At the common minimum-pool-ES, central, and maximum-pool-ES measures, each
project contributes respectively `4.8 / 5.6 / 8.0` to pool ES95. Each
contributes `8.0 / 8.0 / 8.0` to pool ES99. The pair sums reproduce the pool
values `9.6 / 11.2 / 16.0` and `16.0 / 16.0 / 16.0` exactly.

Draws, outstanding principal, and all three reported liquidity peaks are
constant across these four paths, so their probability ranges collapse to one
number. The independently minimized commercial and recovery receipts do not
share one witness; adding `18.20 + 0.62` would not give the minimum total
receipt. The total-receipts range is the coherent nominal receipt comparison;
the NPV range is the coherent discounted net-cash comparison after draws and
pool costs.

The minimum-loss witness is:

```text
both lose                         0.01
both succeed                      0.70
culture loses, scale-up succeeds 0.145
culture succeeds, scale-up loses 0.145
```

The maximum-loss witness is:

```text
both lose                         0.10
both succeed                      0.50
culture loses, scale-up succeeds 0.20
culture succeeds, scale-up loses 0.20
```

The two equal-loss states receive equal mass because their remaining capacities
are equal. With unequal capacities the partial tied block is still filled pro
rata to capacity. The selected witness is canonical and auditable, but no claim
is made that it is the only probability measure attaining the same scalar
endpoint.

## Build and run

From the project directory:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

On Windows with a multi-configuration generator:

```powershell
.\build\dev\Debug\naturalehia-probability-envelope.exe `
  .\scenarios\two-project-participation-pool-synthetic.cfg `
  .\scenarios\two-project-probability-envelope-synthetic.cfg
```

Append `--print-normalized` to print both complete canonical inputs.

## Remaining boundary

The envelope makes uncertainty in fixed scenario weights measurable; it does
not establish that the joint scenario table is complete or that any bound is
empirical. Real calibration must meet
[Portfolio Calibration and Probability-Uncertainty Standard v0.1](PORTFOLIO_CALIBRATION_STANDARD_V0_1.md).

Pricing remains a later research stage. Before it, the project needs at least
one controlled cohort that can populate the common financial interface,
evidence-backed joint bounds, contractual claim terms, counterparty and
recovery analysis, and untranched economics that remain credible under the
probability envelope.
