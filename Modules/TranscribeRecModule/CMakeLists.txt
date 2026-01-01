cmake_minimum_required(VERSION 3.28)

add_library(TranscribeRecModule STATIC
    TranscribeRecModule.cpp
    TranscribeRecModule.h
)

target_include_directories(TranscribeRecModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/PTTModule
)

target_link_libraries(TranscribeRecModule
    PUBLIC
        Mcaster1Core
        PTTModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(TranscribeRecModule PRIVATE MCASTER1PLUGIN_EXPORTS)
