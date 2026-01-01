#pragma once
#include <QString>
#include <QWidget>
#include <QSettings>
#include "AudioBuffer.h"

namespace M1 {

/// Rack unit height in UI pixels (1U = 44px, matches standard 19" rack unit)
constexpr int RACK_UNIT_HEIGHT_PX = 44;

/// Base interface for virtual rack-mount DSP effect units.
/// Effects are loaded into EffectsRackModule slots.
///
/// The process() method is called from the audio engine callback thread —
/// MUST be real-time safe (no allocation, no Qt calls, no blocking).
class IEffectUnit {
public:
    virtual ~IEffectUnit() = default;

    // ------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------
    virtual QString effectId()    const = 0;  ///< "com.mcaster1.fx.eq31"
    virtual QString displayName() const = 0;  ///< "31-Band EQ"
    virtual QString vendor()      const { return "Mcaster1"; }
    virtual int     rackUnits()   const { return 1; }  ///< Height in U (1-4)

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------
    virtual void initialize(double sampleRate, int maxFramesPerBuffer) = 0;
    virtual void reset() = 0;

    // ------------------------------------------------------------------
    // DSP (real-time thread — no Qt, no allocation)
    // ------------------------------------------------------------------
    virtual void process(AudioBuffer& inOut) = 0;

    // ------------------------------------------------------------------
    // Bypass
    // ------------------------------------------------------------------
    virtual bool isBypassed()     const { return m_bypassed; }
    virtual void setBypassed(bool b)    { m_bypassed = b; }

    // ------------------------------------------------------------------
    // UI
    // ------------------------------------------------------------------
    virtual QWidget* createPanel(QWidget* parent) = 0;

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    virtual void saveState(QSettings& s) = 0;
    virtual void loadState(QSettings& s) = 0;

protected:
    bool m_bypassed = false;
};

} // namespace M1
