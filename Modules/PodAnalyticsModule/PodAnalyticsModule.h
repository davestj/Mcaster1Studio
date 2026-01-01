#pragma once
/// @file   PodAnalyticsModule.h
/// @path   Modules/PodAnalyticsModule/PodAnalyticsModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodAnalytics — Podcast Analytics Dashboard Module
/// @purpose Performance dashboard for podcast episodes: tracks downloads,
///          listens, subscribers, growth trends. Manual data entry in v1,
///          API integration stubbed for v2.
/// @reason  Podcasters need visibility into episode performance to make
///          data-driven content decisions.
/// @changelog
///   2026-03-09  Initial implementation — manual entry, bar chart, CSV export

#include "IModule.h"
#include <QList>
#include <QString>

namespace M1 {

struct EpisodeStats {
    QString episodeTitle;
    int     episodeNumber  = 0;
    QString publishDate;       // ISO 8601 date string
    int     downloads      = 0;
    int     listens        = 0;
    int     subscribersAtTime = 0;
};

class PodAnalyticsModule : public IModule {
    Q_OBJECT

public:
    explicit PodAnalyticsModule(QObject* parent = nullptr);
    ~PodAnalyticsModule() override = default;

    QString moduleId()    const override { return "com.mcaster1.podcast.analytics"; }
    QString displayName() const override { return "Podcast Analytics"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {600, 400}; }
    QSize minimumModuleSize() const override { return {450, 300}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Episode stats API
    void addEpisodeStats(const EpisodeStats& stats);
    void removeEpisodeStats(int index);
    QList<EpisodeStats> allStats() const { return m_stats; }

    // Aggregate metrics
    int totalDownloads()   const;
    int averageDownloads() const;
    int subscriberCount()  const;

    // CSV export
    bool exportCsv(const QString& filePath);

signals:
    void statsChanged();

private:
    QList<EpisodeStats> m_stats;
};

} // namespace M1
