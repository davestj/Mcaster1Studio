#pragma once
#include "MediaItem.h"
#include <QWidget>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QProgressBar>
#include <QSplitter>
#include <QTimer>
#include <QStyledItemDelegate>

class LibraryCategoryPanel;

namespace M1 {

class LibraryModel;
class ScanWorker;
class IDatabase;
class MusicBrainzLookup;
class AlbumArtCache;
class AiTrackIntel;
class SqliteManager;

/// AlbumArtDelegate — paints album art thumbnail as decoration in the Title column.
class AlbumArtDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit AlbumArtDelegate(AlbumArtCache* cache, QObject* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
private:
    AlbumArtCache* m_cache = nullptr;
};

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
///   ┌──────────────┬─────────────────────────────────────────────┐
///   │ Categories   │ [Search filter___________________] [Scan]  │
///   │              │ ┌─────────────────────────────────────────┐ │
///   │  All Tracks  │ │ Art │ Title │ Artist │ Album │ …       │ │
///   │  Rock        │ │     │ …     │ …      │ …     │ …       │ │
///   │  Jazz        │ └─────────────────────────────────────────┘ │
///   │  …           │ N tracks                                   │
///   └──────────────┴─────────────────────────────────────────────┘
///
/// Drag a row onto DeckPanel to load the track.
/// Double-click loads to Deck A.
/// Right-click shows context menu (Load A/B, MusicBrainz, AI Lookup, Rating, AutoDJ, Export, Remove).
class LibraryWidget : public QWidget {
    Q_OBJECT

public:
    explicit LibraryWidget(LibraryModel* model,
                           IDatabase* db,
                           ScanWorker* scanner,
                           QWidget* parent = nullptr);

    /// Set the album art cache (owned by MediaLibraryModule).
    void setAlbumArtCache(AlbumArtCache* cache);

    /// Set the AI track intel engine (owned by MediaLibraryModule).
    void setAiTrackIntel(AiTrackIntel* ai);

    /// Set the category panel database (called after DB is ready).
    void setCategoryDatabase(SqliteManager* sqliteMgr);

signals:
    void requestLoadMedia(const M1::MediaItem& item, int deckIndex);
    void requestPlayOnAuxCue(const M1::MediaItem& item);  ///< Play track on AUX Deck CUE device
    void scanDirectoryRequested(const QStringList& dirs);
    void mbLookupRequested(const M1::MediaItem& item);
    void aiLookupRequested(const QString& artistName);

public slots:
    void onScanStarted(int estimatedCount);
    void onItemScanned(const M1::MediaItem& item);
    void onScanProgress(int done, int total);
    void onScanFinished(int total);

private slots:
    void onScanClicked();
    void onScanIntoCategoryRequested(qint64 categoryId);
    void onFilterChanged(const QString& text);
    void onSearchDebounceTimeout();
    void onContextMenu(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& proxyIdx);
    void onTableClicked(const QModelIndex& proxyIdx);
    void onExportM3U();
    void onCategorySelected(qint64 categoryId);
    void onAiProfileReady(const QString& artistName, const QString& profileText,
                          const QString& discographyJson, const QString& aiBackend,
                          const QString& aiModel);

    // AI Category actions
    void onAiRecommendRequested(qint64 categoryId, const QString& personaName);
    void onAiGeneratePlaylistRequested(qint64 categoryId, const QString& personaName);
    void onAiDaypartRequested(qint64 categoryId, const QString& personaName);

private:
    void      setupUi();
    void      loadCategoryTracks(qint64 categoryId);
    MediaItem selectedItem() const;
    void      updateStatusLabel();
    void      refreshIntelBadges();
    void      openArtistIntelDialog(const QString& artistName);
    QString   lookupPersonaSystemPrompt(const QString& personaName) const;
    QString   categoryDisplayName(qint64 categoryId) const;

    LibraryModel*           m_model       = nullptr;
    IDatabase*              m_db          = nullptr;
    ScanWorker*             m_scanner     = nullptr;
    SqliteManager*          m_sqliteMgr   = nullptr;

    // UI elements
    QSplitter*              m_splitter    = nullptr;
    LibraryCategoryPanel*   m_categoryPanel = nullptr;
    LibraryTableView*       m_view        = nullptr;
    QSortFilterProxyModel*  m_proxy       = nullptr;
    QLineEdit*              m_filter      = nullptr;
    QPushButton*            m_scanBtn     = nullptr;
    QLabel*                 m_statusLbl   = nullptr;
    QProgressBar*           m_progress    = nullptr;

    // New components
    AlbumArtCache*          m_artCache    = nullptr;
    AiTrackIntel*           m_aiIntel     = nullptr;
    QTimer*                 m_searchDebounce = nullptr;
    qint64                  m_activeCategory = -1;  ///< -1 = All Tracks
    qint64                  m_pendingScanCategoryId = -1; ///< Category to assign after scan
    QString                 m_pendingScanDir;             ///< Directory being scanned into category
};

} // namespace M1
