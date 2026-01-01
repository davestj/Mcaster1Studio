#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include "MediaItem.h"

class QueueModule;

// ─── QueueListWidget ─────────────────────────────────────────────────────────
/// Drag-and-drop enabled list that accepts external file drops.
class QueueListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit QueueListWidget(QWidget* parent = nullptr);

signals:
    void filesDropped(const QStringList& paths);
    void rowMoved(int from, int to);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e)   override;
    void dropEvent(QDropEvent* e)           override;
};

// ─── QueueWidget ─────────────────────────────────────────────────────────────
/// Simple queue UI — add files, reorder, play now, remove.
/// No AutoDJ, no crossfader, no rotation rules.
class QueueWidget : public QWidget {
    Q_OBJECT

public:
    explicit QueueWidget(QueueModule* module, QWidget* parent = nullptr);

private slots:
    void onQueueChanged();
    void onAddFiles();
    void onPlayNow();
    void onClearQueue();
    void onFilesDropped(const QStringList& paths);
    void onRowMoved(int from, int to);
    void showContextMenu(const QPoint& pos);

private:
    void buildRow(const M1::MediaItem& item, int index, bool isCurrent);

    QueueModule*      m_module     = nullptr;
    QueueListWidget*  m_listWidget = nullptr;

    QPushButton*      m_addBtn     = nullptr;
    QPushButton*      m_playBtn    = nullptr;
    QPushButton*      m_clearBtn   = nullptr;

    QLabel*           m_statusBar  = nullptr;
};
