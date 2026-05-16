cmake_minimum_required(VERSION 3.28)

add_library(MediaLibraryModule STATIC
    DatabaseManager.cpp
    SqliteManager.cpp
    PostgresManager.cpp
    FirebirdManager.cpp
    MssqlManager.cpp
    ScanWorker.cpp
    LibraryModel.cpp
    LibraryWidget.cpp
    MusicBrainzLookup.cpp
    AlbumArtCache.cpp
    AiTrackIntel.cpp
    ArtistIntelDialog.cpp
    LibraryCategoryPanel.cpp
    ScanProgressDialog.cpp
    PersonaManager.cpp
    MediaLibraryModule.cpp
    DatabaseManager.h
    SqliteManager.h
    PostgresManager.h
    FirebirdManager.h
    MssqlManager.h
    ScanWorker.h
    LibraryModel.h
    LibraryWidget.h
    MusicBrainzLookup.h
    AlbumArtCache.h
    AiTrackIntel.h
    ArtistIntelDialog.h
    LibraryCategoryPanel.h
    ScanProgressDialog.h
    PersonaManager.h
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

# PostgreSQL (libpq)
find_package(PostgreSQL QUIET)
if(PostgreSQL_FOUND)
    target_link_libraries(MediaLibraryModule PUBLIC PostgreSQL::PostgreSQL)
    target_compile_definitions(MediaLibraryModule PRIVATE M1_HAS_POSTGRESQL)
    message(STATUS "[MediaLibrary] PostgreSQL found: ${PostgreSQL_VERSION_STRING}")
else()
    message(STATUS "[MediaLibrary] PostgreSQL not found — PG driver disabled")
endif()

# Firebird (fbclient)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(Firebird QUIET)
if(Firebird_FOUND)
    target_link_libraries(MediaLibraryModule PUBLIC Firebird::fbclient)
    target_compile_definitions(MediaLibraryModule PRIVATE M1_HAS_FIREBIRD)
    message(STATUS "[MediaLibrary] Firebird found: ${Firebird_LIBRARY}")
else()
    message(STATUS "[MediaLibrary] Firebird not found — Firebird driver disabled")
endif()

# SQL Server (ODBC — always available on Windows via odbc32.lib)
if(WIN32)
    target_link_libraries(MediaLibraryModule PUBLIC odbc32)
    target_compile_definitions(MediaLibraryModule PRIVATE M1_HAS_MSSQL)
    message(STATUS "[MediaLibrary] SQL Server (ODBC) enabled")
endif()

# Define MCASTER1PLUGIN_EXPORTS so MCASTER1_PLUGIN_API → dllexport, not dllimport
target_compile_definitions(MediaLibraryModule PRIVATE MCASTER1PLUGIN_EXPORTS)

# /EHa — MSVC async exception safety for TagLib on corrupt audio files
if(MSVC)
    target_compile_options(MediaLibraryModule PRIVATE /EHa)
endif()
