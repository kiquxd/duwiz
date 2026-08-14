#!/usr/bin/env bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # no color

set -euo pipefail

if [[ $EUID -eq 0 ]]; then
    SUDO=()
elif command -v sudo >/dev/null 2>&1; then
    SUDO=(sudo)
else
    printf "${RED}ERROR: root user or sudo required." >&2
    exit 1
fi

install_macos() {
    if ! command -v brew >/dev/null 2>&1; then
        echo "ERROR: Homebrew isn't installed: https://brew.sh" >&2
        exit 1
    fi

    brew update
    brew install pkg-config ftxui glib chafa libmagic
}

install_linux() {
    if command -v apt-get >/dev/null 2>&1; then
        "${SUDO[@]}" apt-get update
        "${SUDO[@]}" apt-get install -y \
            pkg-config libftxui-dev libglib2.0-dev libchafa-dev libmagic-dev
    elif command -v dnf >/dev/null 2>&1; then
        "${SUDO[@]}" dnf install -y \
            pkgconf-pkg-config ftxui-devel glib2-devel chafa-devel file-devel
    elif command -v pacman >/dev/null 2>&1; then
        "${SUDO[@]}" pacman -Syu --needed --noconfirm \
            pkgconf ftxui glib2 chafa file
    elif command -v zypper >/dev/null 2>&1; then
        "${SUDO[@]}" zypper --non-interactive install \
            pkg-config ftxui-devel glib2-devel chafa-devel file-devel
    elif command -v apk >/dev/null 2>&1; then
        "${SUDO[@]}" apk add \
            pkgconf ftxui-dev glib-dev chafa-dev file-dev
    else
        printf "${RED}ERROR: unsupported package manager.\n" >&2
        printf "${RED}Supported ones: apt, dnf, pacman, zypper, apk.\n" >&2
        exit 1
    fi
}

case "$(uname -s)" in
    Darwin) install_macos ;;
    Linux)  install_linux ;;
    *)
        printf "${RED}ERROR: unsupported system.\n" >&2
        printf "${RED}Only macOS и Linux are supported for now.\n" >&2
        exit 1
        ;;
esac

printf "${GREEN}Dependencies have been successfully installed.\n"
