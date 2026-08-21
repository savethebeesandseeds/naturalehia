# The Elder Brother of Fauna

**The Elder Brother of Fauna is a Naturalehia project building an efficient
C++ engine for naming tracked animals and estimating the state of individuals
and colonies from heterogeneous observations.**

The engine builds a durable identity and observation history for animals. The
first use cases focus on individually tracked large mammals. Where individual
tracking is neither meaningful nor practical, a collective such as a bee hive
or ant colony can be represented as the tracked entity.

> **Project status:** The Elder Brother of Fauna is at an early development
> stage. Development currently uses deterministic synthetic observations; no
> production wildlife data sources are connected yet. The software must not be
> relied upon for animal welfare, safety, or operational decisions.

## Goals

- Keep identity stable while names, tags, sensors, and source-specific tracks
  change over time.
- Normalize different data sources into a compact, source-independent
  observation model.
- Estimate an entity's current state while preserving provenance, timestamps,
  and confidence.
- Process observations efficiently enough to grow from local experiments to
  large streaming workloads.
- Make simulations deterministic and easy to replay, test, and benchmark.

The engine treats a permanent entity identity and a temporary sensor track as
different concepts. An animal may have several names or aliases without
changing its `EntityId`, and a source may create several short-lived tracks
that ultimately refer to the same entity.

## System direction

```text
source adapter
    -> normalized observation
    -> identity association and state estimation
    -> in-memory entity registry
    -> query, export, or downstream analysis
```

The initial source is synthetic. Future adapters may represent camera traps,
acoustic sensors, GPS collars, field observations, or other ethically sourced
data without requiring the core engine to depend on those systems.

### Available now

- Individual-animal detections and colony population surveys in explicit local
  coordinate frames.
- Stable engine-owned IDs, automatic collision-safe names, renaming, and
  source-local external tag bindings.
- Hard-tag-first identity association, uncertainty-gated candidates, and
  deterministic frame-wide matching that preserves as many valid continuing
  identities as possible before minimizing total normalized innovation.
- Explicit counters for ambiguous, invalid, conflicting, or late evidence.
- Fixed-size constant-velocity Kalman filters for individuals, centroid and
  population filters for colonies, and tentative/confirmed/stale lifecycle
  states.
- A deterministic bounded synthetic source with motion, noise, missing
  detections, intermittently visible tags, and a no-wait command-line demo.

State is currently held in memory. A track's estimate is evaluated at its
`last_observed_at` timestamp; advancing the engine watermark ages lifecycle
state but does not extrapolate the public estimate to a later query time.

Source adapters emit increasing per-source sequences and nondecreasing
timestamps. If one timestamp must be split across bounded reads, tagged
observations are emitted before untagged detections so hard identity evidence
is reserved before proximity matching.

### Identity association v1

Association operates on each source-and-timestamp group presented in an
ingestion batch. Known hard aliases reserve their tracks first. Entity kind,
taxon, coordinate frame, observation time, and the configured uncertainty gate
then define the valid proximity candidates for the remaining observations. An
observation whose two leading candidates fall within the configured ambiguity
margin remains blocked and is reported as ambiguous, as it was before global
matching.

The unresolved candidate graph is solved with a lexicographic objective:

1. maximize the number of observations joined to valid existing tracks; then
2. among those maximum-cardinality assignments, minimize the sum of normalized
   innovation scores.

This frame-wide decision prevents a locally attractive match from consuming the
only valid track for another observation and fragmenting that identity into a
new track. It does not override aliases, compatibility rules, uncertainty
gates, ambiguity blocking, or the one-observation-per-track-per-source-frame
rule.

The solver canonicalizes candidate edges and resolves a fixed valid frame
deterministically. Ingestion also orders distinct envelopes by timestamp,
source, and source sequence, so merely reordering those envelopes within one
batch does not change their processing order. Those ordering keys are part of
the input contract: duplicate keys are invalid, changing sequence values can
change the result, and splitting one timestamp across calls can expose a
partial frame. When a frame is split, send known tags before untagged evidence.
Which equally optimal assignment is selected is deterministic for the current
track IDs and ordering keys, but downstream code should not treat that
tie-break as domain evidence or a stable public policy.

This is a correctness-first baseline, not a production-scale claim. Candidate
construction currently scans observations against tracks, and the global
solver has worst-case `O(F * V * E)` time and `O(V + E)` working space after
`O(E log E)` edge canonicalization, where `F` is the number of matches and `V`
and `E` are the residual graph's vertex and edge counts. Spatial
candidate indexing, independent-component decomposition, and more specialized
assignment algorithms are intentionally deferred until representative
benchmarks show where they are warranted.

### Core vocabulary

| Concept | Meaning |
| --- | --- |
| `EntityId` | Stable, opaque identity owned by the engine. |
| `TrackedEntity` | An individual animal or a colony-level entity. |
| `EntityKind` | Distinguishes an `Individual` from a `Colony`. |
| `SourceId` | Identifies the producer of an observation. |
| `Observation` | Timestamped evidence with source, measurements, and uncertainty. |
| `EstimatedState` | The engine's best estimate at the last observation, including uncertainty. |

## Debian development environment

Development is standardized on the digest-pinned Debian 13 container managed
by [`setup.sh`](setup.sh). No Dockerfile is used. The script creates a container
named `naturalehia`, provisions GCC, Clang, CMake, Ninja, ccache, debugging and
formatting tools, numerical and computer-vision libraries, CUDA 13.1 Update 1,
GPU-enabled LibTorch 2.13, cuDNN 9.20.0.48, cuSPARSELt 0.8.1, NCCL 2.29.7,
and NVSHMEM 3.4.5. Every APT installation uses
`--no-install-recommends`, and all local development builds run inside Linux.

Requirements are Bash, Docker's Linux container engine, and an NVIDIA-capable
Docker runtime. The locked CUDA and LibTorch artifacts currently require a
Debian 13 x86-64/amd64 image; `setup.sh` rejects incompatible image overrides.
The script deliberately requires a local Docker context because repository bind
mounts resolve on the daemon host. Create the environment and run the project
and GPU test suites from the repository root with:

```sh
cd projects/the-elder-brother-of-fauna
bash setup.sh test
bash setup.sh gpu-test
```

The remaining commands in this document assume that project directory is the
current working directory.

Useful commands:

```sh
bash setup.sh shell
bash setup.sh gpu-test
bash setup.sh run --steps 20 --individuals 5 --colonies 2 --seed 42
bash setup.sh status
bash setup.sh stop
```

The Naturalehia repository is bind-mounted at `/workspace/naturalehia`, and
this project is developed from
`/workspace/naturalehia/projects/the-elder-brother-of-fauna`. Linux build
products live below
`/work/naturalehia-build/the-elder-brother-of-fauna`; the development home
directory remains in the persistent `naturalehia-home` Docker volume.
Checksum-locked LibTorch and NVIDIA runtime releases, plus a root-only verified
download cache, live in the root-owned `naturalehia-gpu` volume. The CUDA
toolkit and Debian packages live in the replaceable container filesystem.
Compiler output and downloaded binaries do not contaminate the host checkout.
Build trees are isolated by container configuration, the exact installed
package manifest, GPU architecture, and artifact manifests, while ccache
remains reusable. `recreate` retains the previous idle container for rollback
until the replacement passes structural checks and the CUDA/LibTorch GPU
smokes. It preserves all managed volumes and refuses unmanaged containers or
same-named volumes rather than modifying them.

The base image digest, CUDA top-level package, LibTorch archive, and NVIDIA
runtime wheels are locked. Debian development packages and CUDA's dependency
closure intentionally follow the current signed Debian 13 and NVIDIA
repositories so fresh environments receive current security fixes; therefore a
fresh setup is not claimed to be bit-for-bit reproducible. The exact installed
package set is recorded and used to isolate that container's build tree.

The CUDA toolkit supplies `nvcc`, CUDA-GDB, Compute Sanitizer, headers,
profiling interfaces, and development libraries. LibTorch is installed at the
versioned path reported by `$LIBTORCH_ROOT`; `$CMAKE_PREFIX_PATH` is configured
for `find_package(Torch)`. Vendor headers and libraries are exposed through
`$CUDNN_ROOT`, `$CUSPARSELT_ROOT`, `$NCCL_ROOT`, and `$NVSHMEM_ROOT`.
`bash setup.sh gpu-test` detects the visible GPUs' compute capabilities,
compiles a native CUDA kernel and a LibTorch C++ program for them, and executes
both on the GPU. The LibTorch smoke includes a real cuDNN convolution. The
NVIDIA driver remains on
the host and is exposed through the NVIDIA Container Toolkit; it is never
installed inside the container.

The first setup downloads several gigabytes and can take time. CUDA packages,
LibTorch, and other third-party binaries retain their upstream licenses and are
not relicensed under this repository's MIT License; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Two future interfaces are published to loopback only:

- `127.0.0.1:8080` to container port `8080` for an HTTP API or UI.
- `127.0.0.1:50051` to container port `50051` for ingestion or gRPC.

No service listens on either port yet. Override the host ports with
`NATURALEHIA_HTTP_PORT` and `NATURALEHIA_INGEST_PORT` before first creation;
then use `bash setup.sh recreate` to apply an immutable container-setting
change. Run `bash setup.sh help` for the complete lifecycle interface.

The reusable library target is `naturalehia_fauna`, with public CMake alias
`Naturalehia::Fauna`. The command-line executable is `naturalehia-fauna`.
The CMake project and installed package are named `NaturalehiaFauna`. Public
headers are included as `<naturalehia/fauna/...>`, and the public C++ API lives
in the `naturalehia::fauna` namespace. Build the optional applications with the
`NATURALEHIA_FAUNA_BUILD_CLI` and `NATURALEHIA_FAUNA_BUILD_BENCHMARKS` CMake
options.

### Optional association benchmark

The association baseline is excluded from normal builds. Start the managed
Debian shell, configure a release build with benchmarks enabled, and run it
with:

```sh
bash setup.sh shell
cmake -S /workspace/naturalehia/projects/the-elder-brother-of-fauna \
  -B /work/naturalehia-build/the-elder-brother-of-fauna/association-benchmark \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DNATURALEHIA_FAUNA_BUILD_BENCHMARKS=ON
cmake --build /work/naturalehia-build/the-elder-brother-of-fauna/association-benchmark \
  --target fauna_association_benchmark --parallel
/work/naturalehia-build/the-elder-brother-of-fauna/association-benchmark/fauna_association_benchmark
```

Record the commit, compiler, configuration, workload parameters, and complete
benchmark output when comparing results. Timing is evidence for engineering
decisions, not a pass/fail test, so the benchmark is not registered with
CTest.

## Repository layout

```text
engine/include/naturalehia/fauna/  Public C++ API
engine/src/                        Engine implementation
apps/cli/                          Synthetic-data command-line application
tests/                             Deterministic automated tests
benchmarks/                        Optional deterministic performance baselines
cmake/                             Package configuration and GPU smoke tests
setup.sh                           Debian container lifecycle and Linux build entrypoint
```

## Design principles

- **Source independence:** source-specific formats stop at adapter boundaries.
- **Explicit uncertainty:** estimates retain confidence and provenance rather
  than presenting guesses as facts.
- **Determinism:** seeded simulations and replayable input make failures
  reproducible.
- **Efficiency by measurement:** favor streaming and bounded-memory designs,
  then validate optimizations with representative benchmarks.
- **Portable core:** keep the engine usable without a particular cloud, data
  store, or user interface.

## Roadmap

1. Establish replay, association, load, and long-duration benchmark baselines
   for the current entity, synthetic-source, and state-estimation foundations.
2. Use those profiles to decide when candidate indexing, graph-component
   decomposition, or a more specialized global-assignment algorithm is needed.
3. Add bounded out-of-order windows, explicit current-time prediction, and
   richer multi-source association policies.
4. Add durable observation history, replay, import, and export interfaces.
5. Develop real source adapters with conservation partners and appropriate data
   governance.

Scale is a design constraint, but correctness and a clear evidence trail come
first. The synthetic source lets the project exercise both before access to
real-world sources is arranged.

## Responsible use

Animal location and identity data can expose animals, habitats, and field teams
to harm. Contributions should minimize collection, avoid committing sensitive
coordinates or real tracking records, and make access-control and retention
needs explicit. The repository currently expects synthetic data only.

## Contributing

Contributions are welcome. Start with [CONTRIBUTING.md](CONTRIBUTING.md), which
describes the build workflow, naming conventions, testing expectations, and
requirements for data and asset provenance.

## License

The Elder Brother of Fauna software and documentation are available under
the [MIT License](LICENSE), copyright 2026 Naturalehia contributors.

Contact: [contact@waajacu.com](mailto:contact@waajacu.com)
