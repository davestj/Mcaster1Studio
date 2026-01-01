#pragma once
#include "MediaItem.h"
#include <QList>
#include <QDateTime>
#include <QString>

/// BroadcastLogger — records played tracks and exports logs in two formats:
///
/// - M3U  : Standard extended M3U playlist with EXTINF metadata.
/// - SAM Broadcaster CSV : Timestamp, title, artist, album, duration, filePath.
///
/// The logger is in-memory only during a session; call exportM3U() or
/// exportSamCsv() to persist to disk.
class BroadcastLogger {
public:
    BroadcastLogger() = default;

    /// Record a played track with the wall-clock time it started playing.
    void logTrack(const M1::MediaItem& item, const QDateTime& playedAt);

    /// Export logged tracks as an extended M3U file.
    /// Returns true on success.
    bool exportM3U(const QString& path) const;

    /// Export logged tracks as a SAM Broadcaster-compatible CSV file.
    /// Columns: timestamp, title, artist, album, duration, filePath
    /// Returns true on success.
    bool exportSamCsv(const QString& path) const;

    /// Number of entries in the current log.
    int  count() const { return m_log.size(); }

    /// Clear the in-memory log.
    void clear() { m_log.clear(); }

private:
    struct LogEntry {
        M1::MediaItem item;
        QDateTime     playedAt;
    };

    QList<LogEntry> m_log;
};
