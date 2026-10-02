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
# Wayland (GNOME, KDE Plasma 6, Hyprland, ...): screen capture goes through the desktop portal + PipeWire
if [ -n "$WAYLAND_DISPLAY" ] && ! gst-inspect-1.0 --exists pipewiresrc 2>/dev/null; then
  echo "Note: for Wayland please install GStreamer with the PipeWire plugin"
  echo "      (Arch: sudo pacman -S gst-plugin-pipewire gst-plugins-base, Ubuntu: sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools)."
fi
echo "Clipline installed. Run: clipline"
