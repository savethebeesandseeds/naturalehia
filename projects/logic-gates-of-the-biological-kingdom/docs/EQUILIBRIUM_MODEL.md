# Stage 1 Equilibrium Model

Status: current mechanistic milestone, 2026-08-25.

This model asks a deliberately narrow question: can a two-state equilibrium
system produce a continuous response surface with separated XOR operating
regions? It is a statistical-mechanical model, not a folding simulation, a
sequence-design method, or evidence of a functional protein.

## States and binding polynomials

Let `a` and `b` be the nonnegative activities of inputs A and B. In the current
implementation they are represented by concentrations. For each conformation
`s` in `{OFF, ON}`, define

```text
Q_s(a,b) = 1 + a/K_A,s + b/K_B,s
             + omega_s * a*b/(K_A,s*K_B,s).
```

The four terms represent unbound, A-bound, B-bound, and doubly bound
microstates. The parameters are:

| Parameter | Meaning | Units and domain |
| --- | --- | --- |
| `a`, `b` | Input activities, represented as concentrations | Same concentration unit as their corresponding `K`; nonnegative |
| `K_A,s`, `K_B,s` | State-specific dissociation scales | Concentration; finite and strictly positive |
| `omega_s` | State-specific double-binding coupling factor | Dimensionless and strictly positive |
| `g` | Apo log population ratio, `log(w_ON/w_OFF)` | Dimensionless and finite |

`omega_s = 1` is independent binding within state `s`; values above or below
one favor or penalize double occupancy relative to that independent reference.
This is an effective equilibrium parameter, not by itself a structural
explanation.

Choose the irrelevant common scale so that `w_OFF = 1` and `w_ON = exp(g)`.
The state partition functions and active probability are

```text
Z_OFF = Q_OFF
Z_ON  = exp(g) * Q_ON

P_on = Z_ON/(Z_ON + Z_OFF)
     = sigmoid(log(Z_ON) - log(Z_OFF)).
```

Implementations evaluate each `log(Q_s)` with a log-sum-exp over its four
positive terms and apply a branch-stable sigmoid. This avoids needless
overflow and underflow without changing the mathematical model.

## Operating regions and metrics

Each logical input is a closed concentration interval. The four rectangular
regions are the Cartesian products `low_A x low_B`, `high_A x low_B`,
`low_A x high_B`, and `high_A x high_B`. Define

```text
on_floor        = min(P_on over the 10 and 01 regions)
off_ceiling     = max(P_on over the 00 and 11 regions)
separation      = on_floor - off_ceiling
threshold_margin = min(on_floor - threshold,
                       threshold - off_ceiling)
```

The convention is strict: an intended ON region must remain strictly above
the threshold and an intended OFF region strictly below it. Equality is a
failure, not a favorable tie. A positive separation means some threshold
exists; a positive threshold margin means the declared threshold works.
When a nonnegative required separation is declared, it passes only when
`separation` is strictly greater than that requirement; equality also fails.
`off_ceiling` records worst-case modeled intended-OFF ON-macrostate occupancy,
while the two individual ON floors should be inspected separately to expose
asymmetry. It is not a measurement of reporter leakage or off-target activity.

The implementation evaluates probabilities in IEEE 754 binary64. A pass flag
requires the computed clearance to exceed `64 * epsilon` (approximately
`1.42e-14` in probability units). This is a fixed operational deadband applied
to already-rounded results, not a bound on accumulated floating-point error.
Equality and positive clearances within it fail. A pass means only that the
computed result cleared the software convention; it does not certify the
real-arithmetic inequality. The deadband is not experimental uncertainty or a
confidence interval. Callers that require certified rounding must use a
validated, outward-rounded numerical method.

For fixed parameters the real-arithmetic extrema occur exactly at the four
rectangle corners. Fixing `b` makes `Z_ON/Z_OFF` a ratio of two positive affine
functions of `a`. Its derivative has a constant sign with respect to `a`, so an
extremum lies at an `a` endpoint. Repeating the argument in `b` leaves only the
four corners. Reported values are binary64 approximations to those analytic
extrema. This is a property of this positive bilinear model, not a claim that
corner sampling is sufficient for an arbitrary biochemical model. Sampled
linear or logarithmic surfaces are inspection aids; their grid density does
not determine or certify these bounds.

## Independent-binding null

When `omega_ON = omega_OFF = 1`, each binding polynomial factorizes:

```text
Q_s = (1 + a/K_A,s) * (1 + b/K_B,s).
```

The ON/OFF odds consequently factor into an apo term, an A-only term, and a
B-only term. If increasing A favors ON and increasing B favors ON, both factors
increase. Their combined high-input odds cannot then fall below either
single-high condition to create XOR. Equivalently, the four corner odds obey a
multiplicative identity, so two individually activating inputs cannot make the
double-input corner inactive under one threshold.

The independent case is therefore the null model. XOR requires an additional
non-additive mechanism here, represented minimally by different double-binding
couplings in ON and OFF. If that coupled two-state model lacks a useful
operating region, additional conformational states or a different mechanism
must be considered rather than hidden in a fit.

## Parameter boxes

Parameter boxes are independent stress ranges chosen by the analyst. They are
not confidence intervals, posterior distributions, measurement errors, or
evidence that any biological system has those values.

The real-arithmetic extrema for independent closed parameter intervals occur
at analytic parameter-box corners because the monotonic direction of each
parameter is known:

| Increase in parameter | Direction of `P_on` |
| --- | ---: |
| `g` | non-decreases |
| `K_A,ON` or `K_B,ON` | non-increases |
| `omega_ON` | non-decreases |
| `K_A,OFF` or `K_B,OFF` | non-decreases |
| `omega_OFF` | non-increases |

Thus the minimum uses the lower `g`, upper ON-state `K` values, lower
`omega_ON`, lower OFF-state `K` values, and upper `omega_OFF`; the maximum uses
the opposite endpoints. Combining those directions with the concentration
corner result locates the analytic extrema for this box model without
interpreting the box statistically. Directions can be non-strict at zero
input or when the corresponding occupancy term vanishes; this does not change
which interval endpoints bound the probability. Reported values remain
binary64 approximations.

## Illustrative regression fixture

The checked fixture uses dimensionless normalized concentrations and is
constructed to exercise the equations:

| Quantity | Value |
| --- | ---: |
| `g` | `-4` |
| `K_A,OFF`, `K_B,OFF`, `omega_OFF` | `1`, `1`, `10` |
| `K_A,ON`, `K_B,ON`, `omega_ON` | `0.01`, `0.01`, `0.001` |
| A and B low interval | `[0, 0.01]` |
| A and B high interval | `[3, 10]` |
| Threshold | `0.5` |

Its nominal model bounds are `on_floor = 0.562295`,
`off_ceiling = 0.115416`, `separation = 0.446879`, and
`threshold_margin = 0.062295`.

The companion stress box sets `g` to `[-4.05, -3.95]` and varies each of the
six strictly positive parameters independently by plus or minus five percent.
The resulting robust bounds are `on_floor = 0.526164`,
`off_ceiling = 0.144181`, `separation = 0.381983`, and
`threshold_margin = 0.026164`.

These numbers are numerical regression fixtures. They were not fitted to,
estimated from, or validated against biological measurements. The independent
variation also ignores correlations that a physical system may impose.

## Remaining Stage 1 work

- The [Stage 1 acceptance protocol](STAGE_1_ACCEPTANCE_PROTOCOL.md) now defines
  single-high floor imbalance, a response-envelope gap statistic,
  intended OFF activity, tri-state outcomes, and strict caller-supplied
  criteria. It deliberately supplies no scientific cutoff.
- Define cross-talk only against an explicit off-target panel or model. The
  current `off_ceiling` measures intended-state leakage, not cross-talk.
- Record reset-time and response-time acceptance definitions for a later
  kinetic model. An equilibrium surface cannot evaluate either quantity.
- Extend the current paired coupling audit with a correctly bounded
  competitive architecture only after its partition model and parameter
  provenance are reviewed. Do not score an algebraically equivalent parity
  interpretation as an independent mechanism.

## Assumptions and limitations

- The system is at thermodynamic equilibrium and has only two output-relevant
  conformational macrostates.
- Each state has one effective site for each input and one effective coupling
  factor for double occupancy.
- Input activities are approximated by free concentrations; ligand depletion,
  non-ideal solution behavior, competitors, and off-target binding are absent.
- `P_on` is treated as the output. Reporter transfer functions, expression,
  folding yield, aggregation, degradation, and measurement noise are absent.
- The model is steady-state. It makes no claim about speed, reset time,
  hysteresis, path dependence, energy consumption, or repeated cycles.
- Parameter-box success establishes robustness only to the declared synthetic
  stress ranges. It does not identify parameters or quantify experimental
  uncertainty.
- A nonempty modeled XOR region supports further computational study only. It
  does not imply that a compatible structure or sequence exists, much less a
  validated biological gate.

Primary sources and the limits of their relevance are recorded in
[`PRIOR_ART.md`](PRIOR_ART.md).
