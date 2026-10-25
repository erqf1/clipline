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

finish
exit $FAILS
