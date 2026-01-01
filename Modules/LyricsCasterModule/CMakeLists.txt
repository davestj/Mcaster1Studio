cmake_minimum_required(VERSION 3.28)

add_library(LyricsCasterModule STATIC
    LyricsCasterModule.cpp
    LyricsCasterModule.h
)

target_include_directories(LyricsCasterModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
)

target_link_libraries(LyricsCasterModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(LyricsCasterModule PRIVATE MCASTER1PLUGIN_EXPORTS)
