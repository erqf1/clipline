#pragma once
#include <QDBusVariant>
#include <QObject>
#include <QSize>
#include <QString>
#include <QVariantMap>

// Linux/Wayland: Dort sieht x11grab nur schwarz - Bildschirmaufnahme geht nur über das XDG-Desktop-Portal
// (ScreenCast) und PipeWire. Beim ersten Mal fragt das System per Dialog nach dem Bildschirm; danach reicht
// das gespeicherte restore_token und die Aufnahme startet ohne Rückfrage im Hintergrund.
// Die Bilder holt GStreamer (pipewiresrc) ab und gibt sie als Rohdaten an ffmpeg weiter.
class WaylandCapture : public QObject {
    Q_OBJECT
public:
    explicit WaylandCapture(QObject* parent = nullptr);
    ~WaylandCapture() override;

    static bool isWaylandSession();
    static bool gstreamerAvailable();  // gst-launch-1.0 mit pipewiresrc installiert?

    // Startet die Portal-Sitzung (asynchron); danach kommt genau einmal done(ok, error).
    void open(const QString& restoreToken);
    void close();
    bool isOpen() const { return !session_.isEmpty() && node_ != 0 && step_ == Idle; }
    bool busy() const { return step_ != Idle; }

    // Neue PipeWire-Verbindung für genau einen GStreamer-Start (ohne CLOEXEC, damit das Kind sie erbt; danach
    // selbst schließen). Eine schon benutzte Verbindung lässt sich nicht an einen neuen Prozess weitergeben -
    // genau das ließ die Aufnahme nach jedem Neustart scheitern und den Puffer leeren. -1 = geht nicht.
    int openRemote();
    quint32 node() const { return node_; }
    QSize size() const { return size_; }           // kann leer sein, wenn das Portal keine Größe meldet
    QString restoreToken() const { return token_; }  // für den nächsten Start ohne Dialog

    // gst-launch-1.0-Argumente: PipeWire-Strom -> NV12 in Zielgröße/Bildrate -> stdout
    QStringList gstArgs(int fd, const QSize& out, int fps) const;

signals:
    void done(bool ok, const QString& error);

private slots:
    void onResponse(uint response, const QVariantMap& results);

private:
    enum Step { Idle, Create, Select, Start };
    void request(const QString& method, QList<QVariant> args, QVariantMap options);
    void listen(const QString& path);
    void fail(const QString& error);
    void finish();

    Step step_ = Idle;
    QString session_, token_, requestPath_;
    quint32 node_ = 0;
    QSize size_;
};
