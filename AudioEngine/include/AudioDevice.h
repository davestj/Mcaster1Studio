#pragma once
#include "IAudioEngine.h"
#include <QList>

namespace M1 {

/// Utility functions for PortAudio device enumeration.
/// Called from AudioEngine::initialize() and device selection dialogs.
QList<AudioDeviceInfo> enumerateInputDevices();
QList<AudioDeviceInfo> enumerateOutputDevices();
AudioDeviceInfo        getDefaultInputDevice();
AudioDeviceInfo        getDefaultOutputDevice();
AudioDeviceInfo        getDeviceInfo(int paDeviceIndex);

/// Return human-readable PortAudio host API name
QString hostApiName(int paHostApiIndex);

/// True if the device supports ASIO
bool isAsioDevice(int paDeviceIndex);

} // namespace M1
