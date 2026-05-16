cmake_minimum_required(VERSION 3.28)

add_library(EncoderModule STATIC
    AudioRingBuffer.h
    EncoderConfig.h
    EncoderDsp.h
    EncoderDsp.cpp
    EncoderEventLog.h
    EncoderLogDialog.h
    EncoderLogDialog.cpp
    DnasPoller.h
    DnasPoller.cpp
    IcyPusher.h
    IcyPusher.cpp
    EncoderSlot.h
    EncoderSlot.cpp
    EncoderListWidget.h
    EncoderListWidget.cpp
    EncoderVuPanel.h
    EncoderVuPanel.cpp
    EncoderConfigDialog.h
    EncoderConfigDialog.cpp
    LiveMonitorChart.h
    LiveMonitorChart.cpp
    LiveMonitorWindow.h
    LiveMonitorWindow.cpp
    EncoderModule.h
    EncoderModule.cpp
)

target_include_directories(EncoderModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(EncoderModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Opus::opus
    Ogg::ogg
)

# ── libopusenc ────────────────────────────────────────────────────────────────
find_library(OPUSENC_LIB_RELEASE NAMES opusenc
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH)
find_library(OPUSENC_LIB_DEBUG NAMES opusenc
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH)
find_path(OPUSENC_INCLUDE_DIR NAMES opus/opusenc.h
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include" NO_DEFAULT_PATH)

if(OPUSENC_LIB_RELEASE OR OPUSENC_LIB_DEBUG)
    add_library(opusenc_vcpkg STATIC IMPORTED GLOBAL)
    set_target_properties(opusenc_vcpkg PROPERTIES
        IMPORTED_LOCATION         "${OPUSENC_LIB_RELEASE}"
        IMPORTED_LOCATION_DEBUG   "${OPUSENC_LIB_DEBUG}"
        IMPORTED_LOCATION_RELEASE "${OPUSENC_LIB_RELEASE}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPUSENC_INCLUDE_DIR}"
    )
    target_link_libraries(EncoderModule PUBLIC opusenc_vcpkg)
    message(STATUS "[Encoder] opusenc found: ${OPUSENC_LIB_RELEASE}")
else()
    message(WARNING "[Encoder] opusenc not found — Opus codec will not link")
endif()

# ── LAME MP3 (external/lame/) ─────────────────────────────────────────────────
find_library(LAME_LIB_RELEASE NAMES libmp3lame mp3lame
    PATHS "${CMAKE_SOURCE_DIR}/external/lame/lib/Release"
          "${CMAKE_SOURCE_DIR}/external/lame/lib"
    NO_DEFAULT_PATH)
find_library(LAME_LIB_DEBUG NAMES libmp3lame mp3lame
    PATHS "${CMAKE_SOURCE_DIR}/external/lame/lib/Debug"
          "${CMAKE_SOURCE_DIR}/external/lame/lib"
    NO_DEFAULT_PATH)
find_path(LAME_INCLUDE_DIR NAMES lame/lame.h
    PATHS "${CMAKE_SOURCE_DIR}/external/lame/include" NO_DEFAULT_PATH)

if(LAME_LIB_RELEASE OR LAME_LIB_DEBUG)
    add_library(lame_enc STATIC IMPORTED GLOBAL)
    set_target_properties(lame_enc PROPERTIES
        IMPORTED_LOCATION         "${LAME_LIB_RELEASE}"
        IMPORTED_LOCATION_DEBUG   "${LAME_LIB_DEBUG}"
        IMPORTED_LOCATION_RELEASE "${LAME_LIB_RELEASE}"
        INTERFACE_INCLUDE_DIRECTORIES "${LAME_INCLUDE_DIR}"
    )
    target_link_libraries(EncoderModule PUBLIC lame_enc)
    target_compile_definitions(EncoderModule PRIVATE HAVE_LAME)
    # Copy the runtime DLL to build output so the app can actually load it
    set(LAME_DLL "${CMAKE_SOURCE_DIR}/external/lame/bin/libmp3lame.dll")
    if(EXISTS "${LAME_DLL}")
        add_custom_command(TARGET EncoderModule POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LAME_DLL}"
                "$<TARGET_FILE_DIR:EncoderModule>/libmp3lame.dll"
            COMMENT "Copying libmp3lame.dll to build output"
        )
    endif()
    message(STATUS "[Encoder] LAME found: ${LAME_LIB_RELEASE}")
else()
    message(STATUS "[Encoder] LAME not found — MP3 encoding not available")
endif()

# ── OGG Vorbis (vcpkg) ────────────────────────────────────────────────────────
find_package(Vorbis CONFIG QUIET)
if(Vorbis_FOUND)
    target_link_libraries(EncoderModule PUBLIC Vorbis::vorbis Vorbis::vorbisenc)
    message(STATUS "[Encoder] Vorbis found")
else()
    message(STATUS "[Encoder] Vorbis not found — OGG Vorbis encoding not available")
endif()

# ── FLAC (vcpkg) ──────────────────────────────────────────────────────────────
find_package(FLAC CONFIG QUIET)
if(FLAC_FOUND)
    target_link_libraries(EncoderModule PUBLIC FLAC::FLAC)
    message(STATUS "[Encoder] FLAC found")
else()
    message(STATUS "[Encoder] FLAC not found — FLAC encoding not available")
endif()

# ── fdk-aac (vcpkg, optional) ─────────────────────────────────────────────────
find_package(fdkaac CONFIG QUIET)
if(fdkaac_FOUND)
    target_link_libraries(EncoderModule PUBLIC fdkaac::fdkaac)
    target_compile_definitions(EncoderModule PRIVATE HAVE_FDK_AAC)
    message(STATUS "[Encoder] fdk-aac found")
else()
    message(STATUS "[Encoder] fdk-aac not found — AAC encoding not available")
endif()

# ── PortAudio (already found by root CMakeLists as 'portaudio' target) ────────
if(TARGET portaudio)
    target_link_libraries(EncoderModule PUBLIC portaudio)
    message(STATUS "[Encoder] PortAudio found")
else()
    message(STATUS "[Encoder] PortAudio not found — PortAudioDevice source not available")
endif()

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(EncoderModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(EncoderModule PRIVATE /EHsc)
endif()
