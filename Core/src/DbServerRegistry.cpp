#include "DbServerEntry.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

namespace M1 {

// ─── Singleton ───────────────────────────────────────────────────────────────
DbServerRegistry& DbServerRegistry::instance() {
    static DbServerRegistry s;
    return s;
}

// ─── Lookup ──────────────────────────────────────────────────────────────────
const DbServerEntry* DbServerRegistry::findById(const QString& id) const {
    for (const auto& s : m_servers)
        if (s.id == id) return &s;
    return nullptr;
}

const DbServerEntry* DbServerRegistry::findByName(const QString& name) const {
    for (const auto& s : m_servers)
        if (s.displayName == name) return &s;
    return nullptr;
}

const DbServerEntry* DbServerRegistry::defaultServer() const {
    if (auto* s = findById(m_defaultServerId)) return s;
    return m_servers.isEmpty() ? nullptr : &m_servers.first();
}

// ─── CRUD ────────────────────────────────────────────────────────────────────
void DbServerRegistry::addServer(const DbServerEntry& entry) {
    m_servers.append(entry);
    if (m_servers.size() == 1)
        m_defaultServerId = entry.id;
    saveToSettings();
}

void DbServerRegistry::updateServer(const DbServerEntry& entry) {
    for (auto& s : m_servers) {
        if (s.id == entry.id) {
            s = entry;
            saveToSettings();
            return;
        }
    }
}

void DbServerRegistry::removeServer(const QString& id) {
    m_servers.removeIf([&](const DbServerEntry& e) { return e.id == id; });
    if (m_defaultServerId == id)
        m_defaultServerId = m_servers.isEmpty() ? QString() : m_servers.first().id;
    saveToSettings();
}

void DbServerRegistry::setDefaultServerId(const QString& id) {
    m_defaultServerId = id;
    saveToSettings();
}

// ─── QSettings persistence ──────────────────────────────────────────────────
void DbServerRegistry::loadFromSettings() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    const int count = s.value("dbservers/count", 0).toInt();

    if (count == 0) {
        // Check for legacy single-DB settings and migrate
        if (s.contains("database/backend")) {
            migrateFromLegacy();
            return;
        }
        // First run: create default SQLite entry
        DbServerEntry def;
        def.id          = DbServerEntry::newId();
        def.displayName = "Local SQLite (Default)";
        def.backend     = DbServerEntry::Backend::SQLite;
        m_servers.append(def);
        m_defaultServerId = def.id;
        saveToSettings();
        qInfo() << "[DbServerRegistry] First run — created default SQLite server.";
        return;
    }

    m_servers.clear();
    m_defaultServerId = s.value("dbservers/default").toString();

    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("dbservers/%1").arg(i);
        DbServerEntry e;
        e.id          = s.value(prefix + "/id").toString();
        e.displayName = s.value(prefix + "/name").toString();
        const QString backend = s.value(prefix + "/backend", "sqlite").toString();
        e.backend     = (backend == "mysql") ? DbServerEntry::Backend::MySQL
                                             : DbServerEntry::Backend::SQLite;
        e.sqlitePath  = s.value(prefix + "/sqlite/path").toString();
        e.host        = s.value(prefix + "/mysql/host", "127.0.0.1").toString();
        e.port        = s.value(prefix + "/mysql/port", 3306).toInt();
        e.username    = s.value(prefix + "/mysql/user", "root").toString();
        e.password    = s.value(prefix + "/mysql/pass").toString();
        m_servers.append(e);
    }

    if (m_defaultServerId.isEmpty() && !m_servers.isEmpty())
        m_defaultServerId = m_servers.first().id;

    qInfo() << "[DbServerRegistry] Loaded" << m_servers.size() << "server(s).";
}

void DbServerRegistry::saveToSettings() const {
    QSettings s("Mcaster1", "Mcaster1Studio");
    s.setValue("dbservers/count",   m_servers.size());
    s.setValue("dbservers/default", m_defaultServerId);

    for (int i = 0; i < m_servers.size(); ++i) {
        const QString prefix = QString("dbservers/%1").arg(i);
        const auto& e = m_servers[i];
        s.setValue(prefix + "/id",          e.id);
        s.setValue(prefix + "/name",        e.displayName);
        s.setValue(prefix + "/backend",     e.isSQLite() ? "sqlite" : "mysql");
        s.setValue(prefix + "/sqlite/path", e.sqlitePath);
        s.setValue(prefix + "/mysql/host",  e.host);
        s.setValue(prefix + "/mysql/port",  e.port);
        s.setValue(prefix + "/mysql/user",  e.username);
        s.setValue(prefix + "/mysql/pass",  e.password);
    }

    // Clean up any stale entries beyond current count
    int old = m_servers.size();
    while (s.contains(QString("dbservers/%1/id").arg(old))) {
        s.remove(QString("dbservers/%1").arg(old));
        ++old;
    }
}

// ─── Migration from legacy database/* keys ──────────────────────────────────
void DbServerRegistry::migrateFromLegacy() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    const QString backend = s.value("database/backend", "sqlite").toString();

    DbServerEntry e;
    e.id = DbServerEntry::newId();

    if (backend == "mysql") {
        e.displayName = "MySQL (Migrated)";
        e.backend     = DbServerEntry::Backend::MySQL;
        e.host        = s.value("database/mysql/host",     "127.0.0.1").toString();
        e.port        = s.value("database/mysql/port",     3306).toInt();
        e.username    = s.value("database/mysql/user",     "root").toString();
        e.password    = s.value("database/mysql/password", "").toString();
    } else {
        e.displayName = "Local SQLite (Migrated)";
        e.backend     = DbServerEntry::Backend::SQLite;
        e.sqlitePath  = s.value("database/sqlite/path", "").toString();
    }

    m_servers.append(e);
    m_defaultServerId = e.id;
    saveToSettings();

    // Remove legacy keys
    s.remove("database/backend");
    s.remove("database/sqlite");
    s.remove("database/mysql");

    qInfo() << "[DbServerRegistry] Migrated legacy database settings to server registry.";
}

// ─── Test connection ─────────────────────────────────────────────────────────
QString DbServerRegistry::testConnection(const DbServerEntry& entry) {
    if (entry.isSQLite()) {
        // For SQLite, just verify the directory is writable
        QString path = entry.sqlitePath;
        if (path.isEmpty()) {
            path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        } else {
            path = QFileInfo(path).absolutePath();
        }
        QDir dir(path);
        if (!dir.exists() && !dir.mkpath("."))
            return "Cannot create directory: " + path;
        return {};  // success
    }

    // MySQL: attempt connection (deferred to actual MySQL test when module is available)
    // For now, basic host/port validation
    if (entry.host.isEmpty())
        return "Host cannot be empty";
    if (entry.port < 1 || entry.port > 65535)
        return "Port must be 1-65535";
    if (entry.username.isEmpty())
        return "Username cannot be empty";

    return {};  // basic validation passed
}

} // namespace M1
