# Prüft, ob eine PipeWire-Verbindung nach einem beendeten GStreamer-Prozess für einen zweiten Start taugt.
# Bis 1.7.0 gab Clipline nach jedem Neustart der Aufnahme dieselbe (schon benutzte) Verbindung weiter.
import json
import os
import socket
import subprocess
import time

rt = os.environ["XDG_RUNTIME_DIR"]
src = subprocess.Popen(["gst-launch-1.0", "-q", "videotestsrc", "is-live=true", "!",
                        "video/x-raw,width=320,height=240,framerate=30/1", "!", "pipewiresink", "mode=provide",
                        "stream-properties=props,media.class=Video/Source,node.name=cltest"])
time.sleep(3)
node = None
for o in json.loads(subprocess.run(["pw-dump"], capture_output=True, text=True).stdout):
    if o.get("type", "").endswith("Node") and o.get("info", {}).get("props", {}).get("node.name") == "cltest":
        node = o["id"]
print("test node:", node)


def connect():
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(rt + "/pipewire-0")
    os.set_inheritable(s.fileno(), True)
    return s


def run(fd, target=None):
    sel = f"target-object={target}" if target else f"path={node}"
    try:
        r = subprocess.run(["gst-launch-1.0", "-q", "pipewiresrc", f"fd={fd}", sel, "num-buffers=30", "!",
                            "fakesink"], pass_fds=[fd], timeout=15, capture_output=True, text=True)
        return "ok" if r.returncode == 0 else f"failed ({r.returncode}): {' / '.join(r.stderr.strip().splitlines()[:3])}"
    except subprocess.TimeoutExpired:
        return "hangs (no picture within 15 s)"


print(subprocess.run(["wpctl", "status"], capture_output=True, text=True).stdout[:1500])
print("default connection, by name:   ", run(connect().fileno(), "cltest"))
used = connect()
print("1st start, new connection:      ", run(used.fileno()))
print("2nd start, same connection:     ", run(used.fileno()))
print("3rd start, fresh connection:    ", run(connect().fileno()))
src.terminate()
