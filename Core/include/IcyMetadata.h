#pragma once
#include <QString>
#include <QMap>

namespace M1 {

/// ICY 1.x + ICY 2.2 metadata structure.
///
/// ICY 1.x:  Supported by ALL streaming servers (Icecast2, Shoutcast, Mcaster1DNAS).
///           Injected as in-stream metadata at Icy-MetaInt byte intervals.
///           HTTP source header: icy-name, icy-genre, icy-url, icy-pub, icy-br
///           Metadata block:     StreamTitle=<artist - title>;
///
/// ICY 2.2:  Mcaster1DNAS only. Sent as HTTP headers on SOURCE connection
///           and updated via HTTP PUT to /admin/metadata.
///           Signal capability with header: Icy-Version: 2.2
///           Always also set StreamTitle= for backward compatibility.
///
/// Spec ref: https://mcaster1.com/icy2-spec/
struct IcyMetadata {

    // -----------------------------------------------------------------------
    // ICY 1.x (all encoders, all servers)
    // -----------------------------------------------------------------------
    QString streamTitle;        ///< Combined "Artist - Title" for inline ICY meta

    // -----------------------------------------------------------------------
    // Group 1 — Station (ICY 2.2 / Mcaster1DNAS)
    // -----------------------------------------------------------------------
    QString stationId;
    QString stationName;
    QString stationLogo;        ///< URL to station logo image
    QString stationGenre;
    QString stationUrl;         ///< Station website
    QString stationNotice;      ///< Broadcast notice / station message
    QString stationLanguage;    ///< BCP-47 language tag e.g. "en-US"

    // -----------------------------------------------------------------------
    // Group 2 — Show
    // -----------------------------------------------------------------------
    QString showTitle;
    QString showHost;
    QString showStart;          ///< ISO 8601 datetime
    QString showEnd;            ///< ISO 8601 datetime
    QString showNext;           ///< Next show title
    QString showDescription;
    QString showPoster;         ///< URL to show artwork

    // -----------------------------------------------------------------------
    // Group 3 — Track
    // -----------------------------------------------------------------------
    QString trackTitle;
    QString trackArtist;
    QString trackAlbum;
    QString trackYear;
    QString trackGenre;
    QString trackArtwork;       ///< URL to track artwork
    QString trackBpm;           ///< Beats per minute as string
    QString trackKey;           ///< Musical key e.g. "Am", "Gmaj"
    QString trackIsrc;          ///< ISRC code
    QString trackMbid;          ///< MusicBrainz recording ID
    QString trackLabel;         ///< Record label
    QString trackComposer;
    QString trackLyricist;
    QString trackLanguage;      ///< BCP-47

    // -----------------------------------------------------------------------
    // Group 4 — DJ
    // -----------------------------------------------------------------------
    QString djHandle;           ///< DJ on-air name
    QString djName;             ///< Real name (optional)
    QString djBio;
    QString djAvatar;           ///< URL to DJ avatar image
    QString djWebsite;
    QString djEmail;

    // -----------------------------------------------------------------------
    // Group 5 — Social
    // -----------------------------------------------------------------------
    QString socialTwitter;      ///< @handle (without @)
    QString socialInstagram;
    QString socialTiktok;
    QString socialYoutube;      ///< Channel URL or handle
    QString socialFacebook;
    QString socialTwitch;
    QString socialLinkedin;
    QString socialLinktree;
    QString socialHashtags;     ///< Space or comma-separated hashtags
    QString socialDiscord;      ///< Invite link or server ID

    // -----------------------------------------------------------------------
    // Group 6 — Podcast
    // -----------------------------------------------------------------------
    QString podcastTitle;
    QString podcastEpisode;     ///< Episode number
    QString podcastSeason;      ///< Season number
    QString podcastFeed;        ///< RSS feed URL
    QString podcastGuid;        ///< Episode GUID
    QString podcastDuration;    ///< HH:MM:SS
    QString podcastChapter;     ///< Current chapter title

    // -----------------------------------------------------------------------
    // Group 7 — Broadcast
    // -----------------------------------------------------------------------
    QString broadcastMode;      ///< "live" | "autodj" | "replay"
    QString broadcastRelay;     ///< Relay source URL (if relaying)
    QString broadcastCdn;       ///< CDN region hint
    QString broadcastCrosspost; ///< Comma-separated crosspost targets
    QString broadcastLufs;      ///< Integrated loudness e.g. "-14"
    QString broadcastCodec;     ///< "mp3" | "opus" | "aac" | "flac" | "vorbis"
    QString broadcastSamplerate;///< e.g. "44100"
    QString broadcastChannels;  ///< "1" | "2"

    // -----------------------------------------------------------------------
    // Group 8 — Content Flags
    // -----------------------------------------------------------------------
    QString contentExplicit;    ///< "true" | "false"
    QString contentLive;        ///< "true" | "false"
    QString contentType;        ///< "radio" | "tv" | "podcast" | "socialcast" | "church"
    QString contentRating;      ///< "G" | "PG" | "MA" | "X"

    // -----------------------------------------------------------------------
    // Serialization helpers
    // -----------------------------------------------------------------------

    /// Build ICY 2.2 HTTP headers map (icy2-* keys → values, empty values omitted)
    QMap<QString, QString> toIcy2Headers() const;

    /// Build ICY 1.x StreamTitle value: "trackArtist - trackTitle"
    /// Falls back to streamTitle if individual fields are empty.
    QString toIcy1StreamTitle() const;

    /// Populate streamTitle from trackArtist + trackTitle
    void syncStreamTitle();

    /// True if any ICY 2.2 field is populated
    bool hasIcy2Data() const;

    /// Clear all fields
    void clear();
};

} // namespace M1
