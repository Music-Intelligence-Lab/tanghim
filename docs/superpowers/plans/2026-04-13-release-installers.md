# Release Installers Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace zipped-plugin release artifacts with native installers (macOS `.pkg`, Windows `.exe`, Linux `.tar.gz`) produced automatically by the GitHub Actions workflow on `v*` tag push.

**Architecture:** Split the existing single-job workflow into `build` → `package` → `release` stages. Each platform's installer is produced by a dedicated shell/Inno script under `scripts/installer/<os>/`, driven by the `package` matrix job. A CMake guard fails the build when the pushed tag's version doesn't match `CMakeLists.txt`.

**Tech Stack:** GitHub Actions (YAML), CMake, bash (macOS/Linux), PowerShell (Windows), `pkgbuild` + `productbuild` (macOS built-ins), Inno Setup 6 (Windows, via Chocolatey), `tar` (Linux).

**Spec:** [docs/superpowers/specs/2026-04-13-release-installers-design.md](../specs/2026-04-13-release-installers-design.md)

---

## Context for the implementing engineer

- **The repo is a JUCE 8 C++ plugin project** that already builds on macOS/Windows/Linux via CMake. You don't need to touch any C++ code.
- **The existing workflow** (`.github/workflows/release.yml`) builds plugins, zips them per format, and attaches the zips to a GitHub Release. You're replacing the zipping + release-notes-body portion with installer creation, and leaving the actual CMake build and tests untouched.
- **Plugins after `cmake --build`** live at:
  - `build/Tanghim_artefacts/Release/VST3/Tanghim.vst3`
  - `build/Tanghim_artefacts/Release/AU/Tanghim.component` (macOS only)
  - `build/Tanghim_artefacts/Release/CLAP/Tanghim.clap`
  - `build/TanghimReceiver_artefacts/Release/VST3/Tanghim Receiver.vst3`
  - `build/TanghimReceiver_artefacts/Release/AU/Tanghim Receiver.component` (macOS only)
  - `build/TanghimReceiver_artefacts/Release/CLAP/Tanghim Receiver.clap`
- **M4L device** is at `m4l/Tanghim Receiver.amxd` (committed in-repo, no build step).
- **Version source of truth:** line 2 of `CMakeLists.txt`: `project(Tanghim VERSION X.Y.Z)`. The workflow runs on tags like `v0.9.0` — strip the `v` and you have the expected version.
- **Commits in this repo** use the format `<type>: <subject>` and include `Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>` per [CLAUDE.md](../../../CLAUDE.md).
- **No fallbacks convention:** Per CLAUDE.md, never add fallback logic. If a step fails, it fails loudly.

---

## File structure

```
.github/workflows/release.yml             (MODIFY — split into build/package/release)
CMakeLists.txt                            (MODIFY — add version-consistency guard)
scripts/installer/
├── macos/
│   ├── build-pkg.sh                      (CREATE — orchestrator: pkgbuild + productbuild)
│   ├── distribution.xml                  (CREATE — productbuild component+title config)
│   └── scripts/
│       └── postinstall                   (CREATE — moves M4L to user's Ableton folder)
├── windows/
│   └── tanghim.iss                       (CREATE — Inno Setup script)
└── linux/
    ├── build-tarball.sh                  (CREATE — staging + tar invocation)
    ├── install.sh                        (CREATE — shipped inside tarball)
    └── README.txt                        (CREATE — shipped inside tarball)
tests/
└── CMakeVersionGuardTest.cmake           (CREATE — CTest for version-mismatch behaviour)
```

Each file has one clear responsibility; no file exceeds ~150 lines.

---

## Task 1: Add CMake version-consistency guard

**Files:**
- Modify: `CMakeLists.txt` (insert after `project(...)` on line 2)

- [ ] **Step 1: Read CMakeLists.txt top to confirm current structure**

Run: `head -20 CMakeLists.txt`
Expected: `project(Tanghim VERSION 0.9.0)` on line 2.

- [ ] **Step 2: Insert the guard block**

Open `CMakeLists.txt`. After the line `project(Tanghim VERSION 0.9.0)` (line 2) and its blank line, insert:

```cmake
# ── Version-consistency guard ─────────────────────────────────────────────────
# When the workflow builds a tagged release it passes -DTANGHIM_EXPECTED_VERSION=<version>
# (the git tag with the leading 'v' stripped). If the value doesn't match PROJECT_VERSION,
# the build fails early — preventing releases where the installer filename and the plugin's
# internal version disagree. Local builds that don't set this option skip the check.
if(DEFINED TANGHIM_EXPECTED_VERSION AND NOT TANGHIM_EXPECTED_VERSION STREQUAL "")
    if(NOT PROJECT_VERSION VERSION_EQUAL TANGHIM_EXPECTED_VERSION)
        message(FATAL_ERROR
            "Version mismatch: git tag says ${TANGHIM_EXPECTED_VERSION}, "
            "CMakeLists.txt says ${PROJECT_VERSION}. "
            "Bump the project() version or fix the tag.")
    endif()
endif()

```

- [ ] **Step 3: Verify a clean local build still works (no `TANGHIM_EXPECTED_VERSION` set)**

Run: `cmake -B build-test -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5`
Expected: CMake configure completes without error. (It doesn't need to compile — configure success is enough.)
Cleanup: `rm -rf build-test`

- [ ] **Step 4: Verify the guard fires on mismatch**

Run: `cmake -B build-test -DCMAKE_BUILD_TYPE=Debug -DTANGHIM_EXPECTED_VERSION=9.9.9 2>&1 | tail -5`
Expected: `FATAL_ERROR` containing `Version mismatch: git tag says 9.9.9, CMakeLists.txt says 0.9.0`.
Cleanup: `rm -rf build-test`

- [ ] **Step 5: Verify the guard passes on exact match**

Run: `cmake -B build-test -DCMAKE_BUILD_TYPE=Debug -DTANGHIM_EXPECTED_VERSION=0.9.0 2>&1 | tail -5`
Expected: Configure completes without error.
Cleanup: `rm -rf build-test`

- [ ] **Step 6: Commit**

```bash
git add CMakeLists.txt
git commit -m "$(cat <<'EOF'
build: enforce tag ↔ CMakeLists.txt version consistency

Adds a FATAL_ERROR guard triggered when TANGHIM_EXPECTED_VERSION
(passed from the release workflow) differs from PROJECT_VERSION.
Local builds without the option are unaffected.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: macOS — distribution.xml

**Files:**
- Create: `scripts/installer/macos/distribution.xml`

- [ ] **Step 1: Create the directory**

Run: `mkdir -p scripts/installer/macos/scripts`
Expected: Directory exists (no output).

- [ ] **Step 2: Create `scripts/installer/macos/distribution.xml`**

This file is consumed by `productbuild` to define the installer's UI: title, welcome text, which component packages exist, and which ones are user-optional. The `{VERSION}` and `{ARCH}` tokens are substituted by `build-pkg.sh` in Task 4.

```xml
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Tanghim {VERSION}</title>
    <organization>com.khyamallami</organization>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <options customize="allow" require-scripts="false" hostArchitectures="{ARCH}"/>

    <welcome language="en">
        <![CDATA[
Tanghim {VERSION}

This installer will place the Tanghim (Transmitter) and Tanghim Receiver audio plug-ins (VST3, Audio Unit, CLAP) into your system plug-in folders.

The Tanghim Receiver for Max for Live (M4L) is included as an optional component — enable it on the next screen if you use Ableton Live.

The plug-ins are ad-hoc codesigned. Your DAW may warn about an unidentified developer the first time you scan — this is expected.
        ]]>
    </welcome>

    <choices-outline>
        <line choice="transmitter"/>
        <line choice="receiver"/>
        <line choice="m4l"/>
    </choices-outline>

    <choice id="transmitter" title="Tanghim (Transmitter)" description="VST3, Audio Unit, and CLAP versions of the main Tanghim plug-in." start_selected="true" start_enabled="false">
        <pkg-ref id="com.khyamallami.tanghim.transmitter"/>
    </choice>

    <choice id="receiver" title="Tanghim Receiver" description="VST3, Audio Unit, and CLAP versions of the Receiver plug-in (pitch-bend / MPE output for non-MTS-ESP synths)." start_selected="true" start_enabled="false">
        <pkg-ref id="com.khyamallami.tanghim.receiver"/>
    </choice>

    <choice id="m4l" title="Tanghim Receiver for Ableton Live (M4L)" description="Max for Live device for Ableton Live users. Installs to your user Ableton library." start_selected="true" start_enabled="true">
        <pkg-ref id="com.khyamallami.tanghim.m4l"/>
    </choice>

    <pkg-ref id="com.khyamallami.tanghim.transmitter" version="{VERSION}">Transmitter.pkg</pkg-ref>
    <pkg-ref id="com.khyamallami.tanghim.receiver" version="{VERSION}">Receiver.pkg</pkg-ref>
    <pkg-ref id="com.khyamallami.tanghim.m4l" version="{VERSION}">M4L.pkg</pkg-ref>
</installer-gui-script>
```

- [ ] **Step 3: Commit**

```bash
git add scripts/installer/macos/distribution.xml
git commit -m "$(cat <<'EOF'
build: add macOS productbuild distribution.xml

Defines three components (Transmitter required, Receiver required,
M4L optional), welcome text, and version/arch placeholders
substituted by build-pkg.sh.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: macOS — M4L postinstall script

**Files:**
- Create: `scripts/installer/macos/scripts/postinstall`

The M4L component's payload is installed to a staging directory under `/tmp/tanghim-m4l-staging/`, then this postinstall script moves it to the console user's Ableton library (not `/root/Music/...`, which is where `sudo installer` would otherwise put it).

- [ ] **Step 1: Create `scripts/installer/macos/scripts/postinstall`**

```bash
#!/bin/bash
# Moves the staged Tanghim Receiver.amxd from the package payload location
# (/tmp/tanghim-m4l-staging) to the console user's Ableton library.
#
# Invoked by productbuild when the M4L component is selected.
# productbuild runs this as root, so we detect the real console user via stat.

set -euo pipefail

STAGING_DIR="/tmp/tanghim-m4l-staging"
AMXD_NAME="Tanghim Receiver.amxd"
STAGED_FILE="${STAGING_DIR}/${AMXD_NAME}"

if [[ ! -f "${STAGED_FILE}" ]]; then
    echo "postinstall: staged M4L file not found at ${STAGED_FILE}" >&2
    exit 1
fi

CONSOLE_USER="$(stat -f '%Su' /dev/console)"
if [[ -z "${CONSOLE_USER}" || "${CONSOLE_USER}" == "root" ]]; then
    echo "postinstall: could not determine console user (got '${CONSOLE_USER}')" >&2
    exit 1
fi

USER_HOME="$(dscl . -read "/Users/${CONSOLE_USER}" NFSHomeDirectory | awk '{print $2}')"
DEST_DIR="${USER_HOME}/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim"

mkdir -p "${DEST_DIR}"
cp -f "${STAGED_FILE}" "${DEST_DIR}/${AMXD_NAME}"
chown -R "${CONSOLE_USER}" "${USER_HOME}/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim"

rm -f "${STAGED_FILE}"
rmdir "${STAGING_DIR}" 2>/dev/null || true

exit 0
```

- [ ] **Step 2: Make it executable**

Run: `chmod +x scripts/installer/macos/scripts/postinstall`
Expected: No output.

- [ ] **Step 3: Lint the bash script**

Run: `bash -n scripts/installer/macos/scripts/postinstall`
Expected: No output (no syntax errors).

- [ ] **Step 4: Commit**

```bash
git add scripts/installer/macos/scripts/postinstall
git update-index --chmod=+x scripts/installer/macos/scripts/postinstall
git commit -m "$(cat <<'EOF'
build: add macOS postinstall script for M4L placement

productbuild installs packages as root, which would place the M4L
device under /root/Music/... Instead, this postinstall script moves
the staged .amxd to the console user's Ableton library and fixes
ownership.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: macOS — build-pkg.sh orchestrator

**Files:**
- Create: `scripts/installer/macos/build-pkg.sh`

This script is invoked by the `package` job on `macos-latest`. It expects the built plugin bundles in a staging directory passed as the second argument, and produces `Tanghim-<version>-macOS.pkg` in the output directory.

- [ ] **Step 1: Create `scripts/installer/macos/build-pkg.sh`**

```bash
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
#   Tanghim Receiver.amxd
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

# ── Component 1: Transmitter (VST3 + AU + CLAP) ──────────────────────────────
TRANSMITTER_ROOT="${WORK_DIR}/transmitter-root"
mkdir -p "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/Components"
mkdir -p "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/CLAP"
cp -R "${PLUGINS_DIR}/Tanghim.vst3"       "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/VST3/"
cp -R "${PLUGINS_DIR}/Tanghim.component"  "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/Components/"
cp -R "${PLUGINS_DIR}/Tanghim.clap"       "${TRANSMITTER_ROOT}/Library/Audio/Plug-Ins/CLAP/"

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

pkgbuild \
    --identifier "com.khyamallami.tanghim.receiver" \
    --version "${VERSION}" \
    --root "${RECEIVER_ROOT}" \
    "${WORK_DIR}/Receiver.pkg"

# ── Component 3: M4L (staging + postinstall) ─────────────────────────────────
# The .amxd is placed into /tmp/tanghim-m4l-staging inside the payload. The
# postinstall script then moves it to the console user's Ableton library.
M4L_ROOT="${WORK_DIR}/m4l-root"
mkdir -p "${M4L_ROOT}/tmp/tanghim-m4l-staging"
cp "${PLUGINS_DIR}/Tanghim Receiver.amxd" "${M4L_ROOT}/tmp/tanghim-m4l-staging/"

pkgbuild \
    --identifier "com.khyamallami.tanghim.m4l" \
    --version "${VERSION}" \
    --root "${M4L_ROOT}" \
    --scripts "${SCRIPT_DIR}/scripts" \
    "${WORK_DIR}/M4L.pkg"

# ── Distribution: render distribution.xml with substitutions ─────────────────
DIST_XML="${WORK_DIR}/distribution.xml"
sed -e "s/{VERSION}/${VERSION}/g" -e "s/{ARCH}/x86_64,arm64/g" \
    "${SCRIPT_DIR}/distribution.xml" > "${DIST_XML}"

# ── Build the distribution package ───────────────────────────────────────────
OUTPUT_PKG="${OUTPUT_DIR}/Tanghim-${VERSION}-macOS.pkg"
productbuild \
    --distribution "${DIST_XML}" \
    --package-path "${WORK_DIR}" \
    "${OUTPUT_PKG}"

echo "Built ${OUTPUT_PKG}"
ls -lh "${OUTPUT_PKG}"
```

- [ ] **Step 2: Make it executable**

Run: `chmod +x scripts/installer/macos/build-pkg.sh`
Expected: No output.

- [ ] **Step 3: Lint the bash script**

Run: `bash -n scripts/installer/macos/build-pkg.sh`
Expected: No output.

- [ ] **Step 4: Dry-run-verify with dummy payloads (local only, skip if not on macOS)**

If you're on macOS, exercise the script end-to-end with placeholder contents to catch typos before pushing to CI:

```bash
STAGE=$(mktemp -d)
OUT=$(mktemp -d)
for b in Tanghim.vst3 Tanghim.component Tanghim.clap "Tanghim Receiver.vst3" "Tanghim Receiver.component" "Tanghim Receiver.clap"; do
    mkdir -p "${STAGE}/${b}/Contents"
    echo "placeholder" > "${STAGE}/${b}/Contents/placeholder"
done
echo "placeholder amxd" > "${STAGE}/Tanghim Receiver.amxd"
./scripts/installer/macos/build-pkg.sh 0.9.0 "${STAGE}" "${OUT}"
ls -lh "${OUT}"
pkgutil --expand "${OUT}/Tanghim-0.9.0-macOS.pkg" "${OUT}/expanded"
ls "${OUT}/expanded"
rm -rf "${STAGE}" "${OUT}"
```
Expected: A `Tanghim-0.9.0-macOS.pkg` file several KB in size, and `pkgutil --expand` produces a directory tree containing `Transmitter.pkg`, `Receiver.pkg`, `M4L.pkg`, and `Distribution`.

If not on macOS, skip this step — CI will catch errors.

- [ ] **Step 5: Commit**

```bash
git add scripts/installer/macos/build-pkg.sh
git update-index --chmod=+x scripts/installer/macos/build-pkg.sh
git commit -m "$(cat <<'EOF'
build: add macOS build-pkg.sh orchestrator

Produces a three-component distribution .pkg via pkgbuild +
productbuild. Takes version, plugins dir, and output dir as args.
M4L uses staging-plus-postinstall so it lands in the console
user's Ableton library instead of /root.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Windows — Inno Setup script

**Files:**
- Create: `scripts/installer/windows/tanghim.iss`

Inno Setup consumes a `.iss` script to produce a setup `.exe`. The version is injected via `/D` command-line preprocessor defines, so we don't hardcode it.

- [ ] **Step 1: Create the directory**

Run: `mkdir -p scripts/installer/windows`
Expected: No output.

- [ ] **Step 2: Create `scripts/installer/windows/tanghim.iss`**

```iss
; Tanghim installer — Inno Setup 6 script.
; Invoked by the release workflow:
;   ISCC.exe /DMyAppVersion=<version> /DPluginsDir=<abs-path> /DOutputDir=<abs-path> tanghim.iss
;
; PluginsDir must contain:
;   Tanghim.vst3\
;   Tanghim.clap
;   Tanghim Receiver.vst3\
;   Tanghim Receiver.clap
;   Tanghim Receiver.amxd

#ifndef MyAppVersion
  #error MyAppVersion is required (pass /DMyAppVersion=...)
#endif
#ifndef PluginsDir
  #error PluginsDir is required (pass /DPluginsDir=...)
#endif
#ifndef OutputDir
  #error OutputDir is required (pass /DOutputDir=...)
#endif

#define MyAppName "Tanghim"
#define MyAppPublisher "KhyamAllami"

[Setup]
AppId={{D7F4A1C2-6E3B-4F8A-9B5C-1A2E8F9D0C3B}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Tanghim-{#MyAppVersion}-Windows
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Uninstallable=no
CreateUninstallRegKey=no
WizardStyle=modern

[Components]
Name: "transmitter"; Description: "Tanghim (Transmitter) — VST3 + CLAP"; Types: full custom; Flags: fixed
Name: "receiver";    Description: "Tanghim Receiver — VST3 + CLAP";      Types: full custom; Flags: fixed
Name: "m4l";         Description: "Tanghim Receiver for Ableton Live (M4L)"; Types: full

[Files]
; Transmitter — system plugin folders
Source: "{#PluginsDir}\Tanghim.vst3\*"; DestDir: "{commoncf64}\VST3\Tanghim.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: transmitter
Source: "{#PluginsDir}\Tanghim.clap";   DestDir: "{commoncf64}\CLAP";             Flags: ignoreversion;                                  Components: transmitter

; Receiver — system plugin folders
Source: "{#PluginsDir}\Tanghim Receiver.vst3\*"; DestDir: "{commoncf64}\VST3\Tanghim Receiver.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: receiver
Source: "{#PluginsDir}\Tanghim Receiver.clap";   DestDir: "{commoncf64}\CLAP";                      Flags: ignoreversion;                                  Components: receiver

; M4L — staged under {app}, copied to user profile by [Run] below
Source: "{#PluginsDir}\Tanghim Receiver.amxd"; DestDir: "{app}\m4l-staging"; Flags: ignoreversion; Components: m4l

[Run]
; Copy the staged .amxd into the invoking (non-admin) user's Ableton library.
; runasoriginaluser drops elevation so %USERPROFILE% resolves to the real user,
; not the admin that UAC elevated to.
Filename: "{cmd}"; \
  Parameters: "/c mkdir ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim"" & copy /Y ""{app}\m4l-staging\Tanghim Receiver.amxd"" ""%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\Tanghim Receiver.amxd"""; \
  Flags: runhidden runasoriginaluser; \
  Components: m4l; \
  StatusMsg: "Installing Max for Live device…"
```

Two notes for the engineer unfamiliar with Inno:

- `{commoncf64}` resolves to `C:\Program Files\Common Files` on 64-bit Windows — the standard VST3/CLAP root. `AppId` is a fixed GUID that identifies future upgrades; do not change it between versions.
- `Uninstallable=no` + `CreateUninstallRegKey=no` together mean no uninstaller executable is produced and no Add/Remove Programs entry is created, per the spec.

- [ ] **Step 3: Syntax check the .iss (skip if not on Windows; CI will catch errors)**

If you're on Windows with Inno Setup installed:
```cmd
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DMyAppVersion=0.9.0 /DPluginsDir=C:\fake /DOutputDir=C:\fake scripts\installer\windows\tanghim.iss
```
Expected: ISCC exits with an error about the missing PluginsDir contents — that means the script parsed successfully. A parse error would complain about syntax before reaching file checks.

If not on Windows, skip.

- [ ] **Step 4: Commit**

```bash
git add scripts/installer/windows/tanghim.iss
git commit -m "$(cat <<'EOF'
build: add Windows Inno Setup installer script

Produces Tanghim-<version>-Windows.exe with three components
(Transmitter + Receiver required, M4L optional). M4L uses the
runasoriginaluser Run entry to copy into the invoking user's
Ableton library rather than the admin profile.

No uninstaller (Uninstallable=no), per design spec.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Linux — install.sh (shipped inside the tarball)

**Files:**
- Create: `scripts/installer/linux/install.sh`

This script runs on the end-user's machine after they extract the tarball. It's not run in CI.

- [ ] **Step 1: Create the directory**

Run: `mkdir -p scripts/installer/linux`
Expected: No output.

- [ ] **Step 2: Create `scripts/installer/linux/install.sh`**

```bash
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
```

- [ ] **Step 3: Make it executable**

Run: `chmod +x scripts/installer/linux/install.sh`
Expected: No output.

- [ ] **Step 4: Lint the bash script**

Run: `bash -n scripts/installer/linux/install.sh`
Expected: No output.

- [ ] **Step 5: Commit**

```bash
git add scripts/installer/linux/install.sh
git update-index --chmod=+x scripts/installer/linux/install.sh
git commit -m "$(cat <<'EOF'
build: add Linux install.sh shipped inside release tarball

Installs VST3 bundles to ~/.vst3 and CLAPs to ~/.clap. Idempotent —
re-running overwrites. Exits non-zero on missing source files.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Linux — README.txt shipped inside tarball

**Files:**
- Create: `scripts/installer/linux/README.txt`

- [ ] **Step 1: Create `scripts/installer/linux/README.txt`**

```text
Tanghim — Linux installation
=============================

1. Extract this archive (you've already done this if you're reading
   this file from the extracted folder).

2. Run the installer:

     ./install.sh

   This copies the VST3 plug-ins to ~/.vst3 and the CLAP plug-ins
   to ~/.clap. No sudo is required.

3. Rescan plug-ins in your DAW (Reaper, Bitwig, Ardour, etc.).

Manual install
--------------
If you prefer, copy the contents of plugins/ into your plug-in
folders yourself:

    plugins/Tanghim.vst3/            →  ~/.vst3/
    plugins/Tanghim Receiver.vst3/   →  ~/.vst3/
    plugins/Tanghim.clap             →  ~/.clap/
    plugins/Tanghim Receiver.clap    →  ~/.clap/

Ableton Live users
------------------
Ableton doesn't run on Linux, so the M4L Receiver device isn't
included in this archive.
```

- [ ] **Step 2: Commit**

```bash
git add scripts/installer/linux/README.txt
git commit -m "$(cat <<'EOF'
build: add Linux release tarball README

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Linux — build-tarball.sh (run in CI)

**Files:**
- Create: `scripts/installer/linux/build-tarball.sh`

- [ ] **Step 1: Create `scripts/installer/linux/build-tarball.sh`**

```bash
#!/bin/bash
# Builds the Linux release tarball.
#
# Usage: build-tarball.sh <version> <plugins-dir> <output-dir>
#
# <plugins-dir> must contain:
#   Tanghim.vst3/
#   Tanghim.clap
#   Tanghim Receiver.vst3/
#   Tanghim Receiver.clap
#
# Produces: <output-dir>/Tanghim-<version>-Linux.tar.gz

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

STAGE="${WORK_DIR}/Tanghim-${VERSION}-Linux"
mkdir -p "${STAGE}/plugins"

for item in "Tanghim.vst3" "Tanghim Receiver.vst3"; do
    cp -r "${PLUGINS_DIR}/${item}" "${STAGE}/plugins/"
done
for item in "Tanghim.clap" "Tanghim Receiver.clap"; do
    cp "${PLUGINS_DIR}/${item}" "${STAGE}/plugins/"
done

cp "${SCRIPT_DIR}/install.sh" "${STAGE}/install.sh"
cp "${SCRIPT_DIR}/README.txt" "${STAGE}/README.txt"
chmod +x "${STAGE}/install.sh"

OUTPUT_TGZ="${OUTPUT_DIR}/Tanghim-${VERSION}-Linux.tar.gz"
tar -czf "${OUTPUT_TGZ}" -C "${WORK_DIR}" "Tanghim-${VERSION}-Linux"

echo "Built ${OUTPUT_TGZ}"
ls -lh "${OUTPUT_TGZ}"
tar -tzf "${OUTPUT_TGZ}" | head -20
```

- [ ] **Step 2: Make it executable**

Run: `chmod +x scripts/installer/linux/build-tarball.sh`
Expected: No output.

- [ ] **Step 3: Lint**

Run: `bash -n scripts/installer/linux/build-tarball.sh`
Expected: No output.

- [ ] **Step 4: Local smoke test (if on Linux/macOS)**

```bash
STAGE=$(mktemp -d); OUT=$(mktemp -d)
mkdir -p "${STAGE}/Tanghim.vst3" "${STAGE}/Tanghim Receiver.vst3"
echo "x" > "${STAGE}/Tanghim.vst3/placeholder"
echo "x" > "${STAGE}/Tanghim Receiver.vst3/placeholder"
echo "clap1" > "${STAGE}/Tanghim.clap"
echo "clap2" > "${STAGE}/Tanghim Receiver.clap"
./scripts/installer/linux/build-tarball.sh 0.9.0 "${STAGE}" "${OUT}"
tar -tzf "${OUT}/Tanghim-0.9.0-Linux.tar.gz"
rm -rf "${STAGE}" "${OUT}"
```
Expected: tarball lists `Tanghim-0.9.0-Linux/install.sh`, `README.txt`, and `plugins/` entries.

- [ ] **Step 5: Commit**

```bash
git add scripts/installer/linux/build-tarball.sh
git update-index --chmod=+x scripts/installer/linux/build-tarball.sh
git commit -m "$(cat <<'EOF'
build: add Linux tarball builder

Produces Tanghim-<version>-Linux.tar.gz containing plugins/,
install.sh, and README.txt.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Rewrite release.yml — build job uploads raw plugins

**Files:**
- Modify: `.github/workflows/release.yml` (full rewrite)

This task rewrites the entire workflow file to introduce the build → package → release pipeline. Because the diff is too large to apply surgically, we replace the file wholesale.

- [ ] **Step 1: Read the existing workflow to confirm what we're replacing**

Run: `wc -l .github/workflows/release.yml`
Expected: ~210 lines.

- [ ] **Step 2: Write the new `.github/workflows/release.yml`**

```yaml
name: Build & Release

on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write

jobs:
  # ────────────────────────────────────────────────────────────────────────────
  # 1. build: compile plugins, run tests, upload raw bundles per OS
  # ────────────────────────────────────────────────────────────────────────────
  build:
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: macos-latest
            name: macOS
            cmake_flags: '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
          - os: windows-latest
            name: Windows
            cmake_flags: ''
          - os: ubuntu-latest
            name: Linux
            cmake_flags: ''

    runs-on: ${{ matrix.os }}
    name: Build (${{ matrix.name }})

    steps:
      - name: Checkout with submodules
        uses: actions/checkout@v4
        with:
          submodules: recursive

      - name: Extract version from tag
        id: version
        shell: bash
        run: |
          version="${GITHUB_REF_NAME#v}"
          echo "version=${version}" >> "$GITHUB_OUTPUT"
          echo "Building version ${version}"

      - name: Install Linux dependencies
        if: runner.os == 'Linux'
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            libasound2-dev libjack-jackd2-dev ladspa-sdk libcurl4-openssl-dev \
            libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
            libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
            libgtk-3-dev libglu1-mesa-dev mesa-common-dev

      - name: Configure CMake
        run: >
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          -DTANGHIM_EXPECTED_VERSION=${{ steps.version.outputs.version }}
          ${{ matrix.cmake_flags }}

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        working-directory: build
        run: ctest --output-on-failure -C Release

      - name: Codesign (macOS)
        if: runner.os == 'macOS'
        run: |
          codesign --force --deep --sign - "build/Tanghim_artefacts/Release/VST3/Tanghim.vst3"
          codesign --force --deep --sign - "build/Tanghim_artefacts/Release/AU/Tanghim.component"
          codesign --force --deep --sign - "build/Tanghim_artefacts/Release/CLAP/Tanghim.clap"
          codesign --force --deep --sign - "build/TanghimReceiver_artefacts/Release/VST3/Tanghim Receiver.vst3"
          codesign --force --deep --sign - "build/TanghimReceiver_artefacts/Release/AU/Tanghim Receiver.component"
          codesign --force --deep --sign - "build/TanghimReceiver_artefacts/Release/CLAP/Tanghim Receiver.clap"

      # ── Stage plugins into a single directory per OS for upload ───────────
      - name: Stage plugins (macOS)
        if: runner.os == 'macOS'
        run: |
          mkdir -p staged-plugins
          cp -R build/Tanghim_artefacts/Release/VST3/Tanghim.vst3       staged-plugins/
          cp -R build/Tanghim_artefacts/Release/AU/Tanghim.component    staged-plugins/
          cp -R build/Tanghim_artefacts/Release/CLAP/Tanghim.clap       staged-plugins/
          cp -R "build/TanghimReceiver_artefacts/Release/VST3/Tanghim Receiver.vst3"      staged-plugins/
          cp -R "build/TanghimReceiver_artefacts/Release/AU/Tanghim Receiver.component"   staged-plugins/
          cp -R "build/TanghimReceiver_artefacts/Release/CLAP/Tanghim Receiver.clap"      staged-plugins/
          cp "m4l/Tanghim Receiver.amxd" staged-plugins/

      - name: Stage plugins (Windows)
        if: runner.os == 'Windows'
        shell: pwsh
        run: |
          New-Item -ItemType Directory -Path staged-plugins -Force | Out-Null
          Copy-Item -Recurse "build/Tanghim_artefacts/Release/VST3/Tanghim.vst3"                     "staged-plugins/"
          Copy-Item         "build/Tanghim_artefacts/Release/CLAP/Tanghim.clap"                      "staged-plugins/"
          Copy-Item -Recurse "build/TanghimReceiver_artefacts/Release/VST3/Tanghim Receiver.vst3"    "staged-plugins/"
          Copy-Item         "build/TanghimReceiver_artefacts/Release/CLAP/Tanghim Receiver.clap"     "staged-plugins/"
          Copy-Item         "m4l/Tanghim Receiver.amxd" "staged-plugins/"

      - name: Stage plugins (Linux)
        if: runner.os == 'Linux'
        run: |
          mkdir -p staged-plugins
          cp -r build/Tanghim_artefacts/Release/VST3/Tanghim.vst3   staged-plugins/
          cp    build/Tanghim_artefacts/Release/CLAP/Tanghim.clap   staged-plugins/
          cp -r "build/TanghimReceiver_artefacts/Release/VST3/Tanghim Receiver.vst3"  staged-plugins/
          cp    "build/TanghimReceiver_artefacts/Release/CLAP/Tanghim Receiver.clap"  staged-plugins/

      - name: Upload staged plugins
        uses: actions/upload-artifact@v4
        with:
          name: plugins-${{ matrix.name }}
          path: staged-plugins
          if-no-files-found: error

  # ────────────────────────────────────────────────────────────────────────────
  # 2. package: turn raw plugins into per-OS installers
  # ────────────────────────────────────────────────────────────────────────────
  package:
    needs: build
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: macos-latest
            name: macOS
          - os: windows-latest
            name: Windows
          - os: ubuntu-latest
            name: Linux

    runs-on: ${{ matrix.os }}
    name: Package (${{ matrix.name }})

    steps:
      - name: Checkout (for installer scripts)
        uses: actions/checkout@v4
        with:
          submodules: false

      - name: Extract version from tag
        id: version
        shell: bash
        run: echo "version=${GITHUB_REF_NAME#v}" >> "$GITHUB_OUTPUT"

      - name: Download staged plugins
        uses: actions/download-artifact@v4
        with:
          name: plugins-${{ matrix.name }}
          path: staged-plugins

      - name: Build installer (macOS)
        if: runner.os == 'macOS'
        run: |
          mkdir -p installer-out
          ./scripts/installer/macos/build-pkg.sh \
            "${{ steps.version.outputs.version }}" \
            "$(pwd)/staged-plugins" \
            "$(pwd)/installer-out"

      - name: Install Inno Setup (Windows)
        if: runner.os == 'Windows'
        shell: pwsh
        run: choco install innosetup --version=6.2.2 --no-progress -y

      - name: Build installer (Windows)
        if: runner.os == 'Windows'
        shell: pwsh
        run: |
          New-Item -ItemType Directory -Path installer-out -Force | Out-Null
          $pluginsAbs = (Resolve-Path "staged-plugins").Path
          $outAbs = (Resolve-Path "installer-out").Path
          & "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" `
            "/DMyAppVersion=${{ steps.version.outputs.version }}" `
            "/DPluginsDir=$pluginsAbs" `
            "/DOutputDir=$outAbs" `
            "scripts\installer\windows\tanghim.iss"
          if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

      - name: Build installer (Linux)
        if: runner.os == 'Linux'
        run: |
          mkdir -p installer-out
          ./scripts/installer/linux/build-tarball.sh \
            "${{ steps.version.outputs.version }}" \
            "$(pwd)/staged-plugins" \
            "$(pwd)/installer-out"

      - name: Verify installer produced
        shell: bash
        run: |
          ls -lh installer-out/
          count=$(find installer-out -maxdepth 1 -type f | wc -l | tr -d ' ')
          if [[ "$count" -lt 1 ]]; then
            echo "error: no installer produced" >&2
            exit 1
          fi

      - name: Upload installer
        uses: actions/upload-artifact@v4
        with:
          name: installer-${{ matrix.name }}
          path: installer-out/*
          if-no-files-found: error

  # ────────────────────────────────────────────────────────────────────────────
  # 3. release: delete old releases, publish the three installers
  # ────────────────────────────────────────────────────────────────────────────
  release:
    needs: package
    runs-on: ubuntu-latest
    name: Create Release

    steps:
      - name: Extract version from tag
        id: version
        run: echo "version=${GITHUB_REF_NAME#v}" >> "$GITHUB_OUTPUT"

      - name: Delete all previous releases
        run: |
          gh release list --json tagName -q '.[].tagName' | while read -r tag; do
            echo "Deleting release $tag"
            gh release delete "$tag" --yes --cleanup-tag || true
          done
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
          GH_REPO: ${{ github.repository }}

      - name: Download all installers
        uses: actions/download-artifact@v4
        with:
          path: artifacts
          pattern: installer-*
          merge-multiple: true

      - name: List installers
        run: find artifacts -type f | sort

      - name: Create GitHub Release
        uses: softprops/action-gh-release@v2
        with:
          name: Tanghim ${{ github.ref_name }}
          generate_release_notes: true
          body: |
            ## Downloads

            | Platform | Installer |
            |---|---|
            | macOS (Universal) | Tanghim-${{ steps.version.outputs.version }}-macOS.pkg |
            | Windows (x64) | Tanghim-${{ steps.version.outputs.version }}-Windows.exe |
            | Linux (x64) | Tanghim-${{ steps.version.outputs.version }}-Linux.tar.gz |

            ### Installation

            - **macOS:** Double-click the `.pkg`. On first plug-in launch you may need to right-click the plug-in file in Finder → Open to bypass Gatekeeper (installer is ad-hoc signed, not notarized).
            - **Windows:** Double-click the `.exe`. Admin rights required (installs to `Program Files`).
            - **Linux:** `tar xzf Tanghim-${{ steps.version.outputs.version }}-Linux.tar.gz && cd Tanghim-${{ steps.version.outputs.version }}-Linux && ./install.sh`

            ### What's included

            - Tanghim (Transmitter) — VST3, AU (macOS only), CLAP
            - Tanghim Receiver — VST3, AU (macOS only), CLAP
            - Tanghim Receiver for Ableton Live (M4L) — macOS and Windows installers, optional component

            > **Ableton Live users:** Ableton doesn't support VST3 MIDI effects. Enable the M4L component during installation, or use MTS-ESP-capable synths.
          files: artifacts/*
```

- [ ] **Step 3: Lint the YAML**

Run: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/release.yml'))"` — if `python3 -c "import yaml"` fails, use `ruby -ryaml -e "YAML.load_file('.github/workflows/release.yml')"` instead.
Expected: No output (valid YAML).

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/release.yml
git commit -m "$(cat <<'EOF'
ci: split release workflow into build → package → release

- build: matrix (mac/win/lx) compiles plugins, runs tests, codesigns
  on macOS, and uploads raw plugin bundles as artifacts. CMake now
  receives TANGHIM_EXPECTED_VERSION so the version guard fires on
  tag ↔ CMakeLists.txt drift.
- package: new matrix job per OS. Downloads the plugin artifact and
  runs scripts/installer/<os>/ to produce a native installer (.pkg,
  .exe, .tar.gz). Installs Inno Setup 6.2.2 via Chocolatey on the
  Windows runner.
- release: downloads installers (not raw plugins), attaches them to
  a new GitHub Release. Release-notes body rewritten around the
  three-installer layout from the design spec.

No more per-format zip artifacts.

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Validate end-to-end by retriggering v0.9.0

This is the real integration test — none of the installer pipelines can be fully verified without running on GitHub's macOS/Windows/Linux runners.

- [ ] **Step 1: Push all the new commits to `main`**

```bash
git push origin main
```
Expected: Commits from tasks 1-9 pushed.

- [ ] **Step 2: Retrigger the v0.9.0 tag (per CLAUDE.md procedure)**

```bash
git push origin --delete v0.9.0
git tag -d v0.9.0
git tag v0.9.0
git push origin v0.9.0
```
Expected: Tag deleted and re-pushed; workflow starts automatically.

- [ ] **Step 3: Watch the workflow**

Run: `gh run watch` (or open the Actions tab in the browser).
Expected: Three build jobs succeed → three package jobs succeed → release job succeeds. Total runtime around 15-20 minutes.

- [ ] **Step 4: Verify the release has exactly three assets**

Run: `gh release view v0.9.0 --json assets -q '.assets[].name' | sort`
Expected output (order may vary):
```
Tanghim-0.9.0-Linux.tar.gz
Tanghim-0.9.0-Windows.exe
Tanghim-0.9.0-macOS.pkg
```
No other files (no stray zips from the old workflow).

- [ ] **Step 5: Verify the macOS pkg payload structure**

```bash
gh release download v0.9.0 --pattern 'Tanghim-0.9.0-macOS.pkg' --dir /tmp/tanghim-verify
pkgutil --expand /tmp/tanghim-verify/Tanghim-0.9.0-macOS.pkg /tmp/tanghim-verify/expanded
ls /tmp/tanghim-verify/expanded
```
Expected: `Distribution`, `Transmitter.pkg`, `Receiver.pkg`, `M4L.pkg`.

Cleanup: `rm -rf /tmp/tanghim-verify`

- [ ] **Step 6: Verify the Linux tarball contents**

```bash
gh release download v0.9.0 --pattern 'Tanghim-0.9.0-Linux.tar.gz' --dir /tmp/tanghim-verify
tar -tzf /tmp/tanghim-verify/Tanghim-0.9.0-Linux.tar.gz | sort
```
Expected: entries for `install.sh`, `README.txt`, and bundles under `Tanghim-0.9.0-Linux/plugins/`.

Cleanup: `rm -rf /tmp/tanghim-verify`

- [ ] **Step 7: Manual verification (out-of-band, by the user)**

Download each installer from the release page and run it on a real target system. This is the only way to confirm the UX actually works — GitHub's runners can't self-test this. Deficiencies found here loop back to the relevant task.

---

## Self-review notes

- Every spec section is covered by a task:
  - macOS `.pkg` with 3 components → Tasks 2-4
  - Windows `.exe` with Inno Setup → Task 5
  - Linux `.tar.gz` + install script → Tasks 6-8
  - Version consistency guard → Task 1
  - Workflow rewrite (build / package / release) → Task 9
  - End-to-end validation → Task 10
- No placeholders — all code/commands are literal, all paths exact.
- File-naming convention stays consistent: `Tanghim-<version>-<OS>.<ext>` in every task that produces or consumes an installer name.
- Inno Setup version is pinned (6.2.2) per the Risks section.
- Per CLAUDE.md: no fallbacks added anywhere; failures surface loudly (set -euo pipefail, FATAL_ERROR, if-no-files-found: error, $LASTEXITCODE check).
