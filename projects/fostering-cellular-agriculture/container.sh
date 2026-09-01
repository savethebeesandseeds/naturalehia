#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -Eeuo pipefail
IFS=$'\n\t'

if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* ]]; then
    export PATH="/usr/local/bin:/usr/bin:/bin:${PATH}"
fi

readonly PROJECT_SLUG="fostering-cellular-agriculture"
readonly CONTAINER_NAME="naturalehia-${PROJECT_SLUG}"
readonly CONTAINER_HOSTNAME="naturalehia-${PROJECT_SLUG}"
readonly RECREATE_BACKUP_NAME="${CONTAINER_NAME}-recreate-backup"
readonly IMAGE="emscripten/emsdk:6.0.5@sha256:76a44fff907397784decc435115d07fcb9587a4f1504977f39f3745e538e3a1e"
readonly CONTAINER_PROJECT_DIR="/workspace/${PROJECT_SLUG}"
readonly CONTAINER_HOME="/home/developer"
readonly HOME_VOLUME="${CONTAINER_NAME}-home-v1"
readonly MANAGED_LABEL="org.naturalehia.fostering-cellular-agriculture.managed"
readonly CONFIG_LABEL="org.naturalehia.fostering-cellular-agriculture.config"
readonly PROJECT_LABEL="org.naturalehia.project"
readonly VOLUME_ROLE_LABEL="org.naturalehia.volume-role"
readonly WORKSPACE_LABEL="org.naturalehia.workspace"
readonly IMAGE_LABEL="org.naturalehia.image"
readonly DEV_UID_LABEL="org.naturalehia.developer-uid"
readonly DEV_GID_LABEL="org.naturalehia.developer-gid"
readonly SCHEMA_LABEL="org.naturalehia.container-schema"
readonly CONTAINER_SCHEMA="1"
readonly PIDS_LIMIT="2048"
readonly SHM_SIZE="1g"
readonly SHM_BYTES="1073741824"
readonly LOG_MAX_SIZE="10m"
readonly LOG_MAX_FILES="3"
readonly WAIT_COMMAND='trap "exit 0" TERM INT; while :; do sleep 3600 & wait $!; done'
readonly -a LEGACY_CONTAINER_NAMES=(
    documents-latex-fca-whitepaper-v2-1
    fca-frontier-v02
    fca-issue-price-v02
    fca-priority-cap-v02
)

HOST_PROJECT_DIR=""
WORKSPACE_ID_SOURCE=""
WORKSPACE_ID=""
DEV_UID=""
DEV_GID=""
DEV_USERNAME=""
IMAGE_ID=""
CONFIG_FINGERPRINT=""
WINDOWS_POSIX_SHELL="false"
HOST_LOCK_DIR=""
HOST_LOCK_HELD="false"
RECREATE_TRANSACTION_ACTIVE="false"
RECREATE_WAS_RUNNING="false"
RECREATE_RENAMED="false"
CREATED_CONTAINER_ID=""
RECREATE_ORIGINAL_ID=""
RECREATE_CANDIDATE_ID=""

log() {
    printf '[fostering-cellular-agriculture] %s\n' "$*"
}

fail() {
    printf '[fostering-cellular-agriculture] error: %s\n' "$*" >&2
    exit 1
}

release_host_lock() {
    [[ "${HOST_LOCK_HELD}" == "true" ]] || return 0
    if ! rmdir -- "${HOST_LOCK_DIR}"; then
        printf '[fostering-cellular-agriculture] warning: could not release host lock %s\n' \
            "${HOST_LOCK_DIR}" >&2
    fi
    HOST_LOCK_HELD="false"
}

exit_handler() {
    local exit_status=$?
    trap - ERR EXIT
    set +e
    # Keep a minimal cleanup trap active in case a defensive rollback check
    # deliberately exits rather than returning a status.
    trap release_host_lock EXIT
    if [[ "${RECREATE_TRANSACTION_ACTIVE}" == "true" ]] && ((exit_status != 0)); then
        rollback_recreate "${exit_status}"
    fi
    trap - EXIT
    release_host_lock
    return "${exit_status}"
}

acquire_host_lock() {
    require_command mkdir
    require_command rmdir
    HOST_LOCK_DIR="/tmp/${CONTAINER_NAME}-${WORKSPACE_ID}.lock"
    if ! mkdir -- "${HOST_LOCK_DIR}" 2>/dev/null; then
        fail "another host invocation is active, or a stale lock is preserved at ${HOST_LOCK_DIR}"
    fi
    HOST_LOCK_HELD="true"
    trap exit_handler EXIT
}

usage() {
    cat <<'USAGE'
Usage: bash container.sh <command> [arguments]

Manage the persistent Fostering Cellular Agriculture development container.

Commands:
  up          Create, start, and configure the container (default)
  shell       Open an interactive shell in the project directory
  exec CMD... Run a command in the project directory
  status      Show container, image, mount, and ownership configuration
  stop        Stop the container while preserving its home volume
  recreate    Transactionally replace the managed container and keep its volume
  help        Show this help

Environment overrides:
  NATURALEHIA_FCA_DEV_UID  Developer UID (1000 on Windows; host UID elsewhere)
  NATURALEHIA_FCA_DEV_GID  Developer GID (1000 on Windows; host GID elsewhere)

setup.sh is an in-container provisioning script. It is not a host lifecycle or
project-operation dispatcher. Use CMake, CMakePresets, Makefiles, and the
project executables through `bash container.sh exec ...`.
USAGE
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

validate_id() {
    local label="$1"
    local value="$2"
    [[ "${value}" =~ ^[1-9][0-9]*$ ]] ||
        fail "${label} must be a canonical positive integer"
    ((10#${value} >= 1 && 10#${value} <= 4294967294)) ||
        fail "${label} is outside the supported range"
}

resolve_project() {
    local script_dir
    require_command sha256sum
    script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
    [[ -f "${script_dir}/CMakeLists.txt" ]] ||
        fail "container.sh must remain in the project root"
    [[ -f "${script_dir}/setup.sh" ]] ||
        fail "setup.sh is missing from the project root"

    case "$(uname -s)" in
        MINGW* | MSYS* | CYGWIN*)
            require_command cygpath
            HOST_PROJECT_DIR="$(cygpath --absolute --mixed "${script_dir}")"
            WORKSPACE_ID_SOURCE="${HOST_PROJECT_DIR}"
            WINDOWS_POSIX_SHELL="true"
            export MSYS_NO_PATHCONV=1
            ;;
        *)
            HOST_PROJECT_DIR="${script_dir}"
            WORKSPACE_ID_SOURCE="${script_dir}"
            if command -v wslpath >/dev/null 2>&1 &&
                { [[ -n "${WSL_DISTRO_NAME:-}" ]] || [[ "$(uname -r)" =~ [Mm]icrosoft ]]; }; then
                WORKSPACE_ID_SOURCE="$(wslpath -m "${script_dir}")"
            fi
            ;;
    esac
    [[ "${HOST_PROJECT_DIR}" != *,* ]] ||
        fail "project paths containing commas are unsupported by Docker --mount"
    WORKSPACE_ID="$(printf '%s' "${WORKSPACE_ID_SOURCE}" | sha256sum | cut -d ' ' -f 1)"
}

detect_identity() {
    local detected_uid
    local detected_gid
    detected_uid="$(id -u)"
    detected_gid="$(id -g)"
    if [[ "${WINDOWS_POSIX_SHELL}" == "true" ]]; then
        detected_uid="1000"
        detected_gid="1000"
    elif [[ "${detected_uid}" == "0" || "${detected_gid}" == "0" ]]; then
        [[ -n "${NATURALEHIA_FCA_DEV_UID:-}" && -n "${NATURALEHIA_FCA_DEV_GID:-}" ]] ||
            fail "run as a normal user or set both developer UID/GID overrides"
    fi
    DEV_UID="${NATURALEHIA_FCA_DEV_UID:-${detected_uid}}"
    DEV_GID="${NATURALEHIA_FCA_DEV_GID:-${detected_gid}}"
    validate_id "developer UID" "${DEV_UID}"
    validate_id "developer GID" "${DEV_GID}"
}

docker_cli() {
    MSYS_NO_PATHCONV=1 docker "$@"
}

require_docker() {
    require_command docker
    require_command sha256sum
    docker_cli info >/dev/null 2>&1 ||
        fail "Docker is unavailable or the daemon is not running"
    [[ "$(docker_cli info --format '{{.OSType}}')" == "linux" ]] ||
        fail "Docker's Linux container engine is required"

    local docker_endpoint
    if [[ -n "${DOCKER_HOST:-}" ]]; then
        docker_endpoint="${DOCKER_HOST}"
    else
        docker_endpoint="$(docker_cli context inspect "$(docker_cli context show)" \
            --format '{{.Endpoints.docker.Host}}' 2>/dev/null)" ||
            fail "could not inspect the active Docker context"
    fi
    case "${docker_endpoint}" in
        unix://* | npipe://*) ;;
        *) fail "remote Docker contexts are unsupported for the project bind mount" ;;
    esac
}

container_exists() {
    local container_name="${1:-${CONTAINER_NAME}}"
    docker_cli container inspect "${container_name}" >/dev/null 2>&1
}

container_id() {
    local container_name="$1"
    docker_cli container inspect --format '{{.Id}}' "${container_name}"
}

validate_container_id() {
    local label="$1"
    local value="$2"
    [[ "${value}" =~ ^[0-9a-f]{64}$ ]] ||
        fail "${label} is not a full immutable Docker container ID"
}

assert_named_container_id() {
    local container_name="$1"
    local expected_id="$2"
    local actual_id
    actual_id="$(container_id "${container_name}")" ||
        fail "could not resolve immutable ID for container '${container_name}'"
    [[ "${actual_id}" == "${expected_id}" ]] ||
        fail "container '${container_name}' changed identity; preserving all containers"
}

container_running() {
    local container_name="${1:-${CONTAINER_NAME}}"
    [[ "$(docker_cli container inspect --format '{{.State.Running}}' \
        "${container_name}")" == "true" ]]
}

container_label() {
    local container_name="$1"
    local label_name="$2"
    docker_cli container inspect \
        --format "{{ index .Config.Labels \"${label_name}\" }}" \
        "${container_name}"
}

assert_managed_container() {
    local container_name="${1:-${CONTAINER_NAME}}"
    [[ "$(container_label "${container_name}" "${MANAGED_LABEL}")" == "true" ]] ||
        fail "container '${container_name}' is not managed by this project"
    [[ "$(container_label "${container_name}" "${PROJECT_LABEL}")" == "${PROJECT_SLUG}" ]] ||
        fail "container '${container_name}' belongs to a different project"
    [[ "$(container_label "${container_name}" "${WORKSPACE_LABEL}")" == "${WORKSPACE_ID}" ]] ||
        fail "container '${container_name}' belongs to a different workspace"
}

assert_legacy_names_absent() {
    local legacy_name
    for legacy_name in "${LEGACY_CONTAINER_NAMES[@]}"; do
        if container_exists "${legacy_name}"; then
            fail "legacy container '${legacy_name}' is preserved; resolve it explicitly before continuing"
        fi
    done
    if container_exists "${RECREATE_BACKUP_NAME}"; then
        fail "replacement backup '${RECREATE_BACKUP_NAME}' is preserved; inspect it before continuing"
    fi
}

ensure_image() {
    if ! docker_cli image inspect "${IMAGE}" >/dev/null 2>&1; then
        log "pulling pinned multi-architecture image ${IMAGE}"
        docker_cli pull "${IMAGE}"
    fi
    IMAGE_ID="$(docker_cli image inspect --format '{{.Id}}' "${IMAGE}")"
}

assert_home_volume_exclusive() {
    local allow_recreate_backup="${1:-false}"
    local attached_name
    local attached_names
    attached_names="$(docker_cli ps --all --filter "volume=${HOME_VOLUME}" \
        --format '{{.Names}}')" ||
        fail "could not inspect attachments for home volume '${HOME_VOLUME}'"
    while IFS= read -r attached_name; do
        [[ -n "${attached_name}" ]] || continue
        case "${attached_name}" in
            "${CONTAINER_NAME}")
                assert_recreatable_container "${attached_name}"
                ;;
            "${RECREATE_BACKUP_NAME}")
                [[ "${allow_recreate_backup}" == "true" ]] ||
                    fail "home volume is attached to preserved backup '${attached_name}'"
                assert_recreatable_container "${attached_name}"
                container_running "${attached_name}" &&
                    fail "replacement backup '${attached_name}' must remain stopped"
                ;;
            *)
                fail "home volume '${HOME_VOLUME}' is attached to preserved container '${attached_name}'"
                ;;
        esac
    done <<<"${attached_names}"
}

calculate_fingerprint() {
    CONFIG_FINGERPRINT="$({
        printf 'schema=%s\n' "${CONTAINER_SCHEMA}"
        printf 'image=%s\n' "${IMAGE_ID}"
        printf 'image-ref=%s\n' "${IMAGE}"
        printf 'name=%s\n' "${CONTAINER_NAME}"
        printf 'hostname=%s\n' "${CONTAINER_HOSTNAME}"
        printf 'user=%s:%s\n' "${DEV_UID}" "${DEV_GID}"
        printf 'workdir=%s\n' "${CONTAINER_PROJECT_DIR}"
        printf 'workspace=%s:%s:rprivate\n' "${HOST_PROJECT_DIR}" "${CONTAINER_PROJECT_DIR}"
        printf 'home-volume=%s:%s\n' "${HOME_VOLUME}" "${CONTAINER_HOME}"
        printf 'runtime=init,restart:no,network:bridge,no-new-privileges,pids:%s,shm:%s\n' \
            "${PIDS_LIMIT}" "${SHM_SIZE}"
        printf 'ports=none\n'
        printf 'gpu=none\n'
        printf 'environment=HOME:%s,LANG:C.UTF-8\n' "${CONTAINER_HOME}"
        printf 'logging=local,max-size:%s,max-file:%s\n' \
            "${LOG_MAX_SIZE}" "${LOG_MAX_FILES}"
        printf 'process=/bin/bash -lc %s\n' "${WAIT_COMMAND}"
    } | sha256sum | cut -d ' ' -f 1)"
}

ensure_home_volume() {
    local driver
    local managed
    local project
    local role
    local workspace
    local schema
    if docker_cli volume inspect "${HOME_VOLUME}" >/dev/null 2>&1; then
        managed="$(docker_cli volume inspect --format \
            "{{ index .Labels \"${MANAGED_LABEL}\" }}" "${HOME_VOLUME}")"
        project="$(docker_cli volume inspect --format \
            "{{ index .Labels \"${PROJECT_LABEL}\" }}" "${HOME_VOLUME}")"
        role="$(docker_cli volume inspect --format \
            "{{ index .Labels \"${VOLUME_ROLE_LABEL}\" }}" "${HOME_VOLUME}")"
        workspace="$(docker_cli volume inspect --format \
            "{{ index .Labels \"${WORKSPACE_LABEL}\" }}" "${HOME_VOLUME}")"
        schema="$(docker_cli volume inspect --format \
            "{{ index .Labels \"${SCHEMA_LABEL}\" }}" "${HOME_VOLUME}")"
        [[ "${managed}" == "true" && "${project}" == "${PROJECT_SLUG}" &&
            "${role}" == "home" && "${workspace}" == "${WORKSPACE_ID}" &&
            "${schema}" == "${CONTAINER_SCHEMA}" ]] ||
            fail "volume '${HOME_VOLUME}' exists but is not owned by this project workspace"
        driver="$(docker_cli volume inspect --format '{{.Driver}}' "${HOME_VOLUME}")"
        [[ "${driver}" == "local" ]] ||
            fail "volume '${HOME_VOLUME}' must use Docker's local driver"
        assert_home_volume_exclusive
        return
    fi

    docker_cli volume create \
        --label "${MANAGED_LABEL}=true" \
        --label "${PROJECT_LABEL}=${PROJECT_SLUG}" \
        --label "${VOLUME_ROLE_LABEL}=home" \
        --label "${WORKSPACE_LABEL}=${WORKSPACE_ID}" \
        --label "${SCHEMA_LABEL}=${CONTAINER_SCHEMA}" \
        "${HOME_VOLUME}" >/dev/null
    # Re-enter the existing-volume path so a same-name creation race cannot
    # bypass label, driver, or attachment validation.
    ensure_home_volume
}

create_container() {
    log "creating persistent container '${CONTAINER_NAME}'"
    CREATED_CONTAINER_ID=""
    CREATED_CONTAINER_ID="$(docker_cli create \
        --name "${CONTAINER_NAME}" \
        --hostname "${CONTAINER_HOSTNAME}" \
        --init \
        --restart no \
        --network bridge \
        --pids-limit "${PIDS_LIMIT}" \
        --shm-size "${SHM_SIZE}" \
        --security-opt no-new-privileges:true \
        --user "${DEV_UID}:${DEV_GID}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        --env "HOME=${CONTAINER_HOME}" \
        --env "LANG=C.UTF-8" \
        --mount "type=bind,source=${HOST_PROJECT_DIR},target=${CONTAINER_PROJECT_DIR},bind-propagation=rprivate" \
        --mount "type=volume,source=${HOME_VOLUME},target=${CONTAINER_HOME}" \
        --label "${MANAGED_LABEL}=true" \
        --label "${CONFIG_LABEL}=${CONFIG_FINGERPRINT}" \
        --label "${PROJECT_LABEL}=${PROJECT_SLUG}" \
        --label "${WORKSPACE_LABEL}=${WORKSPACE_ID}" \
        --label "${IMAGE_LABEL}=${IMAGE}" \
        --label "${DEV_UID_LABEL}=${DEV_UID}" \
        --label "${DEV_GID_LABEL}=${DEV_GID}" \
        --label "${SCHEMA_LABEL}=${CONTAINER_SCHEMA}" \
        --log-driver local \
        --log-opt "max-size=${LOG_MAX_SIZE}" \
        --log-opt "max-file=${LOG_MAX_FILES}" \
        --entrypoint /bin/bash \
        "${IMAGE}" \
        -lc "${WAIT_COMMAND}")"
    if [[ "${RECREATE_TRANSACTION_ACTIVE}" == "true" ]]; then
        RECREATE_CANDIDATE_ID="${CREATED_CONTAINER_ID}"
    fi
    validate_container_id "created container ID" "${CREATED_CONTAINER_ID}"
    assert_named_container_id "${CONTAINER_NAME}" "${CREATED_CONTAINER_ID}"
}

normalize_mount_source() {
    local mount_source="${1//\\//}"
    case "${mount_source}" in
        /run/desktop/mnt/host/[A-Za-z]/*)
            mount_source="${mount_source#/run/desktop/mnt/host/}"
            mount_source="${mount_source:0:1}:${mount_source:1}"
            ;;
        /host_mnt/[A-Za-z]/*)
            mount_source="${mount_source#/host_mnt/}"
            mount_source="${mount_source:0:1}:${mount_source:1}"
            ;;
        /mnt/[A-Za-z]/*)
            mount_source="${mount_source#/mnt/}"
            mount_source="${mount_source:0:1}:${mount_source:1}"
            ;;
        /[A-Za-z]/*)
            mount_source="${mount_source#/}"
            mount_source="${mount_source:0:1}:${mount_source:1}"
            ;;
    esac
    if [[ "${mount_source}" =~ ^[A-Za-z]:/ ]]; then
        mount_source="${mount_source,,}"
    fi
    printf '%s' "${mount_source%/}"
}

assert_recreatable_container() {
    local container_name="${1:-${CONTAINER_NAME}}"
    local bind_mount
    local bind_type
    local bind_source
    local bind_rw
    local bind_propagation
    local home_mount
    assert_managed_container "${container_name}"
    [[ "$(docker_cli container inspect --format '{{len .Mounts}}' "${container_name}")" == "2" ]] ||
        fail "container '${container_name}' has unexpected or additional mounts"

    bind_mount="$(docker_cli container inspect --format \
        "{{range .Mounts}}{{if eq .Destination \"${CONTAINER_PROJECT_DIR}\"}}{{.Type}}|{{.Source}}|{{.RW}}|{{.Propagation}}{{end}}{{end}}" \
        "${container_name}")"
    IFS='|' read -r bind_type bind_source bind_rw bind_propagation <<<"${bind_mount}"
    [[ "${bind_type}" == "bind" && "${bind_rw}" == "true" &&
        "${bind_propagation}" == "rprivate" ]] ||
        fail "container '${container_name}' has an unexpected project bind mount"
    [[ "$(normalize_mount_source "${bind_source}")" == \
        "$(normalize_mount_source "${HOST_PROJECT_DIR}")" ]] ||
        fail "container '${container_name}' is bound to a different project workspace"

    home_mount="$(docker_cli container inspect --format \
        "{{range .Mounts}}{{if eq .Destination \"${CONTAINER_HOME}\"}}{{.Type}}|{{.Name}}|{{.RW}}{{end}}{{end}}" \
        "${container_name}")"
    [[ "${home_mount}" == "volume|${HOME_VOLUME}|true" ]] ||
        fail "container '${container_name}' has an unexpected developer-home mount"
}

container_env_has() {
    local expected_entry="$1"
    local actual_entry
    while IFS= read -r actual_entry; do
        [[ "${actual_entry}" == "${expected_entry}" ]] && return 0
    done < <(docker_cli container inspect --format \
        '{{range .Config.Env}}{{println .}}{{end}}' "${CONTAINER_NAME}")
    return 1
}

verify_container_structure() {
    assert_recreatable_container
    [[ "$(container_label "${CONTAINER_NAME}" "${IMAGE_LABEL}")" == "${IMAGE}" ]] ||
        fail "container image-reference label differs from the pinned image"
    [[ "$(container_label "${CONTAINER_NAME}" "${DEV_UID_LABEL}")" == "${DEV_UID}" &&
        "$(container_label "${CONTAINER_NAME}" "${DEV_GID_LABEL}")" == "${DEV_GID}" ]] ||
        fail "container developer-identity labels differ from the requested identity"
    [[ "$(container_label "${CONTAINER_NAME}" "${SCHEMA_LABEL}")" == "${CONTAINER_SCHEMA}" ]] ||
        fail "container schema label differs from container.sh"
    [[ "$(container_label "${CONTAINER_NAME}" "${CONFIG_LABEL}")" == "${CONFIG_FINGERPRINT}" ]] ||
        fail "container configuration changed; run 'bash container.sh recreate'"
    [[ "$(docker_cli container inspect --format '{{.Image}}' "${CONTAINER_NAME}")" == "${IMAGE_ID}" ]] ||
        fail "container image differs from the pinned image"
    [[ "$(docker_cli container inspect --format '{{.Config.Hostname}}' "${CONTAINER_NAME}")" == "${CONTAINER_HOSTNAME}" ]] ||
        fail "container hostname differs from the project slug"
    [[ "$(docker_cli container inspect --format '{{.Config.User}}' "${CONTAINER_NAME}")" == "${DEV_UID}:${DEV_GID}" ]] ||
        fail "container user differs from the configured developer identity"
    [[ "$(docker_cli container inspect --format '{{.Config.WorkingDir}}' "${CONTAINER_NAME}")" == "${CONTAINER_PROJECT_DIR}" ]] ||
        fail "container work directory differs from the project workspace"
    [[ "$(docker_cli container inspect --format '{{.HostConfig.RestartPolicy.Name}}' "${CONTAINER_NAME}")" == "no" ]] ||
        fail "container restart policy must be 'no'"
    [[ "$(docker_cli container inspect --format '{{.HostConfig.NetworkMode}}' "${CONTAINER_NAME}")" == "bridge" ]] ||
        fail "container network mode must be the default bridge"
    [[ "$(docker_cli container inspect --format '{{.HostConfig.PidsLimit}}' "${CONTAINER_NAME}")" == "${PIDS_LIMIT}" ]] ||
        fail "container PID limit differs from the managed configuration"
    [[ "$(docker_cli container inspect --format '{{.HostConfig.ShmSize}}' "${CONTAINER_NAME}")" == "${SHM_BYTES}" ]] ||
        fail "container shared-memory size differs from the managed configuration"
    [[ "$(docker_cli container inspect --format '{{.HostConfig.Init}}' "${CONTAINER_NAME}")" == "true" ]] ||
        fail "container init setting differs from the managed configuration"
    [[ "$(docker_cli container inspect --format '{{json .HostConfig.SecurityOpt}}' "${CONTAINER_NAME}")" == '["no-new-privileges=true"]' ]] ||
        fail "container security options differ from the managed configuration"
    [[ "$(docker_cli container inspect --format '{{len .Config.Entrypoint}}' "${CONTAINER_NAME}")" == "1" &&
        "$(docker_cli container inspect --format '{{index .Config.Entrypoint 0}}' "${CONTAINER_NAME}")" == "/bin/bash" ]] ||
        fail "container entrypoint differs from the managed configuration"
    [[ "$(docker_cli container inspect --format '{{len .Config.Cmd}}' "${CONTAINER_NAME}")" == "2" &&
        "$(docker_cli container inspect --format '{{index .Config.Cmd 0}}' "${CONTAINER_NAME}")" == "-lc" &&
        "$(docker_cli container inspect --format '{{index .Config.Cmd 1}}' "${CONTAINER_NAME}")" == "${WAIT_COMMAND}" ]] ||
        fail "container command differs from the managed configuration"
    [[ "$(docker_cli container inspect --format '{{.HostConfig.LogConfig.Type}}' "${CONTAINER_NAME}")" == "local" ]] ||
        fail "container log driver differs from the managed configuration"
    [[ "$(docker_cli container inspect --format \
        '{{ index .HostConfig.LogConfig.Config "max-size" }}' "${CONTAINER_NAME}")" == "${LOG_MAX_SIZE}" &&
        "$(docker_cli container inspect --format \
        '{{ index .HostConfig.LogConfig.Config "max-file" }}' "${CONTAINER_NAME}")" == "${LOG_MAX_FILES}" ]] ||
        fail "container log rotation differs from the managed configuration"
    container_env_has "HOME=${CONTAINER_HOME}" ||
        fail "container HOME differs from the managed configuration"
    container_env_has "LANG=C.UTF-8" ||
        fail "container locale differs from the managed configuration"

    local port_bindings
    local device_requests
    port_bindings="$(docker_cli container inspect --format '{{json .HostConfig.PortBindings}}' "${CONTAINER_NAME}")"
    device_requests="$(docker_cli container inspect --format '{{json .HostConfig.DeviceRequests}}' "${CONTAINER_NAME}")"
    [[ "${port_bindings}" == "null" || "${port_bindings}" == "{}" ]] ||
        fail "managed container must not publish ports"
    [[ "${device_requests}" == "null" || "${device_requests}" == "[]" ]] ||
        fail "managed container must not request GPUs or other devices"
}

run_setup() {
    log "configuring missing project and white-paper dependencies"
    docker_cli exec \
        --user 0:0 \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        --env "HOME=/root" \
        --env "USER=root" \
        --env "LOGNAME=root" \
        --env "NATURALEHIA_PROJECT_DIR=${CONTAINER_PROJECT_DIR}" \
        --env "NATURALEHIA_DEV_HOME=${CONTAINER_HOME}" \
        --env "NATURALEHIA_DEV_UID=${DEV_UID}" \
        --env "NATURALEHIA_DEV_GID=${DEV_GID}" \
        "${CONTAINER_NAME}" \
        /bin/bash "${CONTAINER_PROJECT_DIR}/setup.sh"
}

resolve_container_identity() {
    local passwd_entry
    local -a passwd_fields
    passwd_entry="$(docker_cli exec \
        --user 0:0 \
        "${CONTAINER_NAME}" \
        getent passwd "${DEV_UID}")" ||
        fail "could not resolve provisioned UID ${DEV_UID}"
    [[ -n "${passwd_entry}" && "${passwd_entry}" != *$'\n'* ]] ||
        fail "provisioned UID ${DEV_UID} did not resolve to exactly one account"
    IFS=':' read -r -a passwd_fields <<<"${passwd_entry}"
    ((${#passwd_fields[@]} >= 7)) ||
        fail "provisioned UID ${DEV_UID} has an invalid passwd entry"
    DEV_USERNAME="${passwd_fields[0]}"
    [[ -n "${DEV_USERNAME}" && "${passwd_fields[2]}" == "${DEV_UID}" ]] ||
        fail "provisioned account does not match UID ${DEV_UID}"
    [[ "${passwd_fields[3]}" == "${DEV_GID}" ]] ||
        fail "provisioned UID ${DEV_UID} has an unexpected primary GID"
}

verify_provisioning() {
    # The single-quoted program is evaluated by Bash inside the container.
    # shellcheck disable=SC2016
    docker_cli exec \
        --user "${DEV_UID}:${DEV_GID}" \
        --workdir "${CONTAINER_PROJECT_DIR}" \
        --env "HOME=${CONTAINER_HOME}" \
        --env "USER=${DEV_USERNAME}" \
        --env "LOGNAME=${DEV_USERNAME}" \
        --env "NATURALEHIA_DEV_UID=${DEV_UID}" \
        --env "NATURALEHIA_DEV_GID=${DEV_GID}" \
        "${CONTAINER_NAME}" \
        /bin/bash -Eeuo pipefail -c '
            [[ -w "${HOME}" ]]
            [[ "${USER}" == "$(id -un)" ]]
            [[ "${LOGNAME}" == "${USER}" ]]
            [[ "$(id -u)" == "${NATURALEHIA_DEV_UID}" ]]
            [[ "$(id -g)" == "${NATURALEHIA_DEV_GID}" ]]
            [[ "$(<"${HOME}/.naturalehia-owner")" == "${NATURALEHIA_DEV_UID}:${NATURALEHIA_DEV_GID}" ]]
            [[ -r /var/lib/naturalehia-fostering-cellular-agriculture/toolchain.tsv ]]
            command -v cmake >/dev/null
            command -v emcc >/dev/null
            command -v emcmake >/dev/null
            command -v latexmk >/dev/null
            command -v node >/dev/null
            command -v shellcheck >/dev/null
        '
}

start_and_configure() {
    local allow_recreate_backup="${1:-false}"
    assert_home_volume_exclusive "${allow_recreate_backup}"
    if ! container_running; then
        log "starting '${CONTAINER_NAME}'"
        docker_cli start "${CONTAINER_NAME}" >/dev/null
    fi
    run_setup
    resolve_container_identity
    verify_container_structure
    verify_provisioning
    assert_home_volume_exclusive "${allow_recreate_backup}"
}

ensure_up() {
    assert_legacy_names_absent
    if container_exists; then
        assert_recreatable_container
    fi
    ensure_image
    calculate_fingerprint
    ensure_home_volume
    if ! container_exists; then
        create_container
    fi
    verify_container_structure
    start_and_configure
}

show_legacy_state() {
    local legacy_name
    local found="false"
    for legacy_name in "${LEGACY_CONTAINER_NAMES[@]}" "${RECREATE_BACKUP_NAME}"; do
        if container_exists "${legacy_name}"; then
            printf 'Preserved legacy/backup container: %s\n' "${legacy_name}"
            found="true"
        fi
    done
    [[ "${found}" == "true" ]] || printf 'Preserved legacy/backup containers: none\n'
}

show_status() {
    show_legacy_state
    if ! container_exists; then
        printf 'Container: %s (not created)\n' "${CONTAINER_NAME}"
    else
        assert_managed_container
        printf 'Container: %s\n' "${CONTAINER_NAME}"
        printf 'ID:        %s\n' "$(docker_cli container inspect --format '{{.Id}}' "${CONTAINER_NAME}")"
        printf 'Running:   %s\n' "$(docker_cli container inspect --format '{{.State.Running}}' "${CONTAINER_NAME}")"
        printf 'Hostname:  %s\n' "$(docker_cli container inspect --format '{{.Config.Hostname}}' "${CONTAINER_NAME}")"
        printf 'Image:     %s\n' "$(docker_cli container inspect --format '{{.Config.Image}}' "${CONTAINER_NAME}")"
        printf 'User:      %s\n' "$(docker_cli container inspect --format '{{.Config.User}}' "${CONTAINER_NAME}")"
        printf 'Workdir:   %s\n' "$(docker_cli container inspect --format '{{.Config.WorkingDir}}' "${CONTAINER_NAME}")"
        printf 'Restart:   %s\n' "$(docker_cli container inspect --format '{{.HostConfig.RestartPolicy.Name}}' "${CONTAINER_NAME}")"
        printf 'Network:   %s\n' "$(docker_cli container inspect --format '{{.HostConfig.NetworkMode}}' "${CONTAINER_NAME}")"
        printf 'Ports:     %s\n' "$(docker_cli container inspect --format '{{json .HostConfig.PortBindings}}' "${CONTAINER_NAME}")"
        printf 'Devices:   %s\n' "$(docker_cli container inspect --format '{{json .HostConfig.DeviceRequests}}' "${CONTAINER_NAME}")"
        printf 'Mounts:    %s\n' "$(docker_cli container inspect --format '{{json .Mounts}}' "${CONTAINER_NAME}")"
        if docker_cli image inspect "${IMAGE}" >/dev/null 2>&1; then
            ensure_image
            calculate_fingerprint
            if [[ "$(container_label "${CONTAINER_NAME}" "${CONFIG_LABEL}")" == "${CONFIG_FINGERPRINT}" ]]; then
                printf 'Config:    matches container.sh\n'
            else
                printf 'Config:    MISMATCH; run bash container.sh recreate\n'
            fi
        else
            printf 'Config:    not comparable; pinned image is not local\n'
        fi
    fi

    if docker_cli volume inspect "${HOME_VOLUME}" >/dev/null 2>&1; then
        printf 'Home:      %s (%s)\n' "${HOME_VOLUME}" \
            "$(docker_cli volume inspect --format '{{.Mountpoint}}' "${HOME_VOLUME}")"
        printf 'Home labels: %s\n' \
            "$(docker_cli volume inspect --format '{{json .Labels}}' "${HOME_VOLUME}")"
    else
        printf 'Home:      %s (not created)\n' "${HOME_VOLUME}"
    fi
}

rollback_recreate() {
    local failure_status="$1"
    local actual_id
    set +e
    if [[ "${RECREATE_RENAMED}" != "true" ]]; then
        if [[ "${RECREATE_WAS_RUNNING}" == "true" && -n "${RECREATE_ORIGINAL_ID}" ]] &&
            container_exists "${RECREATE_ORIGINAL_ID}"; then
            if ! container_exists "${CONTAINER_NAME}"; then
                printf 'rollback found the original container under an unexpected name; preserving it stopped\n' >&2
                return "${failure_status}"
            fi
            actual_id="$(container_id "${CONTAINER_NAME}")" || return "${failure_status}"
            if [[ "${actual_id}" != "${RECREATE_ORIGINAL_ID}" ]]; then
                printf 'rollback refused to touch a different container now named %s\n' \
                    "${CONTAINER_NAME}" >&2
                return "${failure_status}"
            fi
            assert_recreatable_container "${RECREATE_ORIGINAL_ID}"
            if ! container_running "${RECREATE_ORIGINAL_ID}"; then
                log "replacement failed before the rename; restarting the previous managed container"
                docker_cli start "${RECREATE_ORIGINAL_ID}" >/dev/null
            fi
        fi
        log "no previous container was replaced; preserving the failed container for inspection"
        return "${failure_status}"
    fi
    log "replacement failed; restoring the previous managed container"
    if [[ -n "${RECREATE_CANDIDATE_ID}" ]] &&
        container_exists "${RECREATE_CANDIDATE_ID}" &&
        ! container_exists "${CONTAINER_NAME}"; then
        printf 'rollback found the candidate under an unexpected name; preserving all containers\n' >&2
        return "${failure_status}"
    fi
    if container_exists "${CONTAINER_NAME}"; then
        if [[ -z "${RECREATE_CANDIDATE_ID}" ]]; then
            printf 'rollback cannot prove the identity of container %s; preserving all containers\n' \
                "${CONTAINER_NAME}" >&2
            return "${failure_status}"
        fi
        actual_id="$(container_id "${CONTAINER_NAME}")" || return "${failure_status}"
        if [[ "${actual_id}" != "${RECREATE_CANDIDATE_ID}" ]]; then
            printf 'rollback refused to remove a different container now named %s\n' \
                "${CONTAINER_NAME}" >&2
            return "${failure_status}"
        fi
        assert_recreatable_container "${RECREATE_CANDIDATE_ID}"
        docker_cli rm --force "${RECREATE_CANDIDATE_ID}" >/dev/null
    fi
    if [[ "${RECREATE_RENAMED}" == "true" ]] && container_exists "${RECREATE_BACKUP_NAME}"; then
        [[ -n "${RECREATE_ORIGINAL_ID}" ]] || {
            printf 'rollback cannot prove the backup container identity; preserving it\n' >&2
            return "${failure_status}"
        }
        actual_id="$(container_id "${RECREATE_BACKUP_NAME}")" || return "${failure_status}"
        if [[ "${actual_id}" != "${RECREATE_ORIGINAL_ID}" ]]; then
            printf 'rollback refused to rename an unexpected backup container %s\n' \
                "${RECREATE_BACKUP_NAME}" >&2
            return "${failure_status}"
        fi
        assert_recreatable_container "${RECREATE_ORIGINAL_ID}"
        if container_running "${RECREATE_ORIGINAL_ID}"; then
            printf 'rollback found the backup container running; preserving it without renaming\n' >&2
            return "${failure_status}"
        fi
        docker_cli rename "${RECREATE_ORIGINAL_ID}" "${CONTAINER_NAME}" >/dev/null
        assert_named_container_id "${CONTAINER_NAME}" "${RECREATE_ORIGINAL_ID}"
        if [[ "${RECREATE_WAS_RUNNING}" == "true" ]]; then
            docker_cli start "${RECREATE_ORIGINAL_ID}" >/dev/null
        fi
    fi
    return "${failure_status}"
}

recreate_container() {
    assert_legacy_names_absent
    ensure_image
    calculate_fingerprint
    ensure_home_volume

    if container_exists; then
        RECREATE_ORIGINAL_ID="$(container_id "${CONTAINER_NAME}")" ||
            fail "could not capture the existing container's immutable ID"
        validate_container_id "existing container ID" "${RECREATE_ORIGINAL_ID}"
        assert_recreatable_container "${RECREATE_ORIGINAL_ID}"
        assert_named_container_id "${CONTAINER_NAME}" "${RECREATE_ORIGINAL_ID}"
        RECREATE_WAS_RUNNING="$(docker_cli container inspect \
            --format '{{.State.Running}}' "${RECREATE_ORIGINAL_ID}")"
        RECREATE_TRANSACTION_ACTIVE="true"
        if [[ "${RECREATE_WAS_RUNNING}" == "true" ]]; then
            log "stopping the managed container for transactional replacement"
            docker_cli stop "${RECREATE_ORIGINAL_ID}" >/dev/null
        fi
        assert_named_container_id "${CONTAINER_NAME}" "${RECREATE_ORIGINAL_ID}"
        docker_cli rename "${RECREATE_ORIGINAL_ID}" "${RECREATE_BACKUP_NAME}" >/dev/null
        RECREATE_RENAMED="true"
        assert_named_container_id "${RECREATE_BACKUP_NAME}" "${RECREATE_ORIGINAL_ID}"
        assert_home_volume_exclusive true
    else
        RECREATE_TRANSACTION_ACTIVE="true"
    fi

    create_container
    assert_home_volume_exclusive true
    start_and_configure "${RECREATE_RENAMED}"
    if [[ "${RECREATE_RENAMED}" == "true" ]]; then
        assert_named_container_id "${RECREATE_BACKUP_NAME}" "${RECREATE_ORIGINAL_ID}"
        assert_recreatable_container "${RECREATE_ORIGINAL_ID}"
        docker_cli rm "${RECREATE_ORIGINAL_ID}" >/dev/null
        RECREATE_RENAMED="false"
    fi
    RECREATE_TRANSACTION_ACTIVE="false"
    assert_home_volume_exclusive
    log "recreated '${CONTAINER_NAME}'; home volume '${HOME_VOLUME}' was preserved"
}

main() {
    local command_name="${1:-up}"
    if (($# > 0)); then
        shift
    fi

    case "${command_name}" in
        help | -h | --help)
            (($# == 0)) || fail "help does not accept arguments"
            usage
            return
            ;;
        exec)
            (($# > 0)) || fail "exec requires a command"
            ;;
        up | shell | status | stop | recreate)
            (($# == 0)) || fail "${command_name} does not accept arguments"
            ;;
        *)
            usage >&2
            fail "unknown command: ${command_name}"
            ;;
    esac

    resolve_project
    acquire_host_lock
    detect_identity
    require_docker

    case "${command_name}" in
        up)
            ensure_up
            log "environment ready; enter it with 'bash container.sh shell'"
            ;;
        shell)
            ensure_up
            docker_cli exec --interactive --tty \
                --user "${DEV_UID}:${DEV_GID}" \
                --workdir "${CONTAINER_PROJECT_DIR}" \
                --env "HOME=${CONTAINER_HOME}" \
                --env "USER=${DEV_USERNAME}" \
                --env "LOGNAME=${DEV_USERNAME}" \
                "${CONTAINER_NAME}" /bin/bash
            ;;
        exec)
            ensure_up
            docker_cli exec \
                --user "${DEV_UID}:${DEV_GID}" \
                --workdir "${CONTAINER_PROJECT_DIR}" \
                --env "HOME=${CONTAINER_HOME}" \
                --env "USER=${DEV_USERNAME}" \
                --env "LOGNAME=${DEV_USERNAME}" \
                "${CONTAINER_NAME}" "$@"
            ;;
        status)
            show_status
            ;;
        stop)
            if container_exists; then
                assert_managed_container
                if container_running; then
                    docker_cli stop "${CONTAINER_NAME}" >/dev/null
                    log "container stopped; home volume was preserved"
                else
                    log "container is already stopped; home volume is preserved"
                fi
            else
                log "container has not been created"
            fi
            ;;
        recreate)
            recreate_container
            ;;
    esac
}

main "$@"
