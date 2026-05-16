#pragma once
#include "MediaItem.h"
#include <QThread>
#include <QStringList>
#include <atomic>

namespace M1 {

class AlbumArtCache;

/// ScanWorker — background thread that scans directories for audio files using TagLib.
///
/// Compiled with /EHa (MSVC) for SEH exception safety on corrupt audio files.
/// NEVER calls TagLib::String::toWString() — uses to8Bit(true) (UTF-8) instead.
///
/// Signals are emitted cross-thread via Qt::QueuedConnection (automatic for cross-thread).
class ScanWorker : public QThread {
    Q_OBJECT

public:
    explicit ScanWorker(QObject* parent = nullptr);
    ~ScanWorker() override;

    void startScan(const QStringList& directories);
    void cancel();

    int itemsScanned() const { return m_itemsScanned.load(); }

    /// Set album art cache for pre-caching artwork during scan.
    void setAlbumArtCache(AlbumArtCache* cache) { m_artCache = cache; }

signals:
    void scanStarted(int estimatedCount);
    void itemScanned(const M1::MediaItem& item);
    void scanProgress(int done, int total);
    void scanFinished(int totalScanned);
    void scanError(const QString& path, const QString& error);

protected:
    void run() override;

private:
    MediaItem readTags(const QString& filePath);
    bool      hasEmbeddedArt(const QString& filePath);

    QStringList           m_directories;
    std::atomic<bool>     m_cancelled{false};
    std::atomic<int>      m_itemsScanned{0};
    AlbumArtCache*        m_artCache = nullptr;

    static const QStringList s_audioExtensions;
};

} // namespace M1
