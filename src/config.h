#pragma once
#include <QSize>
#include <QString>

// Alle Einstellungen von Clipline, gespeichert per QSettings("WSoftware", "Clipline").
struct Config {
    QString clipsDir;
    int clipSeconds = 60;
    QString monitor;          // Monitor-ID, leer = Hauptbildschirm
    int height = 1080;        // Aufnahmehöhe, 0 = native Auflösung
    int fps = 60;
    int quality = 1;          // 0 = klein, 1 = ausgewogen, 2 = hoch
    bool systemAudio = true;
    QString mic;              // Gerätename, leer = kein Mikrofon
    int micGate = -45;        // Rauschsperre in dB: leiser wird stummgeschaltet, <= -80 = aus
    int micVolume = 100;      // Prozent (0..200)
    int systemVolume = 100;   // Prozent (0..200)
    bool micDenoise = true;   // Tastatur-/Mausklicks und Rauschen aus dem Mikrofon entfernen
    QString hotkey = "Ctrl+Alt+S";
    QString muteHotkey = "Ctrl+Alt+M";  // Mikrofon stumm/an, leer = keiner
    QString accent = "#ff4d6d";
    bool dark = true;
    bool autostart = true;
    bool sound = true;
    QString language;         // "", "en", "de"
    QString waylandToken;     // Linux/Wayland: gemerkte Bildschirmfreigabe (kein Portal-Dialog bei jedem Start)
    bool firstRunDone = false;

    static Config load();
    void save() const;
};

QString defaultClipsDir();
QSize captureSize(const Config& c, QSize native);
double videoBitrate(const Config& c, QSize native);      // bit/s
double estimateBufferMB(const Config& c, QSize native);  // Ringpuffer im RAM
double estimateTotalMB(const Config& c, QSize native);   // Puffer + Programm + Encoder
double estimateClipMB(const Config& c, QSize native);  // Dateigröße eines gespeicherten Clips
QString fmtDuration(int seconds);
QString fmtMB(double mb);
