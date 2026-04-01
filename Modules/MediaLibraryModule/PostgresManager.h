#pragma once
#include "IDatabase.h"

namespace M1 {

/// PostgresManager — PostgreSQL backend for the media library.
///
/// Uses libpq (the official PostgreSQL C client library).
/// Connection handle is kept as opaque void* to avoid leaking <libpq-fe.h>
/// into downstream headers.
///
/// Supports CREATE DATABASE, schema creation, CRUD operations,
/// parameterized queries, and the full IDatabase interface.
class PostgresManager : public IDatabase {
public:
    struct Config {
        QString host     = "127.0.0.1";
        int     port     = 5432;
        QString user     = "postgres";
        QString password;
        QString database = "mcaster1studio";
    };

    PostgresManager();
    ~PostgresManager() override;

    bool    connect(const Config& cfg);

    // IDatabase interface
    bool    isConnected() const override { return m_conn != nullptr; }
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
    QString backendName()  const override { return QStringLiteral("PostgreSQL"); }

private:
    void*   m_conn = nullptr;   ///< PGconn* — opaque to keep libpq-fe.h out of header
    QString m_lastError;

    bool    execSql(const QString& sql);
    QString escapeString(const QString& s);
    bool    ensureDatabase(const Config& cfg);
};

} // namespace M1
