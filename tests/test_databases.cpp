/// test_databases.cpp — Quick integration test for all 5 database drivers.
///
/// Tests:
///   1. SQLite       — embedded, always works
///   2. MySQL        — requires running mysqld
///   3. PostgreSQL   — requires running postgres (or stubs if M1_HAS_POSTGRESQL unset)
///   4. Firebird     — stubs unless 64-bit fbclient is linked
///   5. SQL Server   — via ODBC, tests LocalDB + named instances
///
/// Build:
///   cmake --build build --config Debug --target TestDatabases
///
/// Run:
///   build/bin/Debug/TestDatabases.exe

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <cstdio>

#include "DatabaseFactory.h"
#include "DbServerEntry.h"
#include "SqlDialect.h"
#include "SqliteManager.h"
#include "DatabaseManager.h"
#include "PostgresManager.h"
#include "FirebirdManager.h"
#include "MssqlManager.h"
#include "MediaLibraryModule.h"

static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

// Use fprintf so output appears on Windows console (Qt qInfo → OutputDebugString)
static void check(bool ok, const char* name, const QString& detail = {}) {
    if (ok) {
        fprintf(stdout, "  [PASS] %s\n", name);
        ++g_pass;
    } else {
        fprintf(stdout, "  [FAIL] %s -- %s\n", name, detail.toUtf8().constData());
        ++g_fail;
    }
    fflush(stdout);
}

static void skip(const char* name, const QString& reason) {
    fprintf(stdout, "  [SKIP] %s -- %s\n", name, reason.toUtf8().constData());
    ++g_skip;
    fflush(stdout);
}

static void header(const char* msg) {
    fprintf(stdout, "\n%s\n", msg);
    fflush(stdout);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 0: Driver registration
// ═══════════════════════════════════════════════════════════════════════════
static void testRegistration() {
    header("=== Test 0: Driver Registration ===");

    // Register all drivers
    M1::MediaLibraryModule::registerDrivers();

    check(M1::DatabaseFactory::isDriverRegistered(M1::DbServerEntry::Backend::SQLite),
          "SQLite driver registered");
    check(M1::DatabaseFactory::isDriverRegistered(M1::DbServerEntry::Backend::MySQL),
          "MySQL driver registered");

    // PostgreSQL: conditional
    bool pgReg = M1::DatabaseFactory::isDriverRegistered(M1::DbServerEntry::Backend::PostgreSQL);
#ifdef M1_HAS_POSTGRESQL
    check(pgReg, "PostgreSQL driver registered");
#else
    skip("PostgreSQL driver registered", "M1_HAS_POSTGRESQL not defined");
#endif

    // Firebird: conditional
    bool fbReg = M1::DatabaseFactory::isDriverRegistered(M1::DbServerEntry::Backend::Firebird);
#ifdef M1_HAS_FIREBIRD
    check(fbReg, "Firebird driver registered");
#else
    skip("Firebird driver registered", "M1_HAS_FIREBIRD not defined (need 64-bit fbclient)");
#endif

    // MSSQL: conditional
    bool msReg = M1::DatabaseFactory::isDriverRegistered(M1::DbServerEntry::Backend::MSSQL);
#ifdef M1_HAS_MSSQL
    check(msReg, "MSSQL driver registered");
#else
    skip("MSSQL driver registered", "M1_HAS_MSSQL not defined");
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: SQL Dialects
// ═══════════════════════════════════════════════════════════════════════════
static void testDialects() {
    header("=== Test 1: SQL Dialects ===");

    auto* sqlite = M1::dialectFor(static_cast<int>(M1::DbServerEntry::Backend::SQLite));
    auto* mysql  = M1::dialectFor(static_cast<int>(M1::DbServerEntry::Backend::MySQL));
    auto* pg     = M1::dialectFor(static_cast<int>(M1::DbServerEntry::Backend::PostgreSQL));
    auto* fb     = M1::dialectFor(static_cast<int>(M1::DbServerEntry::Backend::Firebird));
    auto* mssql  = M1::dialectFor(static_cast<int>(M1::DbServerEntry::Backend::MSSQL));

    check(sqlite && sqlite->name() == "SQLite",    "SQLite dialect");
    check(mysql  && mysql->name()  == "MySQL",     "MySQL dialect");
    check(pg     && pg->name()     == "PostgreSQL", "PostgreSQL dialect");
    check(fb     && fb->name()     == "Firebird",   "Firebird dialect");
    check(mssql  && mssql->name()  == "SQL Server", "MSSQL dialect");

    // Test quoting styles
    check(sqlite->quoteId("foo") == "\"foo\"",     "SQLite quotes: \"foo\"");
    check(mysql->quoteId("foo")  == "`foo`",       "MySQL quotes: `foo`");
    check(pg->quoteId("foo")     == "\"foo\"",     "PG quotes: \"foo\"");
    check(fb->quoteId("foo")     == "\"FOO\"",     "Firebird quotes: \"FOO\" (uppercase)");
    check(mssql->quoteId("foo")  == "[foo]",       "MSSQL quotes: [foo]");

    // Test auto-increment PK
    check(sqlite->autoIncrementPK().contains("AUTOINCREMENT"), "SQLite AUTOINCREMENT");
    check(mysql->autoIncrementPK().contains("AUTO_INCREMENT"), "MySQL AUTO_INCREMENT");
    check(pg->autoIncrementPK().contains("BIGSERIAL"),         "PG BIGSERIAL");
    check(fb->autoIncrementPK().contains("IDENTITY"),          "Firebird IDENTITY");
    check(mssql->autoIncrementPK().contains("IDENTITY"),       "MSSQL IDENTITY");

    // Test LIMIT syntax
    check(sqlite->limitClause(10, 5) == "LIMIT 10 OFFSET 5",  "SQLite LIMIT/OFFSET");
    check(fb->limitClause(10, 5).contains("ROWS"),             "Firebird ROWS...TO");
    check(mssql->limitClause(10, 5).contains("OFFSET"),        "MSSQL OFFSET...FETCH");

    // Test bool types
    check(sqlite->boolType() == "INTEGER",   "SQLite bool=INTEGER");
    check(mysql->boolType()  == "TINYINT(1)","MySQL bool=TINYINT(1)");
    check(pg->boolType()     == "BOOLEAN",   "PG bool=BOOLEAN");
    check(fb->boolType()     == "SMALLINT",  "Firebird bool=SMALLINT");
    check(mssql->boolType()  == "BIT",       "MSSQL bool=BIT");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: SQLite — full CRUD
// ═══════════════════════════════════════════════════════════════════════════
static void testSqlite() {
    header("=== Test 2: SQLite ===");

    M1::SqliteManager db;
    const QString testPath = QDir::temp().filePath("mcaster1_test_db.sqlite");
    QFile::remove(testPath);  // Clean slate

    bool ok = db.connect(testPath);
    check(ok, "SQLite connect", db.lastError());
    if (!ok) return;

    check(db.isConnected(), "SQLite isConnected");
    check(db.backendName() == "SQLite", "SQLite backendName");

    // Schema
    ok = db.createSchema();
    check(ok, "SQLite createSchema", db.lastError());

    // Table names
    auto tables = db.tableNames();
    check(tables.contains("media_items"), "SQLite table 'media_items' exists");

    // Insert
    M1::MediaItem item;
    item.filePath = "C:/test/song.mp3";
    item.title    = "Test Song";
    item.artist   = "Test Artist";
    item.album    = "Test Album";
    item.genre    = "Rock";
    item.bpm      = 120.5;
    item.durationMs = 210000;
    item.sampleRate = 44100;
    item.channels   = 2;

    ok = db.insertItem(item);
    check(ok, "SQLite insertItem", db.lastError());
    check(item.id > 0, "SQLite insert returned ID",
          QString("id=%1").arg(item.id));

    // Path exists
    check(db.pathExists("C:/test/song.mp3"), "SQLite pathExists (true)");
    check(!db.pathExists("C:/nonexistent.mp3"), "SQLite pathExists (false)");

    // Load all
    auto loaded = db.loadAll();
    check(loaded.size() == 1, "SQLite loadAll returns 1 item",
          QString("got %1").arg(loaded.size()));
    if (!loaded.isEmpty()) {
        check(loaded[0].title == "Test Song", "SQLite loaded title matches");
        check(loaded[0].artist == "Test Artist", "SQLite loaded artist matches");
        check(qAbs(loaded[0].bpm - 120.5) < 0.01, "SQLite loaded bpm matches");
    }

    // Update
    item.title = "Updated Song";
    item.rating = 5;
    ok = db.updateItem(item);
    check(ok, "SQLite updateItem", db.lastError());

    // Verify update
    loaded = db.loadAll();
    if (!loaded.isEmpty())
        check(loaded[0].title == "Updated Song", "SQLite update persisted");

    // Delete
    ok = db.deleteItem(item.id);
    check(ok, "SQLite deleteItem", db.lastError());
    loaded = db.loadAll();
    check(loaded.isEmpty(), "SQLite delete confirmed empty");

    // Execute raw query
    db.insertItem(item);  // Re-insert for query test
    auto rows = db.executeQuery("SELECT COUNT(*) FROM media_items");
    check(!rows.isEmpty() && rows[0][0].toInt() == 1,
          "SQLite executeQuery COUNT=1");

    db.disconnect();
    check(!db.isConnected(), "SQLite disconnected");

    // Cleanup
    QFile::remove(testPath);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: MySQL — connect + CRUD
// ═══════════════════════════════════════════════════════════════════════════
static void testMySQL() {
    header("=== Test 3: MySQL ===");

    M1::DatabaseManager db;
    M1::DatabaseManager::Config cfg;
    cfg.host     = "127.0.0.1";
    cfg.port     = 3306;
    cfg.user     = "root";
    cfg.password = "";
    cfg.database = "mcaster1_test_db";

    bool ok = db.connect(cfg);
    if (!ok) {
        skip("MySQL tests", "Cannot connect: " + db.lastError());
        return;
    }

    check(true, "MySQL connect");
    check(db.isConnected(), "MySQL isConnected");
    check(db.backendName() == "MySQL", "MySQL backendName");

    ok = db.createSchema();
    check(ok, "MySQL createSchema", db.lastError());

    auto tables = db.tableNames();
    check(!tables.isEmpty(), "MySQL tableNames non-empty",
          QString("%1 tables").arg(tables.size()));

    // Insert
    M1::MediaItem item;
    item.filePath = "C:/test/mysql_song.mp3";
    item.title    = "MySQL Test";
    item.artist   = "MySQL Artist";
    item.album    = "MySQL Album";
    item.bpm      = 130.0;
    item.durationMs = 180000;
    item.sampleRate = 48000;
    item.channels   = 2;

    ok = db.insertItem(item);
    check(ok, "MySQL insertItem", db.lastError());
    check(item.id > 0, "MySQL insert returned ID",
          QString("id=%1").arg(item.id));

    check(db.pathExists("C:/test/mysql_song.mp3"), "MySQL pathExists");

    auto loaded = db.loadAll();
    check(!loaded.isEmpty(), "MySQL loadAll non-empty");

    // Cleanup: delete our test item
    db.deleteItem(item.id);

    // Execute raw query
    auto rows = db.executeQuery("SELECT VERSION()");
    check(!rows.isEmpty(), "MySQL executeQuery VERSION()",
          rows.isEmpty() ? "" : rows[0][0].toString());

    db.disconnect();
    check(!db.isConnected(), "MySQL disconnected");

    // Drop test database
    M1::DatabaseManager cleanup;
    cfg.database = "mysql";
    if (cleanup.connect(cfg)) {
        cleanup.executeQuery("DROP DATABASE IF EXISTS mcaster1_test_db");
        cleanup.disconnect();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: PostgreSQL
// ═══════════════════════════════════════════════════════════════════════════
static void testPostgreSQL() {
    header("=== Test 4: PostgreSQL ===");

#ifndef M1_HAS_POSTGRESQL
    skip("PostgreSQL tests", "M1_HAS_POSTGRESQL not defined");
    return;
#else
    M1::PostgresManager db;
    M1::PostgresManager::Config cfg;
    cfg.host     = "127.0.0.1";
    cfg.port     = 5432;
    cfg.user     = "postgres";
    cfg.password = "postgres";
    cfg.database = "mcaster1_test_db";

    bool ok = db.connect(cfg);
    if (!ok) {
        skip("PostgreSQL tests", "Cannot connect: " + db.lastError());
        return;
    }

    check(true, "PostgreSQL connect");
    check(db.isConnected(), "PostgreSQL isConnected");
    check(db.backendName() == "PostgreSQL", "PostgreSQL backendName");

    ok = db.createSchema();
    check(ok, "PostgreSQL createSchema", db.lastError());

    auto tables = db.tableNames();
    check(tables.contains("media_items"), "PostgreSQL table exists");

    // Insert
    M1::MediaItem item;
    item.filePath = "C:/test/pg_song.mp3";
    item.title    = "PG Test";
    item.artist   = "PG Artist";
    item.bpm      = 140.0;
    item.durationMs = 200000;
    item.sampleRate = 44100;
    item.channels   = 2;

    ok = db.insertItem(item);
    check(ok, "PostgreSQL insertItem", db.lastError());
    check(item.id > 0, "PostgreSQL insert returned ID",
          QString("id=%1").arg(item.id));

    check(db.pathExists("C:/test/pg_song.mp3"), "PostgreSQL pathExists");

    db.deleteItem(item.id);

    auto rows = db.executeQuery("SELECT version()");
    check(!rows.isEmpty(), "PostgreSQL executeQuery version()",
          rows.isEmpty() ? "" : rows[0][0].toString());

    db.disconnect();
    check(!db.isConnected(), "PostgreSQL disconnected");
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Firebird
// ═══════════════════════════════════════════════════════════════════════════
static void testFirebird() {
    header("=== Test 5: Firebird ===");

#ifndef M1_HAS_FIREBIRD
    // Verify stubs work correctly
    M1::FirebirdManager db;
    M1::FirebirdManager::Config cfg;
    cfg.database = "C:/temp/test.fdb";

    bool ok = db.connect(cfg);
    check(!ok, "Firebird stub connect returns false");
    check(!db.lastError().isEmpty(), "Firebird stub sets error message",
          db.lastError());
    check(!db.isConnected(), "Firebird stub isConnected=false");
    check(db.backendName() == "Firebird", "Firebird backendName");
    check(db.loadAll().isEmpty(), "Firebird stub loadAll empty");
    check(db.tableNames().isEmpty(), "Firebird stub tableNames empty");
    skip("Firebird CRUD tests", "M1_HAS_FIREBIRD not defined (need 64-bit fbclient)");
#else
    M1::FirebirdManager db;
    M1::FirebirdManager::Config cfg;
    cfg.database = QDir::temp().filePath("mcaster1_test.fdb");
    cfg.user     = "SYSDBA";
    cfg.password = "masterkey";

    bool ok = db.connect(cfg);
    if (!ok) {
        skip("Firebird CRUD tests", "Cannot connect: " + db.lastError());
        return;
    }

    check(true, "Firebird connect");
    check(db.isConnected(), "Firebird isConnected");

    ok = db.createSchema();
    check(ok, "Firebird createSchema", db.lastError());

    auto tables = db.tableNames();
    check(!tables.isEmpty(), "Firebird tableNames non-empty");

    M1::MediaItem item;
    item.filePath = "C:/test/fb_song.mp3";
    item.title    = "Firebird Test";
    item.artist   = "Firebird Artist";
    item.bpm      = 128.0;
    item.durationMs = 190000;
    item.sampleRate = 44100;
    item.channels   = 2;

    ok = db.insertItem(item);
    check(ok, "Firebird insertItem", db.lastError());
    check(item.id > 0, "Firebird insert returned ID");

    db.deleteItem(item.id);
    db.disconnect();
    QFile::remove(cfg.database);
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: SQL Server (ODBC)
// ═══════════════════════════════════════════════════════════════════════════
static void testMSSQL() {
    header("=== Test 6: SQL Server (ODBC) ===");

#ifndef M1_HAS_MSSQL
    skip("MSSQL tests", "M1_HAS_MSSQL not defined");
    return;
#else
    M1::MssqlManager db;
    M1::MssqlManager::Config cfg;

    // Try LocalDB first (always available on dev machines with VS installed)
    cfg.host     = "(localdb)\\MSSQLLocalDB";
    cfg.port     = 0;  // LocalDB uses named pipes, no TCP port
    cfg.database = "mcaster1_test_db";
    cfg.trustedConnection = true;

    bool ok = db.connect(cfg);
    if (!ok) {
        fprintf(stdout, "  LocalDB failed, trying localhost\\SQLEXPRESS...\n"); fflush(stdout);
        // Try SQL Server Express
        cfg.host = "localhost\\SQLEXPRESS";
        cfg.port = 0;
        ok = db.connect(cfg);
    }
    if (!ok) {
        fprintf(stdout, "  Express failed, trying localhost:1433...\n"); fflush(stdout);
        // Try default instance
        cfg.host = "127.0.0.1";
        cfg.port = 1433;
        cfg.trustedConnection = false;
        cfg.user = "sa";
        cfg.password = "";
        ok = db.connect(cfg);
    }

    if (!ok) {
        skip("MSSQL CRUD tests", "Cannot connect to any SQL Server instance: " + db.lastError());
        // Still verify basics work
        check(db.backendName() == "SQL Server", "MSSQL backendName");
        return;
    }

    check(true, "MSSQL connect");
    check(db.isConnected(), "MSSQL isConnected");
    check(db.backendName() == "SQL Server", "MSSQL backendName");

    ok = db.createSchema();
    check(ok, "MSSQL createSchema", db.lastError());

    auto tables = db.tableNames();
    check(!tables.isEmpty(), "MSSQL tableNames non-empty",
          QString("%1 tables").arg(tables.size()));

    // Insert
    M1::MediaItem item;
    item.filePath = "C:\\test\\mssql_song.mp3";
    item.title    = "MSSQL Test";
    item.artist   = "MSSQL Artist";
    item.album    = "MSSQL Album";
    item.genre    = "Electronic";
    item.bpm      = 145.0;
    item.durationMs = 220000;
    item.sampleRate = 48000;
    item.channels   = 2;

    ok = db.insertItem(item);
    check(ok, "MSSQL insertItem", db.lastError());
    check(item.id > 0, "MSSQL insert returned ID",
          QString("id=%1").arg(item.id));

    // Path exists
    check(db.pathExists("C:\\test\\mssql_song.mp3"), "MSSQL pathExists (true)");
    check(!db.pathExists("C:\\nonexistent.mp3"), "MSSQL pathExists (false)");

    // Load all
    auto loaded = db.loadAll();
    check(!loaded.isEmpty(), "MSSQL loadAll non-empty");
    if (!loaded.isEmpty()) {
        // Find our item
        bool found = false;
        for (const auto& m : loaded) {
            if (m.filePath == "C:\\test\\mssql_song.mp3") {
                found = true;
                check(m.title == "MSSQL Test", "MSSQL loaded title matches");
                check(m.artist == "MSSQL Artist", "MSSQL loaded artist matches");
                check(qAbs(m.bpm - 145.0) < 0.01, "MSSQL loaded bpm matches");
                break;
            }
        }
        check(found, "MSSQL loaded item found by path");
    }

    // Update
    item.title = "Updated MSSQL Song";
    item.rating = 4;
    ok = db.updateItem(item);
    check(ok, "MSSQL updateItem", db.lastError());

    // Delete
    ok = db.deleteItem(item.id);
    check(ok, "MSSQL deleteItem", db.lastError());

    // Execute raw query
    auto rows = db.executeQuery("SELECT @@VERSION");
    check(!rows.isEmpty(), "MSSQL executeQuery @@VERSION",
          rows.isEmpty() ? "" : rows[0][0].toString().left(80));

    db.disconnect();
    check(!db.isConnected(), "MSSQL disconnected");

    // Cleanup: drop test database
    M1::MssqlManager cleanup;
    M1::MssqlManager::Config masterCfg = cfg;
    masterCfg.database = "master";
    if (cleanup.connect(masterCfg)) {
        cleanup.executeQuery("DROP DATABASE IF EXISTS [mcaster1_test_db]");
        cleanup.disconnect();
    }
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: DatabaseFactory integration
// ═══════════════════════════════════════════════════════════════════════════
static void testFactory() {
    header("=== Test 7: DatabaseFactory Integration ===");

    // Test checkDriverAvailable
    QString err;

    err = M1::DatabaseFactory::checkDriverAvailable(M1::DbServerEntry::Backend::SQLite);
    check(err.isEmpty(), "Factory: SQLite available", err);

    err = M1::DatabaseFactory::checkDriverAvailable(M1::DbServerEntry::Backend::MySQL);
    check(err.isEmpty(), "Factory: MySQL available", err);

#ifdef M1_HAS_MSSQL
    err = M1::DatabaseFactory::checkDriverAvailable(M1::DbServerEntry::Backend::MSSQL);
    check(err.isEmpty(), "Factory: MSSQL available", err);
#endif

    // Test factory create with SQLite
    M1::DbServerEntry entry;
    entry.backend = M1::DbServerEntry::Backend::SQLite;
    entry.sqlitePath = QDir::temp().filePath("mcaster1_factory_test.sqlite");
    QFile::remove(entry.sqlitePath);

    auto* db = M1::DatabaseFactory::create(entry);
    check(db != nullptr, "Factory: SQLite create succeeded");
    if (db) {
        check(db->isConnected(), "Factory: SQLite connected");
        check(db->backendName() == "SQLite", "Factory: correct backendName");
        db->disconnect();
        delete db;
    }
    QFile::remove(entry.sqlitePath);

    // Test dialect retrieval
    auto* dialect = M1::DatabaseFactory::dialect(M1::DbServerEntry::Backend::MSSQL);
    check(dialect && dialect->name() == "SQL Server", "Factory: MSSQL dialect");
}

// ═══════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - Database Driver Integration Tests\n");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testRegistration();
    testDialects();
    testSqlite();
    testMySQL();
    testPostgreSQL();
    testFirebird();
    testMSSQL();
    testFactory();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
