cmake_minimum_required(VERSION 3.28)

set(CORE_HEADERS
    include/Mcaster1SDK.h
    include/IModule.h
    include/ISurface.h
    include/IAudioEngine.h
    include/IEffectUnit.h
    include/IPlugin.h
    include/IModuleHost.h
    include/IDatabase.h
    include/IDbAwareModule.h
    include/ModuleRegistry.h
    include/AudioBuffer.h
    include/IcyMetadata.h
    include/MediaItem.h
    include/ModuleEvents.h
    include/SurfaceConfig.h
    include/DbServerEntry.h
    include/SurfaceDbContext.h
    include/SqlDialect.h
    include/DatabaseFactory.h
    include/MonitorManager.h
    include/IThreadPoolAware.h
    include/SurfaceThreadPool.h
    include/ThreadPoolManager.h
    include/PooledRunnable.h
)

set(CORE_SOURCES
    src/ModuleRegistry.cpp
    src/SurfaceManager.cpp
    src/EventBus.cpp
    src/IcyMetadata.cpp
    src/SurfaceConfig.cpp
    src/DbServerRegistry.cpp
    src/SurfaceDbContext.cpp
    src/SqlDialect.cpp
    src/DatabaseFactory.cpp
    src/MonitorManager.cpp
    src/SurfaceThreadPool.cpp
    src/ThreadPoolManager.cpp
)

add_library(Mcaster1Core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(Mcaster1Core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(Mcaster1Core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Concurrent
)

# Windows: DXGI for GPU enumeration, MediaFoundation for capture device enumeration
if(WIN32)
    target_link_libraries(Mcaster1Core PRIVATE dxgi mf mfplat)
endif()

target_compile_definitions(Mcaster1Core
    PRIVATE MCASTER1CORE_LIBRARY
    PUBLIC  MCASTER1CORE_SHARED
)

set_target_properties(Mcaster1Core PROPERTIES
    OUTPUT_NAME "Mcaster1Core"
    VERSION     "${PROJECT_VERSION}"
    SOVERSION   "0"
)
