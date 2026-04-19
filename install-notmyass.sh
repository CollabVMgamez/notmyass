#!/usr/bin/env sh

set -eu

REPO_URL="https://github.com/CollabVMgamez/notmyass.git"
INSTALL_DIR="${NOTMYASS_INSTALL_DIR:-/opt}"
REPO_DIR="${INSTALL_DIR}/notmyass"
PATH_TARGET="/usr/bin/myass"
USE_MUSL_BUILD=0

if [ "$(id -u)" -eq 0 ]; then
        SUDO=""
else
        SUDO="sudo"
fi

install_git() {
        if command -v git >/dev/null 2>&1; then
                return
        fi

        if command -v apt-get >/dev/null 2>&1; then
                ${SUDO} apt-get update
                ${SUDO} apt-get install -y git
                return
        fi

        if command -v apk >/dev/null 2>&1; then
                ${SUDO} apk update
                ${SUDO} apk add --no-cache git
                return
        fi

        if command -v dnf >/dev/null 2>&1; then
                ${SUDO} dnf install -y git
                return
        fi

        if command -v yum >/dev/null 2>&1; then
                ${SUDO} yum install -y git
                return
        fi

        if command -v zypper >/dev/null 2>&1; then
                ${SUDO} zypper --non-interactive install git
                return
        fi

        if command -v pacman >/dev/null 2>&1; then
                ${SUDO} pacman -Sy --noconfirm
                ${SUDO} pacman -S --noconfirm git
                return
        fi

        if command -v tce-load >/dev/null 2>&1; then
                tce-load -wi git
                return
        fi

        echo "No supported package manager found for automatic git installation."
        echo "Install git manually and re-run this script."
        exit 1
}

detect_path_binary() {
        if [ "${USE_MUSL_BUILD}" -eq 1 ] && [ -x "${REPO_DIR}/package-musl/myass-musl" ]; then
                printf '%s' "${REPO_DIR}/package-musl/myass-musl"
                return
        fi

        if [ -x "${REPO_DIR}/package/myass" ]; then
                printf '%s' "${REPO_DIR}/package/myass"
                return
        fi

        if [ -x "${REPO_DIR}/package-musl/myass-musl" ]; then
                printf '%s' "${REPO_DIR}/package-musl/myass-musl"
                return
        fi

        printf '%s' "${REPO_DIR}/exe/myass"
}

link_myass_to_path() {
        local bin
        bin="$(detect_path_binary)"

        if [ ! -x "${bin}" ]; then
                echo "Built binary not found at ${bin}; skipping PATH update."
                return
        fi

        ${SUDO} mkdir -p /usr/bin
        ${SUDO} ln -sf "${bin}" "${PATH_TARGET}"
        echo "Added myass command to PATH: ${PATH_TARGET}"
        echo "Run: myass"
}

main() {
        ${SUDO} mkdir -p "${INSTALL_DIR}"
        ${SUDO} rm -rf "${REPO_DIR}"

        for arg in "$@"; do
                case "${arg}" in
                        --musl)
                                USE_MUSL_BUILD=1
                                ;;
                esac
        done

        install_git
        ${SUDO} git clone "${REPO_URL}" "${REPO_DIR}"

        cd "${REPO_DIR}"
        ${SUDO} chmod +x autoinstaller.sh
        ${SUDO} ./autoinstaller.sh "$@"
        link_myass_to_path
}

main "$@"
