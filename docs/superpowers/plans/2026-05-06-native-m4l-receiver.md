# Native M4L Receiver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `vst~`+VST3+JS M4L Receiver with two pure-native Max patches that read MTS-ESP tuning via ODDSound's MTS-ESP Max Package and process MIDI in stock Max objects (`poly`, `coll`, `expr`, `xbendout`). No `vst~`, no JavaScript, no VST2.

**Architecture:** Each `.amxd` is a thin native Max patch. `MTS-ESP.mtof` reads tuning from MTS-ESP shared memory directly. MPE allocation via `[poly @voices 15 @steal 1]`. Mono PB legato via `[coll]` note stack. Cents → 14-bit PB via `[expr]` then `[xbendout]`. Held-note retune polled at 10 Hz over the active `[coll heldNotes]`.

**Tech Stack:** Max 9 (native objects + ODDSound MTS-ESP Max Package externals, 0BSD license), Python (py2max for patch generation), Inno Setup (Windows installer), pkgbuild (macOS installer), GitHub Actions (CI).

**Test approach (honest framing):** Max patches have no unit-test framework. The C++ side is unchanged so no new C++ tests are added. Verification is structured as: (a) externals load and resolve (smoke test), (b) `.amxd` opens in Live without errors, (c) end-to-end test in Live with the headline metric being **recorded clip note positions on-grid at audio buffer 1024**. Performative unit tests around patch generation would be noise.

**Pinning ODDSound's externals:** the implementation pins to a specific commit of `https://github.com/ODDSound/MTS-ESP-Max-Package` rather than HEAD, recorded in the README at `m4l/externals/`. The pinned binaries are universal Mach-O (arm64 + x86_64) on macOS — verified before plan creation.

---

### Task 1: Source ODDSound MTS-ESP Max Package externals and commit

**Files:**
- Create: `m4l/externals/MTS-ESP.mtof.mxo/` (macOS bundle directory) — only the `mtof` external is required for our devices; we don't ship `mtof~`, `mc.mtof~`, or `ftom`
- Create: `m4l/externals/MTS-ESP.mtof.mxe64` (Windows binary)
- Create: `m4l/externals/README.md` (pinning + provenance + license note)

The macOS external is a folder bundle. The Windows external is a single file. Both must be committed verbatim.

- [ ] **Step 1: Determine which commit to pin**

Run:
```bash
curl -sL "https://api.github.com/repos/ODDSound/MTS-ESP-Max-Package/commits?per_page=1" | python3 -c "import json,sys; c=json.load(sys.stdin)[0]; print(c['sha'], c['commit']['author']['date'], c['commit']['message'].splitlines()[0])"
```
Expected: prints latest commit sha + date + first line of message. Record this sha for the README.

- [ ] **Step 2: Clone the repo at that commit and copy externals into place**

```bash
mkdir -p /tmp/mts-esp-max && cd /tmp/mts-esp-max
git clone --depth 1 https://github.com/ODDSound/MTS-ESP-Max-Package.git .
git rev-parse HEAD  # confirm matches Step 1's sha; record this
mkdir -p /Users/khyamallami/code_projects/tanghim/m4l/externals
cp -R externals/MTS-ESP.mtof.mxo /Users/khyamallami/code_projects/tanghim/m4l/externals/
cp externals/MTS-ESP.mtof.mxe64 /Users/khyamallami/code_projects/tanghim/m4l/externals/
```

- [ ] **Step 3: Verify the macOS external is universal**

```bash
file "/Users/khyamallami/code_projects/tanghim/m4l/externals/MTS-ESP.mtof.mxo/Contents/MacOS/MTS-ESP.mtof"
```
Expected: `Mach-O universal binary with 2 architectures: [x86_64...] [arm64...]`. If not, **STOP and report BLOCKED** — Live's plugin scanner runs x86_64 under Rosetta on Apple Silicon and a non-universal external would not be discoverable.

- [ ] **Step 4: Write provenance README**

Create `m4l/externals/README.md` with this content (substituting the actual pinned sha + date from Step 1):

```markdown
# ODDSound MTS-ESP Max Package — pinned externals

Native Max externals from ODDSound's MTS-ESP Max Package, used by the
Tanghim M4L Receiver patches to read live MTS-ESP tuning data without
hosting any plugin in `vst~`.

## Pinned source

- Repository: https://github.com/ODDSound/MTS-ESP-Max-Package
- Commit: `<SHA from Step 1>` (`<DATE from Step 1>`)
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
```

- [ ] **Step 5: Commit**

```bash
cd /Users/khyamallami/code_projects/tanghim
git add m4l/externals/
git status --short  # verify no other unrelated paths staged
git commit -m "$(cat <<'EOF'
m4l: pin ODDSound MTS-ESP Max Package externals (mtof.mxo + mxe64)

Native Max externals from ODDSound's MTS-ESP-Max-Package repo,
pinned to commit <SHA>. Used by the new M4L Receiver patches to
read MTS-ESP tuning without hosting any plugin in vst~.

License: 0BSD (upstream).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

- [ ] **Step 6: Smoke test — install externals to user Max library and verify Max picks them up**

```bash
# Live's Max-package search path picks externals up from the user library.
# We're testing the binaries themselves, not yet the .amxd patches.
mkdir -p ~/Documents/Max\ 9/Library/MTS-ESP-Max-Package
cp -R "$HOME/code_projects/tanghim/m4l/externals/MTS-ESP.mtof.mxo" ~/Documents/Max\ 9/Library/MTS-ESP-Max-Package/
codesign --force --deep --sign - ~/Documents/Max\ 9/Library/MTS-ESP-Max-Package/MTS-ESP.mtof.mxo
```

Open Max (standalone, not via Live). Create a new patch. Add `[MTS-ESP.mtof]` and verify it doesn't show as unresolved (red). If unresolved, the binary isn't being found — debug Max's package search path.

This step is verification only; no commit.

---

### Task 2: Refactor `m4l/generate_patch.py` — extract common helpers

**Files:**
- Create: `m4l/_patch_common.py` — shared helpers for both device generators
- Modify (do not yet delete): `m4l/generate_patch.py` — extract its `freeze_and_save()` logic into `_patch_common.py` so the new generators can reuse it

The existing `m4l/generate_patch.py` produces a single `.amxd` with vst~+JS. We need to reuse only its freeze-to-amxd logic (the `ampf` header / `mmmmmeta` tag / `ptch`+`dlst` sections — already debugged and working). Extract that into a shared module.

- [ ] **Step 1: Read the current generator**

```bash
cd /Users/khyamallami/code_projects/tanghim
wc -l m4l/generate_patch.py
grep -n "def \|ampf\|mmmmm\|ptch\|dlst" m4l/generate_patch.py
```
Identify the function (or block) that writes the .amxd binary wrapper around a `.maxpat` file. This is the freeze logic.

- [ ] **Step 2: Create `m4l/_patch_common.py`**

Create the file with this content (substituting any imports/helpers needed by the freeze logic — verify by reading the existing `generate_patch.py`):

```python
"""Shared helpers for generating Tanghim Receiver M4L patches.

Both MPE and Mono PB devices share:
  - the .amxd freeze logic (ampf / mmmmmeta=4 / ptch / dlst)
  - the basic patch skeleton (live.midiin -> patch logic -> live.midiout)
  - the PB range numbox + status indicator UI

Differences are limited to:
  - is_mpe patcher metadata (MPE only)
  - title text
  - default PB range value
  - per-device MIDI processing topology (poly allocator vs note stack)
"""

import struct
from pathlib import Path
from py2max import Patcher


def freeze_and_save(p: Patcher, maxpat_path: str, amxd_path: str,
                    embedded_files: list[tuple[str, bytes]] | None = None) -> None:
    """Save the patcher as a frozen .amxd MIDI effect.

    Args:
      p: py2max Patcher (already populated)
      maxpat_path: path to write the .maxpat
      amxd_path: path to write the .amxd
      embedded_files: optional list of (filename, contents) tuples to
        embed via the dlst directory. Default None (no embeds).
    """
    # Move the freeze code from m4l/generate_patch.py here, verbatim
    # (it's the section that builds: ampf header + mmmmmeta tag (value 4 for
    # frozen MIDI effect) + ptch section containing mx@c header + concatenated
    # files + dlst directory).
    raise NotImplementedError(
        "Extract from m4l/generate_patch.py the freeze-write block that "
        "produces the .amxd binary wrapper. Move it here verbatim so both "
        "device generators can reuse it.")
```

- [ ] **Step 3: Move the freeze code from `generate_patch.py` to `_patch_common.py`**

Open `m4l/generate_patch.py`. Find the function/block that writes the `.amxd` (everything after the `Patcher.save_as(...)` call that produces the .maxpat). That block builds the binary `.amxd`. Move it into `_patch_common.py`'s `freeze_and_save()` body, replacing the `NotImplementedError`.

In `generate_patch.py`, replace the moved block with:
```python
from _patch_common import freeze_and_save
freeze_and_save(p, OUT_MAXPAT, OUT_AMXD,
                embedded_files=[("mts_midi_effect.js", js_contents)])
```
(matching whatever variable names the existing script uses)

- [ ] **Step 4: Verify by regenerating the OLD device**

```bash
cd /Users/khyamallami/code_projects/tanghim
python3 m4l/generate_patch.py
# Compare the regenerated .amxd to the one currently committed:
shasum -a 256 "m4l/Tanghim Receiver.amxd"
git show HEAD:"m4l/Tanghim Receiver.amxd" | shasum -a 256
```
Expected: SHAs match (regeneration produces an identical .amxd to what's checked in). If they differ, the extraction lost or reordered bytes — debug before proceeding.

- [ ] **Step 5: Commit the refactor**

```bash
git add m4l/_patch_common.py m4l/generate_patch.py
git commit -m "$(cat <<'EOF'
m4l: extract freeze_and_save into _patch_common.py

Shared helper for both new (MPE + Mono PB) device generators.
The existing single-device generate_patch.py now imports from
_patch_common; regenerating the old .amxd produces a byte-identical
file (verified).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Build the MPE allocator subpatch

**Files:**
- Create: `m4l/mpe_voice.maxpat` — single-voice MPE subpatcher used by `[poly]`

This is the per-voice subpatch that `[poly @voices 15]` instantiates 15 times. Each instance owns one MPE channel (2..16). The subpatch:
- receives `(note, velocity, cents, pb_range, user_pb_offset)` via `[in 1]`
- queries `[thispoly~]` for its voice index → MPE channel = index + 2
- emits PB then Note On then (later) Note Off on its assigned channel

Hand-write the `.maxpat` JSON because py2max doesn't directly support generating poly subpatches with `[thispoly~]` registered correctly.

- [ ] **Step 1: Reference an existing MPE-allocator pattern**

Open Max (standalone). Use `[poly~]` help patch for reference: `Help → Open Help...` then search "poly". Confirm the pattern:
- Subpatch top-level has `[in 1]`, `[in 2]`, ... and `[out 1]`, `[out 2]`, ...
- Inside subpatch, `[thispoly~]` outputs the voice index (1-based) on its right outlet when banged

- [ ] **Step 2: Author `m4l/mpe_voice.maxpat`**

Create the file by hand-writing the patcher JSON (or by authoring it in Max GUI and saving). Required topology:

```
[in 1]   ← receives list: (note, velocity, cents, pb_range_semitones, user_pb_offset_14bit)
   │
   [zl group 5]                     ← bundle into a 5-element list
   │
   [unpack 0 0 0. 0 0]              ← (note, velocity, cents, pb_range, user_pb)
   │     │     │      │       │
   │     │     │      │       └─ store user_pb in [v userPb]
   │     │     │      └─ store pb_range in [v pbRange]
   │     │     └────────────── compute combined_pb:
   │     │                      [expr int(8192 + ($f1 / ($f2 * 100.)) * 8191) + $i3 - 8192]
   │     │                      then [clip 0 16383]
   │     │
   │     ├─ velocity > 0 ? Note On : Note Off
   │     │
   ├──────────────── [thispoly~] → right outlet → +1 → [+ 1] → channel (= voice + 1, since
   │                                                            voice index 1..15 → ch 2..16)
   │
   ▼
   ┌───────────────────────────┐
   │ if Note On (vel > 0):      │
   │   1. emit [xbendout ch] PB │
   │   2. emit [noteout ch]     │
   │ if Note Off (vel == 0):    │
   │   1. emit [noteout ch] 0   │
   │   2. (no PB reset!)        │
   └───────────────────────────┘
```

The hand-written JSON for this subpatcher is too long to include here verbatim. Author it in Max GUI:
1. Open Max, create a new patch.
2. Add the objects above.
3. Wire them.
4. Save as `m4l/mpe_voice.maxpat`.

(If you prefer to hand-author the JSON directly, use the existing `m4l/Tanghim Receiver.maxpat` from `git show HEAD:m4l/Tanghim\ Receiver.maxpat` as a structural reference.)

- [ ] **Step 3: Smoke test the subpatch in Max**

Open `mpe_voice.maxpat` in Max. Wrap it in `[poly mpe_voice 15 @steal 1]` in a test patch. Send a list `60 100 50.0 48 8192` to the poly object. Verify a Note On with PB lands on channel 2 (the first allocated voice). Send another note → channel 3. Send same note 60 → fresh channel.

If `[poly]`'s default `@steal 1` does NOT allocate fresh channels for same-pitch overlap, switch to a `[coll]`-based allocator. **Document the resulting allocator pattern in this step's notes** — it determines the topology the next task uses.

- [ ] **Step 4: Commit**

```bash
git add m4l/mpe_voice.maxpat
git commit -m "$(cat <<'EOF'
m4l: MPE per-voice subpatch for poly allocator

Single voice handler used by [poly @voices 15 @steal 1] in the MPE
Receiver. Receives (note, vel, cents, pb_range, user_pb_offset),
derives MPE channel from thispoly~ voice index + 1, emits PB then
Note On (or Note Off without PB reset) on the assigned channel.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Generate `Tanghim MPE Receiver.amxd`

**Files:**
- Create: `m4l/generate_mpe_patch.py`
- Create: `m4l/Tanghim MPE Receiver.maxpat` (generated)
- Create: `m4l/Tanghim MPE Receiver.amxd` (generated)

- [ ] **Step 1: Create `m4l/generate_mpe_patch.py`**

```python
"""Generate Tanghim MPE Receiver.amxd.

Native Max patch — no vst~, no JS. is_mpe: 1 patcher flag is REQUIRED
or Live collapses output to channel 1.
"""

from py2max import Patcher
from _patch_common import freeze_and_save

OUT_MAXPAT = "m4l/Tanghim MPE Receiver.maxpat"
OUT_AMXD = "m4l/Tanghim MPE Receiver.amxd"


def main() -> None:
    p = Patcher(path=OUT_MAXPAT, layout="horizontal", validate_connections=False)
    p.is_mpe = 1

    # ── MIDI input + parsing ────────────────────────────────────────
    midi_in = p.add("live.midiin", patching_rect=[20, 20, 100, 22])
    parse = p.add("midiparse", patching_rect=[20, 60, 100, 22])
    p.add_line(midi_in, parse)

    # parse outlets: 0=note, 1=velocity, 2=PB14, 3=ch, 4=CC, 5=program, 6=aftertouch
    # We use 0,1 for notes (with channel from outlet 3), 2 for user PB wheel.

    # ── User PB tracking (per-channel offset) ───────────────────────
    user_pb_coll = p.add("coll userPbOffset @embed 1",
                         patching_rect=[400, 60, 240, 22],
                         varname="userPbColl")
    # When PB arrives: store it under its channel index
    pb_to_offset = p.add("expr $i1 - 8192", patching_rect=[400, 100, 120, 22])
    pb_pack = p.add("pack 0 0", patching_rect=[400, 130, 80, 22])  # (channel, offset)
    pb_prepend = p.add("prepend store", patching_rect=[400, 160, 100, 22])
    p.add_line(parse, pb_to_offset, outlet=2)
    p.add_line(pb_to_offset, pb_pack, outlet=0, inlet=1)
    p.add_line(parse, pb_pack, outlet=3, inlet=0)  # channel
    p.add_line(pb_pack, pb_prepend)
    p.add_line(pb_prepend, user_pb_coll)

    # ── MTS-ESP tuning query for incoming note ──────────────────────
    mtof = p.add("MTS-ESP.mtof", patching_rect=[20, 120, 120, 22],
                 varname="mtof")
    p.add_line(parse, mtof, outlet=0)  # bang note → query

    # mtof outlets: 0=freq, 1=ratio, 2=semitones, 3=filter

    # ── Filter-note gate ────────────────────────────────────────────
    filter_gate = p.add("gate", patching_rect=[20, 200, 60, 22],
                        varname="filterGate")
    p.add_line(mtof, filter_gate, outlet=3, inlet=0)  # control input
    p.add_line(parse, filter_gate, outlet=0, inlet=1)  # note value

    # ── Cents from semitones ────────────────────────────────────────
    cents = p.add("expr $f1 * 100.", patching_rect=[160, 200, 100, 22])
    p.add_line(mtof, cents, outlet=2)

    # ── Held-note tracking ──────────────────────────────────────────
    held_coll = p.add("coll heldNotes @embed 1",
                      patching_rect=[700, 200, 200, 22],
                      varname="heldColl")
    # ... (note-on adds, note-off removes — wired up by the per-voice subpatch)

    # ── PB Range parameter ──────────────────────────────────────────
    pb_range = p.add("live.numbox",
                     patching_rect=[260, 20, 60, 22],
                     varname="pbRange",
                     parameter_initial_enable=1,
                     parameter_initial=48,
                     parameter_range=[1, 96],
                     parameter_type=1,  # int
                     parameter_longname="PB Range",
                     parameter_shortname="PB Range")

    # ── MPE allocator (poly + per-voice subpatch) ───────────────────
    poly = p.add("poly mpe_voice 15 @steal 1",
                 patching_rect=[400, 280, 200, 22],
                 varname="poly")
    # Inputs to poly: list (note, velocity, cents, pb_range, user_pb_offset)
    # Outputs from poly (merged across voices):
    #   outlet 1: PB messages (already on the right channel)
    #   outlet 2: Note On/Off (already on the right channel)

    # ── MPE Configuration RPN on load ───────────────────────────────
    loadbang = p.add("loadbang", patching_rect=[20, 320, 60, 22])
    rpn_delay = p.add("delay 50", patching_rect=[100, 320, 60, 22])
    rpn_msg = p.add(
        # CC 100=6, CC 101=0, CC 6=15 on channel 1 = MPE Lower Zone, 15 voices
        'message 176 100 6 176 101 0 176 6 15',
        patching_rect=[180, 320, 240, 22])
    p.add_line(loadbang, rpn_delay)
    p.add_line(rpn_delay, rpn_msg)

    # ── Held-note retune metro ──────────────────────────────────────
    metro = p.add("metro 100", patching_rect=[700, 280, 80, 22])
    metro_toggle = p.add("toggle", patching_rect=[700, 250, 22, 22])
    p.add_line(metro_toggle, metro)
    # Metro fires → iterate held coll → bang mtof per held note → emit updated PB
    # (Detail wiring: see m4l/_patch_common.py helper add_held_retune_block)

    # ── Output ──────────────────────────────────────────────────────
    midi_format = p.add("midiformat", patching_rect=[400, 400, 100, 22])
    midi_out = p.add("live.midiout", patching_rect=[400, 440, 100, 22])
    p.add_line(poly, midi_format, outlet=0)
    p.add_line(midi_format, midi_out)

    # ── Title comment ───────────────────────────────────────────────
    p.add("comment",
          text="Tanghīm MPE Receiver",
          patching_rect=[20, 470, 320, 22],
          fontsize=14.0)

    freeze_and_save(p, OUT_MAXPAT, OUT_AMXD)
    print(f"Generated: {OUT_MAXPAT}")
    print(f"Generated: {OUT_AMXD}")


if __name__ == "__main__":
    main()
```

**Important caveats:**
- The exact wiring of the held-note retune metro to `MTS-ESP.mtof` (iterating the coll, banging mtof per entry, comparing to last-known PB, emitting updates) is genuinely tricky to express in py2max-via-Python. **If py2max struggles with the dispatch logic**, factor that block into a hand-authored sub-patcher (`m4l/held_retune.maxpat`) and reference it via `[p held_retune]`. Don't fight py2max into being a full Max programming environment.
- The PB-wheel-combining math inside the poly subpatch (Task 3) reads from `[v userPb]` and `[v pbRange]` shared variables; the main patch sets these via `[s userPb]` and `[s pbRange]` whenever they change.

- [ ] **Step 2: Run the generator**

```bash
cd /Users/khyamallami/code_projects/tanghim
python3 m4l/generate_mpe_patch.py
```
Expected output:
```
Generated: m4l/Tanghim MPE Receiver.maxpat
Generated: m4l/Tanghim MPE Receiver.amxd
```

- [ ] **Step 3: Verify the .amxd format**

```bash
xxd "m4l/Tanghim MPE Receiver.amxd" | head -3
```
Expected: First 8 bytes are `61 6d 70 66` (`ampf`) followed by size; the meta tag is `mmmmm` with value `04 00 00 00` (4 = frozen MIDI effect).

- [ ] **Step 4: Verify is_mpe flag in the .maxpat**

```bash
grep -c '"is_mpe" : 1' "m4l/Tanghim MPE Receiver.maxpat"
```
Expected: at least `1`.

- [ ] **Step 5: Commit**

```bash
git add m4l/generate_mpe_patch.py "m4l/Tanghim MPE Receiver.maxpat" "m4l/Tanghim MPE Receiver.amxd"
git commit -m "$(cat <<'EOF'
m4l: generate Tanghim MPE Receiver.amxd (native Max, no vst~ or JS)

Reads MTS-ESP tuning via MTS-ESP.mtof external; allocates MPE
channels via [poly mpe_voice 15 @steal 1]; emits 14-bit PB then
Note On (or Note Off without PB reset) per voice. is_mpe flag set.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: Generate `Tanghim Mono PB Receiver.amxd`

**Files:**
- Create: `m4l/generate_monopb_patch.py`
- Create: `m4l/Tanghim Mono PB Receiver.maxpat` (generated)
- Create: `m4l/Tanghim Mono PB Receiver.amxd` (generated)

- [ ] **Step 1: Create `m4l/generate_monopb_patch.py`**

Topology differs from MPE in three ways:
1. NO `is_mpe` patcher flag.
2. NO `[poly]` allocator. Instead a single `[coll noteStack]` for last-note-priority legato.
3. Default PB range = 2 semitones.

```python
"""Generate Tanghim Mono PB Receiver.amxd.

Single-channel output (channel 1). Last-note-priority legato note
stack via [coll noteStack].
"""

from py2max import Patcher
from _patch_common import freeze_and_save

OUT_MAXPAT = "m4l/Tanghim Mono PB Receiver.maxpat"
OUT_AMXD = "m4l/Tanghim Mono PB Receiver.amxd"


def main() -> None:
    p = Patcher(path=OUT_MAXPAT, layout="horizontal", validate_connections=False)
    # NO p.is_mpe = 1 (this is the mono variant)

    midi_in = p.add("live.midiin", patching_rect=[20, 20, 100, 22])
    parse = p.add("midiparse", patching_rect=[20, 60, 100, 22])
    p.add_line(midi_in, parse)

    # MTS-ESP query (same as MPE)
    mtof = p.add("MTS-ESP.mtof", patching_rect=[20, 120, 120, 22],
                 varname="mtof")
    p.add_line(parse, mtof, outlet=0)

    # Filter-note gate (same as MPE)
    filter_gate = p.add("gate", patching_rect=[20, 200, 60, 22])
    p.add_line(mtof, filter_gate, outlet=3, inlet=0)
    p.add_line(parse, filter_gate, outlet=0, inlet=1)

    # Cents from semitones
    cents = p.add("expr $f1 * 100.", patching_rect=[160, 200, 100, 22])
    p.add_line(mtof, cents, outlet=2)

    # Note stack — last-note-priority
    note_stack = p.add("coll noteStack @embed 1",
                       patching_rect=[400, 200, 200, 22],
                       varname="noteStack")
    # ... wiring: Note On pushes (note, semitones); Note Off pops; on pop,
    # if stack non-empty, recompute PB for new top-of-stack note (legato recall).
    # If py2max struggles, use a hand-authored [p mono_stack] subpatcher.

    # PB Range parameter (default 2)
    pb_range = p.add("live.numbox",
                     patching_rect=[260, 20, 60, 22],
                     varname="pbRange",
                     parameter_initial_enable=1,
                     parameter_initial=2,
                     parameter_range=[1, 96],
                     parameter_type=1,
                     parameter_longname="PB Range",
                     parameter_shortname="PB Range")

    # User PB tracking — single channel (1)
    # ... [coll userPbOffset] keyed by channel, but mono only writes to ch 1
    # ... combined_pb = clamp(microbend + userPbOffset, 0, 16383)
    # ... [xbendout 1] then [noteout 1]

    # Held-note retune metro (same as MPE but only one slot active)
    metro = p.add("metro 100", patching_rect=[700, 280, 80, 22])
    metro_toggle = p.add("toggle", patching_rect=[700, 250, 22, 22])
    p.add_line(metro_toggle, metro)

    # Output
    midi_format = p.add("midiformat", patching_rect=[400, 400, 100, 22])
    midi_out = p.add("live.midiout", patching_rect=[400, 440, 100, 22])
    p.add_line(midi_format, midi_out)

    # Title
    p.add("comment",
          text="Tanghīm Mono PB Receiver",
          patching_rect=[20, 470, 320, 22],
          fontsize=14.0)

    freeze_and_save(p, OUT_MAXPAT, OUT_AMXD)
    print(f"Generated: {OUT_MAXPAT}")
    print(f"Generated: {OUT_AMXD}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run the generator and verify**

```bash
python3 m4l/generate_monopb_patch.py
xxd "m4l/Tanghim Mono PB Receiver.amxd" | head -3
grep -c '"is_mpe" : 1' "m4l/Tanghim Mono PB Receiver.maxpat" || echo "0 (expected for mono)"
```
Expected: `ampf` header on the .amxd; **`0`** (no is_mpe flag) for the maxpat grep.

- [ ] **Step 3: Commit**

```bash
git add m4l/generate_monopb_patch.py "m4l/Tanghim Mono PB Receiver.maxpat" "m4l/Tanghim Mono PB Receiver.amxd"
git commit -m "$(cat <<'EOF'
m4l: generate Tanghim Mono PB Receiver.amxd (native Max, no vst~ or JS)

Single-channel output, last-note-priority legato via [coll noteStack].
No is_mpe flag. No PB reset on Note Off (preserves release tail).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 6: End-to-end test in Ableton Live (HEADLINE METRIC) — MANUAL

**Files:**
- No file changes; manual verification only

This is the headline criterion of the entire rebuild. If recording jitter at buffer 1024 is still present, the rebuild has not fixed the problem — escalate to user.

- [ ] **Step 1: Install all artifacts to Live's user library**

```bash
DEST=~/Music/Ableton/User\ Library/Presets/MIDI\ Effects/Max\ MIDI\ Effect/Tanghim
mkdir -p "$DEST"
rm -rf "$DEST/Tanghim MPE Receiver.amxd" "$DEST/Tanghim Mono PB Receiver.amxd" "$DEST/MTS-ESP.mtof.mxo"
cp "m4l/Tanghim MPE Receiver.amxd" "$DEST/"
cp "m4l/Tanghim Mono PB Receiver.amxd" "$DEST/"
cp -R "m4l/externals/MTS-ESP.mtof.mxo" "$DEST/"
codesign --force --deep --sign - "$DEST/MTS-ESP.mtof.mxo"
ls -la "$DEST"
```

Also ensure the user-library Max package picks up the external (alternative path Live searches):
```bash
mkdir -p ~/Documents/Max\ 9/Library/MTS-ESP-Max-Package
cp -R m4l/externals/MTS-ESP.mtof.mxo ~/Documents/Max\ 9/Library/MTS-ESP-Max-Package/ 2>/dev/null || true
codesign --force --deep --sign - ~/Documents/Max\ 9/Library/MTS-ESP-Max-Package/MTS-ESP.mtof.mxo 2>/dev/null || true
```

- [ ] **Step 2: Devices load**

Reopen Live. Drop both `.amxd` files. Open the Max Console. Verify no "external not found" / "MTS-ESP.mtof not loaded" errors.

- [ ] **Step 3: Tuning correctness (MPE)**

Setup: Tanghim Transmitter on track A; **MPE Receiver** before a soft synth on track B; synth in MPE mode. Select Maqam Hijaz on the Transmitter. Play C3–C4 chromatic scale. Verify pitches match the maqam (audibly distinct microtonal degrees).

- [ ] **Step 4: Mono PB mode**

Replace MPE with **Mono PB Receiver**, synth in single-channel mode (PB range 2). Play melody — every note in tune. Hold A, hold B, release A — note B continues at its tuned pitch (legato).

- [ ] **Step 5: Same-pitch MPE allocation**

Switch back to MPE. Trigger note 60 → 60 → 60 with overlap (hold pedal + arp). Each successive 60 lands on a fresh MPE channel; existing 60s ring out cleanly.

- [ ] **Step 6: Filter-note**

Configure Transmitter with a maqam that filters certain notes. Play those notes — Receiver drops them (no sound).

- [ ] **Step 7: PB wheel combining**

Hold tuned note. Move pitch wheel up. Pitch bends FROM the maqam-tuned pitch (not from 12-TET).

- [ ] **Step 8: Held-note retune during automation**

Hold a chord. Change maqam at Transmitter. Chord pitches must update within ~100ms.

- [ ] **Step 9: HEADLINE — Recording jitter at buffer 1024**

Set Live audio buffer to 1024. With MPE Receiver on track B, arm and record a 4-note arpeggio at 120bpm 16th notes. Stop. Open the recorded clip in Note Editor.

**Expected:** notes on-grid (Live's normal sub-millisecond jitter is fine). NOT the random ±5–12ms displacement seen with the JS-based device.

If jitter persists: take a screenshot, escalate to user. The remaining alternatives at this point are extremely limited (likely Live's audio-block quantization is the floor) and would require resetting expectations.

- [ ] **Step 10: Buffer-change survival**

Change Live buffer 256 → 1024 → 256 with the device active and a note loop running. Device must continue passing tuned MIDI without delete-and-readd.

- [ ] **Step 11: Coexistence with ODDSound MIDI Client M4L**

Add ODDSound's MTS-ESP MIDI Client M4L on track C (different synth) alongside our MPE Receiver on track D. Both must produce the maqam tuning.

- [ ] **Step 12: Idle scheduler load**

With no notes held, observe Max CPU. Should be near zero (metro ticks but iterates empty coll).

- [ ] **Step 13: No commit (verification only)**

If all steps pass, proceed to Task 7. If Step 9 fails, **stop and escalate**.

---

### Task 7: Update CI release workflow

**Files:**
- Modify: `.github/workflows/release.yml`

The workflow already builds VST3/AU/CLAP. We need to bundle the new `.amxd` files plus the ODDSound external for both macOS and Windows.

- [ ] **Step 1: Read the current workflow**

```bash
cat .github/workflows/release.yml | head -120
```
Identify: the M4L bundling step (look for `Tanghim Receiver.amxd` references) and the per-OS package step.

- [ ] **Step 2: Update macOS package step**

Replace the existing M4L bundling block with:

```yaml
      - name: Bundle M4L devices (macOS)
        if: runner.os == 'macOS'
        run: |
          mkdir -p packaging/m4l/Tanghim
          cp "m4l/Tanghim MPE Receiver.amxd" packaging/m4l/Tanghim/
          cp "m4l/Tanghim Mono PB Receiver.amxd" packaging/m4l/Tanghim/
          cp -R "m4l/externals/MTS-ESP.mtof.mxo" packaging/m4l/Tanghim/
          codesign --force --deep --sign - "packaging/m4l/Tanghim/MTS-ESP.mtof.mxo"
```

- [ ] **Step 3: Update Windows package step**

```yaml
      - name: Bundle M4L devices (Windows)
        if: runner.os == 'Windows'
        run: |
          mkdir packaging\m4l\Tanghim
          copy "m4l\Tanghim MPE Receiver.amxd" packaging\m4l\Tanghim\
          copy "m4l\Tanghim Mono PB Receiver.amxd" packaging\m4l\Tanghim\
          copy "m4l\externals\MTS-ESP.mtof.mxe64" packaging\m4l\Tanghim\
        shell: cmd
```

- [ ] **Step 4: Verify (push test tag, optional)**

If `act` is installed: `act -W .github/workflows/release.yml`. Otherwise rely on actual release build.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/release.yml
git commit -m "$(cat <<'EOF'
ci: bundle native M4L Receiver devices + MTS-ESP.mtof external

Replaces single-device bundling with two .amxd files (MPE + Mono PB)
plus the ODDSound MTS-ESP.mtof external (universal .mxo on macOS,
.mxe64 on Windows).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Update macOS installer

**Files:**
- Modify: `scripts/installer/macos/build-pkg.sh`

The installer's M4L drop step currently copies one `.amxd`. Update to copy both new `.amxd` files plus the ODDSound external.

- [ ] **Step 1: Read the current installer**

```bash
cat scripts/installer/macos/build-pkg.sh | grep -A 5 -B 2 -E "amxd|M4L|max for live|Max"
```

- [ ] **Step 2: Replace the M4L block**

Find the section that drops the M4L `.amxd` into the staging directory. Replace it with:

```bash
# M4L Receiver devices + ODDSound external
M4L_DEST="$STAGE/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim"
mkdir -p "$M4L_DEST"
cp "$REPO_ROOT/m4l/Tanghim MPE Receiver.amxd" "$M4L_DEST/"
cp "$REPO_ROOT/m4l/Tanghim Mono PB Receiver.amxd" "$M4L_DEST/"
cp -R "$REPO_ROOT/m4l/externals/MTS-ESP.mtof.mxo" "$M4L_DEST/"
```

(Adjust variable names — `$REPO_ROOT`, `$STAGE` — to match the existing script's conventions.)

- [ ] **Step 3: Test build the .pkg locally**

```bash
bash scripts/installer/macos/build-pkg.sh
```
Expected: a `.pkg` is produced; preview its contents with Suspicious Package or `pkgutil --payload-files` and confirm the new files are present.

- [ ] **Step 4: Commit**

```bash
git add scripts/installer/macos/build-pkg.sh
git commit -m "$(cat <<'EOF'
installer/macos: drop two .amxd files + MTS-ESP.mtof external

Replaces single-device M4L drop with the new MPE + Mono PB devices
and the ODDSound external they depend on.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 9: Update Windows installer

**Files:**
- Modify: `scripts/installer/windows/tanghim.iss`

- [ ] **Step 1: Read the current Inno Setup script**

```bash
cat scripts/installer/windows/tanghim.iss | grep -A 2 -B 2 -E "amxd|m4l"
```

- [ ] **Step 2: Replace the existing single-device M4L file entry**

Remove the old line and add:

```
Source: "..\..\m4l\Tanghim MPE Receiver.amxd"; DestDir: "{userdocs}\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim"; Components: m4l; Flags: ignoreversion
Source: "..\..\m4l\Tanghim Mono PB Receiver.amxd"; DestDir: "{userdocs}\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim"; Components: m4l; Flags: ignoreversion
Source: "..\..\m4l\externals\MTS-ESP.mtof.mxe64"; DestDir: "{userdocs}\Ableton\User Library\Presets\MIDI Effects\Max MIDI Effect\Tanghim"; Components: m4l; Flags: ignoreversion
```

If a `[Components]` block doesn't already have an `m4l` component:

```
Name: "m4l"; Description: "Max for Live Receiver devices (Tanghīm MPE + Mono PB)"; Types: full; Flags: dontinheritcheck
```

- [ ] **Step 3: Test build via CI on a throwaway tag (optional, otherwise rely on release CI)**

- [ ] **Step 4: Commit**

```bash
git add scripts/installer/windows/tanghim.iss
git commit -m "$(cat <<'EOF'
installer/windows: drop two .amxd files + MTS-ESP.mtof.mxe64

Replaces single-device M4L drop with the new MPE + Mono PB devices
and the ODDSound external they depend on. Gated behind opt-in m4l
component.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 10: Delete old single-device M4L files

**Files:**
- Delete: `m4l/generate_patch.py`
- Delete: `m4l/mts_midi_effect.js`
- Delete: `m4l/Tanghim Receiver.amxd`
- Delete: `m4l/Tanghim Receiver.maxpat`

**Only run after Task 6 (Live test) succeeded.** The old files remain in git history at the `last-vst3-bridge-amxd` tag.

- [ ] **Step 1: Verify the new files are committed and working**

```bash
ls -la m4l/Tanghim\ MPE\ Receiver.amxd m4l/Tanghim\ Mono\ PB\ Receiver.amxd
git log --oneline -8
```

- [ ] **Step 2: Delete old files from repo**

```bash
git rm m4l/generate_patch.py m4l/mts_midi_effect.js \
       "m4l/Tanghim Receiver.amxd" "m4l/Tanghim Receiver.maxpat"
```

- [ ] **Step 3: Remove old device from local user library**

```bash
rm -rf "$HOME/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/Tanghim Receiver.amxd"
```

- [ ] **Step 4: Commit**

```bash
git commit -m "$(cat <<'EOF'
m4l: remove single-device vst~+VST3+JS receiver

Replaced by Tanghim MPE Receiver.amxd and Tanghim Mono PB Receiver.amxd
(prior commits). The vst~+JS architecture is preserved at tag
last-vst3-bridge-amxd for rollback.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 11: Rewrite CLAUDE.md "Max for Live Wrapper" section

**Files:**
- Modify: `CLAUDE.md`

The current section describes the `vst~`+JS architecture and lists JS-specific gotchas (midiparse 7-bit conversion, JS default initialization, MPE allocation in JS, parameter-poll rate). All obsolete.

- [ ] **Step 1: Locate the section**

```bash
grep -n "^## Max for Live Wrapper" CLAUDE.md
```

- [ ] **Step 2: Read the section to know what's being replaced**

Use Read tool to view from that line through the next `## ` header.

- [ ] **Step 3: Replace with new content via Edit**

New section content:

```markdown
## Max for Live Receiver

Two pure-native Max patches reading MTS-ESP tuning via ODDSound's
MTS-ESP Max Package. No `vst~`, no JavaScript, no VST2.

- **`Tanghim MPE Receiver.amxd`** — `is_mpe: 1`. MPE channel allocator
  via `[poly @voices 15 @steal 1]` (15 voices = MPE channels 2..16,
  channel 1 reserved for the manager).
- **`Tanghim Mono PB Receiver.amxd`** — single-channel output. Last-note
  priority legato via `[coll noteStack]`. No PB reset on Note Off
  (preserves release tail).

Both share `m4l/_patch_common.py` for the .amxd freeze logic. Per-device
generators: `m4l/generate_mpe_patch.py`, `m4l/generate_monopb_patch.py`.

### Tuning data

`MTS-ESP.mtof` external (ODDSound) reads MTS-ESP shared memory
directly. Outlets per query: `[freq, ratio, semitones, filter]`. We
use `semitones × 100` for cents and `filter` to gate Note On.

`MTS-ESP.mtof` provides no notification on tuning change. We poll held
notes only (entries in `[coll heldNotes]`) at 10 Hz via `[metro 100]`.
Idle = zero scheduler activity.

### Per-Note-On data flow

1. `[live.midiin] → [midiparse]` splits MIDI.
2. Bang `MTS-ESP.mtof` with the note number → receive cents + filter.
3. If filter==0, drop the note.
4. Allocate MPE voice (or push onto Mono note stack).
5. Compute combined PB = clamp(microbendPb + userPbOffset, 0, 16383).
   Microbend formula: `expr int(8192 + ($f1 / ($i2 * 100.)) * 8191)`
   where $f1=cents, $i2=PB range semitones.
6. Emit `[xbendout ch]` PB then `[noteout ch]` Note On.

### ODDSound externals (pinned)

`m4l/externals/MTS-ESP.mtof.mxo` (macOS, universal arm64+x86_64) and
`m4l/externals/MTS-ESP.mtof.mxe64` (Windows). Pinned to a specific
upstream commit — see `m4l/externals/README.md` for the sha and how
to update.

License: 0BSD (upstream). Free to redistribute alongside the .amxd
files.

### Known limitations

**MTS-ESP IPC limitation in M4L:** all M4L devices in a Live session
appear to the MTS-ESP master as one combined client because
`libMTSClient` is loaded once per process. Affects only the
client-count display in the Transmitter, not functionality.

### Why not vst~+VST3 anymore?

The previous architecture (Tanghim Receiver VST3 hosted in `vst~`,
parameter-bridge for tuning, JS for per-Note-On MIDI logic) had
unfixable recording jitter at buffer ≥1024: Cycling '74's docs
confirm `js` always runs in the low-priority thread. Native Max
objects run on the high-priority scheduler thread, eliminating the
jitter source. Bonus: no `vst~` means no buffer-change crash.

→ [diary 2026-05-06](diary/2026-05-06.md), [spec 2026-05-06](docs/superpowers/specs/2026-05-06-native-m4l-receiver-design.md)
```

- [ ] **Step 4: Commit**

```bash
git add CLAUDE.md
git commit -m "$(cat <<'EOF'
docs: rewrite CLAUDE.md "Max for Live" section for native architecture

Replaces vst~+VST3+JS gotchas with the pure-native MTS-ESP Max
Package + stock-Max-objects architecture.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 12: Bump version, push, release

**Files:**
- Modify: `CMakeLists.txt` (project version line)

- [ ] **Step 1: Find and bump version**

```bash
grep -n "^project\|VERSION" CMakeLists.txt | head -3
```
Bump the project version (e.g. `0.9.0` → `0.10.0`). This rebuild is significant — minor bump appropriate.

- [ ] **Step 2: Commit version bump**

```bash
git add CMakeLists.txt
git commit -m "build: bump version to 0.10.0 for native M4L Receiver"
```

- [ ] **Step 3: Push commits**

```bash
git push origin main
```

- [ ] **Step 4: Create and push release tag**

```bash
git tag v0.10.0
git push origin v0.10.0
```
Expected: GitHub Actions release workflow runs and produces a release with VST3, AU, CLAP for both Tanghim plugins, plus both `.amxd` files and the ODDSound external bundled.

- [ ] **Step 5: Verify on GitHub Releases page**

After CI completes:
- Release `v0.10.0` exists
- macOS asset includes both .amxd + MTS-ESP.mtof.mxo
- Windows asset includes both .amxd + MTS-ESP.mtof.mxe64
- Previous release auto-deleted (per project convention)

---

## Self-review

**Spec coverage check:**
- ODDSound externals pinned + committed → Task 1 ✓
- Refactor `generate_patch.py` to share helpers → Task 2 ✓
- MPE per-voice subpatch → Task 3 ✓
- Generate MPE Receiver .amxd → Task 4 ✓
- Generate Mono PB Receiver .amxd → Task 5 ✓
- End-to-end Live test (recording jitter at buffer 1024) → Task 6 ✓
- CI release workflow → Task 7 ✓
- macOS installer → Task 8 ✓
- Windows installer → Task 9 ✓
- Delete old M4L files → Task 10 ✓
- CLAUDE.md → Task 11 ✓
- Push + release tag → Task 12 ✓
- Diary entry → already done before this plan was written; not a task

**Placeholder scan:** Each step contains the actual content needed. Two areas are marked as "may need hand-authoring if py2max struggles" (the held-note retune dispatch wiring in Tasks 4/5, and the MPE per-voice subpatch in Task 3). These are legitimate implementation choice points, not vague placeholders — each says concretely what to do in either case.

**Type consistency:**
- VST2 mentions: zero (correctly — VST2 is not built in this plan).
- `vst~` mentions: only in CLAUDE.md as historical context (Task 11) and in deletion of old files (Task 10).
- ODDSound external file paths: `m4l/externals/MTS-ESP.mtof.mxo` (macOS) and `m4l/externals/MTS-ESP.mtof.mxe64` (Windows) used consistently across Tasks 1, 6, 7, 8, 9.
- `.amxd` device names: `Tanghim MPE Receiver.amxd` and `Tanghim Mono PB Receiver.amxd` used consistently.
- `[poly mpe_voice 15 @steal 1]` allocator referenced consistently across Tasks 3, 4, and CLAUDE.md.
- Default PB ranges: 48 (MPE) and 2 (Mono PB) consistent across Tasks 4, 5, and the spec.
- Tag `last-vst3-bridge-amxd` referenced consistently across Tasks 10, 11, and the rollback story.
