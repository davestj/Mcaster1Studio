#include "VUMeterModule.h"
#include "VUMeterWidget.h"
#include "ModuleEvents.h"
#include "IPlugin.h"
#include <QSettings>
#include <QDebug>

namespace M1 {

VUMeterModule::VUMeterModule(QObject* parent)
    : IModule(parent)
{}

VUMeterModule::~VUMeterModule() {
    shutdown();
}

void VUMeterModule::initialize() {
    // Subscribe to global audio level events from the EventBus
    connect(&EventBus::instance(), &EventBus::audioLevelsChanged,
            this, &VUMeterModule::onLevelsChanged,
            Qt::QueuedConnection);
    qInfo() << "[VUMeterModule] Initialized.";
}

void VUMeterModule::shutdown() {
    disconnect(&EventBus::instance(), nullptr, this, nullptr);
    qInfo() << "[VUMeterModule] Shutdown.";
}

QWidget* VUMeterModule::createWidget(QWidget* parent) {
    m_widget = new VUMeterWidget(parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    if (m_engine)
        m_widget->setDeviceName(m_engine->defaultInputDevice().name);
    return m_widget;
}

QWidget* VUMeterModule::createCompactWidget(QWidget* parent) {
    auto* w = new VUMeterWidget(parent);
    w->setCompact(true);
    if (m_engine)
        w->setDeviceName(m_engine->defaultInputDevice().name);
    connect(&EventBus::instance(), &EventBus::audioLevelsChanged,
            w, [w](float inL, float inR, float, float) { w->setLevels(inL, inR); },
            Qt::QueuedConnection);
    return w;
}

void VUMeterModule::setAudioEngine(IAudioEngine* engine) {
    m_engine = engine;
}

void VUMeterModule::onLevelsChanged(float inL, float inR, float /*outL*/, float /*outR*/) {
    if (m_widget)
        m_widget->setLevels(inL, inR);
}

void VUMeterModule::saveState(QSettings& s) {
    s.beginGroup("VUMeterModule");
    s.setValue("mode", "VU");
    s.endGroup();
}

void VUMeterModule::loadState(QSettings& s) {
    s.beginGroup("VUMeterModule");
    // Future: restore mode, scale preference, etc.
    s.endGroup();
}

} // namespace M1

// ─── Plugin C ABI exports ───────────────────────────────────────────────────
// Note: Mcaster1PluginInfo is in global namespace (C ABI struct).
//       IModule / IEffectUnit are in M1:: namespace.
static Mcaster1PluginInfo s_info = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.vumeter",
    "VU Meter",
    "1.0.0",
    "*",           // works on all surfaces
    "module",
    "Mcaster1",
    "Stereo VU/PPM level meter with peak hold"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_plugin_info() { return &s_info; }
MCASTER1_PLUGIN_API IModule* mcaster1_create_module(IModuleHost* /*host*/) {
    return new M1::VUMeterModule();
}
MCASTER1_PLUGIN_API void mcaster1_destroy_module(IModule* m) { delete m; }
MCASTER1_PLUGIN_API IEffectUnit* mcaster1_create_effect(void*) { return nullptr; }
MCASTER1_PLUGIN_API void mcaster1_destroy_effect(IEffectUnit*) {}
}
