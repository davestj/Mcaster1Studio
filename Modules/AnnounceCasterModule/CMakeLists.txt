cmake_minimum_required(VERSION 3.28)

add_library(AnnounceCasterModule STATIC
    AnnounceCasterModule.cpp
    AnnounceCasterModule.h
)

target_include_directories(AnnounceCasterModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
)

target_link_libraries(AnnounceCasterModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(AnnounceCasterModule PRIVATE MCASTER1PLUGIN_EXPORTS)
