#pragma once
#include <QMainWindow>

class SurfaceWidget;

/// SurfaceWindow — A standalone QMainWindow hosting a single popped-out SurfaceWidget.
///
/// Features:
///   - Fully resizable, maximizable, minimizable
///   - F11 toggles fullscreen
///   - Can be dragged to any monitor
///   - Close event docks the surface back to the main tab bar (non-destructive)
///   - Session-persisted (window geometry + monitor index + fullscreen state)
class SurfaceWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit SurfaceWindow(SurfaceWidget* surface, QWidget* parent = nullptr);
    ~SurfaceWindow() override;

    SurfaceWidget* surface() const { return m_surface; }

    /// Move this window to a specific screen by index, maximized.
    void moveToScreen(int screenIndex);

    /// Maximize on a specific screen.
    void maximizeOnScreen(int screenIndex);

    /// Get the screen index this window is currently on.
    int currentScreenIndex() const;

signals:
    /// Emitted when the user closes the pop-out window.
    /// MainWindow should re-embed the surface in the tab bar.
    void dockBackRequested(SurfaceWidget* surface);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void toggleFullscreen();

    SurfaceWidget* m_surface       = nullptr;
    bool           m_isFullscreen  = false;
};
