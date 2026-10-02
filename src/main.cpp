#include <QtGlobal>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#endif
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
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

// Erststart: Sprache wählen (wie in Cutline)
static QString askLanguage() {
    QDialog dlg;
    dlg.setWindowTitle("Clipline");
    dlg.setWindowIcon(cliplineIcon());
    dlg.resize(380, 520);
    auto* v = new QVBoxLayout(&dlg);
    v->setContentsMargins(22, 22, 22, 22);
    v->setSpacing(12);
    auto* logo = new QLabel;
    logo->setPixmap(cliplineLogo(48));
    v->addWidget(logo);
    auto* title = new QLabel("Choose your language · Sprache wählen");
    title->setObjectName("h2");
    v->addWidget(title);
    auto* list = new QListWidget;
    list->setStyleSheet("QListWidget { font-size: 15px; } QListWidget::item { padding: 8px 10px; }");
    list->addItems(languageNames());
    int cur = languageCodes().indexOf(language());
    list->setCurrentRow(cur < 0 ? 0 : cur);
    v->addWidget(list, 1);
    auto* ok = new QPushButton("OK");
    ok->setObjectName("primary");
    ok->setDefault(true);
    v->addWidget(ok);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted) return QString();
    return languageCodes().value(list->currentRow(), "en");
}

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
    cfg.height = qEnvironmentVariableIntValue("CLIPLINE_TESTRES") > 0 ? qEnvironmentVariableIntValue("CLIPLINE_TESTRES") : cfg.height;
    cfg.mic = qEnvironmentVariable("CLIPLINE_TESTMIC");
    if (qEnvironmentVariableIntValue("CLIPLINE_TESTCLIP") > 0) cfg.clipSeconds = qEnvironmentVariableIntValue("CLIPLINE_TESTCLIP");
    setLanguage(cfg.language);
    applyLook(cfg);
    say("ffmpeg: " + ffmpegPath());
    say("encoder: " + Recorder::detectEncoder(true));
    for (const Monitor& m : listMonitors())
        say(QString("monitor %1 dda=%2 rect=%3,%4 %5x%6").arg(m.name).arg(m.ddaIndex).arg(m.rect.x()).arg(m.rect.y())
                .arg(m.rect.width()).arg(m.rect.height()));
    say("mics: " + listMicrophones().join(" | "));
    {
        SetupWizard wiz(cfg);
        auto* stack = wiz.findChild<QStackedWidget*>();
        for (int i = 0; i < 4; ++i) {
            wiz.go(i);
            shot(&wiz, QString("wizard%1").arg(i + 1));
        }
        Q_UNUSED(stack);
    }
    {
        SettingsDialog dlg(cfg);
        auto* tabs = dlg.findChild<QTabWidget*>();
        tabs->setCurrentIndex(1);
        shot(&dlg, "settings");
        tabs->setCurrentIndex(2);
        shot(&dlg, "settings_sound");
    }
    Controller::noRecord = secs <= 0;
    Controller ctl(cfg);
    bool done = secs <= 0, ok = secs <= 0;
    QString clip;
    QObject::connect(&ctl, &Controller::clipDone, [&](const QString& p, bool o) { done = true; ok = o; clip = p; });
    QObject::connect(&ctl.recorder(), &Recorder::clipSaved, [&](const QString&, bool o, const QString& err) {
        if (!o) say("save error: " + err);
    });
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
    // Erster Clip als geöffnet markiert (Schild auf der Kachel), danach wieder zurück
    const QFileInfoList clips = QDir(cfg.clipsDir).entryInfoList({"*.mp4"}, QDir::Files, QDir::Time);
    if (!clips.isEmpty()) g->setOpened(clips.first().absoluteFilePath(), true);
    QElapsedTimer w;
    w.start();
    while (w.elapsed() < 4000) QApplication::processEvents(QEventLoop::AllEvents, 50);
    g->grab().save(dir + "/gallery.png");
    if (!clips.isEmpty()) g->setOpened(clips.first().absoluteFilePath(), false);
    say("gallery saved");
    ctl.quit();
    say("quit done");
    return ok ? 0 : 2;
}

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // Eigene App-ID: eigener Taskleisten-Button, nicht mit anderen Programmen gruppiert
    SetCurrentProcessExplicitAppUserModelID(L"WSoftware.Clipline");
#endif
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
    // Entwickler-Test: Mikrofon-Kette auf eine Datei anwenden (--micfile in.raw out.raw gate denoise)
    if (const int mf = args.indexOf("--micfile"); mf >= 0 && mf + 4 < args.size()) {
        MicTuning t;
        t.gateDb = args[mf + 3].toInt();
        t.denoise = args[mf + 4] == "1";
        return processMicFile(args[mf + 1], args[mf + 2], t) ? 0 : 1;
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
        if (cfg.language.isEmpty()) {
            const QString code = askLanguage();
            if (code.isEmpty()) return 0;
            cfg.language = code;
            setLanguage(code);
        }
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
