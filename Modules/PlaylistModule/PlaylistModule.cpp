#include "PlaylistModule.h"
#include "PlaylistWidget.h"
#include "LibraryModel.h"
#include "DeckPlayer.h"
#include "IPlugin.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <algorithm>
#include <cmath>

// ─── M3U/PLS playlist parser ─────────────────────────────────────────────────
static QStringList parseM3U(const QString& path) {
    QStringList result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    const QDir dir(QFileInfo(path).absoluteDir());
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        result << (QFileInfo(line).isAbsolute() ? line : dir.filePath(line));
    }
    return result;
}

static QStringList parsePLS(const QString& path) {
    QStringList result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    const QDir dir(QFileInfo(path).absoluteDir());
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith("File", Qt::CaseInsensitive) && line.contains('=')) {
            const QString filePath = line.mid(line.indexOf('=') + 1).trimmed();
            if (!filePath.isEmpty())
                result << (QFileInfo(filePath).isAbsolute() ? filePath : dir.filePath(filePath));
        }
    }
    return result;
}

static const QStringList kAudioExts =
    {"mp3","flac","wav","aif","aiff","ogg","opus","m4a","aac","wv","ape","wma"};

namespace M1 {

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

PlaylistModule::PlaylistModule(QObject* parent)
    : IModule(parent)
{
    m_autoDJConfig = AutoDJConfig::defaults();
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout, this, &PlaylistModule::onPollTimer);
}

PlaylistModule::~PlaylistModule()
{
    shutdown();
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::initialize()
{
    qInfo() << "[PlaylistModule] Initialized.";
}

void PlaylistModule::shutdown()
{
    if (m_pollTimer) m_pollTimer->stop();
    qInfo() << "[PlaylistModule] Shutdown.";
}

// ─────────────────────────────────────────────────────────────────────────────
// Library wiring
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::connectLibrary(M1::LibraryModel* model)
{
    m_libraryModel = model;
    qInfo() << "[PlaylistModule] Library connected:"
            << (model ? QString::number(model->itemCount()) + " tracks" : "null");
}

// ─────────────────────────────────────────────────────────────────────────────
// Deck wiring
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::connectDecks(M1::DeckPlayer* a, M1::DeckPlayer* b, M1::DeckModule* dm)
{
    m_deckA   = a;
    m_deckB   = b;
    m_deckMod = dm;
    qInfo() << "[PlaylistModule] connectDecks: A=" << (a ? "YES" : "null")
            << "B=" << (b ? "YES" : "null")
            << "DeckMod=" << (dm ? "YES" : "null");
    if (m_autoDJ && m_deckA && m_deckB) {
        m_pollTimer->start();
        // If AutoDJ was enabled before decks were wired, kick off playback now
        if (!m_queue.isEmpty() && m_currentIndex < 0) {
            m_currentIndex = 0;
            playNext();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UI
// ─────────────────────────────────────────────────────────────────────────────

QWidget* PlaylistModule::createWidget(QWidget* parent)
{
    m_widget = new PlaylistWidget(this, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    return m_widget;
}

// ─────────────────────────────────────────────────────────────────────────────
// Queue management
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::addItem(const MediaItem& item)
{
    m_queue.append(item);
    if (m_currentIndex < 0 && !m_queue.isEmpty())
        m_currentIndex = 0;
    emit queueChanged();
    emit statusChanged(QString("%1 tracks | AutoDJ: %2 | %3")
                           .arg(m_queue.size())
                           .arg(m_autoDJ ? "ON" : "OFF")
                           .arg(strategyName()));

    const int remaining = (int)m_queue.size() - std::max(0, m_currentIndex);
    if (remaining <= m_minQueueDepth)
        emit queueLow(remaining);
}

void PlaylistModule::addFiles(const QStringList& paths)
{
    for (const QString& path : paths) {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "m3u" || ext == "m3u8" || ext == "pls") {
            addPlaylistFile(path);
            continue;
        }
        if (!kAudioExts.contains(ext)) continue;
        MediaItem item;
        item.filePath = path;
        item.title    = QFileInfo(path).completeBaseName();
        addItem(item);
    }
}

void PlaylistModule::addPlaylistFile(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    QStringList entries;
    if (ext == "m3u" || ext == "m3u8")
        entries = parseM3U(path);
    else if (ext == "pls")
        entries = parsePLS(path);
    else
        entries << path;
    addFiles(entries);
}

void PlaylistModule::insertItem(int index, const MediaItem& item)
{
    index = std::clamp(index, 0, (int)m_queue.size());
    m_queue.insert(index, item);
    if (m_currentIndex >= 0 && index <= m_currentIndex)
        ++m_currentIndex;
    if (m_currentIndex < 0 && !m_queue.isEmpty())
        m_currentIndex = 0;
    emit queueChanged();
}

void PlaylistModule::removeItem(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;
    m_queue.removeAt(index);
    if (m_queue.isEmpty()) {
        m_currentIndex = -1;
    } else if (index < m_currentIndex) {
        --m_currentIndex;
    } else if (m_currentIndex >= m_queue.size()) {
        m_currentIndex = m_queue.size() - 1;
    }
    emit queueChanged();
}

void PlaylistModule::moveItem(int from, int to)
{
    if (from < 0 || from >= m_queue.size()) return;
    if (to   < 0 || to   >= m_queue.size()) return;
    if (from == to) return;
    m_queue.move(from, to);
    if (m_currentIndex == from) {
        m_currentIndex = to;
    } else if (from < to) {
        if (m_currentIndex > from && m_currentIndex <= to)
            --m_currentIndex;
    } else {
        if (m_currentIndex >= to && m_currentIndex < from)
            ++m_currentIndex;
    }
    emit queueChanged();
}

void PlaylistModule::clearQueue()
{
    m_queue.clear();
    m_currentIndex = -1;
    emit queueChanged();
    emit statusChanged(QString("0 tracks | AutoDJ: %1").arg(m_autoDJ ? "ON" : "OFF"));
}

void PlaylistModule::loadNextOnDeck(int deckIndex)
{
    const int next = findNextValidIndex(m_currentIndex + 1);
    if (next < 0 || next >= (int)m_queue.size()) return;
    M1::DeckPlayer* player = (deckIndex == 0) ? m_deckA : m_deckB;
    if (!player) return;
    player->loadFile(m_queue[next].filePath);
    m_currentIndex = next;
    emit queueChanged();
    const int remaining = (int)m_queue.size() - m_currentIndex;
    if (remaining <= m_minQueueDepth)
        emit queueLow(remaining);
}

// ─────────────────────────────────────────────────────────────────────────────
// AutoDJ config
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::setAutoDJConfig(const AutoDJConfig& cfg)
{
    m_autoDJConfig = cfg;
    // Sync the separation fields exposed as top-level getters
    qInfo() << "[PlaylistModule] AutoDJ config updated: strategy="
            << (int)cfg.strategy << "queueDepth=" << cfg.queueDepth;
}

QString PlaylistModule::strategyName() const
{
    switch (m_autoDJConfig.strategy) {
    case AutoDJStrategy::Random:           return "Random";
    case AutoDJStrategy::WeightedRandom:   return "Weighted";
    case AutoDJStrategy::CategoryRotation: return "Category";
    case AutoDJStrategy::Clockwheel:       return "Clockwheel";
    }
    return "Random";
}

// ─────────────────────────────────────────────────────────────────────────────
// AutoDJ — enable/disable
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::setAutoDJ(bool enabled)
{
    if (m_autoDJ == enabled)
        return;
    m_autoDJ = enabled;

    if (enabled) {
        // Fill queue from library
        fillQueueFromLibrary();

        if (m_deckA && m_deckB)
            m_pollTimer->start();

        // Auto-start: load and play the first track immediately
        if (!m_queue.isEmpty()) {
            if (m_currentIndex < 0) m_currentIndex = 0;
            playNext();
        }
    } else {
        m_pollTimer->stop();
    }

    emit autoDJChanged(enabled);
    emit statusChanged(QString("%1 tracks | AutoDJ: %2 | %3")
                           .arg(m_queue.size())
                           .arg(m_autoDJ ? "ON" : "OFF")
                           .arg(strategyName()));
    qInfo() << "[PlaylistModule] AutoDJ" << (enabled ? "ON" : "OFF")
            << "strategy:" << strategyName()
            << "queue:" << m_queue.size() << "tracks";
}

// ─────────────────────────────────────────────────────────────────────────────
// AutoDJ — fill queue from library
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::fillQueueFromLibrary()
{
    if (!m_libraryModel || m_libraryModel->itemCount() == 0) {
        qWarning() << "[PlaylistModule] Cannot fill queue: no library connected or library empty.";
        return;
    }

    const int target = m_autoDJConfig.queueDepth;
    const int remaining = (int)m_queue.size() - std::max(0, m_currentIndex);
    const int needed = target - remaining;

    if (needed <= 0) return;

    qInfo() << "[PlaylistModule] Filling queue: need" << needed << "tracks, strategy:" << strategyName();

    for (int i = 0; i < needed; ++i) {
        MediaItem item;

        switch (m_autoDJConfig.strategy) {
        case AutoDJStrategy::Random:
            item = pickFromLibrary();
            break;

        case AutoDJStrategy::WeightedRandom:
            item = pickWeightedFromLibrary();
            break;

        case AutoDJStrategy::CategoryRotation: {
            const auto& cats = m_autoDJConfig.categories;
            if (cats.isEmpty()) {
                item = pickFromLibrary();
            } else {
                const int idx = m_catRotPosition % cats.size();
                item = pickFromLibrary(cats[idx].genreFilter);
                ++m_catRotPosition;
            }
            break;
        }

        case AutoDJStrategy::Clockwheel: {
            const QString cat = nextClockwheelCategory();
            item = pickFromLibrary(cat);
            break;
        }
        }

        if (item.filePath.isEmpty()) {
            // Fallback: pick anything
            item = pickFromLibrary();
        }

        if (!item.filePath.isEmpty()) {
            m_queue.append(item);
        }
    }

    if (m_currentIndex < 0 && !m_queue.isEmpty())
        m_currentIndex = 0;

    emit queueChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Track selection from library
// ─────────────────────────────────────────────────────────────────────────────

MediaItem PlaylistModule::pickFromLibrary(const QString& categoryFilter)
{
    if (!m_libraryModel || m_libraryModel->itemCount() == 0)
        return {};

    const int count = m_libraryModel->itemCount();

    // Build candidate list matching the filter
    QList<int> candidates;
    candidates.reserve(count);

    for (int i = 0; i < count; ++i) {
        const MediaItem& it = m_libraryModel->itemAt(i);

        // Genre filter
        if (!categoryFilter.isEmpty()) {
            if (!it.genre.contains(categoryFilter, Qt::CaseInsensitive))
                continue;
        }

        // Skip if already in queue
        bool inQueue = false;
        for (const auto& q : m_queue) {
            if (q.filePath == it.filePath) { inQueue = true; break; }
        }
        if (inQueue) continue;

        // Skip recently played
        if (m_autoDJConfig.avoidRecentlyPlayed && wasRecentlyPlayed(it))
            continue;

        candidates.append(i);
    }

    if (candidates.isEmpty()) {
        // Relax: allow already-in-queue but still respect recently-played
        for (int i = 0; i < count; ++i) {
            if (!categoryFilter.isEmpty()) {
                if (!m_libraryModel->itemAt(i).genre.contains(categoryFilter, Qt::CaseInsensitive))
                    continue;
            }
            candidates.append(i);
        }
    }

    if (candidates.isEmpty()) {
        // Final fallback: any track
        for (int i = 0; i < count; ++i)
            candidates.append(i);
    }

    if (candidates.isEmpty())
        return {};

    // Random pick
    const int pick = candidates[QRandomGenerator::global()->bounded(candidates.size())];
    return m_libraryModel->itemAt(pick);
}

MediaItem PlaylistModule::pickWeightedFromLibrary()
{
    if (!m_libraryModel || m_libraryModel->itemCount() == 0)
        return {};

    const int count = m_libraryModel->itemCount();

    // Build weighted candidate list
    QList<int> candidates;
    QList<double> weights;
    double totalWeight = 0.0;

    for (int i = 0; i < count; ++i) {
        const MediaItem& it = m_libraryModel->itemAt(i);

        // Skip if already in queue
        bool inQueue = false;
        for (const auto& q : m_queue) {
            if (q.filePath == it.filePath) { inQueue = true; break; }
        }
        if (inQueue) continue;

        if (m_autoDJConfig.avoidRecentlyPlayed && wasRecentlyPlayed(it))
            continue;

        // Use MediaItem::weight (default 1.0) + rating boost
        double w = std::max(0.1, it.weight);
        if (it.rating > 0) w *= (1.0 + it.rating * 0.2);  // 5-star = 2x boost

        candidates.append(i);
        weights.append(w);
        totalWeight += w;
    }

    if (candidates.isEmpty())
        return pickFromLibrary(); // fallback

    // Weighted random selection
    double roll = QRandomGenerator::global()->generateDouble() * totalWeight;
    for (int i = 0; i < candidates.size(); ++i) {
        roll -= weights[i];
        if (roll <= 0.0)
            return m_libraryModel->itemAt(candidates[i]);
    }
    return m_libraryModel->itemAt(candidates.last());
}

QString PlaylistModule::nextClockwheelCategory()
{
    const auto& cw = m_autoDJConfig.clockwheel;
    if (cw.isEmpty()) return {};

    // Map clockwheel category name to its genreFilter
    const QString catName = cw[m_cwPosition % cw.size()];
    ++m_cwPosition;

    // Find the category definition to get its genre filter
    for (const auto& cat : m_autoDJConfig.categories) {
        if (cat.name.compare(catName, Qt::CaseInsensitive) == 0)
            return cat.genreFilter;
    }
    return catName; // fallback: use name as genre filter
}

bool PlaylistModule::wasRecentlyPlayed(const MediaItem& item) const
{
    if (!m_autoDJConfig.avoidRecentlyPlayed)
        return false;

    return m_recentPlayPaths.contains(item.filePath);
}

// ─────────────────────────────────────────────────────────────────────────────
// Preload next track on idle deck
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::preloadNextOnIdleDeck()
{
    if (!m_autoDJ || m_queue.isEmpty()) return;
    if (!m_deckA || !m_deckB) return;

    const int idle = idleDeck();
    if (idle < 0) return;

    // Find the next valid track after current
    int next = findNextValidIndex(m_currentIndex + 1);
    if (next < 0) {
        fillQueueFromLibrary();
        next = findNextValidIndex(m_currentIndex + 1);
        if (next < 0) return;
    }

    const MediaItem& item = m_queue[next];

    // Check if already loaded on the idle deck
    M1::DeckPlayer* idleDeckPlayer = (idle == 0) ? m_deckA : m_deckB;
    if (idleDeckPlayer->loadedPath() == item.filePath)
        return;  // Already preloaded

    qInfo() << "[PlaylistModule] Preloading next on Deck" << (idle == 0 ? "A" : "B")
            << ":" << item.displayTitle();
    emit requestLoadMedia(item, idle);
    // Do NOT auto-play — just load and wait for crossfade trigger
}

// ─────────────────────────────────────────────────────────────────────────────
// Playback control
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::playNext()
{
    if (m_queue.isEmpty() || m_currentIndex < 0 || m_currentIndex >= (int)m_queue.size())
        return;

    const MediaItem& item = m_queue[m_currentIndex];
    const int deck = idleDeck() >= 0 ? idleDeck() : 0;
    qInfo() << "[PlaylistModule] Loading to Deck" << (deck == 0 ? "A" : "B")
            << ":" << item.displayTitle();
    emit requestLoadMedia(item, deck);

    // Track recently played
    m_recentPlayPaths.append(item.filePath);
    m_recentPlayTimes.append(QDateTime::currentDateTime());
    // Trim old entries
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(
        -m_autoDJConfig.recentPlayedHours * 3600);
    while (!m_recentPlayTimes.isEmpty() && m_recentPlayTimes.first() < cutoff) {
        m_recentPlayTimes.removeFirst();
        m_recentPlayPaths.removeFirst();
    }

    // Auto-play the target deck once loading completes, then preload next.
    // Use a short delay to ensure the file load has started before we connect.
    M1::DeckPlayer* targetDeck = (deck == 0) ? m_deckA : m_deckB;
    if (targetDeck) {
        // If deck is already Ready (fast load / cached), play immediately
        if (targetDeck->state() == M1::DeckPlayer::State::Ready) {
            targetDeck->play();
            preloadNextOnIdleDeck();
        } else {
            connect(targetDeck, &M1::DeckPlayer::loadingFinished,
                    this, [this, targetDeck]() {
                        targetDeck->play();
                        preloadNextOnIdleDeck();
                    },
                    Qt::SingleShotConnection);
        }
    }
}

void PlaylistModule::playItemAt(int index)
{
    if (index < 0 || index >= (int)m_queue.size())
        return;
    m_currentIndex = index;
    emit queueChanged();
    playNext();
}

void PlaylistModule::advance()
{
    if (m_queue.isEmpty()) {
        qWarning() << "[PlaylistModule] advance() called on empty queue.";
        return;
    }

    int next = findNextValidIndex(m_currentIndex + 1);
    if (next < 0)
        next = findNextValidIndex(0);

    if (next < 0) {
        qWarning() << "[PlaylistModule] No valid next track found.";
        emit statusChanged("Queue exhausted | AutoDJ: " + QString(m_autoDJ ? "ON" : "OFF"));
        return;
    }

    m_currentIndex = next;

    // Remove played tracks (everything before current index)
    removePlayedTracks();

    emit queueChanged();

    const MediaItem& item = m_queue[m_currentIndex];
    const int deck = idleDeck() >= 0 ? idleDeck() : 0;
    qInfo() << "[PlaylistModule] AutoDJ advance -> Deck" << (deck == 0 ? "A" : "B")
            << ":" << item.displayTitle();
    emit requestLoadMedia(item, deck);

    // Track recently played
    m_recentPlayPaths.append(item.filePath);
    m_recentPlayTimes.append(QDateTime::currentDateTime());

    M1::DeckPlayer* nextDeck = (deck == 0) ? m_deckA : m_deckB;
    if (nextDeck) {
        connect(nextDeck, &M1::DeckPlayer::loadingFinished,
                this, [this, nextDeck]() {
                    nextDeck->play();
                    preloadNextOnIdleDeck();
                },
                Qt::SingleShotConnection);
    }

    emit statusChanged(QString("%1 tracks | AutoDJ: %2 | %3")
                           .arg(m_queue.size())
                           .arg(m_autoDJ ? "ON" : "OFF")
                           .arg(strategyName()));

    // Auto-refill queue
    const int remaining = (int)m_queue.size() - m_currentIndex;
    if (remaining <= m_autoDJConfig.queueDepth / 2)
        fillQueueFromLibrary();

    if (remaining <= m_minQueueDepth)
        emit queueLow(remaining);
}

void PlaylistModule::skip()
{
    if (m_queue.isEmpty())
        return;
    int next = findNextValidIndex(m_currentIndex + 1);
    if (next < 0)
        next = findNextValidIndex(0);
    if (next >= 0 && next != m_currentIndex) {
        m_currentIndex = next;
        emit queueChanged();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// AutoDJ poll — monitors active deck remaining time for crossfade trigger
// ─────────────────────────────────────────────────────────────────────────────

int PlaylistModule::idleDeck() const
{
    if (!m_deckA || !m_deckB) return -1;
    using S = M1::DeckPlayer::State;
    if (m_deckA->state() == S::Empty || m_deckA->state() == S::Ready) return 0;
    if (m_deckB->state() == S::Empty || m_deckB->state() == S::Ready) return 1;
    return -1;
}

void PlaylistModule::onPollTimer()
{
    if (!m_autoDJ || m_queue.isEmpty() || m_fading) return;

    const int idle = idleDeck();
    if (idle < 0) return;

    M1::DeckPlayer* active = (idle == 0) ? m_deckB : m_deckA;
    if (!active) return;

    const double dur = active->durationSeconds();
    const double pos = active->positionSeconds();
    const double rem = dur - pos;
    if (dur <= 0.0 || rem <= 0.0) return;

    QSettings s("Mcaster1", "Mcaster1Studio");
    s.beginGroup("Crossfade");
    const float fadeTime = s.value("fadeOutTime", 5.0f).toFloat();
    s.endGroup();

    if (rem <= (double)fadeTime) {
        int next = findNextValidIndex(m_currentIndex + 1);
        if (next < 0) {
            fillQueueFromLibrary();
            next = findNextValidIndex(m_currentIndex + 1);
            if (next < 0) {
                emit statusChanged("Queue exhausted");
                return;
            }
        }

        m_fading = true;
        m_currentIndex = next;

        // Remove played tracks (everything before current index)
        removePlayedTracks();

        emit queueChanged();

        const MediaItem& item = m_queue[m_currentIndex];

        // Track recently played
        m_recentPlayPaths.append(item.filePath);
        m_recentPlayTimes.append(QDateTime::currentDateTime());

        M1::DeckPlayer* idleDeckPlayer = (idle == 0) ? m_deckA : m_deckB;

        // Check if track was already preloaded on the idle deck
        if (idleDeckPlayer &&
            idleDeckPlayer->loadedPath() == item.filePath &&
            idleDeckPlayer->state() == M1::DeckPlayer::State::Ready)
        {
            // Already preloaded — play immediately
            idleDeckPlayer->play();
        } else {
            // Not preloaded — load then play
            emit requestLoadMedia(item, idle);
            if (idleDeckPlayer) {
                connect(idleDeckPlayer, &M1::DeckPlayer::loadingFinished,
                        idleDeckPlayer, [idleDeckPlayer]() { idleDeckPlayer->play(); },
                        Qt::SingleShotConnection);
            }
        }

        const float cfTarget = (idle == 1) ? 1.0f : 0.0f;
        const int   durMs    = (int)(fadeTime * 1000.0f);
        emit crossfaderAnimateTo(cfTarget, durMs);

        emit statusChanged(
            QString("%1 tracks | AutoDJ: ON | Crossfading to: %2")
                .arg(m_queue.size())
                .arg(item.displayTitle()));

        // After crossfade completes, stop the faded-out deck to prevent
        // its finished() signal from triggering a spurious advance().
        M1::DeckPlayer* fadedOutDeck = active;  // capture before lambda
        QTimer::singleShot(durMs + 500, this, [this, fadedOutDeck]() {
            m_fading = false;

            // Stop the deck that was faded out — it's at 0 volume anyway.
            // This prevents finished() from firing and double-advancing.
            if (fadedOutDeck)
                fadedOutDeck->stop();

            emit statusChanged(QString("%1 tracks | AutoDJ: ON | %2")
                                   .arg(m_queue.size()).arg(strategyName()));

            // Auto-refill after crossfade
            const int remaining = (int)m_queue.size() - m_currentIndex;
            if (remaining <= m_autoDJConfig.queueDepth / 2)
                fillQueueFromLibrary();

            // Preload the next track on the now-idle (stopped) deck
            preloadNextOnIdleDeck();
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Auto-fade completed (CrossfaderModule AUTO button)
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::onAutoFadeCompleted(int stoppedDeck)
{
    if (!m_autoDJ || m_queue.isEmpty()) return;
    // If the poll-timer crossfade is active, it already handles advancement
    if (m_fading) return;

    // Guard: block poll timer from interfering during this sequence
    m_fading = true;

    qInfo() << "[PlaylistModule] Auto-fade completed. Stopped deck:" << stoppedDeck
            << "— advancing queue. currentIndex:" << m_currentIndex
            << "queueSize:" << m_queue.size();

    // Advance to the next track in the queue
    int next = findNextValidIndex(m_currentIndex + 1);
    if (next < 0) {
        fillQueueFromLibrary();
        next = findNextValidIndex(m_currentIndex + 1);
        if (next < 0) {
            qWarning() << "[PlaylistModule] No next track found after auto-fade.";
            m_fading = false;
            return;
        }
    }

    m_currentIndex = next;

    // Remove played tracks (everything before current index)
    removePlayedTracks();

    // Bounds check before accessing queue
    if (m_currentIndex < 0 || m_currentIndex >= (int)m_queue.size()) {
        qWarning() << "[PlaylistModule] onAutoFadeCompleted: index out of bounds:"
                    << m_currentIndex << "queueSize:" << m_queue.size();
        m_fading = false;
        return;
    }

    emit queueChanged();

    // Track recently played
    const MediaItem& item = m_queue[m_currentIndex];
    m_recentPlayPaths.append(item.filePath);
    m_recentPlayTimes.append(QDateTime::currentDateTime());

    emit statusChanged(QString("%1 tracks | AutoDJ: ON | %2")
                           .arg(m_queue.size()).arg(strategyName()));

    // Auto-refill queue if running low
    const int remaining = (int)m_queue.size() - m_currentIndex;
    if (remaining <= m_autoDJConfig.queueDepth / 2)
        fillQueueFromLibrary();

    // Preload the next track on the now-idle (stopped) deck.
    // Use a short delay so the crossfader reset completes first.
    // Clear m_fading after preload so poll timer can resume normally.
    QTimer::singleShot(600, this, [this]() {
        if (!m_autoDJ || m_queue.isEmpty()) {
            m_fading = false;
            return;
        }
        preloadNextOnIdleDeck();
        m_fading = false;
        qInfo() << "[PlaylistModule] Auto-fade sequence complete. Poll timer re-enabled.";
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Played-track cleanup
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::removePlayedTracks()
{
    // Remove all tracks before m_currentIndex (already played)
    if (m_currentIndex <= 0) return;

    const int toRemove = m_currentIndex;
    m_queue.erase(m_queue.begin(), m_queue.begin() + toRemove);
    m_currentIndex = 0;

    qInfo() << "[PlaylistModule] Removed" << toRemove << "played track(s) from queue."
            << "Queue now:" << m_queue.size() << "tracks";
}

// ─────────────────────────────────────────────────────────────────────────────
// Rotation helpers
// ─────────────────────────────────────────────────────────────────────────────

bool PlaylistModule::violatesSeparation(int candidateIndex) const
{
    if (candidateIndex < 0 || candidateIndex >= m_queue.size())
        return false;
    if (m_currentIndex < 0)
        return false;

    const MediaItem& candidate = m_queue[candidateIndex];
    const int lookBack = std::max(m_autoDJConfig.artistSeparation, m_autoDJConfig.titleSeparation);
    const int start    = std::max(0, m_currentIndex - lookBack + 1);

    for (int i = start; i <= m_currentIndex; ++i) {
        const MediaItem& played = m_queue[i];
        const int distance = m_currentIndex - i + 1;

        if (m_autoDJConfig.artistSeparation > 0 &&
            distance <= m_autoDJConfig.artistSeparation &&
            !candidate.artist.isEmpty() &&
            candidate.artist.compare(played.artist, Qt::CaseInsensitive) == 0)
        {
            return true;
        }

        if (m_autoDJConfig.titleSeparation > 0 &&
            distance <= m_autoDJConfig.titleSeparation &&
            !candidate.title.isEmpty() &&
            candidate.title.compare(played.title, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}

int PlaylistModule::findNextValidIndex(int startIndex) const
{
    if (m_queue.isEmpty())
        return -1;
    const int qsize = m_queue.size();
    for (int offset = 0; offset < qsize; ++offset) {
        int idx = (startIndex + offset) % qsize;
        if (!violatesSeparation(idx))
            return idx;
    }
    if (!m_queue.isEmpty())
        return startIndex % qsize;
    return -1;
}

// ─────────────────────────────────────────────────────────────────────────────
// State persistence
// ─────────────────────────────────────────────────────────────────────────────

void PlaylistModule::saveState(QSettings& s)
{
    s.beginGroup("PlaylistModule");
    s.setValue("currentIndex",      m_currentIndex);
    s.setValue("autoDJ",            m_autoDJ);
    s.setValue("minQueueDepth",     m_minQueueDepth);
    s.setValue("cwPosition",        m_cwPosition);
    s.setValue("catRotPosition",    m_catRotPosition);

    QJsonArray arr;
    for (const MediaItem& item : m_queue) {
        QJsonObject obj;
        obj["filePath"]   = item.filePath;
        obj["title"]      = item.title;
        obj["artist"]     = item.artist;
        obj["album"]      = item.album;
        obj["genre"]      = item.genre;
        obj["durationMs"] = item.durationMs;
        obj["weight"]     = item.weight;
        obj["rating"]     = item.rating;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    s.setValue("queue", doc.toJson(QJsonDocument::Compact));
    s.endGroup();

    // Save AutoDJ config
    m_autoDJConfig.save(s);
}

void PlaylistModule::loadState(QSettings& s)
{
    s.beginGroup("PlaylistModule");

    m_autoDJ           = s.value("autoDJ", false).toBool();
    m_minQueueDepth    = std::max(1, s.value("minQueueDepth", 3).toInt());
    m_cwPosition       = s.value("cwPosition", 0).toInt();
    m_catRotPosition   = s.value("catRotPosition", 0).toInt();

    const QByteArray raw = s.value("queue").toByteArray();
    m_queue.clear();

    if (!raw.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (doc.isArray()) {
            for (const QJsonValue& v : doc.array()) {
                QJsonObject obj = v.toObject();
                MediaItem item;
                item.filePath   = obj["filePath"].toString();
                item.title      = obj["title"].toString();
                item.artist     = obj["artist"].toString();
                item.album      = obj["album"].toString();
                item.genre      = obj["genre"].toString();
                item.durationMs = obj["durationMs"].toInt();
                item.weight     = obj["weight"].toDouble(1.0);
                item.rating     = obj["rating"].toInt(0);
                m_queue.append(item);
            }
        }
    }

    m_currentIndex = s.value("currentIndex", m_queue.isEmpty() ? -1 : 0).toInt();
    if (m_currentIndex >= m_queue.size())
        m_currentIndex = m_queue.isEmpty() ? -1 : 0;

    s.endGroup();

    // Load AutoDJ config
    m_autoDJConfig.load(s);

    emit queueChanged();
    emit autoDJChanged(m_autoDJ);
    emit statusChanged(QString("%1 tracks | AutoDJ: %2 | %3")
                           .arg(m_queue.size())
                           .arg(m_autoDJ ? "ON" : "OFF")
                           .arg(strategyName()));
}

} // namespace M1

// ─────────────────────────────────────────────────────────────────────────────
// Plugin C ABI exports
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_playlistInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.playlist",
    "Playlist",
    "1.0.0",
    "alpha,beta,company",
    "module",
    "Mcaster1",
    "Broadcast AutoDJ with clockwheel, category rotation, and weighted random"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_playlist_plugin_info()
{
    return &s_playlistInfo;
}

MCASTER1_PLUGIN_API IModule* mcaster1_playlist_create_module(IModuleHost*)
{
    return new M1::PlaylistModule();
}

MCASTER1_PLUGIN_API void mcaster1_playlist_destroy_module(IModule* m)
{
    delete m;
}

} // extern "C"
