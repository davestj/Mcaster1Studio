cmake_minimum_required(VERSION 3.28)

add_library(MetadataModule STATIC
    MetadataModule.h
    MetadataModule.cpp
    Icy22EditorWidget.h
    Icy22EditorWidget.cpp
    MetadataPusher.h
    MetadataPusher.cpp
)

target_include_directories(MetadataModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(MetadataModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

target_compile_definitions(MetadataModule PRIVATE MCASTER1PLUGIN_EXPORTS)

if(MSVC)
    target_compile_options(MetadataModule PRIVATE /EHsc)
endif()
