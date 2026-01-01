#pragma once
#include <QWidget>
#include <QPoint>
#include <QRect>

/// ResizableWidget — QWidget base class with 8-direction border resize.
///
/// Call setResizable(true) to enter resize mode. The widget exposes a border
/// strip of `borderWidth` pixels on Left, Right and Bottom by adjusting the
/// root layout's content margins (top margin stays 0 so the subclass can put
/// a title bar flush against the top edge).
///
/// Top-edge resize must be initiated by the subclass: detect the top edge inside
/// the title bar's event filter, then call beginResize(4, globalPos).  Subsequent
/// mouse-move events from that child widget should call handleResizeMove(); call
/// endResize() on mouse release.
///
/// Usage (typical ModuleDock pattern):
///   void setFloatingInCanvas(bool v) {
///       setResizable(v, 5);   // exposes 5 px borders L/R/B
///   }
class ResizableWidget : public QWidget {
    Q_OBJECT

public:
    explicit ResizableWidget(QWidget* parent = nullptr);

    /// Enable/disable resize mode and set the grip width in pixels.
    void setResizable(bool on, int borderWidth = 5);

    bool isResizable() const { return m_resizable; }
    bool isResizing()  const { return m_resizeEdges != 0; }

protected:
    /// Begin a resize operation (call from subclass event filter for top-edge etc.).
    void beginResize(int edgeMask, const QPoint& globalStartPos);

    /// Apply geometry update for the current resize operation.
    void handleResizeMove(const QPoint& globalPos);

    /// Finish the current resize operation.
    void endResize() { m_resizeEdges = 0; }

    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

private:
    /// Returns a bitmask of edges hit by localPos: 1=L, 2=R, 4=T, 8=B.
    int edgeHitTest(const QPoint& localPos) const;

    bool   m_resizable      = false;
    int    m_borderWidth    = 5;
    int    m_resizeEdges    = 0;
    QPoint m_resizeStart;
    QRect  m_resizeOrigGeom;
};
