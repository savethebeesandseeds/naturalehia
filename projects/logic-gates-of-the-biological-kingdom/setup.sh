#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -Eeuo pipefail
IFS=$'\n\t'

readonly PROVISION_REVISION="2"
readonly PROVISION_ROOT="/var/lib/naturalehia-logic-gates-of-the-biological-kingdom"
readonly PROVISION_MARKER="${PROVISION_ROOT}/provisioned-${PROVISION_REVISION}"
readonly PACKAGE_MANIFEST="/work/toolchain-packages.tsv"

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

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_unsigned_integer() {
    local label="$1"
    local value="$2"
    [[ "$value" =~ ^[1-9][0-9]*$ ]] ||
        fail "$label must be a canonical positive integer"
    ((10#$value >= 1 && 10#$value <= 4294967294)) ||
        fail "$label is outside the supported range"
}

require_environment() {
    [[ "$#" -eq 0 ]] || fail "setup.sh does not accept commands or arguments"
    ((EUID == 0)) || fail "setup.sh must run as root inside the development container"

    : "${NATURALEHIA_PROTEIN_LOGIC_DEV_UID:?container.sh must set NATURALEHIA_PROTEIN_LOGIC_DEV_UID}"
    : "${NATURALEHIA_PROTEIN_LOGIC_DEV_GID:?container.sh must set NATURALEHIA_PROTEIN_LOGIC_DEV_GID}"
    : "${NATURALEHIA_PROTEIN_LOGIC_DEV_HOME:?container.sh must set NATURALEHIA_PROTEIN_LOGIC_DEV_HOME}"
    : "${NATURALEHIA_PROTEIN_LOGIC_STATE_ROOT:?container.sh must set NATURALEHIA_PROTEIN_LOGIC_STATE_ROOT}"

    require_unsigned_integer "developer UID" "$NATURALEHIA_PROTEIN_LOGIC_DEV_UID"
    require_unsigned_integer "developer GID" "$NATURALEHIA_PROTEIN_LOGIC_DEV_GID"
    [[ "$NATURALEHIA_PROTEIN_LOGIC_DEV_HOME" == /home/* ]] ||
        fail "developer home must be below /home"
    [[ "$NATURALEHIA_PROTEIN_LOGIC_STATE_ROOT" == /work/* ]] ||
        fail "state root must be below /work"

    [[ -r /etc/os-release ]] || fail "the container has no readable OS metadata"
    # shellcheck disable=SC1091
    source /etc/os-release
    [[ "${ID:-}" == "debian" && "${VERSION_ID:-}" == "13" ]] ||
        fail "setup.sh supports Debian 13 only"
    [[ "$(dpkg --print-architecture)" == "amd64" ]] ||
        fail "setup.sh supports Debian amd64 only"
    [[ "$(uname -m)" == "x86_64" ]] ||
        fail "setup.sh supports x86-64 only"
}

install_toolchain() {
    local package
    local -a missing_packages=()

    for package in "${TOOLCHAIN_PACKAGES[@]}"; do
        # dpkg-query expands this format; the provisioning shell must not.
        # shellcheck disable=SC2016
        if ! dpkg-query --show --showformat='${Status}' "$package" 2>/dev/null |
            grep -Fx 'install ok installed' >/dev/null; then
            missing_packages+=("$package")
        fi
    done

    if (("${#missing_packages[@]}" != 0)); then
        export DEBIAN_FRONTEND=noninteractive
        apt-get update
        apt-get install --yes --no-install-recommends "${missing_packages[@]}"
        apt-get clean
        rm -rf /var/lib/apt/lists/*
    fi
}

configure_developer() {
    local dev_gid="$NATURALEHIA_PROTEIN_LOGIC_DEV_GID"
    local dev_home="$NATURALEHIA_PROTEIN_LOGIC_DEV_HOME"
    local dev_uid="$NATURALEHIA_PROTEIN_LOGIC_DEV_UID"
    local state_root="$NATURALEHIA_PROTEIN_LOGIC_STATE_ROOT"
    local dev_group
    local dev_user

    dev_group="$(getent group "$dev_gid" | cut -d: -f1 || true)"
    if [[ -z "$dev_group" ]]; then
        if getent group developer >/dev/null; then
            fail "group name 'developer' already exists with a different GID"
        fi
        groupadd --gid "$dev_gid" developer
        dev_group="developer"
    fi

    dev_user="$(getent passwd "$dev_uid" | cut -d: -f1 || true)"
    if [[ -z "$dev_user" ]]; then
        if id developer >/dev/null 2>&1; then
            fail "user name 'developer' already exists with a different UID"
        fi
        useradd --uid "$dev_uid" --gid "$dev_group" \
            --home-dir "$dev_home" --no-create-home --shell /bin/bash developer
        dev_user="developer"
    elif [[ "$dev_user" != "developer" ]]; then
        fail "developer UID $dev_uid is already assigned to user '$dev_user'"
    fi
    [[ "$(id -g "$dev_user")" == "$dev_gid" ]] ||
        fail "UID $dev_uid exists with a different primary GID"

    install -d -m 0755 -o "$dev_uid" -g "$dev_gid" \
        "$dev_home" "$state_root" "$state_root/ccache"
    chown -R "$dev_uid:$dev_gid" /work "$dev_home"
}

record_environment() {
    local manifest_staging

    manifest_staging="$(mktemp /work/.toolchain-packages.tsv.XXXXXX)"
    trap 'rm -f -- "$manifest_staging"' RETURN
    # dpkg-query expands this format; the provisioning shell must not.
    # shellcheck disable=SC2016
    dpkg-query -W -f='${Package}\t${Version}\n' | sort >"$manifest_staging"
    chown 0:0 "$manifest_staging"
    chmod 0444 "$manifest_staging"
    mv -fT -- "$manifest_staging" "$PACKAGE_MANIFEST"
    trap - RETURN

    install -d -m 0755 "$PROVISION_ROOT"
    printf '%s\n' "$PROVISION_REVISION" >"$PROVISION_MARKER"
    chmod 0444 "$PROVISION_MARKER"
}

verify_environment() {
    local tool
    for tool in cmake ninja make g++ clang++ shellcheck uncrustify; do
        command -v "$tool" >/dev/null ||
            fail "provisioning did not provide required tool: $tool"
    done

    for tool in python python3 python3.13; do
        if command -v "$tool" >/dev/null; then
            fail "Python is outside this project's canonical toolchain but is installed: $tool"
        fi
    done
}

require_environment "$@"
install_toolchain
configure_developer
record_environment
verify_environment
