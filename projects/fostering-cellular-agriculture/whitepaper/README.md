# Fostering Cellular Agriculture white paper

This directory contains the publication source, bibliography, and released PDF
for the project's technical financial white paper.

- `fostering-cellular-agriculture-white-paper.tex` is the LaTeX source.
- `references.bib` is the source bibliography.
- `fostering-cellular-agriculture-white-paper.pdf` is the public artifact.

Version 1.2 adds the Capital Mobilization Frontier v0.2 same-pool rejection and
the conditional issue-price/support window while preserving the legacy v0.1
frontier record and all execution non-claims.

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
