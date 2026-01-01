#include "SurfaceConfig.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace M1 {

// ─── YAML write helpers ───────────────────────────────────────────────────────
static QString yamlStr(const QString& s) {
    // Wrap in double-quotes if the value contains special chars
    if (s.contains(':') || s.contains('#') || s.contains('"') || s.isEmpty())
        return "\"" + QString(s).replace("\"", "\\\"") + "\"";
    return s;
}

static QString dbModeStr(DbConfig::Mode m) {
    return m == DbConfig::Mode::Local ? "local" : "system";
}

static DbConfig::Mode parseModeStr(const QString& s) {
    return (s.trimmed() == "local") ? DbConfig::Mode::Local : DbConfig::Mode::System;
}

// ─── SurfaceConfig::save ─────────────────────────────────────────────────────
bool SurfaceConfig::save(const QString& path) const {
    QDir dir = QFileInfo(path).dir();
    if (!dir.exists())
        dir.mkpath(".");

    QFile f(path);
    if (!f.open(QFile::WriteOnly | QFile::Text)) {
        qWarning() << "[SurfaceConfig] Cannot write:" << path;
        return false;
    }

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);

    out << "# Mcaster1Studio Surface Configuration\n";
    out << "# Auto-generated — safe to edit manually.\n\n";

    out << "surface_type: " << surfaceType << "\n";
    out << "surface_name: " << yamlStr(surfaceName) << "\n\n";

    out << "database:\n";
    out << "  mode: "     << dbModeStr(database.mode)  << "\n";
    out << "  host: "     << yamlStr(database.host)     << "\n";
    out << "  port: "     << database.port              << "\n";
    out << "  database: " << yamlStr(database.database) << "\n";
    out << "  username: " << yamlStr(database.username) << "\n";
    out << "  password: " << yamlStr(database.password) << "\n\n";

    out << "modules:\n";
    for (const auto& m : modules) {
        out << "  - id: "      << yamlStr(m.id)           << "\n";
        out << "    enabled: " << (m.enabled ? "true" : "false") << "\n";
        if (m.hasDbOverride) {
            out << "    database:\n";
            out << "      mode: "     << dbModeStr(m.db.mode)  << "\n";
            out << "      host: "     << yamlStr(m.db.host)     << "\n";
            out << "      port: "     << m.db.port              << "\n";
            out << "      database: " << yamlStr(m.db.database) << "\n";
            out << "      username: " << yamlStr(m.db.username) << "\n";
            out << "      password: " << yamlStr(m.db.password) << "\n";
        }
        // Arbitrary module settings
        for (auto it = m.settings.constBegin(); it != m.settings.constEnd(); ++it) {
            out << "    " << it.key() << ": " << yamlStr(it.value()) << "\n";
        }
    }
    return true;
}

// ─── Minimal line-oriented YAML reader (covers our schema) ───────────────────
// Supports: top-level scalars, one-level mappings, sequences of mappings.
// Does NOT handle: anchors, tags, multi-line scalars, or inline flow notation.

namespace {

struct ParseCtx {
    QStringList lines;
    int         i = 0;

    bool atEnd() const { return i >= lines.size(); }

    QString line() const { return i < lines.size() ? lines[i] : QString{}; }

    // Return indent level (number of leading spaces) of current line.
    int indent() const {
        const QString& l = line();
        int n = 0;
        while (n < l.size() && l[n] == ' ') ++n;
        return n;
    }

    void advance() { ++i; }

    // Strip YAML inline comment and trim
    static QString stripComment(const QString& s) {
        // Only strip ' #' not '#' within quoted strings
        bool inQuote = false;
        for (int j = 0; j < s.size(); ++j) {
            if (s[j] == '"') inQuote = !inQuote;
            if (!inQuote && s[j] == '#' && j > 0 && s[j-1] == ' ')
                return s.left(j).trimmed();
        }
        return s.trimmed();
    }

    // Parse value from "key: value" (returns right-hand side, unquoted).
    static QString parseValue(const QString& s) {
        int col = s.indexOf(':');
        if (col < 0) return {};
        QString v = stripComment(s.mid(col + 1).trimmed());
        if (v.startsWith('"') && v.endsWith('"') && v.size() >= 2)
            v = v.mid(1, v.size() - 2).replace("\\\"", "\"");
        return v;
    }

    static QString parseKey(const QString& s) {
        int col = s.indexOf(':');
        return col >= 0 ? s.left(col).trimmed() : QString{};
    }
};

DbConfig parseDbBlock(ParseCtx& ctx, int blockIndent) {
    DbConfig db;
    while (!ctx.atEnd()) {
        const QString l = ctx.line().trimmed();
        if (l.isEmpty() || l.startsWith('#')) { ctx.advance(); continue; }
        if (ctx.indent() <= blockIndent) break;

        const QString key = ParseCtx::parseKey(ctx.line());
        const QString val = ParseCtx::parseValue(ctx.line());

        if      (key == "mode")     db.mode     = parseModeStr(val);
        else if (key == "host")     db.host     = val;
        else if (key == "port")     db.port     = val.toInt();
        else if (key == "database") db.database = val;
        else if (key == "username") db.username = val;
        else if (key == "password") db.password = val;

        ctx.advance();
    }
    return db;
}

ModuleConfig parseModuleBlock(ParseCtx& ctx, int seqIndent) {
    ModuleConfig mod;
    while (!ctx.atEnd()) {
        const QString raw = ctx.line();
        const QString l   = raw.trimmed();
        if (l.isEmpty() || l.startsWith('#')) { ctx.advance(); continue; }
        // A new "- id:" at the same indent level = next list item → stop
        if (ctx.indent() <= seqIndent && l.startsWith('-')) break;
        if (ctx.indent() <  seqIndent) break;

        const QString key = ParseCtx::parseKey(raw);
        const QString val = ParseCtx::parseValue(raw);

        if      (key == "id")      mod.id      = val;
        else if (key == "enabled") mod.enabled = (val == "true" || val == "1");
        else if (key == "database") {
            ctx.advance();
            mod.db            = parseDbBlock(ctx, ctx.indent() - 1);
            mod.hasDbOverride = true;
            continue;
        } else if (!key.isEmpty()) {
            mod.settings[key] = val;
        }
        ctx.advance();
    }
    return mod;
}

} // anon

// ─── SurfaceConfig::load ─────────────────────────────────────────────────────
SurfaceConfig SurfaceConfig::load(const QString& path) {
    QFile f(path);
    if (!f.open(QFile::ReadOnly | QFile::Text)) {
        qWarning() << "[SurfaceConfig] Cannot read:" << path;
        return {};
    }

    ParseCtx ctx;
    {
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);
        while (!in.atEnd())
            ctx.lines << in.readLine();
    }

    SurfaceConfig cfg;

    while (!ctx.atEnd()) {
        const QString raw = ctx.line();
        const QString l   = raw.trimmed();
        ctx.advance();

        if (l.isEmpty() || l.startsWith('#')) continue;

        const QString key = ParseCtx::parseKey(raw);
        const QString val = ParseCtx::parseValue(raw);

        if      (key == "surface_type") cfg.surfaceType = val;
        else if (key == "surface_name") cfg.surfaceName = val;
        else if (key == "database") {
            cfg.database = parseDbBlock(ctx, ctx.indent() - 1);
        }
        else if (key == "modules") {
            // Parse sequence
            while (!ctx.atEnd()) {
                const QString ml = ctx.line().trimmed();
                if (ml.isEmpty() || ml.startsWith('#')) { ctx.advance(); continue; }
                if (!ml.startsWith('-')) break; // end of modules block
                const int seqIndent = ctx.indent();
                ctx.advance();
                ModuleConfig mod = parseModuleBlock(ctx, seqIndent);
                // The "- id:" line itself may have been the first line of the module
                if (mod.id.isEmpty()) {
                    // id was on the "- id: xxx" line — parse it
                    const QString firstLine = "  " + ml.mid(1).trimmed(); // remove "-"
                    const QString fKey = ParseCtx::parseKey(firstLine);
                    const QString fVal = ParseCtx::parseValue(firstLine);
                    if (fKey == "id") mod.id = fVal;
                }
                if (!mod.id.isEmpty())
                    cfg.modules.append(mod);
            }
        }
    }

    return cfg;
}

// ─── Default configs per surface type ────────────────────────────────────────
SurfaceConfig SurfaceConfig::defaultForType(const QString& typeStr) {
    SurfaceConfig cfg;
    cfg.surfaceType = typeStr;

    // Pretty name
    static const QMap<QString,QString> names = {
        {"alpha",         "Surface Alpha"},
        {"beta",          "Surface Beta"},
        {"company",       "Surface Company"},
        {"dj",            "Surface DJ"},
        {"entertainment", "Surface Entertainment"},
        {"social",        "Surface Social"},
        {"podcast",       "Surface Podcast"},
        {"church",        "Surface Church"},
        {"custom",        "Custom Surface"},
    };
    cfg.surfaceName = names.value(typeStr, "Surface " + typeStr);

    // Default modules vary by surface type
    auto addMod = [&](const QString& id, bool enabled = true) {
        ModuleConfig m; m.id = id; m.enabled = enabled;
        cfg.modules.append(m);
    };

    if (typeStr == "alpha" || typeStr == "beta") {
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.deck");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
        addMod("com.mcaster1.metadata");
        addMod("com.mcaster1.effects", false);
    } else if (typeStr == "dj") {
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.deck");
        addMod("com.mcaster1.effects");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
    } else if (typeStr == "podcast") {
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.ptt");
        addMod("com.mcaster1.podcast");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
    } else if (typeStr == "church") {
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.ptt");
        addMod("com.mcaster1.deck");
        addMod("com.mcaster1.video");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
        addMod("com.mcaster1.podcast");
    } else if (typeStr == "entertainment" || typeStr == "social") {
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.video");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
    } else if (typeStr == "company") {
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.deck");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
        addMod("com.mcaster1.playlist");
    } else {
        // Custom / fallback
        addMod("com.mcaster1.vumeter");
        addMod("com.mcaster1.deck");
        addMod("com.mcaster1.library");
        addMod("com.mcaster1.encoder");
    }

    return cfg;
}

// ─── Config file path ─────────────────────────────────────────────────────────
QString SurfaceConfig::configPath(const QString& typeStr) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                      + "/Mcaster1Studio/surfaces";
    QDir().mkpath(dir);
    return dir + "/surface_" + typeStr + ".yaml";
}

} // namespace M1
