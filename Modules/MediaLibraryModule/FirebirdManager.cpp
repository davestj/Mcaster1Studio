#include "FirebirdManager.h"
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>

#ifdef M1_HAS_FIREBIRD
// ═════════════════════════════════════════════════════════════════════════════
//  Full Firebird implementation — only compiled when fbclient is available
// ═════════════════════════════════════════════════════════════════════════════

#include <ibase.h>

#include <cstdlib>
#include <cstring>

namespace M1 {

// ─── Helpers ─────────────────────────────────────────────────────────────────

/// Build a Database Parameter Block for authentication + UTF-8 charset.
static QByteArray buildDpb(const QString& user, const QString& password) {
    QByteArray dpb;
    dpb.append(static_cast<char>(isc_dpb_version1));

    // User name
    if (!user.isEmpty()) {
        QByteArray u = user.toUtf8();
        dpb.append(static_cast<char>(isc_dpb_user_name));
        dpb.append(static_cast<char>(u.size()));
        dpb.append(u);
    }

    // Password
    if (!password.isEmpty()) {
        QByteArray p = password.toUtf8();
        dpb.append(static_cast<char>(isc_dpb_password));
        dpb.append(static_cast<char>(p.size()));
        dpb.append(p);
    }

    // Force UTF-8 character set
    {
        const char* cs = "UTF8";
        dpb.append(static_cast<char>(isc_dpb_lc_ctype));
        dpb.append(static_cast<char>(4));
        dpb.append(cs, 4);
    }

    // SQL dialect 3
    {
        dpb.append(static_cast<char>(isc_dpb_sql_dialect));
        dpb.append(static_cast<char>(1));
        dpb.append(static_cast<char>(SQL_DIALECT_V6));
    }

    return dpb;
}

/// Read a column value from an XSQLVAR into a QString.
static QString readXsqlVar(const XSQLVAR& var) {
    // Check NULL indicator
    if (var.sqlind && *var.sqlind == -1)
        return {};

    const short dtype = var.sqltype & ~1;  // strip nullable bit

    switch (dtype) {
    case SQL_TEXT: {
        // Fixed-length CHAR — strip trailing spaces
        return QString::fromUtf8(var.sqldata, var.sqllen).trimmed();
    }
    case SQL_VARYING: {
        // VARCHAR — first 2 bytes are length
        ISC_SHORT len = *reinterpret_cast<const ISC_SHORT*>(var.sqldata);
        return QString::fromUtf8(var.sqldata + sizeof(ISC_SHORT), len);
    }
    case SQL_SHORT:
        return QString::number(*reinterpret_cast<const ISC_SHORT*>(var.sqldata));
    case SQL_LONG:
        return QString::number(*reinterpret_cast<const ISC_LONG*>(var.sqldata));
    case SQL_INT64:
        return QString::number(*reinterpret_cast<const ISC_INT64*>(var.sqldata));
    case SQL_FLOAT:
        return QString::number(static_cast<double>(*reinterpret_cast<const float*>(var.sqldata)));
    case SQL_DOUBLE:
        return QString::number(*reinterpret_cast<const double*>(var.sqldata));
    case SQL_TIMESTAMP: {
        // Firebird ISC_TIMESTAMP → QDateTime
        const auto* ts = reinterpret_cast<const ISC_TIMESTAMP*>(var.sqldata);
        // ISC_DATE: days since 17 Nov 1858 (Modified Julian Day)
        // ISC_TIME: fractions of a day (10000ths of a second since midnight)
        struct tm t;
        std::memset(&t, 0, sizeof(t));
        isc_decode_timestamp(const_cast<ISC_TIMESTAMP*>(ts), &t);
        QDateTime dt(QDate(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday),
                     QTime(t.tm_hour, t.tm_min, t.tm_sec));
        return dt.toString(Qt::ISODate);
    }
    case SQL_TYPE_DATE: {
        struct tm t;
        std::memset(&t, 0, sizeof(t));
        auto* d = reinterpret_cast<ISC_DATE*>(var.sqldata);
        isc_decode_sql_date(d, &t);
        return QDate(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday).toString(Qt::ISODate);
    }
    case SQL_TYPE_TIME: {
        struct tm t;
        std::memset(&t, 0, sizeof(t));
        auto* tm_ = reinterpret_cast<ISC_TIME*>(var.sqldata);
        isc_decode_sql_time(tm_, &t);
        return QTime(t.tm_hour, t.tm_min, t.tm_sec).toString(Qt::ISODate);
    }
    case SQL_BLOB:
        return QStringLiteral("[BLOB]");
    default:
        return QStringLiteral("[unknown type %1]").arg(dtype);
    }
}

/// Allocate data buffers for each column in an XSQLDA.
static void allocateXsqlda(XSQLDA* sqlda) {
    for (int i = 0; i < sqlda->sqld; ++i) {
        XSQLVAR& var = sqlda->sqlvar[i];
        const short dtype = var.sqltype & ~1;

        switch (dtype) {
        case SQL_VARYING:
            var.sqldata = static_cast<ISC_SCHAR*>(std::calloc(1, var.sqllen + sizeof(ISC_SHORT)));
            break;
        case SQL_TIMESTAMP:
            var.sqldata = static_cast<ISC_SCHAR*>(std::calloc(1, sizeof(ISC_TIMESTAMP)));
            break;
        case SQL_TYPE_DATE:
            var.sqldata = static_cast<ISC_SCHAR*>(std::calloc(1, sizeof(ISC_DATE)));
            break;
        case SQL_TYPE_TIME:
            var.sqldata = static_cast<ISC_SCHAR*>(std::calloc(1, sizeof(ISC_TIME)));
            break;
        default:
            var.sqldata = static_cast<ISC_SCHAR*>(std::calloc(1, var.sqllen + 2));
            break;
        }
        var.sqlind = static_cast<ISC_SHORT*>(std::calloc(1, sizeof(ISC_SHORT)));

        // Set nullable flag so the engine knows to populate sqlind
        if (!(var.sqltype & 1))
            var.sqltype |= 1;
    }
}

/// Free data buffers allocated by allocateXsqlda().
static void freeXsqlda(XSQLDA* sqlda) {
    if (!sqlda) return;
    for (int i = 0; i < sqlda->sqld; ++i) {
        std::free(sqlda->sqlvar[i].sqldata);
        std::free(sqlda->sqlvar[i].sqlind);
        sqlda->sqlvar[i].sqldata = nullptr;
        sqlda->sqlvar[i].sqlind  = nullptr;
    }
}

// ─── Constructor / Destructor ─────────────────────────────────────────────

FirebirdManager::FirebirdManager() = default;

FirebirdManager::~FirebirdManager() {
    disconnect();
}

// ─── Connection ───────────────────────────────────────────────────────────

QString FirebirdManager::buildConnectionString(const Config& cfg) const {
    if (cfg.host.isEmpty() || cfg.host == "localhost" || cfg.host == "127.0.0.1") {
        // Embedded mode — just the local file path
        // But if host is localhost and database looks like a remote path, use network
        if (cfg.host.isEmpty())
            return cfg.database;
    }

    // Network mode: host/port:database
    if (cfg.port != 0 && cfg.port != 3050)
        return QString("%1/%2:%3").arg(cfg.host).arg(cfg.port).arg(cfg.database);
    return QString("%1:%2").arg(cfg.host).arg(cfg.database);
}

bool FirebirdManager::connect(const Config& cfg) {
    if (m_connected) disconnect();

    // Ensure the database exists (create if needed)
    if (!ensureDatabase(cfg))
        return false;

    // Build connection string and DPB
    const QString connStr = buildConnectionString(cfg);
    const QByteArray connUtf8 = connStr.toUtf8();
    const QByteArray dpb = buildDpb(cfg.user, cfg.password);

    ISC_STATUS_ARRAY status;
    m_dbHandle = 0;

    if (isc_attach_database(status,
                            static_cast<short>(connUtf8.size()),
                            connUtf8.constData(),
                            reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                            static_cast<short>(dpb.size()),
                            dpb.constData())) {
        m_lastError = extractError();
        qWarning() << "[FirebirdManager] Attach failed:" << m_lastError;
        m_dbHandle = 0;
        return false;
    }

    m_connected = true;

    if (!createSchema()) return false;

    qInfo() << "[FirebirdManager] Connected:" << connStr;
    return true;
}

bool FirebirdManager::ensureDatabase(const Config& cfg) {
    // For embedded mode, check if the file exists
    const QString connStr = buildConnectionString(cfg);
    const QByteArray connUtf8 = connStr.toUtf8();
    const QByteArray dpb = buildDpb(cfg.user, cfg.password);

    // Try to attach first — if it works, the database already exists
    ISC_STATUS_ARRAY status;
    isc_db_handle testDb = 0;

    if (isc_attach_database(status,
                            static_cast<short>(connUtf8.size()),
                            connUtf8.constData(),
                            &testDb,
                            static_cast<short>(dpb.size()),
                            dpb.constData()) == 0) {
        // Database exists — detach and return success
        isc_detach_database(status, &testDb);
        return true;
    }

    // Database doesn't exist — create it
    // Build CREATE DATABASE SQL
    const QString createSql = QString(
        "CREATE DATABASE '%1' USER '%2' PASSWORD '%3' "
        "PAGE_SIZE 8192 DEFAULT CHARACTER SET UTF8")
        .arg(connStr)
        .arg(cfg.user)
        .arg(cfg.password);

    const QByteArray createUtf8 = createSql.toUtf8();
    isc_db_handle newDb = 0;
    isc_tr_handle tr = 0;

    if (isc_dsql_execute_immediate(status, &newDb, &tr,
                                    0,  // auto-detect length
                                    createUtf8.constData(),
                                    SQL_DIALECT_V6,
                                    nullptr)) {
        m_lastError = QString("Cannot create database: ");
        // Extract error from status
        char buf[512];
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        qWarning() << "[FirebirdManager] CREATE DATABASE failed:" << m_lastError;
        return false;
    }

    // Detach — we'll re-attach in connect()
    if (newDb) {
        isc_detach_database(status, &newDb);
    }

    qInfo() << "[FirebirdManager] Created database:" << connStr;
    return true;
}

void FirebirdManager::disconnect() {
    if (m_connected && m_dbHandle) {
        ISC_STATUS_ARRAY status;
        isc_detach_database(status, reinterpret_cast<isc_db_handle*>(&m_dbHandle));
        m_dbHandle = 0;
        m_connected = false;
        qInfo() << "[FirebirdManager] Disconnected.";
    }
    m_connected = false;
}

// ─── Error extraction ─────────────────────────────────────────────────────

QString FirebirdManager::extractError() {
    ISC_STATUS_ARRAY status;
    // Read back the status from the most recent call
    // We use the thread-local status from isc_* calls
    // Actually, we need to use the status array from the failed call.
    // This is called right after a failure, so we reconstruct from fb_interpret.
    return m_lastError;  // Set by callers with fb_interpret
}

// ─── Transaction helpers ─────────────────────────────────────────────────

bool FirebirdManager::beginTransaction(unsigned int& trHandle) {
    trHandle = 0;

    // Transaction Parameter Block: read_committed + rec_version + write + nowait
    static const char tpb[] = {
        isc_tpb_version3,
        isc_tpb_write,
        isc_tpb_read_committed,
        isc_tpb_rec_version,
        isc_tpb_nowait
    };

    ISC_STATUS_ARRAY status;
    if (isc_start_transaction(status,
                              reinterpret_cast<isc_tr_handle*>(&trHandle),
                              1,  // number of databases
                              reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                              static_cast<unsigned short>(sizeof(tpb)),
                              tpb)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        qWarning() << "[FirebirdManager] Begin transaction failed:" << m_lastError;
        return false;
    }
    return true;
}

bool FirebirdManager::commitTransaction(unsigned int& trHandle) {
    ISC_STATUS_ARRAY status;
    if (isc_commit_transaction(status, reinterpret_cast<isc_tr_handle*>(&trHandle))) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        return false;
    }
    trHandle = 0;
    return true;
}

void FirebirdManager::rollbackTransaction(unsigned int& trHandle) {
    if (trHandle) {
        ISC_STATUS_ARRAY status;
        isc_rollback_transaction(status, reinterpret_cast<isc_tr_handle*>(&trHandle));
        trHandle = 0;
    }
}

// ─── Execute immediate (DDL / simple non-query SQL) ───────────────────────

bool FirebirdManager::execImmediate(const QString& sql) {
    if (!m_connected) { m_lastError = "Not connected"; return false; }

    unsigned int tr = 0;
    if (!beginTransaction(tr)) return false;

    ISC_STATUS_ARRAY status;
    const QByteArray sqlUtf8 = sql.toUtf8();

    if (isc_dsql_execute_immediate(status,
                                    reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                                    reinterpret_cast<isc_tr_handle*>(&tr),
                                    0,
                                    sqlUtf8.constData(),
                                    SQL_DIALECT_V6,
                                    nullptr)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        qWarning() << "[FirebirdManager] SQL error:" << m_lastError << "\n  SQL:" << sql;
        rollbackTransaction(tr);
        return false;
    }

    return commitTransaction(tr);
}

// ─── Schema creation ─────────────────────────────────────────────────────

bool FirebirdManager::createSchema() {
    // Firebird 3+ supports GENERATED BY DEFAULT AS IDENTITY
    // Check if table exists first by querying RDB$RELATIONS
    const auto tables = tableNames();
    if (tables.contains("MEDIA_ITEMS", Qt::CaseInsensitive))
        return true;

    // Firebird: uppercase identifiers by convention, VARCHAR max 32765
    const char* sql = R"SQL(
        CREATE TABLE MEDIA_ITEMS (
            ID               INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY,
            FILE_PATH        VARCHAR(2048)      NOT NULL UNIQUE,
            TITLE            VARCHAR(512),
            ARTIST           VARCHAR(512),
            ALBUM_ARTIST     VARCHAR(512),
            ALBUM            VARCHAR(512),
            GENRE            VARCHAR(256),
            YEAR_            VARCHAR(10),
            TRACK_NUMBER     VARCHAR(10),
            COMPOSER         VARCHAR(512),
            COMMENT_         VARCHAR(8000),
            ISRC             VARCHAR(32),
            MBID             VARCHAR(64),
            LABEL_           VARCHAR(256),
            LANGUAGE_        VARCHAR(64),
            BPM              DOUBLE PRECISION   DEFAULT 0,
            MUSICAL_KEY      VARCHAR(16),
            DURATION_MS      INTEGER            DEFAULT 0,
            BITRATE          INTEGER            DEFAULT 0,
            SAMPLE_RATE      INTEGER            DEFAULT 44100,
            CHANNELS         INTEGER            DEFAULT 2,
            CODEC            VARCHAR(32),
            FILE_SIZE        BIGINT             DEFAULT 0,
            RATING           INTEGER            DEFAULT 0,
            ENERGY           DOUBLE PRECISION   DEFAULT 0,
            WEIGHT           DOUBLE PRECISION   DEFAULT 1,
            MOOD             VARCHAR(256),
            TAGS             VARCHAR(8000),
            EXPLICIT_CONTENT SMALLINT           DEFAULT 0,
            DATE_ADDED       TIMESTAMP,
            DATE_MODIFIED    TIMESTAMP,
            LAST_PLAYED      TIMESTAMP,
            PLAY_COUNT       INTEGER            DEFAULT 0,
            WAVEFORM_CACHE   VARCHAR(2048)
        )
    )SQL";

    return execImmediate(QString::fromUtf8(sql));
}

// ─── Path check ───────────────────────────────────────────────────────────

bool FirebirdManager::pathExists(const QString& path) {
    if (!m_connected) return false;

    unsigned int tr = 0;
    if (!beginTransaction(tr)) return false;

    ISC_STATUS_ARRAY status;
    isc_stmt_handle stmt = 0;

    // Allocate statement
    if (isc_dsql_allocate_statement(status,
                                     reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                                     &stmt)) {
        rollbackTransaction(tr);
        return false;
    }

    // Prepare — use parameterized query
    // Firebird uses ? placeholders
    const char* sql = "SELECT FIRST 1 ID FROM MEDIA_ITEMS WHERE FILE_PATH = ?";
    XSQLDA* inSqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(1)));
    inSqlda->version = SQLDA_VERSION1;
    inSqlda->sqln = 1;
    inSqlda->sqld = 1;

    XSQLDA* outSqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(1)));
    outSqlda->version = SQLDA_VERSION1;
    outSqlda->sqln = 1;

    if (isc_dsql_prepare(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt,
                          0, sql, SQL_DIALECT_V6, outSqlda)) {
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(inSqlda);
        std::free(outSqlda);
        rollbackTransaction(tr);
        return false;
    }

    // Bind input parameter
    QByteArray pathUtf8 = path.toUtf8();
    ISC_SHORT pathLen = static_cast<ISC_SHORT>(pathUtf8.size());

    // Varying: length prefix + data
    QByteArray varyBuf(sizeof(ISC_SHORT) + pathUtf8.size(), '\0');
    std::memcpy(varyBuf.data(), &pathLen, sizeof(ISC_SHORT));
    std::memcpy(varyBuf.data() + sizeof(ISC_SHORT), pathUtf8.constData(), pathUtf8.size());

    inSqlda->sqlvar[0].sqltype = SQL_VARYING + 1;  // nullable
    inSqlda->sqlvar[0].sqllen  = static_cast<ISC_SHORT>(pathUtf8.size());
    inSqlda->sqlvar[0].sqldata = varyBuf.data();
    ISC_SHORT notNull = 0;
    inSqlda->sqlvar[0].sqlind  = &notNull;

    // Allocate output
    allocateXsqlda(outSqlda);

    // Execute
    if (isc_dsql_execute(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt, 1, inSqlda)) {
        freeXsqlda(outSqlda);
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(inSqlda);
        std::free(outSqlda);
        rollbackTransaction(tr);
        return false;
    }

    // Fetch one row
    bool found = (isc_dsql_fetch(status, &stmt, 1, outSqlda) == 0);

    freeXsqlda(outSqlda);
    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    std::free(inSqlda);
    std::free(outSqlda);
    commitTransaction(tr);
    return found;
}

// ─── Insert ───────────────────────────────────────────────────────────────

bool FirebirdManager::insertItem(MediaItem& item) {
    if (!m_connected) return false;

    unsigned int tr = 0;
    if (!beginTransaction(tr)) return false;

    ISC_STATUS_ARRAY status;
    isc_stmt_handle stmt = 0;

    if (isc_dsql_allocate_statement(status,
                                     reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                                     &stmt)) {
        m_lastError = "Failed to allocate statement";
        rollbackTransaction(tr);
        return false;
    }

    // Firebird: use RETURNING to get the generated ID
    const char* sql =
        "INSERT INTO MEDIA_ITEMS ("
        "  FILE_PATH,TITLE,ARTIST,ALBUM_ARTIST,ALBUM,GENRE,YEAR_,TRACK_NUMBER,"
        "  COMPOSER,COMMENT_,ISRC,MBID,LABEL_,LANGUAGE_,BPM,MUSICAL_KEY,DURATION_MS,"
        "  BITRATE,SAMPLE_RATE,CHANNELS,CODEC,FILE_SIZE,RATING,ENERGY,WEIGHT,"
        "  MOOD,TAGS,EXPLICIT_CONTENT,DATE_ADDED,DATE_MODIFIED"
        ") VALUES ("
        "  ?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,"
        "  ?,?,?,?,?,?,?,?,"
        "  ?,?,?,CURRENT_TIMESTAMP,CURRENT_TIMESTAMP"
        ") RETURNING ID";

    // Output SQLDA for RETURNING ID
    XSQLDA* outSqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(1)));
    outSqlda->version = SQLDA_VERSION1;
    outSqlda->sqln = 1;

    if (isc_dsql_prepare(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt,
                          0, sql, SQL_DIALECT_V6, outSqlda)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        qWarning() << "[FirebirdManager] INSERT prepare failed:" << m_lastError;
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(outSqlda);
        rollbackTransaction(tr);
        return false;
    }

    // Describe output (RETURNING clause)
    isc_dsql_describe(status, &stmt, 1, outSqlda);
    allocateXsqlda(outSqlda);

    // Input: 28 parameters
    static constexpr int PARAM_COUNT = 28;
    XSQLDA* inSqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(PARAM_COUNT)));
    inSqlda->version = SQLDA_VERSION1;
    inSqlda->sqln = PARAM_COUNT;
    inSqlda->sqld = PARAM_COUNT;

    // Describe input parameters
    isc_dsql_describe_bind(status, &stmt, 1, inSqlda);

    // Prepare string values
    QByteArray strVals[PARAM_COUNT];
    strVals[0]  = item.filePath.toUtf8();
    strVals[1]  = item.title.toUtf8();
    strVals[2]  = item.artist.toUtf8();
    strVals[3]  = item.albumArtist.toUtf8();
    strVals[4]  = item.album.toUtf8();
    strVals[5]  = item.genre.toUtf8();
    strVals[6]  = item.year.toUtf8();
    strVals[7]  = item.trackNumber.toUtf8();
    strVals[8]  = item.composer.toUtf8();
    strVals[9]  = item.comment.toUtf8();
    strVals[10] = item.isrc.toUtf8();
    strVals[11] = item.mbid.toUtf8();
    strVals[12] = item.label.toUtf8();
    strVals[13] = item.language.toUtf8();
    strVals[14] = QString::number(item.bpm).toUtf8();
    strVals[15] = item.musicalKey.toUtf8();
    strVals[16] = QString::number(item.durationMs).toUtf8();
    strVals[17] = QString::number(item.bitrate).toUtf8();
    strVals[18] = QString::number(item.sampleRate).toUtf8();
    strVals[19] = QString::number(item.channels).toUtf8();
    strVals[20] = item.codec.toUtf8();
    strVals[21] = QString::number(item.fileSize).toUtf8();
    strVals[22] = QString::number(item.rating).toUtf8();
    strVals[23] = QString::number(item.energy).toUtf8();
    strVals[24] = QString::number(item.weight).toUtf8();
    strVals[25] = item.mood.toUtf8();
    strVals[26] = item.tags.toUtf8();
    strVals[27] = QByteArray(item.explicit_ ? "1" : "0");

    // Bind each input parameter as SQL_VARYING
    QByteArray varyBufs[PARAM_COUNT];
    ISC_SHORT  indicators[PARAM_COUNT];

    for (int i = 0; i < PARAM_COUNT; ++i) {
        XSQLVAR& var = inSqlda->sqlvar[i];
        const short dtype = var.sqltype & ~1;
        indicators[i] = 0;  // not null
        var.sqlind = &indicators[i];

        if (dtype == SQL_VARYING || dtype == SQL_TEXT) {
            ISC_SHORT len = static_cast<ISC_SHORT>(strVals[i].size());
            varyBufs[i].resize(sizeof(ISC_SHORT) + strVals[i].size());
            std::memcpy(varyBufs[i].data(), &len, sizeof(ISC_SHORT));
            std::memcpy(varyBufs[i].data() + sizeof(ISC_SHORT),
                        strVals[i].constData(), strVals[i].size());
            var.sqldata = varyBufs[i].data();
            var.sqllen  = len;
            var.sqltype = SQL_VARYING + 1;
        } else if (dtype == SQL_SHORT) {
            ISC_SHORT val = static_cast<ISC_SHORT>(strVals[i].toInt());
            varyBufs[i].resize(sizeof(ISC_SHORT));
            std::memcpy(varyBufs[i].data(), &val, sizeof(ISC_SHORT));
            var.sqldata = varyBufs[i].data();
        } else if (dtype == SQL_LONG) {
            ISC_LONG val = strVals[i].toInt();
            varyBufs[i].resize(sizeof(ISC_LONG));
            std::memcpy(varyBufs[i].data(), &val, sizeof(ISC_LONG));
            var.sqldata = varyBufs[i].data();
        } else if (dtype == SQL_INT64) {
            ISC_INT64 val = strVals[i].toLongLong();
            varyBufs[i].resize(sizeof(ISC_INT64));
            std::memcpy(varyBufs[i].data(), &val, sizeof(ISC_INT64));
            var.sqldata = varyBufs[i].data();
        } else if (dtype == SQL_DOUBLE) {
            double val = strVals[i].toDouble();
            varyBufs[i].resize(sizeof(double));
            std::memcpy(varyBufs[i].data(), &val, sizeof(double));
            var.sqldata = varyBufs[i].data();
        } else if (dtype == SQL_FLOAT) {
            float val = strVals[i].toFloat();
            varyBufs[i].resize(sizeof(float));
            std::memcpy(varyBufs[i].data(), &val, sizeof(float));
            var.sqldata = varyBufs[i].data();
        } else {
            // Fallback: treat as varying
            ISC_SHORT len = static_cast<ISC_SHORT>(strVals[i].size());
            varyBufs[i].resize(sizeof(ISC_SHORT) + strVals[i].size());
            std::memcpy(varyBufs[i].data(), &len, sizeof(ISC_SHORT));
            std::memcpy(varyBufs[i].data() + sizeof(ISC_SHORT),
                        strVals[i].constData(), strVals[i].size());
            var.sqldata = varyBufs[i].data();
            var.sqllen  = len;
            var.sqltype = SQL_VARYING + 1;
        }
    }

    // Execute with RETURNING → use isc_dsql_execute2
    if (isc_dsql_execute2(status,
                           reinterpret_cast<isc_tr_handle*>(&tr),
                           &stmt, 1, inSqlda, outSqlda)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        qWarning() << "[FirebirdManager] INSERT failed:" << m_lastError;
        freeXsqlda(outSqlda);
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(inSqlda);
        std::free(outSqlda);
        rollbackTransaction(tr);
        return false;
    }

    // Read returned ID
    item.id = readXsqlVar(outSqlda->sqlvar[0]).toLongLong();

    freeXsqlda(outSqlda);
    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    std::free(inSqlda);
    std::free(outSqlda);

    return commitTransaction(tr);
}

// ─── Update ───────────────────────────────────────────────────────────────

bool FirebirdManager::updateItem(const MediaItem& item) {
    if (!m_connected || item.id <= 0) return false;

    // Use simple string interpolation for the update (8 fields + WHERE)
    // Firebird doesn't have PQescapeLiteral, so we escape single quotes manually
    auto esc = [](const QString& s) -> QString {
        QString r = s;
        r.replace('\'', "''");
        return r;
    };

    const QString sql = QString(
        "UPDATE MEDIA_ITEMS SET "
        "TITLE='%1',ARTIST='%2',ALBUM='%3',GENRE='%4',"
        "BPM=%5,RATING=%6,ENERGY=%7,MBID='%8',"
        "DATE_MODIFIED=CURRENT_TIMESTAMP WHERE ID=%9")
        .arg(esc(item.title), esc(item.artist), esc(item.album), esc(item.genre))
        .arg(item.bpm)
        .arg(item.rating)
        .arg(item.energy)
        .arg(esc(item.mbid))
        .arg(item.id);

    return execImmediate(sql);
}

// ─── Delete ───────────────────────────────────────────────────────────────

bool FirebirdManager::deleteItem(qint64 id) {
    if (!m_connected) return false;
    return execImmediate(QString("DELETE FROM MEDIA_ITEMS WHERE ID=%1").arg(id));
}

// ─── Load all ─────────────────────────────────────────────────────────────

QList<MediaItem> FirebirdManager::loadAll() {
    QList<MediaItem> items;
    if (!m_connected) return items;

    unsigned int tr = 0;
    if (!beginTransaction(tr)) return items;

    ISC_STATUS_ARRAY status;
    isc_stmt_handle stmt = 0;

    if (isc_dsql_allocate_statement(status,
                                     reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                                     &stmt)) {
        rollbackTransaction(tr);
        return items;
    }

    const char* sql =
        "SELECT ID,FILE_PATH,TITLE,ARTIST,ALBUM_ARTIST,ALBUM,GENRE,YEAR_,"
        "TRACK_NUMBER,COMPOSER,COMMENT_,ISRC,MBID,LABEL_,LANGUAGE_,BPM,MUSICAL_KEY,"
        "DURATION_MS,BITRATE,SAMPLE_RATE,CHANNELS,CODEC,FILE_SIZE,RATING,ENERGY,"
        "WEIGHT,MOOD,TAGS,EXPLICIT_CONTENT,DATE_ADDED,DATE_MODIFIED,LAST_PLAYED,"
        "PLAY_COUNT FROM MEDIA_ITEMS ORDER BY ARTIST,ALBUM,TRACK_NUMBER";

    // Initial allocation with 1 column — we'll resize after describe
    XSQLDA* sqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(1)));
    sqlda->version = SQLDA_VERSION1;
    sqlda->sqln = 1;

    if (isc_dsql_prepare(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt,
                          0, sql, SQL_DIALECT_V6, sqlda)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(sqlda);
        rollbackTransaction(tr);
        return items;
    }

    // Reallocate if more columns than initially allocated
    if (sqlda->sqld > sqlda->sqln) {
        int n = sqlda->sqld;
        std::free(sqlda);
        sqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(n)));
        sqlda->version = SQLDA_VERSION1;
        sqlda->sqln = static_cast<ISC_SHORT>(n);
        isc_dsql_describe(status, &stmt, 1, sqlda);
    }

    allocateXsqlda(sqlda);

    // Execute
    if (isc_dsql_execute(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt, 1, nullptr)) {
        freeXsqlda(sqlda);
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(sqlda);
        rollbackTransaction(tr);
        return items;
    }

    // Fetch loop
    while (isc_dsql_fetch(status, &stmt, 1, sqlda) == 0) {
        MediaItem item;
        int c = 0;
        item.id           = readXsqlVar(sqlda->sqlvar[c++]).toLongLong();
        item.filePath     = readXsqlVar(sqlda->sqlvar[c++]);
        item.title        = readXsqlVar(sqlda->sqlvar[c++]);
        item.artist       = readXsqlVar(sqlda->sqlvar[c++]);
        item.albumArtist  = readXsqlVar(sqlda->sqlvar[c++]);
        item.album        = readXsqlVar(sqlda->sqlvar[c++]);
        item.genre        = readXsqlVar(sqlda->sqlvar[c++]);
        item.year         = readXsqlVar(sqlda->sqlvar[c++]);
        item.trackNumber  = readXsqlVar(sqlda->sqlvar[c++]);
        item.composer     = readXsqlVar(sqlda->sqlvar[c++]);
        item.comment      = readXsqlVar(sqlda->sqlvar[c++]);
        item.isrc         = readXsqlVar(sqlda->sqlvar[c++]);
        item.mbid         = readXsqlVar(sqlda->sqlvar[c++]);
        item.label        = readXsqlVar(sqlda->sqlvar[c++]);
        item.language     = readXsqlVar(sqlda->sqlvar[c++]);
        item.bpm          = readXsqlVar(sqlda->sqlvar[c++]).toDouble();
        item.musicalKey   = readXsqlVar(sqlda->sqlvar[c++]);
        item.durationMs   = readXsqlVar(sqlda->sqlvar[c++]).toInt();
        item.bitrate      = readXsqlVar(sqlda->sqlvar[c++]).toInt();
        item.sampleRate   = readXsqlVar(sqlda->sqlvar[c++]).toInt();
        item.channels     = readXsqlVar(sqlda->sqlvar[c++]).toInt();
        item.codec        = readXsqlVar(sqlda->sqlvar[c++]);
        item.fileSize     = readXsqlVar(sqlda->sqlvar[c++]).toLongLong();
        item.rating       = readXsqlVar(sqlda->sqlvar[c++]).toInt();
        item.energy       = readXsqlVar(sqlda->sqlvar[c++]).toDouble();
        item.weight       = readXsqlVar(sqlda->sqlvar[c++]).toDouble();
        item.mood         = readXsqlVar(sqlda->sqlvar[c++]);
        item.tags         = readXsqlVar(sqlda->sqlvar[c++]);
        item.explicit_    = (readXsqlVar(sqlda->sqlvar[c++]).toInt() != 0);
        item.dateAdded    = QDateTime::fromString(readXsqlVar(sqlda->sqlvar[c++]), Qt::ISODate);
        item.dateModified = QDateTime::fromString(readXsqlVar(sqlda->sqlvar[c++]), Qt::ISODate);
        item.lastPlayed   = QDateTime::fromString(readXsqlVar(sqlda->sqlvar[c++]), Qt::ISODate);
        item.playCount    = readXsqlVar(sqlda->sqlvar[c++]).toInt();
        items.append(item);
    }

    freeXsqlda(sqlda);
    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    std::free(sqlda);
    commitTransaction(tr);
    return items;
}

// ─── Raw query execution ─────────────────────────────────────────────────

QList<QVariantList> FirebirdManager::executeQuery(const QString& sql) {
    QList<QVariantList> results;
    if (!m_connected) { m_lastError = "Not connected"; return results; }

    unsigned int tr = 0;
    if (!beginTransaction(tr)) return results;

    ISC_STATUS_ARRAY status;
    isc_stmt_handle stmt = 0;

    if (isc_dsql_allocate_statement(status,
                                     reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                                     &stmt)) {
        rollbackTransaction(tr);
        return results;
    }

    const QByteArray sqlUtf8 = sql.toUtf8();

    // Initial: 1 output column
    XSQLDA* sqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(1)));
    sqlda->version = SQLDA_VERSION1;
    sqlda->sqln = 1;

    if (isc_dsql_prepare(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt,
                          0, sqlUtf8.constData(), SQL_DIALECT_V6, sqlda)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(sqlda);
        rollbackTransaction(tr);
        return results;
    }

    // Check if this is a DML/DDL (no output columns)
    if (sqlda->sqld == 0) {
        // Non-query statement — execute immediate
        if (isc_dsql_execute(status,
                              reinterpret_cast<isc_tr_handle*>(&tr),
                              &stmt, 1, nullptr)) {
            char buf[512];
            m_lastError.clear();
            const ISC_STATUS* pStatus = status;
            while (fb_interpret(buf, sizeof(buf), &pStatus))
                m_lastError += QString::fromUtf8(buf) + " ";
        }
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(sqlda);
        commitTransaction(tr);
        return results;
    }

    // Reallocate for actual column count
    if (sqlda->sqld > sqlda->sqln) {
        int n = sqlda->sqld;
        std::free(sqlda);
        sqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(n)));
        sqlda->version = SQLDA_VERSION1;
        sqlda->sqln = static_cast<ISC_SHORT>(n);
        isc_dsql_describe(status, &stmt, 1, sqlda);
    }

    allocateXsqlda(sqlda);

    // Execute
    if (isc_dsql_execute(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt, 1, nullptr)) {
        char buf[512];
        m_lastError.clear();
        const ISC_STATUS* pStatus = status;
        while (fb_interpret(buf, sizeof(buf), &pStatus))
            m_lastError += QString::fromUtf8(buf) + " ";
        freeXsqlda(sqlda);
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(sqlda);
        rollbackTransaction(tr);
        return results;
    }

    // Fetch rows
    const int nCols = sqlda->sqld;
    while (isc_dsql_fetch(status, &stmt, 1, sqlda) == 0) {
        QVariantList row;
        row.reserve(nCols);
        for (int c = 0; c < nCols; ++c) {
            if (sqlda->sqlvar[c].sqlind && *sqlda->sqlvar[c].sqlind == -1)
                row.append(QVariant());
            else
                row.append(readXsqlVar(sqlda->sqlvar[c]));
        }
        results.append(row);
    }

    freeXsqlda(sqlda);
    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    std::free(sqlda);
    commitTransaction(tr);
    return results;
}

// ─── Table names ─────────────────────────────────────────────────────────

QStringList FirebirdManager::tableNames() {
    QStringList names;
    if (!m_connected) return names;

    unsigned int tr = 0;
    if (!beginTransaction(tr)) return names;

    ISC_STATUS_ARRAY status;
    isc_stmt_handle stmt = 0;

    if (isc_dsql_allocate_statement(status,
                                     reinterpret_cast<isc_db_handle*>(&m_dbHandle),
                                     &stmt)) {
        rollbackTransaction(tr);
        return names;
    }

    // Query Firebird system table for user tables
    const char* sql =
        "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS "
        "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NULL "
        "ORDER BY RDB$RELATION_NAME";

    XSQLDA* sqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(1)));
    sqlda->version = SQLDA_VERSION1;
    sqlda->sqln = 1;

    if (isc_dsql_prepare(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt,
                          0, sql, SQL_DIALECT_V6, sqlda)) {
        isc_dsql_free_statement(status, &stmt, DSQL_drop);
        std::free(sqlda);
        rollbackTransaction(tr);
        return names;
    }

    if (sqlda->sqld > sqlda->sqln) {
        int n = sqlda->sqld;
        std::free(sqlda);
        sqlda = static_cast<XSQLDA*>(std::calloc(1, XSQLDA_LENGTH(n)));
        sqlda->version = SQLDA_VERSION1;
        sqlda->sqln = static_cast<ISC_SHORT>(n);
        isc_dsql_describe(status, &stmt, 1, sqlda);
    }

    allocateXsqlda(sqlda);

    if (isc_dsql_execute(status,
                          reinterpret_cast<isc_tr_handle*>(&tr),
                          &stmt, 1, nullptr) == 0) {
        while (isc_dsql_fetch(status, &stmt, 1, sqlda) == 0) {
            QString name = readXsqlVar(sqlda->sqlvar[0]).trimmed();
            if (!name.isEmpty())
                names.append(name);
        }
    }

    freeXsqlda(sqlda);
    isc_dsql_free_statement(status, &stmt, DSQL_drop);
    std::free(sqlda);
    commitTransaction(tr);
    return names;
}

} // namespace M1

#else
// ═════════════════════════════════════════════════════════════════════════════
//  Stub implementation — compiled when Firebird client library is NOT available
// ═════════════════════════════════════════════════════════════════════════════

namespace M1 {

FirebirdManager::FirebirdManager() = default;
FirebirdManager::~FirebirdManager() = default;

bool FirebirdManager::connect(const Config&) {
    m_lastError = "Firebird driver not available. Install Firebird 3+ (64-bit) to enable.";
    qWarning() << "[FirebirdManager]" << m_lastError;
    return false;
}

void    FirebirdManager::disconnect()                  {}
bool    FirebirdManager::createSchema()                { return false; }
bool    FirebirdManager::insertItem(MediaItem&)        { return false; }
bool    FirebirdManager::updateItem(const MediaItem&)  { return false; }
bool    FirebirdManager::deleteItem(qint64)            { return false; }
QList<MediaItem> FirebirdManager::loadAll()            { return {}; }
bool    FirebirdManager::pathExists(const QString&)    { return false; }
QList<QVariantList> FirebirdManager::executeQuery(const QString&) { return {}; }
QStringList FirebirdManager::tableNames()              { return {}; }
bool    FirebirdManager::execImmediate(const QString&) { return false; }
bool    FirebirdManager::beginTransaction(unsigned int&) { return false; }
bool    FirebirdManager::commitTransaction(unsigned int&) { return false; }
void    FirebirdManager::rollbackTransaction(unsigned int&) {}
QString FirebirdManager::buildConnectionString(const Config&) const { return {}; }
QString FirebirdManager::extractError() { return m_lastError; }
bool    FirebirdManager::ensureDatabase(const Config&) { return false; }

} // namespace M1

#endif // M1_HAS_FIREBIRD
