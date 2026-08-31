# Stage 0 Closeout

Closeout date: 2026-08-25  
Baseline revision: `f9b8944be06f4a425bd596c47f8f836d3ba75e3c`

Stage 0 established the specification, software, and evidence practices needed
to begin mechanistic modeling. It did not design a protein or establish that a
biological XOR gate exists.

## Evidence record

| Exit requirement | Repository evidence | Local evidence at closeout | Result |
| --- | --- | --- | --- |
| Declare the XOR truth table, threshold convention, and separation metric | `docs/RESEARCH_CHARTER.md`; deterministic model tests | The endpoint evaluator and tests passed in the canonical Linux container | Complete |
| Build and test with two supported Linux toolchains | CMake presets, `Makefile`, and the Linux CI workflow | GCC Debug and Release builds and tests passed; the Clang ASan/UBSan run passed | Complete |
| Install and consume the CMake package | Installed-package configuration and `tests/package_consumer` | The downstream package-consumer build and test passed | Complete |
| Enforce source and shell quality checks | Uncrustify configuration, ShellCheck configuration, and `make quality` | ShellCheck and Uncrustify checks passed | Complete |
| Provide a persistent, inspectable Linux environment | `setup.sh`, pinned base-image digest, named-volume and toolchain records | Environment creation/resume, CLI execution, and `nvidia-smi` visibility passed | Complete |
| State provenance, claim, and responsible-research limits | Charter, prior-art map, contribution policy, and responsible-research policy | Documentation review found the current outputs described as mathematical model results | Complete |

The local results above were observed in the persistent canonical development
container. This repository does not retain a signed transcript or artifact for
that acceptance run. The workflow under `.github/workflows/` configures hosted
CI, but configuration is not proof that a hosted run occurred; this closeout
therefore makes no claim about a remote CI result.

GPU visibility in Stage 0 only confirmed access to the shared device and
driver. It was not a GPU numerical validation and is not evidence about the
scientific model.

## Decision

Stage 0 is complete. Stage 1 is the current stage. Its purpose is to replace
the phenomenological four-corner evaluator with an explicit, continuous
equilibrium mechanism and to determine where that mechanism can and cannot
meet the XOR specification.

Any future change that invalidates a Stage 0 exit requirement must reopen the
relevant requirement rather than silently preserving this status.
