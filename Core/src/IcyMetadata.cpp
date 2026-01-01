#include "IcyMetadata.h"

namespace M1 {

QMap<QString, QString> IcyMetadata::toIcy2Headers() const {
    QMap<QString, QString> h;
    auto add = [&](const QString& key, const QString& val) {
        if (!val.trimmed().isEmpty()) h[key] = val;
    };
    // Group 1 — Station
    add("icy2-station-id",       stationId);
    add("icy2-station-name",     stationName);
    add("icy2-station-logo",     stationLogo);
    add("icy2-station-genre",    stationGenre);
    add("icy2-station-url",      stationUrl);
    add("icy2-station-notice",   stationNotice);
    add("icy2-station-language", stationLanguage);
    // Group 2 — Show
    add("icy2-show-title",       showTitle);
    add("icy2-show-host",        showHost);
    add("icy2-show-start",       showStart);
    add("icy2-show-end",         showEnd);
    add("icy2-show-next",        showNext);
    add("icy2-show-description", showDescription);
    add("icy2-show-poster",      showPoster);
    // Group 3 — Track
    add("icy2-track-title",      trackTitle);
    add("icy2-track-artist",     trackArtist);
    add("icy2-track-album",      trackAlbum);
    add("icy2-track-year",       trackYear);
    add("icy2-track-genre",      trackGenre);
    add("icy2-track-artwork",    trackArtwork);
    add("icy2-track-bpm",        trackBpm);
    add("icy2-track-key",        trackKey);
    add("icy2-track-isrc",       trackIsrc);
    add("icy2-track-mbid",       trackMbid);
    add("icy2-track-label",      trackLabel);
    add("icy2-track-composer",   trackComposer);
    add("icy2-track-lyricist",   trackLyricist);
    add("icy2-track-language",   trackLanguage);
    // Group 4 — DJ
    add("icy2-dj-handle",        djHandle);
    add("icy2-dj-name",          djName);
    add("icy2-dj-bio",           djBio);
    add("icy2-dj-avatar",        djAvatar);
    add("icy2-dj-website",       djWebsite);
    add("icy2-dj-email",         djEmail);
    // Group 5 — Social
    add("icy2-social-twitter",   socialTwitter);
    add("icy2-social-instagram", socialInstagram);
    add("icy2-social-tiktok",    socialTiktok);
    add("icy2-social-youtube",   socialYoutube);
    add("icy2-social-facebook",  socialFacebook);
    add("icy2-social-twitch",    socialTwitch);
    add("icy2-social-linkedin",  socialLinkedin);
    add("icy2-social-linktree",  socialLinktree);
    add("icy2-social-hashtags",  socialHashtags);
    add("icy2-social-discord",   socialDiscord);
    // Group 6 — Podcast
    add("icy2-podcast-title",    podcastTitle);
    add("icy2-podcast-episode",  podcastEpisode);
    add("icy2-podcast-season",   podcastSeason);
    add("icy2-podcast-feed",     podcastFeed);
    add("icy2-podcast-guid",     podcastGuid);
    add("icy2-podcast-duration", podcastDuration);
    add("icy2-podcast-chapter",  podcastChapter);
    // Group 7 — Broadcast
    add("icy2-broadcast-mode",        broadcastMode);
    add("icy2-broadcast-relay",       broadcastRelay);
    add("icy2-broadcast-cdn",         broadcastCdn);
    add("icy2-broadcast-crosspost",   broadcastCrosspost);
    add("icy2-broadcast-lufs",        broadcastLufs);
    add("icy2-broadcast-codec",       broadcastCodec);
    add("icy2-broadcast-samplerate",  broadcastSamplerate);
    add("icy2-broadcast-channels",    broadcastChannels);
    // Group 8 — Content Flags
    add("icy2-content-explicit",      contentExplicit);
    add("icy2-content-live",          contentLive);
    add("icy2-content-type",          contentType);
    add("icy2-content-rating",        contentRating);
    return h;
}

QString IcyMetadata::toIcy1StreamTitle() const {
    if (!trackArtist.isEmpty() && !trackTitle.isEmpty())
        return trackArtist + " - " + trackTitle;
    if (!trackTitle.isEmpty())
        return trackTitle;
    return streamTitle;
}

void IcyMetadata::syncStreamTitle() {
    streamTitle = toIcy1StreamTitle();
}

bool IcyMetadata::hasIcy2Data() const {
    return !stationId.isEmpty()  || !trackTitle.isEmpty() ||
           !djHandle.isEmpty()   || !showTitle.isEmpty()  ||
           !socialTwitter.isEmpty();
}

void IcyMetadata::clear() {
    *this = IcyMetadata{};
}

} // namespace M1
