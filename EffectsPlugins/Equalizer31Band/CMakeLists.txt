cmake_minimum_required(VERSION 3.28)

add_library(FxEqualizer31Band STATIC
    Equalizer31Band.h
    Equalizer31Band.cpp
)

target_include_directories(FxEqualizer31Band PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
)

target_link_libraries(FxEqualizer31Band PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(FxEqualizer31Band PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(FxEqualizer31Band PRIVATE /EHsc)
endif()
