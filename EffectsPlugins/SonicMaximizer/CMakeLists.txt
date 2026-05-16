cmake_minimum_required(VERSION 3.28)

add_library(FxSonicMaximizer STATIC
    SonicMaximizer.h
    SonicMaximizer.cpp
)

target_include_directories(FxSonicMaximizer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(FxSonicMaximizer PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(FxSonicMaximizer PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(FxSonicMaximizer PRIVATE /EHsc)
endif()
