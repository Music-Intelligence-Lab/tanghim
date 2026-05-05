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
// Per-channel state so overlapping same-pitch notes get distinct channels and
// Note Off releases the right slot. Indices 0-14 map to MIDI channels 2-16.
var channelNote = new Array(15);   // note number on slot, or -1 if free
var channelActive = new Array(15); // true if slot has a sounding note
for (var i = 0; i < 15; i++) { channelNote[i] = -1; channelActive[i] = false; }
var nextCh = 0;

// Mono PB note stack (last-note priority with recall)
var monoNoteStack = [];   // held notes in press order, last = sounding
var monoActiveNote = -1;  // currently sounding note

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
        // MPE: update bend on every member channel currently sounding this note
        var bv = calcBend(cents);
        var lsb = bv & 0x7F;
        var msb = (bv >> 7) & 0x7F;
        for (var i = 0; i < 15; i++) {
            if (channelActive[i] && channelNote[i] === note)
                outlet(0, 0xE0 + (i + 2) - 1, lsb, msb);
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
        // Currently MPE: send Note Off on each sounding member channel
        for (var i = 0; i < 15; i++) {
            if (channelActive[i])
                outlet(0, 0x80 + (i + 2) - 1, channelNote[i], 64);
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
    for (var i = 0; i < 15; i++) { channelNote[i] = -1; channelActive[i] = false; }
    nextCh = 0;
    monoNoteStack = [];
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
        // Force monophony: turn off the sounding note
        if (monoActiveNote >= 0 && monoActiveNote !== pitch)
            outlet(0, 0x80, monoActiveNote, 64);

        // Add to stack (remove first if re-triggered)
        for (var i = monoNoteStack.length - 1; i >= 0; i--) {
            if (monoNoteStack[i] === pitch) { monoNoteStack.splice(i, 1); break; }
        }
        monoNoteStack.push(pitch);

        var cents = (pitch >= 0 && pitch < 128) ? table[pitch] : 0;
        sendCombinedMonoBend(cents);         // Combined microtuning + user PB
        outlet(0, 0x90, pitch, vel);         // Note On ch1
        monoActiveNote = pitch;
    } else {
        // Remove from stack
        for (var i = monoNoteStack.length - 1; i >= 0; i--) {
            if (monoNoteStack[i] === pitch) { monoNoteStack.splice(i, 1); break; }
        }

        if (pitch === monoActiveNote) {
            outlet(0, 0x80, pitch, 64);      // Note Off ch1

            if (monoNoteStack.length > 0) {
                // Recall previously held note
                var prev = monoNoteStack[monoNoteStack.length - 1];
                var cents = (prev >= 0 && prev < 128) ? table[prev] : 0;
                sendCombinedMonoBend(cents);
                outlet(0, 0x90, prev, 100);  // Re-trigger with default velocity
                monoActiveNote = prev;
            } else {
                monoActiveNote = -1;
            }
        }
        // Swallow Note Off for notes already cut from stack
    }
}

// ── MPE ─────────────────────────────────────────────────────────────

function processMPE(pitch, vel) {
    if (vel > 0) {
        // Allocate a free member channel (2-16), round-robin
        var idx = -1;
        for (var i = 0; i < 15; i++) {
            var k = (nextCh + i) % 15;
            if (!channelActive[k]) { idx = k; break; }
        }
        if (idx === -1) {
            // All 15 channels sounding — steal the round-robin slot.
            // Send Note Off for the stolen note so the synth releases that voice.
            idx = nextCh;
            outlet(0, 0x80 + (idx + 2) - 1, channelNote[idx], 64);
        }

        channelNote[idx] = pitch;
        channelActive[idx] = true;
        nextCh = (idx + 1) % 15;
        var ch = idx + 2;

        var cents = (pitch >= 0 && pitch < 128) ? table[pitch] : 0;
        var bv = calcBend(cents);
        var lsb = bv & 0x7F;
        var msb = (bv >> 7) & 0x7F;

        outlet(0, 0xE0 + ch - 1, lsb, msb);   // PB on member channel
        outlet(0, 0x90 + ch - 1, pitch, vel);  // Note On on member channel
    } else {
        // Find the oldest sounding instance of this pitch and release it.
        // FIFO order matches Note On arrival because we walk slots from index 0.
        for (var i = 0; i < 15; i++) {
            if (channelActive[i] && channelNote[i] === pitch) {
                outlet(0, 0x80 + (i + 2) - 1, pitch, 64);
                channelActive[i] = false;
                channelNote[i] = -1;
                // Don't reset PB — synth release tail should stay at correct pitch.
                return;
            }
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
