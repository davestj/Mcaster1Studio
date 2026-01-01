cmake_minimum_required(VERSION 3.28)

set(DECK_SOURCES
    DeckModule.cpp
    DeckAModule.cpp
    DeckBModule.cpp
    DeckPlayer.cpp
    DeckWidget.cpp
    WaveformView.cpp
    CrossfaderWidget.cpp
    CrossfaderModule.cpp
    CrossfaderSettingsDialog.cpp
    StreamReader.cpp
)
set(DECK_HEADERS
    DeckModule.h
    DeckAModule.h
    DeckBModule.h
    DeckPlayer.h
    DeckWidget.h
    WaveformView.h
    CrossfaderWidget.h
    CrossfaderModule.h
    CrossfaderSettingsDialog.h
    StreamReader.h
)

add_library(DeckModule STATIC ${DECK_SOURCES} ${DECK_HEADERS})

target_include_directories(DeckModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI
    ${CMAKE_SOURCE_DIR}/Modules/PTTModule
)

target_link_libraries(DeckModule
    PUBLIC
        Mcaster1Core
        Mcaster1Audio
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Svg
        Qt6::Multimedia
        Qt6::MultimediaWidgets
        Qt6::Concurrent
        Qt6::Network
        Qt6::Charts
    PRIVATE
        TagLib::tag
)

target_compile_definitions(DeckModule PRIVATE MCASTER1PLUGIN_EXPORTS)

# FFmpeg — for StreamReader HTTP stream decode
# Use individual cached variables from vcpkg's FindFFMPEG (full paths)
if(AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY)
    target_include_directories(DeckModule PRIVATE ${AVCODEC_INCLUDE_DIR})
    # Find swresample in the same lib directory
    get_filename_component(_ffmpeg_lib_dir "${AVCODEC_LIBRARY}" DIRECTORY)
    find_library(SWRESAMPLE_LIBRARY NAMES swresample PATHS "${_ffmpeg_lib_dir}" NO_DEFAULT_PATH)
    target_link_libraries(DeckModule PRIVATE
        ${AVFORMAT_LIBRARY}
        ${AVCODEC_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
    )
    target_compile_definitions(DeckModule PRIVATE HAVE_FFMPEG)
    message(STATUS "[DeckModule] FFmpeg found: ${_ffmpeg_lib_dir}")
endif()

# TagLib requires structured exception handling on MSVC
if(MSVC)
    target_compile_options(DeckModule PRIVATE /EHa)
endif()

set_target_properties(DeckModule PROPERTIES
    OUTPUT_NAME "DeckModule"
)
