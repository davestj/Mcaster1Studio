#include "SqliteManager.h"
#include <sqlite3.h>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>

// ─── Helpers (outside namespace — sqlite3 types are C, not M1) ──────────────
static QString fromCol(sqlite3_stmt* stmt, int col) {
    const auto* text = sqlite3_column_text(stmt, col);
    return text ? QString::fromUtf8(reinterpret_cast<const char*>(text)) : QString();
}

static qint64 int64Col(sqlite3_stmt* stmt, int col) {
    return static_cast<qint64>(sqlite3_column_int64(stmt, col));
}

static double dblCol(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_double(stmt, col);
}

static int intCol(sqlite3_stmt* stmt, int col) {
    return sqlite3_column_int(stmt, col);
}

// Bind a QString as UTF-8 to a prepared statement parameter
static void bindText(sqlite3_stmt* stmt, int idx, const QString& s) {
    QByteArray utf8 = s.toUtf8();
    sqlite3_bind_text(stmt, idx, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
}

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────
SqliteManager::SqliteManager() = default;

SqliteManager::~SqliteManager() {
    disconnect();
}

// ─── Connection ───────────────────────────────────────────────────────────────
bool SqliteManager::connect(const QString& dbPath) {
    if (m_db) disconnect();

    // Default path: <appDir>/data/mcaster1studio.db  (portable, self-contained)
    if (dbPath.isEmpty()) {
        const QString dir = QCoreApplication::applicationDirPath() + "/data";
        QDir().mkpath(dir);
        m_dbPath = dir + "/mcaster1studio.db";
    } else {
        m_dbPath = dbPath;
    }

    int rc = sqlite3_open(m_dbPath.toUtf8().constData(), &m_db);
    if (rc != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        qWarning() << "[SqliteManager] Open failed:" << m_lastError;
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // Enable WAL mode for better concurrent read performance
    execSql("PRAGMA journal_mode=WAL");
    execSql("PRAGMA foreign_keys=ON");

    if (!createSchema()) return false;

    qInfo() << "[SqliteManager] Connected:" << m_dbPath;
    return true;
}

void SqliteManager::disconnect() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
        qInfo() << "[SqliteManager] Disconnected.";
    }
}

// ─── Schema ───────────────────────────────────────────────────────────────────
bool SqliteManager::createSchema() {
    // ── media_items (core table) ────────────────────────────────────────────
    const char* sqlMediaItems =
        "CREATE TABLE IF NOT EXISTS media_items ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  file_path       TEXT    NOT NULL UNIQUE,"
        "  title           TEXT,"
        "  artist          TEXT,"
        "  album_artist    TEXT,"
        "  album           TEXT,"
        "  genre           TEXT,"
        "  year            TEXT,"
        "  track_number    TEXT,"
        "  composer        TEXT,"
        "  comment         TEXT,"
        "  isrc            TEXT,"
        "  mbid            TEXT,"
        "  label           TEXT,"
        "  language        TEXT,"
        "  bpm             REAL    DEFAULT 0,"
        "  musical_key     TEXT,"
        "  duration_ms     INTEGER DEFAULT 0,"
        "  bitrate         INTEGER DEFAULT 0,"
        "  sample_rate     INTEGER DEFAULT 44100,"
        "  channels        INTEGER DEFAULT 2,"
        "  codec           TEXT,"
        "  file_size       INTEGER DEFAULT 0,"
        "  rating          INTEGER DEFAULT 0,"
        "  energy          REAL    DEFAULT 0,"
        "  weight          REAL    DEFAULT 1,"
        "  mood            TEXT,"
        "  tags            TEXT,"
        "  explicit_content INTEGER DEFAULT 0,"
        "  date_added      TEXT,"
        "  date_modified   TEXT,"
        "  last_played     TEXT,"
        "  play_count      INTEGER DEFAULT 0,"
        "  waveform_cache  TEXT,"
        "  autodj_weight   INTEGER DEFAULT 50,"
        "  autodj_skip_count INTEGER DEFAULT 0,"
        "  has_art         INTEGER DEFAULT 0"
        ")";
    if (!execSql(sqlMediaItems)) return false;

    // ── Schema migration — add columns that may not exist in older databases ─
    // SQLite ALTER TABLE ADD COLUMN errors if column exists, so we just ignore errors.
    execSql("ALTER TABLE media_items ADD COLUMN autodj_weight INTEGER DEFAULT 50");
    execSql("ALTER TABLE media_items ADD COLUMN autodj_skip_count INTEGER DEFAULT 0");
    execSql("ALTER TABLE media_items ADD COLUMN has_art INTEGER DEFAULT 0");
    execSql("ALTER TABLE media_items ADD COLUMN waveform_cache TEXT");

    // ── FTS5 full-text search virtual table (OPTIONAL — vcpkg sqlite3 may not have FTS5) ──
    const char* sqlFts =
        "CREATE VIRTUAL TABLE IF NOT EXISTS media_items_fts USING fts5("
        "  title, artist, album, genre,"
        "  content=media_items, content_rowid=id"
        ")";
    const bool hasFts5 = execSql(sqlFts);
    if (!hasFts5)
        qWarning() << "[SqliteManager] FTS5 not available — full-text search will use LIKE fallback";

    // FTS sync triggers — only if FTS5 is available
    if (hasFts5) {
    const char* sqlTrigAI =
        "CREATE TRIGGER IF NOT EXISTS trg_mi_ai AFTER INSERT ON media_items BEGIN"
        "  INSERT INTO media_items_fts(rowid, title, artist, album, genre)"
        "  VALUES (new.id, new.title, new.artist, new.album, new.genre);"
        "END";
    execSql(sqlTrigAI);

    const char* sqlTrigAD =
        "CREATE TRIGGER IF NOT EXISTS trg_mi_ad AFTER DELETE ON media_items BEGIN"
        "  INSERT INTO media_items_fts(media_items_fts, rowid, title, artist, album, genre)"
        "  VALUES ('delete', old.id, old.title, old.artist, old.album, old.genre);"
        "END";
    execSql(sqlTrigAD);

    const char* sqlTrigAU =
        "CREATE TRIGGER IF NOT EXISTS trg_mi_au AFTER UPDATE ON media_items BEGIN"
        "  INSERT INTO media_items_fts(media_items_fts, rowid, title, artist, album, genre)"
        "  VALUES ('delete', old.id, old.title, old.artist, old.album, old.genre);"
        "  INSERT INTO media_items_fts(rowid, title, artist, album, genre)"
        "  VALUES (new.id, new.title, new.artist, new.album, new.genre);"
        "END";
    execSql(sqlTrigAU);
    } // end if (hasFts5)

    // ── library_categories ──────────────────────────────────────────────────
    const char* sqlCategories =
        "CREATE TABLE IF NOT EXISTS library_categories ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  parent_id  INTEGER DEFAULT 0,"
        "  name       TEXT    NOT NULL UNIQUE,"
        "  type       TEXT,"
        "  color      TEXT,"
        "  sort_order INTEGER DEFAULT 0,"
        "  created_at TEXT"
        ")";
    if (!execSql(sqlCategories)) return false;

    // ── track_categories (junction) ─────────────────────────────────────────
    const char* sqlTrackCat =
        "CREATE TABLE IF NOT EXISTS track_categories ("
        "  track_id    INTEGER NOT NULL REFERENCES media_items(id) ON DELETE CASCADE,"
        "  category_id INTEGER NOT NULL REFERENCES library_categories(id) ON DELETE CASCADE,"
        "  PRIMARY KEY (track_id, category_id)"
        ")";
    if (!execSql(sqlTrackCat)) return false;

    // ── playlists ───────────────────────────────────────────────────────────
    const char* sqlPlaylists =
        "CREATE TABLE IF NOT EXISTS playlists ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name        TEXT    NOT NULL,"
        "  description TEXT,"
        "  created_at  TEXT,"
        "  updated_at  TEXT"
        ")";
    if (!execSql(sqlPlaylists)) return false;

    // ── playlist_tracks ─────────────────────────────────────────────────────
    const char* sqlPlaylistTracks =
        "CREATE TABLE IF NOT EXISTS playlist_tracks ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,"
        "  track_id    INTEGER NOT NULL REFERENCES media_items(id) ON DELETE CASCADE,"
        "  position    INTEGER DEFAULT 0"
        ")";
    if (!execSql(sqlPlaylistTracks)) return false;

    // ── artist_intel ────────────────────────────────────────────────────────
    const char* sqlArtistIntel =
        "CREATE TABLE IF NOT EXISTS artist_intel ("
        "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  artist_name       TEXT    NOT NULL,"
        "  profile_text      TEXT,"
        "  discography_json  TEXT,"
        "  generated_at      TEXT,"
        "  ai_backend        TEXT,"
        "  ai_model          TEXT,"
        "  version           INTEGER DEFAULT 1,"
        "  UNIQUE(artist_name, version)"
        ")";
    if (!execSql(sqlArtistIntel)) return false;

    // ── stream_favorites ────────────────────────────────────────────────────
    const char* sqlStreamFav =
        "CREATE TABLE IF NOT EXISTS stream_favorites ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  url           TEXT    NOT NULL UNIQUE,"
        "  name          TEXT,"
        "  genre         TEXT,"
        "  bitrate_kbps  INTEGER DEFAULT 0,"
        "  description   TEXT,"
        "  codec         TEXT,"
        "  logo_url      TEXT,"
        "  play_count    INTEGER DEFAULT 0,"
        "  last_played   TEXT,"
        "  sort_order    INTEGER DEFAULT 0"
        ")";
    if (!execSql(sqlStreamFav)) return false;

    // ── ai_personas ─────────────────────────────────────────────────────────
    const char* sqlPersonas =
        "CREATE TABLE IF NOT EXISTS ai_personas ("
        "  id            INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name          TEXT    NOT NULL UNIQUE,"
        "  description   TEXT,"
        "  system_prompt TEXT    NOT NULL,"
        "  color         TEXT    DEFAULT '#1c5caa',"
        "  role_type     TEXT    DEFAULT 'Custom',"
        "  is_preset     INTEGER DEFAULT 0,"
        "  created_at    TEXT    DEFAULT CURRENT_TIMESTAMP"
        ")";
    if (!execSql(sqlPersonas)) return false;

    // ── daypart_schedule ────────────────────────────────────────────────────
    const char* sqlDaypart =
        "CREATE TABLE IF NOT EXISTS daypart_schedule ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  persona_id  INTEGER NOT NULL,"
        "  category_id INTEGER DEFAULT 0,"
        "  start_hour  INTEGER NOT NULL,"
        "  end_hour    INTEGER NOT NULL,"
        "  day_of_week TEXT    DEFAULT '*',"
        "  priority    INTEGER DEFAULT 0"
        ")";
    if (!execSql(sqlDaypart)) return false;

    // ── Schema migration — add persona_id to library_categories ─────────────
    execSql("ALTER TABLE library_categories ADD COLUMN persona_id INTEGER DEFAULT 0");

    // ── Seed default categories ─────────────────────────────────────────────
    const char* sqlSeed =
        "INSERT OR IGNORE INTO library_categories(name, type, color, sort_order) VALUES"
        "('Music',       'Music',      '#1c5caa', 1),"
        "('Stingers',    'Stinger',    '#f97316', 2),"
        "('Station IDs', 'StationID',  '#7c3aed', 3),"
        "('Sweepers',    'Sweeper',    '#22c55e', 4),"
        "('Jingles',     'Jingle',     '#f59e0b', 5),"
        "('Ads',         'Ad',         '#ef4444', 6),"
        "('Spoken Word', 'SpokenWord', '#6b7280', 7)";
    if (!execSql(sqlSeed)) return false;

    return true;
}

// ─── Helper: read a MediaItem from a row ──────────────────────────────────────
MediaItem SqliteManager::readMediaItemRow(sqlite3_stmt* stmt) {
    MediaItem item;
    int c = 0;
    item.id             = int64Col(stmt, c++);
    item.filePath       = fromCol(stmt, c++);
    item.title          = fromCol(stmt, c++);
    item.artist         = fromCol(stmt, c++);
    item.albumArtist    = fromCol(stmt, c++);
    item.album          = fromCol(stmt, c++);
    item.genre          = fromCol(stmt, c++);
    item.year           = fromCol(stmt, c++);
    item.trackNumber    = fromCol(stmt, c++);
    item.composer       = fromCol(stmt, c++);
    item.comment        = fromCol(stmt, c++);
    item.isrc           = fromCol(stmt, c++);
    item.mbid           = fromCol(stmt, c++);
    item.label          = fromCol(stmt, c++);
    item.language       = fromCol(stmt, c++);
    item.bpm            = dblCol(stmt, c++);
    item.musicalKey     = fromCol(stmt, c++);
    item.durationMs     = intCol(stmt, c++);
    item.bitrate        = intCol(stmt, c++);
    item.sampleRate     = intCol(stmt, c++);
    item.channels       = intCol(stmt, c++);
    item.codec          = fromCol(stmt, c++);
    item.fileSize       = int64Col(stmt, c++);
    item.rating         = intCol(stmt, c++);
    item.energy         = dblCol(stmt, c++);
    item.weight         = dblCol(stmt, c++);
    item.mood           = fromCol(stmt, c++);
    item.tags           = fromCol(stmt, c++);
    item.explicit_      = intCol(stmt, c++) != 0;
    item.dateAdded      = QDateTime::fromString(fromCol(stmt, c++), Qt::ISODate);
    item.dateModified   = QDateTime::fromString(fromCol(stmt, c++), Qt::ISODate);
    item.lastPlayed     = QDateTime::fromString(fromCol(stmt, c++), Qt::ISODate);
    item.playCount      = intCol(stmt, c++);
    // waveform_cache is col index 33 — skip for MediaItem (not in struct load)
    c++; // waveform_cache
    item.autoDjWeight    = intCol(stmt, c++);
    item.autoDjSkipCount = intCol(stmt, c++);
    item.hasArt          = intCol(stmt, c++) != 0;
    return item;
}

// ─── Path check ───────────────────────────────────────────────────────────────
bool SqliteManager::pathExists(const QString& path) {
    if (!m_db) return false;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id FROM media_items WHERE file_path=? LIMIT 1";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindText(stmt, 1, path);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return found;
}

// ─── Insert ───────────────────────────────────────────────────────────────────
bool SqliteManager::insertItem(MediaItem& item) {
    if (!m_db) return false;

    const char* sql =
        "INSERT INTO media_items ("
        "  file_path,title,artist,album_artist,album,genre,year,track_number,"
        "  composer,comment,isrc,mbid,label,language,bpm,musical_key,duration_ms,"
        "  bitrate,sample_rate,channels,codec,file_size,rating,energy,weight,"
        "  mood,tags,explicit_content,date_added,date_modified,"
        "  autodj_weight,autodj_skip_count,has_art"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        qWarning() << "[SqliteManager] INSERT prepare:" << m_lastError;
        return false;
    }

    int i = 1;
    bindText(stmt, i++, item.filePath);
    bindText(stmt, i++, item.title);
    bindText(stmt, i++, item.artist);
    bindText(stmt, i++, item.albumArtist);
    bindText(stmt, i++, item.album);
    bindText(stmt, i++, item.genre);
    bindText(stmt, i++, item.year);
    bindText(stmt, i++, item.trackNumber);
    bindText(stmt, i++, item.composer);
    bindText(stmt, i++, item.comment);
    bindText(stmt, i++, item.isrc);
    bindText(stmt, i++, item.mbid);
    bindText(stmt, i++, item.label);
    bindText(stmt, i++, item.language);
    sqlite3_bind_double(stmt, i++, item.bpm);
    bindText(stmt, i++, item.musicalKey);
    sqlite3_bind_int(stmt, i++, item.durationMs);
    sqlite3_bind_int(stmt, i++, item.bitrate);
    sqlite3_bind_int(stmt, i++, item.sampleRate);
    sqlite3_bind_int(stmt, i++, item.channels);
    bindText(stmt, i++, item.codec);
    sqlite3_bind_int64(stmt, i++, item.fileSize);
    sqlite3_bind_int(stmt, i++, item.rating);
    sqlite3_bind_double(stmt, i++, item.energy);
    sqlite3_bind_double(stmt, i++, item.weight);
    bindText(stmt, i++, item.mood);
    bindText(stmt, i++, item.tags);
    sqlite3_bind_int(stmt, i++, item.explicit_ ? 1 : 0);
    bindText(stmt, i++, QDateTime::currentDateTime().toString(Qt::ISODate));
    bindText(stmt, i++, QDateTime::currentDateTime().toString(Qt::ISODate));
    sqlite3_bind_int(stmt, i++, item.autoDjWeight);
    sqlite3_bind_int(stmt, i++, item.autoDjSkipCount);
    sqlite3_bind_int(stmt, i++, item.hasArt ? 1 : 0);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        qWarning() << "[SqliteManager] INSERT step:" << m_lastError;
        sqlite3_finalize(stmt);
        return false;
    }

    item.id = static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
    sqlite3_finalize(stmt);
    return true;
}

// ─── Update ───────────────────────────────────────────────────────────────────
bool SqliteManager::updateItem(const MediaItem& item) {
    if (!m_db || item.id <= 0) return false;

    const char* sql =
        "UPDATE media_items SET "
        "title=?,artist=?,album=?,genre=?,bpm=?,rating=?,energy=?,mbid=?,"
        "date_modified=? WHERE id=?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return false;
    }

    int i = 1;
    bindText(stmt, i++, item.title);
    bindText(stmt, i++, item.artist);
    bindText(stmt, i++, item.album);
    bindText(stmt, i++, item.genre);
    sqlite3_bind_double(stmt, i++, item.bpm);
    sqlite3_bind_int(stmt, i++, item.rating);
    sqlite3_bind_double(stmt, i++, item.energy);
    bindText(stmt, i++, item.mbid);
    bindText(stmt, i++, QDateTime::currentDateTime().toString(Qt::ISODate));
    sqlite3_bind_int64(stmt, i++, item.id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return false;
    }
    return true;
}

// ─── Delete ───────────────────────────────────────────────────────────────────
bool SqliteManager::deleteItem(qint64 id) {
    if (!m_db) return false;

    const char* sql = "DELETE FROM media_items WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

// ─── Load all ─────────────────────────────────────────────────────────────────
QList<MediaItem> SqliteManager::loadAll() {
    QList<MediaItem> items;
    if (!m_db) return items;

    const char* sql =
        "SELECT id,file_path,title,artist,album_artist,album,genre,year,"
        "track_number,composer,comment,isrc,mbid,label,language,bpm,musical_key,"
        "duration_ms,bitrate,sample_rate,channels,codec,file_size,rating,energy,"
        "weight,mood,tags,explicit_content,date_added,date_modified,last_played,"
        "play_count,waveform_cache,autodj_weight,autodj_skip_count,has_art"
        " FROM media_items ORDER BY artist,album,track_number";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return items;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
        items.append(readMediaItemRow(stmt));

    sqlite3_finalize(stmt);
    return items;
}

// ─── FTS5 full-text search ────────────────────────────────────────────────────
QList<MediaItem> SqliteManager::search(const QString& query, int limit) {
    QList<MediaItem> items;
    if (!m_db || query.trimmed().isEmpty()) return items;

    // Append * for prefix matching (e.g. "pink" matches "Pink Floyd")
    const QString ftsQuery = query.trimmed() + QStringLiteral("*");

    const char* sql =
        "SELECT m.id,m.file_path,m.title,m.artist,m.album_artist,m.album,m.genre,"
        "m.year,m.track_number,m.composer,m.comment,m.isrc,m.mbid,m.label,"
        "m.language,m.bpm,m.musical_key,m.duration_ms,m.bitrate,m.sample_rate,"
        "m.channels,m.codec,m.file_size,m.rating,m.energy,m.weight,m.mood,m.tags,"
        "m.explicit_content,m.date_added,m.date_modified,m.last_played,"
        "m.play_count,m.waveform_cache,m.autodj_weight,m.autodj_skip_count,m.has_art"
        " FROM media_items m"
        " WHERE m.id IN ("
        "   SELECT rowid FROM media_items_fts WHERE media_items_fts MATCH ?"
        "   ORDER BY rank LIMIT ?"
        " )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return items;
    }

    bindText(stmt, 1, ftsQuery);
    sqlite3_bind_int(stmt, 2, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        items.append(readMediaItemRow(stmt));

    sqlite3_finalize(stmt);
    return items;
}

// ─── FTS5 search within a category ─────────────────────────────────────────
QList<MediaItem> SqliteManager::searchInCategory(const QString& query,
                                                  qint64 categoryId,
                                                  int limit) {
    QList<MediaItem> items;
    if (!m_db || query.trimmed().isEmpty() || categoryId <= 0) return items;

    const QString ftsQuery = query.trimmed() + QStringLiteral("*");

    const char* sql =
        "SELECT m.id,m.file_path,m.title,m.artist,m.album_artist,m.album,m.genre,"
        "m.year,m.track_number,m.composer,m.comment,m.isrc,m.mbid,m.label,"
        "m.language,m.bpm,m.musical_key,m.duration_ms,m.bitrate,m.sample_rate,"
        "m.channels,m.codec,m.file_size,m.rating,m.energy,m.weight,m.mood,m.tags,"
        "m.explicit_content,m.date_added,m.date_modified,m.last_played,"
        "m.play_count,m.waveform_cache,m.autodj_weight,m.autodj_skip_count,m.has_art"
        " FROM media_items m"
        " JOIN track_categories tc ON tc.track_id = m.id"
        " WHERE tc.category_id = ?"
        "   AND m.id IN ("
        "     SELECT rowid FROM media_items_fts WHERE media_items_fts MATCH ?"
        "     ORDER BY rank LIMIT ?"
        "   )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return items;
    }

    sqlite3_bind_int64(stmt, 1, categoryId);
    bindText(stmt, 2, ftsQuery);
    sqlite3_bind_int(stmt, 3, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        items.append(readMediaItemRow(stmt));

    sqlite3_finalize(stmt);
    return items;
}

// ─── Total track count ─────────────────────────────────────────────────────
int SqliteManager::totalTrackCount() {
    if (!m_db) return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM media_items",
                            -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Category CRUD
// ═════════════════════════════════════════════════════════════════════════════

qint64 SqliteManager::addCategory(const QString& name, const QString& type,
                                   const QString& color, qint64 parentId) {
    if (!m_db) return -1;

    const char* sql =
        "INSERT INTO library_categories(parent_id, name, type, color, sort_order, created_at)"
        " VALUES (?,?,?,?,(SELECT COALESCE(MAX(sort_order),0)+1 FROM library_categories),?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, parentId);
    bindText(stmt, 2, name);
    bindText(stmt, 3, type);
    bindText(stmt, 4, color);
    bindText(stmt, 5, QDateTime::currentDateTime().toString(Qt::ISODate));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
}

bool SqliteManager::removeCategory(qint64 id) {
    if (!m_db) return false;

    const char* sql = "DELETE FROM library_categories WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool SqliteManager::renameCategory(qint64 id, const QString& name) {
    if (!m_db) return false;

    const char* sql = "UPDATE library_categories SET name=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindText(stmt, 1, name);
    sqlite3_bind_int64(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool SqliteManager::setCategoryColor(qint64 id, const QString& color) {
    if (!m_db) return false;

    const char* sql = "UPDATE library_categories SET color=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindText(stmt, 1, color);
    sqlite3_bind_int64(stmt, 2, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

QList<QVariantMap> SqliteManager::allCategories() {
    QList<QVariantMap> cats;
    if (!m_db) return cats;

    const char* sql =
        "SELECT c.id, c.parent_id, c.name, c.type, c.color, c.sort_order,"
        " (SELECT COUNT(*) FROM track_categories tc WHERE tc.category_id=c.id) AS track_count"
        " FROM library_categories c ORDER BY c.sort_order, c.name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return cats;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row["id"]          = int64Col(stmt, 0);
        row["parent_id"]   = int64Col(stmt, 1);
        row["name"]        = fromCol(stmt, 2);
        row["type"]        = fromCol(stmt, 3);
        row["color"]       = fromCol(stmt, 4);
        row["sort_order"]  = intCol(stmt, 5);
        row["track_count"] = intCol(stmt, 6);
        cats.append(row);
    }
    sqlite3_finalize(stmt);
    return cats;
}

void SqliteManager::assignTrackToCategory(qint64 trackId, qint64 categoryId) {
    if (!m_db) return;

    const char* sql =
        "INSERT OR IGNORE INTO track_categories(track_id, category_id) VALUES (?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, trackId);
    sqlite3_bind_int64(stmt, 2, categoryId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteManager::unassignTrackFromCategory(qint64 trackId, qint64 categoryId) {
    if (!m_db) return;

    const char* sql =
        "DELETE FROM track_categories WHERE track_id=? AND category_id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, trackId);
    sqlite3_bind_int64(stmt, 2, categoryId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

QList<qint64> SqliteManager::categoriesForTrack(qint64 trackId) {
    QList<qint64> ids;
    if (!m_db) return ids;

    const char* sql =
        "SELECT category_id FROM track_categories WHERE track_id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return ids;

    sqlite3_bind_int64(stmt, 1, trackId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        ids.append(int64Col(stmt, 0));

    sqlite3_finalize(stmt);
    return ids;
}

QList<MediaItem> SqliteManager::tracksByCategory(qint64 categoryId) {
    QList<MediaItem> items;
    if (!m_db) return items;

    const char* sql =
        "SELECT m.id,m.file_path,m.title,m.artist,m.album_artist,m.album,m.genre,"
        "m.year,m.track_number,m.composer,m.comment,m.isrc,m.mbid,m.label,"
        "m.language,m.bpm,m.musical_key,m.duration_ms,m.bitrate,m.sample_rate,"
        "m.channels,m.codec,m.file_size,m.rating,m.energy,m.weight,m.mood,m.tags,"
        "m.explicit_content,m.date_added,m.date_modified,m.last_played,"
        "m.play_count,m.waveform_cache,m.autodj_weight,m.autodj_skip_count,m.has_art"
        " FROM media_items m"
        " JOIN track_categories tc ON tc.track_id = m.id"
        " WHERE tc.category_id=?"
        " ORDER BY m.artist, m.album, m.track_number";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return items;
    }

    sqlite3_bind_int64(stmt, 1, categoryId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        items.append(readMediaItemRow(stmt));

    sqlite3_finalize(stmt);
    return items;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Playlist CRUD
// ═════════════════════════════════════════════════════════════════════════════

qint64 SqliteManager::createPlaylist(const QString& name, const QString& description) {
    if (!m_db) return -1;

    const char* sql =
        "INSERT INTO playlists(name, description, created_at, updated_at)"
        " VALUES (?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }

    QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    bindText(stmt, 1, name);
    bindText(stmt, 2, description);
    bindText(stmt, 3, now);
    bindText(stmt, 4, now);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
}

bool SqliteManager::deletePlaylist(qint64 id) {
    if (!m_db) return false;

    // CASCADE will clean playlist_tracks, but be explicit for safety
    {
        const char* delTracks = "DELETE FROM playlist_tracks WHERE playlist_id=?";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, delTracks, -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, id);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
    }

    const char* sql = "DELETE FROM playlists WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool SqliteManager::renamePlaylist(qint64 id, const QString& name) {
    if (!m_db) return false;

    const char* sql = "UPDATE playlists SET name=?, updated_at=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindText(stmt, 1, name);
    bindText(stmt, 2, QDateTime::currentDateTime().toString(Qt::ISODate));
    sqlite3_bind_int64(stmt, 3, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

QList<QPair<qint64, QString>> SqliteManager::allPlaylists() {
    QList<QPair<qint64, QString>> result;
    if (!m_db) return result;

    const char* sql = "SELECT id, name FROM playlists ORDER BY name";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.append({int64Col(stmt, 0), fromCol(stmt, 1)});

    sqlite3_finalize(stmt);
    return result;
}

void SqliteManager::addTrackToPlaylist(qint64 playlistId, qint64 trackId, int position) {
    if (!m_db) return;

    // If position == -1, append at end
    if (position < 0) {
        const char* maxSql =
            "SELECT COALESCE(MAX(position),0)+1 FROM playlist_tracks WHERE playlist_id=?";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, maxSql, -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(st, 1, playlistId);
            if (sqlite3_step(st) == SQLITE_ROW)
                position = intCol(st, 0);
            else
                position = 0;
            sqlite3_finalize(st);
        } else {
            position = 0;
        }
    }

    const char* sql =
        "INSERT INTO playlist_tracks(playlist_id, track_id, position) VALUES (?,?,?)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, playlistId);
    sqlite3_bind_int64(stmt, 2, trackId);
    sqlite3_bind_int(stmt, 3, position);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    // Touch updated_at
    {
        const char* upd = "UPDATE playlists SET updated_at=? WHERE id=?";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, upd, -1, &st, nullptr) == SQLITE_OK) {
            bindText(st, 1, QDateTime::currentDateTime().toString(Qt::ISODate));
            sqlite3_bind_int64(st, 2, playlistId);
            sqlite3_step(st);
            sqlite3_finalize(st);
        }
    }
}

void SqliteManager::removeTrackFromPlaylist(qint64 playlistId, int position) {
    if (!m_db) return;

    const char* sql =
        "DELETE FROM playlist_tracks WHERE playlist_id=? AND position=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, playlistId);
    sqlite3_bind_int(stmt, 2, position);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

QList<MediaItem> SqliteManager::playlistTracks(qint64 playlistId) {
    QList<MediaItem> items;
    if (!m_db) return items;

    const char* sql =
        "SELECT m.id,m.file_path,m.title,m.artist,m.album_artist,m.album,m.genre,"
        "m.year,m.track_number,m.composer,m.comment,m.isrc,m.mbid,m.label,"
        "m.language,m.bpm,m.musical_key,m.duration_ms,m.bitrate,m.sample_rate,"
        "m.channels,m.codec,m.file_size,m.rating,m.energy,m.weight,m.mood,m.tags,"
        "m.explicit_content,m.date_added,m.date_modified,m.last_played,"
        "m.play_count,m.waveform_cache,m.autodj_weight,m.autodj_skip_count,m.has_art"
        " FROM playlist_tracks pt"
        " JOIN media_items m ON m.id = pt.track_id"
        " WHERE pt.playlist_id=?"
        " ORDER BY pt.position";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return items;
    }

    sqlite3_bind_int64(stmt, 1, playlistId);
    while (sqlite3_step(stmt) == SQLITE_ROW)
        items.append(readMediaItemRow(stmt));

    sqlite3_finalize(stmt);
    return items;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Artist Intel CRUD
// ═════════════════════════════════════════════════════════════════════════════

qint64 SqliteManager::saveArtistIntel(const QString& artistName,
                                       const QString& profileText,
                                       const QString& discographyJson,
                                       const QString& aiBackend,
                                       const QString& aiModel) {
    if (!m_db) return -1;

    // Auto-increment version per artist
    int nextVer = 1;
    {
        const char* verSql =
            "SELECT COALESCE(MAX(version),0)+1 FROM artist_intel WHERE artist_name=?";
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(m_db, verSql, -1, &st, nullptr) == SQLITE_OK) {
            bindText(st, 1, artistName);
            if (sqlite3_step(st) == SQLITE_ROW)
                nextVer = intCol(st, 0);
            sqlite3_finalize(st);
        }
    }

    const char* sql =
        "INSERT INTO artist_intel(artist_name, profile_text, discography_json,"
        " generated_at, ai_backend, ai_model, version)"
        " VALUES (?,?,?,?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }

    bindText(stmt, 1, artistName);
    bindText(stmt, 2, profileText);
    bindText(stmt, 3, discographyJson);
    bindText(stmt, 4, QDateTime::currentDateTime().toString(Qt::ISODate));
    bindText(stmt, 5, aiBackend);
    bindText(stmt, 6, aiModel);
    sqlite3_bind_int(stmt, 7, nextVer);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
}

QList<QVariantMap> SqliteManager::artistIntelProfiles(const QString& artistName) {
    QList<QVariantMap> profiles;
    if (!m_db) return profiles;

    const char* sql =
        "SELECT id, artist_name, profile_text, discography_json, generated_at,"
        " ai_backend, ai_model, version"
        " FROM artist_intel WHERE artist_name=? ORDER BY version DESC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return profiles;

    bindText(stmt, 1, artistName);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row["id"]               = int64Col(stmt, 0);
        row["artist_name"]      = fromCol(stmt, 1);
        row["profile_text"]     = fromCol(stmt, 2);
        row["discography_json"] = fromCol(stmt, 3);
        row["generated_at"]     = fromCol(stmt, 4);
        row["ai_backend"]       = fromCol(stmt, 5);
        row["ai_model"]         = fromCol(stmt, 6);
        row["version"]          = intCol(stmt, 7);
        profiles.append(row);
    }
    sqlite3_finalize(stmt);
    return profiles;
}

bool SqliteManager::deleteArtistIntel(qint64 id) {
    if (!m_db) return false;

    const char* sql = "DELETE FROM artist_intel WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

QStringList SqliteManager::artistsWithIntel() {
    QStringList names;
    if (!m_db) return names;

    const char* sql =
        "SELECT DISTINCT artist_name FROM artist_intel ORDER BY artist_name";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return names;

    while (sqlite3_step(stmt) == SQLITE_ROW)
        names.append(fromCol(stmt, 0));

    sqlite3_finalize(stmt);
    return names;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Stream Favorites CRUD
// ═════════════════════════════════════════════════════════════════════════════

qint64 SqliteManager::addStreamFavorite(const QString& url, const QString& name,
                                         const QString& genre, int bitrate) {
    if (!m_db) return -1;

    const char* sql =
        "INSERT INTO stream_favorites(url, name, genre, bitrate_kbps, sort_order)"
        " VALUES (?,?,?,?,(SELECT COALESCE(MAX(sort_order),0)+1 FROM stream_favorites))";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }

    bindText(stmt, 1, url);
    bindText(stmt, 2, name);
    bindText(stmt, 3, genre);
    sqlite3_bind_int(stmt, 4, bitrate);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
}

bool SqliteManager::removeStreamFavorite(qint64 id) {
    if (!m_db) return false;

    const char* sql = "DELETE FROM stream_favorites WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

QList<QVariantMap> SqliteManager::allStreamFavorites() {
    QList<QVariantMap> favs;
    if (!m_db) return favs;

    const char* sql =
        "SELECT id, url, name, genre, bitrate_kbps, description, codec, logo_url,"
        " play_count, last_played, sort_order"
        " FROM stream_favorites ORDER BY sort_order, name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return favs;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row["id"]           = int64Col(stmt, 0);
        row["url"]          = fromCol(stmt, 1);
        row["name"]         = fromCol(stmt, 2);
        row["genre"]        = fromCol(stmt, 3);
        row["bitrate_kbps"] = intCol(stmt, 4);
        row["description"]  = fromCol(stmt, 5);
        row["codec"]        = fromCol(stmt, 6);
        row["logo_url"]     = fromCol(stmt, 7);
        row["play_count"]   = intCol(stmt, 8);
        row["last_played"]  = fromCol(stmt, 9);
        row["sort_order"]   = intCol(stmt, 10);
        favs.append(row);
    }
    sqlite3_finalize(stmt);
    return favs;
}

void SqliteManager::incrementStreamPlayCount(qint64 id) {
    if (!m_db) return;

    const char* sql =
        "UPDATE stream_favorites SET play_count=play_count+1, last_played=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    bindText(stmt, 1, QDateTime::currentDateTime().toString(Qt::ISODate));
    sqlite3_bind_int64(stmt, 2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Persona CRUD
// ═════════════════════════════════════════════════════════════════════════════

qint64 SqliteManager::addPersona(const QString& name, const QString& description,
                                   const QString& systemPrompt, const QString& color,
                                   const QString& roleType, bool isPreset) {
    if (!m_db) return -1;

    const char* sql =
        "INSERT OR IGNORE INTO ai_personas(name, description, system_prompt, color,"
        " role_type, is_preset, created_at)"
        " VALUES (?,?,?,?,?,?,?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }

    bindText(stmt, 1, name);
    bindText(stmt, 2, description);
    bindText(stmt, 3, systemPrompt);
    bindText(stmt, 4, color);
    bindText(stmt, 5, roleType);
    sqlite3_bind_int(stmt, 6, isPreset ? 1 : 0);
    bindText(stmt, 7, QDateTime::currentDateTime().toString(Qt::ISODate));

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
}

bool SqliteManager::removePersona(qint64 id) {
    if (!m_db) return false;

    const char* sql = "DELETE FROM ai_personas WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

bool SqliteManager::updatePersona(qint64 id, const QString& name,
                                    const QString& description,
                                    const QString& systemPrompt,
                                    const QString& color,
                                    const QString& roleType) {
    if (!m_db) return false;

    const char* sql =
        "UPDATE ai_personas SET name=?, description=?, system_prompt=?,"
        " color=?, role_type=? WHERE id=?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    bindText(stmt, 1, name);
    bindText(stmt, 2, description);
    bindText(stmt, 3, systemPrompt);
    bindText(stmt, 4, color);
    bindText(stmt, 5, roleType);
    sqlite3_bind_int64(stmt, 6, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE);
}

QList<QVariantMap> SqliteManager::allPersonas() {
    QList<QVariantMap> personas;
    if (!m_db) return personas;

    const char* sql =
        "SELECT id, name, description, system_prompt, color, role_type, is_preset"
        " FROM ai_personas ORDER BY is_preset DESC, role_type, name";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return personas;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row["id"]            = int64Col(stmt, 0);
        row["name"]          = fromCol(stmt, 1);
        row["description"]   = fromCol(stmt, 2);
        row["system_prompt"] = fromCol(stmt, 3);
        row["color"]         = fromCol(stmt, 4);
        row["role_type"]     = fromCol(stmt, 5);
        row["is_preset"]     = intCol(stmt, 6);
        personas.append(row);
    }
    sqlite3_finalize(stmt);
    return personas;
}

QVariantMap SqliteManager::personaById(qint64 id) {
    QVariantMap row;
    if (!m_db) return row;

    const char* sql =
        "SELECT id, name, description, system_prompt, color, role_type, is_preset"
        " FROM ai_personas WHERE id=?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return row;

    sqlite3_bind_int64(stmt, 1, id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        row["id"]            = int64Col(stmt, 0);
        row["name"]          = fromCol(stmt, 1);
        row["description"]   = fromCol(stmt, 2);
        row["system_prompt"] = fromCol(stmt, 3);
        row["color"]         = fromCol(stmt, 4);
        row["role_type"]     = fromCol(stmt, 5);
        row["is_preset"]     = intCol(stmt, 6);
    }
    sqlite3_finalize(stmt);
    return row;
}

QString SqliteManager::personaPrompt(qint64 id) {
    if (!m_db) return {};

    const char* sql = "SELECT system_prompt FROM ai_personas WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    sqlite3_bind_int64(stmt, 1, id);
    QString result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = fromCol(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

// ─── Category Persona Assignment ─────────────────────────────────────────────

void SqliteManager::setCategoryPersona(qint64 categoryId, qint64 personaId) {
    if (!m_db) return;

    const char* sql = "UPDATE library_categories SET persona_id=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, personaId);
    sqlite3_bind_int64(stmt, 2, categoryId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

qint64 SqliteManager::categoryPersona(qint64 categoryId) {
    if (!m_db) return 0;

    const char* sql = "SELECT persona_id FROM library_categories WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int64(stmt, 1, categoryId);
    qint64 result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = int64Col(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

// ─── Daypart Schedule ────────────────────────────────────────────────────────

qint64 SqliteManager::addDaypartEntry(qint64 personaId, qint64 categoryId,
                                        int startHour, int endHour,
                                        const QString& dow) {
    if (!m_db) return -1;

    const char* sql =
        "INSERT INTO daypart_schedule(persona_id, category_id, start_hour,"
        " end_hour, day_of_week, priority)"
        " VALUES (?,?,?,?,?,0)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, personaId);
    sqlite3_bind_int64(stmt, 2, categoryId);
    sqlite3_bind_int(stmt, 3, startHour);
    sqlite3_bind_int(stmt, 4, endHour);
    bindText(stmt, 5, dow);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return -1;
    }
    return static_cast<qint64>(sqlite3_last_insert_rowid(m_db));
}

void SqliteManager::removeDaypartEntry(qint64 id) {
    if (!m_db) return;

    const char* sql = "DELETE FROM daypart_schedule WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

QList<QVariantMap> SqliteManager::allDaypartEntries() {
    QList<QVariantMap> entries;
    if (!m_db) return entries;

    const char* sql =
        "SELECT d.id, d.persona_id, d.category_id, d.start_hour, d.end_hour,"
        " d.day_of_week, d.priority, p.name AS persona_name"
        " FROM daypart_schedule d"
        " LEFT JOIN ai_personas p ON p.id = d.persona_id"
        " ORDER BY d.priority DESC, d.start_hour";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return entries;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantMap row;
        row["id"]           = int64Col(stmt, 0);
        row["persona_id"]   = int64Col(stmt, 1);
        row["category_id"]  = int64Col(stmt, 2);
        row["start_hour"]   = intCol(stmt, 3);
        row["end_hour"]     = intCol(stmt, 4);
        row["day_of_week"]  = fromCol(stmt, 5);
        row["priority"]     = intCol(stmt, 6);
        row["persona_name"] = fromCol(stmt, 7);
        entries.append(row);
    }
    sqlite3_finalize(stmt);
    return entries;
}

qint64 SqliteManager::activeDaypartPersona(int currentHour, const QString& currentDow) {
    if (!m_db) return 0;

    // Match entries where current hour falls within [start_hour, end_hour)
    // and day_of_week is '*' (any day) or matches the current day.
    // Return the highest-priority match.
    const char* sql =
        "SELECT persona_id FROM daypart_schedule"
        " WHERE start_hour <= ? AND end_hour > ?"
        "   AND (day_of_week = '*' OR day_of_week = ?)"
        " ORDER BY priority DESC LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, currentHour);
    sqlite3_bind_int(stmt, 2, currentHour);
    bindText(stmt, 3, currentDow);

    qint64 result = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = int64Col(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
//  AutoDJ
// ═════════════════════════════════════════════════════════════════════════════

void SqliteManager::setAutoDjWeight(qint64 trackId, int weight) {
    if (!m_db) return;

    const char* sql = "UPDATE media_items SET autodj_weight=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int(stmt, 1, weight);
    sqlite3_bind_int64(stmt, 2, trackId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteManager::incrementAutoDjSkipCount(qint64 trackId) {
    if (!m_db) return;

    const char* sql =
        "UPDATE media_items SET autodj_skip_count=autodj_skip_count+1 WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int64(stmt, 1, trackId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

MediaItem SqliteManager::nextAutoDjTrack(qint64 excludeId) {
    MediaItem item;
    if (!m_db) return item;

    // Weighted random selection: higher autodj_weight = more likely to be picked.
    // Exclude recently played (last_played within 60 min) and weight=0 tracks.
    // Uses ABS(RANDOM()) % weight to create a weighted distribution, then picks the top result.
    const char* sql =
        "SELECT id,file_path,title,artist,album_artist,album,genre,year,"
        "track_number,composer,comment,isrc,mbid,label,language,bpm,musical_key,"
        "duration_ms,bitrate,sample_rate,channels,codec,file_size,rating,energy,"
        "weight,mood,tags,explicit_content,date_added,date_modified,last_played,"
        "play_count,waveform_cache,autodj_weight,autodj_skip_count,has_art"
        " FROM media_items"
        " WHERE autodj_weight > 0 AND id != ?"
        "   AND (last_played IS NULL OR last_played < datetime('now','-60 minutes'))"
        " ORDER BY (ABS(RANDOM()) % (autodj_weight + 1)) DESC"
        " LIMIT 1";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return item;
    }

    sqlite3_bind_int64(stmt, 1, excludeId);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        item = readMediaItemRow(stmt);

    sqlite3_finalize(stmt);
    return item;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Play count / rating
// ═════════════════════════════════════════════════════════════════════════════

void SqliteManager::incrementPlayCount(qint64 trackId) {
    if (!m_db) return;

    const char* sql =
        "UPDATE media_items SET play_count=play_count+1, last_played=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    bindText(stmt, 1, QDateTime::currentDateTime().toString(Qt::ISODate));
    sqlite3_bind_int64(stmt, 2, trackId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void SqliteManager::setRating(qint64 trackId, int rating) {
    if (!m_db) return;

    const char* sql = "UPDATE media_items SET rating=? WHERE id=?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    sqlite3_bind_int(stmt, 1, rating);
    sqlite3_bind_int64(stmt, 2, trackId);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ─── Internal ─────────────────────────────────────────────────────────────────
bool SqliteManager::execSql(const char* sql) {
    if (!m_db) { m_lastError = "Not connected"; return false; }
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        m_lastError = err ? QString::fromUtf8(err) : "Unknown SQLite error";
        if (err) sqlite3_free(err);
        qWarning() << "[SqliteManager] SQL error:" << m_lastError;
        return false;
    }
    return true;
}

// ─── Raw query execution ─────────────────────────────────────────────────────
QList<QVariantList> SqliteManager::executeQuery(const QString& sql) {
    QList<QVariantList> results;
    if (!m_db) { m_lastError = "Not connected"; return results; }

    sqlite3_stmt* stmt = nullptr;
    QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(m_db, utf8.constData(), utf8.size(), &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return results;
    }

    const int colCount = sqlite3_column_count(stmt);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QVariantList row;
        row.reserve(colCount);
        for (int c = 0; c < colCount; ++c) {
            switch (sqlite3_column_type(stmt, c)) {
            case SQLITE_INTEGER:
                row.append(static_cast<qint64>(sqlite3_column_int64(stmt, c)));
                break;
            case SQLITE_FLOAT:
                row.append(sqlite3_column_double(stmt, c));
                break;
            case SQLITE_NULL:
                row.append(QVariant());
                break;
            default: // TEXT / BLOB
                row.append(fromCol(stmt, c));
                break;
            }
        }
        results.append(row);
    }
    sqlite3_finalize(stmt);
    return results;
}

QStringList SqliteManager::tableNames() {
    QStringList names;
    if (!m_db) return names;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' "
                      "AND name NOT LIKE 'sqlite_%' ORDER BY name";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return names;

    while (sqlite3_step(stmt) == SQLITE_ROW)
        names.append(fromCol(stmt, 0));

    sqlite3_finalize(stmt);
    return names;
}

} // namespace M1
