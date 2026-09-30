// Linux: Bildschirme (Qt/X11), Hotkey (XGrabKey in eigenem Thread), Autostart (~/.config/autostart)
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QKeySequence>
#include <QScreen>
#include <QStandardPaths>
#include <atomic>
#include <thread>
#include "platform.h"

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
    const QString path = dir + "/clipline.desktop";
    if (!on) return QFile::remove(path) || !QFile::exists(path);
    QDir().mkpath(dir);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    f.write(QString("[Desktop Entry]\nType=Application\nName=Clipline\nExec=\"%1\" --background\nIcon=clipline\n"
                    "X-GNOME-Autostart-enabled=true\nNoDisplay=false\n")
                .arg(QCoreApplication::applicationFilePath()).toUtf8());
    return true;
}

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
};

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {}
GlobalHotkey::~GlobalHotkey() { clear(); }

bool GlobalHotkey::supported() {
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
        XCloseDisplay(dpy);
        return false;
    }
    d->stop = false;
    Impl* s = d.get();
    d->thread = std::thread([this, s, dpy] {
        const int fd = ConnectionNumber(dpy);
        while (!s->stop) {
            while (XPending(dpy)) {
                XEvent ev;
                XNextEvent(dpy, &ev);
                if (ev.type == KeyPress)
                    QMetaObject::invokeMethod(this, [this] { emit activated(); }, Qt::QueuedConnection);
            }
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            timeval tv{0, 150000};
            select(fd + 1, &set, nullptr, nullptr, &tv);
        }
        XCloseDisplay(dpy);
    });
    return true;
}

void GlobalHotkey::clear() {
    d->stop = true;
    if (d->thread.joinable()) d->thread.join();
}
#else
struct GlobalHotkey::Impl {};
GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {}
GlobalHotkey::~GlobalHotkey() = default;
bool GlobalHotkey::supported() { return false; }
bool GlobalHotkey::set(const QString&) { return false; }
void GlobalHotkey::clear() {}
#endif
