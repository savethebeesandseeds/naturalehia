# Stage 1 Steady-State Acceptance Protocol

Status: implemented model-assessment protocol, 2026-08-25.

This protocol defines what the equilibrium software can assess before any
structure or sequence search begins. It does not define biological success.
Every value below is conditional on a declared model, parameter set or stress
box, concentration windows, and concentration units.

## Regional quantities

For XOR region `r`, let

```text
L_r = minimum modeled P_on over the declared region and parameter box
U_r = maximum modeled P_on over the declared region and parameter box.
```

The software reports the four quantities needed for acceptance separately:

```text
input_a_only_on_floor          = L_10
input_b_only_on_floor          = L_01
basal_off_activity_ceiling     = U_00
joint_high_off_activity_ceiling = U_11
```

Aggregates are then

```text
on_floor                     = min(L_10, L_01)
intended_off_activity_ceiling = max(U_00, U_11)
separation                   = on_floor - intended_off_activity_ceiling.
```

`intended_off_activity_ceiling` is modeled ON-macrostate occupancy in the two
intended OFF regions. It is not off-target binding, reporter leakage, or
cross-talk.

## Single-high balance

The narrow descriptive statistic is

```text
single_high_floor_imbalance = abs(L_10 - L_01).
```

This compares two modeled regional floors. It is not full molecular symmetry and
does not bound pointwise A/B exchange symmetry across a parameter box. In
particular, the statistic need not worsen monotonically when a stress box is
widened.

The software also reports the stronger response-envelope quantity

```text
single_high_response_gap_upper_bound =
    max(U_10 - L_01, U_01 - L_10).
```

In the mathematical interval model, it bounds the absolute difference between
any two values drawn from the complete single-high response envelopes. The
reported binary64 value is an approximation, not an outward-rounded certified
upper bound. The quantity also penalizes variation within each region, so
failure does not by itself demonstrate A/B asymmetry. A future pointwise
symmetry claim would require a declared mapping between normalized A and B
concentrations and a separately reviewed optimization method.

## Criteria and outcomes

The API accepts four optional criteria:

- a fixed output threshold `t`;
- a minimum separation `S_min`;
- a maximum intended OFF activity `L_max`;
- a maximum single-high floor imbalance `D_max`.

No criterion has a scientific default. Each present criterion uses a strict
binary64 decision convention:

```text
threshold passes when min(on_floor - t,
                          t - intended_off_activity_ceiling) > tau
separation passes when separation - S_min > tau
intended OFF activity passes when L_max - intended_off_activity_ceiling > tau
floor balance passes when D_max - single_high_floor_imbalance > tau

tau = 64 * binary64 epsilon.
```

Equality and positive clearance within `tau` fail. Here `tau` is a fixed
operational deadband applied to already-rounded binary64 values. It is not a
bound on accumulated floating-point error, measurement uncertainty, or a
certified interval-arithmetic error bound. `pass` means only that the computed
clearance exceeded this software convention; it does not certify the
corresponding real-arithmetic inequality.

Each component and the overall result has one of three outcomes:

- `not_assessed`: no criterion was supplied;
- `pass`: the criterion was supplied and cleared strictly;
- `fail`: the criterion was supplied but did not clear.

Overall acceptance is `pass` only when all four criteria are present and pass.
Any observed component failure makes the overall result `fail`; otherwise a
missing component keeps it `not_assessed`. Scientific failure is a valid
program result and does not produce a process error.

## Criterion provenance

A scientific criteria profile must be identified, versioned, and locked before
candidate screening. Its record must include its basis, units, model version,
operating windows, parameter provenance, and the commit that evaluated it.
Changing a limit creates a new profile; it must not overwrite the earlier
result.

Before real candidate screening, the numerical implementation must add
validated, outward-rounded bounds (or an equivalently reviewed certification
method). The present binary64 engine is suitable for transparent model
development and software regression, not certified decisions near an
acceptance boundary.

The checked command-line example uses this post hoc software-regression profile:

| Profile | Threshold | Minimum separation | Maximum intended OFF activity | Maximum floor imbalance |
| --- | ---: | ---: | ---: | ---: |
| `illustrative-regression-v1` | `0.5` | `0.30` | `0.15` | `0.05` |

These values were selected around an already constructed mathematical fixture.
They have no empirical or biological basis and must not be reused as candidate
acceptance criteria.

## Coupling audit

The `coupling-audit` command evaluates a paired ablation:

1. the declared state-specific double-binding model;
2. the same apo and dissociation-constant boxes with only both `omega`
   intervals forced to exactly one.

There is no refit. This tests whether the declared result depends on the
coupling terms for this parameterization and these apo/dissociation-constant
boxes. It is not a likelihood-ratio test, a family-wide necessity result, a
ranking of biological mechanisms, or evidence that the declared coupling is
physically realizable.

Run the raw audit without acceptance criteria:

```sh
bash container.sh exec make coupling-audit
```

Run the regression-only protocol explicitly with:

```sh
bash container.sh exec make coupling-audit \
  AUDIT_ARGS='--criteria-label illustrative-regression-v1 --criteria-threshold 0.5 --criteria-min-separation 0.30 --criteria-max-intended-off 0.15 --criteria-max-floor-imbalance 0.05'
```

`AUDIT_ARGS` is shell-parsed and is intended only for trusted,
developer-supplied values.

## Evidence outside the equilibrium model

Missing evidence is never serialized as a zero or a favorable Boolean value.
The coupling audit currently records cross-talk, response time, and reset as
`not_assessed`.

Cross-talk requires a versioned off-target panel, identities, concentrations,
context, controls, response definition, replicates, and an aggregation rule.
Any future conclusion is conditional on that panel.

Response time must be recorded for every relevant directed transition. A
future kinetic protocol may define

```text
T_i_to_j = first time the output enters the destination acceptance band
           and remains there for a declared dwell time.
```

Sampled trajectories require interval or right censoring and a declared
observation horizon.

Reset requires more than observing an OFF output. Output recovery and
functional recovery must be recorded separately, followed by a standardized
rechallenge showing that the expected single-high ON response returns.

## Next mechanism-capability audit

The current bilinear response family is not a fair implementation of every
competitive-binding architecture. A literature-grounded candidate for a
future comparator is the asymmetric `Q_UW` heterodimer described by de Ronde,
ten Wolde, and Mugler, in which one ligand can bind either subunit and the
other competes for one of those sites. That theoretical result motivates a
model; it is not experimental evidence for a realizable XOR protein.
[Biophysical Journal 103, 1097–1107 (2012)](https://doi.org/10.1016/j.bpj.2012.07.040)

With cooperative factors fixed to one, a provisional mathematical form is

```text
Q_s = (1 + a/K_AU,s) * (1 + a/K_AW,s + b/K_BW,s).
```

Before adding that comparator, the project must review its partition
polynomial, analytic stationary-point bounds, parameter-count parity, and
parameter provenance. A sampling grid will not be accepted as proof of a
regional bound.

A two-input equilibrium “conformational parity” interpretation whose ON and
OFF partition polynomials are already restricted to positive bilinear form
collapses algebraically into the current response family. In this abstraction,
that restriction permits at most one binding event per input and excludes
terms such as `a^2`, `b^2`, and higher occupancies. Positivity alone does not
impose the restriction. Such a bilinear parity model will therefore be treated
as an interpretation or identifiability audit, not scored as an independent
mechanism unless structural constraints or additional observables make it
distinguishable.

Specifically, each output partition has the form

```text
Z_y = c_y,00 + c_y,10*a + c_y,01*b + c_y,11*a*b,
```

which maps to the present binding polynomial by scaling out `c_y,00` and
setting `K_A,y = c_y,00/c_y,10`, `K_B,y = c_y,00/c_y,01`, and
`omega_y = c_y,11*c_y,00/(c_y,10*c_y,01)`. Zero coefficients are boundary
limits rather than an additional positive-coefficient response family.
