#pragma once
/// @file   TimerClockModule.h
/// @path   Modules/TimerClockModule/TimerClockModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-TimerClock — Master Timer and Clock Module
/// @purpose Provides master clock display (NTP-synced), multiple simultaneous
///          countdown/countup timer instances, alarm thresholds, and segment
///          timing for the Church Surface and any other surface that needs
///          precision time services.
/// @reason  Foundation layer for Church Surface — TimerClock has no dependencies
///          and is consumed by ServiceRunner (segment timers), StageMon (sermon
///          timer display), and AnnounceCaster (pre-service countdown).
/// @changelog
///   2026-03-09  Initial implementation — NTP sync, timer instances, alarms

#include "IModule.h"
#include "IPlugin.h"
#include <QMap>
#include <QElapsedTimer>

class QTimer;

namespace M1 {

// ─── TimerInstance — a single countdown or countup timer ─────────────────────
/// We model each timer as a named instance with its own state. Multiple timers
/// can run simultaneously (e.g., sermon timer + segment timer + service clock).
struct TimerInstance {
    enum class Mode { CountUp, CountDown };
    enum class State { Stopped, Running, Paused, Expired };

    QString   name;                       ///< Human-readable name ("Sermon Timer")
    Mode      mode    = Mode::CountUp;    ///< Count direction
    State     state   = State::Stopped;   ///< Current state
    qint64    durationMs  = 0;            ///< Total duration for countdown (ms)
    qint64    elapsedMs   = 0;            ///< Accumulated elapsed time (ms)
    qint64    warningMs   = 300000;       ///< Warning threshold (default 5 min)
    qint64    alarmMs     = 0;            ///< Alarm threshold (default 0 = at expiry)

    /// We track wall-clock start so we can compute elapsed without drift
    QElapsedTimer wallClock;
    qint64        pauseAccumMs = 0;       ///< Accumulated ms at last pause

    /// Returns remaining ms for countdown, elapsed ms for countup
    qint64 remainingMs() const {
        if (mode == Mode::CountDown)
            return qMax(0LL, durationMs - elapsedMs);
        return elapsedMs;
    }

    /// Returns true if the timer is at or past the warning threshold
    bool isWarning() const {
        if (mode == Mode::CountDown)
            return remainingMs() <= warningMs && remainingMs() > 0;
        return elapsedMs >= (durationMs - warningMs) && durationMs > 0;
    }

    /// Returns true if the timer has expired (countdown reached 0)
    bool isExpired() const {
        return mode == Mode::CountDown && remainingMs() <= 0 && state != State::Stopped;
    }

    /// Format elapsed/remaining as HH:MM:SS or MM:SS
    QString formatted() const {
        qint64 ms = (mode == Mode::CountDown) ? remainingMs() : elapsedMs;
        int totalSec = static_cast<int>(ms / 1000);
        int h = totalSec / 3600;
        int m = (totalSec % 3600) / 60;
        int s = totalSec % 60;
        if (h > 0)
            return QString("%1:%2:%3")
                .arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
        return QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    }
};

// ─── TimerClockModule ────────────────────────────────────────────────────────
/// Master clock and timer system for the Church Surface.
///
/// We provide:
///   - Master clock synced to system time (NTP via OS)
///   - Multiple named timer instances (countdown / countup)
///   - Alarm and warning thresholds with signals
///   - Segment timing for ServiceRunner rundown items
///
/// Other modules connect to our signals to receive timer updates.
class TimerClockModule : public IModule {
    Q_OBJECT

public:
    explicit TimerClockModule(QObject* parent = nullptr);
    ~TimerClockModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.timerclock"; }
    QString displayName() const override { return "Timer / Clock"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {420, 320}; }
    QSize minimumModuleSize() const override { return {300, 200}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Timer management API ────────────────────────────────────────────────

    /// Create a new timer instance. Returns the timer ID.
    int createTimer(const QString& name,
                    TimerInstance::Mode mode = TimerInstance::Mode::CountUp,
                    qint64 durationMs = 0);

    /// Start a timer by ID
    void startTimer(int id);

    /// Pause a running timer
    void pauseTimer(int id);

    /// Resume a paused timer
    void resumeTimer(int id);

    /// Stop and reset a timer
    void stopTimer(int id);

    /// Remove a timer instance
    void removeTimer(int id);

    /// Set the countdown duration (only for CountDown mode)
    void setTimerDuration(int id, qint64 durationMs);

    /// Set warning threshold in ms (timer turns amber)
    void setTimerWarning(int id, qint64 warningMs);

    /// Set alarm threshold in ms (timer turns red, signal emitted)
    void setTimerAlarm(int id, qint64 alarmMs);

    /// Get a timer instance by ID (const)
    const TimerInstance* timer(int id) const;

    /// Get all timer instances
    QMap<int, TimerInstance> allTimers() const { return m_timers; }

    /// Get the current master clock time string (HH:MM:SS AP)
    QString masterClockTime() const { return m_masterTimeStr; }

    /// Get the current master clock date string
    QString masterClockDate() const { return m_masterDateStr; }

signals:
    /// Emitted every tick (~100ms) with updated timer data
    void timerTick(int timerId, qint64 elapsedMs, qint64 remainingMs);

    /// Emitted when a timer enters warning state
    void timerWarning(int timerId);

    /// Emitted when a timer expires (countdown reached 0)
    void timerExpired(int timerId);

    /// Emitted when a timer's state changes
    void timerStateChanged(int timerId, TimerInstance::State newState);

    /// Emitted every second with master clock update
    void masterClockTick(const QString& time, const QString& date);

private slots:
    void onMasterTick();
    void onTimerTick();

private:
    QTimer*  m_masterTimer = nullptr;   ///< 1Hz master clock tick
    QTimer*  m_timerTick   = nullptr;   ///< 100ms timer instance tick

    int      m_nextTimerId = 1;         ///< Auto-incrementing timer ID
    QMap<int, TimerInstance> m_timers;   ///< Active timer instances

    QString  m_masterTimeStr;
    QString  m_masterDateStr;
};

} // namespace M1
