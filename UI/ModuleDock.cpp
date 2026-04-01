#include "ModuleDock.h"
#include "ResizableWidget.h"
#include "IModule.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QLabel>
#include <QSvgRenderer>
#include <QPixmap>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QStyleOption>
#include <QMenu>
#include <QAction>

// ─── Static member definition ─────────────────────────────────────────────────
ModuleDock* ModuleDock::s_dragging = nullptr;

// ─── SVG icon loader ──────────────────────────────────────────────────────────
QIcon ModuleDock::svgIcon(const QString& path, QSize size) {
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) return QIcon();
    QPixmap pm(size * 2); // 2x for HiDPI
    pm.fill(Qt::transparent);
    {
        QPainter painter(&pm);
        renderer.render(&painter);
    } // painter.end() called — safe to modify pm now
    pm.setDevicePixelRatio(2.0);
    return QIcon(pm);
}

QString ModuleDock::iconPathForModuleId(const QString& id) {
    if (id.contains("vumeter"))  return ":/resources/icons/vu-meter.svg";
    if (id.contains("deck"))     return ":/resources/icons/deck.svg";
    if (id.contains("library"))  return ":/resources/icons/library.svg";
    if (id.contains("encoder"))  return ":/resources/icons/encoder.svg";
    if (id.contains("effects"))  return ":/resources/icons/effects.svg";
    if (id.contains("metadata")) return ":/resources/icons/metadata.svg";
    if (id.contains("playlist")) return ":/resources/icons/playlist.svg";
    // Podcast Surface modules — check specific IDs before generic "podcast"
    if (id.contains("podcast.mixer"))      return ":/resources/icons/pod-mixer.svg";
    if (id.contains("podcast.ptt"))        return ":/resources/icons/pod-ptt.svg";
    if (id.contains("podcast.recorder"))   return ":/resources/icons/pod-recorder.svg";
    if (id.contains("podcast.soundboard")) return ":/resources/icons/pod-soundboard.svg";
    if (id.contains("podcast.fx"))         return ":/resources/icons/pod-fx.svg";
    if (id.contains("podcast.editor"))     return ":/resources/icons/pod-editor.svg";
    if (id.contains("podcast.encode"))     return ":/resources/icons/pod-encode.svg";
    if (id.contains("podcast.transcribe")) return ":/resources/icons/pod-transcribe.svg";
    if (id.contains("podcast.shownotes"))  return ":/resources/icons/pod-shownotes.svg";
    if (id.contains("podcast.rss"))        return ":/resources/icons/pod-rss.svg";
    if (id.contains("podcast.publisher"))  return ":/resources/icons/pod-publisher.svg";
    if (id.contains("podcast.analytics"))  return ":/resources/icons/pod-analytics.svg";
    if (id.contains("podcast.remote"))     return ":/resources/icons/pod-remote.svg";
    if (id.contains("podcast"))  return ":/resources/icons/podcast.svg";
    if (id.contains("ptt"))      return ":/resources/icons/ptt.svg";
    if (id.contains("video"))    return ":/resources/icons/video.svg";
    if (id.contains("monitor"))  return ":/resources/icons/monitor.svg";
    if (id.contains("cartwall"))  return ":/resources/icons/cartwall.svg";
    if (id.contains("database")) return ":/resources/icons/database.svg";
    if (id.contains("health"))   return ":/resources/icons/health.svg";
    if (id.contains("timerclock"))  return ":/resources/icons/timerclock.svg";
    if (id.contains("graphics"))    return ":/resources/icons/graphics-engine.svg";
    if (id.contains("lyrics"))      return ":/resources/icons/lyrics-caster.svg";
    if (id.contains("scripture"))   return ":/resources/icons/scripture-caster.svg";
    if (id.contains("announce"))    return ":/resources/icons/announce-caster.svg";
    if (id.contains("teleprompt"))  return ":/resources/icons/teleprompt.svg";
    if (id.contains("mediacaster")) return ":/resources/icons/media-caster.svg";
    if (id.contains("stagemon"))    return ":/resources/icons/stage-monitor.svg";
    if (id.contains("audiomix"))    return ":/resources/icons/audio-mix.svg";
    if (id.contains("transcriberec")) return ":/resources/icons/transcribe-rec.svg";
    if (id.contains("switchcaster"))  return ":/resources/icons/switch-caster.svg";
    if (id.contains("servicerunner")) return ":/resources/icons/service-runner.svg";
    return ":/resources/icons/settings.svg";
}

// ─── Constructor ──────────────────────────────────────────────────────────────
ModuleDock::ModuleDock(M1::IModule* module, QWidget* parent)
    : ResizableWidget(parent)
    , m_module(module)
{
    setObjectName("ModuleDock");
    setMinimumSize(module->minimumModuleSize());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    setupTitleBar();
    root->addWidget(m_titleBar);

    m_moduleWidget = module->createWidget(this);
    m_moduleWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_moduleWidget, 1);

    connect(module, &M1::IModule::statusChanged, this, &ModuleDock::setStatus);
    connect(module, &M1::IModule::moduleError, this, [this](const QString& msg) {
        setStatus("\u26A0 " + msg);
    });
}

ModuleDock::~ModuleDock() = default;

// ─── Title bar ────────────────────────────────────────────────────────────────
void ModuleDock::setupTitleBar() {
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("ModuleDockTitle");
    m_titleBar->setFixedHeight(28);
    m_titleBar->setCursor(Qt::SizeAllCursor); // signals draggable
    m_titleBar->installEventFilter(this);

    auto* layout = new QHBoxLayout(m_titleBar);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(3);

    // Drag handle dots
    m_dragHandle = new QLabel(m_titleBar);
    m_dragHandle->setFixedSize(14, 24);
    {
        QSvgRenderer r(QString(":/resources/icons/drag-handle.svg"));
        if (r.isValid()) {
            QPixmap pm(28, 48);
            pm.fill(Qt::transparent);
            { QPainter p(&pm); r.render(&p); }
            pm.setDevicePixelRatio(2.0);
            m_dragHandle->setPixmap(pm);
        }
    }
    m_dragHandle->setCursor(Qt::SizeAllCursor);

    // Module type icon
    m_iconLabel = new QLabel(m_titleBar);
    m_iconLabel->setFixedSize(18, 18);
    m_iconLabel->setCursor(Qt::ArrowCursor);
    {
        const QString iconPath = iconPathForModuleId(m_module->moduleId());
        QSvgRenderer r(iconPath);
        if (r.isValid()) {
            QPixmap pm(36, 36);
            pm.fill(Qt::transparent);
            { QPainter p(&pm); r.render(&p); }
            pm.setDevicePixelRatio(2.0);
            m_iconLabel->setPixmap(pm);
        }
    }

    // Module name
    m_titleLabel = new QLabel(m_module->displayName(), m_titleBar);
    m_titleLabel->setObjectName("ModuleDockTitleLabel");
    m_titleLabel->setCursor(Qt::ArrowCursor);

    // Status (right-aligned, small)
    m_statusLabel = new QLabel(m_titleBar);
    m_statusLabel->setObjectName("ModuleDockStatus");
    m_statusLabel->setProperty("role", "status");
    m_statusLabel->setCursor(Qt::ArrowCursor);

    // Collapse button (▼ / ▶)
    m_collapseBtn = new QPushButton("▼", m_titleBar);
    m_collapseBtn->setObjectName("ModuleDockBtn");
    m_collapseBtn->setFixedSize(20, 20);
    m_collapseBtn->setFlat(true);
    m_collapseBtn->setCursor(Qt::ArrowCursor);
    m_collapseBtn->setToolTip("Collapse / Expand module");
    connect(m_collapseBtn, &QPushButton::clicked, this, &ModuleDock::toggleCollapse);

    // Pin button
    m_pinBtn = new QPushButton(m_titleBar);
    m_pinBtn->setObjectName("ModuleDockBtn");
    m_pinBtn->setIcon(svgIcon(":/resources/icons/pin.svg"));
    m_pinBtn->setIconSize({16, 16});
    m_pinBtn->setFixedSize(20, 20);
    m_pinBtn->setFlat(true);
    m_pinBtn->setCheckable(true);
    m_pinBtn->setCursor(Qt::ArrowCursor);
    m_pinBtn->setToolTip("Pin position (lock from dragging)");
    connect(m_pinBtn, &QPushButton::toggled, this, &ModuleDock::setPinned);

    // Float button
    m_floatBtn = new QPushButton(m_titleBar);
    m_floatBtn->setObjectName("ModuleDockBtn");
    m_floatBtn->setIcon(svgIcon(":/resources/icons/float.svg"));
    m_floatBtn->setIconSize({16, 16});
    m_floatBtn->setFixedSize(20, 20);
    m_floatBtn->setFlat(true);
    m_floatBtn->setCursor(Qt::ArrowCursor);
    m_floatBtn->setToolTip("Detach to floating window");
    connect(m_floatBtn, &QPushButton::clicked, this, &ModuleDock::floatModule);

    // Close button
    m_closeBtn = new QPushButton(m_titleBar);
    m_closeBtn->setObjectName("ModuleDockBtn");
    m_closeBtn->setIcon(svgIcon(":/resources/icons/close.svg"));
    m_closeBtn->setIconSize({16, 16});
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setFlat(true);
    m_closeBtn->setCursor(Qt::ArrowCursor);
    m_closeBtn->setToolTip("Close module");
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit closeRequested(m_module);
    });

    // Tray button (send to surface tray)
    m_trayBtn = new QPushButton("↓", m_titleBar);
    m_trayBtn->setObjectName("ModuleDockBtn");
    m_trayBtn->setFixedSize(20, 20);
    m_trayBtn->setFlat(true);
    m_trayBtn->setCursor(Qt::ArrowCursor);
    m_trayBtn->setToolTip("Minimize to surface tray");
    connect(m_trayBtn, &QPushButton::clicked, this, &ModuleDock::sendToTray);

    layout->addWidget(m_dragHandle);
    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_statusLabel, 1);
    layout->addWidget(m_collapseBtn);
    layout->addWidget(m_trayBtn);
    layout->addWidget(m_pinBtn);
    layout->addWidget(m_floatBtn);
    layout->addWidget(m_closeBtn);
}

// ─── Status ───────────────────────────────────────────────────────────────────
void ModuleDock::setStatus(const QString& status) {
    if (m_statusLabel) m_statusLabel->setText(status);
}

// ─── Size hint — splitter uses this for initial space allocation ──────────────
QSize ModuleDock::sizeHint() const {
    if (m_module)
        return m_module->preferredSize();
    return ResizableWidget::sizeHint();
}

// ─── Paint ────────────────────────────────────────────────────────────────────
void ModuleDock::paintEvent(QPaintEvent*) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    // Subtle border
    p.setPen(QColor(0x1e, 0x3a, 0x5f));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// Forward declaration — defined below eventFilter to keep it near the context menu.
static void showTitleBarContextMenu(ModuleDock* dock, const QPoint& globalPos);

// ─── Drag tracking via event filter ───────────────────────────────────────────
bool ModuleDock::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_titleBar)
        return QWidget::eventFilter(watched, event);

    static constexpr int kTopGrip = 6; // px from top of title bar that initiates top-edge resize

    switch (event->type()) {
    case QEvent::MouseButtonDblClick: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragTracking = false;
            endResize();
            if (m_floatingInCanvas)
                dockModule();   // floating → snap back into a column
            else
                floatModule();  // docked → detach to canvas float
            return true;
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            if (m_floatingInCanvas && !m_pinned) {
                // Top-edge of title bar → top resize (title bar sits at y=0 of dock)
                if (me->pos().y() < kTopGrip) {
                    beginResize(4 /*Top*/, me->globalPosition().toPoint());
                    return true; // consume — don't start a move
                }
                raise();
            }
            m_dragStartPos = me->pos();
            m_dragTracking = true;
        } else if (me->button() == Qt::RightButton) {
            showTitleBarContextMenu(this, me->globalPosition().toPoint());
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto* me = static_cast<QMouseEvent*>(event);
        // Top-edge resize continuation (mouse is still over title bar)
        if (m_floatingInCanvas && isResizing() && (me->buttons() & Qt::LeftButton)) {
            handleResizeMove(me->globalPosition().toPoint());
            return true;
        }
        // Update top-edge cursor when hovering (no button held)
        if (m_floatingInCanvas && !m_dragTracking) {
            m_titleBar->setCursor(me->pos().y() < kTopGrip
                                  ? Qt::SizeVerCursor : Qt::SizeAllCursor);
        }
        if (!m_dragTracking || m_pinned) break;
        if (m_floatingInCanvas) {
            // Direct canvas move — dock stays a child of m_canvas
            QPoint newPos = parentWidget()->mapFromGlobal(
                                      me->globalPosition().toPoint()) - m_dragStartPos;
            // Clamp to parent canvas bounds so dock can never escape the panel
            if (auto* canvas = parentWidget()) {
                newPos.setX(qBound(0, newPos.x(), qMax(0, canvas->width()  - width())));
                newPos.setY(qBound(0, newPos.y(), qMax(0, canvas->height() - height())));
            }
            move(newPos);
            raise();
        } else if ((me->pos() - m_dragStartPos).manhattanLength() >= 8) {
            m_dragTracking = false;
            startDrag();
        }
        break;
    }
    case QEvent::MouseButtonRelease:
        m_dragTracking = false;
        endResize(); // clears any in-progress resize (including top-edge)
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

// ─── Drag initiation ─────────────────────────────────────────────────────────
void ModuleDock::startDrag() {
    s_dragging = this;

    // Capture a scaled-down pixmap for the drag ghost
    QPixmap pm = grab();
    pm.setDevicePixelRatio(devicePixelRatio());
    QSize halfSz = pm.size() / devicePixelRatioF() / 2;
    QPixmap ghost = pm.scaled(halfSz * 2,
                               Qt::IgnoreAspectRatio,
                               Qt::SmoothTransformation);
    ghost.setDevicePixelRatio(2.0);

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData;
    mime->setData("application/x-m1-module-dock", QByteArray());
    drag->setMimeData(mime);
    drag->setPixmap(ghost);
    drag->setHotSpot(m_dragStartPos / 2);

    emit dragStarted(this);
    drag->exec(Qt::MoveAction);
    s_dragging = nullptr;
}

// ─── Pin / Unpin ──────────────────────────────────────────────────────────────
void ModuleDock::setPinned(bool pinned) {
    m_pinned = pinned;
    m_titleBar->setCursor(pinned ? Qt::ArrowCursor : Qt::SizeAllCursor);
    m_pinBtn->setChecked(pinned);
    m_pinBtn->setIcon(svgIcon(pinned
        ? ":/resources/icons/pin.svg"
        : ":/resources/icons/unpin.svg"));
    m_pinBtn->setToolTip(pinned ? "Unpin (allow dragging)" : "Pin position");
    emit pinChanged(pinned);
}

// ─── Float (detach from column, stay within canvas) ───────────────────────────
void ModuleDock::floatModule() {
    if (m_floating) return;
    m_floating = true;

    // SurfaceWidget handles all reparenting — the dock stays a canvas child
    m_floatBtn->setToolTip("Re-dock to surface");
    disconnect(m_floatBtn, &QPushButton::clicked, this, &ModuleDock::floatModule);
    connect(m_floatBtn, &QPushButton::clicked, this, &ModuleDock::dockModule);

    emit floatRequested(this);
}

// ─── Dock (snap back into a column) ───────────────────────────────────────────
void ModuleDock::dockModule() {
    if (!m_floating) return;
    m_floating = false;

    m_floatBtn->setToolTip("Detach to floating window");
    disconnect(m_floatBtn, &QPushButton::clicked, this, &ModuleDock::dockModule);
    connect(m_floatBtn, &QPushButton::clicked, this, &ModuleDock::floatModule);

    emit dockRequested(this);
}

// ─── Canvas-float flag ────────────────────────────────────────────────────────
void ModuleDock::setFloatingInCanvas(bool v) {
    m_floatingInCanvas = v;
    setResizable(v, 5);              // expose 5 px L/R/B resize borders
    m_titleBar->setMouseTracking(v); // needed for top-edge cursor updates
    if (!v) {
        m_titleBar->setCursor(Qt::SizeAllCursor); // restore drag cursor
    }
    update();
}

// ─── Send to tray ─────────────────────────────────────────────────────────────
void ModuleDock::sendToTray() {
    emit sendToTrayRequested(this);
}

// ─── Collapse / Expand ────────────────────────────────────────────────────────
void ModuleDock::toggleCollapse() {
    m_collapsed = !m_collapsed;

    if (m_moduleWidget) {
        m_moduleWidget->setVisible(!m_collapsed);
    }

    if (m_collapsed) {
        m_expandedHeight = height();
        setFixedHeight(m_titleBar->sizeHint().height());
        m_collapseBtn->setText("▶");
    } else {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
        if (m_expandedHeight > 0)
            resize(width(), m_expandedHeight);
        m_collapseBtn->setText("▼");
    }
}

// ─── Right-click context menu on title bar ─────────────────────────────────
// Called from eventFilter when right-click detected on m_titleBar
static void showTitleBarContextMenu(ModuleDock* dock, const QPoint& globalPos) {
    QMenu menu(dock);

    auto* collapseAct = menu.addAction(dock->isCollapsed() ? "Expand" : "Collapse");
    menu.addSeparator();
    auto* removeAct = menu.addAction("Remove Module");
    removeAct->setToolTip("Remove this module from the surface");

    QAction* chosen = menu.exec(globalPos);
    if (chosen == collapseAct) {
        dock->toggleCollapse();
    } else if (chosen == removeAct) {
        emit dock->removeRequested(dock);
    }
}
