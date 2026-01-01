#include "CompressorLimiter.h"
#include <cmath>
#include <algorithm>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDial>
#include <QLabel>
#include <QFrame>
#include <QCheckBox>
#include <QProgressBar>
#include <QTimer>
#include <QSettings>
#include <QSizePolicy>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace M1 {

// ─── Colour palette ──────────────────────────────────────────────────────────
static constexpr char kPanelBg[]  = "#0c1a2e";
static constexpr char kBorder[]   = "#1e3a5f";
static constexpr char kText[]     = "#e2e8f0";
static constexpr char kAccent[]   = "#0ea5e9";
static constexpr char kGreen[]    = "#22c55e";
static constexpr char kAmber[]    = "#f59e0b";
static constexpr char kRed[]      = "#ef4444";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

CompressorLimiter::CompressorLimiter()
{
    m_threshold.store(-18.0f,  std::memory_order_relaxed);
    m_ratio.store(4.0f,        std::memory_order_relaxed);
    m_attackMs.store(10.0f,    std::memory_order_relaxed);
    m_releaseMs.store(200.0f,  std::memory_order_relaxed);
    m_makeupGainDb.store(6.0f, std::memory_order_relaxed);
    m_kneeDb.store(3.0f,       std::memory_order_relaxed);
    m_limiterEnabled.store(true, std::memory_order_relaxed);
    m_attackCoeff.store(0.0,   std::memory_order_relaxed);
    m_releaseCoeff.store(0.0,  std::memory_order_relaxed);
    m_envDirty.store(true,     std::memory_order_relaxed);
    m_peakLeft.store(0.0f,     std::memory_order_relaxed);
    m_peakRight.store(0.0f,    std::memory_order_relaxed);
    m_gainReductionDb.store(0.0f, std::memory_order_relaxed);

    m_rmsAccum  = {};
    m_rmsCount  = {};
    m_rmsBuf    = {};
    m_rmsBufPos = {};
    m_rmsSumSq  = {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void CompressorLimiter::initialize(double sampleRate, int /*maxFramesPerBuffer*/)
{
    m_sampleRate = sampleRate;
    reset();
    recomputeEnvelopes();
}

void CompressorLimiter::reset()
{
    m_gainDb    = 0.0;
    m_rmsAccum  = {};
    m_rmsCount  = {};
    m_rmsBuf    = {};
    m_rmsBufPos = {};
    m_rmsSumSq  = {};
    m_peakLeft.store(0.0f,   std::memory_order_relaxed);
    m_peakRight.store(0.0f,  std::memory_order_relaxed);
    m_gainReductionDb.store(0.0f, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// recomputeEnvelopes — compute attack/release coefficients from ms values.
//
// One-pole IIR smoothing coefficient:
//   coeff = exp(-1.0 / (tau * Fs))  where tau = time_ms / 1000.0
// ─────────────────────────────────────────────────────────────────────────────
void CompressorLimiter::recomputeEnvelopes()
{
    const double fs = m_sampleRate > 0.0 ? m_sampleRate : 48000.0;
    const double atMs = static_cast<double>(m_attackMs.load(std::memory_order_relaxed));
    const double rlMs = static_cast<double>(m_releaseMs.load(std::memory_order_relaxed));

    const double attackTau  = atMs / 1000.0;
    const double releaseTau = rlMs / 1000.0;

    const double ac = (attackTau  > 0.0) ? std::exp(-1.0 / (attackTau  * fs)) : 0.0;
    const double rc = (releaseTau > 0.0) ? std::exp(-1.0 / (releaseTau * fs)) : 0.0;

    m_attackCoeff.store(ac,  std::memory_order_release);
    m_releaseCoeff.store(rc, std::memory_order_release);
    m_envDirty.store(false,  std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// process — RT-safe
//
// Per-sample processing:
//   1. RMS level detection using a sliding window ring buffer.
//   2. Convert RMS to dBFS.
//   3. Gain computer with soft knee:
//        if (x_dB < threshold - knee/2): gain = 0 dB
//        if (x_dB > threshold + knee/2): gain = (x_dB - threshold) * (1/ratio - 1)
//        else: interpolate in the knee region (smooth transition).
//   4. Apply attack/release envelope to gain.
//   5. Convert gain dB to linear, apply to sample.
//   6. Apply makeup gain.
//   7. Apply brickwall limiter if enabled.
// ─────────────────────────────────────────────────────────────────────────────
void CompressorLimiter::process(AudioBuffer& inOut)
{
    if (!inOut.isValid || !inOut.data)
        return;

    if (m_envDirty.load(std::memory_order_acquire)) {
        recomputeEnvelopes();
    }

    const int frames   = inOut.frames;
    const int channels = std::min(inOut.channels, kMaxChannels);

    const double thresh  = static_cast<double>(m_threshold.load(std::memory_order_relaxed));
    const double ratio   = static_cast<double>(m_ratio.load(std::memory_order_relaxed));
    const double kneeDb  = static_cast<double>(m_kneeDb.load(std::memory_order_relaxed));
    const double makeupLin = std::pow(10.0,
        static_cast<double>(m_makeupGainDb.load(std::memory_order_relaxed)) / 20.0);
    const bool   limEn   = m_limiterEnabled.load(std::memory_order_relaxed);

    const double attackCoeff  = m_attackCoeff.load(std::memory_order_acquire);
    const double releaseCoeff = m_releaseCoeff.load(std::memory_order_acquire);

    const double halfKnee = kneeDb * 0.5;
    const double invRatio = (ratio > 1.0) ? (1.0 / ratio) : 1.0;

    // Peak holders for metering.
    float peakL = 0.0f;
    float peakR = 0.0f;

    for (int f = 0; f < frames; ++f) {
        // ── Step 1: RMS level detection ──────────────────────────────────
        // Average RMS across channels to drive a single gain computer.
        double inputRmsSq = 0.0;
        for (int ch = 0; ch < channels; ++ch) {
            const float s = inOut.at(f, ch);
            const float sOld = m_rmsBuf[ch][m_rmsBufPos[ch]];
            m_rmsSumSq[ch] += static_cast<double>(s * s)
                            -  static_cast<double>(sOld * sOld);
            m_rmsBuf[ch][m_rmsBufPos[ch]] = s;
            m_rmsBufPos[ch] = (m_rmsBufPos[ch] + 1) % kRmsBufSize;
            const double rms = m_rmsSumSq[ch] / static_cast<double>(kRmsBufSize);
            inputRmsSq += std::max(rms, 0.0);
        }
        inputRmsSq /= static_cast<double>(channels);

        // ── Step 2: Convert to dBFS ────────────────────────────────────
        const double rmsLinear = std::sqrt(std::max(inputRmsSq, 1e-30));
        const double xDb = 20.0 * std::log10(rmsLinear + 1e-30);

        // ── Step 3: Gain computer (soft knee) ─────────────────────────
        double targetGainDb = 0.0;
        const double xOverThresh = xDb - thresh;

        if (xOverThresh < -halfKnee) {
            // Below knee: no gain reduction.
            targetGainDb = 0.0;
        } else if (xOverThresh > halfKnee) {
            // Above knee: full compression.
            targetGainDb = xOverThresh * (invRatio - 1.0);
        } else {
            // In the knee: quadratic interpolation.
            const double t = (xOverThresh + halfKnee) / kneeDb;
            targetGainDb = (invRatio - 1.0) * t * t * halfKnee;
        }

        // ── Step 4: Smooth gain (attack / release) ────────────────────
        if (targetGainDb < m_gainDb) {
            // Gain reduction increasing (attack).
            m_gainDb = attackCoeff * m_gainDb + (1.0 - attackCoeff) * targetGainDb;
        } else {
            // Gain reduction decreasing (release).
            m_gainDb = releaseCoeff * m_gainDb + (1.0 - releaseCoeff) * targetGainDb;
        }

        // ── Step 5-6: Apply gain + makeup ─────────────────────────────
        const double linearGain = std::pow(10.0, m_gainDb / 20.0) * makeupLin;

        for (int ch = 0; ch < channels; ++ch) {
            double s = static_cast<double>(inOut.at(f, ch)) * linearGain;

            // ── Step 7: Brickwall limiter ─────────────────────────────
            if (limEn) {
                if (s > kLimiterCeiling)
                    s = kLimiterCeiling;
                else if (s < -kLimiterCeiling)
                    s = -kLimiterCeiling;
            } else {
                s = std::clamp(s, -1.0, 1.0);
            }

            inOut.at(f, ch) = static_cast<float>(s);

            // Update peak meters.
            const float absS = std::abs(static_cast<float>(s));
            if (ch == 0 && absS > peakL) peakL = absS;
            if (ch == 1 && absS > peakR) peakR = absS;
        }
    }

    // Write metering atomics.
    m_peakLeft.store(peakL,  std::memory_order_relaxed);
    m_peakRight.store(peakR, std::memory_order_relaxed);
    m_gainReductionDb.store(static_cast<float>(-m_gainDb),
                            std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────────
// createPanel — modern layout with larger knobs and clearer metering
// ─────────────────────────────────────────────────────────────────────────────

QWidget* CompressorLimiter::createPanel(QWidget* parent)
{
    auto* panel = new QWidget(parent);
    panel->setStyleSheet(QString(
        "QWidget { background: %1; }"
        "QLabel  { color: %2; background: transparent; }"
    ).arg(kPanelBg).arg(kText));
    panel->setMinimumHeight(120);

    auto* mainH = new QHBoxLayout(panel);
    mainH->setContentsMargins(12, 8, 12, 8);
    mainH->setSpacing(6);

    // ── Helper: create a labelled knob ────────────────────────────────────
    auto makeKnob = [&](const QString& label, const QString& tooltip,
                         int minVal, int maxVal, int curVal)
        -> std::pair<QDial*, QLabel*>
    {
        auto* col    = new QWidget(panel);
        col->setStyleSheet("background: transparent;");
        auto* colV   = new QVBoxLayout(col);
        colV->setContentsMargins(0, 0, 0, 0);
        colV->setSpacing(3);

        // Parameter name
        auto* nameLbl = new QLabel(label, col);
        nameLbl->setAlignment(Qt::AlignHCenter);
        nameLbl->setStyleSheet(QString(
            "QLabel { color: %1; font: 9pt; background: transparent; }"
        ).arg(kText));

        // Rotary knob
        auto* dial = new QDial(col);
        dial->setRange(minVal, maxVal);
        dial->setValue(curVal);
        dial->setFixedSize(56, 56);
        dial->setNotchesVisible(true);
        dial->setWrapping(false);
        dial->setToolTip(tooltip);
        dial->setCursor(Qt::PointingHandCursor);
        dial->setStyleSheet(QString(
            "QDial { background: %1; border: 2px solid %2; border-radius: 28px; }"
        ).arg(kPanelBg).arg(kBorder));

        // Value readout
        auto* valLbl = new QLabel(QString::number(curVal), col);
        valLbl->setAlignment(Qt::AlignHCenter);
        valLbl->setStyleSheet(QString(
            "QLabel { color: %1; font: bold 9pt; background: transparent; }"
        ).arg(kAccent));

        colV->addWidget(nameLbl, 0, Qt::AlignHCenter);
        colV->addWidget(dial, 0, Qt::AlignHCenter);
        colV->addWidget(valLbl, 0, Qt::AlignHCenter);

        mainH->addWidget(col);
        return {dial, valLbl};
    };

    // ── Knobs ─────────────────────────────────────────────────────────────

    auto [threshDial, threshVal] = makeKnob("Threshold",
        "Compression threshold (-40 to 0 dBFS)",
        -40, 0,
        static_cast<int>(m_threshold.load(std::memory_order_relaxed)));

    auto [ratioDial, ratioVal] = makeKnob("Ratio",
        "Compression ratio (1:1 to 20:1)",
        1, 20,
        static_cast<int>(m_ratio.load(std::memory_order_relaxed)));

    auto [atkDial, atkVal] = makeKnob("Attack",
        "Attack time in ms (0.1 to 100 ms)",
        1, 1000,
        static_cast<int>(m_attackMs.load(std::memory_order_relaxed) * 10.0f));

    auto [relDial, relVal] = makeKnob("Release",
        "Release time in ms (50 to 2000 ms)",
        50, 2000,
        static_cast<int>(m_releaseMs.load(std::memory_order_relaxed)));

    auto [mkupDial, mkupVal] = makeKnob("Makeup",
        "Makeup gain (0 to +20 dB)",
        0, 20,
        static_cast<int>(m_makeupGainDb.load(std::memory_order_relaxed)));

    auto [kneeDial, kneeVal] = makeKnob("Knee",
        "Soft knee width (0 to 6 dB)",
        0, 6,
        static_cast<int>(m_kneeDb.load(std::memory_order_relaxed)));

    // ── Wire up knobs ─────────────────────────────────────────────────────

    QObject::connect(threshDial, &QDial::valueChanged, panel,
        [this, threshVal](int v) {
            setThreshold(static_cast<float>(v));
            threshVal->setText(QString("%1 dB").arg(v));
        });

    QObject::connect(ratioDial, &QDial::valueChanged, panel,
        [this, ratioVal](int v) {
            setRatio(static_cast<float>(v));
            ratioVal->setText(QString("%1:1").arg(v));
        });

    QObject::connect(atkDial, &QDial::valueChanged, panel,
        [this, atkVal](int v) {
            const float ms = v / 10.0f;
            setAttack(ms);
            atkVal->setText(QString("%1 ms").arg(ms, 0, 'f', 1));
        });

    QObject::connect(relDial, &QDial::valueChanged, panel,
        [this, relVal](int v) {
            setRelease(static_cast<float>(v));
            relVal->setText(QString("%1 ms").arg(v));
        });

    QObject::connect(mkupDial, &QDial::valueChanged, panel,
        [this, mkupVal](int v) {
            setMakeupGain(static_cast<float>(v));
            mkupVal->setText(QString("+%1 dB").arg(v));
        });

    QObject::connect(kneeDial, &QDial::valueChanged, panel,
        [this, kneeVal](int v) {
            setKnee(static_cast<float>(v));
            kneeVal->setText(QString("%1 dB").arg(v));
        });

    // ── Separator ─────────────────────────────────────────────────────────
    auto* sep = new QFrame(panel);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(2);
    sep->setStyleSheet(QString("QFrame { background: %1; border: none; }").arg(kBorder));
    mainH->addWidget(sep);

    // ── Limiter toggle ────────────────────────────────────────────────────
    auto* limiterCol = new QWidget(panel);
    limiterCol->setStyleSheet("background: transparent;");
    auto* limiterV = new QVBoxLayout(limiterCol);
    limiterV->setContentsMargins(0, 0, 0, 0);
    limiterV->setSpacing(4);

    auto* limLbl = new QLabel("Limiter", limiterCol);
    limLbl->setAlignment(Qt::AlignHCenter);
    limLbl->setStyleSheet(QString(
        "QLabel { color: %1; font: 9pt; background: transparent; }"
    ).arg(kText));

    auto* limCheck = new QCheckBox(limiterCol);
    limCheck->setChecked(m_limiterEnabled.load(std::memory_order_relaxed));
    limCheck->setToolTip("Enable brickwall limiter at -0.3 dBFS");
    limCheck->setCursor(Qt::PointingHandCursor);
    limCheck->setStyleSheet(QString(
        "QCheckBox::indicator { width: 22px; height: 22px; border: 2px solid %1; "
        "border-radius: 4px; background: %2; }"
        "QCheckBox::indicator:checked { background: %3; border-color: #0c7ec4; }"
    ).arg(kBorder).arg(kPanelBg).arg(kAccent));

    limiterV->addWidget(limLbl, 0, Qt::AlignHCenter);
    limiterV->addWidget(limCheck, 0, Qt::AlignHCenter);
    limiterV->addStretch();
    mainH->addWidget(limiterCol);

    QObject::connect(limCheck, &QCheckBox::toggled, panel,
        [this](bool checked) { setLimiterEnabled(checked); });

    // ── GR Meter ──────────────────────────────────────────────────────────
    auto* grCol = new QWidget(panel);
    grCol->setStyleSheet("background: transparent;");
    auto* grV = new QVBoxLayout(grCol);
    grV->setContentsMargins(0, 0, 0, 0);
    grV->setSpacing(3);

    auto* grLbl = new QLabel("GR", grCol);
    grLbl->setAlignment(Qt::AlignHCenter);
    grLbl->setStyleSheet(QString(
        "QLabel { color: %1; font: bold 9pt; background: transparent; }"
    ).arg(kText));

    auto* grBar = new QProgressBar(grCol);
    grBar->setOrientation(Qt::Vertical);
    grBar->setRange(0, 200);
    grBar->setValue(0);
    grBar->setTextVisible(false);
    grBar->setFixedWidth(22);
    grBar->setMinimumHeight(60);
    grBar->setToolTip("Gain reduction meter");
    grBar->setStyleSheet(QString(
        "QProgressBar { background: %1; border: 1px solid %2; border-radius: 3px; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        " stop:0 %3, stop:0.5 %4, stop:1 %5); border-radius: 2px; }"
    ).arg(kPanelBg).arg(kBorder).arg(kRed).arg(kAmber).arg(kGreen));

    auto* grValLbl = new QLabel("0 dB", grCol);
    grValLbl->setAlignment(Qt::AlignHCenter);
    grValLbl->setStyleSheet(QString(
        "QLabel { color: %1; font: bold 9pt; background: transparent; }"
    ).arg(kAccent));

    grV->addWidget(grLbl, 0, Qt::AlignHCenter);
    grV->addWidget(grBar, 1, Qt::AlignHCenter);
    grV->addWidget(grValLbl, 0, Qt::AlignHCenter);
    mainH->addWidget(grCol);

    // ── Meter timer (20 Hz refresh) ──────────────────────────────────────
    auto* meterTimer = new QTimer(panel);
    meterTimer->setInterval(50);
    QObject::connect(meterTimer, &QTimer::timeout, panel,
        [this, grBar, grValLbl]() {
            const float grDb = m_gainReductionDb.load(std::memory_order_relaxed);
            const int   grInt = static_cast<int>(std::clamp(grDb, 0.0f, 20.0f) * 10.0f);
            grBar->setValue(grInt);
            grValLbl->setText(QString("-%1").arg(
                static_cast<double>(grDb), 0, 'f', 1));
        });
    meterTimer->start();

    return panel;
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

void CompressorLimiter::saveState(QSettings& s)
{
    s.setValue("threshold",      m_threshold.load(std::memory_order_relaxed));
    s.setValue("ratio",          m_ratio.load(std::memory_order_relaxed));
    s.setValue("attackMs",       m_attackMs.load(std::memory_order_relaxed));
    s.setValue("releaseMs",      m_releaseMs.load(std::memory_order_relaxed));
    s.setValue("makeupGainDb",   m_makeupGainDb.load(std::memory_order_relaxed));
    s.setValue("kneeDb",         m_kneeDb.load(std::memory_order_relaxed));
    s.setValue("limiterEnabled", m_limiterEnabled.load(std::memory_order_relaxed));
}

void CompressorLimiter::loadState(QSettings& s)
{
    setThreshold(s.value("threshold",     -18.0f).toFloat());
    setRatio(s.value("ratio",              4.0f).toFloat());
    setAttack(s.value("attackMs",          10.0f).toFloat());
    setRelease(s.value("releaseMs",        200.0f).toFloat());
    setMakeupGain(s.value("makeupGainDb",  6.0f).toFloat());
    setKnee(s.value("kneeDb",             3.0f).toFloat());
    setLimiterEnabled(s.value("limiterEnabled", true).toBool());
}

// ─── Parameter setters ────────────────────────────────────────────────────────

void CompressorLimiter::setThreshold(float dBFS)
{
    m_threshold.store(std::clamp(dBFS, -40.0f, 0.0f), std::memory_order_relaxed);
}

void CompressorLimiter::setRatio(float ratio)
{
    m_ratio.store(std::clamp(ratio, 1.0f, 20.0f), std::memory_order_relaxed);
}

void CompressorLimiter::setAttack(float ms)
{
    m_attackMs.store(std::clamp(ms, 0.1f, 100.0f), std::memory_order_relaxed);
    m_envDirty.store(true, std::memory_order_release);
}

void CompressorLimiter::setRelease(float ms)
{
    m_releaseMs.store(std::clamp(ms, 50.0f, 2000.0f), std::memory_order_relaxed);
    m_envDirty.store(true, std::memory_order_release);
}

void CompressorLimiter::setMakeupGain(float dB)
{
    m_makeupGainDb.store(std::clamp(dB, 0.0f, 20.0f), std::memory_order_relaxed);
}

void CompressorLimiter::setKnee(float dB)
{
    m_kneeDb.store(std::clamp(dB, 0.001f, 6.0f), std::memory_order_relaxed);
}

void CompressorLimiter::setLimiterEnabled(bool en)
{
    m_limiterEnabled.store(en, std::memory_order_relaxed);
}

// ─── Getters ─────────────────────────────────────────────────────────────────

float CompressorLimiter::threshold()      const { return m_threshold.load(std::memory_order_relaxed); }
float CompressorLimiter::ratio()          const { return m_ratio.load(std::memory_order_relaxed); }
float CompressorLimiter::attackMs()       const { return m_attackMs.load(std::memory_order_relaxed); }
float CompressorLimiter::releaseMs()      const { return m_releaseMs.load(std::memory_order_relaxed); }
float CompressorLimiter::makeupGain()     const { return m_makeupGainDb.load(std::memory_order_relaxed); }
float CompressorLimiter::knee()           const { return m_kneeDb.load(std::memory_order_relaxed); }
bool  CompressorLimiter::limiterEnabled() const { return m_limiterEnabled.load(std::memory_order_relaxed); }
float CompressorLimiter::peakLeft()       const { return m_peakLeft.load(std::memory_order_relaxed); }
float CompressorLimiter::peakRight()      const { return m_peakRight.load(std::memory_order_relaxed); }
float CompressorLimiter::gainReductionDb() const { return m_gainReductionDb.load(std::memory_order_relaxed); }

} // namespace M1

// ─────────────────────────────────────────────────────────────────────────────
// C ABI plugin exports
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_compInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.fx.comp",
    "Compressor/Limiter",
    "1.0.0",
    "*",
    "effect",
    "Mcaster1",
    "Stereo RMS compressor with soft-knee gain computer and brickwall limiter"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_fx_comp_plugin_info()
{
    return &s_compInfo;
}

MCASTER1_PLUGIN_API M1::IEffectUnit* mcaster1_fx_comp_create(void* /*host*/)
{
    return new M1::CompressorLimiter();
}

MCASTER1_PLUGIN_API void mcaster1_fx_comp_destroy(M1::IEffectUnit* fx)
{
    delete fx;
}

} // extern "C"
