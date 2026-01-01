#pragma once
#include <QString>
#include <QMap>

/// IcyPusher — sends ICY metadata updates to a running stream.
///
/// ICY 1.x: HTTP GET to /admin/metadata (Icecast2 / Shoutcast / DNAS)
/// ICY 2.2: HTTP PUT to /admin/metadata with icy2-* headers (DNAS only)
///
/// All methods are synchronous blocking calls — call from the encoder thread,
/// never from the RT audio thread or the UI thread.
class IcyPusher {
public:
    struct Config {
        QString host;
        int     port       = 8000;
        QString mount      = "/stream";
        QString adminUser  = "admin";
        QString adminPass  = "admin";
        bool    icy22      = false;   // true → also send ICY 2.2 headers (DNAS)
    };

    /// Send ICY 1.x metadata: StreamTitle = "artist - title".
    /// Returns true on HTTP 200, false otherwise.
    static bool pushIcy1(const Config& cfg,
                         const QString& artist,
                         const QString& title,
                         QString* errorOut = nullptr);

    /// Send ICY 2.2 extended metadata headers (DNAS target only).
    /// `fields` is a map of ICY 2.2 field names → values.
    static bool pushIcy2(const Config& cfg,
                         const QString& artist,
                         const QString& title,
                         const QMap<QString, QString>& fields = {},
                         QString* errorOut = nullptr);

private:
    /// Synchronous HTTP send/receive over a plain TCP socket.
    /// Returns the response body; sets *error on failure.
    static QByteArray httpRequest(const QString& host, int port,
                                  const QByteArray& requestBytes,
                                  QString* errorOut);

    static QByteArray basicAuth(const QString& user, const QString& pass);
};
