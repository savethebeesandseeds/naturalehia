#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -Eeuo pipefail
IFS=$'\n\t'

if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* ]]; then
    export PATH="/usr/local/bin:/usr/bin:/bin:$PATH"
fi

readonly PROJECT_SLUG="logic-gates-of-the-biological-kingdom"
readonly CONTAINER_NAME="naturalehia-logic-gates-of-the-biological-kingdom"
readonly RECREATE_BACKUP_NAME="${CONTAINER_NAME}-recreate-backup"
readonly LEGACY_CONTAINER_NAME="naturalehia-protein-logic"
readonly CONTAINER_HOSTNAME="naturalehia-logic-gates-of-the-biological-kingdom"
readonly DEFAULT_IMAGE="debian:13-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd"
readonly IMAGE="${NATURALEHIA_PROTEIN_LOGIC_IMAGE:-$DEFAULT_IMAGE}"
readonly HOST_PORT="${NATURALEHIA_PROTEIN_LOGIC_PORT:-38417}"
readonly GPU_MODE="${NATURALEHIA_PROTEIN_LOGIC_GPU:-all}"
readonly CONTAINER_PORT="38417"
readonly CONTAINER_PROJECT_DIR="/workspace/${PROJECT_SLUG}"
readonly CONTAINER_STATE_ROOT="/work/protein-logic"
readonly CONTAINER_HOME="/home/developer"
# These established, labeled volumes intentionally retain their historical names.
readonly BUILD_VOLUME="naturalehia-protein-logic-build-v1"
readonly HOME_VOLUME="naturalehia-protein-logic-home-v1"
readonly MANAGED_LABEL="org.naturalehia.protein-logic.managed"
readonly CONFIG_LABEL="org.naturalehia.protein-logic.config"
readonly VOLUME_ROLE_LABEL="org.naturalehia.protein-logic.volume-role"
readonly WORKSPACE_LABEL="org.naturalehia.protein-logic.workspace"
readonly CONTAINER_SCHEMA="2"

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
HOST_PROJECT_DIR=""
WORKSPACE_ID_SOURCE=""
DEFAULT_DEV_UID=""
DEFAULT_DEV_GID=""
WINDOWS_POSIX_SHELL="false"

case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*)
        HOST_PROJECT_DIR="$(cygpath --mixed "$script_directory")"
        WORKSPACE_ID_SOURCE="$HOST_PROJECT_DIR"
        DEFAULT_DEV_UID="1000"
        DEFAULT_DEV_GID="1000"
        WINDOWS_POSIX_SHELL="true"
        ;;
    *)
        HOST_PROJECT_DIR="$script_directory"
        WORKSPACE_ID_SOURCE="$HOST_PROJECT_DIR"
        if command -v wslpath >/dev/null 2>&1 &&
            { [[ -n "${WSL_DISTRO_NAME:-}" ]] || [[ "$(uname -r)" =~ [Mm]icrosoft ]]; }; then
            WORKSPACE_ID_SOURCE="$(wslpath -m "$script_directory")"
        fi
        DEFAULT_DEV_UID="$(id -u)"
        DEFAULT_DEV_GID="$(id -g)"
        if [[ "$DEFAULT_DEV_UID" == "0" ]]; then
            DEFAULT_DEV_UID="1000"
            DEFAULT_DEV_GID="1000"
        fi
        ;;
esac

readonly HOST_PROJECT_DIR WORKSPACE_ID_SOURCE DEFAULT_DEV_UID DEFAULT_DEV_GID
readonly WINDOWS_POSIX_SHELL
readonly DEV_UID="${NATURALEHIA_PROTEIN_LOGIC_DEV_UID:-$DEFAULT_DEV_UID}"
readonly DEV_GID="${NATURALEHIA_PROTEIN_LOGIC_DEV_GID:-$DEFAULT_DEV_GID}"
WORKSPACE_ID="$(printf '%s' "$WORKSPACE_ID_SOURCE" | sha256sum | cut -d ' ' -f 1)"
readonly WORKSPACE_ID
IMAGE_ID=""
active_build_root=""
LOCK_DIRECTORY=""
LOCK_HELD="false"
LAST_CREATED_CONTAINER_ID=""
RECREATE_TRANSACTION_ACTIVE="false"
RECREATE_RENAMED="false"
RECREATE_BACKUP_ID=""
RECREATE_CANDIDATE_ID=""
command_name=""

usage() {
    cat <<'USAGE'
Usage: bash container.sh <command> [arguments]

Commands:
  up          Create/start the container and provision its C/C++ tools
  shell       Open an interactive shell in the container
  exec CMD... Run a command in the container
  status      Show container, toolchain, GPU, volume, and port status
  stop        Stop the container while preserving its volumes
  recreate    Replace the stopped managed container while preserving its volumes
  help        Show this help

Environment overrides:
  NATURALEHIA_PROTEIN_LOGIC_IMAGE     Linux image override (digest recommended)
  NATURALEHIA_PROTEIN_LOGIC_PORT      Loopback host port (default: 38417)
  NATURALEHIA_PROTEIN_LOGIC_GPU       GPU sharing: all (default) or none
  NATURALEHIA_PROTEIN_LOGIC_DEV_UID   Container developer UID
  NATURALEHIA_PROTEIN_LOGIC_DEV_GID   Container developer GID

container.sh is the host lifecycle/access interface. setup.sh runs only inside
the container and performs idempotent toolchain and developer configuration.
Build, test, sanitizer, formatting, CLI, and GPU-check targets remain in Makefile.
USAGE
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

release_lock() {
    [[ "$LOCK_HELD" == "true" ]] || return 0

    local owner_pid=""
    local owner_file="${LOCK_DIRECTORY}/owner"
    if [[ ! -f "$owner_file" || -L "$owner_file" ]]; then
        printf 'warning: container lock owner file changed; preserving %s\n' \
            "$LOCK_DIRECTORY" >&2
        LOCK_HELD="false"
        return 1
    fi
    IFS= read -r owner_pid <"$owner_file" || true
    if [[ "$owner_pid" != "$$" ]]; then
        printf 'warning: container lock ownership changed; preserving %s\n' \
            "$LOCK_DIRECTORY" >&2
        LOCK_HELD="false"
        return 1
    fi

    rm -f -- "$owner_file" || return 1
    rmdir -- "$LOCK_DIRECTORY" || return 1
    LOCK_HELD="false"
}

rollback_recreate() {
    local recovery_ok="true"
    local actual_id=""

    RECREATE_TRANSACTION_ACTIVE="false"
    printf 'Replacement failed; restoring the previous container.\n' >&2

    if [[ "$RECREATE_RENAMED" != "true" ]]; then
        if container_exists "$RECREATE_BACKUP_NAME" &&
            [[ "$(docker_cli container inspect --format '{{.Id}}' \
                "$RECREATE_BACKUP_NAME")" == "$RECREATE_BACKUP_ID" ]] &&
            ! container_exists "$CONTAINER_NAME"; then
            RECREATE_RENAMED="true"
        else
            return 0
        fi
    fi

    if ! container_exists "$RECREATE_BACKUP_NAME"; then
        printf 'error: recreate backup %s is unavailable\n' "$RECREATE_BACKUP_NAME" >&2
        recovery_ok="false"
    elif [[ "$(docker_cli container inspect --format '{{.Id}}' \
        "$RECREATE_BACKUP_NAME")" != "$RECREATE_BACKUP_ID" ]]; then
        printf 'error: recreate backup identity changed; preserving it\n' >&2
        recovery_ok="false"
    fi

    if [[ -z "$RECREATE_CANDIDATE_ID" &&
        "$LAST_CREATED_CONTAINER_ID" =~ ^[0-9a-f]{64}$ ]]; then
        RECREATE_CANDIDATE_ID="$LAST_CREATED_CONTAINER_ID"
    fi
    if [[ "$recovery_ok" == "true" ]] && container_exists "$CONTAINER_NAME"; then
        actual_id="$(docker_cli container inspect --format '{{.Id}}' "$CONTAINER_NAME")"
        if [[ -z "$RECREATE_CANDIDATE_ID" ||
            "$actual_id" != "$RECREATE_CANDIDATE_ID" ]]; then
            printf 'error: replacement name is occupied by an unexpected container; preserving it\n' >&2
            recovery_ok="false"
        elif ! docker_cli rm --force "$RECREATE_CANDIDATE_ID" >/dev/null; then
            printf 'error: failed replacement container %s could not be removed\n' \
                "$RECREATE_CANDIDATE_ID" >&2
            recovery_ok="false"
        fi
    fi

    if [[ "$recovery_ok" == "true" ]]; then
        if docker_cli rename "$RECREATE_BACKUP_ID" "$CONTAINER_NAME" >/dev/null &&
            [[ "$(docker_cli container inspect --format '{{.Id}}' \
                "$CONTAINER_NAME")" == "$RECREATE_BACKUP_ID" ]]; then
            printf 'Previous container restored as %s; named volumes were preserved.\n' \
                "$CONTAINER_NAME" >&2
        else
            printf 'error: previous container could not be renamed back into place; backup %s is preserved\n' \
                "$RECREATE_BACKUP_NAME" >&2
            recovery_ok="false"
        fi
    else
        printf 'error: automatic rollback was incomplete; backup %s (%s) is preserved\n' \
            "$RECREATE_BACKUP_NAME" "$RECREATE_BACKUP_ID" >&2
    fi

    [[ "$recovery_ok" == "true" ]]
}

cleanup_on_exit() {
    local exit_code=$?
    local cleanup_ok="true"

    trap - EXIT
    set +e
    if [[ "$RECREATE_TRANSACTION_ACTIVE" == "true" ]]; then
        rollback_recreate || cleanup_ok="false"
    fi
    release_lock || cleanup_ok="false"
    if [[ "$cleanup_ok" != "true" && "$exit_code" -eq 0 ]]; then
        exit_code=1
    fi
    exit "$exit_code"
}

acquire_lock() {
    local lock_root="${TMPDIR:-/tmp}"
    local owner_file owner_pid="" stale_directory

    [[ -d "$lock_root" ]] || fail "container lock root does not exist: $lock_root"
    LOCK_DIRECTORY="${lock_root%/}/${CONTAINER_NAME}.lock"
    owner_file="${LOCK_DIRECTORY}/owner"

    if ! mkdir -- "$LOCK_DIRECTORY" 2>/dev/null; then
        [[ -d "$LOCK_DIRECTORY" && ! -L "$LOCK_DIRECTORY" ]] ||
            fail "container lock path is not a real directory: $LOCK_DIRECTORY"
        if [[ -e "$owner_file" || -L "$owner_file" ]]; then
            [[ -f "$owner_file" && ! -L "$owner_file" ]] ||
                fail "container lock owner is not a regular file: $owner_file"
            IFS= read -r owner_pid <"$owner_file" || true
        fi
        if [[ "$owner_pid" =~ ^[0-9]+$ ]] && kill -0 "$owner_pid" 2>/dev/null; then
            fail "another container.sh process (PID $owner_pid) is managing '$CONTAINER_NAME'"
        fi

        stale_directory="${LOCK_DIRECTORY}.stale.$$"
        [[ ! -e "$stale_directory" && ! -L "$stale_directory" ]] ||
            fail "stale-lock staging path already exists: $stale_directory"
        mv -- "$LOCK_DIRECTORY" "$stale_directory" 2>/dev/null ||
            fail "container lock changed while it was inspected; retry the command"
        if [[ -e "${stale_directory}/owner" || -L "${stale_directory}/owner" ]]; then
            [[ -f "${stale_directory}/owner" && ! -L "${stale_directory}/owner" ]] ||
                fail "stale container lock contains an unsafe owner entry"
            rm -f -- "${stale_directory}/owner" ||
                fail "could not remove the stale container lock owner"
        fi
        rmdir -- "$stale_directory" ||
            fail "stale container lock contains unexpected entries; it was preserved"
        mkdir -- "$LOCK_DIRECTORY" || fail "could not acquire the container lock"
    fi

    if ! printf '%s\n' "$$" >"$owner_file"; then
        rmdir -- "$LOCK_DIRECTORY" 2>/dev/null || true
        fail "could not record container lock ownership"
    fi
    LOCK_HELD="true"
    trap cleanup_on_exit EXIT
}

require_no_arguments() {
    (($# == 0)) || fail "'$command_name' does not accept arguments"
}

docker_cli() {
    MSYS_NO_PATHCONV=1 docker "$@"
}

validate_configuration() {
    [[ "$HOST_PORT" =~ ^[0-9]+$ ]] || fail "port must be numeric: $HOST_PORT"
    ((10#$HOST_PORT >= 1024 && 10#$HOST_PORT <= 65535)) ||
        fail "port must be between 1024 and 65535: $HOST_PORT"
    [[ "$GPU_MODE" == "all" || "$GPU_MODE" == "none" ]] ||
        fail "GPU mode must be 'all' or 'none': $GPU_MODE"
    [[ "$DEV_UID" =~ ^[1-9][0-9]*$ ]] ||
        fail "developer UID must be a canonical positive integer: $DEV_UID"
    [[ "$DEV_GID" =~ ^[1-9][0-9]*$ ]] ||
        fail "developer GID must be a canonical positive integer: $DEV_GID"
    ((10#$DEV_UID >= 1 && 10#$DEV_UID <= 4294967294)) ||
        fail "developer UID is outside the supported range: $DEV_UID"
    ((10#$DEV_GID >= 1 && 10#$DEV_GID <= 4294967294)) ||
        fail "developer GID is outside the supported range: $DEV_GID"
    [[ "$HOST_PROJECT_DIR" != *,* ]] ||
        fail "project paths containing commas are unsupported by the mount interface"
}

require_docker() {
    command -v docker >/dev/null 2>&1 || fail "Docker CLI was not found"
    docker_cli info >/dev/null 2>&1 || fail "the Docker engine is not available"
    [[ "$(docker_cli info --format '{{.OSType}}')" == "linux" ]] ||
        fail "a Linux Docker engine is required"

    local docker_endpoint
    if [[ -n "${DOCKER_HOST:-}" ]]; then
        docker_endpoint="${DOCKER_HOST}"
    else
        docker_endpoint="$(docker_cli context inspect "$(docker_cli context show)" \
            --format '{{.Endpoints.docker.Host}}' 2>/dev/null)" ||
            fail "could not inspect the active Docker context"
    fi
    case "$docker_endpoint" in
        unix://* | npipe://*) ;;
        *) fail "remote Docker contexts are unsupported for the project bind mount" ;;
    esac
}

container_exists() {
    docker_cli container inspect "$1" >/dev/null 2>&1
}

container_running() {
    [[ "$(docker_cli container inspect --format '{{.State.Running}}' "$1")" == "true" ]]
}

container_label() {
    local container_name="$1"
    local label="$2"
    docker_cli container inspect --format "{{ index .Config.Labels \"$label\" }}" "$container_name"
}

assert_legacy_absent() {
    if container_exists "$LEGACY_CONTAINER_NAME"; then
        fail "legacy container '$LEGACY_CONTAINER_NAME' exists as $(docker_cli container inspect --format '{{.Id}}' "$LEGACY_CONTAINER_NAME"); it is preserved, and '$CONTAINER_NAME' will not operate while the legacy container could share its volumes"
    fi
}

assert_recreate_backup_absent() {
    if container_exists "$RECREATE_BACKUP_NAME"; then
        fail "recreate backup '$RECREATE_BACKUP_NAME' exists as $(docker_cli container inspect --format '{{.Id}}' "$RECREATE_BACKUP_NAME"); it is preserved and must be resolved before container lifecycle operations continue"
    fi
}

assert_target_managed() {
    [[ "$(container_label "$CONTAINER_NAME" "$MANAGED_LABEL")" == "true" ]] ||
        fail "container name '$CONTAINER_NAME' is occupied by an unmanaged container"
    [[ "$(container_label "$CONTAINER_NAME" "$WORKSPACE_LABEL")" == "$WORKSPACE_ID" ]] ||
        fail "container '$CONTAINER_NAME' belongs to a different project workspace"
}

ensure_image() {
    if ! docker_cli image inspect "$IMAGE" >/dev/null 2>&1; then
        printf 'Pulling pinned development image: %s\n' "$IMAGE"
        docker_cli pull "$IMAGE"
    fi
    IMAGE_ID="$(docker_cli image inspect --format '{{.Id}}' "$IMAGE")"
}

configuration_fingerprint() {
    local setup_sha256
    setup_sha256="$(sha256sum "$script_directory/setup.sh" | cut -d ' ' -f 1)"
    printf '%s\n' \
        "schema=$CONTAINER_SCHEMA" \
        "image=$IMAGE_ID" \
        "name=$CONTAINER_NAME" \
        "hostname=$CONTAINER_HOSTNAME" \
        "workspace=$HOST_PROJECT_DIR:$CONTAINER_PROJECT_DIR" \
        "user=$DEV_UID:$DEV_GID" \
        "volumes=$BUILD_VOLUME:/work,$HOME_VOLUME:$CONTAINER_HOME" \
        "port=127.0.0.1:$HOST_PORT:$CONTAINER_PORT/tcp" \
        "gpu=$GPU_MODE" \
        "runtime=init,restart:no,no-new-privileges,pids:2048,shm:2g" \
        "setup=$setup_sha256" |
        sha256sum | cut -d ' ' -f 1
}

assert_target_matches() {
    local expected_fingerprint="$1"
    assert_target_managed
    [[ "$(container_label "$CONTAINER_NAME" "$CONFIG_LABEL")" == "$expected_fingerprint" ]] ||
        fail "managed container '$CONTAINER_NAME' has a mismatched configuration; it is preserved"
    [[ "$(docker_cli container inspect --format '{{.Image}}' "$CONTAINER_NAME")" == "$IMAGE_ID" ]] ||
        fail "managed container '$CONTAINER_NAME' uses a different image; it is preserved"
    [[ "$(docker_cli container inspect --format '{{.Config.Hostname}}' "$CONTAINER_NAME")" == "$CONTAINER_HOSTNAME" ]] ||
        fail "managed container '$CONTAINER_NAME' has a different hostname; it is preserved"
    [[ "$(docker_cli container inspect --format '{{.Config.User}}' "$CONTAINER_NAME")" == "$DEV_UID:$DEV_GID" ]] ||
        fail "managed container '$CONTAINER_NAME' has a different user; it is preserved"
    assert_exact_mounts
}

normalize_bind_source() {
    local source_path="${1//\\//}"
    case "$source_path" in
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
    if [[ "$source_path" =~ ^[A-Za-z]:/ ]]; then
        source_path="${source_path,,}"
    fi
    printf '%s\n' "${source_path%/}"
}

assert_exact_mounts() {
    local container_name="${1:-$CONTAINER_NAME}"
    local expected_bind_source mount_name mount_records mount_source mount_type
    local mount_destination mount_propagation mount_rw
    local -i build_seen=0 home_seen=0 mount_count=0 project_seen=0
    expected_bind_source="$(normalize_bind_source "$HOST_PROJECT_DIR")"
    mount_records="$(docker_cli container inspect \
        --format '{{range .Mounts}}{{printf "%s\t%s\t%s\tname=%s\t%t\t%s\n" .Type .Source .Destination .Name .RW .Propagation}}{{end}}' \
        "$container_name")" ||
        fail "could not inspect mounts for managed container '$container_name'"

    while IFS=$'\t' read -r mount_type mount_source mount_destination mount_name \
        mount_rw mount_propagation; do
        [[ -n "$mount_type" ]] || continue
        ((mount_count += 1))
        case "$mount_destination" in
            "$CONTAINER_PROJECT_DIR")
                [[ "$mount_type" == "bind" && "$mount_name" == "name=" &&
                    "$mount_rw" == "true" && "$mount_propagation" == "rprivate" &&
                    "$(normalize_bind_source "$mount_source")" == "$expected_bind_source" ]] ||
                    fail "managed container '$container_name' has a mismatched project bind; it is preserved"
                ((project_seen += 1))
                ;;
            /work)
                [[ "$mount_type" == "volume" && "$mount_name" == "name=$BUILD_VOLUME" &&
                    "$mount_rw" == "true" ]] ||
                    fail "managed container '$container_name' does not use retained build volume '$BUILD_VOLUME' at /work; it is preserved"
                ((build_seen += 1))
                ;;
            "$CONTAINER_HOME")
                [[ "$mount_type" == "volume" && "$mount_name" == "name=$HOME_VOLUME" &&
                    "$mount_rw" == "true" ]] ||
                    fail "managed container '$container_name' does not use retained home volume '$HOME_VOLUME' at $CONTAINER_HOME; it is preserved"
                ((home_seen += 1))
                ;;
            *)
                fail "managed container '$container_name' has unexpected mount '$mount_destination'; it is preserved"
                ;;
        esac
    done <<<"$mount_records"

    [[ "$mount_count" -eq 3 && "$project_seen" -eq 1 &&
        "$build_seen" -eq 1 && "$home_seen" -eq 1 ]] ||
        fail "managed container '$container_name' does not have exactly the three expected mounts; it is preserved"
}

assert_recreate_safe_target() {
    assert_target_managed
    assert_exact_mounts
    prepare_volumes
    if container_running "$CONTAINER_NAME"; then
        fail "container is running; use 'bash container.sh stop' before recreate"
    fi
}

assert_recreate_backup_matches() {
    local expected_id="$1"
    [[ "$(docker_cli container inspect --format '{{.Id}}' \
        "$RECREATE_BACKUP_NAME")" == "$expected_id" ]] ||
        fail "recreate backup '$RECREATE_BACKUP_NAME' changed identity; it is preserved"
    [[ "$(container_label "$RECREATE_BACKUP_NAME" "$MANAGED_LABEL")" == "true" ]] ||
        fail "recreate backup '$RECREATE_BACKUP_NAME' is not managed; it is preserved"
    [[ "$(container_label "$RECREATE_BACKUP_NAME" "$WORKSPACE_LABEL")" == "$WORKSPACE_ID" ]] ||
        fail "recreate backup '$RECREATE_BACKUP_NAME' belongs to another workspace; it is preserved"
    if container_running "$RECREATE_BACKUP_NAME"; then
        fail "recreate backup '$RECREATE_BACKUP_NAME' started unexpectedly; it is preserved"
    fi
    assert_exact_mounts "$RECREATE_BACKUP_NAME"
}

ensure_owned_volume() {
    local volume_name="$1"
    local expected_role="$2"
    if docker_cli volume inspect "$volume_name" >/dev/null 2>&1; then
        [[ "$(docker_cli volume inspect --format "{{ index .Labels \"$MANAGED_LABEL\" }}" "$volume_name")" == "true" &&
            "$(docker_cli volume inspect --format "{{ index .Labels \"$VOLUME_ROLE_LABEL\" }}" "$volume_name")" == "$expected_role" &&
            "$(docker_cli volume inspect --format "{{ index .Labels \"$WORKSPACE_LABEL\" }}" "$volume_name")" == "$WORKSPACE_ID" ]] ||
            fail "volume '$volume_name' exists but is not owned by this project workspace"
        return
    fi
    docker_cli volume create \
        --label "$MANAGED_LABEL=true" \
        --label "$VOLUME_ROLE_LABEL=$expected_role" \
        --label "$WORKSPACE_LABEL=$WORKSPACE_ID" \
        "$volume_name" >/dev/null
}

assert_volume_not_shared() {
    local volume_name="$1"
    local attached_id attached_ids attached_name
    attached_ids="$(docker_cli container ls --all --quiet --filter "volume=$volume_name")" ||
        fail "could not inspect containers attached to volume '$volume_name'"
    while IFS= read -r attached_id; do
        [[ -n "$attached_id" ]] || continue
        attached_name="$(docker_cli container inspect --format '{{.Name}}' "$attached_id")"
        attached_name="${attached_name#/}"
        [[ "$attached_name" == "$CONTAINER_NAME" ]] ||
            fail "volume '$volume_name' is attached to preserved container '$attached_name'; refusing to share it"
    done <<<"$attached_ids"
}

prepare_volumes() {
    ensure_owned_volume "$BUILD_VOLUME" build
    ensure_owned_volume "$HOME_VOLUME" home
    assert_volume_not_shared "$BUILD_VOLUME"
    assert_volume_not_shared "$HOME_VOLUME"
}

create_container() {
    local fingerprint="$1"
    local -a gpu_arguments=()
    if [[ "$GPU_MODE" == "all" ]]; then
        gpu_arguments=(--gpus all --env "NVIDIA_VISIBLE_DEVICES=all"
            --env "NVIDIA_DRIVER_CAPABILITIES=compute,utility")
    fi

    printf 'Creating persistent container: %s\n' "$CONTAINER_NAME"
    # The final argument is an intentionally literal program for Bash in the container.
    # shellcheck disable=SC2016
    LAST_CREATED_CONTAINER_ID="$(docker_cli create \
        --name "$CONTAINER_NAME" \
        --hostname "$CONTAINER_HOSTNAME" \
        --init \
        --restart no \
        --pids-limit 2048 \
        --shm-size 2g \
        --security-opt no-new-privileges:true \
        "${gpu_arguments[@]}" \
        --user "$DEV_UID:$DEV_GID" \
        --workdir "$CONTAINER_PROJECT_DIR" \
        --env "HOME=$CONTAINER_HOME" \
        --publish "127.0.0.1:$HOST_PORT:$CONTAINER_PORT/tcp" \
        --mount "type=bind,source=$HOST_PROJECT_DIR,target=$CONTAINER_PROJECT_DIR" \
        --mount "type=volume,source=$BUILD_VOLUME,target=/work" \
        --mount "type=volume,source=$HOME_VOLUME,target=$CONTAINER_HOME" \
        --label "$MANAGED_LABEL=true" \
        --label "$CONFIG_LABEL=$fingerprint" \
        --label "$WORKSPACE_LABEL=$WORKSPACE_ID" \
        --log-driver local \
        --log-opt max-size=10m \
        --log-opt max-file=3 \
        --entrypoint /bin/bash \
        "$IMAGE" -lc 'trap "exit 0" TERM INT; while :; do sleep 3600 & wait $!; done' \
    )"
    [[ "$LAST_CREATED_CONTAINER_ID" =~ ^[0-9a-f]{64}$ ]] ||
        fail "Docker returned an invalid immutable ID for '$CONTAINER_NAME'"
}

provision_container() {
    docker_cli exec --user 0:0 \
        --env "NATURALEHIA_PROTEIN_LOGIC_DEV_UID=$DEV_UID" \
        --env "NATURALEHIA_PROTEIN_LOGIC_DEV_GID=$DEV_GID" \
        --env "NATURALEHIA_PROTEIN_LOGIC_DEV_HOME=$CONTAINER_HOME" \
        --env "NATURALEHIA_PROTEIN_LOGIC_STATE_ROOT=$CONTAINER_STATE_ROOT" \
        "$CONTAINER_NAME" bash "$CONTAINER_PROJECT_DIR/setup.sh"
}

verify_environment() {
    local tool
    for tool in cmake ninja make g++ clang++ shellcheck uncrustify; do
        docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" \
            bash -lc "command -v '$tool' >/dev/null" ||
            fail "provisioning did not provide required tool: $tool"
    done
    for tool in python python3 python3.13; do
        if docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" \
            bash -lc "command -v '$tool' >/dev/null"; then
            fail "Python is outside this project's canonical toolchain but is installed: $tool"
        fi
    done
    if [[ "$GPU_MODE" == "all" ]]; then
        docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" nvidia-smi -L >/dev/null ||
            fail "NVIDIA GPU is not visible; check the host driver and NVIDIA Container Toolkit"
    fi
}

set_active_build_root() {
    local expected_fingerprint="$1"
    local packages_fingerprint
    packages_fingerprint="$(docker_cli exec "$CONTAINER_NAME" \
        sha256sum /work/toolchain-packages.tsv | cut -d ' ' -f 1)"
    active_build_root="$CONTAINER_STATE_ROOT/builds/$(printf '%s\n' \
        "$expected_fingerprint" "$packages_fingerprint" | sha256sum | cut -d ' ' -f 1)"
}

ensure_up() {
    validate_configuration
    require_docker
    assert_legacy_absent
    assert_recreate_backup_absent
    if container_exists "$CONTAINER_NAME"; then
        assert_target_managed
    fi
    ensure_image
    local expected_fingerprint
    expected_fingerprint="$(configuration_fingerprint)"
    if container_exists "$CONTAINER_NAME"; then
        assert_target_matches "$expected_fingerprint"
    fi
    prepare_volumes
    if ! container_exists "$CONTAINER_NAME"; then
        create_container "$expected_fingerprint"
        [[ "$(docker_cli container inspect --format '{{.Id}}' \
            "$CONTAINER_NAME")" == "$LAST_CREATED_CONTAINER_ID" ]] ||
            fail "created container identity changed before startup"
    fi
    if ! container_running "$CONTAINER_NAME"; then
        docker_cli start "$CONTAINER_NAME" >/dev/null
    fi
    provision_container
    verify_environment
    set_active_build_root "$expected_fingerprint"
}

container_exec() {
    docker_cli exec --user "$DEV_UID:$DEV_GID" \
        --workdir "$CONTAINER_PROJECT_DIR" \
        --env "HOME=$CONTAINER_HOME" \
        --env "CCACHE_DIR=$CONTAINER_STATE_ROOT/ccache" \
        --env "NATURALEHIA_PROTEIN_LOGIC_CONTAINER=1" \
        --env "NATURALEHIA_PROTEIN_LOGIC_BUILD_ROOT=$active_build_root" \
        "$CONTAINER_NAME" "$@"
}

interactive_shell() {
    local -a terminal_flags=(-i)
    [[ -t 0 && -t 1 ]] && terminal_flags=(-it)
    if [[ "$WINDOWS_POSIX_SHELL" == "true" && "${terminal_flags[*]}" == "-it" ]] &&
        command -v winpty >/dev/null 2>&1; then
        MSYS_NO_PATHCONV=1 winpty docker exec "${terminal_flags[@]}" \
            --user "$DEV_UID:$DEV_GID" \
            --workdir "$CONTAINER_PROJECT_DIR" \
            --env "HOME=$CONTAINER_HOME" \
            --env "CCACHE_DIR=$CONTAINER_STATE_ROOT/ccache" \
            --env "NATURALEHIA_PROTEIN_LOGIC_CONTAINER=1" \
            --env "NATURALEHIA_PROTEIN_LOGIC_BUILD_ROOT=$active_build_root" \
            "$CONTAINER_NAME" /bin/bash
        return
    fi
    docker_cli exec "${terminal_flags[@]}" \
        --user "$DEV_UID:$DEV_GID" \
        --workdir "$CONTAINER_PROJECT_DIR" \
        --env "HOME=$CONTAINER_HOME" \
        --env "CCACHE_DIR=$CONTAINER_STATE_ROOT/ccache" \
        --env "NATURALEHIA_PROTEIN_LOGIC_CONTAINER=1" \
        --env "NATURALEHIA_PROTEIN_LOGIC_BUILD_ROOT=$active_build_root" \
        "$CONTAINER_NAME" /bin/bash
}

show_status() {
    validate_configuration
    require_docker
    local ok="true"
    if container_exists "$LEGACY_CONTAINER_NAME"; then
        printf 'Legacy conflict: %s id=%s (preserved)\n' "$LEGACY_CONTAINER_NAME" \
            "$(docker_cli container inspect --format '{{.Id}}' "$LEGACY_CONTAINER_NAME")"
        ok="false"
    fi
    if container_exists "$RECREATE_BACKUP_NAME"; then
        printf 'Recreate backup conflict: %s id=%s (preserved)\n' \
            "$RECREATE_BACKUP_NAME" \
            "$(docker_cli container inspect --format '{{.Id}}' "$RECREATE_BACKUP_NAME")"
        ok="false"
    fi
    if ! container_exists "$CONTAINER_NAME"; then
        printf 'Container: %s (not created)\n' "$CONTAINER_NAME"
        [[ "$ok" == "true" ]]
        return
    fi

    local configuration_state="not comparable (requested image is not local)"
    local safe_to_exec="false"
    if [[ "$(container_label "$CONTAINER_NAME" "$MANAGED_LABEL")" != "true" ]]; then
        configuration_state="UNMANAGED; preserved"
        ok="false"
    elif [[ "$(container_label "$CONTAINER_NAME" "$WORKSPACE_LABEL")" != "$WORKSPACE_ID" ]]; then
        configuration_state="DIFFERENT WORKSPACE; preserved"
        ok="false"
    else
        safe_to_exec="true"
        if docker_cli image inspect "$IMAGE" >/dev/null 2>&1; then
            IMAGE_ID="$(docker_cli image inspect --format '{{.Id}}' "$IMAGE")"
            if [[ "$(container_label "$CONTAINER_NAME" "$CONFIG_LABEL")" == \
                "$(configuration_fingerprint)" ]]; then
                configuration_state="matches container.sh"
            else
                configuration_state="MISMATCH; preserved"
                ok="false"
            fi
        fi
    fi
    local running
    running="$(docker_cli container inspect --format '{{.State.Running}}' "$CONTAINER_NAME")"
    printf 'Container:   %s\nRunning:     %s\nImage:       %s\nUser:        %s\nConfig:      %s\nPorts:       %s\nGPU request: %s\nMounts:\n' \
        "$CONTAINER_NAME" \
        "$running" \
        "$(docker_cli container inspect --format '{{.Config.Image}}' "$CONTAINER_NAME")" \
        "$(docker_cli container inspect --format '{{.Config.User}}' "$CONTAINER_NAME")" \
        "$configuration_state" \
        "$(docker_cli container inspect --format '{{json .HostConfig.PortBindings}}' "$CONTAINER_NAME")" \
        "$(docker_cli container inspect --format '{{json .HostConfig.DeviceRequests}}' "$CONTAINER_NAME")"
    docker_cli container inspect \
        --format '{{range .Mounts}}{{println " " .Type .Source "->" .Destination}}{{end}}' \
        "$CONTAINER_NAME"

    if [[ "$running" == "true" && "$safe_to_exec" == "true" ]]; then
        printf '\n'
        docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" bash -lc \
            'printf "CMake:     "; cmake --version | head -n1; printf "GCC:       "; g++ --version | head -n1; printf "Clang:     "; clang++ --version | head -n1' ||
            printf 'Toolchain: unavailable\n'
        docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" \
            nvidia-smi --query-gpu=index,name,driver_version,memory.total \
            --format=csv,noheader ||
            printf 'GPU visibility: unavailable\n'
    fi
    [[ "$ok" == "true" ]]
}

stop_container() {
    validate_configuration
    require_docker
    assert_legacy_absent
    assert_recreate_backup_absent
    if ! container_exists "$CONTAINER_NAME"; then
        printf 'Container has not been created.\n'
        return
    fi
    assert_target_managed
    if container_running "$CONTAINER_NAME"; then
        docker_cli stop "$CONTAINER_NAME" >/dev/null
    fi
    printf 'Container stopped; named volumes were preserved.\n'
}

recreate_container() {
    validate_configuration
    require_docker
    assert_legacy_absent
    assert_recreate_backup_absent
    if ! container_exists "$CONTAINER_NAME"; then
        ensure_up
        printf 'Environment created; no previous container required replacement.\n'
        return
    fi

    assert_recreate_safe_target
    RECREATE_BACKUP_ID="$(docker_cli container inspect --format '{{.Id}}' \
        "$CONTAINER_NAME")"
    [[ "$RECREATE_BACKUP_ID" =~ ^[0-9a-f]{64}$ ]] ||
        fail "managed container has an invalid immutable ID; it is preserved"

    ensure_image
    local expected_fingerprint
    expected_fingerprint="$(configuration_fingerprint)"

    RECREATE_TRANSACTION_ACTIVE="true"
    printf 'Preserving previous container %s as %s during replacement.\n' \
        "$RECREATE_BACKUP_ID" "$RECREATE_BACKUP_NAME"
    docker_cli rename "$RECREATE_BACKUP_ID" "$RECREATE_BACKUP_NAME" >/dev/null
    RECREATE_RENAMED="true"
    assert_recreate_backup_matches "$RECREATE_BACKUP_ID"

    create_container "$expected_fingerprint"
    RECREATE_CANDIDATE_ID="$LAST_CREATED_CONTAINER_ID"
    [[ "$(docker_cli container inspect --format '{{.Id}}' \
        "$CONTAINER_NAME")" == "$RECREATE_CANDIDATE_ID" ]] ||
        fail "replacement container identity changed before startup"
    docker_cli start "$RECREATE_CANDIDATE_ID" >/dev/null
    provision_container
    verify_environment
    assert_target_matches "$expected_fingerprint"
    set_active_build_root "$expected_fingerprint"

    assert_recreate_backup_matches "$RECREATE_BACKUP_ID"
    docker_cli rm "$RECREATE_BACKUP_ID" >/dev/null
    RECREATE_TRANSACTION_ACTIVE="false"
    RECREATE_RENAMED="false"
    RECREATE_BACKUP_ID=""
    RECREATE_CANDIDATE_ID=""
    printf 'Environment recreated; named volumes were preserved.\n'
}

main() {
    command_name="${1:-up}"
    if (($# > 0)); then
        shift
    fi
    case "$command_name" in
        help)
            require_no_arguments "$@"
            usage
            return
            ;;
        up | shell | status | stop | recreate)
            require_no_arguments "$@"
            ;;
        exec)
            (($# > 0)) || fail "exec requires a command"
            ;;
        *)
            printf 'error: unknown command: %s\n' "$command_name" >&2
            usage >&2
            return 2
            ;;
    esac

    acquire_lock

    case "$command_name" in
        up)
            ensure_up
            printf 'Environment ready. Enter with: bash container.sh shell\n'
            ;;
        shell)
            ensure_up
            interactive_shell
            ;;
        exec)
            ensure_up
            container_exec "$@"
            ;;
        status)
            show_status
            ;;
        stop)
            stop_container
            ;;
        recreate)
            recreate_container
            ;;
    esac
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    main "$@"
fi
