#pragma once
#include "IModule.h"
#include "IDbAwareModule.h"
#include "SurfaceDbContext.h"

class DatabaseWidget;

namespace M1 {

/// DatabaseModule — surface-level module for managing the database
/// assigned to the owning surface.
///
/// Provides:
///   - Connection status and diagnostics
///   - Table browser for the surface's database
///   - Backup / Restore / Migration actions
///   - Performance metrics (query count, avg latency)
///
/// Implements IDbAwareModule so it receives its surface's DB context
/// from MainWindow before initialize().
class DatabaseModule : public IModule, public IDbAwareModule {
    Q_OBJECT

public:
    explicit DatabaseModule(QObject* parent = nullptr);
    ~DatabaseModule() override;

    // ── IModule ──────────────────────────────────────────────────
    QString  moduleId()    const override { return "com.mcaster1.database"; }
    QString  displayName() const override { return "Database"; }
    QSize    minimumModuleSize() const override { return {360, 280}; }
    QWidget* createWidget(QWidget* parent) override;
    void     saveState(QSettings& s) override;
    void     loadState(QSettings& s) override;

    // ── IDbAwareModule ───────────────────────────────────────────
    void setSurfaceDbContext(const SurfaceDbContext& ctx) override;
    SurfaceDbContext surfaceDbContext() const override { return m_dbCtx; }

    /// Convenience: the DatabaseWidget (if created).
    DatabaseWidget* widget() const { return m_widget; }

signals:
    /// Emitted when the database context or connection state changes.
    void connectionStateChanged(bool connected);

private:
    SurfaceDbContext m_dbCtx;
    DatabaseWidget*  m_widget = nullptr;
};

} // namespace M1
