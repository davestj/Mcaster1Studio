#pragma once
#include "IcyMetadata.h"
#include <QString>
#include <QObject>

/// MetadataPusher — static helper for fire-and-forget ICY metadata HTTP pushes.
///
/// All methods create a temporary QNetworkAccessManager bound to the given
/// parent QObject and issue async requests. Replies are handled inline via
/// lambdas; errors are logged with qWarning. The caller does not need to wait.
///
/// ICY 1.x: GET  /admin/metadata?mode=updinfo&song=<percent-encoded>
/// ICY 2.2: PUT  /admin/metadata  (all icy2-* headers, empty values omitted)
///
/// IMPORTANT: Never call these methods from the RT audio thread.
class MetadataPusher {
public:
    MetadataPusher() = delete;

    /// Push ICY 1.x StreamTitle= to /admin/metadata (GET).
    /// song is percent-encoded as "Artist - Title" internally.
    static void pushIcy1(const QString&  host,
                         int             port,
                         const QString&  mount,
                         const QString&  user,
                         const QString&  pass,
                         const QString&  artist,
                         const QString&  title,
                         QObject*        parent);

    /// Push full ICY 2.2 headers to /admin/metadata (PUT).
    /// Also embeds the ICY 1.x StreamTitle= header for backward compat.
    static void pushIcy2(const QString&        host,
                         int                   port,
                         const QString&        mount,
                         const QString&        user,
                         const QString&        pass,
                         const M1::IcyMetadata& meta,
                         QObject*              parent);
};
