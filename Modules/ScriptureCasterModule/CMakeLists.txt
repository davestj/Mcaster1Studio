cmake_minimum_required(VERSION 3.28)

add_library(ScriptureCasterModule STATIC
    ScriptureCasterModule.cpp
    ScriptureCasterModule.h
)

target_include_directories(ScriptureCasterModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
)

target_link_libraries(ScriptureCasterModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(ScriptureCasterModule PRIVATE MCASTER1PLUGIN_EXPORTS)
