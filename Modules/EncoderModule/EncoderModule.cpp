#include "EncoderModule.h"
#include "EncoderSlot.h"
#include "EncoderListWidget.h"
#include "EncoderConfigDialog.h"
#include "DnasPoller.h"
#include "AudioBuffer.h"
#include "IcyMetadata.h"
#include <QSettings>
#include <QDebug>
#include <cmath>

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────
EncoderModule::EncoderModule(QObject* parent)
    : IModule(parent)
{
    // Create default slots up front so ring buffers exist before RT thread starts
    for (int i = 0; i < kDefaultSlots; ++i) {
        EncoderConfig cfg;
        cfg.slotId = i;
        cfg.name   = QString("Encoder %1").arg(i + 1);
        auto* slot = new EncoderSlot(this);
        slot->configure(cfg);
        connect(slot, &EncoderSlot::stateChanged, this, &EncoderModule::onSlotStateChanged);
        m_slots.append(slot);
    }
}

EncoderModule::~EncoderModule()
{
    shutdown();
}

// ─── IModule ──────────────────────────────────────────────────────────────────
void EncoderModule::initialize()
{
    // Start DNAS/Icecast2 stats poller
    m_poller = new DnasPoller(this);
    m_poller->start();

    // Auto-start slots that are configured to do so
    for (auto* s : m_slots)
        if (s->config().autoStart)
            s->connectToServer();

    qInfo() << "[EncoderModule] initialized with" << m_slots.size() << "slots.";
}

void EncoderModule::shutdown()
{
    for (auto* s : m_slots)
        if (s->state() != EncoderSlot::State::Idle)
            s->disconnectFromServer();

    if (m_poller) {
        m_poller->stop();
        m_poller->wait(3000);
    }
}

QWidget* EncoderModule::createWidget(QWidget* parent)
{
    m_listView = new EncoderListWidget(m_slots, m_poller, parent);
    connect(m_listView, &QObject::destroyed, this, [this]() { m_listView = nullptr; });
    connect(m_listView, &EncoderListWidget::addSlotRequested,
            this,       &EncoderModule::onAddSlotRequested);
    connect(m_listView, &EncoderListWidget::removeSlotRequested,
            this,       &EncoderModule::onRemoveSlotRequested);

    // Wire DnasPoller → list view refresh
    if (m_poller) {
        connect(m_poller, &DnasPoller::statsUpdated,
                m_listView, [this](const QString&, int, const QString&, const DnasPoller::MountStats&) {
                    if (m_listView) m_listView->refresh();
                });
    }

    return m_listView;
}

// ─── RT audio thread ──────────────────────────────────────────────────────────
void EncoderModule::onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& out)
{
    // Only write to LiveDeckMix slots — other sources feed themselves
    const int frames   = out.frames;
    const int channels = out.channels;
    if (frames <= 0 || channels < 1) return;

    // Measure actual peak levels from the PCM output buffer (RT-safe — no alloc, no Qt)
    // Use fabsf() explicitly — std::abs(float) on MSVC can resolve to int overload
    float peakL = 0.0f, peakR = 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float l = fabsf(out.data[i * channels]);
        if (l > peakL) peakL = l;
        if (channels > 1) {
            const float r = fabsf(out.data[i * channels + 1]);
            if (r > peakR) peakR = r;
        }
    }
    if (channels == 1) peakR = peakL;  // Mono: mirror L→R

    for (auto* s : m_slots) {
        if (s->config().source == EncoderConfig::Source::LiveDeckMix) {
            // Always update peaks so meters show signal even when idle
            s->setPeakLevels(peakL, peakR);
            s->incrementAudioBlockCount();
            if (s->state() == EncoderSlot::State::Streaming)
                s->ringBuffer().write(out.data, frames);
        }
    }
}

// ─── Metadata ─────────────────────────────────────────────────────────────────
void EncoderModule::onMetadataUpdate(const IcyMetadata& meta)
{
    qInfo() << "[EncoderModule] onMetadataUpdate received:"
            << meta.trackArtist << "-" << meta.trackTitle
            << "| streamTitle:" << meta.streamTitle
            << "| slots:" << m_slots.size();
    // Build ICY 2.2 field map from IcyMetadata
    QMap<QString, QString> icy2;
    auto add = [&](const QString& k, const QString& v) {
        if (!v.isEmpty()) icy2[k] = v;
    };
    add("icy2-track-bpm",   meta.trackBpm);
    add("icy2-track-key",   meta.trackKey);
    add("icy2-track-isrc",  meta.trackIsrc);
    add("icy2-track-mbid",  meta.trackMbid);
    add("icy2-track-label", meta.trackLabel);
    add("icy2-show-title",  meta.showTitle);
    add("icy2-dj-handle",   meta.djHandle);

    // Update ALL slots — not just Streaming ones. Slots in Connecting/Reconnecting
    // store the metadata so it's available to push once they enter Streaming state.
    for (auto* s : m_slots)
        s->updateMetadata(meta.trackArtist, meta.trackTitle, icy2);
}

// ─── PTT duck ─────────────────────────────────────────────────────────────────
void EncoderModule::setPttActive(bool active)
{
    for (auto* s : m_slots)
        if (s->config().pttDuckEnabled)
            s->setPttActive(active);
}

// ─── Slot management ──────────────────────────────────────────────────────────
int EncoderModule::slotCount()       const { return m_slots.size(); }
int EncoderModule::activeSlotCount() const
{
    int n = 0;
    for (const auto* s : m_slots)
        if (s->state() == EncoderSlot::State::Streaming) ++n;
    return n;
}

void EncoderModule::addSlot(const EncoderConfig& cfg)
{
    EncoderConfig c = cfg;
    c.slotId = m_slots.size();
    if (c.name.isEmpty()) c.name = QString("Encoder %1").arg(c.slotId + 1);

    auto* slot = new EncoderSlot(this);
    slot->configure(c);
    connect(slot, &EncoderSlot::stateChanged, this, &EncoderModule::onSlotStateChanged);
    m_slots.append(slot);

    if (m_listView) m_listView->rebuild();
}

void EncoderModule::removeSlot(int index)
{
    if (index < 0 || index >= m_slots.size()) return;
    auto* s = m_slots[index];
    if (s->state() != EncoderSlot::State::Idle) return;  // refuse to remove active slot

    m_slots.removeAt(index);
    if (m_poller) m_poller->unregisterMount(s->config().host, s->config().port, s->config().mount);
    s->deleteLater();

    if (m_listView) m_listView->rebuild();
}

void EncoderModule::onSlotStateChanged()
{
    const bool nowLive = (activeSlotCount() > 0);
    if (nowLive != m_wasLive) {
        m_wasLive = nowLive;
        emit encoderLiveChanged(nowLive);
    }
}

void EncoderModule::onAddSlotRequested()
{
    // Add with defaults; config dialog opens from EncoderListWidget
    addSlot({});
}

void EncoderModule::onRemoveSlotRequested(int index)
{
    removeSlot(index);
}

// ─── State persistence ────────────────────────────────────────────────────────
void EncoderModule::saveState(QSettings& s)
{
    s.beginWriteArray("encoder/v2/slots", m_slots.size());
    for (int i = 0; i < m_slots.size(); ++i) {
        s.setArrayIndex(i);
        const auto& cfg = m_slots[i]->config();
        s.setValue("name",           cfg.name);
        s.setValue("mode",           static_cast<int>(cfg.mode));
        s.setValue("source",         static_cast<int>(cfg.source));
        s.setValue("paDeviceIndex",  cfg.paDeviceIndex);
        s.setValue("codec",          static_cast<int>(cfg.codec));
        s.setValue("bitrate",        cfg.bitrate);
        s.setValue("vbrQuality",     static_cast<double>(cfg.vbrQuality));
        s.setValue("sampleRate",     cfg.sampleRate);
        s.setValue("channels",       cfg.channels);
        s.setValue("channelMode",    static_cast<int>(cfg.channelMode));
        s.setValue("serverType",     static_cast<int>(cfg.serverType));
        s.setValue("host",           cfg.host);
        s.setValue("port",           cfg.port);
        s.setValue("mount",          cfg.mount);
        s.setValue("password",       cfg.password);
        s.setValue("adminUser",      cfg.adminUser);
        s.setValue("adminPass",      cfg.adminPass);
        s.setValue("stationName",    cfg.stationName);
        s.setValue("description",    cfg.description);
        s.setValue("genre",          cfg.genre);
        s.setValue("url",            cfg.url);
        s.setValue("isPublic",       cfg.isPublic);
        s.setValue("useSsl",         cfg.useSsl);
        s.setValue("autoReconnect",  cfg.autoReconnect);
        s.setValue("retryInterval",  cfg.retryIntervalSec);
        s.setValue("maxRetries",     cfg.maxRetries);
        s.setValue("autoStart",      cfg.autoStart);
        s.setValue("dspEnabled",     cfg.dspEnabled);
        s.setValue("eqPreset",       cfg.eqPreset);
        s.setValue("agcEnabled",     cfg.agcEnabled);
        s.setValue("agcInputGain",   static_cast<double>(cfg.agcInputGainDb));
        s.setValue("agcThreshold",   static_cast<double>(cfg.agcThresholdDb));
        s.setValue("agcRatio",       static_cast<double>(cfg.agcRatio));
        s.setValue("agcAttack",      static_cast<double>(cfg.agcAttackMs));
        s.setValue("agcRelease",     static_cast<double>(cfg.agcReleaseMs));
        s.setValue("agcMakeup",      static_cast<double>(cfg.agcMakeupGainDb));
        s.setValue("agcLimiter",     static_cast<double>(cfg.agcLimiterDb));
        s.setValue("pttDuck",        cfg.pttDuckEnabled);
        s.setValue("pttAttenDb",     static_cast<double>(cfg.pttDuckAttenDb));
        s.setValue("archiveEnabled", cfg.archiveEnabled);
        s.setValue("archiveDir",     cfg.archiveDir);
        s.setValue("archiveWav",     cfg.archiveWav);
        s.setValue("archiveMp3",     cfg.archiveMp3);
        // ICY 2.2 fields
        s.beginGroup("icy2");
        for (auto it = cfg.icy2Fields.constBegin(); it != cfg.icy2Fields.constEnd(); ++it)
            s.setValue(it.key(), it.value());
        s.endGroup();
    }
    s.endArray();
}

void EncoderModule::loadState(QSettings& s)
{
    const int n = s.beginReadArray("encoder/v2/slots");
    // Resize slot list to match saved count
    while (m_slots.size() > n && n > 0) {
        auto* last = m_slots.takeLast();
        last->deleteLater();
    }
    while (m_slots.size() < n) {
        m_slots.append(new EncoderSlot(this));
    }

    for (int i = 0; i < n; ++i) {
        s.setArrayIndex(i);
        EncoderConfig cfg;
        cfg.slotId = i;
        cfg.name          = s.value("name",          QString("Encoder %1").arg(i+1)).toString();
        cfg.mode          = static_cast<EncoderConfig::Mode>(s.value("mode", 0).toInt());
        cfg.source        = static_cast<EncoderConfig::Source>(s.value("source", 0).toInt());
        cfg.paDeviceIndex = s.value("paDeviceIndex", -1).toInt();
        cfg.codec         = static_cast<EncoderConfig::Codec>(s.value("codec", 0).toInt());
        cfg.bitrate       = s.value("bitrate",        128).toInt();
        cfg.vbrQuality    = static_cast<float>(s.value("vbrQuality", 0.6).toDouble());
        cfg.sampleRate    = s.value("sampleRate",     44100).toInt();
        cfg.channels      = s.value("channels",       2).toInt();
        cfg.channelMode   = static_cast<EncoderConfig::ChannelMode>(s.value("channelMode", 1).toInt());
        cfg.serverType    = static_cast<EncoderConfig::ServerType>(s.value("serverType", 0).toInt());
        cfg.host          = s.value("host",           "localhost").toString();
        cfg.port          = s.value("port",           9000).toInt();
        cfg.mount         = s.value("mount",          "/stream").toString();
        cfg.password      = s.value("password",       "hackme").toString();
        cfg.adminUser     = s.value("adminUser",      "admin").toString();
        cfg.adminPass     = s.value("adminPass",      "admin").toString();
        cfg.stationName   = s.value("stationName").toString();
        cfg.description   = s.value("description").toString();
        cfg.genre         = s.value("genre").toString();
        cfg.url           = s.value("url").toString();
        cfg.isPublic      = s.value("isPublic",       false).toBool();
        cfg.useSsl        = s.value("useSsl",          false).toBool();
        cfg.autoReconnect = s.value("autoReconnect",  true).toBool();
        cfg.retryIntervalSec = s.value("retryInterval", 15).toInt();
        cfg.maxRetries    = s.value("maxRetries",     -1).toInt();
        cfg.autoStart     = s.value("autoStart",      false).toBool();
        cfg.dspEnabled    = s.value("dspEnabled",     false).toBool();
        cfg.eqPreset      = s.value("eqPreset",       "flat").toString();
        cfg.agcEnabled    = s.value("agcEnabled",     false).toBool();
        cfg.agcInputGainDb  = static_cast<float>(s.value("agcInputGain",  0.0).toDouble());
        cfg.agcThresholdDb  = static_cast<float>(s.value("agcThreshold", -18.0).toDouble());
        cfg.agcRatio        = static_cast<float>(s.value("agcRatio",      4.0).toDouble());
        cfg.agcAttackMs     = static_cast<float>(s.value("agcAttack",    10.0).toDouble());
        cfg.agcReleaseMs    = static_cast<float>(s.value("agcRelease",  200.0).toDouble());
        cfg.agcMakeupGainDb = static_cast<float>(s.value("agcMakeup",    0.0).toDouble());
        cfg.agcLimiterDb    = static_cast<float>(s.value("agcLimiter",  -1.0).toDouble());
        cfg.pttDuckEnabled  = s.value("pttDuck",      false).toBool();
        cfg.pttDuckAttenDb  = static_cast<float>(s.value("pttAttenDb", -12.0).toDouble());
        cfg.archiveEnabled  = s.value("archiveEnabled", false).toBool();
        cfg.archiveDir      = s.value("archiveDir").toString();
        cfg.archiveWav      = s.value("archiveWav",  true).toBool();
        cfg.archiveMp3      = s.value("archiveMp3", false).toBool();

        // ICY 2.2 fields
        s.beginGroup("icy2");
        for (const QString& key : s.allKeys())
            cfg.icy2Fields[key] = s.value(key).toString();
        s.endGroup();

        m_slots[i]->configure(cfg);
    }
    s.endArray();

    if (m_listView) m_listView->rebuild();
}

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
static Mcaster1PluginInfo s_encoderInfo{
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.encoder",
    "Encoder",
    "2.0.0",
    "*",
    "module",
    "Mcaster1",
    "Multi-codec streaming encoder — MP3/Opus/Vorbis/FLAC/AAC, DSP, ICY 2.2, DNAS"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_encoder_plugin_info() {
    return &s_encoderInfo;
}
MCASTER1_PLUGIN_API M1::IModule* mcaster1_encoder_create(IModuleHost*) {
    return new M1::EncoderModule();
}
MCASTER1_PLUGIN_API void mcaster1_encoder_destroy(M1::IModule* m) {
    delete m;
}
} // extern "C"
