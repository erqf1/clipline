#include "recorder.h"

#include <QDateTime>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryFile>
#include "i18n.h"

static constexpr int kTsPacket = 188;

Recorder::Recorder(QObject* parent) : QObject(parent) {
    clock_.start();
    retry_.setSingleShot(true);
    connect(&retry_, &QTimer::timeout, this, &Recorder::launch);
}

Recorder::~Recorder() { stop(); }

// Ersten funktionierenden Hardware-Encoder finden; Ergebnis wird pro ffmpeg-Datei zwischengespeichert
QString Recorder::detectEncoder(bool forceRecheck) {
    QSettings s("WSoftware", "Clipline");
    const QString ff = ffmpegPath();
    const QString key = ff + "|" + QString::number(QFileInfo(ff).lastModified().toSecsSinceEpoch());
    if (!forceRecheck && s.value("encoderKey").toString() == key && !s.value("encoder").toString().isEmpty())
        return s.value("encoder").toString();
    QStringList candidates;
#if defined(Q_OS_WIN)
    candidates = {"h264_nvenc", "h264_amf", "h264_qsv"};
#elif defined(Q_OS_MAC)
    candidates = {"h264_videotoolbox"};
#else
    candidates = {"h264_nvenc"};
#endif
    QString found = "libx264";
    for (const QString& enc : candidates) {
        QProcess p;
        p.start(ff, {"-hide_banner", "-v", "error", "-f", "lavfi", "-i", "color=c=black:s=640x360:r=30:d=0.3",
                     "-pix_fmt", "nv12", "-c:v", enc, "-f", "null", "-"});
        if (p.waitForFinished(15000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
            found = enc;
            break;
        }
    }
    s.setValue("encoder", found);
    s.setValue("encoderKey", key);
    return found;
}

bool Recorder::captureChanged(const Config& a, const Config& b) const {
    return a.monitor != b.monitor || a.height != b.height || a.fps != b.fps || a.quality != b.quality ||
           a.systemAudio != b.systemAudio || a.mic != b.mic;
}

MicTuning Recorder::tuning() const {
    MicTuning t;
    t.gateDb = cfg_.micGate;
    t.micVolume = cfg_.micVolume;
    t.systemVolume = cfg_.systemVolume;
    t.denoise = cfg_.micDenoise;
    t.muted = micMuted_;
    return t;
}

// Lautstärken, Sperre, Unterdrückung, Stumm: unter Windows live im Ton-Mischer (keine Lücke im Puffer),
// sonst stecken sie in den ffmpeg-Filtern -> Aufnahme neu starten
void Recorder::applyTuning() {
#if defined(Q_OS_WIN)
    if (sysAudio_) sysAudio_->setTuning(tuning());
#else
    if (wanted_) {
        stop();
        start();
    }
#endif
}

void Recorder::setConfig(const Config& c) {
    const bool restart = captureChanged(cfg_, c) && wanted_;
    const Config old = cfg_;
    cfg_ = c;
    if (restart) {
        audioOk_ = true;
        stop();
        start();
    } else if (old.micGate != c.micGate || old.micVolume != c.micVolume || old.systemVolume != c.systemVolume ||
               old.micDenoise != c.micDenoise) {
        applyTuning();
    }
}

void Recorder::setMicMuted(bool muted) {
    if (micMuted_ == muted) return;
    micMuted_ = muted;
    applyTuning();
}

void Recorder::start() {
    wanted_ = true;
    fails_ = 0;
    if (encoder_.isEmpty()) encoder_ = detectEncoder();
    launch();
}

void Recorder::stop() {
    wanted_ = false;
    retry_.stop();
    if (proc_) {
        proc_->disconnect(this);
        if (proc_->state() != QProcess::NotRunning) {
            proc_->write("q");
            if (!proc_->waitForFinished(1500)) proc_->kill();
            proc_->waitForFinished(1000);
        }
        proc_->deleteLater();
        proc_ = nullptr;
    }
    if (sysAudio_) sysAudio_->stop();
    sysAudio_.reset();
    chunks_.clear();
    total_ = 0;
    gotData_ = false;
    emit stateChanged();
}

double Recorder::bufferedSeconds() const {
    if (chunks_.empty()) return 0;
    return std::min<double>(cfg_.clipSeconds, (clock_.elapsed() - chunks_.front().t) / 1000.0);
}

QStringList Recorder::buildArgs(bool withAudio) {
    const Monitor mon = findMonitor(cfg_.monitor);
    const QSize native = mon.rect.isEmpty() ? QSize(1920, 1080) : mon.rect.size();
    const QSize out = captureSize(cfg_, native);
    const QString fps = QString::number(cfg_.fps);
    // stdin bleibt offen, damit "q" ffmpeg sauber beendet
    QStringList a = {"-hide_banner", "-loglevel", "error"};
    QString vpre;            // Filter vor der Skalierung
    int audioInputs = 0;
    int micInput = -1;  // Eingang mit dem Mikrofon, falls ffmpeg es selbst einliest (macOS/Linux)
    bool audioInVideoInput = false;

#if defined(Q_OS_WIN)
    if (mon.ddaIndex >= 0) {
        a << "-f" << "lavfi" << "-i"
          << QString("ddagrab=output_idx=%1:framerate=%2:draw_mouse=1").arg(mon.ddaIndex).arg(fps);
        vpre = "hwdownload,format=bgra,";
    } else {
        a << "-f" << "gdigrab" << "-framerate" << fps << "-draw_mouse" << "1" << "-offset_x"
          << QString::number(mon.rect.x()) << "-offset_y" << QString::number(mon.rect.y()) << "-video_size"
          << QString("%1x%2").arg(mon.rect.width()).arg(mon.rect.height()) << "-i" << "desktop";
    }
    // Systemton + Mikrofon mischt Clipline selbst: ffmpeg bekommt nur EINE Tonquelle. Zwei getrennte
    // Echtzeit-Quellen (Loopback + DirectShow-Mikrofon) bremsten die Bildschirmaufnahme auf ~1 fps.
    if (withAudio && (cfg_.systemAudio || !cfg_.mic.isEmpty())) {
        sysAudio_ = std::make_unique<SystemAudioPipe>();
        if (sysAudio_->prepare(cfg_.systemAudio, cfg_.mic, tuning())) {
            a << "-thread_queue_size" << "1024" << "-f" << SystemAudioPipe::ffmpegFormat() << "-ar"
              << QString::number(SystemAudioPipe::rate()) << "-ac" << QString::number(SystemAudioPipe::channels()) << "-i"
              << sysAudio_->pipePath();
            ++audioInputs;
        } else {
            sysAudio_.reset();
        }
    }
#elif defined(Q_OS_MAC)
    int micIdx = -1;
    if (withAudio && !cfg_.mic.isEmpty()) micIdx = listMicrophones().indexOf(cfg_.mic);
    a << "-f" << "avfoundation" << "-capture_cursor" << "1" << "-framerate" << fps << "-pixel_format" << "nv12"
      << "-i" << QString("%1:%2").arg(std::max(0, mon.avIndex)).arg(micIdx >= 0 ? QString::number(micIdx) : "none");
    if (micIdx >= 0) {
        audioInVideoInput = true;
        micInput = 0;
    }
#else
    const QString display = qEnvironmentVariable("DISPLAY", ":0");
    a << "-f" << "x11grab" << "-framerate" << fps << "-draw_mouse" << "1" << "-video_size"
      << QString("%1x%2").arg(mon.rect.width()).arg(mon.rect.height()) << "-i"
      << QString("%1+%2,%3").arg(display).arg(mon.rect.x()).arg(mon.rect.y());
    if (withAudio && cfg_.systemAudio) {
        a << "-thread_queue_size" << "1024" << "-f" << "pulse" << "-i" << "@DEFAULT_MONITOR@";
        ++audioInputs;
    }
    if (withAudio && !cfg_.mic.isEmpty()) {
        a << "-thread_queue_size" << "1024" << "-f" << "pulse" << "-i" << cfg_.mic;
        micInput = ++audioInputs;
    }
#endif

    // Video: skalieren + Pixelformat für den Encoder
    QString vf = vpre;
    if (out != native) vf += QString("scale=%1:%2:flags=bicubic,").arg(out.width()).arg(out.height());
    vf += encoder_ == "libx264" ? "format=yuv420p" : "format=nv12";
    QStringList fc = {QString("[0:v]%1[v]").arg(vf)};
    // macOS/Linux: Mikrofon (Rauschunterdrückung, Sperre, Lautstärke, stumm) und Systemton-Lautstärke als
    // ffmpeg-Filter. Unter Windows erledigt das alles der eigene Ton-Mischer (sysAudio_), dort nichts doppelt.
    QStringList micF;
    if (cfg_.micDenoise) micF << "afftdn=nf=-25:tn=1";
    if (cfg_.micGate > -80)
        micF << QString("agate=threshold=%1:ratio=20:attack=5:release=120:range=0.001")
                    .arg(std::pow(10.0, cfg_.micGate / 20.0), 0, 'f', 6);
    micF << QString("volume=%1").arg(micMuted_ ? 0.0 : cfg_.micVolume / 100.0, 0, 'f', 2);
    const QString sysF = QString("volume=%1").arg(cfg_.systemVolume / 100.0, 0, 'f', 2);
    auto src = [&](int input) {
        if (sysAudio_) return QString("%1:a").arg(input);  // Windows: genau eine Tonquelle, direkt zuordnen
        fc << QString("[%1:a]%2[s%1]").arg(input).arg(input == micInput ? micF.join(',') : sysF);
        return QString("[s%1]").arg(input);
    };
    QString amap;
    if (audioInVideoInput) {
        amap = src(0);
    } else if (audioInputs == 1) {
        amap = src(1);
    } else if (audioInputs == 2) {
        const QString x = src(1), y = src(2);
        fc << QString("%1%2amix=inputs=2:duration=longest:normalize=0[a]").arg(x, y);
        amap = "[a]";
    }
    a << "-filter_complex" << fc.join(";") << "-map" << "[v]";
    if (!amap.isEmpty()) a << "-map" << amap;

    const qint64 br = qint64(videoBitrate(cfg_, native));
    const QString b = QString::number(br), maxr = QString::number(br * 3 / 2), buf = QString::number(br * 2);
    const QString gop = fps;  // ein Keyframe pro Sekunde -> Clips starten genau
    if (encoder_ == "h264_nvenc")
        a << "-c:v" << encoder_ << "-preset" << "p2" << "-tune" << "ll" << "-rc" << "vbr" << "-b:v" << b << "-maxrate" << maxr
          << "-bufsize" << buf;
    else if (encoder_ == "h264_amf")
        // "transcoding": nur so hält AMF das Keyframe-Intervall ein (lowlatency setzt nur einen einzigen IDR)
        a << "-c:v" << encoder_ << "-usage" << "transcoding" << "-quality" << "speed" << "-rc" << "vbr_peak" << "-b:v" << b
          << "-maxrate" << maxr << "-bufsize" << buf;
    else if (encoder_ == "h264_qsv")
        a << "-c:v" << encoder_ << "-preset" << "veryfast" << "-b:v" << b << "-maxrate" << maxr << "-bufsize" << buf;
    else if (encoder_ == "h264_videotoolbox")
        a << "-c:v" << encoder_ << "-realtime" << "1" << "-b:v" << b << "-maxrate" << maxr << "-bufsize" << buf;
    else
        a << "-c:v" << "libx264" << "-preset" << "ultrafast" << "-tune" << "zerolatency" << "-b:v" << b << "-maxrate"
          << maxr << "-bufsize" << buf;
    // Keyframe jede Sekunde und SPS/PPS vor jedem Paket -> jeder Ausschnitt aus dem Puffer ist abspielbar.
    // Nicht nur "freq=keyframe": AMF markiert die erzwungenen Keyframes nicht immer als solche, dann fehlten
    // SPS/PPS im Puffer, sobald der Anfang der Aufnahme herausgefallen ist ("non-existing PPS", leerer Clip).
    // Kostet nur ein paar KB pro Sekunde.
    a << "-g" << gop << "-force_key_frames" << "expr:gte(t,n_forced*1)" << "-flags" << "+global_header" << "-bsf:v"
      << "dump_extra=freq=all";
    if (!amap.isEmpty()) a << "-c:a" << "aac" << "-b:a" << "160k" << "-ar" << "48000" << "-ac" << "2";
    a << "-f" << "mpegts" << "-muxdelay" << "0" << "pipe:1";
    return a;
}

void Recorder::launch() {
    if (!wanted_) return;
    if (proc_) {
        proc_->disconnect(this);
        proc_->kill();
        proc_->waitForFinished(1000);
        proc_->deleteLater();
    }
    if (sysAudio_) sysAudio_->stop();
    sysAudio_.reset();
    chunks_.clear();
    total_ = 0;
    gotData_ = false;
    errTail_.clear();

    const bool withAudio = audioOk_ && (cfg_.systemAudio || !cfg_.mic.isEmpty());
    const QStringList args = buildArgs(withAudio);
    proc_ = new QProcess(this);
    proc_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(proc_, &QProcess::readyReadStandardOutput, this, &Recorder::onOutput);
    connect(proc_, &QProcess::readyReadStandardError, this, [this] {
        errTail_ += proc_->readAllStandardError();
        if (errTail_.size() > 4000) errTail_ = errTail_.right(4000);
    });
    connect(proc_, &QProcess::finished, this, &Recorder::onFinished);
    connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) onFinished();
    });
    if (sysAudio_) sysAudio_->start();
    runTime_.start();
    proc_->start(ffmpegPath(), args);
    emit stateChanged();
}

void Recorder::onOutput() {
    const QByteArray data = proc_->readAllStandardOutput();
    if (data.isEmpty()) return;
    const qint64 now = clock_.elapsed();
    if (!gotData_) {
        gotData_ = true;
        fails_ = 0;
        emit stateChanged();
    }
    if (chunks_.empty() || now - chunks_.back().t > 250 || chunks_.back().data.size() > (4 << 20))
        chunks_.push_back({now, total_, data});
    else
        chunks_.back().data.append(data);
    total_ += data.size();
    // Alles verwerfen, was älter ist als Cliplänge + Reserve
    const qint64 keepFrom = now - qint64(cfg_.clipSeconds + 6) * 1000;
    while (chunks_.size() > 1 && chunks_[1].t < keepFrom) chunks_.pop_front();
}

void Recorder::onFinished() {
    if (!proc_) return;
    const bool quick = runTime_.elapsed() < 5000;
    const QString err = QString::fromUtf8(errTail_).trimmed();
    proc_->deleteLater();
    proc_ = nullptr;
    if (sysAudio_) sysAudio_->stop();
    sysAudio_.reset();
    gotData_ = false;
    emit stateChanged();
    if (!wanted_) return;
    fails_ = quick ? fails_ + 1 : 1;
    // Häufigste Ursache für sofortiges Scheitern: Audiogerät. Dann ohne Ton weiter.
    if (quick && fails_ >= 2 && audioOk_ && (cfg_.systemAudio || !cfg_.mic.isEmpty())) {
        audioOk_ = false;
        fails_ = 0;
        emit warning(L("The audio device could not be opened. Recording continues without sound."));
    } else if (fails_ == 4) {
        emit warning(L("Recording failed to start:") + "\n" + err.right(400));
    }
    retry_.start(std::min(15000, 500 * fails_ * fails_));
}

void Recorder::saveClip() {
    if (chunks_.empty() || total_ < kTsPacket * 50) {
        emit clipSaved(QString(), false, L("Nothing recorded yet."));
        return;
    }
    // Ausschnitt: Cliplänge + 3 s Reserve (ffmpeg schneidet später genau auf die Länge)
    const qint64 from = clock_.elapsed() - qint64(cfg_.clipSeconds + 3) * 1000;
    size_t first = 0;
    while (first + 1 < chunks_.size() && chunks_[first].t < from) ++first;
    const qint64 start = (chunks_[first].off + kTsPacket - 1) / kTsPacket * kTsPacket;
    const qint64 end = total_ / kTsPacket * kTsPacket;
    if (end - start < kTsPacket * 50) {
        emit clipSaved(QString(), false, L("Nothing recorded yet."));
        return;
    }

    auto* tmp = new QTemporaryFile(QDir::temp().filePath("clipline-XXXXXX.ts"), this);
    if (!tmp->open()) {
        delete tmp;
        emit clipSaved(QString(), false, L("Could not save the clip."));
        return;
    }
    for (size_t i = first; i < chunks_.size(); ++i) {
        const Chunk& c = chunks_[i];
        const qint64 a = std::max(start, c.off), b = std::min(end, c.off + qint64(c.data.size()));
        if (b > a) tmp->write(c.data.constData() + (a - c.off), b - a);
    }
    tmp->flush();
    tmp->close();

    QDir().mkpath(cfg_.clipsDir);
    const QString base = "Clip " + QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss");
    QString out = QDir(cfg_.clipsDir).filePath(base + ".mp4");
    for (int n = 2; QFileInfo::exists(out); ++n) out = QDir(cfg_.clipsDir).filePath(QString("%1 (%2).mp4").arg(base).arg(n));

    auto* p = new QProcess(this);
    const QString tsPath = tmp->fileName();
    const int secs = cfg_.clipSeconds;
    connect(p, &QProcess::finished, this, [this, p, tmp, out, tsPath, secs](int code, QProcess::ExitStatus st) {
        const QString err = QString::fromUtf8(p->readAllStandardError()).trimmed();
        p->deleteLater();
        const bool ok = st == QProcess::NormalExit && code == 0 && QFileInfo(out).size() > 1024;
        if (ok) {
            tmp->deleteLater();
            emit clipSaved(out, true, QString());
            return;
        }
        // Fallback: ohne Suchen am Ende einfach alles umpacken
        QFile::remove(out);
        auto* p2 = new QProcess(this);
        connect(p2, &QProcess::finished, this, [this, p2, tmp, out, err](int c2, QProcess::ExitStatus s2) {
            p2->deleteLater();
            tmp->deleteLater();
            const bool ok2 = s2 == QProcess::NormalExit && c2 == 0 && QFileInfo(out).size() > 1024;
            if (!ok2) QFile::remove(out);  // keine leere/kaputte Datei im Clip-Ordner liegen lassen
            emit clipSaved(ok2 ? out : QString(), ok2, ok2 ? QString() : err);
        });
        p2->start(ffmpegPath(), {"-hide_banner", "-loglevel", "error", "-y", "-probesize", "32M", "-i", tsPath, "-map", "0", "-c", "copy",
                                 "-movflags", "+faststart", out});
        Q_UNUSED(secs);
    });
    p->start(ffmpegPath(), {"-hide_banner", "-loglevel", "error", "-y", "-probesize", "32M", "-sseof", QString("-%1").arg(secs), "-i", tsPath,
                            "-map", "0", "-c", "copy", "-avoid_negative_ts", "make_zero", "-movflags", "+faststart", out});
}
