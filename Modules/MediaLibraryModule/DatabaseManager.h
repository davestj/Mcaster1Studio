#pragma once
#include "IDatabase.h"

namespace M1 {

/// DatabaseManager — MySQL/MariaDB backend for the media library.
///
/// If the connection fails the library operates in memory-only mode.
/// All methods are safe to call even when isConnected() returns false
/// (they just return false/empty and set lastError()).
///
/// mysql.h is kept out of this header (opaque void* for MYSQL*).
class DatabaseManager : public IDatabase {
public:
    struct Config {
        QString host     = "127.0.0.1";
        int     port     = 3306;
        QString user     = "root";
        QString password;
        QString database = "mcaster1studio";
    };

    DatabaseManager();
    ~DatabaseManager() override;

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
    QString backendName()  const override { return QStringLiteral("MySQL"); }

private:
    void*   m_conn = nullptr;   ///< MYSQL* — opaque to keep mysql.h out of header
    QString m_lastError;

    bool    execSql(const QString& sql);
    QString escapeString(const QString& s);
};

} // namespace M1
