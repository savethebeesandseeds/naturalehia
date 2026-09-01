#!/usr/bin/env bash

# Git Bash can be launched from Windows with a PATH that omits its Unix tools.
case "${OSTYPE:-}" in
    msys* | cygwin*)
        PATH="/usr/local/bin:/usr/bin:/bin${PATH:+:${PATH}}"
        export PATH
        ;;
esac

set -Eeuo pipefail
IFS=$'\n\t'

HOST_SCRIPT_DIRECTORY="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HOST_SCRIPT_DIRECTORY
# shellcheck source=toolchain-locks.sh
source "${HOST_SCRIPT_DIRECTORY}/toolchain-locks.sh"

readonly CONTAINER_NAME="naturalehia-the-elder-brother-of-fauna"
readonly CONTAINER_HOSTNAME="naturalehia-the-elder-brother-of-fauna"
readonly LEGACY_CONTAINER_NAME="naturalehia"
readonly LEGACY_RECREATE_BACKUP_NAME="naturalehia-recreate-backup"
readonly BUILD_VOLUME="naturalehia-build"
readonly HOME_VOLUME="naturalehia-home"
readonly GPU_VOLUME="naturalehia-gpu"
readonly MANAGED_LABEL="io.naturalehia.devcontainer"
readonly CONFIG_LABEL="io.naturalehia.config"
readonly WORKSPACE_LABEL="io.naturalehia.workspace"
readonly RECREATE_BACKUP_NAME="${CONTAINER_NAME}-recreate-backup"
readonly LOOPBACK_ADDRESS="127.0.0.1"
readonly HTTP_CONTAINER_PORT="8080"
readonly INGEST_CONTAINER_PORT="50051"
readonly PIDS_LIMIT="2048"
readonly SHM_SIZE="1g"
readonly SHM_BYTES="1073741824"
readonly LOG_MAX_SIZE="10m"
readonly LOG_MAX_FILES="3"
readonly RECREATE_STATE_BASENAME=".recreate-state"
readonly RECREATE_STATE_FILE="${CONTAINER_GPU_ROOT}/${RECREATE_STATE_BASENAME}"

IMAGE="${NATURALEHIA_IMAGE:-${DEFAULT_IMAGE}}"
HTTP_PORT="${NATURALEHIA_HTTP_PORT:-8080}"
INGEST_PORT="${NATURALEHIA_INGEST_PORT:-50051}"
CREATED_CONTAINER_ID=""
RECREATE_ORIGINAL_ID=""
RECREATE_CANDIDATE_ID=""

log() {
    printf '[naturalehia] %s\n' "$*"
}

die() {
    printf '[naturalehia] error: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: bash projects/the-elder-brother-of-fauna/container.sh [command] [arguments]

Commands:
  up              Create, start, provision, and verify the container (default)
  shell           Open an interactive shell as the non-root development user
  exec CMD...     Run a command as the non-root development user
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

Project tasks are separate from lifecycle management. For example:
  bash projects/the-elder-brother-of-fauna/container.sh exec make test
  bash projects/the-elder-brother-of-fauna/container.sh exec make gpu-test
EOF
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

validate_number() {
    local label="$1"
    local value="$2"
    local maximum="$3"
    [[ "${value}" =~ ^[1-9][0-9]*$ ]] ||
        die "${label} must be a canonical positive integer"
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
        die "container.sh must remain in the project root"
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
    LOCK_DIRECTORY="${lock_root%/}/${CONTAINER_NAME}.lock"
    if ! mkdir "${LOCK_DIRECTORY}" 2>/dev/null; then
        owner_pid=""
        if [[ -r "${LOCK_DIRECTORY}/owner" ]]; then
            IFS= read -r owner_pid <"${LOCK_DIRECTORY}/owner" || true
        fi
        if [[ "${owner_pid}" =~ ^[0-9]+$ ]] && kill -0 "${owner_pid}" 2>/dev/null; then
            die "another container.sh process (PID ${owner_pid}) is managing the container"
        fi

        stale_directory="${LOCK_DIRECTORY}.stale.$$"
        mv -- "${LOCK_DIRECTORY}" "${stale_directory}" 2>/dev/null ||
            die "container lock changed while checking it; retry the command"
        rm -f -- "${stale_directory}/owner"
        rmdir -- "${stale_directory}" || die "could not remove stale container lock"
        mkdir "${LOCK_DIRECTORY}" || die "could not acquire container lock"
    fi
    printf '%s\n' "$$" >"${LOCK_DIRECTORY}/owner"
    trap release_lock EXIT
}

release_lock() {
    rm -f -- "${LOCK_DIRECTORY}/owner"
    rmdir -- "${LOCK_DIRECTORY}" 2>/dev/null || true
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

container_id() {
    docker container inspect --format '{{.Id}}' "$1"
}

valid_container_id() {
    [[ "${1:-}" =~ ^[0-9a-f]{64}$ ]]
}

named_container_has_id() {
    local container_name="$1"
    local expected_id="$2"
    local actual_id
    actual_id="$(container_id "${container_name}")" || return 1
    [[ "${actual_id}" == "${expected_id}" ]]
}

assert_named_container_id() {
    local container_name="$1"
    local expected_id="$2"
    valid_container_id "${expected_id}" ||
        die "container '${container_name}' has an invalid captured immutable ID"
    named_container_has_id "${container_name}" "${expected_id}" ||
        die "container '${container_name}' changed identity; preserving all containers"
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
        die "a container named '${CONTAINER_NAME}' exists but is not managed by container.sh"
    recreate_container_has_expected_mounts "${CONTAINER_NAME}" ||
        die "managed container has unexpected ownership or data mounts"

    local existing_fingerprint
    existing_fingerprint="$(docker container inspect \
        --format "{{ index .Config.Labels \"${CONFIG_LABEL}\" }}" "${CONTAINER_NAME}")"
    [[ "${existing_fingerprint}" == "${CONFIG_FINGERPRINT}" ]] ||
        die "container settings changed; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate' to apply them"

    [[ "$(docker container inspect --format '{{.Image}}' "${CONTAINER_NAME}")" == "${IMAGE_ID}" ]] ||
        die "managed container image differs from the requested image; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.Config.User}}' "${CONTAINER_NAME}")" == "${DEV_UID}:${DEV_GID}" ]] ||
        die "managed container user was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.Config.WorkingDir}}' "${CONTAINER_NAME}")" == "${CONTAINER_WORKSPACE}" ]] ||
        die "managed container work directory was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.RestartPolicy.Name}}' "${CONTAINER_NAME}")" == "no" ]] ||
        die "managed container restart policy was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.PidsLimit}}' "${CONTAINER_NAME}")" == "${PIDS_LIMIT}" ]] ||
        die "managed container PID limit was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.ShmSize}}' "${CONTAINER_NAME}")" == "${SHM_BYTES}" ]] ||
        die "managed container shared-memory limit was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.Init}}' "${CONTAINER_NAME}")" == "true" ]] ||
        die "managed container init setting was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{json .HostConfig.SecurityOpt}}' "${CONTAINER_NAME}")" == '["no-new-privileges=true"]' ]] ||
        die "managed container security options were modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
    [[ "$(docker container inspect --format '{{.HostConfig.LogConfig.Type}}' "${CONTAINER_NAME}")" == "local" ]] ||
        die "managed container log driver was modified; run 'bash projects/the-elder-brother-of-fauna/container.sh recreate'"
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
        die "volume '${volume_name}' exists but is not managed by container.sh"
    driver="$(docker volume inspect --format '{{.Driver}}' "${volume_name}")"
    [[ "${driver}" == "local" ]] ||
        die "managed volume '${volume_name}' must use Docker's local driver"
}

prepare_container_resources() {
    local attachment_mode="${1:-normal}"
    refuse_legacy_containers
    assert_volume_attachment_exclusivity "${attachment_mode}"
    debian_preflight
    gpu_preflight
    ensure_managed_volume "${BUILD_VOLUME}"
    ensure_managed_volume "${HOME_VOLUME}"
    ensure_managed_volume "${GPU_VOLUME}"
}

refuse_legacy_containers() {
    local legacy_name
    for legacy_name in "${LEGACY_CONTAINER_NAME}" "${LEGACY_RECREATE_BACKUP_NAME}"; do
        if docker container inspect "${legacy_name}" >/dev/null 2>&1; then
            die "legacy container '${legacy_name}' exists; inspect and resolve it explicitly before creating '${CONTAINER_NAME}'"
        fi
    done
}

normalize_mount_source() {
    local source_path="${1//\\//}"
    case "${source_path}" in
        /run/desktop/mnt/host/[A-Za-z]/*)
            source_path="${source_path#/run/desktop/mnt/host/}"
            source_path="${source_path:0:1}:${source_path:1}"
            ;;
        /host_mnt/[A-Za-z]/*)
            source_path="${source_path#/host_mnt/}"
            source_path="${source_path:0:1}:${source_path:1}"
            ;;
        /mnt/[A-Za-z]/*)
            source_path="${source_path#/mnt/}"
            source_path="${source_path:0:1}:${source_path:1}"
            ;;
        /[A-Za-z]/*)
            source_path="${source_path#/}"
            source_path="${source_path:0:1}:${source_path:1}"
            ;;
    esac
    if [[ "${source_path}" =~ ^[A-Za-z]:/ ]]; then
        source_path="${source_path,,}"
    fi
    printf '%s\n' "${source_path%/}"
}

recreate_container_has_expected_mounts() {
    local container_name="$1"
    docker container inspect "${container_name}" >/dev/null 2>&1 || return 1
    managed_container "${container_name}" || return 1

    local mounts
    local mount_type
    local mount_source
    local mount_name
    local mount_destination
    local mount_read_write
    local mount_propagation
    local expected_workspace
    local workspace_label
    local mount_count=0
    local workspace_count=0
    local build_count=0
    local home_count=0
    local gpu_count=0
    expected_workspace="$(normalize_mount_source "${HOST_WORKSPACE}")"
    workspace_label="$(docker container inspect --format \
        "{{index .Config.Labels \"${WORKSPACE_LABEL}\"}}" \
        "${container_name}")" || return 1
    [[ "$(normalize_mount_source "${workspace_label}")" == \
        "${expected_workspace}" ]] || return 1
    mounts="$(docker container inspect --format \
        '{{range .Mounts}}{{printf "%s|%s|%s|%s|%t|%s\n" .Type .Source .Name .Destination .RW .Propagation}}{{end}}' \
        "${container_name}")" || return 1
    while IFS='|' read -r mount_type mount_source mount_name mount_destination \
        mount_read_write mount_propagation; do
        [[ -n "${mount_type}" ]] || continue
        ((mount_count += 1))
        if [[ "${mount_type}" == "bind" && \
            "$(normalize_mount_source "${mount_source}")" == \
                "${expected_workspace}" && \
            -z "${mount_name}" && \
            "${mount_destination}" == "${CONTAINER_WORKSPACE}" && \
            "${mount_read_write}" == "true" && \
            "${mount_propagation}" == "rprivate" ]]; then
            ((workspace_count += 1))
        elif [[ "${mount_type}" == "volume" && \
            "${mount_name}" == "${BUILD_VOLUME}" && \
            "${mount_destination}" == "${CONTAINER_BUILD_ROOT}" && \
            "${mount_read_write}" == "true" ]]; then
            ((build_count += 1))
        elif [[ "${mount_type}" == "volume" && \
            "${mount_name}" == "${HOME_VOLUME}" && \
            "${mount_destination}" == "${CONTAINER_HOME}" && \
            "${mount_read_write}" == "true" ]]; then
            ((home_count += 1))
        elif [[ "${mount_type}" == "volume" && \
            "${mount_name}" == "${GPU_VOLUME}" && \
            "${mount_destination}" == "${CONTAINER_GPU_ROOT}" && \
            "${mount_read_write}" == "true" ]]; then
            ((gpu_count += 1))
        else
            return 1
        fi
    done <<<"${mounts}"
    ((mount_count == 4 && workspace_count == 1 && build_count == 1 &&
        home_count == 1 && gpu_count == 1))
}

verified_recreate_backup() {
    local expected_id="${1:-}"
    local container_reference="${RECREATE_BACKUP_NAME}"
    if [[ -n "${expected_id}" ]]; then
        valid_container_id "${expected_id}" || return 1
        named_container_has_id "${RECREATE_BACKUP_NAME}" "${expected_id}" || return 1
        container_reference="${expected_id}"
    fi
    recreate_container_has_expected_mounts "${container_reference}" || return 1
    [[ "$(docker container inspect --format '{{.State.Running}}' \
        "${container_reference}")" == "false" ]]
}

assert_volume_attachment_exclusivity() {
    local attachment_mode="${1:-normal}"
    [[ "${attachment_mode}" == "normal" || "${attachment_mode}" == "recreate" ]] ||
        die "internal error: invalid volume attachment mode '${attachment_mode}'"

    if [[ "${attachment_mode}" == "recreate" ]] &&
        docker container inspect "${RECREATE_BACKUP_NAME}" >/dev/null 2>&1 &&
        ! verified_recreate_backup; then
        die "recreate backup '${RECREATE_BACKUP_NAME}' is not a stopped managed container with the three expected volume mounts"
    fi

    local volume_name
    local attachments
    local attachment_id
    local attachment_name
    for volume_name in "${BUILD_VOLUME}" "${HOME_VOLUME}" "${GPU_VOLUME}"; do
        attachments="$(docker container ls --all --no-trunc \
            --filter "volume=${volume_name}" --format '{{.ID}}\t{{.Names}}')" ||
            die "could not inspect attachments for volume '${volume_name}'"
        while IFS=$'\t' read -r attachment_id attachment_name; do
            [[ -n "${attachment_id}" ]] || continue
            if [[ "${attachment_name}" == "${CONTAINER_NAME}" ]]; then
                continue
            fi
            if [[ "${attachment_mode}" == "recreate" && \
                "${attachment_name}" == "${RECREATE_BACKUP_NAME}" ]] &&
                verified_recreate_backup; then
                continue
            fi
            die "retained volume '${volume_name}' is attached to container '${attachment_name}' (${attachment_id}); only '${CONTAINER_NAME}' may use it"
        done <<<"${attachments}"
    done
}

report_container_conflicts() {
    local legacy_name
    local state
    local volume_name
    local attachments
    local attachment_id
    local attachment_name
    for legacy_name in "${LEGACY_CONTAINER_NAME}" "${LEGACY_RECREATE_BACKUP_NAME}"; do
        if docker container inspect "${legacy_name}" >/dev/null 2>&1; then
            state="$(docker container inspect --format '{{.State.Status}}' "${legacy_name}")"
            log "conflict: legacy container '${legacy_name}' exists (status=${state})"
        fi
    done
    for volume_name in "${BUILD_VOLUME}" "${HOME_VOLUME}" "${GPU_VOLUME}"; do
        attachments="$(docker container ls --all --no-trunc \
            --filter "volume=${volume_name}" --format '{{.ID}}\t{{.Names}}')" || {
            log "warning: could not inspect attachments for volume '${volume_name}'"
            continue
        }
        while IFS=$'\t' read -r attachment_id attachment_name; do
            [[ -n "${attachment_id}" ]] || continue
            if [[ "${attachment_name}" != "${CONTAINER_NAME}" ]]; then
                log "conflict: retained volume '${volume_name}' is attached to '${attachment_name}' (${attachment_id})"
            fi
        done <<<"${attachments}"
    done
}

create_container() {
    local attachment_mode="${1:-normal}"
    refuse_legacy_containers
    assert_volume_attachment_exclusivity "${attachment_mode}"
    log "creating Debian container '${CONTAINER_NAME}'"
    CREATED_CONTAINER_ID=""
    CREATED_CONTAINER_ID="$(docker run --detach \
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
        --label "${WORKSPACE_LABEL}=${HOST_WORKSPACE}" \
        --label "io.naturalehia.image=${IMAGE}" \
        --label "io.naturalehia.gpu-stack=${GPU_STACK_ID}" \
        --log-driver local \
        --log-opt "max-size=${LOG_MAX_SIZE}" \
        --log-opt "max-file=${LOG_MAX_FILES}" \
        "${IMAGE}" sleep infinity)"
    if [[ "${RECREATE_RENAMED:-false}" == "true" ]]; then
        RECREATE_CANDIDATE_ID="${CREATED_CONTAINER_ID}"
    fi
    valid_container_id "${CREATED_CONTAINER_ID}" ||
        die "Docker returned an invalid immutable ID for '${CONTAINER_NAME}'"
    assert_named_container_id "${CONTAINER_NAME}" "${CREATED_CONTAINER_ID}"
}

start_container() {
    if ! container_exists; then
        prepare_container_resources normal
        create_container normal
    else
        assert_volume_attachment_exclusivity normal
        verify_existing_configuration
        if ! container_running; then
            log "starting '${CONTAINER_NAME}'"
            docker start "${CONTAINER_NAME}" >/dev/null
        fi
    fi
}

provision_container() {
    log "running the in-container provisioner"
    docker exec --user 0:0 \
        --env "DEV_UID=${DEV_UID}" \
        --env "DEV_GID=${DEV_GID}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        "${CONTAINER_NAME}" bash ./setup.sh
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
        --env "BOOTSTRAP_VERSION=${BOOTSTRAP_VERSION}" \
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
                cmp -s - /var/lib/naturalehia-fauna/package-manifest
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
            test "$(cat /var/lib/naturalehia-fauna/bootstrap-version)" = "${BOOTSTRAP_VERSION}"
            command -v make >/dev/null
            shellcheck container.sh setup.sh toolchain-locks.sh
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

container_exec() {
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
        --env "NATURALEHIA_FAUNA_CONTAINER=1" \
        --env "NATURALEHIA_FAUNA_CONFIG_FINGERPRINT=${CONFIG_FINGERPRINT}" \
        --env "NATURALEHIA_FAUNA_BUILD_ROOT=${CONTAINER_PROJECT_BUILD_ROOT}/${CONFIG_FINGERPRINT}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        "${CONTAINER_NAME}" "$@"
}

show_status() {
    ensure_docker
    report_container_conflicts
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
    trap - ERR EXIT
    set +e
    printf '[naturalehia] replacement failed; restoring the previous container\n' >&2

    if [[ "${RECREATE_RENAMED:-false}" != "true" ]] &&
        valid_container_id "${RECREATE_ORIGINAL_ID:-}" &&
        ! container_exists &&
        docker container inspect "${RECREATE_BACKUP_NAME}" >/dev/null 2>&1 &&
        named_container_has_id "${RECREATE_BACKUP_NAME}" \
            "${RECREATE_ORIGINAL_ID}"; then
        RECREATE_RENAMED=true
    fi

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
        ! "${RECREATE_ORIGINAL_ID:-}" =~ ^[0-9a-f]{64}$ ]]; then
        recovery_ok=false
        recovery_error="the previous container immutable ID is invalid"
    elif [[ "${recovery_ok}" == "true" &&
        "${RECREATE_RENAMED:-false}" == "true" ]] && {
        [[ ! "${RECREATE_OLD_UID:-}" =~ ^[0-9]+$ ]] ||
            [[ ! "${RECREATE_OLD_GID:-}" =~ ^[0-9]+$ ]]; }; then
        recovery_ok=false
        recovery_error="the previous container user identity is invalid"
    fi

    if [[ "${RECREATE_RENAMED:-false}" == "true" ]]; then
        if [[ "${recovery_ok}" == "true" ]] &&
            ! docker container inspect "${RECREATE_BACKUP_NAME}" >/dev/null 2>&1; then
            recovery_ok=false
            recovery_error="the previous container backup is unavailable"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! verified_recreate_backup "${RECREATE_ORIGINAL_ID}"; then
            recovery_ok=false
            recovery_error="the previous container backup changed identity or has unexpected ownership, state, or data mounts"
        fi
        if [[ "${recovery_ok}" == "true" ]] && container_exists; then
            if ! valid_container_id "${RECREATE_CANDIDATE_ID:-}"; then
                recovery_ok=false
                recovery_error="the replacement container immutable ID is unavailable"
            elif ! named_container_has_id "${CONTAINER_NAME}" \
                "${RECREATE_CANDIDATE_ID}"; then
                recovery_ok=false
                recovery_error="the replacement container changed identity; preserving all containers"
            elif ! recreate_container_has_expected_mounts \
                "${RECREATE_CANDIDATE_ID}"; then
                recovery_ok=false
                recovery_error="the replacement container has unexpected ownership or data mounts"
            fi
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! restore_runtime_selector "${selector_target}"; then
            recovery_ok=false
            recovery_error="the saved NVIDIA runtime selector could not be restored"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! verified_recreate_backup "${RECREATE_ORIGINAL_ID}"; then
            recovery_ok=false
            recovery_error="the previous container backup changed before it could be restored"
        fi
        if [[ "${recovery_ok}" == "true" ]] && container_exists; then
            if ! named_container_has_id "${CONTAINER_NAME}" \
                "${RECREATE_CANDIDATE_ID}"; then
                recovery_ok=false
                recovery_error="the replacement container changed identity; preserving all containers"
            elif ! docker rm --force "${RECREATE_CANDIDATE_ID}" >/dev/null; then
                recovery_ok=false
                recovery_error="the failed replacement container could not be removed"
            fi
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! docker rename "${RECREATE_ORIGINAL_ID}" "${CONTAINER_NAME}" >/dev/null; then
            recovery_ok=false
            recovery_error="the previous container could not be renamed into place"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! named_container_has_id "${CONTAINER_NAME}" \
                "${RECREATE_ORIGINAL_ID}"; then
            recovery_ok=false
            recovery_error="the restored container name no longer identifies the previous container"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! docker start "${RECREATE_ORIGINAL_ID}" >/dev/null; then
            recovery_ok=false
            recovery_error="the previous container could not be started"
        fi
        if [[ "${recovery_ok}" == "true" ]]; then
            if ! docker exec --user 0:0 \
                --env "OLD_UID=${RECREATE_OLD_UID}" \
                --env "OLD_GID=${RECREATE_OLD_GID}" \
                --env "CONTAINER_HOME=${CONTAINER_HOME}" \
                --env "CONTAINER_BUILD_ROOT=${CONTAINER_BUILD_ROOT}" \
                "${RECREATE_ORIGINAL_ID}" bash -Eeuo pipefail -c '
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
                ! docker stop "${RECREATE_ORIGINAL_ID}" >/dev/null; then
                recovery_ok=false
                [[ -n "${recovery_error}" ]] ||
                    recovery_error="the restored container could not be stopped"
            fi
        fi
    else
        if [[ "${recovery_ok}" == "true" ]] &&
            ! named_container_has_id "${CONTAINER_NAME}" \
                "${RECREATE_ORIGINAL_ID}"; then
            recovery_ok=false
            recovery_error="the previous container is unavailable or changed identity"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! recreate_container_has_expected_mounts \
                "${RECREATE_ORIGINAL_ID}"; then
            recovery_ok=false
            recovery_error="the previous container has unexpected ownership or data mounts"
        fi
        if [[ "${recovery_ok}" == "true" ]] &&
            ! restore_runtime_selector "${selector_target}"; then
            recovery_ok=false
            recovery_error="the saved NVIDIA runtime selector could not be restored"
        fi
        if [[ "${recovery_ok}" == "true" &&
            "${RECREATE_WAS_RUNNING}" == "true" ]] &&
            [[ "$(docker container inspect --format '{{.State.Running}}' \
                "${RECREATE_ORIGINAL_ID}")" != "true" ]]; then
            if ! docker start "${RECREATE_ORIGINAL_ID}" >/dev/null; then
                recovery_ok=false
                recovery_error="the previous container could not be restarted"
            fi
        elif [[ "${recovery_ok}" == "true" &&
            "${RECREATE_WAS_RUNNING}" == "false" ]] &&
            [[ "$(docker container inspect --format '{{.State.Running}}' \
                "${RECREATE_ORIGINAL_ID}")" == "true" ]]; then
            if ! docker stop "${RECREATE_ORIGINAL_ID}" >/dev/null; then
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
    release_lock
    exit "${exit_code}"
}

main() {
    local command="${1:-up}"
    if (($# != 0)); then
        shift
    fi
    case "${command}" in
        help)
            (($# == 0)) || die "'${command}' accepts no arguments"
            usage
            return
            ;;
        up | shell | status | stop | recreate)
            (($# == 0)) || die "'${command}' accepts no arguments"
            ;;
        exec)
            (($# != 0)) || die "exec requires a command"
            ;;
        *)
            usage >&2
            die "unknown command: ${command}"
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
            log "enter with: bash projects/the-elder-brother-of-fauna/container.sh shell"
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
                --env "NATURALEHIA_FAUNA_CONTAINER=1" \
                --env "NATURALEHIA_FAUNA_CONFIG_FINGERPRINT=${CONFIG_FINGERPRINT}" \
                --env "NATURALEHIA_FAUNA_BUILD_ROOT=${CONTAINER_PROJECT_BUILD_ROOT}/${CONFIG_FINGERPRINT}" \
                --workdir "${CONTAINER_PROJECT_DIR}" \
                "${CONTAINER_NAME}" bash --login
            ;;
        exec)
            ensure_ready
            container_exec "$@"
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
            prepare_container_resources recreate
            if docker container inspect "${RECREATE_BACKUP_NAME}" >/dev/null 2>&1; then
                container_exists ||
                    die "only '${RECREATE_BACKUP_NAME}' exists; rename it to '${CONTAINER_NAME}' before retrying"
                RECREATE_ORIGINAL_ID="$(container_id "${RECREATE_BACKUP_NAME}")" ||
                    die "could not capture the replacement backup immutable ID"
                RECREATE_CANDIDATE_ID="$(container_id "${CONTAINER_NAME}")" ||
                    die "could not capture the replacement candidate immutable ID"
                valid_container_id "${RECREATE_ORIGINAL_ID}" ||
                    die "replacement backup has an invalid immutable ID"
                valid_container_id "${RECREATE_CANDIDATE_ID}" ||
                    die "replacement candidate has an invalid immutable ID"
                assert_named_container_id "${RECREATE_BACKUP_NAME}" \
                    "${RECREATE_ORIGINAL_ID}"
                assert_named_container_id "${CONTAINER_NAME}" \
                    "${RECREATE_CANDIDATE_ID}"
                verified_recreate_backup "${RECREATE_ORIGINAL_ID}" ||
                    die "replacement backup changed identity or has unexpected ownership, state, or data mounts"
                recreate_container_has_expected_mounts "${RECREATE_CANDIDATE_ID}" ||
                    die "replacement container has unexpected ownership or data mounts"
                verify_existing_configuration

                load_recreate_state ||
                    die "interrupted replacement has missing or invalid running-state/selector metadata"
                local interrupted_old_user
                interrupted_old_user="$(docker container inspect --format '{{.Config.User}}' \
                    "${RECREATE_ORIGINAL_ID}")"
                [[ "${interrupted_old_user}" =~ ^([0-9]+):([0-9]+)$ ]] ||
                    die "replacement backup has an unexpected user specification"
                RECREATE_OLD_UID="${BASH_REMATCH[1]}"
                RECREATE_OLD_GID="${BASH_REMATCH[2]}"
                RECREATE_RENAMED=true
                trap rollback_recreate EXIT

                log "resuming interrupted transactional replacement"
                if [[ "$(docker container inspect --format '{{.State.Running}}' \
                    "${RECREATE_CANDIDATE_ID}")" != "true" ]]; then
                    docker start "${RECREATE_CANDIDATE_ID}" >/dev/null
                fi
                assert_named_container_id "${CONTAINER_NAME}" \
                    "${RECREATE_CANDIDATE_ID}"
                provision_container
                verify_container
                container_exec make gpu-test
                assert_named_container_id "${CONTAINER_NAME}" \
                    "${RECREATE_CANDIDATE_ID}"
                recreate_container_has_expected_mounts "${RECREATE_CANDIDATE_ID}" ||
                    die "replacement candidate changed ownership or data mounts"
                verified_recreate_backup "${RECREATE_ORIGINAL_ID}" ||
                    die "refusing to remove a backup with unexpected ownership, state, or data mounts"
                docker rm "${RECREATE_ORIGINAL_ID}" >/dev/null
                RECREATE_RENAMED=false
                trap release_lock EXIT
                if ! clear_recreate_state; then
                    log "warning: replacement committed, but stale recreate state could not be removed"
                fi
                log "recreated '${CONTAINER_NAME}'"
                return
            fi
            if container_exists; then
                managed_container || die "refusing to replace an unmanaged container"
                RECREATE_ORIGINAL_ID="$(container_id "${CONTAINER_NAME}")" ||
                    die "could not capture the managed container immutable ID"
                valid_container_id "${RECREATE_ORIGINAL_ID}" ||
                    die "managed container has an invalid immutable ID"
                assert_named_container_id "${CONTAINER_NAME}" \
                    "${RECREATE_ORIGINAL_ID}"
                recreate_container_has_expected_mounts "${RECREATE_ORIGINAL_ID}" ||
                    die "refusing to replace a container with unexpected ownership or data mounts"
                ensure_container_idle_for_recreate

                RECREATE_WAS_RUNNING="$(docker container inspect \
                    --format '{{.State.Running}}' "${RECREATE_ORIGINAL_ID}")"
                local old_user
                old_user="$(docker container inspect --format '{{.Config.User}}' \
                    "${RECREATE_ORIGINAL_ID}")"
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
                trap rollback_recreate EXIT

                if [[ "${RECREATE_WAS_RUNNING}" == "true" ]]; then
                    log "stopping the idle managed container for transactional replacement"
                    docker stop "${RECREATE_ORIGINAL_ID}" >/dev/null
                fi
                assert_named_container_id "${CONTAINER_NAME}" \
                    "${RECREATE_ORIGINAL_ID}"
                docker rename "${RECREATE_ORIGINAL_ID}" \
                    "${RECREATE_BACKUP_NAME}" >/dev/null
                RECREATE_RENAMED=true
                assert_named_container_id "${RECREATE_BACKUP_NAME}" \
                    "${RECREATE_ORIGINAL_ID}"
                verified_recreate_backup "${RECREATE_ORIGINAL_ID}" ||
                    die "replacement backup changed identity or has unexpected ownership, state, or data mounts"
            fi
            create_container recreate
            provision_container
            verify_container
            container_exec make gpu-test
            if [[ "${RECREATE_RENAMED:-false}" == "true" ]]; then
                assert_named_container_id "${CONTAINER_NAME}" \
                    "${RECREATE_CANDIDATE_ID}"
                recreate_container_has_expected_mounts "${RECREATE_CANDIDATE_ID}" ||
                    die "replacement candidate changed ownership or data mounts"
                verified_recreate_backup "${RECREATE_ORIGINAL_ID}" ||
                    die "refusing to remove a backup with unexpected ownership, state, or data mounts"
                docker rm "${RECREATE_ORIGINAL_ID}" >/dev/null
                RECREATE_RENAMED=false
                trap release_lock EXIT
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
