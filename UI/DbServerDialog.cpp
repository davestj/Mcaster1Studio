#include "DbServerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>

DbServerDialog::DbServerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Database Server");
    setMinimumWidth(420);
    buildUi();
}

void DbServerDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);

    // ── Name + Backend ─────────────────────────────────────────────────────
    auto* topForm = new QFormLayout;
    topForm->setSpacing(8);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("e.g. Production MySQL, Local SQLite...");
    m_nameEdit->setToolTip("A friendly name for this database server");
    topForm->addRow("Server Name:", m_nameEdit);

    m_backendCombo = new QComboBox(this);
    m_backendCombo->addItem("SQLite (Embedded)", "sqlite");
    m_backendCombo->addItem("MySQL / MariaDB",   "mysql");
    m_backendCombo->setToolTip("SQLite requires no setup. MySQL is for enterprise/multi-user.");
    topForm->addRow("Backend:", m_backendCombo);
    root->addLayout(topForm);

    // ── SQLite Group ───────────────────────────────────────────────────────
    m_sqliteGroup = new QGroupBox("SQLite Settings", this);
    auto* sqliteForm = new QFormLayout(m_sqliteGroup);
    m_sqlitePath = new QLineEdit(this);
    m_sqlitePath->setPlaceholderText("(default: AppData/Mcaster1Studio/)");
    m_sqlitePath->setToolTip("Leave empty for the default AppData location");
    auto* browseRow = new QHBoxLayout;
    browseRow->addWidget(m_sqlitePath, 1);
    auto* browseBtn = new QPushButton("Browse...", this);
    browseRow->addWidget(browseBtn);
    sqliteForm->addRow("Database Path:", browseRow);
    root->addWidget(m_sqliteGroup);

    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(
            this, "SQLite Database File", m_sqlitePath->text(),
            "SQLite Database (*.db);;All Files (*)");
        if (!path.isEmpty()) m_sqlitePath->setText(path);
    });

    // ── MySQL Group ────────────────────────────────────────────────────────
    m_mysqlGroup = new QGroupBox("MySQL / MariaDB Settings", this);
    auto* mysqlForm = new QFormLayout(m_mysqlGroup);
    mysqlForm->setSpacing(8);

    m_mysqlHost = new QLineEdit("127.0.0.1", this);
    m_mysqlHost->setToolTip("MySQL server hostname or IP address");
    mysqlForm->addRow("Host:", m_mysqlHost);

    m_mysqlPort = new QSpinBox(this);
    m_mysqlPort->setRange(1, 65535);
    m_mysqlPort->setValue(3306);
    m_mysqlPort->setToolTip("MySQL server port (default: 3306)");
    mysqlForm->addRow("Port:", m_mysqlPort);

    m_mysqlUser = new QLineEdit("root", this);
    m_mysqlUser->setToolTip("MySQL username");
    mysqlForm->addRow("Username:", m_mysqlUser);

    m_mysqlPass = new QLineEdit(this);
    m_mysqlPass->setEchoMode(QLineEdit::Password);
    m_mysqlPass->setToolTip("MySQL password");
    mysqlForm->addRow("Password:", m_mysqlPass);

    root->addWidget(m_mysqlGroup);

    // ── Test + Status ──────────────────────────────────────────────────────
    auto* testRow = new QHBoxLayout;
    auto* testBtn = new QPushButton("Test Connection", this);
    testBtn->setToolTip("Verify the server is reachable");
    testRow->addWidget(testBtn);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    testRow->addWidget(m_statusLabel, 1);
    root->addLayout(testRow);

    connect(testBtn, &QPushButton::clicked, this, &DbServerDialog::onTestConnection);

    // ── Buttons ────────────────────────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // ── Backend toggle ─────────────────────────────────────────────────────
    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DbServerDialog::onBackendChanged);
    onBackendChanged(0);  // SQLite by default
}

void DbServerDialog::onBackendChanged(int index) {
    const QString backend = m_backendCombo->itemData(index).toString();
    m_sqliteGroup->setVisible(backend == "sqlite");
    m_mysqlGroup->setVisible(backend == "mysql");
    m_statusLabel->clear();
}

void DbServerDialog::onTestConnection() {
    M1::DbServerEntry e = entry();
    const QString err = M1::DbServerRegistry::testConnection(e);
    if (err.isEmpty()) {
        m_statusLabel->setStyleSheet("color: #22c55e; font-weight: bold;");
        m_statusLabel->setText("Connection OK");
    } else {
        m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
        m_statusLabel->setText(err);
    }
}

void DbServerDialog::setEntry(const M1::DbServerEntry& e) {
    m_entryId = e.id;
    m_nameEdit->setText(e.displayName);
    m_backendCombo->setCurrentIndex(e.isMySQL() ? 1 : 0);
    m_sqlitePath->setText(e.sqlitePath);
    m_mysqlHost->setText(e.host);
    m_mysqlPort->setValue(e.port);
    m_mysqlUser->setText(e.username);
    m_mysqlPass->setText(e.password);
}

M1::DbServerEntry DbServerDialog::entry() const {
    M1::DbServerEntry e;
    e.id          = m_entryId.isEmpty() ? M1::DbServerEntry::newId() : m_entryId;
    e.displayName = m_nameEdit->text().trimmed();
    e.backend     = (m_backendCombo->currentData().toString() == "mysql")
                        ? M1::DbServerEntry::Backend::MySQL
                        : M1::DbServerEntry::Backend::SQLite;
    e.sqlitePath  = m_sqlitePath->text().trimmed();
    e.host        = m_mysqlHost->text().trimmed();
    e.port        = m_mysqlPort->value();
    e.username    = m_mysqlUser->text().trimmed();
    e.password    = m_mysqlPass->text();
    return e;
}
