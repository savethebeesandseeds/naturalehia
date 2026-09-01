# Contributing to Logic Gates of the Biological Kingdom

Thank you for helping build careful, open infrastructure for protein-level
logic research. Contributions of C or C++ code, tests, documentation,
mathematical models, design discussion, and reproducible negative results are
welcome.

## Before beginning

- Read the [research charter](docs/RESEARCH_CHARTER.md) and
  [responsible-research policy](docs/RESPONSIBLE_RESEARCH.md).
- Search existing issues and pull requests before starting overlapping work.
- Discuss substantial scientific, API, dependency, or architecture changes in
  an issue before implementation.
- Do not present a prediction, candidate, or simulation as experimental fact.
- Do not commit credentials, restricted data, unlicensed model weights,
  unpublished biological sequences, or sensitive experimental information.

## Supported workflow

Development and CI are Linux-first. Implementation code is C or C++. Project
automation uses GNU Make and CMake. The host-side `container.sh` manages and
accesses the persistent environment, while `setup.sh` performs idempotent
provisioning only inside it. Do not introduce Python project code or
automation.

Create or resume the environment, then run the full local check with:

```sh
bash container.sh up
bash container.sh exec make test
```

For sanitizer coverage:

```sh
bash container.sh exec make sanitize
```

Alternatively, use `bash container.sh shell` once and then invoke `make test`,
`make sanitize`, and other targets directly inside the container. Native Linux
contributors may run the same Makefile without the container.

## C++ conventions

- Use C++20 and the `naturalehia::protein_logic` namespace.
- Use `PascalCase` for types, `snake_case` for functions and variables, and
  `kPascalCase` for constants.
- Prefer value semantics, RAII, explicit units, and explicit ownership.
- Keep the public API small and dependency-light.
- Treat floating-point domain validation and numerical stability as part of
  correctness.
- Format touched C and C++ files with Uncrustify and the project
  `.uncrustify.cfg` configuration. `make quality` checks formatting and
  `make format` applies it.
- Compile without warnings under the supported GCC and Clang toolchains.

## Scientific contributions

Every quantitative result must identify:

- the model and its assumptions;
- input units, ranges, and thresholds;
- code revision, compiler, build type, and dependency versions;
- input provenance and, when applicable, hashes and random seeds;
- uncertainty, exclusions, failure criteria, and negative results; and
- the evidence level claimed in the project evidence ladder.

A logic-gate result must report the underlying response surface or explain why
one is not applicable. Gate thresholds and acceptance criteria should be fixed
before evaluating held-out or experimental results.

## Dependencies and provenance

New dependencies must have an OSI-approved license, be usable locally without
a proprietary service, be versioned or checksum-locked, and be recorded in
`THIRD_PARTY_NOTICES.md`. Open access and open weights are not automatically
the same as open-source licensing.

Do not add datasets, structures, model weights, generated assets, or biological
materials without source, license, and redistribution information.

## Pull request checklist

- [ ] The change is narrowly scoped and clearly described.
- [ ] New behavior has deterministic tests.
- [ ] Debug, Release, and relevant sanitizer checks pass on Linux.
- [ ] Public behavior and scientific assumptions are documented.
- [ ] Claims match the available evidence.
- [ ] Provenance and third-party licensing are complete.
- [ ] Responsible-research implications were considered.

By contributing, you agree that your contribution may be distributed under
the project's [MIT License](LICENSE).
