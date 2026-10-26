// macOS: Bildschirme (avfoundation), Hotkey (Carbon RegisterEventHotKey), Autostart (LaunchAgent)
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QKeySequence>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include "i18n.h"
#include "platform.h"

#include <Carbon/Carbon.h>
#include <QVariant>
#include <utility>
#include <vector>

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
        // macOS nennt Bildschirme beim Namen ("Built-in Retina Display", "DELL U2720Q")
        m.name = QString("%1 · %2 · %3×%4").arg(i + 1).arg(s->name()).arg(m.rect.width()).arg(m.rect.height());
        if (m.primary) m.name += " · " + L("main screen");
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
    switch (key) {
    case Qt::Key_Space: return kVK_Space;
    case Qt::Key_Return: case Qt::Key_Enter: return kVK_Return;
    case Qt::Key_Backspace: return kVK_Delete;
    case Qt::Key_Tab: case Qt::Key_Backtab: return kVK_Tab;
    case Qt::Key_Escape: return kVK_Escape;
    case Qt::Key_Left: return kVK_LeftArrow;
    case Qt::Key_Right: return kVK_RightArrow;
    case Qt::Key_Up: return kVK_UpArrow;
    case Qt::Key_Down: return kVK_DownArrow;
    case Qt::Key_Delete: return kVK_ForwardDelete;
    case Qt::Key_Home: return kVK_Home;
    case Qt::Key_End: return kVK_End;
    case Qt::Key_PageUp: return kVK_PageUp;
    case Qt::Key_PageDown: return kVK_PageDown;
    default: return -1;
    }
}

// Jeder Hotkey bekommt alle Tastendrücke gemeldet und prüft über seine eigene ID, ob es seiner war
OSStatus hotkeyHandler(EventHandlerCallRef, EventRef event, void* userData);
}  // namespace

struct GlobalHotkey::Impl {
    std::vector<EventHotKeyRef> refs;
    EventHandlerRef handler = nullptr;
    UInt32 uid = 0;
};

namespace {
OSStatus hotkeyHandler(EventHandlerCallRef, EventRef event, void* userData) {
    auto* pair = static_cast<std::pair<GlobalHotkey*, UInt32>*>(userData);
    EventHotKeyID id{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(id), nullptr, &id);
    if (id.signature != 'CLPL' || id.id != pair->second) return eventNotHandledErr;
    GlobalHotkey* hk = pair->first;
    QMetaObject::invokeMethod(hk, [hk] { emit hk->activated(); }, Qt::QueuedConnection);
    return noErr;
}
}  // namespace

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {
    static UInt32 counter = 0;
    d->uid = ++counter;
    EventTypeSpec spec{kEventClassKeyboard, kEventHotKeyPressed};
    // Lebt so lange wie der Hotkey (wird im Destruktor mit dem Handler entfernt)
    auto* pair = new std::pair<GlobalHotkey*, UInt32>(this, d->uid);
    InstallApplicationEventHandler(&hotkeyHandler, 1, &spec, pair, &d->handler);
    setProperty("_clipline_pair", QVariant::fromValue(static_cast<void*>(pair)));
}

GlobalHotkey::~GlobalHotkey() {
    clear();
    if (d->handler) RemoveEventHandler(d->handler);
    delete static_cast<std::pair<GlobalHotkey*, UInt32>*>(property("_clipline_pair").value<void*>());
}

bool GlobalHotkey::supported() { return true; }

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty()) return false;
    const QKeyCombination kc = seq[0];
    const int code = keyCode(kc.key());
    if (code < 0) return false;
    auto macMods = [](Qt::KeyboardModifiers m) {
        UInt32 mods = 0;
        if (m & Qt::ControlModifier) mods |= cmdKey;   // Qt: Ctrl = Command auf dem Mac
        if (m & Qt::MetaModifier) mods |= controlKey;
        if (m & Qt::AltModifier) mods |= optionKey;
        if (m & Qt::ShiftModifier) mods |= shiftKey;
        return mods;
    };
    const EventHotKeyID id{'CLPL', d->uid};
    EventHotKeyRef ref = nullptr;
    if (RegisterEventHotKey(UInt32(code), macMods(kc.keyboardModifiers()), id, GetApplicationEventTarget(), 0, &ref) != noErr) return false;
    d->refs.push_back(ref);
    // Auch mit zusätzlich gedrückten Tasten (z. B. Shift) auslösen
    for (const Qt::KeyboardModifiers extra : tolerantModifiers(kc))
        if (RegisterEventHotKey(UInt32(code), macMods(extra), id, GetApplicationEventTarget(), 0, &ref) == noErr) d->refs.push_back(ref);
    return true;
}

void GlobalHotkey::clear() {
    for (EventHotKeyRef r : d->refs) UnregisterEventHotKey(r);
    d->refs.clear();
}
