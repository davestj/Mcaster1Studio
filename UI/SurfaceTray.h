#pragma once
#include <QWidget>
#include <QList>
#include <QHBoxLayout>
#include <QContextMenuEvent>

class ModuleDock;
class QPushButton;
class QScrollArea;
class QLabel;
class SurfaceEventLog;
class SurfaceScheduler;
namespace M1 { class IModule; }

/// SurfaceTrayModuleBtn — taskbar-style button, one per open module.
///
/// Always present while the module is loaded on the surface.
/// - Left-click  : focus / raise the module
/// - Right-click : Dock | Undock | Save Position | Close
class SurfaceTrayModuleBtn : public QWidget {
    Q_OBJECT
public:
    explicit SurfaceTrayModuleBtn(ModuleDock* dock, QWidget* parent = nullptr);
    ModuleDock* dock() const { return m_dock; }

    /// Dim the button when the module has been sent to the tray (minimized).
    void setInTray(bool inTray);
    bool isInTray() const { return m_inTray; }

signals:
    void focusRequested(ModuleDock* dock);
    void dockRequested(ModuleDock* dock);
    void undockRequested(ModuleDock* dock);
    void savePositionRequested(ModuleDock* dock);
    void closeRequested(ModuleDock* dock);

protected:
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    ModuleDock*  m_dock   = nullptr;
    QPushButton* m_btn    = nullptr;
    bool         m_inTray = false;
};

/// SurfaceTray — bottom strip on each surface.
///
/// Layout (left → right):
///   TRAY label  |  Pinned area (LOG, SCHED, …)  |  Module button bar (scrollable)
///
/// The module button bar shows one taskbar-style button for every open module,
/// regardless of whether it is docked, floating, or minimised to the tray.
/// Buttons use the module's display name as the label.
class SurfaceTray : public QWidget {
    Q_OBJECT

public:
    explicit SurfaceTray(QWidget* parent = nullptr);

    /// Register a module button (called from SurfaceWidget::addModule).
    void addModuleButton(ModuleDock* dock);

    /// Remove the module button (called when a module is closed).
    void removeModuleButton(ModuleDock* dock);

    /// Mark a button as "in tray" (dimmed) or visible (normal).
    void setModuleInTray(ModuleDock* dock, bool inTray);

    /// Legacy wrappers — delegate to setModuleInTray.
    void addMinimized(ModuleDock* dock);
    void removeMinimized(ModuleDock* dock);

    /// Add a small persistent widget to the pinned section.
    void addPinnedWidget(QWidget* w, const QString& label = QString());

    /// Add standard system buttons to the pinned section.
    void addLogButton(SurfaceEventLog* log, QWidget* parentWindow);
    void addSchedButton(SurfaceScheduler* scheduler, QWidget* parentWindow);

signals:
    void focusRequested(ModuleDock* dock);        ///< focus / raise (also restore from tray)
    void dockRequested(ModuleDock* dock);         ///< snap floating dock back into a column
    void undockRequested(ModuleDock* dock);       ///< float a docked module onto the canvas
    void savePositionRequested(ModuleDock* dock); ///< save layout now
    void closeRequested(ModuleDock* dock);        ///< remove module from surface

private:
    QHBoxLayout* m_pinnedLayout = nullptr;
    QHBoxLayout* m_moduleLayout = nullptr;  ///< taskbar module buttons
    QList<SurfaceTrayModuleBtn*> m_moduleBtns;
};
