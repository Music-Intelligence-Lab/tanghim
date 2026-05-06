# Vendored: ODDSound MTS-ESP Max Package

The directory `m4l/MTS-ESP-Max-Package/` contains a verbatim vendored copy
of ODDSound's MTS-ESP Max Package. Our installer drops this directory
into the user's Max library on install (one copy per installed Max
version — Max 8, Max 9, etc.), so frozen `.amxd` files can find the
externals via Max's package search.

## Source

- Repository: https://github.com/ODDSound/MTS-ESP-Max-Package
- Pinned commit: `1319621f969da641af4df4201aadbab82943093d` (2025-12-22:
  "Rebuild with latest libMTSClient.")
- License: 0BSD (see `m4l/MTS-ESP-Max-Package/LICENSE`)

## Why vendored, not a submodule

- 0BSD permits redistribution without conditions.
- Vendoring keeps the repo self-contained — no `git clone --recursive`
  required.
- ~1.3 MB, version-pinned, reproducible CI builds.
- Update path (below) is mechanical when needed.

## Why the FULL package, not just the .mxo

Max only fully registers a package when it sees the canonical layout:
`<PackageName>/{externals,init,help,docs,package-info.json,...}` at
`~/Documents/Max <N>/Library/`. Dropping just the `.mxo` binary outside
this layout produces a partial registration where:

- Frozen `.amxd` files don't load the external (Max's frozen-load path
  doesn't search patcher-adjacent directories).
- Reference and help patches don't resolve.

The full package layout fixes both. (Verified empirically during the
2026-05-06 native M4L Receiver rebuild — see `diary/2026-05-06.md`.)

## Updating to a newer upstream commit

```bash
cd /tmp && rm -rf mts-update && mkdir mts-update && cd mts-update
git clone --depth 1 https://github.com/ODDSound/MTS-ESP-Max-Package.git .
git rev-parse HEAD  # record this in this VENDORING file's "Pinned commit"

cd "$REPO_ROOT"
rm -rf m4l/MTS-ESP-Max-Package
cp -R /tmp/mts-update m4l/MTS-ESP-Max-Package
rm -rf m4l/MTS-ESP-Max-Package/.git

# Verify the macOS external is still a universal binary:
file "m4l/MTS-ESP-Max-Package/externals/MTS-ESP.mtof.mxo/Contents/MacOS/MTS-ESP.mtof"
# Expected: Mach-O universal binary with 2 architectures: [x86_64...] [arm64...]
```

Commit the update with the new sha and date in this file.
