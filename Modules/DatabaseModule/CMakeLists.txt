add_library(DatabaseModule STATIC
    DatabaseModule.h  DatabaseModule.cpp
    DatabaseWidget.h  DatabaseWidget.cpp
)

target_include_directories(DatabaseModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
    ${CMAKE_SOURCE_DIR}/UI
)

target_link_libraries(DatabaseModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_compile_definitions(DatabaseModule PRIVATE MCASTER1PLUGIN_EXPORTS)
