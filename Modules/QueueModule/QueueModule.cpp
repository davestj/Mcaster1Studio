#include "QueueModule.h"
#include "QueueWidget.h"
#include "DeckPlayer.h"
#include <QSettings>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <algorithm>

// ─── M3U playlist parser ──────────────────────────────────────────────────────
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

// Audio extensions we accept
static const QStringList kAudioExts =
    {"mp3","flac","wav","aif","aiff","ogg","opus","m4a","aac","wv","ape"};

QueueModule::QueueModule(QObject* parent)
    : M1::IModule(parent)
{}

QueueModule::~QueueModule() = default;

// ─── Deck wiring ──────────────────────────────────────────────────────────────
void QueueModule::connectDeck(M1::DeckPlayer* player) {
    m_deck = player;
    qInfo() << "[QueueModule] connectDeck:" << (player ? "YES" : "null");
}

// ─── UI ───────────────────────────────────────────────────────────────────────
QWidget* QueueModule::createWidget(QWidget* parent) {
    return new QueueWidget(this, parent);
}

// ─── Queue management ─────────────────────────────────────────────────────────
void QueueModule::addFiles(const QStringList& paths) {
    for (const QString& path : paths) {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "m3u" || ext == "m3u8") {
            addFiles(parseM3U(path));
            continue;
        }
        if (ext == "pls") {
            addFiles(parsePLS(path));
            continue;
        }
        if (!kAudioExts.contains(ext)) continue;
        M1::MediaItem item;
        item.filePath = path;
        item.title    = QFileInfo(path).completeBaseName();
        addItem(item);
    }
}

void QueueModule::addPlaylistFile(const QString& path) {
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

void QueueModule::addItem(const M1::MediaItem& item) {
    m_queue.append(item);
    if (m_currentIndex < 0) m_currentIndex = 0;
    emit queueChanged();
}

void QueueModule::removeItem(int index) {
    if (index < 0 || index >= m_queue.size()) return;
    m_queue.removeAt(index);
    if (m_currentIndex >= (int)m_queue.size())
        m_currentIndex = std::max(0, (int)m_queue.size() - 1);
    if (m_queue.isEmpty()) m_currentIndex = -1;
    emit queueChanged();
}

void QueueModule::moveItem(int from, int to) {
    if (from == to) return;
    if (from < 0 || from >= m_queue.size()) return;
    to = std::clamp(to, 0, (int)m_queue.size() - 1);
    const bool wasCurrentFrom = (from == m_currentIndex);
    m_queue.move(from, to);
    if (wasCurrentFrom) {
        m_currentIndex = to;
    } else if (from < m_currentIndex && to >= m_currentIndex) {
        --m_currentIndex;
    } else if (from > m_currentIndex && to <= m_currentIndex) {
        ++m_currentIndex;
    }
    emit queueChanged();
}

void QueueModule::clearQueue() {
    m_queue.clear();
    m_currentIndex = -1;
    emit queueChanged();
}

void QueueModule::loadNextOnDeck(int /*deckIndex*/) {
    // Advance to next item and load on connected deck
    if (m_queue.isEmpty()) return;
    const int next = (m_currentIndex + 1) % m_queue.size();
    playNow(next);
}

void QueueModule::playNow(int index) {
    if (index < 0 || index >= m_queue.size()) return;
    m_currentIndex = index;
    if (m_deck) {
        m_deck->loadFile(m_queue[m_currentIndex].filePath);
    }
    emit requestLoadMedia(m_queue[m_currentIndex], 0);
    emit queueChanged();
}

// ─── State persistence ────────────────────────────────────────────────────────
void QueueModule::saveState(QSettings& s) {
    s.beginGroup("QueueModule");
    QStringList paths;
    for (const auto& item : m_queue) paths << item.filePath;
    s.setValue("queuePaths", paths);
    s.setValue("currentIndex", m_currentIndex);
    s.endGroup();
}

void QueueModule::loadState(QSettings& s) {
    s.beginGroup("QueueModule");
    const QStringList paths = s.value("queuePaths").toStringList();
    const int savedIdx = s.value("currentIndex", -1).toInt();
    s.endGroup();

    m_queue.clear();
    addFiles(paths);
    m_currentIndex = std::clamp(savedIdx, -1, (int)m_queue.size() - 1);
    emit queueChanged();
}
