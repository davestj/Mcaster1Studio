#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

namespace M1 {

/// SqlDialect — abstract interface for generating SQL statements
/// that are correct for a specific database engine.
///
/// Each backend has different syntax for:
///   - Auto-increment primary keys
///   - String quoting and identifier quoting
///   - Date/time functions and types
///   - Schema/table creation clauses
///   - Boolean types
///   - LIMIT/OFFSET syntax
///
/// Concrete subclasses: SqliteDialect, MySqlDialect, PostgresDialect,
///                      FirebirdDialect, MssqlDialect
class SqlDialect {
public:
    virtual ~SqlDialect() = default;

    /// Human-readable name: "SQLite", "MySQL", "PostgreSQL", etc.
    virtual QString name() const = 0;

    // ── Identifier quoting ────────────────────────────────────────────
    /// Quote a table or column name (e.g., `name` or "name" or [name]).
    virtual QString quoteId(const QString& identifier) const = 0;

    // ── Type mappings ─────────────────────────────────────────────────
    /// Primary key auto-increment column definition.
    /// SQLite: "INTEGER PRIMARY KEY AUTOINCREMENT"
    /// MySQL:  "BIGINT AUTO_INCREMENT PRIMARY KEY"
    /// PG:     "BIGSERIAL PRIMARY KEY"
    virtual QString autoIncrementPK() const = 0;

    /// VARCHAR(n) — some backends differ.
    virtual QString varcharType(int maxLen) const {
        return QString("VARCHAR(%1)").arg(maxLen);
    }

    /// TEXT type (unlimited length).
    virtual QString textType() const { return "TEXT"; }

    /// Integer type.
    virtual QString intType() const { return "INT"; }

    /// Big integer type.
    virtual QString bigintType() const { return "BIGINT"; }

    /// Double/float type.
    virtual QString doubleType() const { return "DOUBLE"; }

    /// Real type (for bpm, energy, etc.).
    virtual QString realType() const { return "REAL"; }

    /// Boolean type.
    virtual QString boolType() const { return "INT"; }

    /// Datetime type.
    virtual QString datetimeType() const { return "DATETIME"; }

    // ── Functions ─────────────────────────────────────────────────────
    /// Current timestamp expression for INSERT/UPDATE.
    /// SQLite: datetime('now')  |  MySQL: NOW()  |  PG: NOW()
    virtual QString nowFunction() const = 0;

    // ── DDL helpers ───────────────────────────────────────────────────
    /// CREATE DATABASE statement (empty for embedded dbs like SQLite).
    virtual QString createDatabaseSql(const QString& dbName) const {
        (void)dbName;
        return {};
    }

    /// Table suffix clause (e.g., ENGINE=InnoDB for MySQL, empty otherwise).
    virtual QString tableOptions() const { return {}; }

    /// CREATE TABLE IF NOT EXISTS for the media_items schema.
    /// Uses dialect-specific types and syntax.
    virtual QString createMediaItemsTableSql(const QString& tablePrefix = {}) const;

    /// CREATE TABLE statements for all auxiliary tables (categories, playlists, etc.)
    virtual QString createLibraryCategoriesTableSql(const QString& prefix = {}) const;
    virtual QString createTrackCategoriesTableSql(const QString& prefix = {}) const;
    virtual QString createPlaylistsTableSql(const QString& prefix = {}) const;
    virtual QString createPlaylistTracksTableSql(const QString& prefix = {}) const;
    virtual QString createArtistIntelTableSql(const QString& prefix = {}) const;
    virtual QString createStreamFavoritesTableSql(const QString& prefix = {}) const;
    virtual QString createAiPersonasTableSql(const QString& prefix = {}) const;
    virtual QString createDaypartScheduleTableSql(const QString& prefix = {}) const;

    /// Returns all DDL for complete schema (media_items + all auxiliary tables).
    QStringList createFullSchemaSql(const QString& prefix = {}) const;

    // ── LIMIT/OFFSET ──────────────────────────────────────────────────
    /// LIMIT clause for SELECT (e.g., "LIMIT 10 OFFSET 5").
    /// MSSQL uses TOP or OFFSET...FETCH instead.
    virtual QString limitClause(int limit, int offset = 0) const {
        if (offset > 0)
            return QString("LIMIT %1 OFFSET %2").arg(limit).arg(offset);
        return QString("LIMIT %1").arg(limit);
    }

    // ── String escaping ───────────────────────────────────────────────
    /// Escape a string value for use in SQL (NOT quoting, just escaping).
    /// Default: double single-quotes. Override for backend-specific escaping.
    virtual QString escapeString(const QString& s) const {
        QString result = s;
        result.replace('\'', "''");
        return result;
    }

    /// Wrap a value in single quotes with escaping.
    QString quotedValue(const QString& s) const {
        return '\'' + escapeString(s) + '\'';
    }

    // ── Unique constraint syntax ──────────────────────────────────────
    /// UNIQUE constraint for file_path column.
    /// MySQL needs key length limit; others don't.
    virtual QString uniqueFilePathConstraint() const {
        return "UNIQUE(" + quoteId("file_path") + ")";
    }
};

// ─── Concrete Dialects ──────────────────────────────────────────────────────

class SqliteDialect : public SqlDialect {
public:
    QString name()            const override { return "SQLite"; }
    QString quoteId(const QString& id) const override { return '"' + id + '"'; }
    QString autoIncrementPK() const override { return "INTEGER PRIMARY KEY AUTOINCREMENT"; }
    QString doubleType()      const override { return "REAL"; }
    QString boolType()        const override { return "INTEGER"; }
    QString datetimeType()    const override { return "TEXT"; }
    QString nowFunction()     const override { return "datetime('now')"; }
};

class MySqlDialect : public SqlDialect {
public:
    QString name()            const override { return "MySQL"; }
    QString quoteId(const QString& id) const override { return '`' + id + '`'; }
    QString autoIncrementPK() const override { return "BIGINT AUTO_INCREMENT PRIMARY KEY"; }
    QString realType()        const override { return "DOUBLE"; }
    QString boolType()        const override { return "TINYINT(1)"; }
    QString nowFunction()     const override { return "NOW()"; }

    QString createDatabaseSql(const QString& dbName) const override {
        return QString("CREATE DATABASE IF NOT EXISTS `%1` "
                       "CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci").arg(dbName);
    }

    QString tableOptions() const override {
        return "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci";
    }

    QString uniqueFilePathConstraint() const override {
        return "UNIQUE KEY `idx_file_path` (`file_path`(512))";
    }
};

class PostgresDialect : public SqlDialect {
public:
    QString name()            const override { return "PostgreSQL"; }
    QString quoteId(const QString& id) const override { return '"' + id + '"'; }
    QString autoIncrementPK() const override { return "BIGSERIAL PRIMARY KEY"; }
    QString realType()        const override { return "DOUBLE PRECISION"; }
    QString doubleType()      const override { return "DOUBLE PRECISION"; }
    QString boolType()        const override { return "BOOLEAN"; }
    QString datetimeType()    const override { return "TIMESTAMP"; }
    QString nowFunction()     const override { return "NOW()"; }

    QString createDatabaseSql(const QString& dbName) const override {
        // PostgreSQL: CREATE DATABASE is not IF NOT EXISTS natively.
        // We handle existence checks at the connection layer.
        return QString("CREATE DATABASE \"%1\" ENCODING 'UTF8'").arg(dbName);
    }
};

class FirebirdDialect : public SqlDialect {
public:
    QString name()            const override { return "Firebird"; }
    QString quoteId(const QString& id) const override { return '"' + id.toUpper() + '"'; }
    QString autoIncrementPK() const override { return "INTEGER GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY"; }
    QString varcharType(int maxLen) const override {
        // Firebird max VARCHAR is 32765 bytes
        return QString("VARCHAR(%1)").arg(qMin(maxLen, 32765));
    }
    QString boolType()        const override { return "SMALLINT"; }
    QString datetimeType()    const override { return "TIMESTAMP"; }
    QString nowFunction()     const override { return "CURRENT_TIMESTAMP"; }

    QString limitClause(int limit, int offset) const override {
        // Firebird uses FIRST/SKIP syntax (or ROWS in newer versions)
        if (offset > 0)
            return QString("ROWS %1 TO %2").arg(offset + 1).arg(offset + limit);
        return QString("FIRST %1").arg(limit);
    }
};

class MssqlDialect : public SqlDialect {
public:
    QString name()            const override { return "SQL Server"; }
    QString quoteId(const QString& id) const override { return '[' + id + ']'; }
    QString autoIncrementPK() const override { return "BIGINT IDENTITY(1,1) PRIMARY KEY"; }
    QString realType()        const override { return "FLOAT"; }
    QString doubleType()      const override { return "FLOAT"; }
    QString boolType()        const override { return "BIT"; }
    QString datetimeType()    const override { return "DATETIME2"; }
    QString nowFunction()     const override { return "GETDATE()"; }

    QString createDatabaseSql(const QString& dbName) const override {
        // SQL Server doesn't support IF NOT EXISTS in CREATE DATABASE;
        // handled at the connection layer.
        return QString("CREATE DATABASE [%1]").arg(dbName);
    }

    QString limitClause(int limit, int offset) const override {
        // SQL Server 2012+ uses OFFSET...FETCH
        if (offset > 0)
            return QString("OFFSET %1 ROWS FETCH NEXT %2 ROWS ONLY").arg(offset).arg(limit);
        return QString("OFFSET 0 ROWS FETCH NEXT %1 ROWS ONLY").arg(limit);
    }
};

// ─── Dialect Factory ────────────────────────────────────────────────────────

/// Get a SqlDialect instance for the given backend type.
/// Returns a static singleton — do NOT delete.
SqlDialect* dialectFor(int backendEnumValue);

} // namespace M1
