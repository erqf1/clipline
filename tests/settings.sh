#!/bin/bash
# Einstellungen-Stresstest unter X11 (Xvfb) mit zwei Bildschirmen: Clipline läuft mit echter Aufnahme, öffnet die
# Einstellungen immer wieder, ändert zufällig alles Mögliche und speichert oder bricht ab (clipline --settingstest).
# Aufruf: settings.sh <clipline-programm> <arbeitsordner>
set -u
BIN="$(realpath "$1")"
OUT="$2"
mkdir -p "$OUT"
export HOME="$OUT/home" XDG_RUNTIME_DIR="$OUT/xdg" XDG_SESSION_TYPE=x11 DISPLAY=:98
mkdir -p "$HOME/.config/WSoftware" && mkdir -p -m 700 "$XDG_RUNTIME_DIR"
Xvfb :98 -screen 0 1920x1080x24 > "$OUT/xvfb.log" 2>&1 &
sleep 2
cat > "$OUT/screens.json" <<'JSON'
{"screens": [
  {"name": "DP-1", "x": 0, "y": 0, "width": 1280, "height": 720, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1},
  {"name": "HDMI-1", "x": 1280, "y": 0, "width": 640, "height": 360, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}
]}
JSON
FAILS=0
for seed in 1 2; do
  rm -rf "$HOME/.config/WSoftware/Clipline.conf" "$OUT/run$seed"
  printf '[General]\nfirstRunDone=true\nlanguage=en\nclipsDir=%s\nautostart=false\nsound=false\n' "$OUT/clips" > "$HOME/.config/WSoftware/Clipline.conf"
  QT_QPA_PLATFORM="offscreen:configfile=$OUT/screens.json" ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
    timeout 600 "$BIN" --settingstest "$OUT/run$seed" 20 "$seed" > "$OUT/run$seed.out" 2>&1
  rc=$?
  if [ $rc -ne 0 ] || ! grep -q "settingstest finished" "$OUT/run$seed/settingstest.txt" || grep -q GONE "$OUT/run$seed/settingstest.txt"; then
    echo "FAIL settings stress test, seed $seed (exit $rc)"; FAILS=$((FAILS + 1))
    tail -n 8 "$OUT/run$seed/settingstest.txt"; grep -A25 -E "ERROR: AddressSanitizer|runtime error" "$OUT/run$seed.out" | head -40
  else
    echo "PASS settings stress test, seed $seed ($(tail -1 "$OUT/run$seed/settingstest.txt"))"
  fi
done
exit $FAILS
