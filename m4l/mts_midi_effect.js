// mts_midi_effect.js — MIDI processing for Tanghīm M4L device
// Inlet 0: MIDI notes (pitch vel) from midiparse outlet 0
// Inlet 1: tuning data (note cents) from vst~ parameter polling
// Outlet 0: processed MIDI bytes → midiout

autowatch = 1;
inlets = 2;
outlets = 1;

setinletassist(0, "MIDI notes (pitch vel)");
setinletassist(1, "tuning data (note cents)");
setoutletassist(0, "processed MIDI bytes");

// Tuning table: cents deviation per MIDI note
var table = new Array(128);
for (var i = 0; i < 128; i++) table[i] = 0.0;

var mpeBendRange = 48;   // semitones (default MPE)
var monoBendRange = 2;   // semitones (default Mono PB)
var mode = 0;            // 0 = MPE, 1 = Mono PB (matches live.tab index)

// MPE channel allocation (channels 2-16)
var noteToChannel = {};  // note → channel
var channelInUse = new Array(15);  // index 0-14 → channels 2-16
for (var i = 0; i < 15; i++) channelInUse[i] = false;
var nextCh = 0;

// Mono PB active note tracking (monophonic)
var monoActiveNote = -1;

// User pitch bend wheel tracking (for combining with microtuning in Mono PB)
var userPitchBend = 8192;  // center (no user bend)

// ── Inlet handlers ──────────────────────────────────────────────────

function list() {
    if (inlet === 0) {
        // MIDI note: pitch velocity
        var pitch = arguments[0];
        var vel = arguments[1];
        if (mode === 0)
            processMPE(pitch, vel);
        else
            processMonoPB(pitch, vel);
    }
    else if (inlet === 1) {
        // Tuning data: note cents
        var note = arguments[0];
        var cents = arguments[1];
        if (note >= 0 && note < 128) {
            var oldCents = table[note];
            table[note] = cents;

            // If tuning changed and note is active, update pitch bend in real-time
            if (cents !== oldCents)
                updateActivePitchBend(note, cents);
        }
    }
}

// ── Real-time pitch bend update for held notes ─────────────────────

function updateActivePitchBend(note, cents) {
    if (mode === 0) {
        // MPE: check if this note is active on a member channel
        var ch = noteToChannel[note];
        if (ch !== undefined) {
            var bv = calcBend(cents);
            var lsb = bv & 0x7F;
            var msb = (bv >> 7) & 0x7F;
            outlet(0, 0xE0 + ch - 1, lsb, msb);
        }
    } else {
        // Mono PB: check if this is the currently held note
        if (note === monoActiveNote) {
            sendCombinedMonoBend(cents);
        }
    }
}

// ── Control messages ────────────────────────────────────────────────

function set_mode(m) {
    // Force Note Off for all active notes in CURRENT mode before switching
    if (mode === 0) {
        // Currently MPE: send Note Off on each allocated channel
        for (var pitch in noteToChannel) {
            var ch = noteToChannel[pitch];
            outlet(0, 0x80 + ch - 1, parseInt(pitch), 64);
        }
    } else {
        // Currently Mono PB: send Note Off for the active note (allows release tail)
        if (monoActiveNote >= 0) {
            outlet(0, 0x80, monoActiveNote, 64);
            monoActiveNote = -1;
        }
    }

    mode = m;
    // Reset state
    noteToChannel = {};
    for (var i = 0; i < 15; i++) channelInUse[i] = false;
    nextCh = 0;
    userPitchBend = 8192;

    if (m === 0) {
        // Entering MPE: reset PB on ch1 so leftover Mono PB doesn't
        // act as MPE zone-wide offset (manager channel PB offsets all members)
        outlet(0, 0xE0, 0, 64);
    } else {
        // Entering Mono PB: reset PB on MPE member channels (ch 2-16)
        for (var i = 1; i <= 15; i++) {
            outlet(0, 0xE0 + i, 0, 64);
        }
    }
}

function set_mpe_bend_range(r) {
    mpeBendRange = Math.max(1, Math.min(96, r));
}

function set_mono_bend_range(r) {
    monoBendRange = Math.max(2, Math.min(96, r));
}

// ── Incoming pitch bend wheel handler ────────────────────────────────
// Routed from midiparse outlet 5 via "prepend pitchbend"

function pitchbend(val) {
    // midiparse outlet 5 outputs 0-127 (7-bit, MSB only).
    // Convert to 14-bit for internal tracking: 64→8192 (center)
    userPitchBend = val << 7;
    if (mode === 0) {
        // MPE: forward on ch1 as raw MIDI PB bytes (LSB=0, MSB=val)
        outlet(0, 0xE0, 0, val);
    } else {
        // Mono PB: recalculate combined PB for active note
        if (monoActiveNote >= 0) {
            sendCombinedMonoBend(table[monoActiveNote]);
        }
    }
}

// ── Combined microtuning + user PB for Mono mode ────────────────────

function sendCombinedMonoBend(cents) {
    var microBend = calcBend(cents);
    var userOffset = userPitchBend - 8192;
    var combined = Math.max(0, Math.min(16383, microBend + userOffset));
    var lsb = combined & 0x7F;
    var msb = (combined >> 7) & 0x7F;
    outlet(0, 0xE0, lsb, msb);
}

// ── Mono Pitch Bend ─────────────────────────────────────────────────

function processMonoPB(pitch, vel) {
    if (vel > 0) {
        // Force monophony: turn off the previous note if one is held
        if (monoActiveNote >= 0 && monoActiveNote !== pitch)
            outlet(0, 0x80, monoActiveNote, 64);

        var cents = (pitch >= 0 && pitch < 128) ? table[pitch] : 0;
        sendCombinedMonoBend(cents);         // Combined microtuning + user PB
        outlet(0, 0x90, pitch, vel);         // Note On ch1
        monoActiveNote = pitch;
    } else {
        if (pitch === monoActiveNote) {
            outlet(0, 0x80, pitch, 64);      // Note Off ch1
            monoActiveNote = -1;
        }
        // Swallow Note Off for notes we already cut
    }
}

// ── MPE ─────────────────────────────────────────────────────────────

function processMPE(pitch, vel) {
    if (vel > 0) {
        // Allocate a free member channel (2-16), round-robin with in-use check
        var ch = -1;
        for (var i = 0; i < 15; i++) {
            var idx = (nextCh + i) % 15;
            if (!channelInUse[idx]) {
                ch = idx + 2;
                nextCh = (idx + 1) % 15;
                channelInUse[idx] = true;
                break;
            }
        }
        if (ch === -1) {
            // All 15 channels in use — steal oldest (round-robin position)
            ch = nextCh + 2;
            channelInUse[nextCh] = true;
            nextCh = (nextCh + 1) % 15;
        }
        noteToChannel[pitch] = ch;

        var cents = (pitch >= 0 && pitch < 128) ? table[pitch] : 0;
        var bv = calcBend(cents);
        var lsb = bv & 0x7F;
        var msb = (bv >> 7) & 0x7F;

        outlet(0, 0xE0 + ch - 1, lsb, msb);   // PB on member channel
        outlet(0, 0x90 + ch - 1, pitch, vel);  // Note On on member channel
    } else {
        var ch = noteToChannel[pitch];
        if (ch !== undefined) {
            outlet(0, 0x80 + ch - 1, pitch, 64);  // Note Off
            // Don't reset PB — synth release tail should stay at correct pitch.
            channelInUse[ch - 2] = false;
            delete noteToChannel[pitch];
        }
    }
}

// ── Pitch bend calculation ──────────────────────────────────────────

function calcBend(cents) {
    var br = (mode === 0) ? mpeBendRange : monoBendRange;
    if (br <= 0) return 8192;
    var ratio = cents / (br * 100.0);
    var bv = Math.round(8192 + ratio * 8192);
    return Math.max(0, Math.min(16383, bv));
}
