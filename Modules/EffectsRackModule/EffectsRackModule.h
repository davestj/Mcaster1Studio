#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include "IEffectUnit.h"
#include <QList>

class EffectsRackWidget;

namespace M1 {

/// EffectsRackModule — Phase 5 virtual effects rack.
///
/// Manages up to kMaxSlots IEffectUnit slots arranged as a rack-mount stack.
/// Effect units are processed in slot order on the RT audio thread.
/// Bypassed units are skipped.
///
/// onAudioBlock() chains units: in -> unit[0] -> unit[1] -> ... -> out.
/// The first active unit receives data from 'in'; subsequent units process
/// the output of the previous unit in-place (AudioBuffer 'out' is reused).
class EffectsRackModule : public IModule {
    Q_OBJECT

public:
    static constexpr int kMaxSlots = 8;

    explicit EffectsRackModule(QObject* parent = nullptr);
    ~EffectsRackModule() override;

    // ── IModule ──────────────────────────────────────────────────────────
    QString  moduleId()      const override { return "com.mcaster1.effects"; }
    QString  displayName()   const override { return "Effects Rack"; }
    QSize    preferredSize() const override { return {400, 300}; }

    QWidget* createWidget(QWidget* parent) override;

    void initialize() override;
    void shutdown()   override;

    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Rack management (Qt thread only) ─────────────────────────────────
    void addUnit(IEffectUnit* unit);
    void removeUnit(int slotIndex);
    int  unitCount() const;
    IEffectUnit* unit(int index) const;

private:
    QList<IEffectUnit*> m_units;
    EffectsRackWidget*  m_widget = nullptr;

    // Scratch buffer owned (non-RT); used only when we need a second copy.
    // Allocated once in initialize() — never in onAudioBlock().
    std::vector<float>  m_scratchData;
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_effects_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_effects_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_effects_destroy(M1::IModule*);
}
