# SPDX-License-Identifier: MIT

SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

PROJECT_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

BUILD_ROOT ?= $(NATURALEHIA_PROTEIN_LOGIC_BUILD_ROOT)
ifeq ($(strip $(BUILD_ROOT)),)
BUILD_ROOT := $(PROJECT_ROOT)/build/make
endif

CMAKE ?= cmake
CTEST ?= ctest
GCC_CC ?= gcc
GCC_CXX ?= g++
CLANG_CC ?= clang
CLANG_CXX ?= clang++
RUN_ARGS ?=

GCC_DEBUG_DIR := $(BUILD_ROOT)/gcc-debug
GCC_RELEASE_DIR := $(BUILD_ROOT)/gcc-release
CLANG_SANITIZE_DIR := $(BUILD_ROOT)/clang-sanitize
INSTALL_DIR := $(BUILD_ROOT)/install
PACKAGE_CONSUMER_DIR := $(BUILD_ROOT)/package-consumer

COMMON_CMAKE_ARGS := \
	-G Ninja \
	-DBUILD_TESTING=ON \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DNATURALEHIA_PROTEIN_LOGIC_WARNINGS_AS_ERRORS=ON

CPP_FILES := \
	apps/cli/main.cpp \
	include/naturalehia/protein_logic/model.hpp \
	src/model.cpp \
	tests/model_tests.cpp \
	tests/package_consumer/main.cpp

.PHONY: \
	all \
	help \
	check \
	quality \
	shell-quality \
	format-check \
	format \
	configure-debug \
	build-debug \
	test-debug \
	configure-release \
	build-release \
	test-release \
	package-test \
	configure-sanitize \
	build-sanitize \
	test-sanitize \
	build \
	test \
	sanitize \
	run \
	gpu-test

all: build

help:
	@printf '%s\n' \
		'Logic Gates of the Biological Kingdom' \
		'' \
		'Build targets:' \
		'  make build          GCC Debug build' \
		'  make test           Quality, GCC Debug/Release, install, and consumer tests' \
		'  make sanitize       Clang AddressSanitizer/UndefinedBehaviorSanitizer tests' \
		'  make quality        ShellCheck and C++ formatting verification' \
		'  make format         Format the maintained C++ files' \
		'  make run            Build and run the CLI; pass RUN_ARGS="..." if needed' \
		'  make gpu-test       Check NVIDIA GPU and driver visibility' \
		'' \
		'Container setup remains in setup.sh:' \
		'  bash setup.sh up' \
		'  bash setup.sh shell' \
		'  bash setup.sh exec make test'

shell-quality:
	cd "$(PROJECT_ROOT)" && shellcheck setup.sh

format-check:
	cd "$(PROJECT_ROOT)" && uncrustify --check -c .uncrustify.cfg $(CPP_FILES)

quality: shell-quality format-check

check: quality

format:
	cd "$(PROJECT_ROOT)" && \
		uncrustify -c .uncrustify.cfg --replace --no-backup $(CPP_FILES)

configure-debug:
	CC="$(GCC_CC)" CXX="$(GCC_CXX)" "$(CMAKE)" \
		-S "$(PROJECT_ROOT)" \
		-B "$(GCC_DEBUG_DIR)" \
		$(COMMON_CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug

build-debug: configure-debug
	"$(CMAKE)" --build "$(GCC_DEBUG_DIR)" --parallel

test-debug: build-debug
	"$(CTEST)" --test-dir "$(GCC_DEBUG_DIR)" --output-on-failure

configure-release:
	CC="$(GCC_CC)" CXX="$(GCC_CXX)" "$(CMAKE)" \
		-S "$(PROJECT_ROOT)" \
		-B "$(GCC_RELEASE_DIR)" \
		$(COMMON_CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Release

build-release: configure-release
	"$(CMAKE)" --build "$(GCC_RELEASE_DIR)" --parallel

test-release: build-release
	"$(CTEST)" --test-dir "$(GCC_RELEASE_DIR)" --output-on-failure

package-test: test-release
	"$(CMAKE)" -E remove_directory "$(INSTALL_DIR)"
	"$(CMAKE)" -E remove_directory "$(PACKAGE_CONSUMER_DIR)"
	"$(CMAKE)" --install "$(GCC_RELEASE_DIR)" --prefix "$(INSTALL_DIR)"
	CC="$(GCC_CC)" CXX="$(GCC_CXX)" "$(CMAKE)" \
		-S "$(PROJECT_ROOT)/tests/package_consumer" \
		-B "$(PACKAGE_CONSUMER_DIR)" \
		-G Ninja \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_PREFIX_PATH="$(INSTALL_DIR)"
	"$(CMAKE)" --build "$(PACKAGE_CONSUMER_DIR)" --parallel
	"$(CTEST)" --test-dir "$(PACKAGE_CONSUMER_DIR)" --output-on-failure

configure-sanitize:
	CC="$(CLANG_CC)" CXX="$(CLANG_CXX)" "$(CMAKE)" \
		-S "$(PROJECT_ROOT)" \
		-B "$(CLANG_SANITIZE_DIR)" \
		$(COMMON_CMAKE_ARGS) \
		-DCMAKE_BUILD_TYPE=Debug \
		-DNATURALEHIA_PROTEIN_LOGIC_ENABLE_SANITIZERS=ON

build-sanitize: configure-sanitize
	"$(CMAKE)" --build "$(CLANG_SANITIZE_DIR)" --parallel

test-sanitize: build-sanitize
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
		"$(CTEST)" --test-dir "$(CLANG_SANITIZE_DIR)" --output-on-failure

build: build-debug

test: quality test-debug test-release package-test

sanitize: quality test-sanitize

run: build-debug
	"$(GCC_DEBUG_DIR)/naturalehia-protein-logic" $(RUN_ARGS)

gpu-test:
	@if ! command -v nvidia-smi >/dev/null 2>&1; then \
		echo 'error: nvidia-smi is unavailable; enable GPU sharing in setup.sh' >&2; \
		exit 1; \
	fi
	nvidia-smi
