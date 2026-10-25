# Hält die Tastatur fest wie ein Vollbild-Spiel (XGrabKeyboard). Aufruf: python3 grab.py <sekunden>
import sys
import time

from Xlib import X, display

d = display.Display()
screen = d.screen()
w = screen.root.create_window(0, 0, 300, 300, 0, screen.root_depth)
w.map()
d.sync()
time.sleep(0.5)
result = w.grab_keyboard(True, X.GrabModeAsync, X.GrabModeAsync, X.CurrentTime)
print("keyboard grab:", "ok" if result == X.GrabSuccess else result, flush=True)
end = time.time() + float(sys.argv[1])
while time.time() < end:
    while d.pending_events():
        d.next_event()
    time.sleep(0.05)
d.ungrab_keyboard(X.CurrentTime)
d.sync()
