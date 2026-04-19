#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_BIN_DEFAULT="${ROOT_DIR}/package/myass"
APP_BIN_MUSL="${ROOT_DIR}/package-musl/myass-musl"
APP_FLAGS=("--install-driver")

BUILD_TARGET="default"
use_musl=0
APP_BIN="${APP_BIN_DEFAULT}"
PACKAGE_MANAGER=""
ROOT_NEEDS_ROOT=""

DEFAULT_APT_PKGS=("build-essential" "make" "gcc" "git" "pkg-config" "libx11-dev" "dkms" "kmod" "util-linux")
DEFAULT_APK_PKGS=("build-base" "git" "linux-headers" "pkgconf" "kmod" "which" "bash" "util-linux")
DEFAULT_DNF_PKGS=("gcc" "make" "git" "pkgconf-pkg-config" "libX11-devel" "dkms" "kmod" "which" "bash" "util-linux")
DEFAULT_YUM_PKGS=("gcc" "make" "git" "pkgconfig" "libX11-devel" "dkms" "kmod" "which" "bash" "util-linux")
DEFAULT_ZYPPER_PKGS=("gcc" "make" "git" "pkg-config" "libX11-devel" "dkms" "kmod" "util-linux" "bash")
DEFAULT_PACMAN_PKGS=("base-devel" "git" "pkgconf" "libx11" "dkms" "kmod" "bash" "util-linux" "linux-lts-headers")
DEFAULT_TINYCORE_PKGS=("bash" "binutils" "coreutils" "make" "gcc" "git" "patch" "grep" "sed" "linux-headers" "pkgconf")
MUSL_TINYCORE_PKGS=("musl" "musl-tools" "linux-headers" "make")
MUSL_APT_PKGS=("musl-tools" "musl-dev")
MUSL_APK_PKGS=("musl-dev")
MUSL_DNF_PKGS=("musl-gcc" "musl-devel")
MUSL_YUM_PKGS=("musl-gcc" "musl-devel")
MUSL_ZYPPER_PKGS=("musl-gcc" "musl-devel")
MUSL_PACMAN_PKGS=("musl" "musl-tools")
MUSL_CC=""

run_as_root() {
	if [ "$(id -u)" -eq 0 ]; then
		"$@"
	else
		if [ -z "${ROOT_NEEDS_ROOT}" ]; then
			echo "Sudo access required for dependency and driver installation."
			ROOT_NEEDS_ROOT=1
		fi
		require_command sudo
		sudo "$@"
	fi
}

usage() {
	cat <<'EOF'
Usage: ./autoinstaller.sh [--musl]

Builds the project and performs a persistent driver install using:
  ./exe/myass --install-driver

Options:
  --musl    Force the musl build path (make musl) instead of auto-detection.
EOF
}

is_musl_environment() {
	if [ -f /etc/alpine-release ]; then
		return 0
	fi

	if command -v ldd >/dev/null 2>&1; then
		if ldd --version 2>/dev/null | head -n 1 | grep -qi "musl"; then
			return 0
		fi
	fi

	if command -v getconf >/dev/null 2>&1; then
		if ! getconf GNU_LIBC_VERSION >/dev/null 2>&1; then
			return 0
		fi
	fi

	if [ -e /lib/ld-musl-x86_64.so.1 ] || [ -e /lib/ld-musl-aarch64.so.1 ] || [ -L /lib/ld-musl-x86_64.so.1 ]; then
		return 0
	fi

	return 1
}

select_build_profile() {
	if [ "${use_musl}" -eq 1 ]; then
		APP_BIN="${APP_BIN_MUSL}"
		BUILD_TARGET="musl"
		return
	fi

	if is_musl_environment; then
		echo "Detected musl environment; using musl profile."
		APP_BIN="${APP_BIN_MUSL}"
		BUILD_TARGET="musl"
	else
		APP_BIN="${APP_BIN_DEFAULT}"
		BUILD_TARGET="default"
	fi
}

detect_package_manager() {
	if command -v apt-get >/dev/null 2>&1; then
		PACKAGE_MANAGER="apt"
		return
	fi

	if command -v apk >/dev/null 2>&1; then
		PACKAGE_MANAGER="apk"
		return
	fi

	if command -v dnf >/dev/null 2>&1; then
		PACKAGE_MANAGER="dnf"
		return
	fi

	if command -v yum >/dev/null 2>&1; then
		PACKAGE_MANAGER="yum"
		return
	fi

	if command -v zypper >/dev/null 2>&1; then
		PACKAGE_MANAGER="zypper"
		return
	fi

	if command -v pacman >/dev/null 2>&1; then
		PACKAGE_MANAGER="pacman"
		return
	fi

	if command -v tce-load >/dev/null 2>&1; then
		PACKAGE_MANAGER="tinycore"
		return
	fi

	PACKAGE_MANAGER="unknown"
}

apt_cache_has() {
	apt-cache show "$1" >/dev/null 2>&1
}

apt_install_headers_pkg() {
	local kernel_pkg="linux-headers-$(uname -r)"
	if apt_cache_has "${kernel_pkg}"; then
		echo "${kernel_pkg}"
		return
	fi

	if [ -x /usr/bin/dpkg ]; then
		local arch
		arch="$(dpkg --print-architecture 2>/dev/null || true)"
		case "${arch}" in
			amd64|i386)
				if apt_cache_has "linux-headers-amd64"; then
					echo "linux-headers-amd64"
					return
				fi
				;;
			arm64)
				if apt_cache_has "linux-headers-arm64"; then
					echo "linux-headers-arm64"
					return
				fi
				;;
		esac
	fi

	if apt_cache_has "linux-headers"; then
		echo "linux-headers"
		return
	fi

	echo "linux-headers-$(uname -r)"
}

dnf_install_headers_pkg() {
	local kernel_pkg="kernel-devel-$(uname -r)"
	if dnf -q list installed "${kernel_pkg}" >/dev/null 2>&1; then
		echo "${kernel_pkg}"
		return
	fi
	if dnf -q list available "kernel-devel" >/dev/null 2>&1; then
		echo "kernel-devel"
		return
	fi
	if dnf -q list available "kernel-headers" >/dev/null 2>&1; then
		echo "kernel-headers"
		return
	fi
	echo "kernel-devel"
}

yum_install_headers_pkg() {
	local kernel_pkg="kernel-devel-$(uname -r)"
	if yum -q list installed "${kernel_pkg}" >/dev/null 2>&1; then
		echo "${kernel_pkg}"
		return
	fi
	if yum -q list available "kernel-devel" >/dev/null 2>&1; then
		echo "kernel-devel"
		return
	fi
	if yum -q list available "kernel-headers" >/dev/null 2>&1; then
		echo "kernel-headers"
		return
	fi
	echo "kernel-devel"
}

zypper_install_headers_pkg() {
	local kernel_pkg="kernel-default-devel"
	if zypper --non-interactive info "${kernel_pkg}" >/dev/null 2>&1; then
		echo "${kernel_pkg}"
		return
	fi
	if zypper --non-interactive info "kernel-devel" >/dev/null 2>&1; then
		echo "kernel-devel"
		return
	fi
	echo "${kernel_pkg}"
}

pacman_install_headers_pkg() {
	if pacman -Si linux-headers >/dev/null 2>&1; then
		echo "linux-headers"
		return
	fi
	if pacman -Si linux-lts-headers >/dev/null 2>&1; then
		echo "linux-lts-headers"
		return
	fi
	echo "linux-headers"
}

install_dependencies() {
	local pkg_manager="$1"
	local -a pkgs=()
	local header_pkg
	local missing_packages=0

	install_tinycore_package() {
		if tce-load -wi "$1"; then
			return 0
		fi

		echo "Tiny Core package not found: $1" >&2
		missing_packages=1
		return 1
	}

	install_tinycore_packages() {
		local -a tiny_pkgs=("$@")
		local p

		for p in "${tiny_pkgs[@]}"; do
			install_tinycore_package "$p"
		done

		if [ "${missing_packages}" -ne 0 ]; then
			echo "Some Tiny Core packages were not installed automatically." >&2
			echo "Install equivalent TCE extensions manually for your Tiny Core build and rerun the script." >&2
			return 1
		fi
	}

	install_apk_package_with_fallback() {
		local required=$1
		shift
		local -a candidates=("$@")
		local pkg

		for pkg in "${candidates[@]}"; do
			if run_as_root apk add --no-cache "${pkg}"; then
				return 0
			fi
		done

		if [ "${required}" -eq 1 ]; then
			echo "Could not install required Alpine package. Tried: ${candidates[*]}" >&2
			return 1
		fi

		echo "Optional Alpine package not found, skipping. Tried: ${candidates[*]}" >&2
		return 0
	}

	case "${pkg_manager}" in
		apt)
			pkgs=("${DEFAULT_APT_PKGS[@]}")
			header_pkg="$(apt_install_headers_pkg)"
			pkgs+=("${header_pkg}")
			if [ "${BUILD_TARGET}" = "musl" ]; then
				pkgs+=("${MUSL_APT_PKGS[@]}")
			fi
			run_as_root apt-get update -y
			run_as_root apt-get install -y --no-install-recommends "${pkgs[@]}"
			;;
		apk)
			run_as_root apk update
			install_apk_package_with_fallback 1 "build-base"
			install_apk_package_with_fallback 1 "git"
			install_apk_package_with_fallback 1 "linux-headers"
			install_apk_package_with_fallback 1 "pkgconf"
			install_apk_package_with_fallback 1 "kmod"
			install_apk_package_with_fallback 1 "which"
			install_apk_package_with_fallback 1 "bash"
			install_apk_package_with_fallback 1 "libx11-dev" "libx11"
			install_apk_package_with_fallback 0 "dkms"
			if [ "${BUILD_TARGET}" = "musl" ]; then
				install_apk_package_with_fallback 1 "${MUSL_APK_PKGS[@]}"
			fi
			;;
		dnf)
			pkgs=("${DEFAULT_DNF_PKGS[@]}")
			header_pkg="$(dnf_install_headers_pkg)"
			pkgs+=("${header_pkg}")
			if [ "${BUILD_TARGET}" = "musl" ]; then
				pkgs+=("${MUSL_DNF_PKGS[@]}")
			fi
			run_as_root dnf install -y "${pkgs[@]}"
			;;
		yum)
			pkgs=("${DEFAULT_YUM_PKGS[@]}")
			header_pkg="$(yum_install_headers_pkg)"
			pkgs+=("${header_pkg}")
			if [ "${BUILD_TARGET}" = "musl" ]; then
				pkgs+=("${MUSL_YUM_PKGS[@]}")
			fi
			run_as_root yum install -y "${pkgs[@]}"
			;;
		zypper)
			pkgs=("${DEFAULT_ZYPPER_PKGS[@]}")
			header_pkg="$(zypper_install_headers_pkg)"
			pkgs+=("${header_pkg}")
			if [ "${BUILD_TARGET}" = "musl" ]; then
				pkgs+=("${MUSL_ZYPPER_PKGS[@]}")
			fi
			run_as_root zypper --non-interactive refresh
			run_as_root zypper --non-interactive install "${pkgs[@]}"
			;;
		pacman)
			pkgs=("${DEFAULT_PACMAN_PKGS[@]}")
			header_pkg="$(pacman_install_headers_pkg)"
			pkgs+=("${header_pkg}")
			if [ "${BUILD_TARGET}" = "musl" ]; then
				pkgs+=("${MUSL_PACMAN_PKGS[@]}")
			fi
			run_as_root pacman -Sy --noconfirm
			run_as_root pacman -S --noconfirm "${pkgs[@]}"
			;;
		tinycore)
			pkgs=("${DEFAULT_TINYCORE_PKGS[@]}")
			if [ "${BUILD_TARGET}" = "musl" ]; then
				pkgs+=("${MUSL_TINYCORE_PKGS[@]}")
			fi
			install_tinycore_packages "${pkgs[@]}" || true
			;;
		*)
			echo "Unsupported package manager; skipping automatic dependency installation." >&2
			echo "Known: apt, apk, dnf, yum, zypper, pacman, tinycore (tce-load)." >&2
			echo "Install equivalent packages manually, then rerun the script." >&2
			return
			;;
	esac
}

require_command() {
	local cmd=$1
	if ! command -v "$cmd" >/dev/null 2>&1; then
		echo "Missing required command: ${cmd}" >&2
		exit 1
	fi
}

ensure_built() {
	if [ ! -x "${APP_BIN}" ]; then
		return 1
	fi

	if [ ! -f "${ROOT_DIR}/sys/myass.ko" ]; then
		return 1
	fi

	return 0
}

build_project() {
	pushd "${ROOT_DIR}" >/dev/null
	if [ "${BUILD_TARGET}" = "musl" ]; then
		echo "Building musl binary and module..."
		MUSL_CC="${MUSL_CC}" make musl
	else
		echo "Building default binary and module..."
		make
	fi
	popd >/dev/null
}

run_install_driver() {
	local -a run_binary
	local -a shell_cmd

	if [ "$(id -u)" -eq 0 ]; then
		run_binary=( "${APP_BIN}" )
	else
		require_command sudo
		run_binary=( sudo "${APP_BIN}" )
	fi

	echo "Running ${run_binary[*]} ${APP_FLAGS[*]}"

	if [ -t 0 ] && [ -t 1 ]; then
		"${run_binary[@]}" "${APP_FLAGS[@]}"
		return
	fi

	if command -v script >/dev/null 2>&1; then
		# script provides a pseudo-tty so the app can accept the explicit confirmation.
		# ehhh once again, just in cas .
		chmod +x ${APP_BIN}
		shell_cmd=( "${run_binary[@]}" "${APP_FLAGS[@]}" )
		printf 'INSTALL MYASS\n' | script -q -c "cd '${ROOT_DIR}' && ${shell_cmd[*]}" /dev/null
		return
	fi

	echo "Non-interactive terminal detected and 'script' is unavailable." >&2
	echo "Please run as interactive shell and execute: ${run_binary[*]} ${APP_FLAGS[*]}" >&2
	exit 1
}

main() {
	while [ $# -gt 0 ]; do
		case "$1" in
			--musl)
				use_musl=1
				shift
				;;
			-h|--help)
				usage
				exit 0
				;;
			*)
				echo "Unknown argument: $1" >&2
				usage
				exit 1
				;;
		esac
	done

	select_build_profile
	detect_package_manager
	install_dependencies "${PACKAGE_MANAGER}"

	require_command make
	require_command gcc
	if [ "${BUILD_TARGET}" = "musl" ]; then
		if command -v musl-gcc >/dev/null 2>&1; then
			MUSL_CC="musl-gcc"
		elif command -v gcc >/dev/null 2>&1; then
			MUSL_CC="gcc"
			echo "musl-gcc not found; falling back to gcc for MUSL build." >&2
		else
			echo "Musl build requested but no compiler is available." >&2
			exit 1
		fi
	fi
	require_command kmod
	if command -v dkms >/dev/null 2>&1; then
		:
	else
		echo "DKMS not found; installer will proceed with the app's legacy module fallback if possible." >&2
	fi
	if ! command -v script >/dev/null 2>&1; then
		echo "Warning: 'script' not found; non-interactive confirmation flow is unavailable." >&2
	fi

	if ! ensure_built; then
		build_project
	fi

	run_install_driver
}

main "$@"
