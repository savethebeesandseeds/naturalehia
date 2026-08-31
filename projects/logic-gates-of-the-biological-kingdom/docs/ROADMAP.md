# Research Roadmap

The roadmap is organized by evidence gates rather than promised dates. A stage
is complete only when its exit criteria are met and documented.

## Stage 0 — specification and software foundation

Complete as of 2026-08-25 at baseline revision
`f9b8944be06f4a425bd596c47f8f836d3ba75e3c`. See the
[Stage 0 closeout](STAGE_0_CLOSEOUT.md) for the evidence record and its limits.

- Define canonical input ordering, XOR truth, threshold behavior, and the XOR
  separation margin.
- Provide a dependency-free C++ library, CLI, deterministic tests, Linux CI,
  installable CMake package, and reproducible build entrypoint.
- Document claim boundaries, prior work, provenance rules, and responsible-use
  limits.

Exit criterion: two supported Linux toolchains build, test, install, and consume
the library; every current output is correctly described as a mathematical
model result.

## Stage 1 — thermodynamic operating regions

Current stage.

Delivered in the current milestone:

- Replace four binary points with continuous input-concentration axes.
- Implement the explicit two-state binding-polynomial model described in the
  [equilibrium-model specification](EQUILIBRIUM_MODEL.md), with partition-
  derived `P_on` and log-domain evaluation that avoids direct partition-function
  overflow.
- Treat independent binding as the null model, then evaluate whether
  state-specific double-binding coupling creates a nonempty XOR region.
- Locate analytic operating extrema over declared concentration rectangles and
  independent parameter stress boxes, with documented binary64 decision
  semantics.
- Define and test a narrow single-high floor-imbalance statistic, intended OFF
  activity, strict tri-state acceptance outcomes, and criterion provenance in
  the [Stage 1 acceptance protocol](STAGE_1_ACCEPTANCE_PROTOCOL.md).
- Audit the state-specific coupling terms against a paired independent-binding
  ablation without refitting or claiming general mechanism selection.

Remaining before Stage 1 closeout:

- Implement and analytically bound a parameter-count-matched competitive
  architecture, then compare it under an explicitly declared protocol rather
  than a sampling grid or an undisclosed parameter fit.
- Complete the conformational-parity identifiability audit. Treat parity as an
  interpretation when its partitions are restricted to the present bilinear
  family; require additional observables or structural constraints before
  scoring it as a distinct mechanism.
- Preserve cross-talk, response time, and reset as `not_assessed`. Their data
  requirements and future protocol definitions belong in Stage 1, but their
  numerical evaluation is deferred to an off-target study and a later kinetic
  model.

Exit criterion: at least one explicit mechanism has a nonempty XOR region over
declared independent stress ranges; the paired independent-binding ablation,
the analytically bounded competitive comparator, and the conformational-parity
identifiability audit are complete under documented comparison rules. No result
relies on an undisclosed parameter fit. Cross-talk, response time, and reset
remain explicitly `not_assessed` until their stated evidence requirements are
met. Stress ranges are not described as statistical uncertainty without a
measurement or inference basis.

## Stage 2 — multistate structural objectives

- Define the intended apo, singly occupied, and doubly occupied ensembles.
- Express positive design for intended states and negative design for wrong
  states, off-target complexes, and aggregation-prone alternatives.
- Select open-source, locally runnable C/C++ inference or simulation backends.
- Record model licenses, weights, training-data disclosures, versions, and
  integrity hashes.

Exit criterion: the scoring objective distinguishes the required four states
on controlled benchmarks and does not equate single-structure confidence with
gate function.

## Stage 3 — computational candidates

- Generate diverse candidate backbones and sequences under the multistate
  objective.
- Keep discovery and evaluation sets separate.
- Publish candidate inclusion and exclusion criteria, including failed and
  unstable designs.
- Use independent structural, energetic, and aggregation checks where their
  limitations are documented.

Exit criterion: a reviewable candidate set with complete provenance and no
claim of experimental function.

## Stage 4 — reviewed cell-free validation

- Complete applicable institutional biosafety and synthesis screening before
  ordering or handling biological material.
- Begin with non-pathogenic, non-toxic components and orthogonal peptide inputs
  in an appropriately contained, cell-free assay.
- Measure the full two-dimensional response surface, relevant controls,
  activation and reset, repeated cycles, path dependence, folding, binding,
  aggregation, stability, and soluble yield.
- Replicate results independently before using “validated gate.”

Exit criterion: preregistered XOR acceptance criteria are met by replicated
measurements, or the negative result is published with equal care.

## Stage 5 — a reprogrammable family

- Retune a conserved architecture into at least one contrasting gate, such as
  XNOR, NAND, or NOR.
- Standardize a composable molecular output rather than coupling the scientific
  claim to one reporter.
- Measure response speed, reset, spatial arrangement, expression yield, and
  cross-talk as separate optimization objectives.

Exit criterion: transfer is demonstrated experimentally. Until then, “other
gates may follow” remains a hypothesis rather than a platform claim.
