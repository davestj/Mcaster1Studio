#pragma once
/// @file   ServiceRunnerModule.h
/// @path   Modules/ServiceRunnerModule/ServiceRunnerModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-ServiceRunner — Service Order & Rundown Manager
/// @purpose Manages the sequence of service segments (worship, sermon,
///          offering, etc.) with timing, auto-advance, and coordination
///          of all church modules. Provides a rundown view for operators
///          to follow along and control the flow of the service.
/// @reason  Church services follow a structured order of segments. Operators
///          need a central rundown to track timing, advance cues, and
///          coordinate between lyrics, scripture, media, and recording modules.
/// @changelog
///   2026-03-09  Initial implementation — rundown, segment timer, templates

#include "IModule.h"
#include <QList>
#include <QElapsedTimer>
#include <QJsonObject>

class QTimer;

namespace M1 {

class TimerClockModule;
class SwitchCasterModule;
class TranscribeRecModule;
class AudioMixModule;

// ─── SegmentType — classification of service segments ─────────────────────────
enum class SegmentType {
    Welcome,
    Worship,
    Prayer,
    Scripture,
    Sermon,
    Offering,
    Announcement,
    Communion,
    MediaPlayback,
    Closing,
    Custom
};

// ─── ServiceSegment — a single item in the service order ──────────────────────
struct ServiceSegment {
    int           id            = 0;
    SegmentType   type          = SegmentType::Custom;
    QString       title;
    QString       notes;            ///< Operator notes / instructions
    int           durationSec   = 0;///< Planned duration (0 = no limit)
    bool          autoAdvance   = false;
    QString       color;            ///< Display color hint (hex string)

    QJsonObject toJson() const;
    static ServiceSegment fromJson(const QJsonObject& obj);
};

// ─── SegmentStatus — runtime status of each segment ───────────────────────────
enum class SegmentStatus {
    Pending,
    Live,
    Done,
    Skipped
};

// ─── ServiceRunnerModule ──────────────────────────────────────────────────────
class ServiceRunnerModule : public IModule {
    Q_OBJECT

public:
    explicit ServiceRunnerModule(QObject* parent = nullptr);
    ~ServiceRunnerModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.servicerunner"; }
    QString displayName() const override { return "Service Runner"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {600, 450}; }
    QSize minimumModuleSize() const override { return {400, 300}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Module bindings ─────────────────────────────────────────────────────
    void setTimerClock(TimerClockModule* tc)       { m_timerClock = tc; }
    void setSwitchCaster(SwitchCasterModule* sw)    { m_switchCaster = sw; }
    void setTranscribeRec(TranscribeRecModule* tr)  { m_transcribeRec = tr; }
    void setAudioMix(AudioMixModule* am)            { m_audioMix = am; }

    // ── Service order management ────────────────────────────────────────────
    int  addSegment(const ServiceSegment& seg);
    void insertSegment(int index, const ServiceSegment& seg);
    void removeSegment(int index);
    void moveSegment(int from, int to);
    void clearService();
    int  segmentCount() const { return m_segments.size(); }
    ServiceSegment segment(int index) const;
    void updateSegment(int index, const ServiceSegment& seg);
    QList<ServiceSegment> serviceOrder() const { return m_segments; }

    // ── Service metadata ────────────────────────────────────────────────────
    void setServiceTitle(const QString& title) { m_serviceTitle = title; }
    QString serviceTitle() const { return m_serviceTitle; }

    // ── Live control ────────────────────────────────────────────────────────
    void startService();
    void nextSegment();
    void prevSegment();
    void goToSegment(int index);
    void skipSegment();
    void pauseService();
    void resumeService();
    void stopService();

    // ── State query ─────────────────────────────────────────────────────────
    int  currentSegmentIndex() const { return m_currentIndex; }
    bool isRunning() const { return m_running; }
    bool isPaused()  const { return m_paused; }
    qint64 segmentElapsedMs() const;
    qint64 serviceElapsedMs() const;
    qint64 totalPlannedMs() const;
    SegmentStatus segmentStatus(int index) const;

    // ── Templates ───────────────────────────────────────────────────────────
    bool saveTemplate(const QString& filePath);
    bool loadTemplate(const QString& filePath);
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj);

    // ── Segment type name utility ───────────────────────────────────────────
    static QString segmentTypeName(SegmentType type);
    static QString segmentTypeColor(SegmentType type);

signals:
    void serviceStarted();
    void serviceStopped();
    void servicePaused(bool paused);
    void segmentChanged(int index);
    void segmentAdded(int index);
    void segmentRemoved(int index);
    void segmentMoved(int from, int to);
    void serviceOrderChanged();
    void segmentTimerTick(qint64 elapsedMs, qint64 totalMs);

private slots:
    void onTick();

private:
    // Bound modules
    TimerClockModule*    m_timerClock    = nullptr;
    SwitchCasterModule*  m_switchCaster  = nullptr;
    TranscribeRecModule* m_transcribeRec = nullptr;
    AudioMixModule*      m_audioMix     = nullptr;

    // Service order
    QList<ServiceSegment> m_segments;
    QString m_serviceTitle;
    int m_nextId = 1;

    // Runtime state
    bool   m_running      = false;
    bool   m_paused       = false;
    int    m_currentIndex  = -1;

    // Timing
    QElapsedTimer m_serviceTimer;
    QElapsedTimer m_segmentTimer;
    qint64 m_servicePauseAccum = 0;
    qint64 m_segmentPauseAccum = 0;
    qint64 m_pauseStartMs      = 0;

    // Segment status tracking
    QList<SegmentStatus> m_statuses;

    // Timer
    QTimer* m_tickTimer = nullptr;

    // Timer clock integration
    int m_segmentTimerId = -1;

    void activateSegment(int index);
    void deactivateSegment(int index);
};

} // namespace M1
