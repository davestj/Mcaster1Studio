#include "PlaylistWidget.h"
#include "PlaylistModule.h"
#include "PlaylistConfigDialog.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QListWidgetItem>
#include <QPoint>
#include <QTimer>
#include <QPainter>
#include <QDebug>

const QStringList PlaylistWidget::s_audioExts = {
    "mp3", "flac", "ogg", "opus", "wav", "aac", "m4a", "wma", "aiff", "aif",
    "wv", "ape", "m3u", "m3u8", "pls"
};

// Custom data roles stored in each QListWidgetItem
enum PlaylistRoles {
    IsCurrentRole = Qt::UserRole + 100,  // bool: true if this is the currently playing track
    QueueIndexRole = Qt::UserRole + 101  // int: queue index
};

// =============================================================================
// PlaylistListWidget
// =============================================================================

PlaylistListWidget::PlaylistListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);
    setDropIndicatorShown(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}

void PlaylistListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
    else if (event->source() == this)
        event->acceptProposedAction();
    else
        event->ignore();
}

void PlaylistListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls() || event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
    else if (event->source() == this)
        event->acceptProposedAction();
    else
        event->ignore();
}

void PlaylistListWidget::dropEvent(QDropEvent* event)
{
    if (event->source() == this) {
        const int fromRow = currentRow();
        QListWidget::dropEvent(event);
        const int toRow = currentRow();
        if (fromRow != toRow && fromRow >= 0 && toRow >= 0)
            emit rowMoved(fromRow, toRow);
        return;
    }

    const QMimeData* mime = event->mimeData();
    QList<QUrl> urls;
    if (mime->hasUrls()) {
        urls = mime->urls();
    } else if (mime->hasFormat("text/uri-list")) {
        const QByteArray raw = mime->data("text/uri-list");
        for (const auto& line : raw.split('\n')) {
            const QString str = QString::fromUtf8(line).trimmed();
            if (!str.isEmpty() && !str.startsWith('#'))
                urls << QUrl(str);
        }
    }

    QStringList paths;
    for (const QUrl& url : urls)
        if (url.isLocalFile()) paths << url.toLocalFile();

    if (!paths.isEmpty()) {
        emit filesDropped(paths);
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

// =============================================================================
// PlaylistItemDelegate — handles ALL item painting (background, text, icon)
// This bypasses QListWidget stylesheet item rules so backgrounds always work.
// =============================================================================

PlaylistItemDelegate::PlaylistItemDelegate(QWidget* parent)
    : QStyledItemDelegate(parent) {}

void PlaylistItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const bool isCurrent = index.data(IsCurrentRole).toBool();
    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHover = option.state & QStyle::State_MouseOver;
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::EnterprisePro);

    const QRect r = option.rect;

    // ── Background ──────────────────────────────────────────────────
    if (isCurrent) {
        // Currently playing — high-contrast background
        if (isLight) {
            // Light theme: deep blue background
            painter->fillRect(r, QColor(0x0a, 0x3d, 0x7a));
        } else {
            // Dark theme: very dark navy with subtle gradient
            QLinearGradient grad(r.topLeft(), r.bottomLeft());
            grad.setColorAt(0.0, QColor(0x04, 0x12, 0x2a));
            grad.setColorAt(1.0, QColor(0x08, 0x1e, 0x38));
            painter->fillRect(r, grad);
        }
        // Left accent bar — cyan/blue indicator
        const QColor accent = isLight ? QColor(0x00, 0x8c, 0xff) : QColor(0x00, 0xcc, 0xff);
        painter->fillRect(QRect(r.left(), r.top(), 4, r.height()), accent);

    } else if (isSelected) {
        // Selected (not current) — subtle highlight
        painter->fillRect(r, isLight ? QColor(0x1c, 0x5c, 0xaa) : QColor(0x1e, 0x3a, 0x5f));

    } else if (isHover) {
        // Hover — very subtle
        painter->fillRect(r, isLight ? QColor(0xdb, 0xea, 0xfe) : QColor(0x16, 0x28, 0x40));

    } else {
        // Normal row — alternating subtle stripe
        const bool even = (index.row() % 2 == 0);
        if (isLight) {
            painter->fillRect(r, even ? QColor(0xff, 0xff, 0xff) : QColor(0xf5, 0xf2, 0xed));
        } else {
            painter->fillRect(r, even ? QColor(0x0c, 0x1a, 0x2e) : QColor(0x0e, 0x1e, 0x32));
        }
    }

    // ── Bottom divider line ─────────────────────────────────────────
    painter->setPen(isLight ? QColor(0xed, 0xe8, 0xe0) : QColor(0x11, 0x1e, 0x2e));
    painter->drawLine(r.bottomLeft(), r.bottomRight());

    // ── Text ────────────────────────────────────────────────────────
    QColor textColor;
    if (isCurrent) {
        textColor = isLight ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0xee, 0xff);
    } else if (isSelected) {
        textColor = isLight ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0xd8, 0xff);
    } else {
        textColor = isLight ? QColor(0x00, 0x00, 0x00) : QColor(0xc8, 0xd8, 0xf0);
    }

    QFont font = option.font;
    font.setPixelSize(14);
    if (isCurrent) {
        font.setBold(true);
    }
    painter->setFont(font);
    painter->setPen(textColor);

    const int textLeft = isCurrent ? r.left() + 8 : r.left() + 6;
    const QRect textRect(textLeft, r.top(), r.width() - textLeft - 6, r.height());
    const QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

    painter->restore();
}

QSize PlaylistItemDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                      const QModelIndex& /*index*/) const
{
    return QSize(200, 30);  // fixed row height
}

// =============================================================================
// PlaylistWidget
// =============================================================================

PlaylistWidget::PlaylistWidget(M1::PlaylistModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    buildUi();
    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });

    // Module signals
    connect(m_module, &M1::PlaylistModule::queueChanged,  this, &PlaylistWidget::onQueueChanged);
    connect(m_module, &M1::PlaylistModule::autoDJChanged, this, &PlaylistWidget::onAutoDJChanged);
    connect(m_module, &M1::PlaylistModule::queueLow,      this, &PlaylistWidget::onQueueLow);

    // List widget signals
    connect(m_listWidget, &PlaylistListWidget::filesDropped, this, &PlaylistWidget::onFilesDropped);
    connect(m_listWidget, &PlaylistListWidget::rowMoved,     this, &PlaylistWidget::onRowMoved);

    // Buttons
    connect(m_btnAddFiles, &QPushButton::clicked, this, &PlaylistWidget::onAddFiles);
    connect(m_btnPlay,     &QPushButton::clicked, this, &PlaylistWidget::onPlayNext);
    connect(m_btnSkip,     &QPushButton::clicked, this, &PlaylistWidget::onSkip);
    connect(m_btnAutoDJ,   &QPushButton::clicked, this, &PlaylistWidget::onToggleAutoDJ);
    connect(m_btnConfig,   &QPushButton::clicked, this, &PlaylistWidget::onOpenConfig);
    connect(m_btnClear,    &QPushButton::clicked, this, &PlaylistWidget::onClearQueue);

    // Spinboxes
    connect(m_artistSepSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_module->setArtistSeparation(v);
    });
    connect(m_titleSepSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_module->setTitleSeparation(v);
    });
    connect(m_minQueueSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        m_module->setMinQueueDepth(v);
    });

    // Context menu
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QWidget::customContextMenuRequested, this, &PlaylistWidget::showContextMenu);

    // Sync spinboxes
    m_artistSepSpin->setValue(m_module->artistSeparation());
    m_titleSepSpin->setValue(m_module->titleSeparation());
    m_minQueueSpin->setValue(m_module->minQueueDepth());

    onQueueChanged();
    onAutoDJChanged(m_module->autoDJ());
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaylistWidget::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Toolbar ─────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);

    m_btnAddFiles = new QPushButton("+ Files", this);
    m_btnPlay     = new QPushButton("\u25b6 Play", this);
    m_btnSkip     = new QPushButton("\u23ed Skip", this);
    m_btnAutoDJ   = new QPushButton("AutoDJ OFF", this);
    m_btnConfig   = new QPushButton("Config", this);
    m_btnClear    = new QPushButton("Clear", this);

    for (auto* b : {m_btnAddFiles, m_btnPlay, m_btnSkip, m_btnAutoDJ, m_btnConfig, m_btnClear})
        b->setFixedHeight(28);

    m_btnAutoDJ->setCheckable(true);

    m_btnAddFiles->setToolTip("Add audio files or playlists (M3U/PLS) to the queue");
    m_btnPlay->setToolTip("Play the current track on the next idle deck");
    m_btnSkip->setToolTip("Skip to the next valid track");
    m_btnAutoDJ->setToolTip("Enable/disable AutoDJ — auto-fills queue from library and plays");
    m_btnConfig->setToolTip("Open AutoDJ configuration — strategy, categories, clockwheel");
    m_btnClear->setToolTip("Remove all tracks from the queue");

    toolbar->addWidget(m_btnAddFiles);
    toolbar->addWidget(m_btnPlay);
    toolbar->addWidget(m_btnSkip);
    toolbar->addWidget(m_btnAutoDJ);
    toolbar->addWidget(m_btnConfig);
    toolbar->addStretch();
    toolbar->addWidget(m_btnClear);
    root->addLayout(toolbar);

    // ── Rotation rules bar ──────────────────────────────────────────
    auto* ruleBar = new QHBoxLayout();
    ruleBar->setSpacing(4);

    auto mkLabel = [this](const QString& text) {
        auto* lbl = new QLabel(text, this);
        lbl->setObjectName("PlaylistRuleLabel");
        return lbl;
    };

    ruleBar->addWidget(mkLabel("Artist sep:"));
    m_artistSepSpin = new QSpinBox(this);
    m_artistSepSpin->setRange(0, 20);
    m_artistSepSpin->setMaximumWidth(50);
    m_artistSepSpin->setFixedHeight(22);
    m_artistSepSpin->setToolTip("Minimum tracks between the same artist");
    ruleBar->addWidget(m_artistSepSpin);

    ruleBar->addWidget(mkLabel("Title sep:"));
    m_titleSepSpin = new QSpinBox(this);
    m_titleSepSpin->setRange(0, 20);
    m_titleSepSpin->setMaximumWidth(50);
    m_titleSepSpin->setFixedHeight(22);
    m_titleSepSpin->setToolTip("Minimum tracks between the same title");
    ruleBar->addWidget(m_titleSepSpin);

    ruleBar->addStretch();

    ruleBar->addWidget(mkLabel("Min queue:"));
    m_minQueueSpin = new QSpinBox(this);
    m_minQueueSpin->setRange(1, 50);
    m_minQueueSpin->setMaximumWidth(50);
    m_minQueueSpin->setFixedHeight(22);
    m_minQueueSpin->setToolTip("Warn when fewer than this many tracks remain in queue");
    ruleBar->addWidget(m_minQueueSpin);

    root->addLayout(ruleBar);

    // ── Queue list ──────────────────────────────────────────────────
    m_listWidget = new PlaylistListWidget(this);
    m_listWidget->setItemDelegate(new PlaylistItemDelegate(m_listWidget));
    m_listWidget->setMouseTracking(true);  // needed for hover state in delegate
    root->addWidget(m_listWidget, 1);

    // ── Status bar ──────────────────────────────────────────────────
    m_statusBar = new QLabel("0 tracks | AutoDJ: OFF", this);
    m_statusBar->setObjectName("PlaylistStatusBar");
    m_statusBar->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusBar->setFixedHeight(22);
    root->addWidget(m_statusBar);
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaylistWidget::applyTheme()
{
    const auto tp = ThemePalette::forCurrentTheme();
    const QString bg      = tp.panelBg.name();
    const QString border  = tp.border.name();
    const QString selBg   = tp.border.name();
    const QString selText = tp.info.name();
    const QString btnBg   = tp.cardBg.name();
    const QString btnText = tp.textMuted.name();
    const QString btnHov  = tp.border.name();
    const QString btnHovT = tp.text.name();
    const QString statusC = tp.accent.name();
    const QString lblText = tp.textMuted.name();
    const QString spinBg  = tp.inputBg.name();
    const QString spinTxt = tp.text.name();

    setStyleSheet(QString("PlaylistWidget { background-color: %1; }").arg(bg));

    // Minimal list stylesheet — NO item background/color rules!
    // All item painting is handled by PlaylistItemDelegate::paint().
    m_listWidget->setStyleSheet(QString(R"(
        QListWidget {
            background-color: %1; border: 1px solid %2;
            border-radius: 3px;
        }
        QScrollBar:vertical { background: %1; width: 8px; }
        QScrollBar::handle:vertical { background: %2; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    )").arg(bg, border));

    const QString btnStyle = QString(R"(
        QPushButton {
            background-color: %1; color: %2; border: 1px solid %3;
            border-radius: 3px; padding: 2px 10px; font-size: 12px;
        }
        QPushButton:hover { background-color: %4; color: %5; }
        QPushButton:pressed { background-color: %1; }
        QPushButton:checked { background-color: %6; color: %7; border: 1px solid %7; }
        QPushButton:disabled { color: %3; }
    )").arg(btnBg, btnText, border, btnHov, btnHovT, selBg, selText);

    for (auto* b : {m_btnAddFiles, m_btnPlay, m_btnSkip, m_btnAutoDJ, m_btnConfig, m_btnClear})
        b->setStyleSheet(btnStyle);

    const QString lblStyle = QString("QLabel { color: %1; font-size: 12px; font-weight: bold; background: transparent; }").arg(lblText);
    for (auto* lbl : findChildren<QLabel*>("PlaylistRuleLabel"))
        lbl->setStyleSheet(lblStyle);

    const QString spinStyle = QString(R"(
        QSpinBox { background: %1; color: %2; border: 1px solid %3; border-radius: 2px; font-size: 12px; padding: 1px 3px; }
    )").arg(spinBg, spinTxt, border);
    m_artistSepSpin->setStyleSheet(spinStyle);
    m_titleSepSpin->setStyleSheet(spinStyle);
    m_minQueueSpin->setStyleSheet(spinStyle);

    m_statusBar->setStyleSheet(QString("QLabel { color: %1; font-size: 12px; background: transparent; }").arg(statusC));

    // Force repaint of all items with new theme
    m_listWidget->viewport()->update();
}

// ─────────────────────────────────────────────────────────────────────────────
void PlaylistWidget::onQueueChanged()
{
    m_listWidget->blockSignals(true);
    m_listWidget->clear();

    const auto& queue   = m_module->queue();
    const int   current = m_module->currentIndex();

    for (int i = 0; i < queue.size(); ++i) {
        const M1::MediaItem& item = queue[i];

        QString label = item.displayTitle();
        if (!item.displayArtist().isEmpty())
            label = item.displayArtist() + " \u2014 " + label;
        if (item.durationMs > 0)
            label += "  [" + item.durationString() + "]";
        if (!item.genre.isEmpty())
            label += "  {" + item.genre + "}";

        auto* row = new QListWidgetItem((i == current ? "\u25b6 " : "   ") + label);

        // Store custom data for the delegate to read
        row->setData(IsCurrentRole, (i == current));
        row->setData(QueueIndexRole, i);

        m_listWidget->addItem(row);
    }

    m_listWidget->blockSignals(false);

    if (current >= 0 && current < m_listWidget->count())
        m_listWidget->scrollToItem(m_listWidget->item(current), QAbstractItemView::PositionAtCenter);

    const int remaining = (int)queue.size() - std::max(0, current);
    m_statusBar->setText(QString("%1 tracks | %2 remaining | AutoDJ: %3 | %4")
                             .arg(queue.size())
                             .arg(remaining)
                             .arg(m_module->autoDJ() ? "ON" : "OFF")
                             .arg(m_module->strategyName()));
}

void PlaylistWidget::onAutoDJChanged(bool enabled)
{
    m_btnAutoDJ->setChecked(enabled);
    m_btnAutoDJ->setText(enabled
        ? QString("AutoDJ ON [%1]").arg(m_module->strategyName())
        : "AutoDJ OFF");
    onQueueChanged();
}

void PlaylistWidget::onQueueLow(int remaining)
{
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::EnterprisePro);
    m_statusBar->setStyleSheet(QString(
        "QLabel { background: %1; color: %2; font-size: 12px; font-weight: bold; }"
    ).arg(isLight ? "#fff3e0" : "#2a1a00", isLight ? "#a05000" : "#ffb040"));
    m_statusBar->setText(QString("Queue low — only %1 track%2 remaining. Add more tracks.")
                         .arg(remaining).arg(remaining == 1 ? "" : "s"));

    QTimer::singleShot(5000, this, [this]() {
        applyTheme();
        onQueueChanged();
    });
}

void PlaylistWidget::onPlayNext()     { m_module->playNext(); }
void PlaylistWidget::onSkip()         { m_module->skip(); }
void PlaylistWidget::onToggleAutoDJ() {
    const bool enabling = !m_module->autoDJ();
    m_module->setAutoDJ(enabling);
    // When AutoDJ is turned ON, also trigger play so the first track starts
    if (enabling && !m_module->queue().isEmpty()) {
        QTimer::singleShot(500, this, [this]() {
            m_module->playNext();
        });
    }
}
void PlaylistWidget::onClearQueue()   { m_module->clearQueue(); }

void PlaylistWidget::onAddFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, "Add Tracks to Playlist", {},
        "Audio Files (*.mp3 *.flac *.wav *.aif *.aiff *.ogg *.opus *.m4a *.aac *.wv *.ape);;"
        "Playlists (*.m3u *.m3u8 *.pls);;"
        "All Files (*)");
    if (!paths.isEmpty())
        m_module->addFiles(paths);
}

void PlaylistWidget::onOpenConfig()
{
    PlaylistConfigDialog dlg(m_module->autoDJConfig(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const AutoDJConfig cfg = dlg.config();
    m_module->setAutoDJConfig(cfg);

    // Sync spinboxes
    m_artistSepSpin->blockSignals(true);
    m_titleSepSpin->blockSignals(true);
    m_artistSepSpin->setValue(cfg.artistSeparation);
    m_titleSepSpin->setValue(cfg.titleSeparation);
    m_artistSepSpin->blockSignals(false);
    m_titleSepSpin->blockSignals(false);

    // Update button text
    onAutoDJChanged(m_module->autoDJ());
    onQueueChanged();
}

void PlaylistWidget::onFilesDropped(const QStringList& paths)
{
    m_module->addFiles(paths);
}

void PlaylistWidget::onRowMoved(int from, int to)
{
    m_module->moveItem(from, to);
}

void PlaylistWidget::showContextMenu(const QPoint& pos)
{
    const QListWidgetItem* rowItem = m_listWidget->itemAt(pos);
    const int row = rowItem ? rowItem->data(QueueIndexRole).toInt() : -1;

    const auto tp2 = ThemePalette::forCurrentTheme();
    const QString menuBg   = tp2.cardBg.name();
    const QString menuText = tp2.text.name();
    const QString menuBord = tp2.border.name();
    const QString menuHov  = tp2.border.name();
    const QString menuHovT = tp2.info.name();

    QMenu menu(this);
    menu.setStyleSheet(QString(R"(
        QMenu { background-color: %1; color: %2; border: 1px solid %3; font-size: 12px; }
        QMenu::item { padding: 5px 20px; }
        QMenu::item:selected { background-color: %4; color: %5; }
        QMenu::separator { height: 1px; background: %3; margin: 3px 8px; }
    )").arg(menuBg, menuText, menuBord, menuHov, menuHovT));

    QAction* actPlayNow   = menu.addAction("Play Now");
    QAction* actLoadDeckA = menu.addAction("Load to Deck A");
    QAction* actLoadDeckB = menu.addAction("Load to Deck B");
    menu.addSeparator();
    QAction* actMoveUp     = menu.addAction("Move Up");
    QAction* actMoveDown   = menu.addAction("Move Down");
    QAction* actMoveTop    = menu.addAction("Move to Top");
    QAction* actMoveBottom = menu.addAction("Move to Bottom");
    menu.addSeparator();
    QAction* actRemove       = menu.addAction("Remove");
    QAction* actRemovePlayed = menu.addAction("Remove Played Tracks");
    QAction* actClearAll     = menu.addAction("Clear All");

    const int queueSize = m_module->queueSize();
    if (row < 0) {
        for (auto* a : {actPlayNow, actLoadDeckA, actLoadDeckB, actMoveUp, actMoveDown, actMoveTop, actMoveBottom, actRemove})
            a->setEnabled(false);
    } else {
        actMoveUp->setEnabled(row > 0);
        actMoveDown->setEnabled(row < queueSize - 1);
        actMoveTop->setEnabled(row > 0);
        actMoveBottom->setEnabled(row < queueSize - 1);
    }
    actRemovePlayed->setEnabled(m_module->currentIndex() > 0);
    actClearAll->setEnabled(queueSize > 0);

    const QAction* chosen = menu.exec(m_listWidget->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actPlayNow)        m_module->playItemAt(row);
    else if (chosen == actLoadDeckA) { if (row >= 0 && row < queueSize) emit m_module->requestLoadMedia(m_module->queue()[row], 0); }
    else if (chosen == actLoadDeckB) { if (row >= 0 && row < queueSize) emit m_module->requestLoadMedia(m_module->queue()[row], 1); }
    else if (chosen == actMoveUp)    m_module->moveItem(row, row - 1);
    else if (chosen == actMoveDown)  m_module->moveItem(row, row + 1);
    else if (chosen == actMoveTop)   m_module->moveItem(row, 0);
    else if (chosen == actMoveBottom)m_module->moveItem(row, queueSize - 1);
    else if (chosen == actRemove)    m_module->removeItem(row);
    else if (chosen == actRemovePlayed) { for (int i = m_module->currentIndex() - 1; i >= 0; --i) m_module->removeItem(i); }
    else if (chosen == actClearAll)  m_module->clearQueue();
}
