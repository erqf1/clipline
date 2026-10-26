#include "config.h"
#include "platform.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

QString defaultClipsDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    if (base.isEmpty()) base = QDir::homePath();
    return QDir(base).filePath("Clipline");
}

Config Config::load() {
    QSettings s("WSoftware", "Clipline");
    Config c;
    c.clipsDir = s.value("clipsDir", defaultClipsDir()).toString();
    c.clipSeconds = s.value("clipSeconds", c.clipSeconds).toInt();
    c.monitor = s.value("monitor", c.monitor).toString();
    c.height = s.value("height", c.height).toInt();
    c.fps = s.value("fps", c.fps).toInt();
    c.quality = s.value("quality", c.quality).toInt();
    c.systemAudio = s.value("systemAudio", c.systemAudio).toBool();
    c.mic = s.value("mic", c.mic).toString();
    c.micGate = std::clamp(s.value("micGate", c.micGate).toInt(), -80, -10);
    c.micVolume = std::clamp(s.value("micVolume", c.micVolume).toInt(), 0, 200);
    c.systemVolume = std::clamp(s.value("systemVolume", c.systemVolume).toInt(), 0, 200);
    c.micDenoise = s.value("micDenoise", c.micDenoise).toBool();
    c.hotkey = s.value("hotkey", c.hotkey).toString();
    c.muteHotkey = s.value("muteHotkey", c.muteHotkey).toString();
    c.accent = s.value("accent", c.accent).toString();
    c.dark = s.value("dark", c.dark).toBool();
    c.minimal = s.value("minimal", c.minimal).toBool();
    c.autostart = s.value("autostart", c.autostart).toBool();
    c.sound = s.value("sound", c.sound).toBool();
    c.language = s.value("language", c.language).toString();
    c.waylandToken = s.value("waylandToken").toString();
    c.firstRunDone = s.value("firstRunDone", false).toBool();
    const int n = s.beginReadArray("buttons");
    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        ClipButton b;
        b.hotkey = s.value("hotkey").toString();
        b.seconds = std::clamp(s.value("seconds", b.seconds).toInt(), 5, 1200);
        b.height = s.value("height", b.height).toInt();
        b.fps = s.value("fps", b.fps).toInt();
        b.quality = std::clamp(s.value("quality", b.quality).toInt(), 0, 2);
        b.monitor = s.value("monitor").toString();
        c.extraButtons << b;
    }
    s.endArray();
    return c;
}

ClipButton Config::mainButton() const { return {hotkey, clipSeconds, height, fps, quality, monitor}; }

QList<ClipButton> Config::allButtons() const { return QList<ClipButton>{mainButton()} + extraButtons; }

Config Config::withButton(const ClipButton& b) const {
    Config c = *this;
    c.clipSeconds = b.seconds;
    c.height = b.height;
    c.fps = b.fps;
    c.quality = b.quality;
    c.monitor = b.monitor;
    return c;
}

QString Config::buttonMonitor(int i) const {
    const QList<ClipButton> all = allButtons();
    return i >= 0 && i < all.size() ? findMonitor(all[i].monitor).id : findMonitor(monitor).id;
}

QStringList Config::recordingMonitors() const {
    QStringList out;
    for (int i = 0; i < allButtons().size(); ++i) {
        const QString id = buttonMonitor(i);
        if (!out.contains(id)) out << id;
    }
    return out;
}

Config Config::recordingConfig(const QString& mon) const {
    Config c = *this;
    bool first = true;
    for (int i = 0; i < allButtons().size(); ++i) {
        if (buttonMonitor(i) != mon) continue;
        const ClipButton b = allButtons()[i];
        if (first) {
            c.clipSeconds = b.seconds;
            c.height = b.height;
            c.fps = b.fps;
            c.quality = b.quality;
            first = false;
            continue;
        }
        c.clipSeconds = std::max(c.clipSeconds, b.seconds);
        // 0 = native ist das Höchste
        if (c.height != 0) c.height = b.height == 0 ? 0 : std::max(c.height, b.height);
        c.fps = std::max(c.fps, b.fps);
        c.quality = std::max(c.quality, b.quality);
    }
    c.monitor = mon;
    return c;
}

void Config::save() const {
    QSettings s("WSoftware", "Clipline");
    s.setValue("clipsDir", clipsDir);
    s.setValue("clipSeconds", clipSeconds);
    s.setValue("monitor", monitor);
    s.setValue("height", height);
    s.setValue("fps", fps);
    s.setValue("quality", quality);
    s.setValue("systemAudio", systemAudio);
    s.setValue("mic", mic);
    s.setValue("micGate", micGate);
    s.setValue("micVolume", micVolume);
    s.setValue("systemVolume", systemVolume);
    s.setValue("micDenoise", micDenoise);
    s.setValue("hotkey", hotkey);
    s.setValue("muteHotkey", muteHotkey);
    s.setValue("accent", accent);
    s.setValue("dark", dark);
    s.setValue("minimal", minimal);
    s.setValue("autostart", autostart);
    s.setValue("sound", sound);
    s.setValue("language", language);
    s.setValue("waylandToken", waylandToken);
    s.setValue("firstRunDone", firstRunDone);
    s.remove("buttons");
    s.beginWriteArray("buttons", int(extraButtons.size()));
    for (int i = 0; i < extraButtons.size(); ++i) {
        s.setArrayIndex(i);
        const ClipButton& b = extraButtons[i];
        s.setValue("hotkey", b.hotkey);
        s.setValue("seconds", b.seconds);
        s.setValue("height", b.height);
        s.setValue("fps", b.fps);
        s.setValue("quality", b.quality);
        s.setValue("monitor", b.monitor);
    }
    s.endArray();
}

QSize captureSize(const Config& c, QSize native) {
    if (native.isEmpty()) native = QSize(1920, 1080);
    if (c.height <= 0 || c.height >= native.height()) return QSize(native.width() / 2 * 2, native.height() / 2 * 2);
    int w = int(std::lround(double(native.width()) * c.height / native.height() / 2.0)) * 2;
    return QSize(w, c.height / 2 * 2);
}

// Bits pro Pixel und Bild: genug für scharfe Spielszenen mit H.264, ohne Speicher zu verschwenden.
// 1080p60: ~10 / ~19 / ~31 Mbit/s. Vorher 0,045/0,07/0,11 (1080p60 "ausgewogen" nur 8,7 Mbit/s) - bei schnellen
// Spielszenen sichtbar blockig (Messung: SSIM 0,947 statt 0,975).
double videoBitrate(const Config& c, QSize native) {
    static const double bpp[3] = {0.08, 0.15, 0.25};
    const QSize s = captureSize(c, native);
    const double b = double(s.width()) * s.height() * c.fps * bpp[std::clamp(c.quality, 0, 2)];
    return std::clamp(b, 3e6, 100e6);
}

double estimateBufferMB(const Config& c, QSize native) {
    const double audio = (c.systemAudio || !c.mic.isEmpty()) ? 160e3 : 0;
    // Puffer hält Cliplänge + einige Sekunden Reserve (für den Schnitt am Keyframe)
    return (videoBitrate(c, native) * 1.1 + audio) / 8.0 * (c.clipSeconds + 5) / 1048576.0;
}

double estimateTotalMB(const Config& c, QSize native) {
    // Grundbedarf: Programm im Hintergrund (~30 MB) + ffmpeg mit Hardware-Encoder (~90–130 MB)
    const QSize s = captureSize(c, native);
    const double encoder = 80 + double(s.width()) * s.height() * 4 * 6 / 1048576.0;  // einige Frames im Encoder
    return estimateBufferMB(c, native) + 30 + encoder;
}

// Gespeicherter Clip = genau die Cliplänge aus dem Puffer, ohne Neukodierung (plus etwas MP4-Verwaltung)
double estimateClipMB(const Config& c, QSize native) {
    const double audio = (c.systemAudio || !c.mic.isEmpty()) ? 160e3 : 0;
    return (videoBitrate(c, native) + audio) / 8.0 * c.clipSeconds * 1.01 / 1048576.0;
}

QString fmtDuration(int s) {
    if (s < 60) return QString("%1 s").arg(s);
    if (s % 60 == 0) return QString("%1 min").arg(s / 60);
    return QString("%1:%2 min").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

QString fmtMB(double mb) {
    if (mb >= 1024) return QString("%1 GB").arg(mb / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(int(std::lround(mb)));
}
