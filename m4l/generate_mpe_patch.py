#!/usr/bin/env python3
"""Generate Tanghim MPE Receiver.amxd — Stage 1 (skeleton + [poly] allocator).

Native Max patch with no vst~ and no JS. Reads MTS-ESP tuning via the
MTS-ESP.mtof external (m4l/externals/) and allocates one of 15 MPE voices
via [poly 15 1]. Channel 1 is reserved as the MPE manager; voices 1..15
emit on channels 2..16.

is_mpe: 1 patcher metadata is required, otherwise Live collapses output
to channel 1.

This is stage 1 — minimum viable MPE Receiver:
  - Receives MIDI from Live
  - Queries cents from MTS-ESP per Note On
  - Allocates a voice via [poly 15 1]
  - Emits PB then Note On then Note Off on the allocated MPE channel

Deliberately deferred to later stages:
  - Filter-note (mtof outlet 3) routing
  - Held-note retune (metro + coll iteration)
  - User PB-wheel combining
  - MPE Configuration RPN broadcast on load
  - Connection-status indicator

Trigger-ordering notes:
  - midiparse outlet 0 (note) → [t i i] so mtof query fires BEFORE pack
    triggers poly, ensuring cents are fresh when PB is computed.
  - poly outlet 0 (voice#) is the LAST poly outlet to fire (right-to-left
    evaluation: vel first, pitch second, voice# third). We use voice#
    arrival to trigger ordered emission of channel-set-then-PB-then-NoteOn,
    ensuring PB and Note On both go out on the freshly-allocated channel.
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
p.devicewidth = 135.4765625
p.description = "MTS-ESP MPE Receiver"
p.rect = Rect(100.0, 100.0, 900.0, 600.0)

# ═══════════════════════════════════════════════════════════════════════
# Title block (mirrors legacy Tanghim Receiver UI):
#   "تنغيم" (Cairo Black, large, centered)
#   "Tanghim MPE Receiver" (Ableton Sans Medium, smaller, centered)
# ═══════════════════════════════════════════════════════════════════════

title_arabic = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[45, 475, 292, 29],
    presentation=1, presentation_rect=[-1.0, 2.0, 136.0, 29.0],
    fontname="Cairo Black",
    text="تنغيم",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

title_latin = p.add_box(Box(
    id=p.get_id(), maxclass="comment", numinlets=1, numoutlets=0,
    patching_rect=[30, 460, 178, 21],
    presentation=1, presentation_rect=[-0.5, 33.0, 133.0, 21.0],
    fontname="Ableton Sans Medium",
    text="Tanghim MPE Receiver",
    textcolor=[0.0, 0.0, 0.0, 1.0],
    textjustification=1,
))

# ═══════════════════════════════════════════════════════════════════════
# MIDI input + parse
# ═══════════════════════════════════════════════════════════════════════

midiin = p.add("midiin",
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
# midiparse outlet 0 emits a LIST (pitch, velocity) — verified against
# legacy m4l/Tanghim Receiver.maxpat. We unpack into separate ints.
# [unpack 0 0] outputs right-to-left: outlet 1 (velocity) first, then
# outlet 0 (pitch).
# ═══════════════════════════════════════════════════════════════════════

note_unpack = p.add("unpack 0 0",
    numinlets=1, numoutlets=2, outlettype=["int", "int"],
    patching_rect=[20, 130, 100, 22])

p.add_line(midiparse, note_unpack, outlet=0, inlet=0)  # (pitch, vel) list

# ═══════════════════════════════════════════════════════════════════════
# Trigger to enforce: mtof query fires BEFORE pitch triggers poly.
# Without this, pitch arriving at poly.0 triggers allocation before
# mtof has emitted updated cents — the PB would be computed from stale
# cents.
#
# [t i i] outputs right-to-left: outlet 1 (right) fires first, outlet 0
# fires last. So we wire: outlet 1 → mtof.0, outlet 0 → poly.0.
# ═══════════════════════════════════════════════════════════════════════

note_trig = p.add("t i i",
    numinlets=1, numoutlets=2, outlettype=["int", "int"],
    patching_rect=[20, 165, 60, 22])

p.add_line(note_unpack, note_trig, outlet=0, inlet=0)  # pitch → trigger

# ═══════════════════════════════════════════════════════════════════════
# MTS-ESP cents query
#   inlet 0: MIDI note number
#   outlet 0: frequency (Hz)        (unused)
#   outlet 1: ratio                  (unused)
#   outlet 2: semitones (signed float — semitone offset from 12-EDO)
#   outlet 3: filter (0 = filtered/skip, 1 = play) — UNUSED IN STAGE 1
# ═══════════════════════════════════════════════════════════════════════

mtof = p.add("MTS-ESP.mtof",
    numinlets=1, numoutlets=4,
    outlettype=["float", "float", "float", "int"],
    patching_rect=[100, 165, 140, 22])

# Trigger right outlet (fires first) → mtof query
p.add_line(note_trig, mtof, outlet=1, inlet=0)

# cents = semitones * 100
cents_expr = p.add("expr $f1 * 100.",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[260, 165, 110, 22])
p.add_line(mtof, cents_expr, outlet=2, inlet=0)


# Stash latest cents value via [send] for downstream PB computation.
# By the time poly emits voice#, cents has been updated for the new note.
cents_send = p.add("send cents",
    numinlets=1, numoutlets=0,
    patching_rect=[380, 165, 80, 22])
p.add_line(cents_expr, cents_send)

# ═══════════════════════════════════════════════════════════════════════
# Wire (pitch, velocity) directly to [poly 15 1] — no [pack] in between.
#
# [poly]'s left inlet expects an INT (pitch); right inlet stores velocity
# silently. Sending a list to left inlet may be misinterpreted (poly
# treats lists differently than the (pitch, vel) idiom).
#
# Order:
#   1. midiparse outlet 1 (velocity) fires first (right-to-left), goes to
#      poly inlet 1 (silent store).
#   2. midiparse outlet 0 (pitch) → [t i i] trigger → mtof query first,
#      then pitch arrives at poly inlet 0 → triggers note allocation.
#
# [poly 15 1] = 15 voices, steal-mode 1 (steal oldest held when full).
# Outlets (4 total per Max docs):
#   0: pitch
#   1: velocity
#   2: voice number (1..15)
#   3 (rightmost): overflow (notes that couldn't be allocated; with
#                  steal-mode 1, this should be empty)
# Right-to-left evaluation: voice# (outlet 2) fires before vel (1)
# before pitch (0).
# ═══════════════════════════════════════════════════════════════════════

poly = p.add("poly 15 1",
    numinlets=2, numoutlets=4,
    outlettype=["int", "int", "int", ""],
    patching_rect=[20, 235, 100, 22])

# Velocity (from unpack outlet 1, fires first) → poly inlet 1 (silent store).
p.add_line(note_unpack, poly, outlet=1, inlet=1)

# Pitch (via trigger so mtof fires first) → poly inlet 0 (triggers allocation).
p.add_line(note_trig, poly, outlet=0, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# Per-voice MIDI emission with explicit ordering.
#
# [poly] outlets (verified empirically against test in Live):
#   0: voice number (1..15)
#   1: pitch
#   2: velocity
#   3: overflow (notes that couldn't be allocated; with steal-mode 1 this
#               should be empty)
# Right-to-left evaluation: outlet 2 (vel) fires first, outlet 1 (pitch)
# second, outlet 0 (voice#) third.
#
# Goal: when poly emits, we want
#   1. Channel set on xbendout.1 and noteout.2
#   2. PB emitted on xbendout.0 (uses just-set channel)
#   3. Note On (or Off) emitted on noteout.0 (uses just-set channel + vel)
#
# Strategy:
#   - poly.2 (velocity, fires first) → noteout.1 (silent store).
#   - poly.1 (pitch, fires second) → [int].1 right-inlet (stores silently).
#   - poly.0 (voice#, fires third) → +1 → channel → [t b b i i]:
#       outlet 3 (right, int): channel → xbendout.1 (set)
#       outlet 2 (int):        channel → noteout.2 (set)
#       outlet 1 (bang):       bang pb_expr → ... → xbendout.0 (PB emit)
#       outlet 0 (left, bang): bang stored-pitch → noteout.0 (Note On)
# ═══════════════════════════════════════════════════════════════════════

# voice# (poly outlet 0) + 1 = MPE channel (range 2..16; ch 1 = MPE manager)
plus1 = p.add("+ 1",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[20, 270, 60, 22])
p.add_line(poly, plus1, outlet=0, inlet=0)

# Trigger to fan out channel + bangs in correct order.
# [t b b i i]: outlets fire right-to-left.
#   outlet 3 (rightmost, int): channel for xbendout.1   — fires FIRST
#   outlet 2 (int):             channel for noteout.2   — fires second
#   outlet 1 (bang):            triggers pb_expr → xbendout.0 PB emit
#   outlet 0 (leftmost, bang):  triggers stored pitch → noteout.0 Note On — fires LAST
emit_trig = p.add("t b b i i",
    numinlets=1, numoutlets=4, outlettype=["bang", "bang", "int", "int"],
    patching_rect=[100, 270, 100, 22])
p.add_line(plus1, emit_trig, outlet=0, inlet=0)

# Pitch storage — silent until banged.
# [int]: right inlet (1) stores without output; left inlet (0) bangs to output.
# Pitch comes from poly outlet 1.
pitch_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[20, 305, 60, 22])
p.add_line(poly, pitch_store, outlet=1, inlet=1)  # store pitch silently

# ═══════════════════════════════════════════════════════════════════════
# Compute 14-bit pitch bend from cents and pbRange.
#   pb14 = clamp(int(8192 + (cents / (pbRange * 100.0)) * 8191), 0, 16383)
#
# Triggered by [emit_trig] outlet 1 (bang). The bang re-emits stored
# cents from [receive cents], which flows through the expr chain.
# ═══════════════════════════════════════════════════════════════════════

cents_recv = p.add("receive cents",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[290, 305, 80, 22])

pbrange_recv = p.add("receive pbRange",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[380, 305, 90, 22])

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
# xbendout: 14-bit pitch bend on the allocated MPE channel.
#   inlet 0: PB value (0..16383) — triggers send
#   inlet 1: channel (1..16) — set silently
#
# Wiring: emit_trig.3 (channel, fires first) → xbendout.1.
#         emit_trig.1 (bang) → bangs cents_recv → flows through pb_expr →
#                              pb_clip → xbendout.0 → triggers send.
# ═══════════════════════════════════════════════════════════════════════

# xbendout FORMATS (does not transmit). Its outlet emits the formatted
# MIDI bytes, which must be sent to [midiout] to actually reach the
# host. (Compare [bendout] which transmits directly to MIDI port.)
xbendout = p.add("xbendout",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[290, 415, 80, 22])
p.add_line(pb_clip, xbendout, outlet=0, inlet=0)
p.add_line(emit_trig, xbendout, outlet=3, inlet=1)  # channel set first (rightmost int)

# Route formatted PB bytes to MIDI output.
midiout_pb = p.add("midiout",
    numinlets=1, numoutlets=0,
    patching_rect=[290, 450, 80, 22])
p.add_line(xbendout, midiout_pb, outlet=0, inlet=0)

# Bang from emit_trig.1 → pb_expr (bang causes [expr] to recompute + output
# using stored $f1=cents and $i2=pbRange). Cannot bang [receive] — it has no
# inlet; receives are addressed by name only.
p.add_line(emit_trig, pb_expr, outlet=1, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# noteout: pitch + velocity + channel.
#   inlet 0: pitch — triggers send
#   inlet 1: velocity — set silently
#   inlet 2: channel — set silently
#
# Wiring: poly.2 (vel, fires first) → noteout.1.
#         emit_trig.2 (channel) → noteout.2.
#         emit_trig.0 (bang, fires LAST) → pitch_store.0 → outputs stored
#                                          pitch → noteout.0 → triggers Note send.
# ═══════════════════════════════════════════════════════════════════════

noteout = p.add("noteout",
    numinlets=3, numoutlets=0,
    patching_rect=[20, 345, 90, 22])

p.add_line(poly, noteout, outlet=2, inlet=1)              # velocity (silent — poly.2 = vel)
p.add_line(emit_trig, noteout, outlet=2, inlet=2)         # channel (silent, second int)
p.add_line(emit_trig, pitch_store, outlet=0, inlet=0)     # bang (leftmost) stored pitch
p.add_line(pitch_store, noteout, outlet=0, inlet=0)       # pitch (triggers Note On)

# ═══════════════════════════════════════════════════════════════════════
# PB Range live.dial parameter (mirrors legacy device's MPE PB Range dial).
# Centered horizontally in the 135.5px-wide device.
# ═══════════════════════════════════════════════════════════════════════

pbnum = p.add_box(Box(
    id=p.get_id(), maxclass="live.dial",
    numinlets=1, numoutlets=2,
    outlettype=["", "float"],
    parameter_enable=1,
    patching_rect=[180, 280, 27, 48],
    presentation=1, presentation_rect=[37.75, 92.0, 60.0, 48.0],
    saved_attribute_attributes={
        "valueof": {
            "parameter_initial": [PB_RANGE_DEFAULT],
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

pbrange_send = p.add("send pbRange",
    numinlets=1, numoutlets=0,
    patching_rect=[20, 435, 90, 22])
p.add_line(pbnum, pbrange_send)

# ═══════════════════════════════════════════════════════════════════════
# Save and post-process for M4L-specific patcher properties
# ═══════════════════════════════════════════════════════════════════════

p.save()

with open(OUTPUT_MAXPAT) as f:
    data = json.load(f)

patcher = data["patcher"]
patcher["openrect"] = [0.0, 0.0, 135.4765625, 169.0]
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
