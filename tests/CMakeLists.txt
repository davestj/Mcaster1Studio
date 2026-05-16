cmake_minimum_required(VERSION 3.28)

# ─── Common include directories for all test targets ──────────────────────────
# Most modules use PRIVATE include directories, so we need to add them
# explicitly for the test targets.
set(MODULE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/AudioEngine/include
    ${CMAKE_SOURCE_DIR}/UI
    ${CMAKE_SOURCE_DIR}/Modules/VUMeterModule
    ${CMAKE_SOURCE_DIR}/Modules/DeckModule
    ${CMAKE_SOURCE_DIR}/Modules/MediaLibraryModule
    ${CMAKE_SOURCE_DIR}/Modules/EncoderModule
    ${CMAKE_SOURCE_DIR}/Modules/EffectsRackModule
    ${CMAKE_SOURCE_DIR}/Modules/MetadataModule
    ${CMAKE_SOURCE_DIR}/Modules/PlaylistModule
    ${CMAKE_SOURCE_DIR}/Modules/PTTModule
    ${CMAKE_SOURCE_DIR}/Modules/PodcastModule
    ${CMAKE_SOURCE_DIR}/Modules/VideoModule
    ${CMAKE_SOURCE_DIR}/Modules/MonitorModule
    ${CMAKE_SOURCE_DIR}/Modules/ClockModule
    ${CMAKE_SOURCE_DIR}/Modules/QueueModule
    ${CMAKE_SOURCE_DIR}/Modules/CartWallModule
    ${CMAKE_SOURCE_DIR}/Modules/DatabaseModule
    ${CMAKE_SOURCE_DIR}/Modules/HealthModule
    ${CMAKE_SOURCE_DIR}/Modules/AuxDeckModule
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
)

# ─── Database Driver Integration Tests ──────────────────────────────────────
add_executable(TestDatabases test_databases.cpp)

target_link_libraries(TestDatabases PRIVATE
    MediaLibraryModule
    Mcaster1Core
    Qt6::Core
)

# Propagate compile definitions from MediaLibraryModule
# (M1_HAS_POSTGRESQL, M1_HAS_FIREBIRD, M1_HAS_MSSQL are PUBLIC/PRIVATE on the lib,
#  but the test needs them too for #ifdef guards)
target_compile_definitions(TestDatabases PRIVATE
    $<TARGET_PROPERTY:MediaLibraryModule,COMPILE_DEFINITIONS>
)

# Output to bin/<config>/ alongside main app
set_target_properties(TestDatabases PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# ─── Core Library Tests ──────────────────────────────────────────────────────
add_executable(TestCore test_core.cpp)

target_include_directories(TestCore PRIVATE ${MODULE_INCLUDE_DIRS})

target_link_libraries(TestCore PRIVATE
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(TestCore PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# ─── Broadcast Module Tests ──────────────────────────────────────────────────
# ThemeManager is referenced by several module widgets (VUMeterWidget,
# PlaylistWidget, CrossfaderModule, etc.) but lives in the UI target.
# We compile it directly into the test to resolve the link dependency.
add_executable(TestModules
    test_modules.cpp
    ${CMAKE_SOURCE_DIR}/UI/ThemeManager.cpp
    ${CMAKE_SOURCE_DIR}/UI/ThemeManager.h
)

target_include_directories(TestModules PRIVATE ${MODULE_INCLUDE_DIRS})

target_link_libraries(TestModules PRIVATE
    Mcaster1Core
    Mcaster1Audio
    VUMeterModule
    DeckModule
    MediaLibraryModule
    EncoderModule
    EffectsRackModule
    MetadataModule
    PlaylistModule
    PTTModule
    PodcastModule
    VideoModule
    MonitorModule
    ClockModule
    QueueModule
    CartWallModule
    DatabaseModule
    HealthModule
    AuxDeckModule
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Charts
    Qt6::Svg
    Qt6::Concurrent
    Qt6::Xml
)

target_compile_definitions(TestModules PRIVATE
    MCASTER1PLUGIN_EXPORTS
    $<TARGET_PROPERTY:MediaLibraryModule,COMPILE_DEFINITIONS>
)

set_target_properties(TestModules PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# ─── Church Module Tests ─────────────────────────────────────────────────────
add_executable(TestChurch test_church.cpp)

target_include_directories(TestChurch PRIVATE ${MODULE_INCLUDE_DIRS})

target_link_libraries(TestChurch PRIVATE
    Mcaster1Core
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Svg
)

target_compile_definitions(TestChurch PRIVATE MCASTER1PLUGIN_EXPORTS)

set_target_properties(TestChurch PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# ─── Podcast Module Tests ────────────────────────────────────────────────────
add_executable(TestPodcast test_podcast.cpp)

target_include_directories(TestPodcast PRIVATE ${MODULE_INCLUDE_DIRS})

target_link_libraries(TestPodcast PRIVATE
    Mcaster1Core
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
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
)

target_compile_definitions(TestPodcast PRIVATE MCASTER1PLUGIN_EXPORTS)

set_target_properties(TestPodcast PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# ─── AuxDeck Module Tests ────────────────────────────────────────────────────
add_executable(TestAuxDeck
    test_auxdeck.cpp
    ${CMAKE_SOURCE_DIR}/UI/ThemeManager.cpp
    ${CMAKE_SOURCE_DIR}/UI/ThemeManager.h
)

target_include_directories(TestAuxDeck PRIVATE ${MODULE_INCLUDE_DIRS})

target_link_libraries(TestAuxDeck PRIVATE
    Mcaster1Core
    Mcaster1Audio
    AuxDeckModule
    DeckModule
    PlaylistModule
    MediaLibraryModule
    PTTModule
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Charts
    Qt6::Svg
    Qt6::Concurrent
)

target_compile_definitions(TestAuxDeck PRIVATE MCASTER1PLUGIN_EXPORTS)

set_target_properties(TestAuxDeck PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# ─── Thread Pool Tests ──────────────────────────────────────────────────────
add_executable(TestThreadPool test_threadpool.cpp)

target_include_directories(TestThreadPool PRIVATE ${MODULE_INCLUDE_DIRS})

target_link_libraries(TestThreadPool PRIVATE
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

set_target_properties(TestThreadPool PROPERTIES
    FOLDER "Tests"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
