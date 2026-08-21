# Third-party notices

The initial project library and command-line program use only the C++ standard
library and have no third-party runtime dependencies.

The development environment uses, but does not redistribute as part of the
project binaries:

| Component | Role | Licensing boundary |
| --- | --- | --- |
| Debian 13 slim OCI image | Linux base environment | Debian packages retain their individual free-software licenses and copyright notices. |
| CMake, Ninja, GCC, Clang, Git, ShellCheck, Uncrustify, and related Debian packages | Build and quality tools | Each package retains its upstream and Debian licensing terms. Exact installed versions are recorded in `/work/toolchain-packages.tsv`. |
| Docker-compatible Linux engine and CLI | Host-side container lifecycle | Installed separately. Docker Engine includes open-source components; Docker Desktop is a separate product with its own terms. |
| NVIDIA Container Toolkit | Host-to-container GPU access | Installed separately and maintained as an open-source NVIDIA project under its repository license. |
| NVIDIA display/compute driver | Host GPU driver | Installed separately and subject to NVIDIA's terms; it is not covered by this project's MIT License and may not be open source in a given host configuration. |

The current C++ model is CPU-only. GPU access is prepared for later open
compute backends; `nvidia-smi` visibility alone is not a scientific result or a
GPU-compute validation.

Future third-party software, datasets, model weights, structures, and other
materials must be listed here with their source, version, license, and role.
The project's MIT License does not relicense third-party materials.
