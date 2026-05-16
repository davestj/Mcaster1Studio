#include "AlbumArtCache.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QDebug>

// TagLib — compiled with /EHa for SEH-safe corrupt file handling
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/mp4file.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/mp4coverart.h>

namespace M1 {

AlbumArtCache::AlbumArtCache(const QString& cacheDir, QObject* parent)
    : QObject(parent)
    , m_cacheDir(cacheDir)
{
    QDir dir(m_cacheDir);
    if (!dir.exists())
        dir.mkpath(".");

    qInfo() << "[AlbumArtCache] Disk cache dir:" << m_cacheDir;
}

QPixmap AlbumArtCache::artForTrack(const QString& filePath, int size) {
    QMutexLocker locker(&m_mutex);

    const QString key = filePath + "|" + QString::number(size);

    // --- Tier 1: Memory cache ---
    if (m_memCache.contains(key)) {
        // Move to back of LRU list (most recently used)
        m_accessOrder.removeOne(key);
        m_accessOrder.append(key);
        return m_memCache.value(key);
    }

    // --- Tier 2: Disk cache ---
    const QString diskPath = diskCachePath(filePath);
    if (QFile::exists(diskPath)) {
        QPixmap pm;
        if (pm.load(diskPath, "JPEG")) {
            QPixmap scaled = pm.scaled(size, size, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
            // Store in memory cache
            if (m_memCache.size() >= kMaxMemCache)
                evictOldest();
            m_memCache.insert(key, scaled);
            m_accessOrder.append(key);
            return scaled;
        }
    }

    // --- Tier 3: Extract from file via TagLib ---
    QPixmap art = extractFromFile(filePath, size);

    if (art.isNull()) {
        // Return default artwork — do NOT cache placeholder in memory
        return defaultArt(size);
    }

    // Save to disk cache (full resolution before scaling)
    {
        QPixmap fullArt = extractFromFile(filePath, 0); // 0 = no scaling
        if (!fullArt.isNull()) {
            QImage img = fullArt.toImage();
            img.save(diskPath, "JPEG", 85);
        }
    }

    // Store scaled version in memory cache
    if (m_memCache.size() >= kMaxMemCache)
        evictOldest();
    m_memCache.insert(key, art);
    m_accessOrder.append(key);

    return art;
}

void AlbumArtCache::cacheArt(const QString& filePath) {
    // Disk-only caching for library scans — do NOT populate memory cache
    const QString diskPath = diskCachePath(filePath);
    if (QFile::exists(diskPath))
        return;  // Already cached on disk

    QPixmap art = extractFromFile(filePath, 0);  // 0 = no scaling
    if (!art.isNull()) {
        QImage img = art.toImage();
        img.save(diskPath, "JPEG", 85);
    }
}

QPixmap AlbumArtCache::defaultArt(int size) {
    QPixmap pm(size, size);
    pm.fill(QColor(60, 60, 60));

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Draw a simple music note icon
    QFont font;
    font.setPixelSize(size * 0.55);
    font.setFamily("Segoe UI Symbol");
    p.setFont(font);
    p.setPen(QColor(180, 180, 180));
    p.drawText(pm.rect(), Qt::AlignCenter, QChar(0x266B));  // ♫

    p.end();
    return pm;
}

void AlbumArtCache::clearCache() {
    QMutexLocker locker(&m_mutex);

    // Clear memory cache
    m_memCache.clear();
    m_accessOrder.clear();

    // Clear disk cache
    QDir dir(m_cacheDir);
    const QStringList files = dir.entryList({"*.jpg"}, QDir::Files);
    for (const QString& f : files)
        dir.remove(f);

    qInfo() << "[AlbumArtCache] All caches cleared";
}

QPixmap AlbumArtCache::extractFromFile(const QString& filePath, int size) {
    QByteArray artData;

    try {
        const QByteArray pathUtf8 = filePath.toLocal8Bit();
        const QString ext = QFileInfo(filePath).suffix().toLower();

        // --- MP3: ID3v2 APIC frame ---
        if (ext == "mp3") {
            TagLib::MPEG::File mpegFile(pathUtf8.constData());
            if (mpegFile.isValid() && mpegFile.ID3v2Tag()) {
                TagLib::ID3v2::FrameList frames =
                    mpegFile.ID3v2Tag()->frameListMap()["APIC"];
                if (!frames.isEmpty()) {
                    auto* apic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(
                        frames.front());
                    if (apic) {
                        const TagLib::ByteVector& bv = apic->picture();
                        artData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                    }
                }
            }
        }
        // --- FLAC: pictureList ---
        else if (ext == "flac") {
            TagLib::FLAC::File flacFile(pathUtf8.constData());
            if (flacFile.isValid()) {
                const TagLib::List<TagLib::FLAC::Picture*> pics = flacFile.pictureList();
                if (!pics.isEmpty()) {
                    const TagLib::ByteVector& bv = pics.front()->data();
                    artData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                }
            }
        }
        // --- M4A/AAC: MP4 cover art ---
        else if (ext == "m4a" || ext == "aac" || ext == "mp4") {
            TagLib::MP4::File mp4File(pathUtf8.constData());
            if (mp4File.isValid() && mp4File.tag()) {
                const TagLib::MP4::ItemMap& items = mp4File.tag()->itemMap();
                if (items.contains("covr")) {
                    const TagLib::MP4::CoverArtList covers =
                        items["covr"].toCoverArtList();
                    if (!covers.isEmpty()) {
                        const TagLib::ByteVector& bv = covers.front().data();
                        artData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                    }
                }
            }
        }
        // --- Generic fallback: try FileRef + ID3v2 ---
        else {
            TagLib::FileRef fileRef(pathUtf8.constData());
            if (!fileRef.isNull()) {
                // Try to get as MPEG file for ID3v2
                auto* mpeg = dynamic_cast<TagLib::MPEG::File*>(fileRef.file());
                if (mpeg && mpeg->ID3v2Tag()) {
                    TagLib::ID3v2::FrameList frames =
                        mpeg->ID3v2Tag()->frameListMap()["APIC"];
                    if (!frames.isEmpty()) {
                        auto* apic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(
                            frames.front());
                        if (apic) {
                            const TagLib::ByteVector& bv = apic->picture();
                            artData = QByteArray(bv.data(), static_cast<int>(bv.size()));
                        }
                    }
                }
            }
        }
    } catch (...) {
        qWarning() << "[AlbumArtCache] Exception extracting art from:" << filePath;
        return {};
    }

    if (artData.isEmpty())
        return {};

    QPixmap pm;
    if (!pm.loadFromData(artData))
        return {};

    if (size > 0) {
        pm = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return pm;
}

QString AlbumArtCache::diskCachePath(const QString& filePath) const {
    QByteArray hash = QCryptographicHash::hash(
        filePath.toUtf8(), QCryptographicHash::Sha256);
    return m_cacheDir + "/" + hash.toHex() + ".jpg";
}

void AlbumArtCache::evictOldest() {
    if (m_accessOrder.isEmpty())
        return;

    // Remove least recently used entry
    const QString oldest = m_accessOrder.takeFirst();
    m_memCache.remove(oldest);
}

} // namespace M1
