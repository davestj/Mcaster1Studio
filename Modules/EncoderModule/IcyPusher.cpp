#include "IcyPusher.h"
#include <QTcpSocket>
#include <QUrl>
#include <QDebug>

// ─── Helpers ─────────────────────────────────────────────────────────────────
QByteArray IcyPusher::basicAuth(const QString& user, const QString& pass) {
    return (user + ":" + pass).toUtf8().toBase64();
}

QByteArray IcyPusher::httpRequest(const QString& host, int port,
                                  const QByteArray& requestBytes,
                                  QString* errorOut)
{
    QTcpSocket sock;
    sock.connectToHost(host, static_cast<quint16>(port));
    if (!sock.waitForConnected(4000)) {
        if (errorOut) *errorOut = "connect timeout: " + host;
        return {};
    }
    sock.write(requestBytes);
    if (!sock.waitForBytesWritten(3000)) {
        if (errorOut) *errorOut = "write timeout";
        return {};
    }
    sock.waitForReadyRead(3000);
    QByteArray response = sock.readAll();
    sock.disconnectFromHost();
    return response;
}

// ─── ICY 1.x metadata push ────────────────────────────────────────────────────
bool IcyPusher::pushIcy1(const Config& cfg,
                         const QString& artist,
                         const QString& title,
                         QString* errorOut)
{
    // StreamTitle = "Artist - Title" URL-encoded
    const QString streamTitle = artist.isEmpty()
        ? title
        : artist + " - " + title;
    const QString encoded = QString::fromLatin1(
        QUrl::toPercentEncoding(streamTitle));

    const QByteArray auth = basicAuth(cfg.adminUser, cfg.adminPass);

    QByteArray req;
    req += "GET /admin/metadata?mount=";
    req += cfg.mount.toUtf8();
    req += "&mode=updinfo&song=";
    req += encoded.toUtf8();
    req += " HTTP/1.0\r\n";
    req += "Authorization: Basic ";
    req += auth;
    req += "\r\n";
    req += "User-Agent: Mcaster1Studio/1.0.0\r\n";
    req += "Connection: close\r\n\r\n";

    QString err;
    QByteArray resp = httpRequest(cfg.host, cfg.port, req, &err);
    if (resp.isEmpty()) {
        if (errorOut) *errorOut = err;
        return false;
    }
    const bool ok = resp.contains("200");
    if (!ok && errorOut)
        *errorOut = "Server rejected metadata: " + QString::fromUtf8(resp.left(120));
    return ok;
}

// ─── ICY 2.2 metadata push (Mcaster1DNAS) ────────────────────────────────────
//
// Matches the DSPEncoder protocol: ICY 2.2 fields are sent as query parameters
// on the same GET /admin/metadata request used for ICY 1.x.  DNAS does NOT
// accept a separate PUT request for metadata (responds 401).
//
// GET /admin/metadata?pass=<pass>&mode=updinfo&mount=<mount>
//     &song=<Artist - Title>
//     &title=<Title>&artist=<Artist>
//     &icy-meta-track-title=<Title>&icy-meta-track-artist=<Artist>
//     [&icy-meta-track-album=<Album>][&icy-meta-track-bpm=<BPM>]...
//
// Auth: both pass= query param AND Authorization: Basic header (belt & suspenders).
bool IcyPusher::pushIcy2(const Config& cfg,
                         const QString& artist,
                         const QString& title,
                         const QMap<QString, QString>& fields,
                         QString* errorOut)
{
    const QString streamTitle = artist.isEmpty() ? title : artist + " - " + title;

    // URL-encode all values
    auto enc = [](const QString& s) -> QByteArray {
        return QUrl::toPercentEncoding(s);
    };

    // Build query string — ICY 1.x compat params + ICY 2.2 extended params
    QByteArray query;
    query += "pass=" + enc(cfg.adminPass);
    query += "&mode=updinfo";
    query += "&mount=" + enc(cfg.mount);
    query += "&song="  + enc(streamTitle);
    query += "&title=" + enc(title);
    query += "&artist=" + enc(artist);

    // Core ICY 2.2 track fields (always sent)
    query += "&icy-meta-track-title="  + enc(title);
    query += "&icy-meta-track-artist=" + enc(artist);

    // All additional ICY 2.2 fields from the config/metadata map
    // Convert icy2-xxx-yyy → icy-meta-xxx-yyy for query param format
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (it.value().isEmpty()) continue;
        QString key = it.key();
        // Normalize key: icy2-track-bpm → icy-meta-track-bpm
        if (key.startsWith("icy2-"))
            key = "icy-meta-" + key.mid(5);  // strip "icy2-", prepend "icy-meta-"
        query += "&" + enc(key) + "=" + enc(it.value());
    }

    const QByteArray auth = basicAuth(cfg.adminUser, cfg.adminPass);

    QByteArray req;
    req += "GET /admin/metadata?" + query + " HTTP/1.0\r\n";
    req += "Host: " + cfg.host.toUtf8() + ":" + QByteArray::number(cfg.port) + "\r\n";
    req += "Authorization: Basic " + auth + "\r\n";
    req += "User-Agent: Mcaster1Studio/1.0.0\r\n";
    req += "Connection: close\r\n\r\n";

    QString err;
    QByteArray resp = httpRequest(cfg.host, cfg.port, req, &err);
    if (resp.isEmpty()) {
        if (errorOut) *errorOut = err;
        return false;
    }
    const bool ok = resp.contains("200");
    if (!ok && errorOut)
        *errorOut = "DNAS rejected ICY 2.2 push: " + QString::fromUtf8(resp.left(120));
    return ok;
}
