// Plattformunabhängige Teile von platform.h
#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QDesktopServices>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include <QGuiApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <cmath>
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

// ---------------------------------------------------------------- Hotkey-Regeln (für alle Systeme gleich)
static QStringList g_reserved;
void setReservedHotkeys(const QStringList& exact) {
    g_reserved.clear();
    for (const QString& s : exact)
        if (!s.isEmpty()) g_reserved << QKeySequence(s).toString(QKeySequence::PortableText);
}

QList<Qt::KeyboardModifiers> tolerantModifiers(const QKeyCombination& kc) {
    QList<Qt::KeyboardModifiers> out;
    const Qt::KeyboardModifiers base = kc.keyboardModifiers();
    const Qt::KeyboardModifier extras[3] = {Qt::ShiftModifier, Qt::ControlModifier, Qt::AltModifier};
    for (int mask = 1; mask < 8; ++mask) {
        Qt::KeyboardModifiers m = base;
        for (int b = 0; b < 3; ++b)
            if (mask & (1 << b)) m |= extras[b];
        if (m == base || out.contains(m)) continue;
        const QString seq = QKeySequence(QKeyCombination(m, kc.key())).toString(QKeySequence::PortableText);
        if (!g_reserved.contains(seq)) out << m;
    }
    return out;
}

bool keyAllowedAlone(int key, bool keypad) {
    if (keypad) return true;  // Ziffernblock
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35) return true;
    switch (key) {
    case Qt::Key_Print: case Qt::Key_Pause: case Qt::Key_ScrollLock: case Qt::Key_CapsLock: case Qt::Key_NumLock:
    case Qt::Key_Insert: case Qt::Key_Menu: case Qt::Key_MediaPlay: case Qt::Key_MediaPause: case Qt::Key_MediaTogglePlayPause:
    case Qt::Key_MediaNext: case Qt::Key_MediaPrevious: case Qt::Key_MediaStop: case Qt::Key_MediaRecord:
        return true;
    default:
        return false;
    }
}

QString appLogPath() {
    static const QString path = [] {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (dir.isEmpty()) dir = QDir::tempPath();
        QDir().mkpath(dir);
        return QDir(dir).filePath("clipline.log");
    }();
    return path;
}

void appLog(const QString& line) {
    const QString path = appLogPath();
    if (QFileInfo(path).size() > 512 * 1024) {
        QFile::remove(path + ".old");
        QFile::rename(path, path + ".old");
    }
    QFile f(path);
    if (f.open(QIODevice::Append | QIODevice::Text))
        f.write((QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ") + line + '\n').toUtf8());
}

// Bildschirmliste kurz zwischenspeichern: findMonitor wird beim Einstellen oft hintereinander gebraucht
static QList<Monitor> cachedMonitors() {
    static QList<Monitor> list;
    static QElapsedTimer age;
    if (!age.isValid() || age.elapsed() > 2000 || list.isEmpty()) {
        list = listMonitors();
        age.start();
    }
    return list;
}

Monitor findMonitor(const QString& id) {
    const QList<Monitor> all = cachedMonitors();
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
bool SystemAudioPipe::prepare(bool, const QString&, const MicTuning&) { return false; }
void SystemAudioPipe::setTuning(const MicTuning&) {}
QString SystemAudioPipe::pipePath() const { return QString(); }
void SystemAudioPipe::start() {}
void SystemAudioPipe::stop() {}

struct MicLevelMeter::Impl {};
MicLevelMeter::MicLevelMeter() : d(new Impl) {}
MicLevelMeter::~MicLevelMeter() = default;
bool MicLevelMeter::start(const QString&) { return false; }
void MicLevelMeter::setDenoise(bool) {}
void MicLevelMeter::stop() {}
float MicLevelMeter::levelDb() const { return -90.f; }
bool micMeterSupported() { return false; }
bool processMicFile(const QString&, const QString&, const MicTuning&) { return false; }
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

// Mikrofon testen (macOS/Linux): ffmpeg nimmt auf, mit denselben Filtern wie bei der Aufnahme (recorder.cpp)
bool recordMicTest(const QString& mic, double seconds, const MicTuning& t, const QString& wavOut, QString* error) {
    QStringList af;
    if (t.denoise) af << "afftdn=nf=-25:tn=1";
    if (t.gateDb > -80)
        af << QString("agate=threshold=%1:ratio=20:attack=5:release=120:range=0.001").arg(std::pow(10.0, t.gateDb / 20.0), 0, 'f', 6);
    af << QString("volume=%1").arg(t.micVolume / 100.0, 0, 'f', 2);
    QStringList args = {"-hide_banner", "-loglevel", "error", "-y"};
#ifdef Q_OS_MAC
    const int idx = listMicrophones().indexOf(mic);
    if (idx < 0) {
        if (error) *error = "microphone not found";
        return false;
    }
    args << "-f" << "avfoundation" << "-i" << QString(":%1").arg(idx);
#else
    args << "-f" << "pulse" << "-i" << mic;
#endif
    args << "-t" << QString::number(seconds, 'f', 1) << "-af" << af.join(',') << "-ac" << "2" << "-ar" << "48000" << wavOut;
    QProcess p;
    p.start(ffmpegPath(), args);
    const bool ok = p.waitForFinished(int(seconds * 1000) + 15000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    if (!ok && error) *error = QString::fromUtf8(p.readAllStandardError()).trimmed().right(300);
    return ok && QFileInfo(wavOut).size() > 1000;
}

void playWavFile(const QString& path) {
#ifdef Q_OS_MAC
    QProcess::startDetached("afplay", {path});
#else
    for (const char* player : {"pw-play", "paplay", "aplay"}) {
        const QString exe = QStandardPaths::findExecutable(player);
        if (!exe.isEmpty() && QProcess::startDetached(exe, {path})) return;
    }
#endif
}

bool systemAudioSupported() {
#ifdef Q_OS_MAC
    return false;  // macOS erlaubt keinen Systemton-Mitschnitt ohne virtuelles Audiogerät
#else
    return true;   // PulseAudio/PipeWire: @DEFAULT_MONITOR@
#endif
}

#ifdef Q_OS_MAC
void playSaveSound() { QApplication::beep(); }
void playErrorSound() {
    QApplication::beep();
    QTimer::singleShot(220, [] { QApplication::beep(); });
}
#endif
#endif

#ifndef Q_OS_LINUX
bool desktopNotify(const QString&, const QString&, int, bool) { return false; }  // Windows/macOS: Tray-Hinweise
#endif
