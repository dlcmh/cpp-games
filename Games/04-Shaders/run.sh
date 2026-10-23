#!/usr/bin/env bash
# Build and launch a shader example: ./run.sh 1 or ./run.sh unfilled-rectangle.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

DEPS_DIR="${CPP_GAMES_DEPS_DIR:-${HOME}/Library/Caches/cpp-games/deps}"
BUILD_DIR="${SHADERS_BUILD_DIR:-/tmp/cpp-games-shaders-build}"
EXAMPLE=1
BUILD_ONLY=0

usage() {
    cat <<'USAGE'
Usage: ./run.sh [example] [--build-only]

The example is the shader file name without .slang:
  ./run.sh 1
  ./run.sh unfilled-rectangle
  ./run.sh 2

Environment:
  CPP_GAMES_DEPS_DIR  Dependency cache (default: ~/Library/Caches/cpp-games/deps)
  SHADERS_BUILD_DIR   Build directory (default: /tmp/cpp-games-shaders-build)
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only)
            BUILD_ONLY=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            EXAMPLE="$1"
            ;;
    esac
    shift
done

if [[ ! -f "${SCRIPT_DIR}/Shaders/${EXAMPLE}.slang" ]]; then
    echo "No shader at ${SCRIPT_DIR}/Shaders/${EXAMPLE}.slang" >&2
    exit 1
fi

"${REPO_ROOT}/Tools/install-hooks.sh"
"${SCRIPT_DIR}/ThirdParty/fetch-dependencies.sh" "${DEPS_DIR}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCPP_GAMES_DEPS_DIR="${DEPS_DIR}"

cmake --build "${BUILD_DIR}" --target Shaders --parallel

if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
    cp "${BUILD_DIR}/compile_commands.json" "${SCRIPT_DIR}/compile_commands.json"
fi

APP="${BUILD_DIR}/Shaders.app"
if [[ ! -d "${APP}" ]]; then
    echo "Build succeeded, but ${APP} was not found." >&2
    exit 1
fi

if [[ "${BUILD_ONLY}" -eq 1 ]]; then
    echo "Built ${APP}"
    exit 0
fi

exec "${APP}/Contents/MacOS/Shaders" "${EXAMPLE}"
