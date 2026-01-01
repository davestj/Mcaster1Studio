#pragma once
#include <QObject>
#include <QWidget>
#include <QSize>
#include <QSettings>
#include "AudioBuffer.h"
#include "IcyMetadata.h"
#include "MediaItem.h"

namespace M1 {

/// Base interface for all Mcaster1Studio surface modules.
///
/// Every module is a QObject-derived class. The UI widget is created
/// separately via createWidget() and embedded in a ModuleDock frame.
///
/// Audio processing (onAudioBlock) is called from the audio engine's
/// real-time callback thread — do NOT allocate memory or call Qt APIs
/// from inside onAudioBlock().
class IModule : public QObject {
    Q_OBJECT

public:
    explicit IModule(QObject* parent = nullptr) : QObject(parent) {}
    ~IModule() override = default;

    // ------------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------------
    virtual QString moduleId()     const = 0; ///< "com.mcaster1.vumeter"
    virtual QString displayName()  const = 0; ///< "VU Meter"
    virtual QString version()      const { return "1.0.0"; }
    virtual QString vendor()       const { return "Mcaster1"; }

    /// Preferred size hint for ModuleDock container
    virtual QSize   preferredSize() const { return {320, 240}; }

    /// Minimum size — ModuleDock will not size below this
    virtual QSize   minimumModuleSize() const { return {200, 120}; }

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------

    /// Called after construction, before createWidget().
    /// Safe to call Qt APIs here.
    virtual void initialize() {}

    /// Called before destruction. Release resources here.
    virtual void shutdown() {}

    // ------------------------------------------------------------------
    // UI
    // ------------------------------------------------------------------

    /// Create and return the module's primary widget.
    /// The returned widget is owned by the ModuleDock.
    virtual QWidget* createWidget(QWidget* parent) = 0;

    // ------------------------------------------------------------------
    // Audio (real-time thread — no Qt, no allocation)
    // ------------------------------------------------------------------

    /// Called per audio block by the AudioEngine callback thread.
    /// in:  audio from the engine's selected input bus for this module
    /// out: audio to be contributed to the engine's mix
    /// Default: pass-through (copy in → out)
    virtual void onAudioBlock(AudioBuffer& in, AudioBuffer& out) {
        (void)in; (void)out;
    }

    // ------------------------------------------------------------------
    // Events (Qt thread — safe to update UI)
    // ------------------------------------------------------------------

    /// Called when now-playing metadata changes (e.g. from DeckModule).
    virtual void onMetadataUpdate(const IcyMetadata& meta) { (void)meta; }

    /// Called when a media item is loaded onto a deck.
    virtual void onMediaLoaded(const MediaItem& item, int deckIndex) {
        (void)item; (void)deckIndex;
    }

    // ------------------------------------------------------------------
    // State persistence
    // ------------------------------------------------------------------

    /// Save module state to QSettings (called on app exit / surface save).
    virtual void saveState(QSettings& s) = 0;

    /// Restore module state from QSettings (called on surface load).
    virtual void loadState(QSettings& s) = 0;

signals:
    /// Emit to show an error in the status bar / log
    void moduleError(const QString& msg);

    /// Emit to update the status shown in the ModuleDock title bar
    void statusChanged(const QString& status);

    /// Emit when this module wants to push metadata to connected encoders
    void metadataReady(const M1::IcyMetadata& meta);

    /// Emit when this module requests a media item to be loaded to a deck
    void requestLoadMedia(const M1::MediaItem& item, int deckIndex);
};

} // namespace M1
