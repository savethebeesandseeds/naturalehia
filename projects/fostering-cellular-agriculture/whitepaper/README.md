# Fostering Cellular Agriculture white paper

This directory contains the publication source, bibliography, and released PDF
for the project's technical financial white paper.

- `fostering-cellular-agriculture-white-paper.tex` is the LaTeX source.
- `references.bib` is the source bibliography.
- `fostering-cellular-agriculture-white-paper.pdf` is the public artifact.

Version 2.0 recast the work as a conventional financial-engineering working
paper: a compact title page and abstract, explicit research question, formal
timeline and probability set, pricing and feasibility conditions, propositions
with proofs, complete sensitivity grid, expanded primary literature, empirical
boundary, declarations, and technical appendices. The ten-claim case remains
synthetic and rejects all 25 tested capital-stack candidates. A callable-capital
or warehouse layer remains a proposed funding-liquidity extension. Its v0.1
accounting, eligibility, provider-lineage, maturity, and nonanticipativity
mechanics are now checked, but it has no released file-backed transaction or
numerical candidate result.

Version 2.1 establishes the publication identity: Santiago Restrepo Ruiz is the
human author, Waajacu Open Source Foundation is the publisher, correspondence is
`Santiago.Restrepo.Ruiz@gmail.com`, and OpenAI Codex is disclosed as
computational assistance rather than authorship. It also reconciles the paper's
implementation boundary with the verified 75-test Emscripten release.

The immutable publication identity is the Git tag
`fca-whitepaper-v2.1.0`. `release-manifest-v2.1.tsv` binds the released paper,
bibliography, scenario inputs, funding-bridge implementation and verification,
published WebAssembly runtime, and browser fixture by lowercase SHA-256. The
earlier `fca-whitepaper-v2.0.0` tag and `release-manifest-v2.0.tsv` remain an
immutable prior release. Create the new manifest only after the verified PDF
has been copied into this directory; the manifest intentionally does not hash
itself.

The paper must be built with `latexmk` inside the persistent, digest-pinned
project container. `container.sh up` installs only missing TeX packages into
that container; no separate disposable LaTeX container is used. From the
project directory in Git Bash, WSL with Docker integration, or another Bash
host, run:

```sh
bash container.sh exec /bin/bash -lc \
  'cd whitepaper && latexmk -pdf -interaction=nonstopmode -halt-on-error -file-line-error -outdir=build-v21 fostering-cellular-agriculture-white-paper.tex'
```

Copy the verified file from `build-v21/` to the released PDF path only after
structural and visual review. The numerical case in the paper is synthetic and
is not a security price, offer, forecast, rating, or proof of financeability.
