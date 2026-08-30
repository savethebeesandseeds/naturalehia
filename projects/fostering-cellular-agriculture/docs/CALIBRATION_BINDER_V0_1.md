# Calibration Binder v0.1

**Status:** implemented candidate release. Version 0.1 binds declared evidence
links and method labels to synthetic financial mechanics; it never authorizes
calibrated execution.

The strict build, adversarial controls, independent corrections, and residual
limitations are recorded in the
[Calibration Binder v0.1 verification record](CALIBRATION_BINDER_VERIFICATION_V0_1.md).

## The missing bridge

The evidence gate answers whether a controlled dossier satisfies compiled
minimum requirements. The participation-pool and probability-envelope engines
answer what a declared set of cash paths and weights implies financially.

Neither currently answers:

> Which retained fact and transformation supports this exact draw, receipt,
> recovery, scenario event, factor exposure, or probability bound?

The calibration binder closes that lineage gap without relaxing the current
synthetic-input boundary.

```text
retained evidence
        ↓
declared method and limitation
        ↓
exact normalized model input
        ↓
candidate path / probability envelope
        ↓
risk, exposure, loss and return report
```

## Candidate-only boundary

Version 0.1 has five non-negotiable properties:

1. both bound engine inputs must retain `synthetic_inputs=true`;
2. the binder must declare `candidate_only=true`;
3. the probability measure must be physical `P`;
4. every report states `calibrated_execution_authorized=false`; and
5. no output is fair value, a price, rating, legal opinion, capital ruling,
   offer, or recommendation.

Even a candidate whose evidence dossier passes every compiled gate remains a
candidate. A later release may open a controlled non-synthetic evaluator only
after a real binder has been independently challenged. Version 0.1 cannot be
used as a bypass.

## Bound artifacts

One strict binder configuration names and SHA-256 binds:

- the portfolio joint-scenario configuration;
- its physical-probability envelope;
- one evidence dossier;
- that dossier's evidence manifest; and
- an exact input-lineage manifest.

Paths are relative to and confined beneath the binder directory. Every digest
uses 64 lowercase hexadecimal characters. A changed byte, missing file,
escaped path, or mismatched bound-file digest invalidates the release.

The release header also fixes its schema version, binder, project and dossier
identifiers, as-of date, source note, candidate-only flag, and
probability-measure label. Version 0.1 accepts exactly one portfolio project
and requires both identifiers to match the loaded project and dossier.

## Exact input coverage

The engine's canonical printers define the target namespace. Version 0.1
enumerates every material normalized portfolio and ambiguity leaf. It excludes
schema and descriptive metadata—the model version, label, source note, and
synthetic-input declaration—and structural count fields. Currency label and
monetary basis are material because they determine what every cash amount
means; both require lineage. Excluded fields remain immutable within the
raw-file hash.

Every material target must appear exactly once in the lineage manifest.
Therefore:

- a missing target is an unsupported model input;
- a duplicate target creates conflicting provenance;
- an unknown target is an orphan assertion; and
- a changed input invalidates the artifact hash even if its target name stays
  the same.

The binder does not prove that a source is true or that a transformation is
correct. It proves that the analyst disclosed a complete, immutable mapping
that another reviewer can identify and challenge. Version 0.1's `method_id` is
a label, not a self-contained or executable method definition.

## Lineage record

Each strict tab-separated row contains:

| Field | Meaning |
|---|---|
| `input_id` | stable unique binder identifier |
| `target_path` | exact canonical model key |
| `input_class` | capital, transition, probability, recovery, dependence, qualified-output, commercial-cash, source-credit, cost, policy-hurdle, or instrument-term |
| `input_status` | observed, contractual, derived, estimated, transfer, hypothesis, stress, synthetic, or policy |
| `method_id` | stable label for a method, procedure, or explicit candidate construction |
| `evidence_record_ids` | semicolon-separated evidence records, or `NONE` where permitted |
| `requirement_ids` | corresponding compiled evidence requirements, or `NONE` where permitted |
| `limitations` | bounded reason the input may not transfer or be decision-ready |
| `update_or_retire` | fact that would revise or remove the input |

Observed, contractual, derived, estimated, and transfer statuses require
citations that qualify for controlled gate use on the evaluation date, and the
cited requirements must pass. A contractual row also requires an executed-
contract source. Synthetic, hypothesis, stress, and policy inputs may use
`NONE`, but the absence remains explicit.

Version 0.1 forbids source-derived statuses on probability inputs. It has no
bound population, inclusion, censoring, transfer, or challenger-method ledger,
so even a controlled record for another requirement cannot make a scenario
weight observed, contractual, derived, estimated, or transferred.

## Candidate status

Version 0.1 deliberately has one mechanically valid report state:
`structurally-checked-synthetic-candidate`. The report separately shows the
evidence dossier's status and highest permitted use, while each lineage row
preserves its own observed, contractual, derived, estimated, transfer,
hypothesis, stress, synthetic, or policy label.

This avoids compressing several different questions into a score. Exact
lineage coverage does not mean that every cited source is sufficient. A strong
dossier does not change the hard synthetic-input block. The candidate status
is not a credit grade and implies no market value, bankability,
diversification, or acceptable expected return.

## Financial identities outside v0.1

Exact leaf coverage is the minimum useful implementation. The following
semantic subledgers remain required before a controlled real calibration:

- hash-bound method definitions and exact value, unit, and transformation
  reconciliation;
- milestone-to-draw eligibility and cancelled availability;
- qualified output through release, contract eligibility, buyer acceptance,
  invoice, collection, and commercial cash;
- exposure, collateral or claim, realization cost, priority, recovery timing,
  and investor recovery;
- factor definitions, shared exposures, adverse horizons, and project maps;
  and
- event populations, numerators, denominators, unresolved observations,
  censoring, transfers, challengers, and probability-bound construction.

Capacity never reconciles directly to cash. Gross recovery never reconciles
directly to investor recovery. A different facility name never establishes
independence.

## Release tests

The implemented release rejects:

- unknown, missing, duplicate, unsafe, non-finite, or oversized fields;
- false candidate-only status or a non-`P` probability measure;
- `synthetic_inputs=false` in either engine input;
- hash drift, missing artifacts, or a path that escapes the binder directory;
- project/dossier scope mismatch, oversized artifacts, or post-parse hash
  drift;
- missing, duplicate, or orphan model targets;
- an incompatible target class;
- a source-derived status with no controlled qualifying citation, a failed
  requirement, a nonexistent record, a cited gap or contrary record, or
  requirement IDs that do not match the cited records; and
- any source-derived probability status before a population/method ledger
  exists.

The existing portfolio and probability outputs must be unchanged by the
binder. It adds provenance and a release boundary, not cash or probability.

The CLI returns success only when the candidate package is mechanically valid.
Its report still prints the dossier's fail-closed assessment and always prints
`calibrated_execution_authorized=false`. Therefore process success must not be
read as evidence readiness or investment eligibility.

## Run the implemented synthetic fixture

From a Windows multi-configuration build:

```powershell
.\build\dev\Debug\naturalehia-calibration-binder.exe `
  .\scenarios\calibration-binder-v0.1-synthetic\binder.cfg `
  --print-normalized
```

The fixture has one invented project and one invented scenario. Its 25
material targets exist to verify hashing, confinement, lineage, citation, and
hard-block behavior—not to demonstrate a financeable transaction.

## Current research conclusion

The [public calibration snapshot](PUBLIC_CALIBRATION_EVIDENCE_V0_1.md) can
define real candidate events and acquisition questions. It cannot populate a
defensible joint distribution or investor cash schedule. The Believer Wilson
public dossier remains a useful negative case: it shows precisely why public
capacity, regulatory, financing, and insolvency fragments do not constitute a
calibrated investable asset.
