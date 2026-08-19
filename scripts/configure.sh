#!/usr/bin/env bash
set -euo pipefail

readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
readonly PREVIEW_ROOT="${PREVIEW_ROOT:-${ROOT}/third_party/preview_lib}"
readonly PDFIUM_ROOT="${PREVIEW_ROOT}/.deps/pdfium-work/pdfium"
cache="${BUILD_DIR}/CMakeCache.txt"
fresh=()

case "$(uname -s):$(uname -m)" in
  Linux:x86_64)
    compiler="${PDFIUM_ROOT}/third_party/llvm-build/Release+Asserts/bin/clang++"
    if ! command -v g++ >/dev/null 2>&1; then
      printf 'g++ is required to locate the host libstdc++ installation.\n' >&2
      exit 1
    fi
    gcc_install_dir="$(dirname "$(g++ -print-file-name=libstdc++.so)")"
    platform_args=(
      -DCMAKE_CXX_FLAGS="--gcc-install-dir=${gcc_install_dir}"
      -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
      -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
    )
    ;;
  Darwin:arm64)
    if ! command -v xcrun >/dev/null 2>&1 ||
       ! command -v xcodebuild >/dev/null 2>&1 ||
       ! xcodebuild -version >/dev/null 2>&1; then
      printf 'Full Xcode is required; Command Line Tools alone are insufficient.\n' >&2
      printf 'Select Xcode with:\n' >&2
      printf '  sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer\n' >&2
      exit 1
    fi
    compiler="${PDFIUM_ROOT}/third_party/llvm-build/Release+Asserts/bin/clang++"
    platform_args=(
      -DCMAKE_OSX_ARCHITECTURES=arm64
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
      -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)"
      -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
      -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld
    )
    ;;
  *)
    printf 'supported hosts: Linux x86_64, macOS arm64\n' >&2
    exit 1
    ;;
esac

if [[ ! -x "${compiler}" || ! -x "${PDFIUM_ROOT}/third_party/ninja/ninja" ]]; then
  printf 'PDFium toolchain is missing; run scripts/bootstrap_dependencies.sh.\n' >&2
  exit 1
fi

if [[ -f "${cache}" ]]; then
  cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${cache}")"
  cached_compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "${cache}")"
  if [[ "${cached_source}" != "${ROOT}" ||
        "${cached_compiler}" != "${compiler}" ]]; then
    printf 'Refreshing a relocated or incompatible CMake cache.\n'
    fresh=(--fresh)
  fi
fi

cmake "${fresh[@]}" -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DPREVIEW_ROOT="${PREVIEW_ROOT}" \
  -DCMAKE_MAKE_PROGRAM="${PDFIUM_ROOT}/third_party/ninja/ninja" \
  -DCMAKE_C_COMPILER="${PDFIUM_ROOT}/third_party/llvm-build/Release+Asserts/bin/clang" \
  -DCMAKE_CXX_COMPILER="${compiler}" \
  "${platform_args[@]}"
