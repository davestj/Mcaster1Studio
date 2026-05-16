#include "SonicMaximizer.h"
#include <cmath>
#include <algorithm>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDial>
#include <QLabel>
#include <QFrame>
#include <QSettings>
#include "ThemePalette.h"
#include "ThemeManager.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace M1 {

// ─── Colour helper ───────────────────────────────────────────────────────────
static QString col(const QColor& c) { return c.name(); }

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

SonicMaximizer::SonicMaximizer()
{
    m_loContour.store(0.5f, std::memory_order_relaxed);
    m_process.store(0.5f, std::memory_order_relaxed);
    m_coeffsDirty.store(true, std::memory_order_relaxed);

    m_delayBuf  = {};
    m_delayPos  = {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void SonicMaximizer::initialize(double sampleRate, int /*maxFramesPerBuffer*/)
{
    m_sampleRate = sampleRate;
    reset();
    computeCoefficients();
}

void SonicMaximizer::reset()
{
    m_lowState  = {};
    m_highState = {};
    m_delayBuf  = {};
    m_delayPos  = {};
}

// ─────────────────────────────────────────────────────────────────────────────
// computeCoefficients — 1st-order shelving filters
//
// Low shelf at Fc=150 Hz (isolates bass for delay alignment).
// High shelf at Fc=2500 Hz (presence boost by m_process * 10 dB).
//
// 1st-order low-shelf (boost/cut):
//   Bilinear transform of s-domain shelf filter.
//   Reference: "Audio EQ Cookbook" (Zolzer, 1st-order forms).
//
//   K  = tan(pi * Fc / Fs)
//
//   Low-shelf cut (m_loContour controls depth, default ~flat):
//   We use the low-shelf to ISOLATE the low band by computing:
//     low_out = LowShelf(input)  [shelf gain > 1 = boost lows]
//     high_out = HighShelf(input)[shelf gain from m_process]
//     mid_out  = input - low_out_normalised - high_out_normalised (approx)
// ─────────────────────────────────────────────────────────────────────────────
void SonicMaximizer::computeCoefficients()
{
    const double fs = m_sampleRate > 0.0 ? m_sampleRate : 48000.0;

    // Lo contour: 0.0 = flat, 1.0 = max bass boost (~6 dB).
    const double loContour = static_cast<double>(m_loContour.load(std::memory_order_relaxed));
    const double processVal = static_cast<double>(m_process.load(std::memory_order_relaxed));

    // Low shelf at 150 Hz — cut below to identify low band, then delay.
    {
        const double Fc = 150.0;
        const double K  = std::tan(M_PI * Fc / fs);
        // 1st-order low-pass (used to extract the low band).
        // H_lp(z) = K / (K + 1) + K / (K + 1) * z^-1 - (K - 1)/(K + 1) * z^-1
        const double norm = 1.0 / (K + 1.0);
        m_lowShelf.b0   = K * norm;
        m_lowShelf.b1   = K * norm;
        m_lowShelf.a1   = -(1.0 - K) * norm;   // Note: stored as negative to add in filter
        // Lo-contour gain: 1.0 + loContour * 0.5 (up to +6 dB boost on lo band).
        m_lowShelf.gain = 1.0 + loContour * 0.5;
    }

    // High shelf at 2500 Hz — boost by process * 10 dB.
    {
        const double Fc  = 2500.0;
        const double K   = std::tan(M_PI * Fc / fs);
        // 1st-order high-pass.
        // H_hp(z) = 1/(K+1) - 1/(K+1) * z^-1 - (K-1)/(K+1) * z^-1
        const double norm = 1.0 / (K + 1.0);
        m_highShelf.b0   = 1.0 * norm;
        m_highShelf.b1   = -1.0 * norm;
        m_highShelf.a1   = -(1.0 - K) * norm;
        // Process gain: 0.0 = flat, 1.0 = +10 dB boost.
        const double dB  = processVal * 10.0;
        m_highShelf.gain = std::pow(10.0, dB / 20.0);
    }

    m_coeffsDirty.store(false, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// process — RT-safe, called from audio thread
//
// Algorithm per sample, per channel:
//   1. Low-pass filter → low_band (then delay 8 samples)
//   2. High-pass filter → high_band (with boost gain)
//   3. mid_band = input - low_sample_at_input_time - high_sample_at_input_time
//   4. mid_band *= 0.85 (slight attenuation to clean up midrange)
//   5. output = low_delayed * loGain + mid_band + high_band * highGain
// ─────────────────────────────────────────────────────────────────────────────
void SonicMaximizer::process(AudioBuffer& inOut)
{
    if (!inOut.isValid || !inOut.data)
        return;

    // Recompute coefficients if dirty (parameter changed from Qt thread).
    if (m_coeffsDirty.load(std::memory_order_acquire)) {
        computeCoefficients();
    }

    const int frames   = inOut.frames;
    const int channels = std::min(inOut.channels, kMaxChannels);

    const double b0l = m_lowShelf.b0;
    const double b1l = m_lowShelf.b1;
    const double a1l = m_lowShelf.a1;
    const double glo = m_lowShelf.gain;

    const double b0h = m_highShelf.b0;
    const double b1h = m_highShelf.b1;
    const double a1h = m_highShelf.a1;
    const double ghi = m_highShelf.gain;

    for (int ch = 0; ch < channels; ++ch) {
        double& z1l = m_lowState[ch].z1;
        double& z1h = m_highState[ch].z1;
        float*  del = m_delayBuf[ch].data();
        int&    pos = m_delayPos[ch];

        for (int f = 0; f < frames; ++f) {
            const double x = static_cast<double>(inOut.at(f, ch));

            // ── Low-pass (1st-order direct form I) ───────────────────────
            // y[n] = b0*x[n] + b1*x[n-1] - a1*y[n-1]
            // We store z1 as y[n-1]*(-a1) folded in for efficiency:
            const double low_y  = b0l * x + z1l;
            z1l = b1l * x - a1l * low_y;

            // ── 8-sample circular delay on low band ───────────────────────
            const float delayed_low = del[pos];
            del[pos] = static_cast<float>(low_y * glo);
            pos = (pos + 1) % kDelayLines;

            // ── High-pass (1st-order direct form I) ───────────────────────
            const double high_y = b0h * x + z1h;
            z1h = b1h * x - a1h * high_y;

            // ── Mid band = original - low - high (approximation) ──────────
            const double mid = (x - low_y - high_y) * 0.85;

            // ── Reconstruct output ────────────────────────────────────────
            const double out = static_cast<double>(delayed_low)
                             + mid
                             + high_y * ghi;

            inOut.at(f, ch) = static_cast<float>(std::clamp(out, -1.0, 1.0));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// createPanel — modern layout with larger knobs
// ─────────────────────────────────────────────────────────────────────────────

QWidget* SonicMaximizer::createPanel(QWidget* parent)
{
    auto* panel = new QWidget(parent);
    panel->setMinimumHeight(100);

    auto* hLayout = new QHBoxLayout(panel);
    hLayout->setContentsMargins(16, 10, 16, 10);
    hLayout->setSpacing(24);

    // Helper lambda: create a knob column with value display
    auto makeKnob = [&](const QString& labelText, const QString& tooltip,
                         int initVal, const QString& suffix)
        -> std::pair<QDial*, QLabel*>
    {
        auto* knobCol = new QWidget(panel);
        knobCol->setStyleSheet("background: transparent;");
        auto* vLayout = new QVBoxLayout(knobCol);
        vLayout->setContentsMargins(0, 0, 0, 0);
        vLayout->setSpacing(4);

        // Parameter name label (top)
        auto* lbl = new QLabel(labelText, knobCol);
        lbl->setObjectName("sonicKnobName");
        lbl->setAlignment(Qt::AlignHCenter);

        // Rotary knob
        auto* dial = new QDial(knobCol);
        dial->setObjectName("sonicDial");
        dial->setRange(0, 100);
        dial->setValue(initVal);
        dial->setFixedSize(64, 64);
        dial->setNotchesVisible(true);
        dial->setWrapping(false);
        dial->setToolTip(tooltip);
        dial->setCursor(Qt::PointingHandCursor);

        // Value readout
        auto* valLbl = new QLabel(QString("%1%2").arg(initVal).arg(suffix), knobCol);
        valLbl->setObjectName("sonicKnobValue");
        valLbl->setAlignment(Qt::AlignHCenter);

        vLayout->addWidget(lbl, 0, Qt::AlignHCenter);
        vLayout->addWidget(dial, 0, Qt::AlignHCenter);
        vLayout->addWidget(valLbl, 0, Qt::AlignHCenter);

        hLayout->addWidget(knobCol);
        return {dial, valLbl};
    };

    hLayout->addStretch(1);

    auto [loContourDial, loContourVal] = makeKnob(
        "Lo Contour",
        "Bass alignment contour (0-100%)",
        static_cast<int>(m_loContour.load(std::memory_order_relaxed) * 100.0f),
        "%");

    auto [processDial, processVal] = makeKnob(
        "Process",
        "Presence boost (0-100% = 0-10 dB)",
        static_cast<int>(m_process.load(std::memory_order_relaxed) * 100.0f),
        "%");

    hLayout->addStretch(1);

    // Wire up Lo Contour dial
    QObject::connect(loContourDial, &QDial::valueChanged, panel,
        [this, loContourVal](int v) {
            const float nv = v / 100.0f;
            m_loContour.store(nv, std::memory_order_relaxed);
            m_coeffsDirty.store(true, std::memory_order_release);
            loContourVal->setText(QString("%1%").arg(v));
        });

    // Wire up Process dial
    QObject::connect(processDial, &QDial::valueChanged, panel,
        [this, processVal](int v) {
            const float nv = v / 100.0f;
            m_process.store(nv, std::memory_order_relaxed);
            m_coeffsDirty.store(true, std::memory_order_release);
            processVal->setText(QString("%1%").arg(v));
        });

    // ── Apply theme styles ────────────────────────────────────────────────
    auto applyTheme = [panel]() {
        const auto p = ThemePalette::forCurrentTheme();
        panel->setStyleSheet(QString(
            "QWidget { background: %1; }"
            "QLabel  { color: %2; background: transparent; }"
            "QDial   { background: %1; }"
        ).arg(col(p.panelBg), col(p.text)));

        // Knob name labels
        for (auto* lbl : panel->findChildren<QLabel*>("sonicKnobName"))
            lbl->setStyleSheet(QString(
                "QLabel { color: %1; font: 9pt; background: transparent; }"
            ).arg(col(p.text)));

        // Dials
        for (auto* dial : panel->findChildren<QDial*>("sonicDial"))
            dial->setStyleSheet(QString(
                "QDial { background: %1; border: 2px solid %2; border-radius: 32px; }"
            ).arg(col(p.panelBg), col(p.border)));

        // Value labels
        for (auto* lbl : panel->findChildren<QLabel*>("sonicKnobValue"))
            lbl->setStyleSheet(QString(
                "QLabel { color: %1; font: bold 10pt; background: transparent; }"
            ).arg(col(p.accent)));
    };

    applyTheme();

    QObject::connect(ThemeManager::instance(), &ThemeManager::themeChanged,
                     panel, [applyTheme]() { applyTheme(); });

    return panel;
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

void SonicMaximizer::saveState(QSettings& s)
{
    s.setValue("loContour", m_loContour.load(std::memory_order_relaxed));
    s.setValue("process",   m_process.load(std::memory_order_relaxed));
}

void SonicMaximizer::loadState(QSettings& s)
{
    const float lo = s.value("loContour", 0.5f).toFloat();
    const float pr = s.value("process",   0.5f).toFloat();
    m_loContour.store(std::clamp(lo, 0.0f, 1.0f), std::memory_order_relaxed);
    m_process.store(std::clamp(pr, 0.0f, 1.0f),   std::memory_order_relaxed);
    m_coeffsDirty.store(true, std::memory_order_release);
}

void SonicMaximizer::setLoContour(float v)
{
    m_loContour.store(std::clamp(v, 0.0f, 1.0f), std::memory_order_relaxed);
    m_coeffsDirty.store(true, std::memory_order_release);
}

void SonicMaximizer::setProcess(float v)
{
    m_process.store(std::clamp(v, 0.0f, 1.0f), std::memory_order_relaxed);
    m_coeffsDirty.store(true, std::memory_order_release);
}

float SonicMaximizer::loContour() const
{
    return m_loContour.load(std::memory_order_relaxed);
}

float SonicMaximizer::process() const
{
    return m_process.load(std::memory_order_relaxed);
}

} // namespace M1

// ─────────────────────────────────────────────────────────────────────────────
// C ABI plugin exports
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_sonicInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.fx.sonic",
    "Sonic Maximizer",
    "1.0.0",
    "*",
    "effect",
    "Mcaster1",
    "BBE Sonic Maximizer clone — psychoacoustic bass and presence enhancer"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_fx_sonic_plugin_info()
{
    return &s_sonicInfo;
}

MCASTER1_PLUGIN_API M1::IEffectUnit* mcaster1_fx_sonic_create(void* /*host*/)
{
    return new M1::SonicMaximizer();
}

MCASTER1_PLUGIN_API void mcaster1_fx_sonic_destroy(M1::IEffectUnit* fx)
{
    delete fx;
}

} // extern "C"
