#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <deque>
#include <memory>
#include "config.h"
#include "platform.h"

// Nimmt dauerhaft per ffmpeg auf und hält die letzten Sekunden als MPEG-TS im Arbeitsspeicher.
// Beim Speichern wird der Pufferausschnitt ohne Neukodierung in eine MP4 umgepackt.
class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject* parent = nullptr);
    ~Recorder() override;

    void setConfig(const Config& c);  // startet bei geänderten Aufnahmeparametern neu
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

    Config cfg_;
    QString encoder_;
    QProcess* proc_ = nullptr;
    std::unique_ptr<SystemAudioPipe> sysAudio_;
    std::deque<Chunk> chunks_;
    qint64 total_ = 0;
    QElapsedTimer clock_, runTime_;
    QTimer retry_;
    bool wanted_ = false, audioOk_ = true, gotData_ = false;
    int fails_ = 0;
    QByteArray errTail_;
};
