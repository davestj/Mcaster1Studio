#include "ResizableWidget.h"
#include <QMouseEvent>
#include <QLayout>

// ─── Edge bitmask constants ───────────────────────────────────────────────────
static constexpr int kLeft   = 1;
static constexpr int kRight  = 2;
static constexpr int kTop    = 4;
static constexpr int kBottom = 8;

// ─── Cursor for a given edge bitmask ─────────────────────────────────────────
static Qt::CursorShape cursorForEdges(int e) {
    if ((e & kLeft)  && (e & kTop))    return Qt::SizeFDiagCursor;
    if ((e & kRight) && (e & kBottom)) return Qt::SizeFDiagCursor;
    if ((e & kRight) && (e & kTop))    return Qt::SizeBDiagCursor;
    if ((e & kLeft)  && (e & kBottom)) return Qt::SizeBDiagCursor;
    if (e & (kLeft | kRight))          return Qt::SizeHorCursor;
    if (e & (kTop  | kBottom))         return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

// ─── Constructor ──────────────────────────────────────────────────────────────
ResizableWidget::ResizableWidget(QWidget* parent)
    : QWidget(parent)
{}

// ─── setResizable ─────────────────────────────────────────────────────────────
void ResizableWidget::setResizable(bool on, int borderWidth) {
    m_resizable  = on;
    m_borderWidth = borderWidth;
    setMouseTracking(on);

    if (layout()) {
        // Expose L/R/B borders; keep top=0 so the subclass title bar sits flush.
        if (on)
            layout()->setContentsMargins(borderWidth, 0, borderWidth, borderWidth);
        else
            layout()->setContentsMargins(0, 0, 0, 0);
    }

    if (!on) {
        m_resizeEdges = 0;
        unsetCursor();
    }
}

// ─── beginResize / handleResizeMove ───────────────────────────────────────────
void ResizableWidget::beginResize(int edgeMask, const QPoint& globalStartPos) {
    m_resizeEdges    = edgeMask;
    m_resizeStart    = globalStartPos;
    m_resizeOrigGeom = geometry();
}

void ResizableWidget::handleResizeMove(const QPoint& globalPos) {
    if (!m_resizeEdges) return;

    const QPoint d = globalPos - m_resizeStart;
    QRect g = m_resizeOrigGeom;
    const int mw = minimumWidth();
    const int mh = minimumHeight();

    if (m_resizeEdges & kLeft)   g.setLeft  (qMin(g.right()  - mw, g.left()   + d.x()));
    if (m_resizeEdges & kRight)  g.setRight (qMax(g.left()   + mw, g.right()  + d.x()));
    if (m_resizeEdges & kTop)    g.setTop   (qMin(g.bottom() - mh, g.top()    + d.y()));
    if (m_resizeEdges & kBottom) g.setBottom(qMax(g.top()    + mh, g.bottom() + d.y()));

    // Clamp to parent canvas bounds — prevent escaping above or left
    if (parentWidget()) {
        if (g.left() < 0) g.moveLeft(0);
        if (g.top()  < 0) g.moveTop(0);
        const int pw = parentWidget()->width();
        const int ph = parentWidget()->height();
        if (g.right()  > pw) g.setRight(pw);
        if (g.bottom() > ph) g.setBottom(ph);
    }

    setGeometry(g);
}

// ─── edgeHitTest ─────────────────────────────────────────────────────────────
int ResizableWidget::edgeHitTest(const QPoint& pos) const {
    const QRect r = rect();
    const int   g = m_borderWidth;
    int e = 0;
    if (pos.x() < g)            e |= kLeft;
    if (pos.x() > r.width() - g) e |= kRight;
    // Bottom only — top is handled via the subclass title bar
    if (pos.y() > r.height() - g) e |= kBottom;
    return e;
}

// ─── Mouse events ─────────────────────────────────────────────────────────────
void ResizableWidget::mousePressEvent(QMouseEvent* e) {
    if (!m_resizable || e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    const int edges = edgeHitTest(e->pos());
    if (edges) {
        beginResize(edges, e->globalPosition().toPoint());
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void ResizableWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!m_resizable) { QWidget::mouseMoveEvent(e); return; }

    if (m_resizeEdges && (e->buttons() & Qt::LeftButton)) {
        handleResizeMove(e->globalPosition().toPoint());
    } else {
        const int edges = edgeHitTest(e->pos());
        setCursor(edges ? cursorForEdges(edges) : Qt::ArrowCursor);
    }
    QWidget::mouseMoveEvent(e);
}

void ResizableWidget::mouseReleaseEvent(QMouseEvent* e) {
    endResize();
    QWidget::mouseReleaseEvent(e);
}
