// Tanghim Mono PB Receiver — receiver registry
// Writes ~/Library/Tanghim/receivers/<uuid>.monopb on loadbang.
// No deletion — transmitter cleans stale files (>360s) from its own timer.

inlets = 0;
outlets = 0;

function loadbang() {
    var dir = "~/Library/Tanghim/receivers";
    var uuid = makeUUID();
    var path = dir + "/" + uuid + ".monopb";
    var f = new File(path, "write");
    if (f.isopen) {
        f.writeline("");
        f.close();
    } else {
        post("Tanghim registry: could not write " + path + "\n");
    }
}

function makeUUID() {
    function s4() {
        return Math.floor((1 + Math.random()) * 0x10000).toString(16).substring(1);
    }
    return s4()+s4()+"-"+s4()+"-"+s4()+"-"+s4()+"-"+s4()+s4()+s4();
}
