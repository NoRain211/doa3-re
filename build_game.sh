#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# macOS and Linux counterpart of the Windows launcher's GAME page: ./build_game.sh /path/to/doa3.iso
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
XISO_COMMIT="3f5b62cfe68f000b0e3c8a30104973f3a297948e" # XboxDev/extract-xiso build-202609111233
if [[ "$(uname -s)" == Linux ]]; then
    # SteamOS has no compiler, so its script builds in a container.
    if grep -qx 'ID=steamos' /etc/os-release 2>/dev/null; then
        exec "${SCRIPT_DIR}/build_steamdeck.sh" "$@"
    fi
    # The tools this script and tools/build_game.py need.
    missing=()
    for tool in git cmake ninja; do
        command -v "${tool}" >/dev/null || missing+=("${tool}")
    done
    { command -v c++ || command -v g++ || command -v clang++; } >/dev/null || missing+=("a C++ compiler")
    python3 -c "import ensurepip, venv" 2>/dev/null || missing+=("Python's venv")
    if (( ${#missing[@]} )); then
        echo "Missing build tools: ${missing[*]}." >&2
        if [[ ! -t 0 ]]; then
            echo "Run ./install_deps.sh in a terminal, then build again." >&2
            exit 1
        fi
        echo "Installing them with ./install_deps.sh..." >&2
        "${SCRIPT_DIR}/install_deps.sh"
    elif [[ -t 0 ]] && ! command -v bsdtar >/dev/null; then
        # Only the RAR costume pack needs bsdtar, so a failed install skips that pack, not the build.
        echo "Installing bsdtar with ./install_deps.sh..." >&2
        "${SCRIPT_DIR}/install_deps.sh" ||
            echo "Could not install bsdtar; the build skips the RAR costume pack." >&2
    fi
fi
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
# Distributions and Homebrew have no extract-xiso, so build the pinned release once.
# Only an ISO needs it: a disc folder or a rebuild must still work offline.
iso_given=false
for arg in "$@"; do [[ -f "${arg}" ]] && iso_given=true; done
extractor=()
if ${iso_given} && ! command -v extract-xiso >/dev/null && [[ " $* " != *" --extractor"* ]]; then
    XISO="${SCRIPT_DIR}/private/extract-xiso"
    if [[ ! -x "${XISO}/build/extract-xiso" ]]; then
        echo "Building extract-xiso..."
        rm -rf "${XISO}"
        git clone --quiet https://github.com/XboxDev/extract-xiso.git "${XISO}"
        git -C "${XISO}" checkout --quiet "${XISO_COMMIT}"
        cmake -S "${XISO}" -B "${XISO}/build" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
        cmake --build "${XISO}/build" >/dev/null
    fi
    extractor=(--extractor "${XISO}/build/extract-xiso")
fi
# macOS's bash 3.2 treats an empty array as unset under set -u.
exec "${PYTHON}" "${SCRIPT_DIR}/tools/build_game.py" ${extractor[@]+"${extractor[@]}"} "$@"
