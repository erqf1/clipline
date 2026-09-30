#include "setup.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScreen>
#include <QTabWidget>
#include <QVBoxLayout>
#include "i18n.h"
#include "look.h"

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
    v->setContentsMargins(28, 26, 28, 22);
    v->setSpacing(12);
    *out = v;
    return w;
}

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

// ---------------------------------------------------------------- RAM-Anzeige
QWidget* ConfigPages::ramBox() {
    auto* box = new QFrame;
    box->setObjectName("card");
    auto* v = new QVBoxLayout(box);
    v->setContentsMargins(18, 14, 18, 16);
    v->setSpacing(6);
    auto* top = new QHBoxLayout;
    top->addWidget(label(L("Estimated RAM"), "muted"));
    top->addStretch();
    auto* big = new QLabel;
    big->setObjectName("h2");
    top->addWidget(big);
    v->addLayout(top);
    auto* bar = new QProgressBar;
    bar->setRange(0, 2048);
    bar->setTextVisible(false);
    bar->setFixedHeight(8);
    v->addWidget(bar);
    auto* detail = label(QString(), "muted");
    v->addWidget(detail);
    ramLabels_ << big;
    ramBars_ << bar;
    ramDetail_ << detail;
    updateRam();
    return box;
}

void ConfigPages::updateRam() {
    const QSize n = nativeSize();
    const QSize s = captureSize(cfg_, n);
    const double total = estimateTotalMB(cfg_, n), buf = estimateBufferMB(cfg_, n);
    for (QLabel* l : ramLabels_) l->setText("≈ " + fmtMB(total));
    for (QProgressBar* b : ramBars_) b->setValue(int(std::min(2048.0, total)));
    for (QLabel* l : ramDetail_)
        l->setText(QString("%1 %2 · %3×%4 · %5 fps · %6 Mbit/s")
                       .arg(L("buffer")).arg(fmtMB(buf)).arg(s.width()).arg(s.height()).arg(cfg_.fps)
                       .arg(videoBitrate(cfg_, n) / 1e6, 0, 'f', 1));
    if (lenLabel_) lenLabel_->setText(fmtDuration(cfg_.clipSeconds));
}

// ---------------------------------------------------------------- Seite 1: Ordner + Sprache
QWidget* ConfigPages::folderPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Welcome to Clipline"), "h1"));
    v->addWidget(label(L("Clipline records your screen quietly in the background. Press a key and the last moments are saved as a clip – nothing is written to disk until you do."), "muted"));
    v->addSpacing(10);
    v->addWidget(label(L("Where should your clips go?"), "h2"));
    auto* row = new QHBoxLayout;
    auto* edit = new QLineEdit(cfg_.clipsDir);
    auto* pick = new QPushButton(icon(Ic::Folder, makePalette(cfg_.accent, cfg_.dark).text), " " + L("Choose…"));
    row->addWidget(edit, 1);
    row->addWidget(pick);
    v->addLayout(row);
    connect(edit, &QLineEdit::textChanged, this, [this](const QString& t) { cfg_.clipsDir = t.trimmed(); emit changed(); });
    connect(pick, &QPushButton::clicked, w, [w, edit] {
        const QString d = QFileDialog::getExistingDirectory(w, L("Clips folder"), edit->text());
        if (!d.isEmpty()) edit->setText(QDir::toNativeSeparators(d));
    });
    v->addSpacing(8);
    auto* form = new QFormLayout;
    auto* lang = new QComboBox;
    lang->addItem(L("System"), "");
    lang->addItem("English", "en");
    lang->addItem("Deutsch", "de");
    lang->setCurrentIndex(std::max(0, lang->findData(cfg_.language)));
    connect(lang, &QComboBox::currentIndexChanged, this, [this, lang] { cfg_.language = lang->currentData().toString(); emit changed(); });
    form->addRow(L("Language"), lang);
    v->addLayout(form);
    v->addStretch();
    return w;
}

// ---------------------------------------------------------------- Seite 2: Cliplänge + RAM
QWidget* ConfigPages::lengthPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("How much should one clip contain?"), "h1"));
    v->addWidget(label(L("When you press the hotkey, everything from this long before is saved."), "muted"));
    v->addSpacing(14);
    lenLabel_ = new QLabel;
    lenLabel_->setObjectName("big");
    lenLabel_->setAlignment(Qt::AlignCenter);
    v->addWidget(lenLabel_);
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, kLengthCount - 1);
    int idx = 0;
    for (int i = 0; i < kLengthCount; ++i)
        if (kLengths[i] <= cfg_.clipSeconds) idx = i;
    slider->setValue(idx);
    slider->setPageStep(1);
    v->addWidget(slider);
    auto* ends = new QHBoxLayout;
    ends->addWidget(label(fmtDuration(kLengths[0]), "muted"));
    ends->addStretch();
    ends->addWidget(label(fmtDuration(kLengths[kLengthCount - 1]), "muted"));
    v->addLayout(ends);
    connect(slider, &QSlider::valueChanged, this, [this](int i) { cfg_.clipSeconds = kLengths[i]; updateRam(); emit changed(); });
    v->addSpacing(12);
    v->addWidget(ramBox());
    v->addWidget(label(L("Clipline keeps the recording in memory, so longer clips and higher quality need more RAM."), "muted"));
    v->addStretch();
    updateRam();
    return w;
}

// ---------------------------------------------------------------- Seite 3: Qualität + Ton
QWidget* ConfigPages::qualityPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("How should clips look?"), "h1"));
    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* screen = new QComboBox;
    for (const Monitor& m : monitors_) screen->addItem(m.name, m.id);
    const Monitor cur = findMonitor(cfg_.monitor);
    screen->setCurrentIndex(std::max(0, screen->findData(cur.id)));
    if (monitors_.size() > 1) form->addRow(L("Screen"), screen);
    else screen->hide();

    res_ = new QComboBox;
    fps_ = new QComboBox;
    auto fillRes = [this] {
        const QSize n = nativeSize();
        QSignalBlocker b1(res_), b2(fps_);
        res_->clear();
        res_->addItem(QString("%1 (%2×%3)").arg(L("Native")).arg(n.width()).arg(n.height()), 0);
        for (int h : {2160, 1440, 1080, 900, 720, 480})
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

    auto* sys = new QCheckBox(L("System sound"));
    sys->setChecked(cfg_.systemAudio && systemAudioSupported());
    sys->setEnabled(systemAudioSupported());
    if (!systemAudioSupported()) sys->setToolTip(L("Not available on this system"));
    form->addRow(QString(), sys);
    auto* mic = new QComboBox;
    mic->addItem(L("None"), "");
    for (const QString& m : listMicrophones()) mic->addItem(m, m);
    mic->setCurrentIndex(std::max(0, mic->findData(cfg_.mic)));
    form->addRow(L("Microphone"), mic);
    v->addLayout(form);
    v->addSpacing(8);
    v->addWidget(ramBox());
    v->addStretch();

    connect(screen, &QComboBox::currentIndexChanged, this, [this, screen, fillRes] {
        cfg_.monitor = screen->currentData().toString();
        fillRes();
        updateRam();
        emit changed();
    });
    connect(res_, &QComboBox::currentIndexChanged, this, [this] { cfg_.height = res_->currentData().toInt(); updateRam(); emit changed(); });
    connect(fps_, &QComboBox::currentIndexChanged, this, [this] { cfg_.fps = fps_->currentData().toInt(); updateRam(); emit changed(); });
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { cfg_.quality = id; updateRam(); emit changed(); });
    connect(sys, &QCheckBox::toggled, this, [this](bool on) { cfg_.systemAudio = on; updateRam(); emit changed(); });
    connect(mic, &QComboBox::currentIndexChanged, this, [this, mic] { cfg_.mic = mic->currentData().toString(); updateRam(); emit changed(); });
    return w;
}

// ---------------------------------------------------------------- Seite 4: Aussehen + Verhalten
QWidget* ConfigPages::lookPage() {
    QVBoxLayout* v;
    QWidget* w = page(&v);
    v->addWidget(label(L("Make it yours"), "h1"));
    auto* form = new QFormLayout;
    form->setSpacing(12);

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

    auto* key = new QKeySequenceEdit(QKeySequence(cfg_.hotkey));
    key->setMaximumSequenceLength(1);
    key->setClearButtonEnabled(true);
    connect(key, &QKeySequenceEdit::editingFinished, this, [this, key] {
        const QString s = key->keySequence().toString();
        if (!s.isEmpty()) cfg_.hotkey = s;
        emit changed();
    });
    form->addRow(L("Save hotkey"), key);
    v->addLayout(form);
    v->addSpacing(6);

    auto* autostart = new QCheckBox(L("Start with the computer (runs in the background)"));
    autostart->setChecked(cfg_.autostart);
    connect(autostart, &QCheckBox::toggled, this, [this](bool on) { cfg_.autostart = on; emit changed(); });
    v->addWidget(autostart);
    auto* sound = new QCheckBox(L("Play a sound when a clip is saved"));
    sound->setChecked(cfg_.sound);
    connect(sound, &QCheckBox::toggled, this, [this](bool on) { cfg_.sound = on; emit changed(); });
    v->addWidget(sound);
    v->addStretch();
    return w;
}

// ---------------------------------------------------------------- Assistent (Erststart)
SetupWizard::SetupWizard(const Config& c, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Clipline");
    setWindowIcon(cliplineIcon());
    resize(820, 580);
    pages_ = new ConfigPages(c, this);
    connect(pages_, &ConfigPages::lookChanged, this, [this] { applyLook(pages_->config()); });

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
    auto* name = label("Clipline", "h2");
    sv->addWidget(name);
    sv->addSpacing(22);
    const QStringList names = {L("Clips folder"), L("Clip length"), L("Quality"), L("Look & behaviour")};
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
    stack_->addWidget(pages_->folderPage());
    stack_->addWidget(pages_->lengthPage());
    stack_->addWidget(pages_->qualityPage());
    stack_->addWidget(pages_->lookPage());
    rv->addWidget(stack_, 1);
    auto* nav = new QHBoxLayout;
    nav->setContentsMargins(28, 0, 28, 24);
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
    resize(640, 600);
    pages_ = new ConfigPages(c, this);
    connect(pages_, &ConfigPages::lookChanged, this, [this] { applyLook(pages_->config()); });
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(18, 18, 18, 18);
    auto* tabs = new QTabWidget;
    tabs->addTab(pages_->folderPage(), L("General"));
    tabs->addTab(pages_->lengthPage(), L("Clip length"));
    tabs->addTab(pages_->qualityPage(), L("Quality"));
    tabs->addTab(pages_->lookPage(), L("Look & behaviour"));
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
