/// test_core.cpp — Tests for Core library components.
///
/// Tests:
///   1. ModuleRegistry: availableModules() returns 44 entries, availableEffects() works
///   2. SqlDialect: all 5 dialects produce correct SQL
///   3. DbServerEntry: backend enum round-trip, defaultPort(), allBackends()
///   4. SurfaceConfig: save/load round-trip
///   5. IcyMetadata: toIcy1StreamTitle, toIcy2Headers, hasIcy2Data, syncStreamTitle
///
/// Build:
///   cmake --build build --config Debug --target TestCore
///
/// Run:
///   build/bin/Debug/TestCore.exe

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <cstdio>

#include "ModuleRegistry.h"
#include "SqlDialect.h"
#include "DbServerEntry.h"
#include "SurfaceConfig.h"
#include "IcyMetadata.h"

static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

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

// =============================================================================
//  Test 1: ModuleRegistry
// =============================================================================
static void testModuleRegistry() {
    header("=== Test 1: ModuleRegistry ===");

    // Init with a non-existent plugin dir (no external plugins to scan)
    M1::initModuleRegistry(QDir::temp().filePath("mcaster1_test_nonexistent_plugins"));

    auto modules = M1::availableModules();
    fprintf(stdout, "  availableModules() returned %d entries\n", modules.size());
    fflush(stdout);

    check(modules.size() >= 44,
          "availableModules() returns 44+ entries",
          QString("got %1").arg(modules.size()));

    // Verify some well-known module IDs are present
    auto hasModule = [&](const QString& id) {
        for (const auto& [moduleId, name] : modules)
            if (moduleId == id) return true;
        return false;
    };

    check(hasModule("com.mcaster1.vumeter"),    "VU Meter in registry");
    check(hasModule("com.mcaster1.deck"),       "Deck (Combined) in registry");
    check(hasModule("com.mcaster1.deck.a"),     "Deck A in registry");
    check(hasModule("com.mcaster1.deck.b"),     "Deck B in registry");
    check(hasModule("com.mcaster1.library"),    "Media Library in registry");
    check(hasModule("com.mcaster1.encoder"),    "Encoder in registry");
    check(hasModule("com.mcaster1.effects"),    "Effects Rack in registry");
    check(hasModule("com.mcaster1.metadata"),   "Metadata in registry");
    check(hasModule("com.mcaster1.playlist"),   "Playlist in registry");
    check(hasModule("com.mcaster1.queue"),      "Queue in registry");
    check(hasModule("com.mcaster1.ptt"),        "PTT in registry");
    check(hasModule("com.mcaster1.podcast"),    "Podcast in registry");
    check(hasModule("com.mcaster1.video"),      "Video in registry");
    check(hasModule("com.mcaster1.monitor"),    "Monitor in registry");
    check(hasModule("com.mcaster1.clock"),      "Clock in registry");
    check(hasModule("com.mcaster1.cartwall"),   "Cart Wall in registry");
    check(hasModule("com.mcaster1.crossfader"), "Crossfader in registry");
    check(hasModule("com.mcaster1.database"),   "Database in registry");
    check(hasModule("com.mcaster1.health"),     "System Health in registry");
    check(hasModule("com.mcaster1.auxdeck"),    "AUX Deck in registry");

    // Church modules
    check(hasModule("com.mcaster1.church.timerclock"),    "Church TimerClock in registry");
    check(hasModule("com.mcaster1.church.graphics"),      "Church Graphics in registry");
    check(hasModule("com.mcaster1.church.lyrics"),        "Church Lyrics in registry");
    check(hasModule("com.mcaster1.church.scripture"),     "Church Scripture in registry");
    check(hasModule("com.mcaster1.church.announce"),      "Church Announce in registry");
    check(hasModule("com.mcaster1.church.teleprompt"),    "Church TelePrompt in registry");
    check(hasModule("com.mcaster1.church.mediacaster"),   "Church MediaCaster in registry");
    check(hasModule("com.mcaster1.church.stagemon"),      "Church StageMon in registry");
    check(hasModule("com.mcaster1.church.audiomix"),      "Church AudioMix in registry");
    check(hasModule("com.mcaster1.church.transcriberec"), "Church TranscribeRec in registry");
    check(hasModule("com.mcaster1.church.switchcaster"),  "Church SwitchCaster in registry");
    check(hasModule("com.mcaster1.church.servicerunner"), "Church ServiceRunner in registry");

    // Podcast modules
    check(hasModule("com.mcaster1.podcast.mixer"),       "Podcast Mixer in registry");
    check(hasModule("com.mcaster1.podcast.ptt"),         "Podcast PTT in registry");
    check(hasModule("com.mcaster1.podcast.recorder"),    "Podcast Recorder in registry");
    check(hasModule("com.mcaster1.podcast.soundboard"),  "Podcast Soundboard in registry");
    check(hasModule("com.mcaster1.podcast.fx"),          "Podcast FX in registry");
    check(hasModule("com.mcaster1.podcast.editor"),      "Podcast Editor in registry");
    check(hasModule("com.mcaster1.podcast.encode"),      "Podcast Encode in registry");
    check(hasModule("com.mcaster1.podcast.transcribe"),  "Podcast Transcribe in registry");
    check(hasModule("com.mcaster1.podcast.shownotes"),   "Podcast ShowNotes in registry");
    check(hasModule("com.mcaster1.podcast.rss"),         "Podcast RSS in registry");
    check(hasModule("com.mcaster1.podcast.publisher"),   "Podcast Publisher in registry");
    check(hasModule("com.mcaster1.podcast.analytics"),   "Podcast Analytics in registry");
    check(hasModule("com.mcaster1.podcast.remote"),      "Podcast Remote in registry");

    // availableEffects() should return empty list (no external plugins loaded)
    auto effects = M1::availableEffects();
    check(effects.isEmpty(), "availableEffects() empty without plugins",
          QString("got %1").arg(effects.size()));

    M1::shutdownModuleRegistry();
}

// =============================================================================
//  Test 2: SqlDialect — all 5 dialects
// =============================================================================
static void testSqlDialects() {
    header("=== Test 2: SqlDialect ===");

    using Backend = M1::DbServerEntry::Backend;

    auto* sqlite = M1::dialectFor(static_cast<int>(Backend::SQLite));
    auto* mysql  = M1::dialectFor(static_cast<int>(Backend::MySQL));
    auto* pg     = M1::dialectFor(static_cast<int>(Backend::PostgreSQL));
    auto* fb     = M1::dialectFor(static_cast<int>(Backend::Firebird));
    auto* mssql  = M1::dialectFor(static_cast<int>(Backend::MSSQL));

    // Name checks
    check(sqlite && sqlite->name() == "SQLite",      "SQLite dialect name");
    check(mysql  && mysql->name()  == "MySQL",        "MySQL dialect name");
    check(pg     && pg->name()     == "PostgreSQL",   "PostgreSQL dialect name");
    check(fb     && fb->name()     == "Firebird",     "Firebird dialect name");
    check(mssql  && mssql->name()  == "SQL Server",   "MSSQL dialect name");

    // Identifier quoting
    check(sqlite->quoteId("tbl") == "\"tbl\"",  "SQLite quoteId");
    check(mysql->quoteId("tbl")  == "`tbl`",    "MySQL quoteId");
    check(pg->quoteId("tbl")     == "\"tbl\"",  "PG quoteId");
    check(fb->quoteId("tbl")     == "\"TBL\"",  "Firebird quoteId (uppercase)");
    check(mssql->quoteId("tbl")  == "[tbl]",    "MSSQL quoteId");

    // Auto-increment PK
    check(sqlite->autoIncrementPK().contains("AUTOINCREMENT"), "SQLite AUTOINCREMENT");
    check(mysql->autoIncrementPK().contains("AUTO_INCREMENT"), "MySQL AUTO_INCREMENT");
    check(pg->autoIncrementPK().contains("BIGSERIAL"),         "PG BIGSERIAL");
    check(fb->autoIncrementPK().contains("IDENTITY"),          "Firebird IDENTITY");
    check(mssql->autoIncrementPK().contains("IDENTITY"),       "MSSQL IDENTITY");

    // Bool types
    check(sqlite->boolType() == "INTEGER",    "SQLite boolType");
    check(mysql->boolType()  == "TINYINT(1)", "MySQL boolType");
    check(pg->boolType()     == "BOOLEAN",    "PG boolType");
    check(fb->boolType()     == "SMALLINT",   "Firebird boolType");
    check(mssql->boolType()  == "BIT",        "MSSQL boolType");

    // Datetime types
    check(sqlite->datetimeType() == "TEXT",       "SQLite datetimeType");
    check(mysql->datetimeType()  == "DATETIME",   "MySQL datetimeType");
    check(pg->datetimeType()     == "TIMESTAMP",  "PG datetimeType");
    check(fb->datetimeType()     == "TIMESTAMP",  "Firebird datetimeType");
    check(mssql->datetimeType()  == "DATETIME2",  "MSSQL datetimeType");

    // NOW() function
    check(sqlite->nowFunction() == "datetime('now')",  "SQLite nowFunction");
    check(mysql->nowFunction()  == "NOW()",            "MySQL nowFunction");
    check(pg->nowFunction()     == "NOW()",            "PG nowFunction");
    check(fb->nowFunction()     == "CURRENT_TIMESTAMP","Firebird nowFunction");
    check(mssql->nowFunction()  == "GETDATE()",        "MSSQL nowFunction");

    // LIMIT/OFFSET syntax
    check(sqlite->limitClause(10, 5) == "LIMIT 10 OFFSET 5",  "SQLite LIMIT/OFFSET");
    check(mysql->limitClause(10, 5)  == "LIMIT 10 OFFSET 5",  "MySQL LIMIT/OFFSET");
    check(pg->limitClause(10, 5)     == "LIMIT 10 OFFSET 5",  "PG LIMIT/OFFSET");
    check(fb->limitClause(10, 5).contains("ROWS"),             "Firebird ROWS syntax");
    check(mssql->limitClause(10, 5).contains("OFFSET"),        "MSSQL OFFSET/FETCH");

    // LIMIT without offset
    check(sqlite->limitClause(10) == "LIMIT 10",   "SQLite LIMIT no offset");
    check(fb->limitClause(10).contains("FIRST"),    "Firebird FIRST syntax");

    // CREATE DATABASE
    check(sqlite->createDatabaseSql("test").isEmpty(), "SQLite createDatabaseSql empty");
    check(mysql->createDatabaseSql("test").contains("CREATE DATABASE"),
          "MySQL createDatabaseSql");
    check(pg->createDatabaseSql("test").contains("CREATE DATABASE"),
          "PG createDatabaseSql");
    check(mssql->createDatabaseSql("test").contains("CREATE DATABASE"),
          "MSSQL createDatabaseSql");

    // Table options
    check(mysql->tableOptions().contains("InnoDB"),  "MySQL table options InnoDB");
    check(sqlite->tableOptions().isEmpty(),           "SQLite no table options");

    // varchar type
    check(sqlite->varcharType(255) == "VARCHAR(255)", "SQLite varcharType");
    check(fb->varcharType(40000).contains("32765"),   "Firebird varcharType clamps to 32765");

    // String escaping
    check(sqlite->escapeString("it's") == "it''s", "SQLite escapeString");
    check(mysql->quotedValue("hello") == "'hello'", "MySQL quotedValue");

    // Real types
    check(sqlite->realType() == "REAL",               "SQLite realType");
    check(mysql->realType()  == "DOUBLE",             "MySQL realType");
    check(pg->realType()     == "DOUBLE PRECISION",   "PG realType");
    check(mssql->realType()  == "FLOAT",              "MSSQL realType");

    // CREATE TABLE for media_items (just check it returns non-empty SQL)
    check(!sqlite->createMediaItemsTableSql().isEmpty(), "SQLite createMediaItemsTable");
    check(!mysql->createMediaItemsTableSql().isEmpty(),  "MySQL createMediaItemsTable");
    check(!pg->createMediaItemsTableSql().isEmpty(),     "PG createMediaItemsTable");
    check(!mssql->createMediaItemsTableSql().isEmpty(),  "MSSQL createMediaItemsTable");

    // Dialect singleton stability — calling twice returns same pointer
    auto* sqlite2 = M1::dialectFor(static_cast<int>(Backend::SQLite));
    check(sqlite == sqlite2, "dialectFor returns same singleton pointer");
}

// =============================================================================
//  Test 3: DbServerEntry
// =============================================================================
static void testDbServerEntry() {
    header("=== Test 3: DbServerEntry ===");

    using Backend = M1::DbServerEntry::Backend;

    // allBackends()
    auto backends = M1::DbServerEntry::allBackends();
    check(backends.size() == 5, "allBackends() returns 5",
          QString("got %1").arg(backends.size()));

    check(backends.contains(Backend::SQLite),     "allBackends contains SQLite");
    check(backends.contains(Backend::MySQL),      "allBackends contains MySQL");
    check(backends.contains(Backend::PostgreSQL), "allBackends contains PostgreSQL");
    check(backends.contains(Backend::Firebird),   "allBackends contains Firebird");
    check(backends.contains(Backend::MSSQL),      "allBackends contains MSSQL");

    // backendCount()
    check(M1::DbServerEntry::backendCount() == 5, "backendCount() == 5");

    // defaultPort()
    check(M1::DbServerEntry::defaultPort(Backend::MySQL)      == 3306, "MySQL default port 3306");
    check(M1::DbServerEntry::defaultPort(Backend::PostgreSQL) == 5432, "PG default port 5432");
    check(M1::DbServerEntry::defaultPort(Backend::Firebird)   == 3050, "Firebird default port 3050");
    check(M1::DbServerEntry::defaultPort(Backend::MSSQL)      == 1433, "MSSQL default port 1433");
    check(M1::DbServerEntry::defaultPort(Backend::SQLite)     == 0,    "SQLite default port 0");

    // backendKey() / backendFromKey() round-trip
    M1::DbServerEntry entry;

    entry.backend = Backend::SQLite;
    check(entry.backendKey() == "sqlite", "SQLite backendKey");
    check(M1::DbServerEntry::backendFromKey("sqlite") == Backend::SQLite, "backendFromKey(sqlite)");

    entry.backend = Backend::MySQL;
    check(entry.backendKey() == "mysql", "MySQL backendKey");
    check(M1::DbServerEntry::backendFromKey("mysql") == Backend::MySQL, "backendFromKey(mysql)");

    entry.backend = Backend::PostgreSQL;
    check(entry.backendKey() == "postgresql", "PostgreSQL backendKey");
    check(M1::DbServerEntry::backendFromKey("postgresql") == Backend::PostgreSQL, "backendFromKey(postgresql)");

    entry.backend = Backend::Firebird;
    check(entry.backendKey() == "firebird", "Firebird backendKey");
    check(M1::DbServerEntry::backendFromKey("firebird") == Backend::Firebird, "backendFromKey(firebird)");

    entry.backend = Backend::MSSQL;
    check(entry.backendKey() == "mssql", "MSSQL backendKey");
    check(M1::DbServerEntry::backendFromKey("mssql") == Backend::MSSQL, "backendFromKey(mssql)");

    // Unknown key defaults to SQLite
    check(M1::DbServerEntry::backendFromKey("unknown") == Backend::SQLite,
          "backendFromKey(unknown) defaults to SQLite");

    // backendDisplayName()
    entry.backend = Backend::SQLite;
    check(entry.backendDisplayName() == "SQLite",      "SQLite displayName");
    entry.backend = Backend::MySQL;
    check(entry.backendDisplayName() == "MySQL",       "MySQL displayName");
    entry.backend = Backend::PostgreSQL;
    check(entry.backendDisplayName() == "PostgreSQL",  "PG displayName");
    entry.backend = Backend::Firebird;
    check(entry.backendDisplayName() == "Firebird",    "Firebird displayName");
    entry.backend = Backend::MSSQL;
    check(entry.backendDisplayName() == "SQL Server",  "MSSQL displayName");

    // Boolean helpers
    entry.backend = Backend::SQLite;
    check(entry.isSQLite(),     "isSQLite() true");
    check(!entry.isNetworked(), "SQLite not networked");

    entry.backend = Backend::MySQL;
    check(entry.isMySQL(),      "isMySQL() true");
    check(entry.isNetworked(),  "MySQL is networked");

    entry.backend = Backend::PostgreSQL;
    check(entry.isPostgreSQL(), "isPostgreSQL() true");
    check(entry.isNetworked(),  "PG is networked");

    entry.backend = Backend::Firebird;
    check(entry.isFirebird(),   "isFirebird() true");

    entry.backend = Backend::MSSQL;
    check(entry.isMSSQL(),      "isMSSQL() true");
    check(entry.isNetworked(),  "MSSQL is networked");

    // newId() generates unique UUIDs
    QString id1 = M1::DbServerEntry::newId();
    QString id2 = M1::DbServerEntry::newId();
    check(!id1.isEmpty(), "newId() non-empty");
    check(id1 != id2,     "newId() generates unique IDs");
}

// =============================================================================
//  Test 4: SurfaceConfig
// =============================================================================
static void testSurfaceConfig() {
    header("=== Test 4: SurfaceConfig ===");

    // Create a SurfaceConfig
    M1::SurfaceConfig cfg;
    cfg.surfaceType = "alpha";
    cfg.surfaceName = "Surface Alpha";
    cfg.database.host = "localhost";
    cfg.database.port = 3306;
    cfg.database.database = "mcaster1_alpha";

    M1::ModuleConfig modCfg;
    modCfg.id = "com.mcaster1.vumeter";
    modCfg.enabled = true;
    modCfg.settings["mode"] = "stereo";
    cfg.modules.append(modCfg);

    M1::ModuleConfig modCfg2;
    modCfg2.id = "com.mcaster1.deck";
    modCfg2.enabled = true;
    cfg.modules.append(modCfg2);

    check(cfg.isValid(), "SurfaceConfig isValid");
    check(cfg.modules.size() == 2, "SurfaceConfig has 2 modules");

    // Save to temp file
    QTemporaryDir tmpDir;
    check(tmpDir.isValid(), "TempDir created for save/load test");
    if (!tmpDir.isValid()) return;

    QString path = tmpDir.path() + "/test_surface.yaml";
    bool saved = cfg.save(path);
    check(saved, "SurfaceConfig save succeeded", path);
    check(QFile::exists(path), "Saved file exists");

    // Load back
    M1::SurfaceConfig loaded = M1::SurfaceConfig::load(path);
    check(loaded.isValid(), "Loaded config isValid");
    check(loaded.surfaceType == "alpha",         "Loaded surfaceType matches");
    check(loaded.surfaceName == "Surface Alpha", "Loaded surfaceName matches");
    check(loaded.modules.size() == 2,            "Loaded module count matches",
          QString("got %1").arg(loaded.modules.size()));

    if (loaded.modules.size() >= 1) {
        check(loaded.modules[0].id == "com.mcaster1.vumeter",
              "Loaded module[0].id matches");
        check(loaded.modules[0].enabled == true,
              "Loaded module[0].enabled matches");
    }

    // defaultForType smoke test
    auto djDefault = M1::SurfaceConfig::defaultForType("dj");
    check(djDefault.isValid() || !djDefault.isValid(),
          "defaultForType(dj) does not crash");

    // Invalid load returns invalid config
    auto invalid = M1::SurfaceConfig::load("/nonexistent/path/nope.yaml");
    check(!invalid.isValid(), "Load from nonexistent path returns invalid config");
}

// =============================================================================
//  Test 5: IcyMetadata
// =============================================================================
static void testIcyMetadata() {
    header("=== Test 5: IcyMetadata ===");

    M1::IcyMetadata meta;

    // Empty metadata
    check(!meta.hasIcy2Data(), "Empty meta hasIcy2Data() false");
    check(meta.toIcy1StreamTitle().isEmpty(), "Empty meta toIcy1StreamTitle empty");

    // Set track info
    meta.trackTitle  = "Test Song";
    meta.trackArtist = "Test Artist";
    meta.syncStreamTitle();

    check(meta.streamTitle == "Test Artist - Test Song", "syncStreamTitle combines artist - title");
    check(meta.hasIcy2Data(), "hasIcy2Data true after setting track fields");

    // toIcy1StreamTitle()
    QString icy1 = meta.toIcy1StreamTitle();
    check(icy1.contains("Test Artist"), "toIcy1StreamTitle contains artist");
    check(icy1.contains("Test Song"),   "toIcy1StreamTitle contains title");

    // toIcy2Headers()
    auto headers = meta.toIcy2Headers();
    check(!headers.isEmpty(), "toIcy2Headers non-empty");

    // Set station info (Group 1)
    meta.stationName = "WTEST";
    meta.stationGenre = "Rock";
    headers = meta.toIcy2Headers();
    check(headers.size() >= 2, "toIcy2Headers has station fields",
          QString("got %1 headers").arg(headers.size()));

    // Set all 8 groups to verify they all serialize
    meta.showTitle = "Morning Show";
    meta.djHandle = "DJ Test";
    meta.socialTwitter = "teststation";
    meta.podcastTitle = "Test Pod";
    meta.broadcastMode = "live";
    meta.contentExplicit = "false";
    meta.contentLive = "true";

    headers = meta.toIcy2Headers();
    check(headers.size() >= 8, "toIcy2Headers has fields from multiple groups",
          QString("got %1 headers").arg(headers.size()));

    // clear()
    meta.clear();
    check(meta.streamTitle.isEmpty(),  "clear() clears streamTitle");
    check(meta.trackTitle.isEmpty(),   "clear() clears trackTitle");
    check(meta.stationName.isEmpty(),  "clear() clears stationName");
    check(meta.showTitle.isEmpty(),    "clear() clears showTitle");
    check(meta.djHandle.isEmpty(),     "clear() clears djHandle");
    check(!meta.hasIcy2Data(),         "clear() makes hasIcy2Data false");
}

// =============================================================================
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    fprintf(stdout, "========================================================\n");
    fprintf(stdout, "  Mcaster1Studio - Core Library Tests\n");
    fprintf(stdout, "========================================================\n");
    fflush(stdout);

    testModuleRegistry();
    testSqlDialects();
    testDbServerEntry();
    testSurfaceConfig();
    testIcyMetadata();

    fprintf(stdout, "\n========================================================\n");
    fprintf(stdout, "  Results: %d passed, %d failed, %d skipped\n", g_pass, g_fail, g_skip);
    fprintf(stdout, "========================================================\n\n");
    fflush(stdout);

    return g_fail > 0 ? 1 : 0;
}
