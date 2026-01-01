#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QTimer>

namespace M1 { class DeckAModule; class DeckBModule; class DeckPlayer; }

/// CrossfaderModule — standalone crossfader for DeckA / DeckB.
///
/// A big DJ-style horizontal slider that controls the mix between
/// standalone Deck A and Deck B.  Reads/writes the global CrossfadeSettings
/// (QSettings "Crossfade" group) and applies the selected curve.
///
/// AutoDJ (PlaylistModule / QueueModule) can call animateTo() via signal
/// to drive automatic crossfades.
class CrossfaderModule : public M1::IModule {
    Q_OBJECT

public:
    explicit CrossfaderModule(QObject* parent = nullptr);
    ~CrossfaderModule() override;

    QString moduleId()    const override { return "com.mcaster1.crossfader"; }
    QString displayName() const override { return "Crossfader"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {500, 260}; }
    QSize minimumModuleSize() const override { return {340, 210}; }

    void initialize() override;
    void shutdown()   override;

    QWidget* createWidget(QWidget* parent) override;

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    /// Connect to standalone deck modules so crossfader can adjust their gain.
    void connectDecks(M1::DeckAModule* a, M1::DeckBModule* b);

    /// Current crossfader position [0.0 = full A … 1.0 = full B].
    float value() const;

    /// Current fade duration from the spinbox (seconds).
    float fadeDuration() const;

    /// Set curve mode: 0=Linear, 1=SCurve, 2=Exponential.
    void setCurveMode(int mode);

    /// Open the CrossfaderSettingsDialog.
    void openSettingsDialog();

public slots:
    /// Smooth animated move to target over durationMs ms.
    /// Called by PlaylistModule / QueueModule AutoDJ via signal.
    void animateTo(float target, int durationMs);

    /// Set crossfader position immediately.
    void setValue(float v);

    /// Intelligent auto-crossfade: detects active deck and fades to opposite.
    void autoCrossfade();

signals:
    void crossfaderChanged(float value);

    /// Emitted when an auto-crossfade (from the AUTO button) finishes.
    /// stoppedDeck: 0=Deck A was stopped, 1=Deck B was stopped.
    void autoFadeCompleted(int stoppedDeck);

private slots:
    void onAnimTick();

private:
    void applyGains(float pos);   ///< compute curve-based gains and push to decks

    M1::DeckAModule* m_deckA = nullptr;
    M1::DeckBModule* m_deckB = nullptr;

    float       m_position   = 0.5f;
    int         m_curveMode  = 1;    // 0=Linear, 1=SCurve, 2=Exponential

    // Animation
    QTimer*     m_animTimer  = nullptr;
    float       m_animTarget = 0.5f;
    float       m_animStep   = 0.0f;
    bool        m_autoFadeActive = false;  ///< true during auto crossfade (stop old deck + reset after)
    int         m_fadeSourceDeck = -1;     ///< 0=A was source (fading out), 1=B was source, -1=none

    QWidget*    m_widget     = nullptr;
};
