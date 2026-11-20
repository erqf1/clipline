// Bilder für die Website: rendert die echten Fenster unsichtbar (QT_QPA_PLATFORM=offscreen) in PNGs.
// Aufruf: clipline --siteshots <ordner>   (Sprache über CLIPLINE_LANG, Standard Deutsch)
// tools/siteshots.sh setzt zwei Bildschirme, startet das und wandelt die Bilder in WebP für mert.win/clip um.
#include "siteshots.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTabWidget>
#include "config.h"
#include "controller.h"
#include "gallery.h"
#include "i18n.h"
#include "platform.h"
#include "setup.h"

namespace {
const char* kShownDir = "C:/Users/You/Videos/Clipline";  // Beispielpfad statt des Testordners

void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) QApplication::processEvents(QEventLoop::AllEvents, 20);
}

void prepare(QWidget* w) {
    w->setAttribute(Qt::WA_DontShowOnScreen);
    w->show();
    pump(150);
}

// Beispiel-Clips aus den eingebauten Fotos (Ausschnitt, kurz, als Standbild) mit passenden Zeitstempeln
bool makeClips(const QString& dir, QString* log) {
    QDir().mkpath(dir);
    struct Clip {
        const char* photo;
        int x, y, w;  // Ausschnitt im 3840×2160-Foto (16:9)
        int secs;
        int minutesAgo;
    };
    const Clip clips[] = {
        {"kingfisher", 0, 0, 3840, 60, 12},     {"hedgehog", 400, 200, 3040, 60, 95},
        {"nest", 0, 0, 3840, 30, 60 * 26},      {"kingfisher", 1100, 450, 2200, 45, 60 * 27},
        {"hedgehog", 0, 0, 3840, 60, 60 * 50},  {"nest", 900, 300, 2400, 60, 60 * 74},
    };
    const QDateTime base(QDate(2026, 11, 18), QTime(21, 42, 10));
    for (const Clip& c : clips) {
        const QString src = QDir(dir).filePath(QString("%1.jpg").arg(c.photo));
        if (!QFile::exists(src)) QFile::copy(QString(":/photos/%1.jpg").arg(c.photo), src);
        const QDateTime when = base.addSecs(-60 * c.minutesAgo);
        const QString out = QDir(dir).filePath("Clip " + when.toString("yyyy-MM-dd HH-mm-ss") + ".mp4");
        const int h = c.w * 9 / 16;
        QProcess p;
        p.start(ffmpegPath(), {"-hide_banner", "-loglevel", "error", "-y", "-loop", "1", "-framerate", "1", "-t",
                               QString::number(c.secs), "-i", src, "-vf",
                               QString("crop=%1:%2:%3:%4,scale=1280:720").arg(c.w).arg(h).arg(c.x).arg(c.y), "-c:v",
                               "libx264", "-tune", "stillimage", "-pix_fmt", "yuv420p", "-r", "1", "-movflags", "+faststart", out});
        if (!p.waitForFinished(60000) || p.exitCode() != 0) {
            *log += "ffmpeg failed: " + QString::fromUtf8(p.readAllStandardError()) + "\n";
            return false;
        }
        // Echte Clips (1080p, 60 fps, Ausgewogen) sind etwa 1,5 MB pro Sekunde: Datei auf diese Größe bringen
        // (Nullen am Ende, ohne Platz auf der Platte), damit die Liste realistische Größen zeigt
        QFile f(out);
        f.open(QIODevice::ReadWrite);
        f.resize(qint64(c.secs) * (1450 + 37 * (c.minutesAgo % 7)) * 1024);
        f.setFileTime(when, QFileDevice::FileModificationTime);
    }
    for (const char* photo : {"kingfisher", "hedgehog", "nest"}) QFile::remove(QDir(dir).filePath(QString("%1.jpg").arg(photo)));
    return true;
}

QScrollArea* pageScroll(QTabWidget* tabs) {
    QWidget* w = tabs->currentWidget();
    auto* s = qobject_cast<QScrollArea*>(w);
    return s ? s : w->findChild<QScrollArea*>();
}

// Aufgeklappte Bildschirmwahl: Das Popup ist ein eigenes Fenster, also beide Bilder zusammensetzen
QImage withPopup(QWidget* win, QComboBox* combo) {
    combo->showPopup();
    pump(250);
    QAbstractItemView* view = combo->view();
    QImage img = win->grab().toImage();
    if (view->window() != win) {
        const QPoint at = view->mapToGlobal(QPoint(0, 0)) - win->mapToGlobal(QPoint(0, 0));
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        const QRect r(at, view->size());
        QPainterPath clip;
        clip.addRoundedRect(QRectF(r), 8, 8);
        p.setClipPath(clip);
        p.drawPixmap(at, view->grab());
        p.setClipping(false);
        p.setPen(QPen(win->palette().color(QPalette::Mid), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    }
    combo->hidePopup();
    pump(100);
    return img;
}
}  // namespace

int siteShots(const QString& outDir) {
    QDir().mkpath(outDir);
    QFile logFile(outDir + "/siteshots.txt");
    logFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QString log;
    auto say = [&](const QString& s) { logFile.write((s + "\n").toUtf8()); logFile.flush(); };

    const QString lang = qEnvironmentVariable("CLIPLINE_LANG", "de");
    setLanguage(lang);
    QLocale::setDefault(QLocale(lang));  // Datum in der Clip-Liste wie auf einem Rechner in dieser Sprache
    const bool de = lang == "de";
    SiteShots::mics = de ? QStringList{"Mikrofon (USB Audio Device)", "Headset-Mikrofon (Wireless Headset)"}
                         : QStringList{"Microphone (USB Audio Device)", "Headset Microphone (Wireless Headset)"};
    SiteShots::micLevelDb = -23;  // spricht gerade: grüner Balken über der Schwelle

    const QString clipsDir = QDir(outDir).filePath("clips");
    if (!makeClips(clipsDir, &log)) {
        say(log);
        return 1;
    }
    const QList<Monitor> monitors = listMonitors();
    say(QString("monitors: %1").arg(monitors.size()));

    Config cfg;
    cfg.clipsDir = clipsDir;
    cfg.firstRunDone = true;
    cfg.language = lang;
    cfg.autostart = cfg.sound = true;  // nur Anzeige - hier wird nichts eingerichtet
    cfg.clipSeconds = 60;
    cfg.mic = SiteShots::mics.first();
    // Zweiter Clip-Knopf auf dem zweiten Bildschirm: zeigt Bildschirmwahl und mehrere Tasten
    cfg.extraButtons << ClipButton{"F10", 30, 720, 60, 1, monitors.size() > 1 ? monitors[1].id : QString()};

    Controller::quiet = true;
    Controller::noRecord = true;

    struct Style {
        const char* name;
        bool minimal, acryl;
    };
    const Style styles[] = {{"standard", false, false}, {"minimal", true, false}, {"acryl", false, true}};
    int count = 0;
    auto save = [&](const QImage& img, const QString& name) {
        const bool ok = img.save(QDir(outDir).filePath(name + ".png"));
        say(QString("%1 %2 %3x%4").arg(ok ? "saved" : "FAILED", name).arg(img.width()).arg(img.height()));
        count += ok;
    };

    // Ein Satz Bilder für Stil, Modus und Akzentfarbe; "full" = auch Detailbilder (Mikrofon, Tasten, Einrichtung)
    auto shoot = [&](const Style& st, bool dark, const QString& accent, const QString& pre, bool full) {
        Config c = cfg;
        c.minimal = st.minimal;
        c.acryl = st.acryl;
        c.dark = dark;
        c.accent = accent;
        applyLook(c);

        // Clip-Liste
        {
            Controller ctl(c);
            ctl.showGallery();
            GalleryWindow* g = ctl.gallery();
            g->setAttribute(Qt::WA_DontShowOnScreen);
            g->resize(1060, 700);
            pump(3500);  // Vorschaubilder holt ffmpeg im Hintergrund
            for (QLabel* l : g->findChildren<QLabel*>())
                if (l->text() == QDir::toNativeSeparators(c.clipsDir)) l->setText(QDir::toNativeSeparators(kShownDir));
            save(g->grab().toImage(), pre + "clips");
        }

        // Einstellungen: alle vier Seiten
        Config shown = c;
        shown.clipsDir = kShownDir;
        SettingsDialog dlg(shown);
        dlg.resize(780, 760);
        prepare(&dlg);
        auto* tabs = dlg.findChild<QTabWidget*>();
        const char* names[] = {"settings-general", "settings-sound", "settings-keys", "settings-look"};
        for (int i = 0; i < 4; ++i) {
            tabs->setCurrentIndex(i);
            pump(150);
            save(dlg.grab().toImage(), pre + names[i]);
        }
        if (!full) return;

        // Mikrofon: Pegel und Test ("Sprich jetzt…"), dafür bis zum Mikrofon-Kasten scrollen
        tabs->setCurrentIndex(1);
        pump(100);
        if (QScrollArea* sa = pageScroll(tabs)) {
            sa->verticalScrollBar()->setValue(sa->verticalScrollBar()->maximum());
            pump(100);
            for (QPushButton* b : tabs->currentWidget()->findChildren<QPushButton*>())
                if (b->text() == L("Test microphone")) {
                    b->setText(L("Speak now… %1").arg(3));
                    b->setEnabled(false);
                }
            pump(100);
            save(dlg.grab().toImage(), pre + "mic-test");
        }

        // Clip-Tasten: Bildschirmwahl aufgeklappt und Taste wird gerade aufgenommen
        tabs->setCurrentIndex(2);
        pump(100);
        QComboBox* screen = nullptr;
        for (QComboBox* cb : tabs->currentWidget()->findChildren<QComboBox*>())
            if (cb->isVisible() && monitors.size() > 1 && cb->findData(monitors[0].id) >= 0) {
                screen = cb;
                break;
            }
        if (screen) save(withPopup(&dlg, screen), pre + "screen-select");
        else say("no screen selection (only one screen?)");
        for (QPushButton* b : tabs->currentWidget()->findChildren<QPushButton*>())
            if (b->objectName() == "hotkey" || b->inherits("HotkeyButton")) {
                b->click();
                pump(100);
                save(dlg.grab().toImage(), pre + "key-record");
                break;
            }

        // Ersteinrichtung
        Config fresh = shown;
        fresh.firstRunDone = false;
        SetupWizard wiz(fresh);
        prepare(&wiz);
        for (int i = 0; i < 4; ++i) {
            wiz.go(i);
            pump(120);
            save(wiz.grab().toImage(), pre + QString("setup-%1").arg(i + 1));
        }
    };
    for (const Style& st : styles)
        for (const bool dark : {true, false})
            shoot(st, dark, cfg.accent, QString("%1-%2-").arg(st.name, dark ? "dark" : "light"), !st.minimal && !st.acryl);
    // Andere Akzentfarben (Blau, Violett, Türkis)
    shoot(styles[2], true, "#8b5cf6", "accent-violet-acryl-dark-", false);
    shoot(styles[0], false, "#3b82f6", "accent-blue-standard-light-", false);
    shoot(styles[1], true, "#06b6d4", "accent-cyan-minimal-dark-", false);
    say(QString("done: %1 images").arg(count));
    return count > 0 ? 0 : 1;
}
