cmake_minimum_required(VERSION 3.28)

add_library(MediaLibraryModule STATIC
    DatabaseManager.cpp
    SqliteManager.cpp
    ScanWorker.cpp
    LibraryModel.cpp
    LibraryWidget.cpp
    MusicBrainzLookup.cpp
    MediaLibraryModule.cpp
    DatabaseManager.h
    SqliteManager.h
    ScanWorker.h
    LibraryModel.h
    LibraryWidget.h
    MusicBrainzLookup.h
    MediaLibraryModule.h
)

target_include_directories(MediaLibraryModule PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Core/include
)

target_link_libraries(MediaLibraryModule PUBLIC
    Mcaster1Core
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    TagLib::tag
    unofficial::sqlite3::sqlite3
)

# MySQL/MariaDB — use whichever vcpkg found
if(unofficial-libmariadb_FOUND)
    target_link_libraries(MediaLibraryModule PUBLIC unofficial::libmariadb)
elseif(unofficial-libmysql_FOUND)
    target_link_libraries(MediaLibraryModule PUBLIC unofficial::libmysql::libmysql)
endif()

# Define MCASTER1PLUGIN_EXPORTS so MCASTER1_PLUGIN_API → dllexport, not dllimport
target_compile_definitions(MediaLibraryModule PRIVATE MCASTER1PLUGIN_EXPORTS)

# /EHa — MSVC async exception safety for TagLib on corrupt audio files
if(MSVC)
    target_compile_options(MediaLibraryModule PRIVATE /EHa)
endif()
