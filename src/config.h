#pragma once
#include <QList>
#include <QSize>
#include <QString>

// Ein Clip-Knopf: eigene Taste, Länge und Grafik. Knopf 1 sind die Hauptfelder in Config (hotkey, clipSeconds,
// height, fps, quality), weitere stehen in Config::extraButtons.
struct ClipButton {
    QString hotkey;
    int seconds = 30;
    int height = 0;   // 0 = native Auflösung
    int fps = 60;
    int quality = 1;  // 0 = klein, 1 = ausgewogen, 2 = hoch
};

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
    QString accent = "#ff0033";  // YouTube-Rot (frühere Standardfarbe #ff4d6d bleibt für bestehende Nutzer)
    bool dark = true;
    bool autostart = true;
    bool sound = true;
    QString language;         // "", "en", "de"
    QString waylandToken;     // Linux/Wayland: gemerkte Bildschirmfreigabe (kein Portal-Dialog bei jedem Start)
    bool firstRunDone = false;
    QList<ClipButton> extraButtons;  // Clip-Knopf 2, 3, …

    static Config load();
    void save() const;
    ClipButton mainButton() const;
    QList<ClipButton> allButtons() const;  // Knopf 1 zuerst
    // Was wirklich aufgenommen wird: Auflösung, Bildrate, Qualität und Länge des anspruchsvollsten Knopfs.
    // Knöpfe mit weniger werden beim Speichern aus dieser Aufnahme kleiner gerechnet.
    Config recordingConfig() const;
    Config withButton(const ClipButton& b) const;  // Länge/Grafik dieses Knopfs (für Schätzungen)
};

QString defaultClipsDir();
QSize captureSize(const Config& c, QSize native);
double videoBitrate(const Config& c, QSize native);      // bit/s
double estimateBufferMB(const Config& c, QSize native);  // Ringpuffer im RAM
double estimateTotalMB(const Config& c, QSize native);   // Puffer + Programm + Encoder
double estimateClipMB(const Config& c, QSize native);  // Dateigröße eines gespeicherten Clips
QString fmtDuration(int seconds);
QString fmtMB(double mb);
