#pragma once
#include <QString>
#include <QList>

/// Mcaster1AudioPipe — Virtual audio routing infrastructure scaffold.
///
/// Companion project: Mcaster1AudioPipe (coming soon)
/// Provides virtual loopback/pipe device integration akin to VB-Cable/BlackHole,
/// enabling independent per-surface audio routing and cross-surface mixing.
///
/// This header is a design scaffold only. Implementations will be provided by
/// the Mcaster1AudioPipe driver and Qt plugin when the companion project ships.

namespace M1 {
namespace AudioPipe {

    enum class PipeEndpointType {
        Loopback,   ///< Full-duplex loopback (like VB-Cable)
        Send,       ///< One-way send endpoint (source side)
        Return,     ///< One-way return endpoint (sink side)
        Sidechain   ///< Sidechain/monitor tap (non-destructive copy)
    };

    struct PipeEndpointInfo {
        QString          id;
        QString          name;
        PipeEndpointType type       = PipeEndpointType::Loopback;
        int              channels   = 2;
        int              sampleRate = 48000;
        bool             isConnected = false;
    };

    /// IAudioPipeClient — Interface for connecting to Mcaster1AudioPipe.
    ///
    /// Obtain a concrete implementation via:
    ///   QPluginLoader("Mcaster1AudioPipe_qt6.dll").instance()
    ///   → qobject_cast<IAudioPipeClient*>(instance)
    class IAudioPipeClient {
    public:
        virtual ~IAudioPipeClient() = default;

        /// Connect to a running Mcaster1AudioPipe server.
        /// @param serverAddress  Named pipe / IPC address (e.g. "//./pipe/m1audio")
        virtual bool connect(const QString& serverAddress) = 0;

        virtual void disconnect() = 0;
        virtual bool isConnected() const = 0;

        /// Enumerate available virtual endpoints on the connected server.
        virtual QList<PipeEndpointInfo> enumerateEndpoints() const = 0;

        /// Open a specific endpoint for audio I/O.
        virtual bool openEndpoint(const QString& endpointId) = 0;
        virtual void closeEndpoint() = 0;

        /// Endpoint currently open (empty string if none).
        virtual QString currentEndpointId() const = 0;
    };

} // namespace AudioPipe
} // namespace M1
