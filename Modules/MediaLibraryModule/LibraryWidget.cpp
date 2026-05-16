#include "LibraryWidget.h"
#include "LibraryModel.h"
#include "IDatabase.h"
#include "ScanWorker.h"
#include "SqliteManager.h"
#include "AlbumArtCache.h"
#include "AiTrackIntel.h"
#include "ArtistIntelDialog.h"
#include "LibraryCategoryPanel.h"
#include "PersonaManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMenu>
#include <QDrag>
#include <QMimeData>
#include <QSortFilterProxyModel>
#include <QFileInfo>
#include <QTextStream>
#include <QTextBrowser>
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLabel>
#include <QPainter>
#include <QApplication>
#include <QSplitter>
#include <QSignalBlocker>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QTableWidget>
#include <QListWidget>
#include <QSettings>
#include <QRegularExpression>
#include <QScrollBar>
#include <functional>
#include <memory>

namespace M1 {

// ─── AlbumArtDelegate ─────────────────────────────────────────────────────────
AlbumArtDelegate::AlbumArtDelegate(AlbumArtCache* cache, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_cache(cache)
{}

void AlbumArtDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    // Draw background/selection only — NOT the default text (we draw our own)
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    // Clear the text so the base paint only draws background/highlight
    opt.text.clear();
    QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    if (!m_cache || index.column() != LibraryModel::ColTitle) {
        // For non-title columns, just draw text normally
        const QString text = index.data(Qt::DisplayRole).toString();
        painter->setPen(option.palette.color(
            (option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
        painter->drawText(option.rect.adjusted(4, 0, -2, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
        return;
    }

    // Title column: draw art thumbnail + title text side by side
    const int artSize = option.rect.height() - 2;
    if (artSize > 0) {
        QVariant v = index.data(LibraryModel::ItemRole);
        QPixmap art = AlbumArtCache::defaultArt(artSize);
        if (v.isValid()) {
            const MediaItem item = v.value<MediaItem>();
            if (item.hasArt && !item.filePath.isEmpty())
                art = m_cache->artForTrack(item.filePath, artSize);
        }
        painter->drawPixmap(option.rect.left() + 1, option.rect.top() + 1, artSize, artSize, art);
    }

    // Draw the title text to the right of the art
    const int textLeft = option.rect.left() + artSize + 6;
    const QRect textRect(textLeft, option.rect.top(),
                         option.rect.right() - textLeft, option.rect.height());
    const QString text = index.data(Qt::DisplayRole).toString();
    painter->setPen(option.palette.color(
        (option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
}

QSize AlbumArtDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    // Ensure rows are tall enough for artwork thumbnails
    if (s.height() < 28)
        s.setHeight(28);
    return s;
}

// ─── LibraryTableView ─────────────────────────────────────────────────────────
LibraryTableView::LibraryTableView(QWidget* parent)
    : QTableView(parent)
{
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setAlternatingRowColors(true);
    setShowGrid(false);
    setWordWrap(false);
    setSortingEnabled(true);
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(28); // taller for art thumbnails
}

void LibraryTableView::startDrag(Qt::DropActions /*supportedActions*/) {
    QModelIndexList selected = selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    // QSortFilterProxyModel::mimeData() maps proxy indexes to source, then
    // calls source model's mimeData() — so this produces proper file URLs.
    QMimeData* mime = model()->mimeData(selected);
    if (!mime) return;

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);

    // Thumbnail: show item count
    QPixmap pm(120, 24);
    pm.fill(QColor(14, 165, 233, 200)); // sky-blue semi-transparent
    QPainter painter(&pm);
    painter.setPen(Qt::white);
    painter.setFont(QFont("Consolas", 9, QFont::Bold));
    painter.drawText(pm.rect(), Qt::AlignCenter,
                     QString("%1 track(s)").arg(selected.size()));
    painter.end();
    drag->setPixmap(pm);
    drag->setHotSpot({60, 12});

    drag->exec(Qt::CopyAction, Qt::CopyAction);
}

// ─── LibraryWidget ────────────────────────────────────────────────────────────
LibraryWidget::LibraryWidget(LibraryModel* model, IDatabase* db,
                             ScanWorker* scanner, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_db(db)
    , m_scanner(scanner)
{
    // Search debounce timer — 300ms delay for FTS5 queries
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(300);
    connect(m_searchDebounce, &QTimer::timeout,
            this, &LibraryWidget::onSearchDebounceTimeout);

    setupUi();
}

void LibraryWidget::setAlbumArtCache(AlbumArtCache* cache) {
    m_artCache = cache;
    // Album art delegate disabled for now — title column text was being hidden
    // TODO: Fix delegate painting and re-enable
    (void)cache;
    if (false && m_view && m_artCache) {
        auto* delegate = new AlbumArtDelegate(m_artCache, m_view);
        m_view->setItemDelegateForColumn(LibraryModel::ColTitle, delegate);
    }
}

void LibraryWidget::setAiTrackIntel(AiTrackIntel* ai) {
    m_aiIntel = ai;
    // Note: profileReady signal is handled by ArtistIntelDialog when it opens.
    // No need to connect here — the dialog manages its own save flow.
}

void LibraryWidget::setCategoryDatabase(SqliteManager* sqliteMgr) {
    m_sqliteMgr = sqliteMgr;
    if (m_categoryPanel && m_sqliteMgr) {
        m_categoryPanel->setDatabase(m_sqliteMgr);
        m_categoryPanel->reload();
    }
    // Populate AI Intel badges for artists that already have saved reports
    refreshIntelBadges();
}

void LibraryWidget::setupUi() {
    setObjectName("LibraryWidget");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Horizontal splitter: category panel | main content ───────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setObjectName("LibrarySplitter");
    m_splitter->setHandleWidth(3);

    // ── Left: Category sidebar ───────────────────────────────────────────────
    m_categoryPanel = new LibraryCategoryPanel(m_splitter);
    m_categoryPanel->setMinimumWidth(140);
    connect(m_categoryPanel, &LibraryCategoryPanel::categorySelected,
            this, &LibraryWidget::onCategorySelected);
    connect(m_categoryPanel, &LibraryCategoryPanel::scanIntoCategoryRequested,
            this, &LibraryWidget::onScanIntoCategoryRequested);
    connect(m_categoryPanel, &LibraryCategoryPanel::aiRecommendRequested,
            this, &LibraryWidget::onAiRecommendRequested);
    connect(m_categoryPanel, &LibraryCategoryPanel::aiGeneratePlaylistRequested,
            this, &LibraryWidget::onAiGeneratePlaylistRequested);
    connect(m_categoryPanel, &LibraryCategoryPanel::aiDaypartRequested,
            this, &LibraryWidget::onAiDaypartRequested);

    // ── Right: toolbar + table + status ──────────────────────────────────────
    auto* rightPane = new QWidget(m_splitter);
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(6, 6, 6, 6);
    rightLayout->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(4);

    m_filter = new QLineEdit(rightPane);
    m_filter->setPlaceholderText("Search title, artist, album, genre\u2026");
    m_filter->setClearButtonEnabled(true);
    m_filter->setMinimumHeight(26);
    connect(m_filter, &QLineEdit::textChanged, this, &LibraryWidget::onFilterChanged);

    m_scanBtn = new QPushButton("+ Scan Directory", rightPane);
    m_scanBtn->setMinimumHeight(26);
    {
        auto tp = ThemePalette::forCurrentTheme();
        m_scanBtn->setStyleSheet(QString(
            "QPushButton{background:%1; color:%2; border:1px solid %3;"
            " border-radius:4px; padding:0 10px; font-size:12px; font-weight:700;}"
            "QPushButton:hover{background:%3;}"
            "QPushButton:disabled{color:#334155;}")
            .arg(tp.panelBg.name(), tp.info.name(), tp.border.name()));
    }
    connect(m_scanBtn, &QPushButton::clicked, this, &LibraryWidget::onScanClicked);

    toolbar->addWidget(m_filter, 1);
    toolbar->addWidget(m_scanBtn);
    rightLayout->addLayout(toolbar);

    // ── Table view ────────────────────────────────────────────────────────────
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);        // search all columns
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_view = new LibraryTableView(rightPane);
    m_view->setModel(m_proxy);
    m_view->sortByColumn(LibraryModel::ColArtist, Qt::AscendingOrder);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QTableView::customContextMenuRequested,
            this, &LibraryWidget::onContextMenu);
    connect(m_view, &QTableView::doubleClicked,
            this, &LibraryWidget::onDoubleClicked);
    connect(m_view, &QTableView::clicked,
            this, &LibraryWidget::onTableClicked);

    // Column widths
    auto* hdr = m_view->horizontalHeader();
    hdr->setSectionResizeMode(LibraryModel::ColIntel,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColTitle,    QHeaderView::Stretch);
    hdr->setSectionResizeMode(LibraryModel::ColArtist,   QHeaderView::Interactive);
    hdr->setSectionResizeMode(LibraryModel::ColAlbum,    QHeaderView::Interactive);
    hdr->setSectionResizeMode(LibraryModel::ColGenre,    QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColDuration, QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColBpm,      QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColKey,      QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColBitrate,  QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColRating,   QHeaderView::Fixed);
    hdr->setSectionResizeMode(LibraryModel::ColCodec,    QHeaderView::Fixed);

    m_view->setColumnWidth(LibraryModel::ColIntel,     40);
    m_view->setColumnWidth(LibraryModel::ColArtist,   160);
    m_view->setColumnWidth(LibraryModel::ColAlbum,    160);
    m_view->setColumnWidth(LibraryModel::ColGenre,     90);
    m_view->setColumnWidth(LibraryModel::ColDuration,  65);
    m_view->setColumnWidth(LibraryModel::ColBpm,       60);
    m_view->setColumnWidth(LibraryModel::ColKey,       50);
    m_view->setColumnWidth(LibraryModel::ColBitrate,   75);
    m_view->setColumnWidth(LibraryModel::ColRating,    60);
    m_view->setColumnWidth(LibraryModel::ColCodec,     50);

    rightLayout->addWidget(m_view, 1);

    // ── Status row ────────────────────────────────────────────────────────────
    auto* statusRow = new QHBoxLayout;
    m_statusLbl = new QLabel("0 tracks", rightPane);
    m_statusLbl->setStyleSheet(
        "color:#64748b; font-size:12px; background:transparent;");

    m_progress = new QProgressBar(rightPane);
    m_progress->setFixedHeight(4);
    m_progress->setTextVisible(false);
    {
        auto tp = ThemePalette::forCurrentTheme();
        m_progress->setStyleSheet(QString(
            "QProgressBar{background:%1; border:none; border-radius:2px;}"
            "QProgressBar::chunk{background:%2; border-radius:2px;}")
            .arg(tp.panelBg.name(), tp.accent.name()));
    }
    m_progress->hide();

    statusRow->addWidget(m_statusLbl);
    statusRow->addStretch(1);
    statusRow->addWidget(m_progress);
    rightLayout->addLayout(statusRow);

    // ── Assemble splitter ────────────────────────────────────────────────────
    m_splitter->addWidget(m_categoryPanel);
    m_splitter->addWidget(rightPane);
    m_splitter->setSizes({200, 700});
    m_splitter->setStretchFactor(0, 0);  // sidebar doesn't stretch
    m_splitter->setStretchFactor(1, 1);  // table area stretches

    root->addWidget(m_splitter);

    // Update status label with initial track count
    updateStatusLabel();
    qInfo() << "[LibraryWidget] Init: model rows=" << m_model->rowCount()
            << "proxy rows=" << m_proxy->rowCount();
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void LibraryWidget::onScanClicked() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Music Directory",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (dir.isEmpty()) return;
    m_pendingScanCategoryId = -1;  // no category target for toolbar scan
    emit scanDirectoryRequested({dir});
}

void LibraryWidget::onScanIntoCategoryRequested(qint64 categoryId) {
    if (categoryId <= 0) return;
    QString dir = QFileDialog::getExistingDirectory(
        this, QString("Scan Folder into Category"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (dir.isEmpty()) return;

    m_pendingScanCategoryId = categoryId;
    m_pendingScanDir = dir;

    // First: assign any EXISTING tracks from this directory to the category
    if (m_sqliteMgr) {
        int assigned = 0;
        const int rows = m_model->rowCount();
        for (int r = 0; r < rows; ++r) {
            const auto item = m_model->itemAt(r);
            if (item.id > 0 && item.filePath.startsWith(dir)) {
                m_sqliteMgr->assignTrackToCategory(item.id, categoryId);
                ++assigned;
            }
        }
        if (assigned > 0) {
            m_statusLbl->setText(QString("Assigned %1 existing tracks to category").arg(assigned));
            if (m_categoryPanel) m_categoryPanel->reload();
        }
    }

    // Then: scan for NEW files not yet in the library
    emit scanDirectoryRequested({dir});
}

void LibraryWidget::onFilterChanged(const QString& text) {
    // If we have a SqliteManager for FTS5, use debounced search
    if (m_sqliteMgr && !text.trimmed().isEmpty()) {
        m_searchDebounce->start();  // restart the 300ms timer
    } else {
        // Empty search or no SqliteManager — restore the current category view
        m_searchDebounce->stop();
        m_proxy->setFilterFixedString({});
        if (text.isEmpty()) {
            // Restore the active category view (or all tracks)
            loadCategoryTracks(m_activeCategory);
        } else {
            // No SqliteManager — fall back to proxy column filter
            m_proxy->setFilterFixedString(text);
        }
        updateStatusLabel();
    }
}

void LibraryWidget::onSearchDebounceTimeout() {
    const QString query = m_filter->text().trimmed();
    if (query.isEmpty() || !m_sqliteMgr) return;

    // Use FTS5 full-text search, respecting the active category
    QList<M1::MediaItem> results;
    if (m_activeCategory > 0) {
        // Search within the selected category only
        results = m_sqliteMgr->searchInCategory(query, m_activeCategory);
    } else {
        // Search all tracks
        results = m_sqliteMgr->search(query);
    }
    m_model->setItems(results);
    // Clear proxy filter since the model already has filtered results
    m_proxy->setFilterFixedString({});
    updateStatusLabel();
}

void LibraryWidget::onScanStarted(int estimatedCount) {
    m_scanBtn->setEnabled(false);
    m_progress->setRange(0, estimatedCount > 0 ? estimatedCount : 0);
    m_progress->setValue(0);
    m_progress->show();
    m_statusLbl->setText("Scanning\u2026");
}

void LibraryWidget::onItemScanned(const M1::MediaItem& item) {
    // Update status every N items (called from main thread via queued connection)
    if (m_model->rowCount() % 25 == 0)
        m_statusLbl->setText(QString("Scanning\u2026 %1 found").arg(m_model->rowCount()));
    (void)item;
}

void LibraryWidget::onScanProgress(int done, int total) {
    m_progress->setMaximum(total > 0 ? total : 1);
    m_progress->setValue(done);
}

void LibraryWidget::onScanFinished(int /*total*/) {
    m_scanBtn->setEnabled(true);
    m_progress->hide();

    // If a category was targeted for this scan, assign all newly scanned
    // tracks to that category.  We do this by comparing the model's current
    // items against what was in the category before.
    if (m_pendingScanCategoryId > 0 && m_sqliteMgr && !m_pendingScanDir.isEmpty()) {
        // Assign any NEW tracks from the scanned directory to the target category.
        // Existing tracks were already assigned in onScanIntoCategoryRequested().
        const int rows = m_model->rowCount();
        int assigned = 0;
        for (int r = 0; r < rows; ++r) {
            const auto item = m_model->itemAt(r);
            if (item.id > 0 && item.filePath.startsWith(m_pendingScanDir)) {
                m_sqliteMgr->assignTrackToCategory(item.id, m_pendingScanCategoryId);
                ++assigned;
            }
        }
        if (assigned > 0)
            m_statusLbl->setText(QString("Assigned %1 tracks to category").arg(assigned));
        m_pendingScanCategoryId = -1;
        m_pendingScanDir.clear();
    }

    updateStatusLabel();
    // Refresh category counts after scan
    if (m_categoryPanel)
        m_categoryPanel->reload();
}

void LibraryWidget::onDoubleClicked(const QModelIndex& proxyIdx) {
    // If double-click is on the Intel column, open the dialog (don't load to deck)
    const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
    if (srcIdx.column() == LibraryModel::ColIntel) {
        const MediaItem item = m_model->itemAt(srcIdx.row());
        if (!item.artist.isEmpty())
            openArtistIntelDialog(item.artist);
        return;
    }

    // Double-click on other columns -> load to Deck A
    const MediaItem item = m_model->itemAt(srcIdx.row());
    if (!item.filePath.isEmpty())
        emit requestLoadMedia(item, 0);
}

void LibraryWidget::onTableClicked(const QModelIndex& proxyIdx) {
    if (!proxyIdx.isValid()) return;

    const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
    if (srcIdx.column() != LibraryModel::ColIntel) return;

    const MediaItem item = m_model->itemAt(srcIdx.row());
    if (item.artist.isEmpty()) return;

    // Only respond if this artist has an intel report
    if (m_model->hasIntelForArtist(item.artist))
        openArtistIntelDialog(item.artist);
}

void LibraryWidget::onContextMenu(const QPoint& pos) {
    const QModelIndex proxyIdx = m_view->indexAt(pos);
    if (!proxyIdx.isValid()) return;

    const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
    const MediaItem item = m_model->itemAt(srcIdx.row());
    if (item.filePath.isEmpty()) return;

    QMenu menu(this);
    menu.addAction("Load to Deck A", [this, item]() {
        emit requestLoadMedia(item, 0);
    });
    menu.addAction("Load to Deck B", [this, item]() {
        emit requestLoadMedia(item, 1);
    });
    menu.addSeparator();
    menu.addAction("Play in AUX Deck on CUE device", [this, item]() {
        emit requestPlayOnAuxCue(item);
    });
    menu.addSeparator();

    // ── MusicBrainz Lookup (preserved) ───────────────────────────────────────
    menu.addAction("MusicBrainz Lookup\u2026", [this, item]() {
        emit mbLookupRequested(item);
    });

    // ── AI: Generate Artist Intel Report ────────────────────────────────────
    if (m_aiIntel) {
        const bool busy = m_aiIntel->isBusy();
        auto* aiAction = menu.addAction(
            busy ? "AI: Generate Artist Intel Report (busy\u2026)"
                 : "AI: Generate Artist Intel Report",
            [this, item]() {
                if (!item.artist.isEmpty()) {
                    emit aiLookupRequested(item.artist);
                    openArtistIntelDialog(item.artist);
                } else {
                    QMessageBox::information(this, "Artist Intel",
                        "No artist name available for this track.");
                }
            });
        aiAction->setEnabled(!busy);
    }

    menu.addSeparator();

    // ── Set Rating submenu (0-5 stars) ───────────────────────────────────────
    if (m_sqliteMgr) {
        auto* ratingMenu = menu.addMenu("Set Rating");
        for (int stars = 0; stars <= 5; ++stars) {
            const QString label = (stars == 0)
                ? "No Rating"
                : QString(stars, QChar(0x2605));  // filled stars
            ratingMenu->addAction(label, [this, item, stars]() {
                m_sqliteMgr->setRating(item.id, stars);
                MediaItem updated = item;
                updated.rating = stars;
                m_model->updateItem(updated);
            });
        }
    }

    // ── AutoDJ Weight submenu ────────────────────────────────────────────────
    if (m_sqliteMgr) {
        auto* weightMenu = menu.addMenu("AutoDJ Weight");
        for (int w : {0, 25, 50, 75, 100}) {
            const QString label = (w == 0) ? "Never (0)"
                : (w == 25) ? "Low (25)"
                : (w == 50) ? "Normal (50)"
                : (w == 75) ? "High (75)"
                : "Always (100)";
            weightMenu->addAction(label, [this, item, w]() {
                m_sqliteMgr->setAutoDjWeight(item.id, w);
                MediaItem updated = item;
                updated.autoDjWeight = w;
                m_model->updateItem(updated);
            });
        }
    }

    // ── Assign to Category submenu ────────────────────────────────────────────
    if (m_sqliteMgr) {
        auto* catMenu = menu.addMenu("Assign to Category");
        const auto cats = m_sqliteMgr->allCategories();
        for (const auto& cat : cats) {
            qint64 catId = cat.value("id").toLongLong();
            QString catName = cat.value("name").toString();
            QString catColor = cat.value("color").toString();

            // Build a color dot icon for the menu item
            QPixmap pm(12, 12);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            QColor c(catColor);
            if (c.isValid()) {
                p.setBrush(c);
                p.setPen(Qt::NoPen);
                p.drawEllipse(1, 1, 10, 10);
            }
            p.end();

            catMenu->addAction(QIcon(pm), catName, [this, item, catId, catName]() {
                m_sqliteMgr->assignTrackToCategory(item.id, catId);
                // Refresh category panel to update counts
                if (m_categoryPanel)
                    m_categoryPanel->reload();
            });
        }
        if (cats.isEmpty()) {
            catMenu->addAction("(no categories)")->setEnabled(false);
        }
    }

    menu.addSeparator();
    menu.addAction("Export Visible as M3U\u2026", this, &LibraryWidget::onExportM3U);
    menu.addSeparator();
    menu.addAction("Remove from Library", [this, item]() {
        m_model->removeItem(item.id);
        if (m_db && m_db->isConnected())
            m_db->deleteItem(item.id);
        updateStatusLabel();
        // Refresh category counts
        if (m_categoryPanel)
            m_categoryPanel->reload();
    });

    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void LibraryWidget::onExportM3U() {
    const QString path = QFileDialog::getSaveFileName(
        this, "Export M3U Playlist",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        "M3U Playlist (*.m3u);;M3U8 UTF-8 Playlist (*.m3u8)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, "Export Error",
                             "Could not write to: " + path);
        return;
    }

    QTextStream out(&f);
    if (path.endsWith(".m3u8", Qt::CaseInsensitive))
        out.setEncoding(QStringConverter::Utf8);

    out << "#EXTM3U\n";
    const int rows = m_proxy->rowCount();
    for (int r = 0; r < rows; ++r) {
        const QModelIndex srcIdx = m_proxy->mapToSource(m_proxy->index(r, 0));
        const MediaItem item = m_model->itemAt(srcIdx.row());
        const int secs = item.durationMs / 1000;
        out << QString("#EXTINF:%1,%2 - %3\n")
                .arg(secs)
                .arg(item.artist)
                .arg(item.displayTitle());
        out << item.filePath << "\n";
    }

    m_statusLbl->setText(QString("Exported %1 tracks").arg(rows));
}

void LibraryWidget::onCategorySelected(qint64 categoryId) {
    m_activeCategory = categoryId;

    // Block signals to avoid re-entry via onFilterChanged -> loadCategoryTracks
    const QSignalBlocker blocker(m_filter);
    m_filter->clear();

    loadCategoryTracks(categoryId);
}

void LibraryWidget::loadCategoryTracks(qint64 categoryId) {
    if (categoryId == -1) {
        // Show all tracks
        if (m_db && m_db->isConnected()) {
            m_model->setItems(m_db->loadAll());
        }
    } else if (m_sqliteMgr) {
        // Show tracks in selected category
        const auto tracks = m_sqliteMgr->tracksByCategory(categoryId);
        m_model->setItems(tracks);
    }
    m_proxy->setFilterFixedString({});
    updateStatusLabel();
}

void LibraryWidget::onAiProfileReady(const QString& artistName,
                                     const QString& profileText,
                                     const QString& discographyJson,
                                     const QString& aiBackend,
                                     const QString& aiModel)
{
    // Save to database
    if (m_sqliteMgr) {
        m_sqliteMgr->saveArtistIntel(artistName, profileText,
                                     discographyJson, aiBackend, aiModel);
    }

    // Show result to user
    QMessageBox::information(this, QString("AI Intel: %1").arg(artistName),
        profileText.left(2000));  // truncate very long profiles for display
}

MediaItem LibraryWidget::selectedItem() const {
    const QModelIndexList sel = m_view->selectionModel()->selectedRows();
    if (sel.isEmpty()) return {};
    return m_model->itemAt(m_proxy->mapToSource(sel.first()).row());
}

void LibraryWidget::updateStatusLabel() {
    const int shown = m_proxy->rowCount();
    const int total = m_model->rowCount();
    if (shown < total)
        m_statusLbl->setText(QString("%1 / %2 tracks").arg(shown).arg(total));
    else
        m_statusLbl->setText(QString("%1 tracks").arg(total));
}

void LibraryWidget::refreshIntelBadges() {
    if (m_sqliteMgr && m_model)
        m_model->setArtistsWithIntel(m_sqliteMgr->artistsWithIntel());
}

void LibraryWidget::openArtistIntelDialog(const QString& artistName) {
    if (artistName.isEmpty()) return;

    // Open ArtistIntelDialog — it handles both viewing saved reports
    // and generating new ones internally
    auto* dlg = new ArtistIntelDialog(artistName, m_aiIntel, m_sqliteMgr, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // When the dialog closes, refresh badges in case a report was saved
    connect(dlg, &QDialog::finished, this, [this]() {
        refreshIntelBadges();
    });

    dlg->show();
}

// ─── Persona system prompt lookup ─────────────────────────────────────────────

QString LibraryWidget::lookupPersonaSystemPrompt(const QString& personaName) const {
    if (personaName.isEmpty()) return {};

    // First: try DB lookup — match by name in ai_personas table
    if (m_sqliteMgr) {
        const auto personas = m_sqliteMgr->allPersonas();
        for (const auto& p : personas) {
            if (p.value("name").toString() == personaName)
                return p.value("system_prompt").toString();
        }
    }

    // Fallback: hardcoded mapping for the category panel preset names
    // These map the short names used in LibraryCategoryPanel::personaPresetNames()
    // to the longer DB preset names used in PersonaManager::seedPresets().
    static const QMap<QString, QString> presetNameToDbName = {
        {"Classic DJ",             "Classic Rock DJ"},
        {"Late Night Chill",       "Jazz DJ"},
        {"Morning Show Host",      "Sports Radio Host"},
        {"Top 40 Hype",            "Top 40 DJ"},
        {"Country Radio",          "Country Radio DJ"},
        {"Hip-Hop / R&B",          "Hip-Hop DJ"},
        {"Rock & Metal",           "Alternative DJ"},
        {"Classical / NPR",        "Jazz DJ"},
        {"Electronic / EDM",       "EDM/Club DJ"},
        {"Christian Radio",        "Church Worship Leader"},
        {"Sports Talk",            "Sports Radio Host"},
        {"News / Talk",            "TV News Anchor"},
        {"Podcast Conversational", "Interview Podcast Host"},
        {"Bilingual (EN/ES)",      "Top 40 DJ"},
        {"AI Assistant (Neutral)", "Music Producer"}
    };

    const QString dbName = presetNameToDbName.value(personaName, personaName);
    if (m_sqliteMgr) {
        const auto personas = m_sqliteMgr->allPersonas();
        for (const auto& p : personas) {
            if (p.value("name").toString() == dbName)
                return p.value("system_prompt").toString();
        }
    }

    // Last resort: generic system prompt
    return QString("You are a %1. You are knowledgeable about music, "
                   "radio broadcasting, and playlist programming.").arg(personaName);
}

QString LibraryWidget::categoryDisplayName(qint64 categoryId) const {
    if (!m_sqliteMgr || categoryId <= 0) return QStringLiteral("Unknown");
    const auto cats = m_sqliteMgr->allCategories();
    for (const auto& cat : cats) {
        if (cat.value("id").toLongLong() == categoryId)
            return cat.value("name").toString();
    }
    return QStringLiteral("Unknown");
}

// ─── Task 1: AI DJ Agent Browser ──────────────────────────────────────────────

void LibraryWidget::onAiRecommendRequested(qint64 categoryId, const QString& personaName) {
    if (!m_aiIntel || !m_sqliteMgr) {
        QMessageBox::warning(this, QStringLiteral("AI: Recommend Tracks"),
            QStringLiteral("AI engine or database not available."));
        return;
    }

    const QString catName = categoryDisplayName(categoryId);
    const QString systemPrompt = lookupPersonaSystemPrompt(personaName);

    // Gather library data
    const auto allTracks = m_sqliteMgr->loadAll();
    const auto categoryTracks = m_sqliteMgr->tracksByCategory(categoryId);

    // Build library listing (max 200 tracks)
    QString librarySummary;
    const int maxLib = qMin(static_cast<int>(allTracks.size()), 200);
    for (int i = 0; i < maxLib; ++i) {
        const auto& t = allTracks[i];
        librarySummary += QString("  - %1 - %2\n")
            .arg(t.artist.isEmpty() ? QStringLiteral("Unknown Artist") : t.artist,
                 t.displayTitle());
    }
    if (allTracks.size() > 200)
        librarySummary += QString("  ... and %1 more tracks\n").arg(allTracks.size() - 200);

    // Gather Artist Intel for artists in the library
    const QStringList intelArtists = m_sqliteMgr->artistsWithIntel();
    QString artistIntelBlock;
    for (const auto& artistName : intelArtists) {
        const auto profiles = m_sqliteMgr->artistIntelProfiles(artistName);
        if (!profiles.isEmpty()) {
            QString profileText = profiles.first().value("profile_text").toString();
            if (profileText.length() > 500)
                profileText = profileText.left(500) + QStringLiteral("...");
            artistIntelBlock += QString("### %1\n%2\n\n").arg(artistName, profileText);
        }
    }

    // ── Create the AI DJ Agent dialog ────────────────────────────────────────
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("AI DJ Agent \xe2\x80\x94 %1").arg(personaName));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(700, 500);

    auto pal = ThemePalette::forCurrentTheme();
    const QString accentBtnText =
        ThemePalette::isLightTheme() ? QStringLiteral("#ffffff") : pal.text.name();
    dlg->setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QTextBrowser { background: %3; color: %2; border: 1px solid %4; "
        "  font-size: 13px; padding: 8px; }"
        "QLabel#AiDjStatusLabel { color: %5; font-size: 12px; padding: 2px 4px; }"
        "QLineEdit { background: %6; color: %2; border: 1px solid %4; "
        "  border-radius: 4px; padding: 6px 8px; font-size: 13px; }"
        "QPushButton { background: %7; color: %8; border: 1px solid %4; "
        "  border-radius: 4px; padding: 6px 16px; font-size: 12px; font-weight: 700; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { background: %3; color: %5; }")
        .arg(pal.bg.name(), pal.text.name(), pal.panelBg.name(),
             pal.border.name(), pal.textMuted.name(), pal.inputBg.name(),
             pal.accent.name(), accentBtnText));

    auto* mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ── Conversation browser ─────────────────────────────────────────────────
    auto* browser = new QTextBrowser(dlg);
    browser->setObjectName("AiDjAgentBrowser");
    browser->setOpenExternalLinks(false);
    mainLayout->addWidget(browser, 1);

    // ── Status bar ───────────────────────────────────────────────────────────
    auto* statusLbl = new QLabel(
        QString("Analyzing %1 tracks in your library...").arg(allTracks.size()), dlg);
    statusLbl->setObjectName("AiDjStatusLabel");
    mainLayout->addWidget(statusLbl);

    // ── Input row: text field + Send button ──────────────────────────────────
    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);
    auto* inputEdit = new QLineEdit(dlg);
    inputEdit->setPlaceholderText(QStringLiteral("Ask AI anything..."));
    inputEdit->setEnabled(false);
    auto* sendBtn = new QPushButton(QStringLiteral("Send"), dlg);
    sendBtn->setEnabled(false);
    inputRow->addWidget(inputEdit, 1);
    inputRow->addWidget(sendBtn);
    mainLayout->addLayout(inputRow);

    // ── Bottom button row ────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    auto* addAllBtn = new QPushButton(QStringLiteral("Add All to Category"), dlg);
    addAllBtn->setEnabled(false);
    btnRow->addWidget(addAllBtn);
    btnRow->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    btnRow->addWidget(closeBtn);
    mainLayout->addLayout(btnRow);

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    // ── Shared state for multi-turn conversation ─────────────────────────────
    struct DjAgentState {
        QStringList conversationHistory;
        QString lastAiResponse;
        int turnCounter = 0;
        QString systemPrompt;
        QString catName;
        qint64  categoryId = 0;
    };
    auto state = std::make_shared<DjAgentState>();
    state->systemPrompt = systemPrompt;
    state->catName = catName;
    state->categoryId = categoryId;

    // ── HTML rendering helpers (capture palette colors by value) ──────────────
    const QString palPanelBg  = pal.panelBg.name();
    const QString palAccent   = pal.accent.name();
    const QString palText     = pal.text.name();
    const QString palCardBg   = pal.cardBg.name();
    const QString palBorder   = pal.border.name();
    const QString palTextMuted = pal.textMuted.name();
    const QString palError    = pal.error.name();

    auto renderPersonaMsg = [palPanelBg, palAccent, palText](const QString& text) -> QString {
        QString html = text.toHtmlEscaped();
        html.replace(QRegularExpression(R"(\*\*(.+?)\*\*)"), QStringLiteral("<b>\\1</b>"));
        html.replace(QRegularExpression(R"(^## (.+)$)", QRegularExpression::MultilineOption),
                     QStringLiteral("<h3 style='margin:8px 0 4px 0;'>\\1</h3>"));
        html.replace(QRegularExpression(R"(^### (.+)$)", QRegularExpression::MultilineOption),
                     QStringLiteral("<h4 style='margin:6px 0 2px 0;'>\\1</h4>"));
        html.replace(QRegularExpression(R"(\n)"), QStringLiteral("<br>"));
        return QString(
            "<div style='background:%1; border-left:3px solid %2; "
            "padding:10px 12px; margin:6px 0; border-radius:4px; "
            "font-size:13px; color:%3;'>%4</div>")
            .arg(palPanelBg, palAccent, palText, html);
    };

    auto renderUserMsg = [palCardBg, palBorder, palTextMuted](const QString& text) -> QString {
        return QString(
            "<div style='background:%1; border-left:3px solid %2; "
            "padding:10px 12px; margin:6px 0; border-radius:4px; "
            "font-size:13px; color:%3;'><b>You:</b> %4</div>")
            .arg(palCardBg, palBorder, palTextMuted, text.toHtmlEscaped());
    };

    // Accumulate all HTML blocks for the conversation (shared across lambdas)
    auto htmlBlocks = std::make_shared<QStringList>();

    // Helper to refresh the browser with current HTML blocks
    auto refreshBrowser = [browser, htmlBlocks]() {
        browser->setHtml(
            QStringLiteral("<html><body style='margin:0;'>") +
            htmlBlocks->join(QStringLiteral("\n")) +
            QStringLiteral("</body></html>"));
        auto* sb = browser->verticalScrollBar();
        if (sb) sb->setValue(sb->maximum());
    };

    // ── Wire up AI response handler (persistent for multi-turn) ──────────────
    connect(m_aiIntel, &AiTrackIntel::customPromptReady, dlg,
        [state, htmlBlocks, renderPersonaMsg, refreshBrowser,
         browser, statusLbl, inputEdit, sendBtn, addAllBtn]
        (const QString& ctx, const QString& text,
         const QString& /*json*/, const QString& /*backend*/, const QString& /*model*/) {
            const QString expectedCtx =
                QString("ai-dj-%1-%2").arg(state->categoryId).arg(state->turnCounter);
            if (ctx != expectedCtx) return;

            state->lastAiResponse = text;
            state->conversationHistory.append(QStringLiteral("assistant: ") + text);

            htmlBlocks->append(renderPersonaMsg(text));
            refreshBrowser();

            statusLbl->setText(QStringLiteral("Ready. Ask a follow-up or add tracks."));
            inputEdit->setEnabled(true);
            sendBtn->setEnabled(true);
            addAllBtn->setEnabled(true);
            inputEdit->setFocus();
        });

    connect(m_aiIntel, &AiTrackIntel::customPromptFailed, dlg,
        [state, htmlBlocks, refreshBrowser, palPanelBg, palError, palTextMuted,
         statusLbl, inputEdit, sendBtn]
        (const QString& ctx, const QString& error) {
            const QString expectedCtx =
                QString("ai-dj-%1-%2").arg(state->categoryId).arg(state->turnCounter);
            if (ctx != expectedCtx) return;

            // Determine user-friendly error message
            QString friendlyMsg;
            QString statusMsg;
            if (error.contains("Connection refused", Qt::CaseInsensitive) ||
                error.contains("No route to host", Qt::CaseInsensitive)) {
                friendlyMsg = QStringLiteral(
                    "<b>AI Service Offline</b><br>"
                    "Cannot connect to the AI provider. Please check:<br>"
                    "&bull; Is Ollama running? (<code>ollama serve</code>)<br>"
                    "&bull; Is the URL correct in Preferences &gt; AI?<br>"
                    "&bull; Is the model downloaded? (<code>ollama pull llama3.2</code>)");
                statusMsg = QStringLiteral("AI offline \u2014 check Ollama is running");
            } else if (error.contains("not configured", Qt::CaseInsensitive) ||
                       error.contains("API key", Qt::CaseInsensitive)) {
                friendlyMsg = QStringLiteral(
                    "<b>AI Not Configured</b><br>"
                    "The AI provider needs to be set up in Preferences &gt; AI.<br>"
                    "Select a provider and enter your API key or model name.");
                statusMsg = QStringLiteral("AI not configured \u2014 check Preferences > AI");
            } else if (error.contains("timeout", Qt::CaseInsensitive) ||
                       error.contains("timed out", Qt::CaseInsensitive)) {
                friendlyMsg = QStringLiteral(
                    "<b>AI Request Timed Out</b><br>"
                    "The AI model took too long to respond. Try:<br>"
                    "&bull; Using a smaller/faster model<br>"
                    "&bull; Reducing the number of tracks in the category<br>"
                    "&bull; Checking if the model is still loading");
                statusMsg = QStringLiteral("Timed out \u2014 try a smaller model or fewer tracks");
            } else {
                friendlyMsg = QString("<b>AI Error</b><br>%1").arg(error.toHtmlEscaped());
                statusMsg = QStringLiteral("Error \u2014 you can try asking again");
            }

            htmlBlocks->append(QString(
                "<div style='background:%1; border-left:3px solid %2; "
                "padding:12px 14px; margin:6px 0; border-radius:4px; "
                "font-size:13px; color:%3;'>%4</div>")
                .arg(palPanelBg, palError, palTextMuted, friendlyMsg));
            refreshBrowser();

            statusLbl->setText(statusMsg);
            inputEdit->setEnabled(true);
            sendBtn->setEnabled(true);
        });

    // ── Send follow-up handler ───────────────────────────────────────────────
    auto sendFollowUp = [=]() {
        const QString userText = inputEdit->text().trimmed();
        if (userText.isEmpty()) return;

        inputEdit->clear();
        inputEdit->setEnabled(false);
        sendBtn->setEnabled(false);
        addAllBtn->setEnabled(false);
        statusLbl->setText(QStringLiteral("Thinking..."));

        state->conversationHistory.append(QStringLiteral("user: ") + userText);
        htmlBlocks->append(renderUserMsg(userText));
        refreshBrowser();

        // Advance turn counter for new context name
        state->turnCounter++;
        const QString ctxName =
            QString("ai-dj-%1-%2").arg(state->categoryId).arg(state->turnCounter);

        // Build combined prompt with conversation history
        QString historyBlock;
        for (const auto& entry : std::as_const(state->conversationHistory)) {
            historyBlock += entry + QStringLiteral("\n\n");
        }

        const QString followUpPrompt = QString(
            "Conversation so far:\n%1\n"
            "The user's latest message: \"%2\"\n\n"
            "Continue the conversation. Stay in character. "
            "If recommending tracks, only recommend tracks from the library list "
            "provided earlier. Format recommendations as numbered lists with "
            "**Artist** - Title (reason).")
            .arg(historyBlock, userText);

        m_aiIntel->sendCustomPrompt(ctxName, state->systemPrompt, followUpPrompt);
    };

    connect(sendBtn, &QPushButton::clicked, dlg, sendFollowUp);
    connect(inputEdit, &QLineEdit::returnPressed, dlg, sendFollowUp);

    // ── "Add All to Category" handler ────────────────────────────────────────
    connect(addAllBtn, &QPushButton::clicked, dlg,
        [this, state, dlg]() {
            if (!m_sqliteMgr || state->lastAiResponse.isEmpty()) return;

            const auto libTracks = m_sqliteMgr->loadAll();
            int matched = 0;

            // Parse the latest AI response for "N. **Artist** - Title" patterns
            QRegularExpression lineRe(
                R"(^\d+\.\s+\*{0,2}(.+?)\*{0,2}\s*[-\u2013\u2014]\s*(.+?)(?:\s*\(|$))",
                QRegularExpression::MultilineOption);
            auto it = lineRe.globalMatch(state->lastAiResponse);

            while (it.hasNext()) {
                auto match = it.next();
                const QString sugArtist = match.captured(1).trimmed()
                    .remove(QRegularExpression(R"(\*+)"));
                const QString sugTitle  = match.captured(2).trimmed()
                    .remove(QRegularExpression(R"(\*+)"));

                for (const auto& t : libTracks) {
                    const bool artistMatch =
                        t.artist.compare(sugArtist, Qt::CaseInsensitive) == 0 ||
                        t.artist.contains(sugArtist, Qt::CaseInsensitive);
                    const bool titleMatch =
                        t.displayTitle().compare(sugTitle, Qt::CaseInsensitive) == 0 ||
                        t.displayTitle().contains(sugTitle, Qt::CaseInsensitive);

                    if (artistMatch && titleMatch && t.id > 0) {
                        m_sqliteMgr->assignTrackToCategory(t.id, state->categoryId);
                        ++matched;
                        break;
                    }
                }
            }

            if (m_categoryPanel) m_categoryPanel->reload();
            loadCategoryTracks(m_activeCategory);

            QMessageBox::information(dlg,
                QStringLiteral("Tracks Added"),
                QString("Matched and added %1 track(s) to \"%2\".")
                    .arg(matched).arg(state->catName));
        });

    // ── Show the intro message immediately ───────────────────────────────────
    const QString introHtml = QString(
        "<div style='background:%1; border-left:3px solid %2; "
        "padding:10px 12px; margin:6px 0; border-radius:4px; "
        "font-size:13px; color:%3;'>"
        "<b>Hey!</b> I'm your <b>%4</b>. Let me dig into your library and find "
        "some gems for the <b>%5</b> rotation...<br><br>"
        "<em>Library: %6 tracks | Category: %7 tracks | Intel: %8 artists</em>"
        "</div>")
        .arg(palPanelBg, palAccent, palText,
             personaName.toHtmlEscaped(), catName.toHtmlEscaped())
        .arg(allTracks.size()).arg(categoryTracks.size()).arg(intelArtists.size());
    htmlBlocks->append(introHtml);

    // Add a "thinking" message
    const QString thinkingHtml = QString(
        "<div style='background:%1; border-left:3px solid %2; "
        "padding:10px 12px; margin:6px 0; border-radius:4px; "
        "font-size:13px; color:%3; font-style:italic;'>"
        "&#9889; Sending %4 tracks to AI for analysis... this may take 15-30 seconds "
        "depending on your model. Please wait."
        "</div>")
        .arg(palPanelBg, palBorder, palTextMuted)
        .arg(qMin(static_cast<int>(allTracks.size()), 200));
    htmlBlocks->append(thinkingHtml);
    refreshBrowser();

    // ── Build and send the initial AI request ────────────────────────────────
    QString artistIntelSection;
    if (!artistIntelBlock.isEmpty()) {
        artistIntelSection = QString(
            "\n\nHere is saved Artist Intel for some artists in the library "
            "(use this for deeper recommendations if relevant):\n%1")
            .arg(artistIntelBlock);
    }

    const QString initialUserPrompt = QString(
        "You are starting a conversation with a broadcaster. "
        "Introduce yourself briefly (2 sentences max), then recommend 10 tracks "
        "from this library for the '%1' category. For each track, explain why "
        "in 1 sentence. If you have intel on any artist, reference it.\n\n"
        "IMPORTANT: Only recommend tracks from this list. Do not make up tracks.\n\n"
        "Here are all the tracks in the library (up to 200):\n%2\n"
        "%3\n"
        "Format your recommendations as:\n"
        "## Recommended Tracks\n"
        "1. **Artist** - Track Title (reason)\n"
        "2. **Artist** - Track Title (reason)\n"
        "...")
        .arg(catName, librarySummary, artistIntelSection);

    state->conversationHistory.append(QStringLiteral("user: ") + initialUserPrompt);

    const QString ctxName =
        QString("ai-dj-%1-%2").arg(categoryId).arg(state->turnCounter);

    qInfo() << "[LibraryWidget] AI DJ Agent sending prompt to" << ctxName
            << "| tracks:" << allTracks.size()
            << "| prompt length:" << initialUserPrompt.size() << "chars";

    statusLbl->setText(QString(
        "Sending %1 tracks to AI for analysis... please wait (15-30 sec)")
        .arg(qMin(static_cast<int>(allTracks.size()), 200)));

    m_aiIntel->sendCustomPrompt(ctxName, systemPrompt, initialUserPrompt);

    // Start animated status timer with spinning dots
    auto* elapsedTimer = new QTimer(dlg);
    auto elapsedSec = std::make_shared<int>(0);
    static const QString spinFrames[] = {
        QStringLiteral("\xe2\xa0\x8b"), QStringLiteral("\xe2\xa0\x99"),
        QStringLiteral("\xe2\xa0\xb9"), QStringLiteral("\xe2\xa0\xb8"),
        QStringLiteral("\xe2\xa0\xbc"), QStringLiteral("\xe2\xa0\xb4"),
        QStringLiteral("\xe2\xa0\xa6"), QStringLiteral("\xe2\xa0\xa7"),
        QStringLiteral("\xe2\xa0\x87"), QStringLiteral("\xe2\xa0\x8f")
    };
    connect(elapsedTimer, &QTimer::timeout, dlg, [statusLbl, elapsedSec, allTracks]() {
        (*elapsedSec)++;
        const QString spinner = spinFrames[(*elapsedSec) % 10];
        QString phase;
        if (*elapsedSec < 5)       phase = QStringLiteral("Sending tracks to AI model");
        else if (*elapsedSec < 15) phase = QStringLiteral("AI analyzing your library");
        else if (*elapsedSec < 30) phase = QStringLiteral("AI selecting recommendations");
        else if (*elapsedSec < 60) phase = QStringLiteral("AI composing response (large library)");
        else                       phase = QStringLiteral("Still working \u2014 large context, be patient");

        statusLbl->setText(QString("%1  %2... %3s (%4 tracks)")
            .arg(spinner, phase).arg(*elapsedSec)
            .arg(qMin(static_cast<int>(allTracks.size()), 200)));
    });
    elapsedTimer->start(500);

    // Stop timer when response arrives
    connect(m_aiIntel, &AiTrackIntel::customPromptReady, dlg,
        [elapsedTimer](const QString&, const QString&, const QString&,
                       const QString&, const QString&) {
            elapsedTimer->stop();
        });
    connect(m_aiIntel, &AiTrackIntel::customPromptFailed, dlg,
        [elapsedTimer](const QString&, const QString&) {
            elapsedTimer->stop();
        });

    dlg->show();
}

// ─── Task 2: Playlist Generator Pro ───────────────────────────────────────────

void LibraryWidget::onAiGeneratePlaylistRequested(qint64 categoryId,
                                                   const QString& personaName) {
    if (!m_aiIntel || !m_sqliteMgr) {
        QMessageBox::warning(this, QStringLiteral("Playlist Generator Pro"),
            QStringLiteral("AI engine or database not available."));
        return;
    }

    const QString catName = categoryDisplayName(categoryId);
    const auto categoryTracks = m_sqliteMgr->tracksByCategory(categoryId);
    auto pal = ThemePalette::forCurrentTheme();
    const QString accentBtnText =
        ThemePalette::isLightTheme() ? QStringLiteral("#ffffff") : pal.text.name();

    // ── Build the main dialog (800x600, config left + playlist right) ─────────
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("Playlist Generator Pro \xe2\x80\x94 %1").arg(catName));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(800, 600);

    dlg->setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QLineEdit, QComboBox, QCheckBox, QSpinBox { font-size: 12px; "
        "  background: %3; color: %2; border: 1px solid %4; border-radius: 3px; padding: 3px; }"
        "QGroupBox { font-size: 12px; font-weight: bold; border: 1px solid %4; "
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; color: %2; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QListWidget { background: %3; color: %2; border: 1px solid %4; "
        "  font-size: 12px; }"
        "QListWidget::item { padding: 4px 6px; border-bottom: 1px solid %4; }"
        "QListWidget::item:selected { background: %5; color: %6; }"
        "QLabel#PgpStatusLabel { color: %7; font-size: 12px; padding: 2px 4px; }"
        "QLabel#PgpTotalsLabel { color: %2; font-size: 12px; font-weight: bold; padding: 4px; }"
        "QPushButton { background: %5; color: %6; border: 1px solid %4; "
        "  border-radius: 4px; padding: 6px 16px; font-size: 12px; font-weight: 700; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:disabled { background: %3; color: %7; }")
        .arg(pal.bg.name(), pal.text.name(), pal.panelBg.name(),
             pal.border.name(), pal.accent.name(), accentBtnText,
             pal.textMuted.name()));

    auto* outerLayout = new QVBoxLayout(dlg);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    // ── Top area: splitter with config (left) + playlist output (right) ───────
    auto* splitter = new QSplitter(Qt::Horizontal, dlg);
    splitter->setHandleWidth(3);

    // ── Left panel: CONFIG ────────────────────────────────────────────────────
    auto* leftWidget = new QWidget(splitter);
    leftWidget->setMinimumWidth(200);
    leftWidget->setMaximumWidth(260);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->setSpacing(6);

    auto* cfgGroup = new QGroupBox(QStringLiteral("Configuration"), leftWidget);
    auto* cfgForm = new QFormLayout(cfgGroup);
    cfgForm->setSpacing(6);

    auto* nameEdit = new QLineEdit(leftWidget);
    nameEdit->setText(QString("%1 - AI Playlist").arg(catName));
    nameEdit->setPlaceholderText(QStringLiteral("Enter playlist name..."));
    cfgForm->addRow(QStringLiteral("Name:"), nameEdit);

    // Source category selector — All Tracks or any named category
    auto* sourceCombo = new QComboBox(leftWidget);
    sourceCombo->addItem(QStringLiteral("All Tracks"), QVariant::fromValue<qint64>(-1));
    {
        const auto cats = m_sqliteMgr->allCategories();
        for (const auto& cat : cats) {
            const qint64 cid = cat.value("id").toLongLong();
            const QString cname = cat.value("name").toString();
            const int cnt = cat.value("track_count", 0).toInt();
            sourceCombo->addItem(QString("%1 (%2)").arg(cname).arg(cnt),
                                 QVariant::fromValue<qint64>(cid));
        }
    }
    // Pre-select the category we were invoked from
    for (int i = 0; i < sourceCombo->count(); ++i) {
        if (sourceCombo->itemData(i).toLongLong() == categoryId) {
            sourceCombo->setCurrentIndex(i);
            break;
        }
    }
    cfgForm->addRow(QStringLiteral("Source:"), sourceCombo);

    auto* prioritizeIntel = new QCheckBox(QStringLiteral("Prioritize tracks with AI Intel"), leftWidget);
    prioritizeIntel->setChecked(true);
    prioritizeIntel->setToolTip(QStringLiteral(
        "Tracks that have saved AI Intel reports will be preferred.\n"
        "The AI DJ uses stored artist profiles for smarter selections."));
    cfgForm->addRow(prioritizeIntel);

    auto* durationCombo = new QComboBox(leftWidget);
    durationCombo->addItems({
        QStringLiteral("30 Minutes"),
        QStringLiteral("1 Hour"),
        QStringLiteral("2 Hours"),
        QStringLiteral("4 Hours"),
        QStringLiteral("Full Day (24h)")
    });
    durationCombo->setCurrentIndex(1);  // default 1 Hour
    cfgForm->addRow(QStringLiteral("Duration:"), durationCombo);

    auto* daypartCombo = new QComboBox(leftWidget);
    daypartCombo->addItems({
        QStringLiteral("Morning (6am-10am)"),
        QStringLiteral("Midday (10am-2pm)"),
        QStringLiteral("Afternoon (2pm-6pm)"),
        QStringLiteral("Evening (6pm-10pm)"),
        QStringLiteral("Overnight (10pm-6am)"),
        QStringLiteral("Any / No Preference")
    });
    cfgForm->addRow(QStringLiteral("Daypart:"), daypartCombo);

    leftLayout->addWidget(cfgGroup);

    // ── Broadcast elements checkboxes ─────────────────────────────────────────
    auto* elemGroup = new QGroupBox(QStringLiteral("Broadcast Elements"), leftWidget);
    auto* elemVLayout = new QVBoxLayout(elemGroup);
    elemVLayout->setSpacing(3);

    auto* chkStingers = new QCheckBox(QStringLiteral("Stingers"), leftWidget);
    auto* chkSweepers = new QCheckBox(QStringLiteral("Sweepers"), leftWidget);
    auto* chkIds      = new QCheckBox(QStringLiteral("Station IDs"), leftWidget);
    auto* chkAds      = new QCheckBox(QStringLiteral("Ad Breaks"), leftWidget);
    auto* chkJingles  = new QCheckBox(QStringLiteral("Jingles"), leftWidget);
    chkStingers->setChecked(true);
    chkSweepers->setChecked(true);
    chkIds->setChecked(true);
    chkAds->setChecked(true);
    chkJingles->setChecked(true);
    elemVLayout->addWidget(chkStingers);
    elemVLayout->addWidget(chkSweepers);
    elemVLayout->addWidget(chkIds);
    elemVLayout->addWidget(chkAds);
    elemVLayout->addWidget(chkJingles);
    leftLayout->addWidget(elemGroup);

    // ── Generate button ───────────────────────────────────────────────────────
    auto* generateBtn = new QPushButton(QStringLiteral("Generate"), leftWidget);
    leftLayout->addWidget(generateBtn);

    // ── Status area ───────────────────────────────────────────────────────────
    auto* statusGroup = new QGroupBox(QStringLiteral("Status"), leftWidget);
    auto* statusVLayout = new QVBoxLayout(statusGroup);
    auto* statusLbl = new QLabel(QStringLiteral("Ready. Configure and click Generate."), leftWidget);
    statusLbl->setObjectName("PgpStatusLabel");
    statusLbl->setWordWrap(true);
    statusVLayout->addWidget(statusLbl);
    leftLayout->addWidget(statusGroup);

    leftLayout->addStretch(1);
    splitter->addWidget(leftWidget);

    // ── Right panel: PLAYLIST OUTPUT ──────────────────────────────────────────
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);

    auto* outputLabel = new QLabel(QStringLiteral("Playlist Output"), rightWidget);
    outputLabel->setStyleSheet(QString("font-size: 13px; font-weight: bold; color: %1; padding: 2px;")
        .arg(pal.text.name()));
    rightLayout->addWidget(outputLabel);

    auto* playlistList = new QListWidget(rightWidget);
    playlistList->setObjectName("PgpPlaylistOutput");
    playlistList->setSelectionMode(QAbstractItemView::NoSelection);
    playlistList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    rightLayout->addWidget(playlistList, 1);

    splitter->addWidget(rightWidget);
    splitter->setSizes({220, 560});
    outerLayout->addWidget(splitter, 1);

    // ── Bottom bar: totals + buttons ──────────────────────────────────────────
    auto* bottomBar = new QHBoxLayout;
    bottomBar->setSpacing(8);
    auto* totalsLbl = new QLabel(QStringLiteral("Tracks: 0 | Duration: 0:00"), dlg);
    totalsLbl->setObjectName("PgpTotalsLabel");
    bottomBar->addWidget(totalsLbl, 1);

    auto* saveBtn = new QPushButton(QStringLiteral("Save Playlist"), dlg);
    saveBtn->setEnabled(false);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    bottomBar->addWidget(saveBtn);
    bottomBar->addWidget(closeBtn);
    outerLayout->addLayout(bottomBar);

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    // ── Shared state for the generation process ───────────────────────────────
    struct PgpState {
        QList<MediaItem> categoryTracks;
        QList<QPair<qint64, int>> matchedTracks;  // trackId, durationMs
        int totalDurationMs = 0;
        int targetDurationMs = 0;
        int trackCount = 0;
        QString playlistName;
        qint64 categoryId = 0;
        bool generating = false;
    };
    auto state = std::make_shared<PgpState>();
    state->categoryTracks = categoryTracks;
    state->categoryId = categoryId;

    // ── Duration map (index -> seconds) ───────────────────────────────────────
    static const int durationSecs[] = {1800, 3600, 7200, 14400, 86400};
    static const char* durationLabels[] = {
        "30 minutes", "1 hour", "2 hours", "4 hours", "24 hours (full day)"
    };

    // ── Element colors from ThemePalette ──────────────────────────────────────
    const QString colStinger = pal.warning.name();   // amber
    const QString colAdBreak = pal.error.name();     // red
    const QString colSweeper = pal.info.name();      // blue
    const QString colStationId = pal.success.name(); // green
    const QString colJingle  = QStringLiteral("#a855f7"); // purple

    // Capture palette color names by value for use in async lambdas
    const QColor palTextColor = pal.text;
    const QColor palMutedColor = pal.textMuted;

    // ── Helper: add a broadcast element separator row ─────────────────────────
    auto addElementRow = [playlistList](const QString& label, const QString& color) {
        auto* item = new QListWidgetItem();
        item->setText(QString("  %1  ").arg(label));
        item->setData(Qt::UserRole, QStringLiteral("element"));
        item->setBackground(QColor(color).darker(300));
        item->setForeground(QColor(color));
        QFont f = item->font();
        f.setBold(true);
        f.setPointSize(9);
        item->setFont(f);
        item->setTextAlignment(Qt::AlignCenter);
        item->setFlags(Qt::ItemIsEnabled);
        playlistList->addItem(item);
    };

    // ── Helper: add a matched music track row ─────────────────────────────────
    auto addTrackRow = [playlistList, palTextColor, state]
        (int num, const MediaItem& t) {
        const QString text = QString("[%1]  %2 \xe2\x80\x94 %3 (%4)  [%5]")
            .arg(num)
            .arg(t.artist.isEmpty() ? QStringLiteral("Unknown") : t.artist)
            .arg(t.displayTitle())
            .arg(t.album.isEmpty() ? QStringLiteral("--") : t.album)
            .arg(t.durationString());
        auto* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, QStringLiteral("track"));
        item->setData(Qt::UserRole + 1, QVariant::fromValue<qint64>(t.id));
        item->setForeground(palTextColor);
        QFont f = item->font();
        f.setPointSize(9);
        item->setFont(f);
        item->setFlags(Qt::ItemIsEnabled);
        playlistList->addItem(item);
        state->matchedTracks.append({t.id, t.durationMs});
        state->totalDurationMs += t.durationMs;
        state->trackCount++;
    };

    // ── Helper: update totals label ───────────────────────────────────────────
    auto updateTotals = [totalsLbl, state]() {
        const int totalSec = state->totalDurationMs / 1000;
        const int targetSec = state->targetDurationMs / 1000;
        const QString totalStr = QString("%1:%2")
            .arg(totalSec / 60).arg(totalSec % 60, 2, 10, QChar('0'));
        const QString targetStr = QString("%1:%2")
            .arg(targetSec / 60).arg(targetSec % 60, 2, 10, QChar('0'));
        totalsLbl->setText(QString("Tracks: %1 | Duration: %2 / %3")
            .arg(state->trackCount).arg(totalStr, targetStr));
    };

    // ── GENERATE button handler ───────────────────────────────────────────────
    connect(generateBtn, &QPushButton::clicked, dlg,
        [=]() {
            if (state->generating) return;

            const QString plName = nameEdit->text().trimmed();
            if (plName.isEmpty()) {
                QMessageBox::warning(dlg, QStringLiteral("Playlist Generator Pro"),
                    QStringLiteral("Playlist name cannot be empty."));
                return;
            }
            state->playlistName = plName;

            const int dIdx = durationCombo->currentIndex();
            state->targetDurationMs = durationSecs[dIdx] * 1000;
            const QString durationStr = QString::fromUtf8(durationLabels[dIdx]);
            const QString daypartText = daypartCombo->currentText();

            // Check track count warning
            const int trackCt = static_cast<int>(state->categoryTracks.size());
            if (trackCt < 50) {
                auto reply = QMessageBox::question(dlg,
                    QStringLiteral("Low Track Count"),
                    QString("Add 50+ tracks to this category for best results.\n"
                            "Currently: %1 tracks. Generate anyway?").arg(trackCt),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
                if (reply != QMessageBox::Yes) return;
            }

            // Reset state
            state->matchedTracks.clear();
            state->totalDurationMs = 0;
            state->trackCount = 0;
            state->generating = true;
            playlistList->clear();
            saveBtn->setEnabled(false);
            generateBtn->setEnabled(false);
            updateTotals();

            statusLbl->setText(QStringLiteral("Building track catalog for AI..."));

            // ── Resolve source tracks from selected category or All Tracks ────
            const qint64 sourceId = sourceCombo->currentData().toLongLong();
            QList<M1::MediaItem> sourceTracks;
            if (sourceId < 0) {
                sourceTracks = m_sqliteMgr->loadAll();  // All Tracks
                statusLbl->setText(QString("Building catalog from ALL %1 tracks...")
                    .arg(sourceTracks.size()));
            } else {
                sourceTracks = m_sqliteMgr->tracksByCategory(sourceId);
                statusLbl->setText(QString("Building catalog from %1 tracks in %2...")
                    .arg(sourceTracks.size()).arg(sourceCombo->currentText()));
            }
            // Update state with resolved source tracks
            state->categoryTracks = sourceTracks;

            // ── Gather AI Intel for prioritization ────────────────────────────
            const QStringList intelArtists = m_sqliteMgr->artistsWithIntel();
            QSet<QString> intelArtistSet(intelArtists.begin(), intelArtists.end());
            const bool useIntel = prioritizeIntel->isChecked();

            // ── Build the track catalog, starring intel tracks ────────────────
            QString trackCatalog;
            int intelCount = 0;
            for (const auto& t : sourceTracks) {
                const bool hasIntel = useIntel &&
                    (intelArtistSet.contains(t.artist) || intelArtistSet.contains(t.albumArtist));

                if (hasIntel) {
                    trackCatalog += QStringLiteral("[AI-INTEL] ");
                    ++intelCount;
                }
                trackCatalog += QString("%1 - %2")
                    .arg(t.artist.isEmpty() ? QStringLiteral("Unknown") : t.artist,
                         t.displayTitle());
                if (!t.album.isEmpty())
                    trackCatalog += QString(" (%1)").arg(t.album);
                if (!t.year.isEmpty())
                    trackCatalog += QString(" [%1]").arg(t.year);
                if (t.durationMs > 0)
                    trackCatalog += QString(" {%1}").arg(t.durationString());
                if (!t.genre.isEmpty())
                    trackCatalog += QString(" <%1>").arg(t.genre);
                if (t.bpm > 0)
                    trackCatalog += QString(" %1bpm").arg(static_cast<int>(t.bpm));
                trackCatalog += QStringLiteral("\n");
            }
            if (trackCatalog.isEmpty())
                trackCatalog = QStringLiteral("(no tracks available)\n");

            // ── Build AI Intel context snippets for prioritized tracks ─────────
            QString intelContext;
            if (useIntel && !intelArtists.isEmpty()) {
                statusLbl->setText(QString("Loading AI Intel for %1 artists...").arg(intelArtists.size()));
                for (const auto& name : intelArtists) {
                    const auto profiles = m_sqliteMgr->artistIntelProfiles(name);
                    if (!profiles.isEmpty()) {
                        QString snippet = profiles.first().value("profile_text").toString();
                        if (snippet.length() > 300)
                            snippet = snippet.left(300) + QStringLiteral("...");
                        intelContext += QString("**%1**: %2\n").arg(name, snippet);
                    }
                }
            }

            // ── Build element insertion rules ─────────────────────────────────
            QStringList elemRules;
            if (chkStingers->isChecked())
                elemRules << QStringLiteral("[STINGER] every 3-4 songs");
            if (chkSweepers->isChecked())
                elemRules << QStringLiteral("[SWEEPER] every 6-8 songs");
            if (chkIds->isChecked())
                elemRules << QStringLiteral("[STATION ID] at position 1 and every 20-25 songs");
            if (chkAds->isChecked())
                elemRules << QStringLiteral("[AD BREAK] every 15-20 minutes of music");
            if (chkJingles->isChecked())
                elemRules << QStringLiteral("[JINGLE] after each ad break");

            const QString elemDesc = elemRules.isEmpty()
                ? QStringLiteral("Do NOT include any broadcast element markers.")
                : QString("Insert these broadcast elements at the specified intervals:\n%1")
                      .arg(elemRules.join(QStringLiteral("\n")));

            const QString systemPrompt = lookupPersonaSystemPrompt(personaName);

            // Build intel prioritization instruction
            QString intelInstruction;
            if (useIntel && intelCount > 0) {
                intelInstruction = QString(
                    "\n\nPRIORITY: Tracks marked [AI-INTEL] have saved artist intelligence. "
                    "Prefer these tracks when building the playlist (%1 of %2 tracks have intel). "
                    "Here is context about these artists to help your selections:\n%3\n")
                    .arg(intelCount).arg(sourceTracks.size()).arg(intelContext);
            }

            const QString sourceLabel = sourceId < 0
                ? QStringLiteral("your full library")
                : sourceCombo->currentText();

            const QString userPrompt = QString(
                "Create a %1 broadcast playlist for the %2 daypart.\n\n"
                "Rules:\n"
                "- Artist separation: no same artist within 4 tracks\n"
                "- No back-to-back same genre\n"
                "- Energy curve: build-peak-cool-build pattern\n"
                "- %3\n"
                "%4\n\n"
                "Available tracks from %5 (use ONLY these, Artist - Title format):\n%6\n"
                "Format your output EXACTLY as a numbered list. "
                "For music tracks use: N. Artist - Title\n"
                "For broadcast elements use the marker on its own line: [STINGER], "
                "[SWEEPER], [STATION ID], [AD BREAK], [JINGLE]\n\n"
                "Do NOT add commentary, reasons, album names, or durations. "
                "ONLY output the numbered/marked list. Start now.")
                .arg(durationStr, daypartText, elemDesc, intelInstruction,
                     sourceLabel, trackCatalog);

            qInfo() << "[PlaylistGeneratorPro] Building playlist from"
                    << sourceTracks.size() << "tracks (" << intelCount << "with intel)"
                    << "| prompt length:" << userPrompt.size() << "chars";

            statusLbl->setText(QString("AI DJ building playlist from %1 tracks (%2 with intel)...")
                .arg(sourceTracks.size()).arg(intelCount));

            // ── Animated progress timer ───────────────────────────────────────
            auto* pgpTimer = new QTimer(dlg);
            auto pgpSec = std::make_shared<int>(0);
            static const QString pgpSpinFrames[] = {
                QStringLiteral("\xe2\xa0\x8b"), QStringLiteral("\xe2\xa0\x99"),
                QStringLiteral("\xe2\xa0\xb9"), QStringLiteral("\xe2\xa0\xb8"),
                QStringLiteral("\xe2\xa0\xbc"), QStringLiteral("\xe2\xa0\xb4"),
                QStringLiteral("\xe2\xa0\xa6"), QStringLiteral("\xe2\xa0\xa7"),
                QStringLiteral("\xe2\xa0\x87"), QStringLiteral("\xe2\xa0\x8f")
            };
            connect(pgpTimer, &QTimer::timeout, dlg, [statusLbl, pgpSec, sourceTracks, intelCount]() {
                (*pgpSec)++;
                const QString spinner = pgpSpinFrames[(*pgpSec) % 10];
                QString phase;
                if (*pgpSec < 5)       phase = QStringLiteral("Sending track catalog to AI");
                else if (*pgpSec < 15) phase = QStringLiteral("AI analyzing tracks & energy flow");
                else if (*pgpSec < 30) phase = QStringLiteral("AI building playlist order");
                else if (*pgpSec < 45) phase = QStringLiteral("AI inserting broadcast elements");
                else if (*pgpSec < 60) phase = QStringLiteral("Finalizing playlist (large catalog)");
                else                   phase = QStringLiteral("Still generating \u2014 be patient");

                statusLbl->setText(QString("%1  %2... %3s | %4 tracks (%5 intel)")
                    .arg(spinner, phase).arg(*pgpSec)
                    .arg(sourceTracks.size()).arg(intelCount));
            });
            pgpTimer->start(500);

            // ── Send to AI ────────────────────────────────────────────────────
            const QString contextName = QString("ai-pgp-%1").arg(categoryId);

            auto* connReady = new QMetaObject::Connection;
            auto* connFail  = new QMetaObject::Connection;

            *connReady = connect(m_aiIntel, &AiTrackIntel::customPromptReady, dlg,
                [=](const QString& ctx, const QString& text, const QString& /*json*/,
                     const QString& /*backend*/, const QString& /*model*/) {
                    if (ctx != contextName) return;
                    pgpTimer->stop();
                    qInfo() << "[PlaylistGeneratorPro] AI response:" << text.size() << "chars";
                    QObject::disconnect(*connReady);
                    QObject::disconnect(*connFail);
                    delete connReady;
                    delete connFail;

                    // ── Parse AI response line by line ────────────────────────
                    qInfo() << "[PlaylistGeneratorPro] Raw AI response:\n" << text.left(2000);

                    const QStringList lines = text.split(QRegularExpression(R"([\r\n]+)"),
                        Qt::SkipEmptyParts);
                    int trackNum = 0;

                    // Track line regex: permissive — handles many AI output formats:
                    // "1. Artist - Title", "1) Artist - Title", "1. N. Artist - Title"
                    // "1. **Artist** - Title (reason)", etc.
                    // Uses \x{2013}\x{2014} for en-dash/em-dash (PCRE2 Unicode escapes)
                    QRegularExpression trackRe(
                        R"(^\d+[\.\)]\s*(?:N\.\s*)?\*{0,2}(.+?)\*{0,2}\s*[-\x{2013}\x{2014}]\s*\*{0,2}(.+?)\*{0,2}(?:\s*[\(\[].*)?$)");
                    // Element line regex: "[STINGER]", "[AD BREAK]", etc.
                    QRegularExpression elemRe(
                        R"(\[(STINGER|SWEEPER|STATION ID|AD BREAK|JINGLE)\])",
                        QRegularExpression::CaseInsensitiveOption);

                    for (const QString& rawLine : lines) {
                        const QString line = rawLine.trimmed();
                        if (line.isEmpty()) continue;

                        // Check for broadcast element markers
                        auto elemMatch = elemRe.match(line);
                        if (elemMatch.hasMatch()) {
                            const QString tag = elemMatch.captured(1).toUpper();
                            QString color;
                            if (tag == QStringLiteral("STINGER"))    color = colStinger;
                            else if (tag == QStringLiteral("AD BREAK"))   color = colAdBreak;
                            else if (tag == QStringLiteral("SWEEPER"))    color = colSweeper;
                            else if (tag == QStringLiteral("STATION ID")) color = colStationId;
                            else if (tag == QStringLiteral("JINGLE"))     color = colJingle;
                            else color = palMutedColor.name();

                            addElementRow(QString("[%1]").arg(tag), color);
                            statusLbl->setText(QString("Inserting %1...").arg(tag.toLower()));
                            continue;
                        }

                        // Check for music track line
                        auto trkMatch = trackRe.match(line);
                        if (trkMatch.hasMatch()) {
                            const QString sugArtist = trkMatch.captured(1).trimmed()
                                .remove(QRegularExpression(R"(\*+)"));
                            const QString sugTitle  = trkMatch.captured(2).trimmed()
                                .remove(QRegularExpression(R"(\*+)"));

                            // Match against category tracks
                            bool found = false;
                            for (const auto& t : state->categoryTracks) {
                                const bool artistMatch =
                                    t.artist.compare(sugArtist, Qt::CaseInsensitive) == 0 ||
                                    t.artist.contains(sugArtist, Qt::CaseInsensitive) ||
                                    sugArtist.contains(t.artist, Qt::CaseInsensitive);
                                const bool titleMatch =
                                    t.displayTitle().compare(sugTitle, Qt::CaseInsensitive) == 0 ||
                                    t.displayTitle().contains(sugTitle, Qt::CaseInsensitive) ||
                                    sugTitle.contains(t.displayTitle(), Qt::CaseInsensitive);

                                if (artistMatch && titleMatch && t.id > 0) {
                                    trackNum++;
                                    addTrackRow(trackNum, t);
                                    found = true;
                                    statusLbl->setText(
                                        QString("AI DJ selecting track %1...").arg(trackNum));
                                    break;
                                }
                            }

                            // If no match, show as unmatched
                            if (!found) {
                                trackNum++;
                                const QString unmatchedText = QString("[%1]  %2 \xe2\x80\x94 %3  [unmatched]")
                                    .arg(trackNum).arg(sugArtist, sugTitle);
                                auto* item = new QListWidgetItem(unmatchedText);
                                item->setData(Qt::UserRole, QStringLiteral("unmatched"));
                                item->setForeground(palMutedColor);
                                QFont f = item->font();
                                f.setItalic(true);
                                f.setPointSize(9);
                                item->setFont(f);
                                item->setFlags(Qt::ItemIsEnabled);
                                playlistList->addItem(item);
                            }
                        }
                    }

                    updateTotals();
                    state->generating = false;
                    generateBtn->setEnabled(true);
                    saveBtn->setEnabled(state->trackCount > 0);

                    if (state->trackCount == 0 && playlistList->count() == 0) {
                        // No tracks parsed — show the raw AI response so user can see what happened
                        statusLbl->setText(QStringLiteral(
                            "AI returned no parseable tracks. Showing raw response below."));
                        auto* rawItem = new QListWidgetItem(
                            QStringLiteral("[RAW AI RESPONSE]\n") + text);
                        rawItem->setForeground(palMutedColor);
                        QFont f = rawItem->font();
                        f.setItalic(true);
                        rawItem->setFont(f);
                        playlistList->addItem(rawItem);
                    } else {
                        statusLbl->setText(
                            QString("Done! %1 tracks matched, %2 total items.")
                                .arg(state->trackCount)
                                .arg(playlistList->count()));
                    }

                    // Scroll to top
                    playlistList->scrollToTop();
                });

            *connFail = connect(m_aiIntel, &AiTrackIntel::customPromptFailed, dlg,
                [=](const QString& ctx, const QString& error) {
                    if (ctx != contextName) return;
                    pgpTimer->stop();
                    qWarning() << "[PlaylistGeneratorPro] AI error:" << error;
                    QObject::disconnect(*connReady);
                    QObject::disconnect(*connFail);
                    delete connReady;
                    delete connFail;

                    state->generating = false;
                    generateBtn->setEnabled(true);

                    // User-friendly error messages
                    if (error.contains("Connection refused", Qt::CaseInsensitive))
                        statusLbl->setText(QStringLiteral("AI offline \u2014 is Ollama running?"));
                    else if (error.contains("API key", Qt::CaseInsensitive))
                        statusLbl->setText(QStringLiteral("AI not configured \u2014 check Preferences > AI"));
                    else
                        statusLbl->setText(QString("AI Error: %1").arg(error.left(120)));
                });

            qInfo() << "[PlaylistGeneratorPro] Sending prompt to AI:" << contextName;
            m_aiIntel->sendCustomPrompt(contextName, systemPrompt, userPrompt);
        });

    // ── SAVE PLAYLIST button handler ──────────────────────────────────────────
    connect(saveBtn, &QPushButton::clicked, dlg,
        [this, dlg, state, statusLbl]() {
            if (!m_sqliteMgr || state->matchedTracks.isEmpty()) return;

            const qint64 plId = m_sqliteMgr->createPlaylist(
                state->playlistName,
                QString("AI Playlist Generator Pro - %1")
                    .arg(categoryDisplayName(state->categoryId)));
            if (plId <= 0) {
                QMessageBox::warning(dlg, QStringLiteral("Save Error"),
                    QStringLiteral("Failed to create playlist in database."));
                return;
            }

            int pos = 0;
            for (const auto& [trackId, durMs] : state->matchedTracks) {
                m_sqliteMgr->addTrackToPlaylist(plId, trackId, pos++);
            }

            QMessageBox::information(dlg,
                QStringLiteral("Playlist Saved"),
                QString("Saved \"%1\" with %2 track(s).")
                    .arg(state->playlistName).arg(state->matchedTracks.size()));
            statusLbl->setText(
                QString("Playlist \"%1\" saved to database.").arg(state->playlistName));
        });

    dlg->show();
}

// ─── Task 3: Daypart Scheduler Pro ────────────────────────────────────────────

void LibraryWidget::onAiDaypartRequested(qint64 categoryId, const QString& personaName) {
    if (!m_sqliteMgr) {
        QMessageBox::warning(this, QStringLiteral("Daypart Scheduler"),
            QStringLiteral("Database not available."));
        return;
    }

    const QString catName = categoryDisplayName(categoryId);
    auto pal = ThemePalette::forCurrentTheme();
    const QString accentBtnText =
        ThemePalette::isLightTheme() ? QStringLiteral("#ffffff") : pal.text.name();

    // ── Persona color palette for timeline blocks ─────────────────────────────
    static const QColor blockColors[] = {
        QColor("#3b82f6"), QColor("#ef4444"), QColor("#22c55e"), QColor("#f97316"),
        QColor("#a855f7"), QColor("#ec4899"), QColor("#14b8a6"), QColor("#eab308"),
        QColor("#6366f1"), QColor("#06b6d4"), QColor("#f43f5e"), QColor("#84cc16")
    };
    static const int numBlockColors = 12;

    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(QString("Daypart Scheduler \xe2\x80\x94 %1").arg(catName));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(900, 600);

    dlg->setStyleSheet(QString(
        "QDialog { background: %1; color: %2; }"
        "QGroupBox { font-size: 12px; font-weight: bold; border: 1px solid %3; "
        "  border-radius: 4px; margin-top: 8px; padding-top: 14px; color: %2; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QSpinBox, QComboBox, QCheckBox { font-size: 12px; "
        "  background: %4; color: %2; border: 1px solid %3; border-radius: 3px; padding: 3px; }"
        "QTableWidget { background: %4; color: %2; border: 1px solid %3; "
        "  font-size: 12px; gridline-color: %3; }"
        "QTableWidget::item { padding: 4px; }"
        "QHeaderView::section { background: %4; color: %2; border: 1px solid %3; "
        "  padding: 4px; font-size: 12px; font-weight: bold; }"
        "QPushButton { background: %5; color: %6; border: 1px solid %3; "
        "  border-radius: 4px; padding: 6px 16px; font-size: 12px; font-weight: 700; }"
        "QPushButton:hover { background: %3; }"
        "QPushButton:disabled { background: %4; color: %7; }")
        .arg(pal.bg.name(), pal.text.name(), pal.border.name(), pal.panelBg.name(),
             pal.accent.name(), accentBtnText, pal.textMuted.name()));

    auto* mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ── 24-HOUR VISUAL TIMELINE ───────────────────────────────────────────────
    // Custom widget that draws colored blocks for each scheduled persona
    class TimelineWidget : public QWidget {
    public:
        struct Block {
            int startHour;
            int endHour;
            QString personaName;
            QColor color;
        };
        QList<Block> blocks;

        explicit TimelineWidget(QWidget* parent = nullptr) : QWidget(parent) {
            setMinimumHeight(70);
            setMaximumHeight(70);
        }
    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            const int w = width() - 4;
            const int h = height();
            const int barTop = 20;
            const int barH = h - 34;
            const double hourW = w / 24.0;
            const int x0 = 2;

            // Draw hour labels
            p.setPen(QColor("#aaaaaa"));
            QFont lf = font();
            lf.setPixelSize(10);
            p.setFont(lf);
            for (int i = 0; i <= 23; i += 1) {
                const int xPos = x0 + static_cast<int>(i * hourW);
                if (i % 3 == 0) {
                    p.drawText(QRect(xPos, 0, static_cast<int>(hourW * 3), 18),
                               Qt::AlignLeft | Qt::AlignBottom,
                               QString("%1:00").arg(i, 2, 10, QChar('0')));
                }
                // Tick mark
                p.drawLine(xPos, barTop, xPos, barTop + 3);
            }

            // Draw background bar
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#2a2a3a"));
            p.drawRoundedRect(x0, barTop, w, barH, 4, 4);

            // Draw blocks
            for (const auto& b : blocks) {
                const int bx = x0 + static_cast<int>(b.startHour * hourW);
                const int bw = static_cast<int>((b.endHour - b.startHour) * hourW);
                if (bw <= 0) continue;

                p.setBrush(b.color);
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(bx + 1, barTop + 2, bw - 2, barH - 4, 3, 3);

                // Draw persona name inside block if wide enough
                if (bw > 50) {
                    p.setPen(Qt::white);
                    QFont bf = font();
                    bf.setPixelSize(10);
                    bf.setBold(true);
                    p.setFont(bf);
                    p.drawText(QRect(bx + 4, barTop + 2, bw - 8, barH - 4),
                               Qt::AlignCenter | Qt::TextWordWrap,
                               b.personaName);
                }
            }

            // Draw hour grid lines
            p.setPen(QPen(QColor(255, 255, 255, 30), 1));
            for (int i = 1; i < 24; ++i) {
                const int xPos = x0 + static_cast<int>(i * hourW);
                p.drawLine(xPos, barTop + 2, xPos, barTop + barH - 2);
            }
        }
    };

    auto* timelineGroup = new QGroupBox(QStringLiteral("24-Hour Timeline"), dlg);
    auto* timelineLayout = new QVBoxLayout(timelineGroup);
    timelineLayout->setContentsMargins(4, 12, 4, 4);
    auto* timeline = new TimelineWidget(timelineGroup);
    timelineLayout->addWidget(timeline);
    mainLayout->addWidget(timelineGroup);

    // ── ADD DAYPART BLOCK form ────────────────────────────────────────────────
    auto* addGroup = new QGroupBox(QStringLiteral("Add Daypart Block"), dlg);
    auto* addFormRow = new QHBoxLayout(addGroup);
    addFormRow->setSpacing(8);

    auto* startHourSpin = new QSpinBox(dlg);
    startHourSpin->setRange(0, 23);
    startHourSpin->setValue(6);
    startHourSpin->setSuffix(QStringLiteral(":00"));
    startHourSpin->setPrefix(QStringLiteral("Start: "));

    auto* endHourSpin = new QSpinBox(dlg);
    endHourSpin->setRange(1, 24);
    endHourSpin->setValue(10);
    endHourSpin->setSuffix(QStringLiteral(":00"));
    endHourSpin->setPrefix(QStringLiteral("End: "));

    auto* personaCombo = new QComboBox(dlg);
    personaCombo->setMinimumWidth(160);
    const auto personas = m_sqliteMgr->allPersonas();
    int preselectedIdx = 0;
    for (int i = 0; i < personas.size(); ++i) {
        const auto& p = personas[i];
        personaCombo->addItem(p.value("name").toString(),
                              QVariant::fromValue<qint64>(p.value("id").toLongLong()));
        if (p.value("name").toString() == personaName)
            preselectedIdx = i;
    }
    personaCombo->setCurrentIndex(preselectedIdx);

    addFormRow->addWidget(startHourSpin);
    addFormRow->addWidget(endHourSpin);
    addFormRow->addWidget(new QLabel(QStringLiteral("Persona:"), dlg));
    addFormRow->addWidget(personaCombo, 1);

    // Days checkboxes row
    auto* daysWidget = new QWidget(dlg);
    auto* daysRow = new QHBoxLayout(daysWidget);
    daysRow->setContentsMargins(0, 0, 0, 0);
    daysRow->setSpacing(3);
    daysRow->addWidget(new QLabel(QStringLiteral("Days:"), dlg));

    static const char* dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    QList<QCheckBox*> dayChecks;
    for (const char* dn : dayNames) {
        auto* chk = new QCheckBox(QString::fromUtf8(dn), dlg);
        chk->setChecked(true);
        dayChecks.append(chk);
        daysRow->addWidget(chk);
    }

    auto* addBtn = new QPushButton(QStringLiteral("Add"), dlg);
    daysRow->addStretch(1);
    daysRow->addWidget(addBtn);

    auto* addOuterLayout = new QVBoxLayout;
    addOuterLayout->setContentsMargins(0, 0, 0, 0);
    addOuterLayout->setSpacing(4);
    // Swap addGroup's layout to a vertical one with the form row + days
    // Since addGroup already has addFormRow, we need to restructure
    // Actually, let's put everything inside addGroup properly
    // Remove the existing layout from addGroup and replace
    delete addGroup->layout();
    auto* addGroupLayout = new QVBoxLayout(addGroup);
    addGroupLayout->setSpacing(6);

    auto* addTopRow = new QHBoxLayout;
    addTopRow->setSpacing(8);
    addTopRow->addWidget(startHourSpin);
    addTopRow->addWidget(endHourSpin);
    addTopRow->addWidget(new QLabel(QStringLiteral("Persona:"), dlg));
    addTopRow->addWidget(personaCombo, 1);
    addGroupLayout->addLayout(addTopRow);
    addGroupLayout->addWidget(daysWidget);

    mainLayout->addWidget(addGroup);

    // ── SCHEDULED BLOCKS table ────────────────────────────────────────────────
    auto* tableGroup = new QGroupBox(QStringLiteral("Scheduled Blocks"), dlg);
    auto* tableLayout = new QVBoxLayout(tableGroup);

    auto* table = new QTableWidget(dlg);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({
        QStringLiteral("Time"),
        QStringLiteral("Persona"),
        QStringLiteral("Days"),
        QStringLiteral("Color"),
        QStringLiteral("") // delete button column
    });
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    table->setColumnWidth(0, 90);
    table->setColumnWidth(2, 140);
    table->setColumnWidth(3, 50);
    table->setColumnWidth(4, 50);
    table->verticalHeader()->hide();
    tableLayout->addWidget(table);

    mainLayout->addWidget(tableGroup, 1);

    // ── Build persona->color map ──────────────────────────────────────────────
    auto personaColorMap = std::make_shared<QMap<qint64, QColor>>();
    int colorIdx = 0;
    for (const auto& p : personas) {
        const qint64 pid = p.value("id").toLongLong();
        // Use persona's saved color if it has one, otherwise assign from palette
        const QString savedColor = p.value("color").toString();
        if (!savedColor.isEmpty() && QColor::isValidColor(savedColor)) {
            personaColorMap->insert(pid, QColor(savedColor));
        } else {
            personaColorMap->insert(pid, blockColors[colorIdx % numBlockColors]);
            colorIdx++;
        }
    }

    // ── Refresh table + timeline (shared_ptr<function> for recursive calls) ──
    auto refreshAll = std::make_shared<std::function<void()>>();
    *refreshAll = [=]() {
        table->setRowCount(0);
        timeline->blocks.clear();

        const auto entries = m_sqliteMgr->allDaypartEntries();
        for (const auto& e : entries) {
            if (e.value("category_id").toLongLong() != categoryId)
                continue;

            const qint64 entryId   = e.value("id").toLongLong();
            const qint64 personaId = e.value("persona_id").toLongLong();
            const QString pName    = e.value("persona_name").toString();
            const int startH       = e.value("start_hour").toInt();
            const int endH         = e.value("end_hour").toInt();
            const QString dow      = e.value("day_of_week").toString();
            const QColor blockCol  = personaColorMap->value(personaId,
                blockColors[table->rowCount() % numBlockColors]);

            // Add to table
            const int row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(
                QString("%1:00-%2:00").arg(startH, 2, 10, QChar('0'))
                                      .arg(endH, 2, 10, QChar('0'))));
            table->setItem(row, 1, new QTableWidgetItem(pName));

            const QString dowDisplay = (dow == QStringLiteral("*"))
                ? QStringLiteral("Every Day") : dow;
            table->setItem(row, 2, new QTableWidgetItem(dowDisplay));

            // Color swatch cell
            auto* colorItem = new QTableWidgetItem();
            colorItem->setBackground(blockCol);
            table->setItem(row, 3, colorItem);

            // Delete button
            auto* delBtn = new QPushButton(QStringLiteral("Del"), table);
            delBtn->setStyleSheet(QString(
                "QPushButton { background: %1; padding: 2px 8px; font-size: 11px; }"
                "QPushButton:hover { background: %2; }")
                .arg(pal.error.name(), pal.error.darker(120).name()));
            delBtn->setProperty("entryId", QVariant::fromValue<qint64>(entryId));
            table->setCellWidget(row, 4, delBtn);

            // Wire delete — capture refreshAll shared_ptr for recursive call
            connect(delBtn, &QPushButton::clicked, dlg, [=]() {
                const qint64 eid = delBtn->property("entryId").toLongLong();
                auto reply = QMessageBox::question(dlg, QStringLiteral("Delete Entry"),
                    QString("Delete daypart block %1:00-%2:00 %3?")
                        .arg(startH, 2, 10, QChar('0'))
                        .arg(endH, 2, 10, QChar('0'))
                        .arg(pName),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    m_sqliteMgr->removeDaypartEntry(eid);
                    (*refreshAll)();
                }
            });

            // Add to timeline
            TimelineWidget::Block blk;
            blk.startHour = startH;
            blk.endHour = endH;
            blk.personaName = pName;
            blk.color = blockCol;
            timeline->blocks.append(blk);
        }

        timeline->update();
    };
    (*refreshAll)();

    // ── Button row ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    auto* aiAutoBtn = new QPushButton(QStringLiteral("AI: Auto-Generate Full Schedule"), dlg);
    if (!m_aiIntel) aiAutoBtn->setEnabled(false);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    btnRow->addWidget(aiAutoBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(closeBtn);
    mainLayout->addLayout(btnRow);

    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    // ── ADD button handler ────────────────────────────────────────────────────
    connect(addBtn, &QPushButton::clicked, dlg,
        [=]() {
            const int startH = startHourSpin->value();
            const int endH   = endHourSpin->value();
            if (startH >= endH) {
                QMessageBox::warning(dlg, QStringLiteral("Invalid Time Range"),
                    QStringLiteral("Start hour must be before end hour."));
                return;
            }

            const qint64 personaId = personaCombo->currentData().toLongLong();
            if (personaId <= 0) {
                QMessageBox::warning(dlg, QStringLiteral("No Persona"),
                    QStringLiteral("Please select a persona."));
                return;
            }

            // Build day-of-week string
            QStringList days;
            static const char* dayAbbrevs[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
            bool allChecked = true;
            for (int i = 0; i < dayChecks.size(); ++i) {
                if (dayChecks[i]->isChecked())
                    days << QString::fromUtf8(dayAbbrevs[i]);
                else
                    allChecked = false;
            }
            if (days.isEmpty()) {
                QMessageBox::warning(dlg, QStringLiteral("No Days Selected"),
                    QStringLiteral("Select at least one day."));
                return;
            }
            const QString dow = allChecked ? QStringLiteral("*") : days.join(QStringLiteral(","));

            m_sqliteMgr->addDaypartEntry(personaId, categoryId, startH, endH, dow);
            (*refreshAll)();
        });

    // ── AI AUTO-GENERATE handler ──────────────────────────────────────────────
    connect(aiAutoBtn, &QPushButton::clicked, dlg,
        [=]() {
            if (!m_aiIntel) return;

            auto reply = QMessageBox::question(dlg,
                QStringLiteral("AI Auto-Generate Schedule"),
                QStringLiteral("This will generate a recommended 24-hour daypart schedule.\n"
                               "Existing entries will NOT be removed.\n\n"
                               "The AI will suggest time blocks and personas. "
                               "You can review and delete any entries you disagree with.\n\n"
                               "Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (reply != QMessageBox::Yes) return;

            aiAutoBtn->setEnabled(false);
            aiAutoBtn->setText(QStringLiteral("Generating..."));

            // Build persona list for the prompt
            QString personaList;
            for (const auto& p : personas) {
                personaList += QString("- %1 (ID:%2) - %3\n")
                    .arg(p.value("name").toString())
                    .arg(p.value("id").toLongLong())
                    .arg(p.value("description").toString().left(80));
            }
            if (personaList.isEmpty())
                personaList = QStringLiteral("(no personas defined)\n");

            const QString systemPrompt = QStringLiteral(
                "You are a radio program director creating an optimal 24-hour broadcast schedule. "
                "Consider listener habits, demographics, and energy levels throughout the day.");
            const QString userPrompt = QString(
                "Create a complete 24-hour daypart schedule for the \"%1\" broadcast category.\n\n"
                "Available personas:\n%2\n"
                "Create 5-8 time blocks that cover the full 24 hours (00-24). "
                "Assign the most appropriate persona to each block for optimal listener engagement.\n\n"
                "Format EXACTLY as (one per line, nothing else):\n"
                "START-END PersonaID PersonaName\n\n"
                "Example:\n"
                "00-06 3 Jazz DJ\n"
                "06-10 1 Classic Rock DJ\n"
                "10-14 4 Top 40 DJ\n"
                "...\n\n"
                "Rules:\n"
                "- Cover all 24 hours with no gaps\n"
                "- Use ONLY persona IDs from the list above\n"
                "- Morning drive (06-10) and afternoon drive (15-18) are peak times\n"
                "- Overnight (00-06) should be mellow\n"
                "- Output ONLY the formatted lines, no commentary")
                .arg(catName, personaList);

            const QString contextName = QString("ai-daypart-%1").arg(categoryId);

            auto* connReady = new QMetaObject::Connection;
            auto* connFail  = new QMetaObject::Connection;

            *connReady = connect(m_aiIntel, &AiTrackIntel::customPromptReady, dlg,
                [=](const QString& ctx, const QString& text, const QString& /*json*/,
                     const QString& /*backend*/, const QString& /*model*/) {
                    if (ctx != contextName) return;
                    QObject::disconnect(*connReady);
                    QObject::disconnect(*connFail);
                    delete connReady;
                    delete connFail;

                    // Parse AI response: "START-END PersonaID PersonaName"
                    QRegularExpression lineRe(
                        R"((\d{1,2})\s*[-\u2013]\s*(\d{1,2})\s+(\d+)\s+(.+))");
                    const QStringList lines = text.split(QRegularExpression(R"([\r\n]+)"),
                        Qt::SkipEmptyParts);

                    int added = 0;
                    for (const QString& rawLine : lines) {
                        const QString line = rawLine.trimmed();
                        auto match = lineRe.match(line);
                        if (!match.hasMatch()) continue;

                        const int startH = match.captured(1).toInt();
                        const int endH   = match.captured(2).toInt();
                        const qint64 pId = match.captured(3).toLongLong();

                        // Validate persona ID exists
                        bool validPersona = false;
                        for (const auto& p : personas) {
                            if (p.value("id").toLongLong() == pId) {
                                validPersona = true;
                                break;
                            }
                        }
                        if (!validPersona || startH >= endH || endH > 24) continue;

                        m_sqliteMgr->addDaypartEntry(pId, categoryId, startH, endH,
                                                      QStringLiteral("*"));
                        added++;
                    }

                    (*refreshAll)();

                    aiAutoBtn->setEnabled(true);
                    aiAutoBtn->setText(QStringLiteral("AI: Auto-Generate Full Schedule"));

                    QMessageBox::information(dlg,
                        QStringLiteral("Schedule Generated"),
                        QString("AI created %1 daypart block(s). "
                                "Review the schedule above and delete any entries "
                                "you want to change.").arg(added));
                });

            *connFail = connect(m_aiIntel, &AiTrackIntel::customPromptFailed, dlg,
                [=](const QString& ctx, const QString& error) {
                    if (ctx != contextName) return;
                    QObject::disconnect(*connReady);
                    QObject::disconnect(*connFail);
                    delete connReady;
                    delete connFail;

                    aiAutoBtn->setEnabled(true);
                    aiAutoBtn->setText(QStringLiteral("AI: Auto-Generate Full Schedule"));

                    QMessageBox::warning(dlg,
                        QStringLiteral("AI Error"),
                        QString("Failed to generate schedule: %1").arg(error.left(200)));
                });

            m_aiIntel->sendCustomPrompt(contextName, systemPrompt, userPrompt);
        });

    dlg->show();
}

} // namespace M1
