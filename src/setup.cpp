#include "setup.h"

#include <QApplication>
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
    connect(this, &QPushButton::clicked, this, [this] { recording_ ? stopRecording() : startRecording(); });
    updateText();
}

void HotkeyButton::updateText() {
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

void HotkeyButton::keyPressEvent(QKeyEvent* e) {
    if (!recording_) return QPushButton::keyPressEvent(e);
    const int key = e->key();
    if (key == Qt::Key_Escape) return stopRecording();
    if (allowClear_ && (key == Qt::Key_Backspace || key == Qt::Key_Delete) && !(e->modifiers() & ~Qt::KeypadModifier)) {
        seq_.clear();  // Taste entfernen (z. B. keine Mikrofon-Stummtaste)
        stopRecording();
        emit sequenceChanged(seq_);
        return;
    }
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta ||
        key == Qt::Key_AltGr || key == Qt::Key_unknown)
        return;  // warten, bis eine "echte" Taste kommt
    const Qt::KeyboardModifiers mods =
        e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    const bool fkey = key >= Qt::Key_F1 && key <= Qt::Key_F24;
    if (!mods && !fkey && key != Qt::Key_Print && key != Qt::Key_Pause && key != Qt::Key_ScrollLock) return;
    seq_ = QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
    stopRecording();
    emit sequenceChanged(seq_);
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

QSize ConfigPages::nativeSize() const {
    const Monitor m = findMonitor(cfg_.monitor);
    if (!m.rect.isEmpty()) return m.rect.size();
    QScreen* s = QGuiApplication::primaryScreen();
    return s ? s->size() * s->devicePixelRatio() : QSize(1920, 1080);
}

void ConfigPages::updateRam() {
    const QSize n = nativeSize();
    const QSize s = captureSize(cfg_, n);
    const double total = estimateTotalMB(cfg_, n), buf = estimateBufferMB(cfg_, n);
    if (ramLabel_) ramLabel_->setText("≈ " + fmtMB(total));
    if (ramBar_) ramBar_->setValue(int(std::min(2048.0, total)));
    if (diskLabel_) diskLabel_->setText("≈ " + fmtMB(estimateClipMB(cfg_, n)));
    if (ramDetail_)
        ramDetail_->setText(QString("%1 %2 · %3×%4 · %5 fps · %6 Mbit/s")
                                .arg(L("buffer")).arg(fmtMB(buf)).arg(s.width()).arg(s.height()).arg(cfg_.fps)
                                .arg(videoBitrate(cfg_, n) / 1e6, 0, 'f', 1));
    if (lenLabel_) lenLabel_->setText(fmtDuration(cfg_.clipSeconds));
    if (preview_) preview_->setCaptureHeight(s.height(), n.height());
}

// 1: Ordner + Sprache
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
    v->addStretch();
    return w;
}

// 2: Cliplänge + Qualität + RAM + Vorschau
QWidget* ConfigPages::recordingPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Clip length & quality"), "h1"));

    // Cliplänge
    auto* lenRow = new QHBoxLayout;
    lenRow->addWidget(label(L("Clip length"), "h2"));
    lenRow->addStretch();
    lenLabel_ = new QLabel;
    lenLabel_->setObjectName("big");
    lenRow->addWidget(lenLabel_);
    v->addLayout(lenRow);
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, kLengthCount - 1);
    int idx = 0;
    for (int i = 0; i < kLengthCount; ++i)
        if (kLengths[i] <= cfg_.clipSeconds) idx = i;
    slider->setValue(idx);
    slider->setPageStep(1);
    v->addWidget(slider);
    connect(slider, &QSlider::valueChanged, this, [this](int i) { cfg_.clipSeconds = kLengths[i]; updateRam(); emit changed(); });

    // Qualität
    auto* cols = new QHBoxLayout;
    cols->setSpacing(18);
    auto* form = new QFormLayout;
    form->setSpacing(9);
    auto* screen = new QComboBox;
    for (const Monitor& m : monitors_) screen->addItem(m.name, m.id);
    screen->setCurrentIndex(std::max(0, screen->findData(findMonitor(cfg_.monitor).id)));
    if (monitors_.size() > 1) form->addRow(L("Screen"), screen);
    else screen->hide();
    res_ = new QComboBox;
    fps_ = new QComboBox;
    auto fillRes = [this] {
        const QSize n = nativeSize();
        QSignalBlocker b1(res_), b2(fps_);
        res_->clear();
        res_->addItem(QString("%1 (%2p)").arg(L("Native")).arg(n.height()), 0);
        for (int h : {2160, 1440, 1080, 900, 720, 480, 360})
            if (h < n.height()) res_->addItem(QString("%1p").arg(h), h);
        int i = res_->findData(cfg_.height);
        if (i < 0) { i = 0; cfg_.height = 0; }
        res_->setCurrentIndex(i);
        const double hz = findMonitor(cfg_.monitor).refresh;
        fps_->clear();
        for (int f : {30, 60, 120, 144})
            if (f <= std::max(60.0, hz + 1)) fps_->addItem(QString("%1 fps").arg(f), f);
        int j = fps_->findData(cfg_.fps);
        if (j < 0) { j = fps_->findData(60); cfg_.fps = 60; }
        fps_->setCurrentIndex(std::max(0, j));
    };
    fillRes();
    form->addRow(L("Resolution"), res_);
    form->addRow(L("Frame rate"), fps_);
    auto* seg = new QWidget;
    auto* sl = new QHBoxLayout(seg);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(0);
    auto* group = new QButtonGroup(seg);
    const char* names[3] = {"Small", "Balanced", "High"};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(L(names[i]));
        b->setObjectName("seg");
        b->setCheckable(true);
        b->setChecked(cfg_.quality == i);
        group->addButton(b, i);
        sl->addWidget(b);
    }
    sl->addStretch();
    form->addRow(L("Detail"), seg);
    cols->addLayout(form, 1);
    v->addLayout(cols);
    preview_ = new QualityPreview;
    preview_->setMinimumHeight(190);
    v->addWidget(preview_, 1);

    // RAM
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
    disk->addWidget(label(L("Size per clip"), "muted"));
    disk->addStretch();
    diskLabel_ = new QLabel;
    diskLabel_->setObjectName("h2");
    disk->addWidget(diskLabel_);
    bv->addLayout(disk);
    ramDetail_ = label(QString(), "muted");
    bv->addWidget(ramDetail_);
    v->addWidget(box);

    connect(screen, &QComboBox::currentIndexChanged, this, [this, screen, fillRes] {
        cfg_.monitor = screen->currentData().toString();
        fillRes();
        updateRam();
        emit changed();
    });
    connect(res_, &QComboBox::currentIndexChanged, this, [this] { cfg_.height = res_->currentData().toInt(); updateRam(); emit changed(); });
    connect(fps_, &QComboBox::currentIndexChanged, this, [this] { cfg_.fps = fps_->currentData().toInt(); updateRam(); emit changed(); });
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { cfg_.quality = id; updateRam(); emit changed(); });
    updateRam();
    return w;
}

// 3: Hotkey + Ton + Autostart
QWidget* ConfigPages::hotkeyPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Your clip key"), "h1"));
    v->addWidget(label(L("Click the button and press the combination you want. Works everywhere, even in games."), "muted"));
    v->addSpacing(6);
    auto* key = new HotkeyButton(cfg_.hotkey);
    connect(key, &HotkeyButton::sequenceChanged, this, [this](const QString& s) { cfg_.hotkey = s; emit changed(); });
    v->addWidget(key);
    v->addSpacing(14);

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

    v->addWidget(label(L("Sound"), "h2"));
    auto* sys = new QCheckBox(L("System sound"));
    sys->setChecked(cfg_.systemAudio && systemAudioSupported());
    sys->setEnabled(systemAudioSupported());
    if (!systemAudioSupported()) sys->setToolTip(L("Not available on this system"));
    v->addWidget(sys);
    auto* sysVol = volume(L("System sound volume"), cfg_.systemVolume, [this](int p) { cfg_.systemVolume = p; emit changed(); });
    sysVol->setVisible(sys->isChecked());
    v->addWidget(sysVol);
    v->addSpacing(4);

    auto* form = new QFormLayout;
    auto* mic = new QComboBox;
    mic->addItem(L("None"), "");
    for (const QString& m : listMicrophones()) mic->addItem(m, m);
    mic->setCurrentIndex(std::max(0, mic->findData(cfg_.mic)));
    form->addRow(L("Microphone"), mic);
    v->addLayout(form);

    // Alles, was nur mit Mikrofon Sinn ergibt
    auto* micBox = new QWidget;
    auto* mv = new QVBoxLayout(micBox);
    mv->setContentsMargins(0, 4, 0, 0);
    mv->setSpacing(8);
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

    // Mikrofon stumm per Taste
    mv->addSpacing(4);
    mv->addWidget(label("<b>" + L("Mute key for the microphone") + "</b>"));
    auto* muteKey = new HotkeyButton(cfg_.muteHotkey);
    muteKey->setMinimumHeight(42);
    muteKey->setAllowClear(true);
    connect(muteKey, &HotkeyButton::sequenceChanged, this, [this](const QString& s) { cfg_.muteHotkey = s; emit changed(); });
    mv->addWidget(muteKey);
    mv->addWidget(label(L("Press it once and your voice is no longer recorded, press it again to switch the microphone back on. "
                          "Backspace removes the key."), "muted"));
    v->addWidget(micBox);

    auto showGate = [sensTitle, meter](int db) {
        sensTitle->setText("<b>" + L("Microphone sensitivity") + "</b>  " + (db <= -80 ? L("Off") : QString("%1 dB").arg(db)));
        meter->setThreshold(db);
    };
    showGate(cfg_.micGate);
    micBox->setVisible(!cfg_.mic.isEmpty());
    meter->setDevice(cfg_.mic);
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
    v->addSpacing(10);

    auto* autostart = new QCheckBox(L("Start with the computer (runs in the background)"));
    autostart->setChecked(cfg_.autostart);
    connect(autostart, &QCheckBox::toggled, this, [this](bool on) { cfg_.autostart = on; emit changed(); });
    v->addWidget(autostart);
    auto* sound = new QCheckBox(L("Play a sound when a clip is saved"));
    sound->setChecked(cfg_.sound);
    connect(sound, &QCheckBox::toggled, this, [this](bool on) { cfg_.sound = on; emit changed(); });
    v->addWidget(sound);
    v->addStretch();

    // Die Seite ist lang geworden: bei kleinen Fenstern scrollen statt quetschen
    auto* scroll = new QScrollArea;
    scroll->setObjectName("plain");
    scroll->setWidget(w);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea#plain { border: none; background: transparent; }"
                          "QScrollArea#plain > QWidget > QWidget { background: transparent; }");
    return scroll;
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
    for (const QColor& c : accentChoices()) {
        auto* b = new QPushButton;
        b->setObjectName("swatch");
        b->setCheckable(true);
        b->setStyleSheet(QString("background:%1;").arg(c.name()));
        b->setChecked(c.name().compare(cfg_.accent, Qt::CaseInsensitive) == 0);
        b->setCursor(Qt::PointingHandCursor);
        group->addButton(b);
        sh->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, c] { cfg_.accent = c.name(); emit lookChanged(); emit changed(); });
    }
    sh->addStretch();
    form->addRow(L("Accent colour"), sw);
    auto* mode = new QWidget;
    auto* ml = new QHBoxLayout(mode);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(0);
    auto* mg = new QButtonGroup(mode);
    for (int i = 0; i < 2; ++i) {
        auto* b = new QPushButton(i == 0 ? L("Dark") : L("Light"));
        b->setObjectName("seg");
        b->setCheckable(true);
        b->setChecked(cfg_.dark == (i == 0));
        mg->addButton(b, i);
        ml->addWidget(b);
    }
    ml->addStretch();
    connect(mg, &QButtonGroup::idClicked, this, [this](int id) { cfg_.dark = id == 0; emit lookChanged(); emit changed(); });
    form->addRow(L("Mode"), mode);
    v->addLayout(form);
    v->addStretch();
    return w;
}

// ---------------------------------------------------------------- Assistent (Erststart)
SetupWizard::SetupWizard(const Config& c, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Clipline");
    setWindowIcon(cliplineIcon());
    resize(900, 640);
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
    const QStringList names = {L("Clips folder"), L("Clip length & quality"), L("Hotkey & sound"), L("Look")};
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
    stack_->addWidget(pages_->recordingPage());
    stack_->addWidget(pages_->hotkeyPage());
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
    resize(760, 660);
    pages_ = new ConfigPages(c, this);
    connect(pages_, &ConfigPages::lookChanged, this, [this] { applyLook(pages_->config()); });
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(18, 18, 18, 18);
    auto* tabs = new QTabWidget;
    tabs->addTab(pages_->folderPage(false), L("General"));
    // "&" verdoppeln, sonst macht Qt daraus ein Tastenkürzel und es verschwindet
    tabs->addTab(pages_->recordingPage(), L("Clip length & quality").replace("&", "&&"));
    tabs->addTab(pages_->hotkeyPage(), L("Hotkey & sound").replace("&", "&&"));
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
