#pragma once

/// Mcaster1Studio Plugin ABI
///
/// Third-party modules are DLLs that export these three C functions.
/// The C ABI ensures DLL boundary safety across compiler versions.
///
/// Plugin types:
///   "module"  — Surface module (implements IModule)
///   "effect"  — Effects rack unit (implements IEffectUnit)
///
/// Surface hints (comma-separated, or "*" for all):
///   alpha, beta, company, dj, entertainment, social, podcast, church, custom

#define MCASTER1_PLUGIN_API_VERSION 1

#ifdef _WIN32
  #ifdef MCASTER1PLUGIN_EXPORTS
    #define MCASTER1_PLUGIN_API __declspec(dllexport)
  #else
    #define MCASTER1_PLUGIN_API __declspec(dllimport)
  #endif
#else
  #define MCASTER1_PLUGIN_API __attribute__((visibility("default")))
#endif

// Forward declarations
// IModule and IEffectUnit live in the M1:: namespace.
// For C ABI exports, use the unqualified names within extern "C" blocks —
// they resolve to the M1:: types via using declarations in your .cpp.
namespace M1 { class IModule; class IEffectUnit; }
class IModuleHost;  // Host interface — global scope for C ABI (see IModuleHost.h)

// Convenience aliases for use in C ABI extern "C" export functions
using IModule     = M1::IModule;
using IEffectUnit = M1::IEffectUnit;

/// Static plugin metadata — returned by mcaster1_plugin_info().
/// All pointers must remain valid for the lifetime of the DLL.
struct Mcaster1PluginInfo {
    int         api_version;    ///< Must equal MCASTER1_PLUGIN_API_VERSION
    const char* plugin_id;      ///< Reverse-DNS ID: "com.mcaster1.deck"
    const char* display_name;   ///< "Deck Player"
    const char* version;        ///< "1.0.0"
    const char* surface_hints;  ///< "dj,alpha,beta" or "*"
    const char* plugin_type;    ///< "module" or "effect"
    const char* vendor;         ///< "Mcaster1" or your company name
    const char* description;    ///< One-line description
};

extern "C" {
    /// Returns plugin metadata. Must not return nullptr.
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_plugin_info();

    /// Create a module instance. Called once per surface slot load.
    /// host provides logging and engine info (see IModuleHost.h); may be nullptr.
    MCASTER1_PLUGIN_API IModule* mcaster1_create_module(IModuleHost* host);

    /// Destroy module instance created by mcaster1_create_module().
    MCASTER1_PLUGIN_API void     mcaster1_destroy_module(IModule* module);

    /// (Effect plugins only) Create an effect unit instance.
    MCASTER1_PLUGIN_API IEffectUnit* mcaster1_create_effect(void* host);

    /// (Effect plugins only) Destroy an effect unit instance.
    MCASTER1_PLUGIN_API void         mcaster1_destroy_effect(IEffectUnit* effect);
}
