#include "SampleModule.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QDebug>

namespace Sample {

SampleModule::SampleModule(QObject* parent)
    : M1::IModule(parent)
{
}

QWidget* SampleModule::createWidget(QWidget* parent) {
    auto* w = new QWidget(parent);
    auto* layout = new QVBoxLayout(w);

    m_label = new QLabel("Hello from SDK!\nThis is SampleModule.\n\n"
                         "ID: com.example.sample", w);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setObjectName("SampleModuleLabel");
    layout->addWidget(m_label, 1, Qt::AlignCenter);

    return w;
}

void SampleModule::initialize() {
    qInfo("[SampleModule] initialize() — SDK example plugin running.");
}

void SampleModule::shutdown() {
    qInfo("[SampleModule] shutdown()");
}

void SampleModule::saveState(QSettings& s) {
    s.beginGroup("SampleModule");
    s.endGroup();
}

void SampleModule::loadState(QSettings& s) {
    s.beginGroup("SampleModule");
    s.endGroup();
}

} // namespace Sample

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
//
// These three functions are the only symbols that Mcaster1Studio looks for
// when loading a plugin DLL. Keep them in extern "C" to prevent name mangling.
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_info = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.example.sample",
    "Sample Module",
    "1.0.0",
    "*",           // surface_hints — works on all surface types
    "module",      // plugin_type
    "Example",     // vendor
    "SDK example module — minimal Mcaster1Studio plugin starter"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_plugin_info() {
    return &s_info;
}

MCASTER1_PLUGIN_API IModule* mcaster1_create_module(IModuleHost* /*host*/) {
    return new Sample::SampleModule();
}

MCASTER1_PLUGIN_API void mcaster1_destroy_module(IModule* m) {
    delete m;
}

} // extern "C"
