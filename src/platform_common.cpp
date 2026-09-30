// Plattformunabhängige Teile von platform.h
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include "platform.h"

QString ffmpegPath() {
#ifdef Q_OS_WIN
    const QString name = "ffmpeg.exe";
#else
    const QString name = "ffmpeg";
#endif
#ifdef Q_OS_LINUX
    // Linux: System-ffmpeg bevorzugen (hat x11grab und PulseAudio)
    const QString sys = QStandardPaths::findExecutable("ffmpeg");
    if (!sys.isEmpty()) return sys;
#endif
    const QString dir = QCoreApplication::applicationDirPath();
    for (const QString& c : {dir + "/" + name, dir + "/../Resources/" + name, dir + "/../lib/clipline/" + name})
        if (QFileInfo::exists(c)) return QFileInfo(c).absoluteFilePath();
#ifdef Q_OS_MAC
    for (const char* c : {"/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg"})
        if (QFileInfo::exists(c)) return c;
#endif
    const QString p = QStandardPaths::findExecutable("ffmpeg");
    return p.isEmpty() ? name : p;
}

Monitor findMonitor(const QString& id) {
    const QList<Monitor> all = listMonitors();
    for (const Monitor& m : all)
        if (m.id == id) return m;
    for (const Monitor& m : all)
        if (m.primary) return m;
    return all.isEmpty() ? Monitor{} : all.first();
}

void revealInFolder(const QString& path) {
#ifdef Q_OS_WIN
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", {"-R", path});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

#ifndef Q_OS_WIN
// ---------------------------------------------------------------- Nicht-Windows: kein Loopback per Pipe
struct SystemAudioPipe::Impl {};
SystemAudioPipe::SystemAudioPipe() : d(new Impl) {}
SystemAudioPipe::~SystemAudioPipe() = default;
bool SystemAudioPipe::prepare(QString*, int*, int*) { return false; }
QString SystemAudioPipe::pipePath() const { return QString(); }
void SystemAudioPipe::start() {}
void SystemAudioPipe::stop() {}
#endif

#ifndef Q_OS_WIN
// ffmpeg-Geräteliste für avfoundation (macOS) bzw. pactl (Linux)
QStringList listMicrophones() {
    QStringList out;
#ifdef Q_OS_MAC
    QProcess p;
    p.start(ffmpegPath(), {"-hide_banner", "-f", "avfoundation", "-list_devices", "true", "-i", ""});
    p.waitForFinished(8000);
    const QString e = QString::fromUtf8(p.readAllStandardError());
    bool audio = false;
    for (const QString& line : e.split('\n')) {
        if (line.contains("audio devices")) { audio = true; continue; }
        if (line.contains("video devices")) { audio = false; continue; }
        auto m = QRegularExpression(R"(\[(\d+)\]\s+(.+)$)").match(line.trimmed());
        if (audio && m.hasMatch()) out << m.captured(2).trimmed();
    }
#else
    QProcess p;
    p.start("pactl", {"list", "short", "sources"});
    p.waitForFinished(4000);
    for (const QString& line : QString::fromUtf8(p.readAllStandardOutput()).split('\n')) {
        const QStringList f = line.split('\t');
        if (f.size() > 1 && !f[1].endsWith(".monitor")) out << f[1];
    }
#endif
    return out;
}

bool systemAudioSupported() {
#ifdef Q_OS_MAC
    return false;  // macOS erlaubt keinen Systemton-Mitschnitt ohne virtuelles Audiogerät
#else
    return true;   // PulseAudio/PipeWire: @DEFAULT_MONITOR@
#endif
}

void playSaveSound() { QApplication::beep(); }
#endif
