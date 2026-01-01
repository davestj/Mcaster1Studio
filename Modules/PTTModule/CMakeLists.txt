cmake_minimum_required(VERSION 3.28)

add_library(PTTModule STATIC
    PTTModule.h
    PTTModule.cpp
    PTTWidget.h
    PTTWidget.cpp
)

target_include_directories(PTTModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(PTTModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

target_compile_definitions(PTTModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(PTTModule PRIVATE /EHsc)
endif()
