#include "MetadataPusher.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QByteArray>
#include <QAuthenticator>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMetaPusher, "mcaster1.metadata.pusher")

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/// Build the base URL for the DNAS admin/metadata endpoint.
QUrl buildBaseUrl(const QString& host, int port, const QString& mount)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(host);
    url.setPort(port);

    // Ensure mount starts with /
    QString m = mount;
    if (!m.startsWith('/'))
        m.prepend('/');

    // Strip trailing slash from mount, then append /admin/metadata
    // Standard Icecast/DNAS admin path: /admin/metadata
    url.setPath("/admin/metadata");
    return url;
}

/// Attach Basic-Auth credentials as Authorization header.
void addBasicAuth(QNetworkRequest& req, const QString& user, const QString& pass)
{
    const QString creds = user + ":" + pass;
    const QByteArray b64 = creds.toUtf8().toBase64();
    req.setRawHeader("Authorization", "Basic " + b64);
}

/// Wire reply to log errors and auto-delete itself when finished.
void connectReply(QNetworkReply* reply, const QString& context)
{
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, context]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcMetaPusher)
                << context
                << "failed:"
                << reply->errorString()
                << "(HTTP" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << ")";
        } else {
            qCDebug(lcMetaPusher)
                << context
                << "OK (HTTP"
                << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << ")";
        }
        reply->deleteLater();
    });
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// MetadataPusher::pushIcy1
//
// GET /admin/metadata?mode=updinfo&mount=<mount>&song=<percent-encoded song>
//
// The "song" parameter is the ICY 1.x StreamTitle value, formatted as
// "Artist - Title" when both fields are present.
// ─────────────────────────────────────────────────────────────────────────────
void MetadataPusher::pushIcy1(const QString& host,
                               int            port,
                               const QString& mount,
                               const QString& user,
                               const QString& pass,
                               const QString& artist,
                               const QString& title,
                               QObject*       parent)
{
    if (host.isEmpty()) {
        qCWarning(lcMetaPusher) << "pushIcy1: host is empty, skipping";
        return;
    }

    // Build song string
    QString song;
    if (!artist.isEmpty() && !title.isEmpty())
        song = artist + " - " + title;
    else if (!artist.isEmpty())
        song = artist;
    else if (!title.isEmpty())
        song = title;
    else
        song = "Unknown";

    QUrl url = buildBaseUrl(host, port, mount);
    QUrlQuery query;
    query.addQueryItem("mode",  "updinfo");
    query.addQueryItem("mount", mount.startsWith('/') ? mount : "/" + mount);
    query.addQueryItem("song",  song);  // QUrlQuery percent-encodes automatically
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mcaster1Studio/1.0");
    addBasicAuth(req, user, pass);

    // Create manager parented to the given parent so it lives long enough.
    auto* mgr   = new QNetworkAccessManager(parent);
    auto* reply = mgr->get(req);
    connectReply(reply, "ICY1 push");

    // Manager auto-deletes after reply finishes.
    QObject::connect(reply, &QNetworkReply::finished, mgr, &QNetworkAccessManager::deleteLater);
}

// ─────────────────────────────────────────────────────────────────────────────
// MetadataPusher::pushIcy2
//
// PUT /admin/metadata
// Headers:
//   Content-Type: application/x-icy2-metadata
//   Icy-Version: 2.2
//   Icy-Metadata-Mount: <mount>
//   StreamTitle: <Artist - Title>   (ICY 1.x backward compat)
//   icy2-station-id: ...
//   icy2-station-name: ...
//   ... (all 70+ fields, empty values omitted)
// ─────────────────────────────────────────────────────────────────────────────
void MetadataPusher::pushIcy2(const QString&        host,
                               int                   port,
                               const QString&        mount,
                               const QString&        user,
                               const QString&        pass,
                               const M1::IcyMetadata& meta,
                               QObject*              parent)
{
    if (host.isEmpty()) {
        qCWarning(lcMetaPusher) << "pushIcy2: host is empty, skipping";
        return;
    }

    QUrl url = buildBaseUrl(host, port, mount);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-icy2-metadata");
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mcaster1Studio/1.0");
    addBasicAuth(req, user, pass);

    // ICY 2.2 negotiation header
    req.setRawHeader("Icy-Version", "2.2");

    // Mount header
    const QString mountNorm = mount.startsWith('/') ? mount : "/" + mount;
    req.setRawHeader("Icy-Metadata-Mount", mountNorm.toUtf8());

    // ICY 1.x StreamTitle (always included for backward compatibility)
    const QString streamTitle = meta.toIcy1StreamTitle();
    req.setRawHeader("StreamTitle", streamTitle.toUtf8());

    // Helper lambda: add header only if value is non-empty
    auto addHeader = [&](const char* name, const QString& value) {
        if (!value.isEmpty())
            req.setRawHeader(name, value.toUtf8());
    };

    // ── Group 1: Station ─────────────────────────────────────────────────
    addHeader("icy2-station-id",       meta.stationId);
    addHeader("icy2-station-name",     meta.stationName);
    addHeader("icy2-station-logo",     meta.stationLogo);
    addHeader("icy2-station-genre",    meta.stationGenre);
    addHeader("icy2-station-url",      meta.stationUrl);
    addHeader("icy2-station-notice",   meta.stationNotice);
    addHeader("icy2-station-language", meta.stationLanguage);

    // ── Group 2: Show ─────────────────────────────────────────────────────
    addHeader("icy2-show-title",       meta.showTitle);
    addHeader("icy2-show-host",        meta.showHost);
    addHeader("icy2-show-start",       meta.showStart);
    addHeader("icy2-show-end",         meta.showEnd);
    addHeader("icy2-show-next",        meta.showNext);
    addHeader("icy2-show-description", meta.showDescription);
    addHeader("icy2-show-poster",      meta.showPoster);

    // ── Group 3: Track ────────────────────────────────────────────────────
    addHeader("icy2-track-title",      meta.trackTitle);
    addHeader("icy2-track-artist",     meta.trackArtist);
    addHeader("icy2-track-album",      meta.trackAlbum);
    addHeader("icy2-track-year",       meta.trackYear);
    addHeader("icy2-track-genre",      meta.trackGenre);
    addHeader("icy2-track-artwork",    meta.trackArtwork);
    addHeader("icy2-track-bpm",        meta.trackBpm);
    addHeader("icy2-track-key",        meta.trackKey);
    addHeader("icy2-track-isrc",       meta.trackIsrc);
    addHeader("icy2-track-mbid",       meta.trackMbid);
    addHeader("icy2-track-label",      meta.trackLabel);
    addHeader("icy2-track-composer",   meta.trackComposer);
    addHeader("icy2-track-lyricist",   meta.trackLyricist);
    addHeader("icy2-track-language",   meta.trackLanguage);

    // ── Group 4: DJ ───────────────────────────────────────────────────────
    addHeader("icy2-dj-handle",        meta.djHandle);
    addHeader("icy2-dj-name",          meta.djName);
    addHeader("icy2-dj-bio",           meta.djBio);
    addHeader("icy2-dj-avatar",        meta.djAvatar);
    addHeader("icy2-dj-website",       meta.djWebsite);
    addHeader("icy2-dj-email",         meta.djEmail);

    // ── Group 5: Social ───────────────────────────────────────────────────
    addHeader("icy2-social-twitter",   meta.socialTwitter);
    addHeader("icy2-social-instagram", meta.socialInstagram);
    addHeader("icy2-social-tiktok",    meta.socialTiktok);
    addHeader("icy2-social-youtube",   meta.socialYoutube);
    addHeader("icy2-social-facebook",  meta.socialFacebook);
    addHeader("icy2-social-twitch",    meta.socialTwitch);
    addHeader("icy2-social-linkedin",  meta.socialLinkedin);
    addHeader("icy2-social-linktree",  meta.socialLinktree);
    addHeader("icy2-social-hashtags",  meta.socialHashtags);
    addHeader("icy2-social-discord",   meta.socialDiscord);

    // ── Group 6: Podcast ──────────────────────────────────────────────────
    addHeader("icy2-podcast-title",    meta.podcastTitle);
    addHeader("icy2-podcast-episode",  meta.podcastEpisode);
    addHeader("icy2-podcast-season",   meta.podcastSeason);
    addHeader("icy2-podcast-feed",     meta.podcastFeed);
    addHeader("icy2-podcast-guid",     meta.podcastGuid);
    addHeader("icy2-podcast-duration", meta.podcastDuration);
    addHeader("icy2-podcast-chapter",  meta.podcastChapter);

    // ── Group 7: Broadcast ────────────────────────────────────────────────
    addHeader("icy2-broadcast-mode",       meta.broadcastMode);
    addHeader("icy2-broadcast-relay",      meta.broadcastRelay);
    addHeader("icy2-broadcast-cdn",        meta.broadcastCdn);
    addHeader("icy2-broadcast-crosspost",  meta.broadcastCrosspost);
    addHeader("icy2-broadcast-lufs",       meta.broadcastLufs);
    addHeader("icy2-broadcast-codec",      meta.broadcastCodec);
    addHeader("icy2-broadcast-samplerate", meta.broadcastSamplerate);
    addHeader("icy2-broadcast-channels",   meta.broadcastChannels);

    // ── Group 8: Content Flags ────────────────────────────────────────────
    addHeader("icy2-content-explicit",     meta.contentExplicit);
    addHeader("icy2-content-live",         meta.contentLive);
    addHeader("icy2-content-type",         meta.contentType);
    addHeader("icy2-content-rating",       meta.contentRating);

    // PUT with empty body — all data is in headers per ICY 2.2 spec.
    auto* mgr   = new QNetworkAccessManager(parent);
    auto* reply = mgr->sendCustomRequest(req, "PUT", QByteArray());
    connectReply(reply, "ICY2 push");

    QObject::connect(reply, &QNetworkReply::finished, mgr, &QNetworkAccessManager::deleteLater);
}
