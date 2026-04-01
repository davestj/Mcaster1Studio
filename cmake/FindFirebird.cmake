# FindFirebird.cmake — Locate the Firebird SQL client library (fbclient)
#
# Sets:
#   Firebird_FOUND          - TRUE if Firebird was found
#   Firebird_INCLUDE_DIRS   - Include directories (containing ibase.h)
#   Firebird_LIBRARIES      - Library files
#   Firebird::fbclient      - Imported target for target_link_libraries()
#
# Search order:
#   1. FIREBIRD_ROOT environment variable
#   2. Standard 64-bit Program Files locations (Firebird 5 → 3)
#   3. WOW64 Program Files (32-bit, lower priority)
#
# Usage in CMakeLists.txt:
#   list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#   find_package(Firebird QUIET)
#   if(Firebird_FOUND)
#       target_link_libraries(MyTarget PRIVATE Firebird::fbclient)
#       target_compile_definitions(MyTarget PRIVATE M1_HAS_FIREBIRD)
#   endif()

# ── Header search ───────────────────────────────────────────────────────────
find_path(Firebird_INCLUDE_DIR
    NAMES ibase.h
    HINTS
        $ENV{FIREBIRD_ROOT}/include
    PATHS
        "$ENV{ProgramW6432}/Firebird/Firebird_5_0"
        "$ENV{ProgramW6432}/Firebird/Firebird_4_0"
        "$ENV{ProgramW6432}/Firebird/Firebird_3_0"
        "$ENV{ProgramFiles}/Firebird/Firebird_5_0"
        "$ENV{ProgramFiles}/Firebird/Firebird_4_0"
        "$ENV{ProgramFiles}/Firebird/Firebird_3_0"
        "C:/Program Files/Firebird/Firebird_5_0"
        "C:/Program Files/Firebird/Firebird_4_0"
        "C:/Program Files/Firebird/Firebird_3_0"
    PATH_SUFFIXES include
)

# ── Library search ──────────────────────────────────────────────────────────
find_library(Firebird_LIBRARY
    NAMES fbclient_ms fbclient
    HINTS
        $ENV{FIREBIRD_ROOT}/lib
    PATHS
        "$ENV{ProgramW6432}/Firebird/Firebird_5_0"
        "$ENV{ProgramW6432}/Firebird/Firebird_4_0"
        "$ENV{ProgramW6432}/Firebird/Firebird_3_0"
        "$ENV{ProgramFiles}/Firebird/Firebird_5_0"
        "$ENV{ProgramFiles}/Firebird/Firebird_4_0"
        "$ENV{ProgramFiles}/Firebird/Firebird_3_0"
        "C:/Program Files/Firebird/Firebird_5_0"
        "C:/Program Files/Firebird/Firebird_4_0"
        "C:/Program Files/Firebird/Firebird_3_0"
    PATH_SUFFIXES lib
)

# ── Standard CMake package handling ─────────────────────────────────────────
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Firebird
    REQUIRED_VARS Firebird_LIBRARY Firebird_INCLUDE_DIR
)

# ── Imported target ─────────────────────────────────────────────────────────
if(Firebird_FOUND AND NOT TARGET Firebird::fbclient)
    add_library(Firebird::fbclient UNKNOWN IMPORTED)
    set_target_properties(Firebird::fbclient PROPERTIES
        IMPORTED_LOCATION "${Firebird_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Firebird_INCLUDE_DIR}"
    )
    set(Firebird_INCLUDE_DIRS "${Firebird_INCLUDE_DIR}")
    set(Firebird_LIBRARIES    "${Firebird_LIBRARY}")
endif()

mark_as_advanced(Firebird_INCLUDE_DIR Firebird_LIBRARY)
