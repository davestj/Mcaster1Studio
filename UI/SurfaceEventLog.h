#pragma once
#include <QWidget>
#include <QDialog>
#include <QList>
#include <QString>
#include <QDateTime>

class QListWidget;
class QLabel;
class QPushButton;

/// One timestamped event entry for a surface's event log.
struct SurfaceEventEntry {
    QDateTime   timestamp;
    QString     category;  ///< e.g. "DECK", "ENCODER", "SCHED", "ERROR", "INFO"
    QString     message;
};

/// SurfaceEventLog — stores and displays per-surface event history.
///
/// Events are appended by SurfaceWidget and its modules via appendEvent().
/// The log dialog can be opened from the tray "LOG" button.
/// Unread events are tracked so the tray button can show a badge count.
class SurfaceEventLog : public QObject {
    Q_OBJECT

public:
    explicit SurfaceEventLog(const QString& surfaceName, QObject* parent = nullptr);

    /// Append a new event. Emits eventAppended().
    void appendEvent(const QString& category, const QString& message);

    /// Number of unread events (reset when log dialog is shown).
    int unreadCount() const { return m_unreadCount; }

    /// Open (or raise) the floating log dialog.
    void showDialog(QWidget* parentWidget = nullptr);

    const QList<SurfaceEventEntry>& entries() const { return m_entries; }

signals:
    /// Emitted when a new event is added (e.g. to update badge on tray button).
    void eventAppended(const SurfaceEventEntry& entry);

private:
    QString                    m_surfaceName;
    QList<SurfaceEventEntry>   m_entries;
    int                        m_unreadCount = 0;
    QDialog*                   m_dialog      = nullptr;
    QListWidget*               m_listWidget  = nullptr;

    void buildDialog(QWidget* parentWidget);
};
