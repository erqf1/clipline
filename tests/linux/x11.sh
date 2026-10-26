#!/bin/bash
# Linux/X11: Clip-Taste per xdotool - normal, während ein anderes Programm die Tastatur festhält
# (wie ein Vollbild-Spiel) und mehrmals hintereinander. Aufruf: x11.sh <clipline-programm> <arbeitsordner>
set -u
BIN="$(realpath "$1")"
OUT="$2"
DIR="$(cd "$(dirname "$0")" && pwd)"
source "$DIR/common.sh"

Xvfb :99 -screen 0 1280x720x24 > "$OUT/xvfb.log" 2>&1 &
export DISPLAY=:99
sleep 2
# Bewegung auf dem Bildschirm, damit die Aufnahme laufend neue Bilder hat
(while true; do xsetroot -solid "#$(printf '%06x' $((RANDOM * 500)))"; sleep 0.25; done) &

QT_QPA_PLATFORM=offscreen dbus-run-session -- "$BIN" --background > "$OUT/stdout.txt" 2>&1 &
sleep 15

press() { xdotool keydown ctrl alt s; sleep 0.15; xdotool keyup s alt ctrl; }

press
check "hotkey" 1 5

python3 "$DIR/grab.py" 14 &
GRAB=$!
sleep 2
press
check "hotkey while another program holds the keyboard" 2
wait $GRAB

for n in 3 4 5 6 7; do
    sleep 4
    press
    check "clip $n in a row" "$n" 5
done

# Clip-Taste ändern (Strg+Alt+S -> Strg+Alt+D) und Clipline neu starten: die neue Taste geht, die alte nicht mehr
pkill -f -- "$BIN --background"
sleep 1
sed -i 's/^hotkey=.*/hotkey=Ctrl+Alt+D/' "$HOME/.config/WSoftware/Clipline.conf"
QT_QPA_PLATFORM=offscreen dbus-run-session -- "$BIN" --background >> "$OUT/stdout.txt" 2>&1 &
sleep 15
xdotool keydown ctrl alt d; sleep 0.15; xdotool keyup d alt ctrl
check "changed hotkey (Ctrl+Alt+D)" 8 5
before=$(count)
press
sleep 6
if [ "$(count)" -gt "$before" ]; then echo "FAIL old hotkey still saves a clip"; FAILS=$((FAILS + 1)); else echo "PASS old hotkey does nothing any more"; fi

finish
exit $FAILS
