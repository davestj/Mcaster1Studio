#pragma once
#include "IDatabase.h"

struct sqlite3;  // forward declare — keeps sqlite3.h out of header

namespace M1 {

/// SqliteManager — embedded SQLite3 backend for the media library.
///
/// Zero-config: creates the database file automatically on first connect.
/// Default location: QStandardPaths::AppDataLocation / "mcaster1studio.db"
class SqliteManager : public IDatabase {
public:
    SqliteManager();
    ~SqliteManager() override;

    /// Open (or create) the database at the given file path.
    /// Pass an empty string to use the default location.
    bool    connect(const QString& dbPath = {});

    // IDatabase interface
    bool    isConnected() const override { return m_db != nullptr; }
    void    disconnect()        override;
    bool    createSchema()      override;

    bool    insertItem(MediaItem& item)        override;
    bool    updateItem(const MediaItem& item)   override;
    bool    deleteItem(qint64 id)               override;
    QList<MediaItem> loadAll()                  override;
    bool    pathExists(const QString& path)     override;

    QString lastError()    const override { return m_lastError; }
    QString backendName()  const override { return QStringLiteral("SQLite"); }
    QString databasePath() const { return m_dbPath; }

private:
    sqlite3* m_db = nullptr;
    QString  m_dbPath;
    QString  m_lastError;

    bool    execSql(const char* sql);
};

} // namespace M1
