#!/usr/bin/env bash

set -Eeuo pipefail
IFS=$'\n\t'

readonly CONTAINER_NAME="naturalehia"
readonly CONTAINER_HOSTNAME="naturalehia"
readonly DEFAULT_IMAGE="debian:13-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd"
readonly CONTAINER_WORKSPACE="/workspace/naturalehia"
readonly PROJECT_RELATIVE_PATH="projects/the-elder-brother-of-fauna"
readonly CONTAINER_PROJECT_DIR="${CONTAINER_WORKSPACE}/${PROJECT_RELATIVE_PATH}"
readonly CONTAINER_BUILD_ROOT="/work/naturalehia-build"
readonly CONTAINER_PROJECT_BUILD_ROOT="${CONTAINER_BUILD_ROOT}/the-elder-brother-of-fauna"
readonly CONTAINER_HOME="/home/developer"
readonly CONTAINER_GPU_ROOT="/opt/naturalehia-gpu"
readonly BUILD_VOLUME="naturalehia-build"
readonly HOME_VOLUME="naturalehia-home"
readonly GPU_VOLUME="naturalehia-gpu"
readonly MANAGED_LABEL="io.naturalehia.devcontainer"
readonly CONFIG_LABEL="io.naturalehia.config"
readonly RECREATE_BACKUP_NAME="naturalehia-recreate-backup"
readonly LOOPBACK_ADDRESS="127.0.0.1"
readonly HTTP_CONTAINER_PORT="8080"
readonly INGEST_CONTAINER_PORT="50051"
readonly PIDS_LIMIT="2048"
readonly SHM_SIZE="1g"
readonly SHM_BYTES="1073741824"
readonly LOG_MAX_SIZE="10m"
readonly LOG_MAX_FILES="3"
readonly CONTAINER_PATH="/usr/local/cuda-13.1/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
readonly TOOLCHAIN_PACKAGE_SET="autoconf automake bash bison build-essential ca-certificates ccache clang clang-format clang-tidy clangd cmake cppcheck curl doxygen flex gdb gfortran git graphviz jq lcov less libbenchmark-dev libboost-filesystem-dev libboost-program-options-dev libboost-system-dev libcurl4-openssl-dev libeigen3-dev libfmt-dev libgrpc++-dev libgtest-dev liblapacke-dev liblz4-dev libnuma-dev libomp-dev libopenblas-dev libopencv-dev libprotobuf-dev libspdlog-dev libsqlite3-dev libssl-dev libtbb-dev libyaml-cpp-dev libzstd-dev lld meson nasm ninja-build nlohmann-json3-dev passwd pkg-config protobuf-compiler protobuf-compiler-grpc python3-dev python3-pip python3-venv shellcheck unzip valgrind wget xz-utils zip zlib1g-dev"
readonly CUDA_REPOSITORY="https://developer.download.nvidia.com/compute/cuda/repos/debian13/x86_64"
readonly CUDA_KEYRING_FILE="cuda-keyring_1.1-1_all.deb"
readonly CUDA_KEYRING_SHA256="d0d4ef986a44400f9db33c600ef33a985175e7cc63d805a10e1839c7a1e78f5f"
readonly CUDA_KEYRING_BYTES="4182"
readonly CUDA_TOOLKIT_PACKAGE="cuda-toolkit-13-1"
readonly CUDA_TOOLKIT_PACKAGE_VERSION="13.1.2-1"
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
readonly RECREATE_STATE_BASENAME=".recreate-state"
readonly RECREATE_STATE_FILE="${CONTAINER_GPU_ROOT}/${RECREATE_STATE_BASENAME}"
readonly BOOTSTRAP_VERSION="6"

IMAGE="${NATURALEHIA_IMAGE:-${DEFAULT_IMAGE}}"
HTTP_PORT="${NATURALEHIA_HTTP_PORT:-8080}"
INGEST_PORT="${NATURALEHIA_INGEST_PORT:-50051}"

log() {
    printf '[naturalehia] %s\n' "$*"
}

die() {
    printf '[naturalehia] error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: bash projects/the-elder-brother-of-fauna/setup.sh [command] [arguments]

Commands:
  up              Create, start, provision, and verify the container (default)
  shell           Open an interactive shell as the non-root development user
  exec CMD...     Run a command as the non-root development user
  build           Configure and build the project with GCC, Ninja, and ccache
  test            Build and run the complete CTest suite
  gpu-test        Compile and run CUDA and LibTorch GPU smoke tests
  run [ARGS...]   Build and run The Elder Brother of Fauna synthetic CLI
  status          Show container, mount, port, and GPU configuration
  stop            Stop the container without deleting it or its volumes
  recreate        Replace the managed container while preserving named volumes
  help            Show this help

Environment overrides:
  NATURALEHIA_IMAGE         Debian image reference (digest-pinned by default)
  NATURALEHIA_HTTP_PORT     Host loopback port for container port 8080 (default 8080)
  NATURALEHIA_INGEST_PORT   Host loopback port for container port 50051 (default 50051)
  NATURALEHIA_DEV_UID       Development user UID (detected on Linux; otherwise 1000)
  NATURALEHIA_DEV_GID       Development user GID (detected on Linux; otherwise 1000)

The environment includes a pinned CUDA 13.1 development toolkit and GPU-enabled
LibTorch 2.13. Host NVIDIA drivers remain outside the container.
EOF
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

validate_number() {
    local label="$1"
    local value="$2"
    local maximum="$3"
    [[ "${value}" =~ ^[0-9]+$ ]] || die "${label} must be an integer"
    ((10#${value} >= 1 && 10#${value} <= maximum)) ||
        die "${label} must be between 1 and ${maximum}"
}

detect_identity() {
    local detected_uid
    local detected_gid
    detected_uid="$(id -u)"
    detected_gid="$(id -g)"

    case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*)
            detected_uid=1000
            detected_gid=1000
            ;;
        *)
            if ((detected_uid == 0)); then
                if [[ "${SUDO_UID:-}" =~ ^[0-9]+$ ]] && ((10#${SUDO_UID} != 0)); then
                    detected_uid="${SUDO_UID}"
                elif [[ -z "${NATURALEHIA_DEV_UID:-}" ]]; then
                    die "refusing to infer a development UID from root; run as your normal user or set NATURALEHIA_DEV_UID"
                fi
            fi
            if ((detected_gid == 0)); then
                if [[ "${SUDO_GID:-}" =~ ^[0-9]+$ ]] && ((10#${SUDO_GID} != 0)); then
                    detected_gid="${SUDO_GID}"
                elif [[ -z "${NATURALEHIA_DEV_GID:-}" ]]; then
                    die "refusing to infer a development GID from root; run as your normal user or set NATURALEHIA_DEV_GID"
                fi
            fi
            ;;
    esac

    DEV_UID="${NATURALEHIA_DEV_UID:-${detected_uid}}"
    DEV_GID="${NATURALEHIA_DEV_GID:-${detected_gid}}"
    validate_number "development UID" "${DEV_UID}" 4294967294
    validate_number "development GID" "${DEV_GID}" 4294967294
}

resolve_workspace() {
    local script_dir
    local repository_dir
    script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
    [[ -f "${script_dir}/CMakeLists.txt" ]] ||
        die "setup.sh must remain in the project root"
    repository_dir="$(cd -- "${script_dir}/../.." && pwd -P)"
    [[ "${script_dir}" == "${repository_dir}/${PROJECT_RELATIVE_PATH}" ]] ||
        die "project must remain at ${PROJECT_RELATIVE_PATH} within the Naturalehia repository"
    [[ -e "${repository_dir}/.git" ]] ||
        die "could not find the Naturalehia repository metadata two levels above the project"

    case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*)
            require_command cygpath
            HOST_WORKSPACE="$(cygpath --absolute --mixed "${repository_dir}")"
            # Prevent MSYS from rewriting Linux container paths passed to docker.exe.
            export MSYS_NO_PATHCONV=1
            ;;
        *)
            HOST_WORKSPACE="${repository_dir}"
            ;;
    esac
    [[ "${HOST_WORKSPACE}" != *,* ]] ||
        die "repository paths containing commas are unsupported by Docker --mount"
}

acquire_lock() {
    local lock_root="${TMPDIR:-/tmp}"
    local owner_pid
    local stale_directory
    LOCK_DIRECTORY="${lock_root%/}/naturalehia-container.lock"
    if ! mkdir "${LOCK_DIRECTORY}" 2>/dev/null; then
        owner_pid=""
        if [[ -r "${LOCK_DIRECTORY}/owner" ]]; then
            IFS= read -r owner_pid <"${LOCK_DIRECTORY}/owner" || true
        fi
        if [[ "${owner_pid}" =~ ^[0-9]+$ ]] && kill -0 "${owner_pid}" 2>/dev/null; then
            die "another setup.sh process (PID ${owner_pid}) is managing the container"
        fi

        stale_directory="${LOCK_DIRECTORY}.stale.$$"
        mv -- "${LOCK_DIRECTORY}" "${stale_directory}" 2>/dev/null ||
            die "container lock changed while checking it; retry the command"
        rm -f -- "${stale_directory}/owner"
        rmdir -- "${stale_directory}" || die "could not remove stale container lock"
        mkdir "${LOCK_DIRECTORY}" || die "could not acquire container lock"
    fi
    printf '%s\n' "$$" >"${LOCK_DIRECTORY}/owner"
    trap 'rm -f -- "${LOCK_DIRECTORY}/owner"; rmdir -- "${LOCK_DIRECTORY}" 2>/dev/null || true' EXIT
}

ensure_docker() {
    require_command docker
    require_command sha256sum
    require_command awk

    docker info >/dev/null 2>&1 || die "Docker is unavailable or the daemon is not running"
    local operating_system
    local docker_endpoint
    operating_system="$(docker info --format '{{.OSType}}')"
    [[ "${operating_system}" == "linux" ]] ||
        die "Naturalehia requires Docker's Linux container engine"
    if [[ -n "${DOCKER_HOST:-}" ]]; then
        docker_endpoint="${DOCKER_HOST}"
    else
        docker_endpoint="$(docker context inspect "$(docker context show)" \
            --format '{{.Endpoints.docker.Host}}' 2>/dev/null)" ||
            die "could not inspect the active Docker context"
    fi
    case "${docker_endpoint}" in
        unix://* | npipe://*) ;;
        *) die "remote Docker contexts are unsupported because bind mounts resolve on the daemon host" ;;
    esac
}

container_exists() {
    docker container inspect "${CONTAINER_NAME}" >/dev/null 2>&1
}

container_running() {
    [[ "$(docker container inspect --format '{{.State.Running}}' "${CONTAINER_NAME}")" == "true" ]]
}

managed_container() {
    local container_name="${1:-${CONTAINER_NAME}}"
    [[ "$(docker container inspect --format "{{ index .Config.Labels \"${MANAGED_LABEL}\" }}" \
        "${container_name}")" == "true" ]]
}

ensure_image() {
    if ! docker image inspect "${IMAGE}" >/dev/null 2>&1; then
        log "pulling ${IMAGE}"
        docker pull "${IMAGE}"
    fi
    IMAGE_ID="$(docker image inspect --format '{{.Id}}' "${IMAGE}")"
}

calculate_fingerprint() {
    CONFIG_FINGERPRINT="$({
        printf 'schema=%s\n' "${BOOTSTRAP_VERSION}"
        printf 'image=%s\n' "${IMAGE_ID}"
        printf 'name=%s\n' "${CONTAINER_NAME}"
        printf 'hostname=%s\n' "${CONTAINER_HOSTNAME}"
        printf 'user=%s:%s\n' "${DEV_UID}" "${DEV_GID}"
        printf 'workspace=%s:%s:rprivate\n' "${HOST_WORKSPACE}" "${CONTAINER_WORKSPACE}"
        printf 'build-volume=%s:%s\n' "${BUILD_VOLUME}" "${CONTAINER_BUILD_ROOT}"
        printf 'home-volume=%s:%s\n' "${HOME_VOLUME}" "${CONTAINER_HOME}"
        printf 'gpu-volume=%s:%s\n' "${GPU_VOLUME}" "${CONTAINER_GPU_ROOT}"
        printf 'ports=%s:%s:%s/tcp,%s:%s:%s/tcp\n' \
            "${LOOPBACK_ADDRESS}" "${HTTP_PORT}" "${HTTP_CONTAINER_PORT}" \
            "${LOOPBACK_ADDRESS}" "${INGEST_PORT}" "${INGEST_CONTAINER_PORT}"
        printf 'runtime=init,restart:no,no-new-privileges,pids:%s,shm:%s\n' \
            "${PIDS_LIMIT}" "${SHM_SIZE}"
        printf 'gpu=all:compute,utility\n'
        printf 'environment=HOME:%s,USER:developer,LOGNAME:developer,CCACHE_DIR:%s/.cache/ccache,CUDA_HOME:%s,LIBTORCH_ROOT:%s,PATH:%s,LANG:C.UTF-8\n' \
            "${CONTAINER_HOME}" "${CONTAINER_HOME}" "${CUDA_ROOT}" \
            "${LIBTORCH_ROOT}" "${CONTAINER_PATH}"
        printf 'logging=local,max-size:%s,max-file:%s\n' "${LOG_MAX_SIZE}" "${LOG_MAX_FILES}"
        printf 'packages=%s\n' "${TOOLCHAIN_PACKAGE_SET}"
        printf 'cuda-repository=%s\n' "${CUDA_REPOSITORY}"
        printf 'cuda-keyring=%s:%s:%s\n' "${CUDA_KEYRING_FILE}" \
            "${CUDA_KEYRING_SHA256}" "${CUDA_KEYRING_BYTES}"
        printf 'cuda-toolkit=%s:%s\n' "${CUDA_TOOLKIT_PACKAGE}" "${CUDA_TOOLKIT_PACKAGE_VERSION}"
        printf 'libtorch=%s:%s:%s:%s\n' "${LIBTORCH_VERSION}" "${LIBTORCH_URL}" \
            "${LIBTORCH_SHA256}" "${LIBTORCH_ARCHIVE_BYTES}"
        printf 'cudnn=%s:%s:%s:%s\n' "${CUDNN_VERSION}" "${CUDNN_WHEEL_URL}" \
            "${CUDNN_WHEEL_SHA256}" "${CUDNN_WHEEL_BYTES}"
        printf 'cusparselt=%s:%s:%s:%s\n' "${CUSPARSELT_VERSION}" \
            "${CUSPARSELT_WHEEL_URL}" "${CUSPARSELT_WHEEL_SHA256}" \
            "${CUSPARSELT_WHEEL_BYTES}"
        printf 'nccl=%s:%s:%s:%s\n' "${NCCL_VERSION}" "${NCCL_WHEEL_URL}" \
            "${NCCL_WHEEL_SHA256}" "${NCCL_WHEEL_BYTES}"
        printf 'nvshmem=%s:%s:%s:%s\n' "${NVSHMEM_VERSION}" \
            "${NVSHMEM_WHEEL_URL}" "${NVSHMEM_WHEEL_SHA256}" \
            "${NVSHMEM_WHEEL_BYTES}"
    } | sha256sum | awk '{print $1}')"
}

verify_existing_configuration() {
    managed_container ||
        die "a container named '${CONTAINER_NAME}' exists but is not managed by setup.sh"

    local existing_fingerprint
    existing_fingerprint="$(docker container inspect \
        --format "{{ index .Config.Labels \"${CONFIG_LABEL}\" }}" "${CONTAINER_NAME}")"
    [[ "${existing_fingerprint}" == "${CONFIG_FINGERPRINT}" ]] ||
        die "container settings changed; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate' to apply them"

    [[ "$(docker container inspect --format '{{.Image}}' "${CONTAINER_NAME}")" == "${IMAGE_ID}" ]] ||
        die "managed container image differs from the requested image; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.Config.User}}' "${CONTAINER_NAME}")" == "${DEV_UID}:${DEV_GID}" ]] ||
        die "managed container user was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.Config.WorkingDir}}' "${CONTAINER_NAME}")" == "${CONTAINER_WORKSPACE}" ]] ||
        die "managed container work directory was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.RestartPolicy.Name}}' "${CONTAINER_NAME}")" == "no" ]] ||
        die "managed container restart policy was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.PidsLimit}}' "${CONTAINER_NAME}")" == "${PIDS_LIMIT}" ]] ||
        die "managed container PID limit was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.ShmSize}}' "${CONTAINER_NAME}")" == "${SHM_BYTES}" ]] ||
        die "managed container shared-memory limit was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.Init}}' "${CONTAINER_NAME}")" == "true" ]] ||
        die "managed container init setting was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{json .HostConfig.SecurityOpt}}' "${CONTAINER_NAME}")" == '["no-new-privileges=true"]' ]] ||
        die "managed container security options were modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.LogConfig.Type}}' "${CONTAINER_NAME}")" == "local" ]] ||
        die "managed container log driver was modified; run 'bash projects/the-elder-brother-of-fauna/setup.sh recreate'"
}

debian_preflight() {
    log "verifying Debian base image"
    docker run --rm "${IMAGE}" bash -Eeuo pipefail -c '
        source /etc/os-release
        [[ "${ID}" == "debian" ]]
        [[ "${VERSION_ID}" == "13" ]]
        [[ "$(dpkg --print-architecture)" == "amd64" ]]
        [[ "$(uname -m)" == "x86_64" ]]
    ' || die "NATURALEHIA_IMAGE must reference Debian 13 for x86-64/amd64"
}

gpu_preflight() {
    log "verifying NVIDIA GPU passthrough"
    docker run --rm --gpus all \
        --env NVIDIA_VISIBLE_DEVICES=all \
        --env NVIDIA_DRIVER_CAPABILITIES=compute,utility \
        "${IMAGE}" nvidia-smi -L >/dev/null
}

ensure_managed_volume() {
    local volume_name="$1"
    local driver
    local managed
    if ! docker volume inspect "${volume_name}" >/dev/null 2>&1; then
        docker volume create --label "${MANAGED_LABEL}=true" "${volume_name}" >/dev/null
    fi
    managed="$(docker volume inspect \
        --format "{{ index .Labels \"${MANAGED_LABEL}\" }}" "${volume_name}")"
    [[ "${managed}" == "true" ]] ||
        die "volume '${volume_name}' exists but is not managed by setup.sh"
    driver="$(docker volume inspect --format '{{.Driver}}' "${volume_name}")"
    [[ "${driver}" == "local" ]] ||
        die "managed volume '${volume_name}' must use Docker's local driver"
}

prepare_container_resources() {
    debian_preflight
    gpu_preflight
    ensure_managed_volume "${BUILD_VOLUME}"
    ensure_managed_volume "${HOME_VOLUME}"
    ensure_managed_volume "${GPU_VOLUME}"
}

create_container() {

    log "creating Debian container '${CONTAINER_NAME}'"
    docker run --detach \
        --name "${CONTAINER_NAME}" \
        --hostname "${CONTAINER_HOSTNAME}" \
        --init \
        --restart no \
        --user "${DEV_UID}:${DEV_GID}" \
        --workdir "${CONTAINER_WORKSPACE}" \
        --env "HOME=${CONTAINER_HOME}" \
        --env "USER=developer" \
        --env "LOGNAME=developer" \
        --env "CCACHE_DIR=${CONTAINER_HOME}/.cache/ccache" \
        --env "CUDA_CACHE_PATH=${CONTAINER_HOME}/.cache/nv" \
        --env "CUDA_HOME=${CUDA_ROOT}" \
        --env "CUDAToolkit_ROOT=${CUDA_ROOT}" \
        --env "CUDACXX=${CUDA_ROOT}/bin/nvcc" \
        --env "LIBTORCH_ROOT=${LIBTORCH_ROOT}" \
        --env "NATURALEHIA_GPU_STACK_ID=${GPU_STACK_ID}" \
        --env "NATURALEHIA_RUNTIME_STACK_ID=${NVIDIA_RUNTIME_ID}" \
        --env "CMAKE_PREFIX_PATH=${LIBTORCH_ROOT}" \
        --env "PATH=${CONTAINER_PATH}" \
        --env "LANG=C.UTF-8" \
        --env "NVIDIA_VISIBLE_DEVICES=all" \
        --env "NVIDIA_DRIVER_CAPABILITIES=compute,utility" \
        --gpus all \
        --security-opt no-new-privileges=true \
        --pids-limit "${PIDS_LIMIT}" \
        --shm-size "${SHM_SIZE}" \
        --publish "${LOOPBACK_ADDRESS}:${HTTP_PORT}:${HTTP_CONTAINER_PORT}/tcp" \
        --publish "${LOOPBACK_ADDRESS}:${INGEST_PORT}:${INGEST_CONTAINER_PORT}/tcp" \
        --mount "type=bind,source=${HOST_WORKSPACE},target=${CONTAINER_WORKSPACE},bind-propagation=rprivate" \
        --mount "type=volume,source=${BUILD_VOLUME},target=${CONTAINER_BUILD_ROOT}" \
        --mount "type=volume,source=${HOME_VOLUME},target=${CONTAINER_HOME}" \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT}" \
        --label "${MANAGED_LABEL}=true" \
        --label "${CONFIG_LABEL}=${CONFIG_FINGERPRINT}" \
        --label "io.naturalehia.workspace=${HOST_WORKSPACE}" \
        --label "io.naturalehia.image=${IMAGE}" \
        --label "io.naturalehia.gpu-stack=${GPU_STACK_ID}" \
        --log-driver local \
        --log-opt "max-size=${LOG_MAX_SIZE}" \
        --log-opt "max-file=${LOG_MAX_FILES}" \
        "${IMAGE}" sleep infinity >/dev/null
}

start_container() {
    if ! container_exists; then
        prepare_container_resources
        create_container
    else
        verify_existing_configuration
        if ! container_running; then
            log "starting '${CONTAINER_NAME}'"
            docker start "${CONTAINER_NAME}" >/dev/null
        fi
    fi
}

provision_container() {
    log "provisioning Debian C++, CUDA, and LibTorch development toolchain"
    docker exec --user 0:0 \
        --env "DEV_UID=${DEV_UID}" \
        --env "DEV_GID=${DEV_GID}" \
        --env "BOOTSTRAP_VERSION=${BOOTSTRAP_VERSION}" \
        --env "CONTAINER_HOME=${CONTAINER_HOME}" \
        --env "CONTAINER_BUILD_ROOT=${CONTAINER_BUILD_ROOT}" \
        --env "CONTAINER_GPU_ROOT=${CONTAINER_GPU_ROOT}" \
        --env "DOWNLOAD_CACHE=${DOWNLOAD_CACHE}" \
        --env "TOOLCHAIN_PACKAGE_SET=${TOOLCHAIN_PACKAGE_SET}" \
        --env "CUDA_REPOSITORY=${CUDA_REPOSITORY}" \
        --env "CUDA_KEYRING_FILE=${CUDA_KEYRING_FILE}" \
        --env "CUDA_KEYRING_SHA256=${CUDA_KEYRING_SHA256}" \
        --env "CUDA_KEYRING_BYTES=${CUDA_KEYRING_BYTES}" \
        --env "CUDA_TOOLKIT_PACKAGE=${CUDA_TOOLKIT_PACKAGE}" \
        --env "CUDA_TOOLKIT_PACKAGE_VERSION=${CUDA_TOOLKIT_PACKAGE_VERSION}" \
        --env "CUDA_ROOT=${CUDA_ROOT}" \
        --env "LIBTORCH_VERSION=${LIBTORCH_VERSION}" \
        --env "LIBTORCH_CUDA_TAG=${LIBTORCH_CUDA_TAG}" \
        --env "LIBTORCH_ARCHIVE=${LIBTORCH_ARCHIVE}" \
        --env "LIBTORCH_URL=${LIBTORCH_URL}" \
        --env "LIBTORCH_SHA256=${LIBTORCH_SHA256}" \
        --env "LIBTORCH_ARCHIVE_BYTES=${LIBTORCH_ARCHIVE_BYTES}" \
        --env "LIBTORCH_ROOT=${LIBTORCH_ROOT}" \
        --env "NVIDIA_RUNTIME_ROOT=${NVIDIA_RUNTIME_ROOT}" \
        --env "NVIDIA_RUNTIME_ID=${NVIDIA_RUNTIME_ID}" \
        --env "NVIDIA_RUNTIME_VERSION_ROOT=${NVIDIA_RUNTIME_VERSION_ROOT}" \
        --env "CUDNN_VERSION=${CUDNN_VERSION}" \
        --env "CUDNN_WHEEL=${CUDNN_WHEEL}" \
        --env "CUDNN_WHEEL_URL=${CUDNN_WHEEL_URL}" \
        --env "CUDNN_WHEEL_SHA256=${CUDNN_WHEEL_SHA256}" \
        --env "CUDNN_WHEEL_BYTES=${CUDNN_WHEEL_BYTES}" \
        --env "CUSPARSELT_VERSION=${CUSPARSELT_VERSION}" \
        --env "CUSPARSELT_WHEEL=${CUSPARSELT_WHEEL}" \
        --env "CUSPARSELT_WHEEL_URL=${CUSPARSELT_WHEEL_URL}" \
        --env "CUSPARSELT_WHEEL_SHA256=${CUSPARSELT_WHEEL_SHA256}" \
        --env "CUSPARSELT_WHEEL_BYTES=${CUSPARSELT_WHEEL_BYTES}" \
        --env "NCCL_VERSION=${NCCL_VERSION}" \
        --env "NCCL_WHEEL=${NCCL_WHEEL}" \
        --env "NCCL_WHEEL_URL=${NCCL_WHEEL_URL}" \
        --env "NCCL_WHEEL_SHA256=${NCCL_WHEEL_SHA256}" \
        --env "NCCL_WHEEL_BYTES=${NCCL_WHEEL_BYTES}" \
        --env "NVSHMEM_VERSION=${NVSHMEM_VERSION}" \
        --env "NVSHMEM_WHEEL=${NVSHMEM_WHEEL}" \
        --env "NVSHMEM_WHEEL_URL=${NVSHMEM_WHEEL_URL}" \
        --env "NVSHMEM_WHEEL_SHA256=${NVSHMEM_WHEEL_SHA256}" \
        --env "NVSHMEM_WHEEL_BYTES=${NVSHMEM_WHEEL_BYTES}" \
        --env "GPU_STACK_ID=${GPU_STACK_ID}" \
        "${CONTAINER_NAME}" bash -Eeuo pipefail -c '
            export DEBIAN_FRONTEND=noninteractive
            if [[ -f /etc/apt/sources.list.d/debian.sources ]] &&
                ! grep -Eq "^Components:.*(^|[[:space:]])contrib([[:space:]]|$)" \
                    /etc/apt/sources.list.d/debian.sources; then
                sed -i "/^Components:/ s/$/ contrib/" /etc/apt/sources.list.d/debian.sources
            fi

            read -r -a packages <<<"${TOOLCHAIN_PACKAGE_SET}"
            missing=()
            for package in "${packages[@]}"; do
                dpkg-query --show --showformat="\${Status}" "${package}" 2>/dev/null |
                    grep "install ok installed" >/dev/null || missing+=("${package}")
            done
            if ((${#missing[@]} != 0)); then
                apt-get update
                apt-get install -y --no-install-recommends "${missing[@]}"
            fi

            install -d -m 0700 -o 0 -g 0 "${DOWNLOAD_CACHE}"
            ensure_cached_download() {
                local filename="$1"
                local url="$2"
                local expected_sha256="$3"
                local expected_bytes="$4"
                local target="${DOWNLOAD_CACHE}/${filename}"
                local temporary="${target}.part"

                if [[ -f "${target}" ]] && [[ "$(stat -c %s "${target}")" == "${expected_bytes}" ]] &&
                    printf "%s  %s\n" "${expected_sha256}" "${target}" |
                        sha256sum --check --status; then
                    return
                fi

                if [[ -e "${temporary}" ]] &&
                    { [[ ! -f "${temporary}" ]] ||
                        (( $(stat -c %s "${temporary}") >= expected_bytes )); }; then
                    rm -f -- "${temporary}"
                fi
                (
                    umask 077
                    curl --fail --location --proto "=https" --proto-redir "=https" \
                        --tlsv1.2 \
                        --retry 5 --retry-all-errors --connect-timeout 30 \
                        --continue-at - \
                        --output "${temporary}" "${url}"
                )
                printf "%s  %s\n" "${expected_sha256}" "${temporary}" |
                    sha256sum --check --status || {
                        rm -f -- "${temporary}"
                        echo "checksum verification failed for ${url}" >&2
                        exit 1
                    }
                [[ "$(stat -c %s "${temporary}")" == "${expected_bytes}" ]] || {
                    rm -f -- "${temporary}"
                    echo "size verification failed for ${url}" >&2
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
            installed_keyring="$(dpkg-query --show --showformat="\${Version}" \
                cuda-keyring 2>/dev/null || true)"
            if [[ "${installed_keyring}" != "1.1-1" ]]; then
                apt-get install -y --no-install-recommends "${keyring_path}"
            fi

            installed_cuda="$(dpkg-query --show --showformat="\${Version}" \
                "${CUDA_TOOLKIT_PACKAGE}" 2>/dev/null || true)"
            if [[ "${installed_cuda}" != "${CUDA_TOOLKIT_PACKAGE_VERSION}" ]]; then
                apt-get update
                apt-get install -y --no-install-recommends --allow-downgrades \
                    "${CUDA_TOOLKIT_PACKAGE}=${CUDA_TOOLKIT_PACKAGE_VERSION}"
            fi
            [[ "$(readlink -f /usr/local/cuda)" == "${CUDA_ROOT}" ]]
            if dpkg-query --show --showformat="\${binary:Package}\n" 2>/dev/null |
                grep -E "^(cuda-drivers|nvidia-driver)(:|$)" >/dev/null; then
                echo "refusing an in-container NVIDIA driver installation" >&2
                exit 1
            fi

            apt-get clean
            rm -rf /var/lib/apt/lists/*

            if id developer >/dev/null 2>&1; then
                [[ "$(id -u developer)" == "${DEV_UID}" ]]
                [[ "$(id -g developer)" == "${DEV_GID}" ]]
            else
                if getent passwd "${DEV_UID}" >/dev/null; then
                    echo "UID ${DEV_UID} is already assigned" >&2
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
                    printf "%s\n" "${expected}" >"${marker}"
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
                    echo "existing LibTorch path failed its manifest check: ${LIBTORCH_ROOT}" >&2
                    exit 1
                }
                if unzip -Z1 "${libtorch_archive_path}" |
                    grep -E "(^/|(^|/)\.\.(/|$))" >/dev/null; then
                    echo "unsafe path found in LibTorch archive" >&2
                    exit 1
                fi

                libtorch_staging="${CONTAINER_GPU_ROOT}/.libtorch-staging.$$"
                trap "rm -rf -- \"${libtorch_staging}\"" EXIT
                mkdir "${libtorch_staging}"
                unzip -q "${libtorch_archive_path}" -d "${libtorch_staging}"
                test -r "${libtorch_staging}/libtorch/share/cmake/Torch/TorchConfig.cmake"
                test -r "${libtorch_staging}/libtorch/lib/libtorch.so"
                test -r "${libtorch_staging}/libtorch/lib/libtorch_cuda.so"
                [[ "$(<"${libtorch_staging}/libtorch/build-version")" == \
                    "${LIBTORCH_VERSION}+${LIBTORCH_CUDA_TAG}" ]]
                printf "%s\n" "${expected_libtorch_manifest}" \
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
                printf "cudnn=%s:%s\n" "${CUDNN_VERSION}" "${CUDNN_WHEEL_SHA256}"
                printf "cusparselt=%s:%s\n" \
                    "${CUSPARSELT_VERSION}" "${CUSPARSELT_WHEEL_SHA256}"
                printf "nccl=%s:%s\n" "${NCCL_VERSION}" "${NCCL_WHEEL_SHA256}"
                printf "nvshmem=%s:%s\n" "${NVSHMEM_VERSION}" "${NVSHMEM_WHEEL_SHA256}"
            )"
            installed_runtime_manifest=""
            [[ ! -r "${runtime_manifest}" ]] ||
                installed_runtime_manifest="$(<"${runtime_manifest}")"

            # Migrate the original unversioned installation once. A release that
            # matches the current lock becomes the current versioned runtime. An
            # older release is retained immutably under a manifest-derived path.
            if [[ -d "${NVIDIA_RUNTIME_ROOT}" && ! -L "${NVIDIA_RUNTIME_ROOT}" ]]; then
                unversioned_manifest_path="${NVIDIA_RUNTIME_ROOT}/.naturalehia-manifest"
                [[ -f "${unversioned_manifest_path}" && \
                    ! -L "${unversioned_manifest_path}" ]] || {
                    echo "unversioned NVIDIA runtime has no regular manifest" >&2
                    exit 1
                }
                unversioned_runtime_manifest="$(<"${unversioned_manifest_path}")"
                [[ -n "${unversioned_runtime_manifest}" ]] || {
                    echo "unversioned NVIDIA runtime has an empty manifest" >&2
                    exit 1
                }
                chown -R 0:0 "${NVIDIA_RUNTIME_ROOT}"
                chmod -R a+rX,a-w "${NVIDIA_RUNTIME_ROOT}"

                if [[ "${unversioned_runtime_manifest}" == \
                    "${expected_runtime_manifest}" ]]; then
                    [[ ! -e "${NVIDIA_RUNTIME_VERSION_ROOT}" && \
                        ! -L "${NVIDIA_RUNTIME_VERSION_ROOT}" ]] || {
                        echo "both versioned and unversioned NVIDIA runtimes exist" >&2
                        exit 1
                    }
                    mv "${NVIDIA_RUNTIME_ROOT}" "${NVIDIA_RUNTIME_VERSION_ROOT}"
                    installed_runtime_manifest="${expected_runtime_manifest}"
                else
                    legacy_manifest_hash="$(sha256sum -- "${unversioned_manifest_path}")"
                    legacy_manifest_hash="${legacy_manifest_hash%% *}"
                    [[ "${legacy_manifest_hash}" =~ ^[0-9a-f]{64}$ ]] || {
                        echo "could not fingerprint the legacy NVIDIA runtime manifest" >&2
                        exit 1
                    }
                    legacy_runtime_root="${CONTAINER_GPU_ROOT}/nvidia-legacy-${legacy_manifest_hash}"
                    [[ ! -e "${legacy_runtime_root}" && \
                        ! -L "${legacy_runtime_root}" ]] || {
                        echo "legacy NVIDIA runtime destination already exists: ${legacy_runtime_root}" >&2
                        exit 1
                    }
                    mv "${NVIDIA_RUNTIME_ROOT}" "${legacy_runtime_root}"
                    printf "retained previous NVIDIA runtime at %s\n" \
                        "${legacy_runtime_root}"
                fi
            fi

            if [[ "${installed_runtime_manifest}" != "${expected_runtime_manifest}" ]]; then
                [[ ! -e "${NVIDIA_RUNTIME_VERSION_ROOT}" && \
                    ! -L "${NVIDIA_RUNTIME_VERSION_ROOT}" ]] || {
                    echo "existing NVIDIA runtime failed its manifest check: ${NVIDIA_RUNTIME_VERSION_ROOT}" >&2
                    exit 1
                }

                runtime_staging="${CONTAINER_GPU_ROOT}/.nvidia-runtime-staging.$$"
                trap "rm -rf -- \"${runtime_staging}\"" EXIT
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
                        grep -E "(^/|(^|/)\.\.(/|$))" >/dev/null; then
                        echo "unsafe path found in NVIDIA runtime wheel: ${runtime_wheel}" >&2
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
                    echo "unexpected top-level content in NVIDIA runtime wheels" >&2
                    exit 1
                }

                printf "%s\n" "${expected_runtime_manifest}" \
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
                    echo "refusing to replace a non-symlink NVIDIA runtime selector" >&2
                    exit 1
                }
                runtime_link_staging="${CONTAINER_GPU_ROOT}/.nvidia-runtime-link.$$"
                trap "rm -f -- \"${runtime_link_staging}\"" EXIT
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
                printf "export CUDA_HOME=%q\n" "${CUDA_ROOT}"
                printf "export CUDAToolkit_ROOT=%q\n" "${CUDA_ROOT}"
                printf "export CUDACXX=%q\n" "${CUDA_ROOT}/bin/nvcc"
                printf "export LIBTORCH_ROOT=%q\n" "${LIBTORCH_ROOT}"
                printf "export NVIDIA_RUNTIME_ROOT=%q\n" "${NVIDIA_RUNTIME_ROOT}"
                printf "export NATURALEHIA_RUNTIME_STACK_ID=%q\n" "${NVIDIA_RUNTIME_ID}"
                printf "export CUDNN_ROOT=%q\n" "${NVIDIA_RUNTIME_ROOT}/cudnn"
                printf "export CUSPARSELT_ROOT=%q\n" "${NVIDIA_RUNTIME_ROOT}/cusparselt"
                printf "export NCCL_ROOT=%q\n" "${NVIDIA_RUNTIME_ROOT}/nccl"
                printf "export NVSHMEM_ROOT=%q\n" "${NVIDIA_RUNTIME_ROOT}/nvshmem"
                printf "export NATURALEHIA_GPU_STACK_ID=%q\n" "${GPU_STACK_ID}"
                printf "export CUDA_CACHE_PATH=%q\n" "${CONTAINER_HOME}/.cache/nv"
                printf "export PATH=%q:\${PATH}\n" "${CUDA_ROOT}/bin"
                printf "export CMAKE_PREFIX_PATH=%q\${CMAKE_PREFIX_PATH:+:\${CMAKE_PREFIX_PATH}}\n" \
                    "${LIBTORCH_ROOT}"
            } >/etc/profile.d/naturalehia.sh
            chmod 0644 /etc/profile.d/naturalehia.sh
            install -d -m 0755 /var/lib/naturalehia-dev
            dpkg-query -W -f="\${binary:Package}=\${Version}\n" | sort \
                >/var/lib/naturalehia-dev/package-manifest
            chmod 0444 /var/lib/naturalehia-dev/package-manifest
            printf "%s\n" "${BOOTSTRAP_VERSION}" \
                >/var/lib/naturalehia-dev/bootstrap-version
        '

    docker exec --user "${DEV_UID}:${DEV_GID}" \
        --env "HOME=${CONTAINER_HOME}" \
        --env "WORKSPACE=${CONTAINER_WORKSPACE}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        "${CONTAINER_NAME}" bash -Eeuo pipefail -c '
            if ! git config --global --get-all safe.directory |
                grep -Fx -- "${WORKSPACE}" >/dev/null; then
                git config --global --add safe.directory "${WORKSPACE}"
            fi
        '
}

verify_container() {
    log "verifying Linux toolchain, repository mount, shell, and GPU"
    docker exec --user "${DEV_UID}:${DEV_GID}" \
        --env "HOME=${CONTAINER_HOME}" \
        --env "CONTAINER_BUILD_ROOT=${CONTAINER_BUILD_ROOT}" \
        --env "CUDA_CACHE_PATH=${CONTAINER_HOME}/.cache/nv" \
        --env "CUDA_HOME=${CUDA_ROOT}" \
        --env "CUDAToolkit_ROOT=${CUDA_ROOT}" \
        --env "CUDACXX=${CUDA_ROOT}/bin/nvcc" \
        --env "LIBTORCH_ROOT=${LIBTORCH_ROOT}" \
        --env "NVIDIA_RUNTIME_ROOT=${NVIDIA_RUNTIME_ROOT}" \
        --env "NVIDIA_RUNTIME_VERSION_ROOT=${NVIDIA_RUNTIME_VERSION_ROOT}" \
        --env "CUDNN_ROOT=${NVIDIA_RUNTIME_ROOT}/cudnn" \
        --env "CUSPARSELT_ROOT=${NVIDIA_RUNTIME_ROOT}/cusparselt" \
        --env "NCCL_ROOT=${NVIDIA_RUNTIME_ROOT}/nccl" \
        --env "NVSHMEM_ROOT=${NVIDIA_RUNTIME_ROOT}/nvshmem" \
        --env "CMAKE_PREFIX_PATH=${LIBTORCH_ROOT}" \
        --env "PATH=${CONTAINER_PATH}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        "${CONTAINER_NAME}" bash -Eeuo pipefail -c '
            test -r CMakeLists.txt
            test -w "${PWD}"
            test -w "${HOME}"
            test -w "${CONTAINER_BUILD_ROOT}"
            test "$(uname -s)" = Linux
            source /etc/os-release
            test "${ID}" = debian
            test "${VERSION_ID}" = 13
            test "$(dpkg --print-architecture)" = amd64
            test "$(uname -m)" = x86_64
            command -v cmake >/dev/null
            command -v g++ >/dev/null
            command -v clang++ >/dev/null
            command -v ninja >/dev/null
            command -v ccache >/dev/null
            command -v nvidia-smi >/dev/null
            command -v nvcc >/dev/null
            command -v cuda-gdb >/dev/null
            command -v compute-sanitizer >/dev/null
            nvcc --version | grep -F "release 13.1" >/dev/null
            dpkg-query -W -f="\${binary:Package}=\${Version}\n" | sort | \
                cmp -s - /var/lib/naturalehia-dev/package-manifest
            test -r "${LIBTORCH_ROOT}/share/cmake/Torch/TorchConfig.cmake"
            test -r "${LIBTORCH_ROOT}/lib/libtorch_cuda.so"
            test ! -w "${LIBTORCH_ROOT}"
            test "$(readlink -f "${NVIDIA_RUNTIME_ROOT}")" = \
                "${NVIDIA_RUNTIME_VERSION_ROOT}"
            test ! -w "${NVIDIA_RUNTIME_VERSION_ROOT}"
            test -r "${NVIDIA_RUNTIME_ROOT}/cudnn/lib/libcudnn.so.9"
            test -r "${NVIDIA_RUNTIME_ROOT}/cudnn/include/cudnn.h"
            test -r "${NVIDIA_RUNTIME_ROOT}/cusparselt/lib/libcusparseLt.so.0"
            test -r "${NVIDIA_RUNTIME_ROOT}/cusparselt/include/cusparseLt.h"
            test -r "${NVIDIA_RUNTIME_ROOT}/nccl/lib/libnccl.so.2"
            test -r "${NVIDIA_RUNTIME_ROOT}/nccl/include/nccl.h"
            test -r "${NVIDIA_RUNTIME_ROOT}/nvshmem/lib/libnvshmem_host.so.3"
            test -r "${NVIDIA_RUNTIME_ROOT}/nvshmem/include/nvshmem.h"
            shellcheck setup.sh
            nvidia-smi -L
        '
}

ensure_ready() {
    ensure_docker
    ensure_image
    calculate_fingerprint
    start_container
    provision_container
    verify_container
}

as_developer() {
    docker exec --user "${DEV_UID}:${DEV_GID}" \
        --env "HOME=${CONTAINER_HOME}" \
        --env "USER=developer" \
        --env "LOGNAME=developer" \
        --env "CCACHE_DIR=${CONTAINER_HOME}/.cache/ccache" \
        --env "CUDA_CACHE_PATH=${CONTAINER_HOME}/.cache/nv" \
        --env "CUDA_HOME=${CUDA_ROOT}" \
        --env "CUDAToolkit_ROOT=${CUDA_ROOT}" \
        --env "CUDACXX=${CUDA_ROOT}/bin/nvcc" \
        --env "LIBTORCH_ROOT=${LIBTORCH_ROOT}" \
        --env "NVIDIA_RUNTIME_ROOT=${NVIDIA_RUNTIME_ROOT}" \
        --env "CUDNN_ROOT=${NVIDIA_RUNTIME_ROOT}/cudnn" \
        --env "CUSPARSELT_ROOT=${NVIDIA_RUNTIME_ROOT}/cusparselt" \
        --env "NCCL_ROOT=${NVIDIA_RUNTIME_ROOT}/nccl" \
        --env "NVSHMEM_ROOT=${NVIDIA_RUNTIME_ROOT}/nvshmem" \
        --env "CMAKE_PREFIX_PATH=${LIBTORCH_ROOT}" \
        --env "PATH=${CONTAINER_PATH}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        "${CONTAINER_NAME}" "$@"
}

select_build_directory() {
    local toolchain_fingerprint
    # shellcheck disable=SC2016 # dpkg-query placeholders belong to the inner shell.
    toolchain_fingerprint="$(as_developer bash -Eeuo pipefail -c '
        {
            g++ --version
            g++ -dumpmachine
            cmake --version
            ninja --version
            ccache --version
            nvcc --version
            nvidia-smi --query-gpu=compute_cap --format=csv,noheader | sed "s/[[:space:]]//g" | sort -u
            cat "${LIBTORCH_ROOT}/.naturalehia-manifest"
            cat "${NVIDIA_RUNTIME_ROOT}/.naturalehia-manifest"
            cat /var/lib/naturalehia-dev/package-manifest
        } | sha256sum | cut -d " " -f 1
    ')"
    [[ "${toolchain_fingerprint}" =~ ^[0-9a-f]{64}$ ]] ||
        die "could not fingerprint the installed Debian toolchain"
    CONTAINER_BUILD_DIR="${CONTAINER_PROJECT_BUILD_ROOT}/${CONFIG_FINGERPRINT}/${toolchain_fingerprint}"
}

build_project() {
    select_build_directory
    log "building with Debian GCC and Ninja"
    as_developer cmake \
        -S "${CONTAINER_PROJECT_DIR}" \
        -B "${CONTAINER_BUILD_DIR}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    as_developer cmake --build "${CONTAINER_BUILD_DIR}" --parallel
}

test_project() {
    build_project
    log "running CTest inside Debian"
    as_developer ctest --test-dir "${CONTAINER_BUILD_DIR}" --output-on-failure
}

test_gpu_stack() {
    select_build_directory
    local compute_capabilities
    local capability
    local cuda_architectures=""
    local torch_architectures=""
    compute_capabilities="$(as_developer nvidia-smi \
        --query-gpu=compute_cap --format=csv,noheader | \
        sed 's/[[:space:]]//g' | sort -u)"
    [[ -n "${compute_capabilities}" ]] ||
        die "NVIDIA did not report a GPU compute capability"
    while IFS= read -r capability; do
        [[ "${capability}" =~ ^([0-9]+)\.([0-9]+)$ ]] ||
            die "unexpected NVIDIA compute capability: ${capability}"
        cuda_architectures+="${BASH_REMATCH[1]}${BASH_REMATCH[2]};"
        torch_architectures+="${capability} "
    done <<<"${compute_capabilities}"
    cuda_architectures="${cuda_architectures%;}"
    torch_architectures="${torch_architectures% }"

    local gpu_build_directory="${CONTAINER_BUILD_DIR}/gpu-smoke"
    log "building CUDA and LibTorch smoke tests for compute ${torch_architectures}"
    as_developer cmake \
        -S "${CONTAINER_PROJECT_DIR}/cmake/gpu-smoke" \
        -B "${gpu_build_directory}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_COMPILER=g++ \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CUDA_COMPILER="${CUDA_ROOT}/bin/nvcc" \
        -DCMAKE_CUDA_COMPILER_LAUNCHER=ccache \
        -DNATURALEHIA_FAUNA_CUDA_ARCHITECTURES="${cuda_architectures}" \
        -DNATURALEHIA_FAUNA_TORCH_CUDA_ARCH_LIST="${torch_architectures}" \
        -DTorch_DIR="${LIBTORCH_ROOT}/share/cmake/Torch"
    as_developer cmake --build "${gpu_build_directory}" --parallel
    log "running CUDA and LibTorch smoke tests on the GPU"
    as_developer ctest --test-dir "${gpu_build_directory}" --output-on-failure
}

show_status() {
    ensure_docker
    if ! container_exists; then
        log "container '${CONTAINER_NAME}' does not exist"
        return
    fi
    docker container inspect --format \
        'name={{.Name}} status={{.State.Status}} image={{.Config.Image}} user={{.Config.User}}' \
        "${CONTAINER_NAME}"
    docker container inspect --format \
        '{{range .Mounts}}{{println .Type .Source "->" .Destination}}{{end}}' \
        "${CONTAINER_NAME}"
    docker port "${CONTAINER_NAME}"
    docker container inspect --format 'gpu={{json .HostConfig.DeviceRequests}}' \
        "${CONTAINER_NAME}"
    if container_running; then
        docker exec "${CONTAINER_NAME}" nvidia-smi \
            --query-gpu=name,driver_version,memory.total --format=csv,noheader
        docker exec "${CONTAINER_NAME}" nvcc --version | tail -n 1
        docker exec "${CONTAINER_NAME}" cat "${LIBTORCH_ROOT}/.naturalehia-manifest"
        docker exec "${CONTAINER_NAME}" cat "${NVIDIA_RUNTIME_ROOT}/.naturalehia-manifest"
    fi
}

ensure_container_idle_for_recreate() {
    local process_count
    container_running || return
    process_count="$(docker top "${CONTAINER_NAME}" -eo pid,comm |
        awk 'NR > 1 && NF { count++ } END { print count + 0 }')"
    ((process_count <= 2)) ||
        die "container has active development processes; exit them before recreating"
}

valid_runtime_selector_target() {
    local target="${1:-}"
    ((${#target} <= 255)) &&
        [[ "${target}" =~ ^nvidia-[A-Za-z0-9][A-Za-z0-9._-]*$ ]]
}

normalize_runtime_selector() {
    local current_target="${NVIDIA_RUNTIME_VERSION_ROOT##*/}"
    local expected_manifest_sha256
    valid_runtime_selector_target "${current_target}" || return 1
    expected_manifest_sha256="$({
        printf "cudnn=%s:%s\n" "${CUDNN_VERSION}" "${CUDNN_WHEEL_SHA256}"
        printf "cusparselt=%s:%s\n" \
            "${CUSPARSELT_VERSION}" "${CUSPARSELT_WHEEL_SHA256}"
        printf "nccl=%s:%s\n" "${NCCL_VERSION}" "${NCCL_WHEEL_SHA256}"
        printf "nvshmem=%s:%s\n" "${NVSHMEM_VERSION}" "${NVSHMEM_WHEEL_SHA256}"
    } | sha256sum | awk '{print $1}')"
    [[ "${expected_manifest_sha256}" =~ ^[0-9a-f]{64}$ ]] || return 1

    docker run --rm --user 0:0 --network none --read-only \
        --security-opt no-new-privileges=true \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT}" \
        --env "SELECTOR_BASENAME=${NVIDIA_RUNTIME_SELECTOR_BASENAME}" \
        --env "CURRENT_TARGET=${current_target}" \
        --env "EXPECTED_MANIFEST_SHA256=${expected_manifest_sha256}" \
        "${IMAGE}" bash -Eeuo pipefail -c '
            root=/opt/naturalehia-gpu
            [[ "${SELECTOR_BASENAME}" == "nvidia" ]]
            selector="${root}/${SELECTOR_BASENAME}"
            valid_target() {
                local target="${1:-}"
                ((${#target} <= 255)) &&
                    [[ "${target}" =~ ^nvidia-[A-Za-z0-9][A-Za-z0-9._-]*$ ]]
            }
            valid_target "${CURRENT_TARGET}"
            [[ "${EXPECTED_MANIFEST_SHA256}" =~ ^[0-9a-f]{64}$ ]]

            if [[ -L "${selector}" ]]; then
                target="$(readlink "${selector}")"
                valid_target "${target}"
                target_path="${root}/${target}"
                [[ -d "${target_path}" && ! -L "${target_path}" ]]
                [[ -f "${target_path}/.naturalehia-manifest" &&
                    ! -L "${target_path}/.naturalehia-manifest" ]]
                printf "%s\n" "${target}"
                exit 0
            fi

            [[ -d "${selector}" ]] || {
                echo "NVIDIA runtime selector is neither a directory nor a symlink" >&2
                exit 1
            }
            manifest="${selector}/.naturalehia-manifest"
            [[ -f "${manifest}" && ! -L "${manifest}" ]] || {
                echo "unversioned NVIDIA runtime has no regular manifest" >&2
                exit 1
            }
            chown -R 0:0 "${selector}"
            chmod -R a+rX,a-w "${selector}"
            manifest_hash="$(sha256sum -- "${manifest}")"
            manifest_hash="${manifest_hash%% *}"
            [[ "${manifest_hash}" =~ ^[0-9a-f]{64}$ ]]
            if [[ "${manifest_hash}" == "${EXPECTED_MANIFEST_SHA256}" ]]; then
                target="${CURRENT_TARGET}"
            else
                target="nvidia-legacy-${manifest_hash}"
            fi
            valid_target "${target}"
            target_path="${root}/${target}"
            [[ ! -e "${target_path}" && ! -L "${target_path}" ]] || {
                echo "manifest-derived NVIDIA runtime path already exists: ${target_path}" >&2
                exit 1
            }

            staging="${root}/.${SELECTOR_BASENAME}-normalize.$$"
            [[ ! -e "${staging}" && ! -L "${staging}" ]]
            ln -s -- "${target}" "${staging}"
            if ! mv -- "${selector}" "${target_path}"; then
                rm -f -- "${staging}"
                exit 1
            fi
            if ! mv -Tf -- "${staging}" "${selector}"; then
                rm -f -- "${staging}"
                if ! mv -- "${target_path}" "${selector}"; then
                    echo "failed to restore the unversioned NVIDIA runtime after normalization failed" >&2
                fi
                exit 1
            fi
            [[ "$(readlink "${selector}")" == "${target}" ]]
            printf "%s\n" "${target}"
        '
}

validate_runtime_selector_target() {
    local selector_target="$1"
    valid_runtime_selector_target "${selector_target}" || return 1
    docker run --rm --user 0:0 --network none --read-only \
        --security-opt no-new-privileges=true \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT},readonly" \
        --env "SELECTOR_TARGET=${selector_target}" \
        "${IMAGE}" bash -Eeuo pipefail -c '
            target="${SELECTOR_TARGET}"
            ((${#target} <= 255))
            [[ "${target}" =~ ^nvidia-[A-Za-z0-9][A-Za-z0-9._-]*$ ]]
            target_path="/opt/naturalehia-gpu/${target}"
            [[ -d "${target_path}" && ! -L "${target_path}" ]]
            [[ -f "${target_path}/.naturalehia-manifest" &&
                ! -L "${target_path}/.naturalehia-manifest" ]]
        '
}

write_recreate_state() {
    local was_running="$1"
    local selector_target="$2"
    [[ "${was_running}" == "true" || "${was_running}" == "false" ]] || return 1
    valid_runtime_selector_target "${selector_target}" || return 1
    docker run --rm --user 0:0 --network none --read-only \
        --security-opt no-new-privileges=true \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT}" \
        --env "STATE_BASENAME=${RECREATE_STATE_BASENAME}" \
        --env "SELECTOR_BASENAME=${NVIDIA_RUNTIME_SELECTOR_BASENAME}" \
        --env "RECREATE_WAS_RUNNING=${was_running}" \
        --env "RECREATE_SELECTOR_TARGET=${selector_target}" \
        "${IMAGE}" bash -Eeuo pipefail -c '
            root=/opt/naturalehia-gpu
            [[ "${STATE_BASENAME}" == ".recreate-state" ]]
            [[ "${SELECTOR_BASENAME}" == "nvidia" ]]
            [[ "${RECREATE_WAS_RUNNING}" == "true" ||
                "${RECREATE_WAS_RUNNING}" == "false" ]]
            target="${RECREATE_SELECTOR_TARGET}"
            ((${#target} <= 255))
            [[ "${target}" =~ ^nvidia-[A-Za-z0-9][A-Za-z0-9._-]*$ ]]
            target_path="${root}/${target}"
            [[ -d "${target_path}" && ! -L "${target_path}" ]]
            [[ -f "${target_path}/.naturalehia-manifest" &&
                ! -L "${target_path}/.naturalehia-manifest" ]]
            selector="${root}/${SELECTOR_BASENAME}"
            [[ -L "${selector}" ]]
            [[ "$(readlink "${selector}")" == "${target}" ]]

            state_path="${root}/${STATE_BASENAME}"
            umask 077
            temporary="$(mktemp "${root}/.${STATE_BASENAME#.}.tmp.XXXXXX")"
            trap "rm -f -- \"${temporary}\"" EXIT
            printf "%s\n%s\n" "${RECREATE_WAS_RUNNING}" "${target}" \
                >"${temporary}"
            chown 0:0 "${temporary}"
            chmod 0600 "${temporary}"
            mv -fT -- "${temporary}" "${state_path}"
            trap - EXIT
        '
}

read_recreate_state() {
    docker run --rm --user 0:0 --network none --read-only \
        --security-opt no-new-privileges=true \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT},readonly" \
        --env "STATE_BASENAME=${RECREATE_STATE_BASENAME}" \
        "${IMAGE}" bash -Eeuo pipefail -c '
            [[ "${STATE_BASENAME}" == ".recreate-state" ]]
            state_path="/opt/naturalehia-gpu/${STATE_BASENAME}"
            [[ -f "${state_path}" && ! -L "${state_path}" ]]
            [[ "$(stat -c "%u:%g:%a" "${state_path}")" == "0:0:600" ]]
            cat "${state_path}"
        '
}

load_recreate_state() {
    local serialized_state
    local -a state_lines=()
    serialized_state="$(read_recreate_state)" || return 1
    mapfile -t state_lines <<<"${serialized_state}"
    if ((${#state_lines[@]} == 1)); then
        # Bootstrap version 5 recorded only the running state. Upgrade that
        # root-owned marker in place after safely capturing the old selector.
        [[ "${state_lines[0]}" == "true" || "${state_lines[0]}" == "false" ]] ||
            return 1
        RECREATE_WAS_RUNNING="${state_lines[0]}"
        RECREATE_SELECTOR_TARGET="$(normalize_runtime_selector)" || return 1
        valid_runtime_selector_target "${RECREATE_SELECTOR_TARGET}" || return 1
        validate_runtime_selector_target "${RECREATE_SELECTOR_TARGET}" || return 1
        write_recreate_state "${RECREATE_WAS_RUNNING}" \
            "${RECREATE_SELECTOR_TARGET}" || return 1
        return 0
    fi
    ((${#state_lines[@]} == 2)) || return 1
    [[ "${state_lines[0]}" == "true" || "${state_lines[0]}" == "false" ]] ||
        return 1
    valid_runtime_selector_target "${state_lines[1]}" || return 1
    validate_runtime_selector_target "${state_lines[1]}" || return 1
    RECREATE_WAS_RUNNING="${state_lines[0]}"
    RECREATE_SELECTOR_TARGET="${state_lines[1]}"
}

clear_recreate_state() {
    docker run --rm --user 0:0 --network none --read-only \
        --security-opt no-new-privileges=true \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT}" \
        --env "STATE_BASENAME=${RECREATE_STATE_BASENAME}" \
        "${IMAGE}" bash -Eeuo pipefail -c '
            [[ "${STATE_BASENAME}" == ".recreate-state" ]]
            rm -f -- "/opt/naturalehia-gpu/${STATE_BASENAME}"
        '
}

restore_runtime_selector() {
    local selector_target="$1"
    valid_runtime_selector_target "${selector_target}" || return 1
    docker run --rm --user 0:0 --network none --read-only \
        --security-opt no-new-privileges=true \
        --mount "type=volume,source=${GPU_VOLUME},target=${CONTAINER_GPU_ROOT}" \
        --env "SELECTOR_BASENAME=${NVIDIA_RUNTIME_SELECTOR_BASENAME}" \
        --env "SELECTOR_TARGET=${selector_target}" \
        "${IMAGE}" bash -Eeuo pipefail -c '
            root=/opt/naturalehia-gpu
            [[ "${SELECTOR_BASENAME}" == "nvidia" ]]
            target="${SELECTOR_TARGET}"
            ((${#target} <= 255))
            [[ "${target}" =~ ^nvidia-[A-Za-z0-9][A-Za-z0-9._-]*$ ]]
            target_path="${root}/${target}"
            [[ -d "${target_path}" && ! -L "${target_path}" ]]
            [[ -f "${target_path}/.naturalehia-manifest" &&
                ! -L "${target_path}/.naturalehia-manifest" ]]

            selector="${root}/${SELECTOR_BASENAME}"
            [[ ! -e "${selector}" || -L "${selector}" ]]
            staging="${root}/.${SELECTOR_BASENAME}-restore.$$"
            [[ ! -e "${staging}" && ! -L "${staging}" ]]
            trap "rm -f -- \"${staging}\"" EXIT
            ln -s -- "${target}" "${staging}"
            mv -Tf -- "${staging}" "${selector}"
            trap - EXIT
            [[ "$(readlink "${selector}")" == "${target}" ]]
        '
}

rollback_recreate() {
    local exit_code=$?
    local recovery_ok=true
    local recovery_error=""
    local selector_target="${RECREATE_SELECTOR_TARGET:-}"
    trap - ERR
    set +e
    printf '[naturalehia] replacement failed; restoring the previous container\n' >&2

    if ! valid_runtime_selector_target "${selector_target}"; then
        recovery_ok=false
        recovery_error="the saved NVIDIA runtime selector target is invalid"
    elif [[ "${RECREATE_WAS_RUNNING:-}" != "true" &&
        "${RECREATE_WAS_RUNNING:-}" != "false" ]]; then
        recovery_ok=false
        recovery_error="the saved running state is invalid"
    elif ! validate_runtime_selector_target "${selector_target}"; then
        recovery_ok=false
        recovery_error="the saved NVIDIA runtime selector target is unavailable"
    fi
    if [[ "${recovery_ok}" == "true" &&
        "${RECREATE_RENAMED:-false}" == "true" ]] &&
        { [[ ! "${RECREATE_OLD_UID:-}" =~ ^[0-9]+$ ]] ||
            [[ ! "${RECREATE_OLD_GID:-}" =~ ^[0-9]+$ ]]; }; then
        recovery_ok=false
        recovery_error="the previous container user identity is invalid"
    fi

    if [[ "${RECREATE_RENAMED:-false}" == "true" ]]; then
        if [[ "${recovery_ok}" == "true" ]] && container_exists; then
            if ! managed_container; then
                recovery_ok=false
                recovery_error="the replacement container is not managed by setup.sh"
            elif ! docker rm --force "${CONTAINER_NAME}" >/dev/null; then
                recovery_ok=false
                recovery_error="the failed replacement container could not be removed"
            fi
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! docker container inspect "${RECREATE_BACKUP_NAME}" >/dev/null 2>&1; then
            recovery_ok=false
            recovery_error="the previous container backup is unavailable"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! managed_container "${RECREATE_BACKUP_NAME}"; then
            recovery_ok=false
            recovery_error="the previous container backup is not managed by setup.sh"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! restore_runtime_selector "${selector_target}"; then
            recovery_ok=false
            recovery_error="the saved NVIDIA runtime selector could not be restored"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! docker rename "${RECREATE_BACKUP_NAME}" "${CONTAINER_NAME}" >/dev/null; then
            recovery_ok=false
            recovery_error="the previous container could not be renamed into place"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! docker start "${CONTAINER_NAME}" >/dev/null; then
            recovery_ok=false
            recovery_error="the previous container could not be started"
        fi
        if [[ "${recovery_ok}" == "true" ]]; then
            if ! docker exec --user 0:0 \
                --env "OLD_UID=${RECREATE_OLD_UID}" \
                --env "OLD_GID=${RECREATE_OLD_GID}" \
                --env "CONTAINER_HOME=${CONTAINER_HOME}" \
                --env "CONTAINER_BUILD_ROOT=${CONTAINER_BUILD_ROOT}" \
                "${CONTAINER_NAME}" bash -Eeuo pipefail -c '
                    for path in "${CONTAINER_HOME}" "${CONTAINER_BUILD_ROOT}"; do
                        chown -R "${OLD_UID}:${OLD_GID}" "${path}"
                        printf "%s:%s\n" "${OLD_UID}" "${OLD_GID}" \
                            >"${path}/.naturalehia-owner"
                        chown "${OLD_UID}:${OLD_GID}" "${path}/.naturalehia-owner"
                    done
                '; then
                recovery_ok=false
                recovery_error="managed volume ownership could not be restored"
            fi
            if [[ "${RECREATE_WAS_RUNNING}" != "true" ]] &&
                ! docker stop "${CONTAINER_NAME}" >/dev/null; then
                recovery_ok=false
                [[ -n "${recovery_error}" ]] ||
                    recovery_error="the restored container could not be stopped"
            fi
        fi
    else
        if [[ "${recovery_ok}" == "true" ]] && ! container_exists; then
            recovery_ok=false
            recovery_error="the previous container is unavailable"
        fi
        if [[ "${recovery_ok}" == "true" ]] && ! managed_container; then
            recovery_ok=false
            recovery_error="the previous container is not managed by setup.sh"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! restore_runtime_selector "${selector_target}"; then
            recovery_ok=false
            recovery_error="the saved NVIDIA runtime selector could not be restored"
        fi
        if [[ "${recovery_ok}" == "true" &&
            "${RECREATE_WAS_RUNNING}" == "true" ]] && ! container_running; then
            if ! docker start "${CONTAINER_NAME}" >/dev/null; then
                recovery_ok=false
                recovery_error="the previous container could not be restarted"
            fi
        elif [[ "${recovery_ok}" == "true" &&
            "${RECREATE_WAS_RUNNING}" == "false" ]] && container_running; then
            if ! docker stop "${CONTAINER_NAME}" >/dev/null; then
                recovery_ok=false
                recovery_error="the previous container could not be returned to its stopped state"
            fi
        fi
    fi

    if [[ "${recovery_ok}" == "true" ]]; then
        if ! clear_recreate_state >/dev/null 2>&1; then
            printf '[naturalehia] warning: recovery succeeded, but root-only recovery metadata could not be removed\n' >&2
        fi
        printf '[naturalehia] previous container restored\n' >&2
    else
        printf '[naturalehia] automatic recovery failed: %s\n' "${recovery_error}" >&2
        printf '[naturalehia] recovery metadata remains at %s in volume %s\n' \
            "${RECREATE_STATE_FILE}" "${GPU_VOLUME}" >&2
        printf '[naturalehia] manual recovery: restore %s as a symlink to basename %s, recover container %s or %s, and restore running=%s\n' \
            "${NVIDIA_RUNTIME_ROOT}" "${selector_target:-<invalid>}" \
            "${CONTAINER_NAME}" "${RECREATE_BACKUP_NAME}" \
            "${RECREATE_WAS_RUNNING:-<invalid>}" >&2
    fi
    exit "${exit_code}"
}

main() {
    local command="${1:-up}"
    if (($# != 0)); then
        shift
    fi
    case "${command}" in
        help | --help | -h)
            usage
            return
            ;;
    esac

    require_command id
    require_command uname
    resolve_workspace
    detect_identity
    validate_number "HTTP port" "${HTTP_PORT}" 65535
    validate_number "ingestion port" "${INGEST_PORT}" 65535
    HTTP_PORT="$((10#${HTTP_PORT}))"
    INGEST_PORT="$((10#${INGEST_PORT}))"
    [[ "${HTTP_PORT}" != "${INGEST_PORT}" ]] || die "published ports must be distinct"
    acquire_lock

    case "${command}" in
        up)
            ensure_ready
            log "ready: HTTP ${LOOPBACK_ADDRESS}:${HTTP_PORT}, ingestion ${LOOPBACK_ADDRESS}:${INGEST_PORT}"
            log "enter with: bash projects/the-elder-brother-of-fauna/setup.sh shell"
            ;;
        shell)
            ensure_ready
            docker exec --interactive --tty \
                --user "${DEV_UID}:${DEV_GID}" \
                --env "HOME=${CONTAINER_HOME}" \
                --env "USER=developer" \
                --env "LOGNAME=developer" \
                --env "CCACHE_DIR=${CONTAINER_HOME}/.cache/ccache" \
                --env "CUDA_CACHE_PATH=${CONTAINER_HOME}/.cache/nv" \
                --env "CUDA_HOME=${CUDA_ROOT}" \
                --env "CUDAToolkit_ROOT=${CUDA_ROOT}" \
                --env "CUDACXX=${CUDA_ROOT}/bin/nvcc" \
                --env "LIBTORCH_ROOT=${LIBTORCH_ROOT}" \
                --env "NVIDIA_RUNTIME_ROOT=${NVIDIA_RUNTIME_ROOT}" \
                --env "CUDNN_ROOT=${NVIDIA_RUNTIME_ROOT}/cudnn" \
                --env "CUSPARSELT_ROOT=${NVIDIA_RUNTIME_ROOT}/cusparselt" \
                --env "NCCL_ROOT=${NVIDIA_RUNTIME_ROOT}/nccl" \
                --env "NVSHMEM_ROOT=${NVIDIA_RUNTIME_ROOT}/nvshmem" \
                --env "CMAKE_PREFIX_PATH=${LIBTORCH_ROOT}" \
                --env "PATH=${CONTAINER_PATH}" \
                --workdir "${CONTAINER_PROJECT_DIR}" \
                "${CONTAINER_NAME}" bash --login
            ;;
        exec)
            (($# != 0)) || die "exec requires a command"
            ensure_ready
            as_developer "$@"
            ;;
        build)
            ensure_ready
            build_project
            ;;
        test)
            ensure_ready
            test_project
            ;;
        gpu-test)
            ensure_ready
            test_gpu_stack
            ;;
        run)
            ensure_ready
            build_project
            as_developer "${CONTAINER_BUILD_DIR}/naturalehia-fauna" "$@"
            ;;
        status)
            show_status
            ;;
        stop)
            ensure_docker
            if container_exists; then
                managed_container || die "refusing to stop an unmanaged container"
                docker stop "${CONTAINER_NAME}" >/dev/null
                log "stopped '${CONTAINER_NAME}'; named volumes were preserved"
            else
                log "container '${CONTAINER_NAME}' does not exist"
            fi
            ;;
        recreate)
            ensure_docker
            ensure_image
            calculate_fingerprint
            prepare_container_resources
            if docker container inspect "${RECREATE_BACKUP_NAME}" >/dev/null 2>&1; then
                container_exists ||
                    die "only '${RECREATE_BACKUP_NAME}' exists; rename it to '${CONTAINER_NAME}' before retrying"
                managed_container "${RECREATE_BACKUP_NAME}" ||
                    die "refusing to use an unmanaged replacement backup"
                verify_existing_configuration

                load_recreate_state ||
                    die "interrupted replacement has missing or invalid running-state/selector metadata"
                local interrupted_old_user
                interrupted_old_user="$(docker container inspect --format '{{.Config.User}}' \
                    "${RECREATE_BACKUP_NAME}")"
                [[ "${interrupted_old_user}" =~ ^([0-9]+):([0-9]+)$ ]] ||
                    die "replacement backup has an unexpected user specification"
                RECREATE_OLD_UID="${BASH_REMATCH[1]}"
                RECREATE_OLD_GID="${BASH_REMATCH[2]}"
                RECREATE_RENAMED=true
                trap rollback_recreate ERR

                log "resuming interrupted transactional replacement"
                container_running || docker start "${CONTAINER_NAME}" >/dev/null
                provision_container
                verify_container
                test_gpu_stack
                docker rm "${RECREATE_BACKUP_NAME}" >/dev/null
                RECREATE_RENAMED=false
                trap - ERR
                if ! clear_recreate_state; then
                    log "warning: replacement committed, but stale recreate state could not be removed"
                fi
                log "recreated '${CONTAINER_NAME}'"
                return
            fi
            if container_exists; then
                managed_container || die "refusing to replace an unmanaged container"
                ensure_container_idle_for_recreate

                RECREATE_WAS_RUNNING="$(docker container inspect \
                    --format '{{.State.Running}}' "${CONTAINER_NAME}")"
                local old_user
                old_user="$(docker container inspect --format '{{.Config.User}}' "${CONTAINER_NAME}")"
                [[ "${old_user}" =~ ^([0-9]+):([0-9]+)$ ]] ||
                    die "managed container has an unexpected user specification"
                RECREATE_OLD_UID="${BASH_REMATCH[1]}"
                RECREATE_OLD_GID="${BASH_REMATCH[2]}"
                RECREATE_SELECTOR_TARGET="$(normalize_runtime_selector)" ||
                    die "could not normalize and capture the current NVIDIA runtime selector"
                if ! valid_runtime_selector_target "${RECREATE_SELECTOR_TARGET}" ||
                    ! validate_runtime_selector_target "${RECREATE_SELECTOR_TARGET}"; then
                    die "normalized NVIDIA runtime selector is invalid"
                fi
                write_recreate_state "${RECREATE_WAS_RUNNING}" \
                    "${RECREATE_SELECTOR_TARGET}" ||
                    die "could not persist transactional replacement metadata"
                RECREATE_RENAMED=false
                trap rollback_recreate ERR

                if [[ "${RECREATE_WAS_RUNNING}" == "true" ]]; then
                    log "stopping the idle managed container for transactional replacement"
                    docker stop "${CONTAINER_NAME}" >/dev/null
                fi
                docker rename "${CONTAINER_NAME}" "${RECREATE_BACKUP_NAME}" >/dev/null
                RECREATE_RENAMED=true
            fi
            create_container
            provision_container
            verify_container
            test_gpu_stack
            if [[ "${RECREATE_RENAMED:-false}" == "true" ]]; then
                docker rm "${RECREATE_BACKUP_NAME}" >/dev/null
                RECREATE_RENAMED=false
                trap - ERR
                if ! clear_recreate_state; then
                    log "warning: replacement committed, but stale recreate state could not be removed"
                fi
            fi
            log "recreated '${CONTAINER_NAME}'"
            ;;
        *)
            usage >&2
            die "unknown command: ${command}"
            ;;
    esac
}

main "$@"
