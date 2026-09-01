SHELL := /bin/bash
.SHELLFLAGS := -Eeuo pipefail -c
.ONESHELL:
.DEFAULT_GOAL := build
.DELETE_ON_ERROR:

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_ROOT := $(strip $(NATURALEHIA_FAUNA_BUILD_ROOT))
ifeq ($(BUILD_ROOT),)
BUILD_ROOT := $(PROJECT_ROOT)/build/make
endif

RUN_ARGS ?=

define resolve_build_directory
toolchain_fingerprint="$$( \
    { \
        g++ --version; \
        g++ -dumpmachine; \
        cmake --version; \
        ninja --version; \
        ccache --version; \
        nvcc --version; \
        nvidia-smi --query-gpu=compute_cap --format=csv,noheader | \
            sed 's/[[:space:]]//g' | sort -u; \
        cat "$${LIBTORCH_ROOT}/.naturalehia-manifest"; \
        cat "$${NVIDIA_RUNTIME_ROOT}/.naturalehia-manifest"; \
        cat /var/lib/naturalehia-fauna/package-manifest; \
    } | sha256sum | cut -d ' ' -f 1 \
)"
[[ "$${toolchain_fingerprint}" =~ ^[0-9a-f]{64}$$ ]] || {
    printf 'error: could not fingerprint the installed Debian toolchain\n' >&2
    exit 1
}
build_dir="$(BUILD_ROOT)/$${toolchain_fingerprint}"
endef

.PHONY: help require-container quality build test gpu-test run

help:
	@printf '%s\n' \
		'The Elder Brother of Fauna project tasks' \
		'' \
		'  make build                  Configure and build the Debug tree' \
		'  make test                   Build and run the complete CTest suite' \
		'  make gpu-test               Build and run CUDA/LibTorch GPU smokes' \
		'  make run RUN_ARGS="..."     Build and run the synthetic CLI' \
		'  make quality                ShellCheck the environment scripts' \
		'' \
		'Run canonical tasks through: bash container.sh exec make <target>'

require-container:
	@test "$(NATURALEHIA_FAUNA_CONTAINER)" = 1 || {
		printf 'error: run project tasks through bash container.sh exec make <target>\n' >&2
		exit 1
	}

quality: require-container
	@cd "$(PROJECT_ROOT)"
	shellcheck container.sh setup.sh toolchain-locks.sh

build: require-container
	@$(resolve_build_directory)
	printf '[fauna] building with Debian GCC and Ninja in %s\n' "$${build_dir}"
	cmake \
		-S "$(PROJECT_ROOT)" \
		-B "$${build_dir}" \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_COMPILER=g++ \
		-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	cmake --build "$${build_dir}" --parallel

test: build
	@$(resolve_build_directory)
	printf '[fauna] running CTest in %s\n' "$${build_dir}"
	ctest --test-dir "$${build_dir}" --output-on-failure

gpu-test: require-container
	@$(resolve_build_directory)
	mapfile -t compute_capabilities < <(
		nvidia-smi --query-gpu=compute_cap --format=csv,noheader |
			sed 's/[[:space:]]//g' | sort -u
	)
	(($${#compute_capabilities[@]} != 0)) || {
		printf 'error: NVIDIA did not report a GPU compute capability\n' >&2
		exit 1
	}
	cuda_architectures=''
	torch_architectures=''
	for capability in "$${compute_capabilities[@]}"; do
		[[ "$${capability}" =~ ^([0-9]+)\.([0-9]+)$$ ]] || {
			printf 'error: unexpected NVIDIA compute capability: %s\n' \
				"$${capability}" >&2
			exit 1
		}
		cuda_architectures+="$${BASH_REMATCH[1]}$${BASH_REMATCH[2]};"
		torch_architectures+="$${capability} "
	done
	cuda_architectures="$${cuda_architectures%;}"
	torch_architectures="$${torch_architectures% }"
	gpu_build_directory="$${build_dir}/gpu-smoke"
	printf '[fauna] building CUDA and LibTorch smokes for compute %s\n' \
		"$${torch_architectures}"
	cmake \
		-S "$(PROJECT_ROOT)/cmake/gpu-smoke" \
		-B "$${gpu_build_directory}" \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_CXX_COMPILER=g++ \
		-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
		-DCMAKE_CUDA_COMPILER="$${CUDA_HOME}/bin/nvcc" \
		-DCMAKE_CUDA_COMPILER_LAUNCHER=ccache \
		-DNATURALEHIA_FAUNA_CUDA_ARCHITECTURES="$${cuda_architectures}" \
		-DNATURALEHIA_FAUNA_TORCH_CUDA_ARCH_LIST="$${torch_architectures}" \
		-DTorch_DIR="$${LIBTORCH_ROOT}/share/cmake/Torch"
	cmake --build "$${gpu_build_directory}" --parallel
	printf '[fauna] running CUDA and LibTorch GPU smokes\n'
	ctest --test-dir "$${gpu_build_directory}" --output-on-failure

run: build
	@$(resolve_build_directory)
	"$${build_dir}/naturalehia-fauna" $(RUN_ARGS)
