cmake_minimum_required(VERSION 3.28)

add_library(PlaylistModule STATIC
    PlaylistModule.h
    PlaylistModule.cpp
    PlaylistWidget.h
    PlaylistWidget.cpp
    PlaylistConfigDialog.h
    PlaylistConfigDialog.cpp
    ClockTemplate.h
    ClockTemplate.cpp
    BroadcastLogger.h
    BroadcastLogger.cpp
)

target_include_directories(PlaylistModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/Modules/DeckModule
    ${CMAKE_SOURCE_DIR}/Modules/MediaLibraryModule
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(PlaylistModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

target_compile_definitions(PlaylistModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(PlaylistModule PRIVATE /EHsc)
endif()
