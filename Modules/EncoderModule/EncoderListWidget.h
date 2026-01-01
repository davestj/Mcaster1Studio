#pragma once
#include <QWidget>
#include <QList>
#include <QMap>

class QTableWidget;
class QLabel;
class QPushButton;
class QTimer;
class EncoderSlot;
class DnasPoller;
class EncoderConfigDialog;
class EncoderVuPanel;
class LiveMonitorWindow;

/// EncoderListWidget — QTableWidget-based list view for all encoder slots.
///
/// Shows all slots in rows with live status, codec, bitrate, server, and listener count.
/// Double-click → EncoderConfigDialog. Right-click → context menu (Connect/Disconnect/Wake/…).
/// Refreshes every 2 s via QTimer.
class EncoderListWidget : public QWidget {
    Q_OBJECT

public:
    /// Columns
    enum Column { ColNum=0, ColName, ColStatus, ColCodec, ColBitrate, ColServer, ColListeners, ColCurrentSong, ColCount };

    explicit EncoderListWidget(QList<EncoderSlot*>& slotList,
                               DnasPoller* poller,
                               QWidget* parent = nullptr);

    /// Call after adding/removing slots to rebuild rows.
    void rebuild();

    /// Refresh live status + listener counts.  Called every 2 s and on statsUpdated.
    void refresh();

signals:
    /// Emitted when user requests a new slot (from [+ Add] button).
    void addSlotRequested();
    /// Emitted when user requests removal of the selected idle slot.
    void removeSlotRequested(int slotIndex);

private slots:
    void onDoubleClicked(int row, int col);
    void onContextMenu(const QPoint& pos);
    void onAddClicked();
    void onRemoveClicked();
    void onStartAllClicked();
    void onStopAllClicked();

private:
    QString stateText(int stateInt) const;
    QColor  stateColor(int stateInt) const;
    bool    stateItalic(int stateInt) const;

    QList<EncoderSlot*>* m_slots = nullptr;
    DnasPoller*          m_poller;

    QTableWidget*        m_table      = nullptr;
    EncoderVuPanel*      m_vuPanel    = nullptr;
    QLabel*              m_liveLabel  = nullptr;
    QPushButton*         m_addBtn     = nullptr;
    QPushButton*         m_removeBtn  = nullptr;
    QPushButton*         m_startAllBtn = nullptr;
    QPushButton*         m_stopAllBtn  = nullptr;
    QTimer*              m_refreshTimer = nullptr;

    QMap<EncoderSlot*, LiveMonitorWindow*> m_liveMonitors;
};
