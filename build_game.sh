#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# macOS and Linux counterpart of the Windows launcher's BUILD GAME page: ./build_game.sh /path/to/doa3.iso
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON="python3"
VENV="${SCRIPT_DIR}/private/venv"
# Debian, Ubuntu and Homebrew refuse a global pip install, so the build keeps its own Capstone.
# Without the venv module the system python3 is used as before.
if [[ ! -f "${VENV}/bin/python3" ]]; then
    python3 -m venv "${VENV}" >/dev/null 2>&1 || rm -rf "${VENV}"
fi
if [[ -f "${VENV}/bin/python3" ]]; then
    PYTHON="${VENV}/bin/python3"
fi
exec "${PYTHON}" "${SCRIPT_DIR}/tools/build_game.py" "$@"
