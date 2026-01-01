#include "SampleEffect.h"
#include "IPlugin.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QSettings>
#include <QDebug>
#include <cmath>

namespace Sample {

SampleEffect::SampleEffect() = default;

void SampleEffect::initialize(double sampleRate, int /*maxFramesPerBuffer*/) {
    m_sampleRate = sampleRate;
    qInfo("[SampleEffect] initialize() — sr=%.0f", sampleRate);
}

void SampleEffect::reset() {
    // No filter state to reset for a simple gain
}

void SampleEffect::process(M1::AudioBuffer& inOut) {
    if (m_bypassed || !inOut.data) return;

    const float gain = m_gainLinear.load(std::memory_order_relaxed);
    const int total  = inOut.frames * inOut.channels;
    float* d = inOut.data;
    for (int i = 0; i < total; ++i)
        d[i] *= gain;
}

QWidget* SampleEffect::createPanel(QWidget* parent) {
    auto* w = new QWidget(parent);
    w->setObjectName("SampleEffectPanel");
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(8, 4, 8, 4);

    auto* label = new QLabel("Gain:", w);
    label->setFixedWidth(40);
    lay->addWidget(label);

    auto* slider = new QSlider(Qt::Horizontal, w);
    slider->setRange(-240, 120);  // -24.0 dB to +12.0 dB (×10)
    slider->setValue(0);
    slider->setToolTip("Gain (dB)");
    lay->addWidget(slider, 1);

    auto* dbLabel = new QLabel("0.0 dB", w);
    dbLabel->setFixedWidth(60);
    dbLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(dbLabel);

    QObject::connect(slider, &QSlider::valueChanged, w, [this, dbLabel](int val) {
        const float dB = val / 10.0f;
        m_gainLinear.store(std::pow(10.0f, dB / 20.0f), std::memory_order_relaxed);
        dbLabel->setText(QString("%1 dB").arg(dB, 0, 'f', 1));
    });

    return w;
}

void SampleEffect::saveState(QSettings& s) {
    s.beginGroup("SampleEffect");
    const float dB = 20.0f * std::log10(std::max(1e-6f,
                     m_gainLinear.load(std::memory_order_relaxed)));
    s.setValue("gainDb", dB);
    s.endGroup();
}

void SampleEffect::loadState(QSettings& s) {
    s.beginGroup("SampleEffect");
    const float dB = s.value("gainDb", 0.0f).toFloat();
    m_gainLinear.store(std::pow(10.0f, dB / 20.0f), std::memory_order_relaxed);
    s.endGroup();
}

} // namespace Sample

// ─── C ABI plugin exports ─────────────────────────────────────────────────────

static Mcaster1PluginInfo s_info = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.example.fx.gain",
    "Sample Gain",
    "1.0.0",
    "*",           // surface_hints — works on all surfaces
    "effect",      // plugin_type
    "Example",     // vendor
    "SDK example effect — simple gain control demonstrating IEffectUnit"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_plugin_info() {
    return &s_info;
}

MCASTER1_PLUGIN_API IEffectUnit* mcaster1_create_effect(void* /*host*/) {
    return new Sample::SampleEffect();
}

MCASTER1_PLUGIN_API void mcaster1_destroy_effect(IEffectUnit* e) {
    delete e;
}

} // extern "C"
