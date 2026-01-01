#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <vector>
#include <atomic>

class DeckPanel;
namespace M1 { class DeckPlayer; }

namespace M1 {

/// DeckAModule — standalone Deck A player.
///
/// Independent from Deck B; can be placed anywhere on any surface.
/// Audio is accumulated into the output buffer (no silence — the
/// wireModuleConnections callback handles silencing before deck modules).
class DeckAModule : public IModule {
    Q_OBJECT

public:
    explicit DeckAModule(QObject* parent = nullptr);
    ~DeckAModule() override;

    QString moduleId()    const override { return "com.mcaster1.deck.a"; }
    QString displayName() const override { return "Deck A"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {440, 340}; }
    QSize minimumModuleSize() const override { return {320, 280}; }

    void initialize() override;
    void shutdown()   override;

    QWidget* createWidget(QWidget* parent) override;

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Audio — accumulate deck A into out (no silence; caller silences first)
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;

    DeckPlayer* player() const { return m_player; }
    ::DeckPanel* panel() const { return m_panel; }

    const float* cueMix() const { return m_cueBuf.data(); }
    int cueMixFrames() const { return m_cueMixFrames; }

    /// Crossfader gain applied in onAudioBlock (0.0–1.0). Set by CrossfaderModule.
    void setCrossfaderGain(float g) { m_cfGain.store(std::clamp(g, 0.0f, 1.0f), std::memory_order_release); }
    float crossfaderGain() const { return m_cfGain.load(std::memory_order_relaxed); }

private:
    DeckPlayer*        m_player = nullptr;
    ::DeckPanel*       m_panel  = nullptr;
    std::vector<float> m_tmpBuf;
    std::vector<float> m_cueBuf;
    int                m_cueMixFrames = 0;
    std::atomic<float> m_cfGain{1.0f};
};

} // namespace M1
