// macOS: Bildschirme (avfoundation), Hotkey (Carbon RegisterEventHotKey), Autostart (LaunchAgent)
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QKeySequence>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include "platform.h"

#include <Carbon/Carbon.h>

QList<Monitor> listMonitors() {
    // avfoundation nummeriert Bildschirme als "Capture screen N" nach den Kameras
    QProcess p;
    p.start(ffmpegPath(), {"-hide_banner", "-f", "avfoundation", "-list_devices", "true", "-i", ""});
    p.waitForFinished(8000);
    QList<int> screenIdx;
    for (const QString& line : QString::fromUtf8(p.readAllStandardError()).split('\n')) {
        auto m = QRegularExpression(R"(\[(\d+)\]\s+Capture screen (\d+))").match(line);
        if (m.hasMatch()) screenIdx << m.captured(1).toInt();
    }
    QList<Monitor> out;
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        QScreen* s = screens[i];
        Monitor m;
        m.id = s->name().isEmpty() ? QString("screen%1").arg(i) : s->name();
        const qreal dpr = s->devicePixelRatio();
        m.rect = QRect(s->geometry().topLeft() * dpr, s->geometry().size() * dpr);
        m.primary = s == QGuiApplication::primaryScreen();
        m.refresh = s->refreshRate();
        m.avIndex = i < screenIdx.size() ? screenIdx[i] : -1;
        m.name = QString("%1 %2 · %3×%4").arg(i + 1).arg(m.primary ? "★" : "").arg(m.rect.width()).arg(m.rect.height());
        if (m.avIndex >= 0) out << m;
    }
    return out;
}

// ---------------------------------------------------------------- Autostart (LaunchAgent)
bool setAutostart(bool on) {
    const QString path = QDir::homePath() + "/Library/LaunchAgents/io.github.clipline.plist";
    if (!on) return QFile::remove(path) || !QFile::exists(path);
    QDir().mkpath(QDir::homePath() + "/Library/LaunchAgents");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QString exe = QCoreApplication::applicationFilePath();
    f.write(QString(R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key><string>io.github.clipline</string>
    <key>ProgramArguments</key><array><string>%1</string><string>--background</string></array>
    <key>RunAtLoad</key><true/>
    <key>ProcessType</key><string>Interactive</string>
</dict>
</plist>
)").arg(exe.toHtmlEscaped()).toUtf8());
    return true;
}

// ---------------------------------------------------------------- Globaler Hotkey (Carbon)
namespace {
int keyCode(int key) {
    static const int letters[26] = {kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
                                    kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N,
                                    kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
                                    kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z};
    static const int digits[10] = {kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
                                   kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};
    static const int fkeys[20] = {kVK_F1,  kVK_F2,  kVK_F3,  kVK_F4,  kVK_F5,  kVK_F6,  kVK_F7,
                                  kVK_F8,  kVK_F9,  kVK_F10, kVK_F11, kVK_F12, kVK_F13, kVK_F14,
                                  kVK_F15, kVK_F16, kVK_F17, kVK_F18, kVK_F19, kVK_F20};
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return letters[key - Qt::Key_A];
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return digits[key - Qt::Key_0];
    if (key >= Qt::Key_F1 && key <= Qt::Key_F20) return fkeys[key - Qt::Key_F1];
    if (key == Qt::Key_Space) return kVK_Space;
    return -1;
}

OSStatus hotkeyHandler(EventHandlerCallRef, EventRef, void* userData) {
    GlobalHotkey* hk = static_cast<GlobalHotkey*>(userData);
    QMetaObject::invokeMethod(hk, [hk] { emit hk->activated(); }, Qt::QueuedConnection);
    return noErr;
}
}  // namespace

struct GlobalHotkey::Impl {
    EventHotKeyRef ref = nullptr;
    EventHandlerRef handler = nullptr;
};

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {
    EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&hotkeyHandler, 1, &spec, this, &d->handler);
}

GlobalHotkey::~GlobalHotkey() {
    clear();
    if (d->handler) RemoveEventHandler(d->handler);
}

bool GlobalHotkey::supported() { return true; }

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty()) return false;
    const QKeyCombination kc = seq[0];
    const int code = keyCode(kc.key());
    if (code < 0) return false;
    UInt32 mods = 0;
    const Qt::KeyboardModifiers m = kc.keyboardModifiers();
    if (m & Qt::ControlModifier) mods |= cmdKey;   // Qt: Ctrl = Command auf dem Mac
    if (m & Qt::MetaModifier) mods |= controlKey;
    if (m & Qt::AltModifier) mods |= optionKey;
    if (m & Qt::ShiftModifier) mods |= shiftKey;
    EventHotKeyID id{'CLPL', 1};
    return RegisterEventHotKey(UInt32(code), mods, id, GetApplicationEventTarget(), 0, &d->ref) == noErr;
}

void GlobalHotkey::clear() {
    if (d->ref) UnregisterEventHotKey(d->ref);
    d->ref = nullptr;
}
