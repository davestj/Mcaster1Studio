cmake_minimum_required(VERSION 3.28)

set(UI_SOURCES
    main.cpp
    MainWindow.cpp
    AppRibbon.cpp
    WeatherWidget.cpp
    SurfaceTabBar.cpp
    SurfaceWidget.cpp
    SurfaceTray.cpp
    SurfaceRibbon.cpp
    SurfaceEventLog.cpp
    SurfaceScheduler.cpp
    SurfaceSchedulerDialog.cpp
    ResizableWidget.cpp
    ModuleDock.cpp
    SurfaceConfigYaml.cpp
    SubSurfacePanel.cpp
    SubSurfaceTabBar.cpp
    NowPlayingWidget.cpp
    PreferencesDialog.cpp
    ThemeManager.cpp
    CustomSurfaceDialog.cpp
    DeviceSettingsDialog.cpp
    SurfaceWindow.cpp
    DbServerDialog.cpp
)
set(UI_HEADERS
    MainWindow.h
    AppRibbon.h
    WeatherWidget.h
    SurfaceTabBar.h
    SurfaceWidget.h
    SurfaceTray.h
    SurfaceRibbon.h
    SurfaceEventLog.h
    SurfaceScheduler.h
    SurfaceSchedulerDialog.h
    ResizableWidget.h
    ModuleDock.h
    SurfaceConfigYaml.h
    SubSurfacePanel.h
    SubSurfaceTabBar.h
    NowPlayingWidget.h
    PreferencesDialog.h
    ThemeManager.h
    CustomSurfaceDialog.h
    DeviceSettingsDialog.h
    SurfaceWindow.h
    DbServerDialog.h
)

qt_add_executable(Mcaster1Studio
    ${UI_SOURCES}
    ${UI_HEADERS}
)

target_include_directories(Mcaster1Studio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/AudioEngine/include
    ${CMAKE_SOURCE_DIR}/Modules/VUMeterModule
    ${CMAKE_SOURCE_DIR}/Modules/DeckModule
    ${CMAKE_SOURCE_DIR}/Modules/MediaLibraryModule
    ${CMAKE_SOURCE_DIR}/Modules/EncoderModule
    ${CMAKE_SOURCE_DIR}/Modules/EffectsRackModule
    ${CMAKE_SOURCE_DIR}/Modules/MetadataModule
    ${CMAKE_SOURCE_DIR}/Modules/PlaylistModule
    ${CMAKE_SOURCE_DIR}/Modules/VideoModule
    ${CMAKE_SOURCE_DIR}/Modules/MonitorModule
    ${CMAKE_SOURCE_DIR}/Modules/PTTModule
    ${CMAKE_SOURCE_DIR}/Modules/PodcastModule
    ${CMAKE_SOURCE_DIR}/Modules/ClockModule
    ${CMAKE_SOURCE_DIR}/Modules/QueueModule
    ${CMAKE_SOURCE_DIR}/Modules/CartWallModule
    ${CMAKE_SOURCE_DIR}/Modules/DatabaseModule
    ${CMAKE_SOURCE_DIR}/Modules/HealthModule
    ${CMAKE_SOURCE_DIR}/Modules/TimerClockModule
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
    ${CMAKE_SOURCE_DIR}/Modules/LyricsCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/ScriptureCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/AnnounceCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/TelePromptModule
    ${CMAKE_SOURCE_DIR}/Modules/MediaCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/StageMonModule
    ${CMAKE_SOURCE_DIR}/Modules/AudioMixModule
    ${CMAKE_SOURCE_DIR}/Modules/TranscribeRecModule
    ${CMAKE_SOURCE_DIR}/Modules/SwitchCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/ServiceRunnerModule
    ${CMAKE_SOURCE_DIR}/Modules/PodMixerModule
    ${CMAKE_SOURCE_DIR}/Modules/PodPTTModule
    ${CMAKE_SOURCE_DIR}/Modules/PodRecorderModule
    ${CMAKE_SOURCE_DIR}/Modules/PodSoundboardModule
    ${CMAKE_SOURCE_DIR}/Modules/PodFXModule
    ${CMAKE_SOURCE_DIR}/Modules/PodEditorModule
    ${CMAKE_SOURCE_DIR}/Modules/PodEncodeModule
    ${CMAKE_SOURCE_DIR}/Modules/PodTranscribeModule
    ${CMAKE_SOURCE_DIR}/Modules/PodShowNotesModule
    ${CMAKE_SOURCE_DIR}/Modules/PodRSSModule
    ${CMAKE_SOURCE_DIR}/Modules/PodPublisherModule
    ${CMAKE_SOURCE_DIR}/Modules/PodAnalyticsModule
    ${CMAKE_SOURCE_DIR}/Modules/PodRemoteModule
    ${CMAKE_SOURCE_DIR}/Modules/AuxDeckModule
)

target_link_libraries(Mcaster1Studio PRIVATE
    Mcaster1Core
    Mcaster1Audio
    VUMeterModule
    DeckModule
    MediaLibraryModule
    EncoderModule
    EffectsRackModule
    MetadataModule
    PlaylistModule
    VideoModule
    MonitorModule
    PTTModule
    PodcastModule
    ClockModule
    QueueModule
    CartWallModule
    DatabaseModule
    HealthModule
    TimerClockModule
    GraphicsEngineModule
    LyricsCasterModule
    ScriptureCasterModule
    AnnounceCasterModule
    TelePromptModule
    MediaCasterModule
    StageMonModule
    AudioMixModule
    TranscribeRecModule
    SwitchCasterModule
    ServiceRunnerModule
    PodMixerModule
    PodPTTModule
    PodRecorderModule
    PodSoundboardModule
    PodFXModule
    PodEditorModule
    PodEncodeModule
    PodTranscribeModule
    PodShowNotesModule
    PodRSSModule
    PodPublisherModule
    PodAnalyticsModule
    PodRemoteModule
    AuxDeckModule
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Svg
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Concurrent
)

# ─── Embedded resources ───────────────────────────────────────────────────────
qt_add_resources(Mcaster1Studio "appresources"
    PREFIX "/"
    FILES
        themes/dark.qss
        themes/classic.qss
        themes/light.qss
)

qt_add_resources(Mcaster1Studio "icons"
    PREFIX "/resources/icons"
    BASE   "resources/icons"
    FILES
        resources/icons/close.svg
        resources/icons/pin.svg
        resources/icons/unpin.svg
        resources/icons/float.svg
        resources/icons/drag-handle.svg
        resources/icons/settings.svg
        resources/icons/add.svg
        resources/icons/play.svg
        resources/icons/pause.svg
        resources/icons/stop.svg
        resources/icons/cue.svg
        resources/icons/loop.svg
        resources/icons/vu-meter.svg
        resources/icons/deck.svg
        resources/icons/library.svg
        resources/icons/encoder.svg
        resources/icons/metadata.svg
        resources/icons/monitor.svg
        resources/icons/ptt.svg
        resources/icons/podcast.svg
        resources/icons/video.svg
        resources/icons/effects.svg
        resources/icons/playlist.svg
        resources/icons/clock.svg
        resources/icons/surface-alpha.svg
        resources/icons/surface-beta.svg
        resources/icons/surface-dj.svg
        resources/icons/surface-company.svg
        resources/icons/surface-entertainment.svg
        resources/icons/surface-social.svg
        resources/icons/surface-podcast.svg
        resources/icons/surface-church.svg
        resources/icons/surface-custom.svg
        resources/icons/app-logo.svg
        resources/icons/tray-minimize.svg
        resources/icons/ribbon-on-air.svg
        resources/icons/deck-a.svg
        resources/icons/deck-b.svg
        resources/icons/vinyl-record.svg
        resources/icons/eject.svg
        resources/icons/cartwall.svg
        resources/icons/database.svg
        resources/icons/live-monitor.svg
        resources/icons/health.svg
        resources/icons/timerclock.svg
        resources/icons/graphics-engine.svg
        resources/icons/lyrics-caster.svg
        resources/icons/scripture-caster.svg
        resources/icons/announce-caster.svg
        resources/icons/teleprompt.svg
        resources/icons/media-caster.svg
        resources/icons/stage-monitor.svg
        resources/icons/audio-mix.svg
        resources/icons/transcribe-rec.svg
        resources/icons/switch-caster.svg
        resources/icons/service-runner.svg
        resources/icons/pod-mixer.svg
        resources/icons/pod-ptt.svg
        resources/icons/pod-recorder.svg
        resources/icons/pod-soundboard.svg
        resources/icons/pod-fx.svg
        resources/icons/pod-editor.svg
        resources/icons/pod-encode.svg
        resources/icons/pod-transcribe.svg
        resources/icons/pod-shownotes.svg
        resources/icons/pod-rss.svg
        resources/icons/pod-publisher.svg
        resources/icons/pod-analytics.svg
        resources/icons/pod-remote.svg
        resources/icons/auxdeck.svg
)

qt_add_resources(Mcaster1Studio "appicons"
    PREFIX "/resources/appicon"
    BASE   "resources/appicon"
    FILES
        resources/appicon/app-icon-512.svg
        resources/appicon/app-icon-256.svg
        resources/appicon/app-icon-128.svg
        resources/appicon/app-icon-64.svg
        resources/appicon/app-icon-48.svg
        resources/appicon/app-icon-32.svg
        resources/appicon/app-icon-16.svg
)

# ─── Windows: no console window in Release ────────────────────────────────────
if(WIN32)
    set_target_properties(Mcaster1Studio PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
endif()

# ─── Qt deploy helper (windeployqt) — run after build ────────────────────────
if(WIN32)
    find_program(WINDEPLOYQT windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if(WINDEPLOYQT)
        add_custom_command(TARGET Mcaster1Studio POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                --no-translations
                --no-system-d3d-compiler
                "$<TARGET_FILE:Mcaster1Studio>"
            COMMENT "Running windeployqt..."
            VERBATIM
        )
    endif()
endif()
