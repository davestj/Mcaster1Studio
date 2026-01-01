#pragma once
#include "MediaItem.h"
#include <QWidget>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>

namespace M1 {

class LibraryModel;
class ScanWorker;
class IDatabase;
class MusicBrainzLookup;

/// LibraryTableView — QTableView subclass that provides file-URL drag to DeckPanel.
class LibraryTableView : public QTableView {
    Q_OBJECT
public:
    explicit LibraryTableView(QWidget* parent = nullptr);
protected:
    void startDrag(Qt::DropActions supportedActions) override;
};

/// LibraryWidget — full media library panel.
///
/// Layout:
///   [Search filter___________________________] [Scan Dir]
///   ┌─────────────────────────────────────────────────────┐
///   │  Title  │ Artist │ Album │ Genre │ BPM │ Dur │ …   │
///   │  …      │ …      │ …     │ …     │ …   │ …   │ …   │
///   └─────────────────────────────────────────────────────┘
///   N tracks  [────────── progress ──────────]
///
/// Drag a row onto DeckPanel to load the track.
/// Double-click loads to Deck A.
/// Right-click shows context menu (Load A/B, MusicBrainz, Export, Remove).
class LibraryWidget : public QWidget {
    Q_OBJECT

public:
    explicit LibraryWidget(LibraryModel* model,
                           IDatabase* db,
                           ScanWorker* scanner,
                           QWidget* parent = nullptr);

signals:
    void requestLoadMedia(const M1::MediaItem& item, int deckIndex);
    void scanDirectoryRequested(const QStringList& dirs);
    void mbLookupRequested(const M1::MediaItem& item);

public slots:
    void onScanStarted(int estimatedCount);
    void onItemScanned(const M1::MediaItem& item);
    void onScanProgress(int done, int total);
    void onScanFinished(int total);

private slots:
    void onScanClicked();
    void onFilterChanged(const QString& text);
    void onContextMenu(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& proxyIdx);
    void onExportM3U();

private:
    void      setupUi();
    MediaItem selectedItem() const;

    LibraryModel*           m_model    = nullptr;
    IDatabase*              m_db       = nullptr;
    ScanWorker*             m_scanner  = nullptr;

    LibraryTableView*       m_view     = nullptr;
    QSortFilterProxyModel*  m_proxy    = nullptr;
    QLineEdit*              m_filter   = nullptr;
    QPushButton*            m_scanBtn  = nullptr;
    QLabel*                 m_statusLbl = nullptr;
    QProgressBar*           m_progress = nullptr;
};

} // namespace M1
