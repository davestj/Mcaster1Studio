cmake_minimum_required(VERSION 3.28)

add_library(FxCompressorLimiter STATIC
    CompressorLimiter.h
    CompressorLimiter.cpp
)

target_include_directories(FxCompressorLimiter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(FxCompressorLimiter PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(FxCompressorLimiter PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(FxCompressorLimiter PRIVATE /EHsc)
endif()
