#pragma once
#include "IPlugin.h"
#include <QList>
#include <QPair>
#include <QString>

namespace M1 {

/// Descriptor for a loaded plugin (either built-in or discovered DLL).
struct PluginDescriptor {
    Mcaster1PluginInfo info = {};
    QString            filePath;      ///< DLL path (empty for built-in)
    void*              library = nullptr;  ///< QLibrary* (nullptr for built-in)
};

/// Initialize the plugin registry — scans plugins/modules/ and plugins/effects/
/// for third-party DLLs exporting mcaster1_plugin_info().
void initModuleRegistry(const QString& pluginsBaseDir);

/// Unload all plugin DLLs. Call on app shutdown.
void shutdownModuleRegistry();

/// Returns all discovered external plugins (does NOT include built-in modules).
const QList<PluginDescriptor>& loadedPlugins();

/// Returns the canonical list of built-in modules for Add Module menus.
/// Includes all built-in modules + any discovered "module" plugins.
QList<QPair<QString, QString>> availableModules();

/// Returns discovered "effect" plugins only.
QList<QPair<QString, QString>> availableEffects();

/// Create a module instance from a discovered plugin DLL.
/// Returns nullptr if the plugin is not found or creation fails.
class IModule;
IModule* createPluginModule(const QString& pluginId);

/// Create an effect unit instance from a discovered plugin DLL.
class IEffectUnit;
IEffectUnit* createPluginEffect(const QString& pluginId);

/// Destroy a module created by createPluginModule().
void destroyPluginModule(IModule* m, const QString& pluginId);

/// Destroy an effect created by createPluginEffect().
void destroyPluginEffect(IEffectUnit* e, const QString& pluginId);

} // namespace M1
