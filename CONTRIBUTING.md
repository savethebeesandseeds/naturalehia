# Contributing to Naturalehia

Thank you for helping build Naturalehia. Contributions of code, tests,
documentation, design discussion, and reproducible performance results are all
welcome.

Naturalehia is early-stage software. Prefer small, reviewable changes that make
their assumptions explicit and include enough tests for another contributor to
reproduce the result.

## Before you begin

- Search existing issues and pull requests before starting overlapping work.
- For a substantial API, data-model, storage, or architecture change, open an
  issue first so the design can be discussed before implementation.
- Never commit credentials, precise real-world animal locations, private field
  records, or other sensitive conservation data. Use synthetic fixtures.
- Do not add artwork, fonts, photographs, datasets, or generated media without
  documented provenance and a license that permits redistribution.

## Development workflow

1. Fork the repository and create a focused branch.
2. Start the Debian environment with `bash setup.sh up`.
3. Make the change, including tests and relevant documentation.
4. Build in a warning-enabled configuration and run the full test suite.
5. Open a pull request that explains the problem, approach, tradeoffs, and how
   the result was verified.

### Canonical build and test

```sh
bash setup.sh test
bash setup.sh gpu-test  # Required when changing GPU, CUDA, or LibTorch integration.
```

All routine compilation, testing, formatting, and debugging should happen in
the managed Debian container. The cross-platform CI matrix remains responsible
for portability checks outside that development environment.

## Code organization

- Public headers belong in `engine/include/naturalehia/`.
- Engine implementation belongs in `engine/src/`.
- The command-line application belongs in `apps/naturalehia_cli/`.
- Automated tests belong in `tests/` and use the `*_tests.cpp` suffix.
- The root `src/` directory contains the pre-existing website assets. Do not
  place C++ source there or move those files as part of an engine change.
- Keep platform-specific code behind a narrow interface and out of the core
  domain model where practical.

The library target is `naturalehia_core`, its public CMake alias should use the
`Naturalehia::` namespace, and the command-line target is `naturalehia`.

## C++ style

- Use the `naturalehia` namespace for project code.
- Use `PascalCase` for classes, structs, enums, and type aliases.
- Use `snake_case` for functions, variables, parameters, files, and serialized
  field names.
- Use `kPascalCase` for constants.
- Prefer `enum class`, value semantics, RAII, and explicit ownership.
- Use fixed-width integer types at serialization and external API boundaries.
- Express units and time bases in names or types; do not rely on undocumented
  conventions.
- Avoid unnecessary allocation and copying in observation-processing paths, but
  support performance claims with measurements.
- Format touched C++ files with the repository's `.clang-format` configuration.

Keep permanent identity separate from source-specific tracks. Use stable opaque
IDs for entities; treat names and aliases as mutable metadata. New observation
fields should have documented units, timestamp semantics, provenance, and
confidence behavior.

## Tests and synthetic data

- Add a regression test for every bug fix.
- Keep tests deterministic. Synthetic generators must accept an explicit seed
  whenever randomness affects their output.
- Cover empty input, invalid input, boundary values, out-of-order timestamps,
  and repeated observations where relevant.
- Do not make tests depend on the network, wall-clock timing, or real animal
  data.
- Include benchmark methodology and representative workload parameters with
  performance-oriented changes.

## Pull request checklist

- [ ] The change is scoped and described clearly.
- [ ] New behavior is covered by deterministic tests.
- [ ] The full test suite passes locally.
- [ ] Public API or behavior changes are documented.
- [ ] No sensitive data, secrets, or unlicensed assets are included.
- [ ] New files carry no licensing terms that conflict with the project license.

## Licensing contributions

By submitting a contribution, you agree that it may be distributed under the
repository's [MIT License](LICENSE). The pre-existing visual assets identified
in [ASSET_LICENSES.md](ASSET_LICENSES.md) are excluded from that license while
their rights are being confirmed.

Questions can be sent to [contact@waajacu.com](mailto:contact@waajacu.com).
