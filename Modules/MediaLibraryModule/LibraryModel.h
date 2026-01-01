#pragma once
#include "MediaItem.h"
#include <QAbstractTableModel>
#include <QList>
#include <QStringList>
#include <QMimeData>

namespace M1 {

/// LibraryModel — QAbstractTableModel backed by QList<MediaItem>.
/// Supports drag-to-deck via text/uri-list MIME type.
class LibraryModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColTitle = 0, ColArtist, ColAlbum, ColGenre,
        ColDuration, ColBpm, ColKey, ColBitrate, ColRating, ColCodec,
        ColCount
    };

    static constexpr int ItemRole = Qt::UserRole + 1;

    explicit LibraryModel(QObject* parent = nullptr);

    // QAbstractTableModel
    int      rowCount(const QModelIndex& parent = {}) const override;
    int      columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Drag
    Qt::DropActions supportedDropActions() const override;
    QStringList     mimeTypes() const override;
    QMimeData*      mimeData(const QModelIndexList& indexes) const override;

    // Data management
    void addItem(const MediaItem& item);
    void updateItem(const MediaItem& item);
    void removeItem(qint64 id);
    void setItems(const QList<MediaItem>& items);
    void clear();

    MediaItem itemAt(int row) const;
    int       rowForId(qint64 id) const;
    int       itemCount() const { return m_items.size(); }

private:
    QList<MediaItem> m_items;
};

} // namespace M1
