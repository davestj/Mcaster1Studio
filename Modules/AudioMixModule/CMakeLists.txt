cmake_minimum_required(VERSION 3.28)

add_library(AudioMixModule STATIC
    AudioMixModule.cpp
    AudioMixModule.h
)

target_include_directories(AudioMixModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(AudioMixModule
    PUBLIC
        Mcaster1Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(AudioMixModule PRIVATE MCASTER1PLUGIN_EXPORTS)
