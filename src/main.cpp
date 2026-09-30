#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTabWidget>
#include "gallery.h"
#include "platform.h"
#include <QLocalSocket>
#include <QElapsedTimer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include "config.h"
#include "controller.h"
#include "i18n.h"
#include "look.h"
#include "setup.h"

// Befehl an eine bereits laufende Instanz schicken; true = angekommen
static bool sendToRunning(const QByteArray& cmd) {
    QLocalSocket s;
    s.connectToServer(serverName());
    if (!s.waitForConnected(400)) return false;
    s.write(cmd);
    s.waitForBytesWritten(400);
    s.waitForDisconnected(400);
    return true;
}

// Entwickler-Selbsttest: rendert alle Fenster unsichtbar in PNGs, nimmt kurz auf und speichert einen Clip.
// Aufruf: Clipline --selftest <ordner> [sekunden]
static QString g_logPath;
static void logLine(const QString& s) {
    QFile f(g_logPath);
    if (f.open(QIODevice::Append)) f.write((s + QChar(10)).toUtf8());
}

static int selfTest(QApplication& app, const QString& dir, int secs, const QString& clipsDir) {
    QDir().mkpath(dir);
    g_logPath = dir + "/qt.txt";
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& m) { logLine(m); });
    QDir().mkpath(dir);
    QFile log(dir + "/selftest.txt");
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    auto say = [&log](const QString& s) { log.write((s + QChar(10)).toUtf8()); log.flush(); };
    auto shot = [&dir](QWidget* w, const QString& name) {
        w->setAttribute(Qt::WA_DontShowOnScreen);
        w->show();
        QApplication::processEvents();
        w->grab().save(dir + "/" + name + ".png");
    };
    Controller::quiet = true;
    Config cfg;
    cfg.clipsDir = clipsDir.isEmpty() ? dir + "/clips" : clipsDir;
    cfg.clipSeconds = secs > 0 ? 10 : 60;
    cfg.sound = false;
    cfg.autostart = false;
    cfg.firstRunDone = true;
    cfg.language = qEnvironmentVariable("CLIPLINE_LANG", "en");
    setLanguage(cfg.language);
    applyLook(cfg);
    say("ffmpeg: " + ffmpegPath());
    say("encoder: " + Recorder::detectEncoder(true));
    for (const Monitor& m : listMonitors())
        say(QString("monitor %1 dda=%2 rect=%3,%4 %5x%6").arg(m.name).arg(m.ddaIndex).arg(m.rect.x()).arg(m.rect.y())
                .arg(m.rect.width()).arg(m.rect.height()));
    say("mics: " + listMicrophones().join(" | "));
    say("cutline: " + findCutline());
    {
        SetupWizard wiz(cfg);
        auto* stack = wiz.findChild<QStackedWidget*>();
        for (int i = 0; i < 4; ++i) {
            stack->setCurrentIndex(i);
            shot(&wiz, QString("wizard%1").arg(i + 1));
        }
    }
    {
        SettingsDialog dlg(cfg);
        auto* tabs = dlg.findChild<QTabWidget*>();
        tabs->setCurrentIndex(2);
        shot(&dlg, "settings");
    }
    Controller::noRecord = secs <= 0;
    Controller ctl(cfg);
    bool done = secs <= 0, ok = secs <= 0;
    QString clip;
    QObject::connect(&ctl, &Controller::clipDone, [&](const QString& p, bool o) { done = true; ok = o; clip = p; });
    QTimer::singleShot(secs * 1000, &ctl, &Controller::saveClip);
    QTimer::singleShot(secs * 1000 + 20000, &app, [&] { done = true; });
    QElapsedTimer t;
    t.start();
    qint64 peakKB = 0;
    while (!done) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
        if (t.elapsed() > (secs - 2) * 1000 && peakKB == 0) {
            say(QString("buffered: %1 s, %2 MB, running=%3").arg(ctl.recorder().bufferedSeconds(), 0, 'f', 1)
                    .arg(ctl.recorder().bufferedBytes() / 1048576.0, 0, 'f', 1).arg(ctl.recorder().running()));
            peakKB = 1;
        }
    }
    say(QString("clip ok=%1 path=%2 size=%3").arg(ok).arg(clip).arg(QFileInfo(clip).size()));
    ctl.showGallery();
    GalleryWindow* g = ctl.gallery();
    g->setAttribute(Qt::WA_DontShowOnScreen);
    g->resize(1060, 700);
    QElapsedTimer w;
    w.start();
    while (w.elapsed() < 4000) QApplication::processEvents(QEventLoop::AllEvents, 50);
    g->grab().save(dir + "/gallery.png");
    say("gallery saved");
    ctl.quit();
    say("quit done");
    return ok ? 0 : 2;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("WSoftware");
    QCoreApplication::setApplicationName("Clipline");
    QGuiApplication::setApplicationDisplayName("Clipline");
    app.setWindowIcon(cliplineIcon());
    app.setQuitOnLastWindowClosed(false);

    const QStringList args = app.arguments();
    const int st = args.indexOf("--selftest");
    if (st >= 0 && st + 1 < args.size()) {
        const int rc = selfTest(app, args[st + 1], st + 2 < args.size() ? args[st + 2].toInt() : 15,
                               st + 3 < args.size() ? args[st + 3] : QString());
        QFile f(args[st + 1] + "/selftest.txt");
        f.open(QIODevice::Append);
        f.write("returned from selftest");
        f.close();
        return rc;
    }
    const bool background = args.contains("--background");
    const bool save = args.contains("--save");

    // Nur eine Instanz: weitere Starts reichen ihren Wunsch weiter
    if (sendToRunning(save ? "save" : background ? "noop" : "show")) return 0;
    if (save) return 1;  // nichts läuft, das speichern könnte

    Config cfg = Config::load();
    setLanguage(cfg.language);
    applyLook(cfg);

    if (!cfg.firstRunDone) {
        SetupWizard wizard(cfg);
        if (wizard.exec() != QDialog::Accepted) return 0;
        cfg = wizard.config();
        cfg.firstRunDone = true;
    }

    Controller ctl(cfg);
    ctl.listen();
    if (!background) QTimer::singleShot(0, &ctl, &Controller::showGallery);
    return app.exec();
}
