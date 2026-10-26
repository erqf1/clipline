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
#include <QSysInfo>
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
#ifdef Q_OS_LINUX
#include "wayland_hotkeys.h"
#endif
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QListView>
#include <QScrollBar>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
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
    cfg.minimal = qEnvironmentVariableIsSet("CLIPLINE_TESTMINIMAL");
    const bool siteShots = qEnvironmentVariableIsSet("CLIPLINE_NOSPIN");  // Bilder für die Website
    if (siteShots) cfg.autostart = cfg.sound = true;  // nur Anzeige - im Selbsttest wird nichts eingerichtet
    cfg.height = qEnvironmentVariableIntValue("CLIPLINE_TESTRES") > 0 ? qEnvironmentVariableIntValue("CLIPLINE_TESTRES") : cfg.height;
    cfg.mic = qEnvironmentVariable("CLIPLINE_TESTMIC");
    if (qEnvironmentVariableIntValue("CLIPLINE_TESTCLIP") > 0) cfg.clipSeconds = qEnvironmentVariableIntValue("CLIPLINE_TESTCLIP");
    // Zweiter Clip-Knopf (6 s, kleinere Auflösung): wird nach dem ersten Clip auch gespeichert
    const int extraH = qEnvironmentVariableIntValue("CLIPLINE_TESTBUTTON");
    if (extraH > 0) cfg.extraButtons << ClipButton{QString(), 6, extraH, 30, 0};
    setLanguage(cfg.language);
    applyLook(cfg);
    say("ffmpeg: " + ffmpegPath());
    say("encoder: " + Recorder::detectEncoder(true));
    for (const Monitor& m : listMonitors())
        say(QString("monitor %1 dda=%2 rect=%3,%4 %5x%6").arg(m.name).arg(m.ddaIndex).arg(m.rect.x()).arg(m.rect.y())
                .arg(m.rect.width()).arg(m.rect.height()));
    say("mics: " + listMicrophones().join(" | "));
    {
        Config shown = cfg;
        if (siteShots) shown.clipsDir = "C:/Users/You/Videos/Clipline";
        SetupWizard wiz(shown);
        auto* stack = wiz.findChild<QStackedWidget*>();
        for (int i = 0; i < 4; ++i) {
            wiz.go(i);
            shot(&wiz, QString("wizard%1").arg(i + 1));
        }
        Q_UNUSED(stack);
    }
    {
        Config shown = cfg;
        if (siteShots) shown.clipsDir = "C:/Users/You/Videos/Clipline";
        SettingsDialog dlg(shown);
        // CLIPLINE_TALL: hohes Fenster, damit jede Seite ganz aufs Bild passt
        if (qEnvironmentVariableIntValue("CLIPLINE_TALL") > 0) dlg.resize(780, qEnvironmentVariableIntValue("CLIPLINE_TALL"));
        auto* tabs = dlg.findChild<QTabWidget*>();
        const char* names[] = {"settings_general", "settings_sound", "settings", "settings_look"};
        for (int i : {2, 1, 0, 3}) {
            tabs->setCurrentIndex(i);
            shot(&dlg, names[i]);
        }
    }
    {
        // Tastenaufnahme: was im Feld landet (leer = abgelehnt)
        HotkeyButton hk("F9");
        hk.setAttribute(Qt::WA_DontShowOnScreen);
        hk.show();
        auto press = [&](int key, Qt::KeyboardModifiers mods) {
            hk.setSequence("F9");
            hk.click();
            QKeyEvent ev(QEvent::KeyPress, key, mods);
            QApplication::sendEvent(&hk, &ev);
            const QString got = hk.sequence() == "F9" && key != Qt::Key_F9 ? QString("(rejected)") : hk.sequence();
            hk.setSequence("F9");
            return got;
        };
        say("key Shift+F10: " + press(Qt::Key_F10, Qt::ShiftModifier));
        say("key Enter: " + press(Qt::Key_Return, Qt::NoModifier));
        say("key Ctrl+Enter: " + press(Qt::Key_Return, Qt::ControlModifier));
        say("key Shift+Backspace: " + press(Qt::Key_Backspace, Qt::ShiftModifier));
        say("key CapsLock: " + press(Qt::Key_CapsLock, Qt::NoModifier));
        say("key Ctrl+CapsLock: " + press(Qt::Key_CapsLock, Qt::ControlModifier));
        say("key Ctrl+Tab: " + press(Qt::Key_Tab, Qt::ControlModifier));
        say("key Shift+Tab: " + press(Qt::Key_Backtab, Qt::ShiftModifier));
        say("key Numpad 5: " + press(Qt::Key_5, Qt::KeypadModifier));
        hk.setSequence("F9");
        hk.click();
        QContextMenuEvent cm(QContextMenuEvent::Keyboard, QPoint(5, 5));
        QApplication::sendEvent(&hk, &cm);
        say("key context menu: " + hk.sequence());
        setReservedHotkeys({"F9", "Shift+F9"});
        QStringList tol;
        for (Qt::KeyboardModifiers m : tolerantModifiers(QKeySequence("F9")[0]))
            tol << QKeySequence(QKeyCombination(m, Qt::Key_F9)).toString(QKeySequence::PortableText);
        say("tolerant F9 (Shift+F9 reserved): " + tol.join(", "));
        setReservedHotkeys({});
    }
    Controller::noRecord = secs <= 0;
    Controller ctl(cfg);
    bool done = secs <= 0, ok = secs <= 0;
    QString clip;
    QObject::connect(&ctl, &Controller::clipDone, [&](const QString& p, bool o) { done = true; ok = o; clip = p; });
    QObject::connect(&ctl.recorder(), &Recorder::clipSaved, [&](const QString&, bool o, const QString& err) {
        if (!o) say("save error: " + err);
        else if (!err.isEmpty()) say("clip incomplete: " + err);
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
    if (!cfg.extraButtons.isEmpty() && secs > 0) {
        done = false;
        ctl.saveClipButton(1);
        QElapsedTimer t2;
        t2.start();
        while (!done && t2.elapsed() < 60000) QApplication::processEvents(QEventLoop::AllEvents, 50);
        say(QString("clip2 ok=%1 path=%2 size=%3").arg(ok).arg(clip).arg(QFileInfo(clip).size()));
    }
    say(QString("stall restarts: %1").arg(ctl.recorder().stallRestarts()));
    ctl.showGallery();
    GalleryWindow* g = ctl.gallery();
    g->setAttribute(Qt::WA_DontShowOnScreen);
    g->resize(1060, 700);
    QElapsedTimer w;
    w.start();
    while (w.elapsed() < 3600) QApplication::processEvents(QEventLoop::AllEvents, 50);
    // Ladekreis beim Öffnen (ohne wirklich einen Player zu starten)
    const QFileInfoList clips = QDir(cfg.clipsDir).entryInfoList({"*.mp4"}, QDir::Files, QDir::Time);
    if (!clips.isEmpty() && !qEnvironmentVariableIsSet("CLIPLINE_NOSPIN")) g->showOpening(clips.first().absoluteFilePath());
    while (w.elapsed() < 4000) QApplication::processEvents(QEventLoop::AllEvents, 50);
    if (siteShots)  // Beispielpfad statt des Testordners
        for (QLabel* l : g->findChildren<QLabel*>())
            if (l->text() == QDir::toNativeSeparators(cfg.clipsDir)) l->setText(QDir::toNativeSeparators("C:/Users/You/Videos/Clipline"));
    g->grab().save(dir + "/gallery.png");
    say("gallery saved");
    // Weiches Scrollen: zwei Rastungen am Mausrad, Position über die Zeit mitschreiben
    if (auto* lv = g->findChild<QListView*>(); lv && qEnvironmentVariableIsSet("CLIPLINE_TESTSCROLL")) {
        QScrollBar* sb = lv->verticalScrollBar();
        say(QString("scroll max=%1 start=%2").arg(sb->maximum()).arg(sb->value()));
        const QPointF pos(lv->viewport()->width() / 2.0, lv->viewport()->height() / 2.0);
        for (int k = 0; k < 2; ++k) {
            QWheelEvent ev(pos, lv->viewport()->mapToGlobal(pos), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                           Qt::NoScrollPhase, false);
            QApplication::sendEvent(lv->viewport(), &ev);
        }
        QElapsedTimer st;
        st.start();
        QStringList samples;
        qint64 paintMs = 0;
        int paints = 0;
        while (st.elapsed() < 450) {
            QApplication::processEvents(QEventLoop::AllEvents, 5);
            QElapsedTimer pt;
            pt.start();
            lv->viewport()->grab();  // ein Frame zeichnen
            paintMs += pt.elapsed();
            ++paints;
            samples << QString("%1:%2").arg(st.elapsed()).arg(sb->value());
        }
        say("scroll samples " + samples.join(' '));
        say(QString("scroll end=%1 frames=%2 avg paint=%3 ms").arg(sb->value()).arg(paints).arg(double(paintMs) / std::max(1, paints), 0, 'f', 2));
    }
    ctl.quit();
    say("quit done");
    return ok ? 0 : 2;
}

// "--save" = Clip-Knopf 1, "--save 2" = Clip-Knopf 2 …; "--hotkey <Taste>" kommt von Hyprland/Sway (Wayland).
// Nur an die laufende Instanz weiterreichen - ohne Fenstersystem: Das geht schneller und klappt auch, wenn der
// Desktop-Befehl kein Display hat (unter Wayland ohne XWayland/Qt-Wayland-Plugin stürzte es sonst ab).
static int relay(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    const int saveAt = args.indexOf("--save"), keyAt = args.indexOf("--hotkey");
    QByteArray cmd = "save";
    if (saveAt >= 0) {
        const int n = saveAt + 1 < args.size() ? args[saveAt + 1].toInt() : 0;
        if (n > 1) cmd = "save:" + QByteArray::number(n);
    }
    if (keyAt >= 0 && keyAt + 1 < args.size()) cmd = "hotkey:" + args[keyAt + 1].toUtf8();
    return sendToRunning(cmd) ? 0 : 1;  // 1 = nichts läuft, das speichern könnte
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i)
        if (!qstrcmp(argv[i], "--save") || !qstrcmp(argv[i], "--hotkey")) return relay(argc, argv);
#ifdef Q_OS_WIN
    // Eigene App-ID: eigener Taskleisten-Button, nicht mit anderen Programmen gruppiert
    SetCurrentProcessExplicitAppUserModelID(L"WSoftware.Clipline");
#endif
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("WSoftware");
    QCoreApplication::setApplicationName("Clipline");
    QGuiApplication::setApplicationDisplayName("Clipline");
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(kLinuxAppId);  // Wayland-App-ID = Name der Desktop-Datei
#endif
    app.setWindowIcon(cliplineIcon());
    app.setQuitOnLastWindowClosed(false);
    installNoWheelChange();

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
    // Nur eine Instanz: weitere Starts reichen ihren Wunsch weiter
    if (sendToRunning(background ? "noop" : "show")) return 0;
#ifdef Q_OS_LINUX
    // So früh wie möglich beim Portal anmelden: Das geht nur vor jedem anderen Portal-Aufruf
    if (WaylandHotkeys::isWayland()) registerPortalApp();
#endif

    appLog(QString("start: Clipline %1, %2, session %3, desktop %4").arg(APP_VERSION, QSysInfo::prettyProductName(),
                                                                        qEnvironmentVariable("XDG_SESSION_TYPE", "?"),
                                                                        qEnvironmentVariable("XDG_CURRENT_DESKTOP", "?")));
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
