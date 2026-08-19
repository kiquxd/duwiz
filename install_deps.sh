#!/usr/bin/env bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # no color

set -euo pipefail

install_macos() {
    if ! command -v xcodebuild >/dev/null 2>&1 ||
       ! xcodebuild -version >/dev/null 2>&1; then
        printf "${RED}ERROR: full Xcode with an active macOS SDK is required.\n" >&2
        printf "Install Xcode, then run:\n" >&2
        printf "  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer\n" >&2
        printf "  sudo xcodebuild -runFirstLaunch\n" >&2
        exit 1
    fi

    if ! command -v brew >/dev/null 2>&1; then
        echo "ERROR: Homebrew isn't installed: https://brew.sh" >&2
        exit 1
    fi

    brew update
    brew install cmake git
}

install_linux() {
    if [[ $EUID -eq 0 ]]; then
        SUDO=()
    elif command -v sudo >/dev/null 2>&1; then
        SUDO=(sudo)
    else
        printf "${RED}ERROR: root user or sudo required.\n" >&2
        exit 1
    fi

    if command -v apt-get >/dev/null 2>&1; then
        "${SUDO[@]}" apt-get update
        "${SUDO[@]}" apt-get install -y \
            build-essential ca-certificates cmake curl git libfontconfig1 python3 xz-utils
    elif command -v dnf >/dev/null 2>&1; then
        "${SUDO[@]}" dnf install -y \
            ca-certificates cmake curl fontconfig gcc-c++ git python3 xz
    elif command -v pacman >/dev/null 2>&1; then
        "${SUDO[@]}" pacman -Syu --needed --noconfirm \
            base-devel ca-certificates cmake curl fontconfig git python xz
    elif command -v zypper >/dev/null 2>&1; then
        "${SUDO[@]}" zypper --non-interactive install \
            ca-certificates cmake curl fontconfig gcc-c++ git python3 xz
    elif command -v apk >/dev/null 2>&1; then
        "${SUDO[@]}" apk add \
            build-base ca-certificates cmake curl fontconfig git python3 xz
    else
        printf "${RED}ERROR: unsupported package manager.\n" >&2
        printf "${RED}Supported ones: apt, dnf, pacman, zypper, apk.\n" >&2
        exit 1
    fi
}

case "$(uname -s):$(uname -m)" in
    Darwin:arm64) install_macos ;;
    Linux:x86_64) install_linux ;;
    *)
        printf "${RED}ERROR: unsupported system or architecture.\n" >&2
        printf "Only Linux x86-64 and macOS arm64 are supported.\n" >&2
        exit 1
        ;;
esac

printf "${GREEN}Dependencies have been successfully installed.${NC}\n"
