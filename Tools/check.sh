#!/usr/bin/env bash
# Format, clean up includes, and compile-check the game locally.
#
# Default mode checks all C++ sources without modifying them. The pre-commit
# hook calls `--staged --fix`, which formats and fixes only the staged files,
# re-stages the result, then verifies everything before allowing the commit.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="${ROOT}/Games/01-Barebones"
PROJECT_REL="Games/01-Barebones"
DEPS_DIR="${CPP_GAMES_DEPS_DIR:-/tmp/cpp-games-deps}"
TOOLS_DIR="${CPP_GAMES_TOOLS_DIR:-/tmp/cpp-games-tools}"
BUILD_DIR="${BAREBONES_CHECK_BUILD_DIR:-/tmp/cpp-games-barebones-check}"

MODE="all"
FIX=0

usage() {
    cat <<'EOF'
Usage: Tools/check.sh [options]

Options:
  --all       Check every C++ source in the game (default).
  --staged    Check only files staged for the next commit.
  --fix       Apply clang-format and include-cleaner fixes, then re-stage
              files when running with --staged.
  -h, --help  Show this help.

Environment:
  CPP_GAMES_DEPS_DIR        Build dependency cache (default: /tmp/cpp-games-deps)
  CPP_GAMES_TOOLS_DIR       clang-tidy cache (default: /tmp/cpp-games-tools)
  BAREBONES_CHECK_BUILD_DIR Check build directory (default: /tmp/cpp-games-barebones-check)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)
            MODE="all"
            ;;
        --staged)
            MODE="staged"
            ;;
        --fix)
            FIX=1
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

if [[ "${MODE}" == "staged" ]] && ! git -C "${ROOT}" rev-parse --git-dir >/dev/null 2>&1; then
    echo "check: --staged requires a git repository" >&2
    exit 1
fi

command -v xcrun >/dev/null 2>&1 || {
    echo "check: xcrun is required (install Xcode Command Line Tools)" >&2
    exit 1
}
CLANG_FORMAT="${CLANG_FORMAT:-$(xcrun --find clang-format 2>/dev/null || true)}"
if [[ -z "${CLANG_FORMAT}" ]]; then
    CLANG_FORMAT="$(command -v clang-format || true)"
fi
if [[ -z "${CLANG_FORMAT}" ]]; then
    echo "check: clang-format is required (install Xcode Command Line Tools)" >&2
    exit 1
fi

is_cxx_file() {
    case "$1" in
        *.cpp|*.cc|*.cxx|*.mm|*.m|*.h|*.hh|*.hpp|*.hxx) return 0 ;;
        *) return 1 ;;
    esac
}

is_tidy_file() {
    case "$1" in
        *.cpp|*.cc|*.cxx) return 0 ;;
        *) return 1 ;;
    esac
}

# Anything that can affect compilation, including the shader and build files.
is_build_input() {
    is_cxx_file "$1" && return 0
    case "$1" in
        *.slang|CMakeLists.txt) return 0 ;;
        *) return 1 ;;
    esac
}

CANDIDATES=()
if [[ "${MODE}" == "staged" ]]; then
    while IFS= read -r path; do
        [[ -n "${path}" ]] && CANDIDATES+=("${path}")
    done < <(git -C "${ROOT}" diff --cached --name-only --diff-filter=ACMR -- "${PROJECT_REL}")
else
    while IFS= read -r path; do
        [[ -n "${path}" ]] && CANDIDATES+=("${path}")
    done < <(git -C "${ROOT}" ls-files -- "${PROJECT_REL}")
fi

CXX_FILES=()
TIDY_FILES=()
BUILD_NEEDED=0
if [[ "${#CANDIDATES[@]}" -gt 0 ]]; then
    for relative in "${CANDIDATES[@]}"; do
        absolute="${ROOT}/${relative}"
        [[ -f "${absolute}" ]] || continue
        if is_cxx_file "${relative}"; then
            CXX_FILES+=("${absolute}")
        fi
        if is_tidy_file "${relative}"; then
            TIDY_FILES+=("${absolute}")
        fi
        if is_build_input "${relative}"; then
            BUILD_NEEDED=1
        fi
    done
fi

if [[ "${MODE}" == "staged" ]] && [[ "${BUILD_NEEDED}" -eq 0 ]]; then
    echo "[check] No source or build changes staged; skipping."
    exit 0
fi

if [[ "${BUILD_NEEDED}" -eq 0 ]]; then
    echo "[check] No source or build changes to check."
    exit 0
fi

if [[ "${FIX}" -eq 1 ]] && [[ "${MODE}" == "staged" ]] && [[ "${#CXX_FILES[@]}" -gt 0 ]]; then
    for file in "${CXX_FILES[@]}"; do
        relative="${file#"${ROOT}/"}"
        if ! git -C "${ROOT}" diff --quiet -- "${relative}"; then
            echo "check: ${relative} has unstaged changes." >&2
            echo "check: stage or stash them before using --fix, so fixes cannot mix with unrelated edits" >&2
            exit 1
        fi
    done
fi

echo "[check] Building with warnings as errors..."
"${PROJECT}/ThirdParty/fetch-dependencies.sh" "${DEPS_DIR}" >/dev/null
cmake \
    -S "${PROJECT}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCPP_GAMES_DEPS_DIR="${DEPS_DIR}" \
    -DBAREBONES_WERROR=ON \
    >/dev/null
cmake --build "${BUILD_DIR}" --parallel

if [[ "${FIX}" -eq 1 ]]; then
    if [[ "${#CXX_FILES[@]}" -gt 0 ]]; then
        echo "[check] Formatting ${#CXX_FILES[@]} file(s)..."
        "${CLANG_FORMAT}" -i "${CXX_FILES[@]}"
    fi

    if [[ "${#TIDY_FILES[@]}" -gt 0 ]]; then
        TIDY_BIN="$("${ROOT}/Tools/fetch-clang-tidy.sh" "${TOOLS_DIR}")"
        export SDKROOT
        SDKROOT="$(xcrun --show-sdk-path)"
        echo "[check] Cleaning includes..."
        for file in "${TIDY_FILES[@]}"; do
            "${TIDY_BIN}" -p "${BUILD_DIR}" "${file}" \
                --config-file="${ROOT}/.clang-tidy" \
                --header-filter='^$' \
                --fix \
                --quiet
        done
    fi

    # Include fixes can affect line wrapping, so format once more.
    if [[ "${#CXX_FILES[@]}" -gt 0 ]]; then
        "${CLANG_FORMAT}" -i "${CXX_FILES[@]}"
    fi

    echo "[check] Rebuilding after fixes..."
    cmake --build "${BUILD_DIR}" --parallel
fi

echo "[check] Verifying formatting..."
if [[ "${#CXX_FILES[@]}" -gt 0 ]]; then
    "${CLANG_FORMAT}" --dry-run --Werror "${CXX_FILES[@]}"
fi

if [[ "${#TIDY_FILES[@]}" -gt 0 ]]; then
    TIDY_BIN="$("${ROOT}/Tools/fetch-clang-tidy.sh" "${TOOLS_DIR}")"
    export SDKROOT
    SDKROOT="$(xcrun --show-sdk-path)"
    echo "[check] Verifying include hygiene..."
    for file in "${TIDY_FILES[@]}"; do
        "${TIDY_BIN}" -p "${BUILD_DIR}" "${file}" \
            --config-file="${ROOT}/.clang-tidy" \
            --header-filter='^$' \
            --warnings-as-errors='*' \
            --quiet
    done
fi

if [[ "${FIX}" -eq 1 ]] && [[ "${MODE}" == "staged" ]] && [[ "${#CXX_FILES[@]}" -gt 0 ]]; then
    STAGE_PATHS=()
    for file in "${CXX_FILES[@]}"; do
        STAGE_PATHS+=("${file#"${ROOT}/"}")
    done
    git -C "${ROOT}" add -- "${STAGE_PATHS[@]}"
    echo "[check] Formatting/include fixes applied and staged."
fi

echo "[check] OK."
