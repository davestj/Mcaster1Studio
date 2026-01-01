cmake_minimum_required(VERSION 3.28)

add_library(EffectsRackModule STATIC
    EffectsRackModule.h
    EffectsRackModule.cpp
    EffectsRackWidget.h
    EffectsRackWidget.cpp
)

target_include_directories(EffectsRackModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
    ${CMAKE_SOURCE_DIR}/EffectsPlugins/SonicMaximizer
    ${CMAKE_SOURCE_DIR}/EffectsPlugins/Equalizer31Band
    ${CMAKE_SOURCE_DIR}/EffectsPlugins/CompressorLimiter
)

target_link_libraries(EffectsRackModule PUBLIC
    Mcaster1Core
    FxSonicMaximizer
    FxEqualizer31Band
    FxCompressorLimiter
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(EffectsRackModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(EffectsRackModule PRIVATE /EHsc)
endif()
