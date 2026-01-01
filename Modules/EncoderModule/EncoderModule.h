#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include "EncoderConfig.h"
#include <QList>

class EncoderSlot;
class EncoderListWidget;
class DnasPoller;

namespace M1 {

/// EncoderModule — Phase F revamped multi-slot streaming encoder.
///
/// Features vs Phase 4:
///   - List-view UI (QTableWidget rows) with double-click config dialog
///   - 7-state machine per slot (Idle/Starting/Connecting/Streaming/Reconnecting/Sleep/Error)
///   - 7 codecs: MP3, Opus, Vorbis, FLAC, AAC-LC, HE-AAC v1/v2
///   - 3 audio sources: LiveDeckMix (RT tap), PortAudio device, WASAPI loopback
///   - Per-slot DSP: 10-band EQ + AGC + PTT duck
///   - Reconnect watchdog with configurable retry + sleep
///   - DNAS/Icecast2 listener stats polling (DnasPoller)
///   - Full ICY 2.2 extended metadata (all 70+ fields)
///   - DNAS is the primary / default server type
class EncoderModule : public IModule {
    Q_OBJECT

public:
    static constexpr int kDefaultSlots = 4;  // Start with 4; user can add/remove

    explicit EncoderModule(QObject* parent = nullptr);
    ~EncoderModule() override;

    // ── IModule ──────────────────────────────────────────────────────────────
    QString  moduleId()    const override { return "com.mcaster1.encoder"; }
    QString  displayName() const override { return "Encoder"; }
    QSize    preferredSize() const override { return {900, 180}; }
    QWidget* createWidget(QWidget* parent) override;

    void initialize()                          override;
    void shutdown()                            override;
    void onAudioBlock(AudioBuffer& in,
                      AudioBuffer& out)        override;
    void onMetadataUpdate(const IcyMetadata& meta) override;

    void saveState(QSettings& s)  override;
    void loadState(QSettings& s)  override;

    // ── PTT duck ─────────────────────────────────────────────────────────────
    /// Called by MainWindow when PTTModule state changes.
    /// Propagates to all slots that have pttDuckEnabled = true.
    void setPttActive(bool active);

    // ── Slot management (Qt thread only) ─────────────────────────────────────
    int   slotCount()       const;
    int   activeSlotCount() const;
    void  addSlot(const EncoderConfig& cfg = {});
    void  removeSlot(int index);

signals:
    /// Emitted when the number of live (Streaming) slots transitions between 0 and >0.
    void encoderLiveChanged(bool anyLive);

public slots:
    void onAddSlotRequested();
    void onRemoveSlotRequested(int index);

private:
    void onSlotStateChanged();
    bool m_wasLive = false;
    QList<EncoderSlot*>  m_slots;
    DnasPoller*          m_poller   = nullptr;
    EncoderListWidget*   m_listView = nullptr;   // owned by the widget tree
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_encoder_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_encoder_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_encoder_destroy(M1::IModule*);
}
