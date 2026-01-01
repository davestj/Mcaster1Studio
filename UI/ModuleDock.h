#pragma once
#include "ResizableWidget.h"
#include <QLabel>
#include <QPoint>
#include <QIcon>

namespace M1 { class IModule; }
class QPushButton;

/// ModuleDock — the frame that wraps any IModule widget inside a surface canvas.
///
/// Features:
///   - Drag handle in title bar: grab and drag to reorder within the surface
///   - Pin button: lock position so it can't be accidentally moved
///   - Float button: detach to a free-floating overlay within the canvas (stays a canvas child)
///   - Close button: remove module from surface
///   - Module type icon in title bar (from moduleId)
///   - Status text display from module signals
///
/// Drag/drop protocol:
///   MIME type: "application/x-m1-module-dock"
///   Active dragger: ModuleDock::draggingDock() (static, set during QDrag::exec)
///
/// Floating-in-canvas:
///   When floated, the dock is reparented to the canvas but removed from the QSplitter.
///   Title-bar drag then directly calls move() on the dock (no QDrag) so it stays inside
///   the surface and never becomes an OS window.
class ModuleDock : public ResizableWidget {
    Q_OBJECT

public:
    explicit ModuleDock(M1::IModule* module, QWidget* parent = nullptr);
    ~ModuleDock() override;

    M1::IModule* module()           const { return m_module; }
    bool         isPinned()         const { return m_pinned; }
    bool         isFloating()       const { return m_floating; }
    bool         isFloatingInCanvas()const { return m_floatingInCanvas; }

    void setStatus(const QString& status);
    void setFloatingInCanvas(bool v);

    /// The dock currently being dragged via QDrag (non-null during drag operation).
    static ModuleDock* draggingDock() { return s_dragging; }

    /// Load an SVG resource as a QIcon at the given pixel size.
    static QIcon svgIcon(const QString& resourcePath, QSize size = {16, 16});

    /// Map a module ID to its icon resource path.
    static QString iconPathForModuleId(const QString& moduleId);

signals:
    void closeRequested(M1::IModule* module);
    void removeRequested(ModuleDock* dock);
    void sendToTrayRequested(ModuleDock* dock);  ///< Minimize to surface tray
    void dragStarted(ModuleDock* dock);
    void floatRequested(ModuleDock* dock);
    void dockRequested(ModuleDock* dock);
    void pinChanged(bool pinned);

public slots:
    void setPinned(bool pinned);
    void floatModule();
    void dockModule();
    void toggleCollapse();
    bool isCollapsed() const { return m_collapsed; }
    void sendToTray();           ///< Called to minimize this dock to the surface tray

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void startDrag();
    void setupTitleBar();

    static ModuleDock* s_dragging;

    M1::IModule* m_module       = nullptr;
    QWidget*     m_titleBar     = nullptr;
    QWidget*     m_moduleWidget = nullptr;

    QLabel*      m_dragHandle   = nullptr;
    QLabel*      m_iconLabel    = nullptr;
    QLabel*      m_titleLabel   = nullptr;
    QLabel*      m_statusLabel  = nullptr;
    QPushButton* m_collapseBtn  = nullptr;
    QPushButton* m_trayBtn      = nullptr;  ///< Send to tray (minimize)
    QPushButton* m_pinBtn       = nullptr;
    QPushButton* m_floatBtn     = nullptr;
    QPushButton* m_closeBtn     = nullptr;

    // Float/pin state
    bool         m_pinned           = false;
    bool         m_floating         = false;  ///< floating within canvas (not in any column)
    bool         m_floatingInCanvas = false;  ///< true when free-positioned as canvas child

    // Drag tracking
    bool         m_dragTracking = false;
    QPoint       m_dragStartPos;

    // Collapse state
    bool         m_collapsed   = false;
    int          m_expandedHeight = 0;
};
