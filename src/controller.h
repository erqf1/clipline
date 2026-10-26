#pragma once
#include <QAction>
#include <QLocalServer>
#include <QMenu>
#include <QPointer>
#include <QSystemTrayIcon>
#include <memory>
#include <vector>
#include "config.h"
#include "platform.h"
#include "recorder.h"
#include "updater.h"

class GalleryWindow;

// Läuft im Hintergrund (Tray-Symbol), steuert Aufnahme, Hotkey und Fenster
class Controller : public QObject {
    Q_OBJECT
public:
    explicit Controller(const Config& cfg, QObject* parent = nullptr);
    ~Controller() override;
    const Config& config() const { return cfg_; }
    QString keyText(const QString& sequence) const;  // Taste zum Anzeigen (unter KDE/GNOME die, die das System nutzt)
    const Recorder& recorder() const { return *recs_.front().second; }  // Aufnahme von Clip-Knopf 1
    bool listen();  // Einzelinstanz: nimmt Befehle von weiteren Starts entgegen
    GalleryWindow* gallery() const { return gallery_; }
    static inline bool noRecord = false;  // Selbsttest: nur Fenster rendern
    static inline bool quiet = false;  // Selbsttest: kein Tray, kein Hotkey, nichts speichern

signals:
    void clipDone(const QString& path, bool ok);

public slots:
    void showGallery();
    void saveClip();             // Clip-Knopf 1
    void saveClipButton(int i);  // 0 = Knopf 1, 1 = Knopf 2 …
    void openSettings();
    void togglePause();
    void toggleMute();
    void galleryClosed();
    void quit();

private:
    Updater* updater_ = nullptr;
    void applyConfig(const Config& c, bool first);
    void registerHotkey();
    void refreshTray();
    void command(const QByteArray& cmd);
    void notify(const QString& title, const QString& text, QSystemTrayIcon::MessageIcon icon, int ms);

    // Eine Aufnahme pro Bildschirm, den ein Clip-Knopf braucht (Knopf 1 zuerst)
    void syncRecorders();
    Recorder* recFor(const QString& monitor) const;
    Config effective() const;  // Wayland: das Portal entscheidet den Bildschirm -> alle Knöpfe auf einer Aufnahme
    bool recording() const;    // läuft (nicht pausiert)
    bool micMuted() const;

    Config cfg_;
    std::vector<std::pair<QString, std::unique_ptr<Recorder>>> recs_;
    bool paused_ = false;
    GlobalHotkey hotkey_, muteHotkey_;
    std::vector<std::unique_ptr<GlobalHotkey>> extraKeys_;  // Clip-Knopf 2, 3, …
    QMenu menu_;  // vor tray_: muss länger leben als das Tray-Symbol
    QSystemTrayIcon tray_;
    QAction *actSave_, *actPause_, *actMute_;
    QList<QAction*> extraActs_;  // "Clip 2 speichern" … im Tray-Menü
    QPointer<GalleryWindow> gallery_;
    QLocalServer server_;
    bool toldBackground_ = false, hotkeyOk_ = true, settingsOpen_ = false;
    QString lastClip_;
};

QString serverName();
