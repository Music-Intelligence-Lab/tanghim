// Tanghim Mono PB Receiver — receiver registry
// Heartbeats every 2s. Transmitter stale cutoff is 5s, so a deleted device
// disappears from the badge count within ~5s. freebang is unreliable in M4L.

inlets = 0;
outlets = 0;

var gPath = null;
var gTask = null;

function loadbang() {
    var dir = "~/Library/Tanghim/receivers";
    var uuid = makeUUID();
    gPath = dir + "/" + uuid + ".monopb";
    touch();
    gTask = new Task(touch, this);
    gTask.interval = 2000;
    gTask.repeat();
}

function freebang() {
    if (gTask) { gTask.cancel(); gTask = null; }
}

function touch() {
    var f = new File(gPath, "write");
    if (f.isopen) {
        f.writeline("open");
        f.close();
    } else {
        post("Tanghim registry: could not write " + gPath + "\n");
    }
}

function makeUUID() {
    function s4() {
        return Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
    }
    return s4()+s4()+"-"+s4()+"-"+s4()+"-"+s4()+"-"+s4()+s4()+s4();
}
