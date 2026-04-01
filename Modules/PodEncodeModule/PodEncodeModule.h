#pragma once
/// @file   PodEncodeModule.h
/// @path   Modules/PodEncodeModule/PodEncodeModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodEncode — Podcast Encoding and ID3 Tagging Module
/// @purpose Format conversion (MP3/AAC/OGG/FLAC/Opus) with ID3 metadata
///          injection. Supports batch encoding to multiple formats. Encoding
///          is stubbed for v1 — the UI and data model are production-ready.
/// @reason  Podcast producers need to export finished episodes in multiple
///          delivery formats with embedded ID3 metadata for distribution
///          on Apple Podcasts, Spotify, etc.
/// @changelog
///   2026-03-09  Initial implementation — UI, job model, ID3 form, batch queue

#include "IModule.h"
#include "IThreadPoolAware.h"
#include <QList>
#include <QString>
#include <QMap>

namespace M1 {

class SurfaceThreadPool;

// ─── OutputFormat ────────────────────────────────────────────────────────────
enum class PodEncodeFormat {
    MP3,
    AAC,
    OGG,
    FLAC,
    Opus
};

// ─── EncodeJob status ────────────────────────────────────────────────────────
enum class EncodeJobStatus {
    Pending,
    Running,
    Complete,
    Error
};

// ─── EncodeJob ───────────────────────────────────────────────────────────────
struct EncodeJob {
    int              id          = 0;
    QString          inputPath;
    QString          outputPath;
    PodEncodeFormat  format      = PodEncodeFormat::MP3;
    int              bitrate     = 192;       ///< kbps
    int              sampleRate  = 48000;     ///< Hz
    int              channels    = 2;         ///< 1=mono, 2=stereo
    int              progress    = 0;         ///< 0–100
    EncodeJobStatus  status      = EncodeJobStatus::Pending;
    QString          errorMsg;

    // ID3 metadata (shared from module, can be overridden per-job)
    QString          title;
    int              episodeNumber = 0;
    int              seasonNumber  = 1;
    QString          author;
    QString          description;
    QString          genre;
    QString          coverArtPath;
    QString          copyright;
    QString          websiteUrl;
};

// ─── PodEncodeModule ─────────────────────────────────────────────────────────
class PodEncodeModule : public IModule, public IThreadPoolAware {
    Q_OBJECT

public:
    explicit PodEncodeModule(QObject* parent = nullptr);
    ~PodEncodeModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.podcast.encode"; }
    QString displayName() const override { return "Podcast Encoder"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {550, 450}; }
    QSize minimumModuleSize() const override { return {400, 350}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Input file ──────────────────────────────────────────────────────────
    void    setInputFile(const QString& path);
    QString inputFile() const { return m_inputPath; }

    // ── Job management ──────────────────────────────────────────────────────
    int  addJob(PodEncodeFormat format, int bitrate, const QString& outputPath);
    void removeJob(int id);
    QList<EncodeJob> jobs() const { return m_jobs; }
    void clearJobs();

    // ── Encoding control ────────────────────────────────────────────────────
    void startEncoding();
    void cancelEncoding();
    bool isEncoding() const { return m_encoding; }

    // ── ID3 metadata setters (apply to all future jobs) ─────────────────────
    void setId3Title(const QString& title)       { m_id3Title = title; }
    void setId3EpisodeNumber(int num)             { m_id3Episode = num; }
    void setId3SeasonNumber(int num)              { m_id3Season = num; }
    void setId3Author(const QString& author)     { m_id3Author = author; }
    void setId3Description(const QString& desc)  { m_id3Description = desc; }
    void setId3Genre(const QString& genre)       { m_id3Genre = genre; }
    void setId3CoverArtPath(const QString& path) { m_id3CoverArt = path; }
    void setId3Copyright(const QString& copy)    { m_id3Copyright = copy; }
    void setId3WebsiteUrl(const QString& url)    { m_id3Website = url; }

    // ── ID3 metadata getters ────────────────────────────────────────────────
    QString id3Title()       const { return m_id3Title; }
    int     id3EpisodeNumber() const { return m_id3Episode; }
    int     id3SeasonNumber()  const { return m_id3Season; }
    QString id3Author()      const { return m_id3Author; }
    QString id3Description() const { return m_id3Description; }
    QString id3Genre()       const { return m_id3Genre; }
    QString id3CoverArtPath() const { return m_id3CoverArt; }
    QString id3Copyright()   const { return m_id3Copyright; }
    QString id3WebsiteUrl()  const { return m_id3Website; }

    // ── LUFS normalization ──────────────────────────────────────────────────
    void   setLufsTarget(double lufs) { m_lufsTarget = lufs; }
    double lufsTarget() const { return m_lufsTarget; }

    // ── Format helper ───────────────────────────────────────────────────────
    static QString formatName(PodEncodeFormat fmt);
    static QString formatExtension(PodEncodeFormat fmt);

signals:
    void inputFileChanged(const QString& path);
    void jobAdded(int jobId);
    void jobRemoved(int jobId);
    void jobProgressChanged(int jobId, int percent);
    void jobStatusChanged(int jobId, int status);
    void encodingStarted();
    void encodingFinished();
    void encodeError(const QString& msg);

private:
    // Input file
    QString m_inputPath;

    // Job list
    QList<EncodeJob> m_jobs;
    int              m_nextJobId = 1;
    bool             m_encoding  = false;

    // ID3 metadata (shared defaults for new jobs)
    QString m_id3Title;
    int     m_id3Episode    = 0;
    int     m_id3Season     = 1;
    QString m_id3Author;
    QString m_id3Description;
    QString m_id3Genre      = "Podcast";
    QString m_id3CoverArt;
    QString m_id3Copyright;
    QString m_id3Website;

    // LUFS normalization target
    double m_lufsTarget = -16.0;

    // ── IThreadPoolAware ─────────────────────────────────────────────────────
    void setSurfaceThreadPool(SurfaceThreadPool* pool) override { m_pool = pool; }
    SurfaceThreadPool* surfaceThreadPool() const override { return m_pool; }
    SurfaceThreadPool* m_pool = nullptr;

    // ── Helpers ─────────────────────────────────────────────────────────────
    void applyId3ToJob(EncodeJob& job);
    void encodeJobStub(EncodeJob& job);
};

} // namespace M1
