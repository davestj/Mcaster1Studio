#include "PostgresManager.h"
#include <QDebug>
#include <QDateTime>

#ifdef M1_HAS_POSTGRESQL
// ═════════════════════════════════════════════════════════════════════════════
//  Full libpq implementation — only compiled when PostgreSQL is available
// ═════════════════════════════════════════════════════════════════════════════

#include <libpq-fe.h>

namespace M1 {

// ─── Helpers ─────────────────────────────────────────────────────────────────
static PGconn* toConn(void* p) { return static_cast<PGconn*>(p); }

static QString fromField(PGresult* res, int row, int col) {
    if (PQgetisnull(res, row, col)) return {};
    return QString::fromUtf8(PQgetvalue(res, row, col));
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────
PostgresManager::PostgresManager() = default;

PostgresManager::~PostgresManager() {
    disconnect();
}

// ─── Connection ───────────────────────────────────────────────────────────────
bool PostgresManager::connect(const Config& cfg) {
    if (m_conn) disconnect();

    // First ensure the target database exists
    if (!ensureDatabase(cfg))
        return false;

    // Now connect to the actual database
    const QString connStr = QString(
        "host='%1' port=%2 user='%3' password='%4' dbname='%5' "
        "connect_timeout=5 client_encoding=UTF8")
        .arg(cfg.host)
        .arg(cfg.port)
        .arg(cfg.user)
        .arg(cfg.password)
        .arg(cfg.database);

    PGconn* c = PQconnectdb(connStr.toUtf8().constData());
    if (PQstatus(c) != CONNECTION_OK) {
        m_lastError = QString::fromUtf8(PQerrorMessage(c));
        qWarning() << "[PostgresManager] Connection failed:" << m_lastError;
        PQfinish(c);
        return false;
    }

    m_conn = c;

    if (!createSchema()) return false;

    qInfo() << "[PostgresManager] Connected:" << cfg.host << "/" << cfg.database;
    return true;
}

bool PostgresManager::ensureDatabase(const Config& cfg) {
    // Connect to the default "postgres" database to check/create our target DB
    const QString connStr = QString(
        "host='%1' port=%2 user='%3' password='%4' dbname='postgres' "
        "connect_timeout=5 client_encoding=UTF8")
        .arg(cfg.host)
        .arg(cfg.port)
        .arg(cfg.user)
        .arg(cfg.password);

    PGconn* c = PQconnectdb(connStr.toUtf8().constData());
    if (PQstatus(c) != CONNECTION_OK) {
        m_lastError = QString::fromUtf8(PQerrorMessage(c));
        qWarning() << "[PostgresManager] Cannot connect to postgres:" << m_lastError;
        PQfinish(c);
        return false;
    }

    // Check if target database exists
    const QString checkSql = QString(
        "SELECT 1 FROM pg_database WHERE datname = '%1'")
        .arg(cfg.database);

    PGresult* res = PQexec(c, checkSql.toUtf8().constData());
    const bool exists = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    PQclear(res);

    if (!exists) {
        // Create the database
        const QString createSql = QString("CREATE DATABASE \"%1\" ENCODING 'UTF8'")
            .arg(cfg.database);

        res = PQexec(c, createSql.toUtf8().constData());
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
            qWarning() << "[PostgresManager] CREATE DATABASE failed:" << m_lastError;
            PQclear(res);
            PQfinish(c);
            return false;
        }
        PQclear(res);
        qInfo() << "[PostgresManager] Created database:" << cfg.database;
    }

    PQfinish(c);
    return true;
}

void PostgresManager::disconnect() {
    if (m_conn) {
        PQfinish(toConn(m_conn));
        m_conn = nullptr;
        qInfo() << "[PostgresManager] Disconnected.";
    }
}

// ─── Schema ───────────────────────────────────────────────────────────────────
bool PostgresManager::createSchema() {
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS "media_items" (
            "id"               BIGSERIAL PRIMARY KEY,
            "file_path"        VARCHAR(2048)      NOT NULL UNIQUE,
            "title"            VARCHAR(512),
            "artist"           VARCHAR(512),
            "album_artist"     VARCHAR(512),
            "album"            VARCHAR(512),
            "genre"            VARCHAR(256),
            "year"             VARCHAR(10),
            "track_number"     VARCHAR(10),
            "composer"         VARCHAR(512),
            "comment"          TEXT,
            "isrc"             VARCHAR(32),
            "mbid"             VARCHAR(64),
            "label"            VARCHAR(256),
            "language"         VARCHAR(64),
            "bpm"              DOUBLE PRECISION   DEFAULT 0,
            "musical_key"      VARCHAR(16),
            "duration_ms"      INT                DEFAULT 0,
            "bitrate"          INT                DEFAULT 0,
            "sample_rate"      INT                DEFAULT 44100,
            "channels"         INT                DEFAULT 2,
            "codec"            VARCHAR(32),
            "file_size"        BIGINT             DEFAULT 0,
            "rating"           INT                DEFAULT 0,
            "energy"           DOUBLE PRECISION   DEFAULT 0,
            "weight"           DOUBLE PRECISION   DEFAULT 1,
            "mood"             VARCHAR(256),
            "tags"             TEXT,
            "explicit_content" BOOLEAN            DEFAULT FALSE,
            "date_added"       TIMESTAMP,
            "date_modified"    TIMESTAMP,
            "last_played"      TIMESTAMP,
            "play_count"       INT                DEFAULT 0,
            "waveform_cache"   VARCHAR(2048)
        )
    )SQL";
    return execSql(QString::fromUtf8(sql));
}

// ─── Path check ───────────────────────────────────────────────────────────────
bool PostgresManager::pathExists(const QString& path) {
    if (!m_conn) return false;

    const char* paramValues[1];
    QByteArray pathUtf8 = path.toUtf8();
    paramValues[0] = pathUtf8.constData();

    PGresult* res = PQexecParams(toConn(m_conn),
        "SELECT id FROM media_items WHERE file_path=$1 LIMIT 1",
        1, nullptr, paramValues, nullptr, nullptr, 0);

    bool found = (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    PQclear(res);
    return found;
}

// ─── Insert ───────────────────────────────────────────────────────────────────
bool PostgresManager::insertItem(MediaItem& item) {
    if (!m_conn) return false;

    const char* sql =
        "INSERT INTO media_items ("
        "  file_path,title,artist,album_artist,album,genre,year,track_number,"
        "  composer,comment,isrc,mbid,label,language,bpm,musical_key,duration_ms,"
        "  bitrate,sample_rate,channels,codec,file_size,rating,energy,weight,"
        "  mood,tags,explicit_content,date_added,date_modified"
        ") VALUES ("
        "  $1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,"
        "  $18,$19,$20,$21,$22,$23,$24,$25,$26,$27,$28,NOW(),NOW()"
        ") RETURNING id";

    QByteArray vals[28];
    vals[0]  = item.filePath.toUtf8();
    vals[1]  = item.title.toUtf8();
    vals[2]  = item.artist.toUtf8();
    vals[3]  = item.albumArtist.toUtf8();
    vals[4]  = item.album.toUtf8();
    vals[5]  = item.genre.toUtf8();
    vals[6]  = item.year.toUtf8();
    vals[7]  = item.trackNumber.toUtf8();
    vals[8]  = item.composer.toUtf8();
    vals[9]  = item.comment.toUtf8();
    vals[10] = item.isrc.toUtf8();
    vals[11] = item.mbid.toUtf8();
    vals[12] = item.label.toUtf8();
    vals[13] = item.language.toUtf8();
    vals[14] = QString::number(item.bpm).toUtf8();
    vals[15] = item.musicalKey.toUtf8();
    vals[16] = QString::number(item.durationMs).toUtf8();
    vals[17] = QString::number(item.bitrate).toUtf8();
    vals[18] = QString::number(item.sampleRate).toUtf8();
    vals[19] = QString::number(item.channels).toUtf8();
    vals[20] = item.codec.toUtf8();
    vals[21] = QString::number(item.fileSize).toUtf8();
    vals[22] = QString::number(item.rating).toUtf8();
    vals[23] = QString::number(item.energy).toUtf8();
    vals[24] = QString::number(item.weight).toUtf8();
    vals[25] = item.mood.toUtf8();
    vals[26] = item.tags.toUtf8();
    vals[27] = QByteArray(item.explicit_ ? "true" : "false");

    const char* paramValues[28];
    for (int i = 0; i < 28; ++i)
        paramValues[i] = vals[i].constData();

    PGresult* res = PQexecParams(toConn(m_conn), sql,
        28, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
        qWarning() << "[PostgresManager] INSERT failed:" << m_lastError;
        PQclear(res);
        return false;
    }

    item.id = fromField(res, 0, 0).toLongLong();
    PQclear(res);
    return true;
}

// ─── Update ───────────────────────────────────────────────────────────────────
bool PostgresManager::updateItem(const MediaItem& item) {
    if (!m_conn || item.id <= 0) return false;

    const char* sql =
        "UPDATE media_items SET "
        "title=$1,artist=$2,album=$3,genre=$4,"
        "bpm=$5,rating=$6,energy=$7,mbid=$8,"
        "date_modified=NOW() WHERE id=$9";

    QByteArray vals[9];
    vals[0] = item.title.toUtf8();
    vals[1] = item.artist.toUtf8();
    vals[2] = item.album.toUtf8();
    vals[3] = item.genre.toUtf8();
    vals[4] = QString::number(item.bpm).toUtf8();
    vals[5] = QString::number(item.rating).toUtf8();
    vals[6] = QString::number(item.energy).toUtf8();
    vals[7] = item.mbid.toUtf8();
    vals[8] = QString::number(item.id).toUtf8();

    const char* paramValues[9];
    for (int i = 0; i < 9; ++i)
        paramValues[i] = vals[i].constData();

    PGresult* res = PQexecParams(toConn(m_conn), sql,
        9, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

// ─── Delete ───────────────────────────────────────────────────────────────────
bool PostgresManager::deleteItem(qint64 id) {
    if (!m_conn) return false;

    QByteArray idStr = QString::number(id).toUtf8();
    const char* paramValues[1] = { idStr.constData() };

    PGresult* res = PQexecParams(toConn(m_conn),
        "DELETE FROM media_items WHERE id=$1",
        1, nullptr, paramValues, nullptr, nullptr, 0);

    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!ok) m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
    PQclear(res);
    return ok;
}

// ─── Load all ─────────────────────────────────────────────────────────────────
QList<MediaItem> PostgresManager::loadAll() {
    QList<MediaItem> items;
    if (!m_conn) return items;

    PGresult* res = PQexec(toConn(m_conn),
        "SELECT id,file_path,title,artist,album_artist,album,genre,year,"
        "track_number,composer,comment,isrc,mbid,label,language,bpm,musical_key,"
        "duration_ms,bitrate,sample_rate,channels,codec,file_size,rating,energy,"
        "weight,mood,tags,explicit_content,date_added,date_modified,last_played,"
        "play_count FROM media_items ORDER BY artist,album,track_number");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
        PQclear(res);
        return items;
    }

    const int rowCount = PQntuples(res);
    items.reserve(rowCount);

    for (int r = 0; r < rowCount; ++r) {
        MediaItem item;
        int c = 0;
        item.id           = fromField(res, r, c++).toLongLong();
        item.filePath     = fromField(res, r, c++);
        item.title        = fromField(res, r, c++);
        item.artist       = fromField(res, r, c++);
        item.albumArtist  = fromField(res, r, c++);
        item.album        = fromField(res, r, c++);
        item.genre        = fromField(res, r, c++);
        item.year         = fromField(res, r, c++);
        item.trackNumber  = fromField(res, r, c++);
        item.composer     = fromField(res, r, c++);
        item.comment      = fromField(res, r, c++);
        item.isrc         = fromField(res, r, c++);
        item.mbid         = fromField(res, r, c++);
        item.label        = fromField(res, r, c++);
        item.language     = fromField(res, r, c++);
        item.bpm          = fromField(res, r, c++).toDouble();
        item.musicalKey   = fromField(res, r, c++);
        item.durationMs   = fromField(res, r, c++).toInt();
        item.bitrate      = fromField(res, r, c++).toInt();
        item.sampleRate   = fromField(res, r, c++).toInt();
        item.channels     = fromField(res, r, c++).toInt();
        item.codec        = fromField(res, r, c++);
        item.fileSize     = fromField(res, r, c++).toLongLong();
        item.rating       = fromField(res, r, c++).toInt();
        item.energy       = fromField(res, r, c++).toDouble();
        item.weight       = fromField(res, r, c++).toDouble();
        item.mood         = fromField(res, r, c++);
        item.tags         = fromField(res, r, c++);
        item.explicit_    = (fromField(res, r, c++) == "t");
        item.dateAdded    = QDateTime::fromString(fromField(res, r, c++), Qt::ISODate);
        item.dateModified = QDateTime::fromString(fromField(res, r, c++), Qt::ISODate);
        item.lastPlayed   = QDateTime::fromString(fromField(res, r, c++), Qt::ISODate);
        item.playCount    = fromField(res, r, c++).toInt();
        items.append(item);
    }

    PQclear(res);
    return items;
}

// ─── Raw query execution ─────────────────────────────────────────────────────
QList<QVariantList> PostgresManager::executeQuery(const QString& sql) {
    QList<QVariantList> results;
    if (!m_conn) { m_lastError = "Not connected"; return results; }

    PGresult* res = PQexec(toConn(m_conn), sql.toUtf8().constData());
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
        PQclear(res);
        return results;
    }

    if (status == PGRES_TUPLES_OK) {
        const int nRows = PQntuples(res);
        const int nCols = PQnfields(res);
        results.reserve(nRows);
        for (int r = 0; r < nRows; ++r) {
            QVariantList row;
            row.reserve(nCols);
            for (int c = 0; c < nCols; ++c) {
                if (PQgetisnull(res, r, c))
                    row.append(QVariant());
                else
                    row.append(fromField(res, r, c));
            }
            results.append(row);
        }
    }

    PQclear(res);
    return results;
}

QStringList PostgresManager::tableNames() {
    QStringList names;
    if (!m_conn) return names;

    PGresult* res = PQexec(toConn(m_conn),
        "SELECT tablename FROM pg_tables WHERE schemaname='public' ORDER BY tablename");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return names;
    }

    const int nRows = PQntuples(res);
    for (int r = 0; r < nRows; ++r)
        names.append(fromField(res, r, 0));

    PQclear(res);
    return names;
}

// ─── Internal helpers ─────────────────────────────────────────────────────────
bool PostgresManager::execSql(const QString& sql) {
    if (!m_conn) { m_lastError = "Not connected"; return false; }

    PGresult* res = PQexec(toConn(m_conn), sql.toUtf8().constData());
    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        m_lastError = QString::fromUtf8(PQresultErrorMessage(res));
        qWarning() << "[PostgresManager] SQL error:" << m_lastError;
        PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

QString PostgresManager::escapeString(const QString& s) {
    if (!m_conn || s.isEmpty()) return s;
    QByteArray utf8 = s.toUtf8();
    char* escaped = PQescapeLiteral(toConn(m_conn), utf8.constData(),
                                     static_cast<size_t>(utf8.size()));
    if (!escaped) return s;
    // Strip the surrounding quotes since our callers add their own
    QString result = QString::fromUtf8(escaped);
    if (result.startsWith('\'') && result.endsWith('\''))
        result = result.mid(1, result.size() - 2);
    PQfreemem(escaped);
    return result;
}

} // namespace M1

#else
// ═════════════════════════════════════════════════════════════════════════════
//  Stub implementation — compiled when PostgreSQL is NOT available
// ═════════════════════════════════════════════════════════════════════════════

namespace M1 {

PostgresManager::PostgresManager() = default;
PostgresManager::~PostgresManager() = default;

bool PostgresManager::connect(const Config&) {
    m_lastError = "PostgreSQL driver not available. Add libpq to vcpkg.json to enable.";
    qWarning() << "[PostgresManager]" << m_lastError;
    return false;
}

void    PostgresManager::disconnect()                  {}
bool    PostgresManager::createSchema()                { return false; }
bool    PostgresManager::insertItem(MediaItem&)        { return false; }
bool    PostgresManager::updateItem(const MediaItem&)  { return false; }
bool    PostgresManager::deleteItem(qint64)            { return false; }
QList<MediaItem> PostgresManager::loadAll()            { return {}; }
bool    PostgresManager::pathExists(const QString&)    { return false; }
QList<QVariantList> PostgresManager::executeQuery(const QString&) { return {}; }
QStringList PostgresManager::tableNames()              { return {}; }
bool    PostgresManager::execSql(const QString&)       { return false; }
QString PostgresManager::escapeString(const QString& s) { return s; }
bool    PostgresManager::ensureDatabase(const Config&) { return false; }

} // namespace M1

#endif // M1_HAS_POSTGRESQL
