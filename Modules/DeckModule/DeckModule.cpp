#include "DeckModule.h"
#include "DeckPlayer.h"
#include "DeckWidget.h"    // DeckWidget is global namespace
#include "CrossfaderWidget.h"
#include "CrossfaderSettingsDialog.h"
#include "ModuleEvents.h"
#include "IPlugin.h"
#include <QSettings>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace M1 {

DeckModule::DeckModule(QObject* parent)
    : IModule(parent)
    , m_deckA(new DeckPlayer(0, this))
    , m_deckB(new DeckPlayer(1, this))
{
    // Pre-allocate temp buffers for RT audio (avoid alloc in callback)
    const int maxSamples = 8192 * 2; // 8192 frames × 2 channels
    m_tmpA.resize(maxSamples, 0.0f);
    m_tmpB.resize(maxSamples, 0.0f);
    m_cueBuf.resize(maxSamples, 0.0f);
}

DeckModule::~DeckModule() {
    shutdown();
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────
void DeckModule::initialize() {
    // Publish now-playing metadata when Deck A or B loads a track
    auto publishMeta = [this](int deck) {
        auto* player = (deck == 0) ? m_deckA : m_deckB;
        IcyMetadata meta;
        meta.streamTitle = player->loadedPath();      // basic title
        meta.trackTitle  = player->loadedPath();
        emit metadataReady(meta);
    };

    connect(m_deckA, &DeckPlayer::loadingFinished, this, [this, publishMeta]() { publishMeta(0); });
    connect(m_deckB, &DeckPlayer::loadingFinished, this, [this, publishMeta]() { publishMeta(1); });

    qInfo() << "[DeckModule] Initialized.";
}

void DeckModule::shutdown() {
    qInfo() << "[DeckModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────
QWidget* DeckModule::createWidget(QWidget* parent) {
    m_widget = new ::DeckWidget(m_deckA, m_deckB, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });

    // Sync crossfader value to the atomic used in onAudioBlock
    connect(m_widget->crossfader(), &CrossfaderWidget::crossfaderChanged,
            this, [this](float v) {
        m_crossfader.store(v, std::memory_order_release);
    });

    // Sync curve mode
    connect(m_widget->crossfader(), &CrossfaderWidget::curveModeChanged,
            this, [this](CrossfaderWidget::CurveMode m) {
        m_curveMode.store((int)m, std::memory_order_relaxed);
    });

    return m_widget;
}

// ─── RT audio mixing ─────────────────────────────────────────────────────────
void DeckModule::onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& out) {
    if (!out.isValid || out.frames == 0) return;

    out.silence();
    m_cueMixFrames = 0;

    const int n = out.frames * out.channels;
    if (n > (int)m_tmpA.size()) return; // safety: buffer too small (should never happen)

    const float cf   = m_crossfader.load(std::memory_order_acquire);
    const int   mode = m_curveMode.load(std::memory_order_relaxed);

    float gainA, gainB;
    switch (mode) {
    case 1:  // S-Curve — equal-power cosine (no volume dip at centre)
        gainA = std::cos(cf * (float)M_PI_2);
        gainB = std::cos((1.0f - cf) * (float)M_PI_2);
        break;
    case 2:  // Exponential — sharp transition
        gainA = (1.0f - cf) * (1.0f - cf);
        gainB = cf * cf;
        break;
    default: // Standard — linear
        gainA = 1.0f - cf;
        gainB = cf;
        break;
    }

    // Generate each deck into its own temp buffer
    std::fill_n(m_tmpA.data(), n, 0.0f);
    std::fill_n(m_tmpB.data(), n, 0.0f);
    std::fill_n(m_cueBuf.data(), n, 0.0f);

    if (gainA > 0.0f)
        m_deckA->processBlock(m_tmpA.data(), out.frames, out.channels, gainA);
    if (gainB > 0.0f)
        m_deckB->processBlock(m_tmpB.data(), out.frames, out.channels, gainB);

    // Route to AIR and/or CUE buses
    const bool airA = m_deckA->airOn(), airB = m_deckB->airOn();
    const bool cueA = m_deckA->cueOn(), cueB = m_deckB->cueOn();

    for (int i = 0; i < n; ++i) {
        if (airA) out.data[i] += m_tmpA[i];
        if (airB) out.data[i] += m_tmpB[i];
        if (cueA) m_cueBuf[i] += m_tmpA[i];
        if (cueB) m_cueBuf[i] += m_tmpB[i];
    }
    m_cueMixFrames = (cueA || cueB) ? out.frames : 0;

    // Soft clip master output to avoid inter-module clipping
    for (int i = 0; i < n; ++i) {
        float s = out.data[i];
        if      (s >  1.0f) out.data[i] =  1.0f;
        else if (s < -1.0f) out.data[i] = -1.0f;
    }
}

// ─── AutoDJ crossfader animation (called from QueueModule) ───────────────────
void DeckModule::animateCrossfaderTo(float target, int durationMs) {
    if (m_widget && m_widget->crossfader())
        m_widget->crossfader()->animateTo(target, durationMs);
}

// ─── Crossfade Settings dialog (standalone or via widget) ─────────────────────
void DeckModule::openCrossfadeSettings() {
    if (m_widget && m_widget->crossfader()) {
        // Delegate to the crossfader widget — it syncs duration + curve on accept
        m_widget->crossfader()->openSettingsDialog();
    } else {
        // No widget yet — show standalone dialog, save to QSettings for AutoDJ
        CrossfaderSettingsDialog dlg;
        dlg.loadSettings();
        if (dlg.exec() == QDialog::Accepted)
            dlg.saveSettings();
    }
}

// ─── State persistence ────────────────────────────────────────────────────────
void DeckModule::saveState(QSettings& s) {
    s.beginGroup("DeckModule");
    s.setValue("lastTrackA", m_deckA->loadedPath());
    s.setValue("lastTrackB", m_deckB->loadedPath());
    s.setValue("crossfader",  m_crossfader.load());
    s.endGroup();
}

void DeckModule::loadState(QSettings& s) {
    s.beginGroup("DeckModule");
    const QString pathA = s.value("lastTrackA").toString();
    const QString pathB = s.value("lastTrackB").toString();
    const float   cf    = s.value("crossfader", 0.5f).toFloat();
    s.endGroup();

    if (!pathA.isEmpty()) m_deckA->loadFile(pathA);
    if (!pathB.isEmpty()) m_deckB->loadFile(pathB);
    m_crossfader.store(cf);
    if (m_widget) m_widget->crossfader()->setValue(cf);
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_deckInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.deck",
    "Deck Player",
    "1.0.0",
    "alpha,beta,dj,entertainment",
    "module",
    "Mcaster1",
    "Dual deck player with waveform, crossfader, and cue/loop controls"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_deck_plugin_info() { return &s_deckInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_deck_create_module(IModuleHost*) {
    return new M1::DeckModule();
}
MCASTER1_PLUGIN_API void mcaster1_deck_destroy_module(IModule* m) { delete m; }
}
