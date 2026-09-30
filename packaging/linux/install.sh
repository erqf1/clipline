#!/bin/sh
# Installs Clipline for the current user (~/.local). Needs Qt 6 and ffmpeg from your distribution.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${HOME}/.local"
mkdir -p "$PREFIX/opt/clipline" "$PREFIX/bin" "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor/256x256/apps"
cp "$DIR/clipline" "$PREFIX/opt/clipline/"
ln -sf "$PREFIX/opt/clipline/clipline" "$PREFIX/bin/clipline"
cp "$DIR/icon.png" "$PREFIX/share/icons/hicolor/256x256/apps/clipline.png"
sed "s|^Exec=.*|Exec=$PREFIX/opt/clipline/clipline|" "$DIR/clipline.desktop" > "$PREFIX/share/applications/clipline.desktop"
command -v ffmpeg >/dev/null 2>&1 || echo "Note: please install ffmpeg (e.g. sudo apt install ffmpeg)."
echo "Clipline installed. Run: clipline"
