#pragma once
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class GlobalHotkey;

// Globale Tasten unter Wayland. Ein X11-Grab sieht dort nur Tastendrücke in X11-Fenstern - Clipline bekam
// die Clip-Taste in Spielen und anderen Programmen nie mit ("Clippen geht unter Linux nicht").
//  - Hyprland / Sway: Clipline trägt seine Tasten selbst ein (hyprctl / swaymsg). Die Taste startet
//    "clipline --hotkey <Taste>", das reicht den Druck an die laufende Instanz weiter.
//  - Sonst (KDE Plasma, GNOME 48+ …): XDG-Portal "GlobalShortcuts"; beim ersten Mal fragt das System nach.
class WaylandHotkeys : public QObject {
    Q_OBJECT
public:
    static WaylandHotkeys& instance();
    static bool isWayland();
    bool available() const { return backend_ != None; }
    bool add(GlobalHotkey* hotkey, const QString& sequence, const QString& description);
    void remove(GlobalHotkey* hotkey);
    void trigger(const QString& sequence);  // "clipline --hotkey <Taste>" (Hyprland/Sway)

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
    void portalRebind();
    void portalRequest(const QString& method, QList<QVariant> args, QVariantMap options);
    static QString idFor(const QString& sequence);

    Backend backend_ = None;
    QMap<GlobalHotkey*, Key> keys_;
    QTimer rebind_;                 // Portal: alle Tasten gesammelt in einem Rutsch eintragen
    enum Step { Idle, Create, Bind } step_ = Idle;
    QString session_, requestPath_;
};
