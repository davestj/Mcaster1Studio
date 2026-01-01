#pragma once
#include <QString>
#include <QList>
#include <QMap>

namespace M1 {

// ─── Database connection config ───────────────────────────────────────────────
struct DbConfig {
    enum class Mode { System, Local };

    Mode    mode     = Mode::System;  ///< "system" = use app-level DB; "local" = own connection
    QString host     = "localhost";
    int     port     = 3306;
    QString database = "mcaster1studio";
    QString username = "mcaster1";
    QString password;

    bool isSystem() const { return mode == Mode::System; }
};

// ─── Per-module config within a surface ──────────────────────────────────────
struct ModuleConfig {
    QString               id;           ///< "com.mcaster1.deck"
    bool                  enabled = true;
    QMap<QString,QString> settings;     ///< arbitrary key→value for module-specific settings

    /// Convenience: database override for modules that support per-surface DB
    /// (e.g. com.mcaster1.library).  Empty = inherit surface DB config.
    DbConfig db;
    bool     hasDbOverride = false;
};

// ─── Complete surface configuration ──────────────────────────────────────────
struct SurfaceConfig {
    QString            surfaceType;   ///< "alpha", "beta", "dj", etc.
    QString            surfaceName;   ///< "Surface Alpha"
    DbConfig           database;      ///< default DB for this surface
    QList<ModuleConfig> modules;      ///< ordered list of modules to load

    // ── Factory helpers ──────────────────────────────────────────────────────
    /// Load from a YAML file. Returns a default config on parse failure.
    static SurfaceConfig load(const QString& path);

    /// Save to a YAML file. Returns true on success.
    bool save(const QString& path) const;

    /// Generate the default YAML config for a given surface type.
    static SurfaceConfig defaultForType(const QString& typeStr);

    /// Return the canonical YAML file path for a surface type string,
    /// rooted under the application config directory.
    static QString configPath(const QString& typeStr);

    bool isValid() const { return !surfaceType.isEmpty(); }
};

} // namespace M1
