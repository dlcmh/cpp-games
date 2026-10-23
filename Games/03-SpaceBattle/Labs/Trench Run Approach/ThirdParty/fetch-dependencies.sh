#!/usr/bin/env bash
# Fetch the exact third-party versions Barebones builds against.
# Everything is cached outside the repository, so the checkout stays small.
set -euo pipefail

CACHE_DIR="${1:-${CPP_GAMES_DEPS_DIR:-${HOME}/Library/Caches/cpp-games/deps}}"
DOWNLOADS_DIR="${CACHE_DIR}/downloads"

SDL_VERSION="3.4.18"
SDL_DMG_SHA256="c1adf3da91108dbc3e4205da5dacebc203085d283fa89e86eb2c889abd03a853"
SDL_DMG_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.dmg"

METAL_CPP_TAG="release/metal-cpp_macOS26.4_iOS26.4"
METAL_CPP_SHA256="65a1a2ec3490eac7c5fa16039e1f92471ed24c1912109b0c9591850e74d0c790"
METAL_CPP_URL="https://github.com/apple/metal-cpp/archive/refs/tags/${METAL_CPP_TAG}.tar.gz"

SLANG_VERSION="2026.20"
SLANG_SHA256="ede1865bf086442c8786c5b7c0470ce5f4fd4f032eefe881356726ff9c445f40"

case "$(uname -m)" in
    arm64)
        SLANG_ARCH="aarch64"
        ;;
    x86_64)
        SLANG_ARCH="x86_64"
        ;;
    *)
        echo "Unsupported macOS architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

SLANG_ZIP="slang-${SLANG_VERSION}-macos-${SLANG_ARCH}.zip"
SLANG_URL="https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/${SLANG_ZIP}"

mkdir -p "${DOWNLOADS_DIR}"

log() {
    printf '[deps] %s\n' "$*"
}

verify_sha256() {
    local file="$1"
    local expected="$2"
    local actual
    actual="$(shasum -a 256 "${file}" | awk '{print $1}')"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "Checksum mismatch for ${file}" >&2
        echo "  expected ${expected}" >&2
        echo "  actual   ${actual}" >&2
        return 1
    fi
}

download() {
    local url="$1"
    local destination="$2"
    local checksum="$3"

    if [[ -f "${destination}" ]] && verify_sha256 "${destination}" "${checksum}" 2>/dev/null; then
        return 0
    fi

    rm -f "${destination}" "${destination}.part"
    log "Downloading $(basename "${destination}")"
    curl --fail --location --silent --show-error \
        --retry 3 --retry-delay 1 \
        --output "${destination}.part" "${url}"
    verify_sha256 "${destination}.part" "${checksum}"
    mv "${destination}.part" "${destination}"
}

# ---------------------------------------------------------------------------
# SDL3 (prebuilt universal framework)
# ---------------------------------------------------------------------------
if [[ ! -f "${CACHE_DIR}/sdl3/SDL3.framework/Headers/SDL.h" ]]; then
    SDL_DMG="${DOWNLOADS_DIR}/SDL3-${SDL_VERSION}.dmg"
    download "${SDL_DMG_URL}" "${SDL_DMG}" "${SDL_DMG_SHA256}"

    MOUNT_POINT="$(mktemp -d "${TMPDIR:-/tmp}/barebones-sdl.XXXXXX")"
    cleanup_mount() {
        hdiutil detach "${MOUNT_POINT}" -quiet >/dev/null 2>&1 || true
        rmdir "${MOUNT_POINT}" >/dev/null 2>&1 || true
    }
    trap cleanup_mount EXIT

    log "Extracting SDL3.framework"
    hdiutil attach "${SDL_DMG}" -nobrowse -readonly -quiet -mountpoint "${MOUNT_POINT}"
    rm -rf "${CACHE_DIR}/sdl3"
    mkdir -p "${CACHE_DIR}/sdl3"
    ditto "${MOUNT_POINT}/SDL3.xcframework/macos-arm64_x86_64/SDL3.framework" \
        "${CACHE_DIR}/sdl3/SDL3.framework"

    cleanup_mount
    trap - EXIT
fi

# ---------------------------------------------------------------------------
# metal-cpp (header-only)
# ---------------------------------------------------------------------------
if [[ ! -f "${CACHE_DIR}/metal-cpp/Metal/Metal.hpp" ]]; then
    METAL_CPP_ARCHIVE="${DOWNLOADS_DIR}/metal-cpp.tar.gz"
    download "${METAL_CPP_URL}" "${METAL_CPP_ARCHIVE}" "${METAL_CPP_SHA256}"

    log "Extracting metal-cpp"
    rm -rf "${CACHE_DIR}/metal-cpp"
    mkdir -p "${CACHE_DIR}/metal-cpp"
    tar -xzf "${METAL_CPP_ARCHIVE}" \
        --strip-components=1 \
        -C "${CACHE_DIR}/metal-cpp"
fi

# ---------------------------------------------------------------------------
# Slang compiler (build-time only)
# ---------------------------------------------------------------------------
if [[ ! -x "${CACHE_DIR}/slang/bin/slangc" ]]; then
    SLANG_ARCHIVE="${DOWNLOADS_DIR}/${SLANG_ZIP}"
    download "${SLANG_URL}" "${SLANG_ARCHIVE}" "${SLANG_SHA256}"

    log "Extracting Slang ${SLANG_VERSION}"
    rm -rf "${CACHE_DIR}/slang"
    mkdir -p "${CACHE_DIR}/slang"
    unzip -q "${SLANG_ARCHIVE}" -d "${CACHE_DIR}/slang"
    chmod +x "${CACHE_DIR}/slang/bin/slangc"
fi

log "Dependencies ready in ${CACHE_DIR}"
