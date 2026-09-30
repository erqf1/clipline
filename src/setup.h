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

// Zeigt ein detailreiches Bild so, wie es bei der gewählten Aufnahmehöhe aussieht
class QualityPreview : public QWidget {
public:
    explicit QualityPreview(QWidget* parent = nullptr);
    void setCaptureHeight(int h, int nativeH);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    int height_ = 1080, native_ = 1080;
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
    QLabel *ramLabel_ = nullptr, *ramDetail_ = nullptr, *lenLabel_ = nullptr;
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
const QImage& detailScene();  // 1920×1080, für Qualitätsvorschau (App + Website)
