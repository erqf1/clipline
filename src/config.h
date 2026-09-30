#pragma once
#include <QSize>
#include <QString>

// Alle Einstellungen von Clipline. Gespeichert per QSettings("WSoftware", "Clipline"),
// damit Cutline den Clips-Ordner finden kann.
struct Config {
    QString clipsDir;
    int clipSeconds = 60;
    QString monitor;          // Monitor-ID, leer = Hauptbildschirm
    int height = 1080;        // Aufnahmehöhe, 0 = native Auflösung
    int fps = 60;
    int quality = 1;          // 0 = klein, 1 = ausgewogen, 2 = hoch
    bool systemAudio = true;
    QString mic;              // ffmpeg-Gerätename, leer = kein Mikrofon
    QString hotkey = "Ctrl+Alt+S";
    QString accent = "#ff4d6d";
    bool dark = true;
    bool autostart = true;
    bool sound = true;
    QString language;         // "", "en", "de"
    bool firstRunDone = false;

    static Config load();
    void save() const;
};

QString defaultClipsDir();
QSize captureSize(const Config& c, QSize native);
double videoBitrate(const Config& c, QSize native);      // bit/s
double estimateBufferMB(const Config& c, QSize native);  // Ringpuffer im RAM
double estimateTotalMB(const Config& c, QSize native);   // Puffer + Programm + Encoder
QString fmtDuration(int seconds);
QString fmtMB(double mb);
