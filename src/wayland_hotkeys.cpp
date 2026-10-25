#include "wayland_hotkeys.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QKeySequence>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include "platform.h"

namespace {
const char* kService = "org.freedesktop.portal.Desktop";
const char* kPath = "/org/freedesktop/portal/desktop";
const char* kShortcuts = "org.freedesktop.portal.GlobalShortcuts";
const char* kRequest = "org.freedesktop.portal.Request";

// Eintrag für BindShortcuts: (sa{sv}) = Kennung + {description, preferred_trigger}
struct PortalShortcut {
    QString id;
    QVariantMap props;
};
}  // namespace

Q_DECLARE_METATYPE(PortalShortcut)

static QDBusArgument& operator<<(QDBusArgument& a, const PortalShortcut& s) {
    a.beginStructure();
    a << s.id << s.props;
    a.endStructure();
    return a;
}

static const QDBusArgument& operator>>(const QDBusArgument& a, PortalShortcut& s) {
    a.beginStructure();
    a >> s.id >> s.props;
    a.endStructure();
    return a;
}

namespace {
// Taste einer Kombination als Name, wie ihn Hyprland/Sway/das Portal erwarten (xkb-Namen).
// lower: Buchstaben klein (Sway, Portal) statt groß (Hyprland)
QString keyName(int key, bool lower) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        const QChar c = QChar('A' + (key - Qt::Key_A));
        return lower ? QString(c.toLower()) : QString(c);
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return QString(QChar('0' + (key - Qt::Key_0)));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return QString("F%1").arg(key - Qt::Key_F1 + 1);
    switch (key) {
    case Qt::Key_Space: return "space";
    case Qt::Key_Print: return "Print";
    case Qt::Key_Pause: return "Pause";
    case Qt::Key_Insert: return "Insert";
    case Qt::Key_Home: return "Home";
    case Qt::Key_End: return "End";
    case Qt::Key_ScrollLock: return "Scroll_Lock";
    default: return QString();
    }
}

QKeyCombination combo(const QString& sequence) {
    const QKeySequence seq(sequence);
    return seq.isEmpty() ? QKeyCombination() : seq[0];
}

// Programm, das die Taste startet (bei AppImages die AppImage-Datei selbst)
QString selfPath() {
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    return appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
}

QString shellQuote(QString s) { return "'" + s.replace("'", "'\\''") + "'"; }

QString randomToken() { return QString("clipline%1").arg(QRandomGenerator::global()->generate()); }

bool run(const QString& program, const QStringList& args) {
    QProcess p;
    p.start(program, args);
    if (!p.waitForFinished(3000) || p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) return false;
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed().toLower();
    return !out.contains("error") && !out.contains("invalid");
}
}  // namespace

bool WaylandHotkeys::isWayland() {
    return qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
}

WaylandHotkeys& WaylandHotkeys::instance() {
    static WaylandHotkeys* inst = new WaylandHotkeys;
    return *inst;
}

WaylandHotkeys::WaylandHotkeys() {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();
    rebind_.setSingleShot(true);
    rebind_.setInterval(300);
    connect(&rebind_, &QTimer::timeout, this, &WaylandHotkeys::portalRebind);
    if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") && !QStandardPaths::findExecutable("hyprctl").isEmpty()) {
        backend_ = Hyprland;
        return;
    }
    if (!qEnvironmentVariableIsEmpty("SWAYSOCK") && !QStandardPaths::findExecutable("swaymsg").isEmpty()) {
        backend_ = Sway;
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    // Neuere Portale wollen wissen, welche App fragt (sonst lehnen manche GlobalShortcuts ab)
    QDBusMessage reg = QDBusMessage::createMethodCall(kService, kPath, "org.freedesktop.host.portal.Registry", "Register");
    reg.setArguments({QString("clipline"), QVariantMap()});
    bus.call(reg, QDBus::Block, 2000);
    QDBusMessage get = QDBusMessage::createMethodCall(kService, kPath, "org.freedesktop.DBus.Properties", "Get");
    get.setArguments({QString(kShortcuts), QString("version")});
    const QDBusMessage reply = bus.call(get, QDBus::Block, 3000);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() &&
        reply.arguments().first().value<QDBusVariant>().variant().toUInt() > 0) {
        backend_ = Portal;
        bus.connect(kService, kPath, kShortcuts, "Activated", this,
                    SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
    }
}

QString WaylandHotkeys::idFor(const QString& sequence) {
    return "clipline-" + QKeySequence(sequence).toString(QKeySequence::PortableText).toLower().replace(QRegularExpression("[^a-z0-9]+"), "-");
}

// Hyprland/Sway: Taste direkt im Compositor eintragen bzw. wieder austragen (gilt bis zum Neustart des Compositors;
// Clipline trägt sie bei jedem Start neu ein)
bool WaylandHotkeys::compositorBind(const QString& sequence, bool bind) {
    const QKeyCombination kc = combo(sequence);
    const Qt::KeyboardModifiers m = kc.keyboardModifiers();
    const QString portable = QKeySequence(sequence).toString(QKeySequence::PortableText);
    const QString cmd = shellQuote(selfPath()) + " --hotkey " + shellQuote(portable);
    if (backend_ == Hyprland) {
        const QString key = keyName(kc.key(), false);
        if (key.isEmpty()) return false;
        QStringList mods;
        if (m & Qt::ControlModifier) mods << "CTRL";
        if (m & Qt::AltModifier) mods << "ALT";
        if (m & Qt::ShiftModifier) mods << "SHIFT";
        if (m & Qt::MetaModifier) mods << "SUPER";
        const QString head = mods.join(' ') + ", " + key;
        return bind ? run("hyprctl", {"keyword", "bind", head + ", exec, " + cmd}) : run("hyprctl", {"keyword", "unbind", head});
    }
    const QString key = keyName(kc.key(), true);
    if (key.isEmpty()) return false;
    QStringList parts;
    if (m & Qt::ControlModifier) parts << "Ctrl";
    if (m & Qt::AltModifier) parts << "Mod1";
    if (m & Qt::ShiftModifier) parts << "Shift";
    if (m & Qt::MetaModifier) parts << "Mod4";
    parts << key;
    const QString combo = parts.join('+');
    return bind ? run("swaymsg", {QString("bindsym --no-repeat %1 exec %2").arg(combo, cmd)})
                : run("swaymsg", {QString("unbindsym %1").arg(combo)});
}

bool WaylandHotkeys::add(GlobalHotkey* hotkey, const QString& sequence, const QString& description) {
    if (backend_ == None || combo(sequence).key() == Qt::Key_unknown || QKeySequence(sequence).isEmpty()) return false;
    remove(hotkey);
    if (backend_ == Hyprland || backend_ == Sway) {
        if (!compositorBind(sequence, true)) return false;
    }
    keys_[hotkey] = {sequence, description};
    if (backend_ == Portal) rebind_.start();
    return true;
}

void WaylandHotkeys::remove(GlobalHotkey* hotkey) {
    auto it = keys_.find(hotkey);
    if (it == keys_.end()) return;
    const QString seq = it->sequence;
    keys_.erase(it);
    if (backend_ == Hyprland || backend_ == Sway) {
        bool usedElsewhere = false;
        for (const Key& k : keys_) usedElsewhere |= k.sequence == seq;
        if (!usedElsewhere) compositorBind(seq, false);
    } else if (backend_ == Portal) {
        rebind_.start();
    }
}

void WaylandHotkeys::trigger(const QString& sequence) {
    const QKeySequence want(sequence);
    for (auto it = keys_.begin(); it != keys_.end(); ++it)
        if (QKeySequence(it->sequence) == want) emit it.key()->activated();
}

// ---------------------------------------------------------------- Portal "GlobalShortcuts"
// Neue Sitzung, dann alle Tasten auf einmal eintragen (manche Portale erlauben BindShortcuts nur einmal pro Sitzung)
void WaylandHotkeys::portalRebind() {
    if (step_ != Idle) {  // läuft noch: danach nochmal
        rebind_.start();
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!session_.isEmpty()) {
        bus.call(QDBusMessage::createMethodCall(kService, session_, "org.freedesktop.portal.Session", "Close"), QDBus::NoBlock);
        session_.clear();
    }
    if (keys_.isEmpty()) return;
    step_ = Create;
    QVariantMap opt;
    opt["session_handle_token"] = randomToken();
    portalRequest("CreateSession", {}, opt);
}

void WaylandHotkeys::portalRequest(const QString& method, QList<QVariant> args, QVariantMap options) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString handle = randomToken();
    options["handle_token"] = handle;
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    // Antwort kommt als Signal am Request-Objekt: vorher abonnieren (Pfad ist vorhersagbar)
    if (!requestPath_.isEmpty())
        bus.disconnect(kService, requestPath_, kRequest, "Response", this, SLOT(onResponse(uint, QVariantMap)));
    requestPath_ = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, handle);
    bus.connect(kService, requestPath_, kRequest, "Response", this, SLOT(onResponse(uint, QVariantMap)));
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kShortcuts, method);
    args << options;
    msg.setArguments(args);
    auto* w = new QDBusPendingCallWatcher(bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w] {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> r = *w;
        if (r.isError()) {
            step_ = Idle;
            return;
        }
        if (r.value().path() != requestPath_) {  // sehr alte Portale nutzen einen anderen Pfad
            QDBusConnection b = QDBusConnection::sessionBus();
            b.disconnect(kService, requestPath_, kRequest, "Response", this, SLOT(onResponse(uint, QVariantMap)));
            requestPath_ = r.value().path();
            b.connect(kService, requestPath_, kRequest, "Response", this, SLOT(onResponse(uint, QVariantMap)));
        }
    });
}

void WaylandHotkeys::onResponse(uint response, const QVariantMap& results) {
    if (response != 0) {
        step_ = Idle;
        return;
    }
    if (step_ == Create) {
        session_ = results.value("session_handle").toString();
        QList<PortalShortcut> list;
        QMap<QString, bool> seen;
        for (const Key& k : keys_) {
            const QString id = idFor(k.sequence);
            if (seen.contains(id)) continue;
            seen[id] = true;
            const QKeyCombination kc = combo(k.sequence);
            const Qt::KeyboardModifiers m = kc.keyboardModifiers();
            QStringList trig;
            if (m & Qt::ControlModifier) trig << "CTRL";
            if (m & Qt::AltModifier) trig << "ALT";
            if (m & Qt::ShiftModifier) trig << "SHIFT";
            if (m & Qt::MetaModifier) trig << "LOGO";
            trig << keyName(kc.key(), true);
            PortalShortcut s;
            s.id = id;
            s.props["description"] = k.description.isEmpty() ? QString("Clipline") : k.description;
            s.props["preferred_trigger"] = trig.join('+');
            list << s;
        }
        step_ = Bind;
        portalRequest("BindShortcuts", {QVariant::fromValue(QDBusObjectPath(session_)), QVariant::fromValue(list), QString()}, {});
    } else {
        step_ = Idle;
    }
}

void WaylandHotkeys::onActivated(const QDBusObjectPath& session, const QString& id, qulonglong, const QVariantMap&) {
    if (session.path() != session_) return;
    for (auto it = keys_.begin(); it != keys_.end(); ++it)
        if (idFor(it->sequence) == id) emit it.key()->activated();
}
