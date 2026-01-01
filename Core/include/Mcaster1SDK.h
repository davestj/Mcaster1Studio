#pragma once
/// Mcaster1Studio SDK — umbrella include for third-party module developers.
/// Include this single header to access all required interfaces.
///
/// SDK Version: 1 (MCASTER1_PLUGIN_API_VERSION)
/// Docs: https://mcaster1.com/studio/sdk/
///
/// Build your plugin as a DLL exporting the functions in IPlugin.h.
/// See SDK/examples/SampleModule/ for a complete starter project.

#include "IPlugin.h"
#include "IModule.h"
#include "IModuleHost.h"
#include "IEffectUnit.h"
#include "ISurface.h"
#include "IAudioEngine.h"
#include "AudioBuffer.h"
#include "IcyMetadata.h"
#include "MediaItem.h"
#include "ModuleEvents.h"
