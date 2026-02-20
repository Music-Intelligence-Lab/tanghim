#!/usr/bin/env python3
"""Generate the Tanghīm – receiver M4L patch using py2max.

This produces a valid .maxpat JSON with correct object metadata,
avoiding hand-written JSON errors that cause Max to crash.

Usage: python3 m4l/generate_patch.py
"""

import json
import struct
import py2max as px
from py2max import Box
from py2max.core.common import Rect

OUTPUT_MAXPAT = "m4l/Tanghim Receiver.maxpat"
OUTPUT_AMXD = "m4l/Tanghim Receiver.amxd"

p = px.Patcher(OUTPUT_MAXPAT)
p.openinpresentation = 1
p.devicewidth = 400.0
p.description = "MTS-ESP microtuning via MPE or Pitch Bend"
p.rect = Rect(100.0, 100.0, 900.0, 600.0)

# ═══════════════════════════════════════════════════════════════════════
# Presentation labels
# ═══════════════════════════════════════════════════════════════════════

title = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[30, 460, 200, 22],
    presentation=1, presentation_rect=[10, 8, 200, 22],
    fontsize=14.0,
    text="Tanghim",
    textcolor=[1, 1, 1, 1],
))

mode_label = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[30, 490, 60, 18],
    presentation=1, presentation_rect=[10, 80, 60, 18],
    fontsize=10.0,
    text="Mode:",
    textcolor=[0.7, 0.7, 0.7, 1],
))

status_label = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[200, 460, 200, 18],
    presentation=1, presentation_rect=[10, 100, 200, 18],
    fontsize=10.0,
    text="",
    textcolor=[0.5, 0.5, 0.5, 1],
))

# ═══════════════════════════════════════════════════════════════════════
# MIDI chain: midiin → midiparse → js → midiout
#             midiparse outlets 1-6 → midiformat → midiout (passthrough)
# ═══════════════════════════════════════════════════════════════════════

midiin = p.add("midiin",
    numinlets=1, numoutlets=1, outlettype=["int"],
    patching_rect=[30, 45, 48, 22])

midiparse = p.add("midiparse",
    numinlets=1, numoutlets=8,
    outlettype=["", "", "", "int", "int", "", "int", ""],
    patching_rect=[30, 80, 300, 22])

js = p.add("js mts_midi_effect.js",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[30, 150, 150, 22])

midiformat = p.add("midiformat",
    numinlets=7, numoutlets=1, outlettype=["int"],
    patching_rect=[200, 185, 200, 22])

midiout = p.add("midiout",
    numinlets=1, numoutlets=0,
    patching_rect=[30, 220, 52, 22])

# midiin → midiparse
p.add_line(midiin, midiparse)

# midiparse outlet 0 (notes) → js inlet 0
p.add_line(midiparse, js, outlet=0, inlet=0)

# midiparse passthrough: outlets 1-6 → midiformat inlets 1-6
for i in range(1, 7):
    p.add_line(midiparse, midiformat, outlet=i, inlet=i)

# js → midiout
p.add_line(js, midiout)

# midiformat → midiout
p.add_line(midiformat, midiout)

# ═══════════════════════════════════════════════════════════════════════
# MTS-ESP Data Bridge: vst~ hosts Receiver for parameter polling
# ═══════════════════════════════════════════════════════════════════════

loadbang = p.add("loadbang",
    numinlets=1, numoutlets=1, outlettype=["bang"],
    patching_rect=[450, 45, 58, 22])

trig = p.add("t b b",
    numinlets=1, numoutlets=2, outlettype=["bang", "bang"],
    patching_rect=[450, 80, 40, 22])

delay_plug = p.add("delay 2000",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[450, 115, 72, 22])

plug_msg = p.add_box(Box(
    id=p.get_id(), maxclass="message",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[450, 150, 240, 22],
    text='plug_vst3 "Tanghim Receiver"',
))

delay_metro = p.add("delay 3000",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[560, 115, 72, 22])

metro = p.add("metro 1000",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[560, 150, 72, 22])

uzi = p.add("uzi 128 0",
    numinlets=1, numoutlets=3, outlettype=["bang", "int", "int"],
    patching_rect=[560, 185, 72, 22])

plus4 = p.add("+ 4",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[610, 220, 36, 22])

prepend_get = p.add("prepend get",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[610, 255, 78, 22])

sig0 = p.add("sig~ 0.",
    numinlets=1, numoutlets=1, outlettype=["signal"],
    patching_rect=[450, 255, 50, 22])

vst = p.add("vst~",
    numinlets=2, numoutlets=8,
    outlettype=["signal", "signal", "", "list", "int", "", "", ""],
    patching_rect=[450, 295, 200, 22])

open_msg = p.add_box(Box(
    id=p.get_id(), maxclass="message",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[700, 255, 38, 22],
    text="open",
))

# loadbang → trigger
p.add_line(loadbang, trig)

# trigger left (b) → delay 2000 → plug message → vst~
p.add_line(trig, delay_plug, outlet=0, inlet=0)
p.add_line(delay_plug, plug_msg)
p.add_line(plug_msg, vst)

# trigger right (b) → delay 3000 → metro → uzi
p.add_line(trig, delay_metro, outlet=1, inlet=0)
p.add_line(delay_metro, metro)
p.add_line(metro, uzi)

# uzi outlet 2 (count) → +4 → prepend get → vst~
# Offset 4 = bypass(0) + mode(1) + mpePbRange(2) + monoPbRange(3), so cents_0 = param 4
p.add_line(uzi, plus4, outlet=2, inlet=0)
p.add_line(plus4, prepend_get)
p.add_line(prepend_get, vst)

# sig~ → vst~ inlet 0 (audio)
p.add_line(sig0, vst)

# open button → vst~
p.add_line(open_msg, vst)

# ═══════════════════════════════════════════════════════════════════════
# Parameter denormalization: vst~ outlet 3 → unpack → denorm → js inlet 1
# vst~ get response: "index normalized_value" (list)
# Convert: index-4 = MIDI note, value * 9600 - 4800 = cents
# ═══════════════════════════════════════════════════════════════════════

unpack = p.add("unpack 0 0.",
    numinlets=2, numoutlets=2, outlettype=["int", "float"],
    patching_rect=[530, 340, 72, 22])

minus4 = p.add("- 4",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[530, 375, 36, 22])

denorm = p.add("expr $f1 * 9600. - 4800.",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[580, 375, 152, 22])

pack_tuning = p.add("pack 0 0.",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[530, 410, 72, 22])

trig_list = p.add("t l",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[530, 445, 28, 22])

# vst~ outlet 3 (param dump) → unpack
p.add_line(vst, unpack, outlet=3, inlet=0)

# unpack left (index int) → -4
p.add_line(unpack, minus4, outlet=0, inlet=0)

# unpack right (value float) → expr denormalize
p.add_line(unpack, denorm, outlet=1, inlet=0)

# -4 → pack left (note index)
p.add_line(minus4, pack_tuning, outlet=0, inlet=0)

# expr → pack right (cents value)
p.add_line(denorm, pack_tuning, outlet=0, inlet=1)

# pack → t l → js inlet 1
p.add_line(pack_tuning, trig_list)
p.add_line(trig_list, js, outlet=0, inlet=1)

# ═══════════════════════════════════════════════════════════════════════
# Controls: live.menu (Mode) + live.dial (Bend Range)
# ═══════════════════════════════════════════════════════════════════════

mode_menu = p.add_box(Box(
    id=p.get_id(), maxclass="live.menu",
    numinlets=1, numoutlets=3,
    outlettype=["", "", "float"],
    parameter_enable=1,
    patching_rect=[30, 290, 100, 15],
    presentation=1, presentation_rect=[10, 38, 100, 15],
    saved_attribute_attributes={
        "valueof": {
            "parameter_enum": ["MPE", "Mono PB"],
            "parameter_initial": [0],
            "parameter_initial_enable": 1,
            "parameter_linknames": 1,
            "parameter_longname": "Mode",
            "parameter_mmax": 1,
            "parameter_shortname": "Mode",
            "parameter_type": 2,
        }
    },
    varname="Mode",
))

prepend_mode = p.add("prepend set_mode",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[30, 325, 105, 22])

bend_dial = p.add_box(Box(
    id=p.get_id(), maxclass="live.dial",
    numinlets=1, numoutlets=2,
    outlettype=["", "float"],
    parameter_enable=1,
    patching_rect=[180, 280, 44, 48],
    presentation=1, presentation_rect=[130, 23, 44, 48],
    saved_attribute_attributes={
        "valueof": {
            "parameter_initial": [48],
            "parameter_initial_enable": 1,
            "parameter_linknames": 1,
            "parameter_longname": "Bend Range",
            "parameter_mmax": 96.0,
            "parameter_mmin": 1.0,
            "parameter_shortname": "PB Range",
            "parameter_type": 1,
            "parameter_unitstyle": 9,
        }
    },
    varname="Bend Range",
))

prepend_bend = p.add("prepend set_bend_range",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[180, 340, 130, 22])

# Mode menu → prepend → js inlet 0
p.add_line(mode_menu, prepend_mode, outlet=0, inlet=0)
p.add_line(prepend_mode, js)

# Bend dial → prepend → js inlet 0
p.add_line(bend_dial, prepend_bend, outlet=0, inlet=0)
p.add_line(prepend_bend, js)

# ═══════════════════════════════════════════════════════════════════════
# Save and post-process for M4L-specific patcher properties
# ═══════════════════════════════════════════════════════════════════════

p.save()

# Post-process: add M4L properties that py2max doesn't support natively
with open(OUTPUT_MAXPAT) as f:
    data = json.load(f)

patcher = data["patcher"]
patcher["openrect"] = [0.0, 0.0, 400.0, 170.0]
# Tell Ableton this device outputs MPE (multi-channel MIDI on ch 2-16).
# Without this flag, Ableton normalizes all MIDI output to channel 1.
# ODDSound's MPE M4L device sets this to 1; their non-MPE version sets it to 0.
patcher["is_mpe"] = 1

# M4L device metadata (required for Ableton to load the .amxd)
patcher["title"] = "Tanghim Receiver"
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

# Fix object metadata that py2max defaults incorrectly without MaxRef
for box_wrapper in patcher.get("boxes", []):
    box = box_wrapper.get("box", {})
    mc = box.get("maxclass", "")
    # Comments have 0 outlets
    if mc == "comment":
        box["numoutlets"] = 0
        box.pop("outlettype", None)
    # midiout has 0 outlets
    if box.get("text") == "midiout":
        box["numoutlets"] = 0
        box.pop("outlettype", None)

# Remove the "order" field from patchlines (not standard in Max patches)
for line in patcher.get("lines", []):
    pl = line.get("patchline", {})
    pl.pop("order", None)

# Write .maxpat (for editing in standalone Max)
with open(OUTPUT_MAXPAT, "w") as f:
    json.dump(data, f, indent="\t")
print(f"Generated: {OUTPUT_MAXPAT}")

# Write .amxd (binary container for Ableton Live)
# Format: ampf header + mmmmmeta section + ptch section with JSON + null terminator
json_bytes = json.dumps(data, indent="\t").encode("utf-8") + b"\x00"
with open(OUTPUT_AMXD, "wb") as f:
    f.write(b"ampf")                              # magic
    f.write(struct.pack("<I", 4))                  # version
    f.write(b"mmmmmeta")                           # meta section tag
    f.write(struct.pack("<I", 4))                  # meta data length
    f.write(b"\x00\x00\x00\x00")                  # meta data (empty)
    f.write(b"ptch")                               # patch section tag
    f.write(struct.pack("<I", len(json_bytes)))    # patch data length
    f.write(json_bytes)                            # JSON + null terminator
print(f"Generated: {OUTPUT_AMXD}")
