#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QUuid>

namespace M1 {

// ─── Database server connection entry ────────────────────────────────────────
struct DbServerEntry {
    QString id;            ///< Unique UUID (generated on creation)
    QString displayName;   ///< "Local SQLite", "Production MySQL", etc.

    enum class Backend { SQLite, MySQL, PostgreSQL, Firebird, MSSQL };
    Backend backend = Backend::SQLite;

    // SQLite-specific
    QString sqlitePath;    ///< Empty = AppData default location

    // Firebird-specific
    QString firebirdPath;  ///< Database file path for embedded Firebird

    // Network connection settings (MySQL / PostgreSQL / Firebird / MSSQL)
    QString host     = "127.0.0.1";
    int     port     = 3306;          ///< MySQL 3306, PG 5432, FB 3050, MSSQL 1433
    QString username = "root";
    QString password;

    bool isSQLite()     const { return backend == Backend::SQLite;     }
    bool isMySQL()      const { return backend == Backend::MySQL;      }
    bool isPostgreSQL() const { return backend == Backend::PostgreSQL; }
    bool isFirebird()   const { return backend == Backend::Firebird;   }
    bool isMSSQL()      const { return backend == Backend::MSSQL;      }
    bool isNetworked()  const { return !isSQLite(); }

    /// Human-readable backend name for display.
    QString backendDisplayName() const {
        switch (backend) {
        case Backend::SQLite:     return "SQLite";
        case Backend::MySQL:      return "MySQL";
        case Backend::PostgreSQL: return "PostgreSQL";
        case Backend::Firebird:   return "Firebird";
        case Backend::MSSQL:      return "SQL Server";
        }
        return "Unknown";
    }

    /// Short string key used in QSettings persistence.
    QString backendKey() const {
        switch (backend) {
        case Backend::SQLite:     return "sqlite";
        case Backend::MySQL:      return "mysql";
        case Backend::PostgreSQL: return "postgresql";
        case Backend::Firebird:   return "firebird";
        case Backend::MSSQL:      return "mssql";
        }
        return "sqlite";
    }

    /// Parse a backend key string back to enum.
    static Backend backendFromKey(const QString& key) {
        if (key == "mysql")      return Backend::MySQL;
        if (key == "postgresql") return Backend::PostgreSQL;
        if (key == "firebird")   return Backend::Firebird;
        if (key == "mssql")      return Backend::MSSQL;
        return Backend::SQLite;
    }

    /// Default port for the given backend.
    static int defaultPort(Backend b) {
        switch (b) {
        case Backend::MySQL:      return 3306;
        case Backend::PostgreSQL: return 5432;
        case Backend::Firebird:   return 3050;
        case Backend::MSSQL:      return 1433;
        default:                  return 0;
        }
    }

    /// Number of defined backends (for UI iteration).
    static constexpr int backendCount() { return 5; }

    /// All backend enum values (for UI iteration).
    static QList<Backend> allBackends() {
        return { Backend::SQLite, Backend::MySQL, Backend::PostgreSQL,
                 Backend::Firebird, Backend::MSSQL };
    }

    static QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
};

// ─── Database server registry (singleton) ────────────────────────────────────
/// Manages all configured database servers. Persisted in QSettings under "dbservers/".
/// Emits signals when servers or surface assignments change so live modules can react.
class DbServerRegistry : public QObject {
    Q_OBJECT
public:
    static DbServerRegistry& instance();

    QList<DbServerEntry>     servers() const { return m_servers; }
    const DbServerEntry*     findById(const QString& id) const;
    const DbServerEntry*     findByName(const QString& name) const;
    const DbServerEntry*     defaultServer() const;

    int  count() const { return m_servers.size(); }

    void addServer(const DbServerEntry& entry);
    void updateServer(const DbServerEntry& entry);
    void removeServer(const QString& id);
    void setDefaultServerId(const QString& id);
    QString defaultServerId() const { return m_defaultServerId; }

    void loadFromSettings();
    void saveToSettings() const;

    /// Notify that a surface's DB assignment changed (server combo or schema name).
    /// Called by PreferencesDialog when the user changes surface DB assignments.
    void notifySurfaceAssignmentChanged(const QString& surfaceName);

    /// Test a connection. Returns empty string on success, error message on failure.
    static QString testConnection(const DbServerEntry& entry);

signals:
    /// Emitted when a server entry is added, updated, or removed.
    void serverListChanged();

    /// Emitted when a specific server entry is modified.
    void serverChanged(const QString& serverId);

    /// Emitted when a surface's DB assignment changes (server or schema).
    /// @param surfaceName  The display name of the surface whose assignment changed.
    void surfaceAssignmentChanged(const QString& surfaceName);

private:
    DbServerRegistry();
    void migrateFromLegacy();  ///< Upgrade old database/* keys to dbservers/ format

    QList<DbServerEntry> m_servers;
    QString              m_defaultServerId;
};

} // namespace M1
