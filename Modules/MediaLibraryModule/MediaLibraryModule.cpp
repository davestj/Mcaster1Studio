#include "MediaLibraryModule.h"
#include "IDatabase.h"
#include "SqliteManager.h"
#include "DatabaseManager.h"
#include "ScanWorker.h"
#include "LibraryModel.h"
#include "LibraryWidget.h"
#include "MusicBrainzLookup.h"
#include "IPlugin.h"
#include <QSettings>
#include <QDebug>

namespace M1 {

MediaLibraryModule::MediaLibraryModule(QObject* parent)
    : IModule(parent)
    , m_scanner(new ScanWorker(this))
    , m_model(new LibraryModel(this))
    , m_mb(new MusicBrainzLookup(this))
{
    createDatabase();
}

MediaLibraryModule::~MediaLibraryModule() {
    shutdown();
    delete m_db;
}

// ─── Database factory ─────────────────────────────────────────────────────────
void MediaLibraryModule::createDatabase() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    const QString backend = s.value("database/backend", "sqlite").toString();

    if (backend == "mysql") {
        auto* mysql = new DatabaseManager;
        DatabaseManager::Config cfg;
        cfg.host     = s.value("database/mysql/host",     "127.0.0.1").toString();
        cfg.port     = s.value("database/mysql/port",     3306).toInt();
        cfg.user     = s.value("database/mysql/user",     "root").toString();
        cfg.password = s.value("database/mysql/password",  "").toString();
        cfg.database = s.value("database/mysql/database", "mcaster1studio").toString();
        if (!mysql->connect(cfg)) {
            qWarning() << "[MediaLibraryModule] MySQL connect failed, falling back to SQLite:"
                       << mysql->lastError();
            delete mysql;
            auto* sqlite = new SqliteManager;
            sqlite->connect();
            m_db = sqlite;
        } else {
            m_db = mysql;
        }
    } else {
        auto* sqlite = new SqliteManager;
        const QString dbPath = s.value("database/sqlite/path", "").toString();
        sqlite->connect(dbPath);
        m_db = sqlite;
    }

    if (m_db->isConnected()) {
        // Load existing items into the in-memory model
        const auto items = m_db->loadAll();
        for (const auto& item : items)
            m_model->addItem(item);
        qInfo() << "[MediaLibraryModule] Loaded" << items.size()
                << "items from" << m_db->backendName();
    }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────
void MediaLibraryModule::initialize() {
    connect(m_scanner, &ScanWorker::itemScanned,
            this, &MediaLibraryModule::onItemScanned);
    connect(m_mb, &MusicBrainzLookup::lookupComplete,
            this, &MediaLibraryModule::onMbLookupComplete);
    connect(m_mb, &MusicBrainzLookup::lookupFailed, this, [this](const QString& err) {
        emit moduleError("MusicBrainz: " + err);
    });
    qInfo() << "[MediaLibraryModule] Initialized.";
}

void MediaLibraryModule::shutdown() {
    m_scanner->cancel();
    m_scanner->wait(3000);
    m_db->disconnect();
    qInfo() << "[MediaLibraryModule] Shutdown.";
}

// ─── UI ───────────────────────────────────────────────────────────────────────
QWidget* MediaLibraryModule::createWidget(QWidget* parent) {
    m_widget = new LibraryWidget(m_model, m_db, m_scanner, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });

    // Library → Module signals
    connect(m_widget, &LibraryWidget::scanDirectoryRequested,
            this, &MediaLibraryModule::startScan);
    connect(m_widget, &LibraryWidget::requestLoadMedia,
            this, &IModule::requestLoadMedia);
    connect(m_widget, &LibraryWidget::mbLookupRequested,
            m_mb, &MusicBrainzLookup::lookup);

    // Scanner → Widget progress
    connect(m_scanner, &ScanWorker::scanStarted,
            m_widget, &LibraryWidget::onScanStarted);
    connect(m_scanner, &ScanWorker::itemScanned,
            m_widget, &LibraryWidget::onItemScanned);
    connect(m_scanner, &ScanWorker::scanProgress,
            m_widget, &LibraryWidget::onScanProgress);
    connect(m_scanner, &ScanWorker::scanFinished,
            m_widget, &LibraryWidget::onScanFinished);

    return m_widget;
}

// ─── Scan ─────────────────────────────────────────────────────────────────────
void MediaLibraryModule::startScan(const QStringList& dirs) {
    m_scanDirs = dirs;
    m_scanner->startScan(dirs);
    emit statusChanged(QString("Scanning %1 director%2…")
        .arg(dirs.size())
        .arg(dirs.size() == 1 ? "y" : "ies"));
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void MediaLibraryModule::onItemScanned(const M1::MediaItem& item) {
    // Persist to DB if available; in-memory model is always updated
    if (m_db->isConnected() && !m_db->pathExists(item.filePath)) {
        MediaItem mutable_item = item;
        m_db->insertItem(mutable_item);
        m_model->addItem(mutable_item);  // use the ID-assigned copy
    } else {
        m_model->addItem(item);
    }
}

void MediaLibraryModule::onMbLookupComplete(const M1::MediaItem& enriched) {
    m_model->updateItem(enriched);
    if (m_db->isConnected())
        m_db->updateItem(enriched);
    emit statusChanged(
        QString("MusicBrainz: Updated '%1'").arg(enriched.title));
}

// ─── State persistence ────────────────────────────────────────────────────────
void MediaLibraryModule::saveState(QSettings& s) {
    s.beginGroup("MediaLibraryModule");
    s.setValue("scanDirs", m_scanDirs);
    s.endGroup();
}

void MediaLibraryModule::loadState(QSettings& s) {
    s.beginGroup("MediaLibraryModule");
    m_scanDirs = s.value("scanDirs").toStringList();
    s.endGroup();

    // Re-scan previously scanned directories on load
    if (!m_scanDirs.isEmpty())
        startScan(m_scanDirs);
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_libraryInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.library",
    "Media Library",
    "1.0.0",
    "*",
    "module",
    "Mcaster1",
    "Media library: TagLib scanner, SQLite/MySQL storage, drag-to-deck, MusicBrainz"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_library_plugin_info() { return &s_libraryInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_library_create_module(IModuleHost*) {
    return new M1::MediaLibraryModule();
}
MCASTER1_PLUGIN_API void mcaster1_library_destroy_module(IModule* m) { delete m; }
}
