#!/usr/bin/env bash
# Build and launch one of the shader examples: ./run.sh 1 or ./run.sh 2.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

DEPS_DIR="${CPP_GAMES_DEPS_DIR:-${HOME}/Library/Caches/cpp-games/deps}"
BUILD_DIR="${SHADERS_BUILD_DIR:-/tmp/cpp-games-shaders-build}"
EXAMPLE=1
BUILD_ONLY=0

usage() {
    cat <<'USAGE'
Usage: ./run.sh [1|2] [--build-only]

Examples:
  1  Rectangle outline
  2  Filled rectangle

Environment:
  CPP_GAMES_DEPS_DIR  Dependency cache (default: ~/Library/Caches/cpp-games/deps)
  SHADERS_BUILD_DIR   Build directory (default: /tmp/cpp-games-shaders-build)
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        1|2)
            EXAMPLE="$1"
            ;;
        --build-only)
            BUILD_ONLY=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

"${REPO_ROOT}/Tools/install-hooks.sh"
"${SCRIPT_DIR}/ThirdParty/fetch-dependencies.sh" "${DEPS_DIR}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCPP_GAMES_DEPS_DIR="${DEPS_DIR}"
cmake --build "${BUILD_DIR}" --parallel

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
