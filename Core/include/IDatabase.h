#pragma once
#include "MediaItem.h"
#include <QString>
#include <QList>

namespace M1 {

/// IDatabase — abstract interface for the media library database backend.
///
/// Two implementations:
///   - SqliteManager  — embedded SQLite3, zero-config, default for local use
///   - DatabaseManager — MySQL/MariaDB, for enterprise / multi-user deployments
///
/// All methods are safe to call even when isConnected() returns false
/// (they return false/empty and set lastError()).
class IDatabase {
public:
    virtual ~IDatabase() = default;

    virtual bool    isConnected() const = 0;
    virtual void    disconnect()        = 0;
    virtual bool    createSchema()      = 0;

    virtual bool    insertItem(MediaItem& item)        = 0;  ///< Sets item.id on success
    virtual bool    updateItem(const MediaItem& item)   = 0;
    virtual bool    deleteItem(qint64 id)               = 0;
    virtual QList<MediaItem> loadAll()                  = 0;
    virtual bool    pathExists(const QString& path)     = 0;

    virtual QString lastError() const = 0;
    virtual QString backendName() const = 0;  ///< "SQLite" or "MySQL"
};

} // namespace M1
