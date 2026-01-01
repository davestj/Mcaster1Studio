#pragma once
/// @file   PodPublisherModule.h
/// @path   Modules/PodPublisherModule/PodPublisherModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodPublisher — Podcast Multi-Target Publishing Module
/// @purpose Multi-target file publisher with configurable profiles for
///          SFTP, FTP, HTTP API, and local copy. Upload queue with progress,
///          simultaneous multi-target upload, and post-upload verification.
/// @reason  Podcast production requires distributing finished episodes to
///          multiple hosting targets (CDN, web server, local backup) in one step.
/// @changelog
///   2026-03-09  Initial implementation — Local Copy target, SFTP/FTP/HTTP stubbed

#include "IModule.h"
#include <QList>
#include <QString>

namespace M1 {

/// Transport type for publishing targets.
enum class PublishType {
    SFTP,
    FTP,
    HTTP,
    Local
};

/// Configuration for a single publishing destination.
struct PublishTarget {
    int         id       = 0;
    QString     name;
    PublishType type     = PublishType::Local;
    QString     host;
    int         port     = 22;
    QString     username;
    QString     password;
    QString     remotePath;    ///< Remote directory or local destination path
    bool        enabled  = true;
};

/// Status of a single publish job.
enum class PublishStatus {
    Queued,
    InProgress,
    Completed,
    Failed,
    Cancelled,
    Unsupported   ///< Target type not yet implemented
};

/// Tracks progress of a file upload to one target.
struct PublishJob {
    int           targetId   = 0;
    QString       localPath;
    QString       remotePath;
    int           progress   = 0;    ///< 0–100 percent
    PublishStatus status     = PublishStatus::Queued;
    QString       statusText;        ///< Human-readable status message
};

/// PodPublisherModule — multi-target podcast file publisher.
///
/// Manages a list of publishing targets (SFTP, FTP, HTTP, Local Copy)
/// and uploads files to all enabled targets simultaneously.
///
/// v1 implementation:
///   - Local Copy target: fully functional (QFile::copy)
///   - SFTP/FTP/HTTP: stubbed ("Coming in v2" status)
///
/// Features:
///   - Target profile management (add/edit/remove)
///   - File selector for source files
///   - Publish button starts upload to all enabled targets
///   - Progress display per target
///   - Post-upload verification (file size check for Local Copy)
///   - Cancel support
class PodPublisherModule : public IModule {
    Q_OBJECT

public:
    explicit PodPublisherModule(QObject* parent = nullptr);
    ~PodPublisherModule() override;

    // ── IModule identity ─────────────────────────────────────────────────
    QString moduleId()          const override { return "com.mcaster1.podcast.publisher"; }
    QString displayName()       const override { return "Publisher"; }
    QString version()           const override { return "1.0.0"; }
    QSize   preferredSize()     const override { return {550, 400}; }
    QSize   minimumModuleSize() const override { return {400, 300}; }

    // ── IModule lifecycle ────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;

    // ── IModule UI ───────────────────────────────────────────────────────
    QWidget* createWidget(QWidget* parent) override;

    // ── IModule state persistence ────────────────────────────────────────
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Target management ────────────────────────────────────────────────
    int  addTarget(const PublishTarget& target);
    void removeTarget(int id);
    void updateTarget(int id, const PublishTarget& target);
    QList<PublishTarget> targets() const { return m_targets; }

    // ── Publishing ───────────────────────────────────────────────────────
    void publish(const QString& localFilePath);
    void cancelPublish();
    bool isPublishing() const { return m_publishing; }
    QList<PublishJob> jobs() const { return m_jobs; }

signals:
    void targetsChanged();
    void publishStarted();
    void publishProgress(int targetId, int percent);
    void publishFinished(int targetId, bool success, const QString& message);
    void allPublishingComplete();

private:
    QList<PublishTarget>  m_targets;
    QList<PublishJob>     m_jobs;
    bool                  m_publishing = false;
    int                   m_nextId     = 1;      ///< Auto-increment target ID

    /// Perform local file copy for a single job.
    void doLocalCopy(PublishJob& job);

    /// Verify uploaded file size matches source.
    bool verifyFile(const QString& srcPath, const QString& dstPath);
};

} // namespace M1
