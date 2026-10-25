// Linux: Bildschirme (Qt/X11), Hotkey (XGrabKey in eigenem Thread, unter Wayland über wayland_hotkeys),
// Autostart (~/.config/autostart), Hinweise (Benachrichtigungsdienst) und Töne
#include <QApplication>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <QProcess>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QKeySequence>
#include <QScreen>
#include <QStandardPaths>
#include <atomic>
#include <thread>
#include "platform.h"
#include "wayland_hotkeys.h"

QList<Monitor> listMonitors() {
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
        m.name = QString("%1 %2 · %3×%4 · %5 Hz").arg(i + 1).arg(m.primary ? "★" : "").arg(m.rect.width())
                     .arg(m.rect.height()).arg(int(m.refresh));
        out << m;
    }
    std::stable_sort(out.begin(), out.end(), [](const Monitor& a, const Monitor& b) { return a.primary && !b.primary; });
    return out;
}

bool setAutostart(bool on) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    const QString path = dir + "/" + kLinuxAppId + ".desktop";
    QFile::remove(dir + "/clipline.desktop");  // alter Name (vor der App-ID)
    if (!on) return QFile::remove(path) || !QFile::exists(path);
    QDir().mkpath(dir);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QString("[Desktop Entry]\nType=Application\nName=Clipline\nExec=\"%1\" --background\nIcon=clipline\n"
                    "X-GNOME-Autostart-enabled=true\nNoDisplay=false\n")
                .arg(QCoreApplication::applicationFilePath()).toUtf8());
    return true;
}

// ---------------------------------------------------------------- Hinweise und Töne
// Benachrichtigung über org.freedesktop.Notifications - klappt auch ohne Tray-Leiste (Hyprland, Sway, GNOME)
bool desktopNotify(const QString& title, const QString& text, int ms, bool warning) {
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                                                      "org.freedesktop.Notifications", "Notify");
    QVariantMap hints;
    hints["urgency"] = QVariant::fromValue(uchar(warning ? 2 : 1));
    hints["desktop-entry"] = QString(kLinuxAppId);
    msg.setArguments({QString("Clipline"), uint(0), QString("clipline"), title, text, QStringList(), hints, ms});
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusConnectionInterface* iface = bus.interface();
    if (!iface || !iface->isServiceRegistered("org.freedesktop.Notifications").value()) return false;
    return bus.send(msg);
}

// Ton über den Sound-Server (beep() ist unter Wayland stumm): canberra, sonst paplay/pw-play mit freedesktop-Klängen
static void playNamed(const QString& name, const QString& file) {
    const QString canberra = QStandardPaths::findExecutable("canberra-gtk-play");
    if (!canberra.isEmpty() && QProcess::startDetached(canberra, {"-i", name})) return;
    for (const QString& dir : {QString("/usr/share/sounds/freedesktop/stereo/"), QString("/usr/share/sounds/Yaru/stereo/")}) {
        const QString path = dir + file;
        if (!QFileInfo::exists(path)) continue;
        for (const char* player : {"pw-play", "paplay"}) {
            const QString exe = QStandardPaths::findExecutable(player);
            if (!exe.isEmpty() && QProcess::startDetached(exe, {path})) return;
        }
    }
    QApplication::beep();
}
void playSaveSound() { playNamed("camera-shutter", "camera-shutter.oga"); }
void playErrorSound() { playNamed("dialog-warning", "dialog-warning.oga"); }

// ---------------------------------------------------------------- Globaler Hotkey
#ifdef HAVE_X11
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/select.h>

namespace {
unsigned long keysymFor(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return XK_a + (key - Qt::Key_A);
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return XK_0 + (key - Qt::Key_0);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return XK_F1 + (key - Qt::Key_F1);
    switch (key) {
    case Qt::Key_Space: return XK_space;
    case Qt::Key_Print: return XK_Print;
    case Qt::Key_Pause: return XK_Pause;
    case Qt::Key_Insert: return XK_Insert;
    case Qt::Key_Home: return XK_Home;
    case Qt::Key_End: return XK_End;
    case Qt::Key_ScrollLock: return XK_Scroll_Lock;
    default: return 0;
    }
}
std::atomic<bool> g_grabFailed{false};
int onXError(Display*, XErrorEvent* e) {
    if (e->error_code == BadAccess) g_grabFailed = true;
    return 0;
}
}  // namespace

struct GlobalHotkey::Impl {
    std::thread thread;
    std::atomic<bool> stop{false};
    bool wayland = false;
};

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {}
GlobalHotkey::~GlobalHotkey() { clear(); }

bool GlobalHotkey::supported() {
    if (WaylandHotkeys::isWayland()) return WaylandHotkeys::instance().available();
    if (qEnvironmentVariableIsEmpty("DISPLAY")) return false;
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;
    XCloseDisplay(dpy);
    return true;
}

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty()) return false;
    // Wayland: XWayland bekommt nur Tasten, wenn ein X11-Fenster vorne ist - also über den Compositor/das Portal
    if (WaylandHotkeys::isWayland()) {
        d->wayland = WaylandHotkeys::instance().add(this, sequence, description_);
        return d->wayland;
    }
    const QKeyCombination kc = seq[0];
    const unsigned long sym = keysymFor(kc.key());
    if (!sym) return false;
    unsigned int mods = 0;
    const Qt::KeyboardModifiers m = kc.keyboardModifiers();
    if (m & Qt::ControlModifier) mods |= ControlMask;
    if (m & Qt::AltModifier) mods |= Mod1Mask;
    if (m & Qt::ShiftModifier) mods |= ShiftMask;
    if (m & Qt::MetaModifier) mods |= Mod4Mask;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;
    const Window root = DefaultRootWindow(dpy);
    const KeyCode code = XKeysymToKeycode(dpy, sym);
    g_grabFailed = false;
    XErrorHandler old = XSetErrorHandler(onXError);
    // Auch mit aktivem NumLock (Mod2) und CapsLock (Lock) auslösen
    for (unsigned int extra : {0u, unsigned(LockMask), unsigned(Mod2Mask), unsigned(LockMask | Mod2Mask)})
        XGrabKey(dpy, code, mods | extra, root, True, GrabModeAsync, GrabModeAsync);
    XSync(dpy, False);
    XSetErrorHandler(old);
    if (g_grabFailed) {
        appLog(QString("hotkey: X11 grab of %1 refused (used by another program)").arg(sequence));
        XCloseDisplay(dpy);
        return false;
    }
    // Zusätzlich den Tastaturzustand abfragen: Hält ein Programm die ganze Tastatur fest (viele Vollbild-Spiele),
    // kommt der Grab-Druck nie an - "Clippen klappt nur manchmal". Die Modifikatoren müssen genau passen, wie beim Grab.
    struct Poll {
        KeyCode key;
        KeyCode mod[4][2];
        bool need[4];
    } poll{code,
           {{XKeysymToKeycode(dpy, XK_Control_L), XKeysymToKeycode(dpy, XK_Control_R)},
            {XKeysymToKeycode(dpy, XK_Alt_L), XKeysymToKeycode(dpy, XK_Alt_R)},
            {XKeysymToKeycode(dpy, XK_Shift_L), XKeysymToKeycode(dpy, XK_Shift_R)},
            {XKeysymToKeycode(dpy, XK_Super_L), XKeysymToKeycode(dpy, XK_Super_R)}},
           {bool(m & Qt::ControlModifier), bool(m & Qt::AltModifier), bool(m & Qt::ShiftModifier), bool(m & Qt::MetaModifier)}};
    appLog(QString("hotkey: X11 grab %1 (keycode %2)").arg(sequence).arg(code));
    d->stop = false;
    Impl* s = d.get();
    d->thread = std::thread([this, s, dpy, poll] {
        const int fd = ConnectionNumber(dpy);
        auto isDown = [](const char* keys, KeyCode kc) { return kc && (keys[kc / 8] & (1 << (kc % 8))); };
        bool down = false;  // Kombination gerade gedrückt (nur einmal pro Druck auslösen)
        while (!s->stop) {
            bool grabbed = false;
            while (XPending(dpy)) {
                XEvent ev;
                XNextEvent(dpy, &ev);
                grabbed |= ev.type == KeyPress;
            }
            char keys[32];
            XQueryKeymap(dpy, keys);
            bool held = isDown(keys, poll.key);
            for (int g = 0; g < 4 && held; ++g)
                held = (isDown(keys, poll.mod[g][0]) || isDown(keys, poll.mod[g][1])) == poll.need[g];
            if ((grabbed || held) && !down) {
                const bool viaPoll = !grabbed;
                QMetaObject::invokeMethod(this, [this, viaPoll] {
                    appLog(QString("hotkey: X11 key pressed (%1)").arg(viaPoll ? "keyboard state" : "grab"));
                    emit activated();
                }, Qt::QueuedConnection);
            }
            down = held;
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            timeval tv{0, 25000};
            select(fd + 1, &set, nullptr, nullptr, &tv);
        }
        XCloseDisplay(dpy);
    });
    return true;
}

void GlobalHotkey::clear() {
    if (d->wayland) {
        WaylandHotkeys::instance().remove(this);
        d->wayland = false;
    }
    d->stop = true;
    if (d->thread.joinable()) d->thread.join();
}
#else
// Ohne X11 (reiner Wayland-Build): nur der Wayland-Weg
struct GlobalHotkey::Impl {
    bool wayland = false;
};
GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {}
GlobalHotkey::~GlobalHotkey() { clear(); }
bool GlobalHotkey::supported() { return WaylandHotkeys::isWayland() && WaylandHotkeys::instance().available(); }
bool GlobalHotkey::set(const QString& sequence) {
    clear();
    if (!WaylandHotkeys::isWayland() || QKeySequence(sequence).isEmpty()) return false;
    d->wayland = WaylandHotkeys::instance().add(this, sequence, description_);
    return d->wayland;
}
void GlobalHotkey::clear() {
    if (d->wayland) WaylandHotkeys::instance().remove(this);
    d->wayland = false;
}
#endif
