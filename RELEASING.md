# Releasing Tanghīm

## Versioning

While in beta the minor version stays at `0.9` and **every release bumps the patch**:
`0.9.1`, `0.9.2`, `0.9.3`… Reserve `0.10.0` for a large beta milestone if one is warranted,
and `1.0.0` for the first public release.

Patch-only increments keep versions trivially comparable as three integers, which is what
the in-plugin update check relies on — no pre-release suffix parsing.

**`CMakeLists.txt` line 2 is the single source of truth for the version.** Installer
filenames, the plugin's internal version string, the status-bar label and the update check
all derive from `PROJECT_VERSION`. The build fails if the git tag and `PROJECT_VERSION`
disagree (`TANGHIM_EXPECTED_VERSION` guard), so they cannot drift.

## Changelog — one source of truth

**`CHANGELOG.md` at the repo root is the only place release notes are written.**

- The **GitHub Release body** is generated from the matching `## [x.y.z]` section by the
  release workflow. Do not hand-write notes in the GitHub UI, and do not rely on
  `generate_release_notes` (raw commit subjects are written for us, not for users).
- The **website changelog** reads the same file (see "Website" below).

Write entries for **users, not for the repo**: say what changed in the plugin and what it
means for their work. Keep commit-level detail in the diary. Group under
`Added` / `Changed` / `Fixed` / `Removed`.

Accumulate entries under `## [Unreleased]` as you work; at release time rename that heading
to the new version with the date and start a fresh `## [Unreleased]`.

## Cutting a release

```bash
# 1. Move [Unreleased] entries under a new version heading + date in CHANGELOG.md,
#    and add the compare link at the bottom of the file.

# 2. Bump the version (single source of truth)
#    CMakeLists.txt line 2:  project(Tanghim VERSION 0.9.2)

# 3. Commit both together
git add CMakeLists.txt CHANGELOG.md
git commit -m "release: v0.9.2"

# 4. Tag and push — CI builds all platforms and publishes the release
git tag v0.9.2
git push origin main v0.9.2
```

To **retrigger the same version** after a fix (no version bump):

```bash
git push origin --delete v0.9.2
git tag -d v0.9.2
git tag v0.9.2
git push origin v0.9.2
```

## Website changelog — TODO

Not yet implemented. When it is, it must read from the same source:

- **Source:** `CHANGELOG.md` in this repo — never a second hand-maintained copy.
- **Suggested approach:** the Astro site fetches the GitHub Releases API
  (`GET /repos/Music-Intelligence-Lab/tanghim/releases`) at build time and renders the
  release bodies, which the workflow already populated from `CHANGELOG.md`. That keeps
  GitHub authoritative and avoids the website depending on this repo's file layout.
- **Alternative:** import `CHANGELOG.md` directly if the website is given access to this
  repo. Simpler, but then GitHub and the website can diverge if one is edited by hand.
- Add a `/changelog` route, link it from the download page and from the release
  notification in the plugin.
- Translations: the EN changelog is authoritative; AR/FR can fall back to EN (same pattern
  as the FAQ).

## Update notifications

The plugin checks `GET /repos/Music-Intelligence-Lab/tanghim/releases/latest` and compares
`tag_name` (minus the leading `v`) against `PROJECT_VERSION` as three integers. A newer
version lights an indicator in the status bar linking to the release page.

This is **notify-only** — Tanghīm never replaces its own binaries. Plugin files are
memory-mapped by the host while loaded, and installers need elevated privileges on both
Windows and macOS, so a self-replacing update could corrupt a running install. The user
quits their DAW and runs the installer.

Automatic checking is on by default and can be disabled in **Settings → Check for updates
automatically** (persisted in `~/Library/Tanghim/settings.json` as `autoUpdateCheck`).
Disabling it leaves the manual **Check for Updates** button working.
