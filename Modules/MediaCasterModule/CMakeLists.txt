cmake_minimum_required(VERSION 3.28)

add_library(MediaCasterModule STATIC
    MediaCasterModule.cpp
    MediaCasterModule.h
)

target_include_directories(MediaCasterModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
)

target_link_libraries(MediaCasterModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::MultimediaWidgets
)

target_compile_definitions(MediaCasterModule PRIVATE MCASTER1PLUGIN_EXPORTS)
