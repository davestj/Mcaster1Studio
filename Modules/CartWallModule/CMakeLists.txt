cmake_minimum_required(VERSION 3.28)

set(CARTWALL_SOURCES
    CartWallModule.cpp
    CartWallWidget.cpp
    CartButton.cpp
    CartPlayer.cpp
)
set(CARTWALL_HEADERS
    CartWallModule.h
    CartWallWidget.h
    CartButton.h
    CartPlayer.h
)

add_library(CartWallModule STATIC ${CARTWALL_SOURCES} ${CARTWALL_HEADERS})

target_include_directories(CartWallModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(CartWallModule
    PUBLIC
        Mcaster1Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
)

target_compile_definitions(CartWallModule PRIVATE MCASTER1PLUGIN_EXPORTS)

set_target_properties(CartWallModule PROPERTIES
    OUTPUT_NAME "CartWallModule"
)
