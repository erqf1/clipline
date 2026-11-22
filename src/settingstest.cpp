// Entwickler-Test: Einstellungen immer wieder öffnen, zufällig ändern und speichern/abbrechen - mit echter
// Aufnahme, Hotkeys und Tray (wie beim Nutzer). Findet Abstürze beim Ändern der Einstellungen im Nachhinein.
// Aufruf: clipline --settingstest <ordner> [runden] [seed]   (Protokoll: <ordner>/settingstest.txt)
#include "settingstest.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <random>
#include "controller.h"
#include "gallery.h"
#include "i18n.h"
#include "setup.h"

namespace {
void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) QApplication::processEvents(QEventLoop::AllEvents, 10);
}

SettingsDialog* openDialog() {
    for (QWidget* w : QApplication::topLevelWidgets())
        if (auto* d = qobject_cast<SettingsDialog*>(w); d && d->isVisible()) return d;
    return nullptr;
}
}  // namespace

int settingsTest(const QString& dir, int rounds, unsigned seed) {
    QDir().mkpath(dir);
    QFile log(dir + "/settingstest.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    auto say = [&log](const QString& s) { log.write((s + "\n").toUtf8()); log.flush(); };
    std::mt19937 rng(seed);
    auto chance = [&](int percent) { return int(rng() % 100) < percent; };

    Config cfg = Config::load();
    cfg.firstRunDone = true;
    setLanguage(cfg.language.isEmpty() ? "en" : cfg.language);
    applyLook(cfg);
    Controller ctl(cfg);
    ctl.listen();
    ctl.showGallery();
    pump(3000);  // Aufnahme anlaufen lassen

    // Eine zufällige Änderung auf der sichtbaren Seite. Nur Bedienelemente ohne eigenes Fenster (keine Ordner-/
    // Farbauswahl, kein Mikrofon-Test, keine Tastenaufnahme).
    auto fiddle = [&](SettingsDialog* dlg, QString* what) {
        auto* tabs = dlg->findChild<QTabWidget*>();
        QWidget* page = tabs->currentWidget();
        QList<QWidget*> cands;
        for (QWidget* w : page->findChildren<QWidget*>()) {
            if (!w->isVisible() || !w->isEnabled()) continue;
            if (qobject_cast<QComboBox*>(w) || qobject_cast<QSlider*>(w) || qobject_cast<QCheckBox*>(w)) cands << w;
            else if (auto* b = qobject_cast<QPushButton*>(w)) {
                const QString n = b->objectName();
                if (n == "seg" || n == "swatch" || n == "dashed" || n == "flat") cands << w;
            }
        }
        if (cands.isEmpty()) return;
        QWidget* w = cands[int(rng() % cands.size())];
        if (auto* c = qobject_cast<QComboBox*>(w)) {
            const int i = int(rng() % std::max(1, c->count()));
            *what = QString("combo -> %1").arg(c->itemText(i));
            c->setCurrentIndex(i);
        } else if (auto* s = qobject_cast<QSlider*>(w)) {
            const int v = s->minimum() + int(rng() % std::max(1, s->maximum() - s->minimum() + 1));
            *what = QString("slider -> %1").arg(v);
            s->setValue(v);
        } else if (auto* b = qobject_cast<QAbstractButton*>(w)) {
            *what = QString("click %1 '%2'").arg(b->objectName().isEmpty() ? "checkbox" : b->objectName(), b->text());
            if (b->objectName() == "swatch" && b->text().isEmpty() && b->toolTip().contains("…")) return;  // eigene Farbe
            b->click();
        }
    };

    int saved = 0, cancelled = 0;
    for (int r = 1; r <= rounds; ++r) {
        QTimer::singleShot(400, &ctl, [&, r] {
            SettingsDialog* dlg = openDialog();
            if (!dlg) {
                say(QString("round %1: settings window did not open").arg(r));
                return;
            }
            QPointer<SettingsDialog> guard(dlg);
            auto* tabs = dlg->findChild<QTabWidget*>();
            const int steps = 3 + int(rng() % 10);
            for (int k = 0; k < steps && guard; ++k) {
                if (chance(35)) tabs->setCurrentIndex(int(rng() % tabs->count()));
                pump(30);
                QString what;
                fiddle(dlg, &what);
                if (!what.isEmpty()) say(QString("round %1 step %2: tab %3, %4").arg(r).arg(k).arg(tabs->currentIndex()).arg(what));
                pump(60);  // gelöschte Karten (deleteLater) wirklich weg, Signale abgearbeitet
            }
            if (!guard) return;
            const bool save = chance(85);
            for (QPushButton* b : dlg->findChildren<QPushButton*>())
                if (save ? b->objectName() == "primary" : b->text() == L("Cancel")) {
                    say(QString("round %1: %2").arg(r).arg(save ? "save" : "cancel"));
                    b->click();
                    break;
                }
            save ? ++saved : ++cancelled;
        });
        ctl.openSettings();  // blockiert, bis gespeichert/abgebrochen
        pump(400 + int(rng() % 1200));  // Aufnahme startet neu, Galerie wird neu aufgebaut
        say(QString("round %1 done, gallery %2").arg(r).arg(ctl.gallery() && ctl.gallery()->isVisible() ? "visible" : "GONE"));
    }
    say(QString("settingstest finished: %1 rounds, %2 saved, %3 cancelled").arg(rounds).arg(saved).arg(cancelled));
    ctl.quit();
    return 0;
}
