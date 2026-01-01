#pragma once
#include <QString>

namespace M1 {

class IDatabase;

/// Encapsulates the database identity for a single surface.
/// Passed to DB-aware modules at creation time so they connect to the right DB/schema.
struct SurfaceDbContext {
    QString serverId;      ///< DbServerEntry UUID from registry
    QString schemaName;    ///< Database name (MySQL) or file stem (SQLite)
    QString tablePrefix;   ///< Optional prefix for shared-server table isolation

    /// Create an IDatabase* connected to this surface's database.
    /// Caller owns the returned pointer. Returns nullptr on failure.
    IDatabase* createConnection() const;

    /// Generate a schema name from a surface display name.
    /// "GRR FM Broadcast" → "mcaster1_grr_fm_broadcast"
    static QString nameToSchema(const QString& surfaceName);

    bool isValid() const { return !serverId.isEmpty() && !schemaName.isEmpty(); }
};

} // namespace M1
