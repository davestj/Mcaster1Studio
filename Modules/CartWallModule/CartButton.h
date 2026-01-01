#pragma once
#include <QWidget>
#include <QLabel>
#include <QColor>
#include <QString>

namespace M1 {

class CartPlayer;

/// CartButton — a single cart slot widget in the cart wall grid.
///
/// Displays: title (top), duration (bottom-right), colored background.
/// Click = instant play/stop toggle. Right-click = context menu (load, clear, color).
/// Visual feedback: pulsing border while playing, dimmed when empty.
class CartButton : public QWidget {
    Q_OBJECT

public:
    explicit CartButton(int index, CartPlayer* player, QWidget* parent = nullptr);

    int          index()    const { return m_index; }
    CartPlayer*  player()   const { return m_player; }

    void setTitle(const QString& title);
    void setColor(const QColor& color);
    void setFilePath(const QString& path);

    QString title()    const { return m_title; }
    QColor  color()    const { return m_color; }
    QString filePath() const { return m_filePath; }

signals:
    void triggered(int index);    ///< Left-click: play/stop
    void configRequested(int index); ///< Right-click: configure

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onPlayerStateChanged();
    void refreshDisplay();

private:
    int          m_index;
    CartPlayer*  m_player;
    QString      m_title;
    QString      m_filePath;
    QColor       m_color = QColor(60, 60, 60);

    QLabel*      m_titleLabel    = nullptr;
    QLabel*      m_durationLabel = nullptr;
    QLabel*      m_indexLabel    = nullptr;

    QTimer*      m_refreshTimer  = nullptr;
};

} // namespace M1
