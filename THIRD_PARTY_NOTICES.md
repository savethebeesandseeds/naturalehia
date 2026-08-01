# Third-party development dependencies

Naturalehia's source code and documentation are MIT-licensed as described in
[`LICENSE`](LICENSE). The development environment downloads and installs
third-party software that is **not** redistributed under that MIT License.

## NVIDIA CUDA Toolkit

- Version: CUDA Toolkit 13.1 Update 1 (`cuda-toolkit-13-1`)
- Source: NVIDIA's signed Debian 13 package repository
- Repository: <https://developer.download.nvidia.com/compute/cuda/repos/debian13/x86_64/>
- License: <https://docs.nvidia.com/cuda/eula/>

The NVIDIA display/compute driver is supplied by the host and is not installed
inside the Naturalehia container.

## LibTorch

- Version: LibTorch 2.13.0 built for CUDA 13.0
- Source: the official PyTorch LibTorch distribution
- Archive index: <https://download.pytorch.org/libtorch/cu130/>
- PyTorch license: <https://github.com/pytorch/pytorch/blob/main/LICENSE>

The archive is stored only in a local Docker volume/cache. It is not committed
to or distributed from this repository. The SHA-256 value in `setup.sh` is a
maintainer-enrolled integrity pin calculated from the official HTTPS artifact;
PyTorch does not publish a companion checksum file for this archive.

## NVIDIA libraries required by LibTorch

PyTorch 2.13.0's official CUDA 13 wheel metadata pins the following NVIDIA
runtime components, which the LibTorch binary expects in sibling directories:

- cuDNN 9.20.0.48
- cuSPARSELt 0.8.1
- NCCL 2.29.7
- NVSHMEM 3.4.5

`setup.sh` downloads the matching x86-64 wheels from NVIDIA's package index,
checks their exact byte lengths and the SHA-256 values published by the
official PyTorch wheel index, and extracts them into the local
`naturalehia-gpu` volume as an immutable, versioned runtime release selected by
`$NVIDIA_RUNTIME_ROOT`. Verified archives remain in a root-only cache in that
volume. The wheels are not committed to or redistributed by this repository.
Their upstream metadata and license files are retained below
`$NVIDIA_RUNTIME_ROOT/.wheel-metadata/`.

- NVIDIA package index: <https://pypi.nvidia.com/>
- PyTorch CUDA 13 wheel index: <https://download.pytorch.org/whl/cu130/>
- cuDNN license: <https://docs.nvidia.com/deeplearning/cudnn/latest/reference/eula.html>
- NCCL license: <https://github.com/NVIDIA/nccl/blob/master/LICENSE.txt>
- NVSHMEM license: <https://docs.nvidia.com/nvshmem/api/sla.html>

## Debian packages

Additional compiler, analysis, numerical, networking, computer-vision, and test
packages come from the configured Debian repositories. Their individual
copyright and license files are installed under `/usr/share/doc/<package>/` in
the container. Those packages follow current signed Debian 13 updates rather
than a dated snapshot; the exact installed package manifest is recorded inside
each provisioned container.
