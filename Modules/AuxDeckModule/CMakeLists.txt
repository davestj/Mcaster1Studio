cmake_minimum_required(VERSION 3.28)

add_library(AuxDeckModule STATIC
    AuxDeckModule.h
    AuxDeckModule.cpp
)

target_include_directories(AuxDeckModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Modules/DeckModule   # for DeckPlayer.h, DeckWidget.h (DeckInlineMeter), WaveformView.h
    ${CMAKE_SOURCE_DIR}/AudioEngine/include   # for AudioDevice.h
    ${CMAKE_SOURCE_DIR}/UI                    # for ThemeManager.h
)

target_link_libraries(AuxDeckModule PUBLIC
    Mcaster1Core
    Mcaster1Audio
    DeckModule
    portaudio
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
)

set_target_properties(AuxDeckModule PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
)
