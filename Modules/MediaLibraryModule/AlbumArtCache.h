#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>
#include <QHash>
#include <QMutex>
#include <QStringList>

namespace M1 {

/// AlbumArtCache — 3-tier album art caching system.
///
/// Tier 1: Memory LRU cache (max 200 entries) — QHash<QString, QPixmap>
/// Tier 2: Disk cache at <appDir>/cache/artwork/ — JPEG files named by SHA256
/// Tier 3: Extract from file via TagLib on cache miss
///
/// Thread-safe: all public methods are mutex-protected.
class AlbumArtCache : public QObject {
    Q_OBJECT

public:
    explicit AlbumArtCache(const QString& cacheDir, QObject* parent = nullptr);

    /// Get artwork for a track. Returns cached or extracts from file.
    /// Returns defaultArt() if no artwork found.
    QPixmap artForTrack(const QString& filePath, int size = 64);

    /// Pre-cache artwork during library scan (extracts and saves to disk only).
    /// Does NOT populate memory cache — scanning may process thousands of files.
    void cacheArt(const QString& filePath);

    /// Placeholder artwork when no art available.
    static QPixmap defaultArt(int size);

    /// Clear all caches (memory + disk).
    void clearCache();

private:
    QPixmap extractFromFile(const QString& filePath, int size);
    QString diskCachePath(const QString& filePath) const;
    void evictOldest();

    QString m_cacheDir;
    QHash<QString, QPixmap> m_memCache;
    QStringList m_accessOrder;  ///< LRU tracking — most recent at back
    QMutex m_mutex;
    static constexpr int kMaxMemCache = 200;
};

} // namespace M1
