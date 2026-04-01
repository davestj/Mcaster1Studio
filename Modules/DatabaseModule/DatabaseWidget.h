#pragma once
#include <QWidget>
#include <QTimer>

class QLabel;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QComboBox;

namespace M1 {
class DatabaseModule;
class IDatabase;
struct DbServerEntry;
}

/// DatabaseWidget — UI for the Database Module.
///
/// Shows:
///   1. Connection status panel (server name, backend, schema, status LED)
///   2. Table browser (list tables, row counts)
///   3. Action buttons: Test, Backup, Restore, Initialize, Run Migration
///   4. Query log / diagnostics area
class DatabaseWidget : public QWidget {
    Q_OBJECT

public:
    explicit DatabaseWidget(M1::DatabaseModule* module, QWidget* parent = nullptr);
    ~DatabaseWidget() override;

    /// Re-read the module's SurfaceDbContext and update all panels.
    void refreshConnectionInfo();

private slots:
    void onTestConnection();
    void onBackupDatabase();
    void onRestoreDatabase();
    void onInitializeDatabase();
    void onRefreshTables();

private:
    void buildUi();
    void setStatusLed(bool connected);
    void appendLog(const QString& level, const QString& msg);

    M1::DatabaseModule* m_module = nullptr;

    // Connection info panel
    QLabel* m_serverNameLabel  = nullptr;
    QLabel* m_backendLabel     = nullptr;
    QLabel* m_schemaLabel      = nullptr;
    QLabel* m_statusLed        = nullptr;
    QLabel* m_statusText       = nullptr;

    // Table browser
    QTableWidget* m_tableList  = nullptr;

    // Action buttons
    QPushButton*  m_testBtn       = nullptr;
    QPushButton*  m_backupBtn     = nullptr;
    QPushButton*  m_restoreBtn    = nullptr;
    QPushButton*  m_initBtn       = nullptr;
    QPushButton*  m_refreshBtn    = nullptr;

    // Diagnostic log
    QTextEdit*    m_logOutput     = nullptr;
};
