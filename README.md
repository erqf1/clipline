# Clipline

Save the last moments of your screen with one key. Clipline records quietly in the background, keeps the last minutes in memory and writes a clip only when you press the hotkey – like a replay buffer, but small, fast and free.

- **Runs in the background.** Starts with your computer, sits in the tray and uses the graphics card to record (NVIDIA, AMD, Intel, Apple). About 5 % of one CPU core at 1080p60 on a typical PC.
- **Clips from memory.** The last 10 s to 10 min are kept in RAM, nothing touches your disk until you save. The setup shows you the estimated RAM use before you start.
- **One key.** `Ctrl+Alt+S` (configurable) saves the clip as an MP4 in your folder, with system sound and optionally your microphone.
- **Clip gallery.** Open Clipline to see all clips with previews. With [Cutline](https://github.com/erqf1/cutline) installed you can open any clip straight in the editor – and Cutline shows your Clipline clips too.
- **Your way.** Clips folder anywhere, clip length slider with live RAM estimate, resolution from native down to 360p with a pixel preview, frame rate, quality, your own hotkey, accent colour, dark or light – all set on first start and changeable later.

## Download

Get the latest build from the [Releases page](../../releases/latest):

| Platform | File |
| --- | --- |
| Windows installer | `Clipline-windows-x64-setup.exe` |
| Windows portable | `Clipline-windows-x64.zip` |
| Linux (Debian / Ubuntu) | `clipline_amd64.deb` – `sudo apt install ./clipline_amd64.deb` |
| Linux (Arch) | `clipline-x86_64.pkg.tar.zst` – `sudo pacman -U clipline-x86_64.pkg.tar.zst` |
| Linux (any, tar.gz) | `clipline-linux-x86_64.tar.gz` – needs Qt 6 and ffmpeg |
| macOS Apple Silicon | `Clipline-macos-apple-silicon.dmg` |
| macOS Intel | `Clipline-macos-intel.dmg` |

### Platform notes

- **Windows:** full support – screen via Desktop Duplication, system sound via WASAPI loopback.
- **Linux:** X11 sessions. System sound via PulseAudio/PipeWire. On Wayland, screen capture and global hotkeys are limited; bind `clipline --save` to a key in your desktop's shortcut settings.
- **macOS:** allow *Screen Recording* in System Settings → Privacy on first start. macOS does not allow recording system sound without a virtual audio device (e.g. BlackHole, selectable as microphone). Builds are not notarized: right-click → Open the first time.

## Command line

| Command | Effect |
| --- | --- |
| `clipline` | Start, or show the gallery if already running |
| `clipline --background` | Start in the tray only (used by autostart) |
| `clipline --save` | Save a clip in the running instance |

## Build from source

Requires Qt 6.4+ (Widgets, Network), CMake 3.21+, a C++17 compiler and `ffmpeg` (next to the executable or on `PATH`). On Linux also libX11.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

On Windows, `build.bat` builds and creates the portable zip. Release packages for all platforms are built by GitHub Actions when a `v*` tag is pushed.

## License

MIT for the Clipline source code. Release packages bundle [FFmpeg](https://ffmpeg.org) (GPL build) and [Qt](https://www.qt.io) (LGPLv3), which keep their own licenses.
