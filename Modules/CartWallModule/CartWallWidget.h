#pragma once
#include <QWidget>
#include <QList>
#include <QSettings>

class QGridLayout;
class QSpinBox;
class QToolButton;
class QLabel;

namespace M1 {

class CartPlayer;
class CartButton;

/// CartWallWidget — grid UI for the cart wall.
///
/// Displays a configurable grid of CartButton widgets (default 4x4 = 16 carts).
/// Supports drag-and-drop of audio files, right-click configuration per slot,
/// and keyboard shortcuts (F1-F12 trigger first 12 carts).
class CartWallWidget : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMaxCarts = 48;
    static constexpr int kDefaultRows = 4;
    static constexpr int kDefaultCols = 4;

    explicit CartWallWidget(QList<CartPlayer*>& players, QWidget* parent = nullptr);

    void setGridSize(int rows, int cols);
    int  rows() const { return m_rows; }
    int  cols() const { return m_cols; }

    void saveState(QSettings& s);
    void loadState(QSettings& s);

signals:
    void cartTriggered(int index);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onCartTriggered(int index);
    void onCartConfigRequested(int index);
    void onGridSizeChanged();
    void onStopAll();

private:
    void rebuildGrid();
    void showCartContextMenu(int index, const QPoint& globalPos);

    QList<CartPlayer*>& m_players;
    QList<CartButton*>  m_buttons;

    QGridLayout* m_grid     = nullptr;
    QWidget*     m_gridContainer = nullptr;
    QSpinBox*    m_rowsSpin = nullptr;
    QSpinBox*    m_colsSpin = nullptr;
    QLabel*      m_statusLabel = nullptr;

    int m_rows = kDefaultRows;
    int m_cols = kDefaultCols;
};

} // namespace M1
