cmake_minimum_required(VERSION 3.28)

# ─── Mcaster1SDK — interface-only target ─────────────────────────────────────
#
# Link against this target to get access to all Mcaster1Studio public headers.
# No compiled sources; header-only interface.
#
# Usage (in your plugin's CMakeLists.txt):
#   find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
#   add_subdirectory(/path/to/Mcaster1Studio/SDK "${CMAKE_BINARY_DIR}/sdk")
#   target_link_libraries(MyPlugin PRIVATE Mcaster1SDK Qt6::Widgets)

add_library(Mcaster1SDK INTERFACE)

target_include_directories(Mcaster1SDK INTERFACE
    ${CMAKE_SOURCE_DIR}/Core/include
)

# ─── Example: SampleModule ───────────────────────────────────────────────────
# A minimal working plugin that compiles using only SDK headers.
# Demonstrates the C ABI factory pattern and IModule interface.
add_subdirectory(examples/SampleModule)
add_subdirectory(examples/SampleEffect)
