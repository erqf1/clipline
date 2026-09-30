#include "i18n.h"

#include <QHash>
#include <QLocale>

static QString g_lang = "en";

void setLanguage(const QString& code) {
    QString c = code;
    if (c.isEmpty()) c = QLocale::system().name().left(2);
    g_lang = (c == "de") ? "de" : "en";
}

QString language() { return g_lang; }

static const QHash<QString, QString>& german() {
    static const QHash<QString, QString> h = {
        {"Welcome to Clipline", "Willkommen bei Clipline"},
        {"Clipline records your screen quietly in the background. Press a key and the last moments are saved as a clip – nothing is written to disk until you do.",
         "Clipline nimmt deinen Bildschirm leise im Hintergrund auf. Ein Tastendruck, und die letzten Momente werden als Clip gespeichert – vorher wird nichts auf die Festplatte geschrieben."},
        {"Clips folder", "Clips-Ordner"},
        {"Where should your clips go?", "Wo sollen deine Clips landen?"},
        {"Choose…", "Auswählen…"},
        {"Language", "Sprache"},
        {"System", "System"},
        {"Clip length", "Cliplänge"},
        {"How much should one clip contain?", "Wie viel soll ein Clip enthalten?"},
        {"When you press the hotkey, everything from this long before is saved.", "Beim Drücken des Hotkeys wird alles aus dieser Zeit davor gespeichert."},
        {"Estimated RAM", "Geschätzter RAM"},
        {"buffer", "Puffer"},
        {"Clipline keeps the recording in memory, so longer clips and higher quality need more RAM.",
         "Clipline hält die Aufnahme im Arbeitsspeicher. Längere Clips und höhere Qualität brauchen mehr RAM."},
        {"Quality", "Qualität"},
        {"How should clips look?", "Wie sollen die Clips aussehen?"},
        {"Screen", "Bildschirm"},
        {"Resolution", "Auflösung"},
        {"Frame rate", "Bildrate"},
        {"Detail", "Detail"},
        {"Small", "Klein"},
        {"Balanced", "Ausgewogen"},
        {"High", "Hoch"},
        {"Native", "Nativ"},
        {"System sound", "Systemton"},
        {"Microphone", "Mikrofon"},
        {"None", "Keins"},
        {"Look & behaviour", "Aussehen & Verhalten"},
        {"Make it yours", "Mach es zu deinem"},
        {"Accent colour", "Akzentfarbe"},
        {"Mode", "Modus"},
        {"Dark", "Dunkel"},
        {"Light", "Hell"},
        {"Save hotkey", "Speicher-Hotkey"},
        {"Start with the computer (runs in the background)", "Mit dem Computer starten (läuft im Hintergrund)"},
        {"Play a sound when a clip is saved", "Ton abspielen, wenn ein Clip gespeichert wird"},
        {"Back", "Zurück"},
        {"Next", "Weiter"},
        {"Start recording", "Aufnahme starten"},
        {"Settings", "Einstellungen"},
        {"General", "Allgemein"},
        {"Save", "Speichern"},
        {"Cancel", "Abbrechen"},
        {"Save clip", "Clip speichern"},
        {"Open folder", "Ordner öffnen"},
        {"Open Clipline", "Clipline öffnen"},
        {"Pause recording", "Aufnahme pausieren"},
        {"Resume recording", "Aufnahme fortsetzen"},
        {"Quit", "Beenden"},
        {"Recording", "Nimmt auf"},
        {"Paused", "Pausiert"},
        {"Starting…", "Startet…"},
        {"last %1", "letzte %1"},
        {"Clip saved", "Clip gespeichert"},
        {"Could not save the clip.", "Der Clip konnte nicht gespeichert werden."},
        {"Nothing recorded yet.", "Noch nichts aufgenommen."},
        {"Clipline is still running in the background. Press %1 to save a clip.",
         "Clipline läuft weiter im Hintergrund. Drücke %1, um einen Clip zu speichern."},
        {"No clips yet", "Noch keine Clips"},
        {"Press %1 and the last %2 are saved here.", "Drücke %1 und die letzten %2 landen hier."},
        {"%1 clips", "%1 Clips"},
        {"1 clip", "1 Clip"},
        {"Play", "Abspielen"},
        {"Edit in Cutline", "In Cutline bearbeiten"},
        {"Show in folder", "Im Ordner zeigen"},
        {"Rename…", "Umbenennen…"},
        {"Move to trash", "In den Papierkorb"},
        {"Rename clip", "Clip umbenennen"},
        {"New name:", "Neuer Name:"},
        {"Move \"%1\" to the trash?", "\"%1\" in den Papierkorb verschieben?"},
        {"Tip: install Cutline to trim and edit your clips.", "Tipp: Mit Cutline kannst du deine Clips schneiden und bearbeiten."},
        {"Get Cutline", "Cutline holen"},
        {"The hotkey %1 is already used by another program. Choose a different one in the settings.",
         "Der Hotkey %1 wird schon von einem anderen Programm benutzt. Wähle in den Einstellungen einen anderen."},
        {"Global hotkeys are not available here. Bind the command \"clipline --save\" to a key in your system settings.",
         "Globale Hotkeys sind hier nicht verfügbar. Lege den Befehl \"clipline --save\" in den Systemeinstellungen auf eine Taste."},
        {"The audio device could not be opened. Recording continues without sound.",
         "Das Audiogerät konnte nicht geöffnet werden. Die Aufnahme läuft ohne Ton weiter."},
        {"Recording failed to start:", "Die Aufnahme konnte nicht starten:"},
        {"Not available on this system", "Auf diesem System nicht verfügbar"},
        {"Press keys…", "Tasten drücken…"},
        {"Encoder", "Encoder"},
        {"CPU (x264)", "CPU (x264)"},
        {"Primary", "Hauptbildschirm"},
        {"About", "Über"},
        {"Newest first", "Neueste zuerst"},
    };
    return h;
}

QString L(const char* english) {
    const QString key = QString::fromUtf8(english);
    if (g_lang == "de") {
        auto it = german().constFind(key);
        if (it != german().constEnd()) return *it;
    }
    return key;
}
