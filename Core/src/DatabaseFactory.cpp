#include "DatabaseFactory.h"
#include <QDebug>

namespace M1 {

// ─── Static driver registry ─────────────────────────────────────────────────

QMap<int, DatabaseFactory::DriverCreator>& DatabaseFactory::drivers() {
    static QMap<int, DriverCreator> s_drivers;
    return s_drivers;
}

void DatabaseFactory::registerDriver(DbServerEntry::Backend backend, DriverCreator creator) {
    drivers()[static_cast<int>(backend)] = std::move(creator);
    qInfo() << "[DatabaseFactory] Registered driver for"
            << DbServerEntry({{}, {}, backend}).backendDisplayName();
}

bool DatabaseFactory::isDriverRegistered(DbServerEntry::Backend backend) {
    return drivers().contains(static_cast<int>(backend));
}

// ─── Driver availability check ──────────────────────────────────────────────

QString DatabaseFactory::checkDriverAvailable(DbServerEntry::Backend backend) {
    if (isDriverRegistered(backend))
        return {};  // Driver is registered and available

    // Not registered — return a helpful message
    switch (backend) {
    case DbServerEntry::Backend::SQLite:
        return "SQLite driver not registered. Ensure MediaLibraryModule is loaded.";
    case DbServerEntry::Backend::MySQL:
        return "MySQL driver not registered. Ensure MediaLibraryModule is loaded.";
    case DbServerEntry::Backend::PostgreSQL:
        return "PostgreSQL driver not yet available. Add libpq to vcpkg.json to enable.";
    case DbServerEntry::Backend::Firebird:
        return "Firebird driver not yet available. Coming in a future release.";
    case DbServerEntry::Backend::MSSQL:
        return "SQL Server driver not yet available. Coming in a future release.";
    }
    return "Unknown backend";
}

// ─── Factory ────────────────────────────────────────────────────────────────

IDatabase* DatabaseFactory::create(const DbServerEntry& entry,
                                   const QString& dbName,
                                   const QString& dbPath) {
    const int key = static_cast<int>(entry.backend);
    auto it = drivers().find(key);

    if (it == drivers().end()) {
        qWarning() << "[DatabaseFactory] No driver registered for"
                   << entry.backendDisplayName();
        return nullptr;
    }

    qInfo() << "[DatabaseFactory] Creating" << entry.backendDisplayName()
            << "connection for database:" << (dbName.isEmpty() ? "(default)" : dbName);

    return it.value()(entry, dbName, dbPath);
}

} // namespace M1
