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
reported margin.

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

## Initial quantitative metric

For response values `p00`, `p10`, `p01`, and `p11` on a common scale, the
initial XOR separation margin is:

```text
margin = min(p10, p01) - max(p00, p11)
```

A positive margin means that some threshold separates the four endpoint
states. It does not establish a broad operating region, acceptable noise,
successful reset behavior, a mechanism, or biological validity. Later metrics must extend
this definition across declared concentration ranges and experimental
uncertainty.

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
