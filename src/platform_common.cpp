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

// Cutline schreibt beim Start seinen Pfad nach QSettings("WSoftware", "VideoEditor")/exePath
QString findCutline() {
    const QString p = QSettings("WSoftware", "VideoEditor").value("exePath").toString();
    if (!p.isEmpty() && QFileInfo::exists(p)) return p;
#ifdef Q_OS_WIN
    for (const char* root : {"HKEY_CURRENT_USER", "HKEY_LOCAL_MACHINE"}) {
        QSettings reg(QString("%1\\Software\\Classes\\Applications\\Cutline.exe\\shell\\open\\command").arg(root),
                      QSettings::NativeFormat);
        QString cmd = reg.value("Default").toString();
        auto m = QRegularExpression("^\"([^\"]+)\"").match(cmd);
        if (m.hasMatch() && QFileInfo::exists(m.captured(1))) return m.captured(1);
    }
#elif defined(Q_OS_MAC)
    for (const QString& c : {QString("/Applications/Cutline.app/Contents/MacOS/Cutline"),
                             QDir::homePath() + "/Applications/Cutline.app/Contents/MacOS/Cutline"})
        if (QFileInfo::exists(c)) return c;
#else
    const QString c = QStandardPaths::findExecutable("cutline");
    if (!c.isEmpty()) return c;
#endif
    return QString();
}

bool openInCutline(const QString& file) {
    const QString exe = findCutline();
    if (exe.isEmpty()) return false;
    return QProcess::startDetached(exe, {QDir::toNativeSeparators(file)});
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
