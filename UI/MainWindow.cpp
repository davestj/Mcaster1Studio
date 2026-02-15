#include "MainWindow.h"
#include "AppRibbon.h"
#include "WeatherWidget.h"
#include "SurfaceTabBar.h"
#include "SurfaceWidget.h"
#include "SubSurfaceTabBar.h"
#include "PreferencesDialog.h"
#include "DeviceSettingsDialog.h"
#include "ThemeManager.h"
#include "CustomSurfaceDialog.h"
#include "VUMeterModule.h"
#include "DeckModule.h"
#include "DeckAModule.h"
#include "DeckBModule.h"
#include "DeckPlayer.h"
#include "DeckWidget.h"
#include "CrossfaderSettingsDialog.h"
#include "MediaLibraryModule.h"
#include "LibraryModel.h"
#include "EncoderModule.h"
#include "EffectsRackModule.h"
#include "MetadataModule.h"
#include "PlaylistModule.h"
#include "PTTModule.h"
#include "PodcastModule.h"
#include "VideoModule.h"
#include "MonitorModule.h"
#include "ClockModule.h"
#include "QueueModule.h"
#include "CartWallModule.h"
#include "HealthModule.h"
#include "TimerClockModule.h"
#include "GraphicsEngineModule.h"
#include "LyricsCasterModule.h"
#include "ScriptureCasterModule.h"
#include "AnnounceCasterModule.h"
#include "TelePromptModule.h"
#include "MediaCasterModule.h"
#include "StageMonModule.h"
#include "AudioMixModule.h"
#include "TranscribeRecModule.h"
#include "SwitchCasterModule.h"
#include "ServiceRunnerModule.h"
#include "PodMixerModule.h"
#include "PodPTTModule.h"
#include "PodRecorderModule.h"
#include "PodSoundboardModule.h"
#include "PodFXModule.h"
#include "PodEditorModule.h"
#include "PodEncodeModule.h"
#include "PodTranscribeModule.h"
#include "PodShowNotesModule.h"
#include "PodRSSModule.h"
#include "PodPublisherModule.h"
#include "PodAnalyticsModule.h"
#include "PodRemoteModule.h"
#include "CrossfaderModule.h"
#include "ModuleRegistry.h"
#include "SurfaceRibbon.h"
#include "SubSurfacePanel.h"
#include "SurfaceConfig.h"
#include "SurfaceConfigYaml.h"
#include "ModuleEvents.h"
#include "SurfaceWindow.h"
#include "DbServerEntry.h"
#include "MonitorManager.h"
#include "version.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QSettings>
#include <QApplication>
#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QString("%1 v%2 — Broadcast Automation Studio")
                   .arg(MCASTER1STUDIO_APP_NAME)
                   .arg(MCASTER1STUDIO_VERSION_STRING));
    setMinimumSize(1024, 600);

    // ── App icon — multi-resolution from SVG sources ───────────────────────────
    {
        QIcon appIcon;
        static const struct { const char* path; int sz; } kSizes[] = {
            { ":/resources/appicon/app-icon-16.svg",  16  },
            { ":/resources/appicon/app-icon-32.svg",  32  },
            { ":/resources/appicon/app-icon-48.svg",  48  },
            { ":/resources/appicon/app-icon-64.svg",  64  },
            { ":/resources/appicon/app-icon-128.svg", 128 },
            { ":/resources/appicon/app-icon-256.svg", 256 },
            { ":/resources/appicon/app-icon-512.svg", 512 },
        };
        for (const auto& s : kSizes) {
            QSvgRenderer renderer(QString(s.path));
            if (renderer.isValid()) {
                QPixmap pm(s.sz, s.sz);
                pm.fill(Qt::transparent);
                QPainter p(&pm);
                renderer.render(&p);
                p.end();
                pm.setDevicePixelRatio(1.0);
                appIcon.addPixmap(pm);
            }
        }
        if (!appIcon.isNull()) {
            setWindowIcon(appIcon);
            qApp->setWindowIcon(appIcon);
        }
    }

    applyTheme();

    // ── App ribbon + surface tab bar wrapped in a container ────────────────────
    m_appRibbon  = new AppRibbon(this);
    m_surfaceBar = new SurfaceTabBar(this);

    auto* container = new QWidget(this);
    auto* vlay      = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);
    vlay->addWidget(m_appRibbon);
    vlay->addWidget(m_surfaceBar, 1);
    setCentralWidget(container);

    m_audio = new M1::AudioEngine(this);
    connect(m_audio, &M1::AudioEngine::stateChanged,
            this, &MainWindow::onAudioStateChanged);
    connect(m_audio, &M1::AudioEngine::levelsUpdated,
            this, &MainWindow::onLevelsUpdated);

    // Auto-save debounce timer: 1.5s single-shot, fires saveAllSessionState
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setSingleShot(true);
    m_autoSaveTimer->setInterval(1500);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::saveAllSessionState);

    setupMenuBar();
    setupStatusBar();

    // ── Plugin discovery — scan plugins/ dir before factory init ─────────────
    const QString pluginsDir = QCoreApplication::applicationDirPath() + "/plugins";
    M1::initModuleRegistry(pluginsDir);

    initModuleFactory();

    connect(m_surfaceBar, &SurfaceTabBar::addSurfaceRequested, this, [this]() {
        if (m_surfacesMenu) {
            const QPoint globalPos = m_surfaceBar->mapToGlobal(
                QPoint(m_surfaceBar->width() - 120, m_surfaceBar->height()));
            m_surfacesMenu->exec(globalPos);
        }
    });

    // Master surface tab right-click "Save Session" → save all session state
    connect(m_surfaceBar, &SurfaceTabBar::saveSessionRequested,
            this, [this](SurfaceWidget* sw) {
        if (sw) saveSurface(sw);
    });

    // Master surface tab rename (double-click) → schedule auto-save
    connect(m_surfaceBar->tabBar(), &QTabBar::tabBarDoubleClicked,
            this, [this]() { scheduleAutoSave(); });

    // Tab right-click "Add Module" → addModuleToCurrentSurface (start floating at min size)
    connect(m_surfaceBar, &SurfaceTabBar::addModuleToSurfaceRequested,
            this, [this](SurfaceWidget* sw, const QString& id) {
        M1::IModule* mod = createModule(id, this);
        if (mod) {
            sw->addModule(mod, /*startFloating=*/true);
            wireModuleConnections();
        }
    });

    // Tab right-click "Create new Sub Surface" (custom or template)
    connect(m_surfaceBar, &SurfaceTabBar::createSubSurfaceRequested,
            this, [this](SurfaceWidget* sw, const QString& name,
                         const QColor& color, const QStringList& moduleIds) {
        // Make sure this surface tab is active
        m_surfaceBar->setCurrentWidget(sw);
        // Create the chip + backing panel
        const int subIdx = sw->subTabBar()->addSubSurface(name, color);
        sw->subTabBar()->setCurrentIndex(subIdx);
        // Add template modules (if any) to the newly-active sub-surface panel
        for (const auto& id : moduleIds) {
            M1::IModule* mod = createModule(id, this);
            if (mod) {
                sw->addModule(mod);
            }
        }
        if (!moduleIds.isEmpty())
            wireModuleConnections();
    });

    // Tab right-click "Pop Out Window" → detach surface to standalone window
    connect(m_surfaceBar, &SurfaceTabBar::popOutRequested,
            this, [this](SurfaceWidget* sw) { popOutSurface(sw); });

    // Tab right-click "Send to Monitor" → pop out + maximize on target screen
    connect(m_surfaceBar, &SurfaceTabBar::sendToMonitorRequested,
            this, [this](SurfaceWidget* sw, int screenIndex) {
        sendSurfaceToMonitor(sw, screenIndex);
    });

    // AppRibbon "+ Add" picker signals
    connect(m_appRibbon, &AppRibbon::addClockRequested, this, [this]() {
        auto* clock = new M1::ClockModule(m_appRibbon);
        clock->initialize();
        m_appRibbon->addBox(clock->createCompactWidget(m_appRibbon), "clock");
    });
    connect(m_appRibbon, &AppRibbon::addWeatherRequested, this, [this]() {
        static int weatherCount = 0;
        auto* w = new WeatherWidget(QString::number(++weatherCount), m_appRibbon);
        m_appRibbon->addBox(w, "weather");
    });
    connect(m_appRibbon, &AppRibbon::addVUMeterRequested, this, [this]() {
        auto* vu = new M1::VUMeterModule(m_appRibbon);
        vu->setAudioEngine(m_audio);
        vu->initialize();
        m_appRibbon->addBox(vu->createCompactWidget(m_appRibbon), "vumeter");
    });

    // Initialize singletons — DB server registry + monitor manager
    M1::DbServerRegistry::instance().loadFromSettings();
    M1::MonitorManager::instance().refresh();

    restoreWindowState();
    loadSavedSurfaces();
    startAudioEngine();

    // Default ribbon boxes: clock + weather
    auto* defaultClock = new M1::ClockModule(m_appRibbon);
    defaultClock->initialize();
    m_appRibbon->addBox(defaultClock->createCompactWidget(m_appRibbon), "clock");

    auto* defaultWeather = new WeatherWidget("0", m_appRibbon);
    m_appRibbon->addBox(defaultWeather, "weather");
}

MainWindow::~MainWindow() {
    if (m_audio) m_audio->shutdown();
    M1::shutdownModuleRegistry();
}

// ─── Theme ────────────────────────────────────────────────────────────────────
void MainWindow::applyTheme() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    ThemeManager::instance()->loadFromSettings(s);
}

// ─── Menu bar ─────────────────────────────────────────────────────────────────
void MainWindow::setupMenuBar() {
    // ── File ──────────────────────────────────────────────────────────────
    auto* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("E&xit", qApp, &QApplication::quit, QKeySequence::Quit);

    // ── Settings ──────────────────────────────────────────────────────────
    auto* settingsMenu = menuBar()->addMenu("&Settings");
    settingsMenu->addAction("&Preferences...", this, &MainWindow::onPreferences,
                            QKeySequence::Preferences);
    settingsMenu->addAction("&Device Settings...", this, &MainWindow::onDeviceSettings);
    settingsMenu->addSeparator();
    settingsMenu->addAction("C&rossfade Settings...", this, [this]() {
        if (m_deck) {
            m_deck->openCrossfadeSettings();
        } else {
            CrossfaderSettingsDialog dlg(this);
            dlg.loadSettings();
            if (dlg.exec() == QDialog::Accepted)
                dlg.saveSettings();
        }
    });
    settingsMenu->addAction("Deck &Appearance...", this, []() {
        QMessageBox::information(nullptr, "Deck Appearance",
            "Deck appearance settings will be available in a future release.");
    });

    m_surfacesMenu = menuBar()->addMenu("&Surfaces");
    auto* surfaceMenu = m_surfacesMenu;
    auto addSurface = [&](const QString& name, M1::SurfaceType type, const QString& typeStr) {
        surfaceMenu->addAction(name, this, [this, type, typeStr]() {
            // Open from saved config or create default
            const QString path = M1::SurfaceConfig::configPath(typeStr);
            M1::SurfaceConfig cfg = QFile::exists(path)
                ? M1::SurfaceConfig::load(path)
                : M1::SurfaceConfig::defaultForType(typeStr);
            if (!cfg.isValid()) cfg = M1::SurfaceConfig::defaultForType(typeStr);
            m_surfaceBar->openSurface(type); // creates the tab
            // The actual surface widget is the last tab
            auto* sw = qobject_cast<SurfaceWidget*>(
                m_surfaceBar->widget(m_surfaceBar->count() - 1));
            if (sw) openSurfaceFromConfig(cfg);
        });
    };

    addSurface("Surface &Alpha",         M1::SurfaceType::Alpha,         "alpha");
    addSurface("Surface &Beta",          M1::SurfaceType::Beta,          "beta");
    addSurface("Surface &Company",       M1::SurfaceType::Company,       "company");
    addSurface("Surface &DJ",            M1::SurfaceType::DJ,            "dj");
    addSurface("Surface &Entertainment", M1::SurfaceType::Entertainment, "entertainment");
    addSurface("Surface &Social",        M1::SurfaceType::Social,        "social");
    addSurface("Surface &Podcast",       M1::SurfaceType::Podcast,       "podcast");
    addSurface("Surface C&hurch",        M1::SurfaceType::Church,        "church");
    surfaceMenu->addSeparator();
    surfaceMenu->addAction("&Custom Surface...", this, &MainWindow::onAddCustomSurface);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About Mcaster1Studio...", this, &MainWindow::onAbout);
}

// ─── Status bar ───────────────────────────────────────────────────────────────
void MainWindow::setupStatusBar() {
    m_audioStatusLabel  = new QLabel("Audio: Initializing...", this);
    m_engineStatusLabel = new QLabel("Engine: Stopped", this);
    m_statusNowPlaying  = new QLabel("Now: —", this);
    m_statusEncoders    = new QLabel("Enc: 0/8", this);

    statusBar()->addWidget(m_audioStatusLabel, 1);
    statusBar()->addPermanentWidget(m_statusNowPlaying);
    statusBar()->addPermanentWidget(new QLabel(" | ", this));
    statusBar()->addPermanentWidget(m_statusEncoders);
    statusBar()->addPermanentWidget(new QLabel(" | ", this));
    statusBar()->addPermanentWidget(m_engineStatusLabel);
    statusBar()->showMessage("Welcome to Mcaster1Studio v" MCASTER1STUDIO_VERSION_STRING, 3000);
}

void MainWindow::updateNowPlayingStatus(const QString& trackTitle) {
    if (m_statusNowPlaying)
        m_statusNowPlaying->setText("Now: " + (trackTitle.isEmpty() ? "—" : trackTitle));
}

void MainWindow::updateEncoderStatus() {
    if (!m_statusEncoders || !m_encoder) return;
    int active = m_encoder->activeSlotCount();
    int total  = m_encoder->slotCount();
    m_statusEncoders->setText(QString("Enc: %1/%2").arg(active).arg(total));
}

// ─── Module factory ───────────────────────────────────────────────────────────
void MainWindow::initModuleFactory() {
    m_moduleFactory["com.mcaster1.vumeter"] = [this](QObject* p) -> M1::IModule* {
        auto* m = new M1::VUMeterModule(p);
        m->setAudioEngine(m_audio);
        return m;
    };
    m_moduleFactory["com.mcaster1.deck"] = [this](QObject* p) -> M1::IModule* {
        if (!m_deck) {
            m_deck = new M1::DeckModule(p);
        }
        return m_deck;
    };
    m_moduleFactory["com.mcaster1.deck.a"] = [this](QObject* p) -> M1::IModule* {
        if (!m_deckA) {
            m_deckA = new M1::DeckAModule(p);
        }
        return m_deckA;
    };
    m_moduleFactory["com.mcaster1.deck.b"] = [this](QObject* p) -> M1::IModule* {
        if (!m_deckB) {
            m_deckB = new M1::DeckBModule(p);
        }
        return m_deckB;
    };
    m_moduleFactory["com.mcaster1.library"] = [this](QObject* p) -> M1::IModule* {
        if (!m_library) {
            m_library = new M1::MediaLibraryModule(p);
        }
        return m_library;
    };
    m_moduleFactory["com.mcaster1.encoder"] = [this](QObject* p) -> M1::IModule* {
        if (!m_encoder) {
            m_encoder = new M1::EncoderModule(p);
        }
        return m_encoder;
    };
    m_moduleFactory["com.mcaster1.effects"] = [this](QObject* p) -> M1::IModule* {
        if (!m_effects) {
            m_effects = new M1::EffectsRackModule(p);
        }
        return m_effects;
    };
    m_moduleFactory["com.mcaster1.metadata"] = [this](QObject* p) -> M1::IModule* {
        if (!m_metadata) {
            m_metadata = new M1::MetadataModule(p);
        }
        return m_metadata;
    };
    m_moduleFactory["com.mcaster1.playlist"] = [this](QObject* p) -> M1::IModule* {
        if (!m_playlist) {
            m_playlist = new M1::PlaylistModule(p);
        }
        return m_playlist;
    };
    m_moduleFactory["com.mcaster1.ptt"] = [this](QObject* p) -> M1::IModule* {
        if (!m_ptt) {
            m_ptt = new M1::PTTModule(p);
        }
        return m_ptt;
    };
    m_moduleFactory["com.mcaster1.podcast"] = [this](QObject* p) -> M1::IModule* {
        if (!m_podcast) {
            m_podcast = new M1::PodcastModule(p);
        }
        return m_podcast;
    };
    m_moduleFactory["com.mcaster1.video"] = [this](QObject* p) -> M1::IModule* {
        if (!m_video) {
            m_video = new M1::VideoModule(p);
        }
        return m_video;
    };
    m_moduleFactory["com.mcaster1.monitor"] = [this](QObject* p) -> M1::IModule* {
        if (!m_monitor) {
            m_monitor = new M1::MonitorModule(p);
        }
        return m_monitor;
    };
    // Clock module: each surface creates its own instance (independent timezone per surface)
    m_moduleFactory["com.mcaster1.clock"] = [](QObject* p) -> M1::IModule* {
        return new M1::ClockModule(p);
    };
    m_moduleFactory["com.mcaster1.queue"] = [this](QObject* p) -> M1::IModule* {
        if (!m_queue) {
            m_queue = new QueueModule(p);
            // Connect to a single DeckPlayer (combined DeckModule's deck A)
            if (m_deck)
                m_queue->connectDeck(m_deck->deckA());
            else if (m_deckA)
                m_queue->connectDeck(m_deckA->player());
        }
        return m_queue;
    };
    m_moduleFactory["com.mcaster1.cartwall"] = [this](QObject* p) -> M1::IModule* {
        if (!m_cartwall) {
            m_cartwall = new M1::CartWallModule(p);
        }
        return m_cartwall;
    };
    m_moduleFactory["com.mcaster1.crossfader"] = [this](QObject* p) -> M1::IModule* {
        if (!m_crossfader) {
            m_crossfader = new CrossfaderModule(p);
        }
        return m_crossfader;
    };
    m_moduleFactory["com.mcaster1.health"] = [this](QObject* p) -> M1::IModule* {
        if (!m_health) {
            m_health = new M1::HealthModule(p);
            m_health->setEncoderModule(m_encoder);
            m_health->setDeckModule(m_deck);
            m_health->setDeckAModule(m_deckA);
            m_health->setDeckBModule(m_deckB);
        }
        return m_health;
    };
    // Church Surface modules — per-instance, but capture `this` for cross-wiring
    m_moduleFactory["com.mcaster1.church.timerclock"] = [](QObject* p) -> M1::IModule* {
        return new M1::TimerClockModule(p);
    };
    m_moduleFactory["com.mcaster1.church.graphics"] = [](QObject* p) -> M1::IModule* {
        return new M1::GraphicsEngineModule(p);
    };
    m_moduleFactory["com.mcaster1.church.lyrics"] = [](QObject* p) -> M1::IModule* {
        return new M1::LyricsCasterModule(p);
    };
    m_moduleFactory["com.mcaster1.church.scripture"] = [](QObject* p) -> M1::IModule* {
        return new M1::ScriptureCasterModule(p);
    };
    m_moduleFactory["com.mcaster1.church.announce"] = [](QObject* p) -> M1::IModule* {
        return new M1::AnnounceCasterModule(p);
    };
    m_moduleFactory["com.mcaster1.church.teleprompt"] = [](QObject* p) -> M1::IModule* {
        return new M1::TelePromptModule(p);
    };
    m_moduleFactory["com.mcaster1.church.mediacaster"] = [](QObject* p) -> M1::IModule* {
        return new M1::MediaCasterModule(p);
    };
    m_moduleFactory["com.mcaster1.church.stagemon"] = [](QObject* p) -> M1::IModule* {
        return new M1::StageMonModule(p);
    };
    m_moduleFactory["com.mcaster1.church.audiomix"] = [](QObject* p) -> M1::IModule* {
        return new M1::AudioMixModule(p);
    };
    m_moduleFactory["com.mcaster1.church.transcriberec"] = [](QObject* p) -> M1::IModule* {
        return new M1::TranscribeRecModule(p);
    };
    m_moduleFactory["com.mcaster1.church.switchcaster"] = [](QObject* p) -> M1::IModule* {
        return new M1::SwitchCasterModule(p);
    };
    m_moduleFactory["com.mcaster1.church.servicerunner"] = [](QObject* p) -> M1::IModule* {
        return new M1::ServiceRunnerModule(p);
    };
    // Podcast Surface modules — per-instance, no singletons
    m_moduleFactory["com.mcaster1.podcast.mixer"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodMixerModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.ptt"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodPTTModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.recorder"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodRecorderModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.soundboard"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodSoundboardModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.fx"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodFXModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.editor"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodEditorModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.encode"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodEncodeModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.transcribe"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodTranscribeModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.shownotes"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodShowNotesModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.rss"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodRSSModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.publisher"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodPublisherModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.analytics"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodAnalyticsModule(p);
    };
    m_moduleFactory["com.mcaster1.podcast.remote"] = [](QObject* p) -> M1::IModule* {
        return new M1::PodRemoteModule(p);
    };
}

// ─── Church module cross-wiring helper ──────────────────────────────────────
// Scans all modules on all surfaces and wires church module dependencies.
// Called from wireModuleConnections(). Safe to call multiple times.
void MainWindow::wireChurchModules() {
    // Collect all modules from all surfaces
    QList<M1::IModule*> allMods;
    for (int i = 0; i < m_surfaceBar->count(); ++i) {
        auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(i));
        if (sw) allMods.append(sw->modules());
    }

    // Find church modules by qobject_cast
    M1::GraphicsEngineModule*  graphics    = nullptr;
    M1::LyricsCasterModule*    lyrics      = nullptr;
    M1::ScriptureCasterModule* scripture   = nullptr;
    M1::AnnounceCasterModule*  announce    = nullptr;
    M1::MediaCasterModule*     mediaCaster = nullptr;
    M1::TimerClockModule*      timerClock  = nullptr;
    M1::SwitchCasterModule*    switcher    = nullptr;
    M1::ServiceRunnerModule*   runner      = nullptr;
    M1::StageMonModule*        stageMon    = nullptr;
    M1::TelePromptModule*      telePrompt  = nullptr;
    M1::TranscribeRecModule*   transcribe  = nullptr;
    M1::AudioMixModule*        audioMix    = nullptr;

    for (auto* mod : allMods) {
        if (!graphics)    graphics    = qobject_cast<M1::GraphicsEngineModule*>(mod);
        if (!lyrics)      lyrics      = qobject_cast<M1::LyricsCasterModule*>(mod);
        if (!scripture)   scripture   = qobject_cast<M1::ScriptureCasterModule*>(mod);
        if (!announce)    announce    = qobject_cast<M1::AnnounceCasterModule*>(mod);
        if (!mediaCaster) mediaCaster = qobject_cast<M1::MediaCasterModule*>(mod);
        if (!timerClock)  timerClock  = qobject_cast<M1::TimerClockModule*>(mod);
        if (!switcher)    switcher    = qobject_cast<M1::SwitchCasterModule*>(mod);
        if (!runner)      runner      = qobject_cast<M1::ServiceRunnerModule*>(mod);
        if (!stageMon)    stageMon    = qobject_cast<M1::StageMonModule*>(mod);
        if (!telePrompt)  telePrompt  = qobject_cast<M1::TelePromptModule*>(mod);
        if (!transcribe)  transcribe  = qobject_cast<M1::TranscribeRecModule*>(mod);
        if (!audioMix)    audioMix    = qobject_cast<M1::AudioMixModule*>(mod);
    }

    // ── Graphics Engine → visual modules ─────────────────────────────────
    if (graphics) {
        if (lyrics)      lyrics->setGraphicsEngine(graphics);
        if (scripture)   scripture->setGraphicsEngine(graphics);
        if (announce)    announce->setGraphicsEngine(graphics);
        if (mediaCaster) mediaCaster->setGraphicsEngine(graphics);
    }

    // ── SwitchCaster → all visual source modules ─────────────────────────
    if (switcher) {
        if (graphics)    switcher->setGraphicsEngine(graphics);
        if (lyrics)      switcher->setLyricsCaster(lyrics);
        if (scripture)   switcher->setScriptureCaster(scripture);
        if (announce)    switcher->setAnnounceCaster(announce);
        if (mediaCaster) switcher->setMediaCaster(mediaCaster);
        if (m_video)     switcher->setVideoModule(m_video);
    }

    // ── StageMonitor → content modules ───────────────────────────────────
    if (stageMon) {
        if (graphics)   stageMon->setGraphicsEngine(graphics);
        if (lyrics)     stageMon->setLyricsCaster(lyrics);
        if (scripture)  stageMon->setScriptureCaster(scripture);
        if (timerClock) stageMon->setTimerClock(timerClock);
    }

    // ── TelePrompter → graphics engine ──────────────────────────────────
    if (telePrompt && graphics) {
        telePrompt->setGraphicsEngine(graphics);
    }

    // ── ServiceRunner → coordination modules ─────────────────────────────
    if (runner) {
        if (timerClock) runner->setTimerClock(timerClock);
        if (switcher)   runner->setSwitchCaster(switcher);
        if (transcribe) runner->setTranscribeRec(transcribe);
        if (audioMix)   runner->setAudioMix(audioMix);
    }

    // ── TranscribeRec → PTT mic binding ──────────────────────────────────
    if (transcribe && m_ptt) {
        transcribe->setPTTModule(m_ptt);
    }

    // ── AudioMix + TranscribeRec into audio callback chain ───────────────
    // Church modules that need onAudioBlock() are appended after existing chain
    // (handled by the audio callback lambda in wireModuleConnections above)

    int churchCount = 0;
    if (graphics)    churchCount++;
    if (lyrics)      churchCount++;
    if (scripture)   churchCount++;
    if (announce)    churchCount++;
    if (switcher)    churchCount++;
    if (runner)      churchCount++;
    if (churchCount > 0)
        qInfo() << "[MainWindow] Church modules wired:" << churchCount << "cross-connections";
}

M1::IModule* MainWindow::createModule(const QString& moduleId, QObject* parent) {
    auto it = m_moduleFactory.find(moduleId);
    if (it != m_moduleFactory.end())
        return it.value()(parent);

    // Fall back to plugin registry for discovered third-party modules
    auto* mod = M1::createPluginModule(moduleId);
    if (mod) {
        mod->setParent(parent);
        return mod;
    }

    qWarning() << "[MainWindow] Unknown module id:" << moduleId;
    return nullptr;
}

// ─── Surface load / save ──────────────────────────────────────────────────────
SurfaceWidget* MainWindow::openSurfaceFromConfig(const M1::SurfaceConfig& cfg) {
    // Find the surface widget that matches this type (most recently added)
    SurfaceWidget* sw = nullptr;
    for (int i = 0; i < m_surfaceBar->count(); ++i) {
        auto* candidate = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(i));
        if (candidate && M1::surfaceTypeString(candidate->surfaceType()) == cfg.surfaceType) {
            sw = candidate;
        }
    }
    if (!sw) {
        qWarning() << "[MainWindow] No matching surface tab for type:" << cfg.surfaceType;
        return nullptr;
    }

    // First-run / YAML-config path: load modules from the flat config (no session data)
    for (const M1::ModuleConfig& mc : cfg.modules) {
        if (!mc.enabled) continue;
        M1::IModule* mod = createModule(mc.id, this);
        if (!mod) continue;
        sw->addModule(mod);
    }

    wireModuleConnections();
    connectSurfaceAutoSave(sw);
    return sw;
}

void MainWindow::wireModuleConnections() {
    // Audio callback chain: deck(s) → PTT (mic mix) → effects → podcast (record) → encoder
    // If split decks (m_deckA/m_deckB) are used, the caller silences before accumulating each.
    m_audio->setAudioCallback([this](M1::AudioBuffer& in, M1::AudioBuffer& out) {
        // Combined deck: silences output internally, then mixes A+B with crossfader
        if (m_deck) {
            m_deck->onAudioBlock(in, out);
        }
        // Standalone decks: additive mix (no internal silence — they just +=)
        // Process ALWAYS, even when combined deck exists, so all loaded decks produce sound
        if (m_deckA) m_deckA->onAudioBlock(in, out);
        if (m_deckB) m_deckB->onAudioBlock(in, out);
        if (m_ptt)      m_ptt->onAudioBlock(in, out);
        if (m_cartwall) m_cartwall->onAudioBlock(in, out);
        if (m_effects)  m_effects->onAudioBlock(in, out);
        if (m_podcast) m_podcast->onAudioBlock(in, out);
        if (m_encoder) m_encoder->onAudioBlock(in, out);

        // ── Feed CUE ring buffer for headphone monitor output ─────────
        if (m_deck && m_deck->cueMixFrames() > 0) {
            m_audio->writeCue(m_deck->cueMix(), m_deck->cueMixFrames());
        }
        if (m_deckA && m_deckA->cueMixFrames() > 0) {
            m_audio->writeCue(m_deckA->cueMix(), m_deckA->cueMixFrames());
        }
        if (m_deckB && m_deckB->cueMixFrames() > 0) {
            m_audio->writeCue(m_deckB->cueMix(), m_deckB->cueMixFrames());
        }
    });

    // ── Combined DeckModule wiring ─────────────────────────────────────────────
    if (m_library && m_deck) {
        disconnect(m_library, &M1::IModule::requestLoadMedia, nullptr, nullptr);
        connect(m_library, &M1::IModule::requestLoadMedia,
                this, [this](const M1::MediaItem& item, int deckIndex) {
            // If PlaylistModule exists, route to playlist queue instead of direct deck load
            if (m_playlist) {
                m_playlist->addItem(item);
                return;
            }
            auto* player = (deckIndex == 0) ? m_deck->deckA() : m_deck->deckB();
            if (player) player->loadFile(item.filePath);
        });
    }
    // ── Playlist/AutoDJ → deck wiring ─────────────────────────────────────────
    // Prefer standalone decks over combined DeckModule (same priority as Queue wiring)
    if (m_playlist && m_deckA && m_deckB) {
        qInfo() << "[MainWindow] Wiring PlaylistModule to STANDALONE decks A+B";
        // requestLoadMedia → standalone players
        disconnect(m_playlist, &M1::IModule::requestLoadMedia, nullptr, nullptr);
        connect(m_playlist, &M1::IModule::requestLoadMedia,
                this, [this](const M1::MediaItem& item, int deckIndex) {
            if (deckIndex == 1 && m_deckB) m_deckB->player()->loadFile(item.filePath);
            else if (m_deckA)              m_deckA->player()->loadFile(item.filePath);
        });
        // Connect AutoDJ poll timer to standalone players
        m_playlist->connectDecks(m_deckA->player(), m_deckB->player(), nullptr);
        // Crossfader animation (only if combined deck also exists)
        if (m_deck) {
            disconnect(m_playlist, &M1::PlaylistModule::crossfaderAnimateTo, nullptr, nullptr);
            connect(m_playlist, &M1::PlaylistModule::crossfaderAnimateTo,
                    m_deck, &M1::DeckModule::animateCrossfaderTo);
        }
        // Finished signal fallback for standalone decks
        disconnect(m_deckA->player(), &M1::DeckPlayer::finished, this, nullptr);
        connect(m_deckA->player(), &M1::DeckPlayer::finished, this, [this]() {
            if (m_playlist && m_playlist->autoDJ() && !m_playlist->isFading())
                m_playlist->advance();
        });
        disconnect(m_deckB->player(), &M1::DeckPlayer::finished, this, nullptr);
        connect(m_deckB->player(), &M1::DeckPlayer::finished, this, [this]() {
            if (m_playlist && m_playlist->autoDJ() && !m_playlist->isFading())
                m_playlist->advance();
        });
        // Metadata from standalone decks — wire BOTH Deck A and Deck B
        auto buildMetaFromDeck = [](M1::DeckPlayer* p) -> M1::IcyMetadata {
            M1::IcyMetadata meta;
            meta.trackArtist = p->tagArtist();
            meta.trackTitle  = p->tagTitle();
            meta.trackAlbum  = p->tagAlbum();
            meta.trackGenre  = p->tagGenre();
            meta.trackYear   = p->tagYear();
            if (p->bpm() > 0.0f)
                meta.trackBpm = QString::number(static_cast<int>(p->bpm()));
            // Build ICY 1.x StreamTitle
            if (!meta.trackArtist.isEmpty() && !meta.trackTitle.isEmpty())
                meta.streamTitle = meta.trackArtist + " - " + meta.trackTitle;
            else if (!meta.trackTitle.isEmpty())
                meta.streamTitle = meta.trackTitle;
            else {
                const QFileInfo fi(p->loadedPath());
                meta.streamTitle = fi.completeBaseName();
            }
            return meta;
        };

        // Push metadata when a deck starts PLAYING (not on loadingFinished).
        // Preloaded tracks fire loadingFinished but aren't playing yet —
        // that would overwrite the current track's metadata at the server.
        auto* playerA = m_deckA->player();
        disconnect(playerA, &M1::DeckPlayer::stateChanged, this, nullptr);
        connect(playerA, &M1::DeckPlayer::stateChanged, this,
                [this, playerA, buildMetaFromDeck](M1::DeckPlayer::State st) {
            if (st != M1::DeckPlayer::State::Playing) return;
            const auto meta = buildMetaFromDeck(playerA);
            updateNowPlayingStatus(meta.streamTitle);
            if (m_metadata)
                m_metadata->onMetadataUpdate(meta);
        });
        if (m_deckB) {
            auto* playerB = m_deckB->player();
            disconnect(playerB, &M1::DeckPlayer::stateChanged, this, nullptr);
            connect(playerB, &M1::DeckPlayer::stateChanged, this,
                    [this, playerB, buildMetaFromDeck](M1::DeckPlayer::State st) {
                if (st != M1::DeckPlayer::State::Playing) return;
                const auto meta = buildMetaFromDeck(playerB);
                updateNowPlayingStatus(meta.streamTitle);
                if (m_metadata)
                    m_metadata->onMetadataUpdate(meta);
            });
        }
    } else if (m_playlist && m_deck) {
        qInfo() << "[MainWindow] Wiring PlaylistModule to COMBINED DeckModule";
        disconnect(m_playlist, &M1::IModule::requestLoadMedia, nullptr, nullptr);
        connect(m_playlist, &M1::IModule::requestLoadMedia,
                this, [this](const M1::MediaItem& item, int deckIndex) {
            auto* player = (deckIndex == 1) ? m_deck->deckB() : m_deck->deckA();
            if (player) player->loadFile(item.filePath);
        });
        disconnect(m_playlist, &M1::PlaylistModule::crossfaderAnimateTo, nullptr, nullptr);
        connect(m_playlist, &M1::PlaylistModule::crossfaderAnimateTo,
                m_deck, &M1::DeckModule::animateCrossfaderTo);
        m_playlist->connectDecks(m_deck->deckA(), m_deck->deckB(), m_deck);
        // Finished signal fallback for combined deck
        auto* deckA = m_deck->deckA();
        auto* deckB = m_deck->deckB();
        if (deckA) {
            disconnect(deckA, &M1::DeckPlayer::finished, this, nullptr);
            connect(deckA, &M1::DeckPlayer::finished, this, [this]() {
                if (m_playlist && m_playlist->autoDJ() && !m_playlist->isFading())
                    m_playlist->advance();
            });
        }
        if (deckB) {
            disconnect(deckB, &M1::DeckPlayer::finished, this, nullptr);
            connect(deckB, &M1::DeckPlayer::finished, this, [this]() {
                if (m_playlist && m_playlist->autoDJ() && !m_playlist->isFading())
                    m_playlist->advance();
            });
        }
        // Metadata from combined decks — wire BOTH Deck A and Deck B
        auto buildMetaFromDeck = [](M1::DeckPlayer* p) -> M1::IcyMetadata {
            M1::IcyMetadata meta;
            meta.trackArtist = p->tagArtist();
            meta.trackTitle  = p->tagTitle();
            meta.trackAlbum  = p->tagAlbum();
            meta.trackGenre  = p->tagGenre();
            meta.trackYear   = p->tagYear();
            if (p->bpm() > 0.0f)
                meta.trackBpm = QString::number(static_cast<int>(p->bpm()));
            if (!meta.trackArtist.isEmpty() && !meta.trackTitle.isEmpty())
                meta.streamTitle = meta.trackArtist + " - " + meta.trackTitle;
            else if (!meta.trackTitle.isEmpty())
                meta.streamTitle = meta.trackTitle;
            else {
                const QFileInfo fi(p->loadedPath());
                meta.streamTitle = fi.completeBaseName();
            }
            return meta;
        };

        if (deckA) {
            disconnect(deckA, &M1::DeckPlayer::stateChanged, this, nullptr);
            connect(deckA, &M1::DeckPlayer::stateChanged, this,
                    [this, deckA, buildMetaFromDeck](M1::DeckPlayer::State st) {
                if (st != M1::DeckPlayer::State::Playing) return;
                const auto meta = buildMetaFromDeck(deckA);
                updateNowPlayingStatus(meta.streamTitle);
                if (m_metadata)
                    m_metadata->onMetadataUpdate(meta);
            });
        }
        if (deckB) {
            disconnect(deckB, &M1::DeckPlayer::stateChanged, this, nullptr);
            connect(deckB, &M1::DeckPlayer::stateChanged, this,
                    [this, deckB, buildMetaFromDeck](M1::DeckPlayer::State st) {
                if (st != M1::DeckPlayer::State::Playing) return;
                const auto meta = buildMetaFromDeck(deckB);
                updateNowPlayingStatus(meta.streamTitle);
                if (m_metadata)
                    m_metadata->onMetadataUpdate(meta);
            });
        }
    }

    // ── Library → deck loading (standalone decks) ──────────────────────────
    if (m_library && m_deckA) {
        disconnect(m_library, &M1::IModule::requestLoadMedia, nullptr, nullptr);
        connect(m_library, &M1::IModule::requestLoadMedia,
                this, [this](const M1::MediaItem& item, int deckIndex) {
            // If PlaylistModule exists, route to playlist queue
            if (m_playlist) {
                m_playlist->addItem(item);
                return;
            }
            if (deckIndex == 0 && m_deckA) m_deckA->player()->loadFile(item.filePath);
            if (deckIndex == 1 && m_deckB) m_deckB->player()->loadFile(item.filePath);
        });
    }

    // ── Playlist ← Library model (AutoDJ track selection) ─────────────────────
    if (m_playlist && m_library && m_library->model()) {
        m_playlist->connectLibrary(m_library->model());
    }

    // ── Queue → single deck wiring ─────────────────────────────────────────────
    // Queue is a simple file playlist — wire to one DeckPlayer only.
    if (m_queue) {
        if (m_deck) {
            qInfo() << "[MainWindow] Wiring QueueModule to COMBINED DeckModule (Deck A)";
            m_queue->connectDeck(m_deck->deckA());
        } else if (m_deckA) {
            qInfo() << "[MainWindow] Wiring QueueModule to STANDALONE Deck A";
            m_queue->connectDeck(m_deckA->player());
        }
        // Wire requestLoadMedia for playNow() routing
        disconnect(m_queue, &M1::IModule::requestLoadMedia, nullptr, nullptr);
        connect(m_queue, &M1::IModule::requestLoadMedia,
                this, [this](const M1::MediaItem& item, int) {
            if (m_deck && m_deck->deckA())
                m_deck->deckA()->loadFile(item.filePath);
            else if (m_deckA)
                m_deckA->player()->loadFile(item.filePath);
        });
    }

    // ── Crossfader module → standalone decks ────────────────────────────────
    if (m_crossfader && m_deckA && m_deckB) {
        m_crossfader->connectDecks(m_deckA, m_deckB);
    }
    // Wire PlaylistModule crossfader animation to CrossfaderModule
    // (prefer CrossfaderModule over combined DeckModule crossfader when both exist)
    if (m_crossfader) {
        if (m_playlist) {
            disconnect(m_playlist, &M1::PlaylistModule::crossfaderAnimateTo, nullptr, nullptr);
            connect(m_playlist, &M1::PlaylistModule::crossfaderAnimateTo,
                    m_crossfader, &CrossfaderModule::animateTo);
            // AUTO CROSSFADE complete → advance playlist queue + preload next
            disconnect(m_crossfader, &CrossfaderModule::autoFadeCompleted, nullptr, nullptr);
            connect(m_crossfader, &CrossfaderModule::autoFadeCompleted,
                    m_playlist, &M1::PlaylistModule::onAutoFadeCompleted);
        }
    }

    // ── PTT strip embedded in DeckWidget ─────────────────────────────────────
    if (m_deck && m_deck->deckWidget() && m_ptt) {
        m_deck->deckWidget()->setPTTModule(m_ptt);
    }

    // ── Deck empty-state loading requests ────────────────────────────────────
    if (auto* dw = m_deck ? m_deck->deckWidget() : nullptr) {
        disconnect(dw, &DeckWidget::loadNextFromQueueRequested, nullptr, nullptr);
        connect(dw, &DeckWidget::loadNextFromQueueRequested,
                this, [this](int deckIndex) {
            if (m_queue)
                m_queue->loadNextOnDeck(deckIndex);
            else
                statusBar()->showMessage(
                    "Add a Queue/AutoDJ module to load tracks from the queue.", 4000);
        });

        disconnect(dw, &DeckWidget::loadFromLibraryRequested, nullptr, nullptr);
        connect(dw, &DeckWidget::loadFromLibraryRequested,
                this, [this](int deckIndex) {
            const QString msg = m_library
                ? QString("Drag a track from the Media Library onto Deck %1, "
                          "or double-click a library track.").arg(deckIndex == 0 ? "A" : "B")
                : "Add a Media Library module to browse your music collection.";
            statusBar()->showMessage(msg, 5000);
        });

        disconnect(dw, &DeckWidget::addPlaylistToQueueRequested, nullptr, nullptr);
        connect(dw, &DeckWidget::addPlaylistToQueueRequested,
                this, [this](const QString& path) {
            if (m_queue)
                m_queue->addPlaylistFile(path);
            else
                statusBar()->showMessage(
                    "Add a Queue/AutoDJ module to use playlist files.", 4000);
        });
    }

    // ── Deck browser tabs — populate from library/queue ──────────────────────
    if (auto* dw = m_deck ? m_deck->deckWidget() : nullptr) {
        // Ensure library module exists (it's a singleton — creates DB + model on first call)
        // even if the user hasn't added it to the surface. Decks always need library data.
        if (!m_library) {
            m_library = new M1::MediaLibraryModule(this);
            m_library->initialize();
        }

        // Helper: refresh all library items from model → deck browser Library tab
        auto refreshLibrary = [this, dw]() {
            if (!m_library || !m_library->model()) return;
            const int n = m_library->model()->itemCount();
            QList<M1::MediaItem> libItems;
            libItems.reserve(n);
            for (int i = 0; i < n; ++i)
                libItems.append(m_library->model()->itemAt(i));
            dw->setLibraryItems(libItems);
        };

        // Initial population
        refreshLibrary();

        // Reactive: refresh when library model changes (scan adds items, etc.)
        if (auto* model = m_library->model()) {
            disconnect(model, &QAbstractItemModel::rowsInserted, dw, nullptr);
            disconnect(model, &QAbstractItemModel::modelReset, dw, nullptr);
            connect(model, &QAbstractItemModel::rowsInserted,
                    dw, refreshLibrary);
            connect(model, &QAbstractItemModel::modelReset,
                    dw, refreshLibrary);
        }

        // Queue items → deck browser Queue tab
        // Prefer PlaylistModule queue (AutoDJ) over QueueModule if both exist
        if (m_playlist) {
            dw->setQueueItems(m_playlist->queue());
            disconnect(m_playlist, &M1::PlaylistModule::queueChanged, dw, nullptr);
            connect(m_playlist, &M1::PlaylistModule::queueChanged, dw, [this, dw]() {
                dw->setQueueItems(m_playlist->queue());
            });
        } else if (m_queue) {
            dw->setQueueItems(m_queue->queue());
            disconnect(m_queue, &QueueModule::queueChanged, dw, nullptr);
            connect(m_queue, &QueueModule::queueChanged, dw, [this, dw]() {
                dw->setQueueItems(m_queue->queue());
            });
        }

        // Context menu: load to deck
        disconnect(dw, &DeckWidget::loadToDeckRequested, nullptr, nullptr);
        connect(dw, &DeckWidget::loadToDeckRequested,
                this, [this](const QString& filePath, int targetDeck) {
            if (m_deck) {
                auto* player = (targetDeck == 0) ? m_deck->deckA() : m_deck->deckB();
                if (player) player->loadFile(filePath);
            }
        });

        // Context menu: add to queue
        disconnect(dw, &DeckWidget::addToQueueRequested, nullptr, nullptr);
        connect(dw, &DeckWidget::addToQueueRequested,
                this, [this](const M1::MediaItem& item) {
            if (m_queue)
                m_queue->addItem(item);
            else
                statusBar()->showMessage(
                    "Add a Queue/AutoDJ module to use the queue.", 4000);
        });

        // Context menu: edit tags (show status for now — tag editor can be wired later)
        disconnect(dw, &DeckWidget::editTagsRequested, nullptr, nullptr);
        connect(dw, &DeckWidget::editTagsRequested,
                this, [this](const M1::MediaItem& item) {
            statusBar()->showMessage(
                QString("Edit tags: %1 — %2").arg(item.artist, item.title), 4000);
        });
    }

    // ── Split deck (DeckA / DeckB) browser tabs ──────────────────────────────
    {
        // Collect all standalone DeckPanel pointers from split deck modules
        QList<DeckPanel*> splitPanels;
        if (m_deckA && m_deckA->panel()) splitPanels.append(m_deckA->panel());
        if (m_deckB && m_deckB->panel()) splitPanels.append(m_deckB->panel());

        if (!splitPanels.isEmpty()) {
            // Ensure library module exists for split decks too
            if (!m_library) {
                m_library = new M1::MediaLibraryModule(this);
                m_library->initialize();
            }

            auto refreshSplitLibrary = [this, splitPanels]() {
                if (!m_library || !m_library->model()) return;
                const int n = m_library->model()->itemCount();
                QList<M1::MediaItem> libItems;
                libItems.reserve(n);
                for (int i = 0; i < n; ++i)
                    libItems.append(m_library->model()->itemAt(i));
                for (auto* panel : splitPanels)
                    panel->setLibraryItems(libItems);
            };

            refreshSplitLibrary();

            if (auto* model = m_library->model()) {
                for (auto* panel : splitPanels) {
                    disconnect(model, &QAbstractItemModel::rowsInserted, panel, nullptr);
                    disconnect(model, &QAbstractItemModel::modelReset, panel, nullptr);
                }
                for (auto* panel : splitPanels) {
                    connect(model, &QAbstractItemModel::rowsInserted,
                            panel, refreshSplitLibrary);
                    connect(model, &QAbstractItemModel::modelReset,
                            panel, refreshSplitLibrary);
                }
            }

            // Queue → split deck panels
            // Prefer PlaylistModule queue (AutoDJ) over QueueModule
            if (m_playlist) {
                const auto& q = m_playlist->queue();
                for (auto* panel : splitPanels)
                    panel->setQueueItems(q);
                for (auto* panel : splitPanels) {
                    disconnect(m_playlist, &M1::PlaylistModule::queueChanged, panel, nullptr);
                    connect(m_playlist, &M1::PlaylistModule::queueChanged, panel,
                            [this, panel]() { panel->setQueueItems(m_playlist->queue()); });
                }
            } else if (m_queue) {
                const auto& q = m_queue->queue();
                for (auto* panel : splitPanels)
                    panel->setQueueItems(q);
                for (auto* panel : splitPanels) {
                    disconnect(m_queue, &QueueModule::queueChanged, panel, nullptr);
                    connect(m_queue, &QueueModule::queueChanged, panel,
                            [this, panel]() { panel->setQueueItems(m_queue->queue()); });
                }
            }

            // Wire interactive signals for standalone DeckPanels
            for (auto* panel : splitPanels) {
                // Load next from queue
                disconnect(panel, &DeckPanel::loadNextFromQueueRequested, nullptr, nullptr);
                connect(panel, &DeckPanel::loadNextFromQueueRequested,
                        this, [this](int deckIndex) {
                    if (m_queue)
                        m_queue->loadNextOnDeck(deckIndex);
                    else
                        statusBar()->showMessage(
                            "Add a Queue/AutoDJ module to load tracks from the queue.", 4000);
                });

                // Load from library
                disconnect(panel, &DeckPanel::loadFromLibraryRequested, nullptr, nullptr);
                connect(panel, &DeckPanel::loadFromLibraryRequested,
                        this, [this](int deckIndex) {
                    const QString msg = m_library
                        ? QString("Drag a track from the Media Library onto Deck %1, "
                                  "or double-click a library track.").arg(deckIndex == 0 ? "A" : "B")
                        : "Add a Media Library module to browse your music collection.";
                    statusBar()->showMessage(msg, 5000);
                });

                // Add playlist to queue
                disconnect(panel, &DeckPanel::addPlaylistToQueueRequested, nullptr, nullptr);
                connect(panel, &DeckPanel::addPlaylistToQueueRequested,
                        this, [this](const QString& path) {
                    if (m_queue)
                        m_queue->addPlaylistFile(path);
                    else
                        statusBar()->showMessage(
                            "Add a Queue/AutoDJ module to use playlist files.", 4000);
                });

                // Load to deck (from browser context menu)
                disconnect(panel, &DeckPanel::loadToDeckRequested, nullptr, nullptr);
                connect(panel, &DeckPanel::loadToDeckRequested,
                        this, [this](const QString& filePath, int targetDeck) {
                    if (targetDeck == 0 && m_deckA)
                        m_deckA->player()->loadFile(filePath);
                    else if (targetDeck == 1 && m_deckB)
                        m_deckB->player()->loadFile(filePath);
                    else if (m_deck) {
                        auto* player = (targetDeck == 0) ? m_deck->deckA() : m_deck->deckB();
                        if (player) player->loadFile(filePath);
                    }
                });

                // Add to queue (from browser context menu)
                disconnect(panel, &DeckPanel::addToQueueRequested, nullptr, nullptr);
                connect(panel, &DeckPanel::addToQueueRequested,
                        this, [this](const M1::MediaItem& item) {
                    if (m_queue)
                        m_queue->addItem(item);
                    else
                        statusBar()->showMessage(
                            "Add a Queue/AutoDJ module to use the queue.", 4000);
                });

                // Edit tags
                disconnect(panel, &DeckPanel::editTagsRequested, nullptr, nullptr);
                connect(panel, &DeckPanel::editTagsRequested,
                        this, [this](const M1::MediaItem& item) {
                    statusBar()->showMessage(
                        QString("Edit tags: %1 — %2").arg(item.artist, item.title), 4000);
                });
            }
        }
    }

    // ── Metadata → Encoder (ICY 1.x + ICY 2.2) ──────────────────────────────────
    // When MetadataModule receives a track update, forward it to the EncoderModule
    // so all active encoder slots push ICY metadata to their streaming servers.
    if (m_metadata && m_encoder) {
        disconnect(m_metadata, &M1::MetadataModule::metadataChanged, m_encoder, nullptr);
        connect(m_metadata, &M1::MetadataModule::metadataChanged,
                m_encoder, &M1::EncoderModule::onMetadataUpdate);
    }

    // ── Direct Deck → Encoder metadata (fallback when MetadataModule not on surface) ──
    // MetadataModule is lazy-created — if not placed on a surface, m_metadata is null
    // and the pipeline above is disconnected. Wire deck → encoder directly as backup.
    // Note: Qt::UniqueConnection requires member function pointers — cannot use with
    // lambdas. Use tagged disconnect/connect pattern instead.
    // ── Direct Deck → Encoder metadata (fallback when MetadataModule not on surface) ──
    // Connect on stateChanged→Playing (NOT loadingFinished) to avoid pushing
    // metadata for preloaded tracks that haven't started playing yet.
    if (m_encoder) {
        auto directMetaToEncoder = [this](M1::DeckPlayer* p) {
            M1::IcyMetadata meta;
            meta.trackArtist = p->tagArtist();
            meta.trackTitle  = p->tagTitle();
            meta.trackAlbum  = p->tagAlbum();
            meta.trackGenre  = p->tagGenre();
            meta.trackYear   = p->tagYear();
            if (p->bpm() > 0.0f)
                meta.trackBpm = QString::number(static_cast<int>(p->bpm()));
            if (!meta.trackArtist.isEmpty() && !meta.trackTitle.isEmpty())
                meta.streamTitle = meta.trackArtist + " - " + meta.trackTitle;
            else if (!meta.trackTitle.isEmpty())
                meta.streamTitle = meta.trackTitle;
            else {
                const QFileInfo fi(p->loadedPath());
                meta.streamTitle = fi.completeBaseName();
            }
            qInfo() << "[MainWindow] Direct Deck→Encoder metadata:"
                    << meta.streamTitle;
            m_encoder->onMetadataUpdate(meta);
        };

        auto wirePlayerToEncoder = [this, directMetaToEncoder](M1::DeckPlayer* p) {
            disconnect(p, &M1::DeckPlayer::stateChanged, m_encoder, nullptr);
            connect(p, &M1::DeckPlayer::stateChanged, m_encoder,
                    [this, p, directMetaToEncoder](M1::DeckPlayer::State st) {
                        if (st == M1::DeckPlayer::State::Playing)
                            directMetaToEncoder(p);
                    });
        };

        // Wire standalone decks
        if (m_deckA) wirePlayerToEncoder(m_deckA->player());
        if (m_deckB) wirePlayerToEncoder(m_deckB->player());
        // Wire combined deck
        if (m_deck) {
            if (auto* a = m_deck->deckA()) wirePlayerToEncoder(a);
            if (auto* b = m_deck->deckB()) wirePlayerToEncoder(b);
        }
    }

    // ── PTT → Encoder DSP duck ────────────────────────────────────────────────
    // Propagate PTT active state to encoder slots that have pttDuckEnabled = true.
    if (m_ptt && m_encoder) {
        connect(m_ptt, &M1::PTTModule::stateChanged,
                m_encoder, [this](M1::PTTModule::State st) {
                    m_encoder->setPttActive(st == M1::PTTModule::State::Live);
                });
    }

    // ── Church module cross-wiring ───────────────────────────────────────────
    wireChurchModules();

    // ── Status indicators ─────────────────────────────────────────────────────
    wireStatusIndicators();
}

void MainWindow::wireStatusIndicators() {
    // ── Encoder LIVE → ON-AIR auto-toggle on sub-surface tabs + app ribbon ────
    if (m_encoder) {
        disconnect(m_encoder, &M1::EncoderModule::encoderLiveChanged, this, nullptr);
        connect(m_encoder, &M1::EncoderModule::encoderLiveChanged,
                this, [this](bool anyLive) {
            // Toggle app ribbon ON-AIR
            m_appRibbon->setOnAir(anyLive);

            // Find sub-surface tabs that have encoder docked → set ON-AIR or revert
            for (int si = 0; si < m_surfaceBar->count(); ++si) {
                auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(si));
                if (!sw) continue;
                const auto encPanels = sw->panelIndicesWithModule("com.mcaster1.encoder");
                for (int pi : encPanels) {
                    const auto mode = anyLive
                        ? OnAirButton::StatusMode::OnAir
                        : OnAirButton::StatusMode::OffAir;
                    sw->subTabBar()->setTabStatusMode(pi, mode);
                }
            }
        });
    }

    // ── Playlist AutoDJ → AUTO-DJ / IDLE on sub-surface tabs ──────────────────
    // Also handles auto-save trigger (consolidated here to avoid disconnect conflicts
    // with connectSurfaceAutoSave's autoDJChanged wiring).
    if (m_playlist) {
        disconnect(m_playlist, &M1::PlaylistModule::autoDJChanged, this, nullptr);
        connect(m_playlist, &M1::PlaylistModule::autoDJChanged,
                this, [this](bool active) {
            // Update status indicators on tabs with playlist
            for (int si = 0; si < m_surfaceBar->count(); ++si) {
                auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(si));
                if (!sw) continue;
                const auto plPanels = sw->panelIndicesWithModule("com.mcaster1.playlist");
                for (int pi : plPanels) {
                    // Don't override ON-AIR if encoder is also streaming on this panel
                    auto* chip = sw->subTabBar()->chip(pi);
                    if (chip && chip->statusMode() == OnAirButton::StatusMode::OnAir)
                        continue;

                    const auto mode = active
                        ? OnAirButton::StatusMode::AutoDJ
                        : OnAirButton::StatusMode::Idle;
                    sw->subTabBar()->setTabStatusMode(pi, mode);
                }
            }
            // Also trigger auto-save
            scheduleAutoSave();
        });
    }
}

void MainWindow::addModuleToCurrentSurface(const QString& moduleId) {
    auto* sw = m_surfaceBar->currentSurface();
    if (!sw) return;
    M1::IModule* mod = createModule(moduleId, this);
    if (!mod) return;
    sw->addModule(mod, /*startFloating=*/true);
    wireModuleConnections();
}

// ─── Session persistence — v2 stable index-based keys ─────────────────────────

void MainWindow::saveAllSessionState() {
    QSettings s("Mcaster1", "Mcaster1Studio");

    const int count = m_surfaceBar->count();
    s.setValue("session/v2/count", count);

    for (int si = 0; si < count; ++si) {
        auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(si));
        if (!sw) continue;

        const QString sbase = QString("session/v2/%1").arg(si);
        s.setValue(sbase + "/type",          M1::surfaceTypeString(sw->surfaceType()));
        s.setValue(sbase + "/customName",    sw->customName());
        s.setValue(sbase + "/tabLabel",      m_surfaceBar->tabText(si));
        s.setValue(sbase + "/subPanelCount", sw->panelCount());

        for (int pi = 0; pi < sw->panelCount(); ++pi) {
            auto* panel = sw->panel(pi);
            if (!panel) continue;

            const QString pbase = sbase + QString("/sub/%1").arg(pi);
            s.setValue(pbase + "/name",  panel->name());
            if (auto* chip = sw->subTabBar()->chip(pi))
                s.setValue(pbase + "/color", chip->color().name());

            QStringList ids;
            for (auto* mod : panel->modules())
                ids << mod->moduleId();
            s.setValue(pbase + "/moduleIds", ids);

            // Save each module's own state under its panel prefix
            for (int mi = 0; mi < panel->modules().size(); ++mi) {
                auto* mod = panel->modules()[mi];
                const QString mbase = pbase + QString("/mod/%1").arg(mi);
                s.beginGroup(mbase);
                mod->saveState(s);
                s.endGroup();
            }

            // Save splitter/dock layout for this sub-panel
            panel->saveLayout(s, pbase);
        }
    }

    qInfo() << "[MainWindow] Session auto-saved:" << count << "surface(s)";
}

void MainWindow::scheduleAutoSave() {
    if (m_autoSaveTimer) {
        m_autoSaveTimer->stop();
        m_autoSaveTimer->start();
    }
}

void MainWindow::connectSurfaceAutoSave(SurfaceWidget* sw) {
    // Immediate save on explicit right-click "Save Session"
    connect(sw, &SurfaceWidget::saveSettingsRequested,
            this, [this, sw]() {
        saveAllSessionState();
        statusBar()->showMessage(
            QString("Session saved: %1").arg(
                sw->customName().isEmpty() ? sw->surfaceName() : sw->customName()), 3000);
    });

    // Debounced auto-save on structural changes
    connect(sw, &SurfaceWidget::moduleLayoutChanged,
            this, &MainWindow::scheduleAutoSave);
    connect(sw->subTabBar(), &SubSurfaceTabBar::subSurfaceAdded,
            this, [this](int, const QString&, const QColor&) { scheduleAutoSave(); });
    connect(sw->subTabBar(), &SubSurfaceTabBar::subSurfaceRemoved,
            this, [this](int) { scheduleAutoSave(); });
    connect(sw->subTabBar(), &SubSurfaceTabBar::tabRenamed,
            this, [this](int, const QString&) { scheduleAutoSave(); });
    connect(sw->subTabBar(), &SubSurfaceTabBar::tabColorChanged,
            this, [this](int, const QColor&) { scheduleAutoSave(); });

    // Wire playlist queue changes to auto-save
    // NOTE: autoDJChanged auto-save is handled in wireStatusIndicators()
    // to avoid disconnect conflicts with the status indicator wiring.
    if (m_playlist) {
        disconnect(m_playlist, &M1::PlaylistModule::queueChanged,
                   this, &MainWindow::scheduleAutoSave);
        connect(m_playlist, &M1::PlaylistModule::queueChanged,
                this, &MainWindow::scheduleAutoSave);
    }

    // Sub-tab chip right-click "Add Module" → float the module on the correct sub-panel
    disconnect(sw, &SurfaceWidget::addModuleRequested, nullptr, nullptr);
    connect(sw, &SurfaceWidget::addModuleRequested,
            this, [this, sw](const QString& id) {
        M1::IModule* mod = createModule(id, this);
        if (mod) {
            sw->addModule(mod, /*startFloating=*/true);
            wireModuleConnections();
        }
    });
}

void MainWindow::saveSurface(SurfaceWidget* sw) {
    if (!sw) return;
    saveAllSessionState();
    const QString displayName = sw->customName().isEmpty() ? sw->surfaceName() : sw->customName();
    statusBar()->showMessage(QString("Session saved: %1").arg(displayName), 3000);
}

void MainWindow::onAddCustomSurface() {
    CustomSurfaceDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    const QString name       = dlg.surfaceName();
    const QStringList modIds = dlg.selectedModuleIds();
    if (name.isEmpty()) return;

    auto* sw = m_surfaceBar->openCustomSurface(name);
    if (!sw) return;

    for (const QString& id : modIds) {
        M1::IModule* mod = createModule(id, this);
        if (mod) sw->addModule(mod);
    }
    wireModuleConnections();
    connectSurfaceAutoSave(sw);
}

void MainWindow::loadSavedSurfaces() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    const int v2count = s.value("session/v2/count", 0).toInt();

    if (v2count > 0) {
        // ── v2 restore: stable index-based keys ────────────────────────────────
        // Collect deferred loadState calls — must run AFTER wireModuleConnections()
        struct DeferredLoad { M1::IModule* mod; QString mbase; };
        QList<DeferredLoad> deferredLoads;

        for (int si = 0; si < v2count; ++si) {
            const QString sbase      = QString("session/v2/%1").arg(si);
            const QString typeStr    = s.value(sbase + "/type", "alpha").toString();
            const QString customName = s.value(sbase + "/customName").toString();
            const QString tabLabel   = s.value(sbase + "/tabLabel").toString();

            const M1::SurfaceType stype = M1::surfaceTypeFromString(typeStr);
            SurfaceWidget* sw = m_surfaceBar->openSurface(stype);
            if (!sw) continue;

            // Apply custom name / tab label (stable across renames)
            const QString displayName = !tabLabel.isEmpty() ? tabLabel : customName;
            if (!displayName.isEmpty()) {
                sw->setCustomName(displayName);
                for (int t = 0; t < m_surfaceBar->count(); ++t) {
                    if (m_surfaceBar->widget(t) == sw) {
                        m_surfaceBar->setTabText(t, displayName);
                        break;
                    }
                }
            }

            // Restore sub-panels
            const int savedPanels = s.value(sbase + "/subPanelCount", 1).toInt();
            for (int pi = 0; pi < savedPanels; ++pi) {
                const QString pbase = sbase + QString("/sub/%1").arg(pi);

                if (pi == 0) {
                    // Update default panel name/color if saved
                    const QString pname = s.value(pbase + "/name").toString();
                    if (!pname.isEmpty() && sw->panel(0)) {
                        sw->subTabBar()->setTabName(0, pname);
                        sw->panel(0)->setName(pname);
                    }
                } else {
                    // Create additional sub-panels
                    const QString pname  = s.value(pbase + "/name",
                        QString("Sub-Surface %1").arg(pi + 1)).toString();
                    const QColor  pcolor(s.value(pbase + "/color", "#0ea5e9").toString());
                    sw->subTabBar()->addSubSurface(pname, pcolor);
                }

                auto* targetPanel = sw->panel(pi);
                if (!targetPanel) continue;

                const QStringList ids = s.value(pbase + "/moduleIds").toStringList();
                for (int mi = 0; mi < ids.size(); ++mi) {
                    M1::IModule* mod = createModule(ids[mi], this);
                    if (!mod) continue;
                    targetPanel->addModule(mod);

                    // Defer loadState until after wiring is complete
                    const QString mbase = pbase + QString("/mod/%1").arg(mi);
                    deferredLoads.append({mod, mbase});
                }
                targetPanel->restoreLayout(s, pbase);
            }

            sw->subTabBar()->setCurrentIndex(0);
            wireModuleConnections();
            connectSurfaceAutoSave(sw);
        }

        // ── Now restore all module states (after wiring) ─────────────────────
        for (const auto& dl : deferredLoads) {
            s.beginGroup(dl.mbase);
            dl.mod->loadState(s);
            s.endGroup();
        }
    } else {
        // ── First run / no v2 data: open Surface Alpha from YAML config ────────
        const QString alphaPath = M1::SurfaceConfig::configPath("alpha");
        M1::SurfaceConfig alphaCfg = QFile::exists(alphaPath)
            ? M1::SurfaceConfig::load(alphaPath)
            : M1::SurfaceConfig::defaultForType("alpha");
        if (!alphaCfg.isValid())
            alphaCfg = M1::SurfaceConfig::defaultForType("alpha");

        m_surfaceBar->openSurface(M1::SurfaceType::Alpha);
        openSurfaceFromConfig(alphaCfg);
    }

    // NOTE: YAML restoreAllLayouts() is NO LONGER called here.
    // All dock positions, sizes, and column structure are now fully persisted
    // in QSettings via panel->saveLayout() / restoreLayout(), which runs above
    // for each sub-panel. The YAML file is kept as a legacy backup only.

    // ── Auto-Recovery: if PlaylistModule has autoRecovery enabled, start AutoDJ ──
    // Note: loadState() may have already set m_autoDJ=true from saved state,
    // which causes setAutoDJ(true) to early-return. Force it off first so
    // setAutoDJ(true) actually runs the full startup sequence (fill queue,
    // start poll timer, load+play first track).
    if (m_playlist && m_playlist->autoDJConfig().autoRecovery) {
        m_playlist->setAutoDJ(false);  // reset so setAutoDJ(true) doesn't early-return
        QTimer::singleShot(2000, this, [this]() {
            if (!m_playlist) return;
            qInfo() << "[MainWindow] Auto-Recovery: starting AutoDJ";
            m_playlist->fillQueueFromLibrary();
            m_playlist->setAutoDJ(true);
            // Only force-play if setAutoDJ didn't already auto-start.
            // When autoStartOnEnable is true, setAutoDJ(true) already calls playNext()
            // — calling playItemAt here would be a DOUBLE playNext, creating two
            // SingleShotConnections on loadingFinished and racing the decoder.
            if (!m_playlist->autoDJConfig().autoStartOnEnable &&
                m_playlist->autoDJ() && !m_playlist->queue().isEmpty()) {
                m_playlist->playItemAt(
                    qMax(0, m_playlist->currentIndex()));
            }
        });
    }
}

void MainWindow::saveAllSurfaces() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    int saved = 0;

    for (int i = 0; i < m_surfaceBar->count(); ++i) {
        auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(i));
        if (!sw) continue;

        const QString typeStr = M1::surfaceTypeString(sw->surfaceType());
        s.setValue(QString("session/surface%1").arg(i), typeStr);

        // Build config from live state
        M1::SurfaceConfig cfg;
        cfg.surfaceType = typeStr;
        cfg.surfaceName = sw->surfaceName();

        for (auto* mod : sw->modules()) {
            M1::ModuleConfig mc;
            mc.id      = mod->moduleId();
            mc.enabled = true;
            cfg.modules.append(mc);
        }

        cfg.save(M1::SurfaceConfig::configPath(typeStr));
        ++saved;
    }

    s.setValue("session/openSurfaces", saved);
}

// ─── Layout YAML persistence ──────────────────────────────────────────────────
QString MainWindow::layoutYamlPath() {
    return QCoreApplication::applicationDirPath() + "/mcaster1_studio_layout.yaml";
}

void MainWindow::saveAllLayouts() {
    QMap<QString, SurfaceLayoutSnapshot> snaps;
    for (int i = 0; i < m_surfaceBar->count(); ++i) {
        auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(i));
        if (sw) snaps[sw->surfaceName()] = sw->layoutSnapshot();
    }
    SurfaceConfigYaml::save(layoutYamlPath(), snaps);
}

void MainWindow::restoreAllLayouts() {
    const auto data = SurfaceConfigYaml::load(layoutYamlPath());
    if (data.isEmpty()) return;
    for (int i = 0; i < m_surfaceBar->count(); ++i) {
        auto* sw = qobject_cast<SurfaceWidget*>(m_surfaceBar->widget(i));
        if (!sw) continue;
        auto it = data.find(sw->surfaceName());
        if (it != data.end()) sw->applyLayoutSnapshot(*it);
    }
}

// ─── Audio engine ─────────────────────────────────────────────────────────────
void MainWindow::startAudioEngine() {
    if (!m_audio->initialize()) {
        m_audioStatusLabel->setText("Audio: Error — " + m_audio->lastError());
        return;
    }

    QSettings s("Mcaster1", "Mcaster1Studio");
    int inDev  = s.value("audio/inputDevice",  m_audio->defaultInputDevice().index).toInt();
    int outDev = s.value("audio/outputDevice", m_audio->defaultOutputDevice().index).toInt();
    int sr     = s.value("audio/sampleRate",   48000).toInt();
    int buf    = s.value("audio/bufferSize",   512).toInt();

    if (m_audio->openStream(inDev, outDev, sr, buf)) {
        m_audio->startStream();
    } else {
        qWarning() << "[MainWindow] Preferred devices failed, trying defaults.";
        if (m_audio->openStream(
                m_audio->defaultInputDevice().index,
                m_audio->defaultOutputDevice().index, sr, buf)) {
            m_audio->startStream();
        } else {
            m_audioStatusLabel->setText("Audio: Failed to open stream — " + m_audio->lastError());
        }
    }

    // Open CUE headphone monitor stream on separate device (if configured)
    int cueDev = s.value("audio/cueDevice", -1).toInt();
    if (cueDev >= 0) {
        m_audio->openCueStream(cueDev);
    }
}

// ─── Slots ────────────────────────────────────────────────────────────────────
void MainWindow::onAudioStateChanged(M1::AudioEngineState state) {
    switch (state) {
    case M1::AudioEngineState::Uninitialized:
        m_engineStatusLabel->setText("Engine: Stopped"); break;
    case M1::AudioEngineState::Initialized:
        m_engineStatusLabel->setText("Engine: Initialized");
        m_audioStatusLabel->setText(QString("Audio: %1 Hz | %2 frames")
            .arg(m_audio->sampleRate()).arg(m_audio->framesPerBuffer())); break;
    case M1::AudioEngineState::Running:
        m_engineStatusLabel->setText("● Engine: Running");
        m_audioStatusLabel->setText(QString("Audio: %1 Hz | %2 frames/buf")
            .arg(m_audio->sampleRate()).arg(m_audio->framesPerBuffer())); break;
    case M1::AudioEngineState::Error:
        m_engineStatusLabel->setText("⚠ Engine: Error");
        m_audioStatusLabel->setText("Audio: " + m_audio->lastError()); break;
    }
}

void MainWindow::onLevelsUpdated(float, float, float, float) {}

void MainWindow::onPreferences() {
    bool wasRunning = m_audio->isRunning();
    if (wasRunning) m_audio->stopStream();
    m_audio->stopCueStream();

    PreferencesDialog dlg(m_audio, this);
    if (dlg.exec() == QDialog::Accepted) {
        QSettings s("Mcaster1", "Mcaster1Studio");
        s.setValue("audio/inputDevice",  dlg.selectedInputDevice());
        s.setValue("audio/outputDevice", dlg.selectedOutputDevice());
        s.setValue("audio/cueDevice",    dlg.selectedCueDevice());
        s.setValue("audio/sampleRate",   dlg.selectedSampleRate());
        s.setValue("audio/bufferSize",   dlg.selectedBufferSize());
        m_audio->openStream(dlg.selectedInputDevice(), dlg.selectedOutputDevice(),
                            dlg.selectedSampleRate(), dlg.selectedBufferSize());
        m_audio->startStream();
        // Open CUE stream on separate device
        if (dlg.selectedCueDevice() >= 0)
            m_audio->openCueStream(dlg.selectedCueDevice());
    } else if (wasRunning) {
        m_audio->startStream();
    }
}

void MainWindow::onDeviceSettings() {
    DeviceSettingsDialog dlg(m_audio, m_surfaceBar, this);
    if (dlg.exec() == QDialog::Accepted) {
        dlg.applyGlobalSettings();
    }
}

void MainWindow::onAddSurface() {
    m_surfaceBar->openSurface(M1::SurfaceType::Custom);
    auto* sw = qobject_cast<SurfaceWidget*>(
        m_surfaceBar->widget(m_surfaceBar->count() - 1));
    if (sw) {
        M1::SurfaceConfig cfg = M1::SurfaceConfig::defaultForType("custom");
        openSurfaceFromConfig(cfg);
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(this,
        "About Mcaster1Studio",
        QString("<b>Mcaster1Studio</b> v%1<br>"
                "Broadcast Automation Software Suite<br><br>"
                "© 2026 Mcaster1. All rights reserved.<br>"
                "<a href='https://mcaster1.com'>mcaster1.com</a>")
        .arg(MCASTER1STUDIO_VERSION_STRING));
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveAllSessionState();   // v2 stable-key save (module lists + splitter sizes)
    saveAllLayouts();        // YAML floating dock positions
    saveWindowState();

    // Close all pop-out windows (dock surfaces back first to avoid orphans)
    const auto poppedOut = m_poppedOutWindows.keys();
    for (auto* sw : poppedOut)
        dockBackSurface(sw);

    if (m_audio) m_audio->shutdown();
    event->accept();
}

void MainWindow::saveWindowState() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    s.setValue("window/geometry", saveGeometry());
    s.setValue("window/state",    saveState());
}

void MainWindow::restoreWindowState() {
    QSettings s("Mcaster1", "Mcaster1Studio");
    if (s.contains("window/geometry"))
        restoreGeometry(s.value("window/geometry").toByteArray());
    if (s.contains("window/state"))
        restoreState(s.value("window/state").toByteArray());
    else
        resize(1400, 800);
}

// ─── Pop-out / Dock-back surface management ──────────────────────────────────

void MainWindow::popOutSurface(SurfaceWidget* sw) {
    if (!sw) return;

    // Already popped out? Just raise the existing window
    if (m_poppedOutWindows.contains(sw)) {
        auto* win = m_poppedOutWindows[sw];
        win->raise();
        win->activateWindow();
        return;
    }

    // Remember the tab label and position
    const int tabIndex = m_surfaceBar->indexOf(sw);
    const QString tabLabel = (tabIndex >= 0) ? m_surfaceBar->tabText(tabIndex) : sw->surfaceName();

    // Remove from tab bar without deleting
    if (tabIndex >= 0) {
        m_surfaceBar->removeTab(tabIndex);
    }

    // Create the standalone window
    auto* win = new SurfaceWindow(sw, nullptr);  // top-level, no parent
    win->setWindowTitle(QString("Mcaster1Studio \u2014 %1").arg(tabLabel));
    win->show();

    // Dock-back when the user closes the pop-out window
    connect(win, &SurfaceWindow::dockBackRequested,
            this, [this](SurfaceWidget* surface) { dockBackSurface(surface); });

    m_poppedOutWindows.insert(sw, win);

    qInfo() << "[MainWindow] Popped out surface:" << tabLabel;
    statusBar()->showMessage(QString("Surface \"%1\" popped out to window").arg(tabLabel), 3000);
}

void MainWindow::sendSurfaceToMonitor(SurfaceWidget* sw, int screenIndex) {
    if (!sw) return;

    // Pop out first (or reuse existing window)
    popOutSurface(sw);

    auto* win = m_poppedOutWindows.value(sw);
    if (win) {
        win->maximizeOnScreen(screenIndex);
        qInfo() << "[MainWindow] Sent surface to monitor" << screenIndex;
    }
}

void MainWindow::dockBackSurface(SurfaceWidget* sw) {
    if (!sw) return;

    auto* win = m_poppedOutWindows.value(sw);
    if (!win) return;

    // Detach the surface from the pop-out window
    win->takeCentralWidget();

    // Re-add to the tab bar
    const QString name = sw->customName().isEmpty() ? sw->surfaceName() : sw->customName();
    const int idx = m_surfaceBar->addTab(sw, name);
    m_surfaceBar->setCurrentIndex(idx);

    // Clean up the window
    m_poppedOutWindows.remove(sw);
    win->deleteLater();

    qInfo() << "[MainWindow] Docked back surface:" << name;
    statusBar()->showMessage(QString("Surface \"%1\" docked back").arg(name), 3000);
    scheduleAutoSave();
}
