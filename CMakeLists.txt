cmake_minimum_required(VERSION 3.28)

project(Mcaster1Studio
    VERSION 0.2.0
    DESCRIPTION "Mcaster1Studio - Broadcast Automation Software Suite"
    HOMEPAGE_URL "https://mcaster1.com"
    LANGUAGES CXX
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Output directories — all outputs (dll, lib, exe) in bin/<config>/ for VS multi-config
# This keeps DLLs and their import libs (.lib) together, avoiding linker path issues.
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/bin/${cfg}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/bin/${cfg}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${CMAKE_BINARY_DIR}/bin/${cfg}")
endforeach()

# vcpkg toolchain is passed via -DCMAKE_TOOLCHAIN_FILE on the command line
# Qt6 is found via -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"

# Platform defines
if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        _CRT_SECURE_NO_WARNINGS
    )
    # Enable MSVC parallel builds
    add_compile_options(/MP)
endif()

# Find Qt6
find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
    Network
    Xml
    Svg
    Concurrent
    Charts
)
qt_standard_project_setup()

# Find vcpkg packages
find_package(PkgConfig QUIET)
find_package(portaudio CONFIG REQUIRED)
find_package(FLAC CONFIG REQUIRED)
find_package(Opus CONFIG REQUIRED)
find_package(Vorbis CONFIG REQUIRED)
find_package(Ogg CONFIG REQUIRED)
find_package(unofficial-libmariadb CONFIG QUIET)
if(NOT unofficial-libmariadb_FOUND)
    find_package(unofficial-libmysql CONFIG REQUIRED)
endif()
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(taglib CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(ffmpeg COMPONENTS avcodec avformat avutil swresample swscale QUIET)

# Version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_BINARY_DIR}/generated/version.h"
    @ONLY
)

# Global include for generated headers
include_directories("${CMAKE_BINARY_DIR}/generated")

# Sub-projects
add_subdirectory(Core)
add_subdirectory(AudioEngine)
add_subdirectory(Modules/VUMeterModule)
add_subdirectory(Modules/DeckModule)
add_subdirectory(Modules/MediaLibraryModule)
add_subdirectory(Modules/EncoderModule)
add_subdirectory(EffectsPlugins/SonicMaximizer)
add_subdirectory(EffectsPlugins/Equalizer31Band)
add_subdirectory(EffectsPlugins/CompressorLimiter)
add_subdirectory(Modules/EffectsRackModule)
add_subdirectory(Modules/MetadataModule)
add_subdirectory(Modules/PlaylistModule)
add_subdirectory(Modules/PTTModule)
add_subdirectory(Modules/PodcastModule)
add_subdirectory(Modules/VideoModule)
add_subdirectory(Modules/MonitorModule)
add_subdirectory(Modules/ClockModule)
add_subdirectory(Modules/QueueModule)
add_subdirectory(Modules/CartWallModule)
add_subdirectory(Modules/HealthModule)
add_subdirectory(Modules/TimerClockModule)
add_subdirectory(Modules/GraphicsEngineModule)
add_subdirectory(Modules/LyricsCasterModule)
add_subdirectory(Modules/ScriptureCasterModule)
add_subdirectory(Modules/AnnounceCasterModule)
add_subdirectory(Modules/TelePromptModule)
add_subdirectory(Modules/MediaCasterModule)
add_subdirectory(Modules/StageMonModule)
add_subdirectory(Modules/AudioMixModule)
add_subdirectory(Modules/TranscribeRecModule)
add_subdirectory(Modules/SwitchCasterModule)
add_subdirectory(Modules/ServiceRunnerModule)
add_subdirectory(Modules/PodMixerModule)
add_subdirectory(Modules/PodPTTModule)
add_subdirectory(Modules/PodRecorderModule)
add_subdirectory(Modules/PodSoundboardModule)
add_subdirectory(Modules/PodFXModule)
add_subdirectory(Modules/PodEditorModule)
add_subdirectory(Modules/PodEncodeModule)
add_subdirectory(Modules/PodTranscribeModule)
add_subdirectory(Modules/PodShowNotesModule)
add_subdirectory(Modules/PodRSSModule)
add_subdirectory(Modules/PodPublisherModule)
add_subdirectory(Modules/PodAnalyticsModule)
add_subdirectory(Modules/PodRemoteModule)
add_subdirectory(SDK)
add_subdirectory(UI)

# Install targets (Phase 12)
# install(TARGETS Mcaster1Studio DESTINATION bin)

message(STATUS "")
message(STATUS "=== Mcaster1Studio v${PROJECT_VERSION} Configuration ===")
message(STATUS "  Build type    : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard  : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Qt version    : ${Qt6_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output dir    : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "=========================================")
message(STATUS "")
