#include "EffectsRackModule.h"
#include "EffectsRackWidget.h"
#include "AudioBuffer.h"
#include "SonicMaximizer.h"
#include "Equalizer31Band.h"
#include "CompressorLimiter.h"
#include <cstring>

namespace M1 {

// ─────────────────────────────────────────────────────────────────────────────
// EffectsRackModule
// ─────────────────────────────────────────────────────────────────────────────

EffectsRackModule::EffectsRackModule(QObject* parent)
    : IModule(parent)
{
}

EffectsRackModule::~EffectsRackModule()
{
    for (auto* u : m_units) delete u;
    m_units.clear();
}

void EffectsRackModule::initialize()
{
    // Allocate scratch buffer sized for a typical max block (2ch x 4096 frames).
    // This is the only allocation that touches non-RT paths.
    m_scratchData.assign(2 * 4096, 0.0f);

    // Pre-populate 3 built-in effects if the rack is empty (no saved state loaded).
    if (m_units.isEmpty()) {
        auto* sonic = new SonicMaximizer();
        auto* eq    = new Equalizer31Band();
        auto* comp  = new CompressorLimiter();
        sonic->initialize(48000.0, 512);
        eq->initialize(48000.0, 512);
        comp->initialize(48000.0, 512);
        m_units.append(sonic);
        m_units.append(eq);
        m_units.append(comp);
    }
}

void EffectsRackModule::shutdown()
{
    m_scratchData.clear();
    m_scratchData.shrink_to_fit();
}

QWidget* EffectsRackModule::createWidget(QWidget* parent)
{
    m_widget = new EffectsRackWidget(this, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    // Sync widget display to any already-loaded units (e.g., after loadState).
    // refresh() rebuilds the slot UI without mutating m_units.
    m_widget->refresh();
    return m_widget;
}

// ─── RT-thread audio processing ───────────────────────────────────────────────
//
// Chain: in -> copy to out -> unit[0](out) -> unit[1](out) -> ... -> out
//
// All active (non-bypassed) units process AudioBuffer& inOut in-place.
// We copy 'in' to 'out' first so that if there are no units the behaviour is
// pass-through (consistent with the IModule default).
//
// SAFETY: m_units is not modified from this thread — modifications happen on
// the Qt main thread and must be avoided while RT thread is running. A proper
// production system would use a lock-free ring here; for Phase 5 we document
// the constraint that units must not be added/removed during live audio.
// ─────────────────────────────────────────────────────────────────────────────
void EffectsRackModule::onAudioBlock(AudioBuffer& in, AudioBuffer& out)
{
    (void)in;  // Effects rack processes the accumulated mix, not raw device input

    if (!out.isValid)
        return;

    // Process 'out' in-place — it already contains the accumulated mix from
    // previous modules in the chain (deck → PTT → cartwall → effects).
    for (IEffectUnit* unit : m_units) {
        if (!unit || unit->isBypassed())
            continue;
        unit->process(out);
    }
}

// ─── Rack management (Qt thread only) ────────────────────────────────────────

void EffectsRackModule::addUnit(IEffectUnit* unit)
{
    if (!unit || m_units.size() >= kMaxSlots)
        return;
    m_units.append(unit);
}

void EffectsRackModule::removeUnit(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= m_units.size())
        return;
    delete m_units.takeAt(slotIndex);
}

int EffectsRackModule::unitCount() const
{
    return m_units.size();
}

IEffectUnit* EffectsRackModule::unit(int index) const
{
    if (index < 0 || index >= m_units.size())
        return nullptr;
    return m_units[index];
}

// ─── State persistence ────────────────────────────────────────────────────────

void EffectsRackModule::saveState(QSettings& s)
{
    s.beginGroup("EffectsRack");
    s.setValue("unitCount", m_units.size());
    for (int i = 0; i < m_units.size(); ++i) {
        s.beginGroup(QString("unit_%1").arg(i));
        s.setValue("effectId", m_units[i]->effectId());
        s.setValue("bypassed", m_units[i]->isBypassed());
        m_units[i]->saveState(s);
        s.endGroup();
    }
    s.endGroup();
}

void EffectsRackModule::loadState(QSettings& s)
{
    s.beginGroup("EffectsRack");
    const int count = s.value("unitCount", 0).toInt();
    for (int i = 0; i < count && i < m_units.size(); ++i) {
        s.beginGroup(QString("unit_%1").arg(i));
        const bool bypassed = s.value("bypassed", false).toBool();
        m_units[i]->setBypassed(bypassed);
        m_units[i]->loadState(s);
        s.endGroup();
    }
    s.endGroup();
}

} // namespace M1

// ─────────────────────────────────────────────────────────────────────────────
// C ABI plugin exports
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_effectsInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.effects",
    "Effects Rack",
    "1.0.0",
    "*",
    "module",
    "Mcaster1",
    "Virtual rack-mount DSP effects chain with up to 8 slots"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_effects_plugin_info()
{
    return &s_effectsInfo;
}

MCASTER1_PLUGIN_API M1::IModule* mcaster1_effects_create(IModuleHost* /*host*/)
{
    return new M1::EffectsRackModule();
}

MCASTER1_PLUGIN_API void mcaster1_effects_destroy(M1::IModule* mod)
{
    delete mod;
}

} // extern "C"
