#pragma once
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include "config.h"
#include "platform.h"

// Knopf, der beim Anklicken die nächste Tastenkombination aufnimmt (läuft mit jeder Qt-6-Version)
class HotkeyButton : public QPushButton {
    Q_OBJECT
public:
    explicit HotkeyButton(const QString& seq, QWidget* parent = nullptr);
    QString sequence() const { return seq_; }
    void setSequence(const QString& seq);  // z. B. leer = Taste entfernen

signals:
    void sequenceChanged(const QString& seq);

protected:
    bool event(QEvent* e) override;  // beim Aufnehmen bekommt der Knopf jede Taste (auch Tab und Kürzel des Fensters)
    void keyPressEvent(QKeyEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;  // Shift+F10 und Menütaste (Windows)
    void focusOutEvent(QFocusEvent* e) override;

private:
    void startRecording();
    void stopRecording();
    void updateText();
    void take(Qt::KeyboardModifiers mods, int key);
    QString seq_;
    bool recording_ = false;
    QTimer hintTimer_;  // kurzer Hinweis im Knopf ("nur mit Strg, Alt oder Shift")
};

// Zeigt ein echtes Foto genau in der gewählten Aufnahmeauflösung: Das Foto wird auf diese Auflösung
// heruntergerechnet und dann wie im Vollbild auf einem 1080p-Monitor angezeigt (Ausschnitt, geglättet).
// Pfeile links/rechts wechseln das Foto.
class QualityPreview : public QWidget {
public:
    explicit QualityPreview(QWidget* parent = nullptr);
    void setCaptureHeight(int h, int nativeH);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent*) override;

private:
    QRectF imageRect() const;
    QRectF arrowRect(int dir) const;
    void ensureImage();

    int height_ = 1080, native_ = 1080, index_ = 0, hover_ = 0;
    QImage cached_;
    int cachedH_ = -1, cachedIdx_ = -1;
};

// Live-Pegel des Mikrofons mit der Schwelle der Rauschsperre: grün = kommt in den Clip,
// grau = wird als Hintergrundgeräusch stummgeschaltet
class MicMeter : public QWidget {
public:
    explicit MicMeter(QWidget* parent = nullptr);
    void setDevice(const QString& mic);
    void setThreshold(int db);
    void setAccent(const QColor& c) { accent_ = c; update(); }
    void setDenoise(bool on) { meter_.setDenoise(on); }

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    MicLevelMeter meter_;
    QTimer timer_;
    QString device_;
    int threshold_ = -45;
    QColor accent_ = QColor("#ff4d6d");
};

// Die vier Einstellungsseiten (Allgemein, Ton, Clip-Tasten, Aussehen). Werden vom Ersteinrichtungs-Assistenten
// nacheinander und im Einstellungsfenster als Tabs benutzt.
class ConfigPages : public QObject {
    Q_OBJECT
public:
    ConfigPages(const Config& c, QObject* parent);
    QWidget* folderPage(bool welcome);
    QWidget* soundPage();
    QWidget* buttonsPage();
    QWidget* lookPage();
    Config config() const { return cfg_; }

signals:
    void changed();
    void lookChanged();  // Akzent/Modus live anwenden

private:
    void updateRam();
    QSize nativeSize(const QString& monitor) const;
    ClipButton button(int i) const;              // 0 = Clip-Knopf 1
    void setButton(int i, const ClipButton& b);
    void rebuildButtons();     // Karten aller Clip-Knöpfe neu aufbauen
    QWidget* buttonCard(int i);
    void checkDuplicates();    // zwei Knöpfe mit derselben Taste?

    Config cfg_;
    QVBoxLayout* buttonsBox_ = nullptr;
    QLabel* dupWarn_ = nullptr;
    QLabel* multiNote_ = nullptr;  // Hinweis, wenn es mehrere Knöpfe gibt
    QList<Monitor> monitors_;
    QLabel *ramLabel_ = nullptr, *ramDetail_ = nullptr, *diskLabel_ = nullptr, *diskTitle_ = nullptr, *previewTitle_ = nullptr;
    QProgressBar* ramBar_ = nullptr;
    QualityPreview* preview_ = nullptr;
    int previewIdx_ = 0;  // Vorschau und Clip-Größe zeigen diesen Knopf (zuletzt bearbeitet)
};

class SetupWizard : public QDialog {
    Q_OBJECT
public:
    explicit SetupWizard(const Config& c, QWidget* parent = nullptr);
    Config config() const { return pages_->config(); }
    void go(int step);

private:
    ConfigPages* pages_;
    QStackedWidget* stack_;
    QList<QLabel*> steps_;
    QPushButton *back_, *next_;
    int step_ = 0;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const Config& c, QWidget* parent = nullptr);
    Config config() const { return pages_->config(); }

private:
    ConfigPages* pages_;
};

void applyLook(const Config& c);
