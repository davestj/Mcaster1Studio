#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QMenu>
#include <QMap>
#include <QTimer>
#include <functional>
#include "AudioEngine.h"
#include "ISurface.h"
#include "SurfaceConfig.h"

class SurfaceTabBar;
class SurfaceWidget;
class SurfaceWindow;
class AppRibbon;
class QueueModule;
class CrossfaderModule;

namespace M1 {
class IModule;
class VUMeterModule;
class DeckModule;
class DeckAModule;
class DeckBModule;
class MediaLibraryModule;
class EncoderModule;
class EffectsRackModule;
class MetadataModule;
class PlaylistModule;
class PTTModule;
class PodcastModule;
class VideoModule;
class MonitorModule;
class ClockModule;
class CartWallModule;
class HealthModule;
}

/// MainWindow — top-level application window for Mcaster1Studio.
///
/// Surface life-cycle:
///   - On startup: load saved YAML configs from AppConfigLocation/surfaces/
///   - On "+" / Surfaces menu: open a new surface tab, load or create its YAML config
///   - On exit: save all open surface configs back to YAML
///
/// Module instantiation is handled by the internal module factory map,
/// which maps module IDs ("com.mcaster1.deck") to factory lambdas.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onPreferences();
    void onDeviceSettings();
    void onAddSurface();
    void onAudioStateChanged(M1::AudioEngineState state);
    void onLevelsUpdated(float inL, float inR, float outL, float outR);
    void onAbout();
    void onAddCustomSurface();
    void updateNowPlayingStatus(const QString& trackTitle);
    void updateEncoderStatus();

private:
    void setupMenuBar();
    void setupStatusBar();
    void startAudioEngine();
    void initModuleFactory();

    /// Load all surfaces from saved YAML configs. If none exist, create Surface Alpha defaults.
    void loadSavedSurfaces();

    /// Save all open surface configs to YAML.
    void saveAllSurfaces();

    /// Open a surface tab from a SurfaceConfig (or default config).
    SurfaceWidget* openSurfaceFromConfig(const M1::SurfaceConfig& cfg);

    /// Create a module by ID using the factory map.
    M1::IModule* createModule(const QString& moduleId, QObject* parent);

    /// Wire inter-module connections (deck → encoder, library → deck, etc.)
    /// Called after each new Surface Alpha / DJ surface is built.
    void wireModuleConnections();
    void wireChurchModules();
    void wireStatusIndicators();

    void applyTheme();
    void saveWindowState();
    void restoreWindowState();
    void addModuleToCurrentSurface(const QString& moduleId);

    /// Save ALL open surfaces to QSettings using stable v2 index-based keys.
    /// This is the single source of truth for session persistence.
    void saveAllSessionState();

    /// Restart the auto-save debounce timer (1.5s). Called on any structural change.
    void scheduleAutoSave();

    /// Connect a newly-opened SurfaceWidget to auto-save triggers and module-add handler.
    void connectSurfaceAutoSave(SurfaceWidget* sw);

    /// Save one surface's session (delegates to saveAllSessionState + shows status msg).
    void saveSurface(SurfaceWidget* sw);

    /// Save all surface layout snapshots to the YAML layout file.
    void saveAllLayouts();
    /// Restore all surface layout snapshots from the YAML layout file.
    void restoreAllLayouts();
    /// Path to the layout YAML file (same directory as the executable).
    static QString layoutYamlPath();

    /// Pop out a surface into a standalone SurfaceWindow.
    void popOutSurface(SurfaceWidget* sw);
    /// Pop out a surface directly onto a specific monitor.
    void sendSurfaceToMonitor(SurfaceWidget* sw, int screenIndex);
    /// Dock a popped-out surface back into the tab bar.
    void dockBackSurface(SurfaceWidget* sw);

    AppRibbon*       m_appRibbon      = nullptr;
    SurfaceTabBar*   m_surfaceBar     = nullptr;
    M1::AudioEngine* m_audio          = nullptr;
    QMenu*           m_surfacesMenu   = nullptr;
    QTimer*          m_autoSaveTimer  = nullptr;

    // Status bar widgets
    QLabel* m_audioStatusLabel  = nullptr;
    QLabel* m_engineStatusLabel = nullptr;
    QLabel* m_statusNowPlaying  = nullptr;
    QLabel* m_statusEncoders    = nullptr;

    // Module factory: moduleId → lambda(parent) → IModule*
    using ModuleFactoryFn = std::function<M1::IModule*(QObject*)>;
    QMap<QString, ModuleFactoryFn> m_moduleFactory;

    // Well-known module refs for cross-wiring (can be null if not loaded)
    M1::DeckModule*         m_deck     = nullptr;
    M1::DeckAModule*        m_deckA    = nullptr;
    M1::DeckBModule*        m_deckB    = nullptr;
    M1::MediaLibraryModule* m_library  = nullptr;
    M1::EncoderModule*      m_encoder  = nullptr;
    M1::EffectsRackModule*  m_effects  = nullptr;
    M1::MetadataModule*     m_metadata = nullptr;
    M1::PlaylistModule*     m_playlist = nullptr;
    M1::PTTModule*          m_ptt      = nullptr;
    M1::PodcastModule*      m_podcast  = nullptr;
    M1::VideoModule*        m_video    = nullptr;
    M1::MonitorModule*      m_monitor  = nullptr;
    M1::ClockModule*        m_clock    = nullptr;
    M1::CartWallModule*     m_cartwall = nullptr;
    M1::HealthModule*       m_health   = nullptr;
    QueueModule*            m_queue      = nullptr;
    CrossfaderModule*       m_crossfader = nullptr;

    // Pop-out window management
    QMap<SurfaceWidget*, SurfaceWindow*> m_poppedOutWindows;
};
