#include "look.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <cmath>

const QList<QColor>& accentChoices() {
    static const QList<QColor> c = {QColor("#ff0033"), QColor("#ff4d6d"), QColor("#ff7a3d"), QColor("#f5b400"), QColor("#22c55e"),
                                    QColor("#06b6d4"), QColor("#3b82f6"), QColor("#8b5cf6"), QColor("#ec4899")};
    return c;
}

Palette makePalette(const QString& accent, bool dark) {
    Palette p;
    p.accent = QColor(accent).isValid() ? QColor(accent) : QColor("#ff0033");
    // Grautöne wie bei YouTube
    if (dark) {
        p.bg = "#0f0f0f"; p.panel = "#181818"; p.panel2 = "#272727"; p.border = "#303030";
        p.text = "#f1f1f1"; p.muted = "#aaaaaa"; p.hover = "#3f3f3f";
    } else {
        p.bg = "#ffffff"; p.panel = "#f9f9f9"; p.panel2 = "#f2f2f2"; p.border = "#e5e5e5";
        p.text = "#0f0f0f"; p.muted = "#606060"; p.hover = "#e5e5e5";
    }
    p.accentText = p.accent.lightnessF() > 0.62 ? QColor("#15161c") : QColor("#ffffff");
    return p;
}

QString styleSheet(const Palette& p) {
    QString s = R"(
* { font-family: 'Roboto','Segoe UI','SF Pro Text','Helvetica Neue','Ubuntu','Noto Sans',sans-serif; font-size: 13px; color: @text@; }
QWidget#root, QDialog, QMainWindow { background: @bg@; }
QLabel { background: transparent; }
QLabel#muted { color: @muted@; }
QLabel#h1 { font-size: 24px; font-weight: 700; }
QLabel#h2 { font-size: 17px; font-weight: 650; }
QLabel#big { font-size: 30px; font-weight: 750; color: @accent@; }
QLabel#pill { background: @panel2@; border: 1px solid @border@; border-radius: 12px; padding: 4px 12px; }
QFrame#card { background: @panel@; border: none; border-radius: 12px; }
QFrame#side { background: @panel@; border: none; }
QPushButton { background: @panel2@; border: none; border-radius: 18px; padding: 8px 16px; font-weight: 500; }
QPushButton:hover { background: @hover@; }
QPushButton:pressed { background: @border@; }
QPushButton#dashed { background: transparent; border: 2px dashed @border@; border-radius: 12px; padding: 11px; color: @muted@; font-weight: 600; }
QPushButton#dashed:hover { border-color: @text@; color: @text@; }
QPushButton:disabled { color: @muted@; }
QPushButton#primary { background: @accent@; color: @acctext@; border-color: @accent@; font-weight: 650; }
QPushButton#primary:hover { background: @accent2@; }
QPushButton#flat { background: transparent; border: none; color: @muted@; padding: 6px 8px; border-radius: 16px; }
QPushButton#flat:hover { background: @panel2@; }
QPushButton#flat:hover { color: @text@; }
QPushButton#hotkey { font-size: 20px; font-weight: 700; letter-spacing: 1px; padding: 12px; border-radius: 12px;
    border: 2px solid @border@; border-bottom-width: 5px; background: @panel2@; }
QPushButton#hotkey:hover { border-color: @muted@; background: @panel2@; }
QPushButton#hotkey[recording="true"] { border-color: @accent@; color: @accent@; background: @panel@; }
QPushButton#swatch { border-radius: 15px; min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px; padding: 0; border: 2px solid transparent; }
QPushButton#swatch:checked { border: 3px solid @text@; }
/* Auswahl wie die Chips bei YouTube: gewählt = Textfarbe als Fläche */
QPushButton#seg { border-radius: 8px; padding: 7px 14px; margin-right: 8px; }
QPushButton#seg:checked { background: @text@; color: @bg@; }
QComboBox, QLineEdit, QKeySequenceEdit { background: @bg@; border: 1px solid @border@; border-radius: 8px; padding: 6px 9px; min-height: 20px; }
QComboBox:hover, QLineEdit:hover { border-color: @muted@; }
QComboBox:focus, QLineEdit:focus { border-color: #3ea6ff; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView { background: @panel@; border: 1px solid @border@; outline: 0; selection-background-color: @accent@; selection-color: @acctext@; }
QCheckBox { spacing: 9px; }
QCheckBox::indicator { width: 18px; height: 18px; border-radius: 5px; border: 1px solid @border@; background: @panel2@; }
QCheckBox::indicator:checked { background: @accent@; border-color: @accent@; }
QSlider::groove:horizontal { height: 4px; background: @border@; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent@; border-radius: 2px; }
QSlider::handle:horizontal { width: 16px; height: 16px; margin: -6px 0; background: @accent@; border-radius: 8px; }
QProgressBar { background: @panel2@; border: none; border-radius: 5px; height: 10px; }
QProgressBar::chunk { background: @accent@; border-radius: 5px; }
QListView { background: transparent; border: none; outline: 0; }
QListWidget { background: transparent; border: none; padding: 4px; }
QListWidget::item { border-radius: 8px; }
QListWidget::item:hover { background: @panel2@; }
QListWidget::item:selected { background: @hover@; color: @text@; }
QScrollBar:vertical { width: 10px; background: transparent; }
QScrollBar::handle:vertical { background: @hover@; border-radius: 5px; min-height: 30px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QTabWidget::pane { border: none; border-top: 1px solid @border@; border-radius: 0; background: @bg@; top: -1px; }
QTabBar::tab { background: transparent; border: none; padding: 10px 18px; color: @muted@; font-weight: 600; }
QTabBar::tab:hover { color: @text@; }
QTabBar::tab:selected { color: @text@; border-bottom: 3px solid @text@; }
QMenu { background: @panel2@; border: none; border-radius: 12px; padding: 8px 0; }
QMenu::item { padding: 8px 36px 8px 16px; }
QMenu::item:selected { background: @hover@; }
QMenu::separator { height: 1px; background: @border@; margin: 6px 0; }
QToolTip { background: #616161; color: #ffffff; border: none; border-radius: 4px; padding: 6px 8px; }
QMessageBox { background: @bg@; }
)";
    s.replace("@bg@", p.bg.name()).replace("@panel@", p.panel.name()).replace("@panel2@", p.panel2.name())
        .replace("@border@", p.border.name()).replace("@text@", p.text.name()).replace("@muted@", p.muted.name())
        .replace("@accent2@", p.accent.lighter(112).name()).replace("@accent@", p.accent.name())
        .replace("@acctext@", p.accentText.name()).replace("@hover@", p.hover.name());
    return s;
}

// ---------------------------------------------------------------- Logos
// Clipline: Koralle -> Violett, weißer Rückspul-Kreis mit Aufnahmepunkt
QPixmap cliplineLogo(int size, bool recording, bool paused) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 100.0, size / 100.0);
    QLinearGradient g(0, 0, 100, 100);
    if (paused) {
        g.setColorAt(0, QColor("#8b90a0"));
        g.setColorAt(1, QColor("#565b6b"));
    } else {
        g.setColorAt(0, QColor("#ff5f6d"));
        g.setColorAt(1, QColor("#8b5cf6"));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(QRectF(4, 4, 92, 92), 24, 24);
    // Glanz
    QLinearGradient gl(0, 4, 0, 50);
    gl.setColorAt(0, QColor(255, 255, 255, 60));
    gl.setColorAt(1, QColor(255, 255, 255, 0));
    p.setBrush(gl);
    p.drawRoundedRect(QRectF(4, 4, 92, 46), 24, 24);
    // Rückspul-Bogen mit Pfeil
    QPen pen(Qt::white, 8.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF arc(24, 24, 52, 52);
    p.drawArc(arc, 120 * 16, 290 * 16);  // offen oben links
    // Pfeilspitze am Bogenanfang (120°), zeigt im Uhrzeigersinn in die Lücke
    const double a = 120 * 3.14159265 / 180;
    const QPointF P(50 + 26 * std::cos(a), 50 - 26 * std::sin(a));
    const QPointF dir(std::sin(a), std::cos(a)), nrm(std::cos(a), -std::sin(a));
    QPainterPath head;
    head.moveTo(P + dir * 12);
    head.lineTo(P + nrm * 10 - dir * 3);
    head.lineTo(P - nrm * 10 - dir * 3);
    head.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawPath(head);
    // Aufnahmepunkt
    p.setBrush(recording ? QColor("#ffffff") : QColor(255, 255, 255, 235));
    p.drawEllipse(QPointF(50, 50), 11, 11);
    if (recording) {
        p.setBrush(QColor("#ff2d55"));
        p.drawEllipse(QPointF(50, 50), 7, 7);
    }
    return pm;
}

QIcon cliplineIcon() {
    QIcon ic;
    for (int s : {16, 24, 32, 48, 64, 128, 256}) ic.addPixmap(cliplineLogo(s));
    return ic;
}

// ---------------------------------------------------------------- UI-Symbole
QIcon icon(Ic id, const QColor& c) {
    const int S = 64;
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(S / 100.0, S / 100.0);
    QPen pen(c, 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    auto fill = [&] { p.setPen(Qt::NoPen); p.setBrush(c); };
    switch (id) {
    case Ic::Record:
        p.drawEllipse(QPointF(50, 50), 36, 36);
        fill();
        p.drawEllipse(QPointF(50, 50), 19, 19);
        break;
    case Ic::Folder: {
        QPainterPath f;
        f.moveTo(10, 30); f.lineTo(10, 78); f.quadTo(10, 84, 16, 84); f.lineTo(84, 84); f.quadTo(90, 84, 90, 78);
        f.lineTo(90, 38); f.quadTo(90, 32, 84, 32); f.lineTo(46, 32); f.lineTo(38, 20); f.lineTo(16, 20);
        f.quadTo(10, 20, 10, 26); f.closeSubpath();
        p.drawPath(f);
        break;
    }
    case Ic::Gear:
        p.drawEllipse(QPointF(50, 50), 25, 25);
        p.drawEllipse(QPointF(50, 50), 9, 9);
        for (int i = 0; i < 8; ++i) {
            p.save(); p.translate(50, 50); p.rotate(i * 45); p.drawLine(0, -25, 0, -39); p.restore();
        }
        break;
    case Ic::Play: {
        fill();
        QPainterPath t; t.moveTo(30, 16); t.lineTo(84, 50); t.lineTo(30, 84); t.closeSubpath();
        p.drawPath(t);
        break;
    }
    case Ic::Edit:
        p.drawLine(24, 76, 34, 52); p.drawLine(34, 52, 68, 18); p.drawLine(68, 18, 82, 32);
        p.drawLine(82, 32, 48, 66); p.drawLine(48, 66, 24, 76);
        break;
    case Ic::Reveal:
        p.drawRoundedRect(QRectF(12, 20, 58, 62), 8, 8);
        p.drawLine(46, 54, 88, 12); p.drawLine(88, 12, 64, 12); p.drawLine(88, 12, 88, 36);
        break;
    case Ic::Trash: {
        p.drawLine(16, 28, 84, 28); p.drawLine(38, 28, 38, 16); p.drawLine(38, 16, 62, 16); p.drawLine(62, 16, 62, 28);
        QPainterPath b; b.moveTo(24, 28); b.lineTo(29, 84); b.lineTo(71, 84); b.lineTo(76, 28);
        p.drawPath(b);
        break;
    }
    case Ic::Rename:
        p.drawLine(20, 80, 80, 80); p.drawLine(30, 64, 64, 20); p.drawLine(64, 20, 76, 30); p.drawLine(76, 30, 42, 74);
        p.drawLine(42, 74, 28, 76); p.drawLine(28, 76, 30, 64);
        break;
    case Ic::Pause:
        fill();
        p.drawRoundedRect(QRectF(24, 16, 18, 68), 5, 5);
        p.drawRoundedRect(QRectF(58, 16, 18, 68), 5, 5);
        break;
    case Ic::Check:
        p.setPen(QPen(c, 14, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(22, 52, 42, 72); p.drawLine(42, 72, 80, 30);
        break;
    }
    p.end();
    return QIcon(pm);
}
