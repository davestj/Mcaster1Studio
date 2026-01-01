#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <atomic>
#include <vector>

// DeckPlayer is in M1:: namespace; DeckWidget/DeckPanel are in global namespace
namespace M1 { class DeckPlayer; }
class DeckWidget;

namespace M1 {

/// DeckModule — Phase 2 dual-deck player module.
///
/// Manages two DeckPlayer instances (Deck A, Deck B) and a crossfader.
/// Implements onAudioBlock() for real-time audio mixing:
///   output = DeckA × (1 - crossfader) + DeckB × crossfader
class DeckModule : public IModule {
    Q_OBJECT

public:
    explicit DeckModule(QObject* parent = nullptr);
    ~DeckModule() override;

    // IModule identity
    QString moduleId()    const override { return "com.mcaster1.deck"; }
    QString displayName() const override { return "Deck Player"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {900, 340}; }
    QSize minimumModuleSize() const override { return {640, 280}; }

    // Lifecycle
    void initialize() override;
    void shutdown()   override;

    // UI
    QWidget* createWidget(QWidget* parent) override;
    ::DeckWidget* deckWidget() const { return m_widget; }

    // State persistence
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Audio processing (called from RT thread via AudioEngine callback)
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;

    DeckPlayer* deckA() const { return m_deckA; }
    DeckPlayer* deckB() const { return m_deckB; }

    /// RT-safe crossfader control (called from QueueModule AutoDJ).
    void setCrossfader(float v) {
        m_crossfader.store(std::clamp(v, 0.0f, 1.0f), std::memory_order_release);
    }
    float crossfader() const { return m_crossfader.load(std::memory_order_relaxed); }

    /// Crossfade curve mode (0=Standard, 1=S-Curve, 2=Exponential).
    void setCurveMode(int mode) {
        m_curveMode.store(mode, std::memory_order_relaxed);
    }
    int curveMode() const { return m_curveMode.load(std::memory_order_relaxed); }

public slots:
    /// Animate the crossfader widget to target over durationMs ms.
    /// Called by QueueModule AutoDJ via signal-slot connection.
    void animateCrossfaderTo(float target, int durationMs);

    /// Open the crossfade settings dialog.
    /// If the deck widget is visible the dialog opens sheet-style on the crossfader;
    /// otherwise it opens as a standalone top-level dialog.
    void openCrossfadeSettings();

    /// Returns the CUE mix buffer from the last onAudioBlock call.
    /// MainWindow reads this to feed the CUE ring buffer.
    const float* cueMix() const { return m_cueBuf.data(); }
    int cueMixFrames() const { return m_cueMixFrames; }

private:
    DeckPlayer*         m_deckA     = nullptr;
    DeckPlayer*         m_deckB     = nullptr;
    ::DeckWidget*       m_widget    = nullptr;
    std::atomic<float>  m_crossfader{0.5f};    // 0.0=A, 1.0=B
    std::atomic<int>    m_curveMode{1};         // default: S-Curve

    // Per-deck temp buffers + CUE mix (pre-allocated, no RT alloc)
    std::vector<float>  m_tmpA;
    std::vector<float>  m_tmpB;
    std::vector<float>  m_cueBuf;
    int                 m_cueMixFrames = 0;
};

} // namespace M1
