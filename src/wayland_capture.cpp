#include "wayland_capture.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QGuiApplication>
#include <QProcess>
#include <QRandomGenerator>
#include <QScreen>
#include <QStandardPaths>
#include <unistd.h>
#include "i18n.h"

namespace {
const char* kService = "org.freedesktop.portal.Desktop";
const char* kPath = "/org/freedesktop/portal/desktop";
const char* kScreenCast = "org.freedesktop.portal.ScreenCast";
const char* kRequest = "org.freedesktop.portal.Request";

QString randomToken() { return QString("clipline%1").arg(QRandomGenerator::global()->generate()); }

// Eigenschaft des ScreenCast-Portals lesen (0, wenn es sie nicht gibt)
uint portalProperty(const QString& name) {
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, "org.freedesktop.DBus.Properties", "Get");
    msg.setArguments({QString(kScreenCast), name});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 5000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return 0;
    return reply.arguments().first().value<QDBusVariant>().variant().toUInt();
}
}  // namespace

WaylandCapture::WaylandCapture(QObject* parent) : QObject(parent) {}
WaylandCapture::~WaylandCapture() { close(); }

bool WaylandCapture::isWaylandSession() {
    return qEnvironmentVariable("XDG_SESSION_TYPE") == "wayland" || !qEnvironmentVariable("WAYLAND_DISPLAY").isEmpty();
}

bool WaylandCapture::gstreamerAvailable() {
    static int cached = -1;
    if (cached < 0) {
        cached = 0;
        const QString inspect = QStandardPaths::findExecutable("gst-inspect-1.0");
        if (!QStandardPaths::findExecutable("gst-launch-1.0").isEmpty() && !inspect.isEmpty()) {
            QProcess p;
            p.start(inspect, {"--exists", "pipewiresrc"});
            cached = p.waitForFinished(5000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
        }
    }
    return cached == 1;
}

void WaylandCapture::open(const QString& restoreToken) {
    close();
    token_ = restoreToken;
    step_ = Create;
    QVariantMap opt;
    opt["session_handle_token"] = randomToken();
    request("CreateSession", {}, opt);
}

void WaylandCapture::close() {
    if (!session_.isEmpty()) {
        QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(kService, session_, "org.freedesktop.portal.Session", "Close"),
                                           QDBus::NoBlock);
        session_.clear();
    }
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    node_ = 0;
    if (!requestPath_.isEmpty())
        QDBusConnection::sessionBus().disconnect(kService, requestPath_, kRequest, "Response", this,
                                                 SLOT(onResponse(uint, QVariantMap)));
    requestPath_.clear();
    step_ = Idle;
}

// Die Antwort kommt als Signal am Request-Objekt. Dessen Pfad ist vorhersagbar - vor dem Aufruf abonnieren,
// sonst kann die Antwort (bei gespeichertem Token sofort) verloren gehen.
void WaylandCapture::listen(const QString& path) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!requestPath_.isEmpty())
        bus.disconnect(kService, requestPath_, kRequest, "Response", this, SLOT(onResponse(uint, QVariantMap)));
    requestPath_ = path;
    bus.connect(kService, requestPath_, kRequest, "Response", this, SLOT(onResponse(uint, QVariantMap)));
}

void WaylandCapture::request(const QString& method, QList<QVariant> args, QVariantMap options) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    const QString handle = randomToken();
    options["handle_token"] = handle;
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    listen(QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, handle));

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenCast, method);
    args << options;
    msg.setArguments(args);
    auto* w = new QDBusPendingCallWatcher(bus.asyncCall(msg), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w] {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> r = *w;
        if (r.isError()) return fail(r.error().message());
        // Sehr alte Portale nutzen einen anderen Pfad als vorhergesagt
        if (r.value().path() != requestPath_) listen(r.value().path());
    });
}

void WaylandCapture::onResponse(uint response, const QVariantMap& results) {
    if (response != 0) {
        return fail(response == 1 ? L("Screen sharing was cancelled.") : L("Screen sharing is not available."));
    }
    if (step_ == Create) {
        session_ = results.value("session_handle").toString();
        // Monitor wählen; Mauszeiger ins Bild, wenn das Portal das kann; Wahl merken (persist_mode 2)
        const uint cursorModes = portalProperty("AvailableCursorModes");
        const uint version = portalProperty("version");
        QVariantMap opt;
        opt["types"] = uint(1);  // Monitor
        opt["multiple"] = false;
        if (cursorModes & 2) opt["cursor_mode"] = uint(2);  // eingebettet
        if (version >= 4) {
            opt["persist_mode"] = uint(2);
            if (!token_.isEmpty()) opt["restore_token"] = token_;
        }
        step_ = Select;
        request("SelectSources", {QVariant::fromValue(QDBusObjectPath(session_))}, opt);
    } else if (step_ == Select) {
        step_ = Start;
        request("Start", {QVariant::fromValue(QDBusObjectPath(session_)), QString()}, {});
    } else if (step_ == Start) {
        if (results.contains("restore_token")) token_ = results.value("restore_token").toString();
        // streams: a(ua{sv}) - wir nehmen den ersten (einzigen) Bildschirm
        const QDBusArgument streams = results.value("streams").value<QDBusArgument>();
        streams.beginArray();
        bool first = true;
        while (!streams.atEnd()) {
            quint32 node = 0;
            QVariantMap props;
            streams.beginStructure();
            streams >> node >> props;
            streams.endStructure();
            if (!first) continue;
            first = false;
            node_ = node;
            if (props.contains("size")) {
                const QDBusArgument s = props.value("size").value<QDBusArgument>();
                int w = 0, h = 0;
                s.beginStructure();
                s >> w >> h;
                s.endStructure();
                size_ = QSize(w, h);
            }
        }
        streams.endArray();
        if (!node_) return fail(L("No screen was shared."));
        finish();
    }
}

void WaylandCapture::finish() {
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kScreenCast, "OpenPipeWireRemote");
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(session_)), QVariantMap()});
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 10000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return fail(reply.errorMessage().isEmpty() ? L("PipeWire could not be opened.") : reply.errorMessage());
    const QDBusUnixFileDescriptor fd = reply.arguments().first().value<QDBusUnixFileDescriptor>();
    // Eigene Kopie ohne CLOEXEC: GStreamer erbt sie als Kindprozess
    fd_ = fd.isValid() ? ::dup(fd.fileDescriptor()) : -1;
    if (fd_ < 0) return fail(L("PipeWire could not be opened."));
    if (size_.isEmpty() && QGuiApplication::primaryScreen()) {
        QScreen* s = QGuiApplication::primaryScreen();
        size_ = s->geometry().size() * s->devicePixelRatio();
    }
    step_ = Idle;
    emit done(true, QString());
}

void WaylandCapture::fail(const QString& error) {
    const QString keep = token_;
    close();
    token_ = keep;
    emit done(false, error);
}

QStringList WaylandCapture::gstArgs(const QSize& out, int fps) const {
    // keepalive-time: PipeWire schickt bei stillem Bild nichts - das letzte Bild alle 100 ms wiederholen,
    // videorate füllt auf die Bildrate auf (sonst stockt ffmpeg und der Ton läuft davon)
    return {"-q",
            "pipewiresrc", QString("fd=%1").arg(fd_), QString("path=%1").arg(node_), "do-timestamp=true",
            "keepalive-time=100", "always-copy=true",
            "!", "videoconvert", "!", "videoscale", "!", "videorate",
            "!", QString("video/x-raw,format=NV12,width=%1,height=%2,framerate=%3/1,pixel-aspect-ratio=1/1")
                     .arg(out.width()).arg(out.height()).arg(fps),
            "!", "fdsink", "fd=1", "sync=false"};
}
