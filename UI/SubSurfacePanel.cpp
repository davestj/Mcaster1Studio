#include "SubSurfacePanel.h"
#include "ModuleDock.h"
#include "SurfaceTray.h"
#include "IModule.h"
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QSettings>
#include <QApplication>
#include <QScreen>
#include <QDebug>

SubSurfacePanel::SubSurfacePanel(const QString& name, QWidget* parent)
    : QWidget(parent)
    , m_name(name)
{
    setObjectName("SubSurfacePanel");

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_canvas = new QWidget();
    m_canvas->setObjectName("SurfaceInner");
    m_canvas->setAcceptDrops(true);
    m_canvas->installEventFilter(this);

    m_splitter = new QSplitter(Qt::Horizontal, m_canvas);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(8);
    m_splitter->setObjectName("SurfaceSplitter");
    m_splitter->setAcceptDrops(true);
    m_splitter->installEventFilter(this);

    auto* canvasLayout = new QHBoxLayout(m_canvas);
    canvasLayout->setContentsMargins(8, 8, 8, 8);
    canvasLayout->setSpacing(0);
    canvasLayout->addWidget(m_splitter);

    m_scroll->setWidget(m_canvas);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_scroll);

    m_dropIndicator = new QFrame(m_canvas);
    m_dropIndicator->setFrameShape(QFrame::VLine);
    m_dropIndicator->setFrameShadow(QFrame::Plain);
    m_dropIndicator->setStyleSheet(
        "background:#0ea5e9; border:none; min-width:3px; max-width:3px;");
    m_dropIndicator->setFixedWidth(3);
    m_dropIndicator->hide();
    m_dropIndicator->raise();

    m_hDropIndicator = new QFrame(m_canvas);
    m_hDropIndicator->setFrameShape(QFrame::HLine);
    m_hDropIndicator->setFrameShadow(QFrame::Plain);
    m_hDropIndicator->setStyleSheet(
        "background:#0ea5e9; border:none; min-height:3px; max-height:3px;");
    m_hDropIndicator->setFixedHeight(3);
    m_hDropIndicator->hide();
    m_hDropIndicator->raise();
}

SubSurfacePanel::~SubSurfacePanel() = default;

// ─── Column helpers ───────────────────────────────────────────────────────────
QSplitter* SubSurfacePanel::makeColumn() {
    auto* col = new QSplitter(Qt::Vertical, m_canvas);
    col->setChildrenCollapsible(false);
    col->setHandleWidth(8);
    col->setObjectName("SurfaceColumn");
    return col;
}

QSplitter* SubSurfacePanel::columnForDock(ModuleDock* dock) const {
    for (auto* col : m_columns)
        for (int i = 0; i < col->count(); ++i)
            if (col->widget(i) == dock) return col;
    return nullptr;
}

void SubSurfacePanel::pruneEmptyColumn(QSplitter* col) {
    if (!col || col->count() > 0) return;
    m_columns.removeOne(col);
    col->setParent(nullptr);
    col->deleteLater();
}

void SubSurfacePanel::syncDocksFromColumns() {
    m_docks.clear();
    for (auto* col : m_columns)
        for (int i = 0; i < col->count(); ++i)
            if (auto* dock = qobject_cast<ModuleDock*>(col->widget(i)))
                m_docks.append(dock);
}

// ─── Add / Remove modules ─────────────────────────────────────────────────────
void SubSurfacePanel::addModule(M1::IModule* module, bool startFloating) {
    auto* dock = new ModuleDock(module, m_canvas);

    connect(dock, &ModuleDock::closeRequested,
            this, &SubSurfacePanel::onModuleCloseRequested);
    connect(dock, &ModuleDock::removeRequested,
            this, [this](ModuleDock* d) { onModuleRemoveRequested(d); });
    connect(dock, &ModuleDock::sendToTrayRequested,
            this, [this](ModuleDock* d) { sendDockToTray(d); });
    connect(dock, &ModuleDock::floatRequested,
            this, &SubSurfacePanel::onModuleFloatRequested);
    connect(dock, &ModuleDock::dockRequested,
            this, &SubSurfacePanel::onModuleDockRequested);

    if (startFloating) {
        // Start as a free-floating window at minimum size, staggered position
        const QSize minSz = module->minimumModuleSize();
        const int offset  = (m_floatingDocks.size() % 6) * 28;
        const int dw = minSz.width()  > 0 ? minSz.width()  : 340;
        const int dh = minSz.height() > 0 ? minSz.height() : 260;
        dock->resize(dw, dh);
        {
            int fx = 20 + offset;
            int fy = 20 + offset;
            if (m_canvas->width() > 0 && m_canvas->height() > 0) {
                fx = qBound(0, fx, qMax(0, m_canvas->width()  - dw));
                fy = qBound(0, fy, qMax(0, m_canvas->height() - dh));
            }
            dock->move(fx, fy);
        }
        dock->setFloatingInCanvas(true);
        dock->show();
        dock->raise();
        m_floatingDocks.append(dock);
    } else {
        auto* col = makeColumn();
        col->addWidget(dock);
        col->setStretchFactor(0, 1);
        m_splitter->addWidget(col);
        m_splitter->setStretchFactor(m_splitter->count() - 1, 1);
        m_columns.append(col);
        m_docks.append(dock);
    }

    module->initialize();
    emit dockCreated(dock);
}

void SubSurfacePanel::removeModule(M1::IModule* module) {
    for (int i = 0; i < m_docks.size(); ++i) {
        if (m_docks[i]->module() == module) {
            emit dockDestroyed(m_docks[i]);
            module->shutdown();
            auto* col = columnForDock(m_docks[i]);
            m_docks[i]->setParent(nullptr);
            m_docks.removeAt(i);
            pruneEmptyColumn(col);
            return;
        }
    }
    for (int i = 0; i < m_floatingDocks.size(); ++i) {
        if (m_floatingDocks[i]->module() == module) {
            emit dockDestroyed(m_floatingDocks[i]);
            module->shutdown();
            m_floatingDocks[i]->setParent(nullptr);
            m_floatingDocks[i]->deleteLater();
            m_floatingDocks.removeAt(i);
            return;
        }
    }
}

QList<M1::IModule*> SubSurfacePanel::modules() const {
    QList<M1::IModule*> result;
    for (auto* d : m_docks)         result.append(d->module());
    for (auto* d : m_floatingDocks) result.append(d->module());
    return result;
}

QList<ModuleDock*> SubSurfacePanel::allDocks() const {
    QList<ModuleDock*> result;
    result.append(m_docks);
    result.append(m_floatingDocks);
    return result;
}

// ─── Module slot handlers ─────────────────────────────────────────────────────
void SubSurfacePanel::onModuleCloseRequested(M1::IModule* module) {
    removeModule(module);
}

void SubSurfacePanel::onModuleRemoveRequested(ModuleDock* dock) {
    M1::IModule* mod = dock->module();
    emit dockDestroyed(dock);
    mod->shutdown();
    auto* col = columnForDock(dock);
    m_docks.removeOne(dock);
    m_floatingDocks.removeOne(dock);
    dock->setParent(nullptr);
    dock->deleteLater();
    pruneEmptyColumn(col);
}

void SubSurfacePanel::onModuleFloatRequested(ModuleDock* dock) {
    const QPoint globalPos = dock->mapToGlobal(QPoint(0, 0));
    auto* col = columnForDock(dock);
    if (col) {
        dock->setParent(nullptr);
        dock->setParent(m_canvas);
        pruneEmptyColumn(col);
    }
    m_docks.removeOne(dock);
    {
        QPoint pos = m_canvas->mapFromGlobal(globalPos);
        pos.setX(qBound(0, pos.x(), qMax(0, m_canvas->width()  - dock->width())));
        pos.setY(qBound(0, pos.y(), qMax(0, m_canvas->height() - dock->height())));
        dock->move(pos);
    }
    dock->setFloatingInCanvas(true);
    dock->show();
    dock->raise();
    m_floatingDocks.append(dock);
}

void SubSurfacePanel::onModuleDockRequested(ModuleDock* dock) {
    m_floatingDocks.removeOne(dock);
    dock->setFloatingInCanvas(false);
    auto* col = makeColumn();
    dock->setParent(m_canvas);
    col->addWidget(dock);
    m_splitter->addWidget(col);
    m_splitter->setStretchFactor(m_splitter->count() - 1, 1);
    dock->show();
    m_columns.append(col);
    m_docks.append(dock);
}

void SubSurfacePanel::sendDockToTray(ModuleDock* dock) {
    if (!dock) return;
    auto* col = columnForDock(dock);
    m_docks.removeOne(dock);
    m_floatingDocks.removeOne(dock);
    dock->setFloatingInCanvas(false);
    dock->setVisible(false);
    dock->setParent(this);
    pruneEmptyColumn(col);
    emit dockSentToTray(dock);
}

void SubSurfacePanel::restoreFromTray(ModuleDock* dock) {
    if (!dock) return;
    auto* col = makeColumn();
    dock->setParent(m_canvas);
    col->addWidget(dock);
    m_splitter->addWidget(col);
    m_splitter->setStretchFactor(m_splitter->count() - 1, 1);
    dock->setVisible(true);
    m_columns.append(col);
    m_docks.append(dock);
    emit dockRestoredFromTray(dock);
}

// ─── Public tray command slots ────────────────────────────────────────────────
void SubSurfacePanel::focusDock(ModuleDock* dock) {
    if (!dock->isVisible())
        restoreFromTray(dock);
    else
        dock->raise();
}

void SubSurfacePanel::floatDock(ModuleDock* dock) {
    onModuleFloatRequested(dock);
}

void SubSurfacePanel::redockDock(ModuleDock* dock) {
    onModuleDockRequested(dock);
}

void SubSurfacePanel::closeDock(ModuleDock* dock) {
    onModuleRemoveRequested(dock);
}

// ─── Drag / Drop via event filter ─────────────────────────────────────────────
bool SubSurfacePanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_canvas && watched != m_splitter)
        return QWidget::eventFilter(watched, event);

    auto toCanvas = [&](const QPoint& p) -> QPoint {
        return (watched == m_splitter) ? p + m_splitter->pos() : p;
    };

    switch (event->type()) {
    case QEvent::DragEnter:
        return handleDragEnter(static_cast<QDragEnterEvent*>(event));

    case QEvent::DragMove: {
        auto* e = static_cast<QDragMoveEvent*>(event);
        if (!e->mimeData()->hasFormat("application/x-m1-module-dock")) return false;
        updateDropIndicators(calcDropPos(toCanvas(e->position().toPoint())));
        e->acceptProposedAction();
        return true;
    }

    case QEvent::Drop: {
        auto* e = static_cast<QDropEvent*>(event);
        if (!e->mimeData()->hasFormat("application/x-m1-module-dock")) return false;
        handleDragLeave();
        ModuleDock* src = ModuleDock::draggingDock();
        if (!src) return false;

        const DropPos dp = calcDropPos(toCanvas(e->position().toPoint()));

        if (dp.stack && dp.col) {
            auto* srcCol = columnForDock(src);
            const bool fromOutside = (srcCol == nullptr);
            if (fromOutside) {
                onModuleDockRequested(src);
                m_docks.removeOne(src);
                auto* srcOwnCol = columnForDock(src);
                if (srcOwnCol) {
                    m_columns.removeOne(srcOwnCol);
                    srcOwnCol->setParent(nullptr);
                }
                src->setParent(dp.col);
                dp.col->insertWidget(dp.rowIdx, src);
                dp.col->setStretchFactor(dp.rowIdx, 1);
                src->show();
                syncDocksFromColumns();
            } else {
                src->setParent(nullptr);
                pruneEmptyColumn(srcCol);
                const int insertRow = qBound(0, dp.rowIdx, dp.col->count());
                src->setParent(dp.col);
                dp.col->insertWidget(insertRow, src);
                dp.col->setStretchFactor(insertRow, 1);
                src->show();
                syncDocksFromColumns();
            }
        } else {
            auto* srcCol = columnForDock(src);
            const int destColIdx = qBound(0, dp.colIdx, m_columns.size());
            if (srcCol == nullptr) {
                auto* col = makeColumn();
                src->setParent(m_canvas);
                col->addWidget(src);
                col->setStretchFactor(0, 1);
                m_splitter->insertWidget(destColIdx, col);
                m_columns.insert(destColIdx, col);
                src->show();
            } else {
                const int srcColIdx = m_columns.indexOf(srcCol);
                if (srcColIdx != destColIdx && srcColIdx != destColIdx - 1) {
                    m_columns.removeAt(srcColIdx);
                    const int adjusted = (destColIdx > srcColIdx) ? destColIdx - 1 : destColIdx;
                    m_columns.insert(adjusted, srcCol);
                    for (int i = 0; i < m_columns.size(); ++i)
                        m_splitter->insertWidget(i, m_columns[i]);
                }
            }
            syncDocksFromColumns();
        }

        e->acceptProposedAction();
        return true;
    }

    case QEvent::DragLeave:
        handleDragLeave();
        return false;

    case QEvent::ContextMenu: {
        auto* e = static_cast<QContextMenuEvent*>(event);
        showCanvasContextMenu(e->globalPos());
        return true;
    }

    default:
        return QWidget::eventFilter(watched, event);
    }
}

bool SubSurfacePanel::handleDragEnter(QDragEnterEvent* e) {
    if (e->mimeData()->hasFormat("application/x-m1-module-dock")) {
        e->acceptProposedAction();
        return true;
    }
    return false;
}

bool SubSurfacePanel::handleDragMove(QDragMoveEvent* e) {
    if (!e->mimeData()->hasFormat("application/x-m1-module-dock")) return false;
    updateDropIndicators(calcDropPos(e->position().toPoint()));
    e->acceptProposedAction();
    return true;
}

bool SubSurfacePanel::handleDrop(QDropEvent* e) {
    (void)e;
    return false;
}

void SubSurfacePanel::handleDragLeave() {
    m_dropIndicator->hide();
    m_hDropIndicator->hide();
    m_dropIndex = -1;
}

// ─── Drop position ────────────────────────────────────────────────────────────
SubSurfacePanel::DropPos SubSurfacePanel::calcDropPos(const QPoint& canvasPos) const {
    const QPoint splitterOrigin = m_splitter->pos();

    for (int ci = 0; ci < m_columns.size(); ++ci) {
        auto* col = m_columns[ci];
        const QRect colRect = col->geometry().translated(splitterOrigin);
        if (!colRect.contains(canvasPos)) continue;

        for (int ri = 0; ri < col->count(); ++ri) {
            auto* w = col->widget(ri);
            if (!w) continue;
            const QRect dockRect = w->geometry().translated(colRect.topLeft());
            if (!dockRect.contains(canvasPos)) continue;

            const int stackThreshold = dockRect.top() + dockRect.height() * 55 / 100;
            if (canvasPos.y() > stackThreshold)
                return { true, ci, col, ri + 1 };

            const bool leftHalf = canvasPos.x() < dockRect.center().x();
            return { false, leftHalf ? ci : ci + 1, nullptr, -1 };
        }
    }

    for (int ci = 0; ci < m_columns.size(); ++ci) {
        const QRect colRect = m_columns[ci]->geometry().translated(splitterOrigin);
        if (canvasPos.x() < colRect.center().x())
            return { false, ci, nullptr, -1 };
    }
    return { false, (int)m_columns.size(), nullptr, -1 };
}

// ─── Drop indicators ──────────────────────────────────────────────────────────
void SubSurfacePanel::updateDropIndicators(const DropPos& dp) {
    const QPoint splitterOrigin = m_splitter->pos();
    const int canvasH = m_canvas->height() - 16;

    if (dp.stack && dp.col) {
        m_dropIndicator->hide();
        const QRect colRect = dp.col->geometry().translated(splitterOrigin);
        int y = 0;
        if (dp.rowIdx <= 0 || dp.col->count() == 0) {
            y = colRect.top() + 4;
        } else if (dp.rowIdx >= dp.col->count()) {
            auto* last = dp.col->widget(dp.col->count() - 1);
            y = last->geometry().bottom() + colRect.top() + 2;
        } else {
            auto* above = dp.col->widget(dp.rowIdx - 1);
            auto* below = dp.col->widget(dp.rowIdx);
            y = colRect.top() + (above->geometry().bottom() + below->geometry().top()) / 2;
        }
        m_hDropIndicator->setGeometry(colRect.left(), y - 1, colRect.width(), 3);
        m_hDropIndicator->show();
        m_hDropIndicator->raise();
    } else {
        m_hDropIndicator->hide();
        const int colIdx = dp.colIdx;
        int x = splitterOrigin.x() + 4;
        if (colIdx > 0 && colIdx <= m_columns.size()) {
            const QRect prev = m_columns[colIdx - 1]->geometry();
            const int nextLeft = (colIdx < m_columns.size())
                               ? m_columns[colIdx]->geometry().left()
                               : prev.right() + 8;
            x = splitterOrigin.x() + prev.right() + (nextLeft - prev.right()) / 2;
        } else if (!m_columns.isEmpty()) {
            x = splitterOrigin.x() + m_columns[0]->geometry().left() - 4;
        }
        if (m_dropIndex == colIdx) return;
        m_dropIndex = colIdx;
        m_dropIndicator->setGeometry(x - 1, 8, 3, canvasH);
        m_dropIndicator->show();
        m_dropIndicator->raise();
    }
}

// ─── Canvas context menu ──────────────────────────────────────────────────────
void SubSurfacePanel::showCanvasContextMenu(const QPoint& globalPos) {
    static const QList<QPair<QString, QString>> kModules = {
        {"com.mcaster1.vumeter",  "VU Meter"},
        {"com.mcaster1.deck.a",   "Deck A"},
        {"com.mcaster1.deck.b",   "Deck B"},
        {"com.mcaster1.deck",     "Deck (Combined A+B)"},
        {"com.mcaster1.library",  "Media Library"},
        {"com.mcaster1.encoder",  "Encoder"},
        {"com.mcaster1.effects",  "Effects Rack"},
        {"com.mcaster1.metadata", "Metadata"},
        {"com.mcaster1.playlist", "Playlist / AutoDJ"},
        {"com.mcaster1.ptt",      "Push-to-Talk"},
        {"com.mcaster1.podcast",  "Podcast"},
        {"com.mcaster1.video",    "Video"},
        {"com.mcaster1.monitor",  "Stream Monitor"},
        {"com.mcaster1.queue",       "Queue"},
        {"com.mcaster1.crossfader",  "Crossfader"},
    };

    QMenu menu(this);
    QMenu* addMenu = menu.addMenu("Add Module");
    for (const auto& [id, name] : kModules) {
        addMenu->addAction(name, this, [this, id = id]() {
            emit addModuleRequested(id);
        });
    }
    menu.addSeparator();
    menu.addAction("Save Settings", this, [this]() {
        emit saveSettingsRequested();
    });
    menu.exec(globalPos);
}

// ─── Layout snapshot ──────────────────────────────────────────────────────────
SurfaceLayoutSnapshot SubSurfacePanel::layoutSnapshot() const {
    SurfaceLayoutSnapshot snap;
    snap.columnWidths = m_splitter->sizes();
    for (auto* col : m_columns)
        snap.columnHeights.append(col->sizes());
    for (auto* dock : m_floatingDocks) {
        snap.floatingItems.append({
            dock->module()->moduleId(),
            dock->x(), dock->y(), dock->width(), dock->height()
        });
    }
    return snap;
}

void SubSurfacePanel::applyLayoutSnapshot(const SurfaceLayoutSnapshot& snap) {
    for (const auto& item : snap.floatingItems) {
        ModuleDock* found = nullptr;
        for (auto* dock : m_docks) {
            if (dock->module()->moduleId() == item.moduleId) { found = dock; break; }
        }
        if (!found) continue;
        onModuleFloatRequested(found);
        found->resize(item.w, item.h);
        {
            int rx = qBound(0, item.x, qMax(0, m_canvas->width()  - item.w));
            int ry = qBound(0, item.y, qMax(0, m_canvas->height() - item.h));
            found->move(rx, ry);
        }
    }
    if (snap.columnWidths.size() == m_splitter->count())
        m_splitter->setSizes(snap.columnWidths);
    for (int ci = 0; ci < qMin(snap.columnHeights.size(), (int)m_columns.size()); ++ci) {
        if (snap.columnHeights[ci].size() == m_columns[ci]->count())
            m_columns[ci]->setSizes(snap.columnHeights[ci]);
    }
}

// ─── QSettings layout persistence ────────────────────────────────────────────
void SubSurfacePanel::saveLayout(QSettings& s, const QString& keyBase) const {
    s.setValue(keyBase + "/floatingCount", (int)m_floatingDocks.size());
    for (int i = 0; i < m_floatingDocks.size(); ++i) {
        const QString key = keyBase + QString("/floating%1").arg(i);
        auto* dock = m_floatingDocks[i];
        s.setValue(key + "/moduleId", dock->module()->moduleId());
        s.setValue(key + "/x",        dock->x());
        s.setValue(key + "/y",        dock->y());
        s.setValue(key + "/width",    dock->width());
        s.setValue(key + "/height",   dock->height());
    }
    {
        const QList<int> sizes = m_splitter->sizes();
        QStringList parts;
        parts.reserve(sizes.size());
        for (int v : sizes) parts << QString::number(v);
        s.setValue(keyBase + "/columnWidths", parts.join(','));
    }
    s.setValue(keyBase + "/columnCount", m_columns.size());
    for (int ci = 0; ci < m_columns.size(); ++ci) {
        const QList<int> rows = m_columns[ci]->sizes();
        QStringList parts;
        parts.reserve(rows.size());
        for (int v : rows) parts << QString::number(v);
        s.setValue(keyBase + QString("/column%1/rowHeights").arg(ci), parts.join(','));
    }
}

void SubSurfacePanel::restoreLayout(QSettings& s, const QString& keyBase) {
    const int floatingCount = s.value(keyBase + "/floatingCount", 0).toInt();
    for (int i = 0; i < floatingCount; ++i) {
        const QString key      = keyBase + QString("/floating%1").arg(i);
        const QString moduleId = s.value(key + "/moduleId").toString();
        const int x = s.value(key + "/x",      20 + i * 20).toInt();
        const int y = s.value(key + "/y",      20 + i * 20).toInt();
        const int w = s.value(key + "/width",  300).toInt();
        const int h = s.value(key + "/height", 200).toInt();
        ModuleDock* found = nullptr;
        for (auto* dock : m_docks) {
            if (dock->module()->moduleId() == moduleId) { found = dock; break; }
        }
        if (!found) continue;
        onModuleFloatRequested(found);
        found->resize(w, h);
        {
            int rx = qBound(0, x, qMax(0, m_canvas->width()  - w));
            int ry = qBound(0, y, qMax(0, m_canvas->height() - h));
            found->move(rx, ry);
        }
    }
    {
        const QString val = s.value(keyBase + "/columnWidths").toString();
        if (!val.isEmpty()) {
            QList<int> sizes;
            for (const QString& p : val.split(',')) {
                bool ok; const int v = p.trimmed().toInt(&ok);
                if (ok) sizes << v;
            }
            if (sizes.size() == m_splitter->count())
                m_splitter->setSizes(sizes);
        }
    }
    for (int ci = 0; ci < m_columns.size(); ++ci) {
        const QString val = s.value(
            keyBase + QString("/column%1/rowHeights").arg(ci)).toString();
        if (val.isEmpty()) continue;
        QList<int> rows;
        for (const QString& p : val.split(',')) {
            bool ok; const int v = p.trimmed().toInt(&ok);
            if (ok) rows << v;
        }
        if (rows.size() == m_columns[ci]->count())
            m_columns[ci]->setSizes(rows);
    }
}

void SubSurfacePanel::initSplitterSizes() {
    if (m_splitter->count() == 0) return;
    auto* screen = QApplication::primaryScreen();
    const int screenW = screen ? screen->availableGeometry().width() : 1920;
    if (screenW < 1400) {
        const int sw  = m_splitter->width() > 0 ? m_splitter->width() : screenW;
        const int per = sw / m_splitter->count();
        m_splitter->setSizes(QList<int>(m_splitter->count(), per));
    }
}
