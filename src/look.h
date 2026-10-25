#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Farben + Stylesheet (Akzentfarbe und Hell/Dunkel sind einstellbar)
struct Palette {
    QColor bg, panel, panel2, border, text, muted, accent, accentText, hover;
};
Palette makePalette(const QString& accent, bool dark);
QString styleSheet(const Palette& p);
// Stil: Standard (bisheriges Aussehen) oder Minimal - gilt für makePalette() und styleSheet()
void setMinimalLook(bool on);
bool minimalLook();
const QList<QColor>& accentChoices();

// Logos + Symbole (mit QPainter gezeichnet)
QPixmap cliplineLogo(int size, bool recording = false, bool paused = false);
QIcon cliplineIcon();

enum class Ic { Record, Folder, Gear, Play, Edit, Reveal, Trash, Rename, Pause, Check };
QIcon icon(Ic id, const QColor& c);

// Mausrad scrollt weich (kurze Animation statt Sprüngen), wie im Browser. Touchpads scrollen schon weich.
class QAbstractScrollArea;
void enableSmoothScroll(QAbstractScrollArea* area);
