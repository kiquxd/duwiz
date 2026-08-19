#!/usr/bin/env bash
set -euo pipefail

readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly BUILD_DIR="${BUILD_DIR:-${ROOT}/build}"
readonly PREVIEW_ROOT="${PREVIEW_ROOT:-${ROOT}/third_party/preview_lib}"
cache="${BUILD_DIR}/CMakeCache.txt"
fresh=false

cmake_args=(-S "${ROOT}" -B "${BUILD_DIR}" -DPREVIEW_ROOT="${PREVIEW_ROOT}")
case "$(uname -s):$(uname -m)" in
  Linux:x86_64)
    if ! command -v c++ >/dev/null 2>&1; then
      printf 'A C++20 compiler is required.\n' >&2
      exit 1
    fi
    ;;
  Darwin:arm64)
    if ! command -v xcrun >/dev/null 2>&1 ||
       ! xcrun --find clang++ >/dev/null 2>&1; then
      printf 'Apple Command Line Tools are required; run: xcode-select --install\n' >&2
      exit 1
    fi
    cmake_args+=(
      -DCMAKE_OSX_ARCHITECTURES=arm64
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
    )
    ;;
  *)
    printf 'supported hosts: Linux x86-64, macOS arm64\n' >&2
    exit 1
    ;;
esac

if [[ -f "${cache}" ]]; then
  cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${cache}")"
  cached_make="$(sed -n 's/^CMAKE_MAKE_PROGRAM:[^=]*=//p' "${cache}")"
  cached_compiler="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "${cache}")"
  if [[ "${cached_source}" != "${ROOT}" ||
        "${cached_make}" == *pdfium-work* ||
        "${cached_compiler}" == *pdfium-work* ]]; then
    printf 'Refreshing a relocated or legacy CMake cache.\n'
    fresh=true
  fi
fi

if [[ "${fresh}" == true ]]; then
  cmake --fresh "${cmake_args[@]}"
else
  cmake "${cmake_args[@]}"
fi
