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
p.devicewidth = 135.4765625
p.description = "MTS-ESP microtuning via MPE or Pitch Bend"
p.rect = Rect(100.0, 100.0, 900.0, 600.0)

# ═══════════════════════════════════════════════════════════════════════
# Presentation labels
# ═══════════════════════════════════════════════════════════════════════

title = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[45, 475, 292, 29],
    presentation=1, presentation_rect=[-1.0, 2.0, 136.0, 29.0],
    fontname="Cairo Black",
    text="\u062a\u0646\u063a\u064a\u0645",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

subtitle = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[30, 460, 178, 21],
    presentation=1, presentation_rect=[-0.5, 33.0, 133.0, 21.0],
    fontname="Ableton Sans Medium",
    text="Tanghim Receiver",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
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

# midiparse passthrough: outlets 1-4, 6 → midiformat (skip 5 = pitch bend)
for i in [1, 2, 3, 4, 6]:
    p.add_line(midiparse, midiformat, outlet=i, inlet=i)

# midiparse outlet 5 (pitch bend) → js for combining with microtuning PB
prepend_pb = p.add("prepend pitchbend",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[200, 115, 112, 22])
p.add_line(midiparse, prepend_pb, outlet=5, inlet=0)
p.add_line(prepend_pb, js)

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

delay_plug = p.add("delay 300",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[450, 115, 72, 22])

plug_msg = p.add_box(Box(
    id=p.get_id(), maxclass="message",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[450, 150, 240, 22],
    text='plug_vst3 "Tanghim Receiver"',
))

delay_metro = p.add("delay 750",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[560, 115, 72, 22])

metro = p.add("metro 100",
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
# Controls: live.tab (Mode) + live.dial (MPE PB) + live.dial (Mono PB)
# ═══════════════════════════════════════════════════════════════════════

mode_tab = p.add_box(Box(
    id=p.get_id(), maxclass="live.tab",
    numinlets=1, numoutlets=3,
    outlettype=["", "", "float"],
    parameter_enable=1,
    num_lines_patching=1,
    num_lines_presentation=1,
    patching_rect=[30, 290, 200, 20],
    presentation=1, presentation_rect=[6.75, 62.0, 121.0, 20.0],
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

mpe_bend_dial = p.add_box(Box(
    id=p.get_id(), maxclass="live.dial",
    numinlets=1, numoutlets=2,
    outlettype=["", "float"],
    parameter_enable=1,
    patching_rect=[180, 280, 27, 48],
    presentation=1, presentation_rect=[8.0, 92.0, 60.0, 48.0],
    saved_attribute_attributes={
        "valueof": {
            "parameter_initial": [48],
            "parameter_initial_enable": 1,
            "parameter_linknames": 1,
            "parameter_longname": "MPE PB Range",
            "parameter_mmax": 96.0,
            "parameter_mmin": 1.0,
            "parameter_shortname": "PB Range",
            "parameter_type": 1,
            "parameter_unitstyle": 9,
        }
    },
    varname="MPE PB Range",
))

mono_bend_dial = p.add_box(Box(
    id=p.get_id(), maxclass="live.dial",
    numinlets=1, numoutlets=2,
    outlettype=["", "float"],
    parameter_enable=1,
    patching_rect=[260, 280, 27, 48],
    presentation=1, presentation_rect=[68.0, 92.0, 58.75, 48.0],
    saved_attribute_attributes={
        "valueof": {
            "parameter_initial": [2],
            "parameter_initial_enable": 1,
            "parameter_linknames": 1,
            "parameter_longname": "Mono PB Range",
            "parameter_mmax": 96.0,
            "parameter_mmin": 2.0,
            "parameter_shortname": "PB Range",
            "parameter_type": 1,
            "parameter_unitstyle": 9,
        }
    },
    varname="Mono PB Range",
))

prepend_mpe_bend = p.add("prepend set_mpe_bend_range",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[180, 340, 155, 22])

prepend_mono_bend = p.add("prepend set_mono_bend_range",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[260, 340, 160, 22])

# Mode: prepend → js (mode_tab routes through pattr below, not direct)
p.add_line(prepend_mode, js)

# Mode tab → pattr (store mode, saved with patch) → pak → vst~
# VST3 param index 1 = mode (0=bypass, 1=mode, 2=mpePbRange, 3=monoPbRange)
# pattr saves its value with the patch, so on session reload we can re-send it
mode_pattr = p.add_box(Box(
    id=p.get_id(), maxclass="newobj", numinlets=1, numoutlets=3,
    outlettype=["", "", ""],
    patching_rect=[30, 360, 95, 22],
    text="pattr mode_value @default 0",
))

set_mode_pak = p.add("pak 1 0.",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[30, 395, 60, 22])

# mode_tab → pattr (store on change) → pak → vst~
#                                     → prepend → js (for MIDI processing)
p.add_line(mode_tab, mode_pattr, outlet=0, inlet=0)
p.add_line(mode_pattr, set_mode_pak, outlet=0, inlet=1)
p.add_line(set_mode_pak, vst)
p.add_line(mode_pattr, prepend_mode, outlet=0, inlet=0)  # also update JS

# MPE PB: live.dial → pattr (store+output) → prepend → js
mpe_pb_pattr = p.add_box(Box(
    id=p.get_id(), maxclass="newobj", numinlets=1, numoutlets=3,
    outlettype=["", "", ""],
    patching_rect=[180, 370, 145, 22],
    text="pattr mpe_pb_value @default 48",
))
p.add_line(mpe_bend_dial, mpe_pb_pattr, outlet=0, inlet=0)
p.add_line(mpe_pb_pattr, prepend_mpe_bend, outlet=0, inlet=0)
p.add_line(prepend_mpe_bend, js)

# Mono PB: live.dial → pattr (store+output) → prepend → js
mono_pb_pattr = p.add_box(Box(
    id=p.get_id(), maxclass="newobj", numinlets=1, numoutlets=3,
    outlettype=["", "", ""],
    patching_rect=[340, 370, 150, 22],
    text="pattr mono_pb_value @default 2",
))
p.add_line(mono_bend_dial, mono_pb_pattr, outlet=0, inlet=0)
p.add_line(mono_pb_pattr, prepend_mono_bend, outlet=0, inlet=0)
p.add_line(prepend_mono_bend, js)

# State persistence: pattrstorage ensures all pattr values survive session reload in M4L
# Without this, pattr resets to @default on reload (no file-based persistence in M4L)
pattrstorage = p.add_box(Box(
    id=p.get_id(), maxclass="newobj", numinlets=1, numoutlets=1,
    outlettype=[""],
    patching_rect=[450, 430, 180, 22],
    text="pattrstorage tanghim_state @greedy 1",
))

# Session reload: after VST loads (300ms), re-send all pattr values (500ms)
# Bang each pattr to output its saved value → JS gets mode + PB ranges
delay_resend = p.add("delay 500",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[100, 115, 72, 22])

p.add_line(trig, delay_resend, outlet=0, inlet=0)
p.add_line(delay_resend, mode_pattr, outlet=0, inlet=0)
p.add_line(delay_resend, mpe_pb_pattr, outlet=0, inlet=0)
p.add_line(delay_resend, mono_pb_pattr, outlet=0, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# Save and post-process for M4L-specific patcher properties
# ═══════════════════════════════════════════════════════════════════════

p.save()

# Post-process: add M4L properties that py2max doesn't support natively
with open(OUTPUT_MAXPAT) as f:
    data = json.load(f)

patcher = data["patcher"]
patcher["openrect"] = [0.0, 0.0, 135.4765625, 169.0]
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
