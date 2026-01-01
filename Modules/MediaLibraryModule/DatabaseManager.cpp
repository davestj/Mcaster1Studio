#include "DatabaseManager.h"
#include <mysql/mysql.h>
#include <QDebug>
#include <QDateTime>

namespace M1 {

// ─── Helpers ─────────────────────────────────────────────────────────────────
static MYSQL* toConn(void* p) { return static_cast<MYSQL*>(p); }

static QString fromRow(const char* s) {
    return s ? QString::fromUtf8(s) : QString();
}

// ─── Constructor / Destructor ─────────────────────────────────────────────────
DatabaseManager::DatabaseManager() = default;

DatabaseManager::~DatabaseManager() {
    disconnect();
}

// ─── Connection ───────────────────────────────────────────────────────────────
bool DatabaseManager::connect(const Config& cfg) {
    MYSQL* c = mysql_init(nullptr);
    if (!c) {
        m_lastError = "mysql_init() failed — out of memory";
        return false;
    }

    unsigned int timeout = 5;
    mysql_options(c, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(c, MYSQL_OPT_RECONNECT, "\x01");

    // Connect without selecting a specific database yet
    if (!mysql_real_connect(c,
            cfg.host.toUtf8().constData(),
            cfg.user.toUtf8().constData(),
            cfg.password.toUtf8().constData(),
            nullptr,
            static_cast<unsigned int>(cfg.port),
            nullptr, 0))
    {
        m_lastError = QString::fromUtf8(mysql_error(c));
        mysql_close(c);
        qWarning() << "[MediaLibrary DB] Connection failed:" << m_lastError;
        return false;
    }

    m_conn = c;

    // Create database if it doesn't exist
    QString createDb = QString(
        "CREATE DATABASE IF NOT EXISTS `%1` "
        "CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci")
        .arg(cfg.database);
    if (!execSql(createDb)) return false;

    // Select it
    if (mysql_select_db(toConn(m_conn), cfg.database.toUtf8().constData()) != 0) {
        m_lastError = QString::fromUtf8(mysql_error(toConn(m_conn)));
        return false;
    }

    if (!createSchema()) return false;

    qInfo() << "[MediaLibrary DB] Connected:" << cfg.host << "/" << cfg.database;
    return true;
}

void DatabaseManager::disconnect() {
    if (m_conn) {
        mysql_close(toConn(m_conn));
        m_conn = nullptr;
        qInfo() << "[MediaLibrary DB] Disconnected.";
    }
}

// ─── Schema ───────────────────────────────────────────────────────────────────
bool DatabaseManager::createSchema() {
    const char* sql = R"SQL(
        CREATE TABLE IF NOT EXISTS `media_items` (
            `id`               BIGINT AUTO_INCREMENT PRIMARY KEY,
            `file_path`        VARCHAR(2048)  NOT NULL,
            `title`            VARCHAR(512),
            `artist`           VARCHAR(512),
            `album_artist`     VARCHAR(512),
            `album`            VARCHAR(512),
            `genre`            VARCHAR(256),
            `year`             VARCHAR(10),
            `track_number`     VARCHAR(10),
            `composer`         VARCHAR(512),
            `comment`          TEXT,
            `isrc`             VARCHAR(32),
            `mbid`             VARCHAR(64),
            `label`            VARCHAR(256),
            `language`         VARCHAR(64),
            `bpm`              DOUBLE         DEFAULT 0,
            `musical_key`      VARCHAR(16),
            `duration_ms`      INT            DEFAULT 0,
            `bitrate`          INT            DEFAULT 0,
            `sample_rate`      INT            DEFAULT 44100,
            `channels`         INT            DEFAULT 2,
            `codec`            VARCHAR(32),
            `file_size`        BIGINT         DEFAULT 0,
            `rating`           INT            DEFAULT 0,
            `energy`           DOUBLE         DEFAULT 0,
            `weight`           DOUBLE         DEFAULT 1,
            `mood`             VARCHAR(256),
            `tags`             TEXT,
            `explicit_content` TINYINT(1)     DEFAULT 0,
            `date_added`       DATETIME,
            `date_modified`    DATETIME,
            `last_played`      DATETIME,
            `play_count`       INT            DEFAULT 0,
            `waveform_cache`   VARCHAR(2048),
            UNIQUE KEY `idx_file_path` (`file_path`(512))
        ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci
    )SQL";
    return execSql(QString::fromUtf8(sql));
}

// ─── Path check ───────────────────────────────────────────────────────────────
bool DatabaseManager::pathExists(const QString& path) {
    if (!m_conn) return false;
    QString sql = QString("SELECT id FROM media_items WHERE file_path='%1' LIMIT 1")
                  .arg(escapeString(path));
    if (mysql_query(toConn(m_conn), sql.toUtf8().constData()) != 0) return false;
    MYSQL_RES* res = mysql_store_result(toConn(m_conn));
    if (!res) return false;
    bool found = mysql_num_rows(res) > 0;
    mysql_free_result(res);
    return found;
}

// ─── Insert ───────────────────────────────────────────────────────────────────
bool DatabaseManager::insertItem(MediaItem& item) {
    if (!m_conn) return false;

    // Build column list and value list separately to avoid arg() numbering issues
    QString cols =
        "file_path,title,artist,album_artist,album,genre,year,track_number,"
        "composer,comment,isrc,mbid,label,language,bpm,musical_key,duration_ms,"
        "bitrate,sample_rate,channels,codec,file_size,rating,energy,weight,"
        "mood,tags,explicit_content,date_added,date_modified";

    QStringList vals;
    auto esc = [&](const QString& s) -> QString {
        return '\'' + escapeString(s) + '\'';
    };
    vals << esc(item.filePath)
         << esc(item.title)
         << esc(item.artist)
         << esc(item.albumArtist)
         << esc(item.album)
         << esc(item.genre)
         << esc(item.year)
         << esc(item.trackNumber)
         << esc(item.composer)
         << esc(item.comment)
         << esc(item.isrc)
         << esc(item.mbid)
         << esc(item.label)
         << esc(item.language)
         << QString::number(item.bpm)
         << esc(item.musicalKey)
         << QString::number(item.durationMs)
         << QString::number(item.bitrate)
         << QString::number(item.sampleRate)
         << QString::number(item.channels)
         << esc(item.codec)
         << QString::number(item.fileSize)
         << QString::number(item.rating)
         << QString::number(item.energy)
         << QString::number(item.weight)
         << esc(item.mood)
         << esc(item.tags)
         << QString::number(item.explicit_ ? 1 : 0)
         << "NOW()"
         << "NOW()";

    QString sql = QString("INSERT INTO media_items (%1) VALUES (%2)")
                  .arg(cols)
                  .arg(vals.join(','));

    if (!execSql(sql)) return false;
    item.id = static_cast<qint64>(mysql_insert_id(toConn(m_conn)));
    return true;
}

// ─── Update ───────────────────────────────────────────────────────────────────
bool DatabaseManager::updateItem(const MediaItem& item) {
    if (!m_conn || item.id <= 0) return false;

    QString sql = QString(
        "UPDATE media_items SET "
        "title='%1',artist='%2',album='%3',genre='%4',"
        "bpm=%5,rating=%6,energy=%7,mbid='%8',date_modified=NOW() "
        "WHERE id=%9")
        .arg(escapeString(item.title))
        .arg(escapeString(item.artist))
        .arg(escapeString(item.album))
        .arg(escapeString(item.genre))
        .arg(item.bpm)
        .arg(item.rating)
        .arg(item.energy)
        .arg(escapeString(item.mbid))
        .arg(item.id);

    return execSql(sql);
}

// ─── Delete ───────────────────────────────────────────────────────────────────
bool DatabaseManager::deleteItem(qint64 id) {
    if (!m_conn) return false;
    return execSql(QString("DELETE FROM media_items WHERE id=%1").arg(id));
}

// ─── Load all ─────────────────────────────────────────────────────────────────
QList<MediaItem> DatabaseManager::loadAll() {
    QList<MediaItem> items;
    if (!m_conn) return items;

    const char* sql =
        "SELECT id,file_path,title,artist,album_artist,album,genre,year,"
        "track_number,composer,comment,isrc,mbid,label,language,bpm,musical_key,"
        "duration_ms,bitrate,sample_rate,channels,codec,file_size,rating,energy,"
        "weight,mood,tags,explicit_content,date_added,date_modified,last_played,play_count "
        "FROM media_items ORDER BY artist,album,track_number";

    if (mysql_query(toConn(m_conn), sql) != 0) {
        m_lastError = QString::fromUtf8(mysql_error(toConn(m_conn)));
        return items;
    }

    MYSQL_RES* res = mysql_store_result(toConn(m_conn));
    if (!res) return items;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        MediaItem item;
        int c = 0;
        item.id           = fromRow(row[c++]).toLongLong();
        item.filePath     = fromRow(row[c++]);
        item.title        = fromRow(row[c++]);
        item.artist       = fromRow(row[c++]);
        item.albumArtist  = fromRow(row[c++]);
        item.album        = fromRow(row[c++]);
        item.genre        = fromRow(row[c++]);
        item.year         = fromRow(row[c++]);
        item.trackNumber  = fromRow(row[c++]);
        item.composer     = fromRow(row[c++]);
        item.comment      = fromRow(row[c++]);
        item.isrc         = fromRow(row[c++]);
        item.mbid         = fromRow(row[c++]);
        item.label        = fromRow(row[c++]);
        item.language     = fromRow(row[c++]);
        item.bpm          = fromRow(row[c++]).toDouble();
        item.musicalKey   = fromRow(row[c++]);
        item.durationMs   = fromRow(row[c++]).toInt();
        item.bitrate      = fromRow(row[c++]).toInt();
        item.sampleRate   = fromRow(row[c++]).toInt();
        item.channels     = fromRow(row[c++]).toInt();
        item.codec        = fromRow(row[c++]);
        item.fileSize     = fromRow(row[c++]).toLongLong();
        item.rating       = fromRow(row[c++]).toInt();
        item.energy       = fromRow(row[c++]).toDouble();
        item.weight       = fromRow(row[c++]).toDouble();
        item.mood         = fromRow(row[c++]);
        item.tags         = fromRow(row[c++]);
        item.explicit_    = fromRow(row[c++]).toInt() != 0;
        item.dateAdded    = QDateTime::fromString(fromRow(row[c++]), Qt::ISODate);
        item.dateModified = QDateTime::fromString(fromRow(row[c++]), Qt::ISODate);
        item.lastPlayed   = QDateTime::fromString(fromRow(row[c++]), Qt::ISODate);
        item.playCount    = fromRow(row[c++]).toInt();
        items.append(item);
    }
    mysql_free_result(res);
    return items;
}

// ─── Internal helpers ─────────────────────────────────────────────────────────
bool DatabaseManager::execSql(const QString& sql) {
    if (!m_conn) { m_lastError = "Not connected"; return false; }
    QByteArray utf8 = sql.toUtf8();
    if (mysql_real_query(toConn(m_conn), utf8.constData(),
                         static_cast<unsigned long>(utf8.size())) != 0) {
        m_lastError = QString::fromUtf8(mysql_error(toConn(m_conn)));
        qWarning() << "[MediaLibrary DB] SQL error:" << m_lastError;
        return false;
    }
    // Consume any result set to keep the connection in a clean state
    MYSQL_RES* res = mysql_store_result(toConn(m_conn));
    if (res) mysql_free_result(res);
    return true;
}

QString DatabaseManager::escapeString(const QString& s) {
    if (!m_conn || s.isEmpty()) return s;
    QByteArray utf8 = s.toUtf8();
    QByteArray buf(utf8.size() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(
        toConn(m_conn), buf.data(), utf8.constData(),
        static_cast<unsigned long>(utf8.size()));
    return QString::fromUtf8(buf.constData(), static_cast<int>(len));
}

} // namespace M1
