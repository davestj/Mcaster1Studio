cmake_minimum_required(VERSION 3.28)

add_library(MonitorModule STATIC
    MonitorModule.h
    MonitorModule.cpp
    MonitorWidget.h
    MonitorWidget.cpp
)

target_include_directories(MonitorModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(MonitorModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# MCASTER1PLUGIN_EXPORTS → dllexport on the C ABI functions
target_compile_definitions(MonitorModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(MonitorModule PRIVATE /EHsc)
endif()
