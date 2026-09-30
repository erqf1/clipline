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

// ---------------------------------------------------------------- Systemton (nur Windows: WASAPI-Loopback)
// Liefert den Systemton als Rohdaten über eine Named Pipe an ffmpeg.
class SystemAudioPipe {
public:
    SystemAudioPipe();
    ~SystemAudioPipe();
    // Format des Wiedergabegeräts; false = kein Gerät
    bool prepare(QString* ffmpegFormat, int* rate, int* channels);
    QString pipePath() const;
    void start();
    void stop();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
