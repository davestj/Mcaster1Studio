cmake_minimum_required(VERSION 3.28)

set(VU_SOURCES
    VUMeterModule.cpp
    VUMeterWidget.cpp
)
set(VU_HEADERS
    VUMeterModule.h
    VUMeterWidget.h
)

add_library(VUMeterModule STATIC ${VU_SOURCES} ${VU_HEADERS})

target_include_directories(VUMeterModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(VUMeterModule
    PUBLIC
        Mcaster1Core
        Mcaster1Audio
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

target_compile_definitions(VUMeterModule PRIVATE MCASTER1PLUGIN_EXPORTS)

set_target_properties(VUMeterModule PROPERTIES
    OUTPUT_NAME "VUMeterModule"
)
