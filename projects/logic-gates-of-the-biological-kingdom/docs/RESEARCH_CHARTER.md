# Research Charter

## Central question

Can an open, reproducible design process produce a resettable protein-level
system whose measured response implements XOR for two defined molecular
inputs, and can the same physical architecture be reparameterized into other
Boolean functions?

This question concerns an engineered system. It does not assume that such a
gate occurs naturally, that a designed sequence will function because it folds,
or that one successful XOR implementation proves general biological
computation.

## Operational definition

An XOR gate must have two declared inputs, one declared output, concentration
and timing definitions for logical 0 and 1, and this steady-state mapping:

| A | B | Output |
| ---: | ---: | --- |
| 0 | 0 | OFF |
| 1 | 0 | ON |
| 0 | 1 | ON |
| 1 | 1 | OFF |

The molecular response remains analog. A Boolean description is acceptable
only when a fixed threshold separates the intended ON and OFF regions with a
reported margin. The endpoint classifier maps equality to ON for backward
compatibility. An operating-region success claim additionally requires a
strictly positive threshold margin, so equality at any regional extremum is
not accepted as evidence of separation.

For a combinational gate, the final output must not depend materially on the
order in which inputs arrived. Order dependence, hysteresis, or persistent
state may be valuable, but it must be described as memory or sequential logic
rather than silently counted as XOR.

## Claim vocabulary

Use the least advanced term supported by the evidence:

- **specification:** a desired input-output relation;
- **model:** an explicit mathematical approximation;
- **simulation:** a numerical result produced by a model;
- **candidate:** a proposed structure or sequence not yet validated;
- **predicted structure:** a computational output, not a solved structure;
- **measured state:** a physical observation with method and uncertainty;
- **validated gate:** replicated behavior meeting preregistered gate criteria;
- **platform:** a mechanism shown to transfer across more than one gate or
  output, not merely proposed to do so.

The words “working protein,” “designed gate,” “fast,” “robust,” and “first”
require direct supporting evidence and a documented comparison basis.

## Evidence rules

1. Claims never outrun evidence.
2. Success thresholds are fixed before held-out or experimental results are
   interpreted.
3. Four selected measurements do not substitute for a two-dimensional
   concentration-response surface.
4. Activation and reset kinetics are reported separately.
5. Negative results, excluded candidates, and changed assumptions remain part
   of the record.
6. Structure prediction confidence is not treated as evidence of switching,
   binding affinity, catalysis, or experimental function.
7. A result must be reproducible from recorded code, inputs, configuration,
   units, tool versions, and random seeds.
8. A parameter range must be named according to its basis. An independently
   chosen stress box is not a confidence interval, posterior distribution, or
   empirical uncertainty estimate.

## Initial quantitative metric

For response values `p00`, `p10`, `p01`, and `p11` on a common scale, the
initial XOR separation margin is:

```text
margin = min(p10, p01) - max(p00, p11)
```

A positive margin means that some threshold separates the four endpoint
states. It does not establish a broad operating region, acceptable noise,
successful reset behavior, a mechanism, or biological validity. Later metrics
must extend this definition across declared concentration ranges and
experimental uncertainty.

## Stage 1 mechanistic scope

The current equilibrium milestone replaces binary inputs with declared low and
high concentration intervals. It evaluates a two-conformation binding model:

```text
Q_s(a,b) = 1 + a/K_A,s + b/K_B,s
             + omega_s * a*b/(K_A,s*K_B,s)

P_on = Z_ON/(Z_ON + Z_OFF),  Z_s = w_s * Q_s.
```

The output sigmoid is therefore derived from state partition functions rather
than imposed as a fitted Hill curve. Regional ON floors and OFF ceilings are
located at concentration corners exactly in real arithmetic for this model;
reported values are binary64 approximations. Independent parameter boxes may
be used to stress the conclusion, but they carry no statistical meaning unless
supported by a separate measurement and inference procedure.

Independent binding, `omega_ON = omega_OFF = 1`, is the null model. Its ON/OFF
odds factor into separate A and B effects, so two inputs that each favor ON
cannot jointly turn the system back OFF. A modeled XOR region therefore
requires a non-additive mechanism, such as state-dependent double-binding
coupling, and its existence remains only a model result.

The equations, proof sketches, metrics, fixture, and limitations are specified
in [the Stage 1 equilibrium-model document](EQUILIBRIUM_MODEL.md). Even a
positive robust margin does not show that a compatible fold, sequence, kinetic
response, or biological implementation exists.

The [Stage 1 acceptance protocol](STAGE_1_ACCEPTANCE_PROTOCOL.md) separates
raw regional bounds from optional caller-supplied criteria. Missing criteria
remain `not_assessed`. Overall acceptance requires all four criteria to be
supplied and to pass strictly; supplying only a subset cannot yield a pass. Its
single-high floor imbalance is a narrow model statistic, intended OFF activity
is not cross-talk, and the regression profile has no biological standing.

## Reproducibility record

Quantitative work should preserve:

- repository commit and dirty-state status;
- compiler, build type, operating system, and hardware description;
- dependency names, versions, licenses, and integrity hashes;
- complete model parameters with units;
- dataset or structure provenance and redistribution terms;
- random-number generator and seed where randomness is used;
- all acceptance, exclusion, and failure criteria; and
- raw results or an explicit reason they cannot be redistributed.

## Revision

This charter is part of the research design, not promotional text. Material
changes should be reviewed like code, explain what evidence or risk motivated
them, and never erase the historical conditions under which earlier claims
were made.
