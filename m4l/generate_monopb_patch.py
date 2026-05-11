#!/usr/bin/env python3
"""Generate Tanghim Mono PB Receiver.amxd.

Derived from generate_mpe_patch.py with MPE stripped out:
  - [poly 15 1] replaced by [ddg.mono 0] (last-note priority, retrigger)
  - channel hardwired to 1 (xbendout 1, noteout ch=1 via loadbang)
  - emit_trig simplified: cents_store bang (PB) then pitch_store bang (Note)
  - held-note retune metro/coll removed (not needed for mono)
  - is_mpe flag removed

Everything else (mtof, cents_store, vel_gate, pb_expr, user PB wheel,
live.dial params, send/receive, post-processing) is unchanged from MPE patch.
"""

import json
import os
import sys

import py2max as px
from py2max import Box
from py2max.core.common import Rect

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _patch_common import freeze_and_save  # noqa: E402

OUTPUT_MAXPAT = "m4l/Tanghim Mono PB Receiver.maxpat"
OUTPUT_AMXD = "m4l/Tanghim Mono PB Receiver.amxd"

PB_RANGE_DEFAULT = 2

p = px.Patcher(OUTPUT_MAXPAT)
p.openinpresentation = 1
p.devicewidth = 99.0
p.description = "MTS-ESP Mono PB Receiver"
p.rect = Rect(100.0, 100.0, 900.0, 600.0)

# ═══════════════════════════════════════════════════════════════════════
# Title block
# ═══════════════════════════════════════════════════════════════════════

title_arabic = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[45, 475, 292, 29],
    presentation=1, presentation_rect=[0.0, 0.0, 100.0, 29.0],
    fontname="Cairo Black",
    text="تنغيم",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

title_latin = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[30, 460, 178, 21],
    presentation=1, presentation_rect=[0.0, 30.0, 100.0, 35.0],
    presentation_linecount=2,
    fontname="Ableton Sans Medium",
    fontsize=12.0,
    text="Tanghim Mono PB Receiver",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

# ═══════════════════════════════════════════════════════════════════════
# MIDI input + parse  (identical to MPE patch)
# ═══════════════════════════════════════════════════════════════════════

midiin = p.add("midiin",
    numinlets=1, numoutlets=1, outlettype=["int"],
    patching_rect=[20, 60, 100, 22])

midiparse = p.add("midiparse @hires 2",
    numinlets=1, numoutlets=8,
    outlettype=["", "", "", "int", "int", "int", "int", ""],
    patching_rect=[20, 95, 300, 22])
p.add_line(midiin, midiparse)

note_unpack = p.add("unpack 0 0",
    numinlets=1, numoutlets=2, outlettype=["int", "int"],
    patching_rect=[20, 130, 100, 22])
p.add_line(midiparse, note_unpack, outlet=0, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# [t i i]: right outlet fires first (→ mtof), left outlet fires last
# (→ ddg.mono). Identical ordering to MPE patch's note_trig → poly.
# ═══════════════════════════════════════════════════════════════════════

note_trig = p.add("t i i",
    numinlets=1, numoutlets=2, outlettype=["int", "int"],
    patching_rect=[20, 165, 60, 22])
p.add_line(note_unpack, note_trig, outlet=0, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# MTS-ESP cents query  (identical to MPE patch)
# ═══════════════════════════════════════════════════════════════════════

mtof = p.add("MTS-ESP.mtof",
    numinlets=1, numoutlets=4,
    outlettype=["float", "float", "float", "int"],
    patching_rect=[100, 165, 140, 22])
p.add_line(note_trig, mtof, outlet=1, inlet=0)  # right fires first

cents_expr = p.add("expr $f1 * 100.",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[260, 165, 110, 22])
p.add_line(mtof, cents_expr, outlet=2, inlet=0)

cents_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[380, 165, 60, 22])
p.add_line(cents_expr, cents_store, outlet=0, inlet=1)  # silent store

# ═══════════════════════════════════════════════════════════════════════
# User PB wheel  (identical to MPE patch)
# ═══════════════════════════════════════════════════════════════════════

user_pb_offset = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[440, 100, 60, 22])

user_pb_trig = p.add("t b i",
    numinlets=1, numoutlets=2, outlettype=["bang", "int"],
    patching_rect=[440, 60, 60, 22])
p.add_line(midiparse, user_pb_trig, outlet=5, inlet=0)
p.add_line(user_pb_trig, user_pb_offset, outlet=1, inlet=0)  # silent store
p.add_line(user_pb_trig, cents_store, outlet=0, inlet=0)     # bang → re-emit PB

# ═══════════════════════════════════════════════════════════════════════
# Filter-note routing  (identical to MPE patch)
# ═══════════════════════════════════════════════════════════════════════

vel_gate = p.add("* 1",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[150, 200, 60, 22])
p.add_line(note_unpack, vel_gate, outlet=1, inlet=1)  # vel → right (silent)
p.add_line(mtof, vel_gate, outlet=3, inlet=0)         # filter → left (triggers)

# ═══════════════════════════════════════════════════════════════════════
# Voice management: [ddg.mono 0] replaces [poly 15 1].
#
# ddg.mono mode 0 = retrigger, last-note priority.
# Inlets:  0 = pitch (triggers), 1 = velocity (silent store).
# Outlets: 0 = pitch, 1 = velocity.
# Right-to-left: vel_gate fires before pitch, so vel is stored silently
# before pitch triggers ddg.mono's allocation logic.
#
# ddg.mono outlet 0 (pitch) fires last — same role as poly outlet 0
# (voice#) in the MPE patch: it's the trigger that drives emit_trig.
# ═══════════════════════════════════════════════════════════════════════

# ═══════════════════════════════════════════════════════════════════════
# Voice management: [poly 1 1] — 1 voice, steal mode 1.
# Steal mode 1 = steal oldest held note. With 1 voice this means any new
# Note On automatically emits Note Off for the held note first, then
# Note On for the new one — forced monophony, no extra logic needed.
#
# Outlets (right-to-left, same as MPE patch's [poly 15 1]):
#   0: voice# (always 1)
#   1: pitch
#   2: velocity
# ═══════════════════════════════════════════════════════════════════════

poly = p.add("poly 1 1",
    numinlets=2, numoutlets=4,
    outlettype=["int", "int", "int", ""],
    patching_rect=[20, 235, 80, 22])
p.add_line(vel_gate, poly, outlet=0, inlet=1)   # filtered vel → right (silent)
p.add_line(note_trig, poly, outlet=0, inlet=0)  # pitch → left (triggers)

# Pitch and velocity stored silently; banged by emit_trig.
pitch_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[20, 270, 60, 22])
p.add_line(poly, pitch_store, outlet=1, inlet=1)  # pitch → right (silent)

# ═══════════════════════════════════════════════════════════════════════
# emit_trig driven by poly outlet 0 (voice#, fires last — same as MPE patch).
# [t b b]: outlet 1 (first) → PB, outlet 0 (last) → Note On/Off.
# ═══════════════════════════════════════════════════════════════════════

emit_trig = p.add("t b b",
    numinlets=1, numoutlets=2, outlettype=["bang", "bang"],
    patching_rect=[20, 305, 70, 22])
p.add_line(poly, emit_trig, outlet=0, inlet=0)         # voice# triggers emit
p.add_line(emit_trig, cents_store, outlet=1, inlet=0)  # bang → PB (fires first)
p.add_line(emit_trig, pitch_store, outlet=0, inlet=0)  # bang → Note (fires second)

noteout = p.add("noteout",
    numinlets=3, numoutlets=0,
    patching_rect=[20, 345, 90, 22])
p.add_line(poly, noteout, outlet=2, inlet=1)           # vel → right (silent, fires first)
p.add_line(pitch_store, noteout, outlet=0, inlet=0)    # pitch → left (triggers send)

# Hardwire channel 1 via loadbang — identical to MPE patch pattern.
chan_loadbang = p.add("loadbang",
    numinlets=1, numoutlets=1, outlettype=["bang"],
    patching_rect=[130, 310, 70, 22])
chan_msg = p.add_box(Box(
    id=p.get_id(), maxclass="message",
    patching_rect=[130, 335, 25, 22],
    text="1",
))
p.add_line(chan_loadbang, chan_msg, outlet=0, inlet=0)
p.add_line(chan_msg, noteout, outlet=0, inlet=2)       # ch 1 → noteout (silent)

# ═══════════════════════════════════════════════════════════════════════
# PB chain  (identical to MPE patch except xbendout hardwired to ch 1)
# ═══════════════════════════════════════════════════════════════════════

pbrange_recv = p.add("receive pbRange",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[380, 305, 90, 22])

user_pbrange_recv = p.add("receive userPbRange",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[480, 305, 110, 22])

pb_expr = p.add(
    "expr int(8192 + ($f1 / ($f2 * 100.)) * 8191 + ($i3 * $f4 / $f2))",
    numinlets=4, numoutlets=1, outlettype=[""],
    patching_rect=[290, 340, 360, 22])
p.add_line(cents_store, pb_expr, outlet=0, inlet=0)
p.add_line(pbrange_recv, pb_expr, outlet=0, inlet=1)
p.add_line(user_pb_offset, pb_expr, outlet=0, inlet=2)
p.add_line(user_pbrange_recv, pb_expr, outlet=0, inlet=3)

pb_clip = p.add("clip 0 16383",
    numinlets=3, numoutlets=1, outlettype=[""],
    patching_rect=[290, 375, 120, 22])
p.add_line(pb_expr, pb_clip)

# xbendout 1: hardwired channel 1 (no variable channel inlet needed)
xbendout = p.add("xbendout 1",
    numinlets=1, numoutlets=1, outlettype=["int"],
    patching_rect=[290, 415, 80, 22])
p.add_line(pb_clip, xbendout, outlet=0, inlet=0)

midiout_pb = p.add("midiout",
    numinlets=1, numoutlets=0,
    patching_rect=[290, 450, 80, 22])
p.add_line(xbendout, midiout_pb, outlet=0, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# UI: single PB Range dial (synth + wheel share the same range on mono)
# ═══════════════════════════════════════════════════════════════════════

pb_dial_label = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[180, 250, 80, 18],
    presentation=1, presentation_rect=[10.0, 80.0, 80.0, 18.0],
    fontname="Ableton Sans Medium",
    fontsize=10.0,
    text="PB Range",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

pbnum = p.add_box(Box(
    id=p.get_id(), maxclass="live.dial",
    numinlets=1, numoutlets=2,
    outlettype=["", "float"],
    parameter_enable=1,
    patching_rect=[180, 280, 27, 48],
    presentation=1, presentation_rect=[20.0, 90.0, 60.0, 48.0],
    saved_attribute_attributes={
        "valueof": {
            "parameter_initial": [PB_RANGE_DEFAULT],
            "parameter_initial_enable": 1,
            "parameter_linknames": 1,
            "parameter_longname": "PB Range",
            "parameter_mmax": 96.0,
            "parameter_mmin": 1.0,
            "parameter_shortname": " ",
            "parameter_type": 1,
            "parameter_unitstyle": 9,
        }
    },
    varname="PB Range",
))

pbrange_send = p.add("send pbRange",
    numinlets=1, numoutlets=0,
    patching_rect=[20, 435, 90, 22])
p.add_line(pbnum, pbrange_send)

user_pbrange_send = p.add("send userPbRange",
    numinlets=1, numoutlets=0,
    patching_rect=[20, 460, 110, 22])
p.add_line(pbnum, user_pbrange_send)

# ═══════════════════════════════════════════════════════════════════════
# Receiver registry — writes uuid.monopb on load so the Tanghim
# Transmitter badge can count this device.
# ═══════════════════════════════════════════════════════════════════════

registry_js = p.add("js registry_monopb.js",
    numinlets=0, numoutlets=0,
    patching_rect=[600, 60, 160, 22])

# ═══════════════════════════════════════════════════════════════════════
# Save + post-process  (identical to MPE patch minus is_mpe flag)
# ═══════════════════════════════════════════════════════════════════════

p.save()

with open(OUTPUT_MAXPAT) as f:
    data = json.load(f)

patcher = data["patcher"]
patcher["openrect"] = [0.0, 0.0, 99.0, 169.0]
# NO is_mpe flag — mono PB emits on channel 1 only

patcher["title"] = "Tanghim Mono PB Receiver"
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
    "amxdtype": 1835887981,
    "readonly": 0,
    "devpathtype": 0,
    "devpath": ".",
    "sortmode": 0,
    "viewmode": 0,
}

for box_wrapper in patcher.get("boxes", []):
    box = box_wrapper.get("box", {})
    mc = box.get("maxclass", "")
    text = box.get("text", "")
    if mc == "comment":
        box["numoutlets"] = 0
        box.pop("outlettype", None)
    if text in ("noteout", "xbendout", "xbendout 1", "midiout") or text.startswith("send "):
        box["numoutlets"] = 0
        box.pop("outlettype", None)

for line in patcher.get("lines", []):
    pl = line.get("patchline", {})
    pl.pop("order", None)

with open(OUTPUT_MAXPAT, "w") as f:
    json.dump(data, f, indent="\t")
print(f"Generated: {OUTPUT_MAXPAT}")

with open("m4l/registry_monopb.js", "rb") as f:
    registry_js_bytes = f.read()
freeze_and_save(p, OUTPUT_MAXPAT, OUTPUT_AMXD,
                embedded_files=[("registry_monopb.js", registry_js_bytes)])
print(f"Generated: {OUTPUT_AMXD} (frozen)")
