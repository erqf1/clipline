#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Farben + Stylesheet (Akzentfarbe und Hell/Dunkel sind einstellbar)
struct Palette {
    QColor bg, panel, panel2, border, text, muted, accent, accentText;
};
Palette makePalette(const QString& accent, bool dark);
QString styleSheet(const Palette& p);
const QList<QColor>& accentChoices();

// Logos + Symbole (mit QPainter gezeichnet)
QPixmap cliplineLogo(int size, bool recording = false, bool paused = false);
QIcon cliplineIcon();

enum class Ic { Record, Folder, Gear, Play, Edit, Reveal, Trash, Rename, Pause, Check };
QIcon icon(Ic id, const QColor& c);
