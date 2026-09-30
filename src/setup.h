#pragma once
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QSlider>
#include <QStackedWidget>
#include "config.h"
#include "platform.h"

// Die vier Einstellungsseiten. Werden vom Ersteinrichtungs-Assistenten nacheinander
// und im Einstellungsfenster als Tabs benutzt.
class ConfigPages : public QObject {
    Q_OBJECT
public:
    ConfigPages(const Config& c, QObject* parent);
    QWidget* folderPage();
    QWidget* lengthPage();
    QWidget* qualityPage();
    QWidget* lookPage();
    Config config() const { return cfg_; }

signals:
    void changed();
    void lookChanged();  // Akzent/Modus live anwenden

private:
    QWidget* ramBox();
    void updateRam();
    QSize nativeSize() const;

    Config cfg_;
    QList<Monitor> monitors_;
    QList<QLabel*> ramLabels_, ramDetail_;
    QList<QProgressBar*> ramBars_;
    QLabel* lenLabel_ = nullptr;
    QComboBox *res_ = nullptr, *fps_ = nullptr;
};

class SetupWizard : public QDialog {
    Q_OBJECT
public:
    explicit SetupWizard(const Config& c, QWidget* parent = nullptr);
    Config config() const { return pages_->config(); }

private:
    void go(int step);
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
