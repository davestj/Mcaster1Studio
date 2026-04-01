#pragma once
#include "IDatabase.h"
#include "DbServerEntry.h"
#include "SqlDialect.h"
#include <functional>
#include <QMap>

namespace M1 {

/// DatabaseFactory — central factory for creating IDatabase connections
/// from a DbServerEntry configuration.
///
/// Uses a registration pattern: each driver module calls registerDriver()
/// at startup. The factory dispatches create() to the registered creator.
/// This avoids pulling sqlite3/mysql/libpq dependencies into Core.
///
/// Usage:
///   // At startup (e.g., in MediaLibraryModule or main):
///   DatabaseFactory::registerDriver(DbServerEntry::Backend::SQLite, sqliteCreator);
///   DatabaseFactory::registerDriver(DbServerEntry::Backend::MySQL,  mysqlCreator);
///
///   // When creating a connection:
///   auto* db = DatabaseFactory::create(serverEntry, "my_database");
///   if (db && db->isConnected()) { ... }
///
/// The caller owns the returned IDatabase*.
class DatabaseFactory {
public:
    /// Driver creator function type.
    /// @param entry    The server configuration
    /// @param dbName   The database/schema name (for networked backends)
    /// @param dbPath   Override file path (for SQLite/Firebird embedded)
    /// @return Connected IDatabase*, or nullptr on failure. Caller owns.
    using DriverCreator = std::function<IDatabase*(const DbServerEntry& entry,
                                                   const QString& dbName,
                                                   const QString& dbPath)>;

    /// Register a driver creator for a backend type.
    /// Replaces any previously registered creator for the same backend.
    static void registerDriver(DbServerEntry::Backend backend, DriverCreator creator);

    /// Check if a driver is registered for the given backend.
    static bool isDriverRegistered(DbServerEntry::Backend backend);

    /// Create and connect a database from a server entry.
    /// @param entry    The server configuration
    /// @param dbName   The database/schema name to use (for networked backends)
    /// @param dbPath   Override SQLite file path (empty = use entry.sqlitePath or default)
    /// @return Connected IDatabase*, or nullptr on failure. Caller owns.
    static IDatabase* create(const DbServerEntry& entry,
                             const QString& dbName = {},
                             const QString& dbPath = {});

    /// Get the SQL dialect for a backend type.
    /// Returns a static singleton — do NOT delete.
    static SqlDialect* dialect(DbServerEntry::Backend backend) {
        return dialectFor(static_cast<int>(backend));
    }

    /// Test if the required driver/library is available for a backend.
    /// Returns empty string if driver is registered, error message if not.
    static QString checkDriverAvailable(DbServerEntry::Backend backend);

private:
    static QMap<int, DriverCreator>& drivers();
};

} // namespace M1
