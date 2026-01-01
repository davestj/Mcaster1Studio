cmake_minimum_required(VERSION 3.28)

add_library(ClockModule STATIC
    ClockModule.cpp
    ClockModule.h
)

target_include_directories(ClockModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ClockModule
    PUBLIC
        Mcaster1Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(ClockModule PRIVATE MCASTER1PLUGIN_EXPORTS)
