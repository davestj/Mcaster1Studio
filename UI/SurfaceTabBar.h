#pragma once
#include <QTabWidget>
#include <QColor>
#include <QStringList>
#include "ISurface.h"

class SurfaceWidget;

/// SurfaceTabBar — horizontal strip of open surfaces.
/// Each tab is a SurfaceWidget. Tabs can be closed (minimum one always kept).
/// A "+" button in the top-right corner emits addSurfaceRequested() so
/// MainWindow can open the Surfaces menu.
class SurfaceTabBar : public QTabWidget {
    Q_OBJECT

public:
    explicit SurfaceTabBar(QWidget* parent = nullptr);

    /// Open a surface (creates a new tab or activates the existing one)
    SurfaceWidget* openSurface(M1::SurfaceType type);

    /// Open a custom-named surface (always creates a new tab)
    SurfaceWidget* openCustomSurface(const QString& name);

    /// Find an open surface by type (nullptr if not open)
    SurfaceWidget* findSurface(M1::SurfaceType type) const;

    /// Currently visible surface
    SurfaceWidget* currentSurface() const;

signals:
    /// Emitted when the user clicks the "+" corner button
    void addSurfaceRequested();

    /// Emitted when user selects a module from the tab right-click "Add Module" submenu.
    void addModuleToSurfaceRequested(SurfaceWidget* surface, const QString& moduleId);

    /// Emitted when user right-clicks a surface tab and chooses "Create new Sub Surface"
    /// (either custom-named or from a template). moduleIds may be empty for a blank sub-surface.
    void createSubSurfaceRequested(SurfaceWidget* surface,
                                   const QString& name,
                                   const QColor&  color,
                                   const QStringList& moduleIds);

    /// Emitted when user right-clicks a surface tab and chooses "Save Session".
    void saveSessionRequested(SurfaceWidget* surface);

    /// Emitted when user right-clicks and selects "Pop Out Window".
    void popOutRequested(SurfaceWidget* surface);

    /// Emitted when user right-clicks and selects "Send to Monitor" → specific screen.
    void sendToMonitorRequested(SurfaceWidget* surface, int screenIndex);

private slots:
    void onTabCloseRequested(int index);
    void onTabDoubleClicked(int index);
    void onTabBarContextMenu(const QPoint& pos);

private:
    QList<SurfaceWidget*> m_surfaces;
};
