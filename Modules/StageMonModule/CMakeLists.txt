cmake_minimum_required(VERSION 3.28)

add_library(StageMonModule STATIC
    StageMonModule.cpp
    StageMonModule.h
)

target_include_directories(StageMonModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
    ${CMAKE_SOURCE_DIR}/Modules/LyricsCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/ScriptureCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/TimerClockModule
)

target_link_libraries(StageMonModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        LyricsCasterModule
        ScriptureCasterModule
        TimerClockModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(StageMonModule PRIVATE MCASTER1PLUGIN_EXPORTS)
