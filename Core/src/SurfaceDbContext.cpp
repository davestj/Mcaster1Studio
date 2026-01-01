#include "SurfaceDbContext.h"
#include "DbServerEntry.h"
#include "IDatabase.h"
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDebug>

// Forward declarations for concrete DB implementations
// (headers live in MediaLibraryModule, but we link against them)
class SqliteManager;
class DatabaseManager;

namespace M1 {

// ─── Create a database connection for this surface context ───────────────────
IDatabase* SurfaceDbContext::createConnection() const {
    if (!isValid()) {
        qWarning() << "[SurfaceDbContext] Invalid context — no serverId or schemaName.";
        return nullptr;
    }

    const auto* server = DbServerRegistry::instance().findById(serverId);
    if (!server) {
        qWarning() << "[SurfaceDbContext] Server not found:" << serverId;
        return nullptr;
    }

    // The actual connection creation is deferred to the module layer
    // since SqliteManager and DatabaseManager live in MediaLibraryModule.
    // This method returns nullptr; modules use the context to create their own
    // connections via their local factory.
    //
    // Usage pattern in modules:
    //   if (ctx.server->isSQLite()) {
    //       auto* db = new SqliteManager;
    //       db->connect(ctx.sqlitePath());
    //   } else {
    //       auto* db = new DatabaseManager;
    //       DatabaseManager::Config cfg { ... };
    //       cfg.database = ctx.schemaName;
    //       db->connect(cfg);
    //   }

    qInfo() << "[SurfaceDbContext] Context ready for schema:" << schemaName
            << "on server:" << server->displayName;
    return nullptr;  // Modules create their own connection
}

// ─── Generate schema name from surface display name ──────────────────────────
QString SurfaceDbContext::nameToSchema(const QString& surfaceName) {
    // "GRR FM Broadcast" → "mcaster1_grr_fm_broadcast"
    QString schema = surfaceName.toLower().trimmed();
    // Replace non-alphanumeric with underscores
    schema.replace(QRegularExpression("[^a-z0-9]+"), "_");
    // Remove leading/trailing underscores
    schema.replace(QRegularExpression("^_+|_+$"), "");
    // Prepend namespace
    schema = "mcaster1_" + schema;
    // Limit to 64 chars (MySQL identifier limit)
    if (schema.size() > 64)
        schema = schema.left(64);
    return schema;
}

} // namespace M1
