cmake_minimum_required(VERSION 3.28)

# SampleModule — minimal Mcaster1Studio plugin example.
# This DLL can be dropped into Mcaster1Studio's plugins/modules/ folder.
add_library(SampleModule SHARED
    SampleModule.h
    SampleModule.cpp
)

target_link_libraries(SampleModule PRIVATE
    Mcaster1SDK
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(SampleModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(SampleModule PRIVATE /EHsc)
    set_target_properties(SampleModule PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()

# Deploy SampleModule.dll → plugins/modules/ next to the main executable
add_custom_command(TARGET SampleModule POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:SampleModule>/plugins/modules"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SampleModule>"
        "$<TARGET_FILE_DIR:SampleModule>/plugins/modules/$<TARGET_FILE_NAME:SampleModule>"
    COMMENT "Deploying SampleModule.dll to plugins/modules/"
)
