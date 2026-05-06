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


# Silent storage of latest cents value via [int].right inlet.
#
# We deliberately do NOT use [send cents] → [receive cents] here because
# receive auto-triggers downstream pb_expr → xbendout, causing a race:
# every mtof query (Note On AND retune) would emit PB on whatever
# channel xbendout.1 was last set to, BEFORE the current note's channel
# is established. Symptom: tuning of previously-held notes shifts every
# time a new note is added.
#
# Instead, we silently store the cents value here and explicitly bang
# this [int] from the Note On chain (emit_trig.2) and the retune chain
# only when the correct xbendout channel has been established.
cents_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[380, 165, 60, 22])
p.add_line(cents_expr, cents_store, outlet=0, inlet=1)  # silent store

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

# ═══════════════════════════════════════════════════════════════════════
# Filter-note routing: drop notes that mtof marks as filtered.
#
# mtof outlet 3 emits filter (1 = play, 0 = filtered). We multiply the
# incoming velocity by filter so filtered Note Ons arrive at poly with
# vel=0 (which poly treats as no-op / Note Off lookup).
#
# Order of operations (verified against Max's right-to-left evaluation):
#   1. unpack outlet 1 (vel) fires first  → [* 1] right inlet (silent store)
#   2. unpack outlet 0 (pitch) fires      → [t i i] → mtof query
#   3. mtof outlet 3 (filter, rightmost)  → [* 1] left inlet (TRIGGERS,
#                                              output = filter * vel)
#   4. [* 1] output                       → poly inlet 1 (silent store)
#   5. mtof outlet 2 (semitones)          → cents chain (PB)
#   6. t i i outlet 0 (pitch)             → poly inlet 0 (triggers allocation
#                                              with filtered vel)
#
# Note Off (incoming vel=0) → 0 * filter = 0 → poly frees voice by pitch.
# Filtered Note On (vel=N, filter=0) → 0 → poly ignores.
# Allowed Note On (vel=N, filter=1) → N → poly allocates.
# ═══════════════════════════════════════════════════════════════════════

vel_gate = p.add("* 1",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[150, 200, 60, 22])
p.add_line(note_unpack, vel_gate, outlet=1, inlet=1)  # vel → right (silent)
p.add_line(mtof, vel_gate, outlet=3, inlet=0)         # filter → left (triggers)

poly = p.add("poly 15 1",
    numinlets=2, numoutlets=4,
    outlettype=["int", "int", "int", ""],
    patching_rect=[20, 235, 100, 22])

# Filtered velocity → poly inlet 1 (silent store).
p.add_line(vel_gate, poly, outlet=0, inlet=1)

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
# [t b b b i i]: outlets fire right-to-left.
#   outlet 4 (rightmost, int): channel for xbendout.1   — fires FIRST
#   outlet 3 (int):             channel for noteout.2   — fires second
#   outlet 2 (bang):            triggers pb_expr → xbendout.0 PB emit
#   outlet 1 (bang):            triggers stored pitch → noteout.0 Note On
#   outlet 0 (leftmost, bang):  triggers held-coll update — fires LAST
emit_trig = p.add("t b b b i i",
    numinlets=1, numoutlets=5,
    outlettype=["bang", "bang", "bang", "int", "int"],
    patching_rect=[100, 270, 100, 22])
p.add_line(plus1, emit_trig, outlet=0, inlet=0)

# Silent stores for held-note tracking (Stage 2b).
# Velocity, channel, and pitch are captured silently from poly so the
# coll-update logic (driven by emit_trig.0) can read them after Note
# On/Off has already been emitted.
vel_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[260, 305, 60, 22])
p.add_line(poly, vel_store, outlet=2, inlet=1)  # silent store of velocity

# Two separate chan_stores so the store and remove paths don't trigger
# each other when banged. Both fed silently from +1 (channel from poly).
chan_store_for_remove = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[200, 305, 60, 22])
p.add_line(plus1, chan_store_for_remove, outlet=0, inlet=1)

chan_store_for_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[140, 305, 60, 22])
p.add_line(plus1, chan_store_for_store, outlet=0, inlet=1)

# Two pitch storages so the remove and store paths can each bang their
# own [int] without firing the other path's downstream chain.
pitch_for_remove = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[330, 305, 60, 22])
p.add_line(poly, pitch_for_remove, outlet=1, inlet=1)  # silent store

pitch_for_store = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[400, 305, 60, 22])
p.add_line(poly, pitch_for_store, outlet=1, inlet=1)  # silent store

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
# pb_expr is triggered ONLY by explicit bangs to cents_store.left
# (which outputs the silently-stored cents into pb_expr's $f1 inlet).
# This decoupling prevents pb_expr from auto-firing every time mtof
# updates cents — see cents_store comment for the race-condition
# rationale.
# ═══════════════════════════════════════════════════════════════════════

pbrange_recv = p.add("receive pbRange",
    numinlets=0, numoutlets=1, outlettype=[""],
    patching_rect=[380, 305, 90, 22])

pb_expr = p.add("expr int(8192 + ($f1 / ($f2 * 100.)) * 8191)",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[290, 340, 280, 22])
p.add_line(cents_store, pb_expr, outlet=0, inlet=0)  # cents (when banged)
p.add_line(pbrange_recv, pb_expr, outlet=0, inlet=1)  # pbRange (auto-stored)

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
p.add_line(emit_trig, xbendout, outlet=4, inlet=1)  # channel set first (rightmost int)

# Route formatted PB bytes to MIDI output.
midiout_pb = p.add("midiout",
    numinlets=1, numoutlets=0,
    patching_rect=[290, 450, 80, 22])
p.add_line(xbendout, midiout_pb, outlet=0, inlet=0)

# Bang from emit_trig.2 → cents_store.left → outputs stored cents → pb_expr
# (which triggers compute + output using current $f2=pbRange) → pb_clip →
# xbendout.0 (PB emits on the channel just set by emit_trig.4).
p.add_line(emit_trig, cents_store, outlet=2, inlet=0)

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
p.add_line(emit_trig, noteout, outlet=3, inlet=2)         # channel (silent, second int)
p.add_line(emit_trig, pitch_store, outlet=1, inlet=0)     # bang stored pitch
p.add_line(pitch_store, noteout, outlet=0, inlet=0)       # pitch (triggers Note On)

# ═══════════════════════════════════════════════════════════════════════
# Held-note tracking (Stage 2b — v2)
#
# coll key/value scheme: KEY = MPE channel (2..16), VALUE = pitch.
# Why channel-as-key (not pitch-as-key): coll's `dump` emits ALL data
# first then ALL addresses (NOT per-entry interleaved). To iterate
# per-entry, we send sequential channel ints (2..16) via [uzi] and let
# coll respond per-lookup with (data, address) in proper order.
#
# emit_trig.0 (leftmost bang, fires LAST after Note On/Off is emitted)
# drives the coll-update logic. Read velocity from vel_store; branch:
#   - Vel == 0 → bang chan_store → "remove <channel>" → coll
#   - Vel >  0 → trigger [t b b] → bang chan (pak.left, triggers) and
#                  bang pitch (pak.right, silent) → "<channel> <pitch>"
#                  → coll stores pitch at key=channel
# ═══════════════════════════════════════════════════════════════════════

held_coll = p.add("coll heldByChannel @embed 1",
    patching_rect=[450, 250, 200, 22],
    varname="heldByChannel")

# Bang vel_store from emit_trig.0 → output velocity → branch
p.add_line(emit_trig, vel_store, outlet=0, inlet=0)  # bang triggers output

# select 0: matched → bang on outlet 0; unmatched → int passes through outlet 1
vel_branch = p.add("select 0",
    numinlets=1, numoutlets=2, outlettype=["bang", "int"],
    patching_rect=[260, 345, 80, 22])
p.add_line(vel_store, vel_branch, outlet=0, inlet=0)

# REMOVE PATH (vel == 0): bang chan_store_for_remove → "remove <channel>" → coll
p.add_line(vel_branch, chan_store_for_remove, outlet=0, inlet=0)  # bang
prepend_remove = p.add("prepend remove",
    numinlets=1, numoutlets=1, outlettype=[""],
    patching_rect=[330, 380, 130, 22])
p.add_line(chan_store_for_remove, prepend_remove, outlet=0, inlet=0)
p.add_line(prepend_remove, held_coll, outlet=0, inlet=0)

# STORE PATH (vel > 0): vel int triggers [t b b]
#   .1 (right, fires first): bang pitch_for_store → output pitch → pak.right (silent)
#   .0 (left, fires second): bang chan_store → output channel → pak.left (triggers)
# pak emits "<channel> <pitch>" → coll stores pitch at key=channel.
store_trig = p.add("t b b",
    numinlets=1, numoutlets=2, outlettype=["bang", "bang"],
    patching_rect=[480, 345, 80, 22])
p.add_line(vel_branch, store_trig, outlet=1, inlet=0)  # vel passes through as bang

store_pak = p.add("pack 0 0",
    numinlets=2, numoutlets=1, outlettype=[""],
    patching_rect=[480, 415, 80, 22])
# t.1 fires first: bang pitch_for_store → pak.1 (silent right inlet)
p.add_line(store_trig, pitch_for_store, outlet=1, inlet=0)
p.add_line(pitch_for_store, store_pak, outlet=0, inlet=1)
# t.0 fires second: bang chan_store_for_store → pak.0 (triggers output as channel)
p.add_line(store_trig, chan_store_for_store, outlet=0, inlet=0)
p.add_line(chan_store_for_store, store_pak, outlet=0, inlet=0)
p.add_line(store_pak, held_coll, outlet=0, inlet=0)

# DIAGNOSTIC: confirm store path actually emits to coll on Note On
store_print = p.add("print store_to_coll",
    numinlets=1, numoutlets=0,
    patching_rect=[600, 450, 180, 22])
p.add_line(store_pak, store_print, outlet=0, inlet=0)

# DIAGNOSTIC: confirm remove path on Note Off
remove_print = p.add("print remove_to_coll",
    numinlets=1, numoutlets=0,
    patching_rect=[600, 480, 180, 22])
p.add_line(prepend_remove, remove_print, outlet=0, inlet=0)

# ═══════════════════════════════════════════════════════════════════════
# Held-note retune (Stage 2b — v2)
#
# Iterate via [uzi 15] which emits 1..15. Add 1 → channels 2..16.
# Each int → coll left inlet → coll looks up entry at that key:
#   - If entry exists: outputs value (pitch) on outlet 0, then key
#     (channel) on outlet 1 — PER-LOOKUP, properly interleaved.
#   - If no entry at that key: emits nothing (silent skip).
#
# Per-entry retune wiring:
#   - coll outlet 0 (data = pitch) → mtof.0 (mtof.2 silently updates
#                                    cents_store)
#   - coll outlet 1 (address = channel) → [t b i]:
#       outlet 1 (i, fires first):  channel → xbendout.1 (set)
#       outlet 0 (b, fires second): bang cents_store → PB emit on the
#                                   just-set channel
# ═══════════════════════════════════════════════════════════════════════

# Held-note retune polled at 20 Hz (metro 50). Each held note costs
# ~12 scheduler messages per tick (coll dump + mtof query + PB emit
# chain). At 8 held notes × 20 Hz × 12 = 1920 msg/sec which approaches
# the jitter threshold; at 4 notes × 20 Hz × 12 = 960 msg/sec we're
# safely below. Lowering further is possible if needed (e.g. metro 33
# for 30 Hz) but we'd want empirical jitter testing.
# Held-note retune polled at 20 Hz (metro 50). Each metro tick triggers
# [uzi 15] which emits 15 sequential bangs/ints (1..15), each gated through
# [+ 1] into channels (2..16). Each channel is sent to coll as a key
# lookup; coll responds per-lookup if entry exists.
retune_metro = p.add("metro 50",
    numinlets=2, numoutlets=1, outlettype=["bang"],
    patching_rect=[700, 60, 80, 22])

# DIAGNOSTIC: confirm metro is firing
metro_print = p.add("print metro_tick",
    numinlets=1, numoutlets=0,
    patching_rect=[820, 60, 140, 22])
p.add_line(retune_metro, metro_print, outlet=0, inlet=0)

# DIAGNOSTIC: confirm uzi is firing
uzi_print = p.add("print uzi_out",
    numinlets=1, numoutlets=0,
    patching_rect=[820, 90, 140, 22])

# DIAGNOSTIC: confirm +1 is firing (channels 2..16)
plus1_print = p.add("print uzi_chan",
    numinlets=1, numoutlets=0,
    patching_rect=[820, 120, 140, 22])

# DIAGNOSTIC: confirm coll receives lookups
coll_in_print = p.add("print coll_lookup_key",
    numinlets=1, numoutlets=0,
    patching_rect=[820, 150, 180, 22])

# Always-on: loadbang triggers a "1" message that starts the metro.
retune_loadbang = p.add("loadbang",
    numinlets=1, numoutlets=1, outlettype=["bang"],
    patching_rect=[700, 5, 70, 22])
retune_start = p.add_box(Box(
    id=p.get_id(), maxclass="message",
    patching_rect=[700, 30, 30, 22],
    text="1",
))
p.add_line(retune_loadbang, retune_start, outlet=0, inlet=0)
p.add_line(retune_start, retune_metro, outlet=0, inlet=0)

# Metro → [uzi 15] → emits 15 bangs per tick (outlet 0)
# Empirically verified: uzi's other outlets in this Max version don't
# emit the count integer reliably. So we drive a [counter 0 2 16]
# from uzi's bang stream — each bang increments and outputs 2..16.
retune_uzi = p.add("uzi 15",
    patching_rect=[700, 90, 60, 22])
p.add_line(retune_metro, retune_uzi, outlet=0, inlet=0)

# counter 0 2 16: direction=up (0), min=2, max=16. 15 bangs per metro
# tick → outputs 2, 3, 4, ..., 16. Wraps cleanly each tick.
retune_counter = p.add("counter 0 2 16",
    patching_rect=[700, 120, 80, 22])
p.add_line(retune_uzi, retune_counter, outlet=0, inlet=0)

# Diagnostic prints (kept until live retune is verified)
p.add_line(retune_uzi, uzi_print, outlet=0, inlet=0)
p.add_line(retune_counter, plus1_print, outlet=0, inlet=0)

# Channel storage for retune emission (the chain after coll lookup
# doesn't get the channel from coll — outlet 1/address only emits on
# bang/dump/next/prev/sub triggers, NOT on direct int-key lookup. So we
# capture the channel here from the uzi+1 stream directly.)
retune_chan = p.add("int",
    numinlets=2, numoutlets=1, outlettype=["int"],
    patching_rect=[450, 285, 60, 22])

# Channel C → [t i i]:
#   outlet 1 (rightmost, fires first):  C → retune_chan.right (silent set)
#   outlet 0 (leftmost, fires second):  C → coll.0 (lookup — if entry exists,
#                                       coll emits stored pitch on its outlet 0)
retune_dispatch = p.add("t i i",
    numinlets=1, numoutlets=2, outlettype=["int", "int"],
    patching_rect=[700, 150, 60, 22])
p.add_line(retune_counter, retune_dispatch, outlet=0, inlet=0)
p.add_line(retune_dispatch, retune_chan, outlet=1, inlet=1)  # silent set first
p.add_line(retune_dispatch, held_coll, outlet=0, inlet=0)     # lookup second
p.add_line(retune_dispatch, coll_in_print, outlet=0, inlet=0)  # diagnostic

# coll outlet 0 (pitch) ONLY emits when the lookup matches a stored entry.
# This is our gate — emission chain runs only for held channels, not all 15.
#
# pitch → [t b i]:
#   outlet 1 (i, fires first):  pitch → mtof.0 (silent cents_store update)
#   outlet 0 (b, fires second) → final emit chain (channel set + PB emit)
retune_emit_pitch = p.add("t b i",
    numinlets=1, numoutlets=2, outlettype=["bang", "int"],
    patching_rect=[450, 360, 60, 22])
p.add_line(held_coll, retune_emit_pitch, outlet=0, inlet=0)
# .1 (i, fires first): pitch → mtof.0
p.add_line(retune_emit_pitch, mtof, outlet=1, inlet=0)

# DIAGNOSTIC: confirm coll lookup returned a pitch (= held channel found)
coll_print = p.add("print retune_coll_pitch",
    numinlets=1, numoutlets=0,
    patching_rect=[600, 360, 180, 22])
p.add_line(held_coll, coll_print, outlet=0, inlet=0)

# .0 (b, fires second) → [t b b] sub-sequencer:
#   .1 (fires first):  bang retune_chan → outputs channel → xbendout.1 (set)
#   .0 (fires second): bang cents_store → outputs cents → pb_expr → pb_clip
#                      → xbendout.0 (PB emits on the just-set channel)
retune_emit_final = p.add("t b b",
    numinlets=1, numoutlets=2, outlettype=["bang", "bang"],
    patching_rect=[450, 395, 80, 22])
p.add_line(retune_emit_pitch, retune_emit_final, outlet=0, inlet=0)
p.add_line(retune_emit_final, retune_chan, outlet=1, inlet=0)
p.add_line(retune_chan, xbendout, outlet=0, inlet=1)
p.add_line(retune_emit_final, cents_store, outlet=0, inlet=0)

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
