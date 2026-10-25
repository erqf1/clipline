#!/bin/bash
# Linux/Wayland (Sway ohne Bildschirm): Aufnahme über das Desktop-Portal + PipeWire, Taste über swaymsg.
# Prüft die Taste, Clips nach einem Neustart der Aufnahme und die Taste nach "swaymsg reload".
# Aufruf: wayland.sh <clipline-programm> <arbeitsordner>
set -u
if [ -z "${CLIPLINE_INNER:-}" ]; then
    export CLIPLINE_INNER=1
    exec dbus-run-session -- bash "$0" "$@"
fi
BIN="$(realpath "$1")"
OUT="$2"
DIR="$(cd "$(dirname "$0")" && pwd)"
source "$DIR/common.sh"

export XDG_RUNTIME_DIR="$OUT/xdg"
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
export XDG_CURRENT_DESKTOP=sway XDG_SESSION_TYPE=wayland QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME/.config/xdg-desktop-portal-wlr" "$HOME/.config/xdg-desktop-portal"
printf '[screencast]\noutput_name=HEADLESS-1\nchooser_type=none\nmax_fps=30\n' > "$HOME/.config/xdg-desktop-portal-wlr/config"
printf '[preferred]\ndefault=wlr;gtk\n' > "$HOME/.config/xdg-desktop-portal/sway-portals.conf"

pipewire > "$OUT/pipewire.log" 2>&1 &
sleep 1
wireplumber > "$OUT/wireplumber.log" 2>&1 &
sleep 1
printf 'output HEADLESS-1 resolution 1280x720\n' > "$OUT/sway.conf"
WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 sway -c "$OUT/sway.conf" > "$OUT/sway.log" 2>&1 &
for _ in $(seq 1 50); do ls "$XDG_RUNTIME_DIR"/sway-ipc.*.sock > /dev/null 2>&1 && break; sleep 0.2; done
export SWAYSOCK="$(ls "$XDG_RUNTIME_DIR"/sway-ipc.*.sock | head -1)"
export WAYLAND_DISPLAY="$(ls "$XDG_RUNTIME_DIR" | grep -E '^wayland-[0-9]+$' | head -1)"
echo "sway: $SWAYSOCK / $WAYLAND_DISPLAY"
dbus-update-activation-environment WAYLAND_DISPLAY XDG_CURRENT_DESKTOP XDG_SESSION_TYPE SWAYSOCK XDG_RUNTIME_DIR
/usr/libexec/xdg-desktop-portal-wlr -l DEBUG > "$OUT/xdpw.log" 2>&1 &
sleep 1
/usr/libexec/xdg-desktop-portal -r -v > "$OUT/xdp.log" 2>&1 &
sleep 2
swaymsg -t get_outputs | grep -E '"name"|"active"|"current_mode"' -A0 | head -5
grim "$OUT/shot.png" && echo "screencopy works (grim)" || echo "screencopy does NOT work here (grim failed)"
# Bewegung auf dem Bildschirm
(while true; do swaymsg -q output HEADLESS-1 bg "#$(printf '%06x' $((RANDOM * 500)))" solid_color; sleep 0.25; done) &

"$BIN" --background > "$OUT/stdout.txt" 2>&1 &
sleep 20

press() { wtype -M ctrl -M alt -k s -m alt -m ctrl; }

press
check "hotkey (sway)" 1 5

"$BIN" --save
check "clipline --save" 2 5

# Aufnahme neu starten (passiert z. B. bei Bildpausen oder Einstellungsänderungen): danach muss weiter geclippt werden
for n in 3 4; do
    pkill -f gst-launch-1.0
    sleep 12
    "$BIN" --save
    check "clip after recording restart $((n - 2))" "$n" 5
done

# Sway lädt seine Einstellungen neu: die Taste muss danach weiter gehen
swaymsg reload > /dev/null
sleep 3
press
check "hotkey after sway reload" 5

echo "----- PipeWire: dieselbe Verbindung für einen zweiten GStreamer-Start?"
python3 "$DIR/pwreuse.py"
echo "----- portal (wlr) screencopy"
grep -i -E 'shm|format|buffer|dmabuf|gbm|screencopy|frame|error|fail' "$OUT/xdpw.log" | head -n 40
finish
exit $FAILS
