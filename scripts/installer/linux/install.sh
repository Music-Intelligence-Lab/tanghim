#!/bin/bash
# Tanghim Linux installer.
# Place this script and the plugins/ directory in the same folder, then run:
#   ./install.sh
# Installs plugins to the user's ~/.vst3 and ~/.clap directories. No sudo.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGINS_SRC="${SCRIPT_DIR}/plugins"

if [[ ! -d "${PLUGINS_SRC}" ]]; then
    echo "error: plugins/ directory not found next to install.sh" >&2
    echo "  expected: ${PLUGINS_SRC}" >&2
    exit 1
fi

VST3_DEST="${HOME}/.vst3"
CLAP_DEST="${HOME}/.clap"

mkdir -p "${VST3_DEST}" "${CLAP_DEST}"

echo "Installing Tanghim plug-ins…"

for bundle in "Tanghim.vst3" "Tanghim Receiver.vst3"; do
    SRC="${PLUGINS_SRC}/${bundle}"
    if [[ ! -d "${SRC}" ]]; then
        echo "error: missing ${SRC}" >&2
        exit 1
    fi
    rm -rf "${VST3_DEST}/${bundle}"
    cp -r "${SRC}" "${VST3_DEST}/"
    echo "  installed ${bundle} → ${VST3_DEST}/"
done

for clap in "Tanghim.clap" "Tanghim Receiver.clap"; do
    SRC="${PLUGINS_SRC}/${clap}"
    if [[ ! -f "${SRC}" ]]; then
        echo "error: missing ${SRC}" >&2
        exit 1
    fi
    cp -f "${SRC}" "${CLAP_DEST}/"
    echo "  installed ${clap} → ${CLAP_DEST}/"
done

echo ""
echo "Installation complete. Rescan plug-ins in your DAW."
