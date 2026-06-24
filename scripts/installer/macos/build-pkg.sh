#!/bin/bash
# Builds the macOS distribution .pkg for Tanghim.
#
# Usage: build-pkg.sh <version> <plugins-dir> <output-dir>
#
# <plugins-dir> must contain:
#   Tanghim.vst3/
#   Tanghim.component/
#   Tanghim.clap/
#   Tanghim Receiver.vst3/
#   Tanghim Receiver.component/
#   Tanghim Receiver.clap/
#   Tanghim MPE Receiver.amxd
#   Tanghim Mono PB Receiver.amxd
#   MTS-ESP-Max-Package/
#
# Produces: <output-dir>/Tanghim-<version>-macOS.pkg

set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <version> <plugins-dir> <output-dir>" >&2
    exit 2
fi

VERSION="$1"
PLUGINS_DIR="$2"
OUTPUT_DIR="$3"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "${OUTPUT_DIR}"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

# Restore executable bit on plugin Mach-O binaries — actions/upload-artifact
# strips the +x bit from files when archiving a directory, so by the time the
# package job downloads the artifact the binaries are 0644. Without +x, DAWs
# silently skip the bundle during plugin scans. Re-apply before pkgbuild.
restore_plugin_executable_bits() {
    local bundle_root="$1"
    find "${bundle_root}" -type d -name "MacOS" -print0 | while IFS= read -r -d '' macos_dir; do
        find "${macos_dir}" -type f -exec chmod +x {} +
    done
}

# ── Component 1: Transmitter (VST3 + AU + CLAP) ──────────────────────────────
TRANSMITTER_ROOT="${WORK_DIR}/transmitter-root"
mkdir -p "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/Components"
mkdir -p "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/CLAP"
cp -R "${PLUGINS_DIR}/Tanghim.vst3"       "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/VST3/"
cp -R "${PLUGINS_DIR}/Tanghim.component"  "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/Components/"
cp -R "${PLUGINS_DIR}/Tanghim.clap"       "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/CLAP/"
restore_plugin_executable_bits "${TRANSMITTER_ROOT}"

pkgbuild \
    --identifier "com.khyamallami.tanghim.transmitter" \
    --version "${VERSION}" \
    --root "${TRANSMITTER_ROOT}" \
    "${WORK_DIR}/Transmitter.pkg"

# ── Component 2: Receiver (VST3 + AU + CLAP) ─────────────────────────────────
RECEIVER_ROOT="${WORK_DIR}/receiver-root"
mkdir -p "${RECEIVER_ROOT}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${RECEIVER_ROOT}/Library/Audio/Plug-Ins/Components"
mkdir -p "${RECEIVER_ROOT}/Library/Audio/Plug-Ins/CLAP"
cp -R "${PLUGINS_DIR}/Tanghim Receiver.vst3"      "${RECEIVER_ROOT}/Library/Audio/Plug-Ins/VST3/"
cp -R "${PLUGINS_DIR}/Tanghim Receiver.component" "${RECEIVER_ROOT}/Library/Audio/Plug-Ins/Components/"
cp -R "${PLUGINS_DIR}/Tanghim Receiver.clap"      "${RECEIVER_ROOT}/Library/Audio/Plug-Ins/CLAP/"
restore_plugin_executable_bits "${RECEIVER_ROOT}"

pkgbuild \
    --identifier "com.khyamallami.tanghim.receiver" \
    --version "${VERSION}" \
    --root "${RECEIVER_ROOT}" \
    "${WORK_DIR}/Receiver.pkg"

# ── Component 3: M4L (staging + postinstall) ─────────────────────────────────
# The .amxd files and the MTS-ESP-Max-Package are placed into
# /tmp/tanghim-m4l-staging inside the payload. The postinstall script then
# moves them to the console user's Ableton library and Max user library.
M4L_ROOT="${WORK_DIR}/m4l-root"
mkdir -p "${M4L_ROOT}/tmp/tanghim-m4l-staging"
cp "${PLUGINS_DIR}/Tanghim MPE Receiver.amxd"     "${M4L_ROOT}/tmp/tanghim-m4l-staging/"
cp "${PLUGINS_DIR}/Tanghim Mono PB Receiver.amxd" "${M4L_ROOT}/tmp/tanghim-m4l-staging/"
cp -R "${PLUGINS_DIR}/MTS-ESP-Max-Package"        "${M4L_ROOT}/tmp/tanghim-m4l-staging/"

pkgbuild \
    --identifier "com.khyamallami.tanghim.m4l" \
    --version "${VERSION}" \
    --root "${M4L_ROOT}" \
    --scripts "${SCRIPT_DIR}/scripts" \
    "${WORK_DIR}/M4L.pkg"

# ── Distribution: render distribution.xml with substitutions ─────────────────
DIST_XML="${WORK_DIR}/distribution.xml"
sed -e "s|{VERSION}|${VERSION}|g" \
    "${SCRIPT_DIR}/distribution.xml" > "${DIST_XML}"

# ── Build the distribution package ───────────────────────────────────────────
OUTPUT_PKG="${OUTPUT_DIR}/Tanghim-${VERSION}-macOS.pkg"
productbuild \
    --distribution "${DIST_XML}" \
    --package-path "${WORK_DIR}" \
    "${OUTPUT_PKG}"

echo "Built ${OUTPUT_PKG}"
ls -lh "${OUTPUT_PKG}"
