cmake_minimum_required(VERSION 3.28)

add_library(ServiceRunnerModule STATIC
    ServiceRunnerModule.cpp
    ServiceRunnerModule.h
)

target_include_directories(ServiceRunnerModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/TimerClockModule
    ${CMAKE_SOURCE_DIR}/Modules/SwitchCasterModule
    ${CMAKE_SOURCE_DIR}/Modules/TranscribeRecModule
    ${CMAKE_SOURCE_DIR}/Modules/AudioMixModule
)

target_link_libraries(ServiceRunnerModule
    PUBLIC
        Mcaster1Core
        TimerClockModule
        SwitchCasterModule
        TranscribeRecModule
        AudioMixModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(ServiceRunnerModule PRIVATE MCASTER1PLUGIN_EXPORTS)
