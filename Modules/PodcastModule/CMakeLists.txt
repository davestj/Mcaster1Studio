cmake_minimum_required(VERSION 3.28)

add_library(PodcastModule STATIC
    PodcastModule.h
    PodcastModule.cpp
    PodcastWidget.h
    PodcastWidget.cpp
)

target_include_directories(PodcastModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(PodcastModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

target_compile_definitions(PodcastModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(PodcastModule PRIVATE /EHsc)
endif()
