# Contributing to The Elder Brother of Fauna

The Elder Brother of Fauna is a Naturalehia project, and contributions of
code, tests, documentation, design discussion, and reproducible performance
results are welcome.

The project is early-stage software. Prefer small, reviewable changes that make
their assumptions explicit and include enough tests for another contributor
to reproduce the result.

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
2. From the repository root, run
   `cd projects/the-elder-brother-of-fauna`.
3. Start the Debian environment with `bash container.sh up`.
4. Make the change, including tests and relevant documentation.
5. Build in a warning-enabled configuration and run the full test suite.
6. Open a pull request that explains the problem, approach, tradeoffs, and how
   the result was verified.

### Canonical build and test

```sh
bash container.sh exec make test
bash container.sh exec make gpu-test  # Required for GPU, CUDA, or LibTorch changes.
```

All routine compilation, testing, formatting, and debugging should happen in
the managed Debian container. The cross-platform CI matrix remains responsible
for portability checks outside that development environment.
All commands below assume the project directory selected in step 2.

Keep the environment boundaries explicit: `container.sh` owns host-side Docker
lifecycle, `setup.sh` owns idempotent in-container provisioning, and `Makefile`
owns build, test, GPU-smoke, and run tasks. Do not add Docker lifecycle commands
to `setup.sh` or project build targets to `container.sh`.

## Code organization

- Public headers belong in `engine/include/naturalehia/fauna/`.
- Engine implementation belongs in `engine/src/`.
- The command-line application belongs in `apps/cli/`.
- Automated tests belong in `tests/` and use the `*_tests.cpp` suffix.
- Keep platform-specific code behind a narrow interface and out of the core
  domain model where practical.

The library target is `naturalehia_fauna`, its public CMake alias is
`Naturalehia::Fauna`, and the command-line executable is `naturalehia-fauna`.
The CMake project and package names are both `NaturalehiaFauna`. The principal
test targets are `fauna_tests` and `fauna_association_tests`.

## C++ style

- Use the `naturalehia::fauna` namespace for project code.
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

### Association correctness

Identity-association changes must preserve its staged contract. Known hard
aliases reserve tracks before proximity association. Compatibility, time, and
uncertainty checks define candidate edges, and observations inside the
ambiguity margin remain blocked. Only the unresolved frame graph is assigned:
it must first maximize the number of valid continuing identities and then
minimize total normalized innovation among assignments of that cardinality.

Tests should include cases where the best local edge would reduce total match
count, equal-cardinality assignments with different total innovation, edge and
input permutations, exact ties, disconnected graphs, and malformed inputs.
Use a small independent oracle for randomized cases when practical. Do not make
tests assert a domain meaning for a particular equally optimal tie-break.

Distinct valid envelopes are processed by timestamp, source, and source
sequence. Duplicate ordering keys are invalid rather than interchangeable, and
call boundaries can expose partial source frames. Tests that split a timestamp
across calls must deliver known tags before untagged evidence, matching the
source-adapter contract.

### Performance baselines

The optional association benchmark is configured and run inside the managed
Debian container:

```sh
bash container.sh shell
cmake -S /workspace/naturalehia/projects/the-elder-brother-of-fauna \
  -B /work/naturalehia-build/the-elder-brother-of-fauna/association-benchmark \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNATURALEHIA_FAUNA_BUILD_BENCHMARKS=ON
cmake --build /work/naturalehia-build/the-elder-brother-of-fauna/association-benchmark \
  --target fauna_association_benchmark --parallel
/work/naturalehia-build/the-elder-brother-of-fauna/association-benchmark/fauna_association_benchmark
```

Benchmark reports should include the commit, compiler and version, build type,
workload parameters, candidate density where available, repetition count, and
complete output. Compare identical workloads and report variability rather
than selecting a single favorable run. Do not add wall-clock thresholds to
CTest.

The current association path is deliberately a correctness-first baseline:
candidate generation scans observations against tracks, and the global solver
has worst-case `O(F * V * E)` time and `O(V + E)` working space after edge
canonicalization. Proposals for spatial indexing, connected-component solving,
or algorithm replacement should begin with a reproducible profile showing the
relevant bottleneck and include correctness comparisons against the existing
solver.

## Pull request checklist

- [ ] The change is scoped and described clearly.
- [ ] New behavior is covered by deterministic tests.
- [ ] The full test suite passes locally.
- [ ] Public API or behavior changes are documented.
- [ ] No sensitive data, secrets, or unlicensed assets are included.
- [ ] New files carry no licensing terms that conflict with the project license.

## Licensing contributions

By submitting a contribution, you agree that it may be distributed under the
project's [MIT License](LICENSE).

Questions can be sent to [contact@waajacu.com](mailto:contact@waajacu.com).
