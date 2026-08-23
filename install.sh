#!/usr/bin/env bash
set -euo pipefail

readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly build_dir="${BUILD_DIR:-${ROOT}/build}"
prefix="${DUWIZ_INSTALL_PREFIX:-${HOME}/.local}"

usage() {
  printf 'Usage: %s [--prefix PATH]\n' "${0##*/}"
  printf 'Build and install duwiz (default: %s).\n' "${HOME}/.local"
}

while (($# > 0)); do
  case "$1" in
    --prefix)
      if (($# < 2)); then
        printf '%s: --prefix requires a path\n' "${0##*/}" >&2
        exit 2
      fi
      prefix="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf '%s: unknown option: %s\n' "${0##*/}" "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [[ -z "${PREVIEW_ROOT+x}" && -f "${ROOT}/.gitmodules" ]] &&
   git -C "${ROOT}" rev-parse --git-dir >/dev/null 2>&1; then
  git -C "${ROOT}" submodule update --init --recursive -- \
    third_party/preview_lib
fi

"${ROOT}/scripts/bootstrap_dependencies.sh"
BUILD_DIR="${build_dir}" "${ROOT}/scripts/configure.sh"
cmake --build "${build_dir}" -j
cmake --install "${build_dir}" --prefix "${prefix}"

readonly bin_dir="${prefix}/bin"
printf '\nduwiz was installed at %s/duwiz\n' "${bin_dir}"

case ":${PATH:-}:" in
  *":${bin_dir}:"*)
    printf 'Run it with: duwiz\n'
    ;;
  *)
    printf '%s is not currently in PATH. Add this line to your shell profile:\n' \
      "${bin_dir}"
    printf '  export PATH="%s:$PATH"\n' "${bin_dir}"
    ;;
esac
