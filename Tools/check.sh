#!/usr/bin/env bash
# Format, clean up includes, and compile-check every game and lab locally.
#
# Default mode checks all C++ sources without modifying them. The pre-commit
# hook calls `--staged --fix`, which formats and fixes only the staged games,
# re-stages the result, then verifies everything before allowing the commit.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPS_DIR="${CPP_GAMES_DEPS_DIR:-${HOME}/Library/Caches/cpp-games/deps}"
TOOLS_DIR="${CPP_GAMES_TOOLS_DIR:-${HOME}/Library/Caches/cpp-games/tools}"
CHECK_ROOT="${CPP_GAMES_CHECK_BUILD_ROOT:-/tmp/cpp-games-check}"

MODE="all"
FIX=0

usage() {
    cat <<'EOF'
Usage: Tools/check.sh [options]

Options:
  --all       Check all files in every game and lab (default).
  --staged    Check only files staged for the next commit.
  --fix       Apply clang-format and include-cleaner fixes, then re-stage
              files when running with --staged.
  -h, --help  Show this help.

Environment:
  CPP_GAMES_DEPS_DIR         Build dependency cache (default: ~/Library/Caches/cpp-games/deps)
  CPP_GAMES_TOOLS_DIR        clang-tidy cache (default: ~/Library/Caches/cpp-games/tools)
  CPP_GAMES_CHECK_BUILD_ROOT Check build root (default: /tmp/cpp-games-check)
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

# Anything that can affect compilation, including shaders and build files.
is_build_input() {
    is_cxx_file "$1" && return 0
    case "$1" in
        *.slang|CMakeLists.txt|*/CMakeLists.txt) return 0 ;;
        *) return 1 ;;
    esac
}

PROJECTS=()
while IFS= read -r cmake_file; do
    [[ -n "${cmake_file}" ]] && PROJECTS+=("$(dirname "${cmake_file}")")
done < <(find "${ROOT}/Games" -mindepth 2 -maxdepth 5 -name CMakeLists.txt -print | sort)

if [[ "${#PROJECTS[@]}" -eq 0 ]]; then
    echo "[check] No game or lab projects found."
    exit 0
fi

CANDIDATES=()
if [[ "${MODE}" == "staged" ]]; then
    while IFS= read -r path; do
        [[ -n "${path}" ]] && CANDIDATES+=("${path}")
    done < <(git -C "${ROOT}" diff --cached --name-only --diff-filter=ACMR -- Games Engine)
else
    while IFS= read -r path; do
        [[ -n "${path}" ]] && CANDIDATES+=("${path}")
    done < <(git -C "${ROOT}" ls-files -- Games Engine)
fi

CXX_FILES=()
TIDY_FILES=()
BUILD_PROJECTS=()
ENGINE_CHANGED=0
ENGINE_PROJECT="${PROJECTS[0]}"

for project in "${PROJECTS[@]}"; do
    case "${project}" in
        */Games/03-SpaceBattle) ENGINE_PROJECT="${project}" ;;
    esac
done

if [[ "${#CANDIDATES[@]}" -gt 0 ]]; then
    for relative in "${CANDIDATES[@]}"; do
        case "${relative}" in
            Engine/*) ENGINE_CHANGED=1 ;;
        esac
    done
fi

if [[ "${MODE}" == "all" ]]; then
    BUILD_PROJECTS=("${PROJECTS[@]}")
else
    if [[ "${ENGINE_CHANGED}" -eq 1 ]]; then
        # Shared modules affect every consumer; rebuild all discoverable apps.
        BUILD_PROJECTS=("${PROJECTS[@]}")
    else
    for project in "${PROJECTS[@]}"; do
        project_relative="${project#"${ROOT}/"}"
        project_needs_build=0
        if [[ "${#CANDIDATES[@]}" -gt 0 ]]; then
            for relative in "${CANDIDATES[@]}"; do
                case "${relative}" in
                    "${project_relative}"/*)
                        if is_build_input "${relative}"; then
                            project_needs_build=1
                            break
                        fi
                        ;;
                esac
            done
        fi
        if [[ "${project_needs_build}" -eq 1 ]]; then
            BUILD_PROJECTS+=("${project}")
        fi
    done
    fi
fi

if [[ "${MODE}" == "staged" ]] && [[ "${#BUILD_PROJECTS[@]}" -eq 0 ]]; then
    echo "[check] No source or build changes staged; skipping."
    exit 0
fi

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
    done
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

project_build_dir() {
    local relative="${1#"${ROOT}/"}"
    printf '%s/%s\n' "${CHECK_ROOT}" "${relative//\//-}"
}

file_belongs_to_project() {
    local file="$1"
    local project="$2"
    local owner=""

    for candidate in "${PROJECTS[@]}"; do
        case "${file}" in
            "${candidate}"/*)
                if [[ "${#candidate}" -gt "${#owner}" ]]; then
                    owner="${candidate}"
                fi
                ;;
        esac
    done
    if [[ -z "${owner}" ]] && [[ "${project}" == "${ENGINE_PROJECT}" ]]; then
        case "${file}" in
            "${ROOT}/Engine"/*) owner="${project}" ;;
        esac
    fi
    [[ "${owner}" == "${project}" ]]
}

build_project() {
    local project="$1"
    local build_dir
    build_dir="$(project_build_dir "${project}")"
    "${project}/ThirdParty/fetch-dependencies.sh" "${DEPS_DIR}" >/dev/null
    cmake \
        -S "${project}" \
        -B "${build_dir}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCPP_GAMES_DEPS_DIR="${DEPS_DIR}" \
        -DCPP_GAMES_WERROR=ON \
        >/dev/null
    cmake --build "${build_dir}" --parallel
}

echo "[check] Building ${#BUILD_PROJECTS[@]} project(s) with warnings as errors..."
for project in "${BUILD_PROJECTS[@]}"; do
    build_project "${project}"
done

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
        for project in "${BUILD_PROJECTS[@]}"; do
            build_dir="$(project_build_dir "${project}")"
            for file in "${TIDY_FILES[@]}"; do
                if file_belongs_to_project "${file}" "${project}"; then
                    "${TIDY_BIN}" -p "${build_dir}" "${file}" \
                        --config-file="${ROOT}/.clang-tidy" \
                        --header-filter='^$' \
                        --fix \
                        --quiet
                fi
            done
        done
    fi

    # Include fixes can affect line wrapping, so format once more.
    if [[ "${#CXX_FILES[@]}" -gt 0 ]]; then
        "${CLANG_FORMAT}" -i "${CXX_FILES[@]}"
    fi

    echo "[check] Rebuilding after fixes..."
    for project in "${BUILD_PROJECTS[@]}"; do
        cmake --build "$(project_build_dir "${project}")" --parallel
    done
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
    for project in "${BUILD_PROJECTS[@]}"; do
        build_dir="$(project_build_dir "${project}")"
        for file in "${TIDY_FILES[@]}"; do
            if file_belongs_to_project "${file}" "${project}"; then
                "${TIDY_BIN}" -p "${build_dir}" "${file}" \
                    --config-file="${ROOT}/.clang-tidy" \
                    --header-filter='^$' \
                    --warnings-as-errors='*' \
                    --quiet
            fi
        done
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
