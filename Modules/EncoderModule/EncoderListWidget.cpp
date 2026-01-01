#include "EncoderListWidget.h"
#include "EncoderSlot.h"
#include "EncoderVuPanel.h"
#include "DnasPoller.h"
#include "EncoderConfigDialog.h"
#include "EncoderLogDialog.h"
#include "LiveMonitorWindow.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMenu>
#include <QFont>
#include <QColor>
#include <QBrush>

// ─────────────────────────────────────────────────────────────────────────────
EncoderListWidget::EncoderListWidget(QList<EncoderSlot*>& slotList,
                                     DnasPoller* poller,
                                     QWidget* parent)
    : QWidget(parent)
    , m_poller(poller)
{
    m_slots = &slotList;
    setObjectName("EncoderListWidget");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(6);

    m_addBtn      = new QPushButton("+ Add",    this);
    m_removeBtn   = new QPushButton("Remove",   this);
    m_startAllBtn = new QPushButton("▶ Start All", this);
    m_stopAllBtn  = new QPushButton("■ Stop All",  this);
    m_liveLabel   = new QLabel("– Live", this);

    for (auto* btn : {m_addBtn, m_removeBtn, m_startAllBtn, m_stopAllBtn})
        btn->setFixedHeight(22);

    m_liveLabel->setObjectName("EncoderLiveLabel");

    toolbar->addWidget(m_addBtn);
    toolbar->addWidget(m_removeBtn);
    toolbar->addSpacing(8);
    toolbar->addWidget(m_startAllBtn);
    toolbar->addWidget(m_stopAllBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_liveLabel);

    root->addLayout(toolbar);

    // ── Table ────────────────────────────────────────────────────────────────
    m_table = new QTableWidget(this);
    m_table->setObjectName("EncoderTable");
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({"#","Name","Status","Codec","Bitrate","Server","Listeners","Current Song"});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColName,        QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(ColServer,      QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColCurrentSong, QHeaderView::Stretch);
    m_table->setColumnWidth(ColNum,       30);
    m_table->setColumnWidth(ColName,     120);
    m_table->setColumnWidth(ColStatus,   110);
    m_table->setColumnWidth(ColCodec,     70);
    m_table->setColumnWidth(ColBitrate,   60);
    m_table->setColumnWidth(ColListeners, 70);

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    // ── Content: Table + VU Panel side by side ──────────────────────────────
    auto* content = new QHBoxLayout;
    content->setSpacing(0);
    content->addWidget(m_table, /*stretch=*/1);

    m_vuPanel = new EncoderVuPanel(m_slots, this);
    content->addWidget(m_vuPanel, /*stretch=*/0);

    root->addLayout(content);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_addBtn,      &QPushButton::clicked, this, &EncoderListWidget::onAddClicked);
    connect(m_removeBtn,   &QPushButton::clicked, this, &EncoderListWidget::onRemoveClicked);
    connect(m_startAllBtn, &QPushButton::clicked, this, &EncoderListWidget::onStartAllClicked);
    connect(m_stopAllBtn,  &QPushButton::clicked, this, &EncoderListWidget::onStopAllClicked);
    connect(m_table,       &QTableWidget::cellDoubleClicked, this, &EncoderListWidget::onDoubleClicked);
    connect(m_table,       &QTableWidget::customContextMenuRequested, this, &EncoderListWidget::onContextMenu);

    // ── Timer ─────────────────────────────────────────────────────────────────
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(2000);
    connect(m_refreshTimer, &QTimer::timeout, this, &EncoderListWidget::refresh);
    m_refreshTimer->start();

    rebuild();
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderListWidget::rebuild()
{
    m_table->setRowCount(m_slots->size());
    for (int i = 0; i < m_slots->size(); ++i) {
        const auto& cfg = (*m_slots)[i]->config();
        auto setCell = [&](int col, const QString& text) {
            auto* item = m_table->item(i, col);
            if (!item) { item = new QTableWidgetItem; m_table->setItem(i, col, item); }
            item->setText(text);
        };
        setCell(ColNum,    QString::number(i + 1));
        setCell(ColName,   cfg.name);
        setCell(ColCodec,  cfg.codecName());
        setCell(ColBitrate,QString::number(cfg.bitrate) + "k");
        setCell(ColServer, cfg.host + ":" + QString::number(cfg.port) + cfg.mount);
        setCell(ColStatus, "IDLE");
        setCell(ColListeners, "\xe2\x80\x93");
        setCell(ColCurrentSong, "\xe2\x80\x93");
        m_table->setRowHeight(i, 20);
    }
    if (m_vuPanel) m_vuPanel->rebuild();
    refresh();
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderListWidget::refresh()
{
    int liveCount = 0;

    for (int i = 0; i < m_slots->size(); ++i) {
        auto*  slot = (*m_slots)[i];
        int    st   = static_cast<int>(slot->state());
        const auto& cfg = slot->config();

        // Status cell
        auto* statusItem = m_table->item(i, ColStatus);
        if (!statusItem) { statusItem = new QTableWidgetItem; m_table->setItem(i, ColStatus, statusItem); }
        statusItem->setText(stateText(st));
        statusItem->setForeground(QBrush(stateColor(st)));
        QFont f = statusItem->font();
        f.setItalic(stateItalic(st));
        statusItem->setFont(f);

        // Listeners
        auto* lisItem = m_table->item(i, ColListeners);
        if (!lisItem) { lisItem = new QTableWidgetItem; m_table->setItem(i, ColListeners, lisItem); }
        if (m_poller && slot->state() == EncoderSlot::State::Streaming) {
            auto stats = m_poller->stats(cfg.host, cfg.port, cfg.mount);
            lisItem->setText(stats.listeners >= 0 ? QString::number(stats.listeners) : "\xe2\x80\x93");
        } else {
            lisItem->setText("\xe2\x80\x93");
        }

        // Current Song
        auto* songItem = m_table->item(i, ColCurrentSong);
        if (!songItem) { songItem = new QTableWidgetItem; m_table->setItem(i, ColCurrentSong, songItem); }
        const QString streamTitle = slot->lastStreamTitle();
        songItem->setText(streamTitle.isEmpty() ? QString::fromUtf8("\xe2\x80\x93") : streamTitle);

        using S = EncoderSlot::State;
        if (slot->state() == S::Streaming) ++liveCount;
    }

    m_liveLabel->setText(QString("● %1 Live  ◌ %2 Idle")
                         .arg(liveCount)
                         .arg(m_slots->size() - liveCount));
}

// ─────────────────────────────────────────────────────────────────────────────
QString EncoderListWidget::stateText(int st) const
{
    using S = EncoderSlot::State;
    switch (static_cast<S>(st)) {
        case S::Idle:         return "◌  IDLE";
        case S::Starting:     return "…  STARTING";
        case S::Connecting:   return "→  CONNECTING";
        case S::Streaming:    return "●  LIVE";
        case S::Reconnecting: return "↻  RECONNECTING";
        case S::Sleep:        return "⊘  SLEEP";
        case S::Error:        return "✗  ERROR";
    }
    return "?";
}

QColor EncoderListWidget::stateColor(int st) const
{
    using S = EncoderSlot::State;
    switch (static_cast<S>(st)) {
        case S::Streaming:    return QColor("#16a34a");  // green — visible on light+dark
        case S::Connecting:
        case S::Reconnecting: return QColor("#d97706");  // amber — visible on light+dark
        case S::Starting:     return QColor("#2563eb");  // blue
        case S::Sleep:        return QColor("#6b7280");  // slate
        case S::Error:        return QColor("#dc2626");  // red
        default:              return QColor("#6b7280");  // slate for idle
    }
}

bool EncoderListWidget::stateItalic(int st) const
{
    return static_cast<EncoderSlot::State>(st) == EncoderSlot::State::Sleep;
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderListWidget::onDoubleClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_slots->size()) return;
    EncoderConfigDialog dlg((*m_slots)[row], this);
    dlg.exec();
    rebuild();  // refresh in case config changed
}

void EncoderListWidget::onContextMenu(const QPoint& pos)
{
    const int row = m_table->rowAt(pos.y());
    if (row < 0 || row >= m_slots->size()) return;

    auto* slot = (*m_slots)[row];
    using S = EncoderSlot::State;
    const S st = slot->state();

    QMenu menu(this);

    auto* connectAct    = menu.addAction("Connect");
    auto* disconnectAct = menu.addAction("Disconnect");
    auto* wakeAct       = menu.addAction("Wake");
    menu.addSeparator();
    auto* configAct     = menu.addAction("Configure\xe2\x80\xa6");
    auto* logAct        = menu.addAction("View Encoder Event Log");
    auto* monitorAct    = menu.addAction("Live Monitor");
    menu.addSeparator();
    auto* removeAct     = menu.addAction("Remove");

    connectAct->setEnabled(st == S::Idle || st == S::Error);
    disconnectAct->setEnabled(st == S::Streaming || st == S::Connecting || st == S::Reconnecting);
    wakeAct->setEnabled(st == S::Sleep);
    removeAct->setEnabled(st == S::Idle || st == S::Error);

    auto* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));

    if (!chosen) return;
    if (chosen == connectAct)    { slot->connectToServer(); }
    else if (chosen == disconnectAct) { slot->disconnectFromServer(); }
    else if (chosen == wakeAct)  { slot->wake(); }
    else if (chosen == configAct) {
        EncoderConfigDialog dlg(slot, this);
        dlg.exec();
        rebuild();
    }
    else if (chosen == logAct) {
        // Non-modal log dialog — user can keep it open while watching encoder
        auto* dlg = new EncoderLogDialog(slot->eventLog(), slot->config().name, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    }
    else if (chosen == monitorAct) {
        // Reuse existing window if already open for this slot
        auto it = m_liveMonitors.find(slot);
        if (it != m_liveMonitors.end() && *it) {
            (*it)->raise();
            (*it)->activateWindow();
        } else {
            auto* win = new LiveMonitorWindow(slot, nullptr);
            win->setAttribute(Qt::WA_DeleteOnClose);
            connect(win, &QObject::destroyed, this, [this, slot]() {
                m_liveMonitors.remove(slot);
            });
            m_liveMonitors[slot] = win;
            win->show();
        }
    }
    else if (chosen == removeAct) {
        emit removeSlotRequested(row);
    }
}

void EncoderListWidget::onAddClicked()    { emit addSlotRequested(); }
void EncoderListWidget::onRemoveClicked()
{
    const int row = m_table->currentRow();
    if (row >= 0 && row < m_slots->size())
        emit removeSlotRequested(row);
}

void EncoderListWidget::onStartAllClicked()
{
    for (auto* s : *m_slots)
        if (s->state() == EncoderSlot::State::Idle)
            s->connectToServer();
}

void EncoderListWidget::onStopAllClicked()
{
    for (auto* s : *m_slots)
        if (s->state() != EncoderSlot::State::Idle)
            s->disconnectFromServer();
}
