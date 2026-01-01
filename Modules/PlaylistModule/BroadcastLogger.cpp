#include "BroadcastLogger.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
// Logging
// ─────────────────────────────────────────────────────────────────────────────

void BroadcastLogger::logTrack(const M1::MediaItem& item, const QDateTime& playedAt)
{
    m_log.append({item, playedAt});
    qInfo() << "[BroadcastLogger] Logged:" << item.displayTitle()
            << "at" << playedAt.toString(Qt::ISODate);
}

// ─────────────────────────────────────────────────────────────────────────────
// M3U export
// Format:
//   #EXTM3U
//   #EXTINF:<duration_seconds>,<Artist> - <Title>
//   <filePath>
// ─────────────────────────────────────────────────────────────────────────────

bool BroadcastLogger::exportM3U(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[BroadcastLogger] Cannot write M3U to:" << path;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "#EXTM3U\n";

    for (const LogEntry& entry : m_log) {
        const M1::MediaItem& item = entry.item;

        const int durationSecs = item.durationMs > 0 ? item.durationMs / 1000 : -1;

        // Build display string: "Artist - Title" or just "Title"
        QString display = item.displayTitle();
        const QString artist = item.displayArtist();
        if (!artist.isEmpty())
            display = artist + " - " + display;

        out << "#EXTINF:" << durationSecs << "," << display << "\n";
        out << item.filePath << "\n";
    }

    qInfo() << "[BroadcastLogger] Exported M3U:" << path
            << "(" << m_log.size() << " tracks)";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// SAM Broadcaster CSV export
// Columns: timestamp,title,artist,album,duration,filePath
// ─────────────────────────────────────────────────────────────────────────────

bool BroadcastLogger::exportSamCsv(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[BroadcastLogger] Cannot write SAM CSV to:" << path;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Header row
    out << "timestamp,title,artist,album,duration,filePath\n";

    auto csvEscape = [](const QString& s) -> QString {
        // Wrap in quotes; escape internal quotes by doubling them
        QString escaped = s;
        escaped.replace('"', "\"\"");
        return '"' + escaped + '"';
    };

    for (const LogEntry& entry : m_log) {
        const M1::MediaItem& item = entry.item;
        const int durationSecs = item.durationMs > 0 ? item.durationMs / 1000 : 0;

        out << csvEscape(entry.playedAt.toString(Qt::ISODate)) << ","
            << csvEscape(item.displayTitle())                  << ","
            << csvEscape(item.displayArtist())                 << ","
            << csvEscape(item.album)                           << ","
            << durationSecs                                    << ","
            << csvEscape(item.filePath)                        << "\n";
    }

    qInfo() << "[BroadcastLogger] Exported SAM CSV:" << path
            << "(" << m_log.size() << " tracks)";
    return true;
}
