#include "SurfaceTray.h"
#include "ModuleDock.h"
#include "SurfaceEventLog.h"
#include "SurfaceScheduler.h"
#include "SurfaceSchedulerDialog.h"
#include "IModule.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QMenu>
#include <QStyle>

// ─── SurfaceTrayModuleBtn ─────────────────────────────────────────────────────
SurfaceTrayModuleBtn::SurfaceTrayModuleBtn(ModuleDock* dock, QWidget* parent)
    : QWidget(parent)
    , m_dock(dock)
{
    setObjectName("SurfaceTrayModuleBtn");
    setMinimumHeight(30);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    const QString name = dock->module() ? dock->module()->displayName() : "Module";

    m_btn = new QPushButton(name, this);
    m_btn->setObjectName("TrayModuleBtn");
    m_btn->setToolTip(name);
    m_btn->setCursor(Qt::PointingHandCursor);
    m_btn->setFixedHeight(26);
    m_btn->setMinimumWidth(70);
    m_btn->setMaximumWidth(160);
    m_btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(m_btn, &QPushButton::clicked, this, [this]() {
        emit focusRequested(m_dock);
    });

    layout->addWidget(m_btn);
}

void SurfaceTrayModuleBtn::setInTray(bool inTray) {
    m_inTray = inTray;
    // Dynamic QSS property — re-polish so [inTray="true"] rules apply
    m_btn->setProperty("inTray", inTray);
    m_btn->style()->unpolish(m_btn);
    m_btn->style()->polish(m_btn);
}

void SurfaceTrayModuleBtn::contextMenuEvent(QContextMenuEvent* e) {
    QMenu menu(this);

    auto* dockAct   = menu.addAction("Dock");
    auto* undockAct = menu.addAction("Undock");
    menu.addSeparator();
    menu.addAction("Save Position", this, [this]() {
        emit savePositionRequested(m_dock);
    });
    menu.addSeparator();
    menu.addAction("Close", this, [this]() {
        emit closeRequested(m_dock);
    });

    // Contextual enable/disable
    const bool floating = m_dock->isFloatingInCanvas();
    dockAct->setEnabled(floating || m_inTray);   // can dock if floating or in tray
    undockAct->setEnabled(!floating && !m_inTray); // can undock if currently in a column

    connect(dockAct,   &QAction::triggered, this, [this]() { emit dockRequested(m_dock); });
    connect(undockAct, &QAction::triggered, this, [this]() { emit undockRequested(m_dock); });

    menu.exec(e->globalPos());
}

// ─── SurfaceTray ─────────────────────────────────────────────────────────────
SurfaceTray::SurfaceTray(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("SurfaceTray");
    setFixedHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(6, 4, 6, 4);
    root->setSpacing(0);

    // ── TRAY label ────────────────────────────────────────────────
    auto* trayLabel = new QLabel("TRAY", this);
    trayLabel->setObjectName("TrayLabel");
    trayLabel->setFixedWidth(36);

    // ── Pinned area (left) ────────────────────────────────────────
    auto* pinnedContainer = new QWidget(this);
    pinnedContainer->setObjectName("TrayPinnedArea");
    m_pinnedLayout = new QHBoxLayout(pinnedContainer);
    m_pinnedLayout->setContentsMargins(0, 0, 0, 0);
    m_pinnedLayout->setSpacing(6);

    // ── Separator ─────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setObjectName("TraySeparator");
    sep->setFixedWidth(2);

    // ── Module button bar (right, scrollable) ─────────────────────
    auto* moduleContainer = new QWidget(this);
    moduleContainer->setObjectName("TrayModuleArea");
    m_moduleLayout = new QHBoxLayout(moduleContainer);
    m_moduleLayout->setContentsMargins(4, 0, 4, 0);
    m_moduleLayout->setSpacing(4);
    m_moduleLayout->addStretch(1);  // pushes buttons left

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(moduleContainer);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFixedHeight(36);

    root->addWidget(trayLabel);
    root->addWidget(pinnedContainer, 0);
    root->addWidget(sep);
    root->addWidget(scroll, 1);
}

// ─── Module button management ─────────────────────────────────────────────────
void SurfaceTray::addModuleButton(ModuleDock* dock) {
    // Guard against duplicates
    for (auto* b : m_moduleBtns)
        if (b->dock() == dock) return;

    auto* btn = new SurfaceTrayModuleBtn(dock, this);
    connect(btn, &SurfaceTrayModuleBtn::focusRequested,
            this, &SurfaceTray::focusRequested);
    connect(btn, &SurfaceTrayModuleBtn::dockRequested,
            this, &SurfaceTray::dockRequested);
    connect(btn, &SurfaceTrayModuleBtn::undockRequested,
            this, &SurfaceTray::undockRequested);
    connect(btn, &SurfaceTrayModuleBtn::savePositionRequested,
            this, &SurfaceTray::savePositionRequested);
    connect(btn, &SurfaceTrayModuleBtn::closeRequested,
            this, &SurfaceTray::closeRequested);

    // Insert before the trailing stretch
    const int insertPos = qMax(0, m_moduleLayout->count() - 1);
    m_moduleLayout->insertWidget(insertPos, btn);
    m_moduleBtns.append(btn);
}

void SurfaceTray::removeModuleButton(ModuleDock* dock) {
    for (int i = 0; i < m_moduleBtns.size(); ++i) {
        if (m_moduleBtns[i]->dock() == dock) {
            m_moduleBtns[i]->setParent(nullptr);
            m_moduleBtns[i]->deleteLater();
            m_moduleBtns.removeAt(i);
            return;
        }
    }
}

void SurfaceTray::setModuleInTray(ModuleDock* dock, bool inTray) {
    for (auto* btn : m_moduleBtns)
        if (btn->dock() == dock) { btn->setInTray(inTray); return; }
}

// ─── Legacy wrappers ──────────────────────────────────────────────────────────
void SurfaceTray::addMinimized(ModuleDock* dock) {
    setModuleInTray(dock, true);
}

void SurfaceTray::removeMinimized(ModuleDock* dock) {
    setModuleInTray(dock, false);
}

// ─── Pinned system buttons ────────────────────────────────────────────────────
void SurfaceTray::addLogButton(SurfaceEventLog* log, QWidget* parentWindow) {
    auto* btn = new QPushButton("LOG", this);
    btn->setObjectName("TraySystemBtn");
    btn->setToolTip("View surface event log");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(24);

    connect(log, &SurfaceEventLog::eventAppended, btn, [btn, log]() {
        const int unread = log->unreadCount();
        btn->setText(unread > 0 ? QString("LOG [%1]").arg(unread) : "LOG");
    });
    connect(btn, &QPushButton::clicked, log, [log, parentWindow]() {
        log->showDialog(parentWindow);
    });

    m_pinnedLayout->addWidget(btn);
}

void SurfaceTray::addSchedButton(SurfaceScheduler* scheduler, QWidget* parentWindow) {
    auto* btn = new QPushButton("SCHED", this);
    btn->setObjectName("TraySystemBtn");
    btn->setToolTip("Open surface automation scheduler");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(24);

    connect(btn, &QPushButton::clicked, [scheduler, parentWindow]() {
        auto* dlg = new SurfaceSchedulerDialog(scheduler, "Surface", parentWindow);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });

    m_pinnedLayout->addWidget(btn);
}

void SurfaceTray::addPinnedWidget(QWidget* w, const QString& label) {
    if (!label.isEmpty()) {
        auto* lbl = new QLabel(label, this);
        lbl->setObjectName("TrayPinnedLabel");
        m_pinnedLayout->addWidget(lbl);
    }
    m_pinnedLayout->addWidget(w);
}
