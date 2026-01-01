#include "SurfaceWidget.h"
#include "SubSurfacePanel.h"
#include "SubSurfaceTabBar.h"
#include "ModuleDock.h"
#include "SurfaceTray.h"
#include "SurfaceEventLog.h"
#include "SurfaceScheduler.h"
#include "SurfaceSchedulerDialog.h"
#include "IModule.h"
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QSettings>
#include <QApplication>
#include <QScreen>
#include <QDebug>

SurfaceWidget::SurfaceWidget(M1::SurfaceType type, QWidget* parent)
    : QWidget(parent)
    , m_type(type)
{
    setObjectName("SurfaceWidget");

    // ── Sub-surface tab bar ───────────────────────────────────────
    const QString defaultName = M1::surfaceTypeName(type);
    m_subTabBar = new SubSurfaceTabBar(defaultName, QColor("#0ea5e9"), this);

    // ── Stacked canvas ────────────────────────────────────────────
    m_stack = new QStackedWidget(this);

    // ── Event log + scheduler ─────────────────────────────────────
    m_eventLog  = new SurfaceEventLog(defaultName, this);
    m_scheduler = new SurfaceScheduler(this);
    m_scheduler->start();

    // ── Tray ──────────────────────────────────────────────────────
    m_tray = new SurfaceTray(this);

    // focusRequested: switch to the correct sub-surface page, then focus the dock
    connect(m_tray, &SurfaceTray::focusRequested, this, [this](ModuleDock* dock) {
        if (auto* panel = panelForDock(dock)) {
            const int idx = m_panels.indexOf(panel);
            if (idx >= 0) {
                m_subTabBar->setCurrentIndex(idx);
                m_stack->setCurrentIndex(idx);
            }
            panel->focusDock(dock);
        }
    });
    connect(m_tray, &SurfaceTray::dockRequested, this, [this](ModuleDock* dock) {
        if (auto* panel = panelForDock(dock))
            panel->redockDock(dock);
    });
    connect(m_tray, &SurfaceTray::undockRequested, this, [this](ModuleDock* dock) {
        if (auto* panel = panelForDock(dock))
            panel->floatDock(dock);
    });
    connect(m_tray, &SurfaceTray::savePositionRequested,
            this, [this](ModuleDock*) { emit saveSettingsRequested(); });
    connect(m_tray, &SurfaceTray::closeRequested, this, [this](ModuleDock* dock) {
        if (auto* panel = panelForDock(dock))
            panel->closeDock(dock);
    });

    m_tray->addLogButton(m_eventLog, this);
    m_tray->addSchedButton(m_scheduler, this);

    connect(m_scheduler, &SurfaceScheduler::eventFired,
            this, [this](const ScheduledEvent& ev) {
        logEvent("SCHED", ev.label + " → " + ev.data);
    });

    // ── Root layout ───────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_subTabBar);
    root->addWidget(m_stack, 1);
    root->addWidget(m_tray);

    // ── Sub-surface tab bar signal wiring ─────────────────────────
    connect(m_subTabBar, &SubSurfaceTabBar::currentChanged,
            this, [this](int index) {
        m_stack->setCurrentIndex(index);
    });

    connect(m_subTabBar, &SubSurfaceTabBar::subSurfaceAdded,
            this, [this](int /*index*/, const QString& name, const QColor& color) {
        addPanel(name, color);
    });

    connect(m_subTabBar, &SubSurfaceTabBar::subSurfaceRemoved,
            this, [this](int index) {
        removePanel(index);
    });

    connect(m_subTabBar, &SubSurfaceTabBar::addModuleRequested,
            this, [this](int tabIndex, const QString& id) {
        // Switch to the requested tab, then signal MainWindow to add the module
        m_subTabBar->setCurrentIndex(tabIndex);
        m_stack->setCurrentIndex(tabIndex);
        emit addModuleRequested(id);
    });

    connect(m_subTabBar, &SubSurfaceTabBar::saveSessionRequested,
            this, [this](int) { emit saveSettingsRequested(); });

    connect(m_subTabBar, &SubSurfaceTabBar::tabRenamed,
            this, [this](int index, const QString& name) {
        if (index >= 0 && index < m_panels.size())
            m_panels[index]->setName(name);
    });

    // ── Create the initial (default) panel ───────────────────────
    // SubSurfaceTabBar already added one chip; connect a panel to it
    auto* firstPanel = new SubSurfacePanel(defaultName, this);
    m_stack->addWidget(firstPanel);
    m_panels.append(firstPanel);
    connectPanel(firstPanel);
}

SurfaceWidget::~SurfaceWidget() = default;

// ─── Surface name ─────────────────────────────────────────────────────────────
QString SurfaceWidget::surfaceName() const {
    if (!m_customName.isEmpty()) return m_customName;
    return M1::surfaceTypeName(m_type);
}

// ─── Sub-surface panel management ────────────────────────────────────────────
SubSurfacePanel* SurfaceWidget::panel(int i) const {
    return (i >= 0 && i < m_panels.size()) ? m_panels[i] : nullptr;
}

QList<int> SurfaceWidget::panelIndicesWithModule(const QString& moduleId) const {
    QList<int> result;
    for (int i = 0; i < m_panels.size(); ++i) {
        for (auto* mod : m_panels[i]->modules()) {
            if (mod->moduleId() == moduleId) {
                result.append(i);
                break;
            }
        }
    }
    return result;
}

SubSurfacePanel* SurfaceWidget::currentPanel() const {
    const int idx = m_stack->currentIndex();
    return (idx >= 0 && idx < m_panels.size()) ? m_panels[idx] : nullptr;
}

SubSurfacePanel* SurfaceWidget::panelForDock(ModuleDock* dock) const {
    for (auto* panel : m_panels)
        if (panel->allDocks().contains(dock))
            return panel;
    return nullptr;
}

void SurfaceWidget::addPanel(const QString& name, const QColor& color) {
    auto* panel = new SubSurfacePanel(name, this);
    m_stack->addWidget(panel);
    m_panels.append(panel);
    connectPanel(panel);
    Q_UNUSED(color);
}

void SurfaceWidget::removePanel(int index) {
    if (index < 0 || index >= m_panels.size()) return;
    auto* panel = m_panels.takeAt(index);

    // Remove all tray buttons for docks on this panel
    for (auto* dock : panel->allDocks())
        m_tray->removeModuleButton(dock);

    m_stack->removeWidget(panel);
    panel->deleteLater();
}

void SurfaceWidget::connectPanel(SubSurfacePanel* panel) {
    // ── Dock lifecycle → tray buttons + auto-save trigger ─────────
    connect(panel, &SubSurfacePanel::dockCreated, this, [this](ModuleDock* dock) {
        m_tray->addModuleButton(dock);
        emit moduleLayoutChanged();
    });
    connect(panel, &SubSurfacePanel::dockDestroyed, this, [this](ModuleDock* dock) {
        m_tray->removeModuleButton(dock);
        emit moduleLayoutChanged();
    });
    connect(panel, &SubSurfacePanel::dockSentToTray, this, [this](ModuleDock* dock) {
        m_tray->setModuleInTray(dock, true);
    });
    connect(panel, &SubSurfacePanel::dockRestoredFromTray, this, [this](ModuleDock* dock) {
        m_tray->setModuleInTray(dock, false);
    });

    // ── Canvas right-click → bubble up to MainWindow ──────────────
    connect(panel, &SubSurfacePanel::addModuleRequested,
            this, &SurfaceWidget::addModuleRequested);
    connect(panel, &SubSurfacePanel::saveSettingsRequested,
            this, &SurfaceWidget::saveSettingsRequested);
}

// ─── Module operations ────────────────────────────────────────────────────────
void SurfaceWidget::addModule(M1::IModule* module, bool startFloating) {
    if (auto* panel = currentPanel())
        panel->addModule(module, startFloating);
}

void SurfaceWidget::removeModule(M1::IModule* module) {
    for (auto* panel : m_panels)
        panel->removeModule(module);
}

QList<M1::IModule*> SurfaceWidget::modules() const {
    QList<M1::IModule*> result;
    for (auto* panel : m_panels)
        result.append(panel->modules());
    return result;
}

// ─── Event log ────────────────────────────────────────────────────────────────
void SurfaceWidget::logEvent(const QString& category, const QString& message) {
    if (m_eventLog) m_eventLog->appendEvent(category, message);
}

// ─── Layout persistence ───────────────────────────────────────────────────────
SurfaceLayoutSnapshot SurfaceWidget::layoutSnapshot() const {
    // Return snapshot of the current (visible) panel only for now
    if (auto* p = currentPanel())
        return p->layoutSnapshot();
    return {};
}

void SurfaceWidget::applyLayoutSnapshot(const SurfaceLayoutSnapshot& snap) {
    if (auto* p = currentPanel())
        p->applyLayoutSnapshot(snap);
}

void SurfaceWidget::saveLayout(QSettings& s) const {
    const QString base = "Surface/" + surfaceName();
    s.setValue(base + "/subPanelCount", m_panels.size());
    for (int i = 0; i < m_panels.size(); ++i) {
        const QString key = base + QString("/sub%1").arg(i);
        s.setValue(key + "/name", m_panels[i]->name());
        m_panels[i]->saveLayout(s, key);
    }
}

void SurfaceWidget::restoreLayout(QSettings& s) {
    const QString base = "Surface/" + surfaceName();
    // Panel 0 always exists; restore its layout
    if (!m_panels.isEmpty())
        m_panels[0]->restoreLayout(s, base + "/sub0");

    // Restore additional panels (if any were saved)
    const int savedCount = s.value(base + "/subPanelCount", 1).toInt();
    for (int i = 1; i < savedCount; ++i) {
        const QString key  = base + QString("/sub%1").arg(i);
        const QString name = s.value(key + "/name",
                                     QString("Sub-Surface %1").arg(i + 1)).toString();
        // Add via the tab bar so chip + panel stay in sync.
        // subSurfaceAdded signal will call addPanel() automatically.
        if (i >= m_panels.size())
            m_subTabBar->addSubSurface(name, QColor("#0ea5e9"));
        m_panels[i]->restoreLayout(s, key);
    }
}

void SurfaceWidget::initSplitterSizes() {
    for (auto* panel : m_panels)
        panel->initSplitterSizes();
}
