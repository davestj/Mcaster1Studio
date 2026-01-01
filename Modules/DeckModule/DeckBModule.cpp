#include "DeckBModule.h"
#include "DeckPlayer.h"
#include "DeckWidget.h"
#include "IPlugin.h"
#include <QSettings>
#include <QDebug>
#include <algorithm>

namespace M1 {

DeckBModule::DeckBModule(QObject* parent)
    : IModule(parent)
    , m_player(new DeckPlayer(1, this))
{
    const int maxSamples = 8192 * 2;
    m_tmpBuf.resize(maxSamples, 0.0f);
    m_cueBuf.resize(maxSamples, 0.0f);
}

DeckBModule::~DeckBModule() {
    shutdown();
}

void DeckBModule::initialize() {
    auto publishMeta = [this]() {
        IcyMetadata meta;
        meta.streamTitle = m_player->loadedPath();
        meta.trackTitle  = m_player->loadedPath();
        emit metadataReady(meta);
    };
    connect(m_player, &DeckPlayer::loadingFinished, this, publishMeta);
    qInfo() << "[DeckBModule] Initialized.";
}

void DeckBModule::shutdown() {
    qInfo() << "[DeckBModule] Shutdown.";
}

QWidget* DeckBModule::createWidget(QWidget* parent) {
    m_panel = new ::DeckPanel(m_player, 1, parent);
    connect(m_panel, &QObject::destroyed, this, [this]() { m_panel = nullptr; });
    return m_panel;
}

void DeckBModule::onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& out) {
    if (!out.isValid || out.frames == 0) return;

    m_cueMixFrames = 0;
    const int n = out.frames * out.channels;
    if (n > (int)m_tmpBuf.size()) return;

    std::fill_n(m_tmpBuf.data(), n, 0.0f);
    std::fill_n(m_cueBuf.data(), n, 0.0f);

    const float cfGain = m_cfGain.load(std::memory_order_acquire);
    m_player->processBlock(m_tmpBuf.data(), out.frames, out.channels, cfGain);

    const bool air = m_player->airOn();
    const bool cue = m_player->cueOn();
    for (int i = 0; i < n; ++i) {
        if (air) out.data[i] += m_tmpBuf[i];
        if (cue) m_cueBuf[i] += m_tmpBuf[i];
    }
    m_cueMixFrames = cue ? out.frames : 0;
}

void DeckBModule::saveState(QSettings& s) {
    s.beginGroup("DeckBModule");
    s.setValue("lastTrack", m_player->loadedPath());
    s.endGroup();
}

void DeckBModule::loadState(QSettings& s) {
    s.beginGroup("DeckBModule");
    const QString path = s.value("lastTrack").toString();
    s.endGroup();
    if (!path.isEmpty()) m_player->loadFile(path);
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_deckBInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.deck.b",
    "Deck B",
    "1.0.0",
    "alpha,beta,dj,entertainment,custom",
    "module",
    "Mcaster1",
    "Standalone Deck B player — place independently on any surface"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_deckb_plugin_info() { return &s_deckBInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_deckb_create_module(IModuleHost*) {
    return new M1::DeckBModule();
}
MCASTER1_PLUGIN_API void mcaster1_deckb_destroy_module(IModule* m) { delete m; }
}
