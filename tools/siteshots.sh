#!/bin/sh
# Website-Bilder für mert.win/clip: rendert alle Fenster unsichtbar (offscreen, zwei Bildschirme) und
# speichert sie als WebP. Braucht ffmpeg und cwebp (Paket "webp").
#   tools/siteshots.sh [build/clipline] [website-updates/img]      Sprache: CLIPLINE_LANG=de (Standard) oder en
set -eu
BIN="${1:-build/clipline}"
OUT="${2:-website-updates/img}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -m 700 "$WORK/run"
cat > "$WORK/screens.json" <<'JSON'
{"screens": [
  {"name": "DP-1", "x": 0, "y": 0, "width": 2560, "height": 1440, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1},
  {"name": "HDMI-1", "x": 2560, "y": 0, "width": 1920, "height": 1080, "logicalDpi": 96, "logicalBaseDpi": 96, "dpr": 1}
]}
JSON
# Eigenes HOME: keine echten Einstellungen oder Vorschaubilder des Benutzers anfassen
HOME="$WORK/home" XDG_RUNTIME_DIR="$WORK/run" QT_QPA_PLATFORM="offscreen:configfile=$WORK/screens.json" \
  "$BIN" --siteshots "$WORK/png"
mkdir -p "$OUT"
for f in "$WORK"/png/*.png; do
  cwebp -quiet -q 86 -m 6 "$f" -o "$OUT/$(basename "${f%.png}").webp"
done
grep -c '^saved' "$WORK/png/siteshots.txt" | sed 's/$/ Bilder/'
if grep -q FAILED "$WORK/png/siteshots.txt"; then grep FAILED "$WORK/png/siteshots.txt"; exit 1; fi
