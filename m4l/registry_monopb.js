// Tanghim Mono PB Receiver — receiver registry
// loadbang: writes ~/Library/Tanghim/receivers/<uuid>.monopb, heartbeats every 30s.
// freebang: overwrites file with "closed" for immediate removal from badge count.
// Transmitter skips files whose first line is "closed"; cleanStale() removes them after 90s.

inlets = 0;
outlets = 0;

var gPath = null;
var gTask = null;

function loadbang() {
    var dir = "~/Library/Tanghim/receivers";
    var uuid = makeUUID();
    gPath = dir + "/" + uuid + ".monopb";
    touch("open");
    gTask = new Task(heartbeat, this);
    gTask.interval = 30000;
    gTask.repeat();
}

function freebang() {
    if (gTask) { gTask.cancel(); gTask = null; }
    if (gPath) touch("closed");
}

function heartbeat() {
    if (gPath) touch("open");
}

function touch(content) {
    var f = new File(gPath, "write");
    if (f.isopen) {
        f.writeline(content);
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
