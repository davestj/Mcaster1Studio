#pragma once
#include "DbServerEntry.h"
#include <QDialog>

class QLineEdit;
class QSpinBox;
class QComboBox;
class QGroupBox;
class QLabel;

/// Dialog for adding or editing a database server entry.
class DbServerDialog : public QDialog {
    Q_OBJECT
public:
    explicit DbServerDialog(QWidget* parent = nullptr);
    ~DbServerDialog() override = default;

    /// Pre-fill the dialog with an existing entry (for editing).
    void setEntry(const M1::DbServerEntry& entry);

    /// Get the configured entry from the dialog.
    M1::DbServerEntry entry() const;

private slots:
    void onBackendChanged(int index);
    void onTestConnection();

private:
    void buildUi();

    QLineEdit*  m_nameEdit     = nullptr;
    QComboBox*  m_backendCombo = nullptr;

    // SQLite group
    QGroupBox*  m_sqliteGroup  = nullptr;
    QLineEdit*  m_sqlitePath   = nullptr;

    // Network DB group (MySQL / PostgreSQL — shared connection fields)
    QGroupBox*  m_mysqlGroup   = nullptr;
    QLineEdit*  m_mysqlHost    = nullptr;
    QSpinBox*   m_mysqlPort    = nullptr;
    QLineEdit*  m_mysqlUser    = nullptr;
    QLineEdit*  m_mysqlPass    = nullptr;

    QLabel*     m_statusLabel  = nullptr;

    QString     m_entryId;  ///< Preserved across edit sessions
};
