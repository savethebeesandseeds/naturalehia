# Scientific Starting Points

Last reviewed: 2026-08-21.

This is a concise starting map, not a systematic literature review or a claim
of novelty. Contributors should search current primary literature before using
words such as “first,” “new,” or “unprecedented.”

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
