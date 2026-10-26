#include "wayland_hotkeys.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QFileInfo>
#include <QKeySequence>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include "i18n.h"
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

static QMap<QString, QString> triggerMap(const QList<PortalShortcut>& list);  // Kennung -> Taste laut System

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
    const bool finished = p.waitForFinished(3000);
    const QString out = QString::fromUtf8(p.readAllStandardOutput() + p.readAllStandardError()).trimmed();
    const bool ok = finished && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0 &&
                    !out.toLower().contains("error") && !out.toLower().contains("invalid");
    appLog(QString("hotkey: %1 %2 -> %3 %4").arg(program, args.join(' '), ok ? "ok" : "FAILED", out.left(200)));
    return ok;
}
}  // namespace

void registerPortalApp() {
    static bool done = false;
    if (done) return;
    done = true;
    QDBusMessage reg = QDBusMessage::createMethodCall(kService, kPath, "org.freedesktop.host.portal.Registry", "Register");
    reg.setArguments({QString(kLinuxAppId), QVariantMap()});
    const QDBusMessage r = QDBusConnection::sessionBus().call(reg, QDBus::Block, 3000);
    appLog(r.type() == QDBusMessage::ReplyMessage ? QString("portal: registered as %1").arg(kLinuxAppId)
                                                  : "portal: register not possible: " + r.errorMessage());
}

// KDE: Ohne gültige App-ID legte KDE für Clipline bei jedem Start eine neue Tastengruppe mit Zufallsnamen an.
// Die alten Gruppen hielten Strg+Alt+S weiter belegt, die neue bekam keine Taste - die Clip-Taste ging nie.
// Solche Reste entfernen: nur Gruppen, die ausschließlich Clipline-Tasten ("clipline-…") enthalten.
// keepOwn: In Cliplines eigener Gruppe alle Clipline-Tasten entfernen, die nicht mehr gebraucht werden (nach dem
// Ändern der Taste hielt KDE sonst die alte Kombination weiter belegt). Leer = eigene Gruppe nicht anfassen.
static void kdeCleanup(const QStringList& keepOwn) {
    if (!qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE", Qt::CaseInsensitive)) return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    const char* kAccel = "org.kde.kglobalaccel";
    const QDBusMessage r = bus.call(QDBusMessage::createMethodCall(kAccel, "/kglobalaccel", "org.kde.KGlobalAccel", "allComponents"),
                                    QDBus::Block, 3000);
    if (r.type() != QDBusMessage::ReplyMessage || r.arguments().isEmpty()) {
        appLog("hotkey: KDE shortcut list not available: " + r.errorMessage());
        return;
    }
    for (const QDBusObjectPath& path : qdbus_cast<QList<QDBusObjectPath>>(r.arguments().first())) {
        QDBusInterface comp(kAccel, path.path(), "org.kde.kglobalaccel.Component", bus);
        const QString name = comp.property("uniqueName").toString();
        const bool own = name == kLinuxAppId;
        if (name.isEmpty() || (own && keepOwn.isEmpty())) continue;
        const QDBusReply<QStringList> names = comp.call("shortcutNames", QString("default"));
        if (!names.isValid() || names.value().isEmpty()) continue;
        QStringList drop;
        bool onlyClipline = true;
        for (const QString& n : names.value()) {
            onlyClipline &= n.startsWith("clipline-");
            if (n.startsWith("clipline-") && !keepOwn.contains(n)) drop << n;
        }
        if (!own && !onlyClipline) continue;
        for (const QString& n : drop) {
            QDBusMessage un = QDBusMessage::createMethodCall(kAccel, "/kglobalaccel", "org.kde.KGlobalAccel", "unregister");
            un.setArguments({name, n});
            bus.call(un, QDBus::Block, 2000);
        }
        if (!drop.isEmpty())
            appLog(QString("hotkey: removed %1 KDE shortcut(s) %2 from %3").arg(own ? "unused" : "old").arg(drop.join(", "), name));
    }
}

bool WaylandHotkeys::isWayland() {
    return qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
}

WaylandHotkeys& WaylandHotkeys::instance() {
    static WaylandHotkeys* inst = new WaylandHotkeys;
    return *inst;
}

QString WaylandHotkeys::backendName() const {
    switch (backend_) {
    case Hyprland: return "Hyprland";
    case Sway: return "Sway";
    case Portal: return "Portal";
    default: return "none";
    }
}

WaylandHotkeys::WaylandHotkeys() {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();
    rebind_.setSingleShot(true);
    rebind_.setInterval(300);
    connect(&rebind_, &QTimer::timeout, this, &WaylandHotkeys::portalRebind);
    watchRetry_.setSingleShot(true);
    watchRetry_.setInterval(5000);
    connect(&watchRetry_, &QTimer::timeout, this, &WaylandHotkeys::watchCompositor);
    registerPortalApp();
    if (!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE") && !QStandardPaths::findExecutable("hyprctl").isEmpty()) {
        backend_ = Hyprland;
    } else if (!qEnvironmentVariableIsEmpty("SWAYSOCK") && !QStandardPaths::findExecutable("swaymsg").isEmpty()) {
        backend_ = Sway;
    } else {
        QDBusConnection bus = QDBusConnection::sessionBus();
        QDBusMessage get = QDBusMessage::createMethodCall(kService, kPath, "org.freedesktop.DBus.Properties", "Get");
        get.setArguments({QString(kShortcuts), QString("version")});
        const QDBusMessage reply = bus.call(get, QDBus::Block, 3000);
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            portalVersion_ = reply.arguments().first().value<QDBusVariant>().variant().toUInt();
        if (portalVersion_ > 0) {
            backend_ = Portal;
            bus.connect(kService, kPath, kShortcuts, "Activated", this,
                        SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
            bus.connect(kService, kPath, kShortcuts, "ShortcutsChanged", this, SLOT(onShortcutsChanged(QDBusMessage)));
            kdeCleanup({});
        } else {
            appLog("hotkey: no GlobalShortcuts portal: " +
                   (reply.errorMessage().isEmpty() ? QString("version 0") : reply.errorMessage()));
        }
    }
    appLog(QString("hotkey: Wayland backend %1 (desktop %2%3)").arg(backendName(), qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
                                                                     portalVersion_ ? QString(", portal v%1").arg(portalVersion_) : QString()));
    if (backend_ == Hyprland || backend_ == Sway) {
        connect(&hyprEvents_, &QLocalSocket::readyRead, this, [this] {
            bool reloaded = false;
            while (hyprEvents_.canReadLine()) reloaded |= hyprEvents_.readLine().startsWith("configreloaded>>");
            if (reloaded) QTimer::singleShot(300, this, &WaylandHotkeys::rebindCompositor);
        });
        connect(&hyprEvents_, &QLocalSocket::disconnected, &watchRetry_, qOverload<>(&QTimer::start));
        connect(&swayEvents_, &QProcess::readyReadStandardOutput, this, [this] {
            static const QRegularExpression reload(R"("change"\s*:\s*"reload")");
            if (reload.match(QString::fromUtf8(swayEvents_.readAllStandardOutput())).hasMatch())
                QTimer::singleShot(300, this, &WaylandHotkeys::rebindCompositor);
        });
        connect(&swayEvents_, &QProcess::finished, &watchRetry_, qOverload<>(&QTimer::start));
        watchCompositor();
    }
}

// Hyprland/Sway vergessen per Befehl eingetragene Tasten, sobald ihre Einstellungen neu geladen werden
// (bei Hyprland schon beim Speichern der Konfigurationsdatei). Darauf horchen und neu eintragen.
void WaylandHotkeys::watchCompositor() {
    if (backend_ == Hyprland) {
        if (hyprEvents_.state() != QLocalSocket::UnconnectedState) return;
        const QString sig = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
        for (const QString& dir : {qEnvironmentVariable("XDG_RUNTIME_DIR") + "/hypr/", QString("/tmp/hypr/")}) {
            const QString path = dir + sig + "/.socket2.sock";
            if (!QFileInfo::exists(path)) continue;
            hyprEvents_.connectToServer(path);
            if (hyprEvents_.waitForConnected(1000)) return;
        }
        watchRetry_.start();
    } else if (backend_ == Sway) {
        if (swayEvents_.state() != QProcess::NotRunning) return;
        swayEvents_.start("swaymsg", {"-m", "-t", "subscribe", "[\"workspace\"]"});
    }
}

void WaylandHotkeys::rebindCompositor() {
    QStringList done;
    for (const Key& k : keys_) {
        if (done.contains(k.sequence)) continue;
        done << k.sequence;
        compositorBind(k.sequence, true);
    }
    appLog(QString("hotkey: compositor settings reloaded, %1 key(s) bound again").arg(done.size()));
}

QString WaylandHotkeys::idFor(const QString& sequence) {
    return "clipline-" + QKeySequence(sequence).toString(QKeySequence::PortableText).toLower().replace(QRegularExpression("[^a-z0-9]+"), "-");
}

// Hyprland/Sway: Taste direkt im Compositor eintragen bzw. wieder austragen (gilt bis zum Neustart des Compositors;
// Clipline trägt sie bei jedem Start neu ein). Vor dem Eintragen austragen: Reste eines abgestürzten Clipline
// würden sonst doppelt auslösen.
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
        if (!bind) return run("hyprctl", {"keyword", "unbind", head});
        QProcess::execute("hyprctl", {"keyword", "unbind", head});
        return run("hyprctl", {"keyword", "bind", head + ", exec, " + cmd});
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
    int hits = 0;
    for (auto it = keys_.begin(); it != keys_.end(); ++it)
        if (QKeySequence(it->sequence) == want) {
            ++hits;
            emit it.key()->activated();
        }
    appLog(QString("hotkey: %1 pressed (compositor), %2 action(s)").arg(sequence).arg(hits));
}

// ---------------------------------------------------------------- Portal "GlobalShortcuts"
QString WaylandHotkeys::portalTrigger(const QString& sequence) {
    const QKeyCombination kc = combo(sequence);
    const Qt::KeyboardModifiers m = kc.keyboardModifiers();
    QStringList trig;
    if (m & Qt::ControlModifier) trig << "CTRL";
    if (m & Qt::AltModifier) trig << "ALT";
    if (m & Qt::ShiftModifier) trig << "SHIFT";
    if (m & Qt::MetaModifier) trig << "LOGO";
    trig << keyName(kc.key(), true);
    return trig.join('+');
}

QString WaylandHotkeys::portalWanted() const {
    QStringList all;
    for (const Key& k : keys_) {
        const QString e = idFor(k.sequence) + "=" + portalTrigger(k.sequence) + "=" + k.description;
        if (!all.contains(e)) all << e;
    }
    all.sort();
    return all.join(';');
}

// Neue Sitzung, dann alle Tasten auf einmal eintragen (manche Portale erlauben BindShortcuts nur einmal pro Sitzung).
// Sind es dieselben Tasten wie schon eingetragen, bleibt die Sitzung einfach bestehen - auch wenn die Tasten kurz
// weg waren (Einstellungen offen). So fragt das System nicht bei jedem Speichern der Einstellungen neu.
void WaylandHotkeys::portalRebind() {
    if (step_ != Idle) {  // läuft noch: danach nochmal
        rebind_.start();
        return;
    }
    if (keys_.isEmpty()) return;
    const QString want = portalWanted();
    if (want == bound_ && !session_.isEmpty()) return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!session_.isEmpty()) {
        bus.call(QDBusMessage::createMethodCall(kService, session_, "org.freedesktop.portal.Session", "Close"), QDBus::NoBlock);
        session_.clear();
    }
    bound_.clear();
    step_ = Create;
    QVariantMap opt;
    opt["session_handle_token"] = randomToken();
    appLog("portal: creating shortcut session");
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
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, method] {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> r = *w;
        if (r.isError()) {
            step_ = Idle;
            bound_.clear();
            appLog(QString("portal: %1 failed: %2").arg(method, r.error().message()));
            emit problem(L("Your desktop did not set up the hotkey. Set a key for Clipline in your system settings "
                           "(Shortcuts), or bind the command \"clipline --save\" to a key."));
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
        appLog(QString("portal: %1 answered %2 (1 = declined)").arg(step_ == Create ? "CreateSession" : "BindShortcuts").arg(response));
        step_ = Idle;
        bound_.clear();
        emit problem(L("Your desktop did not set up the hotkey. Set a key for Clipline in your system settings "
                       "(Shortcuts), or bind the command \"clipline --save\" to a key."));
        return;
    }
    if (step_ == Create) {
        session_ = results.value("session_handle").toString();
        QList<PortalShortcut> list;
        QStringList seen;
        for (const Key& k : keys_) {
            const QString id = idFor(k.sequence);
            if (seen.contains(id)) continue;
            seen << id;
            PortalShortcut s;
            s.id = id;
            s.props["description"] = k.description.isEmpty() ? QString("Clipline") : k.description;
            s.props["preferred_trigger"] = portalTrigger(k.sequence);
            list << s;
        }
        appLog(QString("portal: session %1, binding %2").arg(session_, seen.join(", ")));
        step_ = Bind;
        bound_ = portalWanted();
        portalRequest("BindShortcuts", {QVariant::fromValue(QDBusObjectPath(session_)), QVariant::fromValue(list), QString()}, {});
    } else {
        step_ = Idle;
        // Was das System eingetragen hat: ohne Taste (belegt/abgelehnt) löst es nie aus
        QList<PortalShortcut> got;
        const QVariant v = results.value("shortcuts");
        if (v.canConvert<QDBusArgument>()) v.value<QDBusArgument>() >> got;
        QStringList missing, info;
        for (const PortalShortcut& s : got) {
            const QString trig = s.props.value("trigger_description").toString();
            info << s.id + " -> " + (trig.isEmpty() ? QString("(no key)") : trig);
            if (trig.isEmpty()) missing << s.id;
        }
        appLog("portal: bound " + (info.isEmpty() ? QString("(no details)") : info.join(", ")));
        setTriggers(triggerMap(got));
        QStringList ids;
        for (const Key& k : keys_) ids << idFor(k.sequence);
        kdeCleanup(ids);
        if (!missing.isEmpty() && missing.size() == got.size()) {
            // KDE fragt beim ersten Mal per Dialog, ob die Taste gilt. Wurde der weggeklickt (oder Clipline vorher
            // beendet), bleibt die Taste leer - dann den Tasten-Dialog des Systems einmal neu öffnen (Portal v2)
            // (CLIPLINE_TEST_NODIALOG: Linux-Test ohne Bildschirm - dort stürzt KWin beim Öffnen des Dialogs ab)
            if (portalVersion_ >= 2 && !askedConfigure_ && !qEnvironmentVariableIsSet("CLIPLINE_TEST_NODIALOG")) {
                askedConfigure_ = true;
                QDBusMessage cfg = QDBusMessage::createMethodCall(kService, kPath, kShortcuts, "ConfigureShortcuts");
                cfg.setArguments({QVariant::fromValue(QDBusObjectPath(session_)), QString(), QVariantMap()});
                QDBusConnection::sessionBus().call(cfg, QDBus::NoBlock);
                appLog("portal: no key assigned, opened the system shortcut dialog");
            }
            emit problem(L("Your desktop did not set up the hotkey. Set a key for Clipline in your system settings "
                           "(Shortcuts), or bind the command \"clipline --save\" to a key."));
        }
    }
}

static QMap<QString, QString> triggerMap(const QList<PortalShortcut>& list) {
    QMap<QString, QString> m;
    for (const PortalShortcut& s : list) m[s.id] = s.props.value("trigger_description").toString();
    return m;
}

void WaylandHotkeys::setTriggers(const QMap<QString, QString>& triggers) {
    triggers_ = triggers;
    emit keysChanged();
}

// Taste in den Systemeinstellungen geändert: neue Belegung merken (Menü und Hinweise zeigen sie dann)
void WaylandHotkeys::onShortcutsChanged(const QDBusMessage& msg) {
    const QList<QVariant> args = msg.arguments();
    if (args.size() < 2 || args[0].value<QDBusObjectPath>().path() != session_) return;
    QList<PortalShortcut> list;
    args[1].value<QDBusArgument>() >> list;
    setTriggers(triggerMap(list));
    QStringList info;
    for (const PortalShortcut& s : list) info << s.id + " -> " + s.props.value("trigger_description").toString();
    appLog("portal: shortcuts changed in the system settings: " + info.join(", "));
}

QString WaylandHotkeys::systemKey(const QString& sequence) const { return triggers_.value(idFor(sequence)); }

void WaylandHotkeys::onActivated(const QDBusObjectPath& session, const QString& id, qulonglong, const QVariantMap&) {
    if (session.path() != session_) {
        appLog(QString("portal: %1 pressed for an old session, ignored").arg(id));
        return;
    }
    int hits = 0;
    for (auto it = keys_.begin(); it != keys_.end(); ++it)
        if (idFor(it->sequence) == id) {
            ++hits;
            emit it.key()->activated();
        }
    appLog(QString("portal: %1 pressed, %2 action(s)").arg(id).arg(hits));
}
