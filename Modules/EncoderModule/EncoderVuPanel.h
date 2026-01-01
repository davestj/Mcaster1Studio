#pragma once
#include <QWidget>
#include <QList>
#include <QTimer>
#include <vector>

class EncoderSlot;

/// EncoderVuPanel — mini vertical stereo VU meters for each encoder slot.
///
/// Displays REAL audio levels measured from the LiveDeckMix pipeline in
/// EncoderModule::onAudioBlock(). Levels are stored as std::atomic<float>
/// on each EncoderSlot and polled here at 20 fps with broadcast-standard
/// ballistic smoothing (fast attack, slow release, 3-second peak hold).
///
/// If no audio is playing (decks silent, AutoDJ stopped), the meters
/// show zero — they reflect actual measured PCM peak values, never faked.
class EncoderVuPanel : public QWidget {
    Q_OBJECT

public:
    explicit EncoderVuPanel(QList<EncoderSlot*>* slotList, QWidget* parent = nullptr);

    /// Call after adding/removing encoder slots to resize the meter array.
    void rebuild();

    QSize sizeHint()        const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void onTick();

    /// Per-slot smoothed meter state (UI thread only).
    struct MeterState {
        float smoothL = 0.0f, smoothR = 0.0f;    // Smoothed linear levels
        float peakL   = 0.0f, peakR   = 0.0f;    // Peak hold levels (linear)
        int   holdL   = 0,    holdR   = 0;        // Frames remaining in peak hold
    };

    QList<EncoderSlot*>*   m_slots = nullptr;
    QTimer*                m_timer = nullptr;
    std::vector<MeterState> m_meters;

    // ── Layout constants ────────────────────────────────────────────────────
    static constexpr int kBarW        = 7;    // Width of one L or R bar (px)
    static constexpr int kLRGap       = 2;    // Gap between L and R bars
    static constexpr int kSlotGap     = 6;    // Gap between slot pairs
    static constexpr int kLabelH      = 14;   // Height for slot number label
    static constexpr int kMargin      = 4;    // Edge margin

    // ── Ballistic constants ─────────────────────────────────────────────────
    static constexpr float kAttack        = 0.7f;   // Fast attack (~14ms @20fps)
    static constexpr float kRelease       = 0.04f;  // Slow release (~2.5s @20fps)
    static constexpr int   kPeakHoldFrames = 60;    // 3 seconds @ 20fps
    static constexpr float kPeakDecay     = 0.97f;  // Decay after hold expires

    // ── dB range ────────────────────────────────────────────────────────────
    static constexpr float kDbMin  = -60.0f;
    static constexpr float kDbClip =   0.0f;
};
