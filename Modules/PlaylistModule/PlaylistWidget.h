#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QMenu>
#include <QStyledItemDelegate>

namespace M1 { class PlaylistModule; }

// ─────────────────────────────────────────────────────────────────────────────
// PlaylistListWidget — drag-drop enabled QListWidget
// ─────────────────────────────────────────────────────────────────────────────

class PlaylistListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit PlaylistListWidget(QWidget* parent = nullptr);

signals:
    void filesDropped(const QStringList& paths);
    void rowMoved(int from, int to);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent*  event) override;
    void dropEvent(QDropEvent*          event) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// PlaylistItemDelegate — custom painter for queue items
// Paints the currently-playing track with a distinct background that cannot be
// overridden by the list widget stylesheet (the root cause of the white-bg bug).
// ─────────────────────────────────────────────────────────────────────────────

class PlaylistItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PlaylistItemDelegate(QWidget* parent = nullptr);
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};

// ─────────────────────────────────────────────────────────────────────────────
// PlaylistWidget — main widget for PlaylistModule (Playlist / AutoDJ combo)
// ─────────────────────────────────────────────────────────────────────────────

class PlaylistWidget : public QWidget {
    Q_OBJECT

public:
    explicit PlaylistWidget(M1::PlaylistModule* module, QWidget* parent = nullptr);
    ~PlaylistWidget() override = default;

private slots:
    void onQueueChanged();
    void onAutoDJChanged(bool enabled);
    void onQueueLow(int remaining);
    void onPlayNext();
    void onSkip();
    void onToggleAutoDJ();
    void onClearQueue();
    void onAddFiles();
    void onOpenConfig();
    void onFilesDropped(const QStringList& paths);
    void onRowMoved(int from, int to);
    void showContextMenu(const QPoint& pos);

private:
    void buildUi();
    void applyTheme();

    M1::PlaylistModule*   m_module     = nullptr;
    PlaylistListWidget*   m_listWidget = nullptr;

    // Toolbar buttons
    QPushButton*          m_btnAddFiles = nullptr;
    QPushButton*          m_btnPlay     = nullptr;
    QPushButton*          m_btnSkip     = nullptr;
    QPushButton*          m_btnAutoDJ   = nullptr;
    QPushButton*          m_btnConfig   = nullptr;
    QPushButton*          m_btnClear    = nullptr;

    // Rotation / queue depth controls
    QSpinBox*             m_artistSepSpin = nullptr;
    QSpinBox*             m_titleSepSpin  = nullptr;
    QSpinBox*             m_minQueueSpin  = nullptr;

    // Status bar
    QLabel*               m_statusBar  = nullptr;

    // Supported audio extensions for drop filtering
    static const QStringList s_audioExts;
};
