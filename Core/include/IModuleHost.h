#pragma once
#include <QString>

/// Host interface passed to plugins at creation time.
///
/// Provides services that the host application offers to plugins:
///   - Logging
///   - Module/host version queries
///   - Audio engine parameters
///
/// This is a C++-safe class (not extern "C") — safe because plugin DLLs
/// receive a pointer to the host's implementation, never constructing it.
class IModuleHost {
public:
    virtual ~IModuleHost() = default;

    /// Log a message to the host application log.
    /// level: 0=info, 1=warning, 2=error
    virtual void log(int level, const QString& msg) = 0;

    /// Returns the host application name ("Mcaster1Studio").
    virtual QString hostName() const = 0;

    /// Returns the host application version string.
    virtual QString hostVersion() const = 0;

    /// Returns the current audio sample rate (e.g. 48000).
    virtual int sampleRate() const = 0;

    /// Returns the current audio buffer size in frames (e.g. 512).
    virtual int framesPerBuffer() const = 0;

    /// Returns the path to the plugins directory.
    virtual QString pluginsDir() const = 0;
};
