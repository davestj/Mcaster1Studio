#include "ModuleRegistry.h"
#include "IModule.h"
#include "IEffectUnit.h"
#include "IModuleHost.h"
#include <QDir>
#include <QLibrary>
#include <QDebug>
#include <QFileInfo>

namespace M1 {

// ─── Internal state ─────────────────────────────────────────────────────────

static QList<PluginDescriptor> s_plugins;

// ─── Built-in module catalog (canonical list for Add Module menus) ───────────

static const QList<QPair<QString, QString>> s_builtinModules = {
    {"com.mcaster1.vumeter",  "VU Meter"},
    {"com.mcaster1.deck.a",   "Deck A"},
    {"com.mcaster1.deck.b",   "Deck B"},
    {"com.mcaster1.deck",     "Deck (Combined A+B)"},
    {"com.mcaster1.library",  "Media Library"},
    {"com.mcaster1.encoder",  "Encoder"},
    {"com.mcaster1.effects",  "Effects Rack"},
    {"com.mcaster1.metadata", "Metadata"},
    {"com.mcaster1.playlist", "Playlist / AutoDJ"},
    {"com.mcaster1.queue",    "Queue"},
    {"com.mcaster1.ptt",      "Push-to-Talk"},
    {"com.mcaster1.podcast",  "Podcast"},
    {"com.mcaster1.video",    "Video"},
    {"com.mcaster1.monitor",  "Stream Monitor"},
    {"com.mcaster1.clock",    "Clock"},
    {"com.mcaster1.cartwall",    "Cart Wall"},
    {"com.mcaster1.crossfader",  "Crossfader"},
    {"com.mcaster1.health",               "System Health"},
    {"com.mcaster1.church.timerclock",     "Timer / Clock (Church)"},
    {"com.mcaster1.church.graphics",       "Graphics Engine (Church)"},
    {"com.mcaster1.church.lyrics",         "Lyrics Caster (Church)"},
    {"com.mcaster1.church.scripture",      "Scripture Caster (Church)"},
    {"com.mcaster1.church.announce",       "Announce Caster (Church)"},
    {"com.mcaster1.church.teleprompt",     "TelePrompter (Church)"},
    {"com.mcaster1.church.mediacaster",    "Media Caster (Church)"},
    {"com.mcaster1.church.stagemon",       "Stage Monitor (Church)"},
    {"com.mcaster1.church.audiomix",       "Audio Mixer (Church)"},
    {"com.mcaster1.church.transcriberec",  "Transcription Recorder (Church)"},
    {"com.mcaster1.church.switchcaster",   "Production Switcher (Church)"},
    {"com.mcaster1.church.servicerunner",  "Service Runner (Church)"},
    {"com.mcaster1.podcast.mixer",         "Podcast Mixer"},
    {"com.mcaster1.podcast.ptt",           "Podcast PTT"},
    {"com.mcaster1.podcast.recorder",      "Podcast Recorder"},
    {"com.mcaster1.podcast.soundboard",    "Podcast Soundboard"},
    {"com.mcaster1.podcast.fx",            "Podcast Effects"},
    {"com.mcaster1.podcast.editor",        "Podcast Editor"},
    {"com.mcaster1.podcast.encode",        "Podcast Encoder"},
    {"com.mcaster1.podcast.transcribe",    "Podcast Transcription"},
    {"com.mcaster1.podcast.shownotes",     "Show Notes"},
    {"com.mcaster1.podcast.rss",           "RSS Feed"},
    {"com.mcaster1.podcast.publisher",     "Publisher"},
    {"com.mcaster1.podcast.analytics",     "Podcast Analytics"},
    {"com.mcaster1.podcast.remote",        "Remote Guests"},
};

// ─── DLL scanning ───────────────────────────────────────────────────────────

static void scanPluginDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");  // create plugins/modules/ or plugins/effects/ if missing
        return;
    }

    const QStringList dlls = dir.entryList({"*.dll", "*.so", "*.dylib"}, QDir::Files);
    for (const QString& fn : dlls) {
        const QString fullPath = dir.absoluteFilePath(fn);
        auto* lib = new QLibrary(fullPath);
        if (!lib->load()) {
            qWarning() << "[ModuleRegistry] Failed to load plugin:" << fullPath
                       << "-" << lib->errorString();
            delete lib;
            continue;
        }

        using InfoFn = Mcaster1PluginInfo*(*)();
        auto infoFn = reinterpret_cast<InfoFn>(lib->resolve("mcaster1_plugin_info"));
        if (!infoFn) {
            qWarning() << "[ModuleRegistry] Not an M1 plugin (missing mcaster1_plugin_info):" << fullPath;
            lib->unload();
            delete lib;
            continue;
        }

        Mcaster1PluginInfo* info = infoFn();
        if (!info || info->api_version != MCASTER1_PLUGIN_API_VERSION) {
            qWarning() << "[ModuleRegistry] Plugin API version mismatch:" << fullPath
                       << "expected" << MCASTER1_PLUGIN_API_VERSION
                       << "got" << (info ? info->api_version : -1);
            lib->unload();
            delete lib;
            continue;
        }

        qInfo() << "[ModuleRegistry] Loaded plugin:" << info->display_name
                << "v" << info->version << "(" << info->plugin_type << ")"
                << "from" << QFileInfo(fullPath).fileName();

        PluginDescriptor desc;
        desc.info     = *info;
        desc.filePath = fullPath;
        desc.library  = lib;
        s_plugins.append(desc);
    }
}

// ─── Public API ─────────────────────────────────────────────────────────────

void initModuleRegistry(const QString& pluginsBaseDir) {
    scanPluginDirectory(pluginsBaseDir + "/modules");
    scanPluginDirectory(pluginsBaseDir + "/effects");

    const int modCount = std::count_if(s_plugins.begin(), s_plugins.end(),
        [](const PluginDescriptor& p) {
            return QString(p.info.plugin_type) == "module";
        });
    const int fxCount = s_plugins.size() - modCount;

    qInfo() << "[ModuleRegistry]" << s_plugins.size() << "external plugin(s) loaded"
            << "(" << modCount << "modules," << fxCount << "effects).";
}

void shutdownModuleRegistry() {
    for (auto& p : s_plugins) {
        if (auto* lib = static_cast<QLibrary*>(p.library)) {
            lib->unload();
            delete lib;
        }
    }
    s_plugins.clear();
}

const QList<PluginDescriptor>& loadedPlugins() {
    return s_plugins;
}

QList<QPair<QString, QString>> availableModules() {
    // Start with built-in modules
    auto result = s_builtinModules;

    // Append discovered module plugins
    for (const auto& p : s_plugins) {
        if (QString(p.info.plugin_type) == "module") {
            const QString id   = QString(p.info.plugin_id);
            const QString name = QString(p.info.display_name);
            // Don't add duplicates (e.g. if a built-in ID matches a plugin)
            bool found = false;
            for (const auto& [builtinId, _] : result) {
                if (builtinId == id) { found = true; break; }
            }
            if (!found)
                result.append({id, name});
        }
    }

    return result;
}

QList<QPair<QString, QString>> availableEffects() {
    QList<QPair<QString, QString>> result;
    for (const auto& p : s_plugins) {
        if (QString(p.info.plugin_type) == "effect") {
            result.append({QString(p.info.plugin_id), QString(p.info.display_name)});
        }
    }
    return result;
}

IModule* createPluginModule(const QString& pluginId) {
    for (const auto& p : s_plugins) {
        if (QString(p.info.plugin_id) == pluginId && QString(p.info.plugin_type) == "module") {
            auto* lib = static_cast<QLibrary*>(p.library);
            if (!lib) return nullptr;

            using CreateFn = IModule*(*)(IModuleHost*);
            auto createFn = reinterpret_cast<CreateFn>(lib->resolve("mcaster1_create_module"));
            if (!createFn) {
                qWarning() << "[ModuleRegistry] Plugin" << pluginId
                           << "missing mcaster1_create_module()";
                return nullptr;
            }

            qInfo() << "[ModuleRegistry] Creating module:" << pluginId;
            return createFn(nullptr);  // host will be set later
        }
    }
    return nullptr;
}

IEffectUnit* createPluginEffect(const QString& pluginId) {
    for (const auto& p : s_plugins) {
        if (QString(p.info.plugin_id) == pluginId && QString(p.info.plugin_type) == "effect") {
            auto* lib = static_cast<QLibrary*>(p.library);
            if (!lib) return nullptr;

            using CreateFn = IEffectUnit*(*)(void*);
            auto createFn = reinterpret_cast<CreateFn>(lib->resolve("mcaster1_create_effect"));
            if (!createFn) {
                qWarning() << "[ModuleRegistry] Plugin" << pluginId
                           << "missing mcaster1_create_effect()";
                return nullptr;
            }

            qInfo() << "[ModuleRegistry] Creating effect:" << pluginId;
            return createFn(nullptr);
        }
    }
    return nullptr;
}

void destroyPluginModule(IModule* m, const QString& pluginId) {
    if (!m) return;
    for (const auto& p : s_plugins) {
        if (QString(p.info.plugin_id) == pluginId) {
            auto* lib = static_cast<QLibrary*>(p.library);
            if (!lib) break;

            using DestroyFn = void(*)(IModule*);
            auto destroyFn = reinterpret_cast<DestroyFn>(lib->resolve("mcaster1_destroy_module"));
            if (destroyFn) {
                destroyFn(m);
                return;
            }
            break;
        }
    }
    // Fallback — if DLL doesn't provide destroy, just delete
    delete m;
}

void destroyPluginEffect(IEffectUnit* e, const QString& pluginId) {
    if (!e) return;
    for (const auto& p : s_plugins) {
        if (QString(p.info.plugin_id) == pluginId) {
            auto* lib = static_cast<QLibrary*>(p.library);
            if (!lib) break;

            using DestroyFn = void(*)(IEffectUnit*);
            auto destroyFn = reinterpret_cast<DestroyFn>(lib->resolve("mcaster1_destroy_effect"));
            if (destroyFn) {
                destroyFn(e);
                return;
            }
            break;
        }
    }
    delete e;
}

} // namespace M1
