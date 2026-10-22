#!/usr/bin/env bash
# Point this repository at the tracked local hooks. Safe to run repeatedly.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

chmod +x .githooks/pre-commit Tools/check.sh Tools/fetch-clang-tidy.sh

current="$(git config --local --get core.hooksPath || true)"
if [[ "${current}" != ".githooks" ]]; then
    git config --local core.hooksPath .githooks
    echo "[hooks] Enabled local pre-commit checks (bypass with: git commit --no-verify)."
fi
