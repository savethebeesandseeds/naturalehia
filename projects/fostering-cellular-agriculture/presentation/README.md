# Fostering Cellular Agriculture: one-page presentation

One-page IEEE-style synthesis prepared on 16 September 2026 from the original
31-page **Financing Cellular-Agriculture Scale-Up under Correlated Technical
Risk**, Working Paper v2.1 (1 September 2026).

Author: Santiago Restrepo Ruiz. Publisher: Waajacu Open Source Foundation.
Correspondence: Santiago.Restrepo.Ruiz@gmail.com.

## Read and edit

- `fostering-cellular-agriculture-one-page.pdf`: one-page reading copy.
- `fostering-cellular-agriculture-one-page.tex`: editable manuscript.
- `ieee-preamble.tex`: unchanged formatting from the Cryptographic Electronics reference.
- `references.bib`: three entries copied from the original bibliography.
- `vendor/IEEEtran/`: complete, unmodified IEEEtran distribution with its notices.
- `build.ps1` and `build.sh`: reproducible build helpers.
- `build/`: generated compilation records and page preview; ignored by Git.

The format matches
`C:\Work\applied_cryptography\crytographic_electronics\docs\white-paper`:
IEEEtran 1.8b conference, US Letter, two columns, normal 10-point body text,
standard margins and IEEEtran bibliography. No font or margin reduction is
used. This independent synthesis is not an IEEE publication or endorsement.

## Scope and provenance

The presentation focuses on the method and financial engineering: claim rights,
milestone draws, explicit portfolio dependence, funded loss allocation,
alternative guarantees, robust return constraints, conditional pricing and
funding integrity. At the author's request, the synthetic findings and numerical
case are omitted. They remain available in the full whitepaper. This editorial
focus does not change the source's results or establish live financeability.
No model or code results were recomputed.

The full paper, evidence, declarations and technical appendices remain in the
adjacent `../whitepaper/` directory. The source release is
[`fca-whitepaper-v2.1.0`](https://github.com/savethebeesandseeds/naturalehia/tree/fca-whitepaper-v2.1.0/projects/fostering-cellular-agriculture/whitepaper).
This new presentation is a derivative summary, not a replacement release or
an artifact covered by that tag's original release manifest. SHA-256 checks
confirmed all 18 preexisting files under `whitepaper/` were unchanged when
this presentation was delivered.

The source paper discloses OpenAI Codex computational assistance and assigns
authorship and scholarly responsibility to Santiago Restrepo Ruiz. Codex also
assisted with this condensation, formatting and layout checks. No new external
peer review is claimed. The underlying empirical and transaction limitations
remain those of Working Paper v2.1.

## Build in the established project environment

Use the existing persistent `naturalehia-fostering-cellular-agriculture`
container managed by the project's `container.sh`. It uses
`emscripten/emsdk:6.0.5@sha256:76a44fff907397784decc435115d07fcb9587a4f1504977f39f3745e538e3a1e`.
Container lifecycle and dependency provisioning remain with the existing
launcher and `setup.sh`; this directory defines no container, image, volume,
mount or package installation.

From the project root in Git Bash, with Docker Desktop running:

```sh
bash container.sh up
```

Then from this `presentation` directory in PowerShell:

```powershell
.\build.ps1 -Render
```

The helper requires the existing managed container to be running, copies the
editable sources into a unique `/tmp/fca-one-page-*` snapshot, invokes
latexmk/pdfLaTeX/BibTeX, retrieves build records and rejects multi-page output,
undefined citations or overfull boxes. `-Render` requires local Poppler's
`pdftoppm`; `pdfinfo`, if available, also checks the PDF's page count. The
container and snapshots are preserved. Inspect the rendered page before
publishing future edits. Neither helper reads or writes the original
whitepaper as part of compilation.

The equivalent direct Linux build, inside the established container, is:

```sh
bash presentation/build.sh
```

This writes `presentation/build/fostering-cellular-agriculture-one-page.pdf`;
copy the verified result to the presentation directory for reading.
