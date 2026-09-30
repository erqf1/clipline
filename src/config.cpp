#include "config.h"

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
    c.hotkey = s.value("hotkey", c.hotkey).toString();
    c.accent = s.value("accent", c.accent).toString();
    c.dark = s.value("dark", c.dark).toBool();
    c.autostart = s.value("autostart", c.autostart).toBool();
    c.sound = s.value("sound", c.sound).toBool();
    c.language = s.value("language", c.language).toString();
    c.firstRunDone = s.value("firstRunDone", false).toBool();
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
    s.setValue("hotkey", hotkey);
    s.setValue("accent", accent);
    s.setValue("dark", dark);
    s.setValue("autostart", autostart);
    s.setValue("sound", sound);
    s.setValue("language", language);
    s.setValue("firstRunDone", firstRunDone);
}

QSize captureSize(const Config& c, QSize native) {
    if (native.isEmpty()) native = QSize(1920, 1080);
    if (c.height <= 0 || c.height >= native.height()) return QSize(native.width() / 2 * 2, native.height() / 2 * 2);
    int w = int(std::lround(double(native.width()) * c.height / native.height() / 2.0)) * 2;
    return QSize(w, c.height / 2 * 2);
}

// Bits pro Pixel und Bild: genug für scharfe Spielszenen mit H.264, ohne Speicher zu verschwenden
double videoBitrate(const Config& c, QSize native) {
    static const double bpp[3] = {0.045, 0.07, 0.11};
    const QSize s = captureSize(c, native);
    const double b = double(s.width()) * s.height() * c.fps * bpp[std::clamp(c.quality, 0, 2)];
    return std::clamp(b, 1.5e6, 80e6);
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

QString fmtDuration(int s) {
    if (s < 60) return QString("%1 s").arg(s);
    if (s % 60 == 0) return QString("%1 min").arg(s / 60);
    return QString("%1:%2 min").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

QString fmtMB(double mb) {
    if (mb >= 1024) return QString("%1 GB").arg(mb / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(int(std::lround(mb)));
}
