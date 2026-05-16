#include "QueueWidget.h"
#include "QueueModule.h"
#include "MediaItem.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QMenu>
#include <QAction>
#include <QLabel>

// ─────────────────────────────────────────────────────────────────────────────
// QueueListWidget
// ─────────────────────────────────────────────────────────────────────────────
QueueListWidget::QueueListWidget(QWidget* parent)
    : QListWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setDefaultDropAction(Qt::MoveAction);
}

void QueueListWidget::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls() || e->source() == this)
        e->acceptProposedAction();
}

void QueueListWidget::dragMoveEvent(QDragMoveEvent* e) {
    if (e->mimeData()->hasUrls() || e->source() == this)
        e->acceptProposedAction();
}

void QueueListWidget::dropEvent(QDropEvent* e) {
    if (e->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl& url : e->mimeData()->urls()) {
            const QString p = url.toLocalFile();
            if (!p.isEmpty()) paths << p;
        }
        if (!paths.isEmpty()) { e->acceptProposedAction(); emit filesDropped(paths); }
        return;
    }
    const int from = currentRow();
    QListWidget::dropEvent(e);
    const int to = currentRow();
    if (from != to && from >= 0 && to >= 0) emit rowMoved(from, to);
}

// ─────────────────────────────────────────────────────────────────────────────
// QueueWidget
// ─────────────────────────────────────────────────────────────────────────────
QueueWidget::QueueWidget(QueueModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    setObjectName("QueueWidget");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(4);

    m_addBtn   = new QPushButton("+ Files", this);
    m_playBtn  = new QPushButton("\u25b6 Play", this);
    m_clearBtn = new QPushButton("Clear", this);

    m_addBtn->setFixedHeight(28);
    m_playBtn->setFixedHeight(28);
    m_clearBtn->setFixedHeight(28);

    m_addBtn->setToolTip("Add audio files or playlists (M3U/PLS)");
    m_playBtn->setToolTip("Play selected track on the deck");
    m_clearBtn->setToolTip("Remove all tracks from the queue");

    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_playBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_clearBtn);
    root->addLayout(toolbar);

    // ── Track list ───────────────────────────────────────────────
    m_listWidget = new QueueListWidget(this);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_listWidget, 1);

    // ── Status bar ───────────────────────────────────────────────
    m_statusBar = new QLabel("Empty queue", this);
    m_statusBar->setObjectName("QueueStatusBar");
    m_statusBar->setFixedHeight(22);
    root->addWidget(m_statusBar);

    // ── Connections ──────────────────────────────────────────────
    connect(m_module, &QueueModule::queueChanged, this, &QueueWidget::onQueueChanged);
    connect(m_addBtn,   &QPushButton::clicked, this, &QueueWidget::onAddFiles);
    connect(m_playBtn,  &QPushButton::clicked, this, &QueueWidget::onPlayNow);
    connect(m_clearBtn, &QPushButton::clicked, this, &QueueWidget::onClearQueue);
    connect(m_listWidget, &QueueListWidget::filesDropped, this, &QueueWidget::onFilesDropped);
    connect(m_listWidget, &QueueListWidget::rowMoved,     this, &QueueWidget::onRowMoved);
    connect(m_listWidget, &QListWidget::customContextMenuRequested,
            this, &QueueWidget::showContextMenu);

    // Apply theme-adaptive styling
    auto applyTheme = [this]() {
        const auto tp = ThemePalette::forCurrentTheme();
        const QString bg     = tp.panelBg.name();
        const QString text   = tp.text.name();
        const QString border = tp.border.name();
        const QString rowDiv = tp.cardBg.name();
        const QString selBg  = tp.border.name();
        const QString selTx  = tp.info.name();
        const QString hovBg  = tp.inputBg.name();
        const QString btnBg  = tp.cardBg.name();
        const QString btnTx  = tp.textMuted.name();
        const QString btnHov = tp.border.name();
        const QString btnHT  = tp.text.name();
        const QString stC    = tp.textMuted.name();

        setStyleSheet(QString("QueueWidget { background-color: %1; }").arg(bg));

        m_listWidget->setStyleSheet(QString(R"(
            QListWidget {
                background-color: %1; border: 1px solid %3;
                border-radius: 3px; font-size: 14px; font-weight: bold;
            }
            QListWidget::item { padding: 4px 6px; border-bottom: 1px solid %4; }
            QListWidget::item:selected { background-color: %5; color: %6; border-left: 3px solid %6; }
            QListWidget::item:hover:!selected { background-color: %7; }
            QScrollBar:vertical { background: %1; width: 8px; }
            QScrollBar::handle:vertical { background: %3; border-radius: 4px; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        )").arg(bg, text, border, rowDiv, selBg, selTx, hovBg));

        const QString btnStyle = QString(R"(
            QPushButton {
                background-color: %1; color: %2;
                border: 1px solid %3; border-radius: 3px;
                padding: 2px 10px; font-size: 12px;
            }
            QPushButton:hover { background-color: %4; color: %5; }
            QPushButton:pressed { background-color: %1; }
            QPushButton:disabled { color: %3; }
        )").arg(btnBg, btnTx, border, btnHov, btnHT);

        m_addBtn->setStyleSheet(btnStyle);
        m_playBtn->setStyleSheet(btnStyle);
        m_clearBtn->setStyleSheet(btnStyle);

        m_statusBar->setStyleSheet(QString(
            "QLabel { color: %1; font-size: 12px; background: transparent; }"
        ).arg(stC));
    };

    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [applyTheme](ThemeManager::Theme) { applyTheme(); });

    onQueueChanged();
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void QueueWidget::onQueueChanged() {
    m_listWidget->blockSignals(true);
    m_listWidget->clear();

    const auto& q   = m_module->queue();
    const int   cur = m_module->currentIndex();

    for (int i = 0; i < q.size(); ++i)
        buildRow(q[i], i, i == cur);

    if (cur >= 0 && cur < m_listWidget->count())
        m_listWidget->scrollToItem(m_listWidget->item(cur));

    m_listWidget->blockSignals(false);

    m_statusBar->setText(QString("%1 track%2")
        .arg(q.size()).arg(q.size() == 1 ? "" : "s"));
}

void QueueWidget::buildRow(const M1::MediaItem& item, int index, bool isCurrent) {
    const QString artist = item.artist.isEmpty() ? "Unknown" : item.artist;
    const QString title  = item.displayTitle();
    const QString dur    = item.durationMs > 0 ? item.durationString() : "--:--";
    const QString text   = QString("%1. %2 \u2014 %3  [%4]")
                           .arg(index + 1).arg(artist, title, dur);

    auto* listItem = new QListWidgetItem(
        (isCurrent ? "\u25b6 " : "   ") + text, m_listWidget);

    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::EnterprisePro);

    if (isCurrent) {
        listItem->setBackground(isLight ? QColor(0x1c, 0x5c, 0xaa) : QColor(0x1e, 0x3a, 0x5f));
        listItem->setForeground(isLight ? QColor(0xff, 0xff, 0xff) : QColor(0x00, 0xd8, 0xff));
        QFont f = listItem->font();
        f.setBold(true);
        listItem->setFont(f);
    } else {
        listItem->setForeground(isLight ? QColor(0x00, 0x00, 0x00) : QColor(0xc8, 0xd8, 0xf0));
    }
    listItem->setData(Qt::UserRole, index);
}

void QueueWidget::onAddFiles() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, "Add Tracks to Queue", {},
        "Audio Files (*.mp3 *.flac *.wav *.aif *.aiff *.ogg *.opus *.m4a *.aac *.wv *.ape);;"
        "Playlists (*.m3u *.m3u8 *.pls);;"
        "All Files (*)");
    if (!paths.isEmpty()) m_module->addFiles(paths);
}

void QueueWidget::onPlayNow() {
    const int sel = m_listWidget->currentRow();
    if (sel >= 0) m_module->playNow(sel);
}

void QueueWidget::onClearQueue() {
    m_module->clearQueue();
}

void QueueWidget::onFilesDropped(const QStringList& paths) {
    m_module->addFiles(paths);
}

void QueueWidget::onRowMoved(int from, int to) {
    m_module->moveItem(from, to);
}

void QueueWidget::showContextMenu(const QPoint& pos) {
    auto* item = m_listWidget->itemAt(pos);
    const int idx = item ? item->data(Qt::UserRole).toInt() : -1;

    const auto tp2 = ThemePalette::forCurrentTheme();
    const QString menuBg   = tp2.cardBg.name();
    const QString menuText = tp2.text.name();
    const QString menuBord = tp2.border.name();
    const QString menuHov  = tp2.border.name();
    const QString menuHovT = tp2.info.name();

    QMenu menu(this);
    menu.setStyleSheet(QString(R"(
        QMenu {
            background-color: %1; color: %2;
            border: 1px solid %3; font-size: 12px;
        }
        QMenu::item { padding: 5px 20px; }
        QMenu::item:selected { background-color: %4; color: %5; }
        QMenu::separator { height: 1px; background: %3; margin: 3px 8px; }
    )").arg(menuBg, menuText, menuBord, menuHov, menuHovT));

    auto* actPlay   = menu.addAction("Play Now");
    menu.addSeparator();
    auto* actMoveUp   = menu.addAction("Move Up");
    auto* actMoveDown = menu.addAction("Move Down");
    auto* actTop      = menu.addAction("Move to Top");
    auto* actBottom   = menu.addAction("Move to Bottom");
    menu.addSeparator();
    auto* actRemove   = menu.addAction("Remove");
    auto* actClearAll = menu.addAction("Clear All");

    const int total = m_module->queueSize();
    if (idx < 0) {
        actPlay->setEnabled(false);
        actMoveUp->setEnabled(false);
        actMoveDown->setEnabled(false);
        actTop->setEnabled(false);
        actBottom->setEnabled(false);
        actRemove->setEnabled(false);
    } else {
        actMoveUp->setEnabled(idx > 0);
        actMoveDown->setEnabled(idx < total - 1);
        actTop->setEnabled(idx > 0);
        actBottom->setEnabled(idx < total - 1);
    }
    actClearAll->setEnabled(total > 0);

    auto* chosen = menu.exec(m_listWidget->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == actPlay) {
        m_module->playNow(idx);
    } else if (chosen == actMoveUp) {
        m_module->moveItem(idx, idx - 1);
    } else if (chosen == actMoveDown) {
        m_module->moveItem(idx, idx + 1);
    } else if (chosen == actTop) {
        m_module->moveItem(idx, 0);
    } else if (chosen == actBottom) {
        m_module->moveItem(idx, total - 1);
    } else if (chosen == actRemove) {
        m_module->removeItem(idx);
    } else if (chosen == actClearAll) {
        m_module->clearQueue();
    }
}
