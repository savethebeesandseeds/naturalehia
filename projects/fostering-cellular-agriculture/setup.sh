#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

set -Eeuo pipefail
IFS=$'\n\t'

readonly PROJECT_SLUG="fostering-cellular-agriculture"
readonly DEFAULT_PROJECT_DIR="/workspace/${PROJECT_SLUG}"
readonly DEFAULT_DEV_HOME="/home/developer"
readonly PROVISION_REVISION="2"

readonly -a REQUIRED_APT_PACKAGES=(
    latexmk
    passwd
    shellcheck
    texlive-fonts-recommended
    texlive-latex-base
    texlive-latex-extra
    texlive-latex-recommended
    texlive-pictures
)

fail() {
    printf 'setup.sh: %s\n' "$*" >&2
    exit 1
}

(($# == 0)) || fail "this in-container setup script does not accept commands or arguments"
[[ "${EUID}" == "0" ]] || fail "run inside the project container as root"

# The pinned emscripten/emsdk:6.0.5 image is built on Ubuntu Noble. Refuse a
# different userspace instead of applying this package contract opportunistically.
# shellcheck disable=SC1091
source /etc/os-release
[[ "${ID:-}" == "ubuntu" && "${VERSION_CODENAME:-}" == "noble" ]] ||
    fail "the pinned Ubuntu Noble base userspace is required"

readonly PROJECT_DIR="${NATURALEHIA_PROJECT_DIR:-${DEFAULT_PROJECT_DIR}}"
readonly DEV_HOME="${NATURALEHIA_DEV_HOME:-${DEFAULT_DEV_HOME}}"
readonly DEV_UID="${NATURALEHIA_DEV_UID:-1000}"
readonly DEV_GID="${NATURALEHIA_DEV_GID:-1000}"

[[ "${DEV_UID}" =~ ^[1-9][0-9]*$ ]] ||
    fail "developer UID must be a canonical positive integer"
[[ "${DEV_GID}" =~ ^[1-9][0-9]*$ ]] ||
    fail "developer GID must be a canonical positive integer"
((10#${DEV_UID} >= 1 && 10#${DEV_UID} <= 4294967294)) ||
    fail "developer UID is outside the supported range"
((10#${DEV_GID} >= 1 && 10#${DEV_GID} <= 4294967294)) ||
    fail "developer GID is outside the supported range"
[[ -f "${PROJECT_DIR}/CMakeLists.txt" ]] ||
    fail "project bind mount is missing at ${PROJECT_DIR}"
[[ -x /emsdk/upstream/emscripten/emcc ]] ||
    fail "the pinned Emscripten SDK is missing from /emsdk"

missing_packages=()
for package_name in "${REQUIRED_APT_PACKAGES[@]}"; do
    package_status="$(dpkg-query --show --showformat='${db:Status-Abbrev}' \
        "${package_name}" 2>/dev/null || true)"
    if [[ "${package_status}" != "ii " ]]; then
        missing_packages+=("${package_name}")
    fi
done

if ((${#missing_packages[@]} > 0)); then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install --yes --no-install-recommends "${missing_packages[@]}"
    apt-get clean
    rm -rf /var/lib/apt/lists/*
fi

DEV_GROUP="$(getent group "${DEV_GID}" | cut -d: -f1 || true)"
if [[ -z "${DEV_GROUP}" ]]; then
    if getent group developer >/dev/null; then
        fail "group name 'developer' already exists with a different GID"
    fi
    groupadd --gid "${DEV_GID}" developer
    DEV_GROUP="developer"
fi

DEV_USER="$(getent passwd "${DEV_UID}" | cut -d: -f1 || true)"
if [[ -z "${DEV_USER}" ]]; then
    if getent passwd developer >/dev/null; then
        fail "user name 'developer' already exists with a different UID"
    fi
    useradd \
        --uid "${DEV_UID}" \
        --gid "${DEV_GROUP}" \
        --home-dir "${DEV_HOME}" \
        --no-create-home \
        --shell /bin/bash \
        developer
    DEV_USER="developer"
fi
[[ "$(id -u "${DEV_USER}")" == "${DEV_UID}" ]] ||
    fail "resolved account '${DEV_USER}' has an unexpected UID"
[[ "$(id -g "${DEV_USER}")" == "${DEV_GID}" ]] ||
    fail "UID ${DEV_UID} exists with a different primary GID"

install -d -m 0755 -o "${DEV_UID}" -g "${DEV_GID}" "${DEV_HOME}"
owner_marker="${DEV_HOME}/.naturalehia-owner"
expected_owner="${DEV_UID}:${DEV_GID}"
recorded_owner=""
if [[ -r "${owner_marker}" ]]; then
    IFS= read -r recorded_owner <"${owner_marker}" || true
fi
if [[ "${recorded_owner}" != "${expected_owner}" ]]; then
    chown --recursive --no-dereference "${DEV_UID}:${DEV_GID}" "${DEV_HOME}"
    printf '%s\n' "${expected_owner}" >"${owner_marker}"
    chown "${DEV_UID}:${DEV_GID}" "${owner_marker}"
    chmod 0644 "${owner_marker}"
fi

readonly STATE_DIR="/var/lib/naturalehia-${PROJECT_SLUG}"
install -d -m 0755 "${STATE_DIR}"
state_tmp="${STATE_DIR}/toolchain.tsv.tmp"
emcc_version="$(emcc --version)"
{
    printf 'provision_revision\t%s\n' "${PROVISION_REVISION}"
    printf 'developer_user\t%s\n' "${DEV_USER}"
    printf 'developer_group\t%s\n' "${DEV_GROUP}"
    printf 'developer_uid\t%s\n' "${DEV_UID}"
    printf 'developer_gid\t%s\n' "${DEV_GID}"
    printf 'emscripten\t%s\n' "${emcc_version%%$'\n'*}"
    for package_name in "${REQUIRED_APT_PACKAGES[@]}"; do
        dpkg-query --show --showformat='${binary:Package}\t${Version}\n' \
            "${package_name}"
    done
} >"${state_tmp}"
mv -f "${state_tmp}" "${STATE_DIR}/toolchain.tsv"
chmod 0644 "${STATE_DIR}/toolchain.tsv"

for required_command in cmake emcc emcmake latexmk make node pdflatex shellcheck; do
    command -v "${required_command}" >/dev/null ||
        fail "required command is unavailable after provisioning: ${required_command}"
done
for required_tex_file in cleveref.sty mathpazo.sty pgfplots.sty; do
    kpsewhich "${required_tex_file}" >/dev/null ||
        fail "required TeX file is unavailable after provisioning: ${required_tex_file}"
done

printf 'Fostering Cellular Agriculture container dependencies are configured.\n'
