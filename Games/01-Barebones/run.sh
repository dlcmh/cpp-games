#!/usr/bin/env bash
# Build Barebones into a disposable directory and launch it.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

DEPS_DIR="${CPP_GAMES_DEPS_DIR:-${HOME}/Library/Caches/cpp-games/deps}"
BUILD_DIR="${BAREBONES_BUILD_DIR:-/tmp/cpp-games-barebones-build}"
BUILD_ONLY=0
STATS=0

usage() {
    cat <<'EOF'
Usage: ./run.sh [options]

Options:
  --build-only   Build the app bundle without launching it.
  --clean        Delete the previous build directory first.
  --stats        Print CPU and memory usage every second while running.
  -h, --help     Show this help.

Environment:
  CPP_GAMES_DEPS_DIR   Dependency cache (default: ~/Library/Caches/cpp-games/deps)
  BAREBONES_BUILD_DIR  Build directory (default: /tmp/cpp-games-barebones-build)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build-only)
            BUILD_ONLY=1
            ;;
        --clean)
            rm -rf "${BUILD_DIR}"
            ;;
        --stats)
            STATS=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
    shift
done

"${REPO_ROOT}/Tools/install-hooks.sh"
"${SCRIPT_DIR}/ThirdParty/fetch-dependencies.sh" "${DEPS_DIR}"

cmake \
    -S "${SCRIPT_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCPP_GAMES_DEPS_DIR="${DEPS_DIR}"

cmake --build "${BUILD_DIR}" --parallel

# clangd discovers the nearest compile database automatically.
if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
    cp "${BUILD_DIR}/compile_commands.json" "${SCRIPT_DIR}/compile_commands.json"
fi

APP="${BUILD_DIR}/Barebones.app"
if [[ ! -d "${APP}" ]]; then
    echo "Build succeeded, but ${APP} was not found." >&2
    exit 1
fi

if [[ "${BUILD_ONLY}" -eq 1 ]]; then
    echo "Built ${APP}"
    exit 0
fi

if [[ "${STATS}" -eq 1 ]]; then
    exec "${REPO_ROOT}/Tools/run-with-stats.sh" "${APP}/Contents/MacOS/Barebones"
fi

exec "${APP}/Contents/MacOS/Barebones"
