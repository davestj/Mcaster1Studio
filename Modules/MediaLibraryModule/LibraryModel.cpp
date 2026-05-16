#include "LibraryModel.h"
#include "ThemePalette.h"
#include <QMimeData>
#include <QUrl>
#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>
#include <QFontMetrics>

namespace M1 {

static const char* kHeaders[] = {
    "", "Title", "Artist", "Album", "Genre",
    "Duration", "BPM", "Key", "Bitrate", "Rating", "Codec"
};
static_assert(std::size(kHeaders) == LibraryModel::ColCount,
              "Header count must match Column enum");

// ─── Badge icon generation (programmatic QPainter rendering) ─────────────────

/// Generate the "AI" report badge: a compact rounded pill with gradient fill.
/// The badge is drawn at the requested height with auto-calculated width.
static QPixmap makeAiReportBadge(int height)
{
    const auto pal = ThemePalette::forCurrentTheme();

    // Badge text
    const QString text = QStringLiteral("AI");
    QFont font(QStringLiteral("Segoe UI"), 0, QFont::Bold);
    font.setPixelSize(qMax(9, height - 6));
    QFontMetrics fm(font);

    const int textWidth = fm.horizontalAdvance(text);
    const int hPad = 5;
    const int w = textWidth + hPad * 2;
    const int h = height;

    QPixmap pm(w, h);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Gradient: navy to bright blue (themed via info/accent)
    QLinearGradient grad(0, 0, 0, h);
    grad.setColorAt(0.0, pal.info);                          // #1c5caa navy (enterprise) / #d4891e (classic)
    grad.setColorAt(1.0, pal.accent.lighter(130));           // brighter shade

    // Rounded rect with 1px radius
    QPainterPath path;
    path.addRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 1.0, 1.0);
    p.fillPath(path, grad);

    // Subtle top highlight (1px bright line)
    QPainterPath topHighlight;
    topHighlight.addRoundedRect(QRectF(1.0, 1.0, w - 2, h / 2.0 - 1), 1.0, 1.0);
    QLinearGradient shine(0, 0, 0, h / 2.0);
    shine.setColorAt(0.0, QColor(255, 255, 255, 60));
    shine.setColorAt(1.0, QColor(255, 255, 255, 0));
    p.fillPath(topHighlight, shine);

    // Border
    p.setPen(QPen(pal.info.darker(130), 0.8));
    p.drawPath(path);

    // Text: white, bold, centered
    p.setFont(font);
    p.setPen(Qt::white);
    const QRect textRect(0, 0, w, h);
    p.drawText(textRect, Qt::AlignCenter, text);

    p.end();
    return pm;
}

// ─── Model implementation ────────────────────────────────────────────────────

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

    // ── ColIntel: AI Intel badge ────────────────────────────────────────────
    if (index.column() == ColIntel) {
        if (role == Qt::DecorationRole) {
            if (!item.artist.isEmpty() && m_artistsWithIntel.contains(item.artist)) {
                // Check if cached badge needs regeneration (theme changed)
                const int themeIdx = ThemePalette::isLightTheme() ? 0 : 1;
                if (m_cachedReportBadge.isNull() || m_cachedBadgeTheme != themeIdx) {
                    m_cachedReportBadge = makeAiReportBadge(20);
                    m_cachedBadgeTheme = themeIdx;
                }
                return m_cachedReportBadge;
            }
            return {};  // no badge for artists without intel
        }
        if (role == Qt::TextAlignmentRole)
            return static_cast<int>(Qt::AlignCenter);
        if (role == Qt::ToolTipRole) {
            if (!item.artist.isEmpty() && m_artistsWithIntel.contains(item.artist))
                return QStringLiteral("AI Intel report available for %1").arg(item.artist);
            return {};
        }
        return {};  // no display text for intel column
    }

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

// ─── AI Intel badge support ──────────────────────────────────────────────────

void LibraryModel::setArtistsWithIntel(const QStringList& artists) {
    m_artistsWithIntel.clear();
    for (const QString& name : artists)
        m_artistsWithIntel.insert(name);

    // Invalidate cached badge so it regenerates with current theme
    m_cachedReportBadge = QPixmap();

    // Notify views that the intel column data may have changed for all rows
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0, ColIntel),
                         index(static_cast<int>(m_items.size()) - 1, ColIntel),
                         {Qt::DecorationRole, Qt::ToolTipRole});
    }
}

bool LibraryModel::hasIntelForArtist(const QString& artist) const {
    return !artist.isEmpty() && m_artistsWithIntel.contains(artist);
}

} // namespace M1
