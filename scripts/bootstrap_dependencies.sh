#!/usr/bin/env bash
set -euo pipefail

readonly FTXUI_REVISION="c100eab535db2283b78d30fcb6d082a1f84fb683"
readonly ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly FTXUI_DIR="${ROOT}/.deps/ftxui"
readonly DEFAULT_PREVIEW_ROOT="${ROOT}/third_party/preview_lib"
readonly PREVIEW_ROOT="${PREVIEW_ROOT:-${DEFAULT_PREVIEW_ROOT}}"

case "$(uname -s):$(uname -m)" in
  Linux:x86_64|Darwin:arm64) ;;
  *)
    printf 'supported hosts: Linux x86_64, macOS arm64\n' >&2
    exit 1
    ;;
esac

if [[ "${PREVIEW_ROOT}" == "${DEFAULT_PREVIEW_ROOT}" &&
      ! -x "${PREVIEW_ROOT}/scripts/bootstrap_dependencies.sh" &&
      -f "${ROOT}/.gitmodules" ]]; then
  git -C "${ROOT}" submodule update --init --recursive -- \
    third_party/preview_lib
fi

if [[ ! -x "${PREVIEW_ROOT}/scripts/bootstrap_dependencies.sh" ]]; then
  printf 'preview_lib was not found at %s\n' "${PREVIEW_ROOT}" >&2
  printf 'initialize the submodule or set PREVIEW_ROOT explicitly\n' >&2
  exit 1
fi

mkdir -p "${ROOT}/.deps"

if [[ ! -d "${FTXUI_DIR}/.git" ]]; then
  git clone --filter=blob:none --no-checkout \
    https://github.com/ArthurSonzogni/FTXUI.git "${FTXUI_DIR}"
fi

git -C "${FTXUI_DIR}" fetch --depth 1 origin "${FTXUI_REVISION}"
git -C "${FTXUI_DIR}" checkout --detach "${FTXUI_REVISION}"

actual_revision="$(git -C "${FTXUI_DIR}" rev-parse HEAD)"
if [[ "${actual_revision}" != "${FTXUI_REVISION}" ]]; then
  printf 'Unexpected FTXUI revision: %s\n' "${actual_revision}" >&2
  exit 1
fi

printf 'FTXUI is ready at %s\n' "${FTXUI_REVISION}"

"${PREVIEW_ROOT}/scripts/bootstrap_dependencies.sh"
