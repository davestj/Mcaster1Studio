#pragma once
#include "EncoderEventLog.h"
#include <QDialog>

class QListWidget;
class QComboBox;
class QPushButton;
class QCheckBox;

/// EncoderLogDialog — per-encoder event log viewer.
///
/// Shows a scrollable list of log entries with level filtering.
/// Auto-scrolls to bottom on new entries. Non-modal so user can keep it open
/// while watching the encoder connect/stream.
class EncoderLogDialog : public QDialog {
    Q_OBJECT

public:
    explicit EncoderLogDialog(EncoderEventLog* log, const QString& encoderName,
                              QWidget* parent = nullptr);

private slots:
    void onNewEntry(const EncoderEventLog::Entry& entry);
    void onFilterChanged();
    void onClearClicked();
    void onCopyClicked();

private:
    void addEntryToList(const EncoderEventLog::Entry& entry);
    bool passesFilter(EncoderEventLog::Level level) const;

    EncoderEventLog* m_log     = nullptr;
    QListWidget*     m_list    = nullptr;
    QComboBox*       m_filter  = nullptr;
    QCheckBox*       m_autoScroll = nullptr;
};
