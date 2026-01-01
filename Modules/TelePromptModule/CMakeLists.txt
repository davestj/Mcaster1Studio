cmake_minimum_required(VERSION 3.28)

add_library(TelePromptModule STATIC
    TelePromptModule.cpp
    TelePromptModule.h
)

target_include_directories(TelePromptModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/GraphicsEngineModule
)

target_link_libraries(TelePromptModule
    PUBLIC
        Mcaster1Core
        GraphicsEngineModule
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(TelePromptModule PRIVATE MCASTER1PLUGIN_EXPORTS)
