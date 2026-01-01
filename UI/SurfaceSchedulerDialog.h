#pragma once
#include <QDialog>
#include "SurfaceScheduler.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QTimeEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;

/// SurfaceSchedulerDialog — editor for a surface's automation schedule.
///
/// Shows a list of ScheduledEvents with add/edit/remove/enable-toggle controls.
/// Opened from the tray "SCHED" chip.
class SurfaceSchedulerDialog : public QDialog {
    Q_OBJECT

public:
    explicit SurfaceSchedulerDialog(SurfaceScheduler* scheduler,
                                    const QString& surfaceName,
                                    QWidget* parent = nullptr);

private slots:
    void onAddEvent();
    void onRemoveEvent();
    void onEventSelectionChanged();
    void onToggleEnabled();
    void onEditCommit();

private:
    void buildUi(const QString& surfaceName);
    void refreshList();
    ScheduledEvent eventFromForm() const;
    void populateForm(const ScheduledEvent& ev);

    SurfaceScheduler* m_scheduler = nullptr;

    QListWidget* m_list       = nullptr;
    QTimeEdit*   m_timeEdit   = nullptr;
    QComboBox*   m_typeCombo  = nullptr;
    QLineEdit*   m_labelEdit  = nullptr;
    QLineEdit*   m_dataEdit   = nullptr;
    QLineEdit*   m_data2Edit  = nullptr;
    QCheckBox*   m_repeatChk  = nullptr;
    QPushButton* m_addBtn     = nullptr;
    QPushButton* m_removeBtn  = nullptr;
    QPushButton* m_toggleBtn  = nullptr;
    QPushButton* m_applyBtn   = nullptr;
};
