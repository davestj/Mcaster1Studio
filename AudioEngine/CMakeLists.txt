cmake_minimum_required(VERSION 3.28)

set(AUDIO_HEADERS
    include/AudioEngine.h
    include/AudioMixer.h
    include/AudioDevice.h
    include/AudioResampler.h
)

set(AUDIO_SOURCES
    src/AudioEngine.cpp
    src/AudioMixer.cpp
    src/AudioDevice.cpp
    src/AudioResampler.cpp
)

add_library(Mcaster1Audio STATIC ${AUDIO_SOURCES} ${AUDIO_HEADERS})

target_include_directories(Mcaster1Audio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Mcaster1Audio
    PUBLIC
        Mcaster1Core
        Qt6::Core
    PRIVATE
        portaudio       # vcpkg portaudio (with ASIO)
        Qt6::Concurrent
)

target_compile_definitions(Mcaster1Audio
    PRIVATE MCASTER1AUDIO_LIBRARY
    PUBLIC  MCASTER1AUDIO_SHARED
)

set_target_properties(Mcaster1Audio PROPERTIES
    OUTPUT_NAME "Mcaster1Audio"
    VERSION     "${PROJECT_VERSION}"
    SOVERSION   "0"
)
