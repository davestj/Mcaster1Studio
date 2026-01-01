cmake_minimum_required(VERSION 3.28)

set(QUEUE_SOURCES
    QueueModule.cpp
    QueueWidget.cpp
)
set(QUEUE_HEADERS
    QueueModule.h
    QueueWidget.h
)

add_library(QueueModule STATIC ${QUEUE_SOURCES} ${QUEUE_HEADERS})

target_include_directories(QueueModule PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/Modules/DeckModule
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(QueueModule
    PUBLIC
        Mcaster1Core
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        DeckModule
)

target_compile_definitions(QueueModule PRIVATE MCASTER1PLUGIN_EXPORTS)

set_target_properties(QueueModule PROPERTIES
    OUTPUT_NAME "QueueModule"
)
