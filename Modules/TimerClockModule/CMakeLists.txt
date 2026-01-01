cmake_minimum_required(VERSION 3.28)

add_library(TimerClockModule STATIC
    TimerClockModule.cpp
    TimerClockModule.h
)

target_include_directories(TimerClockModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(TimerClockModule
    PUBLIC
        Mcaster1Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(TimerClockModule PRIVATE MCASTER1PLUGIN_EXPORTS)
