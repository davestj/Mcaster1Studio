#include "LibraryWidget.h"
#include "LibraryModel.h"
#include "IDatabase.h"
#include "ScanWorker.h"
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
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QLabel>
#include <QPainter>
#include <QApplication>

namespace M1 {

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
    verticalHeader()->setDefaultSectionSize(22);
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
    setupUi();
}

void LibraryWidget::setupUi() {
    setObjectName("LibraryWidget");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(4);

    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText("Search title, artist, album, genre…");
    m_filter->setClearButtonEnabled(true);
    m_filter->setMinimumHeight(26);
    connect(m_filter, &QLineEdit::textChanged, this, &LibraryWidget::onFilterChanged);

    m_scanBtn = new QPushButton("+ Scan Directory", this);
    m_scanBtn->setMinimumHeight(26);
    m_scanBtn->setStyleSheet(
        "QPushButton{background:#0c1a2e; color:#38bdf8; border:1px solid #1e3a5f;"
        " border-radius:4px; padding:0 10px; font-size:10px; font-weight:700;}"
        "QPushButton:hover{background:#1e3a5f;}"
        "QPushButton:disabled{color:#334155;}");
    connect(m_scanBtn, &QPushButton::clicked, this, &LibraryWidget::onScanClicked);

    toolbar->addWidget(m_filter, 1);
    toolbar->addWidget(m_scanBtn);
    root->addLayout(toolbar);

    // ── Table view ────────────────────────────────────────────────────────────
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);        // search all columns
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_view = new LibraryTableView(this);
    m_view->setModel(m_proxy);
    m_view->sortByColumn(LibraryModel::ColArtist, Qt::AscendingOrder);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view, &QTableView::customContextMenuRequested,
            this, &LibraryWidget::onContextMenu);
    connect(m_view, &QTableView::doubleClicked,
            this, &LibraryWidget::onDoubleClicked);

    // Column widths
    auto* hdr = m_view->horizontalHeader();
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

    m_view->setColumnWidth(LibraryModel::ColArtist,   160);
    m_view->setColumnWidth(LibraryModel::ColAlbum,    160);
    m_view->setColumnWidth(LibraryModel::ColGenre,     90);
    m_view->setColumnWidth(LibraryModel::ColDuration,  65);
    m_view->setColumnWidth(LibraryModel::ColBpm,       60);
    m_view->setColumnWidth(LibraryModel::ColKey,       50);
    m_view->setColumnWidth(LibraryModel::ColBitrate,   75);
    m_view->setColumnWidth(LibraryModel::ColRating,    60);
    m_view->setColumnWidth(LibraryModel::ColCodec,     50);

    root->addWidget(m_view, 1);

    // ── Status row ────────────────────────────────────────────────────────────
    auto* statusRow = new QHBoxLayout;
    m_statusLbl = new QLabel("0 tracks", this);
    m_statusLbl->setStyleSheet(
        "color:#64748b; font-size:9px; background:transparent;");

    m_progress = new QProgressBar(this);
    m_progress->setFixedHeight(4);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(
        "QProgressBar{background:#0c1a2e; border:none; border-radius:2px;}"
        "QProgressBar::chunk{background:#0ea5e9; border-radius:2px;}");
    m_progress->hide();

    statusRow->addWidget(m_statusLbl);
    statusRow->addStretch(1);
    statusRow->addWidget(m_progress);
    root->addLayout(statusRow);
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void LibraryWidget::onScanClicked() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "Select Music Directory",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (dir.isEmpty()) return;
    emit scanDirectoryRequested({dir});
}

void LibraryWidget::onFilterChanged(const QString& text) {
    m_proxy->setFilterFixedString(text);
    m_statusLbl->setText(
        text.isEmpty()
            ? QString("%1 tracks").arg(m_model->rowCount())
            : QString("%1 / %2 tracks").arg(m_proxy->rowCount()).arg(m_model->rowCount()));
}

void LibraryWidget::onScanStarted(int estimatedCount) {
    m_scanBtn->setEnabled(false);
    m_progress->setRange(0, estimatedCount > 0 ? estimatedCount : 0);
    m_progress->setValue(0);
    m_progress->show();
    m_statusLbl->setText("Scanning…");
}

void LibraryWidget::onItemScanned(const M1::MediaItem& item) {
    // Update status every N items (called from main thread via queued connection)
    if (m_model->rowCount() % 25 == 0)
        m_statusLbl->setText(QString("Scanning… %1 found").arg(m_model->rowCount()));
    (void)item;
}

void LibraryWidget::onScanProgress(int done, int total) {
    m_progress->setMaximum(total > 0 ? total : 1);
    m_progress->setValue(done);
}

void LibraryWidget::onScanFinished(int /*total*/) {
    m_scanBtn->setEnabled(true);
    m_progress->hide();
    m_statusLbl->setText(QString("%1 tracks").arg(m_model->rowCount()));
}

void LibraryWidget::onDoubleClicked(const QModelIndex& proxyIdx) {
    // Double-click → load to Deck A
    const QModelIndex srcIdx = m_proxy->mapToSource(proxyIdx);
    const MediaItem item = m_model->itemAt(srcIdx.row());
    if (!item.filePath.isEmpty())
        emit requestLoadMedia(item, 0);
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
    menu.addAction("MusicBrainz Lookup…", [this, item]() {
        emit mbLookupRequested(item);
    });
    menu.addSeparator();
    menu.addAction("Export Visible as M3U…", this, &LibraryWidget::onExportM3U);
    menu.addSeparator();
    menu.addAction("Remove from Library", [this, item]() {
        m_model->removeItem(item.id);
        if (m_db && m_db->isConnected())
            m_db->deleteItem(item.id);
        m_statusLbl->setText(QString("%1 tracks").arg(m_model->rowCount()));
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

MediaItem LibraryWidget::selectedItem() const {
    const QModelIndexList sel = m_view->selectionModel()->selectedRows();
    if (sel.isEmpty()) return {};
    return m_model->itemAt(m_proxy->mapToSource(sel.first()).row());
}

} // namespace M1
