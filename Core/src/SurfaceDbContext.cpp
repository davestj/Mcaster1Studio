#include "SurfaceDbContext.h"
#include "DbServerEntry.h"
#include "DatabaseFactory.h"
#include "IDatabase.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include <QDir>
#include <QDebug>

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

    if (!DatabaseFactory::isDriverRegistered(server->backend)) {
        qWarning() << "[SurfaceDbContext] No driver registered for"
                    << server->backendDisplayName();
        return nullptr;
    }

    // Build the DB file path for SQLite
    QString dbPath;
    if (server->isSQLite()) {
        QString baseDir = server->sqlitePath;
        if (baseDir.isEmpty())
            baseDir = QCoreApplication::applicationDirPath() + "/data";
        QDir dir(baseDir);
        if (!dir.exists()) dir.mkpath(".");
        dbPath = dir.absoluteFilePath(schemaName + ".db");
    }

    IDatabase* db = DatabaseFactory::create(*server, schemaName, dbPath);
    if (!db) {
        qWarning() << "[SurfaceDbContext] DatabaseFactory::create() returned null for"
                    << server->backendDisplayName() << "schema:" << schemaName;
        return nullptr;
    }

    if (!db->isConnected()) {
        qWarning() << "[SurfaceDbContext] Connection failed:" << db->lastError();
        delete db;
        return nullptr;
    }

    qInfo() << "[SurfaceDbContext] Connected to" << server->backendDisplayName()
            << "schema:" << schemaName;
    return db;
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
