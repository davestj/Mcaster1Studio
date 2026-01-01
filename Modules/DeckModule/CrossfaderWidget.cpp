#include "CrossfaderWidget.h"
#include "CrossfaderSettingsDialog.h"
#include "DeckPlayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QGroupBox>
#include <QPainter>
#include <QLinearGradient>
#include <QSettings>
#include <algorithm>
#include <cmath>

// ─── Shared sandy-theme helpers ───────────────────────────────────────────────
namespace {
    const QColor kBgTop(245, 240, 233);
    const QColor kBgBot(232, 226, 218);
    const QColor kAccent("#1c5caa");
    const QColor kDivider(190, 180, 166);

    // Simple flat button QSS
    QString btnQss(const QString& checkColor = "#1c5caa") {
        return QString(
            "QPushButton {"
            "  background:#e1dbd2; color:#1a1814;"
            "  border:1px solid #a09080; border-radius:2px;"
            "  font-size:9px; font-weight:700; padding:1px 5px;"
            "}"
            "QPushButton:hover  { background:#d4cec5; border-color:%1; }"
            "QPushButton:pressed{ background:#cbc4bb; }"
            "QPushButton:checked{ background:%1; color:#ffffff; border-color:%1; }"
            "QPushButton:disabled{ background:#eae6e0; color:#aaa49a; }"
        ).arg(checkColor);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CrossfaderWidget
// ─────────────────────────────────────────────────────────────────────────────
CrossfaderWidget::CrossfaderWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("CrossfaderWidget");
    setMinimumSize(160, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // ── Root layout ──────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 5, 8, 5);
    root->setSpacing(5);

    // ── Title ─────────────────────────────────────────────────────────────────
    auto* titleLbl = new QLabel("CROSSFADER", this);
    titleLbl->setStyleSheet(
        "QLabel { color:#1c5caa; font-size:10px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace;"
        "  border-bottom:1px solid #c0b8ae; padding-bottom:2px; }");
    titleLbl->setAlignment(Qt::AlignHCenter);
    root->addWidget(titleLbl);

    // ── A / B deck BPM display ────────────────────────────────────────────────
    {
        auto* bpmRow = new QHBoxLayout;
        bpmRow->setSpacing(4);
        auto mkBpmLbl = [&](const QString& deck) -> QLabel* {
            auto* l = new QLabel(deck + ": ---", this);
            l->setStyleSheet("QLabel { color:#6b6258; font-size:8px;"
                             "  font-family:'Consolas','Courier New',monospace; }");
            return l;
        };
        m_aBpmLabel = mkBpmLbl("A");
        m_bBpmLabel = mkBpmLbl("B");
        bpmRow->addWidget(m_aBpmLabel);
        bpmRow->addStretch(1);
        bpmRow->addWidget(m_bBpmLabel);
        root->addLayout(bpmRow);
    }

    // ── A ── slider ── B row ──────────────────────────────────────────────────
    {
        auto* cfRow = new QHBoxLayout;
        cfRow->setSpacing(4);

        auto mkABLbl = [&](const QString& t) {
            auto* l = new QLabel(t, this);
            l->setStyleSheet("QLabel { color:#1c5caa; font-size:11px; font-weight:900;"
                             "  font-family:'Consolas','Courier New',monospace; }");
            l->setAlignment(Qt::AlignCenter);
            l->setFixedWidth(14);
            return l;
        };

        m_cfSlider = new QSlider(Qt::Horizontal, this);
        m_cfSlider->setObjectName("CrossfaderSlider");
        m_cfSlider->setRange(0, 1000);
        m_cfSlider->setValue(500);
        m_cfSlider->setTickPosition(QSlider::TicksBelow);
        m_cfSlider->setTickInterval(100);
        m_cfSlider->setToolTip("Crossfader — drag left for Deck A, right for Deck B");
        m_cfSlider->setStyleSheet(
            "QSlider::groove:horizontal {"
            "  background:#c8c2b8; height:7px; border-radius:3px; border:1px solid #a09080;"
            "}"
            "QSlider::handle:horizontal {"
            "  background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "    stop:0 #e8e2d8, stop:1 #cec8be);"
            "  border:1px solid #8a8070; width:14px; height:20px;"
            "  margin:-7px 0; border-radius:3px;"
            "}"
            "QSlider::sub-page:horizontal { background:#1c5caa; border-radius:3px; }"
            "QSlider::add-page:horizontal { background:#c8c2b8; border-radius:3px; }"
        );
        connect(m_cfSlider, &QSlider::valueChanged, this, &CrossfaderWidget::onCfSliderMoved);

        cfRow->addWidget(mkABLbl("A"));
        cfRow->addWidget(m_cfSlider, 1);
        cfRow->addWidget(mkABLbl("B"));
        root->addLayout(cfRow);
    }

    // ── Curve buttons ─────────────────────────────────────────────────────────
    {
        auto* curveRow = new QHBoxLayout;
        curveRow->setSpacing(3);
        auto* curveLbl = new QLabel("Curve:", this);
        curveLbl->setStyleSheet("QLabel { color:#6b6258; font-size:8px; font-weight:700; }");

        m_stdBtn  = new QPushButton("STD",   this);
        m_sCrvBtn = new QPushButton("S-CRV", this);
        m_expBtn  = new QPushButton("EXP",   this);

        for (auto* b : {m_stdBtn, m_sCrvBtn, m_expBtn}) {
            b->setCheckable(true);
            b->setMaximumHeight(20);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setStyleSheet(btnQss());
        }
        m_stdBtn ->setToolTip("Standard linear crossfade");
        m_sCrvBtn->setToolTip("Equal-power S-Curve (recommended for music)");
        m_expBtn ->setToolTip("Exponential curve — sharp transition");
        m_sCrvBtn->setChecked(true);   // default

        connect(m_stdBtn,  &QPushButton::clicked, this, [this]() { onCurveSelected(0); });
        connect(m_sCrvBtn, &QPushButton::clicked, this, [this]() { onCurveSelected(1); });
        connect(m_expBtn,  &QPushButton::clicked, this, [this]() { onCurveSelected(2); });

        curveRow->addWidget(curveLbl);
        curveRow->addWidget(m_stdBtn,  1);
        curveRow->addWidget(m_sCrvBtn, 1);
        curveRow->addWidget(m_expBtn,  1);
        root->addLayout(curveRow);
    }

    // ── Fade duration ─────────────────────────────────────────────────────────
    {
        auto* durRow = new QHBoxLayout;
        durRow->setSpacing(4);
        auto* durLbl = new QLabel("Fade:", this);
        durLbl->setStyleSheet("QLabel { color:#6b6258; font-size:8px; font-weight:700; }");

        m_durSpin = new QDoubleSpinBox(this);
        m_durSpin->setRange(0.5, 30.0);
        m_durSpin->setSingleStep(0.5);
        m_durSpin->setValue(3.0);
        m_durSpin->setSuffix(" s");
        m_durSpin->setDecimals(1);
        m_durSpin->setMaximumHeight(22);
        m_durSpin->setToolTip("Crossfade duration in seconds (used by Auto-fade and AutoDJ)");
        m_durSpin->setStyleSheet(
            "QDoubleSpinBox {"
            "  background:#ece6de; color:#1a1814;"
            "  border:1px solid #a09080; border-radius:2px;"
            "  font-size:9px; padding:1px 3px;"
            "}"
        );

        durRow->addWidget(durLbl);
        durRow->addWidget(m_durSpin, 1);
        root->addLayout(durRow);
    }

    // ── Separator ─────────────────────────────────────────────────────────────
    {
        auto* sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("QFrame { color:#c0b8ae; }");
        root->addWidget(sep);
    }

    // ── Auto-fade buttons (AUTO SYNC + A→B + B→A) ────────────────────────────
    {
        auto* fadeRow = new QHBoxLayout;
        fadeRow->setSpacing(3);

        m_autoFadeBtn = new QPushButton("AUTO SYNC", this);
        m_autoFadeBtn->setCheckable(true);
        m_autoFadeBtn->setMaximumHeight(22);
        m_autoFadeBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_autoFadeBtn->setStyleSheet(btnQss("#d97706"));  // amber when checked/active
        m_autoFadeBtn->setToolTip(
            "AUTO SYNC FADE — detects the currently playing deck and fades to the opposite\n"
            "deck over the remaining track time. Starts immediately, ends when track ends.");
        connect(m_autoFadeBtn, &QPushButton::clicked,
                this, &CrossfaderWidget::onAutoFadeSynced);

        m_fadeABBtn = new QPushButton("A→B", this);
        m_fadeBABtn = new QPushButton("B→A", this);

        for (auto* b : {m_fadeABBtn, m_fadeBABtn}) {
            b->setMaximumHeight(22);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            b->setStyleSheet(btnQss("#1c5caa"));
        }
        m_fadeABBtn->setToolTip("Fade from Deck A to Deck B over configured fade duration");
        m_fadeBABtn->setToolTip("Fade from Deck B to Deck A over configured fade duration");

        connect(m_fadeABBtn, &QPushButton::clicked, this, &CrossfaderWidget::onAutoFadeAB);
        connect(m_fadeBABtn, &QPushButton::clicked, this, &CrossfaderWidget::onAutoFadeBA);

        fadeRow->addWidget(m_autoFadeBtn, 2);
        fadeRow->addWidget(m_fadeABBtn,   1);
        fadeRow->addWidget(m_fadeBABtn,   1);
        root->addLayout(fadeRow);
    }

    // ── Sync BPM button ───────────────────────────────────────────────────────
    {
        m_syncBtn = new QPushButton("A=B  Sync BPM", this);
        m_syncBtn->setMaximumHeight(22);
        m_syncBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_syncBtn->setStyleSheet(btnQss("#1a8a40"));
        m_syncBtn->setToolTip("Match Deck B BPM to Deck A (adjusts pitch)");
        connect(m_syncBtn, &QPushButton::clicked, this, &CrossfaderWidget::syncDecksRequested);
        root->addWidget(m_syncBtn);
    }

    // ── Crossfade Settings button ─────────────────────────────────────────────
    {
        auto* cfgBtn = new QPushButton("⚙  Crossfade Settings...", this);
        cfgBtn->setMaximumHeight(22);
        cfgBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        cfgBtn->setStyleSheet(btnQss("#5a7a9a"));
        cfgBtn->setToolTip("Open advanced crossfade curve settings (SAM-style)");
        connect(cfgBtn, &QPushButton::clicked, this, &CrossfaderWidget::openSettingsDialog);
        root->addWidget(cfgBtn);
    }

    // ── Animation timer ───────────────────────────────────────────────────────
    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(40);  // 25 fps
    connect(m_animTimer, &QTimer::timeout, this, &CrossfaderWidget::onAnimTick);
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void CrossfaderWidget::onCfSliderMoved(int pos) {
    emit crossfaderChanged((float)pos / 1000.0f);
}

void CrossfaderWidget::onCurveSelected(int mode) {
    m_curve = static_cast<CurveMode>(mode);
    m_stdBtn ->setChecked(mode == 0);
    m_sCrvBtn->setChecked(mode == 1);
    m_expBtn ->setChecked(mode == 2);
    emit curveModeChanged(m_curve);
}

void CrossfaderWidget::setDecks(M1::DeckPlayer* a, M1::DeckPlayer* b) {
    m_deckA = a;
    m_deckB = b;
}

void CrossfaderWidget::onAutoFadeSynced() {
    if (!m_deckA || !m_deckB) {
        if (m_autoFadeBtn) m_autoFadeBtn->setChecked(false);
        return;
    }

    const bool aPlaying = (m_deckA->state() == M1::DeckPlayer::State::Playing);
    const bool bPlaying = (m_deckB->state() == M1::DeckPlayer::State::Playing);

    float  target    = 0.5f;
    double remaining = (double)fadeDuration();  // fallback: configured fade duration

    if (aPlaying && !bPlaying) {
        target    = 1.0f;   // A is live — fade toward B
        remaining = m_deckA->durationSeconds() - m_deckA->positionSeconds();
    } else if (bPlaying && !aPlaying) {
        target    = 0.0f;   // B is live — fade toward A
        remaining = m_deckB->durationSeconds() - m_deckB->positionSeconds();
    } else {
        // Both or neither playing — use crossfader position to pick direction
        if (value() <= 0.5f) {
            target    = 1.0f;
            remaining = m_deckA->durationSeconds() - m_deckA->positionSeconds();
        } else {
            target    = 0.0f;
            remaining = m_deckB->durationSeconds() - m_deckB->positionSeconds();
        }
    }

    remaining = std::max(0.5, std::min(120.0, remaining));
    animateTo(target, static_cast<int>(remaining * 1000.0));
}

void CrossfaderWidget::onAutoFadeAB() {
    animateTo(1.0f, (int)(fadeDuration() * 1000.0));
}

void CrossfaderWidget::onAutoFadeBA() {
    animateTo(0.0f, (int)(fadeDuration() * 1000.0));
}

void CrossfaderWidget::animateTo(float target, int durationMs) {
    if (durationMs <= 0) { setValue(target); return; }
    m_animTarget = target;
    // Number of ticks at 40ms interval
    const int ticks = std::max(1, durationMs / 40);
    const float current = value();
    m_animStep = (target - current) / (float)ticks;
    if (!m_animTimer->isActive()) m_animTimer->start();
}

void CrossfaderWidget::onAnimTick() {
    const float current = value();
    const float delta   = m_animTarget - current;
    if (std::abs(delta) < 0.004f) {
        m_cfSlider->setValue((int)(m_animTarget * 1000.0f));
        m_animTimer->stop();
        if (m_autoFadeBtn) m_autoFadeBtn->setChecked(false);
        return;
    }
    // Use the pre-computed linear step (not exponential) for predictable fade timing
    float next = current + m_animStep;
    // Clamp to target if we'd overshoot
    if (m_animStep > 0) next = std::min(next, m_animTarget);
    else                next = std::max(next, m_animTarget);
    m_cfSlider->setValue((int)(next * 1000.0f));
}

// ─── Public API ───────────────────────────────────────────────────────────────
float CrossfaderWidget::value() const {
    return (float)m_cfSlider->value() / 1000.0f;
}

void CrossfaderWidget::setValue(float v) {
    m_cfSlider->setValue((int)(v * 1000.0f));
}

float CrossfaderWidget::fadeDuration() const {
    return (float)m_durSpin->value();
}

void CrossfaderWidget::setDeckABpm(float bpm) {
    if (m_aBpmLabel)
        m_aBpmLabel->setText(QString("A: %1").arg(bpm, 0, 'f', 1));
}

void CrossfaderWidget::setDeckBBpm(float bpm) {
    if (m_bBpmLabel)
        m_bBpmLabel->setText(QString("B: %1").arg(bpm, 0, 'f', 1));
}

// ─── Settings dialog ──────────────────────────────────────────────────────────
void CrossfaderWidget::openSettingsDialog() {
    CrossfaderSettingsDialog dlg(this);
    dlg.loadSettings();
    if (dlg.exec() != QDialog::Accepted) return;
    dlg.saveSettings();

    const CrossfadeSettings cfg = dlg.settings();

    // Sync fade duration from fade-out time
    m_durSpin->setValue((double)cfg.fadeOutTime);

    // Sync curve mode — map CrossfadeSettings::CurveType → CrossfaderWidget::CurveMode
    // Linear→Standard(0), SCurve→SCurve(1), Exponential→Exponential(2), Custom→SCurve(1)
    int curveIdx = 1; // default S-Curve
    switch (cfg.fadeOutCurve) {
        case CrossfadeSettings::Linear:      curveIdx = 0; break;
        case CrossfadeSettings::SCurve:      curveIdx = 1; break;
        case CrossfadeSettings::Exponential: curveIdx = 2; break;
        case CrossfadeSettings::Custom:      curveIdx = 1; break;
    }
    onCurveSelected(curveIdx);
}

// ─── paintEvent — sandy background ───────────────────────────────────────────
void CrossfaderWidget::paintEvent(QPaintEvent* e) {
    QPainter p(this);
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, kBgTop);
    bg.setColorAt(1.0, kBgBot);
    p.fillRect(rect(), bg);
    p.setPen(QPen(kDivider, 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    QWidget::paintEvent(e);
}
