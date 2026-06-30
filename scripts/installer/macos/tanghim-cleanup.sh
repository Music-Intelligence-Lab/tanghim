#!/bin/bash
# Tanghim cleanup — single source of truth for removing installed files.
#
# Used in two contexts:
#   1. As the pkg's preinstall (transmitter component, runs first):
#      --scope plugins — removes the plug-in bundles so each install lays down
#      a fresh bundle. M4L files and the Max package are NOT touched here (the
#      M4L component self-cleans in its postinstall, so deselecting M4L on an
#      update never orphans the user's devices). Runtime data is never touched.
#   2. As the standalone uninstaller:
#      --scope all --mode all (the defaults) — full removal, including M4L,
#      the Max package (current + stale locations), runtime data and receipts.
#
# Usage:  sudo bash tanghim-cleanup.sh [--scope plugins|all] [--mode keep-data|all]
#         defaults: --scope all --mode all  (standalone-uninstaller behaviour)
#
# Must run as root: the plug-ins live in the system /Library (and the pkg
# preinstall runs as root). The real (console / sudo) user's home is resolved
# so user-scoped files are removed from the right profile.

set -euo pipefail

SCOPE="all"
MODE="all"
USER_HOME_OVERRIDE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --scope) SCOPE="${2:-}"; shift 2 ;;
        --mode)  MODE="${2:-}"; shift 2 ;;
        --user-home) USER_HOME_OVERRIDE="${2:-}"; shift 2 ;;
        *) echo "tanghim-cleanup: unknown arg '$1'" >&2; exit 2 ;;
    esac
done

[[ "${SCOPE}" == "plugins" || "${SCOPE}" == "all" ]] || { echo "tanghim-cleanup: --scope must be 'plugins' or 'all'" >&2; exit 2; }
[[ "${MODE}" == "keep-data" || "${MODE}" == "all" ]] || { echo "tanghim-cleanup: --mode must be 'keep-data' or 'all'" >&2; exit 2; }

if [[ "$(id -u)" -ne 0 ]]; then
    echo "tanghim-cleanup: must run as root (use: sudo bash $0)" >&2
    exit 1
fi

# ── Resolve the real (non-root) user's home ──────────────────────────────────
if [[ -n "${USER_HOME_OVERRIDE}" ]]; then
    USER_HOME="${USER_HOME_OVERRIDE}"
else
    REAL_USER=""
    if [[ -n "${SUDO_USER:-}" && "${SUDO_USER}" != "root" ]]; then
        REAL_USER="${SUDO_USER}"
    else
        REAL_USER="$(stat -f '%Su' /dev/console 2>/dev/null || true)"
    fi
    if [[ -z "${REAL_USER}" || "${REAL_USER}" == "root" ]]; then
        echo "tanghim-cleanup: could not determine the real user (got '${REAL_USER}')" >&2
        exit 1
    fi
    USER_HOME="$(dscl . -read "/Users/${REAL_USER}" NFSHomeDirectory 2>/dev/null | awk '{print $2}')"
    [[ -n "${USER_HOME}" ]] || { echo "tanghim-cleanup: could not read home for '${REAL_USER}'" >&2; exit 1; }
fi

echo "tanghim-cleanup: scope=${SCOPE}, mode=${MODE}, user home=${USER_HOME}"

rm_path() {  # rm -rf with a log line; no-op (and no error) if absent
    [[ -e "$1" ]] && rm -rf "$1" && echo "  removed: $1" || true
}

# ── Plug-ins (system + user locations; VST3/AU/CLAP) — both scopes ───────────
for ROOT in "/Library/Audio/Plug-Ins" "${USER_HOME}/Library/Audio/Plug-Ins"; do
    rm_path "${ROOT}/VST3/Tanghim.vst3"
    rm_path "${ROOT}/VST3/Tanghim Receiver.vst3"
    rm_path "${ROOT}/Components/Tanghim.component"
    rm_path "${ROOT}/Components/Tanghim Receiver.component"
    rm_path "${ROOT}/CLAP/Tanghim.clap"
    rm_path "${ROOT}/CLAP/Tanghim Receiver.clap"
done

if [[ "${SCOPE}" == "all" ]]; then
    # ── M4L devices (whole device subfolder, incl. legacy combined device) ───
    rm_path "${USER_HOME}/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim"

    # ── MTS-ESP Max Package (current Packages + stale Library, Max 8 + Max 9) ─
    for V in "Max 8" "Max 9"; do
        rm_path "${USER_HOME}/Documents/${V}/Packages/MTS-ESP-Max-Package"
        rm_path "${USER_HOME}/Documents/${V}/Library/MTS-ESP-Max-Package"   # stale (wrong) location
    done

    # ── DELIBERATELY NOT removed: the MTS-ESP shared library ─────────────────
    # /Library/Application Support/MTS-ESP/libMTS.dylib is a system-wide resource
    # installed and shared by every MTS-ESP product (Surge, ODDSound, etc.).
    # Deleting it would break tuning for other software on this machine. The
    # installer installs it only if absent (preinstall) and never downgrades it.
    # Leave it (and MTS-ESP.conf) in place.

    # ── Runtime data + pkg receipts (full uninstall only) ────────────────────
    if [[ "${MODE}" == "all" ]]; then
        rm_path "${USER_HOME}/Library/Tanghim"   # cache, settings.json, presets.json, receivers, midi-export
        for ID in transmitter receiver m4l; do
            if pkgutil --pkg-info "com.khyamallami.tanghim.${ID}" >/dev/null 2>&1; then
                pkgutil --forget "com.khyamallami.tanghim.${ID}" >/dev/null 2>&1 \
                    && echo "  forgot receipt: com.khyamallami.tanghim.${ID}"
            fi
        done
    fi
fi

echo "tanghim-cleanup: done."
