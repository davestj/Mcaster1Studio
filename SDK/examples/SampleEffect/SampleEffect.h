#pragma once
#include "IEffectUnit.h"
#include <QSlider>
#include <QLabel>
#include <atomic>

namespace Sample {

/// SampleEffect — Minimal Mcaster1Studio effect plugin example.
///
/// A simple gain/volume control DSP unit that demonstrates
/// the IEffectUnit C ABI plugin interface.
class SampleEffect : public M1::IEffectUnit {
public:
    SampleEffect();

    // Identity
    QString effectId()    const override { return "com.example.fx.gain"; }
    QString displayName() const override { return "Sample Gain"; }
    QString vendor()      const override { return "Example"; }
    int     rackUnits()   const override { return 1; }

    // Lifecycle
    void initialize(double sampleRate, int maxFramesPerBuffer) override;
    void reset() override;

    // DSP (RT-safe)
    void process(M1::AudioBuffer& inOut) override;

    // UI
    QWidget* createPanel(QWidget* parent) override;

    // State
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

private:
    std::atomic<float> m_gainLinear{1.0f};
    double m_sampleRate = 48000.0;
};

} // namespace Sample
