#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QTime>

class QTimer;

/// Types of scheduled automation event.
enum class ScheduledEventType {
    LoadPlaylist,   ///< Load a playlist file into AutoDJ queue
    InsertJingle,   ///< Load a media file on Deck A and auto-play
    InsertVideo,    ///< Trigger a video file load
    LoadMedia,      ///< Load a media file on a specific deck (deckIndex in data2)
    RunCommand,     ///< Arbitrary string command (future extension point)
};

/// A single time-triggered automation event for a surface.
struct ScheduledEvent {
    int               id         = 0;
    bool              enabled    = true;
    QTime             triggerTime;       ///< Time of day to fire
    ScheduledEventType type      = ScheduledEventType::LoadPlaylist;
    QString           label;            ///< Human-readable description
    QString           data;             ///< Primary payload (file path, command, etc.)
    QString           data2;            ///< Secondary payload (deck index "0"/"1", etc.)
    bool              repeat     = true; ///< If false, disable after first fire
};

/// SurfaceScheduler — per-surface time-based automation.
///
/// Polls a QTimer every 30 s, evaluates ScheduledEvents against the current
/// wall-clock time, and emits signals that SurfaceWidget/MainWindow can act on.
///
/// Events fire when current time matches trigger time within a ±30s window.
/// One-shot events are disabled after firing.
class SurfaceScheduler : public QObject {
    Q_OBJECT

public:
    explicit SurfaceScheduler(QObject* parent = nullptr);

    void addEvent(const ScheduledEvent& ev);
    void removeEvent(int id);
    void updateEvent(const ScheduledEvent& ev);
    const QList<ScheduledEvent>& events() const { return m_events; }

    /// Start the polling timer.
    void start();
    void stop();

signals:
    void loadPlaylist(const QString& path);
    void insertJingle(const QString& path, int deckIndex);
    void insertVideo(const QString& path);
    void loadMedia(const QString& path, int deckIndex);

    /// Emitted when any event fires (for the event log).
    void eventFired(const ScheduledEvent& ev);

private slots:
    void onPoll();

private:
    QList<ScheduledEvent> m_events;
    QTimer*               m_timer     = nullptr;
    int                   m_nextId    = 1;
    QTime                 m_lastPoll;
};
