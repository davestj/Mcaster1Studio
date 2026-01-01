#pragma once
#include <QWidget>
#include <QList>
#include "SurfaceConfigYaml.h"

class ModuleDock;
class QScrollArea;
class QSplitter;
class QFrame;
class QSettings;
namespace M1 { class IModule; }

/// SubSurfacePanel — module canvas for one sub-surface slot.
///
/// Owns the scroll area, horizontal column splitter, column QSplitters, and all
/// ModuleDock widgets (docked or floating within the canvas).  Multiple panels
/// can coexist inside a SurfaceWidget, one per sub-surface tab, stacked in a
/// QStackedWidget.
///
/// Dock lifecycle signals (dockCreated / dockDestroyed / dockSentToTray /
/// dockRestoredFromTray) are consumed by SurfaceWidget to drive SurfaceTray
/// buttons — the panel itself has no dependency on SurfaceTray.
class SubSurfacePanel : public QWidget {
    Q_OBJECT

public:
    explicit SubSurfacePanel(const QString& name, QWidget* parent = nullptr);
    ~SubSurfacePanel() override;

    QString name() const      { return m_name; }
    void    setName(const QString& n) { m_name = n; }

    void                addModule(M1::IModule* module, bool startFloating = false);
    void                removeModule(M1::IModule* module);
    QList<M1::IModule*> modules()  const;
    QList<ModuleDock*>  allDocks() const;  ///< all docks (column + floating)

    SurfaceLayoutSnapshot layoutSnapshot() const;
    void                  applyLayoutSnapshot(const SurfaceLayoutSnapshot& snap);

    void saveLayout(QSettings& s, const QString& keyBase) const;
    void restoreLayout(QSettings& s, const QString& keyBase);
    void initSplitterSizes();

signals:
    // ── Canvas context-menu ───────────────────────────────────────
    void addModuleRequested(const QString& moduleId);
    void saveSettingsRequested();

    // ── Dock lifecycle — consumed by SurfaceWidget → SurfaceTray ──
    void dockCreated(ModuleDock* dock);
    void dockDestroyed(ModuleDock* dock);
    void dockSentToTray(ModuleDock* dock);
    void dockRestoredFromTray(ModuleDock* dock);

public slots:
    // ── Tray command slots — called by SurfaceWidget ──────────────
    void focusDock(ModuleDock* dock);   ///< restore from tray OR raise
    void floatDock(ModuleDock* dock);   ///< detach to canvas float
    void redockDock(ModuleDock* dock);  ///< snap float back into column
    void closeDock(ModuleDock* dock);   ///< remove module entirely

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onModuleCloseRequested(M1::IModule* module);
    void onModuleRemoveRequested(ModuleDock* dock);
    void onModuleFloatRequested(ModuleDock* dock);
    void onModuleDockRequested(ModuleDock* dock);
    void sendDockToTray(ModuleDock* dock);
    void restoreFromTray(ModuleDock* dock);

private:
    // ── Column management ────────────────────────────────────────
    QSplitter* makeColumn();
    QSplitter* columnForDock(ModuleDock* dock) const;
    void       pruneEmptyColumn(QSplitter* col);
    void       syncDocksFromColumns();

    // ── Drag/drop helpers ─────────────────────────────────────────
    bool handleDragEnter(QDragEnterEvent* e);
    bool handleDragMove(QDragMoveEvent* e);
    bool handleDrop(QDropEvent* e);
    void handleDragLeave();

    struct DropPos {
        bool       stack;
        int        colIdx;
        QSplitter* col;
        int        rowIdx;
    };
    DropPos calcDropPos(const QPoint& canvasPos) const;
    void    updateDropIndicators(const DropPos& dp);

    void showCanvasContextMenu(const QPoint& globalPos);

    // ── State ─────────────────────────────────────────────────────
    QString              m_name;
    QList<ModuleDock*>   m_docks;
    QList<ModuleDock*>   m_floatingDocks;
    QList<QSplitter*>    m_columns;

    QScrollArea* m_scroll         = nullptr;
    QWidget*     m_canvas         = nullptr;
    QSplitter*   m_splitter       = nullptr;

    QFrame*      m_dropIndicator  = nullptr;
    QFrame*      m_hDropIndicator = nullptr;
    int          m_dropIndex      = -1;
};
