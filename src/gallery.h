#pragma once
#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QLabel>
#include <QListView>
#include <QProcess>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>
#include <QWidget>

// Vorschaubilder + Dauer der Clips, auf der Platte zwischengespeichert
class ThumbCache : public QObject {
    Q_OBJECT
public:
    explicit ThumbCache(QObject* parent = nullptr);
    struct Info {
        QPixmap pix;
        double duration = -1;
        bool pending = false;
    };
    const Info& get(const QString& path);

signals:
    void ready(const QString& path);

private:
    QString key(const QString& path) const;
    void next();
    QString dir_;
    QHash<QString, Info> cache_;
    QStringList queue_;
    QProcess* proc_ = nullptr;
    QString cur_;
};

class Controller;

// Kurz nach dem Anklicken: Kachel grau mit Ladekreis ("Wird geöffnet…"), bis der Player aufgeht
struct OpeningHint {
    static constexpr int kMs = 1100;
    QString path;
    QElapsedTimer clock;
    bool active() const { return !path.isEmpty() && clock.isValid() && clock.elapsed() < kMs; }
};

class GalleryWindow : public QWidget {
    Q_OBJECT
public:
    explicit GalleryWindow(Controller* ctl);
    void reload();
    void updateStatus();
    void showOpening(const QString& path);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void openClip(const QString& path);
    void contextMenu(const QPoint& pos);
    QString selected() const;

    Controller* ctl_;
    QStandardItemModel model_;
    QListView* view_;
    ThumbCache thumbs_;
    QFileSystemWatcher watcher_;
    QTimer reloadTimer_, statusTimer_, spinTimer_;
    OpeningHint opening_;
    QLabel *status_, *folder_, *count_, *empty_;
};
