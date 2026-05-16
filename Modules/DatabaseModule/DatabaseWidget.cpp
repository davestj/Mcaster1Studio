#include "DatabaseWidget.h"
#include "DatabaseModule.h"
#include "DbServerEntry.h"
#include "SurfaceDbContext.h"
#include "DatabaseFactory.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

DatabaseWidget::DatabaseWidget(M1::DatabaseModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    setObjectName("DatabaseWidget");
    buildUi();
    refreshConnectionInfo();
}

DatabaseWidget::~DatabaseWidget() = default;

void DatabaseWidget::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ── Connection Info Panel ────────────────────────────────────────────
    auto* connGroup = new QGroupBox("Connection", this);
    connGroup->setObjectName("DbConnGroup");
    auto* connGrid = new QGridLayout(connGroup);
    connGrid->setSpacing(6);

    // Status LED + text
    m_statusLed = new QLabel(this);
    m_statusLed->setFixedSize(14, 14);
    m_statusLed->setObjectName("DbStatusLed");
    setStatusLed(false);

    m_statusText = new QLabel("Not configured", this);
    m_statusText->setObjectName("DbStatusText");

    auto* statusRow = new QHBoxLayout;
    statusRow->addWidget(m_statusLed);
    statusRow->addWidget(m_statusText, 1);
    connGrid->addLayout(statusRow, 0, 0, 1, 2);

    connGrid->addWidget(new QLabel("Server:", this), 1, 0);
    m_serverNameLabel = new QLabel("—", this);
    m_serverNameLabel->setObjectName("DbServerName");
    connGrid->addWidget(m_serverNameLabel, 1, 1);

    connGrid->addWidget(new QLabel("Backend:", this), 2, 0);
    m_backendLabel = new QLabel("—", this);
    connGrid->addWidget(m_backendLabel, 2, 1);

    connGrid->addWidget(new QLabel("Database:", this), 3, 0);
    m_schemaLabel = new QLabel("—", this);
    m_schemaLabel->setObjectName("DbSchemaName");
    connGrid->addWidget(m_schemaLabel, 3, 1);

    root->addWidget(connGroup);

    // ── Action Buttons ──────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);

    m_testBtn = new QPushButton("Test Connection", this);
    m_testBtn->setToolTip("Test the database connection for this surface");
    connect(m_testBtn, &QPushButton::clicked, this, &DatabaseWidget::onTestConnection);
    btnRow->addWidget(m_testBtn);

    m_initBtn = new QPushButton("Initialize DB", this);
    m_initBtn->setToolTip("Create the database schema for this surface (tables, indexes)");
    connect(m_initBtn, &QPushButton::clicked, this, &DatabaseWidget::onInitializeDatabase);
    btnRow->addWidget(m_initBtn);

    m_backupBtn = new QPushButton("Backup", this);
    m_backupBtn->setToolTip("Export the surface database to a backup file");
    connect(m_backupBtn, &QPushButton::clicked, this, &DatabaseWidget::onBackupDatabase);
    btnRow->addWidget(m_backupBtn);

    m_restoreBtn = new QPushButton("Restore", this);
    m_restoreBtn->setToolTip("Restore the surface database from a backup file");
    connect(m_restoreBtn, &QPushButton::clicked, this, &DatabaseWidget::onRestoreDatabase);
    btnRow->addWidget(m_restoreBtn);

    btnRow->addStretch();
    root->addLayout(btnRow);

    // ── Table Browser ───────────────────────────────────────────────────
    auto* tableGroup = new QGroupBox("Tables", this);
    tableGroup->setObjectName("DbTableGroup");
    auto* tableLayout = new QVBoxLayout(tableGroup);

    auto* tableToolbar = new QHBoxLayout;
    m_refreshBtn = new QPushButton("Refresh", this);
    m_refreshBtn->setToolTip("Refresh the table list from the database");
    connect(m_refreshBtn, &QPushButton::clicked, this, &DatabaseWidget::onRefreshTables);
    tableToolbar->addWidget(m_refreshBtn);
    tableToolbar->addStretch();
    tableLayout->addLayout(tableToolbar);

    m_tableList = new QTableWidget(0, 3, this);
    m_tableList->setHorizontalHeaderLabels({"Table Name", "Rows", "Size"});
    m_tableList->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableList->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tableList->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tableList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableList->verticalHeader()->hide();
    m_tableList->setMaximumHeight(160);
    tableLayout->addWidget(m_tableList);

    root->addWidget(tableGroup);

    // ── Diagnostic Log ──────────────────────────────────────────────────
    auto* logGroup = new QGroupBox("Diagnostics", this);
    logGroup->setObjectName("DbLogGroup");
    auto* logLayout = new QVBoxLayout(logGroup);

    m_logOutput = new QTextEdit(this);
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(100);
    m_logOutput->setPlaceholderText("Database operations log...");
    m_logOutput->setObjectName("DbLogOutput");
    logLayout->addWidget(m_logOutput);

    root->addWidget(logGroup);
    root->addStretch();
}

void DatabaseWidget::setStatusLed(bool connected) {
    const auto pal = ThemePalette::forCurrentTheme();
    const QColor color = connected ? pal.success : pal.textDisabled;
    m_statusLed->setStyleSheet(
        QString("background: %1; border-radius: 7px; border: 1px solid %2;")
            .arg(color.name(), color.darker(130).name()));
}

void DatabaseWidget::appendLog(const QString& level, const QString& msg) {
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const auto pal = ThemePalette::forCurrentTheme();
    QString color = pal.textMuted.name();
    if (level == "OK")    color = pal.success.name();
    if (level == "ERROR") color = pal.error.name();
    if (level == "WARN")  color = pal.warning.name();
    m_logOutput->append(QString("<span style='color:%1'>[%2] [%3] %4</span>")
                            .arg(color, ts, level, msg.toHtmlEscaped()));
}

void DatabaseWidget::refreshConnectionInfo() {
    if (!m_module) return;

    const auto ctx = m_module->surfaceDbContext();
    if (!ctx.isValid()) {
        m_serverNameLabel->setText("Not assigned");
        m_backendLabel->setText("—");
        m_schemaLabel->setText("—");
        m_statusText->setText("No database assigned to this surface");
        setStatusLed(false);
        m_testBtn->setEnabled(false);
        m_initBtn->setEnabled(false);
        m_backupBtn->setEnabled(false);
        m_restoreBtn->setEnabled(false);
        m_refreshBtn->setEnabled(false);
        return;
    }

    const auto* server = M1::DbServerRegistry::instance().findById(ctx.serverId);
    if (server) {
        m_serverNameLabel->setText(server->displayName);
        m_backendLabel->setText(server->backendDisplayName());
    } else {
        m_serverNameLabel->setText("(server not found)");
        m_backendLabel->setText("—");
    }
    m_schemaLabel->setText(ctx.schemaName);
    m_statusText->setText("Configured — click Test Connection to verify");
    setStatusLed(false);

    m_testBtn->setEnabled(true);
    m_initBtn->setEnabled(true);
    m_backupBtn->setEnabled(true);
    m_restoreBtn->setEnabled(true);
    m_refreshBtn->setEnabled(true);
}

void DatabaseWidget::onTestConnection() {
    if (!m_module) return;
    const auto ctx = m_module->surfaceDbContext();
    if (!ctx.isValid()) {
        appendLog("ERROR", "No database context assigned.");
        return;
    }

    const auto* server = M1::DbServerRegistry::instance().findById(ctx.serverId);
    if (!server) {
        appendLog("ERROR", "Server not found in registry: " + ctx.serverId);
        setStatusLed(false);
        m_statusText->setText("Server not found");
        return;
    }

    appendLog("INFO", "Testing connection to " + server->displayName + "...");

    const QString err = M1::DbServerRegistry::testConnection(*server);
    if (err.isEmpty()) {
        setStatusLed(true);
        m_statusText->setText("Connected — " + server->backendDisplayName());
        appendLog("OK", "Connection successful. Schema: " + ctx.schemaName);
    } else {
        setStatusLed(false);
        m_statusText->setText("Connection failed");
        appendLog("ERROR", err);
    }
}

void DatabaseWidget::onInitializeDatabase() {
    if (!m_module) return;
    const auto ctx = m_module->surfaceDbContext();
    if (!ctx.isValid()) {
        appendLog("ERROR", "No database context assigned.");
        return;
    }

    const auto* server = M1::DbServerRegistry::instance().findById(ctx.serverId);
    if (!server) {
        appendLog("ERROR", "Server not found.");
        return;
    }

    auto reply = QMessageBox::question(this, "Initialize Database",
        QString("This will create the database \"%1\" on server \"%2\".\n\n"
                "If the database already exists, missing tables will be created.\n\n"
                "Continue?")
            .arg(ctx.schemaName, server->displayName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    appendLog("INFO", "Initializing database: " + ctx.schemaName);

    // Check if a driver is registered for this backend
    const QString driverErr = M1::DatabaseFactory::checkDriverAvailable(server->backend);
    if (!driverErr.isEmpty()) {
        appendLog("WARN", driverErr);
        appendLog("INFO", "Schema creation will proceed when the driver becomes available.");
        return;
    }

    // Attempt to create and connect using the factory
    QString dbPath;
    if (server->isSQLite()) {
        // Build the SQLite file path from the server path + schema name
        QString baseDir = server->sqlitePath;
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath() + "/data";
        QDir dir(baseDir);
        if (!dir.exists()) dir.mkpath(".");
        dbPath = dir.absoluteFilePath(ctx.schemaName + ".db");
    }

    M1::IDatabase* db = M1::DatabaseFactory::create(*server, ctx.schemaName, dbPath);
    if (!db) {
        appendLog("ERROR", QString("Failed to create connection: %1")
                  .arg(server->isNetworked() ? "check server credentials and network" : "check file path"));
        return;
    }

    if (db->isConnected() && db->createSchema()) {
        appendLog("OK", "Database initialized successfully.");
        appendLog("OK", "Schema: " + ctx.schemaName + " on " + server->backendDisplayName());
        setStatusLed(true);
        m_statusText->setText("Initialized — " + server->backendDisplayName());
    } else {
        appendLog("ERROR", "Schema creation failed: " + db->lastError());
    }

    db->disconnect();
    delete db;
}

void DatabaseWidget::onBackupDatabase() {
    if (!m_module) return;
    const auto ctx = m_module->surfaceDbContext();
    if (!ctx.isValid()) {
        appendLog("ERROR", "No database context assigned.");
        return;
    }

    const QString defaultName = ctx.schemaName + "_backup_" +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".sql";
    const QString path = QFileDialog::getSaveFileName(this, "Backup Database",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + defaultName,
        "SQL Files (*.sql);;All Files (*)");

    if (path.isEmpty()) return;

    appendLog("INFO", "Backup requested → " + path);
    appendLog("WARN", "Full backup requires database driver integration (coming in Phase 12+).");
}

void DatabaseWidget::onRestoreDatabase() {
    if (!m_module) return;
    const auto ctx = m_module->surfaceDbContext();
    if (!ctx.isValid()) {
        appendLog("ERROR", "No database context assigned.");
        return;
    }

    const QString path = QFileDialog::getOpenFileName(this, "Restore Database",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "SQL Files (*.sql);;All Files (*)");

    if (path.isEmpty()) return;

    auto reply = QMessageBox::warning(this, "Restore Database",
        QString("This will REPLACE the current database \"%1\" with the backup.\n\n"
                "This action cannot be undone. Continue?")
            .arg(ctx.schemaName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    appendLog("INFO", "Restore requested from " + path);
    appendLog("WARN", "Full restore requires database driver integration (coming in Phase 12+).");
}

void DatabaseWidget::onRefreshTables() {
    if (!m_module) return;
    const auto ctx = m_module->surfaceDbContext();
    if (!ctx.isValid()) {
        appendLog("ERROR", "No database context assigned.");
        return;
    }

    m_tableList->setRowCount(0);
    appendLog("INFO", "Refreshing table list for: " + ctx.schemaName);

    const auto* server = M1::DbServerRegistry::instance().findById(ctx.serverId);
    if (!server) {
        appendLog("ERROR", "Server not found.");
        return;
    }

    if (!M1::DatabaseFactory::isDriverRegistered(server->backend)) {
        appendLog("WARN", M1::DatabaseFactory::checkDriverAvailable(server->backend));
        return;
    }

    // Build dbPath for SQLite
    QString dbPath;
    if (server->isSQLite()) {
        QString baseDir = server->sqlitePath;
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath() + "/data";
        dbPath = QDir(baseDir).absoluteFilePath(ctx.schemaName + ".db");
    }

    M1::IDatabase* db = M1::DatabaseFactory::create(*server, ctx.schemaName, dbPath);
    if (!db || !db->isConnected()) {
        appendLog("ERROR", "Cannot connect to get table list.");
        delete db;
        return;
    }

    const QStringList tables = db->tableNames();
    m_tableList->setRowCount(tables.size());
    for (int i = 0; i < tables.size(); ++i) {
        m_tableList->setItem(i, 0, new QTableWidgetItem(tables[i]));
        // Try to get row count for each table
        const QString countSql = QString("SELECT COUNT(*) FROM %1")
            .arg(M1::DatabaseFactory::dialect(server->backend)->quoteId(tables[i]));
        auto rows = db->executeQuery(countSql);
        const QString rowCount = (!rows.isEmpty() && !rows[0].isEmpty())
            ? rows[0][0].toString() : "—";
        m_tableList->setItem(i, 1, new QTableWidgetItem(rowCount));
        m_tableList->setItem(i, 2, new QTableWidgetItem("—"));
    }

    db->disconnect();
    delete db;

    appendLog("OK", QString("Found %1 table(s).").arg(tables.size()));
}
