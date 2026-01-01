#include "SurfaceTabBar.h"
#include "SurfaceWidget.h"
#include "ModuleRegistry.h"
#include "MonitorManager.h"
#include <QToolButton>
#include <QTabBar>
#include <QInputDialog>
#include <QMenu>
#include <QAction>
#include <QColorDialog>
#include <QGuiApplication>
#include <QScreen>

SurfaceTabBar::SurfaceTabBar(QWidget* parent)
    : QTabWidget(parent)
{
    setDocumentMode(true);
    setMovable(true);
    setTabsClosable(true);

    // "+" button in the top-right corner — opens the Surfaces menu
    auto* addBtn = new QToolButton(this);
    addBtn->setText("+");
    addBtn->setToolTip("Open a surface");
    addBtn->setObjectName("SurfaceAddBtn");
    addBtn->setStyleSheet(
        "QToolButton{"
        "  background:#0c1a2e; color:#38bdf8;"
        "  border:1px solid #1e3a5f; border-radius:4px;"
        "  font-size:16px; font-weight:900; padding:0 8px;"
        "  min-width:28px; min-height:26px;"
        "}"
        "QToolButton:hover{ background:#1e3a5f; }");
    connect(addBtn, &QToolButton::clicked, this, &SurfaceTabBar::addSurfaceRequested);
    setCornerWidget(addBtn, Qt::TopRightCorner);

    connect(this, &QTabWidget::tabCloseRequested, this, &SurfaceTabBar::onTabCloseRequested);

    // Double-click tab label → inline rename
    connect(tabBar(), &QTabBar::tabBarDoubleClicked,
            this, &SurfaceTabBar::onTabDoubleClicked);

    // Right-click tab → context menu (Rename / Close)
    tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tabBar(), &QTabBar::customContextMenuRequested,
            this, &SurfaceTabBar::onTabBarContextMenu);
}

SurfaceWidget* SurfaceTabBar::openSurface(M1::SurfaceType type) {
    // If already open, just switch to it
    if (auto* existing = findSurface(type)) {
        setCurrentWidget(existing);
        return existing;
    }

    auto* surface = new SurfaceWidget(type, this);
    m_surfaces.append(surface);
    const int idx = addTab(surface, surface->surfaceName());
    setCurrentIndex(idx);
    return surface;
}

SurfaceWidget* SurfaceTabBar::findSurface(M1::SurfaceType type) const {
    for (auto* s : m_surfaces) {
        if (s->surfaceType() == type) return s;
    }
    return nullptr;
}

SurfaceWidget* SurfaceTabBar::currentSurface() const {
    return qobject_cast<SurfaceWidget*>(currentWidget());
}

SurfaceWidget* SurfaceTabBar::openCustomSurface(const QString& name) {
    auto* surface = new SurfaceWidget(M1::SurfaceType::Custom, this);
    surface->setCustomName(name);
    m_surfaces.append(surface);
    const int idx = addTab(surface, name);
    setCurrentIndex(idx);
    return surface;
}

void SurfaceTabBar::onTabCloseRequested(int index) {
    // Always keep at least one surface open
    if (count() <= 1) return;

    auto* surface = qobject_cast<SurfaceWidget*>(widget(index));
    if (surface) {
        m_surfaces.removeOne(surface);
        removeTab(index);
        surface->deleteLater();
    }
}

void SurfaceTabBar::onTabDoubleClicked(int index) {
    if (index < 0) return;
    bool ok;
    const QString current = tabText(index);
    const QString newName = QInputDialog::getText(
        this, "Rename Surface", "New name:",
        QLineEdit::Normal, current, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    const QString trimmed = newName.trimmed();
    setTabText(index, trimmed);
    if (auto* sw = qobject_cast<SurfaceWidget*>(widget(index)))
        sw->setCustomName(trimmed);
}

void SurfaceTabBar::onTabBarContextMenu(const QPoint& pos) {
    const int idx = tabBar()->tabAt(pos);
    if (idx < 0) return;

    auto* sw = qobject_cast<SurfaceWidget*>(widget(idx));

    QMenu menu(this);

    // ── Add Module submenu (built-in + discovered plugins) ────────
    const auto kModules = M1::availableModules();

    QMenu* addMenu = menu.addMenu("Add Module");
    for (const auto& [id, name] : kModules) {
        addMenu->addAction(name, this, [this, sw, id = id]() {
            if (sw) emit addModuleToSurfaceRequested(sw, id);
        });
    }

    // ── Create new Sub Surface submenu ────────────────────────────
    menu.addSeparator();

    struct SubTemplate {
        QString     name;
        QColor      color;
        QStringList moduleIds;
    };
    static const QList<SubTemplate> kTemplates = {
        { "Broadcast",
          QColor("#0ea5e9"),
          {"com.mcaster1.deck.a","com.mcaster1.deck.b",
           "com.mcaster1.encoder","com.mcaster1.vumeter"} },
        { "DJ Mix",
          QColor("#7c3aed"),
          {"com.mcaster1.deck.a","com.mcaster1.deck.b","com.mcaster1.library"} },
        { "Podcast",
          QColor("#22c55e"),
          {"com.mcaster1.ptt","com.mcaster1.podcast",
           "com.mcaster1.encoder","com.mcaster1.metadata"} },
        { "Talk Radio",
          QColor("#f59e0b"),
          {"com.mcaster1.ptt","com.mcaster1.encoder",
           "com.mcaster1.metadata","com.mcaster1.vumeter"} },
        { "Church AV",
          QColor("#ef4444"),
          {"com.mcaster1.ptt","com.mcaster1.podcast",
           "com.mcaster1.encoder","com.mcaster1.video","com.mcaster1.library"} },
        { "Social Stream",
          QColor("#38bdf8"),
          {"com.mcaster1.encoder","com.mcaster1.metadata",
           "com.mcaster1.monitor","com.mcaster1.video"} },
        { "Monitor Wall",
          QColor("#a855f7"),
          {"com.mcaster1.monitor","com.mcaster1.vumeter"} },
    };

    QMenu* newSubMenu = menu.addMenu("Create new Sub Surface");

    // Custom (blank) option
    newSubMenu->addAction("Custom (blank)...", this, [this, sw]() {
        if (!sw) return;
        bool ok = false;
        QString name = QInputDialog::getText(this, "New Sub Surface",
            "Sub surface name:", QLineEdit::Normal, "New Sub Surface", &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const QColor color = QColorDialog::getColor(QColor("#0ea5e9"), this, "Sub Surface Color");
        emit createSubSurfaceRequested(sw, name.trimmed(),
                                       color.isValid() ? color : QColor("#0ea5e9"),
                                       {});
    });

    newSubMenu->addSeparator();
    newSubMenu->addSection("From template");

    for (const auto& tmpl : kTemplates) {
        const QString label = tmpl.name;
        newSubMenu->addAction(label, this, [this, sw, tmpl]() {
            if (!sw) return;
            bool ok = false;
            QString name = QInputDialog::getText(this, "New Sub Surface",
                "Sub surface name:", QLineEdit::Normal, tmpl.name, &ok);
            if (!ok || name.trimmed().isEmpty()) return;
            emit createSubSurfaceRequested(sw, name.trimmed(), tmpl.color, tmpl.moduleIds);
        });
    }

    // ── Pop Out / Send to Monitor ──────────────────────────────────────
    menu.addSeparator();

    auto* popOutAct = menu.addAction("Pop Out Window");
    popOutAct->setToolTip("Open this surface in a standalone resizable window");

    // Build "Send to Monitor" submenu from live screen list
    QMenu* monitorMenu = menu.addMenu("Send to Monitor");
    const auto screens = QGuiApplication::screens();
    for (int si = 0; si < screens.size(); ++si) {
        const QScreen* scr = screens[si];
        const QString label = QString("%1: %2 (%3x%4)")
            .arg(si + 1)
            .arg(scr->name())
            .arg(scr->size().width())
            .arg(scr->size().height());
        monitorMenu->addAction(label, this, [this, sw, si]() {
            if (sw) emit sendToMonitorRequested(sw, si);
        });
    }
    if (screens.isEmpty())
        monitorMenu->setEnabled(false);

    // ── Save / Rename / Close ───────────────────────────────────────
    menu.addSeparator();

    auto* saveAct   = menu.addAction("Save Session");
    saveAct->setToolTip("Save all sub-tabs, modules, and layout for this surface");
    auto* renameAct = menu.addAction("Rename...");
    menu.addSeparator();
    auto* closeAct = menu.addAction("Close Surface");
    closeAct->setEnabled(count() > 1);

    QAction* chosen = menu.exec(tabBar()->mapToGlobal(pos));
    if (chosen == popOutAct)
        emit popOutRequested(sw);
    else if (chosen == saveAct)
        emit saveSessionRequested(sw);
    else if (chosen == renameAct)
        onTabDoubleClicked(idx);
    else if (chosen == closeAct)
        onTabCloseRequested(idx);
}
