# Mcaster1Studio Plugin SDK — Developer Guide

## Overview

Mcaster1Studio supports third-party plugins via a C ABI DLL interface.
Plugins are compiled as shared libraries (`.dll` on Windows, `.so` on Linux,
`.dylib` on macOS) and discovered at application startup.

**Plugin types:**
- **Module** — A surface panel (implements `M1::IModule`). Examples: VU meter, media player, chat panel.
- **Effect** — A DSP rack unit (implements `M1::IEffectUnit`). Examples: EQ, compressor, reverb.

## Quick Start

1. Copy the `SDK/` directory (or add it as a CMake subdirectory).
2. Create your plugin source files (see `SDK/examples/SampleModule/`).
3. Link against `Mcaster1SDK` (header-only) and `Qt6::Widgets`.
4. Export the required C ABI functions.
5. Build and place the DLL in `plugins/modules/` or `plugins/effects/`.

## Directory Structure

```
Mcaster1Studio/
  build/bin/Debug/
    Mcaster1Studio.exe
    plugins/
      modules/       <-- Module plugin DLLs go here
        SampleModule.dll
        MyPlugin.dll
      effects/       <-- Effect plugin DLLs go here
        SampleEffect.dll
```

## Required C ABI Exports

Every plugin DLL must export `mcaster1_plugin_info()`. Module plugins also export
`mcaster1_create_module()` and `mcaster1_destroy_module()`. Effect plugins export
`mcaster1_create_effect()` and `mcaster1_destroy_effect()`.

### Plugin Info (required for all plugins)

```cpp
#include "IPlugin.h"

static Mcaster1PluginInfo s_info = {
    MCASTER1_PLUGIN_API_VERSION,   // Must match host (currently 1)
    "com.yourcompany.myplugin",    // Reverse-DNS plugin ID
    "My Plugin",                   // Display name in UI
    "1.0.0",                       // Version string
    "*",                           // Surface hints: "*" = all, or "dj,alpha"
    "module",                      // "module" or "effect"
    "Your Company",                // Vendor name
    "A brief description"          // One-line description
};

extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_plugin_info() {
        return &s_info;
    }
}
```

### Module Plugin Exports

```cpp
extern "C" {
    MCASTER1_PLUGIN_API IModule* mcaster1_create_module(IModuleHost* host) {
        // host provides logging, version info, audio params (may be nullptr)
        return new MyModule();
    }

    MCASTER1_PLUGIN_API void mcaster1_destroy_module(IModule* m) {
        delete m;
    }
}
```

### Effect Plugin Exports

```cpp
extern "C" {
    MCASTER1_PLUGIN_API IEffectUnit* mcaster1_create_effect(void* host) {
        return new MyEffect();
    }

    MCASTER1_PLUGIN_API void mcaster1_destroy_effect(IEffectUnit* e) {
        delete e;
    }
}
```

## IModule Interface

```cpp
class IModule : public QObject {
public:
    // Identity (pure virtual)
    virtual QString moduleId()    const = 0;  // "com.yourcompany.myplugin"
    virtual QString displayName() const = 0;  // "My Plugin"
    virtual QString version()     const;      // default "1.0.0"
    virtual QString vendor()      const;      // default "Mcaster1"
    virtual QSize   preferredSize()     const; // default {320, 240}
    virtual QSize   minimumModuleSize() const; // default {200, 120}

    // Lifecycle
    virtual void initialize();       // Called after construction, before createWidget()
    virtual void shutdown();         // Called before destruction

    // UI (pure virtual)
    virtual QWidget* createWidget(QWidget* parent) = 0;

    // Audio (real-time thread - NO Qt, NO alloc, NO mutex)
    virtual void onAudioBlock(AudioBuffer& in, AudioBuffer& out);

    // Events (Qt thread - safe to update UI)
    virtual void onMetadataUpdate(const IcyMetadata& meta);
    virtual void onMediaLoaded(const MediaItem& item, int deckIndex);

    // State persistence (pure virtual)
    virtual void saveState(QSettings& s) = 0;
    virtual void loadState(QSettings& s) = 0;

signals:
    void moduleError(const QString& msg);
    void statusChanged(const QString& status);
    void metadataReady(const M1::IcyMetadata& meta);
    void requestLoadMedia(const M1::MediaItem& item, int deckIndex);
};
```

## IEffectUnit Interface

```cpp
class IEffectUnit {
public:
    // Identity (pure virtual)
    virtual QString effectId()    const = 0;  // "com.yourcompany.fx.myfx"
    virtual QString displayName() const = 0;
    virtual QString vendor()      const;      // default "Mcaster1"
    virtual int     rackUnits()   const;      // default 1 (height in rack U)

    // Lifecycle (pure virtual)
    virtual void initialize(double sampleRate, int maxFramesPerBuffer) = 0;
    virtual void reset() = 0;

    // DSP (real-time thread - NO Qt, NO alloc)
    virtual void process(AudioBuffer& inOut) = 0;

    // Bypass
    virtual bool isBypassed() const;
    virtual void setBypassed(bool b);

    // UI (pure virtual)
    virtual QWidget* createPanel(QWidget* parent) = 0;

    // State (pure virtual)
    virtual void saveState(QSettings& s) = 0;
    virtual void loadState(QSettings& s) = 0;
};
```

## AudioBuffer

```cpp
struct AudioBuffer {
    float*   data;         // Interleaved PCM: [L0, R0, L1, R1, ...]
    int      channels;     // 1=mono, 2=stereo
    int      frames;       // Samples per channel
    double   sampleRate;   // Hz (typically 48000)
    bool     isValid;

    int   totalSamples() const;       // channels * frames
    float& at(int frame, int ch);     // Sample access
    void  silence();                  // Zero-fill
};
```

## IModuleHost Interface

The host interface provides services to plugins at creation time:

```cpp
class IModuleHost {
public:
    virtual void    log(int level, const QString& msg) = 0;  // 0=info, 1=warn, 2=error
    virtual QString hostName()        const = 0;  // "Mcaster1Studio"
    virtual QString hostVersion()     const = 0;  // "0.1.0"
    virtual int     sampleRate()      const = 0;  // e.g. 48000
    virtual int     framesPerBuffer() const = 0;  // e.g. 512
    virtual QString pluginsDir()      const = 0;  // Path to plugins/ directory
};
```

## Surface Hints

The `surface_hints` field controls which surfaces your plugin appears on:
- `"*"` — Available on all surfaces
- `"alpha,beta"` — Only Alpha and Beta surfaces
- `"dj"` — Only DJ surface

Available surface names: `alpha`, `beta`, `company`, `dj`, `entertainment`,
`social`, `podcast`, `church`, `custom`.

## CMake Build Template

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyPlugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Add Mcaster1Studio SDK
add_subdirectory(/path/to/Mcaster1Studio/SDK "${CMAKE_BINARY_DIR}/sdk")

add_library(MyPlugin SHARED MyPlugin.h MyPlugin.cpp)

target_link_libraries(MyPlugin PRIVATE
    Mcaster1SDK       # Header-only SDK interface
    Qt6::Core Qt6::Gui Qt6::Widgets
)

target_compile_definitions(MyPlugin PRIVATE MCASTER1PLUGIN_EXPORTS)
```

## Real-Time Audio Rules

When implementing `onAudioBlock()` (modules) or `process()` (effects):

- **NO Qt API calls** — No signals, no slots, no QObject methods
- **NO memory allocation** — No `new`, no `std::vector::push_back`, no `QString`
- **NO blocking** — No mutex lock, no file I/O, no network
- **Use `std::atomic<>`** for data shared with the Qt thread
- **Use `QTimer`** in the Qt thread to poll atomics and update UI

## Inter-Module Communication

Plugins communicate via the `EventBus` singleton:

```cpp
#include "ModuleEvents.h"

// Listen for now-playing changes
connect(&M1::EventBus::instance(), &M1::EventBus::nowPlayingChanged,
        this, [](const M1::IcyMetadata& meta, int deck) {
    // Update UI with new track info
});

// Emit a log message
M1::EventBus::instance().logMessage("My plugin started", 0);
```

## Examples

- `SDK/examples/SampleModule/` — Minimal module plugin (Hello World)
- `SDK/examples/SampleEffect/` — Minimal effect plugin (simple gain control)

## API Version

Current API version: **1** (`MCASTER1_PLUGIN_API_VERSION`)

The host checks `api_version` in `Mcaster1PluginInfo` against the compiled-in
constant. Plugins with mismatched versions are skipped with a warning.
