#include "ScanWorker.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>

// TagLib — compiled with /EHa for SEH-safe corrupt file handling
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

namespace M1 {

const QStringList ScanWorker::s_audioExtensions = {
    "mp3", "flac", "wav", "aif", "aiff",
    "ogg", "opus", "m4a", "aac", "wv", "ape", "mpc"
};

ScanWorker::ScanWorker(QObject* parent) : QThread(parent) {}

ScanWorker::~ScanWorker() {
    cancel();
    wait(3000);
}

void ScanWorker::startScan(const QStringList& directories) {
    if (isRunning()) {
        cancel();
        wait(2000);
    }
    m_directories = directories;
    m_cancelled.store(false);
    m_itemsScanned.store(0);
    start(QThread::LowPriority);
}

void ScanWorker::cancel() {
    m_cancelled.store(true);
}

void ScanWorker::run() {
    // Pass 1: count audio files for progress bar
    int total = 0;
    for (const QString& dir : m_directories) {
        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (s_audioExtensions.contains(
                    QFileInfo(it.filePath()).suffix().toLower()))
                ++total;
        }
    }

    emit scanStarted(total);
    qInfo() << "[ScanWorker] Starting scan of" << m_directories << "—" << total << "estimated files";

    // Pass 2: read tags and emit items
    int done = 0;
    for (const QString& dir : m_directories) {
        if (m_cancelled.load()) break;

        QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext() && !m_cancelled.load()) {
            it.next();
            const QString path = it.filePath();
            if (!s_audioExtensions.contains(
                    QFileInfo(path).suffix().toLower()))
                continue;

            ++done;
            MediaItem item = readTags(path);
            if (!item.filePath.isEmpty()) {
                m_itemsScanned.fetch_add(1);
                emit itemScanned(item);
            } else {
                emit scanError(path, "Could not read file");
            }

            if (done % 20 == 0)
                emit scanProgress(done, total);
        }
    }

    emit scanProgress(done, total);
    emit scanFinished(m_itemsScanned.load());
    qInfo() << "[ScanWorker] Scan complete —" << m_itemsScanned.load() << "items read";
}

MediaItem ScanWorker::readTags(const QString& filePath) {
    MediaItem item;
    item.filePath = filePath;

    QFileInfo fi(filePath);
    item.fileSize     = fi.size();
    item.dateModified = fi.lastModified();
    item.dateAdded    = QDateTime::currentDateTime();
    item.codec        = fi.suffix().toLower();

    // TagLib read — catch all exceptions (including SEH on MSVC with /EHa)
    try {
        // Use local8Bit path for TagLib (TagLib accepts UTF-8 on most platforms)
        TagLib::FileRef fileRef(filePath.toLocal8Bit().constData());

        if (!fileRef.isNull() && fileRef.tag()) {
            TagLib::Tag* tag = fileRef.tag();

            // NEVER call tag->title().toWString() — use to8Bit(true) [UTF-8] instead.
            // This is safe across DLL boundaries.
            item.title       = QString::fromStdString(tag->title().to8Bit(true));
            item.artist      = QString::fromStdString(tag->artist().to8Bit(true));
            item.album       = QString::fromStdString(tag->album().to8Bit(true));
            item.genre       = QString::fromStdString(tag->genre().to8Bit(true));
            item.comment     = QString::fromStdString(tag->comment().to8Bit(true));

            if (tag->year()  > 0) item.year        = QString::number(tag->year());
            if (tag->track() > 0) item.trackNumber  = QString::number(tag->track());
        }

        if (!fileRef.isNull() && fileRef.audioProperties()) {
            TagLib::AudioProperties* ap = fileRef.audioProperties();
            item.durationMs = ap->lengthInMilliseconds();
            item.bitrate    = ap->bitrate();
            item.sampleRate = ap->sampleRate();
            item.channels   = ap->channels();
        }
    } catch (...) {
        // Corrupt or unreadable file — clear filePath to signal failure
        qWarning() << "[ScanWorker] Exception reading:" << filePath;
        item.filePath.clear();
    }

    return item;
}

} // namespace M1
