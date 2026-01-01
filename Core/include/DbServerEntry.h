#pragma once
#include <QString>
#include <QList>
#include <QUuid>

namespace M1 {

// ─── Database server connection entry ────────────────────────────────────────
struct DbServerEntry {
    QString id;            ///< Unique UUID (generated on creation)
    QString displayName;   ///< "Local SQLite", "Production MySQL", etc.

    enum class Backend { SQLite, MySQL };
    Backend backend = Backend::SQLite;

    // SQLite-specific
    QString sqlitePath;    ///< Empty = AppData default location

    // MySQL-specific
    QString host     = "127.0.0.1";
    int     port     = 3306;
    QString username = "root";
    QString password;

    bool isSQLite() const { return backend == Backend::SQLite; }
    bool isMySQL()  const { return backend == Backend::MySQL;  }

    static QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
};

// ─── Database server registry (singleton) ────────────────────────────────────
/// Manages all configured database servers. Persisted in QSettings under "dbservers/".
class DbServerRegistry {
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

    /// Test a connection. Returns empty string on success, error message on failure.
    static QString testConnection(const DbServerEntry& entry);

private:
    DbServerRegistry() = default;
    void migrateFromLegacy();  ///< Upgrade old database/* keys to dbservers/ format

    QList<DbServerEntry> m_servers;
    QString              m_defaultServerId;
};

} // namespace M1
