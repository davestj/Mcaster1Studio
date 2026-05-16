#pragma once
#include <QObject>
#include <QVariantMap>
#include <QList>
#include <QString>

namespace M1 {

class SqliteManager;

/// PersonaManager — manages AI persona presets, CRUD, and daypart resolution.
///
/// Provides:
///   - seedPresets()      — inserts 15 built-in personas (INSERT OR IGNORE, idempotent)
///   - resolvedPrompt()   — daypart > surface > global persona resolution
///   - allPersonas()      — all personas for UI display
///   - personaPrompt(id)  — quick system_prompt lookup by ID
class PersonaManager : public QObject {
    Q_OBJECT

public:
    explicit PersonaManager(M1::SqliteManager* db, QObject* parent = nullptr);

    /// Seed preset personas if none exist in DB. Safe to call multiple times.
    void seedPresets();

    /// Get resolved persona for current context (daypart > surface > global).
    QString resolvedPrompt(const QString& surfaceName = {}) const;

    /// Get all personas for UI display.
    QList<QVariantMap> allPersonas() const;

    /// Quick access to a persona's system_prompt by ID.
    QString personaPrompt(qint64 id) const;

private:
    M1::SqliteManager* m_db;
};

} // namespace M1
