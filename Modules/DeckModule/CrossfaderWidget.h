#pragma once
#include <QWidget>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

namespace M1 { class DeckPlayer; }

/// CrossfaderWidget — standalone Fade Control panel between Deck A and Deck B.
///
/// Responsibilities:
///   • Main crossfader slider: 0.0 = full Deck A, 1.0 = full Deck B
///   • Fade curve selector: Standard (linear) / S-Curve (equal-power) / Exponential
///   • Fade duration setting (spinbox, 0.5–30 s)
///   • Animated auto-fade: A→B or B→A over the configured duration
///   • Sync BPM button (emits syncDecksRequested)
///   • animateTo(target, ms): called by QueueModule AutoDJ to drive automatic crossfades
///
/// All buttons have tooltips. No pitch strips (those are on the individual deck panels).
class CrossfaderWidget : public QWidget {
    Q_OBJECT

public:
    enum class CurveMode { Standard = 0, SCurve = 1, Exponential = 2 };
    Q_ENUM(CurveMode)

    explicit CrossfaderWidget(QWidget* parent = nullptr);

    /// Current crossfader position [0.0 = full A … 1.0 = full B].
    float value() const;
    void  setValue(float v);

    /// Smooth animated move to target over durationMs milliseconds.
    /// Called by QueueModule AutoDJ.
    void animateTo(float target, int durationMs);

    /// Wire deck players so AUTO SYNC FADE can read remaining track time.
    void setDecks(M1::DeckPlayer* a, M1::DeckPlayer* b);

    /// Currently selected fade curve.
    CurveMode curveMode() const { return m_curve; }

    /// Crossfade duration (seconds) from the spinbox.
    float fadeDuration() const;

    /// Update per-deck BPM display (shown as information only).
    void setDeckABpm(float bpm);
    void setDeckBBpm(float bpm);

    /// Open the cross-fading settings dialog (Qt Charts interactive curve editor).
    void openSettingsDialog();

signals:
    void crossfaderChanged(float value);        ///< [0.0..1.0] on every slider move
    void curveModeChanged(CurveMode mode);      ///< on curve button click
    void syncDecksRequested();                  ///< A=B BPM sync button

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onCfSliderMoved(int pos);
    void onCurveSelected(int mode);
    void onAutoFadeAB();
    void onAutoFadeBA();
    void onAutoFadeSynced();
    void onAnimTick();

private:
    // Crossfader
    QSlider*        m_cfSlider   = nullptr;
    CurveMode       m_curve      = CurveMode::SCurve;

    // Curve toggle buttons
    QPushButton*    m_stdBtn     = nullptr;
    QPushButton*    m_sCrvBtn    = nullptr;
    QPushButton*    m_expBtn     = nullptr;

    // Duration spinbox
    QDoubleSpinBox* m_durSpin    = nullptr;

    // Auto-fade + sync buttons
    QPushButton*    m_autoFadeBtn = nullptr;   // AUTO SYNC FADE
    QPushButton*    m_fadeABBtn  = nullptr;
    QPushButton*    m_fadeBABtn  = nullptr;
    QPushButton*    m_syncBtn    = nullptr;

    // Deck references for AUTOFADE remaining-time detection
    M1::DeckPlayer* m_deckA      = nullptr;
    M1::DeckPlayer* m_deckB      = nullptr;

    // BPM info labels (read-only display)
    QLabel*         m_aBpmLabel  = nullptr;
    QLabel*         m_bBpmLabel  = nullptr;

    // Animation
    QTimer*         m_animTimer  = nullptr;
    float           m_animTarget = 0.5f;
    float           m_animStep   = 0.0f;   // per-tick step (set in animateTo)
};
