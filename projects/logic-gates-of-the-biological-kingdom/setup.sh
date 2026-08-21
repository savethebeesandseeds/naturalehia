#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -Eeuo pipefail

if [[ "${OSTYPE:-}" == msys* || "${OSTYPE:-}" == cygwin* ]]; then
    export PATH="/usr/local/bin:/usr/bin:/bin:$PATH"
fi

readonly PROJECT_SLUG="logic-gates-of-the-biological-kingdom"
readonly CONTAINER_NAME="naturalehia-protein-logic"
readonly CONTAINER_HOSTNAME="protein-logic"
readonly DEFAULT_IMAGE="debian:13-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd"
readonly IMAGE="${NATURALEHIA_PROTEIN_LOGIC_IMAGE:-$DEFAULT_IMAGE}"
readonly HOST_PORT="${NATURALEHIA_PROTEIN_LOGIC_PORT:-38417}"
readonly GPU_MODE="${NATURALEHIA_PROTEIN_LOGIC_GPU:-all}"
readonly CONTAINER_PORT="38417"
readonly CONTAINER_PROJECT_DIR="/workspace/${PROJECT_SLUG}"
readonly CONTAINER_STATE_ROOT="/work/protein-logic"
readonly CONTAINER_HOME="/home/developer"
readonly BUILD_VOLUME="naturalehia-protein-logic-build-v1"
readonly HOME_VOLUME="naturalehia-protein-logic-home-v1"
readonly MANAGED_LABEL="org.naturalehia.protein-logic.managed"
readonly CONFIG_LABEL="org.naturalehia.protein-logic.config"
readonly VOLUME_ROLE_LABEL="org.naturalehia.protein-logic.volume-role"
readonly WORKSPACE_LABEL="org.naturalehia.protein-logic.workspace"
readonly PROVISION_REVISION="1"

readonly -a TOOLCHAIN_PACKAGES=(
    bash
    build-essential
    ca-certificates
    ccache
    clang
    cmake
    git
    less
    libclang-rt-19-dev
    ninja-build
    passwd
    pkg-config
    shellcheck
    uncrustify
)

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
HOST_PROJECT_DIR=""
WORKSPACE_ID_SOURCE=""
DEFAULT_DEV_UID=""
DEFAULT_DEV_GID=""
WINDOWS_POSIX_SHELL="false"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
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
            windows_workspace_path="$(wslpath -m "$script_directory")"
            WORKSPACE_ID_SOURCE="$windows_workspace_path"
        fi
        detected_uid="$(id -u)"
        detected_gid="$(id -g)"
        if [[ "$detected_uid" == "0" ]]; then
            detected_uid="1000"
            detected_gid="1000"
        fi
        DEFAULT_DEV_UID="$detected_uid"
        DEFAULT_DEV_GID="$detected_gid"
        ;;
esac

readonly HOST_PROJECT_DIR WORKSPACE_ID_SOURCE DEFAULT_DEV_UID DEFAULT_DEV_GID
readonly WINDOWS_POSIX_SHELL
readonly DEV_UID="${NATURALEHIA_PROTEIN_LOGIC_DEV_UID:-$DEFAULT_DEV_UID}"
readonly DEV_GID="${NATURALEHIA_PROTEIN_LOGIC_DEV_GID:-$DEFAULT_DEV_GID}"
WORKSPACE_ID="$(printf '%s' "$WORKSPACE_ID_SOURCE" | sha256sum | cut -d ' ' -f 1)"
readonly WORKSPACE_ID
active_build_root=""

usage() {
    cat <<'USAGE'
Usage: bash setup.sh <command> [arguments]

Manage the persistent Linux development environment for
Logic Gates of the Biological Kingdom. The default command is "up".

Commands:
  up          Create/start the container and provision its C/C++ tools
  shell       Open an interactive shell in the container
  exec        Run a command in the container
  status      Show container, toolchain, GPU, volume, and port status
  stop        Stop the container while preserving its volumes
  recreate    Replace the managed container while preserving its volumes
  help        Show this help

Environment overrides (set before the first "up", then run "recreate"):
  NATURALEHIA_PROTEIN_LOGIC_IMAGE     Linux image override (digest recommended)
  NATURALEHIA_PROTEIN_LOGIC_PORT      Loopback host port (default: 38417)
  NATURALEHIA_PROTEIN_LOGIC_GPU       GPU sharing: all (default) or none
  NATURALEHIA_PROTEIN_LOGIC_DEV_UID   Container developer UID
  NATURALEHIA_PROTEIN_LOGIC_DEV_GID   Container developer GID

The project directory is bind-mounted. Build products and the developer home
live in named Docker volumes and survive stop/restart/recreate operations.
Build, test, sanitizer, formatting, CLI, and GPU-check targets live in Makefile.
USAGE
}

fail() {
    echo "error: $*" >&2
    exit 1
}

docker_cli() {
    # Git Bash otherwise rewrites Linux container paths into Windows paths.
    MSYS_NO_PATHCONV=1 docker "$@"
}

validate_configuration() {
    [[ "$HOST_PORT" =~ ^[0-9]+$ ]] || fail "port must be numeric: $HOST_PORT"
    ((HOST_PORT >= 1024 && HOST_PORT <= 65535)) || \
        fail "port must be between 1024 and 65535: $HOST_PORT"
    [[ "$GPU_MODE" == "all" || "$GPU_MODE" == "none" ]] ||
        fail "GPU mode must be 'all' or 'none': $GPU_MODE"
    [[ "$DEV_UID" =~ ^[0-9]+$ ]] || fail "developer UID must be numeric: $DEV_UID"
    [[ "$DEV_GID" =~ ^[0-9]+$ ]] || fail "developer GID must be numeric: $DEV_GID"
}

require_docker() {
    command -v docker >/dev/null 2>&1 || fail "Docker CLI was not found"
    docker_cli info >/dev/null 2>&1 || fail "the Docker engine is not available"

    local engine_os
    engine_os="$(docker_cli info --format '{{.OSType}}')"
    [[ "$engine_os" == "linux" ]] || fail "a Linux Docker engine is required"
}

ensure_image() {
    if ! docker_cli image inspect "$IMAGE" >/dev/null 2>&1; then
        echo "Pulling pinned development image: $IMAGE"
        docker_cli pull "$IMAGE"
    fi
}

configuration_fingerprint() {
    local image_id package_list
    image_id="$(docker_cli image inspect --format '{{.Id}}' "$IMAGE")"
    package_list="${TOOLCHAIN_PACKAGES[*]}"
    printf '%s\n' \
        "$PROVISION_REVISION" "$image_id" "$HOST_PROJECT_DIR" \
        "$DEV_UID:$DEV_GID" "$BUILD_VOLUME" "$HOME_VOLUME" \
        "127.0.0.1:$HOST_PORT:$CONTAINER_PORT" "gpu=$GPU_MODE" "$package_list" |
        sha256sum | cut -d ' ' -f 1
}

container_exists() {
    docker_cli container inspect "$CONTAINER_NAME" >/dev/null 2>&1
}

container_running() {
    [[ "$(docker_cli container inspect --format '{{.State.Running}}' "$CONTAINER_NAME")" == "true" ]]
}

container_label() {
    local label="$1"
    docker_cli container inspect \
        --format "{{ index .Config.Labels \"$label\" }}" "$CONTAINER_NAME"
}

assert_managed_container() {
    [[ "$(container_label "$MANAGED_LABEL")" == "true" ]] ||
        fail "container name '$CONTAINER_NAME' is occupied by an unmanaged container"
}

ensure_owned_volume() {
    local volume_name="$1"
    local expected_role="$2"

    if docker_cli volume inspect "$volume_name" >/dev/null 2>&1; then
        local managed role workspace
        managed="$(docker_cli volume inspect \
            --format "{{ index .Labels \"$MANAGED_LABEL\" }}" "$volume_name")"
        role="$(docker_cli volume inspect \
            --format "{{ index .Labels \"$VOLUME_ROLE_LABEL\" }}" "$volume_name")"
        workspace="$(docker_cli volume inspect \
            --format "{{ index .Labels \"$WORKSPACE_LABEL\" }}" "$volume_name")"

        [[ "$managed" == "true" && "$role" == "$expected_role" && \
            "$workspace" == "$WORKSPACE_ID" ]] ||
            fail "volume '$volume_name' exists but is not owned by this project workspace"
        return
    fi

    docker_cli volume create \
        --label "$MANAGED_LABEL=true" \
        --label "$VOLUME_ROLE_LABEL=$expected_role" \
        --label "$WORKSPACE_LABEL=$WORKSPACE_ID" \
        "$volume_name" >/dev/null
}

create_container() {
    local fingerprint="$1"
    local -a gpu_arguments=()

    if [[ "$GPU_MODE" == "all" ]]; then
        gpu_arguments=(
            --gpus all
            --env "NVIDIA_VISIBLE_DEVICES=all"
            --env "NVIDIA_DRIVER_CAPABILITIES=compute,utility"
        )
    fi

    ensure_owned_volume "$BUILD_VOLUME" build
    ensure_owned_volume "$HOME_VOLUME" home

    echo "Creating persistent container: $CONTAINER_NAME"
    docker_cli create \
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
        --log-driver local \
        --log-opt max-size=10m \
        --log-opt max-file=3 \
        --entrypoint /bin/bash \
        "$IMAGE" \
        -lc 'trap "exit 0" TERM INT; while :; do sleep 3600 & wait $!; done' \
        >/dev/null
}

start_container() {
    if ! container_running; then
        echo "Starting persistent container: $CONTAINER_NAME"
        docker_cli start "$CONTAINER_NAME" >/dev/null
    fi
}

provision_container() {
    local marker="/var/lib/naturalehia-protein-logic/provisioned-$PROVISION_REVISION"

    if docker_cli exec --user 0:0 "$CONTAINER_NAME" test -f "$marker"; then
        return
    fi

    echo "Provisioning C/C++ tools (first start can take a few minutes)..."
    # The single-quoted program is intentionally expanded by Bash in the container.
    # shellcheck disable=SC2016
    docker_cli exec \
        --user 0:0 \
        --env "DEV_UID=$DEV_UID" \
        --env "DEV_GID=$DEV_GID" \
        --env "DEV_HOME=$CONTAINER_HOME" \
        --env "STATE_ROOT=$CONTAINER_STATE_ROOT" \
        --env "PROVISION_MARKER=$marker" \
        --env DEBIAN_FRONTEND=noninteractive \
        "$CONTAINER_NAME" \
        bash -Eeuo pipefail -c '
            apt-get update
            apt-get install --yes --no-install-recommends "$@"

            if getent group "$DEV_GID" >/dev/null; then
                dev_group="$(getent group "$DEV_GID" | cut -d: -f1)"
            else
                dev_group="developer"
                groupadd --gid "$DEV_GID" "$dev_group"
            fi

            if getent passwd "$DEV_UID" >/dev/null; then
                dev_user="$(getent passwd "$DEV_UID" | cut -d: -f1)"
            else
                dev_user="developer"
                useradd --uid "$DEV_UID" --gid "$dev_group" \
                    --home-dir "$DEV_HOME" --shell /bin/bash "$dev_user"
            fi

            mkdir -p "$STATE_ROOT" "$DEV_HOME" "$(dirname "$PROVISION_MARKER")"
            chown -R "$DEV_UID:$DEV_GID" /work "$DEV_HOME"
            dpkg-query -W -f="\${Package}\t\${Version}\n" | sort > /work/toolchain-packages.tsv
            touch "$PROVISION_MARKER"
        ' bootstrap "${TOOLCHAIN_PACKAGES[@]}"
}

verify_environment() {
    local tool
    for tool in cmake ninja make g++ clang++ shellcheck uncrustify; do
        docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" \
            bash -lc "command -v '$tool' >/dev/null" ||
            fail "provisioning did not provide required tool: $tool"
    done

    for forbidden_tool in python python3 python3.13; do
        if docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" \
            bash -lc "command -v '$forbidden_tool' >/dev/null"; then
            fail "Python is outside this project's toolchain but was installed: $forbidden_tool"
        fi
    done

    if [[ "$GPU_MODE" == "all" ]]; then
        docker_cli exec --user "$DEV_UID:$DEV_GID" "$CONTAINER_NAME" \
            nvidia-smi -L >/dev/null 2>&1 ||
            fail "NVIDIA GPU is not visible; check the host driver and NVIDIA Container Toolkit"
    fi
}

ensure_up() {
    validate_configuration
    require_docker
    ensure_image

    local expected_fingerprint
    expected_fingerprint="$(configuration_fingerprint)"

    if container_exists; then
        assert_managed_container
        if [[ "$(container_label "$CONFIG_LABEL")" != "$expected_fingerprint" ]]; then
            fail "container configuration changed; run 'bash setup.sh recreate'"
        fi
    else
        create_container "$expected_fingerprint"
    fi

    start_container
    provision_container
    verify_environment

    local installed_packages_fingerprint build_fingerprint
    installed_packages_fingerprint="$(docker_cli exec "$CONTAINER_NAME" \
        sha256sum /work/toolchain-packages.tsv | cut -d ' ' -f 1)"
    build_fingerprint="$(printf '%s\n' "$expected_fingerprint" \
        "$installed_packages_fingerprint" | sha256sum | cut -d ' ' -f 1)"
    active_build_root="$CONTAINER_STATE_ROOT/builds/$build_fingerprint"
}

container_exec() {
    docker_cli exec \
        --user "$DEV_UID:$DEV_GID" \
        --workdir "$CONTAINER_PROJECT_DIR" \
        --env "HOME=$CONTAINER_HOME" \
        --env "CCACHE_DIR=$CONTAINER_STATE_ROOT/ccache" \
        --env "NATURALEHIA_PROTEIN_LOGIC_CONTAINER=1" \
        --env "NATURALEHIA_PROTEIN_LOGIC_BUILD_ROOT=$active_build_root" \
        "$CONTAINER_NAME" "$@"
}

interactive_shell() {
    local -a terminal_flags=(-i)
    if [[ -t 0 && -t 1 ]]; then
        terminal_flags=(-it)
    fi

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

    if ! container_exists; then
        echo "Container: $CONTAINER_NAME (not created)"
        echo "Run: bash setup.sh up"
        return
    fi

    assert_managed_container
    local actual_fingerprint expected_fingerprint configuration_state
    actual_fingerprint="$(container_label "$CONFIG_LABEL")"
    configuration_state="not comparable (requested image is not local)"
    if docker_cli image inspect "$IMAGE" >/dev/null 2>&1; then
        expected_fingerprint="$(configuration_fingerprint)"
        if [[ "$actual_fingerprint" == "$expected_fingerprint" ]]; then
            configuration_state="matches setup.sh"
        else
            configuration_state="MISMATCH; run 'bash setup.sh recreate'"
        fi
    fi

    echo "Container: $CONTAINER_NAME"
    echo "Running:   $(docker_cli container inspect --format '{{.State.Running}}' "$CONTAINER_NAME")"
    echo "Image:     $(docker_cli container inspect --format '{{.Config.Image}}' "$CONTAINER_NAME")"
    echo "User:      $(docker_cli container inspect --format '{{.Config.User}}' "$CONTAINER_NAME")"
    echo "Config:    $configuration_state"
    echo "Ports:     $(docker_cli container inspect --format '{{json .HostConfig.PortBindings}}' "$CONTAINER_NAME")"
    echo "GPU request: $(docker_cli container inspect --format '{{json .HostConfig.DeviceRequests}}' "$CONTAINER_NAME")"
    echo "Mounts:"
    docker_cli container inspect \
        --format '{{range .Mounts}}{{println " " .Type .Source "->" .Destination}}{{end}}' \
        "$CONTAINER_NAME"

    if container_running; then
        echo
        docker_cli exec "$CONTAINER_NAME" bash -lc \
            'printf "CMake:     "; cmake --version | head -n1; printf "GCC:       "; g++ --version | head -n1; printf "Clang:     "; clang++ --version | head -n1' ||
            echo "Toolchain: not provisioned"
        docker_cli exec "$CONTAINER_NAME" \
            nvidia-smi --query-gpu=index,name,driver_version,memory.total \
            --format=csv,noheader || echo "GPU visibility: unavailable"
    fi
}

remove_managed_container() {
    if ! container_exists; then
        return
    fi
    assert_managed_container
    echo "Removing managed container; named volumes are preserved."
    docker_cli rm --force "$CONTAINER_NAME" >/dev/null
}

command_name="${1:-up}"
if (($# > 0)); then
    shift
fi

case "$command_name" in
    up)
        ensure_up
        echo "Environment ready. Enter it with: bash setup.sh shell"
        echo "Run the full checks with: bash setup.sh exec make test"
        ;;
    shell)
        ensure_up
        interactive_shell
        ;;
    exec)
        (($# > 0)) || fail "exec requires a command"
        ensure_up
        container_exec "$@"
        ;;
    status)
        show_status
        ;;
    stop)
        require_docker
        if container_exists; then
            assert_managed_container
            if container_running; then
                docker_cli stop "$CONTAINER_NAME" >/dev/null
                echo "Container stopped; named volumes were preserved."
            else
                echo "Container is already stopped; named volumes are preserved."
            fi
        else
            echo "Container has not been created."
        fi
        ;;
    recreate)
        validate_configuration
        require_docker
        ensure_image
        ensure_owned_volume "$BUILD_VOLUME" build
        ensure_owned_volume "$HOME_VOLUME" home
        remove_managed_container
        ensure_up
        echo "Environment recreated; named volumes were preserved."
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        echo "error: unknown command: $command_name" >&2
        usage >&2
        exit 2
        ;;
esac
