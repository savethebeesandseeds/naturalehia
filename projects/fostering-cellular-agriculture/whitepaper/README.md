# Fostering Cellular Agriculture white paper

This directory contains the publication source, bibliography, and released PDF
for the project's technical financial white paper.

- `fostering-cellular-agriculture-white-paper.tex` is the LaTeX source.
- `references.bib` is the source bibliography.
- `fostering-cellular-agriculture-white-paper.pdf` is the public artifact.

Version 2.0 recasts the work as a conventional financial-engineering working
paper: a compact title page and abstract, explicit research question, formal
timeline and probability set, pricing and feasibility conditions, propositions
with proofs, complete sensitivity grid, expanded primary literature, empirical
boundary, declarations, and technical appendices. The ten-claim case remains
synthetic and rejects all 25 tested capital-stack candidates. A callable-capital
or warehouse layer is described only as a proposed funding-liquidity extension;
it has no released quantitative result until its accounting, eligibility,
recovery, provider and nonanticipativity controls pass independent review.

The immutable publication identity is the Git tag
`fca-whitepaper-v2.0.0`. `release-manifest-v2.0.tsv` binds the released paper,
bibliography, scenario inputs, published WebAssembly runtime, and browser
fixture by lowercase SHA-256. Create the manifest only after the verified PDF
has been copied into this directory; the manifest intentionally does not hash
itself.

The paper must be built with `latexmk` inside the project's
`documents-latex:bookworm` container. From Windows PowerShell, run:

```powershell
docker run --rm --name documents-latex-fca-whitepaper-v2 --mount "type=bind,source=C:\Work\Naturalehia\projects\fostering-cellular-agriculture,target=/work" -w /work/whitepaper documents-latex:bookworm latexmk -pdf -interaction=nonstopmode -halt-on-error -file-line-error -outdir=build-v2 fostering-cellular-agriculture-white-paper.tex
```

Copy the verified file from `build-v2/` to the released PDF path only after
structural and visual review. The numerical case in the paper is synthetic and
is not a security price, offer, forecast, rating, or proof of financeability.
