cmake_minimum_required(VERSION 3.28)

add_library(HealthModule STATIC
    HealthModule.h
    HealthModule.cpp
    HealthWidget.h
    HealthWidget.cpp
    MetricsServer.h
    MetricsServer.cpp
)

target_include_directories(HealthModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
    ${CMAKE_SOURCE_DIR}/Modules/EncoderModule
    ${CMAKE_SOURCE_DIR}/Modules/DeckModule
)

target_link_libraries(HealthModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
)

target_compile_definitions(HealthModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(HealthModule PRIVATE /EHsc)
endif()
