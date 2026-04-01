#include "MssqlManager.h"
#include <QDebug>
#include <QDateTime>

#ifdef _WIN32
// ═════════════════════════════════════════════════════════════════════════════
//  Full ODBC implementation — always available on Windows (odbc32.lib)
// ═════════════════════════════════════════════════════════════════════════════

#include <windows.h>
#include <sql.h>
#include <sqlext.h>

namespace M1 {

// ─── Helpers ─────────────────────────────────────────────────────────────────

/// Check ODBC return code for success.
static bool sqlOk(SQLRETURN rc) {
    return (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO);
}

/// Read a wide-string column from the current row.
static QString getStringCol(SQLHSTMT stmt, SQLUSMALLINT col) {
    SQLWCHAR buf[4096];
    SQLLEN indicator = 0;
    SQLRETURN rc = SQLGetData(stmt, col, SQL_C_WCHAR, buf, sizeof(buf), &indicator);
    if (!sqlOk(rc) || indicator == SQL_NULL_DATA)
        return {};
    return QString::fromWCharArray(buf);
}

/// Read an integer column.
static qint64 getInt64Col(SQLHSTMT stmt, SQLUSMALLINT col) {
    SQLBIGINT val = 0;
    SQLLEN indicator = 0;
    SQLRETURN rc = SQLGetData(stmt, col, SQL_C_SBIGINT, &val, sizeof(val), &indicator);
    if (!sqlOk(rc) || indicator == SQL_NULL_DATA)
        return 0;
    return static_cast<qint64>(val);
}

/// Read a double column.
static double getDoubleCol(SQLHSTMT stmt, SQLUSMALLINT col) {
    SQLDOUBLE val = 0.0;
    SQLLEN indicator = 0;
    SQLRETURN rc = SQLGetData(stmt, col, SQL_C_DOUBLE, &val, sizeof(val), &indicator);
    if (!sqlOk(rc) || indicator == SQL_NULL_DATA)
        return 0.0;
    return val;
}

/// Read a timestamp column as ISO date string.
static QString getTimestampCol(SQLHSTMT stmt, SQLUSMALLINT col) {
    SQL_TIMESTAMP_STRUCT ts;
    memset(&ts, 0, sizeof(ts));
    SQLLEN indicator = 0;
    SQLRETURN rc = SQLGetData(stmt, col, SQL_C_TYPE_TIMESTAMP, &ts, sizeof(ts), &indicator);
    if (!sqlOk(rc) || indicator == SQL_NULL_DATA)
        return {};
    QDateTime dt(QDate(ts.year, ts.month, ts.day),
                 QTime(ts.hour, ts.minute, ts.second));
    return dt.toString(Qt::ISODate);
}

// ─── Constructor / Destructor ─────────────────────────────────────────────

MssqlManager::MssqlManager() = default;

MssqlManager::~MssqlManager() {
    disconnect();
}

// ─── ODBC driver detection ───────────────────────────────────────────────

QString MssqlManager::detectDriver() const {
    // Try newest to oldest — all work with all SQL Server editions
    static const char* candidates[] = {
        "ODBC Driver 18 for SQL Server",
        "ODBC Driver 17 for SQL Server",
        "ODBC Driver 13 for SQL Server",
        "SQL Server Native Client 11.0",
        "SQL Server"  // Legacy — always present on Windows
    };

    // Enumerate installed ODBC drivers via SQLDrivers()
    SQLHENV hEnv = SQL_NULL_HENV;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv)))
        return "SQL Server";  // Fallback

    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    QStringList installed;
    SQLWCHAR driverDesc[512];
    SQLWCHAR driverAttr[512];
    SQLSMALLINT descLen, attrLen;

    SQLRETURN rc = SQLDriversW(hEnv, SQL_FETCH_FIRST,
                               driverDesc, sizeof(driverDesc) / sizeof(SQLWCHAR), &descLen,
                               driverAttr, sizeof(driverAttr) / sizeof(SQLWCHAR), &attrLen);
    while (sqlOk(rc)) {
        installed.append(QString::fromWCharArray(driverDesc, descLen));
        rc = SQLDriversW(hEnv, SQL_FETCH_NEXT,
                         driverDesc, sizeof(driverDesc) / sizeof(SQLWCHAR), &descLen,
                         driverAttr, sizeof(driverAttr) / sizeof(SQLWCHAR), &attrLen);
    }
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);

    for (const char* candidate : candidates) {
        const QString name = QString::fromLatin1(candidate);
        for (const QString& drv : installed) {
            if (drv.compare(name, Qt::CaseInsensitive) == 0)
                return name;
        }
    }

    return "SQL Server";  // Ultimate fallback
}

// ─── Connection string ───────────────────────────────────────────────────

QString MssqlManager::buildConnectionString(const Config& cfg) const {
    const QString driver = detectDriver();

    // MSSQL uses comma for port: SERVER=host,port
    QString server = cfg.host;
    if (cfg.port != 0 && cfg.port != 1433)
        server += ',' + QString::number(cfg.port);

    QString cs = QString("DRIVER={%1};SERVER=%2;DATABASE=%3;")
                     .arg(driver, server, cfg.database);

    if (cfg.trustedConnection) {
        cs += "Trusted_Connection=yes;";
    } else {
        cs += QString("UID=%1;PWD=%2;").arg(cfg.user, cfg.password);
    }

    // ODBC Driver 18 defaults to Encrypt=Mandatory — override for local dev
    if (driver.contains("18"))
        cs += "TrustServerCertificate=yes;Encrypt=optional;";

    return cs;
}

// ─── Connection ──────────────────────────────────────────────────────────

bool MssqlManager::connect(const Config& cfg) {
    if (m_connected) disconnect();

    // Ensure the target database exists
    if (!ensureDatabase(cfg))
        return false;

    // Allocate ODBC environment
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_hEnv))) {
        m_lastError = "Failed to allocate ODBC environment handle";
        return false;
    }
    SQLSetEnvAttr(m_hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    // Allocate connection handle
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_DBC, m_hEnv, &m_hDbc))) {
        m_lastError = "Failed to allocate ODBC connection handle";
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = nullptr;
        return false;
    }

    // Set connection timeout
    SQLSetConnectAttr(m_hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

    // Connect
    const QString cs = buildConnectionString(cfg);
    std::wstring wcs = cs.toStdWString();
    SQLWCHAR outBuf[1024];
    SQLSMALLINT outLen;

    SQLRETURN rc = SQLDriverConnectW(
        static_cast<SQLHDBC>(m_hDbc),
        nullptr,
        reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wcs.c_str())),
        static_cast<SQLSMALLINT>(wcs.size()),
        outBuf,
        sizeof(outBuf) / sizeof(SQLWCHAR),
        &outLen,
        SQL_DRIVER_NOPROMPT);

    if (!sqlOk(rc)) {
        m_lastError = extractDiag(SQL_HANDLE_DBC, m_hDbc);
        qWarning() << "[MssqlManager] Connect failed:" << m_lastError;
        SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hDbc = nullptr;
        m_hEnv = nullptr;
        return false;
    }

    m_connected = true;

    if (!createSchema()) return false;

    qInfo() << "[MssqlManager] Connected:" << cfg.host << "/" << cfg.database;
    return true;
}

bool MssqlManager::ensureDatabase(const Config& cfg) {
    // Connect to the "master" database first to check/create our target DB
    SQLHENV hEnv = SQL_NULL_HENV;
    SQLHDBC hDbc = SQL_NULL_HDBC;

    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv)))
        return false;
    SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc))) {
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }

    SQLSetConnectAttr(hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

    // Build connection string to master database
    Config masterCfg = cfg;
    masterCfg.database = "master";
    const QString cs = buildConnectionString(masterCfg);
    std::wstring wcs = cs.toStdWString();
    SQLWCHAR outBuf[1024];
    SQLSMALLINT outLen;

    SQLRETURN rc = SQLDriverConnectW(
        hDbc, nullptr,
        reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wcs.c_str())),
        static_cast<SQLSMALLINT>(wcs.size()),
        outBuf, sizeof(outBuf) / sizeof(SQLWCHAR), &outLen,
        SQL_DRIVER_NOPROMPT);

    if (!sqlOk(rc)) {
        m_lastError = "Cannot connect to master database: ";
        // Extract diagnostic
        SQLWCHAR state[6], msg[1024];
        SQLINTEGER nativeErr;
        SQLSMALLINT msgLen;
        if (sqlOk(SQLGetDiagRecW(SQL_HANDLE_DBC, hDbc, 1, state, &nativeErr,
                                  msg, sizeof(msg) / sizeof(SQLWCHAR), &msgLen))) {
            m_lastError += QString::fromWCharArray(msg, msgLen);
        }
        qWarning() << "[MssqlManager]" << m_lastError;
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        return false;
    }

    // Check if target database exists
    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

    const QString checkSql = QString(
        "SELECT 1 FROM sys.databases WHERE name = N'%1'")
        .arg(cfg.database);
    std::wstring wCheck = checkSql.toStdWString();

    rc = SQLExecDirectW(hStmt,
                        reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wCheck.c_str())),
                        SQL_NTS);

    bool exists = false;
    if (sqlOk(rc)) {
        exists = sqlOk(SQLFetch(hStmt));
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

    if (!exists) {
        // Create the database
        SQLHSTMT hCreate = SQL_NULL_HSTMT;
        SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hCreate);

        const QString createSql = QString("CREATE DATABASE [%1]").arg(cfg.database);
        std::wstring wCreate = createSql.toStdWString();

        rc = SQLExecDirectW(hCreate,
                            reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wCreate.c_str())),
                            SQL_NTS);
        if (!sqlOk(rc)) {
            m_lastError = "CREATE DATABASE failed: ";
            SQLWCHAR state[6], msg[1024];
            SQLINTEGER nativeErr;
            SQLSMALLINT msgLen;
            if (sqlOk(SQLGetDiagRecW(SQL_HANDLE_STMT, hCreate, 1, state, &nativeErr,
                                      msg, sizeof(msg) / sizeof(SQLWCHAR), &msgLen))) {
                m_lastError += QString::fromWCharArray(msg, msgLen);
            }
            qWarning() << "[MssqlManager]" << m_lastError;
            SQLFreeHandle(SQL_HANDLE_STMT, hCreate);
            SQLDisconnect(hDbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            return false;
        }
        SQLFreeHandle(SQL_HANDLE_STMT, hCreate);
        qInfo() << "[MssqlManager] Created database:" << cfg.database;
    }

    SQLDisconnect(hDbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
    SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
    return true;
}

void MssqlManager::disconnect() {
    if (m_connected && m_hDbc) {
        SQLDisconnect(static_cast<SQLHDBC>(m_hDbc));
        qInfo() << "[MssqlManager] Disconnected.";
    }
    if (m_hDbc) {
        SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
        m_hDbc = nullptr;
    }
    if (m_hEnv) {
        SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
        m_hEnv = nullptr;
    }
    m_connected = false;
}

// ─── Diagnostics ─────────────────────────────────────────────────────────

QString MssqlManager::extractDiag(short handleType, void* handle) {
    SQLWCHAR state[6], msg[2048];
    SQLINTEGER nativeErr;
    SQLSMALLINT msgLen;
    SQLSMALLINT recNum = 1;
    QString result;

    while (sqlOk(SQLGetDiagRecW(handleType, handle, recNum++, state, &nativeErr,
                                 msg, sizeof(msg) / sizeof(SQLWCHAR), &msgLen))) {
        if (!result.isEmpty()) result += ' ';
        result += QString("[%1] %2")
                      .arg(QString::fromWCharArray(state, 5))
                      .arg(QString::fromWCharArray(msg, msgLen));
    }
    return result;
}

// ─── Execute (DDL / non-query) ───────────────────────────────────────────

bool MssqlManager::execDirect(const QString& sql) {
    if (!m_connected) { m_lastError = "Not connected"; return false; }

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt))) {
        m_lastError = "Failed to allocate statement handle";
        return false;
    }

    std::wstring wSql = sql.toStdWString();
    SQLRETURN rc = SQLExecDirectW(hStmt,
                                   reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wSql.c_str())),
                                   SQL_NTS);

    if (!sqlOk(rc)) {
        m_lastError = extractDiag(SQL_HANDLE_STMT, hStmt);
        qWarning() << "[MssqlManager] SQL error:" << m_lastError << "\n  SQL:" << sql;
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

// ─── Schema creation ─────────────────────────────────────────────────────

bool MssqlManager::createSchema() {
    // MSSQL: check if table exists via INFORMATION_SCHEMA
    auto tables = tableNames();
    if (tables.contains("media_items", Qt::CaseInsensitive))
        return true;

    // MSSQL uses NVARCHAR for Unicode, DATETIME2 for timestamps, BIT for bool,
    // IDENTITY(1,1) for auto-increment
    const char* sql = R"SQL(
        CREATE TABLE [media_items] (
            [id]               BIGINT IDENTITY(1,1) PRIMARY KEY,
            [file_path]        NVARCHAR(2048)     NOT NULL,
            [title]            NVARCHAR(512),
            [artist]           NVARCHAR(512),
            [album_artist]     NVARCHAR(512),
            [album]            NVARCHAR(512),
            [genre]            NVARCHAR(256),
            [year]             NVARCHAR(10),
            [track_number]     NVARCHAR(10),
            [composer]         NVARCHAR(512),
            [comment]          NVARCHAR(MAX),
            [isrc]             NVARCHAR(32),
            [mbid]             NVARCHAR(64),
            [label]            NVARCHAR(256),
            [language]         NVARCHAR(64),
            [bpm]              FLOAT              DEFAULT 0,
            [musical_key]      NVARCHAR(16),
            [duration_ms]      INT                DEFAULT 0,
            [bitrate]          INT                DEFAULT 0,
            [sample_rate]      INT                DEFAULT 44100,
            [channels]         INT                DEFAULT 2,
            [codec]            NVARCHAR(32),
            [file_size]        BIGINT             DEFAULT 0,
            [rating]           INT                DEFAULT 0,
            [energy]           FLOAT              DEFAULT 0,
            [weight]           FLOAT              DEFAULT 1,
            [mood]             NVARCHAR(256),
            [tags]             NVARCHAR(MAX),
            [explicit_content] BIT                DEFAULT 0,
            [date_added]       DATETIME2,
            [date_modified]    DATETIME2,
            [last_played]      DATETIME2,
            [play_count]       INT                DEFAULT 0,
            [waveform_cache]   NVARCHAR(2048),

            CONSTRAINT [UQ_media_items_file_path] UNIQUE ([file_path])
        )
    )SQL";

    return execDirect(QString::fromUtf8(sql));
}

// ─── Path check ──────────────────────────────────────────────────────────

bool MssqlManager::pathExists(const QString& path) {
    if (!m_connected) return false;

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt)))
        return false;

    // Parameterized query
    const wchar_t* sql = L"SELECT TOP 1 [id] FROM [media_items] WHERE [file_path] = ?";
    SQLRETURN rc = SQLPrepareW(hStmt, const_cast<SQLWCHAR*>(sql), SQL_NTS);
    if (!sqlOk(rc)) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    // Bind parameter
    std::wstring wPath = path.toStdWString();
    SQLLEN pathLen = static_cast<SQLLEN>(wPath.size() * sizeof(wchar_t));
    // Note: StrLen_or_Ind for WCHAR data = number of bytes, not characters
    SQLLEN strLenOrInd = SQL_NTS;

    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                     wPath.size(), 0,
                     const_cast<wchar_t*>(wPath.c_str()), 0, &strLenOrInd);

    rc = SQLExecute(hStmt);
    if (!sqlOk(rc)) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    bool found = sqlOk(SQLFetch(hStmt));
    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return found;
}

// ─── Insert ──────────────────────────────────────────────────────────────

bool MssqlManager::insertItem(MediaItem& item) {
    if (!m_connected) return false;

    // Use string interpolation with proper escaping.
    // MSSQL: N'...' for NVARCHAR literals, SCOPE_IDENTITY() for generated ID.
    auto esc = [](const QString& s) -> QString {
        QString r = s;
        r.replace('\'', "''");
        return r;
    };

    const QString sql = QString(
        "INSERT INTO [media_items] ("
        "  [file_path],[title],[artist],[album_artist],[album],[genre],[year],[track_number],"
        "  [composer],[comment],[isrc],[mbid],[label],[language],[bpm],[musical_key],[duration_ms],"
        "  [bitrate],[sample_rate],[channels],[codec],[file_size],[rating],[energy],[weight],"
        "  [mood],[tags],[explicit_content],[date_added],[date_modified]"
        ") VALUES ("
        "  N'%1',N'%2',N'%3',N'%4',N'%5',N'%6',N'%7',N'%8',"
        "  N'%9',N'%10',N'%11',N'%12',N'%13',N'%14',%15,N'%16',%17,"
        "  %18,%19,%20,N'%21',%22,%23,%24,%25,"
        "  N'%26',N'%27',%28,GETDATE(),GETDATE()"
        "); SELECT SCOPE_IDENTITY() AS id")
        .arg(esc(item.filePath), esc(item.title), esc(item.artist),
             esc(item.albumArtist), esc(item.album), esc(item.genre),
             esc(item.year), esc(item.trackNumber), esc(item.composer))
        .arg(esc(item.comment), esc(item.isrc), esc(item.mbid),
             esc(item.label), esc(item.language))
        .arg(item.bpm)
        .arg(esc(item.musicalKey))
        .arg(item.durationMs)
        .arg(item.bitrate)
        .arg(item.sampleRate)
        .arg(item.channels)
        .arg(esc(item.codec))
        .arg(item.fileSize)
        .arg(item.rating)
        .arg(item.energy)
        .arg(item.weight)
        .arg(esc(item.mood), esc(item.tags))
        .arg(item.explicit_ ? 1 : 0);

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt))) {
        m_lastError = "Failed to allocate statement handle";
        return false;
    }

    std::wstring wSql = sql.toStdWString();
    SQLRETURN rc = SQLExecDirectW(hStmt,
                                   reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wSql.c_str())),
                                   SQL_NTS);

    if (!sqlOk(rc)) {
        m_lastError = extractDiag(SQL_HANDLE_STMT, hStmt);
        qWarning() << "[MssqlManager] INSERT failed:" << m_lastError;
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    // The INSERT produces no result set, but SCOPE_IDENTITY() does.
    // Move to the next result set (SCOPE_IDENTITY SELECT)
    rc = SQLMoreResults(hStmt);
    if (sqlOk(rc)) {
        if (sqlOk(SQLFetch(hStmt))) {
            item.id = getInt64Col(hStmt, 1);
            if (item.id == 0) {
                // Fallback: try reading as string
                QString idStr = getStringCol(hStmt, 1);
                item.id = idStr.toLongLong();
            }
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return true;
}

// ─── Update ──────────────────────────────────────────────────────────────

bool MssqlManager::updateItem(const MediaItem& item) {
    if (!m_connected || item.id <= 0) return false;

    auto esc = [](const QString& s) -> QString {
        QString r = s;
        r.replace('\'', "''");
        return r;
    };

    const QString sql = QString(
        "UPDATE [media_items] SET "
        "[title]=N'%1',[artist]=N'%2',[album]=N'%3',[genre]=N'%4',"
        "[bpm]=%5,[rating]=%6,[energy]=%7,[mbid]=N'%8',"
        "[date_modified]=GETDATE() WHERE [id]=%9")
        .arg(esc(item.title), esc(item.artist), esc(item.album), esc(item.genre))
        .arg(item.bpm)
        .arg(item.rating)
        .arg(item.energy)
        .arg(esc(item.mbid))
        .arg(item.id);

    return execDirect(sql);
}

// ─── Delete ──────────────────────────────────────────────────────────────

bool MssqlManager::deleteItem(qint64 id) {
    if (!m_connected) return false;
    return execDirect(QString("DELETE FROM [media_items] WHERE [id]=%1").arg(id));
}

// ─── Load all ────────────────────────────────────────────────────────────

QList<MediaItem> MssqlManager::loadAll() {
    QList<MediaItem> items;
    if (!m_connected) return items;

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt)))
        return items;

    const wchar_t* sql =
        L"SELECT [id],[file_path],[title],[artist],[album_artist],[album],[genre],[year],"
        L"[track_number],[composer],[comment],[isrc],[mbid],[label],[language],[bpm],[musical_key],"
        L"[duration_ms],[bitrate],[sample_rate],[channels],[codec],[file_size],[rating],[energy],"
        L"[weight],[mood],[tags],[explicit_content],[date_added],[date_modified],[last_played],"
        L"[play_count] FROM [media_items] ORDER BY [artist],[album],[track_number]";

    SQLRETURN rc = SQLExecDirectW(hStmt, const_cast<SQLWCHAR*>(sql), SQL_NTS);
    if (!sqlOk(rc)) {
        m_lastError = extractDiag(SQL_HANDLE_STMT, hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return items;
    }

    while (sqlOk(SQLFetch(hStmt))) {
        MediaItem item;
        SQLUSMALLINT c = 1;
        item.id           = getInt64Col(hStmt, c++);
        item.filePath     = getStringCol(hStmt, c++);
        item.title        = getStringCol(hStmt, c++);
        item.artist       = getStringCol(hStmt, c++);
        item.albumArtist  = getStringCol(hStmt, c++);
        item.album        = getStringCol(hStmt, c++);
        item.genre        = getStringCol(hStmt, c++);
        item.year         = getStringCol(hStmt, c++);
        item.trackNumber  = getStringCol(hStmt, c++);
        item.composer     = getStringCol(hStmt, c++);
        item.comment      = getStringCol(hStmt, c++);
        item.isrc         = getStringCol(hStmt, c++);
        item.mbid         = getStringCol(hStmt, c++);
        item.label        = getStringCol(hStmt, c++);
        item.language     = getStringCol(hStmt, c++);
        item.bpm          = getDoubleCol(hStmt, c++);
        item.musicalKey   = getStringCol(hStmt, c++);
        item.durationMs   = static_cast<int>(getInt64Col(hStmt, c++));
        item.bitrate      = static_cast<int>(getInt64Col(hStmt, c++));
        item.sampleRate   = static_cast<int>(getInt64Col(hStmt, c++));
        item.channels     = static_cast<int>(getInt64Col(hStmt, c++));
        item.codec        = getStringCol(hStmt, c++);
        item.fileSize     = getInt64Col(hStmt, c++);
        item.rating       = static_cast<int>(getInt64Col(hStmt, c++));
        item.energy       = getDoubleCol(hStmt, c++);
        item.weight       = getDoubleCol(hStmt, c++);
        item.mood         = getStringCol(hStmt, c++);
        item.tags         = getStringCol(hStmt, c++);
        item.explicit_    = (getInt64Col(hStmt, c++) != 0);
        item.dateAdded    = QDateTime::fromString(getTimestampCol(hStmt, c++), Qt::ISODate);
        item.dateModified = QDateTime::fromString(getTimestampCol(hStmt, c++), Qt::ISODate);
        item.lastPlayed   = QDateTime::fromString(getTimestampCol(hStmt, c++), Qt::ISODate);
        item.playCount    = static_cast<int>(getInt64Col(hStmt, c++));
        items.append(item);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return items;
}

// ─── Raw query execution ─────────────────────────────────────────────────

QList<QVariantList> MssqlManager::executeQuery(const QString& sql) {
    QList<QVariantList> results;
    if (!m_connected) { m_lastError = "Not connected"; return results; }

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt)))
        return results;

    std::wstring wSql = sql.toStdWString();
    SQLRETURN rc = SQLExecDirectW(hStmt,
                                   reinterpret_cast<SQLWCHAR*>(const_cast<wchar_t*>(wSql.c_str())),
                                   SQL_NTS);

    if (!sqlOk(rc)) {
        m_lastError = extractDiag(SQL_HANDLE_STMT, hStmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return results;
    }

    // Get column count
    SQLSMALLINT nCols = 0;
    SQLNumResultCols(hStmt, &nCols);

    if (nCols == 0) {
        // Non-query (DDL/DML) — no results
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return results;
    }

    // Fetch rows
    while (sqlOk(SQLFetch(hStmt))) {
        QVariantList row;
        row.reserve(nCols);
        for (SQLUSMALLINT c = 1; c <= static_cast<SQLUSMALLINT>(nCols); ++c) {
            SQLWCHAR buf[4096];
            SQLLEN indicator = 0;
            rc = SQLGetData(hStmt, c, SQL_C_WCHAR, buf, sizeof(buf), &indicator);
            if (sqlOk(rc) && indicator != SQL_NULL_DATA)
                row.append(QString::fromWCharArray(buf));
            else
                row.append(QVariant());
        }
        results.append(row);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return results;
}

// ─── Table names ─────────────────────────────────────────────────────────

QStringList MssqlManager::tableNames() {
    QStringList names;
    if (!m_connected) return names;

    SQLHSTMT hStmt = SQL_NULL_HSTMT;
    if (!sqlOk(SQLAllocHandle(SQL_HANDLE_STMT, m_hDbc, &hStmt)))
        return names;

    const wchar_t* sql =
        L"SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
        L"WHERE TABLE_TYPE = 'BASE TABLE' ORDER BY TABLE_NAME";

    SQLRETURN rc = SQLExecDirectW(hStmt, const_cast<SQLWCHAR*>(sql), SQL_NTS);
    if (!sqlOk(rc)) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return names;
    }

    while (sqlOk(SQLFetch(hStmt))) {
        QString name = getStringCol(hStmt, 1).trimmed();
        if (!name.isEmpty())
            names.append(name);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return names;
}

} // namespace M1

#else
// ═════════════════════════════════════════════════════════════════════════════
//  Stub implementation — compiled on non-Windows platforms (no ODBC SDK)
// ═════════════════════════════════════════════════════════════════════════════

namespace M1 {

MssqlManager::MssqlManager() = default;
MssqlManager::~MssqlManager() = default;

bool MssqlManager::connect(const Config&) {
    m_lastError = "SQL Server driver requires Windows (ODBC).";
    qWarning() << "[MssqlManager]" << m_lastError;
    return false;
}

void    MssqlManager::disconnect()                  {}
bool    MssqlManager::createSchema()                { return false; }
bool    MssqlManager::insertItem(MediaItem&)        { return false; }
bool    MssqlManager::updateItem(const MediaItem&)  { return false; }
bool    MssqlManager::deleteItem(qint64)            { return false; }
QList<MediaItem> MssqlManager::loadAll()            { return {}; }
bool    MssqlManager::pathExists(const QString&)    { return false; }
QList<QVariantList> MssqlManager::executeQuery(const QString&) { return {}; }
QStringList MssqlManager::tableNames()              { return {}; }
bool    MssqlManager::execDirect(const QString&)    { return false; }
QString MssqlManager::extractDiag(short, void*)     { return {}; }
QString MssqlManager::detectDriver() const          { return {}; }
QString MssqlManager::buildConnectionString(const Config&) const { return {}; }
bool    MssqlManager::ensureDatabase(const Config&) { return false; }

} // namespace M1

#endif // _WIN32
