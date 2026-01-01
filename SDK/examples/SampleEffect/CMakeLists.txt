cmake_minimum_required(VERSION 3.28)

# SampleEffect — minimal Mcaster1Studio effect plugin example.
# This DLL can be dropped into Mcaster1Studio's plugins/effects/ folder.
add_library(SampleEffect SHARED
    SampleEffect.h
    SampleEffect.cpp
)

target_link_libraries(SampleEffect PRIVATE
    Mcaster1SDK
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(SampleEffect PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(SampleEffect PRIVATE /EHsc)
    set_target_properties(SampleEffect PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# Deploy SampleEffect.dll → plugins/effects/ next to the main executable
add_custom_command(TARGET SampleEffect POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SampleEffect>/plugins/effects"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SampleEffect>"
        "$<TARGET_FILE_DIR:SampleEffect>/plugins/effects/$<TARGET_FILE_NAME:SampleEffect>"
    COMMENT "Deploying SampleEffect.dll to plugins/effects/"
)
