#!/bin/sh
# Installs Clipline for the current user (~/.local). Needs Qt 6 and ffmpeg from your distribution.
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${HOME}/.local"
mkdir -p "$PREFIX/opt/clipline" "$PREFIX/bin" "$PREFIX/share/applications" "$PREFIX/share/icons/hicolor/256x256/apps"
cp "$DIR/clipline" "$PREFIX/opt/clipline/"
ln -sf "$PREFIX/opt/clipline/clipline" "$PREFIX/bin/clipline"
cp "$DIR/icon.png" "$PREFIX/share/icons/hicolor/256x256/apps/clipline.png"
# The desktop file is named like the app ID (desktop portals recognise Clipline by it); remove the old one
sed "s|^Exec=.*|Exec=$PREFIX/opt/clipline/clipline|" "$DIR/win.mert.Clipline.desktop" > "$PREFIX/share/applications/win.mert.Clipline.desktop"
rm -f "$PREFIX/share/applications/clipline.desktop"
command -v ffmpeg >/dev/null 2>&1 || echo "Note: please install ffmpeg (e.g. sudo apt install ffmpeg)."
# Wayland (GNOME, KDE Plasma 6, Hyprland, ...): screen capture goes through the desktop portal + PipeWire
if [ -n "$WAYLAND_DISPLAY" ] && ! gst-inspect-1.0 --exists pipewiresrc 2>/dev/null; then
  echo "Note: for Wayland please install GStreamer with the PipeWire plugin"
  echo "      (Arch: sudo pacman -S gst-plugin-pipewire gst-plugins-base, Ubuntu: sudo apt install gstreamer1.0-pipewire gstreamer1.0-tools)."
fi
# Wayland: without the Qt Wayland plugin Clipline opens through XWayland; without XWayland as well it can't open at all
if [ -n "$WAYLAND_DISPLAY" ]; then
  QT_WAYLAND=""
  for f in /usr/lib/qt6/plugins/platforms/libqwayland* /usr/lib/*/qt6/plugins/platforms/libqwayland* /usr/lib64/qt6/plugins/platforms/libqwayland*; do
    if [ -e "$f" ]; then QT_WAYLAND=1; fi
  done
  if [ -z "$QT_WAYLAND" ]; then
    if [ -n "$DISPLAY" ]; then
      echo "Note: Clipline will run through XWayland. For a native Wayland window install the Qt Wayland plugin"
    else
      echo "Warning: Clipline can't open its window here - please install the Qt Wayland plugin (no XWayland found)"
    fi
    echo "      (Arch: sudo pacman -S qt6-wayland, Ubuntu/Debian: sudo apt install qt6-wayland, Fedora: sudo dnf install qt6-qtwayland)."
  fi
fi
echo "Clipline installed. Run: clipline"
