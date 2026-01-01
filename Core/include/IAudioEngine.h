#pragma once
#include <QString>
#include <QStringList>
#include <functional>
#include "AudioBuffer.h"

namespace M1 {

/// Audio device descriptor
struct AudioDeviceInfo {
    int     index       = -1;
    QString name;
    QString hostApi;    ///< "ASIO", "WASAPI", "MME", "DirectSound"
    int     maxInputChannels  = 0;
    int     maxOutputChannels = 0;
    double  defaultSampleRate = 48000.0;
    bool    isAsio      = false;
    bool    isDefault   = false;
};

/// Audio engine state
enum class AudioEngineState {
    Uninitialized,
    Initialized,
    Running,
    Error
};

/// Real-time audio callback signature.
/// Called from the PortAudio callback thread — MUST be real-time safe.
/// Do not call Qt APIs, allocate memory, or hold mutexes inside.
using AudioCallback = std::function<void(AudioBuffer& input, AudioBuffer& output)>;

/// Abstract interface to the Mcaster1Studio audio engine.
/// Concrete implementation: AudioEngine (AudioEngine/src/AudioEngine.cpp)
class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    // ------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------
    virtual bool initialize() = 0;
    virtual void shutdown()   = 0;

    // ------------------------------------------------------------------
    // Device enumeration
    // ------------------------------------------------------------------
    virtual QList<AudioDeviceInfo> inputDevices()  const = 0;
    virtual QList<AudioDeviceInfo> outputDevices() const = 0;
    virtual AudioDeviceInfo defaultInputDevice()   const = 0;
    virtual AudioDeviceInfo defaultOutputDevice()  const = 0;

    // ------------------------------------------------------------------
    // Stream control
    // ------------------------------------------------------------------
    virtual bool openStream(int inputDeviceIndex,
                            int outputDeviceIndex,
                            int sampleRate    = 48000,
                            int framesPerBuffer = 512) = 0;
    virtual bool startStream() = 0;
    virtual void stopStream()  = 0;
    virtual bool isRunning()   const = 0;

    // ------------------------------------------------------------------
    // Levels (thread-safe, updated each callback)
    // ------------------------------------------------------------------
    virtual float inputLevelL()  const = 0;  ///< Peak level [0..1]
    virtual float inputLevelR()  const = 0;
    virtual float outputLevelL() const = 0;
    virtual float outputLevelR() const = 0;

    // ------------------------------------------------------------------
    // CUE (headphone monitor) output — separate device
    // ------------------------------------------------------------------
    virtual bool openCueStream(int cueDeviceIndex) = 0;
    virtual void stopCueStream() = 0;
    virtual bool isCueRunning() const = 0;
    /// Write interleaved stereo frames to the CUE ring buffer (RT-safe).
    virtual void writeCue(const float* data, int frames) = 0;

    // ------------------------------------------------------------------
    // Callback registration
    // ------------------------------------------------------------------
    virtual void setAudioCallback(AudioCallback cb) = 0;

    // ------------------------------------------------------------------
    // State
    // ------------------------------------------------------------------
    virtual AudioEngineState state()    const = 0;
    virtual QString          lastError() const = 0;
    virtual int              sampleRate()     const = 0;
    virtual int              framesPerBuffer() const = 0;
};

} // namespace M1
