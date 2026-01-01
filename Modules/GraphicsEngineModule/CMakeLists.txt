cmake_minimum_required(VERSION 3.28)

add_library(GraphicsEngineModule STATIC
    GraphicsEngineModule.cpp
    GraphicsEngineModule.h
)

target_include_directories(GraphicsEngineModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(GraphicsEngineModule
    PUBLIC
        Mcaster1Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(GraphicsEngineModule PRIVATE MCASTER1PLUGIN_EXPORTS)
