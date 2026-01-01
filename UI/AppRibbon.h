#pragma once
#include <QWidget>
#include <QList>
#include <QHBoxLayout>
#include <QPushButton>

class QFrame;

/// RibbonBox — draggable container wrapping a compact widget in the AppRibbon.
///
/// Layout: [grip] [content widget] [× close]
/// Height: 44px (fixed).  Width: max(80, content->sizeHint().width()) + 22px overhead.
/// Drag MIME: "application/x-m1-ribbon-box" carrying the RibbonBox pointer as hex.
class RibbonBox : public QWidget {
    Q_OBJECT

public:
    explicit RibbonBox(QWidget* content, const QString& id, QWidget* parent = nullptr);

    QString  boxId()    const { return m_id; }
    QWidget* content()  const { return m_content; }

signals:
    void closeRequested(RibbonBox* box);

protected:
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    QString      m_id;
    QWidget*     m_content    = nullptr;
    QPushButton* m_closeBtn   = nullptr;
    QWidget*     m_grip       = nullptr;
    QPoint       m_dragStart;
    bool         m_dragging   = false;
};

// ─── AppRibbon ───────────────────────────────────────────────────────────────

/// AppRibbon — window-level 48px bar above the surface tab bar.
///
/// Left:   ◉ MCASTER1 logo label (fixed)
/// Center: drag-drop zone with RibbonBox widgets (scrollable, reorderable)
/// Right:  [+ Add] button  [● ON AIR] toggle
///
/// Drag protocol:
///   Source: RibbonBox initiates QDrag with MIME "application/x-m1-ribbon-box"
///           containing the box pointer as a hex string.
///   Target: AppRibbon drop zone accepts the drag, repositions, calls rebuildOrder().
class AppRibbon : public QWidget {
    Q_OBJECT

public:
    explicit AppRibbon(QWidget* parent = nullptr);

    /// Wrap content in a RibbonBox and add to ribbon. Returns the new box.
    RibbonBox* addBox(QWidget* content, const QString& id);

    /// Remove a box from the ribbon.
    void removeBox(RibbonBox* box);

    /// Re-insert all boxes from m_boxes into the drop zone layout in order.
    void rebuildOrder();

    bool isOnAir() const;

    /// Programmatically set the ON-AIR state (e.g., auto-toggle from encoder status).
    void setOnAir(bool onAir);

signals:
    void onAirToggled(bool onAir);
    void addClockRequested();
    void addWeatherRequested();
    void addVUMeterRequested();

private slots:
    void onAddClicked();
    void onBoxCloseRequested(RibbonBox* box);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool handleDragEnter(QDragEnterEvent* e);
    bool handleDragMove(QDragMoveEvent* e);
    bool handleDrop(QDropEvent* e);
    void handleDragLeave();
    int  dropIndexAt(const QPoint& pos) const;
    void positionDropIndicator(int index);

    QWidget*     m_dropZone       = nullptr;
    QHBoxLayout* m_dropLayout     = nullptr;
    QFrame*      m_dropIndicator  = nullptr;
    QPushButton* m_onAirBtn       = nullptr;
    QList<RibbonBox*> m_boxes;
    int          m_dropIndex      = -1;
};
