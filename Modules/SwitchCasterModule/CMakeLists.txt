cmake_minimum_required(VERSION 3.28)

add_library(SwitchCasterModule STATIC
    SwitchCasterModule.cpp
    SwitchCasterModule.h
)

target_include_directories(SwitchCasterModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
    ${CMAKE_SOURCE_DIR}/Modules/LyricsCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/ScriptureCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/AnnounceCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/MediaCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/VideoModule
)

target_link_libraries(SwitchCasterModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        LyricsCasterModule
        ScriptureCasterModule
        AnnounceCasterModule
        MediaCasterModule
        VideoModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(SwitchCasterModule PRIVATE MCASTER1PLUGIN_EXPORTS)
