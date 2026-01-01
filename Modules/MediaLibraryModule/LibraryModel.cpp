#include "LibraryModel.h"
#include <QMimeData>
#include <QUrl>
#include <QColor>

namespace M1 {

static const char* kHeaders[] = {
    "Title", "Artist", "Album", "Genre",
    "Duration", "BPM", "Key", "Bitrate", "Rating", "Codec"
};
static_assert(std::size(kHeaders) == LibraryModel::ColCount,
              "Header count must match Column enum");

LibraryModel::LibraryModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

int LibraryModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

int LibraryModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColCount;
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size())
        return {};

    const MediaItem& item = m_items[index.row()];

    if (role == ItemRole)
        return QVariant::fromValue(item);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColTitle:    return item.displayTitle();
        case ColArtist:   return item.artist;
        case ColAlbum:    return item.album;
        case ColGenre:    return item.genre;
        case ColDuration: return item.durationString();
        case ColBpm:
            return item.bpm > 0.0 ? QString::number(item.bpm, 'f', 1) : QString("—");
        case ColKey:
            return item.musicalKey.isEmpty() ? QString("—") : item.musicalKey;
        case ColBitrate:
            return item.bitrate > 0 ? QString("%1k").arg(item.bitrate) : QString("—");
        case ColRating:
            return item.rating > 0 ? QString(item.rating, QChar(0x2605)) : QString("—");
        case ColCodec:
            return item.codec.toUpper();
        }
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColDuration:
        case ColBpm:
        case ColBitrate:
        case ColRating:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        case ColKey:
        case ColCodec:
            return static_cast<int>(Qt::AlignHCenter | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::ForegroundRole && index.column() == ColRating && item.rating > 0)
        return QColor("#f59e0b"); // amber stars

    return {};
}

QVariant LibraryModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return (section >= 0 && section < ColCount) ? kHeaders[section] : QVariant{};
    return {};
}

Qt::ItemFlags LibraryModel::flags(const QModelIndex& index) const {
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled;
    return f;
}

Qt::DropActions LibraryModel::supportedDropActions() const {
    return Qt::CopyAction;
}

QStringList LibraryModel::mimeTypes() const {
    return {"text/uri-list", "application/x-m1-media-item"};
}

QMimeData* LibraryModel::mimeData(const QModelIndexList& indexes) const {
    // Collect unique rows
    QList<int> rows;
    for (const QModelIndex& idx : indexes) {
        if (!rows.contains(idx.row()))
            rows.append(idx.row());
    }

    QList<QUrl> urls;
    QByteArray  itemBytes;
    QDataStream stream(&itemBytes, QIODevice::WriteOnly);

    for (int row : rows) {
        if (row < 0 || row >= m_items.size()) continue;
        const MediaItem& item = m_items[row];
        urls << QUrl::fromLocalFile(item.filePath);
        stream << item.filePath;
    }

    auto* mime = new QMimeData;
    mime->setUrls(urls);
    mime->setData("application/x-m1-media-item", itemBytes);
    return mime;
}

// ─── Data management ─────────────────────────────────────────────────────────
void LibraryModel::addItem(const MediaItem& item) {
    const int row = static_cast<int>(m_items.size());
    beginInsertRows({}, row, row);
    m_items.append(item);
    endInsertRows();
}

void LibraryModel::updateItem(const MediaItem& item) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == item.id ||
            m_items[i].filePath == item.filePath) {
            m_items[i] = item;
            emit dataChanged(index(i, 0), index(i, ColCount - 1));
            return;
        }
    }
}

void LibraryModel::removeItem(qint64 id) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            beginRemoveRows({}, i, i);
            m_items.removeAt(i);
            endRemoveRows();
            return;
        }
    }
}

void LibraryModel::setItems(const QList<MediaItem>& items) {
    beginResetModel();
    m_items = items;
    endResetModel();
}

void LibraryModel::clear() {
    beginResetModel();
    m_items.clear();
    endResetModel();
}

MediaItem LibraryModel::itemAt(int row) const {
    if (row < 0 || row >= m_items.size()) return {};
    return m_items[row];
}

int LibraryModel::rowForId(qint64 id) const {
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id) return i;
    return -1;
}

} // namespace M1
