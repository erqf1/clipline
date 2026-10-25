#pragma once
#include <QDBusObjectPath>
#include <QLocalSocket>
#include <QMap>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class GlobalHotkey;

// Beim Desktop-Portal als "win.mert.Clipline" anmelden. Muss vor jedem anderen Portal-Aufruf passieren,
// sonst kennt das Portal die App nicht (KDE/GNOME merken sich Tasten und Freigaben dann nicht).
void registerPortalApp();

// Globale Tasten unter Wayland. Ein X11-Grab sieht dort nur Tastendrücke in X11-Fenstern - Clipline bekam
// die Clip-Taste in Spielen und anderen Programmen nie mit ("Clippen geht unter Linux nicht").
//  - Hyprland / Sway: Clipline trägt seine Tasten selbst ein (hyprctl / swaymsg). Die Taste startet
//    "clipline --hotkey <Taste>", das reicht den Druck an die laufende Instanz weiter. Nach einem Neuladen der
//    Compositor-Einstellungen sind solche Tasten weg - dann trägt Clipline sie neu ein.
//  - Sonst (KDE Plasma, GNOME 48+ …): XDG-Portal "GlobalShortcuts"; beim ersten Mal fragt das System nach.
class WaylandHotkeys : public QObject {
    Q_OBJECT
public:
    static WaylandHotkeys& instance();
    static bool isWayland();
    bool available() const { return backend_ != None; }
    QString backendName() const;
    bool add(GlobalHotkey* hotkey, const QString& sequence, const QString& description);
    void remove(GlobalHotkey* hotkey);
    void trigger(const QString& sequence);  // "clipline --hotkey <Taste>" (Hyprland/Sway)

signals:
    // Das System hat die Taste nicht übernommen (abgelehnt, ohne Taste eingetragen, Portal-Fehler)
    void problem(const QString& text);

private slots:
    void onResponse(uint response, const QVariantMap& results);
    void onActivated(const QDBusObjectPath& session, const QString& id, qulonglong timestamp, const QVariantMap& options);

private:
    WaylandHotkeys();
    enum Backend { None, Hyprland, Sway, Portal };
    struct Key {
        QString sequence, description;
    };
    bool compositorBind(const QString& sequence, bool bind);
    void watchCompositor();
    void rebindCompositor();
    void portalRebind();
    void portalRequest(const QString& method, QList<QVariant> args, QVariantMap options);
    QString portalWanted() const;  // Kennung der gewünschten Tasten (gleich = nichts neu eintragen)
    static QString idFor(const QString& sequence);
    static QString portalTrigger(const QString& sequence);

    Backend backend_ = None;
    uint portalVersion_ = 0;
    bool askedConfigure_ = false;   // Portal: Tasten-Dialog des Systems in diesem Lauf schon einmal geöffnet
    QMap<GlobalHotkey*, Key> keys_;
    QTimer rebind_;                 // Portal: alle Tasten gesammelt in einem Rutsch eintragen
    enum Step { Idle, Create, Bind } step_ = Idle;
    QString session_, requestPath_, bound_;
    QLocalSocket hyprEvents_;       // Hyprland: "configreloaded" -> Tasten neu eintragen
    QProcess swayEvents_;           // Sway: Workspace-Ereignis "reload" -> Tasten neu eintragen
    QTimer watchRetry_;
};
