# Fostering Cellular Agriculture white paper

This directory contains the publication source, bibliography, and released PDF
for the project's technical financial white paper.

- `fostering-cellular-agriculture-white-paper.tex` is the LaTeX source.
- `references.bib` is the source bibliography.
- `fostering-cellular-agriculture-white-paper.pdf` is the public artifact.

Version 1.3 adds the empirical-underwriting boundary, a controlled-cohort
evidence standard, and the retained Meatable / Netherlands Enterprise Agency
development-credit precedent. It also records the later resolution to dissolve
and wind down Meatable and the Agronomics equity write-off without treating
either as evidence of RVO credit default, recovery, remission, or loss. The
three controlled transaction dossiers remain ineligible for pricing or loss
calibration; the paper preserves the v0.2 frontier rejection, the legacy v0.1
record, and all execution non-claims.

The immutable publication identity is the Git tag
`fca-whitepaper-v1.3.0`. `release-manifest-v1.3.tsv` binds the released paper,
bibliography, scenario inputs, published WebAssembly runtime, and browser
fixture by lowercase SHA-256. Create the manifest only after the verified PDF
has been copied into this directory; the manifest intentionally does not hash
itself.

The paper must be built with `latexmk` inside the project's
`documents-latex:bookworm` container. From the repository root, the equivalent
Linux-container command is:

```sh
docker run --rm --name documents-latex-fca-whitepaper \
  --mount type=bind,source=/mnt/host/c/Work/Naturalehia/projects/fostering-cellular-agriculture,target=/work \
  -w /work/whitepaper documents-latex:bookworm \
  latexmk -pdf -interaction=nonstopmode -halt-on-error -file-line-error \
  -outdir=build fostering-cellular-agriculture-white-paper.tex
```

Copy the verified file from `build/` to the released PDF path only after
structural and visual review. The numerical case in the paper is synthetic and
is not a security price, offer, forecast, rating, or proof of financeability.
