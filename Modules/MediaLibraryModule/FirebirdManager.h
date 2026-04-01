#pragma once
#include "IDatabase.h"

namespace M1 {

/// FirebirdManager — Firebird SQL database backend for the media library.
///
/// Uses the Firebird C client library (fbclient / ibase.h).
/// Connection and transaction handles are kept as opaque types to avoid
/// leaking <ibase.h> into downstream headers.
///
/// Supports both embedded (local .fdb file) and client/server modes.
/// Requires Firebird 3.0+ for IDENTITY column support (SQL dialect 3).
///
/// When compiled without M1_HAS_FIREBIRD, all methods return failure stubs.
class FirebirdManager : public IDatabase {
public:
    struct Config {
        QString host;                              ///< Empty = embedded mode
        int     port     = 3050;
        QString user     = "SYSDBA";
        QString password = "masterkey";
        QString database;                          ///< File path (embedded) or server-side path/alias
    };

    FirebirdManager();
    ~FirebirdManager() override;

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
    QString backendName()  const override { return QStringLiteral("Firebird"); }

private:
    // Opaque handles — actual types are isc_db_handle (unsigned int on x64)
    // and isc_tr_handle, but we hide ibase.h from the header.
    unsigned int m_dbHandle = 0;
    bool         m_connected = false;
    QString      m_lastError;

    bool    execImmediate(const QString& sql);
    bool    beginTransaction(unsigned int& trHandle);
    bool    commitTransaction(unsigned int& trHandle);
    void    rollbackTransaction(unsigned int& trHandle);
    QString buildConnectionString(const Config& cfg) const;
    QString extractError();
    bool    ensureDatabase(const Config& cfg);
};

} // namespace M1
