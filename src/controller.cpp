#include "controller.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QSettings>
#include "gallery.h"
#include "i18n.h"
#include "look.h"
#include "setup.h"

QString serverName() {
    QString user = qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", "user"));
    return "clipline-" + user.remove(QRegularExpression("[^A-Za-z0-9_-]"));
}

Controller::Controller(const Config& cfg, QObject* parent) : QObject(parent), cfg_(cfg) {
    tray_.setContextMenu(&menu_);
    auto* open = menu_.addAction(L("Open Clipline"), this, &Controller::showGallery);
    QFont f = open->font();
    f.setBold(true);
    open->setFont(f);
    actSave_ = menu_.addAction(L("Save clip"), this, &Controller::saveClip);
    actPause_ = menu_.addAction(QString(), this, &Controller::togglePause);
    menu_.addSeparator();
    menu_.addAction(L("Open folder"), this, [this] { revealInFolder(cfg_.clipsDir); });
    menu_.addAction(L("Settings"), this, &Controller::openSettings);
    menu_.addSeparator();
    menu_.addAction(L("Quit"), this, &Controller::quit);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) showGallery();
    });
    connect(&tray_, &QSystemTrayIcon::messageClicked, this, &Controller::showGallery);

    connect(&hotkey_, &GlobalHotkey::activated, this, &Controller::saveClip);
    connect(&rec_, &Recorder::stateChanged, this, &Controller::refreshTray);
    connect(&rec_, &Recorder::warning, this, [this](const QString& w) {
        notify("Clipline", w, QSystemTrayIcon::Warning, 6000);
    });
    connect(&rec_, &Recorder::clipSaved, this, [this](const QString& path, bool ok, const QString& err) {
        if (ok) {
            lastClip_ = path;
            notify(L("Clip saved"), QFileInfo(path).fileName(), QSystemTrayIcon::Information, 3500);
            if (gallery_) gallery_->reload();
            emit clipDone(path, true);
        } else {
            emit clipDone(QString(), false);
            notify("Clipline", err.isEmpty() ? L("Could not save the clip.") : err.left(300),
                              QSystemTrayIcon::Warning, 5000);
        }
    });

    applyConfig(cfg_, true);
    if (!quiet) tray_.show();
    if (!noRecord) rec_.start();
    refreshTray();
}

Controller::~Controller() {
    delete gallery_;
    rec_.stop();
    // Recorder/Hotkey leben kürzer als der QObject-Teil des Controllers: keine Signale mehr an uns
    rec_.disconnect(this);
    hotkey_.disconnect(this);
}

bool Controller::listen() {
    QLocalServer::removeServer(serverName());
    if (!server_.listen(serverName())) return false;
    connect(&server_, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* s = server_.nextPendingConnection()) {
            connect(s, &QLocalSocket::readyRead, this, [this, s] { command(s->readAll().trimmed()); s->disconnectFromServer(); });
            connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        }
    });
    return true;
}

void Controller::command(const QByteArray& cmd) {
    if (cmd == "save") saveClip();
    else if (cmd == "show") showGallery();
    else if (cmd == "quit") quit();
}

void Controller::applyConfig(const Config& c, bool first) {
    const bool autostartChanged = first || c.autostart != cfg_.autostart;
    cfg_ = c;
    if (!quiet) cfg_.save();
    setLanguage(cfg_.language);
    applyLook(cfg_);
    QDir().mkpath(cfg_.clipsDir);
    if (autostartChanged && !quiet) setAutostart(cfg_.autostart);
    rec_.setConfig(cfg_);
    registerHotkey();
    if (gallery_) {  // neu aufbauen, damit Farben/Sprache greifen
        const bool told = toldBackground_;
        toldBackground_ = true;
        gallery_->close();
        toldBackground_ = told;
        showGallery();
    }
    refreshTray();
}

void Controller::registerHotkey() {
    if (quiet) return;
    hotkeyOk_ = hotkey_.set(cfg_.hotkey);
    if (!hotkeyOk_) {
        const QString key = QKeySequence(cfg_.hotkey).toString(QKeySequence::NativeText);
        notify("Clipline",
                          GlobalHotkey::supported()
                              ? L("The hotkey %1 is already used by another program. Choose a different one in the settings.").arg(key)
                              : L("Global hotkeys are not available here. Bind the command \"clipline --save\" to a key in your system settings."),
                          QSystemTrayIcon::Warning, 8000);
    }
}

void Controller::refreshTray() {
    const bool rec = rec_.wanted();
    tray_.setIcon(QIcon(cliplineLogo(64, rec && rec_.running(), !rec)));
    const QString key = QKeySequence(cfg_.hotkey).toString(QKeySequence::NativeText);
    tray_.setToolTip(rec ? QString("Clipline – %1 · %2 = %3").arg(L("Recording"), key, L("last %1").arg(fmtDuration(cfg_.clipSeconds)))
                         : QString("Clipline – %1").arg(L("Paused")));
    actPause_->setText(rec ? L("Pause recording") : L("Resume recording"));
    actSave_->setText(L("Save clip") + "\t" + key);
    actSave_->setEnabled(rec);
    if (gallery_) gallery_->updateStatus();
}

void Controller::showGallery() {
    if (!gallery_) gallery_ = new GalleryWindow(this);
    gallery_->show();
    gallery_->setWindowState((gallery_->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    gallery_->raise();
    gallery_->activateWindow();
}

void Controller::galleryClosed() {
    if (!toldBackground_) {
        toldBackground_ = true;
        notify("Clipline",
                          L("Clipline is still running in the background. Press %1 to save a clip.")
                              .arg(QKeySequence(cfg_.hotkey).toString(QKeySequence::NativeText)),
                          QSystemTrayIcon::Information, 4000);
    }
}

void Controller::saveClip() {
    if (!rec_.wanted()) return;
    if (cfg_.sound) playSaveSound();
    rec_.saveClip();
}

void Controller::togglePause() {
    if (rec_.wanted()) rec_.stop();
    else rec_.start();
    refreshTray();
}

void Controller::openSettings() {
    if (settingsOpen_) return;
    settingsOpen_ = true;
    hotkey_.clear();  // sonst fängt der globale Hotkey die Eingabe im Tastenfeld ab
    SettingsDialog dlg(cfg_, gallery_);
    if (dlg.exec() == QDialog::Accepted) applyConfig(dlg.config(), false);
    else { applyLook(cfg_); registerHotkey(); }
    settingsOpen_ = false;
}

void Controller::quit() {
    rec_.stop();
    tray_.hide();
    QApplication::quit();
}

void Controller::notify(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon, int ms) {
    if (!quiet && tray_.isVisible()) tray_.showMessage(title, text, icon, ms);
}
