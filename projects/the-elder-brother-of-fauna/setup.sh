#!/usr/bin/env bash

# Idempotent provisioning for the already-running Linux development container.
# Container creation and lifecycle belong in container.sh; project tasks belong
# in Makefile.

set -Eeuo pipefail
IFS=$'\n\t'

SCRIPT_DIRECTORY="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly SCRIPT_DIRECTORY
# shellcheck source=toolchain-locks.sh
source "${SCRIPT_DIRECTORY}/toolchain-locks.sh"

readonly CUDA_APT_CACHE_ROOT="${CONTAINER_GPU_ROOT}/.apt-cache"
readonly CUDA_APT_ARCHIVES="${CUDA_APT_CACHE_ROOT}/archives"
readonly -a CUDA_APT_OPTIONS=(
    -o "Dir::Cache::archives=${CUDA_APT_ARCHIVES}/"
    -o "APT::Keep-Downloaded-Packages=true"
    -o "Binary::apt::APT::Keep-Downloaded-Packages=true"
)

(($# == 0)) || {
    printf 'setup.sh accepts no commands or arguments\n' >&2
    exit 2
}
[[ "$(id -u)" == "0" ]] || {
    printf 'setup.sh must run as root inside the development container\n' >&2
    exit 1
}
: "${DEV_UID:?container.sh must provide DEV_UID}"
: "${DEV_GID:?container.sh must provide DEV_GID}"
[[ "${DEV_UID}" =~ ^[1-9][0-9]*$ && "${DEV_GID}" =~ ^[1-9][0-9]*$ ]] || {
    printf 'DEV_UID and DEV_GID must be canonical positive integers\n' >&2
    exit 1
}
[[ "${DEV_UID}" != "0" && "${DEV_GID}" != "0" ]] || {
    printf 'the development identity must be non-root\n' >&2
    exit 1
}
[[ "${SCRIPT_DIRECTORY}" == "${CONTAINER_PROJECT_DIR}" ]] || {
    printf 'setup.sh must run from %s\n' "${CONTAINER_PROJECT_DIR}" >&2
    exit 1
}

# shellcheck disable=SC1091
source /etc/os-release
[[ "${ID}" == "debian" && "${VERSION_ID}" == "13" ]] || {
    printf 'setup.sh requires Debian 13\n' >&2
    exit 1
}
[[ "$(dpkg --print-architecture)" == "amd64" && "$(uname -m)" == "x86_64" ]] || {
    printf 'setup.sh requires x86-64/amd64\n' >&2
    exit 1
}

export DEBIAN_FRONTEND=noninteractive

adopt_apt_cache_directory() {
    local source_directory="$1"
    local destination_directory="$2"
    local destination_owner="$3"
    local destination_mode="$4"
    local filename_pattern="$5"
    local source_path
    local filename
    local destination_path
    local temporary

    [[ -d "${source_directory}" && ! -L "${source_directory}" ]] || return 0
    [[ -d "${destination_directory}" && ! -L "${destination_directory}" ]] || {
        printf 'persistent APT cache path is unsafe: %s\n' \
            "${destination_directory}" >&2
        return 1
    }

    while IFS= read -r -d '' source_path; do
        filename="${source_path##*/}"
        destination_path="${destination_directory}/${filename}"
        if [[ -e "${destination_path}" || -L "${destination_path}" ]]; then
            continue
        fi
        temporary="$(mktemp "${destination_directory}/.${filename}.adopt.XXXXXX")"
        if ! cp --reflink=auto --sparse=always -- "${source_path}" "${temporary}"; then
            rm -f -- "${temporary}"
            return 1
        fi
        chown "${destination_owner}:0" "${temporary}"
        chmod "${destination_mode}" "${temporary}"
        if [[ -e "${destination_path}" || -L "${destination_path}" ]]; then
            rm -f -- "${temporary}"
            continue
        fi
        mv -T -- "${temporary}" "${destination_path}"
    done < <(find "${source_directory}" -mindepth 1 -maxdepth 1 -type f \
        -name "${filename_pattern}" -print0)
}

install -d -m 0755 -o 0 -g 0 \
    "${CONTAINER_GPU_ROOT}" "${CUDA_APT_CACHE_ROOT}" "${CUDA_APT_ARCHIVES}"
install -d -m 0700 -o _apt -g 0 "${CUDA_APT_ARCHIVES}/partial"
adopt_apt_cache_directory /var/cache/apt/archives "${CUDA_APT_ARCHIVES}" \
    0 0644 '*.deb'
adopt_apt_cache_directory /var/cache/apt/archives/partial \
    "${CUDA_APT_ARCHIVES}/partial" _apt 0600 '*.deb*'

if [[ -f /etc/apt/sources.list.d/debian.sources ]] &&
    ! grep -Eq '^Components:.*(^|[[:space:]])contrib([[:space:]]|$)' \
        /etc/apt/sources.list.d/debian.sources; then
    sed -i '/^Components:/ s/$/ contrib/' /etc/apt/sources.list.d/debian.sources
fi

IFS=' ' read -r -a packages <<<"${TOOLCHAIN_PACKAGE_SET}"
missing_packages=()
for package in "${packages[@]}"; do
    dpkg-query --show --showformat='${Status}' "${package}" 2>/dev/null |
        grep 'install ok installed' >/dev/null || missing_packages+=("${package}")
done
if ((${#missing_packages[@]} != 0)); then
    apt-get update
    apt-get install -y --no-install-recommends "${missing_packages[@]}"
fi

install -d -m 0700 -o 0 -g 0 "${DOWNLOAD_CACHE}"

ensure_cached_download() {
    local filename="$1"
    local url="$2"
    local expected_sha256="$3"
    local expected_bytes="$4"
    local target="${DOWNLOAD_CACHE}/${filename}"
    local temporary="${target}.part"

    if [[ -f "${target}" ]] &&
        [[ "$(stat -c %s "${target}")" == "${expected_bytes}" ]] &&
        printf '%s  %s\n' "${expected_sha256}" "${target}" |
            sha256sum --check --status; then
        return
    fi

    if [[ -e "${temporary}" ]] &&
        { [[ ! -f "${temporary}" ]] ||
            (($(stat -c %s "${temporary}") >= expected_bytes)); }; then
        rm -f -- "${temporary}"
    fi
    (
        umask 077
        curl --fail --location --proto '=https' --proto-redir '=https' \
            --tlsv1.2 --retry 5 --retry-all-errors --connect-timeout 30 \
            --continue-at - --output "${temporary}" "${url}"
    )
    printf '%s  %s\n' "${expected_sha256}" "${temporary}" |
        sha256sum --check --status || {
        rm -f -- "${temporary}"
        printf 'checksum verification failed for %s\n' "${url}" >&2
        exit 1
    }
    [[ "$(stat -c %s "${temporary}")" == "${expected_bytes}" ]] || {
        rm -f -- "${temporary}"
        printf 'size verification failed for %s\n' "${url}" >&2
        exit 1
    }
    chown 0:0 "${temporary}"
    chmod 0400 "${temporary}"
    mv -fT -- "${temporary}" "${target}"
}

ensure_cached_download "${CUDA_KEYRING_FILE}" \
    "${CUDA_REPOSITORY}/${CUDA_KEYRING_FILE}" "${CUDA_KEYRING_SHA256}" \
    "${CUDA_KEYRING_BYTES}"
keyring_path="${DOWNLOAD_CACHE}/${CUDA_KEYRING_FILE}"
[[ "$(dpkg-deb --field "${keyring_path}" Package)" == "cuda-keyring" ]]
[[ "$(dpkg-deb --field "${keyring_path}" Version)" == "1.1-1" ]]
installed_keyring="$(dpkg-query --show --showformat='${Version}' \
    cuda-keyring 2>/dev/null || true)"
if [[ "${installed_keyring}" != "1.1-1" ]]; then
    apt-get "${CUDA_APT_OPTIONS[@]}" install -y --no-install-recommends \
        "${keyring_path}"
fi

installed_cuda="$(dpkg-query --show --showformat='${Version}' \
    "${CUDA_TOOLKIT_PACKAGE}" 2>/dev/null || true)"
if [[ "${installed_cuda}" != "${CUDA_TOOLKIT_PACKAGE_VERSION}" ]]; then
    apt-get "${CUDA_APT_OPTIONS[@]}" update
    apt-get "${CUDA_APT_OPTIONS[@]}" install -y --no-install-recommends \
        --allow-downgrades \
        "${CUDA_TOOLKIT_PACKAGE}=${CUDA_TOOLKIT_PACKAGE_VERSION}"
fi
[[ "$(readlink -f /usr/local/cuda)" == "${CUDA_ROOT}" ]]
if dpkg-query --show --showformat='${binary:Package}\n' 2>/dev/null |
    grep -E '^(cuda-drivers|nvidia-driver)(:|$)' >/dev/null; then
    printf 'refusing an in-container NVIDIA driver installation\n' >&2
    exit 1
fi

apt-get -o Dir::Cache::archives=/var/cache/apt/archives/ clean
rm -rf /var/lib/apt/lists/*

if id developer >/dev/null 2>&1; then
    [[ "$(id -u developer)" == "${DEV_UID}" ]]
    [[ "$(id -g developer)" == "${DEV_GID}" ]]
else
    if getent passwd "${DEV_UID}" >/dev/null; then
        printf 'UID %s is already assigned\n' "${DEV_UID}" >&2
        exit 1
    fi
    getent group "${DEV_GID}" >/dev/null ||
        groupadd --gid "${DEV_GID}" developer
    useradd --uid "${DEV_UID}" --gid "${DEV_GID}" \
        --home-dir "${CONTAINER_HOME}" --no-create-home --shell /bin/bash developer
fi

install -d -m 0755 "${CONTAINER_HOME}" "${CONTAINER_BUILD_ROOT}"
install -d -m 0755 -o 0 -g 0 "${CONTAINER_GPU_ROOT}"

migrate_managed_volume() {
    local path="$1"
    local marker="${path}/.naturalehia-owner"
    local expected="${DEV_UID}:${DEV_GID}"
    local recorded=""
    [[ ! -r "${marker}" ]] || recorded="$(<"${marker}")"
    if [[ "${recorded}" != "${expected}" ]]; then
        chown -R "${DEV_UID}:${DEV_GID}" "${path}"
        printf '%s\n' "${expected}" >"${marker}"
        chown "${DEV_UID}:${DEV_GID}" "${marker}"
    fi
}

migrate_managed_volume "${CONTAINER_HOME}"
migrate_managed_volume "${CONTAINER_BUILD_ROOT}"

ensure_cached_download "${LIBTORCH_ARCHIVE}" "${LIBTORCH_URL}" \
    "${LIBTORCH_SHA256}" "${LIBTORCH_ARCHIVE_BYTES}"
libtorch_archive_path="${DOWNLOAD_CACHE}/${LIBTORCH_ARCHIVE}"
libtorch_manifest="${LIBTORCH_ROOT}/.naturalehia-manifest"
expected_libtorch_manifest="${LIBTORCH_VERSION}:${LIBTORCH_CUDA_TAG}:${LIBTORCH_SHA256}"
installed_libtorch_manifest=""
[[ ! -r "${libtorch_manifest}" ]] ||
    installed_libtorch_manifest="$(<"${libtorch_manifest}")"
if [[ "${installed_libtorch_manifest}" != "${expected_libtorch_manifest}" ]]; then
    [[ ! -e "${LIBTORCH_ROOT}" ]] || {
        printf 'existing LibTorch path failed its manifest check: %s\n' \
            "${LIBTORCH_ROOT}" >&2
        exit 1
    }
    if unzip -Z1 "${libtorch_archive_path}" |
        grep -E '(^/|(^|/)\.\.(/|$))' >/dev/null; then
        printf 'unsafe path found in LibTorch archive\n' >&2
        exit 1
    fi

    libtorch_staging="${CONTAINER_GPU_ROOT}/.libtorch-staging.$$"
    trap 'rm -rf -- "${libtorch_staging}"' EXIT
    mkdir "${libtorch_staging}"
    unzip -q "${libtorch_archive_path}" -d "${libtorch_staging}"
    test -r "${libtorch_staging}/libtorch/share/cmake/Torch/TorchConfig.cmake"
    test -r "${libtorch_staging}/libtorch/lib/libtorch.so"
    test -r "${libtorch_staging}/libtorch/lib/libtorch_cuda.so"
    [[ "$(<"${libtorch_staging}/libtorch/build-version")" == \
        "${LIBTORCH_VERSION}+${LIBTORCH_CUDA_TAG}" ]]
    printf '%s\n' "${expected_libtorch_manifest}" \
        >"${libtorch_staging}/libtorch/.naturalehia-manifest"
    chown -R 0:0 "${libtorch_staging}/libtorch"
    chmod -R a+rX,a-w "${libtorch_staging}/libtorch"
    mv "${libtorch_staging}/libtorch" "${LIBTORCH_ROOT}"
    rmdir "${libtorch_staging}"
    trap - EXIT
fi

ensure_cached_download "${CUDNN_WHEEL}" "${CUDNN_WHEEL_URL}" \
    "${CUDNN_WHEEL_SHA256}" "${CUDNN_WHEEL_BYTES}"
ensure_cached_download "${CUSPARSELT_WHEEL}" "${CUSPARSELT_WHEEL_URL}" \
    "${CUSPARSELT_WHEEL_SHA256}" "${CUSPARSELT_WHEEL_BYTES}"
ensure_cached_download "${NCCL_WHEEL}" "${NCCL_WHEEL_URL}" \
    "${NCCL_WHEEL_SHA256}" "${NCCL_WHEEL_BYTES}"
ensure_cached_download "${NVSHMEM_WHEEL}" "${NVSHMEM_WHEEL_URL}" \
    "${NVSHMEM_WHEEL_SHA256}" "${NVSHMEM_WHEEL_BYTES}"

runtime_manifest="${NVIDIA_RUNTIME_VERSION_ROOT}/.naturalehia-manifest"
expected_runtime_manifest="$(
    printf 'cudnn=%s:%s\n' "${CUDNN_VERSION}" "${CUDNN_WHEEL_SHA256}"
    printf 'cusparselt=%s:%s\n' \
        "${CUSPARSELT_VERSION}" "${CUSPARSELT_WHEEL_SHA256}"
    printf 'nccl=%s:%s\n' "${NCCL_VERSION}" "${NCCL_WHEEL_SHA256}"
    printf 'nvshmem=%s:%s\n' "${NVSHMEM_VERSION}" "${NVSHMEM_WHEEL_SHA256}"
)"
installed_runtime_manifest=""
[[ ! -r "${runtime_manifest}" ]] ||
    installed_runtime_manifest="$(<"${runtime_manifest}")"

# Migrate the original unversioned installation once. A matching release becomes
# the current version; an older release is retained immutably by manifest hash.
if [[ -d "${NVIDIA_RUNTIME_ROOT}" && ! -L "${NVIDIA_RUNTIME_ROOT}" ]]; then
    unversioned_manifest_path="${NVIDIA_RUNTIME_ROOT}/.naturalehia-manifest"
    [[ -f "${unversioned_manifest_path}" && ! -L "${unversioned_manifest_path}" ]] || {
        printf 'unversioned NVIDIA runtime has no regular manifest\n' >&2
        exit 1
    }
    unversioned_runtime_manifest="$(<"${unversioned_manifest_path}")"
    [[ -n "${unversioned_runtime_manifest}" ]] || {
        printf 'unversioned NVIDIA runtime has an empty manifest\n' >&2
        exit 1
    }
    chown -R 0:0 "${NVIDIA_RUNTIME_ROOT}"
    chmod -R a+rX,a-w "${NVIDIA_RUNTIME_ROOT}"

    if [[ "${unversioned_runtime_manifest}" == "${expected_runtime_manifest}" ]]; then
        [[ ! -e "${NVIDIA_RUNTIME_VERSION_ROOT}" &&
            ! -L "${NVIDIA_RUNTIME_VERSION_ROOT}" ]] || {
            printf 'both versioned and unversioned NVIDIA runtimes exist\n' >&2
            exit 1
        }
        mv "${NVIDIA_RUNTIME_ROOT}" "${NVIDIA_RUNTIME_VERSION_ROOT}"
        installed_runtime_manifest="${expected_runtime_manifest}"
    else
        legacy_manifest_hash="$(sha256sum -- "${unversioned_manifest_path}")"
        legacy_manifest_hash="${legacy_manifest_hash%% *}"
        [[ "${legacy_manifest_hash}" =~ ^[0-9a-f]{64}$ ]] || {
            printf 'could not fingerprint the legacy NVIDIA runtime manifest\n' >&2
            exit 1
        }
        legacy_runtime_root="${CONTAINER_GPU_ROOT}/nvidia-legacy-${legacy_manifest_hash}"
        [[ ! -e "${legacy_runtime_root}" && ! -L "${legacy_runtime_root}" ]] || {
            printf 'legacy NVIDIA runtime destination already exists: %s\n' \
                "${legacy_runtime_root}" >&2
            exit 1
        }
        mv "${NVIDIA_RUNTIME_ROOT}" "${legacy_runtime_root}"
        printf 'retained previous NVIDIA runtime at %s\n' "${legacy_runtime_root}"
    fi
fi

if [[ "${installed_runtime_manifest}" != "${expected_runtime_manifest}" ]]; then
    [[ ! -e "${NVIDIA_RUNTIME_VERSION_ROOT}" &&
        ! -L "${NVIDIA_RUNTIME_VERSION_ROOT}" ]] || {
        printf 'existing NVIDIA runtime failed its manifest check: %s\n' \
            "${NVIDIA_RUNTIME_VERSION_ROOT}" >&2
        exit 1
    }

    runtime_staging="${CONTAINER_GPU_ROOT}/.nvidia-runtime-staging.$$"
    trap 'rm -rf -- "${runtime_staging}"' EXIT
    mkdir "${runtime_staging}"
    runtime_wheels=(
        "${CUDNN_WHEEL}"
        "${CUSPARSELT_WHEEL}"
        "${NCCL_WHEEL}"
        "${NVSHMEM_WHEEL}"
    )
    for runtime_wheel in "${runtime_wheels[@]}"; do
        runtime_wheel_path="${DOWNLOAD_CACHE}/${runtime_wheel}"
        if unzip -Z1 "${runtime_wheel_path}" |
            grep -E '(^/|(^|/)\.\.(/|$))' >/dev/null; then
            printf 'unsafe path found in NVIDIA runtime wheel: %s\n' \
                "${runtime_wheel}" >&2
            exit 1
        fi
        unzip -q -n "${runtime_wheel_path}" -d "${runtime_staging}"
    done

    test -r "${runtime_staging}/nvidia/cudnn/lib/libcudnn.so.9"
    test -r "${runtime_staging}/nvidia/cusparselt/lib/libcusparseLt.so.0"
    test -r "${runtime_staging}/nvidia/nccl/lib/libnccl.so.2"
    test -r "${runtime_staging}/nvidia/nvshmem/lib/libnvshmem_host.so.3"

    cudnn_metadata="${runtime_staging}/nvidia_cudnn_cu13-${CUDNN_VERSION}.dist-info"
    cusparselt_metadata="${runtime_staging}/nvidia_cusparselt_cu13-${CUSPARSELT_VERSION}.dist-info"
    nccl_metadata="${runtime_staging}/nvidia_nccl_cu13-${NCCL_VERSION}.dist-info"
    nvshmem_metadata="${runtime_staging}/nvidia_nvshmem_cu13-${NVSHMEM_VERSION}.dist-info"
    runtime_metadata_root="${runtime_staging}/nvidia/.wheel-metadata"
    mkdir "${runtime_metadata_root}"
    for metadata_directory in \
        "${cudnn_metadata}" "${cusparselt_metadata}" \
        "${nccl_metadata}" "${nvshmem_metadata}"; do
        test -r "${metadata_directory}/METADATA"
        mv "${metadata_directory}" "${runtime_metadata_root}/"
    done
    [[ -z "$(find "${runtime_staging}" -mindepth 1 -maxdepth 1 \
        ! -name nvidia -print -quit)" ]] || {
        printf 'unexpected top-level content in NVIDIA runtime wheels\n' >&2
        exit 1
    }

    printf '%s\n' "${expected_runtime_manifest}" \
        >"${runtime_staging}/nvidia/.naturalehia-manifest"
    chown -R 0:0 "${runtime_staging}/nvidia"
    chmod -R a+rX,a-w "${runtime_staging}/nvidia"
    mv "${runtime_staging}/nvidia" "${NVIDIA_RUNTIME_VERSION_ROOT}"
    rmdir "${runtime_staging}"
    trap - EXIT
fi

[[ "$(<"${NVIDIA_RUNTIME_VERSION_ROOT}/.naturalehia-manifest")" == \
    "${expected_runtime_manifest}" ]]
runtime_link_target="${NVIDIA_RUNTIME_VERSION_ROOT##*/}"
if [[ ! -L "${NVIDIA_RUNTIME_ROOT}" ]] ||
    [[ "$(readlink "${NVIDIA_RUNTIME_ROOT}")" != "${runtime_link_target}" ]]; then
    [[ ! -e "${NVIDIA_RUNTIME_ROOT}" || -L "${NVIDIA_RUNTIME_ROOT}" ]] || {
        printf 'refusing to replace a non-symlink NVIDIA runtime selector\n' >&2
        exit 1
    }
    runtime_link_staging="${CONTAINER_GPU_ROOT}/.nvidia-runtime-link.$$"
    trap 'rm -f -- "${runtime_link_staging}"' EXIT
    ln -s "${runtime_link_target}" "${runtime_link_staging}"
    mv -Tf "${runtime_link_staging}" "${NVIDIA_RUNTIME_ROOT}"
    trap - EXIT
fi
[[ "$(readlink -f "${NVIDIA_RUNTIME_ROOT}")" == \
    "${NVIDIA_RUNTIME_VERSION_ROOT}" ]]

install -d -m 0755 -o "${DEV_UID}" -g "${DEV_GID}" \
    "${CONTAINER_HOME}" "${CONTAINER_HOME}/.cache" \
    "${CONTAINER_HOME}/.cache/ccache" "${CONTAINER_HOME}/.cache/nv" \
    "${CONTAINER_BUILD_ROOT}"
{
    printf 'export CUDA_HOME=%q\n' "${CUDA_ROOT}"
    printf 'export CUDAToolkit_ROOT=%q\n' "${CUDA_ROOT}"
    printf 'export CUDACXX=%q\n' "${CUDA_ROOT}/bin/nvcc"
    printf 'export LIBTORCH_ROOT=%q\n' "${LIBTORCH_ROOT}"
    printf 'export NVIDIA_RUNTIME_ROOT=%q\n' "${NVIDIA_RUNTIME_ROOT}"
    printf 'export NATURALEHIA_RUNTIME_STACK_ID=%q\n' "${NVIDIA_RUNTIME_ID}"
    printf 'export CUDNN_ROOT=%q\n' "${NVIDIA_RUNTIME_ROOT}/cudnn"
    printf 'export CUSPARSELT_ROOT=%q\n' "${NVIDIA_RUNTIME_ROOT}/cusparselt"
    printf 'export NCCL_ROOT=%q\n' "${NVIDIA_RUNTIME_ROOT}/nccl"
    printf 'export NVSHMEM_ROOT=%q\n' "${NVIDIA_RUNTIME_ROOT}/nvshmem"
    printf 'export NATURALEHIA_GPU_STACK_ID=%q\n' "${GPU_STACK_ID}"
    printf 'export CUDA_CACHE_PATH=%q\n' "${CONTAINER_HOME}/.cache/nv"
    # PATH is intentionally expanded when the generated profile is sourced.
    # shellcheck disable=SC2016
    printf 'export PATH=%q:${PATH}\n' "${CUDA_ROOT}/bin"
    # CMAKE_PREFIX_PATH is intentionally expanded when the profile is sourced.
    # shellcheck disable=SC2016
    printf 'export CMAKE_PREFIX_PATH=%q${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}\n' \
        "${LIBTORCH_ROOT}"
} >/etc/profile.d/naturalehia-fauna.sh
chmod 0644 /etc/profile.d/naturalehia-fauna.sh

install -d -m 0755 /var/lib/naturalehia-fauna
dpkg-query -W -f='${binary:Package}=${Version}\n' | sort \
    >/var/lib/naturalehia-fauna/package-manifest
chmod 0444 /var/lib/naturalehia-fauna/package-manifest
printf '%s\n' "${BOOTSTRAP_VERSION}" \
    >/var/lib/naturalehia-fauna/bootstrap-version

git_config="${CONTAINER_HOME}/.gitconfig"
if ! git config --file "${git_config}" --get-all safe.directory 2>/dev/null |
    grep -Fx -- "${CONTAINER_WORKSPACE}" >/dev/null; then
    git config --file "${git_config}" --add safe.directory "${CONTAINER_WORKSPACE}"
fi
chown "${DEV_UID}:${DEV_GID}" "${git_config}"

printf 'Naturalehia fauna development environment provisioned (schema %s)\n' \
    "${BOOTSTRAP_VERSION}"
