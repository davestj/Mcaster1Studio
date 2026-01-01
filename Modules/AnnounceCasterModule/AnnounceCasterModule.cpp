/// @file   AnnounceCasterModule.cpp
/// @path   Modules/AnnounceCasterModule/AnnounceCasterModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-AnnounceCaster — Announcements and Lower Third Display Implementation
/// @purpose Implements announcement slide management, pre/post-service loops,
///          lower third overlays, and live display control. We delegate all
///          rendering to GraphicsEngineModule.
/// @reason  Handles all non-worship, non-sermon visual content.
/// @changelog
///   2026-03-09  Initial implementation

#include "AnnounceCasterModule.h"
#include "GraphicsEngineModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPainter>
#include <QSettings>
#include <QDebug>

namespace M1 {

namespace {

// ─── OutputPreview ──────────────────────────────────────────────────────────
class AnnouncePreview : public QWidget {
    Q_OBJECT
public:
    explicit AnnouncePreview(AnnounceCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("AnnounceOutputPreview");
        setMinimumSize(320, 180);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setToolTip("Congregation view — what the projector shows");
        connect(mod, &AnnounceCasterModule::frameUpdated, this,
                [this](const QImage& frame) { m_frame = frame; update(); });
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.fillRect(rect(), Qt::black);
        if (!m_frame.isNull()) {
            QSize s = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
            int x = (width() - s.width()) / 2;
            int y = (height() - s.height()) / 2;
            p.drawImage(QRect(x, y, s.width(), s.height()), m_frame);
        }
        p.setPen(QColor(60, 60, 60));
        p.drawRect(0, 0, width() - 1, height() - 1);
    }
private:
    QImage m_frame;
};

// ─── SlideEditorDialog — add/edit an announcement slide ─────────────────────
class SlideEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SlideEditorDialog(const AnnouncementSlide& slide = {}, QWidget* parent = nullptr)
        : QDialog(parent), m_slide(slide)
    {
        setWindowTitle(slide.id > 0 ? "Edit Slide" : "New Slide");
        setMinimumSize(440, 420);

        auto* lay = new QVBoxLayout(this);
        auto* grid = new QGridLayout();
        int row = 0;

        grid->addWidget(new QLabel("Type:", this), row, 0);
        m_typeCombo = new QComboBox(this);
        m_typeCombo->addItems({"Announcement", "Welcome", "Offering",
                               "Closing", "Lower Third", "Countdown", "Custom"});
        m_typeCombo->setCurrentIndex(static_cast<int>(slide.type));
        grid->addWidget(m_typeCombo, row++, 1);

        grid->addWidget(new QLabel("Title:", this), row, 0);
        m_title = new QLineEdit(slide.title, this);
        grid->addWidget(m_title, row++, 1);

        grid->addWidget(new QLabel("Body:", this), row, 0);
        m_body = new QTextEdit(this);
        m_body->setPlainText(slide.body);
        m_body->setMaximumHeight(100);
        grid->addWidget(m_body, row++, 1);

        grid->addWidget(new QLabel("Name (LT):", this), row, 0);
        m_ltName = new QLineEdit(slide.lowerThirdLine1, this);
        m_ltName->setToolTip("Lower third: person's name");
        grid->addWidget(m_ltName, row++, 1);

        grid->addWidget(new QLabel("Title (LT):", this), row, 0);
        m_ltTitle = new QLineEdit(slide.lowerThirdLine2, this);
        m_ltTitle->setToolTip("Lower third: person's role/title");
        grid->addWidget(m_ltTitle, row++, 1);

        grid->addWidget(new QLabel("QR URL:", this), row, 0);
        m_qrUrl = new QLineEdit(slide.qrCodeUrl, this);
        m_qrUrl->setPlaceholderText("https://give.church.com");
        m_qrUrl->setToolTip("URL for QR code generation");
        grid->addWidget(m_qrUrl, row++, 1);

        grid->addWidget(new QLabel("Duration:", this), row, 0);
        m_duration = new QSpinBox(this);
        m_duration->setRange(3, 120);
        m_duration->setValue(slide.displayDurationSec);
        m_duration->setSuffix(" sec");
        m_duration->setToolTip("Display duration in loop mode");
        grid->addWidget(m_duration, row++, 1);

        grid->addWidget(new QLabel("Start Date:", this), row, 0);
        m_startDate = new QDateEdit(this);
        m_startDate->setCalendarPopup(true);
        m_startDate->setDate(slide.startDate.isValid() ? slide.startDate : QDate::currentDate());
        m_startDate->setToolTip("Start showing from this date");
        grid->addWidget(m_startDate, row++, 1);

        grid->addWidget(new QLabel("End Date:", this), row, 0);
        m_endDate = new QDateEdit(this);
        m_endDate->setCalendarPopup(true);
        m_endDate->setDate(slide.endDate.isValid() ? slide.endDate
                           : QDate::currentDate().addMonths(1));
        m_endDate->setToolTip("Stop showing after this date (auto-expire)");
        grid->addWidget(m_endDate, row++, 1);

        m_enabled = new QCheckBox("Enabled", this);
        m_enabled->setChecked(slide.enabled);
        grid->addWidget(m_enabled, row++, 1);

        lay->addLayout(grid);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(buttons);
    }

    AnnouncementSlide result() const {
        AnnouncementSlide s = m_slide;
        s.type             = static_cast<SlideType>(m_typeCombo->currentIndex());
        s.title            = m_title->text();
        s.body             = m_body->toPlainText();
        s.lowerThirdLine1  = m_ltName->text();
        s.lowerThirdLine2  = m_ltTitle->text();
        s.qrCodeUrl        = m_qrUrl->text();
        s.displayDurationSec = m_duration->value();
        s.startDate        = m_startDate->date();
        s.endDate          = m_endDate->date();
        s.enabled          = m_enabled->isChecked();
        return s;
    }

private:
    AnnouncementSlide m_slide;
    QComboBox*   m_typeCombo = nullptr;
    QLineEdit*   m_title     = nullptr;
    QTextEdit*   m_body      = nullptr;
    QLineEdit*   m_ltName    = nullptr;
    QLineEdit*   m_ltTitle   = nullptr;
    QLineEdit*   m_qrUrl     = nullptr;
    QSpinBox*    m_duration  = nullptr;
    QDateEdit*   m_startDate = nullptr;
    QDateEdit*   m_endDate   = nullptr;
    QCheckBox*   m_enabled   = nullptr;
};

} // anonymous namespace

// ─── AnnounceCasterModule implementation ─────────────────────────────────────

AnnounceCasterModule::AnnounceCasterModule(QObject* parent)
    : IModule(parent)
    , m_loopTimer(new QTimer(this))
{
    connect(m_loopTimer, &QTimer::timeout,
            this, &AnnounceCasterModule::onLoopAdvance);
}

AnnounceCasterModule::~AnnounceCasterModule() {
    shutdown();
}

void AnnounceCasterModule::initialize() {
    qInfo() << "[AnnounceCaster] Initialized — slides:" << m_slides.size();
}

void AnnounceCasterModule::shutdown() {
    m_loopTimer->stop();
}

QWidget* AnnounceCasterModule::createWidget(QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setObjectName("AnnounceCasterWidget");
    auto* mainLay = new QVBoxLayout(container);
    mainLay->setContentsMargins(4, 4, 4, 4);
    mainLay->setSpacing(4);

    auto* splitter = new QSplitter(Qt::Horizontal, container);

    // ── Left panel: slide list + management ─────────────────────────────────
    auto* leftPanel = new QWidget(splitter);
    auto* leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(0, 0, 0, 0);
    leftLay->setSpacing(4);

    auto* slideList = new QListWidget(leftPanel);
    slideList->setObjectName("AnnounceSlideList");
    slideList->setToolTip("Double-click to show slide — drag to reorder");
    leftLay->addWidget(slideList, 1);

    auto* btnRow = new QHBoxLayout();
    auto* addBtn = new QPushButton("+ New", leftPanel);
    addBtn->setToolTip("Create a new announcement slide");
    auto* editBtn = new QPushButton("Edit", leftPanel);
    editBtn->setToolTip("Edit selected slide");
    auto* removeBtn = new QPushButton("Remove", leftPanel);
    removeBtn->setToolTip("Remove selected slide");
    btnRow->addWidget(addBtn);
    btnRow->addWidget(editBtn);
    btnRow->addWidget(removeBtn);
    leftLay->addLayout(btnRow);

    // Loop controls
    auto* loopGroup = new QGroupBox("Pre-Service Loop", leftPanel);
    auto* loopLay = new QHBoxLayout(loopGroup);
    auto* startLoopBtn = new QPushButton("Start Loop", loopGroup);
    startLoopBtn->setToolTip("Start cycling through active announcements");
    startLoopBtn->setCheckable(true);
    loopLay->addWidget(startLoopBtn);
    auto* loopStatusLabel = new QLabel("Stopped", loopGroup);
    loopStatusLabel->setObjectName("AnnounceLoopStatus");
    loopLay->addWidget(loopStatusLabel);
    leftLay->addWidget(loopGroup);

    splitter->addWidget(leftPanel);

    // ── Right panel: preview + transport + lower third ──────────────────────
    auto* rightPanel = new QWidget(splitter);
    auto* rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(4);

    // Output preview
    auto* preview = new AnnouncePreview(this, rightPanel);
    preview->setMinimumHeight(140);
    rightLay->addWidget(preview, 2);

    // Transport
    auto* transportRow = new QHBoxLayout();
    auto* prevBtn = new QPushButton("<< Prev", rightPanel);
    prevBtn->setFixedHeight(36);
    auto* blankBtn = new QPushButton("BLANK", rightPanel);
    blankBtn->setCheckable(true);
    blankBtn->setFixedHeight(36);
    blankBtn->setToolTip("Toggle blank/black screen");
    auto* nextBtn = new QPushButton("Next >>", rightPanel);
    nextBtn->setFixedHeight(36);
    transportRow->addWidget(prevBtn);
    transportRow->addWidget(blankBtn);
    transportRow->addWidget(nextBtn);
    rightLay->addLayout(transportRow);

    // Lower third quick control
    auto* ltGroup = new QGroupBox("Quick Lower Third", rightPanel);
    auto* ltLay = new QGridLayout(ltGroup);
    auto* ltNameInput = new QLineEdit(ltGroup);
    ltNameInput->setPlaceholderText("Name...");
    ltNameInput->setToolTip("Speaker name for lower third overlay");
    ltLay->addWidget(new QLabel("Name:", ltGroup), 0, 0);
    ltLay->addWidget(ltNameInput, 0, 1);
    auto* ltTitleInput = new QLineEdit(ltGroup);
    ltTitleInput->setPlaceholderText("Title/Role...");
    ltTitleInput->setToolTip("Speaker role for lower third overlay");
    ltLay->addWidget(new QLabel("Title:", ltGroup), 1, 0);
    ltLay->addWidget(ltTitleInput, 1, 1);
    auto* ltShowBtn = new QPushButton("Show LT", ltGroup);
    ltShowBtn->setToolTip("Show lower third overlay on output");
    ltShowBtn->setCheckable(true);
    ltLay->addWidget(ltShowBtn, 0, 2, 2, 1);
    rightLay->addWidget(ltGroup);

    // Current slide info
    auto* infoLabel = new QLabel("No slide displayed", rightPanel);
    infoLabel->setObjectName("AnnounceSlideInfo");
    rightLay->addWidget(infoLabel);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    mainLay->addWidget(splitter);

    // ── Wire signals ────────────────────────────────────────────────────────

    auto refreshSlideList = [this, slideList]() {
        slideList->clear();
        for (const auto& s : m_slides) {
            QString label = QString("[%1] %2").arg(slideTypeName(s.type), s.title);
            if (!s.enabled) label += " (disabled)";
            auto* item = new QListWidgetItem(label, slideList);
            item->setData(Qt::UserRole, s.id);
        }
    };
    refreshSlideList();

    connect(this, &AnnounceCasterModule::slidesChanged, container, refreshSlideList);

    // Double-click → show slide
    connect(slideList, &QListWidget::itemDoubleClicked, container,
            [this](QListWidgetItem* item) {
        showSlide(item->data(Qt::UserRole).toInt());
    });

    connect(addBtn, &QPushButton::clicked, container, [this]() {
        SlideEditorDialog dlg({}, nullptr);
        if (dlg.exec() == QDialog::Accepted)
            addSlide(dlg.result());
    });

    connect(editBtn, &QPushButton::clicked, container,
            [this, slideList]() {
        auto* item = slideList->currentItem();
        if (!item) return;
        int id = item->data(Qt::UserRole).toInt();
        auto* s = slide(id);
        if (!s) return;
        SlideEditorDialog dlg(*s, nullptr);
        if (dlg.exec() == QDialog::Accepted) {
            AnnouncementSlide updated = dlg.result();
            updated.id = id;
            updateSlide(updated);
        }
    });

    connect(removeBtn, &QPushButton::clicked, container,
            [this, slideList]() {
        auto* item = slideList->currentItem();
        if (!item) return;
        removeSlide(item->data(Qt::UserRole).toInt());
    });

    // Transport
    connect(prevBtn, &QPushButton::clicked, this, &AnnounceCasterModule::prevSlide);
    connect(nextBtn, &QPushButton::clicked, this, &AnnounceCasterModule::nextSlide);
    connect(blankBtn, &QPushButton::clicked, this, [this](bool c) { setBlank(c); });
    connect(this, &AnnounceCasterModule::blankChanged, blankBtn, &QPushButton::setChecked);

    // Loop
    connect(startLoopBtn, &QPushButton::clicked, this, [this](bool checked) {
        if (checked) startLoop(); else stopLoop();
    });
    connect(this, &AnnounceCasterModule::loopStateChanged, container,
            [startLoopBtn, loopStatusLabel](bool looping) {
        startLoopBtn->setChecked(looping);
        loopStatusLabel->setText(looping ? "Running" : "Stopped");
    });

    // Lower third
    connect(ltShowBtn, &QPushButton::clicked, this,
            [this, ltNameInput, ltTitleInput](bool checked) {
        if (checked)
            showLowerThird(ltNameInput->text(), ltTitleInput->text());
        else
            hideLowerThird();
    });
    connect(this, &AnnounceCasterModule::lowerThirdStateChanged,
            ltShowBtn, &QPushButton::setChecked);

    // Slide changed → update info
    connect(this, &AnnounceCasterModule::slideChanged, container,
            [this, infoLabel](int id) {
        auto* s = slide(id);
        infoLabel->setText(s ? QString("[%1] %2").arg(slideTypeName(s->type), s->title)
                             : "No slide displayed");
    });

    return container;
}

void AnnounceCasterModule::saveState(QSettings& s) {
    s.beginGroup("AnnounceCaster");
    s.setValue("slideCount", m_slides.size());
    int idx = 0;
    for (auto it = m_slides.constBegin(); it != m_slides.constEnd(); ++it, ++idx) {
        const QString prefix = QString("slide_%1/").arg(idx);
        const auto& sl = it.value();
        s.setValue(prefix + "id",       sl.id);
        s.setValue(prefix + "type",     static_cast<int>(sl.type));
        s.setValue(prefix + "title",    sl.title);
        s.setValue(prefix + "body",     sl.body);
        s.setValue(prefix + "image",    sl.imagePath);
        s.setValue(prefix + "lt1",      sl.lowerThirdLine1);
        s.setValue(prefix + "lt2",      sl.lowerThirdLine2);
        s.setValue(prefix + "qrUrl",    sl.qrCodeUrl);
        s.setValue(prefix + "duration", sl.displayDurationSec);
        s.setValue(prefix + "start",    sl.startDate);
        s.setValue(prefix + "end",      sl.endDate);
        s.setValue(prefix + "enabled",  sl.enabled);
    }
    s.endGroup();
}

void AnnounceCasterModule::loadState(QSettings& s) {
    s.beginGroup("AnnounceCaster");
    int count = s.value("slideCount", 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("slide_%1/").arg(i);
        AnnouncementSlide sl;
        sl.id                = s.value(prefix + "id", 0).toInt();
        sl.type              = static_cast<SlideType>(s.value(prefix + "type", 0).toInt());
        sl.title             = s.value(prefix + "title").toString();
        sl.body              = s.value(prefix + "body").toString();
        sl.imagePath         = s.value(prefix + "image").toString();
        sl.lowerThirdLine1   = s.value(prefix + "lt1").toString();
        sl.lowerThirdLine2   = s.value(prefix + "lt2").toString();
        sl.qrCodeUrl         = s.value(prefix + "qrUrl").toString();
        sl.displayDurationSec = s.value(prefix + "duration", 8).toInt();
        sl.startDate         = s.value(prefix + "start").toDate();
        sl.endDate           = s.value(prefix + "end").toDate();
        sl.enabled           = s.value(prefix + "enabled", true).toBool();

        if (sl.id >= m_nextSlideId) m_nextSlideId = sl.id + 1;
        m_slides.insert(sl.id, sl);
    }
    s.endGroup();
}

// ─── Slide management ────────────────────────────────────────────────────────

int AnnounceCasterModule::addSlide(const AnnouncementSlide& slide) {
    AnnouncementSlide s = slide;
    s.id = m_nextSlideId++;
    m_slides.insert(s.id, s);
    emit slidesChanged();
    qInfo() << "[AnnounceCaster] Added slide:" << s.title << "id:" << s.id;
    return s.id;
}

void AnnounceCasterModule::removeSlide(int id) {
    m_slides.remove(id);
    emit slidesChanged();
}

void AnnounceCasterModule::updateSlide(const AnnouncementSlide& slide) {
    if (m_slides.contains(slide.id)) {
        m_slides[slide.id] = slide;
        emit slidesChanged();
    }
}

const AnnouncementSlide* AnnounceCasterModule::slide(int id) const {
    auto it = m_slides.constFind(id);
    return (it != m_slides.constEnd()) ? &it.value() : nullptr;
}

QList<AnnouncementSlide> AnnounceCasterModule::activeSlides() const {
    QList<AnnouncementSlide> result;
    QDate today = QDate::currentDate();
    for (const auto& s : m_slides) {
        if (!s.enabled) continue;
        if (s.startDate.isValid() && today < s.startDate) continue;
        if (s.endDate.isValid() && today > s.endDate) continue;
        result.append(s);
    }
    return result;
}

// ─── Live control ────────────────────────────────────────────────────────────

void AnnounceCasterModule::showSlide(int id) {
    if (!m_slides.contains(id)) return;
    m_currentSlideId = id;
    renderCurrentSlide();
    emit slideChanged(id);
}

void AnnounceCasterModule::nextSlide() {
    auto active = activeSlides();
    if (active.isEmpty()) return;

    // We find the current position in active list and advance
    int currentPos = -1;
    for (int i = 0; i < active.size(); ++i) {
        if (active[i].id == m_currentSlideId) { currentPos = i; break; }
    }
    int next = (currentPos + 1) % active.size();
    showSlide(active[next].id);
}

void AnnounceCasterModule::prevSlide() {
    auto active = activeSlides();
    if (active.isEmpty()) return;

    int currentPos = -1;
    for (int i = 0; i < active.size(); ++i) {
        if (active[i].id == m_currentSlideId) { currentPos = i; break; }
    }
    int prev = (currentPos <= 0) ? active.size() - 1 : currentPos - 1;
    showSlide(active[prev].id);
}

void AnnounceCasterModule::setBlank(bool blank) {
    if (m_blank == blank) return;
    m_blank = blank;
    renderCurrentSlide();
    emit blankChanged(blank);
}

// ─── Loop mode ───────────────────────────────────────────────────────────────

void AnnounceCasterModule::startLoop() {
    m_activeSlideIds.clear();
    for (const auto& s : activeSlides())
        m_activeSlideIds.append(s.id);

    if (m_activeSlideIds.isEmpty()) {
        qWarning() << "[AnnounceCaster] No active slides for loop";
        return;
    }

    m_loopIndex = 0;
    m_looping = true;
    showSlide(m_activeSlideIds[0]);

    // We start the timer with the first slide's duration
    auto* first = slide(m_activeSlideIds[0]);
    m_loopTimer->start((first ? first->displayDurationSec : 8) * 1000);

    emit loopStateChanged(true);
    qInfo() << "[AnnounceCaster] Loop started —" << m_activeSlideIds.size() << "slides";
}

void AnnounceCasterModule::stopLoop() {
    m_looping = false;
    m_loopTimer->stop();
    emit loopStateChanged(false);
    qInfo() << "[AnnounceCaster] Loop stopped";
}

void AnnounceCasterModule::onLoopAdvance() {
    if (!m_looping || m_activeSlideIds.isEmpty()) return;

    m_loopIndex = (m_loopIndex + 1) % m_activeSlideIds.size();
    int nextId = m_activeSlideIds[m_loopIndex];
    showSlide(nextId);

    // We set the timer for the next slide's duration
    auto* s = slide(nextId);
    m_loopTimer->start((s ? s->displayDurationSec : 8) * 1000);
}

// ─── Lower third ─────────────────────────────────────────────────────────────

void AnnounceCasterModule::showLowerThird(const QString& name, const QString& title) {
    m_ltName = name;
    m_ltTitle = title;
    m_lowerThirdVisible = true;
    renderCurrentSlide();
    emit lowerThirdStateChanged(true);
}

void AnnounceCasterModule::hideLowerThird() {
    m_lowerThirdVisible = false;
    renderCurrentSlide();
    emit lowerThirdStateChanged(false);
}

// ─── Rendering ───────────────────────────────────────────────────────────────

void AnnounceCasterModule::renderCurrentSlide() {
    if (m_blank || !m_graphics) {
        m_currentFrame = m_graphics ? m_graphics->renderBlank() : QImage();
        emit frameUpdated(m_currentFrame);
        return;
    }

    auto* sl = slide(m_currentSlideId);

    if (!sl) {
        // We render just the lower third if visible, otherwise blank
        if (m_lowerThirdVisible && !m_ltName.isEmpty()) {
            m_currentFrame = m_graphics->renderLowerThird(m_ltName, m_ltTitle);
        } else {
            m_currentFrame = m_graphics->renderBlank();
        }
        emit frameUpdated(m_currentFrame);
        return;
    }

    // We render based on slide type
    switch (sl->type) {
        case SlideType::Announcement:
        case SlideType::Custom:
            m_currentFrame = m_graphics->renderTitleCard(sl->title, sl->body);
            break;
        case SlideType::Welcome:
            m_currentFrame = m_graphics->renderTitleCard("Welcome", sl->body);
            break;
        case SlideType::Offering:
            m_currentFrame = m_graphics->renderTitleCard(
                sl->title.isEmpty() ? "Giving" : sl->title, sl->body);
            break;
        case SlideType::Closing:
            m_currentFrame = m_graphics->renderTitleCard(
                sl->title.isEmpty() ? "Thank You" : sl->title, sl->body);
            break;
        case SlideType::LowerThird:
            m_currentFrame = m_graphics->renderLowerThird(
                sl->lowerThirdLine1, sl->lowerThirdLine2);
            break;
        case SlideType::Countdown:
            m_currentFrame = m_graphics->renderCountdown(
                sl->title, sl->body);
            break;
    }

    // We overlay a lower third if it's independently active
    if (m_lowerThirdVisible && !m_ltName.isEmpty() &&
        sl->type != SlideType::LowerThird) {
        QPainter p(&m_currentFrame);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        // We manually paint the lower third bar on top of the current frame
        // (in production, we would use GraphicsEngine's paintLowerThird)
        QImage ltFrame = m_graphics->renderLowerThird(m_ltName, m_ltTitle);
        p.setOpacity(0.9);
        p.drawImage(0, 0, ltFrame);
    }

    emit frameUpdated(m_currentFrame);
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_announceInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.church.announce",
    "Announce Caster",
    "1.0.0",
    "church",
    "module",
    "Mcaster1",
    "Announcements, lower thirds, pre-service loops, and transition slides"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_announce_plugin_info() { return &s_announceInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_announce_create_module(IModuleHost*) {
    return new M1::AnnounceCasterModule();
}
MCASTER1_PLUGIN_API void mcaster1_announce_destroy_module(IModule* m) { delete m; }
}

#include "AnnounceCasterModule.moc"
