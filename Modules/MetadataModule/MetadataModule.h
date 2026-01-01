#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include "IcyMetadata.h"
#include <QString>

class Icy22EditorWidget;

namespace M1 {

/// MetadataModule — Phase 6 ICY 1.x + ICY 2.2 metadata editor and pusher.
///
/// Provides a full 8-tab ICY 2.2 metadata editor (70+ fields) and a
/// DNAS target configuration panel. On "Push Now" or programmatic push,
/// sends ICY 1.x StreamTitle= AND ICY 2.2 headers to the configured
/// Mcaster1DNAS endpoint.
///
/// onMetadataUpdate() is the receive path — called when DeckModule or
/// PlaylistModule resolves a new track. It stores the metadata and, if
/// auto-push is enabled, pushes it immediately.
///
/// Qt thread safety: all methods must be called from the Qt main thread
/// unless noted. The RT audio thread never touches this module.
class MetadataModule : public IModule {
    Q_OBJECT

public:
    explicit MetadataModule(QObject* parent = nullptr);
    ~MetadataModule() override;

    // ── IModule ──────────────────────────────────────────────────────────
    QString  moduleId()      const override { return "com.mcaster1.metadata"; }
    QString  displayName()   const override { return "Metadata"; }
    QSize    preferredSize() const override { return {700, 500}; }

    QWidget* createWidget(QWidget* parent) override;

    void initialize() override;
    void shutdown()   override;

    void onMetadataUpdate(const IcyMetadata& meta) override;

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── DNAS push ────────────────────────────────────────────────────────

    /// Push current metadata to the configured DNAS target.
    /// Sends ICY 1.x StreamTitle= first, then ICY 2.2 headers.
    void pushToDnas(const QString& host,
                    int            port,
                    const QString& mount,
                    const QString& user,
                    const QString& pass);

    // ── Accessors (Qt thread) ────────────────────────────────────────────
    const IcyMetadata& currentMetadata() const { return m_current; }

    // DNAS connection params (persisted)
    QString dnasHost()  const { return m_dnasHost; }
    int     dnasPort()  const { return m_dnasPort; }
    QString dnasMount() const { return m_dnasMount; }
    QString dnasUser()  const { return m_dnasUser; }
    QString dnasPass()  const { return m_dnasPass; }

    void setDnasHost(const QString& v)  { m_dnasHost  = v; }
    void setDnasPort(int v)             { m_dnasPort  = v; }
    void setDnasMount(const QString& v) { m_dnasMount = v; }
    void setDnasUser(const QString& v)  { m_dnasUser  = v; }
    void setDnasPass(const QString& v)  { m_dnasPass  = v; }

signals:
    /// Emitted after m_current is updated (Qt thread, queued-safe).
    void metadataChanged(const M1::IcyMetadata& meta);

private:
    IcyMetadata        m_current;
    Icy22EditorWidget* m_widget = nullptr;

    // DNAS connection params
    QString m_dnasHost;
    int     m_dnasPort  = 8000;
    QString m_dnasMount = "/live";
    QString m_dnasUser  = "source";
    QString m_dnasPass;
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_metadata_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_metadata_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_metadata_destroy(M1::IModule*);
}
