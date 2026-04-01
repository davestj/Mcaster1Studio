#include "SqliteManager.h"
#include <sqlite3.h>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QDateTime>

namespace M1 {

// ─── Helpers ─────────────────────────────────────────────────────────────────
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

// ─── Constructor / Destructor ─────────────────────────────────────────────────
SqliteManager::SqliteManager() = default;

SqliteManager::~SqliteManager() {
    disconnect();
}

// ─── Connection ───────────────────────────────────────────────────────────────
bool SqliteManager::connect(const QString& dbPath) {
    if (m_db) disconnect();

    // Default path: AppData/Mcaster1Studio/mcaster1studio.db
    if (dbPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
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
    const char* sql =
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
        "  waveform_cache  TEXT"
        ")";
    return execSql(sql);
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
        "  mood,tags,explicit_content,date_added,date_modified"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

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
        "play_count FROM media_items ORDER BY artist,album,track_number";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        m_lastError = QString::fromUtf8(sqlite3_errmsg(m_db));
        return items;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MediaItem item;
        int c = 0;
        item.id           = int64Col(stmt, c++);
        item.filePath     = fromCol(stmt, c++);
        item.title        = fromCol(stmt, c++);
        item.artist       = fromCol(stmt, c++);
        item.albumArtist  = fromCol(stmt, c++);
        item.album        = fromCol(stmt, c++);
        item.genre        = fromCol(stmt, c++);
        item.year         = fromCol(stmt, c++);
        item.trackNumber  = fromCol(stmt, c++);
        item.composer     = fromCol(stmt, c++);
        item.comment      = fromCol(stmt, c++);
        item.isrc         = fromCol(stmt, c++);
        item.mbid         = fromCol(stmt, c++);
        item.label        = fromCol(stmt, c++);
        item.language     = fromCol(stmt, c++);
        item.bpm          = dblCol(stmt, c++);
        item.musicalKey   = fromCol(stmt, c++);
        item.durationMs   = intCol(stmt, c++);
        item.bitrate      = intCol(stmt, c++);
        item.sampleRate   = intCol(stmt, c++);
        item.channels     = intCol(stmt, c++);
        item.codec        = fromCol(stmt, c++);
        item.fileSize     = int64Col(stmt, c++);
        item.rating       = intCol(stmt, c++);
        item.energy       = dblCol(stmt, c++);
        item.weight       = dblCol(stmt, c++);
        item.mood         = fromCol(stmt, c++);
        item.tags         = fromCol(stmt, c++);
        item.explicit_    = intCol(stmt, c++) != 0;
        item.dateAdded    = QDateTime::fromString(fromCol(stmt, c++), Qt::ISODate);
        item.dateModified = QDateTime::fromString(fromCol(stmt, c++), Qt::ISODate);
        item.lastPlayed   = QDateTime::fromString(fromCol(stmt, c++), Qt::ISODate);
        item.playCount    = intCol(stmt, c++);
        items.append(item);
    }
    sqlite3_finalize(stmt);
    return items;
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
