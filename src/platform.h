#pragma once
#include <QList>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <memory>

// ---------------------------------------------------------------- Bildschirme
struct Monitor {
    QString id;          // stabile Kennung (Gerätename)
    QString name;        // Anzeige
    QRect rect;          // physische Pixel im virtuellen Desktop
    bool primary = false;
    double refresh = 60;
    int ddaIndex = -1;   // Windows: Index für ddagrab (Desktop Duplication), -1 = gdigrab
    int avIndex = -1;    // macOS: avfoundation-Geräteindex
};
QList<Monitor> listMonitors();
Monitor findMonitor(const QString& id);  // leer/unbekannt = Hauptbildschirm

// ---------------------------------------------------------------- Ton
QStringList listMicrophones();   // ffmpeg-Gerätenamen
bool systemAudioSupported();

// ---------------------------------------------------------------- System
bool setAutostart(bool on);
QString ffmpegPath();
void revealInFolder(const QString& path);
void playSaveSound();

// ---------------------------------------------------------------- Globaler Hotkey
class GlobalHotkey : public QObject {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;
    bool set(const QString& sequence);  // false = belegt oder nicht unterstützt
    void clear();
    static bool supported();

signals:
    void activated();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

// ---------------------------------------------------------------- Ton-Mischer (nur Windows: WASAPI)
// Systemton (Loopback) und Mikrofon werden hier gemischt und als EIN Datenstrom (f32le, 48 kHz, Stereo)
// über eine Named Pipe an ffmpeg gegeben. Jede weitere Echtzeit-Tonquelle, die ffmpeg selbst einliest,
// bremst die Bildschirmaufnahme stark aus (bis auf ~1 Bild pro Sekunde).
struct MicTuning {
    int gateDb = -45;   // Rauschsperre: leiser als das wird stummgeschaltet; <= -80 = aus
    int volume = 100;   // Prozent
};

class SystemAudioPipe {
public:
    SystemAudioPipe();
    ~SystemAudioPipe();
    // false = weder Systemton noch Mikrofon verfügbar
    bool prepare(bool systemSound, const QString& mic, const MicTuning& tuning);
    QString pipePath() const;
    static QString ffmpegFormat() { return "f32le"; }
    static int rate() { return 48000; }
    static int channels() { return 2; }
    void start();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

// Live-Pegel eines Mikrofons (für die Empfindlichkeits-Einstellung)
class MicLevelMeter {
public:
    MicLevelMeter();
    ~MicLevelMeter();
    bool start(const QString& mic);  // false = nicht unterstützt / Gerät fehlt
    void stop();
    float levelDb() const;           // -90 .. 0

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
bool micMeterSupported();
