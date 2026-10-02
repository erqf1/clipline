#include "gallery.h"

#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>
#include "controller.h"
#include "i18n.h"
#include "look.h"
#include "platform.h"

static constexpr int kPathRole = Qt::UserRole + 1;
static constexpr int kSizeRole = Qt::UserRole + 2;
static constexpr int kTimeRole = Qt::UserRole + 3;
static constexpr int kOpenedRole = Qt::UserRole + 4;

// Geöffnete Clips: eigene kleine Datei statt Registry, im Clip-Ordner landet nichts
static QString openedFile() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + "/opened.ini";
}
static QString openedKey(const QString& path) {
    return QCryptographicHash::hash(QDir::cleanPath(path).toLower().toUtf8(), QCryptographicHash::Sha1).toHex().left(20);
}

// ---------------------------------------------------------------- ThumbCache
ThumbCache::ThumbCache(QObject* parent) : QObject(parent) {
    dir_ = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/thumbs";
    QDir().mkpath(dir_);
}

QString ThumbCache::key(const QString& path) const {
    QFileInfo fi(path);
    const QByteArray k = (path + "|" + QString::number(fi.size()) + "|" +
                          QString::number(fi.lastModified().toMSecsSinceEpoch())).toUtf8();
    return QCryptographicHash::hash(k, QCryptographicHash::Sha1).toHex().left(20);
}

const ThumbCache::Info& ThumbCache::get(const QString& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return *it;
    Info info;
    const QString k = key(path);
    QSettings idx(dir_ + "/index.ini", QSettings::IniFormat);
    const QString jpg = dir_ + "/" + k + ".jpg";
    if (QFileInfo::exists(jpg) && idx.contains(k)) {
        info.pix.load(jpg);
        info.duration = idx.value(k).toDouble();
    } else {
        info.pending = true;
        queue_ << path;
        QTimer::singleShot(0, this, &ThumbCache::next);
    }
    return *cache_.insert(path, info);
}

void ThumbCache::next() {
    if (proc_ || queue_.isEmpty()) return;
    cur_ = queue_.takeFirst();
    const QString jpg = dir_ + "/" + key(cur_) + ".jpg";
    proc_ = new QProcess(this);
    connect(proc_, &QProcess::finished, this, [this, jpg] {
        const QString err = QString::fromUtf8(proc_->readAllStandardError());
        proc_->deleteLater();
        proc_ = nullptr;
        Info& info = cache_[cur_];
        info.pending = false;
        auto m = QRegularExpression(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))").match(err);
        if (m.hasMatch()) info.duration = m.captured(1).toInt() * 3600 + m.captured(2).toInt() * 60 + m.captured(3).toDouble();
        info.pix.load(jpg);
        QSettings idx(dir_ + "/index.ini", QSettings::IniFormat);
        idx.setValue(key(cur_), info.duration);
        emit ready(cur_);
        next();
    });
    proc_->start(ffmpegPath(), {"-hide_banner", "-y", "-ss", "1", "-i", cur_, "-frames:v", "1", "-vf", "scale=480:-2",
                                "-q:v", "4", jpg});
}

// ---------------------------------------------------------------- Kachel-Darstellung
namespace {
class TileDelegate : public QStyledItemDelegate {
public:
    TileDelegate(ThumbCache* t, const Palette& pal, QObject* parent) : QStyledItemDelegate(parent), thumbs_(t), pal_(pal) {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return QSize(276, 222); }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const override {
        const Palette& pal = pal_;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF card = QRectF(opt.rect).adjusted(8, 8, -8, -8);
        const bool sel = opt.state & QStyle::State_Selected, hov = opt.state & QStyle::State_MouseOver;
        p->setPen(QPen(sel ? pal.accent : hov ? pal.border.lighter(130) : pal.border, sel ? 2 : 1));
        p->setBrush(pal.panel);
        p->drawRoundedRect(card, 12, 12);

        const QString path = idx.data(kPathRole).toString();
        const ThumbCache::Info& info = thumbs_->get(path);
        const QRectF thumb(card.left() + 1, card.top() + 1, card.width() - 2, (card.width() - 2) * 9.0 / 16.0);
        QPainterPath clip;
        clip.addRoundedRect(thumb, 11, 11);
        p->setClipPath(clip);
        p->fillRect(thumb, pal.panel2);
        if (!info.pix.isNull()) {
            const QSizeF s = QSizeF(info.pix.size()).scaled(thumb.size(), Qt::KeepAspectRatioByExpanding);
            p->drawPixmap(QRectF(thumb.center().x() - s.width() / 2, thumb.center().y() - s.height() / 2, s.width(), s.height()),
                          info.pix, QRectF(info.pix.rect()));
        }
        if (hov) {  // Abspiel-Symbol beim Überfahren
            p->fillRect(thumb, QColor(0, 0, 0, 70));
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(255, 255, 255, 230));
            p->drawEllipse(thumb.center(), 22, 22);
            QPainterPath tri;
            const QPointF c = thumb.center();
            tri.moveTo(c + QPointF(-6, -10)); tri.lineTo(c + QPointF(11, 0)); tri.lineTo(c + QPointF(-6, 10)); tri.closeSubpath();
            p->setBrush(QColor("#15161c"));
            p->drawPath(tri);
        }
        p->setClipping(false);
        if (info.duration > 0) {
            const int d = int(info.duration + 0.5);
            const QString t = QString("%1:%2").arg(d / 60).arg(d % 60, 2, 10, QChar('0'));
            QFont f = opt.font;
            f.setPixelSize(11);
            f.setBold(true);
            p->setFont(f);
            const QRectF b(thumb.right() - 50, thumb.bottom() - 26, 42, 19);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 170));
            p->drawRoundedRect(b, 9, 9);
            p->setPen(Qt::white);
            p->drawText(b, Qt::AlignCenter, t);
        }
        if (idx.data(kOpenedRole).toBool()) {  // schon geöffnet: Schild oben links mit Haken
            QFont f = opt.font;
            f.setPixelSize(11);
            f.setBold(true);
            p->setFont(f);
            const QString t = L("Opened");
            const QRectF b(thumb.left() + 8, thumb.top() + 8, QFontMetricsF(f).horizontalAdvance(t) + 34, 21);
            p->setPen(Qt::NoPen);
            p->setBrush(QColor(0, 0, 0, 175));
            p->drawRoundedRect(b, 10.5, 10.5);
            QPainterPath check;
            check.moveTo(b.left() + 9, b.center().y() + 0.5);
            check.lineTo(b.left() + 12.5, b.center().y() + 4);
            check.lineTo(b.left() + 19, b.center().y() - 3.5);
            p->setPen(QPen(pal.accent.lighter(130), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->setBrush(Qt::NoBrush);
            p->drawPath(check);
            p->setPen(Qt::white);
            p->drawText(b.adjusted(24, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, t);
        }
        QFont f = opt.font;
        f.setPixelSize(13);
        f.setWeight(QFont::DemiBold);
        p->setFont(f);
        p->setPen(pal.text);
        const QRectF txt(card.left() + 12, thumb.bottom() + 8, card.width() - 24, 20);
        p->drawText(txt, Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(f).elidedText(QFileInfo(path).completeBaseName(), Qt::ElideMiddle, int(txt.width())));
        f.setPixelSize(12);
        f.setWeight(QFont::Normal);
        p->setFont(f);
        p->setPen(pal.muted);
        const QDateTime when = idx.data(kTimeRole).toDateTime();
        const QString sub = QLocale().toString(when, QLocale::ShortFormat) + "  ·  " +
                            QLocale().formattedDataSize(idx.data(kSizeRole).toLongLong(), 1);
        p->drawText(txt.translated(0, 20), Qt::AlignLeft | Qt::AlignVCenter, sub);
        p->restore();
    }

private:
    ThumbCache* thumbs_;
    Palette pal_;
};
}  // namespace

// ---------------------------------------------------------------- Fenster
GalleryWindow::GalleryWindow(Controller* ctl) : ctl_(ctl) {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("root");
    setWindowTitle("Clipline");
    setWindowIcon(cliplineIcon());
    resize(1060, 700);
    setMinimumSize(640, 440);
    const Palette pal = makePalette(ctl_->config().accent, ctl_->config().dark);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(22, 18, 22, 16);
    v->setSpacing(12);

    // Kopfzeile
    auto* head = new QHBoxLayout;
    head->setSpacing(10);
    auto* logo = new QLabel;
    logo->setPixmap(cliplineLogo(40));
    head->addWidget(logo);
    auto* title = new QLabel("Clipline");
    title->setObjectName("h1");
    head->addWidget(title);
    head->addSpacing(8);
    status_ = new QLabel;
    status_->setObjectName("pill");
    head->addWidget(status_);
    head->addStretch();
    auto* save = new QPushButton(icon(Ic::Record, pal.accentText), " " + L("Save clip"));
    save->setObjectName("primary");
    save->setCursor(Qt::PointingHandCursor);
    connect(save, &QPushButton::clicked, ctl_, &Controller::saveClip);
    auto* openDir = new QPushButton(icon(Ic::Folder, pal.text), " " + L("Open folder"));
    connect(openDir, &QPushButton::clicked, this, [this] {
        QDir().mkpath(ctl_->config().clipsDir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(ctl_->config().clipsDir));
    });
    auto* settings = new QPushButton(icon(Ic::Gear, pal.text), QString());
    settings->setToolTip(L("Settings"));
    connect(settings, &QPushButton::clicked, ctl_, &Controller::openSettings);
    head->addWidget(save);
    head->addWidget(openDir);
    head->addWidget(settings);
    v->addLayout(head);

    // Unterzeile: Ordner und Anzahl
    auto* sub = new QHBoxLayout;
    folder_ = new QLabel;
    folder_->setObjectName("muted");
    count_ = new QLabel;
    count_->setObjectName("muted");
    sub->addWidget(folder_);
    sub->addWidget(new QLabel("·"));
    sub->addWidget(count_);
    sub->addStretch();
    v->addLayout(sub);

    // Raster
    view_ = new QListView;
    view_->setViewMode(QListView::IconMode);
    view_->setResizeMode(QListView::Adjust);
    view_->setMovement(QListView::Static);
    view_->setUniformItemSizes(true);
    view_->setSpacing(0);
    view_->setMouseTracking(true);
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    view_->setContextMenuPolicy(Qt::CustomContextMenu);
    view_->setModel(&model_);
    view_->setItemDelegate(new TileDelegate(&thumbs_, pal, view_));
    connect(&thumbs_, &ThumbCache::ready, view_->viewport(), qOverload<>(&QWidget::update));
    // Ein Klick öffnet den Clip (Rechtsklick für weitere Aktionen)
    view_->setCursor(Qt::PointingHandCursor);
    connect(view_, &QListView::clicked, this, [this](const QModelIndex& i) { openClip(i.data(kPathRole).toString()); });
    connect(view_, &QListView::customContextMenuRequested, this, &GalleryWindow::contextMenu);
    v->addWidget(view_, 1);

    empty_ = new QLabel;
    empty_->setAlignment(Qt::AlignCenter);
    empty_->setObjectName("muted");
    empty_->setWordWrap(true);
    v->addWidget(empty_, 1);

    reloadTimer_.setSingleShot(true);
    reloadTimer_.setInterval(400);
    connect(&reloadTimer_, &QTimer::timeout, this, &GalleryWindow::reload);
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, &reloadTimer_, qOverload<>(&QTimer::start));
    statusTimer_.setInterval(1000);
    connect(&statusTimer_, &QTimer::timeout, this, &GalleryWindow::updateStatus);
    statusTimer_.start();
    QSettings ini(openedFile(), QSettings::IniFormat);
    for (const QString& k : ini.childKeys()) opened_.insert(k);
    reload();
    updateStatus();
}

void GalleryWindow::setOpened(const QString& path, bool opened) {
    const QString k = openedKey(path);
    if (opened_.contains(k) == opened) return;
    QSettings ini(openedFile(), QSettings::IniFormat);
    if (opened) {
        opened_.insert(k);
        ini.setValue(k, QDateTime::currentDateTime().toString(Qt::ISODate));
    } else {
        opened_.remove(k);
        ini.remove(k);
    }
    for (int r = 0; r < model_.rowCount(); ++r)
        if (QStandardItem* it = model_.item(r); it->data(kPathRole).toString() == path) it->setData(opened, kOpenedRole);
}

void GalleryWindow::reload() {
    const QString dir = ctl_->config().clipsDir;
    QDir().mkpath(dir);
    if (!watcher_.directories().contains(dir)) {
        if (!watcher_.directories().isEmpty()) watcher_.removePaths(watcher_.directories());
        watcher_.addPath(dir);
    }
    const QString keep = selected();
    model_.clear();
    const QFileInfoList files = QDir(dir).entryInfoList({"*.mp4", "*.mkv", "*.mov"}, QDir::Files, QDir::Time);
    for (const QFileInfo& fi : files) {
        auto* it = new QStandardItem;
        it->setData(fi.absoluteFilePath(), kPathRole);
        it->setData(fi.size(), kSizeRole);
        it->setData(fi.lastModified(), kTimeRole);
        it->setData(opened_.contains(openedKey(fi.absoluteFilePath())), kOpenedRole);
        it->setToolTip(fi.fileName());
        it->setEditable(false);
        model_.appendRow(it);
        if (fi.absoluteFilePath() == keep) view_->setCurrentIndex(model_.indexFromItem(it));
    }
    folder_->setText(QDir::toNativeSeparators(dir));
    count_->setText(files.size() == 1 ? L("1 clip") : L("%1 clips").arg(files.size()));
    const bool none = files.isEmpty();
    view_->setVisible(!none);
    empty_->setVisible(none);
    empty_->setText(QString("<div style='font-size:20px;font-weight:650'>%1</div><div style='margin-top:8px'>%2</div>")
                        .arg(L("No clips yet"),
                             L("Press %1 and the last %2 are saved here.")
                                 .arg("<b>" + QKeySequence(ctl_->config().hotkey).toString(QKeySequence::NativeText) + "</b>",
                                      fmtDuration(ctl_->config().clipSeconds))));
}

void GalleryWindow::updateStatus() {
    const Recorder& r = ctl_->recorder();
    const QString key = QKeySequence(ctl_->config().hotkey).toString(QKeySequence::NativeText);
    QString dot, text;
    if (!r.wanted() && !Controller::noRecord) {
        dot = "#8d93a6";
        text = L("Paused");
    } else if (!r.running() && !Controller::noRecord) {
        dot = "#f5b400";
        text = L("Starting…");
    } else {
        dot = "#ff2d55";
        text = L("Recording") + " · " + L("last %1").arg(fmtDuration(ctl_->config().clipSeconds)) + " · " + key;
    }
    status_->setText(QString("<span style='color:%1'>●</span>&nbsp; %2").arg(dot, text));
}

QString GalleryWindow::selected() const {
    const QModelIndex i = view_->currentIndex();
    return i.isValid() && view_->selectionModel()->isSelected(i) ? i.data(kPathRole).toString() : QString();
}

void GalleryWindow::openClip(const QString& path) {
    if (path.isEmpty()) return;
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(path))) setOpened(path, true);
}

void GalleryWindow::contextMenu(const QPoint& pos) {
    const QModelIndex idx = view_->indexAt(pos);
    if (!idx.isValid()) return;
    view_->setCurrentIndex(idx);
    const QString path = idx.data(kPathRole).toString();
    const Palette pal = makePalette(ctl_->config().accent, ctl_->config().dark);
    QMenu m(this);
    m.addAction(icon(Ic::Play, pal.text), L("Play"), this, [this, path] { openClip(path); });
    m.addAction(icon(Ic::Reveal, pal.text), L("Show in folder"), this, [path] { revealInFolder(path); });
    if (idx.data(kOpenedRole).toBool())
        m.addAction(L("Mark as new"), this, [this, path] { setOpened(path, false); });
    m.addSeparator();
    m.addAction(icon(Ic::Rename, pal.text), L("Rename…"), this, [this, path] {
        QFileInfo fi(path);
        bool ok = false;
        const QString n = QInputDialog::getText(this, L("Rename clip"), L("New name:"), QLineEdit::Normal,
                                                fi.completeBaseName(), &ok).trimmed();
        const QString target = fi.dir().filePath(n + "." + fi.suffix());
        if (ok && !n.isEmpty() && n != fi.completeBaseName() && QFile::rename(path, target)) {
            if (opened_.contains(openedKey(path))) {  // Schild wandert mit
                setOpened(path, false);
                setOpened(target, true);
            }
            reload();
        }
    });
    m.addAction(icon(Ic::Trash, pal.text), L("Move to trash"), this, [this, path] {
        if (QMessageBox::question(this, "Clipline", L("Move \"%1\" to the trash?").arg(QFileInfo(path).fileName())) ==
            QMessageBox::Yes) {
            if (QFile::moveToTrash(path)) setOpened(path, false);
            reload();
        }
    });
    m.exec(view_->viewport()->mapToGlobal(pos));
}

void GalleryWindow::closeEvent(QCloseEvent* e) {
    ctl_->galleryClosed();
    QWidget::closeEvent(e);
}
