#!/bin/bash
# Linux/Wayland (Sway ohne Bildschirm): Aufnahme über das Desktop-Portal + PipeWire, Taste über swaymsg.
# Prüft den Weg der Taste, auch nach "swaymsg reload". (Die Aufnahme selbst geht ohne Grafik hier nicht.)
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
LOG="$HOME/.local/share/WSoftware/Clipline/clipline.log"
# Bildschirmaufnahme geht in dieser Umgebung nicht (Sway ohne Grafik), geprüft wird der Weg der Taste:
# Compositor -> "clipline --hotkey" -> laufendes Clipline -> Clip speichern
saves() { grep -c "save: clip button 1" "$LOG" 2>/dev/null; }
expect_save() {
    local before=$1; sleep 3
    if [ "$(saves)" -gt "$before" ]; then echo "PASS $2"; else echo "FAIL $2"; FAILS=$((FAILS + 1)); fi
}

n=$(saves); press; expect_save "$n" "hotkey (sway)"
n=$(saves); "$BIN" --save; expect_save "$n" "clipline --save"
swaymsg reload > /dev/null
sleep 3
n=$(saves); press; expect_save "$n" "hotkey after sway reload"
n=$(saves); press; expect_save "$n" "hotkey pressed again"

# Clip-Taste ändern (Strg+Alt+S -> Strg+Alt+D) und Clipline neu starten: die neue Taste geht, die alte nicht mehr
pkill -f -- "$BIN --background"
sleep 1
sed -i 's/^hotkey=.*/hotkey=Ctrl+Alt+D/' "$HOME/.config/WSoftware/Clipline.conf"
"$BIN" --background >> "$OUT/stdout.txt" 2>&1 &
sleep 8
n=$(saves); wtype -M ctrl -M alt -k d -m alt -m ctrl; expect_save "$n" "changed hotkey (Ctrl+Alt+D) works"
n=$(saves); press; sleep 1
if [ "$(saves)" -gt "$n" ]; then echo "FAIL old hotkey still saves a clip"; FAILS=$((FAILS + 1)); else echo "PASS old hotkey does nothing any more"; fi

echo "----- portal (wlr) screencopy"
grep -i -E 'shm|format|buffer|dmabuf|gbm|screencopy|frame|error|fail' "$OUT/xdpw.log" | head -n 40
finish
exit $FAILS
