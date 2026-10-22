#!/usr/bin/env bash
# Fetch a pinned clang-tidy binary. The macOS Command Line Tools do not ship
# clang-tidy, so this keeps local linting available without Homebrew or CI.
set -euo pipefail

CACHE_DIR="${1:-${CPP_GAMES_TOOLS_DIR:-${HOME}/Library/Caches/cpp-games/tools}}"
DOWNLOAD_DIR="${CACHE_DIR}/downloads"
INSTALL_DIR="${CACHE_DIR}/clang-tidy"
TIDY_BIN="${INSTALL_DIR}/clang_tidy/data/bin/clang-tidy"
VERSION="23.1.3"

case "$(uname -m)" in
    arm64)
        WHEEL="clang_tidy-${VERSION}-py2.py3-none-macosx_11_0_arm64.whl"
        SHA256="cbd31dea271cee7ed5b5048847e73b8208c8dde90e321e09c20d9146011f8ff2"
        ;;
    x86_64)
        WHEEL="clang_tidy-${VERSION}-py2.py3-none-macosx_10_9_x86_64.whl"
        SHA256="6d745b16e375bfc2219bca5f6697dc17348313f6b1fc58f8fa7321172a59ecdf"
        ;;
    *)
        echo "Unsupported macOS architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

URL="https://files.pythonhosted.org/packages"
case "$(uname -m)" in
    arm64)
        URL="${URL}/26/42/65a2c5ee9be88b81d3890df15fd1af7f5bbab72a16ccf4d302454385e067/${WHEEL}"
        ;;
    x86_64)
        URL="${URL}/f0/a6/b5b34b1770cc50aa216d33a45456c1a69375dee40a0abad91ef2656d570c/${WHEEL}"
        ;;
esac

if [[ -x "${TIDY_BIN}" ]]; then
    printf '%s\n' "${TIDY_BIN}"
    exit 0
fi

mkdir -p "${DOWNLOAD_DIR}"
ARCHIVE="${DOWNLOAD_DIR}/${WHEEL}"

if [[ ! -f "${ARCHIVE}" ]] || \
   [[ "$(shasum -a 256 "${ARCHIVE}" | awk '{print $1}')" != "${SHA256}" ]]; then
    rm -f "${ARCHIVE}" "${ARCHIVE}.part"
    printf '[tools] Downloading clang-tidy %s\n' "${VERSION}" >&2
    curl --fail --location --silent --show-error \
        --retry 3 --retry-delay 1 \
        --output "${ARCHIVE}.part" "${URL}"
    ACTUAL="$(shasum -a 256 "${ARCHIVE}.part" | awk '{print $1}')"
    if [[ "${ACTUAL}" != "${SHA256}" ]]; then
        rm -f "${ARCHIVE}.part"
        echo "Checksum mismatch for ${WHEEL}" >&2
        exit 1
    fi
    mv "${ARCHIVE}.part" "${ARCHIVE}"
fi

printf '[tools] Extracting clang-tidy %s\n' "${VERSION}" >&2
rm -rf "${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}"
unzip -q "${ARCHIVE}" -d "${INSTALL_DIR}"
chmod +x "${TIDY_BIN}"

printf '%s\n' "${TIDY_BIN}"
