#include "PreferencesDialog.h"
#include "AudioEngine.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QApplication>
#include <QTabWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QListWidget>
#include <QTextEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStackedWidget>
#include <QScrollArea>
#include <QTableWidget>
#include <QHeaderView>
#include "DbServerEntry.h"
#include "MonitorManager.h"
#include "DbServerDialog.h"

PreferencesDialog::PreferencesDialog(M1::IAudioEngine* engine, QWidget* parent)
    : QDialog(parent)
    , m_engine(engine)
{
    setWindowTitle("Preferences");
    setMinimumWidth(520);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(12);

    // ── Tab widget ──────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildAudioPage(),      "Audio");
    m_tabs->addTab(buildAppearancePage(), "Appearance");
    m_tabs->addTab(buildDatabasePage(),   "Database");
    m_tabs->addTab(buildDbServersPage(), "DB Servers");
    m_tabs->addTab(buildDisplaysPage(),  "Displays & Capture");
    m_tabs->addTab(buildMetricsPage(),   "Metrics / Grafana");
    m_tabs->addTab(buildAIPage(),        "AI Integration");
    root->addWidget(m_tabs);

    // ── Dialog buttons ──────────────────────────────────────────
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccepted);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // Restore last-used audio devices
    QSettings s("Mcaster1", "Mcaster1Studio");
    int lastIn  = s.value("audio/inputDevice", -1).toInt();
    int lastOut = s.value("audio/outputDevice", -1).toInt();
    int lastCue = s.value("audio/cueDevice", -1).toInt();
    for (int i = 0; i < m_inputIndices.size(); ++i) {
        if (m_inputIndices[i] == lastIn) { m_inputCombo->setCurrentIndex(i); break; }
    }
    for (int i = 0; i < m_outputIndices.size(); ++i) {
        if (m_outputIndices[i] == lastOut) { m_outputCombo->setCurrentIndex(i); break; }
    }
    for (int i = 0; i < m_cueIndices.size(); ++i) {
        if (m_cueIndices[i] == lastCue) { m_cueCombo->setCurrentIndex(i); break; }
    }
    int sr  = s.value("audio/sampleRate", 48000).toInt();
    int buf = s.value("audio/bufferSize", 512).toInt();
    m_sampleRateCombo->setCurrentIndex(m_sampleRateCombo->findData(sr));
    m_bufferSizeCombo->setCurrentIndex(m_bufferSizeCombo->findData(buf));
}

// ─── Audio page ──────────────────────────────────────────────────────────────
QWidget* PreferencesDialog::buildAudioPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* audioGroup = new QGroupBox("Audio Device");
    auto* form = new QFormLayout(audioGroup);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_inputCombo  = new QComboBox(this);
    m_outputCombo = new QComboBox(this);
    m_cueCombo    = new QComboBox(this);
    m_sampleRateCombo = new QComboBox(this);
    m_bufferSizeCombo = new QComboBox(this);

    for (int r : {44100, 48000, 88200, 96000, 192000})
        m_sampleRateCombo->addItem(QString("%1 Hz").arg(r), r);
    m_sampleRateCombo->setCurrentIndex(1); // 48000

    for (int b : {64, 128, 256, 512, 1024, 2048})
        m_bufferSizeCombo->addItem(QString("%1 frames").arg(b), b);
    m_bufferSizeCombo->setCurrentIndex(3); // 512

    form->addRow("Input Device:",  m_inputCombo);
    form->addRow("Output Device (AIR):", m_outputCombo);
    form->addRow("CUE / Headphone Device:", m_cueCombo);
    form->addRow("Sample Rate:",   m_sampleRateCombo);
    form->addRow("Buffer Size:",   m_bufferSizeCombo);

    auto* refreshBtn = new QPushButton("Refresh Devices");
    refreshBtn->setToolTip("Rescan available audio devices");
    connect(refreshBtn, &QPushButton::clicked, this, &PreferencesDialog::onRefreshDevices);
    form->addRow("", refreshBtn);

    auto* asioNote = new QLabel(
        "ASIO devices are listed if your audio interface supports ASIO.\n"
        "Use ASIO for lowest latency professional monitoring.", this);
    asioNote->setWordWrap(true);
    asioNote->setProperty("role", "status");
    form->addRow("", asioNote);

    layout->addWidget(audioGroup);
    layout->addStretch();

    populateDevices();
    return page;
}

// ─── Appearance page ─────────────────────────────────────────────────────────
QWidget* PreferencesDialog::buildAppearancePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    auto* group = new QGroupBox("Theme");
    auto* form  = new QFormLayout(group);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("Dark (Default)",       "dark");
    m_themeCombo->addItem("Classic (Brown)",      "classic");
    m_themeCombo->addItem("Enterprise (Pro)",     "light");

    const QString cur = ThemeManager::themeName(ThemeManager::instance()->currentTheme());
    int idx = m_themeCombo->findData(cur);
    if (idx >= 0) m_themeCombo->setCurrentIndex(idx);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::onThemeChanged);

    form->addRow("Theme:", m_themeCombo);
    layout->addWidget(group);
    layout->addStretch();
    return page;
}

// ─── Database page ───────────────────────────────────────────────────────────
QWidget* PreferencesDialog::buildDatabasePage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // Backend selector
    auto* backendGroup = new QGroupBox("Database Backend");
    auto* backendForm  = new QFormLayout(backendGroup);
    m_dbBackendCombo = new QComboBox(this);
    m_dbBackendCombo->addItem("SQLite (Embedded, Default)", "sqlite");
    m_dbBackendCombo->addItem("MySQL / MariaDB",            "mysql");
    m_dbBackendCombo->setToolTip("SQLite requires no setup. MySQL is for enterprise/multi-user deployments.");
    backendForm->addRow("Backend:", m_dbBackendCombo);

    m_dbStatusLabel = new QLabel(this);
    m_dbStatusLabel->setWordWrap(true);
    backendForm->addRow("", m_dbStatusLabel);
    layout->addWidget(backendGroup);

    // SQLite group
    m_sqliteGroup = new QGroupBox("SQLite Settings");
    auto* sqliteForm = new QFormLayout(m_sqliteGroup);
    m_sqlitePath = new QLineEdit(this);
    m_sqlitePath->setPlaceholderText("(default: AppData/Mcaster1Studio/mcaster1studio.db)");
    m_sqlitePath->setToolTip("Leave blank to use the default location");
    auto* browseRow = new QHBoxLayout;
    browseRow->addWidget(m_sqlitePath);
    auto* browseBtn = new QPushButton("Browse...", this);
    browseBtn->setToolTip("Choose a custom database file location");
    connect(browseBtn, &QPushButton::clicked, this, &PreferencesDialog::onSqliteBrowse);
    browseRow->addWidget(browseBtn);
    sqliteForm->addRow("Database File:", browseRow);
    layout->addWidget(m_sqliteGroup);

    // MySQL group
    m_mysqlGroup = new QGroupBox("MySQL / MariaDB Settings");
    auto* mysqlForm = new QFormLayout(m_mysqlGroup);
    m_mysqlHost = new QLineEdit("127.0.0.1", this);
    m_mysqlHost->setToolTip("MySQL server hostname or IP address");
    m_mysqlPort = new QSpinBox(this);
    m_mysqlPort->setRange(1, 65535);
    m_mysqlPort->setValue(3306);
    m_mysqlPort->setToolTip("MySQL server port");
    m_mysqlUser = new QLineEdit("root", this);
    m_mysqlUser->setToolTip("MySQL user name");
    m_mysqlPass = new QLineEdit(this);
    m_mysqlPass->setEchoMode(QLineEdit::Password);
    m_mysqlPass->setToolTip("MySQL password");
    m_mysqlDb   = new QLineEdit("mcaster1studio", this);
    m_mysqlDb->setToolTip("Database name (will be created if it doesn't exist)");

    mysqlForm->addRow("Host:",     m_mysqlHost);
    mysqlForm->addRow("Port:",     m_mysqlPort);
    mysqlForm->addRow("User:",     m_mysqlUser);
    mysqlForm->addRow("Password:", m_mysqlPass);
    mysqlForm->addRow("Database:", m_mysqlDb);
    layout->addWidget(m_mysqlGroup);

    layout->addStretch();

    // Load saved values
    QSettings s("Mcaster1", "Mcaster1Studio");
    const QString backend = s.value("database/backend", "sqlite").toString();
    int backendIdx = m_dbBackendCombo->findData(backend);
    if (backendIdx >= 0) m_dbBackendCombo->setCurrentIndex(backendIdx);

    m_sqlitePath->setText(s.value("database/sqlite/path", "").toString());
    m_mysqlHost->setText(s.value("database/mysql/host",     "127.0.0.1").toString());
    m_mysqlPort->setValue(s.value("database/mysql/port",    3306).toInt());
    m_mysqlUser->setText(s.value("database/mysql/user",     "root").toString());
    m_mysqlPass->setText(s.value("database/mysql/password", "").toString());
    m_mysqlDb->setText(s.value("database/mysql/database",   "mcaster1studio").toString());

    connect(m_dbBackendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::onDbBackendChanged);
    onDbBackendChanged(m_dbBackendCombo->currentIndex());

    return page;
}

void PreferencesDialog::onDbBackendChanged(int index) {
    const QString backend = m_dbBackendCombo->itemData(index).toString();
    m_sqliteGroup->setVisible(backend == "sqlite");
    m_mysqlGroup->setVisible(backend == "mysql");

    if (backend == "sqlite") {
        m_dbStatusLabel->setText("SQLite is zero-config. The database file is created automatically.");
    } else {
        m_dbStatusLabel->setText(
            "Requires a running MySQL/MariaDB server. "
            "Changes take effect after restarting the application.");
    }
}

void PreferencesDialog::onSqliteBrowse() {
    QString path = QFileDialog::getSaveFileName(
        this, "SQLite Database Location",
        m_sqlitePath->text().isEmpty() ? QString() : m_sqlitePath->text(),
        "SQLite Database (*.db);;All Files (*)");
    if (!path.isEmpty())
        m_sqlitePath->setText(path);
}

// ─── Metrics / Grafana page ──────────────────────────────────────────────────
QWidget* PreferencesDialog::buildMetricsPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // Prometheus group
    auto* promGroup = new QGroupBox("Prometheus Endpoint");
    auto* promForm  = new QFormLayout(promGroup);

    m_metricsEnabled = new QCheckBox("Enable Prometheus /metrics endpoint", this);
    m_metricsEnabled->setToolTip("Serve health metrics on a local HTTP port for Prometheus scraping");
    promForm->addRow(m_metricsEnabled);

    m_metricsPort = new QSpinBox(this);
    m_metricsPort->setRange(1024, 65535);
    m_metricsPort->setValue(9100);
    m_metricsPort->setToolTip("TCP port for the Prometheus metrics endpoint");
    promForm->addRow("Port:", m_metricsPort);

    connect(m_metricsEnabled, &QCheckBox::toggled,
            m_metricsPort, &QWidget::setEnabled);

    layout->addWidget(promGroup);

    // Grafana Cloud group
    auto* grafGroup = new QGroupBox("Grafana Cloud (Remote Push)");
    auto* grafForm  = new QFormLayout(grafGroup);

    m_grafanaEnabled = new QCheckBox("Enable Grafana Cloud push", this);
    m_grafanaEnabled->setToolTip("Push metrics to a Grafana Cloud Prometheus endpoint");
    grafForm->addRow(m_grafanaEnabled);

    m_grafanaEndpoint = new QLineEdit(this);
    m_grafanaEndpoint->setPlaceholderText("https://prometheus-us-central1.grafana.net/api/prom/push");
    m_grafanaEndpoint->setToolTip("Grafana Cloud Prometheus remote write URL");
    grafForm->addRow("Endpoint URL:", m_grafanaEndpoint);

    m_grafanaInterval = new QSpinBox(this);
    m_grafanaInterval->setRange(5, 300);
    m_grafanaInterval->setValue(15);
    m_grafanaInterval->setSuffix(" seconds");
    m_grafanaInterval->setToolTip("How often to push metrics to Grafana Cloud");
    grafForm->addRow("Push Interval:", m_grafanaInterval);

    m_grafanaApiKey = new QLineEdit(this);
    m_grafanaApiKey->setEchoMode(QLineEdit::Password);
    m_grafanaApiKey->setToolTip("Grafana Cloud API key (kept locally, never transmitted in plain text)");
    grafForm->addRow("API Key:", m_grafanaApiKey);

    connect(m_grafanaEnabled, &QCheckBox::toggled, this, [this](bool on) {
        m_grafanaEndpoint->setEnabled(on);
        m_grafanaInterval->setEnabled(on);
        m_grafanaApiKey->setEnabled(on);
    });

    layout->addWidget(grafGroup);
    layout->addStretch();

    // Load saved values
    QSettings s("Mcaster1", "Mcaster1Studio");
    m_metricsEnabled->setChecked(s.value("metrics/enabled", false).toBool());
    m_metricsPort->setValue(s.value("metrics/port", 9100).toInt());
    m_metricsPort->setEnabled(m_metricsEnabled->isChecked());

    m_grafanaEnabled->setChecked(s.value("metrics/grafana/enabled", false).toBool());
    m_grafanaEndpoint->setText(s.value("metrics/grafana/endpoint", "").toString());
    m_grafanaInterval->setValue(s.value("metrics/grafana/interval", 15).toInt());
    m_grafanaApiKey->setText(s.value("metrics/grafana/apiKey", "").toString());
    const bool grafOn = m_grafanaEnabled->isChecked();
    m_grafanaEndpoint->setEnabled(grafOn);
    m_grafanaInterval->setEnabled(grafOn);
    m_grafanaApiKey->setEnabled(grafOn);

    return page;
}

// ─── AI Integration page ─────────────────────────────────────────────────────

static QWidget* buildApiProviderPanel(const QString& providerName,
                                       QLineEdit*& apiKeyOut,
                                       QComboBox*& modelOut,
                                       const QStringList& defaultModels,
                                       QWidget* parent)
{
    auto* page = new QWidget(parent);
    auto* layout = new QVBoxLayout(page);

    auto* group = new QGroupBox(providerName + " Settings", page);
    auto* form  = new QFormLayout(group);

    apiKeyOut = new QLineEdit(page);
    apiKeyOut->setEchoMode(QLineEdit::Password);
    apiKeyOut->setPlaceholderText("Enter your " + providerName + " API key");
    apiKeyOut->setToolTip(providerName + " API key (stored locally in QSettings)");
    form->addRow("API Key:", apiKeyOut);

    modelOut = new QComboBox(page);
    for (const auto& m : defaultModels)
        modelOut->addItem(m);
    modelOut->setEditable(true);
    modelOut->setToolTip("Select or enter a model name");
    form->addRow("Model:", modelOut);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* PreferencesDialog::buildAIPage() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);

    // Network manager for Ollama API calls
    m_aiNam = new QNetworkAccessManager(this);

    // ── Provider selector ─────────────────────────────────────────
    auto* providerGroup = new QGroupBox("AI Provider");
    auto* providerForm  = new QFormLayout(providerGroup);

    m_aiProviderCombo = new QComboBox(this);
    m_aiProviderCombo->addItem("Ollama (Local)",  "ollama");
    m_aiProviderCombo->addItem("Claude (Anthropic)", "claude");
    m_aiProviderCombo->addItem("Grok (xAI)",      "grok");
    m_aiProviderCombo->addItem("Gemini (Google)",  "gemini");
    m_aiProviderCombo->addItem("ChatGPT (OpenAI)", "chatgpt");
    m_aiProviderCombo->addItem("Venice",           "venice");
    m_aiProviderCombo->setToolTip("Select your preferred AI provider for Studio AI features");
    providerForm->addRow("Provider:", m_aiProviderCombo);

    layout->addWidget(providerGroup);

    // ── Stacked pages (one per provider) ──────────────────────────
    m_aiStack = new QStackedWidget(this);

    // ── Page 0: Ollama ────────────────────────────────────────────
    {
        auto* ollamaPage = new QWidget(this);
        auto* ollamaLayout = new QVBoxLayout(ollamaPage);

        // Connection group
        auto* connGroup = new QGroupBox("Ollama Connection", ollamaPage);
        auto* connForm  = new QFormLayout(connGroup);

        m_ollamaUrl = new QLineEdit("http://localhost:11434", ollamaPage);
        m_ollamaUrl->setToolTip("Ollama server URL (default: http://localhost:11434)");
        connForm->addRow("Server URL:", m_ollamaUrl);

        m_ollamaActiveModel = new QComboBox(ollamaPage);
        m_ollamaActiveModel->setEditable(true);
        m_ollamaActiveModel->setToolTip("Select the model to use for AI features");
        connForm->addRow("Active Model:", m_ollamaActiveModel);

        auto* connBtnRow = new QHBoxLayout;
        auto* testBtn = new QPushButton("Test Connection", ollamaPage);
        testBtn->setToolTip("Test connectivity to the local Ollama instance");
        connect(testBtn, &QPushButton::clicked, this, &PreferencesDialog::onOllamaTestConnection);
        connBtnRow->addWidget(testBtn);

        auto* diagBtn = new QPushButton("Diagnostic Check", ollamaPage);
        diagBtn->setToolTip("Run a diagnostic check with the selected model");
        connect(diagBtn, &QPushButton::clicked, this, &PreferencesDialog::onOllamaDiagnostic);
        connBtnRow->addWidget(diagBtn);
        connBtnRow->addStretch();
        connForm->addRow("", connBtnRow);

        ollamaLayout->addWidget(connGroup);

        // Model management group
        auto* modelGroup = new QGroupBox("Model Management", ollamaPage);
        auto* modelLayout = new QVBoxLayout(modelGroup);

        auto* modelBtnRow = new QHBoxLayout;
        auto* fetchBtn = new QPushButton("Fetch Installed Models", ollamaPage);
        fetchBtn->setToolTip("Get list of models installed on the local Ollama instance");
        connect(fetchBtn, &QPushButton::clicked, this, &PreferencesDialog::onOllamaFetchModels);
        modelBtnRow->addWidget(fetchBtn);

        auto* installBtn = new QPushButton("Install Model...", ollamaPage);
        installBtn->setToolTip("Pull a new model from the Ollama model library");
        connect(installBtn, &QPushButton::clicked, this, &PreferencesDialog::onOllamaInstallModel);
        modelBtnRow->addWidget(installBtn);
        modelBtnRow->addStretch();
        modelLayout->addLayout(modelBtnRow);

        m_ollamaModelList = new QListWidget(ollamaPage);
        m_ollamaModelList->setToolTip("Installed models on your local Ollama instance");
        m_ollamaModelList->setMaximumHeight(120);
        modelLayout->addWidget(m_ollamaModelList);

        ollamaLayout->addWidget(modelGroup);

        // Log output
        auto* logGroup = new QGroupBox("Log", ollamaPage);
        auto* logLayout = new QVBoxLayout(logGroup);
        m_ollamaLog = new QTextEdit(ollamaPage);
        m_ollamaLog->setReadOnly(true);
        m_ollamaLog->setMaximumHeight(100);
        m_ollamaLog->setPlaceholderText("Ollama connection and diagnostic output will appear here...");
        logLayout->addWidget(m_ollamaLog);
        ollamaLayout->addWidget(logGroup);

        ollamaLayout->addStretch();
        m_aiStack->addWidget(ollamaPage);
    }

    // ── Page 1: Claude ────────────────────────────────────────────
    m_aiStack->addWidget(buildApiProviderPanel("Claude", m_claudeApiKey, m_claudeModel,
        {"claude-opus-4-6", "claude-sonnet-4-6", "claude-haiku-4-5-20251001"}, this));

    // ── Page 2: Grok ──────────────────────────────────────────────
    m_aiStack->addWidget(buildApiProviderPanel("Grok", m_grokApiKey, m_grokModel,
        {"grok-3", "grok-3-mini", "grok-2"}, this));

    // ── Page 3: Gemini ────────────────────────────────────────────
    m_aiStack->addWidget(buildApiProviderPanel("Gemini", m_geminiApiKey, m_geminiModel,
        {"gemini-2.5-pro", "gemini-2.5-flash", "gemini-2.0-flash"}, this));

    // ── Page 4: ChatGPT ───────────────────────────────────────────
    m_aiStack->addWidget(buildApiProviderPanel("ChatGPT", m_chatgptApiKey, m_chatgptModel,
        {"gpt-4o", "gpt-4o-mini", "o3", "o4-mini"}, this));

    // ── Page 5: Venice ────────────────────────────────────────────
    m_aiStack->addWidget(buildApiProviderPanel("Venice", m_veniceApiKey, m_veniceModel,
        {"llama-3.3-70b", "deepseek-r1-671b", "qwen-2.5-vl"}, this));

    layout->addWidget(m_aiStack);

    // Connect provider combo → stack page
    connect(m_aiProviderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PreferencesDialog::onAiProviderChanged);

    // Load saved values
    QSettings s("Mcaster1", "Mcaster1Studio");
    const QString savedProvider = s.value("ai/provider", "ollama").toString();
    int provIdx = m_aiProviderCombo->findData(savedProvider);
    if (provIdx >= 0) m_aiProviderCombo->setCurrentIndex(provIdx);
    m_aiStack->setCurrentIndex(m_aiProviderCombo->currentIndex());

    m_ollamaUrl->setText(s.value("ai/ollama/url", "http://localhost:11434").toString());
    const QString ollamaModel = s.value("ai/ollama/model", "").toString();
    if (!ollamaModel.isEmpty()) m_ollamaActiveModel->setEditText(ollamaModel);

    m_claudeApiKey->setText(s.value("ai/claude/apiKey", "").toString());
    const QString claudeModel = s.value("ai/claude/model", "claude-opus-4-6").toString();
    int ci = m_claudeModel->findText(claudeModel);
    if (ci >= 0) m_claudeModel->setCurrentIndex(ci); else m_claudeModel->setEditText(claudeModel);

    m_grokApiKey->setText(s.value("ai/grok/apiKey", "").toString());
    const QString grokModel = s.value("ai/grok/model", "grok-3").toString();
    ci = m_grokModel->findText(grokModel);
    if (ci >= 0) m_grokModel->setCurrentIndex(ci); else m_grokModel->setEditText(grokModel);

    m_geminiApiKey->setText(s.value("ai/gemini/apiKey", "").toString());
    const QString geminiModel = s.value("ai/gemini/model", "gemini-2.5-pro").toString();
    ci = m_geminiModel->findText(geminiModel);
    if (ci >= 0) m_geminiModel->setCurrentIndex(ci); else m_geminiModel->setEditText(geminiModel);

    m_chatgptApiKey->setText(s.value("ai/chatgpt/apiKey", "").toString());
    const QString chatgptModel = s.value("ai/chatgpt/model", "gpt-4o").toString();
    ci = m_chatgptModel->findText(chatgptModel);
    if (ci >= 0) m_chatgptModel->setCurrentIndex(ci); else m_chatgptModel->setEditText(chatgptModel);

    m_veniceApiKey->setText(s.value("ai/venice/apiKey", "").toString());
    const QString veniceModel = s.value("ai/venice/model", "llama-3.3-70b").toString();
    ci = m_veniceModel->findText(veniceModel);
    if (ci >= 0) m_veniceModel->setCurrentIndex(ci); else m_veniceModel->setEditText(veniceModel);

    return page;
}

void PreferencesDialog::onAiProviderChanged(int index) {
    if (m_aiStack) m_aiStack->setCurrentIndex(index);
}

void PreferencesDialog::onOllamaFetchModels() {
    const QString baseUrl = m_ollamaUrl->text().trimmed();
    if (baseUrl.isEmpty()) return;

    m_ollamaLog->append("[INFO] Fetching installed models from " + baseUrl + "...");

    QNetworkRequest req{QUrl(baseUrl + "/api/tags")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = m_aiNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_ollamaLog->append("[ERROR] " + reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto models = doc.object()["models"].toArray();

        m_ollamaModelList->clear();
        m_ollamaActiveModel->clear();

        for (const auto& m : models) {
            const auto obj = m.toObject();
            const QString name = obj["name"].toString();
            const qint64 size  = obj["size"].toInteger();
            const QString sizeStr = QString::number(size / (1024 * 1024)) + " MB";
            m_ollamaModelList->addItem(name + "  (" + sizeStr + ")");
            m_ollamaActiveModel->addItem(name);
        }

        m_ollamaLog->append(QString("[OK] Found %1 installed model(s).").arg(models.size()));
    });
}

void PreferencesDialog::onOllamaInstallModel() {
    const QString baseUrl = m_ollamaUrl->text().trimmed();
    if (baseUrl.isEmpty()) return;

    // Simple input — user types the model name (e.g. "llama3.2", "mistral")
    bool ok = false;
    const QString modelName = QInputDialog::getText(this, "Install Ollama Model",
        "Model name (e.g. llama3.2, mistral, phi3):",
        QLineEdit::Normal, "", &ok);
    if (!ok || modelName.trimmed().isEmpty()) return;

    m_ollamaLog->append("[INFO] Pulling model: " + modelName + " (this may take a while)...");

    QJsonObject body;
    body["name"] = modelName.trimmed();

    QNetworkRequest req{QUrl(baseUrl + "/api/pull")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto* reply = m_aiNam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, modelName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_ollamaLog->append("[ERROR] Pull failed: " + reply->errorString());
            return;
        }
        m_ollamaLog->append("[OK] Model '" + modelName + "' pull complete. Refreshing model list...");
        onOllamaFetchModels();
    });
}

void PreferencesDialog::onOllamaTestConnection() {
    const QString baseUrl = m_ollamaUrl->text().trimmed();
    if (baseUrl.isEmpty()) return;

    m_ollamaLog->append("[INFO] Testing connection to " + baseUrl + "...");

    QNetworkRequest req{QUrl(baseUrl + "/api/version")};
    auto* reply = m_aiNam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_ollamaLog->append("[ERROR] Connection failed: " + reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const QString version = doc.object()["version"].toString();
        m_ollamaLog->append("[OK] Connected to Ollama v" + version);
    });
}

void PreferencesDialog::onOllamaDiagnostic() {
    const QString baseUrl = m_ollamaUrl->text().trimmed();
    const QString model   = m_ollamaActiveModel->currentText().trimmed();
    if (baseUrl.isEmpty() || model.isEmpty()) {
        m_ollamaLog->append("[WARN] Please set the server URL and select a model first.");
        return;
    }

    m_ollamaLog->append("[INFO] Running diagnostic with model: " + model + "...");

    QJsonObject body;
    body["model"]  = model;
    body["prompt"] = "Respond with exactly: 'Mcaster1Studio AI integration OK'. "
                     "Do not add any other text.";
    body["stream"] = false;

    QNetworkRequest req{QUrl(baseUrl + "/api/generate")};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setTransferTimeout(30000);

    auto* reply = m_aiNam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, model]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_ollamaLog->append("[ERROR] Diagnostic failed: " + reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const QString response = doc.object()["response"].toString().trimmed();
        const double totalDuration = doc.object()["total_duration"].toDouble() / 1e9;

        m_ollamaLog->append("[OK] Model '" + model + "' responded in " +
                            QString::number(totalDuration, 'f', 2) + "s:");
        m_ollamaLog->append("  > " + response);

        if (response.contains("OK", Qt::CaseInsensitive))
            m_ollamaLog->append("[PASS] Diagnostic check passed!");
        else
            m_ollamaLog->append("[WARN] Unexpected response — model may need different prompting.");
    });
}

// ─── Device enumeration ──────────────────────────────────────────────────────
void PreferencesDialog::populateDevices() {
    m_inputCombo->clear();
    m_outputCombo->clear();
    m_cueCombo->clear();
    m_inputIndices.clear();
    m_outputIndices.clear();
    m_cueIndices.clear();

    if (!m_engine) return;

    m_inputCombo->addItem("(None)", -1);
    m_inputIndices.append(-1);

    const auto inputs = m_engine->inputDevices();
    for (const auto& d : inputs) {
        QString name = QString("[%1] %2").arg(d.hostApi).arg(d.name);
        if (d.isDefault) name += " *";
        m_inputCombo->addItem(name, d.index);
        m_inputIndices.append(d.index);
    }

    m_outputCombo->addItem("(None)", -1);
    m_outputIndices.append(-1);
    m_cueCombo->addItem("(None — CUE disabled)", -1);
    m_cueIndices.append(-1);

    const auto outputs = m_engine->outputDevices();
    for (const auto& d : outputs) {
        QString name = QString("[%1] %2").arg(d.hostApi).arg(d.name);
        if (d.isDefault) name += " *";
        m_outputCombo->addItem(name, d.index);
        m_outputIndices.append(d.index);
        m_cueCombo->addItem(name, d.index);
        m_cueIndices.append(d.index);
    }
}

void PreferencesDialog::onRefreshDevices() {
    populateDevices();
}

void PreferencesDialog::onThemeChanged(int index) {
    const QString name = m_themeCombo->itemData(index).toString();
    ThemeManager::instance()->applyTheme(ThemeManager::themeFromName(name));
}

void PreferencesDialog::onAccepted() {
    QSettings s("Mcaster1", "Mcaster1Studio");

    // Audio
    s.setValue("audio/inputDevice",  selectedInputDevice());
    s.setValue("audio/outputDevice", selectedOutputDevice());
    s.setValue("audio/cueDevice",    selectedCueDevice());
    s.setValue("audio/sampleRate",   selectedSampleRate());
    s.setValue("audio/bufferSize",   selectedBufferSize());

    // Theme
    ThemeManager::instance()->saveToSettings(s);

    // Database
    const QString backend = m_dbBackendCombo->currentData().toString();
    s.setValue("database/backend", backend);
    s.setValue("database/sqlite/path",     m_sqlitePath->text());
    s.setValue("database/mysql/host",      m_mysqlHost->text());
    s.setValue("database/mysql/port",      m_mysqlPort->value());
    s.setValue("database/mysql/user",      m_mysqlUser->text());
    s.setValue("database/mysql/password",  m_mysqlPass->text());
    s.setValue("database/mysql/database",  m_mysqlDb->text());

    // Metrics
    s.setValue("metrics/enabled",           m_metricsEnabled->isChecked());
    s.setValue("metrics/port",              m_metricsPort->value());
    s.setValue("metrics/grafana/enabled",   m_grafanaEnabled->isChecked());
    s.setValue("metrics/grafana/endpoint",  m_grafanaEndpoint->text());
    s.setValue("metrics/grafana/interval",  m_grafanaInterval->value());
    s.setValue("metrics/grafana/apiKey",    m_grafanaApiKey->text());

    // AI Integration
    s.setValue("ai/provider",              m_aiProviderCombo->currentData().toString());
    s.setValue("ai/ollama/url",            m_ollamaUrl->text());
    s.setValue("ai/ollama/model",          m_ollamaActiveModel->currentText());
    s.setValue("ai/claude/apiKey",         m_claudeApiKey->text());
    s.setValue("ai/claude/model",          m_claudeModel->currentText());
    s.setValue("ai/grok/apiKey",           m_grokApiKey->text());
    s.setValue("ai/grok/model",            m_grokModel->currentText());
    s.setValue("ai/gemini/apiKey",         m_geminiApiKey->text());
    s.setValue("ai/gemini/model",          m_geminiModel->currentText());
    s.setValue("ai/chatgpt/apiKey",        m_chatgptApiKey->text());
    s.setValue("ai/chatgpt/model",         m_chatgptModel->currentText());
    s.setValue("ai/venice/apiKey",         m_veniceApiKey->text());
    s.setValue("ai/venice/model",          m_veniceModel->currentText());

    accept();
}

int PreferencesDialog::selectedInputDevice()  const {
    if (m_inputCombo->currentIndex() < 0) return -1;
    return m_inputIndices.value(m_inputCombo->currentIndex(), -1);
}
int PreferencesDialog::selectedOutputDevice() const {
    if (m_outputCombo->currentIndex() < 0) return -1;
    return m_outputIndices.value(m_outputCombo->currentIndex(), -1);
}
int PreferencesDialog::selectedCueDevice() const {
    if (m_cueCombo->currentIndex() < 0) return -1;
    return m_cueIndices.value(m_cueCombo->currentIndex(), -1);
}
int PreferencesDialog::selectedSampleRate()   const {
    return m_sampleRateCombo->currentData().toInt();
}
int PreferencesDialog::selectedBufferSize()   const {
    return m_bufferSizeCombo->currentData().toInt();
}

// ─── DB Servers Page (K.1 / K.3) ────────────────────────────────────────────
QWidget* PreferencesDialog::buildDbServersPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // Toolbar
    auto* toolbar = new QHBoxLayout;
    auto* addBtn     = new QPushButton("+ Add Server", page);
    auto* editBtn    = new QPushButton("Edit", page);
    auto* removeBtn  = new QPushButton("Remove", page);
    auto* defaultBtn = new QPushButton("Set Default", page);
    auto* testBtn    = new QPushButton("Test", page);
    addBtn->setToolTip("Add a new database server");
    editBtn->setToolTip("Edit the selected server");
    removeBtn->setToolTip("Remove the selected server");
    defaultBtn->setToolTip("Set the selected server as the default for new surfaces");
    testBtn->setToolTip("Test connection to the selected server");
    toolbar->addWidget(addBtn);
    toolbar->addWidget(editBtn);
    toolbar->addWidget(removeBtn);
    toolbar->addWidget(defaultBtn);
    toolbar->addWidget(testBtn);
    toolbar->addStretch();
    layout->addLayout(toolbar);

    // Server table
    m_dbServerTable = new QTableWidget(0, 5, page);
    m_dbServerTable->setHorizontalHeaderLabels({"", "Name", "Backend", "Host", "Status"});
    m_dbServerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_dbServerTable->setColumnWidth(0, 30);
    m_dbServerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_dbServerTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_dbServerTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_dbServerTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_dbServerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dbServerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dbServerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dbServerTable->verticalHeader()->hide();
    layout->addWidget(m_dbServerTable, 1);

    refreshDbServerTable();

    // ── Add button ─────────────────────────────────────────────────────────
    connect(addBtn, &QPushButton::clicked, this, [this]() {
        DbServerDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            M1::DbServerRegistry::instance().addServer(dlg.entry());
            refreshDbServerTable();
        }
    });

    // ── Edit button ────────────────────────────────────────────────────────
    connect(editBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_dbServerTable->currentRow();
        if (row < 0) return;
        const auto servers = M1::DbServerRegistry::instance().servers();
        if (row >= servers.size()) return;
        DbServerDialog dlg(this);
        dlg.setEntry(servers[row]);
        if (dlg.exec() == QDialog::Accepted) {
            M1::DbServerRegistry::instance().updateServer(dlg.entry());
            refreshDbServerTable();
        }
    });

    // ── Remove button ──────────────────────────────────────────────────────
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_dbServerTable->currentRow();
        if (row < 0) return;
        const auto servers = M1::DbServerRegistry::instance().servers();
        if (row >= servers.size()) return;
        if (servers.size() <= 1) return;  // Can't remove last server
        M1::DbServerRegistry::instance().removeServer(servers[row].id);
        refreshDbServerTable();
    });

    // ── Set Default button ─────────────────────────────────────────────────
    connect(defaultBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_dbServerTable->currentRow();
        if (row < 0) return;
        const auto servers = M1::DbServerRegistry::instance().servers();
        if (row >= servers.size()) return;
        M1::DbServerRegistry::instance().setDefaultServerId(servers[row].id);
        refreshDbServerTable();
    });

    // ── Test button ────────────────────────────────────────────────────────
    connect(testBtn, &QPushButton::clicked, this, [this]() {
        const int row = m_dbServerTable->currentRow();
        if (row < 0) return;
        const auto servers = M1::DbServerRegistry::instance().servers();
        if (row >= servers.size()) return;
        const QString err = M1::DbServerRegistry::testConnection(servers[row]);
        auto* item = m_dbServerTable->item(row, 4);
        if (!item) { item = new QTableWidgetItem; m_dbServerTable->setItem(row, 4, item); }
        if (err.isEmpty()) {
            item->setText("OK");
            item->setForeground(QColor("#22c55e"));
        } else {
            item->setText(err);
            item->setForeground(QColor("#ef4444"));
        }
    });

    return page;
}

void PreferencesDialog::refreshDbServerTable() {
    if (!m_dbServerTable) return;
    const auto& reg = M1::DbServerRegistry::instance();
    const auto servers = reg.servers();
    m_dbServerTable->setRowCount(servers.size());

    for (int i = 0; i < servers.size(); ++i) {
        const auto& s = servers[i];
        const bool isDefault = (s.id == reg.defaultServerId());

        auto* starItem = new QTableWidgetItem(isDefault ? "\u2605" : "");
        starItem->setTextAlignment(Qt::AlignCenter);
        starItem->setToolTip(isDefault ? "Default server" : "");
        m_dbServerTable->setItem(i, 0, starItem);

        m_dbServerTable->setItem(i, 1, new QTableWidgetItem(s.displayName));
        m_dbServerTable->setItem(i, 2, new QTableWidgetItem(
            s.isSQLite() ? "SQLite" : "MySQL"));
        m_dbServerTable->setItem(i, 3, new QTableWidgetItem(
            s.isSQLite() ? "(embedded)" : s.host));
        m_dbServerTable->setItem(i, 4, new QTableWidgetItem("--"));
    }
}

// ─── Displays & Capture Page (K.5) ──────────────────────────────────────────
QWidget* PreferencesDialog::buildDisplaysPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // ── Monitors section ───────────────────────────────────────────────────
    auto* monLabel = new QLabel("Monitors", page);
    monLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(monLabel);

    auto* monToolbar = new QHBoxLayout;
    monToolbar->addStretch();
    auto* monRefresh = new QPushButton("Refresh", page);
    monRefresh->setToolTip("Re-detect attached monitors");
    monToolbar->addWidget(monRefresh);
    layout->addLayout(monToolbar);

    m_monitorTable = new QTableWidget(0, 6, page);
    m_monitorTable->setHorizontalHeaderLabels(
        {"#", "Name", "Resolution", "Hz", "DPI", "GPU"});
    m_monitorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_monitorTable->setColumnWidth(0, 30);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_monitorTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_monitorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_monitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_monitorTable->verticalHeader()->hide();
    layout->addWidget(m_monitorTable);

    // ── Capture Devices section ────────────────────────────────────────────
    auto* capLabel = new QLabel("Video Capture Devices", page);
    capLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(capLabel);

    auto* capToolbar = new QHBoxLayout;
    capToolbar->addStretch();
    auto* capRefresh = new QPushButton("Refresh", page);
    capRefresh->setToolTip("Re-detect video capture devices");
    capToolbar->addWidget(capRefresh);
    layout->addLayout(capToolbar);

    m_captureTable = new QTableWidget(0, 4, page);
    m_captureTable->setHorizontalHeaderLabels(
        {"Device", "Type", "Max FPS", "Virtual"});
    m_captureTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_captureTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_captureTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_captureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_captureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_captureTable->verticalHeader()->hide();
    layout->addWidget(m_captureTable);

    // ── GPU Info ───────────────────────────────────────────────────────────
    auto* gpuLabel = new QLabel("GPU Information", page);
    gpuLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(gpuLabel);

    m_gpuInfoLabel = new QLabel(page);
    m_gpuInfoLabel->setWordWrap(true);
    layout->addWidget(m_gpuInfoLabel);

    layout->addStretch();

    // Populate
    refreshMonitorTable();
    refreshCaptureTable();

    // Refresh buttons
    connect(monRefresh, &QPushButton::clicked, this, [this]() {
        M1::MonitorManager::instance().refresh();
        refreshMonitorTable();
        refreshCaptureTable();
    });
    connect(capRefresh, &QPushButton::clicked, this, [this]() {
        M1::MonitorManager::instance().refreshCaptureDevices();
        refreshCaptureTable();
    });

    return page;
}

void PreferencesDialog::refreshMonitorTable() {
    if (!m_monitorTable) return;
    const auto monitors = M1::MonitorManager::instance().monitors();
    m_monitorTable->setRowCount(monitors.size());

    for (int i = 0; i < monitors.size(); ++i) {
        const auto& m = monitors[i];
        auto* idxItem = new QTableWidgetItem(
            QString::number(m.index + 1) + (m.isPrimary ? " \u2605" : ""));
        idxItem->setTextAlignment(Qt::AlignCenter);
        m_monitorTable->setItem(i, 0, idxItem);
        m_monitorTable->setItem(i, 1, new QTableWidgetItem(m.name));
        m_monitorTable->setItem(i, 2, new QTableWidgetItem(
            QString("%1x%2").arg(m.resolution.width()).arg(m.resolution.height())));
        m_monitorTable->setItem(i, 3, new QTableWidgetItem(
            QString::number(m.refreshRate, 'f', 0)));
        m_monitorTable->setItem(i, 4, new QTableWidgetItem(
            QString::number(m.dpi, 'f', 0)));
        m_monitorTable->setItem(i, 5, new QTableWidgetItem(m.gpuName));
    }

    // GPU info
    if (m_gpuInfoLabel) {
        const auto gpus = M1::MonitorManager::instance().gpus();
        QStringList lines;
        for (const auto& gpu : gpus) {
            lines << QString("%1 \u2014 %2 MB VRAM")
                      .arg(gpu.name)
                      .arg(gpu.dedicatedVramBytes / (1024 * 1024));
        }
        m_gpuInfoLabel->setText(lines.isEmpty() ? "No GPU detected" : lines.join("\n"));
    }
}

void PreferencesDialog::refreshCaptureTable() {
    if (!m_captureTable) return;
    const auto devices = M1::MonitorManager::instance().captureDevices();
    m_captureTable->setRowCount(devices.size());

    for (int i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];
        m_captureTable->setItem(i, 0, new QTableWidgetItem(d.displayName));

        QString typeStr;
        switch (d.type) {
        case M1::CaptureDeviceInfo::Type::Webcam:        typeStr = "Webcam"; break;
        case M1::CaptureDeviceInfo::Type::CaptureCard:   typeStr = "Capture Card"; break;
        case M1::CaptureDeviceInfo::Type::ScreenCapture:  typeStr = "Screen Capture"; break;
        case M1::CaptureDeviceInfo::Type::VirtualCamera:  typeStr = "Virtual Camera"; break;
        }
        m_captureTable->setItem(i, 1, new QTableWidgetItem(typeStr));
        m_captureTable->setItem(i, 2, new QTableWidgetItem(
            QString::number(d.maxFps, 'f', 0)));
        auto* virtItem = new QTableWidgetItem(d.isVirtualCamera ? "\u2713" : "");
        virtItem->setTextAlignment(Qt::AlignCenter);
        m_captureTable->setItem(i, 3, virtItem);
    }
}
