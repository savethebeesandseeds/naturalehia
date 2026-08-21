# Research Roadmap

The roadmap is organized by evidence gates rather than promised dates. A stage
is complete only when its exit criteria are met and documented.

## Stage 0 — specification and software foundation

Current stage.

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

- Replace four binary points with continuous input-concentration axes.
- Compare additive, joint-coupling, competitive-binding, and conformational
  parity mechanisms.
- Propagate parameter uncertainty and quantify the size of the XOR operating
  region.
- Define activation, reset, symmetry, leakage, and cross-talk metrics.

Exit criterion: at least one explicit mechanism has a nonempty, uncertainty-
aware XOR region without relying on an undisclosed parameter fit.

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
