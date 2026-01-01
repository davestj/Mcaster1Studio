#include "CrossfaderModule.h"
#include "CrossfaderSettingsDialog.h"
#include "DeckAModule.h"
#include "DeckBModule.h"
#include "DeckPlayer.h"
#include "ThemeManager.h"
#include "IPlugin.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QDoubleSpinBox>
#include <QPainter>
#include <QLinearGradient>
#include <QSettings>
#include <QDebug>
#include <cmath>
#include <algorithm>

#ifndef M_PI_2
#  define M_PI_2 1.57079632679489661923
#endif

// ─── Theme-aware colors ─────────────────────────────────────────────────────
namespace {

struct CfColors {
    QString bg, panel, border, textPrimary, textMuted, accent;
    QString grooveColor, handleBg, handleBorder;
    QString subPageColor, addPageColor;
};

CfColors cfColors() {
    using T = ThemeManager::Theme;
    switch (ThemeManager::instance()->currentTheme()) {
    case T::Light:
        return { "#f0f0f0","#ffffff","#cccccc","#1a1a1a","#888888","#1c5caa",
                 "#d0d0d0","qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #ffffff,stop:1 #d8d8d8)",
                 "#999999","#1c5caa","#d0d0d0" };
    case T::Classic:
        return { "#e8e0d0","#f0e8d8","#bfb090","#2a1810","#806040","#8b3a0a",
                 "#c8c2b8","qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #e8e2d8,stop:1 #cec8be)",
                 "#8a8070","#8b3a0a","#c8c2b8" };
    default: // Dark
        return { "#1e2128","#252830","#3a3f4b","#e0e4ec","#8890a0","#1c5caa",
                 "#2a2e38","qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #4a5060,stop:1 #353a48)",
                 "#5a6070","#1c5caa","#2a2e38" };
    }
}

QString cfBtnQss(const CfColors& c, const QString& checkColor = QString()) {
    const QString chk = checkColor.isEmpty() ? c.accent : checkColor;
    return QString(
        "QPushButton {"
        "  background:%1; color:%2; border:1px solid %3; border-radius:3px;"
        "  font-size:12px; font-weight:700; padding:4px 10px; min-height:26px;"
        "}"
        "QPushButton:hover  { background:%4; border-color:%5; }"
        "QPushButton:pressed{ background:%6; }"
        "QPushButton:checked{ background:%5; color:#ffffff; border-color:%5; }"
        "QPushButton:disabled{ color:%7; }"
    ).arg(c.panel, c.textPrimary, c.border,
          QColor(c.panel).lighter(115).name(), chk,
          QColor(c.panel).darker(115).name(), c.textMuted);
}

QString cfSliderQss(const CfColors& c) {
    return QString(
        "QSlider::groove:horizontal {"
        "  background:%1; height:14px; border-radius:7px;"
        "  border:2px solid %2;"
        "}"
        "QSlider::handle:horizontal {"
        "  background:%3;"
        "  border:2px solid %4; width:28px; height:36px;"
        "  margin:-12px 0; border-radius:5px;"
        "}"
        "QSlider::handle:horizontal:hover {"
        "  border-color:%5;"
        "}"
        "QSlider::sub-page:horizontal {"
        "  background:%6; border-radius:7px;"
        "}"
        "QSlider::add-page:horizontal {"
        "  background:%7; border-radius:7px;"
        "}"
    ).arg(c.grooveColor, c.border, c.handleBg, c.handleBorder,
          c.accent, c.subPageColor, c.addPageColor);
}

} // anonymous namespace

// ─── CrossfaderPanel — the actual widget ─────────────────────────────────────
class CrossfaderPanel : public QWidget {
    Q_OBJECT
public:
    explicit CrossfaderPanel(CrossfaderModule* mod, QWidget* parent = nullptr);

    QSlider*        slider()       const { return m_slider; }
    QPushButton*    autoBtn()      const { return m_autoBtn; }
    QDoubleSpinBox* fadeDurSpin()  const { return m_durSpin; }
    QPushButton*    stdBtn()       const { return m_stdBtn; }
    QPushButton*    sCrvBtn()      const { return m_sCrvBtn; }
    QPushButton*    expBtn()       const { return m_expBtn; }
    QLabel*         posLabel()     const { return m_posLabel; }

    void refreshTheme();

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    CrossfaderModule* m_mod;
    QSlider*        m_slider   = nullptr;
    QPushButton*    m_autoBtn  = nullptr;
    QPushButton*    m_fadeABBtn= nullptr;
    QPushButton*    m_fadeBABtn= nullptr;
    QPushButton*    m_stdBtn   = nullptr;
    QPushButton*    m_sCrvBtn  = nullptr;
    QPushButton*    m_expBtn   = nullptr;
    QPushButton*    m_cfgBtn   = nullptr;
    QDoubleSpinBox* m_durSpin  = nullptr;
    QLabel*         m_posLabel = nullptr;
    QLabel*         m_aLabel   = nullptr;
    QLabel*         m_bLabel   = nullptr;
};

CrossfaderPanel::CrossfaderPanel(CrossfaderModule* mod, QWidget* parent)
    : QWidget(parent), m_mod(mod)
{
    setObjectName("CrossfaderPanel");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 6, 10, 6);
    root->setSpacing(4);

    // ── A ═══ SLIDER ═══ B row ─────────────────────────────────────────────
    {
        auto* sliderRow = new QHBoxLayout;
        sliderRow->setSpacing(8);

        m_aLabel = new QLabel("A", this);
        m_aLabel->setObjectName("CfDeckLabel");
        m_aLabel->setAlignment(Qt::AlignCenter);
        m_aLabel->setFixedWidth(28);

        m_slider = new QSlider(Qt::Horizontal, this);
        m_slider->setObjectName("CrossfaderMainSlider");
        m_slider->setRange(0, 1000);
        m_slider->setValue(500);
        m_slider->setTickPosition(QSlider::TicksBelow);
        m_slider->setTickInterval(100);
        m_slider->setMinimumHeight(36);
        m_slider->setToolTip("Crossfader — drag left for Deck A, right for Deck B");

        m_bLabel = new QLabel("B", this);
        m_bLabel->setObjectName("CfDeckLabel");
        m_bLabel->setAlignment(Qt::AlignCenter);
        m_bLabel->setFixedWidth(28);

        sliderRow->addWidget(m_aLabel);
        sliderRow->addWidget(m_slider, 1);
        sliderRow->addWidget(m_bLabel);
        root->addLayout(sliderRow);
    }

    // ── Position indicator ─────────────────────────────────────────────────
    {
        m_posLabel = new QLabel("50%", this);
        m_posLabel->setObjectName("CfPosLabel");
        m_posLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(m_posLabel);
    }

    // ── Curve buttons + fade duration + settings ───────────────────────────
    {
        auto* ctrlRow = new QHBoxLayout;
        ctrlRow->setSpacing(4);

        m_stdBtn  = new QPushButton("STD",   this);
        m_sCrvBtn = new QPushButton("S-CRV", this);
        m_expBtn  = new QPushButton("EXP",   this);

        for (auto* b : {m_stdBtn, m_sCrvBtn, m_expBtn}) {
            b->setCheckable(true);
            b->setFixedHeight(24);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        m_stdBtn ->setToolTip("Standard linear crossfade");
        m_sCrvBtn->setToolTip("Equal-power S-Curve (recommended)");
        m_expBtn ->setToolTip("Exponential curve — sharp cut");
        m_sCrvBtn->setChecked(true);

        // Curve button signals → module
        connect(m_stdBtn,  &QPushButton::clicked, mod, [mod, this]() {
            mod->setCurveMode(0);
            m_stdBtn->setChecked(true); m_sCrvBtn->setChecked(false); m_expBtn->setChecked(false);
        });
        connect(m_sCrvBtn, &QPushButton::clicked, mod, [mod, this]() {
            mod->setCurveMode(1);
            m_stdBtn->setChecked(false); m_sCrvBtn->setChecked(true); m_expBtn->setChecked(false);
        });
        connect(m_expBtn,  &QPushButton::clicked, mod, [mod, this]() {
            mod->setCurveMode(2);
            m_stdBtn->setChecked(false); m_sCrvBtn->setChecked(false); m_expBtn->setChecked(true);
        });

        auto* durLbl = new QLabel("Fade:", this);
        durLbl->setObjectName("CfSmallLabel");

        m_durSpin = new QDoubleSpinBox(this);
        m_durSpin->setObjectName("CfDurSpin");
        m_durSpin->setRange(0.5, 30.0);
        m_durSpin->setSingleStep(0.5);
        m_durSpin->setValue(5.0);
        m_durSpin->setSuffix(" s");
        m_durSpin->setDecimals(1);
        m_durSpin->setFixedHeight(24);
        m_durSpin->setToolTip("Crossfade duration in seconds");

        m_cfgBtn = new QPushButton("Settings", this);
        m_cfgBtn->setFixedHeight(24);
        m_cfgBtn->setToolTip("Open advanced crossfade curve settings");
        connect(m_cfgBtn, &QPushButton::clicked, mod, [mod]() {
            mod->openSettingsDialog();
        });

        ctrlRow->addWidget(m_stdBtn,  1);
        ctrlRow->addWidget(m_sCrvBtn, 1);
        ctrlRow->addWidget(m_expBtn,  1);
        ctrlRow->addSpacing(6);
        ctrlRow->addWidget(durLbl);
        ctrlRow->addWidget(m_durSpin);
        ctrlRow->addSpacing(6);
        ctrlRow->addWidget(m_cfgBtn);
        root->addLayout(ctrlRow);
    }

    // ── Separator ──────────────────────────────────────────────────────────
    {
        auto* sep = new QFrame(this);
        sep->setObjectName("CfSep");
        sep->setFrameShape(QFrame::HLine);
        sep->setFixedHeight(2);
        root->addWidget(sep);
    }

    // ── AUTO CROSSFADE + A→B / B→A buttons ─────────────────────────────────
    {
        auto* fadeRow = new QHBoxLayout;
        fadeRow->setSpacing(4);

        m_fadeBABtn = new QPushButton("A", this);
        m_fadeBABtn->setFixedHeight(32);
        m_fadeBABtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_fadeBABtn->setToolTip("Fade to Deck A over configured duration");
        connect(m_fadeBABtn, &QPushButton::clicked, mod, [mod]() {
            mod->animateTo(0.0f, (int)(mod->fadeDuration() * 1000.0f));
        });

        m_autoBtn = new QPushButton("AUTO CROSSFADE", this);
        m_autoBtn->setObjectName("CfAutoButton");
        m_autoBtn->setCheckable(true);
        m_autoBtn->setFixedHeight(32);
        m_autoBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_autoBtn->setToolTip(
            "AUTO CROSSFADE — Intelligently detects the active deck and fades\n"
            "to the opposite deck. Uses remaining track time for timing.\n"
            "Respects the configured fade curve and duration.");
        connect(m_autoBtn, &QPushButton::clicked, mod, [mod]() {
            mod->autoCrossfade();
        });

        m_fadeABBtn = new QPushButton("B", this);
        m_fadeABBtn->setFixedHeight(32);
        m_fadeABBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_fadeABBtn->setToolTip("Fade to Deck B over configured duration");
        connect(m_fadeABBtn, &QPushButton::clicked, mod, [mod]() {
            mod->animateTo(1.0f, (int)(mod->fadeDuration() * 1000.0f));
        });

        fadeRow->addWidget(m_fadeBABtn, 1);
        fadeRow->addWidget(m_autoBtn,   3);
        fadeRow->addWidget(m_fadeABBtn, 1);
        root->addLayout(fadeRow);
    }

    // ── Slider → module ────────────────────────────────────────────────────
    connect(m_slider, &QSlider::valueChanged, this, [this, mod](int pos) {
        const float v = (float)pos / 1000.0f;
        m_posLabel->setText(QString("%1%").arg((int)(v * 100.0f)));
        mod->setValue(v);
    });

    // ── Theme wiring ───────────────────────────────────────────────────────
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &CrossfaderPanel::refreshTheme);
    refreshTheme();
}

void CrossfaderPanel::refreshTheme() {
    const auto c = cfColors();

    // Deck A/B labels
    const QString deckLblQss = QString(
        "QLabel { color:%1; font-size:16px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace;"
        "  background:transparent; }"
    ).arg(c.accent);
    m_aLabel->setStyleSheet(deckLblQss);
    m_bLabel->setStyleSheet(deckLblQss);

    // Position label
    m_posLabel->setStyleSheet(QString(
        "QLabel { color:%1; font-size:12px; font-weight:700;"
        "  font-family:'Consolas','Courier New',monospace;"
        "  background:transparent; }"
    ).arg(c.textMuted));

    // Main slider
    m_slider->setStyleSheet(cfSliderQss(c));

    // Curve + control buttons
    for (auto* b : {m_stdBtn, m_sCrvBtn, m_expBtn, m_cfgBtn})
        b->setStyleSheet(cfBtnQss(c));

    // Fade A / B buttons
    m_fadeBABtn->setStyleSheet(cfBtnQss(c, "#22aa55"));
    m_fadeABBtn->setStyleSheet(cfBtnQss(c, "#22aa55"));

    // AUTO CROSSFADE — amber/gold accent
    m_autoBtn->setStyleSheet(QString(
        "QPushButton {"
        "  background:%1; color:%2; border:2px solid %3; border-radius:4px;"
        "  font-size:13px; font-weight:900; letter-spacing:1px;"
        "}"
        "QPushButton:hover  { background:%4; border-color:#d97706; }"
        "QPushButton:pressed{ background:%5; }"
        "QPushButton:checked{ background:#d97706; color:#ffffff; border-color:#b45309; }"
    ).arg(c.panel, c.textPrimary, c.border,
          QColor(c.panel).lighter(115).name(),
          QColor(c.panel).darker(115).name()));

    // Fade duration spinbox
    m_durSpin->setStyleSheet(QString(
        "QDoubleSpinBox {"
        "  background:%1; color:%2; border:1px solid %3; border-radius:3px;"
        "  font-size:12px; padding:1px 4px;"
        "}"
    ).arg(c.panel, c.textPrimary, c.border));

    // Small labels
    for (auto* l : findChildren<QLabel*>("CfSmallLabel"))
        l->setStyleSheet(QString(
            "QLabel { color:%1; font-size:12px; font-weight:700; background:transparent; }"
        ).arg(c.textMuted));

    // Separator
    if (auto* sep = findChild<QFrame*>("CfSep"))
        sep->setStyleSheet(QString("QFrame { color:%1; }").arg(c.border));

    update();
}

void CrossfaderPanel::paintEvent(QPaintEvent* e) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto c = cfColors();
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(c.bg));
    bg.setColorAt(1.0, QColor(c.panel));
    p.fillRect(rect(), bg);
    p.setPen(QPen(QColor(c.border), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);
    QWidget::paintEvent(e);
}

// ═════════════════════════════════════════════════════════════════════════════
// CrossfaderModule — IModule implementation
// ═════════════════════════════════════════════════════════════════════════════
CrossfaderModule::CrossfaderModule(QObject* parent)
    : M1::IModule(parent)
{
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(30);  // ~33 fps for smooth animation
    connect(m_animTimer, &QTimer::timeout, this, &CrossfaderModule::onAnimTick);
}

CrossfaderModule::~CrossfaderModule() {
    shutdown();
}

void CrossfaderModule::initialize() {
    // Load saved crossfade settings to sync curve mode
    QSettings s("Mcaster1", "Mcaster1Studio");
    CrossfadeSettings cfg;
    cfg.loadFromSettings(s);

    switch (cfg.fadeOutCurve) {
    case CrossfadeSettings::Linear:      m_curveMode = 0; break;
    case CrossfadeSettings::SCurve:      m_curveMode = 1; break;
    case CrossfadeSettings::Exponential: m_curveMode = 2; break;
    default:                             m_curveMode = 1; break;
    }

    qInfo() << "[CrossfaderModule] Initialized. Curve mode:" << m_curveMode;
}

void CrossfaderModule::shutdown() {
    if (m_animTimer && m_animTimer->isActive())
        m_animTimer->stop();
}

QWidget* CrossfaderModule::createWidget(QWidget* parent) {
    auto* panel = new CrossfaderPanel(this, parent);
    m_widget = panel;
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });

    // Sync initial state
    panel->slider()->setValue((int)(m_position * 1000.0f));
    if (auto* dur = panel->fadeDurSpin())
        dur->setValue(5.0);

    // Sync curve buttons
    if (panel->stdBtn())  panel->stdBtn()->setChecked(m_curveMode == 0);
    if (panel->sCrvBtn()) panel->sCrvBtn()->setChecked(m_curveMode == 1);
    if (panel->expBtn())  panel->expBtn()->setChecked(m_curveMode == 2);

    return panel;
}

void CrossfaderModule::connectDecks(M1::DeckAModule* a, M1::DeckBModule* b) {
    m_deckA = a;
    m_deckB = b;
    qInfo() << "[CrossfaderModule] Connected to decks: A="
            << (a ? "YES" : "null") << " B=" << (b ? "YES" : "null");
    // Apply current gains immediately
    applyGains(m_position);
}

float CrossfaderModule::value() const {
    return m_position;
}

float CrossfaderModule::fadeDuration() const {
    if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget)) {
        if (panel->fadeDurSpin())
            return (float)panel->fadeDurSpin()->value();
    }
    return 5.0f;
}

void CrossfaderModule::setCurveMode(int mode) {
    m_curveMode = std::clamp(mode, 0, 2);
    applyGains(m_position);
}

void CrossfaderModule::setValue(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    m_position = v;
    applyGains(v);
    emit crossfaderChanged(v);
}

void CrossfaderModule::animateTo(float target, int durationMs) {
    if (durationMs <= 0) {
        setValue(target);
        // Update slider widget
        if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget))
            panel->slider()->setValue((int)(target * 1000.0f));
        return;
    }
    m_animTarget = std::clamp(target, 0.0f, 1.0f);
    const int ticks = std::max(1, durationMs / 30);
    m_animStep = (m_animTarget - m_position) / (float)ticks;
    if (!m_animTimer->isActive()) m_animTimer->start();

    qInfo() << "[CrossfaderModule] animateTo:" << target
            << "over" << durationMs << "ms, step=" << m_animStep;
}

void CrossfaderModule::autoCrossfade() {
    M1::DeckPlayer* playerA = m_deckA ? m_deckA->player() : nullptr;
    M1::DeckPlayer* playerB = m_deckB ? m_deckB->player() : nullptr;

    if (!playerA && !playerB) {
        qWarning() << "[CrossfaderModule] autoCrossfade: No deck players connected.";
        if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget))
            panel->autoBtn()->setChecked(false);
        return;
    }

    const bool aPlaying = playerA && (playerA->state() == M1::DeckPlayer::State::Playing);
    const bool bPlaying = playerB && (playerB->state() == M1::DeckPlayer::State::Playing);
    const bool aReady   = playerA && (playerA->state() == M1::DeckPlayer::State::Ready ||
                                      playerA->state() == M1::DeckPlayer::State::Paused);
    const bool bReady   = playerB && (playerB->state() == M1::DeckPlayer::State::Ready ||
                                      playerB->state() == M1::DeckPlayer::State::Paused);

    float target = 0.5f;
    m_fadeSourceDeck = -1;

    if (aPlaying && !bPlaying) {
        target = 1.0f;
        m_fadeSourceDeck = 0;  // A is fading out
        if (bReady && playerB) playerB->play();
    } else if (bPlaying && !aPlaying) {
        target = 0.0f;
        m_fadeSourceDeck = 1;  // B is fading out
        if (aReady && playerA) playerA->play();
    } else {
        // Both playing or neither — use crossfader position to pick direction
        if (m_position <= 0.5f) {
            target = 1.0f;
            m_fadeSourceDeck = 0;  // A is fading out
            if (bReady && playerB) playerB->play();
        } else {
            target = 0.0f;
            m_fadeSourceDeck = 1;  // B is fading out
            if (aReady && playerA) playerA->play();
        }
    }

    m_autoFadeActive = true;

    // Always use the configured fade duration from the spinbox
    const float dur = fadeDuration();
    const int durMs = (int)(dur * 1000.0f);

    qInfo() << "[CrossfaderModule] AUTO CROSSFADE: target=" << target
            << "duration=" << dur << "s, source deck=" << m_fadeSourceDeck;

    animateTo(target, durMs);
}

void CrossfaderModule::openSettingsDialog() {
    CrossfaderSettingsDialog dlg(m_widget);
    dlg.loadSettings();
    if (dlg.exec() != QDialog::Accepted) return;
    dlg.saveSettings();

    const CrossfadeSettings cfg = dlg.settings();

    // Sync fade duration
    if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget))
        panel->fadeDurSpin()->setValue((double)cfg.fadeOutTime);

    // Sync curve mode
    int curveIdx = 1;
    switch (cfg.fadeOutCurve) {
    case CrossfadeSettings::Linear:      curveIdx = 0; break;
    case CrossfadeSettings::SCurve:      curveIdx = 1; break;
    case CrossfadeSettings::Exponential: curveIdx = 2; break;
    case CrossfadeSettings::Custom:      curveIdx = 1; break;
    }
    setCurveMode(curveIdx);

    // Sync curve button visuals
    if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget)) {
        panel->stdBtn()->setChecked(curveIdx == 0);
        panel->sCrvBtn()->setChecked(curveIdx == 1);
        panel->expBtn()->setChecked(curveIdx == 2);
    }
}

void CrossfaderModule::onAnimTick() {
    const float delta = m_animTarget - m_position;
    if (std::abs(delta) < 0.003f) {
        // Snap to target
        m_position = m_animTarget;
        m_animTimer->stop();

        // ── Auto-fade complete: stop old deck + reset to center ──────────
        if (m_autoFadeActive) {
            m_autoFadeActive = false;

            // Stop the source deck (the one that was fading out)
            if (m_fadeSourceDeck == 0 && m_deckA && m_deckA->player()) {
                qInfo() << "[CrossfaderModule] Auto-fade complete. Stopping Deck A.";
                m_deckA->player()->stop();
            } else if (m_fadeSourceDeck == 1 && m_deckB && m_deckB->player()) {
                qInfo() << "[CrossfaderModule] Auto-fade complete. Stopping Deck B.";
                m_deckB->player()->stop();
            }
            const int stoppedDeck = m_fadeSourceDeck;
            m_fadeSourceDeck = -1;

            // Notify PlaylistModule so it can advance + preload next track
            emit autoFadeCompleted(stoppedDeck);

            // Reset crossfader to center (50/50) after a brief delay
            // so the stop takes effect first
            QTimer::singleShot(200, this, [this]() {
                m_position = 0.5f;
                applyGains(m_position);
                emit crossfaderChanged(m_position);
                if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget)) {
                    panel->slider()->blockSignals(true);
                    panel->slider()->setValue(500);
                    panel->slider()->blockSignals(false);
                    panel->posLabel()->setText("50%");
                }
                qInfo() << "[CrossfaderModule] Crossfader reset to center. Ready for next fade.";
            });
        }

        // Uncheck AUTO button
        if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget))
            panel->autoBtn()->setChecked(false);
    } else {
        float next = m_position + m_animStep;
        // Clamp to avoid overshoot
        if (m_animStep > 0) next = std::min(next, m_animTarget);
        else                next = std::max(next, m_animTarget);
        m_position = next;
    }

    applyGains(m_position);
    emit crossfaderChanged(m_position);

    // Update slider widget (block signals to prevent feedback loop)
    if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget)) {
        panel->slider()->blockSignals(true);
        panel->slider()->setValue((int)(m_position * 1000.0f));
        panel->slider()->blockSignals(false);
        panel->posLabel()->setText(QString("%1%").arg((int)(m_position * 100.0f)));
    }
}

void CrossfaderModule::applyGains(float pos) {
    float gainA = 1.0f, gainB = 1.0f;

    switch (m_curveMode) {
    case 0: // Linear
        gainA = 1.0f - pos;
        gainB = pos;
        break;

    case 1: // S-Curve (equal-power cosine)
        gainA = std::cos(pos * (float)M_PI_2);
        gainB = std::cos((1.0f - pos) * (float)M_PI_2);
        break;

    case 2: // Exponential
        gainA = (1.0f - pos) * (1.0f - pos);
        gainB = pos * pos;
        break;

    default:
        gainA = 1.0f - pos;
        gainB = pos;
        break;
    }

    gainA = std::clamp(gainA, 0.0f, 1.0f);
    gainB = std::clamp(gainB, 0.0f, 1.0f);

    if (m_deckA) m_deckA->setCrossfaderGain(gainA);
    if (m_deckB) m_deckB->setCrossfaderGain(gainB);
}

void CrossfaderModule::saveState(QSettings& s) {
    s.beginGroup("CrossfaderModule");
    s.setValue("position",  (double)m_position);
    s.setValue("curveMode", m_curveMode);
    if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget))
        s.setValue("fadeDuration", panel->fadeDurSpin()->value());
    s.endGroup();
}

void CrossfaderModule::loadState(QSettings& s) {
    s.beginGroup("CrossfaderModule");
    m_position  = (float)s.value("position", 0.5).toDouble();
    m_curveMode = s.value("curveMode", 1).toInt();
    const double dur = s.value("fadeDuration", 5.0).toDouble();
    s.endGroup();

    applyGains(m_position);

    if (auto* panel = qobject_cast<CrossfaderPanel*>(m_widget)) {
        panel->slider()->setValue((int)(m_position * 1000.0f));
        panel->fadeDurSpin()->setValue(dur);
        panel->stdBtn()->setChecked(m_curveMode == 0);
        panel->sCrvBtn()->setChecked(m_curveMode == 1);
        panel->expBtn()->setChecked(m_curveMode == 2);
    }
}

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_crossfaderInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.crossfader",
    "Crossfader",
    "1.0.0",
    "alpha,beta,dj,entertainment,custom",
    "module",
    "Mcaster1",
    "Standalone DJ-style crossfader for Deck A / Deck B"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_crossfader_plugin_info() { return &s_crossfaderInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_crossfader_create_module(IModuleHost*) {
    return new CrossfaderModule();
}
MCASTER1_PLUGIN_API void mcaster1_crossfader_destroy_module(IModule* m) { delete m; }
}

#include "CrossfaderModule.moc"
