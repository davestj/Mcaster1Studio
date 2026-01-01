#include "Equalizer31Band.h"
#include <cmath>
#include <algorithm>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QSizePolicy>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace M1 {

// ─── Frequency label abbreviations for the 31 bands ─────────────────────────
static constexpr const char* kFreqLabels[Equalizer31Band::kBands] = {
    "20",  "25",  "31",  "40",  "50",
    "63",  "80",  "100", "125", "160",
    "200", "250", "315", "400", "500",
    "630", "800", "1k",  "1.2k","1.6k",
    "2k",  "2.5k","3.1k","4k",  "5k",
    "6.3k","8k",  "10k", "12k", "16k",
    "20k"
};

// ─── Colour palette ──────────────────────────────────────────────────────────
static constexpr char kPanelBg[] = "#0c1a2e";
static constexpr char kBorder[]  = "#1e3a5f";
static constexpr char kText[]    = "#e2e8f0";
static constexpr char kAccent[]  = "#0ea5e9";

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────

Equalizer31Band::Equalizer31Band()
{
    for (int i = 0; i < kBands; ++i) {
        m_gainDb[i].store(0.0f, std::memory_order_relaxed);
        m_dirty[i].store(true, std::memory_order_relaxed);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void Equalizer31Band::initialize(double sampleRate, int /*maxFramesPerBuffer*/)
{
    m_sampleRate = sampleRate;
    for (int b = 0; b < kBands; ++b) {
        m_dirty[b].store(true, std::memory_order_relaxed);
    }
    reset();
    for (int b = 0; b < kBands; ++b) {
        computeBandCoeffs(b);
    }
}

void Equalizer31Band::reset()
{
    for (int b = 0; b < kBands; ++b) {
        for (int ch = 0; ch < kMaxChannels; ++ch) {
            m_state[b][ch].w1 = 0.0;
            m_state[b][ch].w2 = 0.0;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// computeBandCoeffs — peaking EQ biquad (Audio EQ Cookbook)
//
// For a peaking EQ filter:
//   A  = sqrt(10^(dBgain/20)) = 10^(dBgain/40)
//   w0 = 2*pi*f0/Fs
//   alpha = sin(w0)/(2*Q)
//
//   b0 =   1 + alpha*A
//   b1 =  -2*cos(w0)
//   b2 =   1 - alpha*A
//   a0 =   1 + alpha/A
//   a1 =  -2*cos(w0)
//   a2 =   1 - alpha/A
//
// All coefficients normalised by a0.
// ─────────────────────────────────────────────────────────────────────────────
void Equalizer31Band::computeBandCoeffs(int band)
{
    const double dB = static_cast<double>(m_gainDb[band].load(std::memory_order_relaxed));
    const double f0 = kCenterFreqs[band];
    const double fs = m_sampleRate > 0.0 ? m_sampleRate : 48000.0;

    // Guard against Nyquist.
    if (f0 >= fs * 0.5) {
        m_coeffs[band] = {1.0, 0.0, 0.0, 0.0, 0.0};
        m_dirty[band].store(false, std::memory_order_release);
        return;
    }

    if (std::abs(dB) < 0.001) {
        // Unity gain — identity filter.
        m_coeffs[band] = {1.0, 0.0, 0.0, 0.0, 0.0};
        m_dirty[band].store(false, std::memory_order_release);
        return;
    }

    const double A     = std::pow(10.0, dB / 40.0);
    const double w0    = 2.0 * M_PI * f0 / fs;
    const double alpha = std::sin(w0) / (2.0 * kQ);

    const double a0 = 1.0 + alpha / A;
    const double inv_a0 = 1.0 / a0;

    m_coeffs[band].b0 = (1.0 + alpha * A)  * inv_a0;
    m_coeffs[band].b1 = (-2.0 * std::cos(w0)) * inv_a0;
    m_coeffs[band].b2 = (1.0 - alpha * A)  * inv_a0;
    m_coeffs[band].a1 = (-2.0 * std::cos(w0)) * inv_a0;
    m_coeffs[band].a2 = (1.0 - alpha / A)  * inv_a0;

    m_dirty[band].store(false, std::memory_order_release);
}

// ─────────────────────────────────────────────────────────────────────────────
// process — RT-safe
//
// Direct Form II Transposed biquad per sample per channel per band.
// ─────────────────────────────────────────────────────────────────────────────
void Equalizer31Band::process(AudioBuffer& inOut)
{
    if (!inOut.isValid || !inOut.data)
        return;

    // Recompute any dirty bands.
    for (int b = 0; b < kBands; ++b) {
        if (m_dirty[b].load(std::memory_order_acquire)) {
            computeBandCoeffs(b);
        }
    }

    const int frames   = inOut.frames;
    const int channels = std::min(inOut.channels, kMaxChannels);

    for (int f = 0; f < frames; ++f) {
        for (int ch = 0; ch < channels; ++ch) {
            double x = static_cast<double>(inOut.at(f, ch));

            for (int b = 0; b < kBands; ++b) {
                const auto& c = m_coeffs[b];
                auto& st = m_state[b][ch];

                // Direct Form II Transposed:
                // y  = b0*x + w1
                // w1 = b1*x - a1*y + w2
                // w2 = b2*x - a2*y
                const double y = c.b0 * x + st.w1;
                st.w1 = c.b1 * x - c.a1 * y + st.w2;
                st.w2 = c.b2 * x - c.a2 * y;
                x = y;
            }

            inOut.at(f, ch) = static_cast<float>(std::clamp(x, -1.0, 1.0));
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// createPanel — modern layout with taller sliders, larger labels, flat button
// ─────────────────────────────────────────────────────────────────────────────

QWidget* Equalizer31Band::createPanel(QWidget* parent)
{
    auto* panel = new QWidget(parent);
    panel->setStyleSheet(QString("QWidget { background: %1; }").arg(kPanelBg));
    panel->setMinimumHeight(160);

    auto* outerV = new QVBoxLayout(panel);
    outerV->setContentsMargins(8, 6, 8, 8);
    outerV->setSpacing(6);

    // ── Top row: +12 / 0 / -12 markers + Flat button ────────────────────
    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    // dB scale labels
    auto* scaleLbl = new QLabel("+12        0       -12 dB", panel);
    scaleLbl->setStyleSheet(QString(
        "QLabel { color: %1; font: 9pt; background: transparent; }"
    ).arg(kText));
    topRow->addWidget(scaleLbl);
    topRow->addStretch(1);

    // Flat button — resets all bands to 0
    auto* flatBtn = new QPushButton("Flat", panel);
    flatBtn->setFixedHeight(26);
    flatBtn->setCursor(Qt::PointingHandCursor);
    flatBtn->setToolTip("Reset all bands to 0 dB (flat response)");
    flatBtn->setStyleSheet(QString(
        "QPushButton { background: %1; color: %2; font: bold 9pt; "
        "border: 1px solid %3; border-radius: 4px; padding: 2px 12px; }"
        "QPushButton:hover { background: %3; color: white; }"
    ).arg(kPanelBg).arg(kAccent).arg(kBorder));
    topRow->addWidget(flatBtn);

    outerV->addLayout(topRow);

    // ── Scroll area for the band sliders ────────────────────────────────
    auto* scroll = new QScrollArea(panel);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString(
        "QScrollArea { border: none; background: %1; }"
        "QScrollBar:horizontal { background: %2; height: 8px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal { background: %3; border-radius: 3px; min-width: 30px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    ).arg(kPanelBg).arg(kBorder).arg(kAccent));

    auto* sliderContainer = new QWidget();
    sliderContainer->setStyleSheet(QString("background: %1;").arg(kPanelBg));
    auto* hLayout = new QHBoxLayout(sliderContainer);
    hLayout->setContentsMargins(4, 2, 4, 2);
    hLayout->setSpacing(1);

    // Store slider pointers for flat button
    QList<QSlider*> allSliders;

    for (int b = 0; b < kBands; ++b) {
        auto* col = new QWidget(sliderContainer);
        col->setStyleSheet("background: transparent;");
        auto* colV = new QVBoxLayout(col);
        colV->setContentsMargins(0, 0, 0, 0);
        colV->setSpacing(2);

        // dB value label (top)
        const int curGain = static_cast<int>(bandGain(b));
        auto* dbLabel = new QLabel(curGain == 0 ? "0" :
            (curGain > 0 ? QString("+%1").arg(curGain) : QString::number(curGain)), col);
        dbLabel->setAlignment(Qt::AlignHCenter);
        dbLabel->setFixedWidth(32);
        dbLabel->setStyleSheet(QString(
            "QLabel { color: %1; font: bold 8pt; background: transparent; }"
        ).arg(kAccent));

        // Vertical slider: -12 to +12
        auto* slider = new QSlider(Qt::Vertical, col);
        slider->setRange(-12, 12);
        slider->setValue(curGain);
        slider->setSingleStep(1);
        slider->setPageStep(3);
        slider->setTickPosition(QSlider::NoTicks);
        slider->setFixedWidth(32);
        slider->setMinimumHeight(100);
        slider->setCursor(Qt::PointingHandCursor);
        slider->setToolTip(QString("%1 Hz").arg(kFreqLabels[b]));
        slider->setStyleSheet(QString(
            "QSlider::groove:vertical { background: %1; width: 4px; border-radius: 2px; }"
            "QSlider::handle:vertical { background: %2; height: 14px; width: 20px; "
            "margin: -8px; border-radius: 3px; border: 1px solid %3; }"
            "QSlider::sub-page:vertical { background: %1; border-radius: 2px; }"
            "QSlider::add-page:vertical { background: %4; border-radius: 2px; }"
        ).arg(kBorder).arg(kAccent).arg("#0c7ec4").arg(kBorder));

        allSliders.append(slider);

        // Frequency label (bottom)
        auto* freqLabel = new QLabel(kFreqLabels[b], col);
        freqLabel->setAlignment(Qt::AlignHCenter);
        freqLabel->setFixedWidth(32);
        freqLabel->setStyleSheet(QString(
            "QLabel { color: %1; font: 7pt; background: transparent; }"
        ).arg(kText));

        colV->addWidget(dbLabel, 0, Qt::AlignHCenter);
        colV->addWidget(slider, 1, Qt::AlignHCenter);
        colV->addWidget(freqLabel, 0, Qt::AlignHCenter);

        hLayout->addWidget(col);

        // Wire up slider
        const int capturedBand = b;
        QObject::connect(slider, &QSlider::valueChanged, panel,
            [this, capturedBand, dbLabel](int v) {
                setBandGain(capturedBand, static_cast<float>(v));
                dbLabel->setText(v == 0 ? "0" :
                    (v > 0 ? QString("+%1").arg(v) : QString::number(v)));
            });
    }

    // Wire flat button to reset all sliders
    QObject::connect(flatBtn, &QPushButton::clicked, panel,
        [this, allSliders]() {
            for (int b = 0; b < kBands; ++b)
                setBandGain(b, 0.0f);
            for (auto* s : allSliders)
                s->setValue(0);
        });

    scroll->setWidget(sliderContainer);
    outerV->addWidget(scroll, 1);

    return panel;
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────

void Equalizer31Band::saveState(QSettings& s)
{
    for (int b = 0; b < kBands; ++b) {
        s.setValue(QString("band_%1_gain").arg(b),
                   m_gainDb[b].load(std::memory_order_relaxed));
    }
}

void Equalizer31Band::loadState(QSettings& s)
{
    for (int b = 0; b < kBands; ++b) {
        const float g = s.value(QString("band_%1_gain").arg(b), 0.0f).toFloat();
        setBandGain(b, g);
    }
}

void Equalizer31Band::setBandGain(int band, float dB)
{
    if (band < 0 || band >= kBands)
        return;
    const float clamped = std::clamp(dB, -12.0f, 12.0f);
    m_gainDb[band].store(clamped, std::memory_order_relaxed);
    m_dirty[band].store(true, std::memory_order_release);
}

float Equalizer31Band::bandGain(int band) const
{
    if (band < 0 || band >= kBands)
        return 0.0f;
    return m_gainDb[band].load(std::memory_order_relaxed);
}

} // namespace M1

// ─────────────────────────────────────────────────────────────────────────────
// C ABI plugin exports
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_eq31Info = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.fx.eq31",
    "31-Band EQ",
    "1.0.0",
    "*",
    "effect",
    "Mcaster1",
    "ISO 31-band graphic equalizer with biquad peak/notch filters, Q=4.0"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_fx_eq31_plugin_info()
{
    return &s_eq31Info;
}

MCASTER1_PLUGIN_API M1::IEffectUnit* mcaster1_fx_eq31_create(void* /*host*/)
{
    return new M1::Equalizer31Band();
}

MCASTER1_PLUGIN_API void mcaster1_fx_eq31_destroy(M1::IEffectUnit* fx)
{
    delete fx;
}

} // extern "C"
