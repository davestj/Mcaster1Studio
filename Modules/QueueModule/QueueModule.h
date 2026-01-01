#pragma once
#include "IModule.h"
#include "MediaItem.h"
#include <QList>
#include <QSettings>

namespace M1 { class DeckPlayer; }

/// QueueModule — Simple file queue for a single DeckPlayer.
///
/// Manages an ordered playback queue of audio files. No AutoDJ,
/// no crossfader animation, no rotation rules. Designed to work
/// with the combined DeckModule's DeckPlayer (not standalone
/// Deck A / Deck B).
///
/// For full AutoDJ with crossfading and rotation rules, use
/// PlaylistModule instead.
class QueueModule : public M1::IModule {
    Q_OBJECT

public:
    explicit QueueModule(QObject* parent = nullptr);
    ~QueueModule() override;

    // ─── IModule identity ────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.queue"; }
    QString displayName() const override { return "Queue"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {340, 400}; }
    QSize minimumModuleSize() const override { return {260, 260}; }

    QWidget* createWidget(QWidget* parent) override;
    void initialize() override {}
    void shutdown() override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ─── Deck wiring ─────────────────────────────────────────────
    /// Connect to a single DeckPlayer (from combined DeckModule).
    void connectDeck(M1::DeckPlayer* player);

    // ─── Queue management ─────────────────────────────────────────
    void addFiles(const QStringList& paths);
    void addItem(const M1::MediaItem& item);
    void addPlaylistFile(const QString& path);
    void removeItem(int index);
    void moveItem(int from, int to);
    void clearQueue();
    void playNow(int index);
    void loadNextOnDeck(int deckIndex);   ///< Load next queued item (deckIndex ignored; uses connected deck)

    const QList<M1::MediaItem>& queue() const { return m_queue; }
    int  currentIndex() const { return m_currentIndex; }
    int  queueSize()    const { return m_queue.size(); }

signals:
    void queueChanged();

private:
    QList<M1::MediaItem>  m_queue;
    int                   m_currentIndex = -1;
    M1::DeckPlayer*       m_deck = nullptr;
};
