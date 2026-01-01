#pragma once
#include <QObject>
#include "IcyMetadata.h"
#include "MediaItem.h"

namespace M1 {

/// Global event bus for inter-module communication.
/// Modules connect to these signals via EventBus::instance().
/// This decouples modules — they don't need direct references to each other.
class EventBus : public QObject {
    Q_OBJECT

public:
    static EventBus& instance();

signals:
    /// Emitted by DeckModule when now-playing changes
    void nowPlayingChanged(const M1::IcyMetadata& meta, int deckIndex);

    /// Emitted by DeckModule when a track finishes
    void trackFinished(const M1::MediaItem& item, int deckIndex);

    /// Emitted by DeckModule when a track starts
    void trackStarted(const M1::MediaItem& item, int deckIndex);

    /// Emitted by any module requesting metadata push to encoders
    void metadataPushRequested(const M1::IcyMetadata& meta);

    /// Emitted by AudioEngine when levels update (fast timer, ~20fps)
    void audioLevelsChanged(float inL, float inR, float outL, float outR);

    /// Emitted when audio engine state changes
    void audioEngineStateChanged(int state); // M1::AudioEngineState

    /// Emitted by EncoderModule when connection status changes
    void encoderStatusChanged(int slotIndex, bool connected, const QString& serverUrl);

    /// Emitted by PlaylistModule when automation state changes
    void automationStateChanged(bool running);

    /// Global application log message (shown in any log panel)
    void logMessage(const QString& msg, int level); // 0=info, 1=warn, 2=error

private:
    EventBus() = default;
};

} // namespace M1
