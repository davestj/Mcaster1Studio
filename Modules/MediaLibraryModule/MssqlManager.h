#pragma once
#include "IDatabase.h"

namespace M1 {

/// MssqlManager — Microsoft SQL Server backend for the media library.
///
/// Uses ODBC (sql.h / sqlext.h) — part of the Windows SDK, always available.
/// Works with all SQL Server editions: Express, Developer, Standard, Enterprise,
/// Azure SQL Database, and LocalDB.
///
/// Auto-detects the best installed ODBC driver:
///   1. "ODBC Driver 18 for SQL Server"
///   2. "ODBC Driver 17 for SQL Server"
///   3. "SQL Server" (legacy, always installed on Windows)
///
/// Connection and statement handles are kept as opaque void* to avoid
/// leaking <sql.h> into downstream headers.
class MssqlManager : public IDatabase {
public:
    struct Config {
        QString host     = "127.0.0.1";
        int     port     = 1433;
        QString user     = "sa";
        QString password;
        QString database = "mcaster1studio";
        bool    trustedConnection = false;  ///< Use Windows Authentication
    };

    MssqlManager();
    ~MssqlManager() override;

    bool    connect(const Config& cfg);

    // IDatabase interface
    bool    isConnected() const override { return m_connected; }
    void    disconnect()        override;
    bool    createSchema()      override;

    bool    insertItem(MediaItem& item)        override;
    bool    updateItem(const MediaItem& item)   override;
    bool    deleteItem(qint64 id)               override;
    QList<MediaItem> loadAll()                  override;
    bool    pathExists(const QString& path)     override;

    QList<QVariantList> executeQuery(const QString& sql) override;
    QStringList         tableNames() override;

    QString lastError()    const override { return m_lastError; }
    QString backendName()  const override { return QStringLiteral("SQL Server"); }

private:
    void*   m_hEnv  = nullptr;   ///< SQLHENV  — ODBC environment handle
    void*   m_hDbc  = nullptr;   ///< SQLHDBC  — ODBC connection handle
    bool    m_connected = false;
    QString m_lastError;

    bool    execDirect(const QString& sql);
    QString extractDiag(short handleType, void* handle);
    QString detectDriver() const;
    QString buildConnectionString(const Config& cfg) const;
    bool    ensureDatabase(const Config& cfg);
};

} // namespace M1
