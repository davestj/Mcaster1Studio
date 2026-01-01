#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include "MediaItem.h"
#include "PlaylistConfigDialog.h"
#include <QList>
#include <QTimer>
#include <QDateTime>
#include <QRandomGenerator>

class PlaylistWidget;

namespace M1 {
class DeckPlayer;
class DeckModule;
class LibraryModel;

/// PlaylistModule — Phase 7 Playlist / AutoDJ module.
///
/// Full broadcast-grade AutoDJ with:
///   - 4 strategies: Random, Weighted Random, Category Rotation, Clockwheel
///   - Auto-fill queue from Media Library based on strategy & rules
///   - Auto-play on Deck A when enabled, crossfade on track end
///   - Rotation rules (artist/title separation, recently-played cooldown)
///   - Configurable queue depth (auto-refills to maintain depth)
///   - Clockwheel: hourly broadcast clock pattern
class PlaylistModule : public IModule {
    Q_OBJECT

public:
    explicit PlaylistModule(QObject* parent = nullptr);
    ~PlaylistModule() override;

    // ── IModule identity ────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.playlist"; }
    QString displayName() const override { return "Playlist"; }
    QString version()     const override { return "1.0.0"; }
    QString vendor()      const override { return "Mcaster1"; }

    QSize preferredSize()     const override { return {500, 400}; }
    QSize minimumModuleSize() const override { return {320, 220}; }

    // ── Lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;

    // ── UI ─────────────────────────────────────────────────────────
    QWidget* createWidget(QWidget* parent) override;

    // ── State persistence ───────────────────────────────────────────
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Queue management ────────────────────────────────────────────
    void              addItem(const MediaItem& item);
    void              addFiles(const QStringList& paths);
    void              addPlaylistFile(const QString& path);
    void              insertItem(int index, const MediaItem& item);
    void              removeItem(int index);
    void              moveItem(int from, int to);
    void              clearQueue();
    void              loadNextOnDeck(int deckIndex);

    int               currentIndex()  const { return m_currentIndex; }
    int               queueSize()     const { return m_queue.size(); }
    const QList<MediaItem>& queue()   const { return m_queue; }

    // ── Rotation rules ──────────────────────────────────────────────
    int  artistSeparation()           const { return m_autoDJConfig.artistSeparation; }
    int  titleSeparation()            const { return m_autoDJConfig.titleSeparation; }
    void setArtistSeparation(int n)         { m_autoDJConfig.artistSeparation = n; }
    void setTitleSeparation(int n)          { m_autoDJConfig.titleSeparation  = n; }

    // ── Queue depth alert ────────────────────────────────────────────
    int  minQueueDepth()              const { return m_minQueueDepth; }
    void setMinQueueDepth(int n)            { m_minQueueDepth = std::max(1, n); }

    // ── Library wiring ──────────────────────────────────────────────
    void connectLibrary(M1::LibraryModel* model);

    // ── Deck wiring ─────────────────────────────────────────────────
    void connectDecks(M1::DeckPlayer* a, M1::DeckPlayer* b, M1::DeckModule* dm);

    // ── AutoDJ ──────────────────────────────────────────────────────
    bool autoDJ()                     const { return m_autoDJ; }
    bool isFading()                   const { return m_fading; }
    void setAutoDJ(bool enabled);

    const AutoDJConfig& autoDJConfig() const { return m_autoDJConfig; }
    void setAutoDJConfig(const AutoDJConfig& cfg);

    // ── Playback control ────────────────────────────────────────────
    void playNext();
    void playItemAt(int index);
    void advance();
    void skip();

    /// Auto-fill the queue from the library to reach queueDepth.
    void fillQueueFromLibrary();

    /// Called when CrossfaderModule's AUTO CROSSFADE button completes a fade.
    /// Advances the queue and preloads the next track on the now-idle deck.
    void onAutoFadeCompleted(int stoppedDeck);

    /// Get the current strategy name for display.
    QString strategyName() const;

signals:
    void queueChanged();
    void autoDJChanged(bool enabled);
    void crossfaderAnimateTo(float target, int durationMs);
    void queueLow(int remaining);

private:
    int findNextValidIndex(int startIndex) const;
    bool violatesSeparation(int candidateIndex) const;
    int idleDeck() const;

    /// Pick a track from the library matching the given category.
    /// Returns a MediaItem with empty filePath if nothing found.
    MediaItem pickFromLibrary(const QString& categoryFilter = QString());

    /// Pick a track using weighted random from library.
    MediaItem pickWeightedFromLibrary();

    /// Get the next clockwheel category name.
    QString nextClockwheelCategory();

    /// Check if a track was recently played (within recentPlayedHours).
    bool wasRecentlyPlayed(const MediaItem& item) const;

    /// Preload the next track (per playlist rules) on the idle deck.
    /// Called after each track starts playing so the crossfade target is ready.
    void preloadNextOnIdleDeck();

    /// Remove all played tracks (before m_currentIndex) from the queue.
    void removePlayedTracks();

    QList<MediaItem>    m_queue;
    int                 m_currentIndex     = -1;
    bool                m_autoDJ           = false;
    bool                m_fading           = false;
    int                 m_minQueueDepth    = 3;

    AutoDJConfig        m_autoDJConfig;
    int                 m_cwPosition       = 0;   // clockwheel position
    int                 m_catRotPosition   = 0;   // category rotation position
    QList<QDateTime>    m_recentPlayTimes;         // timestamps of recently played
    QStringList         m_recentPlayPaths;         // file paths of recently played

    M1::LibraryModel*   m_libraryModel = nullptr;
    M1::DeckPlayer*     m_deckA   = nullptr;
    M1::DeckPlayer*     m_deckB   = nullptr;
    M1::DeckModule*     m_deckMod = nullptr;

    QTimer*             m_pollTimer = nullptr;
    PlaylistWidget*     m_widget    = nullptr;

private slots:
    void onPollTimer();
};

} // namespace M1
