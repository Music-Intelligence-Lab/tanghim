// Tanghim Mono PB Receiver — receiver registry
// Heartbeats every 2s. Transmitter stale cutoff is 5s, so a deleted device
// disappears from the badge count within ~5s. freebang is unreliable in M4L.
//
// The registry dir must match the C++ Transmitter's scan dir, which is JUCE
// userApplicationDataDirectory/Tanghim/receivers — a per-OS location:
//   macOS   -> ~/Library/Tanghim/receivers
//   Windows -> ~/AppData/Roaming/Tanghim/receivers   (= %APPDATA%, = CSIDL_APPDATA)
// Max resolves "~" to the user home folder on both platforms. This M4L device
// has no C++ component, so it cannot create the dir (the legacy js File object
// has no mkdir API); the Transmitter creates it at startup instead.

inlets = 0;
outlets = 0;

var gPath = null;
var gTask = null;

// Per-OS registry dir. No fallback: an unrecognised OS disables the heartbeat
// rather than guessing a default that would write to the wrong place.
function registryDir() {
    if (max.os == "macintosh") return "~/Library/Tanghim/receivers";
    if (max.os == "windows")   return "~/AppData/Roaming/Tanghim/receivers";
    post("Tanghim registry: unsupported max.os '" + max.os + "' - heartbeat disabled\n");
    return null;
}

function loadbang() {
    var dir = registryDir();
    if (dir === null) return;
    var uuid = makeUUID();
    gPath = dir + "/" + uuid + ".monopb";
    // One-shot self-report: reveals the resolved path + OS in the Max console
    // so a Windows load can be verified. isopen=true is necessary but NOT
    // sufficient — the authoritative check is the Transmitter's Mono PB badge.
    post("Tanghim registry [" + max.os + "]: " + gPath + "\n");
    touch(true);
    gTask = new Task(touch, this);
    gTask.interval = 2000;
    gTask.repeat();
}

function freebang() {
    if (gTask) { gTask.cancel(); gTask = null; }
}

function touch(verbose) {
    var f = new File(gPath, "write");
    if (f.isopen) {
        f.writeline("open");
        f.close();
        if (verbose)
            post("Tanghim registry: isopen=true (confirm via Transmitter badge)\n");
    } else {
        // Always report a failed write — catches a missing receivers dir.
        post("Tanghim registry: WRITE FAILED for " + gPath + "\n");
    }
}

function makeUUID() {
    function s4() {
        return Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
    }
    return s4()+s4()+"-"+s4()+"-"+s4()+"-"+s4()+"-"+s4()+s4()+s4();
}
