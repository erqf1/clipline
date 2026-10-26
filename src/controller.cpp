#include "controller.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QKeySequence>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include "gallery.h"
#include "i18n.h"
#include "look.h"
#include "setup.h"
#include <algorithm>
#if defined(Q_OS_LINUX)
#include "wayland_capture.h"
static bool waylandSession() { return WaylandCapture::isWaylandSession(); }
#else
static bool waylandSession() { return false; }
#endif
#if defined(Q_OS_LINUX)
#include "wayland_hotkeys.h"
#endif

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
    actMute_ = menu_.addAction(QString(), this, &Controller::toggleMute);
    actMute_->setCheckable(true);
    menu_.addSeparator();
    menu_.addAction(L("Open folder"), this, [this] { revealInFolder(cfg_.clipsDir); });
    menu_.addAction(L("Settings"), this, &Controller::openSettings);
    menu_.addAction(L("Check for updates"), this, [this] { if (updater_) updater_->check(true); });
    menu_.addSeparator();
    menu_.addAction(L("Quit"), this, &Controller::quit);
    connect(&tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) showGallery();
    });
    connect(&tray_, &QSystemTrayIcon::messageClicked, this, &Controller::showGallery);

    // Updates: nach dem Start und dann täglich still prüfen; fragt nur bei neuer, nicht ignorierter Version
    if (!quiet) {
        Updater::Options uo;
        uo.repo = "erqf1/clipline";
        uo.appName = "Clipline";
        uo.version = APP_VERSION;
        uo.innoAppId = "4F2C9A61-7D3B-4E8A-A5C2-9B1E6D0F3A77";  // packaging/windows/clipline.iss
        uo.parent = [this]() -> QWidget* { return gallery_; };
        uo.texts = [] {
            return UpdaterTexts{L("Update available"),
                                L("%1 %2 is available (you have %3). Update now? The app restarts afterwards."),
                                L("Update now"), L("Ignore this update"), L("Later"), L("Downloading update…"),
                                L("The update couldn't be installed automatically. The download page opens instead."),
                                L("You're using the latest version."), L("Cancel")};
        };
        uo.quit = [this] { quit(); };
        updater_ = new Updater(uo, this);
        updater_->startAutoCheck(15000);
    }

    connect(&hotkey_, &GlobalHotkey::activated, this, &Controller::saveClip);
#if defined(Q_OS_LINUX)
    // Wayland: Das System hat eine Taste abgelehnt oder ohne Taste eingetragen -> sagen, was zu tun ist
    if (!quiet && WaylandHotkeys::isWayland())
        connect(&WaylandHotkeys::instance(), &WaylandHotkeys::problem, this, [this](const QString& text) {
            notify("Clipline", text, QSystemTrayIcon::Warning, 10000);
        });
    // Taste in den Systemeinstellungen geändert: Menü, Hinweise und Galerie zeigen die neue
    if (!quiet && WaylandHotkeys::isWayland())
        connect(&WaylandHotkeys::instance(), &WaylandHotkeys::keysChanged, this, [this] {
            refreshTray();
            if (gallery_) gallery_->reload();
        });
#endif
    connect(&muteHotkey_, &GlobalHotkey::activated, this, &Controller::toggleMute);

    paused_ = noRecord;
    applyConfig(cfg_, true);
    if (!quiet) tray_.show();
    refreshTray();
}

Controller::~Controller() {
    delete gallery_;
    // Recorder/Hotkey leben kürzer als der QObject-Teil des Controllers: keine Signale mehr an uns
    for (auto& [mon, rec] : recs_) {
        rec->stop();
        rec->disconnect(this);
    }
    hotkey_.disconnect(this);
}

Config Controller::effective() const {
    Config c = cfg_;
    if (waylandSession())
        for (ClipButton& b : c.extraButtons) b.monitor = c.monitor;
    return c;
}

Recorder* Controller::recFor(const QString& monitor) const {
    for (const auto& [mon, rec] : recs_)
        if (mon == monitor) return rec.get();
    return recs_.empty() ? nullptr : recs_.front().second.get();
}

bool Controller::recording() const {
    for (const auto& [mon, rec] : recs_)
        if (rec->wanted()) return true;
    return false;
}

bool Controller::micMuted() const { return !recs_.empty() && recs_.front().second->micMuted(); }

// Für jeden Bildschirm, den ein Clip-Knopf braucht, läuft eine eigene Aufnahme (mit Länge und Grafik des
// anspruchsvollsten Knopfs dort). Nicht mehr gebrauchte Aufnahmen werden beendet.
void Controller::syncRecorders() {
    const Config eff = effective();
    const QStringList want = eff.recordingMonitors();
    for (auto it = recs_.begin(); it != recs_.end();) {
        if (!want.contains(it->first)) {
            it->second->stop();
            it->second->disconnect(this);
            it = recs_.erase(it);
        } else {
            ++it;
        }
    }
    std::vector<std::pair<QString, std::unique_ptr<Recorder>>> ordered;
    for (const QString& mon : want) {
        auto it = std::find_if(recs_.begin(), recs_.end(), [&](const auto& p) { return p.first == mon; });
        std::unique_ptr<Recorder> rec;
        const bool fresh = it == recs_.end();
        if (!fresh) rec = std::move(it->second);
        else {
            rec = std::make_unique<Recorder>();
            Recorder* r = rec.get();
            connect(r, &Recorder::stateChanged, this, &Controller::refreshTray);
            // Linux/Wayland: gemerkte Bildschirmfreigabe speichern -> beim nächsten Start kein Portal-Dialog
            connect(r, &Recorder::waylandTokenChanged, this, [this](const QString& token) {
                cfg_.waylandToken = token;
                if (!quiet) cfg_.save();
            });
            connect(r, &Recorder::warning, this, [this](const QString& w) { notify("Clipline", w, QSystemTrayIcon::Warning, 6000); });
            // Ton erst, wenn der Clip geprüft ist: hell nur für einen vollständigen Clip, sonst der tiefe Fehlerton
            // (Hinweise blendet Windows im Spiel oft aus)
            connect(r, &Recorder::clipSaved, this, [this](const QString& path, bool ok, const QString& err) {
                appLog(QString("save: %1 %2 %3").arg(ok ? "ok" : "FAILED", path, err));
                if (cfg_.sound) ok && err.isEmpty() ? playSaveSound() : playErrorSound();
                if (ok) {
                    lastClip_ = path;
                    if (err.isEmpty())
                        notify(L("Clip saved"), QFileInfo(path).fileName(), QSystemTrayIcon::Information, 3500);
                    else
                        notify(L("Clip incomplete"), QFileInfo(path).fileName() + "\n" + err, QSystemTrayIcon::Warning, 6000);
                    if (gallery_) gallery_->reload();
                    emit clipDone(path, true);
                } else {
                    emit clipDone(QString(), false);
                    notify("Clipline", err.isEmpty() ? L("Could not save the clip.") : err.left(300), QSystemTrayIcon::Warning, 5000);
                }
            });
        }
        rec->setConfig(eff.recordingConfig(mon));
        if (!recs_.empty() && !fresh) rec->setMicMuted(micMuted());
        if (fresh && !paused_) rec->start();
        ordered.emplace_back(mon, std::move(rec));
    }
    recs_ = std::move(ordered);
    appLog(QString("record: %1 recording(s): %2").arg(recs_.size()).arg(want.join(", ")));
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
    else if (cmd.startsWith("save:")) saveClipButton(cmd.mid(5).toInt() - 1);  // "clipline --save 2"
#if defined(Q_OS_LINUX)
    else if (cmd.startsWith("hotkey:")) WaylandHotkeys::instance().trigger(QString::fromUtf8(cmd.mid(7)));  // Hyprland/Sway
#endif
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
    syncRecorders();
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
    muteHotkey_.clear();
    extraKeys_.clear();
    hotkey_.clear();
    // Alle eigenen Tasten vorab melden: "F9 auch mit Shift" darf einer eigenen Shift+F9-Taste nichts wegnehmen
    QStringList exact = {cfg_.hotkey, cfg_.muteHotkey};
    for (const ClipButton& b : cfg_.extraButtons) exact << b.hotkey;
    setReservedHotkeys(exact);
    hotkey_.setDescription(L("Save clip"));
    muteHotkey_.setDescription(L("Mute microphone"));
    if (!cfg_.muteHotkey.isEmpty() && !cfg_.mic.isEmpty() && !muteHotkey_.set(cfg_.muteHotkey)) {
        const QString key = QKeySequence(cfg_.muteHotkey).toString(QKeySequence::NativeText);
        notify("Clipline", L("The hotkey %1 is already used by another program. Choose a different one in the settings.").arg(key),
               QSystemTrayIcon::Warning, 8000);
    }
    hotkeyOk_ = hotkey_.set(cfg_.hotkey);
    appLog(QString("hotkey: %1 %2").arg(cfg_.hotkey, hotkeyOk_ ? "set" : "NOT set"));
    if (!hotkeyOk_) {
        const QString key = QKeySequence(cfg_.hotkey).toString(QKeySequence::NativeText);
        notify("Clipline",
                          GlobalHotkey::supported()
                              ? L("The hotkey %1 is already used by another program. Choose a different one in the settings.").arg(key)
                              : L("Global hotkeys are not available here. Bind the command \"clipline --save\" to a key in your system settings."),
                          QSystemTrayIcon::Warning, 8000);
    }
    // Weitere Clip-Knöpfe: je eine eigene Taste
    for (int i = 0; i < cfg_.extraButtons.size(); ++i) {
        const ClipButton& b = cfg_.extraButtons[i];
        if (b.hotkey.isEmpty()) continue;
        auto key = std::make_unique<GlobalHotkey>();
        key->setDescription(L("Save clip %1").arg(i + 2));
        connect(key.get(), &GlobalHotkey::activated, this, [this, i] { saveClipButton(i + 1); });
        if (!key->set(b.hotkey) && GlobalHotkey::supported()) {
            notify("Clipline", L("The hotkey %1 is already used by another program. Choose a different one in the settings.")
                                   .arg(QKeySequence(b.hotkey).toString(QKeySequence::NativeText)),
                   QSystemTrayIcon::Warning, 8000);
        }
        extraKeys_.push_back(std::move(key));
    }
}

void Controller::refreshTray() {
    const bool rec = recording();
    bool running = !recs_.empty();
    for (const auto& [mon, r] : recs_) running = running && r->running();
    tray_.setIcon(QIcon(cliplineLogo(64, rec && running, !rec)));
    const QString key = keyText(cfg_.hotkey);
    tray_.setToolTip((rec ? QString("Clipline – %1 · %2 = %3").arg(L("Recording"), key, L("last %1").arg(fmtDuration(cfg_.clipSeconds)))
                          : QString("Clipline – %1").arg(L("Paused"))) +
                     (micMuted() ? " · " + L("Microphone muted") : QString()));
    actPause_->setText(rec ? L("Pause recording") : L("Resume recording"));
    actMute_->setVisible(!cfg_.mic.isEmpty());
    actMute_->setChecked(micMuted());
    const QString muteKey = cfg_.muteHotkey.isEmpty() ? QString() : keyText(cfg_.muteHotkey);
    actMute_->setText(L("Mute microphone") + (muteKey.isEmpty() ? QString() : QStringLiteral("\t") + muteKey));
    actSave_->setText(L("Save clip") + (cfg_.extraButtons.isEmpty() ? QString() : QString(" (%1)").arg(fmtDuration(cfg_.clipSeconds))) +
                      "\t" + key);
    actSave_->setEnabled(rec);
    // Weitere Clip-Knöpfe gleich unter "Clip speichern"
    for (QAction* a : extraActs_) {
        menu_.removeAction(a);
        a->deleteLater();
    }
    extraActs_.clear();
    const QList<QAction*> all = menu_.actions();
    QAction* before = all.value(all.indexOf(actSave_) + 1);
    for (int i = 0; i < cfg_.extraButtons.size(); ++i) {
        const ClipButton& b = cfg_.extraButtons[i];
        auto* a = new QAction(L("Save clip %1").arg(i + 2) + QString(" (%1)").arg(fmtDuration(b.seconds)) + "\t" +
                                  (b.hotkey.isEmpty() ? QString() : keyText(b.hotkey)), &menu_);
        a->setEnabled(rec);
        connect(a, &QAction::triggered, this, [this, i] { saveClipButton(i + 1); });
        menu_.insertAction(before, a);
        extraActs_ << a;
    }
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
                              .arg(keyText(cfg_.hotkey)),
                          QSystemTrayIcon::Information, 4000);
    }
}

void Controller::saveClip() { saveClipButton(0); }

// Unter KDE/GNOME (Portal) legt das System die Taste fest und man kann sie dort ändern - dann die anzeigen,
// die es wirklich nutzt, sonst die eingestellte
QString Controller::keyText(const QString& sequence) const {
#if defined(Q_OS_LINUX)
    if (WaylandHotkeys::isWayland()) {
        const QString sys = WaylandHotkeys::instance().systemKey(sequence);
        if (!sys.isEmpty()) return sys;
    }
#endif
    return QKeySequence(sequence).toString(QKeySequence::NativeText);
}

void Controller::saveClipButton(int i) {
    const Config eff = effective();
    const QList<ClipButton> buttons = eff.allButtons();
    if (i < 0 || i >= buttons.size()) return;
    Recorder* rec = recFor(eff.buttonMonitor(i));
    if (!rec) return;
    appLog(QString("save: clip button %1 (%2 s), buffered %3 s").arg(i + 1).arg(buttons[i].seconds)
               .arg(rec->bufferedSeconds(), 0, 'f', 1));
    if (!rec->wanted()) {
        if (cfg_.sound) playErrorSound();
        notify("Clipline", L("Recording is paused – no clip was saved."), QSystemTrayIcon::Warning, 5000);
        return;
    }
    rec->saveClip(buttons[i]);
}

void Controller::togglePause() {
    paused_ = recording();
    for (auto& [mon, rec] : recs_) {
        if (paused_) rec->stop();
        else rec->start();
    }
    refreshTray();
}

void Controller::openSettings() {
    if (settingsOpen_) return;
    settingsOpen_ = true;
    hotkey_.clear();  // sonst fängt der globale Hotkey die Eingabe im Tastenfeld ab
    muteHotkey_.clear();
    extraKeys_.clear();
    SettingsDialog dlg(cfg_, gallery_);
    if (dlg.exec() == QDialog::Accepted) applyConfig(dlg.config(), false);
    else { applyLook(cfg_); registerHotkey(); }
    settingsOpen_ = false;
}

// Mikrofon stumm/an (Hotkey oder Tray-Menü) - wirkt sofort, die Aufnahme läuft weiter
void Controller::toggleMute() {
    if (cfg_.mic.isEmpty()) return;
    const bool mute = !micMuted();
    for (auto& [mon, rec] : recs_) rec->setMicMuted(mute);
    notify(mute ? L("Microphone muted") : L("Microphone on"),
           mute ? L("Your voice is not recorded until you press the key again.") : QString(),
           QSystemTrayIcon::Information, 1500);
    refreshTray();
}

void Controller::quit() {
    for (auto& [mon, rec] : recs_) rec->stop();
    tray_.hide();
    QApplication::quit();
}

void Controller::notify(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon, int ms) {
    if (quiet) return;
    if (icon == QSystemTrayIcon::Warning) appLog(QString("notice: %1: %2").arg(title, text).simplified());
    // Linux: über den Benachrichtigungsdienst - viele Wayland-Desktops (Hyprland, GNOME) haben kein Tray
    if (desktopNotify(title, text, ms, icon == QSystemTrayIcon::Warning)) return;
    if (tray_.isVisible()) tray_.showMessage(title, text, icon, ms);
}
