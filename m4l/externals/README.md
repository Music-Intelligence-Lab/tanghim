# ODDSound MTS-ESP Max Package — pinned externals

Native Max externals from ODDSound's MTS-ESP Max Package, used by the
Tanghim M4L Receiver patches to read live MTS-ESP tuning data without
hosting any plugin in `vst~`.

## Pinned source

- Repository: https://github.com/ODDSound/MTS-ESP-Max-Package
- Commit: `1319621f969d` (`2025-12-22`)
- License: 0BSD (in upstream `LICENSE`)

## Externals shipped

| File | Platform | Role |
|---|---|---|
| `MTS-ESP.mtof.mxo/` (bundle directory) | macOS (universal: arm64 + x86_64) | Note → cents/freq + filter flag |
| `MTS-ESP.mtof.mxe64` | Windows x64 | same |

We do not ship `MTS-ESP.mtof~`, `mc.MTS-ESP.mtof~`, or `MTS-ESP.ftom`
because our patches don't use them.

## Updating

To update to a newer upstream commit:

```bash
cd /tmp && rm -rf mts-esp-max && mkdir mts-esp-max && cd mts-esp-max
git clone --depth 1 https://github.com/ODDSound/MTS-ESP-Max-Package.git .
git rev-parse HEAD  # record this in the table above
cp -R externals/MTS-ESP.mtof.mxo "$REPO_ROOT/m4l/externals/"
cp externals/MTS-ESP.mtof.mxe64 "$REPO_ROOT/m4l/externals/"
file "$REPO_ROOT/m4l/externals/MTS-ESP.mtof.mxo/Contents/MacOS/MTS-ESP.mtof"  # verify universal
```
