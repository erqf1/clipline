#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <deque>
#include <memory>
#include "config.h"
#include "platform.h"

class WaylandCapture;

// Nimmt dauerhaft per ffmpeg auf und hält die letzten Sekunden als MPEG-TS im Arbeitsspeicher.
// Beim Speichern wird der Pufferausschnitt ohne Neukodierung in eine MP4 umgepackt.
class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject* parent = nullptr);
    ~Recorder() override;

    void setConfig(const Config& c);  // startet bei geänderten Aufnahmeparametern neu
    void setMicMuted(bool muted);
    bool micMuted() const { return micMuted_; }
    void start();
    void stop();
    bool wanted() const { return wanted_; }
    bool running() const { return proc_ && proc_->state() == QProcess::Running && gotData_; }
    double bufferedSeconds() const;
    qint64 bufferedBytes() const { return total_ - (chunks_.empty() ? total_ : chunks_.front().off); }
    QString encoder() const { return encoder_; }
    void saveClip();

    static QString detectEncoder(bool forceRecheck = false);

signals:
    void stateChanged();
    void clipSaved(const QString& path, bool ok, const QString& error);
    void warning(const QString& text);
    void waylandTokenChanged(const QString& token);  // Linux/Wayland: gemerkte Bildschirmfreigabe

private:
    struct Chunk {
        qint64 t;    // Ankunftszeit in ms
        qint64 off;  // Byte-Position im Stream
        QByteArray data;
    };
    QStringList buildArgs(bool withAudio);
    void launch();
    void onOutput();
    void onFinished();
    bool captureChanged(const Config& a, const Config& b) const;
    MicTuning tuning() const;
    bool startWayland();  // false = Portal-Dialog läuft noch / nicht möglich
    void applyTuning();

    Config cfg_;
    QString encoder_;
    QProcess* proc_ = nullptr;
    QProcess* gst_ = nullptr;          // Linux/Wayland: GStreamer liefert die Bilder an ffmpeg
    WaylandCapture* wayland_ = nullptr;
    QSize waylandOut_;                 // Bildgröße, die GStreamer liefert
    void stopGst();
    std::unique_ptr<SystemAudioPipe> sysAudio_;
    std::deque<Chunk> chunks_;
    qint64 total_ = 0;
    QElapsedTimer clock_, runTime_;
    QTimer retry_;
    bool wanted_ = false, audioOk_ = true, gotData_ = false, micMuted_ = false;
    int fails_ = 0;
    QByteArray errTail_;
};
