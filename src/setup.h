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
#include "config.h"
#include "platform.h"

// Knopf, der beim Anklicken die nächste Tastenkombination aufnimmt (läuft mit jeder Qt-6-Version)
class HotkeyButton : public QPushButton {
    Q_OBJECT
public:
    explicit HotkeyButton(const QString& seq, QWidget* parent = nullptr);
    QString sequence() const { return seq_; }

signals:
    void sequenceChanged(const QString& seq);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void startRecording();
    void stopRecording();
    void updateText();
    QString seq_;
    bool recording_ = false;
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

// Die vier Einstellungsseiten. Werden vom Ersteinrichtungs-Assistenten nacheinander
// und im Einstellungsfenster als Tabs benutzt.
class ConfigPages : public QObject {
    Q_OBJECT
public:
    ConfigPages(const Config& c, QObject* parent);
    QWidget* folderPage(bool welcome);
    QWidget* recordingPage();
    QWidget* hotkeyPage();
    QWidget* lookPage();
    Config config() const { return cfg_; }

signals:
    void changed();
    void lookChanged();  // Akzent/Modus live anwenden

private:
    void updateRam();
    QSize nativeSize() const;

    Config cfg_;
    QList<Monitor> monitors_;
    QLabel *ramLabel_ = nullptr, *ramDetail_ = nullptr, *lenLabel_ = nullptr, *diskLabel_ = nullptr;
    QProgressBar* ramBar_ = nullptr;
    QComboBox *res_ = nullptr, *fps_ = nullptr;
    QualityPreview* preview_ = nullptr;
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
