#include "AudioDevice.h"
#include <portaudio.h>
#include <QDebug>
#include <cmath>

namespace M1 {

static AudioDeviceInfo buildInfo(int paIdx) {
    const PaDeviceInfo* d = Pa_GetDeviceInfo(paIdx);
    if (!d) return {};

    const PaHostApiInfo* api = Pa_GetHostApiInfo(d->hostApi);
    AudioDeviceInfo info;
    info.index              = paIdx;
    info.name               = QString::fromUtf8(d->name);
    info.hostApi            = api ? QString::fromUtf8(api->name) : "Unknown";
    info.maxInputChannels   = d->maxInputChannels;
    info.maxOutputChannels  = d->maxOutputChannels;
    info.defaultSampleRate  = d->defaultSampleRate;
    info.isAsio             = (api && api->type == paASIO);
    return info;
}

QList<AudioDeviceInfo> enumerateInputDevices() {
    QList<AudioDeviceInfo> list;
    const int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(i);
        if (d && d->maxInputChannels > 0) {
            auto info = buildInfo(i);
            info.isDefault = (i == Pa_GetDefaultInputDevice());
            list.append(info);
        }
    }
    return list;
}

QList<AudioDeviceInfo> enumerateOutputDevices() {
    QList<AudioDeviceInfo> list;
    const int count = Pa_GetDeviceCount();
    for (int i = 0; i < count; ++i) {
        const PaDeviceInfo* d = Pa_GetDeviceInfo(i);
        if (d && d->maxOutputChannels > 0) {
            auto info = buildInfo(i);
            info.isDefault = (i == Pa_GetDefaultOutputDevice());
            list.append(info);
        }
    }
    return list;
}

AudioDeviceInfo getDefaultInputDevice() {
    int idx = Pa_GetDefaultInputDevice();
    if (idx == paNoDevice) return {};
    auto info = buildInfo(idx);
    info.isDefault = true;
    return info;
}

AudioDeviceInfo getDefaultOutputDevice() {
    int idx = Pa_GetDefaultOutputDevice();
    if (idx == paNoDevice) return {};
    auto info = buildInfo(idx);
    info.isDefault = true;
    return info;
}

AudioDeviceInfo getDeviceInfo(int paDeviceIndex) {
    return buildInfo(paDeviceIndex);
}

QString hostApiName(int paHostApiIndex) {
    const PaHostApiInfo* api = Pa_GetHostApiInfo(paHostApiIndex);
    return api ? QString::fromUtf8(api->name) : "Unknown";
}

bool isAsioDevice(int paDeviceIndex) {
    const PaDeviceInfo* d = Pa_GetDeviceInfo(paDeviceIndex);
    if (!d) return false;
    const PaHostApiInfo* api = Pa_GetHostApiInfo(d->hostApi);
    return api && api->type == paASIO;
}

} // namespace M1
