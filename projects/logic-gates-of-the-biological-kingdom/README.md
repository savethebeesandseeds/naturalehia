# Logic Gates of the Biological Kingdom

**Logic Gates of the Biological Kingdom is a Linux-first C++ research project
for the open, auditable design of protein-level Boolean logic, beginning with a
two-input exclusive-OR (XOR) specification and an explicit reset
requirement.**

> **Project status: research concept / pre-experimental.** No functional
> protein logic gate has been designed or experimentally validated by this
> project. The current software evaluates a simplified mathematical
> specification. Its outputs are not protein sequences and are not evidence of
> biological function.

We want this work to be useful to people who come after us. That requires more
than publishing source code: assumptions must be visible, claims must remain
smaller than the evidence, failures must be learnable, and results must be
reproducible without a proprietary service.

## Mission

Build open tools and evidence practices for designing molecular systems whose
measured responses implement Boolean logic. The first scientific target is a
protein system with two defined inputs and the XOR truth table:

| Input A | Input B | Expected output |
| ---: | ---: | --- |
| 0 | 0 | OFF |
| 1 | 0 | ON |
| 0 | 1 | ON |
| 1 | 1 | OFF |

Molecular inputs are concentrations, not literal bits. A credible experiment
must therefore define concentration windows for 0 and 1 and measure the full
two-dimensional response surface rather than four selected points.

XOR is a useful first target because two independent activating effects cannot
produce it: the joint-input state needs an additional, non-additive coupling
that turns the response back off.

## Available now

The initial C++ library implements a small phenomenological log-odds model:

```text
log_odds(ON) = baseline + A * effect_a + B * effect_b
               + A * B * joint_effect
```

It evaluates all four input states, converts log odds to active probability,
and reports the separation between the intended XOR ON and OFF states. This is
a formal specification and a test bed for metrics. It does **not** simulate
folding, binding, allostery, molecular dynamics, or experimental noise.

The command-line program ships with an illustrative parameter set and accepts
alternatives:

```sh
bash setup.sh exec make run
bash setup.sh exec make run \
  RUN_ARGS='--baseline -4 --input-a 8 --input-b 8 --joint -16 --threshold 0.5'
```

`RUN_ARGS` is parsed by the container shell. Quote the complete assignment as
shown and use it only for trusted, developer-supplied arguments.

## Supported development environment

Linux is the supported development and CI platform. The canonical development
environment is a named, persistent Debian container managed entirely by
`setup.sh`; there is deliberately no Dockerfile. The project uses C++20,
CMake, GNU Make, Ninja, GCC or Clang, and the C++ standard library. No project
code or automation is written in Python, and setup verifies that no Python
interpreter is installed in the canonical container.

The host needs Bash and a local Docker installation using a Linux engine. The
default environment also requires an NVIDIA GPU made available through the
NVIDIA Container Toolkit. Create or resume the environment and run its checks
with:

```sh
bash setup.sh up
bash setup.sh exec make test
bash setup.sh exec make sanitize
bash setup.sh exec make gpu-test
```

The container is called `naturalehia-protein-logic`. It shares all host GPUs
with compute and utility capabilities, bind-mounts only this project, and
keeps build products and its developer home in named volumes. It reserves
`127.0.0.1:38417` for a future local service; nothing listens on that port yet,
and the port is not exposed beyond the host. Use `bash setup.sh shell` to enter
the environment, `bash setup.sh status` to inspect it, and `bash setup.sh stop`
to stop it without deleting either volume. Once inside the environment, use
ordinary Make targets such as `make test`, `make sanitize`, and `make run`.

`setup.sh` is intentionally limited to provisioning and lifecycle management.
The `Makefile` owns builds, tests, quality checks, sanitizer runs, package
verification, and program execution. GNU Make is provided inside the
container, so Windows hosts do not need a native `make` installation.

`make gpu-test` checks device and driver visibility with `nvidia-smi`; it does not
yet execute a GPU compute kernel. A GPU compiler and compute backend will be
added only when the first model requires them. On Windows, run the script from
Git Bash or from WSL with Docker integration; plain PowerShell's `bash` alias
may select a WSL installation that is not connected to Docker.

Contributors without an NVIDIA runtime can explicitly create a CPU-only
environment with `NATURALEHIA_PROTEIN_LOGIC_GPU=none bash setup.sh up`. GPU
sharing remains the default for the canonical research environment.

The base image is pinned by digest. Debian packages are installed from the
active Debian repositories on first creation, so recreating the environment at
a later date is not claimed to be bit-for-bit reproducible. The exact installed
package versions are recorded inside the build volume at
`/work/toolchain-packages.tsv`.

Native Linux developers may run the same Makefile without the container:

```sh
make test
make sanitize
```

The default native build root is `build/make`. CMake presets remain available
for direct, single-configuration work:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

The release and sanitizer presets are named `linux-release` and
`linux-sanitize`.

## Technical identity

- CMake project and installed package: `NaturalehiaProteinLogic`
- Library target: `naturalehia_protein_logic`
- CMake alias: `Naturalehia::ProteinLogic`
- C++ namespace: `naturalehia::protein_logic`
- Command-line program: `naturalehia-protein-logic`

The first version has no third-party runtime dependencies. Future dependencies
must be open source, versioned, attributable, locally runnable, and recorded in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). A proprietary hosted API
must not become necessary to reproduce a project result.

## Evidence ladder

Project language follows the strongest evidence actually available:

1. **Specification:** an abstract truth table and quantitative success metric.
2. **Simulation:** a response predicted by an explicitly documented model.
3. **Candidate:** a computationally proposed structure or sequence.
4. **Biophysical evidence:** measured folding, binding, state, or kinetics.
5. **Validated gate:** replicated experimental XOR behavior across a declared
   operating region, including reset and order-dependence tests.

A structure prediction is not a validated structure. A sequence is not a
working gate. Four endpoint measurements are not a response surface.

## Project layout

```text
include/naturalehia/protein_logic/  Public C++ API
src/                                Library implementation
apps/cli/                           Command-line evaluator
tests/                              Deterministic tests and package consumer
cmake/                              Installed-package configuration
docs/                               Research charter, roadmap, and safeguards
Makefile                            Build, test, quality, and execution targets
setup.sh                            Linux environment setup and lifecycle only
```

## Research direction

The intended progression is:

1. define the XOR operating region and quantitative acceptance criteria;
2. compare thermodynamic mechanisms that can generate negative joint coupling;
3. represent the intended active and inactive states in a multistate design
   objective;
4. generate and rank candidates with open, locally runnable C/C++ tooling;
5. test candidates first in an appropriately reviewed, cell-free setting;
6. measure response, reset, path dependence, cross-talk, stability, and yield;
7. determine whether one conserved architecture can be reparameterized into
   other Boolean functions.

See the [research charter](docs/RESEARCH_CHARTER.md),
[roadmap](docs/ROADMAP.md), and
[responsible-research policy](docs/RESPONSIBLE_RESEARCH.md).

## Non-goals

The project does not currently provide medical, diagnostic, therapeutic, or
safety-certified software. It does not claim to have discovered natural logic
gates, and an XOR result alone would not establish universal computation.
NAND and NOR, rather than XOR, are the conventional functionally complete
single-gate bases.

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and
the [community standards](CODE_OF_CONDUCT.md) before beginning substantial
work.

## License

Original project software and documentation are available under the
[MIT License](LICENSE), copyright 2026 Naturalehia contributors. Third-party
datasets, model weights, software, and biological materials retain their own
terms and must be documented separately.

Contact: [contact@waajacu.com](mailto:contact@waajacu.com)
