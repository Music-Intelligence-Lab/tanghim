// mts_midi_effect.js — MIDI processing for Arabic Maqam Tuner M4L device
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

var bendRange = 48;  // semitones (default MPE)
var mode = 0;        // 0 = MPE, 1 = Mono PB (matches live.menu index)

// MPE channel allocation (channels 2-16)
var noteToChannel = {};  // note → channel
var nextCh = 0;

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
        if (note >= 0 && note < 128)
            table[note] = cents;
    }
}

// ── Control messages ────────────────────────────────────────────────

function set_mode(m) {
    mode = m;
    // Reset MPE state on mode switch
    noteToChannel = {};
    nextCh = 0;
}

function set_bend_range(r) {
    bendRange = Math.max(1, Math.min(96, r));
}

// ── Mono Pitch Bend ─────────────────────────────────────────────────

function processMonoPB(pitch, vel) {
    if (vel > 0) {
        var cents = (pitch >= 0 && pitch < 128) ? table[pitch] : 0;
        var bv = calcBend(cents);
        var lsb = bv & 0x7F;
        var msb = (bv >> 7) & 0x7F;
        outlet(0, 0xE0, lsb, msb);          // Pitch Bend ch1
        outlet(0, 0x90, pitch, vel);         // Note On ch1
    } else {
        outlet(0, 0x80, pitch, 64);          // Note Off ch1
        outlet(0, 0xE0, 0, 64);             // PB reset to center
    }
}

// ── MPE ─────────────────────────────────────────────────────────────

function processMPE(pitch, vel) {
    if (vel > 0) {
        // Allocate member channel (2-16)
        var ch = (nextCh % 15) + 2;
        nextCh = (nextCh + 1) % 15;
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
            outlet(0, 0xE0 + ch - 1, 0, 64);      // PB reset to center
            delete noteToChannel[pitch];
        }
    }
}

// ── Pitch bend calculation ──────────────────────────────────────────

function calcBend(cents) {
    if (bendRange <= 0) return 8192;
    var ratio = cents / (bendRange * 100.0);
    var bv = Math.round(8192 + ratio * 8192);
    return Math.max(0, Math.min(16383, bv));
}
