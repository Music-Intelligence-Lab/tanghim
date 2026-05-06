#!/usr/bin/env python3
"""Generate Tanghim MPE Receiver.amxd — Stage 1 (skeleton + [poly] allocator).

Native Max patch with no vst~ and no JS. Reads MTS-ESP tuning via the
MTS-ESP.mtof external (m4l/externals/) and allocates one of 15 MPE voices
via [poly 15 1]. Channel 1 is reserved as the MPE manager; voices 1..15
emit on channels 2..16.

is_mpe: 1 patcher metadata is required, otherwise Live collapses output
to channel 1.

This is stage 1: held-note retune (metro + coll iteration), user PB-wheel
combining, MPE Configuration RPN on load, and a connection-status
indicator are deliberately deferred to subsequent stages.
"""

import json
import os
import sys

import py2max as px
from py2max import Box
from py2max.core.common import Rect

# Allow running as `python3 m4l/generate_mpe_patch.py` from repo root.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _patch_common import freeze_and_save  # noqa: E402

OUTPUT_MAXPAT = "m4l/Tanghim MPE Receiver.maxpat"
OUTPUT_AMXD = "m4l/Tanghim MPE Receiver.amxd"

PB_RANGE_DEFAULT = 48

p = px.Patcher(OUTPUT_MAXPAT)
p.openinpresentation = 1
p.devicewidth = 200.0
p.description = "MTS-ESP MPE Receiver"
p.rect = Rect(100.0, 100.0, 900.0, 600.0)

# ═══════════════════════════════════════════════════════════════════════
# Title comment
# ═══════════════════════════════════════════════════════════════════════

title = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[20, 20, 320, 22],
    presentation=1, presentation_rect=[8.0, 6.0, 184.0, 22.0],
    fontname="Ableton Sans Medium",
    fontsize=14.0,
    text="Tanghim MPE Receiver",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

# ═══════════════════════════════════════════════════════════════════════
# MIDI input + parse
# ═══════════════════════════════════════════════════════════════════════

midiin = p.add("live.midiin",
    numinlets=1, numoutlets=1, outlettype=["int"],
    patching_rect=[20, 60, 100, 22])

# midiparse outlets:
#   0: notes (pitch),        1: notes (velocity)
#   2: poly aftertouch,      3: control change,
#   4: program change,       5: pitch bend (14-bit hires),
#   6: channel aftertouch,   7: channel
midiparse = p.add("midiparse @hires 1",
    numinlets=1, numoutlets=8,
    outlettype=["", "", "", "int", "int", "", "int", ""],
    patching_rect=[20, 95, 300, 22])

p.add_line(midiin, midiparse)

# ═══════════════════════════════════════════════════════════════════════
# MTS-ESP cents query
#   inlet 0: MIDI note number
#   outlet 0: frequency (Hz)
#   outlet 1: ratio
#   outlet 2: semitones (signed float — semitone offset from 12-EDO)
#   outlet 3: filter (0 = filtered/skip, 1 = play)
# ═══════════════════════════════════════════════════════════════════════

mtof = p.add("MTS-ESP.mtof",
    numinlets=1, numoutlets=4,
    outlettype=["float", "float", "float", "int"],
    patching_rect=[20, 135, 140, 22])

p.add_line(midiparse, mtof, outlet=0, inlet=0)  # note → mtof

# cents = semitones * 100
cents_expr = p.add("expr $f1 * 100.",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[170, 135, 110, 22])
p.add_line(mtof, cents_expr, outlet=2, inlet=0)

# Stash latest cents value for downstream PB computation.
cents_send = p.add("send cents",
    numinlets=1, numoutlets=0,
    patching_rect=[290, 135, 80, 22])
p.add_line(cents_expr, cents_send)

# ═══════════════════════════════════════════════════════════════════════
# Filter gate: drop notes where mtof reports filter == 0.
# We gate the velocity stream on the filter flag. A filtered note still
# reaches midiparse, but its velocity is muted to 0 before reaching the
# allocator, so [poly] never allocates a voice for it.
#
# Note Off (vel == 0) must always pass through so [poly] can free the
# voice. We OR the filter flag with (vel == 0) using a [maximum] to keep
# Note Offs flowing even when the filter is set.
# ═══════════════════════════════════════════════════════════════════════

# is_note_off: 1 if velocity == 0
is_note_off = p.add("== 0",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[150, 95, 60, 22])
p.add_line(midiparse, is_note_off, outlet=1, inlet=0)

# Latest filter value from mtof outlet 3
filter_recv = p.add("send filter",
    numinlets=1, numoutlets=0,
    patching_rect=[400, 135, 80, 22])
p.add_line(mtof, filter_recv, outlet=3)

filter_recv_in = p.add("receive filter",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[230, 95, 80, 22])

# pass = filter || is_note_off  (max of 0/1 ints)
pass_or = p.add("maximum 0",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[230, 130, 80, 22])
p.add_line(filter_recv_in, pass_or, outlet=0, inlet=0)
p.add_line(is_note_off, pass_or, outlet=0, inlet=1)

# velgate: control = pass flag, value = velocity
velgate = p.add("gate 1 0",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[230, 165, 80, 22])
p.add_line(pass_or, velgate, outlet=0, inlet=0)         # control
p.add_line(midiparse, velgate, outlet=1, inlet=1)       # value (velocity)

# ═══════════════════════════════════════════════════════════════════════
# Pack (note, gated_velocity) → [poly 15 1]
#
# [poly] outlets (left to right, per Cycling '74 docs):
#   0: voice number (1..15)
#   1: pitch
#   2: velocity
# Steal mode 1 = steal oldest held voice when capacity exceeded.
# ═══════════════════════════════════════════════════════════════════════

notepack = p.add("pack 0 0",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[20, 200, 100, 22])
p.add_line(midiparse, notepack, outlet=0, inlet=0)  # pitch
p.add_line(velgate, notepack, outlet=0, inlet=1)    # gated velocity

poly = p.add("poly 15 1",
    numinlets=2, numoutlets=3,
    outlettype=["int", "int", "int"],
    patching_rect=[20, 235, 100, 22])
p.add_line(notepack, poly)

# voice# (poly outlet 0) + 1 = MPE channel (range 2..16; ch 1 = MPE manager)
plus1 = p.add("+ 1",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[20, 270, 60, 22])
p.add_line(poly, plus1, outlet=0, inlet=0)

# Distribute the channel number to xbendout and noteout.
ch_send = p.add("send mpechan",
    numinlets=1, numoutlets=0,
    patching_rect=[90, 270, 90, 22])
p.add_line(plus1, ch_send)

ch_recv_pb = p.add("receive mpechan",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[400, 305, 90, 22])
ch_recv_note = p.add("receive mpechan",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[200, 270, 90, 22])

# ═══════════════════════════════════════════════════════════════════════
# Compute 14-bit pitch bend from cents and pbRange.
#   pb14 = clamp(int(8192 + (cents / (pbRange * 100.0)) * 8191), 0, 16383)
#
# We need a fresh PB value every Note On. Trigger by piping the voice#
# (which only emits on Note On / Off) into a [t b] that bangs the
# combine chain via shared variables.
# ═══════════════════════════════════════════════════════════════════════

cents_recv = p.add("receive cents",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[290, 305, 80, 22])

pbrange_recv = p.add("receive pbRange",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[380, 305, 90, 22])

# expr int(8192 + ($f1 / ($f2 * 100.)) * 8191)
pb_expr = p.add("expr int(8192 + ($f1 / ($f2 * 100.)) * 8191)",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[290, 340, 280, 22])
p.add_line(cents_recv, pb_expr, outlet=0, inlet=0)
p.add_line(pbrange_recv, pb_expr, outlet=0, inlet=1)

pb_clip = p.add("clip 0 16383",
    numinlets=3, numoutlets=1, outlettype=[""],
    patching_rect=[290, 375, 120, 22])
p.add_line(pb_expr, pb_clip)

# ═══════════════════════════════════════════════════════════════════════
# xbendout: 14-bit pitch bend on the allocated MPE channel
#   inlet 0: PB value (0..16383)
#   inlet 1: channel (1..16)
# Emit PB *before* Note On.
# ═══════════════════════════════════════════════════════════════════════

xbendout = p.add("xbendout",
    numinlets=2, numoutlets=0,
    patching_rect=[290, 415, 80, 22])
p.add_line(pb_clip, xbendout, outlet=0, inlet=0)
p.add_line(ch_recv_pb, xbendout, outlet=0, inlet=1)

# ═══════════════════════════════════════════════════════════════════════
# noteout: pitch + velocity + channel
#   inlet 0: pitch
#   inlet 1: velocity
#   inlet 2: channel
# ═══════════════════════════════════════════════════════════════════════

noteout = p.add("noteout",
    numinlets=3, numoutlets=0,
    patching_rect=[20, 305, 90, 22])
p.add_line(poly, noteout, outlet=1, inlet=0)         # pitch
p.add_line(poly, noteout, outlet=2, inlet=1)         # velocity
p.add_line(ch_recv_note, noteout, outlet=0, inlet=2) # channel

# ═══════════════════════════════════════════════════════════════════════
# PB Range live.numbox parameter
# ═══════════════════════════════════════════════════════════════════════

pbnum = p.add_box(Box(
    id=p.get_id(), maxclass="live.numbox",
    numinlets=1, numoutlets=2,
    outlettype=["", "float"],
    parameter_enable=1,
    patching_rect=[20, 360, 60, 22],
    presentation=1, presentation_rect=[8.0, 36.0, 80.0, 22.0],
    saved_attribute_attributes={
        "valueof": {
            "parameter_initial": [PB_RANGE_DEFAULT],
            "parameter_initial_enable": 1,
            "parameter_linknames": 1,
            "parameter_longname": "PB Range",
            "parameter_mmax": 96.0,
            "parameter_mmin": 1.0,
            "parameter_shortname": "PB Range",
            "parameter_type": 1,
            "parameter_unitstyle": 9,
        }
    },
    varname="PB Range",
))

pbrange_send = p.add("send pbRange",
    numinlets=1, numoutlets=0,
    patching_rect=[20, 395, 90, 22])
p.add_line(pbnum, pbrange_send)

# ═══════════════════════════════════════════════════════════════════════
# Save and post-process for M4L-specific patcher properties
# ═══════════════════════════════════════════════════════════════════════

p.save()

with open(OUTPUT_MAXPAT) as f:
    data = json.load(f)

patcher = data["patcher"]
patcher["openrect"] = [0.0, 0.0, 200.0, 80.0]
# is_mpe: 1 — without it Live collapses MPE output to channel 1.
patcher["is_mpe"] = 1

# M4L device metadata (required for Ableton to load the .amxd)
patcher["title"] = "Tanghim MPE Receiver"
patcher["latency"] = 0
patcher["project"] = {
    "version": 1,
    "creationdate": 3590052786,
    "modificationdate": 3590052786,
    "viewrect": [0.0, 0.0, 300.0, 500.0],
    "autoorganize": 1,
    "hideprojectwindow": 1,
    "showdependencies": 1,
    "autolocalize": 0,
    "contents": {"patchers": {}, "code": {}},
    "layout": {},
    "searchpath": {},
    "detailsvisible": 0,
    "amxdtype": 1835887981,  # 0x6D696469 = "midi" — M4L MIDI effect
    "readonly": 0,
    "devpathtype": 0,
    "devpath": ".",
    "sortmode": 0,
    "viewmode": 0,
}

# Fix object metadata that py2max defaults incorrectly without MaxRef.
for box_wrapper in patcher.get("boxes", []):
    box = box_wrapper.get("box", {})
    mc = box.get("maxclass", "")
    text = box.get("text", "")
    # Comments have 0 outlets
    if mc == "comment":
        box["numoutlets"] = 0
        box.pop("outlettype", None)
    # Sink objects (no outlets)
    if text in ("noteout", "xbendout") or text.startswith("send "):
        box["numoutlets"] = 0
        box.pop("outlettype", None)

# Remove the "order" field from patchlines (not standard in Max patches).
for line in patcher.get("lines", []):
    pl = line.get("patchline", {})
    pl.pop("order", None)

with open(OUTPUT_MAXPAT, "w") as f:
    json.dump(data, f, indent="\t")
print(f"Generated: {OUTPUT_MAXPAT}")

# Freeze .amxd. No embedded JS dependencies — everything is native Max.
freeze_and_save(p, OUTPUT_MAXPAT, OUTPUT_AMXD)
print(f"Generated: {OUTPUT_AMXD} (frozen, no JS deps)")
