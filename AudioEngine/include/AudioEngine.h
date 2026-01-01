#pragma once
#include <QObject>
#include <QTimer>
#include <atomic>
#include <mutex>
#include <vector>
#include "IAudioEngine.h"

// Forward-declare PortAudio types so consumers don't need portaudio.h.
// portaudio.h is only included in AudioEngine.cpp (implementation).
typedef void            PaStream;
typedef unsigned long   PaStreamCallbackFlags;
struct PaStreamCallbackTimeInfo;

namespace M1 {

/// Concrete PortAudio-backed audio engine.
/// Supports ASIO, WASAPI, MME, and DirectSound on Windows.
/// ASIO requires portaudio built with ASIO SDK support (vcpkg portaudio[asio]).
class AudioEngine : public QObject, public IAudioEngine {
    Q_OBJECT

public:
    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    // IAudioEngine interface
    bool initialize() override;
    void shutdown()   override;

    QList<AudioDeviceInfo> inputDevices()  const override;
    QList<AudioDeviceInfo> outputDevices() const override;
    AudioDeviceInfo        defaultInputDevice()  const override;
    AudioDeviceInfo        defaultOutputDevice() const override;

    bool openStream(int inputDeviceIndex,
                    int outputDeviceIndex,
                    int sampleRate      = 48000,
                    int framesPerBuffer = 512) override;
    bool startStream() override;
    void stopStream()  override;
    bool isRunning()   const override;

    // CUE output stream (headphone monitor on a separate device)
    bool openCueStream(int cueDeviceIndex) override;
    void stopCueStream() override;
    bool isCueRunning() const override;
    void writeCue(const float* data, int frames) override;

    float inputLevelL()  const override;
    float inputLevelR()  const override;
    float outputLevelL() const override;
    float outputLevelR() const override;

    void setAudioCallback(AudioCallback cb) override;

    AudioEngineState state()           const override;
    QString          lastError()        const override;
    int              sampleRate()       const override;
    int              framesPerBuffer()  const override;

signals:
    /// Emitted from the Qt thread (via level poll timer) with current peak levels
    void levelsUpdated(float inL, float inR, float outL, float outR);

    /// Emitted when engine state changes
    void stateChanged(M1::AudioEngineState newState);

private:
    // PortAudio callback (static, called from PA thread)
    static int paCallback(const void* inputBuffer,
                          void*       outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void*       userData);

    void setState(AudioEngineState s);

    PaStream*         m_stream    = nullptr;  // PaStream defined in portaudio.h
    AudioEngineState  m_state     = AudioEngineState::Uninitialized;
    QString           m_lastError;
    int               m_sampleRate       = 48000;
    int               m_framesPerBuffer  = 512;
    int               m_inputDevIndex    = -1;
    int               m_outputDevIndex   = -1;

    AudioCallback     m_callback;
    std::mutex        m_callbackMutex;

    // Atomic peak levels (written in RT thread, read in Qt thread)
    std::atomic<float> m_inLevelL  {0.0f};
    std::atomic<float> m_inLevelR  {0.0f};
    std::atomic<float> m_outLevelL {0.0f};
    std::atomic<float> m_outLevelR {0.0f};

    // Level decay timer (Qt thread)
    QTimer* m_levelTimer = nullptr;

    // Internal buffer used in callback
    AudioBufferOwned m_inputBuf;
    AudioBufferOwned m_outputBuf;

    // ─── CUE output stream (headphone monitor) ───────────────────
    static int paCueCallback(const void* inputBuffer,
                              void*       outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void*       userData);

    PaStream* m_cueStream     = nullptr;
    int       m_cueDevIndex   = -1;

    // Lock-free SPSC ring buffer: main callback writes, CUE callback reads
    static constexpr int kCueRingCapacity = 8192; // frames (power of 2)
    std::vector<float>   m_cueRing;               // kCueRingCapacity * 2 floats
    std::atomic<int>     m_cueWrite{0};
    std::atomic<int>     m_cueRead{0};
};

} // namespace M1
