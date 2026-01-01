cmake_minimum_required(VERSION 3.28)

add_library(VideoModule STATIC
    VideoModule.h
    VideoModule.cpp
    VideoWidget.h
    VideoWidget.cpp
)

target_include_directories(VideoModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
)

target_link_libraries(VideoModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# MCASTER1PLUGIN_EXPORTS → dllexport on the C ABI functions
target_compile_definitions(VideoModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(VideoModule PRIVATE /EHsc)
endif()
