#include "setup.h"

#include <QApplication>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QPointer>
#include <memory>
#include <thread>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QScreen>
#include <QScrollArea>
#include <functional>
#include <QSlider>
#include <QStyle>
#include <cmath>
#include <QTabWidget>
#include <QVBoxLayout>
#include "i18n.h"
#include "look.h"
#if defined(Q_OS_LINUX)
#include "wayland_capture.h"
static bool waylandSession() { return WaylandCapture::isWaylandSession(); }
#else
static bool waylandSession() { return false; }
#endif

// ---------------------------------------------------------------- Mikrofon-Pegel
static constexpr double kMeterMin = -80.0;

MicMeter::MicMeter(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(16);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    timer_.setInterval(50);
    connect(&timer_, &QTimer::timeout, this, [this] { update(); });
}

void MicMeter::setDevice(const QString& mic) {
    device_ = mic;
    meter_.stop();
    timer_.stop();
    if (isVisible() && !device_.isEmpty() && meter_.start(device_)) timer_.start();
    update();
}

void MicMeter::setThreshold(int db) {
    threshold_ = db;
    update();
}

void MicMeter::showEvent(QShowEvent*) { setDevice(device_); }

void MicMeter::hideEvent(QHideEvent*) {
    meter_.stop();
    timer_.stop();
}

void MicMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    auto xOf = [&](double db) { return r.left() + (std::clamp(db, kMeterMin, 0.0) - kMeterMin) / -kMeterMin * r.width(); };
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(128, 128, 128, 45));
    p.drawRoundedRect(r, 4, 4);
    const double level = timer_.isActive() ? meter_.levelDb() : kMeterMin;
    const bool open = threshold_ <= -80 || level > threshold_;
    QRectF fill = r;
    fill.setRight(xOf(level));
    p.setBrush(open ? QColor("#35c46a") : QColor(140, 140, 140, 150));
    p.drawRoundedRect(fill, 4, 4);
    if (threshold_ > -80) {
        const double x = xOf(threshold_);
        p.setBrush(accent_);
        p.drawRoundedRect(QRectF(x - 1.5, r.top() - 1, 3, r.height() + 2), 1.5, 1.5);
    }
}

static const int kLengths[] = {10, 15, 20, 30, 45, 60, 90, 120, 180, 240, 300, 420, 600};
static constexpr int kLengthCount = int(sizeof(kLengths) / sizeof(kLengths[0]));

void applyLook(const Config& c) {
    setMinimalLook(c.minimal);
    qApp->setStyleSheet(styleSheet(makePalette(c.accent, c.dark)));
}

static QLabel* label(const QString& text, const char* obj = nullptr) {
    auto* l = new QLabel(text);
    if (obj) l->setObjectName(obj);
    l->setWordWrap(true);
    return l;
}

static QWidget* page(QVBoxLayout** out) {
    auto* w = new QWidget;
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(28, 24, 28, 18);
    v->setSpacing(10);
    *out = v;
    return w;
}

// ---------------------------------------------------------------- Qualitätsvorschau mit echten Fotos
namespace {
struct Photo {
    const char* file;
    double fx, fy;  // Bildmitte des Ausschnitts (0..1)
};
// Wikimedia Commons: Baltimore Oriole chicks – Juliancolton, public domain · Eisvogel – Frank-2.0, CC0 ·
// Erinaceus roumanicus – George Chernilevsky, public domain
const Photo kPhotos[] = {{":/photos/nest.jpg", 0.47, 0.45}, {":/photos/kingfisher.jpg", 0.36, 0.46}, {":/photos/hedgehog.jpg", 0.42, 0.60}};
constexpr int kPhotoCount = 3;
constexpr double kZoom = 1.0;  // ganzes Bild in der echten Aufnahmeauflösung (kein Ausschnitt)
}  // namespace

QualityPreview::QualityPreview(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
}

void QualityPreview::setCaptureHeight(int h, int nativeH) {
    height_ = h > 0 ? h : nativeH;
    native_ = nativeH;
    update();
}

QRectF QualityPreview::imageRect() const {
    QRectF r = rect().adjusted(1, 1, -1, -1);
    if (r.width() / r.height() > 16.0 / 9) r.setWidth(r.height() * 16 / 9);
    else r.setHeight(r.width() * 9 / 16);
    r.moveCenter(QRectF(rect()).center());
    return r;
}

QRectF QualityPreview::arrowRect(int dir) const {
    const QRectF r = imageRect();
    const double s = 36;
    return QRectF(dir < 0 ? r.left() + 10 : r.right() - 10 - s, r.center().y() - s / 2, s, s);
}

// Foto direkt in der Aufnahmeauflösung dekodieren (JPEG-Skalierung beim Lesen: schnell und sparsam),
// dann nur den gezeigten Ausschnitt behalten
void QualityPreview::ensureImage() {
    const int h = std::min(height_, native_);
    if (cachedH_ == h && cachedIdx_ == index_ && !cached_.isNull()) return;
    const Photo& ph = kPhotos[index_];
    const QSize full(int(std::lround(h * 16.0 / 9 / 2)) * 2, h);
    const QSize part(std::max(4, int(full.width() / kZoom)), std::max(4, int(full.height() / kZoom)));
    QRect clip(int(full.width() * ph.fx - part.width() / 2.0), int(full.height() * ph.fy - part.height() / 2.0),
               part.width(), part.height());
    clip.moveLeft(std::clamp(clip.left(), 0, full.width() - part.width()));
    clip.moveTop(std::clamp(clip.top(), 0, full.height() - part.height()));
    QImageReader reader(ph.file);
    reader.setScaledSize(full);
    reader.setScaledClipRect(clip);
    cached_ = reader.read();
    cachedH_ = h;
    cachedIdx_ = index_;
}

void QualityPreview::paintEvent(QPaintEvent*) {
    ensureImage();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = imageRect();
    QPainterPath clip;
    clip.addRoundedRect(r, 12, 12);
    p.setClipPath(clip);
    p.fillRect(r, Qt::black);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);  // wie ein Videoplayer: geglättet, nicht verpixelt
    if (!cached_.isNull()) p.drawImage(r, cached_);
    p.setClipping(false);

    // Pfeile
    for (int dir : {-1, 1}) {
        const QRectF a = arrowRect(dir);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, hover_ == dir ? 190 : 120));
        p.drawEllipse(a);
        QPen pen(Qt::white, 2.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const QPointF c = a.center();
        const double d = 5 * dir;
        p.drawPolyline(QPolygonF({c + QPointF(-d, -8), c + QPointF(d, 0), c + QPointF(-d, 8)}));
    }
    // Punkte
    for (int i = 0; i < kPhotoCount; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(i == index_ ? QColor(255, 255, 255) : QColor(255, 255, 255, 110));
        p.drawEllipse(QPointF(r.center().x() + (i - 1) * 14, r.bottom() - 14), 3.5, 3.5);
    }
    // Plakette mit echter Auflösung
    QFont f = font();
    f.setPixelSize(12);
    f.setBold(true);
    p.setFont(f);
    const int h = std::min(height_, native_);
    const QString t = QString("%1p · %2×%3").arg(h).arg(int(std::lround(h * 16.0 / 9 / 2)) * 2).arg(h);
    const double bw = QFontMetrics(f).horizontalAdvance(t) + 20;
    const QRectF b(r.left() + 10, r.bottom() - 32, bw, 22);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(b, 11, 11);
    p.setPen(Qt::white);
    p.drawText(b, Qt::AlignCenter, t);
}

void QualityPreview::mousePressEvent(QMouseEvent* e) {
    for (int dir : {-1, 1}) {
        if (arrowRect(dir).contains(e->position())) {
            index_ = (index_ + dir + kPhotoCount) % kPhotoCount;
            update();
            return;
        }
    }
}

void QualityPreview::mouseMoveEvent(QMouseEvent* e) {
    int h = 0;
    for (int dir : {-1, 1})
        if (arrowRect(dir).contains(e->position())) h = dir;
    if (h != hover_) {
        hover_ = h;
        setCursor(h ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void QualityPreview::leaveEvent(QEvent*) {
    hover_ = 0;
    update();
}

// ---------------------------------------------------------------- Hotkey-Aufnahme
HotkeyButton::HotkeyButton(const QString& seq, QWidget* parent) : QPushButton(parent), seq_(seq) {
    setObjectName("hotkey");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(54);
    hintTimer_.setSingleShot(true);
    connect(&hintTimer_, &QTimer::timeout, this, [this] { updateText(); });
    connect(this, &QPushButton::clicked, this, [this] { recording_ ? stopRecording() : startRecording(); });
    updateText();
}

void HotkeyButton::setSequence(const QString& seq) {
    seq_ = seq;
    if (recording_) stopRecording();
    updateText();
}

void HotkeyButton::updateText() {
    if (hintTimer_.isActive()) return;
    if (recording_) {
        setText(L("Press keys…"));
        return;
    }
    const QString native = QKeySequence(seq_).toString(QKeySequence::NativeText);
    setText(native.isEmpty() ? "—" : native.split('+').join("  +  "));
}

void HotkeyButton::startRecording() {
    recording_ = true;
    setProperty("recording", true);
    style()->unpolish(this);
    style()->polish(this);
    setFocus();
    grabKeyboard();
    updateText();
}

void HotkeyButton::stopRecording() {
    recording_ = false;
    setProperty("recording", false);
    style()->unpolish(this);
    style()->polish(this);
    releaseKeyboard();
    updateText();
}

// Taste übernehmen - oder kurz sagen, warum nicht
void HotkeyButton::take(Qt::KeyboardModifiers mods, int key) {
    const bool keypad = mods & Qt::KeypadModifier;
    const Qt::KeyboardModifiers real = mods & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (!real && !keyAllowedAlone(key, keypad)) {
        // Enter, Buchstaben usw. allein: die könnte man sonst in keinem Programm mehr tippen
        hintTimer_.start(2200);
        setText(L("Only together with Ctrl, Alt or Shift"));
        return;
    }
    seq_ = QKeySequence(QKeyCombination(real | (keypad ? Qt::KeypadModifier : Qt::NoModifier), Qt::Key(key))).toString(QKeySequence::PortableText);
    stopRecording();
    emit sequenceChanged(seq_);
}

bool HotkeyButton::event(QEvent* e) {
    if (recording_ && e->type() == QEvent::ShortcutOverride) {
        e->accept();
        return true;
    }
    if (recording_ && e->type() == QEvent::KeyPress) {  // sonst springt Tab zum nächsten Feld
        keyPressEvent(static_cast<QKeyEvent*>(e));
        return true;
    }
    return QPushButton::event(e);
}

void HotkeyButton::keyPressEvent(QKeyEvent* e) {
    if (!recording_) return QPushButton::keyPressEvent(e);
    int key = e->key();
    Qt::KeyboardModifiers mods = e->modifiers();
    if (key == Qt::Key_Escape && !(mods & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))) return stopRecording();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta ||
        key == Qt::Key_AltGr || key == Qt::Key_unknown || key == 0)
        return;  // warten, bis eine "echte" Taste kommt
    if (key == Qt::Key_Backtab) {  // Shift+Tab meldet Qt als eigene Taste
        key = Qt::Key_Tab;
        mods |= Qt::ShiftModifier;
    }
    // Ziffernblock nur für Ziffern und Rechenzeichen merken (Pfeile vom Ziffernblock sind normale Pfeile)
    const bool digitOrOp = (key >= Qt::Key_0 && key <= Qt::Key_9) || key == Qt::Key_Plus || key == Qt::Key_Minus ||
                           key == Qt::Key_Asterisk || key == Qt::Key_Slash || key == Qt::Key_Period || key == Qt::Key_Comma ||
                           key == Qt::Key_Enter;
    if (!digitOrOp) mods &= ~Qt::KeypadModifier;
    take(mods, key);
}

// Windows macht aus Shift+F10 (und der Menütaste) ein "Kontextmenü" - Qt meldet dann keinen Tastendruck
void HotkeyButton::contextMenuEvent(QContextMenuEvent* e) {
    if (!recording_ || e->reason() != QContextMenuEvent::Keyboard) return QPushButton::contextMenuEvent(e);
    const Qt::KeyboardModifiers mods = QGuiApplication::queryKeyboardModifiers() &
                                       (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (mods & Qt::ShiftModifier) take(mods, Qt::Key_F10);
    else take(mods, Qt::Key_Menu);
}

void HotkeyButton::focusOutEvent(QFocusEvent* e) {
    if (recording_) stopRecording();
    QPushButton::focusOutEvent(e);
}

// ---------------------------------------------------------------- Seiten
ConfigPages::ConfigPages(const Config& c, QObject* parent) : QObject(parent), cfg_(c) {
    monitors_ = listMonitors();
    if (cfg_.clipsDir.isEmpty()) cfg_.clipsDir = defaultClipsDir();
}

QSize ConfigPages::nativeSize(const QString& monitor) const {
    const Monitor m = findMonitor(monitor);
    if (!m.rect.isEmpty()) return m.rect.size();
    QScreen* s = QGuiApplication::primaryScreen();
    return s ? s->size() * s->devicePixelRatio() : QSize(1920, 1080);
}

ClipButton ConfigPages::button(int i) const { return cfg_.allButtons().value(i); }

void ConfigPages::setButton(int i, const ClipButton& b) {
    if (i == 0) {
        cfg_.hotkey = b.hotkey;
        cfg_.clipSeconds = b.seconds;
        cfg_.height = b.height;
        cfg_.fps = b.fps;
        cfg_.quality = b.quality;
        cfg_.monitor = b.monitor;
    } else if (i - 1 < cfg_.extraButtons.size()) {
        cfg_.extraButtons[i - 1] = b;
    }
}

// Arbeitsspeicher: pro Bildschirm eine Aufnahme, jeweils für den anspruchsvollsten Knopf dort
void ConfigPages::updateRam() {
    double total = 0, buffers = 0;
    QStringList parts;
    const QStringList mons = cfg_.recordingMonitors();
    for (int k = 0; k < mons.size(); ++k) {
        const Config rec = cfg_.recordingConfig(mons[k]);
        const QSize n = nativeSize(mons[k]), rs = captureSize(rec, n);
        const double buf = estimateBufferMB(rec, n);
        total += estimateTotalMB(rec, n) - (k > 0 ? 30 : 0);  // das Programm selbst läuft nur einmal
        buffers += buf;
        parts << QString("%1×%2 · %3 fps · %4 Mbit/s").arg(rs.width()).arg(rs.height()).arg(rec.fps).arg(videoBitrate(rec, n) / 1e6, 0, 'f', 1);
    }
    const int pi = std::clamp(previewIdx_, 0, int(cfg_.allButtons().size()) - 1);
    const ClipButton pb = button(pi);
    const QSize pn = nativeSize(pb.monitor);
    if (ramLabel_) ramLabel_->setText("≈ " + fmtMB(total));
    if (ramBar_) ramBar_->setValue(int(std::min(2048.0, total)));
    if (diskLabel_) diskLabel_->setText("≈ " + fmtMB(estimateClipMB(cfg_.withButton(pb), pn)));
    if (diskTitle_) diskTitle_->setText(L("Size per clip (clip button %1)").arg(pi + 1));
    if (ramDetail_) ramDetail_->setText(QString("%1 %2 · %3").arg(L("buffer")).arg(fmtMB(buffers)).arg(parts.join("  ·  ")));
    if (multiNote_) multiNote_->setVisible(cfg_.allButtons().size() > 1);
    if (preview_) {
        preview_->setCaptureHeight(captureSize(cfg_.withButton(pb), pn).height(), pn.height());
        if (previewTitle_) previewTitle_->setText(L("Preview: clip button %1").arg(pi + 1));
    }
}

// Taste, die noch kein Knopf hat (für einen neuen Clip-Knopf)
static QString freeHotkey(const Config& c) {
    QStringList used = {c.hotkey, c.muteHotkey};
    for (const ClipButton& b : c.extraButtons) used << b.hotkey;
    for (const char* k : {"F10", "F8", "F7", "F6", "Ctrl+F9", "Ctrl+F10", "Ctrl+Alt+D", "Ctrl+Alt+F"})
        if (!used.contains(k)) return k;
    return QString();
}

void ConfigPages::checkDuplicates() {
    if (!dupWarn_) return;
    QStringList keys;
    for (const ClipButton& b : cfg_.allButtons()) keys << b.hotkey;
    keys.removeAll(QString());
    bool dup = false;
    for (int i = 0; i < keys.size(); ++i) dup |= keys.indexOf(keys[i], i + 1) >= 0;
    dupWarn_->setVisible(dup);
}

static QWidget* segmented(const QStringList& names, int current, QButtonGroup** out) {
    auto* seg = new QWidget;
    auto* sl = new QHBoxLayout(seg);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    auto* group = new QButtonGroup(seg);
    for (int q = 0; q < names.size(); ++q) {
        auto* qb = new QPushButton(names[q]);
        qb->setObjectName("seg");
        qb->setCheckable(true);
        qb->setChecked(current == q);
        group->addButton(qb, q);
        sl->addWidget(qb);
    }
    sl->addStretch();
    *out = group;
    return seg;
}

// Karte für Clip-Knopf i+1 (0 = Knopf 1): Taste, Länge, Bildschirm, Auflösung, Bildrate, Detail - ab Knopf 2 auch Entfernen
QWidget* ConfigPages::buttonCard(int i) {
    auto* card = new QFrame;
    card->setObjectName("card");
    auto* cv = new QVBoxLayout(card);
    cv->setContentsMargins(16, 12, 16, 14);
    cv->setSpacing(8);
    auto* head = new QHBoxLayout;
    auto* title = label("<b>" + L("Clip button %1").arg(i + 1) + "</b>" +
                        (i == 0 ? QString("  <span style='font-weight:400'>· %1</span>").arg(L("main key")) : QString()));
    title->setWordWrap(false);
    head->addWidget(title);
    head->addStretch();
    if (i > 0) {
        auto* del = new QPushButton(icon(Ic::Trash, makePalette(cfg_.accent, cfg_.dark).muted), QString());
        del->setObjectName("flat");
        del->setToolTip(L("Delete"));
        del->setCursor(Qt::PointingHandCursor);
        head->addWidget(del);
        connect(del, &QPushButton::clicked, this, [this, i] {
            if (i - 1 >= cfg_.extraButtons.size()) return;
            cfg_.extraButtons.removeAt(i - 1);
            previewIdx_ = 0;
            QTimer::singleShot(0, this, [this] { rebuildButtons(); });  // nicht im Klick des Knopfs selbst löschen
            updateRam();
            emit changed();
        });
    }
    cv->addLayout(head);
    const ClipButton b = button(i);
    auto* key = new HotkeyButton(b.hotkey);
    cv->addWidget(key);
    auto* form = new QFormLayout;
    form->setSpacing(8);
    auto* len = new QComboBox;
    for (int k = 0; k < kLengthCount; ++k) len->addItem(fmtDuration(kLengths[k]), kLengths[k]);
    int li = len->findData(b.seconds);
    if (li < 0) {  // alte Länge, die es in der Liste nicht gibt: die nächstgrößere nehmen
        li = kLengthCount - 1;
        for (int k = kLengthCount - 1; k >= 0; --k)
            if (kLengths[k] >= b.seconds) li = k;
    }
    len->setCurrentIndex(li);
    auto* screen = new QComboBox;
    for (const Monitor& m : monitors_) screen->addItem(m.name, m.id);
    screen->setCurrentIndex(std::max(0, screen->findData(findMonitor(b.monitor).id)));
    auto* res = new QComboBox;
    auto* fps = new QComboBox;
    auto fill = [this, i, res, fps] {
        const ClipButton cur = button(i);
        const QSize n = nativeSize(cur.monitor);
        QSignalBlocker b1(res), b2(fps);
        res->clear();
        res->addItem(QString("%1 (%2p)").arg(L("Native")).arg(n.height()), 0);
        for (int h : {2160, 1440, 1080, 900, 720, 480, 360})
            if (h < n.height()) res->addItem(QString("%1p").arg(h), h);
        res->setCurrentIndex(std::max(0, res->findData(cur.height)));
        const double hz = findMonitor(cur.monitor).refresh;
        fps->clear();
        for (int f : {30, 60, 120, 144, 165, 240})
            if (f <= std::max(60.0, hz + 1)) fps->addItem(QString("%1 fps").arg(f), f);
        int j = fps->findData(cur.fps);
        if (j < 0) j = std::max(0, fps->findData(60));
        fps->setCurrentIndex(j);
    };
    fill();
    QButtonGroup* group = nullptr;
    QWidget* seg = segmented({L("Small"), L("Balanced"), L("High")}, b.quality, &group);
    form->addRow(L("Clip length"), len);
    // Unter Wayland wählt man den Bildschirm im Freigabe-Dialog des Systems
    if (monitors_.size() > 1 && !waylandSession()) form->addRow(L("Screen"), screen);
    else screen->hide();
    form->addRow(L("Resolution"), res);
    form->addRow(L("Frame rate"), fps);
    form->addRow(L("Detail"), seg);
    cv->addLayout(form);

    auto edit = [this, i](std::function<void(ClipButton&)> fn) {
        ClipButton cur = button(i);
        fn(cur);
        setButton(i, cur);
        previewIdx_ = i;
        updateRam();
        checkDuplicates();
        emit changed();
    };
    connect(key, &HotkeyButton::sequenceChanged, this, [edit](const QString& s) { edit([s](ClipButton& c) { c.hotkey = s; }); });
    connect(len, &QComboBox::currentIndexChanged, this, [edit, len] { const int v = len->currentData().toInt(); edit([v](ClipButton& c) { c.seconds = v; }); });
    connect(screen, &QComboBox::currentIndexChanged, this, [edit, screen, fill, res, fps] {
        const QString v = screen->currentData().toString();
        edit([v](ClipButton& c) { c.monitor = v; });
        fill();
        const int h = res->currentData().toInt(), f = fps->currentData().toInt();
        edit([h, f](ClipButton& c) { c.height = h; c.fps = f; });
    });
    connect(res, &QComboBox::currentIndexChanged, this, [edit, res] { const int v = res->currentData().toInt(); edit([v](ClipButton& c) { c.height = v; }); });
    connect(fps, &QComboBox::currentIndexChanged, this, [edit, fps] { const int v = fps->currentData().toInt(); edit([v](ClipButton& c) { c.fps = v; }); });
    connect(group, &QButtonGroup::idClicked, this, [edit](int q) { edit([q](ClipButton& c) { c.quality = q; }); });
    return card;
}

void ConfigPages::rebuildButtons() {
    if (!buttonsBox_) return;
    while (QLayoutItem* it = buttonsBox_->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    for (int i = 0; i < cfg_.allButtons().size(); ++i) buttonsBox_->addWidget(buttonCard(i));
    checkDuplicates();
    updateRam();
}

static QScrollArea* scrollable(QWidget* w) {
    // Lange Seiten: bei kleinen Fenstern scrollen statt quetschen
    auto* scroll = new QScrollArea;
    scroll->setObjectName("plain");
    scroll->setWidget(w);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea#plain { border: none; background: transparent; }"
                          "QScrollArea#plain > QWidget > QWidget { background: transparent; }");
    enableSmoothScroll(scroll);
    return scroll;
}

// 1: Ordner, Sprache, Autostart
QWidget* ConfigPages::folderPage(bool welcome) {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    if (welcome) {
        v->addWidget(label(L("Welcome to Clipline"), "h1"));
        v->addWidget(label(L("Clipline records your screen quietly in the background. Press a key and the last moments are saved as a clip – nothing is written to disk until you do."), "muted"));
        v->addSpacing(10);
    }
    v->addWidget(label(L("Where should your clips go?"), "h2"));
    auto* row = new QHBoxLayout;
    auto* edit = new QLineEdit(QDir::toNativeSeparators(cfg_.clipsDir));
    auto* pick = new QPushButton(icon(Ic::Folder, makePalette(cfg_.accent, cfg_.dark).text), " " + L("Choose…"));
    row->addWidget(edit, 1);
    row->addWidget(pick);
    v->addLayout(row);
    connect(edit, &QLineEdit::textChanged, this, [this](const QString& t) { cfg_.clipsDir = QDir::fromNativeSeparators(t.trimmed()); emit changed(); });
    connect(pick, &QPushButton::clicked, w, [w, edit] {
        const QString d = QFileDialog::getExistingDirectory(w, L("Clips folder"), edit->text());
        if (!d.isEmpty()) edit->setText(QDir::toNativeSeparators(d));
    });
    v->addSpacing(10);
    auto* form = new QFormLayout;
    auto* lang = new QComboBox;
    lang->addItem(L("System"), "");
    const QStringList codes = languageCodes(), names = languageNames();
    for (int i = 0; i < codes.size(); ++i) lang->addItem(names[i], codes[i]);
    lang->setCurrentIndex(std::max(0, lang->findData(cfg_.language)));
    connect(lang, &QComboBox::currentIndexChanged, this, [this, lang] { cfg_.language = lang->currentData().toString(); emit changed(); });
    form->addRow(L("Language"), lang);
    v->addLayout(form);
    v->addSpacing(10);
    auto* autostart = new QCheckBox(L("Start with the computer (runs in the background)"));
    autostart->setChecked(cfg_.autostart);
    connect(autostart, &QCheckBox::toggled, this, [this](bool on) { cfg_.autostart = on; emit changed(); });
    v->addWidget(autostart);
    v->addStretch();
    return w;
}

// 2: Ton - Systemton, Mikrofon (mit Test), Stummtaste, Ton beim Speichern
QWidget* ConfigPages::soundPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Sound"), "h1"));
    v->addWidget(label(L("What should be heard in your clips?"), "muted"));
    v->addSpacing(4);

    // Regler mit Titel und Prozentangabe (Lautstärken 0..200 %)
    auto volume = [](const QString& title, int value, std::function<void(int)> set) {
        auto* box = new QWidget;
        auto* bv = new QVBoxLayout(box);
        bv->setContentsMargins(0, 0, 0, 0);
        bv->setSpacing(4);
        auto* head = label(QString());
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 200);
        slider->setSingleStep(5);
        slider->setPageStep(10);
        slider->setValue(value);
        auto show = [head, title](int p) { head->setText("<b>" + title + "</b>  " + QString("%1 %").arg(p)); };
        show(value);
        QObject::connect(slider, &QSlider::valueChanged, box, [show, set](int p) { show(p); set(p); });
        bv->addWidget(head);
        bv->addWidget(slider);
        return box;
    };

    auto* sys = new QCheckBox(L("System sound"));
    sys->setChecked(cfg_.systemAudio && systemAudioSupported());
    sys->setEnabled(systemAudioSupported());
    if (!systemAudioSupported()) sys->setToolTip(L("Not available on this system"));
    v->addWidget(sys);
    auto* sysVol = volume(L("System sound volume"), cfg_.systemVolume, [this](int p) { cfg_.systemVolume = p; emit changed(); });
    sysVol->setVisible(sys->isChecked());
    v->addWidget(sysVol);
    v->addSpacing(8);

    auto* form = new QFormLayout;
    auto* mic = new QComboBox;
    mic->addItem(L("None"), "");
    for (const QString& m : listMicrophones()) mic->addItem(m, m);
    mic->setCurrentIndex(std::max(0, mic->findData(cfg_.mic)));
    form->addRow(L("Microphone"), mic);
    v->addLayout(form);

    // Alles, was nur mit Mikrofon Sinn ergibt - ohne Mikrofon eingeklappt
    auto* micBox = new QFrame;
    micBox->setObjectName("card");
    auto* mv = new QVBoxLayout(micBox);
    mv->setContentsMargins(16, 12, 16, 14);
    mv->setSpacing(9);
    mv->addWidget(volume(L("Microphone volume"), cfg_.micVolume, [this](int p) { cfg_.micVolume = p; emit changed(); }));

    auto* denoise = new QCheckBox(L("Filter out keyboard and mouse clicks (AI noise suppression)"));
    denoise->setChecked(cfg_.micDenoise);
    mv->addWidget(denoise);

    // Empfindlichkeit: Rauschsperre mit Live-Pegel, damit Hintergrundgeräusche nicht im Clip landen
    auto* sensTitle = label(QString());
    mv->addWidget(sensTitle);
    auto* meter = new MicMeter;
    meter->setAccent(makePalette(cfg_.accent, cfg_.dark).accent);
    meter->setDenoise(cfg_.micDenoise);
    mv->addWidget(meter);
    auto* gate = new QSlider(Qt::Horizontal);
    gate->setRange(-80, -10);
    gate->setValue(cfg_.micGate);
    mv->addWidget(gate);
    auto* ends = new QHBoxLayout;
    ends->addWidget(label(L("Record everything"), "muted"));
    ends->addStretch();
    auto* endR = label(L("Only loud voices"), "muted");
    endR->setAlignment(Qt::AlignRight);
    ends->addWidget(endR);
    mv->addLayout(ends);
    mv->addWidget(label(L("Talk normally: the bar should turn green when you speak and stay grey when you are quiet. "
                          "Everything below the line is muted, so background noise stays out of your clips."), "muted"));

    // Mikrofon testen: ein paar Sekunden aufnehmen und mit allen Filtern vorspielen
    auto* testRow = new QHBoxLayout;
    auto* test = new QPushButton(L("Test microphone"));
    test->setCursor(Qt::PointingHandCursor);
    auto* testHint = label(L("Records 4 seconds and plays them back – exactly as they would sound in a clip."), "muted");
    testRow->addWidget(test);
    testRow->addWidget(testHint, 1);
    mv->addLayout(testRow);
    connect(test, &QPushButton::clicked, this, [this, test, testHint] {
        if (!test->isEnabled() || cfg_.mic.isEmpty()) return;
        test->setEnabled(false);
        MicTuning t;
        t.gateDb = cfg_.micGate;
        t.micVolume = cfg_.micVolume;
        t.systemVolume = cfg_.systemVolume;
        t.denoise = cfg_.micDenoise;
        const QString mic = cfg_.mic, wav = QDir::temp().filePath("clipline-mictest.wav");
        const QPointer<QPushButton> btn(test);
        const QPointer<QLabel> hint(testHint);
        auto* tick = new QTimer(test);
        auto left = std::make_shared<int>(4);
        test->setText(L("Speak now… %1").arg(*left));
        connect(tick, &QTimer::timeout, test, [test, left] {
            if (--*left > 0) test->setText(L("Speak now… %1").arg(*left));
        });
        tick->start(1000);
        std::thread([=] {
            QString err;
            const bool ok = recordMicTest(mic, 4.0, t, wav, &err);
            QMetaObject::invokeMethod(qApp, [=] {
                if (!btn) return;
                tick->deleteLater();
                if (!ok) {
                    btn->setEnabled(true);
                    btn->setText(L("Test microphone"));
                    if (hint) hint->setText(L("The microphone could not be opened.") + (err.isEmpty() ? QString() : " (" + err + ")"));
                    return;
                }
                btn->setText(L("Playing back…"));
                playWavFile(wav);
                QTimer::singleShot(4300, btn, [btn] {
                    if (!btn) return;
                    btn->setEnabled(true);
                    btn->setText(L("Test microphone"));
                });
            }, Qt::QueuedConnection);
        }).detach();
    });

    // Mikrofon stumm per Taste
    mv->addSpacing(4);
    mv->addWidget(label("<b>" + L("Mute key for the microphone") + "</b>"));
    auto* muteRow = new QHBoxLayout;
    auto* muteKey = new HotkeyButton(cfg_.muteHotkey);
    auto* muteClear = new QPushButton(L("Remove"));
    muteClear->setCursor(Qt::PointingHandCursor);
    muteClear->setVisible(!cfg_.muteHotkey.isEmpty());
    muteRow->addWidget(muteKey, 1);
    muteRow->addWidget(muteClear);
    mv->addLayout(muteRow);
    connect(muteKey, &HotkeyButton::sequenceChanged, this, [this, muteClear](const QString& s) {
        cfg_.muteHotkey = s;
        muteClear->setVisible(!s.isEmpty());
        emit changed();
    });
    connect(muteClear, &QPushButton::clicked, this, [this, muteKey, muteClear] {
        muteKey->setSequence(QString());
        cfg_.muteHotkey.clear();
        muteClear->hide();
        emit changed();
    });
    mv->addWidget(label(L("Press it once and your voice is no longer recorded, press it again to switch the microphone back on."), "muted"));
    v->addWidget(micBox);

    auto showGate = [sensTitle, meter](int db) {
        sensTitle->setText("<b>" + L("Microphone sensitivity") + "</b>  " + (db <= -80 ? L("Off") : QString("%1 dB").arg(db)));
        meter->setThreshold(db);
    };
    showGate(cfg_.micGate);
    micBox->setVisible(!mic->currentData().toString().isEmpty());
    meter->setDevice(mic->currentData().toString());
    connect(gate, &QSlider::valueChanged, this, [this, showGate](int db) { cfg_.micGate = db; showGate(db); emit changed(); });
    connect(denoise, &QCheckBox::toggled, this, [this, meter](bool on) {
        cfg_.micDenoise = on;
        meter->setDenoise(on);
        emit changed();
    });
    connect(sys, &QCheckBox::toggled, this, [this, sysVol](bool on) {
        cfg_.systemAudio = on;
        sysVol->setVisible(on);
        emit changed();
    });
    connect(mic, &QComboBox::currentIndexChanged, this, [this, mic, micBox, meter] {
        cfg_.mic = mic->currentData().toString();
        micBox->setVisible(!cfg_.mic.isEmpty());
        meter->setDevice(cfg_.mic);
        emit changed();
    });
    v->addSpacing(12);
    auto* sound = new QCheckBox(L("Play a sound when a clip is saved"));
    sound->setChecked(cfg_.sound);
    connect(sound, &QCheckBox::toggled, this, [this](bool on) { cfg_.sound = on; emit changed(); });
    v->addWidget(sound);
    v->addStretch();
    return scrollable(w);
}

// 3: Clip-Tasten - jede mit eigener Länge, Bildschirm und Grafik, darunter Vorschau und Arbeitsspeicher
QWidget* ConfigPages::buttonsPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Your clip keys"), "h1"));
    v->addWidget(label(L("Each key saves its own clip: choose how long it is, which screen and in what quality. "
                         "Click on the key field and press the key you want."), "muted"));
    v->addSpacing(4);
    buttonsBox_ = new QVBoxLayout;
    buttonsBox_->setSpacing(10);
    v->addLayout(buttonsBox_);
    auto* add = new QPushButton("+   " + L("Add another clip button"));
    add->setObjectName("dashed");
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, [this] {
        ClipButton b;
        b.hotkey = freeHotkey(cfg_);
        b.seconds = 30;
        b.height = cfg_.height;
        b.fps = cfg_.fps;
        b.quality = cfg_.quality;
        b.monitor = cfg_.monitor;
        cfg_.extraButtons << b;
        previewIdx_ = int(cfg_.extraButtons.size());
        rebuildButtons();
        emit changed();
    });
    v->addWidget(add);
    dupWarn_ = label(L("Two clip buttons use the same key."));
    dupWarn_->setStyleSheet(QString("color:%1;font-weight:600;").arg(makePalette(cfg_.accent, cfg_.dark).accent.name()));
    v->addWidget(dupWarn_);
    v->addWidget(label(L("Tip: the key also works while you hold Shift, Ctrl or Alt (e.g. while running in a game)."), "muted"));
    v->addSpacing(8);

    previewTitle_ = label(QString(), "h2");
    v->addWidget(previewTitle_);
    preview_ = new QualityPreview;
    preview_->setMinimumHeight(200);
    v->addWidget(preview_);

    // Arbeitsspeicher
    auto* box = new QFrame;
    box->setObjectName("card");
    auto* bv = new QVBoxLayout(box);
    bv->setContentsMargins(16, 10, 16, 12);
    bv->setSpacing(5);
    auto* top = new QHBoxLayout;
    top->addWidget(label(L("Estimated RAM"), "muted"));
    top->addStretch();
    ramLabel_ = new QLabel;
    ramLabel_->setObjectName("h2");
    top->addWidget(ramLabel_);
    bv->addLayout(top);
    ramBar_ = new QProgressBar;
    ramBar_->setRange(0, 2048);
    ramBar_->setTextVisible(false);
    ramBar_->setFixedHeight(8);
    bv->addWidget(ramBar_);
    auto* disk = new QHBoxLayout;
    diskTitle_ = label(QString(), "muted");
    diskTitle_->setWordWrap(false);
    disk->addWidget(diskTitle_);
    disk->addStretch();
    diskLabel_ = new QLabel;
    diskLabel_->setObjectName("h2");
    disk->addWidget(diskLabel_);
    bv->addLayout(disk);
    ramDetail_ = label(QString(), "muted");
    bv->addWidget(ramDetail_);
    multiNote_ = label(L("Each screen is recorded once, for its most demanding clip button; clips with less are scaled down when saved."), "muted");
    bv->addWidget(multiNote_);
    v->addWidget(box);
    v->addStretch();
    rebuildButtons();
    return scrollable(w);
}

// 4: Aussehen
QWidget* ConfigPages::lookPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Make it yours"), "h1"));
    auto* form = new QFormLayout;
    form->setSpacing(14);
    auto* sw = new QWidget;
    auto* sh = new QHBoxLayout(sw);
    sh->setContentsMargins(0, 0, 0, 0);
    sh->setSpacing(8);
    auto* group = new QButtonGroup(sw);
    bool known = false;
    for (const QColor& c : accentChoices()) {
        auto* b = new QPushButton;
        b->setObjectName("swatch");
        b->setCheckable(true);
        b->setStyleSheet(QString("background:%1;").arg(c.name()));
        const bool on = c.name().compare(cfg_.accent, Qt::CaseInsensitive) == 0;
        known |= on;
        b->setChecked(on);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b);
        sh->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, c] { cfg_.accent = c.name(); emit lookChanged(); emit changed(); });
    }
    // Eigene Farbe: jede beliebige über den Farbwähler
    auto* own = new QPushButton;
    own->setObjectName("swatch");
    own->setCheckable(true);
    own->setChecked(!known);
    own->setToolTip(L("Any colour…"));
    own->setCursor(Qt::PointingHandCursor);
    own->setStyleSheet(known ? QString("background: qconicalgradient(cx:0.5, cy:0.5, angle:90, stop:0 #ff4d4d, stop:0.17 #ffd24d, "
                                       "stop:0.33 #4dd96b, stop:0.5 #4dd2ff, stop:0.67 #5b6bff, stop:0.83 #d24dff, stop:1 #ff4d4d);")
                             : QString("background:%1;").arg(cfg_.accent));
    group->addButton(own);
    sh->addWidget(own);
    auto* pick = new QPushButton(L("Any colour…"));
    pick->setCursor(Qt::PointingHandCursor);
    sh->addWidget(pick);
    auto choose = [this, w, own] {
        const QColor c = QColorDialog::getColor(QColor(cfg_.accent), w, L("Accent colour"));
        if (!c.isValid()) return;
        cfg_.accent = c.name();
        own->setStyleSheet(QString("background:%1;").arg(c.name()));
        own->setChecked(true);
        emit lookChanged();
        emit changed();
    };
    connect(pick, &QPushButton::clicked, this, choose);
    connect(own, &QPushButton::clicked, this, choose);
    sh->addStretch();
    form->addRow(L("Accent colour"), sw);
    QButtonGroup* mg = nullptr;
    QWidget* mode = segmented({L("Dark"), L("Light")}, cfg_.dark ? 0 : 1, &mg);
    connect(mg, &QButtonGroup::idClicked, this, [this](int id) { cfg_.dark = id == 0; emit lookChanged(); emit changed(); });
    form->addRow(L("Mode"), mode);
    // Stil: Standard (bisheriges Aussehen) oder Minimal (ruhiger, ohne Rahmen)
    QButtonGroup* sg = nullptr;
    QWidget* style = segmented({L("Standard"), L("Minimal")}, cfg_.minimal ? 1 : 0, &sg);
    connect(sg, &QButtonGroup::idClicked, this, [this](int id) { cfg_.minimal = id == 1; emit lookChanged(); emit changed(); });
    form->addRow(L("Style"), style);
    v->addLayout(form);
    v->addStretch();
    return w;
}

// ---------------------------------------------------------------- Assistent (Erststart)
SetupWizard::SetupWizard(const Config& c, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Clipline");
    setWindowIcon(cliplineIcon());
    resize(920, 680);
    pages_ = new ConfigPages(c, this);
    connect(pages_, &ConfigPages::lookChanged, this, [this] { applyLook(pages_->config()); go(step_); });

    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    auto* side = new QFrame;
    side->setObjectName("side");
    side->setFixedWidth(220);
    auto* sv = new QVBoxLayout(side);
    sv->setContentsMargins(22, 26, 22, 22);
    sv->setSpacing(6);
    auto* logo = new QLabel;
    logo->setPixmap(cliplineLogo(56));
    sv->addWidget(logo);
    sv->addWidget(label("Clipline", "h2"));
    sv->addSpacing(22);
    const QStringList names = {L("Clips folder"), L("Sound"), L("Clip keys"), L("Look")};
    for (int i = 0; i < names.size(); ++i) {
        auto* l = new QLabel(QString("%1   %2").arg(i + 1).arg(names[i]));
        l->setMinimumHeight(30);
        steps_ << l;
        sv->addWidget(l);
    }
    sv->addStretch();
    h->addWidget(side);

    auto* right = new QWidget;
    right->setObjectName("root");
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(0, 0, 0, 0);
    stack_ = new QStackedWidget;
    stack_->addWidget(pages_->folderPage(true));
    stack_->addWidget(pages_->soundPage());
    stack_->addWidget(pages_->buttonsPage());
    stack_->addWidget(pages_->lookPage());
    rv->addWidget(stack_, 1);
    auto* nav = new QHBoxLayout;
    nav->setContentsMargins(28, 0, 28, 22);
    back_ = new QPushButton(L("Back"));
    next_ = new QPushButton;
    next_->setObjectName("primary");
    next_->setDefault(true);
    nav->addWidget(back_);
    nav->addStretch();
    nav->addWidget(next_);
    rv->addLayout(nav);
    h->addWidget(right, 1);

    connect(back_, &QPushButton::clicked, this, [this] { go(step_ - 1); });
    connect(next_, &QPushButton::clicked, this, [this] {
        if (step_ == stack_->count() - 1) accept();
        else go(step_ + 1);
    });
    go(0);
}

void SetupWizard::go(int step) {
    step_ = std::clamp(step, 0, stack_->count() - 1);
    stack_->setCurrentIndex(step_);
    back_->setVisible(step_ > 0);
    next_->setText(step_ == stack_->count() - 1 ? L("Start recording") : L("Next"));
    const Palette p = makePalette(pages_->config().accent, pages_->config().dark);
    for (int i = 0; i < steps_.size(); ++i)
        steps_[i]->setStyleSheet(i == step_ ? QString("color:%1;font-weight:700;").arg(p.accent.name())
                                            : QString("color:%1;").arg(p.muted.name()));
}

// ---------------------------------------------------------------- Einstellungen
SettingsDialog::SettingsDialog(const Config& c, QWidget* parent) : QDialog(parent) {
    setWindowTitle(L("Settings") + " – Clipline");
    setWindowIcon(cliplineIcon());
    resize(780, 720);
    pages_ = new ConfigPages(c, this);
    connect(pages_, &ConfigPages::lookChanged, this, [this] { applyLook(pages_->config()); });
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(18, 18, 18, 18);
    auto* tabs = new QTabWidget;
    tabs->addTab(pages_->folderPage(false), L("General"));
    tabs->addTab(pages_->soundPage(), L("Sound"));
    tabs->addTab(pages_->buttonsPage(), L("Clip keys"));
    tabs->addTab(pages_->lookPage(), L("Look"));
    v->addWidget(tabs, 1);
    auto* row = new QHBoxLayout;
    auto* cancel = new QPushButton(L("Cancel"));
    auto* ok = new QPushButton(L("Save"));
    ok->setObjectName("primary");
    ok->setDefault(true);
    row->addStretch();
    row->addWidget(cancel);
    row->addWidget(ok);
    v->addLayout(row);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
}
