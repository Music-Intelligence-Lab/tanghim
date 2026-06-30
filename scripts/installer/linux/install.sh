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

# ── MTS-ESP shared library (libMTS) — REQUIRED for tuning to work at all ──────
# The Transmitter is an MTS-ESP *master*; libMTSMaster.cpp loads this .so at
# runtime via dlopen("/usr/local/lib/libMTS.so"). Without it, the load fails
# silently, every MTS_* function pointer is null, MTS_RegisterMaster() is a
# no-op, and NO master registers in shared memory — so no client ever connects
# and nothing is tuned. libMTS is a SHARED, system-wide resource installed by
# every MTS-ESP product: install-if-absent (never clobber a newer copy). This
# step needs write access to /usr/local/lib, which usually requires root.
MTS_LIB_SRC="${PLUGINS_SRC}/libMTS.so"
MTS_LIB_DEST="/usr/local/lib/libMTS.so"
if [[ -f "${MTS_LIB_DEST}" ]]; then
    echo "  libMTS already present at ${MTS_LIB_DEST}; leaving untouched"
elif [[ ! -f "${MTS_LIB_SRC}" ]]; then
    echo "  warning: ${MTS_LIB_SRC} not found in this package; skipping libMTS" >&2
elif install -d /usr/local/lib 2>/dev/null && install -m 0755 "${MTS_LIB_SRC}" "${MTS_LIB_DEST}" 2>/dev/null; then
    echo "  installed libMTS → ${MTS_LIB_DEST}"
elif command -v sudo >/dev/null 2>&1 \
     && sudo install -d /usr/local/lib \
     && sudo install -m 0755 "${MTS_LIB_SRC}" "${MTS_LIB_DEST}"; then
    echo "  installed libMTS → ${MTS_LIB_DEST} (via sudo)"
else
    echo "" >&2
    echo "  WARNING: could not install the MTS-ESP shared library to ${MTS_LIB_DEST}." >&2
    echo "  Tanghim's tuning will NOT work until it is present. Install it manually:" >&2
    echo "    sudo install -m 0755 '${MTS_LIB_SRC}' '${MTS_LIB_DEST}'" >&2
    echo "" >&2
fi

echo ""
echo "Installation complete. Rescan plug-ins in your DAW."
