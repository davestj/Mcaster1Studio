/// @file   SwitchCasterModule.cpp
/// @path   Modules/SwitchCasterModule/SwitchCasterModule.cpp

#include "SwitchCasterModule.h"
#include "GraphicsEngineModule.h"
#include "ThemePalette.h"
#include "LyricsCasterModule.h"
#include "ScriptureCasterModule.h"
#include "AnnounceCasterModule.h"
#include "MediaCasterModule.h"
#include "VideoModule.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QPainter>
#include <QTimer>
#include <QSettings>
#include <QFrame>
#include <QSizePolicy>
#include <QLineEdit>

namespace {

// ─── PreviewLabel — renders a QImage scaled to fit ──────────────────────────
class PreviewLabel : public QLabel {
public:
    explicit PreviewLabel(QWidget* parent = nullptr) : QLabel(parent) {
        setMinimumSize(160, 90);
        setAlignment(Qt::AlignCenter);
        setObjectName("SwitchPreviewLabel");
        const auto pal = ThemePalette::forCurrentTheme();
        setStyleSheet(QString("background: %1; border: 2px solid %2; border-radius: 4px;")
            .arg(pal.bg.name(), pal.border.name()));
    }
    void setFrame(const QImage& img) {
        m_frame = img;
        update();
    }
    void setLiveIndicator(bool live) {
        m_live = live;
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto pal = ThemePalette::forCurrentTheme();
        p.fillRect(rect(), pal.bg);

        if (!m_frame.isNull()) {
            const QSize scaled = m_frame.size().scaled(size() - QSize(4, 4), Qt::KeepAspectRatio);
            const int x = (width()  - scaled.width())  / 2;
            const int y = (height() - scaled.height()) / 2;
            p.drawImage(QRect(x, y, scaled.width(), scaled.height()), m_frame);
        } else {
            p.setPen(pal.textDisabled);
            p.drawText(rect(), Qt::AlignCenter, "No Signal");
        }

        // Live indicator
        if (m_live) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(pal.error.red(), pal.error.green(), pal.error.blue(), 200));
            p.drawRoundedRect(6, 6, 40, 18, 4, 4);
            p.setPen(Qt::white);
            QFont f = font();
            f.setPixelSize(12);
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRect(6, 6, 40, 18), Qt::AlignCenter, "LIVE");
        }
    }
private:
    QImage m_frame;
    bool   m_live = false;
};

// ─── SourceButton — colored toggle for a source ────────────────────────────
class SourceButton : public QPushButton {
public:
    SourceButton(const QString& text, M1::SourceType src, QWidget* parent = nullptr)
        : QPushButton(text, parent), m_src(src) {
        setCheckable(true);
        setMinimumHeight(36);
        setObjectName("SwitchSourceBtn");
        QFont f = font();
        f.setPixelSize(12);
        f.setBold(true);
        setFont(f);
    }
    M1::SourceType sourceType() const { return m_src; }
private:
    M1::SourceType m_src;
};

// ─── SwitchCasterWidget ─────────────────────────────────────────────────────
class SwitchCasterWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwitchCasterWidget(M1::SwitchCasterModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("SwitchCasterWidget");
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(8, 8, 8, 8);
        root->setSpacing(6);

        // ── Preview / Program monitors ──────────────────────────────────
        auto* monitorRow = new QHBoxLayout;
        monitorRow->setSpacing(8);

        // Preview side
        auto* previewGroup = new QGroupBox("PREVIEW");
        previewGroup->setObjectName("SwitchGroup");
        auto* pvLay = new QVBoxLayout(previewGroup);
        m_previewLabel = new PreviewLabel;
        pvLay->addWidget(m_previewLabel);
        m_previewName = new QLabel("None");
        m_previewName->setAlignment(Qt::AlignCenter);
        m_previewName->setObjectName("SwitchSourceLabel");
        pvLay->addWidget(m_previewName);
        monitorRow->addWidget(previewGroup, 1);

        // Program side
        auto* programGroup = new QGroupBox("PROGRAM (LIVE)");
        programGroup->setObjectName("SwitchGroupLive");
        auto* pgLay = new QVBoxLayout(programGroup);
        m_programLabel = new PreviewLabel;
        m_programLabel->setLiveIndicator(true);
        pgLay->addWidget(m_programLabel);
        m_programName = new QLabel("Blank");
        m_programName->setAlignment(Qt::AlignCenter);
        m_programName->setObjectName("SwitchSourceLabel");
        pgLay->addWidget(m_programName);
        monitorRow->addWidget(programGroup, 1);

        root->addLayout(monitorRow, 3);

        // ── Source selection row ─────────────────────────────────────────
        auto* srcGroup = new QGroupBox("Sources");
        srcGroup->setObjectName("SwitchGroup");
        auto* srcLay = new QHBoxLayout(srcGroup);
        srcLay->setSpacing(4);

        struct SrcDef { QString label; M1::SourceType type; };
        const SrcDef sources[] = {
            {"Lyrics",    M1::SourceType::Lyrics},
            {"Scripture", M1::SourceType::Scripture},
            {"Announce",  M1::SourceType::Announce},
            {"Media",     M1::SourceType::MediaCaster},
            {"Video",     M1::SourceType::Video},
            {"Logo",      M1::SourceType::Logo},
            {"BLK",       M1::SourceType::Blank},
        };
        for (auto& sd : sources) {
            auto* btn = new SourceButton(sd.label, sd.type);
            connect(btn, &QPushButton::clicked, this, [this, btn]() {
                m_mod->setPreview(btn->sourceType());
            });
            srcLay->addWidget(btn);
            m_sourceButtons.append(btn);
        }
        root->addWidget(srcGroup);

        // ── Transition controls ─────────────────────────────────────────
        auto* transRow = new QHBoxLayout;
        transRow->setSpacing(6);

        auto* cutBtn = new QPushButton("CUT");
        cutBtn->setObjectName("SwitchTransBtn");
        cutBtn->setMinimumHeight(36);
        QFont bf = cutBtn->font();
        bf.setPixelSize(13);
        bf.setBold(true);
        cutBtn->setFont(bf);
        connect(cutBtn, &QPushButton::clicked, m_mod, &M1::SwitchCasterModule::cut);
        transRow->addWidget(cutBtn);

        auto* dissolveBtn = new QPushButton("DISSOLVE");
        dissolveBtn->setObjectName("SwitchTransBtn");
        dissolveBtn->setMinimumHeight(36);
        dissolveBtn->setFont(bf);
        m_dissolveSpin = new QSpinBox;
        m_dissolveSpin->setRange(200, 5000);
        m_dissolveSpin->setValue(1000);
        m_dissolveSpin->setSuffix(" ms");
        m_dissolveSpin->setObjectName("SwitchSpinBox");
        connect(dissolveBtn, &QPushButton::clicked, this, [this]() {
            m_mod->dissolve(m_dissolveSpin->value());
        });
        transRow->addWidget(dissolveBtn);
        transRow->addWidget(m_dissolveSpin);

        auto* ftbBtn = new QPushButton("FTB");
        ftbBtn->setObjectName("SwitchTransBtn");
        ftbBtn->setMinimumHeight(36);
        ftbBtn->setFont(bf);
        ftbBtn->setToolTip("Fade to Black");
        connect(ftbBtn, &QPushButton::clicked, m_mod, &M1::SwitchCasterModule::fadeToBlack);
        transRow->addWidget(ftbBtn);

        transRow->addStretch();

        // Lower third controls
        auto* ltLabel = new QLabel("Lower Third:");
        ltLabel->setObjectName("SwitchLabel");
        transRow->addWidget(ltLabel);

        m_ltName = new QLineEdit;
        m_ltName->setPlaceholderText("Name");
        m_ltName->setObjectName("SwitchLineEdit");
        m_ltName->setMaximumWidth(120);
        transRow->addWidget(m_ltName);

        m_ltTitle = new QLineEdit;
        m_ltTitle->setPlaceholderText("Title");
        m_ltTitle->setObjectName("SwitchLineEdit");
        m_ltTitle->setMaximumWidth(120);
        transRow->addWidget(m_ltTitle);

        auto* ltShowBtn = new QPushButton("Show");
        ltShowBtn->setObjectName("SwitchTransBtn");
        connect(ltShowBtn, &QPushButton::clicked, this, [this]() {
            m_mod->showLowerThird(m_ltName->text(), m_ltTitle->text());
        });
        transRow->addWidget(ltShowBtn);

        auto* ltHideBtn = new QPushButton("Hide");
        ltHideBtn->setObjectName("SwitchTransBtn");
        connect(ltHideBtn, &QPushButton::clicked, m_mod, &M1::SwitchCasterModule::hideLowerThird);
        transRow->addWidget(ltHideBtn);

        root->addLayout(transRow);

        // ── Transition progress bar ─────────────────────────────────────
        m_transProgress = new QLabel;
        m_transProgress->setFixedHeight(4);
        m_transProgress->setObjectName("SwitchTransProgress");
        root->addWidget(m_transProgress);

        // ── Refresh connection ──────────────────────────────────────────
        connect(m_mod, &M1::SwitchCasterModule::frameUpdated, this, &SwitchCasterWidget::onRefresh);
        connect(m_mod, &M1::SwitchCasterModule::programChanged, this, &SwitchCasterWidget::onRefresh);
        connect(m_mod, &M1::SwitchCasterModule::previewChanged, this, &SwitchCasterWidget::onRefresh);

        onRefresh();
    }

private slots:
    void onRefresh() {
        m_previewLabel->setFrame(m_mod->previewFrame());
        m_programLabel->setFrame(m_mod->programFrame());
        m_previewName->setText(M1::SwitchCasterModule::sourceTypeName(m_mod->previewSource()));
        m_programName->setText(M1::SwitchCasterModule::sourceTypeName(m_mod->programSource()));

        // Update source button checked states (preview selection)
        for (auto* btn : m_sourceButtons) {
            btn->setChecked(btn->sourceType() == m_mod->previewSource());
        }
    }

private:
    M1::SwitchCasterModule* m_mod;
    PreviewLabel*           m_previewLabel;
    PreviewLabel*           m_programLabel;
    QLabel*                 m_previewName;
    QLabel*                 m_programName;
    QList<SourceButton*>    m_sourceButtons;
    QSpinBox*               m_dissolveSpin;
    QLineEdit*              m_ltName;
    QLineEdit*              m_ltTitle;
    QLabel*                 m_transProgress;
};

} // anonymous namespace

#include "SwitchCasterModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
SwitchCasterModule::SwitchCasterModule(QObject* parent)
    : IModule(parent)
{
}

SwitchCasterModule::~SwitchCasterModule() = default;

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void SwitchCasterModule::initialize() {
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100); // 10fps preview refresh
    connect(m_refreshTimer, &QTimer::timeout, this, &SwitchCasterModule::onRefresh);
    m_refreshTimer->start();

    m_transTimer = new QTimer(this);
    m_transTimer->setInterval(16); // ~60fps transition
    connect(m_transTimer, &QTimer::timeout, this, &SwitchCasterModule::onTransitionTick);
}

void SwitchCasterModule::shutdown() {
    if (m_refreshTimer) m_refreshTimer->stop();
    if (m_transTimer)   m_transTimer->stop();
}

QWidget* SwitchCasterModule::createWidget(QWidget* parent) {
    return new SwitchCasterWidget(this, parent);
}

void SwitchCasterModule::saveState(QSettings& s) {
    s.setValue("programSource", static_cast<int>(m_programSource));
    s.setValue("previewSource", static_cast<int>(m_previewSource));
}

void SwitchCasterModule::loadState(QSettings& s) {
    m_programSource = static_cast<SourceType>(s.value("programSource", 0).toInt());
    m_previewSource = static_cast<SourceType>(s.value("previewSource", 0).toInt());
}

// ─── Program / Preview bus ──────────────────────────────────────────────────
void SwitchCasterModule::setPreview(SourceType src) {
    if (m_previewSource == src) return;
    m_previewSource = src;
    m_previewImg = frameForSource(src);
    emit previewChanged(static_cast<int>(src));
    emit frameUpdated();
}

void SwitchCasterModule::setProgram(SourceType src) {
    if (m_programSource == src) return;
    m_programSource = src;
    m_programImg = frameForSource(src);
    emit programChanged(static_cast<int>(src));
    emit frameUpdated();
}

// ─── Transitions ────────────────────────────────────────────────────────────
void SwitchCasterModule::cut() {
    if (m_previewSource == SourceType::None) return;

    m_programSource = m_previewSource;
    m_programImg    = frameForSource(m_programSource);

    emit programChanged(static_cast<int>(m_programSource));
    emit frameUpdated();
}

void SwitchCasterModule::dissolve(int durationMs) {
    if (m_previewSource == SourceType::None || m_transitioning) return;

    m_transitioning   = true;
    m_transType       = TransitionType::Dissolve;
    m_transFromSource = m_programSource;
    m_transToSource   = m_previewSource;
    m_transDurationMs = durationMs;
    m_transElapsedMs  = 0;
    m_transFromFrame  = m_programImg;
    m_transToFrame    = frameForSource(m_transToSource);

    emit transitionStarted(static_cast<int>(TransitionType::Dissolve));
    m_transTimer->start();
}

void SwitchCasterModule::fadeToBlack() {
    if (m_transitioning) return;

    m_transitioning   = true;
    m_transType       = TransitionType::FadeToBlack;
    m_transFromSource = m_programSource;
    m_transToSource   = SourceType::Blank;
    m_transDurationMs = 800;
    m_transElapsedMs  = 0;
    m_transFromFrame  = m_programImg;
    m_transToFrame    = QImage(); // black

    m_previewSource = SourceType::Blank;
    emit previewChanged(static_cast<int>(SourceType::Blank));
    emit transitionStarted(static_cast<int>(TransitionType::FadeToBlack));
    m_transTimer->start();
}

void SwitchCasterModule::fadeFromBlack() {
    if (m_previewSource == SourceType::None || m_transitioning) return;

    m_transitioning   = true;
    m_transType       = TransitionType::FadeToBlack; // reuse dissolve logic
    m_transFromSource = SourceType::Blank;
    m_transToSource   = m_previewSource;
    m_transDurationMs = 800;
    m_transElapsedMs  = 0;
    m_transFromFrame  = QImage();
    m_transToFrame    = frameForSource(m_transToSource);

    emit transitionStarted(static_cast<int>(TransitionType::Dissolve));
    m_transTimer->start();
}

float SwitchCasterModule::transitionProgress() const {
    if (!m_transitioning || m_transDurationMs <= 0) return 0.0f;
    return std::clamp(static_cast<float>(m_transElapsedMs) / m_transDurationMs, 0.0f, 1.0f);
}

// ─── Lower third ───────────────────────────────────────────────────────────
void SwitchCasterModule::showLowerThird(const QString& name, const QString& title) {
    m_lowerThirdName    = name;
    m_lowerThirdTitle   = title;
    m_lowerThirdVisible = true;

    // If we have a graphics engine, tell the announce caster to show it
    if (m_announce) {
        m_announce->showLowerThird(name, title);
    }

    emit lowerThirdChanged(true);
    emit frameUpdated();
}

void SwitchCasterModule::hideLowerThird() {
    m_lowerThirdVisible = false;

    if (m_announce) {
        m_announce->hideLowerThird();
    }

    emit lowerThirdChanged(false);
    emit frameUpdated();
}

// ─── Frame access ──────────────────────────────────────────────────────────
QImage SwitchCasterModule::programFrame() const {
    if (m_transitioning) {
        const float prog = std::clamp(
            static_cast<float>(m_transElapsedMs) / std::max(m_transDurationMs, 1), 0.0f, 1.0f);
        return blendFrames(m_transFromFrame, m_transToFrame, prog);
    }
    return m_programImg;
}

QImage SwitchCasterModule::previewFrame() const {
    return m_previewImg;
}

// ─── Source name ───────────────────────────────────────────────────────────
QString SwitchCasterModule::sourceTypeName(SourceType src) {
    switch (src) {
    case SourceType::None:        return "None";
    case SourceType::Lyrics:      return "Lyrics";
    case SourceType::Scripture:   return "Scripture";
    case SourceType::Announce:    return "Announcements";
    case SourceType::MediaCaster: return "Media";
    case SourceType::Video:       return "Video";
    case SourceType::Camera:      return "Camera";
    case SourceType::Logo:        return "Logo";
    case SourceType::Blank:       return "Blank";
    }
    return "Unknown";
}

// ─── Private slots ─────────────────────────────────────────────────────────
void SwitchCasterModule::onRefresh() {
    // Refresh the current program and preview frames from source modules
    m_programImg = frameForSource(m_programSource);
    m_previewImg = frameForSource(m_previewSource);
    emit frameUpdated();
}

void SwitchCasterModule::onTransitionTick() {
    m_transElapsedMs += 16;

    if (m_transElapsedMs >= m_transDurationMs) {
        // Transition complete
        m_transitioning = false;
        m_transTimer->stop();

        m_programSource = m_transToSource;
        m_programImg    = frameForSource(m_programSource);

        emit programChanged(static_cast<int>(m_programSource));
        emit transitionFinished();
    }
    emit frameUpdated();
}

// ─── Helpers ────────────────────────────────────────────────────────────────
QImage SwitchCasterModule::frameForSource(SourceType src) const {
    switch (src) {
    case SourceType::Lyrics:
        if (m_lyrics) return m_lyrics->currentFrame();
        break;
    case SourceType::Scripture:
        if (m_scripture) return m_scripture->currentFrame();
        break;
    case SourceType::Announce:
        if (m_announce) return m_announce->currentFrame();
        break;
    case SourceType::MediaCaster:
        // Media caster doesn't expose a frame directly — use graphics engine
        break;
    case SourceType::Video:
        // Video module uses QMediaPlayer — frame access would need grab
        break;
    case SourceType::Logo:
        if (m_graphics) return m_graphics->renderBlank();
        break;
    case SourceType::Blank:
        // Return empty image (renders as black)
        return QImage();
    case SourceType::Camera:
    case SourceType::None:
        break;
    }

    // Fallback — blank frame
    return QImage();
}

QImage SwitchCasterModule::blendFrames(const QImage& from, const QImage& to, float progress) const {
    const QSize sz(640, 360); // preview resolution

    QImage result(sz, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::black);

    QPainter p(&result);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // Draw "from" frame with fading opacity
    if (!from.isNull()) {
        p.setOpacity(1.0 - progress);
        p.drawImage(result.rect(), from);
    }

    // Draw "to" frame with increasing opacity
    if (!to.isNull()) {
        p.setOpacity(progress);
        p.drawImage(result.rect(), to);
    }

    return result;
}

} // namespace M1
