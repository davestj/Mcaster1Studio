#pragma once
#include <QWidget>
#include <QList>
#include <QStringList>
#include "ISurface.h"
#include "SurfaceConfigYaml.h"

class ModuleDock;
class SubSurfacePanel;
class SubSurfaceTabBar;
class SurfaceTray;
class SurfaceRibbon;   // forward-declare only — ribbon() returns nullptr
class SurfaceEventLog;
class SurfaceScheduler;
class QStackedWidget;
class QSettings;
namespace M1 { class IModule; }

/// SurfaceWidget — top-level canvas for one surface (Alpha, DJ, Podcast, etc.).
///
/// Contains a SubSurfaceTabBar below the ribbon that lets users create multiple
/// named sub-surfaces (tabs), each with its own module canvas (SubSurfacePanel).
///
/// Module operations (addModule / removeModule / modules) delegate to the
/// currently visible SubSurfacePanel.  The SurfaceTray shows buttons for all
/// modules across ALL sub-surfaces.
class SurfaceWidget : public QWidget {
    Q_OBJECT

public:
    explicit SurfaceWidget(M1::SurfaceType type, QWidget* parent = nullptr);
    ~SurfaceWidget() override;

    M1::SurfaceType surfaceType() const { return m_type; }
    QString         surfaceName() const;

    // ── Module operations — delegate to current sub-surface ────────
    void addModule(M1::IModule* module, bool startFloating = false);
    void removeModule(M1::IModule* module);
    QList<M1::IModule*> modules() const;  ///< from ALL sub-surfaces

    // ── Sub-panel accessors ────────────────────────────────────────
    int              panelCount() const { return m_panels.size(); }
    SubSurfacePanel* panel(int i) const;

    /// Return indices of panels that contain a module with the given moduleId.
    QList<int> panelIndicesWithModule(const QString& moduleId) const;

    // ── Layout persistence ─────────────────────────────────────────
    SurfaceLayoutSnapshot layoutSnapshot() const;
    void applyLayoutSnapshot(const SurfaceLayoutSnapshot& snap);

    void saveLayout(QSettings& s) const;
    void restoreLayout(QSettings& s);
    void initSplitterSizes();

    /// Always null — ribbon removed; use subTabBar() for surface header needs.
    SurfaceRibbon*    ribbon()    const { return nullptr; }
    SubSurfaceTabBar* subTabBar() const { return m_subTabBar; }
    SurfaceTray*      tray()      const { return m_tray;      }
    SurfaceEventLog*  eventLog()  const { return m_eventLog;  }
    SurfaceScheduler* scheduler() const { return m_scheduler; }

    /// Append a timestamped event to this surface's event log.
    void logEvent(const QString& category, const QString& message);

    QString customName() const { return m_customName; }
    void    setCustomName(const QString& name) { m_customName = name; }

signals:
    void addModuleRequested(const QString& moduleId);
    void saveSettingsRequested();
    /// Emitted when any dock is created or destroyed on any sub-panel.
    /// MainWindow connects this to scheduleAutoSave().
    void moduleLayoutChanged();

private:
    // ── Sub-surface management ─────────────────────────────────────
    SubSurfacePanel* currentPanel() const;
    SubSurfacePanel* panelForDock(ModuleDock* dock) const;
    void addPanel(const QString& name, const QColor& color);
    void removePanel(int index);

    // ── Tray button routing ────────────────────────────────────────
    void connectPanel(SubSurfacePanel* panel);

    M1::SurfaceType    m_type;
    QString            m_customName;

    SubSurfaceTabBar*  m_subTabBar  = nullptr;
    QStackedWidget*    m_stack      = nullptr;
    SurfaceTray*       m_tray       = nullptr;
    SurfaceEventLog*   m_eventLog   = nullptr;
    SurfaceScheduler*  m_scheduler  = nullptr;

    QList<SubSurfacePanel*> m_panels;
};
