# Release Installers — Design

**Date:** 2026-04-13
**Status:** Approved — ready for implementation plan

## Goal

Replace the current zipped-plugin release artifacts with proper installable packages for macOS, Windows, and Linux, published automatically when a `v*` tag is pushed. End-users download one installer per platform, run it, and the plugins land in the correct directories with no manual unzipping.

## Scope

- Produce native installers for macOS (`.pkg`), Windows (`.exe`), and Linux (`.tar.gz` with install script).
- Installers are the **only** distributed artifacts — no standalone zips on the release page.
- Ship both plugins (Transmitter + Receiver) and the M4L Receiver device in a single installer per platform.
- Enforce version consistency between the git tag and `CMakeLists.txt` at build time.
- No uninstaller, no notarization, no code-signing certificates (ad-hoc signing on macOS stays).

## Out of scope

- Notarization / signed installers (ad-hoc only, macOS users will see Gatekeeper warning and right-click → Open).
- Uninstallers. Users delete plugin files manually if they want to remove them.
- Linux `.deb` / `.rpm` packages. Tarball + script only — matches audio plugin conventions.
- Auto-update inside the installer. The plugin already has its own in-app update check.

## Platform details

### macOS — `.pkg` distribution installer

**Tooling:** `pkgbuild` + `productbuild` (macOS built-ins, no extra installs on the runner).

**Structure:** One distribution `.pkg` containing three component packages:

| Component | Payload | Install root | Default |
|---|---|---|---|
| Tanghim (Transmitter) | `Tanghim.vst3`, `Tanghim.component`, `Tanghim.clap` | `/Library/Audio/Plug-Ins/VST3`, `/Library/Audio/Plug-Ins/Components`, `/Library/Audio/Plug-Ins/CLAP` | required |
| Tanghim Receiver | `Tanghim Receiver.vst3`, `Tanghim Receiver.component`, `Tanghim Receiver.clap` | same three system plugin folders | required |
| M4L Receiver | `Tanghim Receiver.amxd` | `~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/` | optional (checkbox) |

**Signing:** Each `.vst3`, `.component`, `.clap` is already ad-hoc codesigned in the existing build job. The `.pkg` itself is unsigned.

**Architecture:** Universal binary (arm64 + x86_64) — already produced by the existing CMake flag `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.

**M4L placement via postinstall script:** The M4L component's install root in `productbuild` is a system location, but the file belongs under the user's `~/Music/Ableton/...`. A postinstall script run by `productbuild` copies the bundled `.amxd` from a staging location to the current console user's home (`$USER` via `stat -f %Su /dev/console`). If M4L is unchecked, the script isn't invoked for that component.

**Output:** `Tanghim-<version>-macOS.pkg`

### Windows — Inno Setup `.exe`

**Tooling:** [Inno Setup 6](https://jrsoftware.org/isinfo.php), installed on the `windows-latest` runner via `choco install innosetup`.

**Privileges:** `PrivilegesRequired=admin` — the installer prompts UAC, writes to `C:\Program Files\Common Files\VST3` and `...\CLAP`.

**Components (user picks at install time):**

| Component | Payload | Install root | Default |
|---|---|---|---|
| Tanghim (Transmitter) | `Tanghim.vst3`, `Tanghim.clap` | `{commoncf64}\VST3`, `{commoncf64}\CLAP` | checked |
| Tanghim Receiver | `Tanghim Receiver.vst3`, `Tanghim Receiver.clap` | same | checked |
| M4L Receiver | `Tanghim Receiver.amxd` | `{userdocs}\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\` | unchecked |

**Note on M4L path:** Inno's `{userdocs}` resolves to the installing user's Documents folder. Because the installer runs elevated (admin), `{userdocs}` points to the admin profile, not the invoking user — wrong target. The `.iss` script uses the `[Files]` `runasoriginaluser` flag combined with a staging-then-copy pattern: the `.amxd` is installed to a staging directory under `{app}`, and a `[Run]` entry invokes `cmd /c copy` with `runasoriginaluser` to place it under the invoking user's `%USERPROFILE%\Documents\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim\`.

**Uninstaller:** Inno Setup creates an Add/Remove Programs entry and an uninstaller executable by default. To match the "no uninstaller" decision, the `.iss` uses `Uninstallable=no` and `CreateUninstallRegKey=no`, producing an install-only package.

**Output:** `Tanghim-<version>-Windows.exe`

### Linux — `.tar.gz` + install script

**Layout inside the tarball:**
```
Tanghim-<version>-Linux/
  plugins/
    Tanghim.vst3/
    Tanghim.clap
    Tanghim Receiver.vst3/
    Tanghim Receiver.clap
  install.sh
  README.txt
```

**`install.sh`:**
- Runs as the invoking user (no sudo).
- Creates `~/.vst3` and `~/.clap` if missing.
- Copies plugins into place with `cp -r`.
- Prints "Installation complete. Rescan plugins in your DAW." on success.
- Exits non-zero with a diagnostic message on any copy failure.
- Idempotent: re-running overwrites existing installs.

**No M4L on Linux** — Ableton Live doesn't run on Linux.

**Output:** `Tanghim-<version>-Linux.tar.gz`

## Version consistency check

**Problem:** A release tag `v0.9.1` can be pushed while `CMakeLists.txt` still says `0.9.0`, producing installers whose filenames say `0.9.1` but whose plugins report `0.9.0` internally.

**Fix:** A new CMake cache option `TANGHIM_EXPECTED_VERSION`. The workflow extracts the version from the tag (`${GITHUB_REF_NAME#v}`) and passes it in:

```cmake
if(DEFINED TANGHIM_EXPECTED_VERSION AND NOT PROJECT_VERSION VERSION_EQUAL TANGHIM_EXPECTED_VERSION)
  message(FATAL_ERROR
    "Version mismatch: tag says ${TANGHIM_EXPECTED_VERSION}, CMakeLists.txt says ${PROJECT_VERSION}")
endif()
```

Local builds without `-DTANGHIM_EXPECTED_VERSION=...` skip the check entirely — no friction for day-to-day development.

## Workflow structure

The existing `release.yml` has two jobs: `build` and `release`. We split `build` into two stages and add a packaging job:

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   build     │───▶│  package    │───▶│   release   │
│ (matrix:    │    │ (matrix:    │    │ (single:    │
│  mac/win/lx)│    │  mac/win/lx)│    │  ubuntu)    │
└─────────────┘    └─────────────┘    └─────────────┘
```

### `build` job (modified)
- Matrix per OS (unchanged).
- Adds `TANGHIM_EXPECTED_VERSION=${version}` to the CMake configure step, where `${version}` is `github.ref_name` with the leading `v` stripped.
- Runs tests.
- Codesigns macOS bundles (ad-hoc) — unchanged.
- **No longer zips the plugins.** Instead, uploads the raw plugin bundles as artifacts (`tanghim-plugins-<os>`).

### `package` job (new)
- Matrix per OS.
- `needs: build`.
- Downloads the plugin artifact for its OS.
- Builds the platform-specific installer:
  - macOS: runs `scripts/installer/macos/build-pkg.sh` → `.pkg`
  - Windows: installs Inno Setup, runs ISCC.exe on `scripts/installer/windows/tanghim.iss` → `.exe`
  - Linux: runs `scripts/installer/linux/build-tarball.sh` → `.tar.gz`
- Uploads the installer as artifact `tanghim-installer-<os>`.

### `release` job (modified)
- `needs: package` (not `build`).
- Downloads only the three installer artifacts.
- Deletes previous releases (unchanged).
- Publishes new release with three files: `Tanghim-<version>-macOS.pkg`, `Tanghim-<version>-Windows.exe`, `Tanghim-<version>-Linux.tar.gz`.
- Release notes body updates to reflect installer filenames and drop the per-format zip table.

## Repository layout additions

```
scripts/
  installer/
    macos/
      build-pkg.sh              # orchestrator: pkgbuild + productbuild
      distribution.xml          # component definition with M4L checkbox
      scripts/
        postinstall-m4l         # moves M4L to user's Ableton folder
    windows/
      tanghim.iss               # Inno Setup script
    linux/
      build-tarball.sh          # staging + tar invocation
      install.sh                # shipped inside tarball
      README.txt                # shipped inside tarball
```

Scripts take the version number as a command-line argument, not hardcoded — the workflow passes `${version}` in.

## Release notes body

The new body (written in the workflow YAML) is substantially shorter:

```markdown
## Downloads

| Platform | Installer |
|---|---|
| macOS (Universal) | Tanghim-<version>-macOS.pkg |
| Windows (x64) | Tanghim-<version>-Windows.exe |
| Linux (x64) | Tanghim-<version>-Linux.tar.gz |

### Installation

- **macOS:** Double-click the `.pkg`. On first launch you may need to right-click the plugin file in Finder → Open to bypass Gatekeeper (installer is ad-hoc signed, not notarized).
- **Windows:** Double-click the `.exe`. Admin rights required (installs to `Program Files`).
- **Linux:** `tar xzf Tanghim-<version>-Linux.tar.gz && cd Tanghim-<version>-Linux && ./install.sh`

### What's included

- Tanghim (Transmitter) — VST3, AU (macOS only), CLAP
- Tanghim Receiver — VST3, AU (macOS only), CLAP
- Tanghim Receiver for Ableton Live (M4L) — macOS and Windows installers, optional component

> **Ableton Live users:** Ableton doesn't support VST3 MIDI effects. Enable the M4L component during installation, or use MTS-ESP-capable synths.
```

## Testing

Fully validating installers requires running them on clean target systems — not feasible in CI without per-OS VMs. Minimum automated checks we'll add:

1. **Version check fires:** a CI smoke test that configures CMake with a deliberately-mismatched `TANGHIM_EXPECTED_VERSION` and expects the configure step to fail.
2. **Installer produced:** the `package` job fails if the expected output file doesn't exist at the expected path.
3. **macOS pkg payload:** `pkgutil --expand` the pkg and assert all three bundles are present.
4. **Windows exe sanity:** `Expand-Archive` won't work on Inno exes; instead, check the file exists and is non-trivial size (≥2 MB).
5. **Linux tarball payload:** `tar tzf` and grep for expected entries.

**Manual verification per release:** Download each installer from the published draft release and run it on a target machine once before announcing the release. No automation for this.

## Risks and mitigations

- **Inno Setup install on the runner.** Mitigation: pin Inno Setup version in the `choco install` command so upstream changes don't silently break the build.
- **macOS Gatekeeper warnings scare users.** Mitigation: spell out the "right-click → Open" workaround in the release notes.
- **M4L path on Windows under admin UAC.** Mitigation: `runasoriginaluser` pattern as designed. If it fails at release time, the failure is loud (M4L component simply doesn't land) and is caught in manual verification before the release is announced.
- **Installer size growth.** Three platforms × ~40 MB each is fine for GitHub Releases (2 GB per file limit).

## Open questions

None. All six clarifying questions answered during brainstorming.
