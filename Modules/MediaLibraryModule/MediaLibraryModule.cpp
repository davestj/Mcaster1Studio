#include "MediaLibraryModule.h"
#include "IDatabase.h"
#include "SqliteManager.h"
#include "DatabaseManager.h"
#include "PostgresManager.h"
#include "FirebirdManager.h"
#include "MssqlManager.h"
#include "DatabaseFactory.h"
#include "ScanWorker.h"
#include "LibraryModel.h"
#include "LibraryWidget.h"
#include "MusicBrainzLookup.h"
#include "AlbumArtCache.h"
#include "AiTrackIntel.h"
#include "PersonaManager.h"
#include "ScanProgressDialog.h"
#include "IPlugin.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>

namespace M1 {

MediaLibraryModule::MediaLibraryModule(QObject* parent)
    : IModule(parent)
    , m_scanner(new ScanWorker(this))
    , m_model(new LibraryModel(this))
    , m_mb(new MusicBrainzLookup(this))
{
    // Create album art cache in portable location
    const QString cacheDir = QCoreApplication::applicationDirPath() + "/cache/artwork";
    QDir().mkpath(cacheDir);
    m_artCache = new AlbumArtCache(cacheDir, this);

    // Create AI track intel engine
    m_aiIntel = new AiTrackIntel(this);

    // Set album art cache on scanner for pre-caching during scan
    m_scanner->setAlbumArtCache(m_artCache);

    registerDrivers();
    createDatabase();

    // Create PersonaManager and seed presets (if DB is SQLite)
    auto* sqliteMgr = dynamic_cast<SqliteManager*>(m_db);
    if (sqliteMgr) {
        m_personas = new PersonaManager(sqliteMgr, this);
        m_personas->seedPresets();
    }
}

// ─── Driver registration ─────────────────────────────────────────────────────
void MediaLibraryModule::registerDrivers() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    // SQLite driver
    DatabaseFactory::registerDriver(
        DbServerEntry::Backend::SQLite,
        [](const DbServerEntry& entry, const QString& /*dbName*/,
           const QString& dbPath) -> IDatabase* {
            auto* mgr = new SqliteManager;
            const QString path = dbPath.isEmpty() ? entry.sqlitePath : dbPath;
            if (!mgr->connect(path)) {
                qWarning() << "[DatabaseFactory] SQLite connect failed:" << mgr->lastError();
                delete mgr;
                return nullptr;
            }
            return mgr;
        });

    // MySQL driver
    DatabaseFactory::registerDriver(
        DbServerEntry::Backend::MySQL,
        [](const DbServerEntry& entry, const QString& dbName,
           const QString& /*dbPath*/) -> IDatabase* {
            auto* mgr = new DatabaseManager;
            DatabaseManager::Config cfg;
            cfg.host     = entry.host;
            cfg.port     = entry.port;
            cfg.user     = entry.username;
            cfg.password = entry.password;
            cfg.database = dbName.isEmpty() ? "mcaster1studio" : dbName;
            if (!mgr->connect(cfg)) {
                qWarning() << "[DatabaseFactory] MySQL connect failed:" << mgr->lastError();
                delete mgr;
                return nullptr;
            }
            return mgr;
        });

    // PostgreSQL driver (only if libpq was linked)
#ifdef M1_HAS_POSTGRESQL
    DatabaseFactory::registerDriver(
        DbServerEntry::Backend::PostgreSQL,
        [](const DbServerEntry& entry, const QString& dbName,
           const QString& /*dbPath*/) -> IDatabase* {
            auto* mgr = new PostgresManager;
            PostgresManager::Config cfg;
            cfg.host     = entry.host;
            cfg.port     = entry.port;
            cfg.user     = entry.username;
            cfg.password = entry.password;
            cfg.database = dbName.isEmpty() ? "mcaster1studio" : dbName;
            if (!mgr->connect(cfg)) {
                qWarning() << "[DatabaseFactory] PostgreSQL connect failed:" << mgr->lastError();
                delete mgr;
                return nullptr;
            }
            return mgr;
        });
#endif

    // Firebird driver (only if fbclient was linked)
#ifdef M1_HAS_FIREBIRD
    DatabaseFactory::registerDriver(
        DbServerEntry::Backend::Firebird,
        [](const DbServerEntry& entry, const QString& dbName,
           const QString& dbPath) -> IDatabase* {
            auto* mgr = new FirebirdManager;
            FirebirdManager::Config cfg;
            cfg.host     = entry.host;
            cfg.port     = entry.port;
            cfg.user     = entry.username;
            cfg.password = entry.password;
            // Firebird database: prefer firebirdPath, then dbPath, then dbName
            if (!entry.firebirdPath.isEmpty())
                cfg.database = entry.firebirdPath;
            else if (!dbPath.isEmpty())
                cfg.database = dbPath;
            else
                cfg.database = dbName.isEmpty() ? "mcaster1studio.fdb" : dbName;
            if (!mgr->connect(cfg)) {
                qWarning() << "[DatabaseFactory] Firebird connect failed:" << mgr->lastError();
                delete mgr;
                return nullptr;
            }
            return mgr;
        });
#endif

    // SQL Server driver (ODBC — always available on Windows)
#ifdef M1_HAS_MSSQL
    DatabaseFactory::registerDriver(
        DbServerEntry::Backend::MSSQL,
        [](const DbServerEntry& entry, const QString& dbName,
           const QString& /*dbPath*/) -> IDatabase* {
            auto* mgr = new MssqlManager;
            MssqlManager::Config cfg;
            cfg.host     = entry.host;
            cfg.port     = entry.port;
            cfg.user     = entry.username;
            cfg.password = entry.password;
            cfg.database = dbName.isEmpty() ? "mcaster1studio" : dbName;
            // Use Windows auth if no username provided
            cfg.trustedConnection = entry.username.isEmpty();
            if (!mgr->connect(cfg)) {
                qWarning() << "[DatabaseFactory] SQL Server connect failed:" << mgr->lastError();
                delete mgr;
                return nullptr;
            }
            return mgr;
        });
#endif
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

    // AI Track Intel signals
    connect(m_aiIntel, &AiTrackIntel::profileReady,
            this, &MediaLibraryModule::onAiProfileReady);
    connect(m_aiIntel, &AiTrackIntel::lookupFailed, this,
            [this](const QString& artist, const QString& err) {
        emit moduleError(QString("AI Lookup (%1): %2").arg(artist, err));
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

    // Pass new components to widget
    m_widget->setAlbumArtCache(m_artCache);
    m_widget->setAiTrackIntel(m_aiIntel);

    // If DB is SQLite, wire up category panel and FTS5 search
    auto* sqliteMgr = dynamic_cast<SqliteManager*>(m_db);
    if (sqliteMgr) {
        m_widget->setCategoryDatabase(sqliteMgr);
    }

    // Library -> Module signals
    connect(m_widget, &LibraryWidget::scanDirectoryRequested,
            this, &MediaLibraryModule::startScan);
    connect(m_widget, &LibraryWidget::requestLoadMedia,
            this, &IModule::requestLoadMedia);
    connect(m_widget, &LibraryWidget::requestPlayOnAuxCue,
            this, &MediaLibraryModule::requestPlayOnAuxCue);
    connect(m_widget, &LibraryWidget::mbLookupRequested,
            m_mb, &MusicBrainzLookup::lookup);

    // Scanner -> Widget progress
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

    // Show ScanProgressDialog if widget exists
    if (m_widget) {
        auto* dlg = new ScanProgressDialog(m_widget);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle("Scanning Media Library");

        // Wire scanner signals to dialog
        connect(m_scanner, &ScanWorker::scanStarted,  dlg, &ScanProgressDialog::onScanStarted);
        connect(m_scanner, &ScanWorker::scanProgress,  dlg, &ScanProgressDialog::onScanProgress);
        connect(m_scanner, &ScanWorker::scanFinished,  dlg, &ScanProgressDialog::onScanFinished);
        connect(dlg, &ScanProgressDialog::cancelRequested, m_scanner, &ScanWorker::cancel);

        dlg->show();
    }

    m_scanner->startScan(dirs);
    emit statusChanged(QString("Scanning %1 director%2\u2026")
        .arg(dirs.size())
        .arg(dirs.size() == 1 ? "y" : "ies"));
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void MediaLibraryModule::onItemScanned(const M1::MediaItem& item) {
    // Only add to DB + model if this path isn't already in the library.
    // loadAll() at startup already populated the model, so re-scanning
    // the same directory must NOT create duplicate rows.
    if (m_db->isConnected() && !m_db->pathExists(item.filePath)) {
        MediaItem mutable_item = item;
        m_db->insertItem(mutable_item);
        m_model->addItem(mutable_item);  // use the ID-assigned copy
    }
    // If path already exists, skip entirely — no duplicates
}

void MediaLibraryModule::onMbLookupComplete(const M1::MediaItem& enriched) {
    m_model->updateItem(enriched);
    if (m_db->isConnected())
        m_db->updateItem(enriched);
    emit statusChanged(
        QString("MusicBrainz: Updated '%1'").arg(enriched.title));
}

void MediaLibraryModule::onAiProfileReady(const QString& artistName,
                                          const QString& profileText,
                                          const QString& discographyJson,
                                          const QString& aiBackend,
                                          const QString& aiModel)
{
    // Save to DB via SqliteManager if available
    auto* sqliteMgr = dynamic_cast<SqliteManager*>(m_db);
    if (sqliteMgr) {
        sqliteMgr->saveArtistIntel(artistName, profileText,
                                   discographyJson, aiBackend, aiModel);
    }
    emit statusChanged(
        QString("AI Intel: Saved profile for '%1'").arg(artistName));
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

    // Re-scan on startup — but silently (no progress dialog).
    // The dialog is only shown for user-initiated scans via the Scan button.
    if (!m_scanDirs.isEmpty()) {
        m_scanner->startScan(m_scanDirs);
        emit statusChanged(QString("Scanning %1 director%2\u2026")
            .arg(m_scanDirs.size())
            .arg(m_scanDirs.size() == 1 ? "y" : "ies"));
    }
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
