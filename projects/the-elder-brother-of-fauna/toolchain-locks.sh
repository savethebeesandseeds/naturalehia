#!/usr/bin/env bash

# shellcheck disable=SC2034 # Data-only constants are consumed by sourcing scripts.

# Shared, immutable inputs for the host lifecycle and in-container provisioner.
# This file defines data only; it performs no installation or lifecycle work.

readonly PROJECT_SLUG="the-elder-brother-of-fauna"
readonly PROJECT_RELATIVE_PATH="projects/${PROJECT_SLUG}"
readonly DEFAULT_IMAGE="debian:13-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd"
readonly CONTAINER_WORKSPACE="/workspace/naturalehia"
readonly CONTAINER_PROJECT_DIR="${CONTAINER_WORKSPACE}/${PROJECT_RELATIVE_PATH}"
readonly CONTAINER_BUILD_ROOT="/work/naturalehia-build"
readonly CONTAINER_PROJECT_BUILD_ROOT="${CONTAINER_BUILD_ROOT}/${PROJECT_SLUG}"
readonly CONTAINER_HOME="/home/developer"
readonly CONTAINER_GPU_ROOT="/opt/naturalehia-gpu"
readonly CONTAINER_PATH="/usr/local/cuda-13.1/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

readonly TOOLCHAIN_PACKAGE_SET="bash ca-certificates ccache clang-format cmake curl g++ git make ninja-build passwd shellcheck unzip"

readonly CUDA_REPOSITORY="https://developer.download.nvidia.com/compute/cuda/repos/debian13/x86_64"
readonly CUDA_KEYRING_FILE="cuda-keyring_1.1-1_all.deb"
readonly CUDA_KEYRING_SHA256="d0d4ef986a44400f9db33c600ef33a985175e7cc63d805a10e1839c7a1e78f5f"
readonly CUDA_KEYRING_BYTES="4182"
# This is the complete set of CUDA capabilities used by the documented GPU
# build and smoke test: nvcc and the CUDA runtime development files, NVRTC's
# exported LibTorch link interface, CUPTI required by libtorch_cpu.so, and the
# runtime libraries named by the canonical smoke executable's LibTorch ELF
# dependency chain. Keep every
# requested package version explicit; APT still resolves their signed
# transitive closure from the CUDA repository.
readonly CUDA_PACKAGE_SET="cuda-nvcc-13-1=13.1.115-1 \
cuda-cudart-dev-13-1=13.1.80-1 \
cuda-cudart-13-1=13.1.80-1 \
cuda-nvrtc-dev-13-1=13.1.115-1 \
cuda-nvrtc-13-1=13.1.115-1 \
cuda-cupti-13-1=13.1.115-1 \
libcublas-13-1=13.2.2.2-1 \
libcufft-13-1=12.1.0.78-1 \
libcufile-13-1=1.16.1.26-1 \
libcurand-13-1=10.4.1.81-1 \
libcusparse-13-1=12.7.3.1-1"
readonly CUDA_ROOT="/usr/local/cuda-13.1"

readonly LIBTORCH_VERSION="2.13.0"
readonly LIBTORCH_CUDA_TAG="cu130"
readonly LIBTORCH_ARCHIVE="libtorch-2.13.0-cu130.zip"
readonly LIBTORCH_URL="https://download.pytorch.org/libtorch/cu130/libtorch-shared-with-deps-2.13.0%2Bcu130.zip"
readonly LIBTORCH_SHA256="945c5a3d946a28b387ad9dc9fddda7ba03e35fae1375b84ebff15df789436f82"
readonly LIBTORCH_ARCHIVE_BYTES="500687821"
readonly LIBTORCH_ROOT="${CONTAINER_GPU_ROOT}/libtorch-${LIBTORCH_VERSION}-${LIBTORCH_CUDA_TAG}"

readonly NVIDIA_RUNTIME_SELECTOR_BASENAME="nvidia"
readonly NVIDIA_RUNTIME_ROOT="${CONTAINER_GPU_ROOT}/${NVIDIA_RUNTIME_SELECTOR_BASENAME}"
readonly CUDNN_VERSION="9.20.0.48"
readonly CUDNN_WHEEL="nvidia_cudnn_cu13-${CUDNN_VERSION}-py3-none-manylinux_2_27_x86_64.whl"
readonly CUDNN_WHEEL_URL="https://pypi.nvidia.com/nvidia-cudnn-cu13/${CUDNN_WHEEL}"
readonly CUDNN_WHEEL_SHA256="0c45dd8eeb50b603f07995b1b300c62ffe6a1980482b82b3bcf94a4ca9d49304"
readonly CUDNN_WHEEL_BYTES="366173588"
readonly CUSPARSELT_VERSION="0.8.1"
readonly CUSPARSELT_WHEEL="nvidia_cusparselt_cu13-${CUSPARSELT_VERSION}-py3-none-manylinux2014_x86_64.whl"
readonly CUSPARSELT_WHEEL_URL="https://pypi.nvidia.com/nvidia-cusparselt-cu13/${CUSPARSELT_WHEEL}"
readonly CUSPARSELT_WHEEL_SHA256="786ce87568c303fadb5afcc7102d454cd3040d75f6f8626f5db460d1871f4dd0"
readonly CUSPARSELT_WHEEL_BYTES="170148586"
readonly NCCL_VERSION="2.29.7"
readonly NCCL_WHEEL="nvidia_nccl_cu13-${NCCL_VERSION}-py3-none-manylinux_2_18_x86_64.whl"
readonly NCCL_WHEEL_URL="https://pypi.nvidia.com/nvidia-nccl-cu13/${NCCL_WHEEL}"
readonly NCCL_WHEEL_SHA256="edd81538446786ec3b73972543e53bb43bcaf0bfc8ef76cb679fcc390ffe136d"
readonly NCCL_WHEEL_BYTES="205976000"
readonly NVSHMEM_VERSION="3.4.5"
readonly NVSHMEM_WHEEL="nvidia_nvshmem_cu13-${NVSHMEM_VERSION}-py3-none-manylinux2014_x86_64.manylinux_2_17_x86_64.whl"
readonly NVSHMEM_WHEEL_URL="https://pypi.nvidia.com/nvidia-nvshmem-cu13/${NVSHMEM_WHEEL}"
readonly NVSHMEM_WHEEL_SHA256="290f0a2ee94c9f3687a02502f3b9299a9f9fe826e6d0287ee18482e78d495b80"
readonly NVSHMEM_WHEEL_BYTES="60412546"
readonly NVIDIA_RUNTIME_ID="cudnn-${CUDNN_VERSION}-${CUDNN_WHEEL_SHA256:0:12}_cusparselt-${CUSPARSELT_VERSION}-${CUSPARSELT_WHEEL_SHA256:0:12}_nccl-${NCCL_VERSION}-${NCCL_WHEEL_SHA256:0:12}_nvshmem-${NVSHMEM_VERSION}-${NVSHMEM_WHEEL_SHA256:0:12}"
readonly NVIDIA_RUNTIME_VERSION_ROOT="${CONTAINER_GPU_ROOT}/nvidia-${NVIDIA_RUNTIME_ID}"
readonly GPU_STACK_ID="cuda-13.1.2_libtorch-${LIBTORCH_VERSION}-${LIBTORCH_CUDA_TAG}-${LIBTORCH_SHA256:0:12}_${NVIDIA_RUNTIME_ID}"
readonly DOWNLOAD_CACHE="${CONTAINER_GPU_ROOT}/.downloads"
readonly BOOTSTRAP_VERSION="9"
