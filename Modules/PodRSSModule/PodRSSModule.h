#pragma once
/// @file   PodRSSModule.h
/// @path   Modules/PodRSSModule/PodRSSModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodRSS — Podcast RSS Feed Generator Module
/// @purpose RSS 2.0 + iTunes namespace feed generator with show-level
///          settings, episode management, feed validation, XML preview,
///          and file export.
/// @reason  Podcast distribution requires a valid RSS 2.0 feed with
///          iTunes extensions for directory submission and player apps.
/// @changelog
///   2026-03-09  Initial implementation — RSS 2.0 + iTunes, show/episode management

#include "IModule.h"
#include <QList>
#include <QString>
#include <QDateTime>

namespace M1 {

/// Show-level podcast metadata for RSS feed generation.
struct PodcastShow {
    QString title;
    QString description;
    QString author;
    QString ownerEmail;
    QString category;         ///< iTunes category (e.g. "Technology")
    QString coverArtPath;     ///< Local path to cover image
    QString website;          ///< Podcast website URL
    QString language = "en";  ///< ISO 639 language code
    bool    isExplicit = false;
    QString type = "episodic"; ///< "episodic" or "serial"
};

/// Episode-level metadata for RSS feed generation.
struct PodcastEpisode {
    QString   title;
    QString   description;
    int       number      = 1;
    int       season      = 1;
    QString   type        = "full";    ///< "full", "trailer", or "bonus"
    QDateTime pubDate;
    QString   enclosureUrl;
    qint64    enclosureSize = 0;       ///< File size in bytes
    int       duration      = 0;       ///< Duration in seconds
    QString   guid;
    bool      isExplicit    = false;
    QString   coverArt;                ///< Episode-specific cover art URL
    QString   transcriptUrl;
};

/// PodRSSModule — RSS 2.0 feed generator for podcast distribution.
///
/// Manages show-level configuration and a list of episodes. Generates
/// standards-compliant RSS 2.0 XML with the iTunes podcast namespace
/// (`xmlns:itunes`) for Apple Podcasts, Spotify, and other directories.
///
/// Features:
///   - Show settings form: title, description, author, category, cover art, etc.
///   - Episode table: add, edit, remove episodes
///   - Feed generation with iTunes namespace extensions
///   - Feed validation (returns list of warnings)
///   - XML preview dialog
///   - Export feed XML to file
class PodRSSModule : public IModule {
    Q_OBJECT

public:
    explicit PodRSSModule(QObject* parent = nullptr);
    ~PodRSSModule() override;

    // ── IModule identity ─────────────────────────────────────────────────
    QString moduleId()          const override { return "com.mcaster1.podcast.rss"; }
    QString displayName()       const override { return "RSS Feed"; }
    QString version()           const override { return "1.0.0"; }
    QSize   preferredSize()     const override { return {550, 450}; }
    QSize   minimumModuleSize() const override { return {400, 350}; }

    // ── IModule lifecycle ────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;

    // ── IModule UI ───────────────────────────────────────────────────────
    QWidget* createWidget(QWidget* parent) override;

    // ── IModule state persistence ────────────────────────────────────────
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Show API ─────────────────────────────────────────────────────────
    void        setShow(const PodcastShow& show);
    PodcastShow show() const { return m_show; }

    // ── Episode API ──────────────────────────────────────────────────────
    int  addEpisode(const PodcastEpisode& ep);
    void removeEpisode(int index);
    void updateEpisode(int index, const PodcastEpisode& ep);
    QList<PodcastEpisode> episodes() const { return m_episodes; }

    // ── Feed generation ──────────────────────────────────────────────────
    QString     generateFeed() const;
    bool        exportFeed(const QString& filePath) const;
    QStringList validateFeed() const;

    /// Format duration in seconds to HH:MM:SS string
    static QString formatDuration(int seconds);

signals:
    void showChanged();
    void episodesChanged();

private:
    PodcastShow              m_show;
    QList<PodcastEpisode>    m_episodes;

    /// Escape XML special characters
    static QString xmlEscape(const QString& text);
};

} // namespace M1
