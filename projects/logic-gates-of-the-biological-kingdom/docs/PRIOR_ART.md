# Scientific Starting Points

Last reviewed: 2026-08-25.

This is a concise starting map, not a systematic literature review or a claim
of novelty. Contributors should search current primary literature before using
words such as “first,” “new,” or “unprecedented.”

## Equilibrium modeling and parameter interpretation

- Monod, Wyman, and Changeux introduced a concerted equilibrium model in which
  ligand affinities differ between conformational states and ligand binding
  shifts their populations.
  [Journal of Molecular Biology 12, 88–118 (1965)](https://doi.org/10.1016/S0022-2836(65)80285-6)
- Buchler, Gerland, and Hwa treated regulator concentrations as continuous
  inputs, calculated two-dimensional response surfaces, and analyzed
  combinatorial transcriptional logic including an XOR construction.
  [Proceedings of the National Academy of Sciences 100, 5136–5141 (2003)](https://doi.org/10.1073/pnas.0930314100)
- Razo-Mejia and colleagues tested a statistical-mechanical allostery model in
  which induction curves follow from state probabilities and free energies,
  illustrating why a partition-derived sigmoid carries more mechanistic
  meaning than a standalone phenomenological fit.
  [Cell Systems 6, 456–469.e10 (2018)](https://doi.org/10.1016/j.cels.2018.02.004)
- Gutenkunst and colleagues found broad, anisotropic parameter sensitivities
  across a collection of systems-biology models, motivating attention to
  prediction ranges rather than overinterpretation of a single fitted
  parameter vector.
  [PLoS Computational Biology 3, e189 (2007)](https://doi.org/10.1371/journal.pcbi.0030189)

These papers motivate discrete-state partition functions, continuous response
surfaces, and explicit parameter sensitivity. They do not validate this
project's two-state XOR mechanism. The MWC model is an idealized concerted
model, the Buchler construction concerns transcriptional regulation, the
Razo-Mejia validation concerns a one-input repressor, and the Gutenkunst result
does not prove that this smaller equilibrium model has the same sensitivity
structure. The Stage 1 ranges are consequently described as independent stress
boxes, not confidence intervals.

## Protein-level logic

- de Ronde, ten Wolde, and Mugler developed a statistical-mechanical framework
  in which a receptor heterodimer can realize all two-input Boolean functions,
  including XOR through ligand competition and antagonism.
  [Biophysical Journal 103, 1097–1107 (2012)](https://doi.org/10.1016/j.bpj.2012.07.040)
- Fink and colleagues constructed fast proteolysis-based signaling and Boolean
  logic circuits from split proteases and designed coiled coils.
  [Nature Chemical Biology 15, 115–122 (2019)](https://doi.org/10.1038/s41589-018-0181-6)
- Chen and colleagues experimentally demonstrated AND, OR, NAND, NOR, XNOR,
  and NOT gates from de novo designed protein interaction components.
  [Science 368, 78–84 (2020)](https://doi.org/10.1126/science.aay2790)
- Plaper and colleagues encoded a complete set of two-input Boolean protein
  logic functions in single gate polypeptides using inserted coiled-coil
  segments and protease inputs.
  [Cell Discovery 10 (2024)](https://doi.org/10.1038/s41421-023-00635-y)

These results mean this project must not claim to have invented protein XOR or
protein logic. A plausible research contribution would need a narrower basis,
such as a resettable, fully de novo, multistate XOR architecture that can be
sequence-reprogrammed while retaining a common input/output grammar.

The de Ronde XOR construction uses an asymmetric heterodimer in which one
ligand can bind either subunit while the other competes for one site. It is not
equivalent to simply deleting the double-occupancy term from the current
bilinear model. A fair implementation must preserve that topology, match model
complexity, and bound its nonmonotonic response analytically. The paper is a
theoretical capability result, not evidence that this project's fixture or a
future sequence is physically realizable.

## Dynamic and allosteric design

- Langan and colleagues designed bioactive protein switches by coupling
  conditionally exposed peptides to designed cages.
  [Nature 572, 205–210 (2019)](https://doi.org/10.1038/s41586-019-1432-8)
- Pillai and colleagues designed allosterically switchable protein assemblies
  by coupling peptide-responsive hinges to alternative oligomeric states.
  [Nature 632, 911–920 (2024)](https://doi.org/10.1038/s41586-024-07813-2)
- Guo and colleagues demonstrated deep-learning-guided design of proteins that
  exchange between controlled conformational states.
  [Science 388, eadr7094 (2025)](https://doi.org/10.1126/science.adr7094)

These works support the plausibility of multistate design. They do not show
that a generic inverse-folding run will produce XOR.

## Sequence and structure design

- ProteinMPNN maps specified protein backbones to compatible sequences and was
  validated across monomers, assemblies, and interfaces.
  [Science 378, 49–56 (2022)](https://doi.org/10.1126/science.add2187)
- RFdiffusion generates protein backbones for several classes of structural
  and functional specifications.
  [Nature 620, 1089–1100 (2023)](https://doi.org/10.1038/s41586-023-06415-8)

Inverse folding is therefore one layer of the intended project. XOR first
requires a physical mechanism, multiple target states, state-dependent energy
constraints, and negative design against unintended states. Experimental
characterization remains necessary.
