#pragma once
#include <QString>
#include <QMap>
#include <QMetaType>

/// EncoderConfig — comprehensive per-slot encoder configuration.
///
/// Replaces the minimal EncoderSlot::Config from Phase 4.
/// Covers: codec, audio source, server target (DNAS/Icecast2/Shoutcast),
/// per-slot DSP chain (EQ + AGC + PTT duck), archive recording,
/// and the full ICY 2.2 field set for Mcaster1DNAS targets.
struct EncoderConfig {

    // ── Identity ──────────────────────────────────────────────────────────────
    int     slotId = 0;
    QString name   = "Encoder";

    // ── Encoder type (affects dialog icon / defaults only) ────────────────────
    enum class Mode { Radio, Podcast, TV };
    Mode mode = Mode::Radio;

    // ── Audio input source ────────────────────────────────────────────────────
    enum class Source {
        LiveDeckMix,        ///< RT mix from EncoderModule::onAudioBlock() — default
        PortAudioDevice,    ///< Live soundcard capture via PortAudio
        WasapiLoopback,     ///< Windows system-audio loopback (WASAPI)
    };
    Source  source        = Source::LiveDeckMix;
    int     paDeviceIndex = -1;   ///< PortAudio device index (-1 = system default input)

    // ── Codec ─────────────────────────────────────────────────────────────────
    enum class Codec {
        MP3,        ///< LAME MP3 (HAVE_LAME required)
        Opus,       ///< Ogg Opus via libopusenc (always available)
        Vorbis,     ///< Ogg Vorbis (libvorbis)
        FLAC,       ///< FLAC (libFLAC)
        AacLC,      ///< AAC-LC (fdk-aac, HAVE_FDK_AAC required)
        HEAacV1,    ///< HE-AAC v1 / SBR (fdk-aac)
        HEAacV2,    ///< HE-AAC v2 / SBR+PS (fdk-aac, stereo only)
    };
    Codec   codec      = Codec::MP3;
    int     bitrate    = 128;     ///< kbps (CBR for MP3/AAC/Opus)
    float   vbrQuality = 0.6f;   ///< 0.0–1.0 (Vorbis VBR quality)
    int     sampleRate = 44100;
    int     channels   = 2;
    enum class ChannelMode { Stereo, JointStereo, Mono };
    ChannelMode channelMode = ChannelMode::JointStereo;

    // ── Server target ─────────────────────────────────────────────────────────
    /// DNAS is listed first — it is the primary target for Mcaster1Studio.
    enum class ServerType { DNAS, Icecast2, ShoutcastV1, ShoutcastV2 };
    ServerType serverType = ServerType::DNAS;

    QString host      = "localhost";
    QString mount     = "/stream";
    QString password  = "hackme";
    QString adminUser = "admin";
    QString adminPass = "admin";
    int     port      = 9000;     ///< DNAS default 9000; Icecast2 default 8000

    bool    useSsl           = false;  ///< Use SSL/TLS for source connection (HTTPS streaming)
    bool    autoReconnect    = true;
    int     retryIntervalSec = 15;
    int     maxRetries       = -1;  ///< -1 = infinite

    QString stationName;
    QString description;
    QString genre;
    QString url;
    bool    isPublic = false;

    // ── Per-slot DSP (processed in encoder thread, BEFORE encoding) ───────────
    /// Enabling DSP does NOT affect the global EffectsRackModule processing.
    bool    dspEnabled      = false;

    /// EQ preset name applied to per-slot biquad IIR filter bank.
    /// Values: "flat", "broadcast", "spoken_word", "classic_rock", "country", "modern_rock"
    QString eqPreset        = "flat";

    bool    agcEnabled      = false;
    float   agcInputGainDb  = 0.0f;    ///< pre-compression gain  (-24 to +24 dB)
    float   agcThresholdDb  = -18.0f;  ///< compression onset
    float   agcRatio        = 4.0f;    ///< compression ratio (1:1 – 20:1)
    float   agcAttackMs     = 10.0f;
    float   agcReleaseMs    = 200.0f;
    float   agcMakeupGainDb = 0.0f;    ///< post-compression gain
    float   agcLimiterDb    = -1.0f;   ///< hard limiter ceiling (dBFS)

    bool    pttDuckEnabled  = false;
    float   pttDuckAttenDb  = -12.0f;  ///< attenuation when PTT mic is active

    // ── Archive recording ─────────────────────────────────────────────────────
    bool    archiveEnabled  = false;
    QString archiveDir;               ///< directory for archive files
    bool    archiveWav      = true;   ///< always record WAV when archive enabled
    bool    archiveMp3      = false;  ///< also record MP3 (requires HAVE_LAME)

    // ── Behavior ──────────────────────────────────────────────────────────────
    bool    autoStart = false;  ///< connect automatically on module initialize()
    bool    enabled   = true;

    // ── ICY 2.2 Extended Metadata (Mcaster1DNAS only) ─────────────────────────
    ///
    /// All 70+ ICY 2.2 fields stored as key → value.
    /// Keys use the spec format: "icy2-<group>-<field>"
    ///
    /// Group 1 — Station/Identity:
    ///   icy2-station-id, icy2-station-logo, icy2-verify-status
    ///
    /// Group 2 — Show/Programming:
    ///   icy2-show-title, icy2-show-start, icy2-show-end,
    ///   icy2-next-show, icy2-schedule-url, icy2-auto-dj, icy2-playlist-name
    ///
    /// Group 3 — Track:
    ///   icy2-track-title, icy2-track-artist, icy2-track-album,
    ///   icy2-track-artwork, icy2-track-bpm, icy2-track-key,
    ///   icy2-track-isrc, icy2-track-mbid, icy2-track-label,
    ///   icy2-track-year, icy2-track-genre, icy2-track-duration-ms
    ///
    /// Group 4 — DJ/Host:
    ///   icy2-dj-handle, icy2-dj-bio, icy2-dj-genre, icy2-dj-rating
    ///
    /// Group 5 — Social/Discovery:
    ///   icy2-creator-handle, icy2-social-twitter, icy2-social-twitch,
    ///   icy2-social-instagram, icy2-social-tiktok, icy2-social-youtube,
    ///   icy2-social-facebook, icy2-social-linkedin, icy2-social-linktree,
    ///   icy2-emoji, icy2-hashtags
    ///
    /// Group 6 — Engagement:
    ///   icy2-request-enabled, icy2-request-url, icy2-chat-url,
    ///   icy2-tip-url, icy2-events-url, icy2-crosspost-platforms,
    ///   icy2-cdn-region, icy2-relay-origin
    ///
    /// Group 7 — Compliance/Legal:
    ///   icy2-nsfw, icy2-ai-generated, icy2-royalty-free,
    ///   icy2-license-type, icy2-license-territory, icy2-geo-region,
    ///   icy2-notice-text, icy2-notice-url, icy2-notice-expires,
    ///   icy2-loudness-lufs
    ///
    /// Group 8 — Video/Simulcast:
    ///   icy2-video-type, icy2-video-link, icy2-video-title,
    ///   icy2-video-poster, icy2-video-platform, icy2-video-live,
    ///   icy2-video-codec, icy2-video-fps, icy2-video-resolution,
    ///   icy2-video-rating, icy2-video-nsfw
    ///
    /// Only fields with non-empty values are sent.
    /// IcyPusher::pushIcy2() accepts this map directly.
    QMap<QString, QString> icy2Fields;

    // ── Helpers ───────────────────────────────────────────────────────────────
    /// Returns true if this slot targets Mcaster1DNAS (ICY 2.2 eligible).
    bool isDnas() const { return serverType == ServerType::DNAS; }

    /// Human-readable codec name for UI display.
    QString codecName() const {
        switch (codec) {
            case Codec::MP3:     return "MP3";
            case Codec::Opus:    return "Opus";
            case Codec::Vorbis:  return "OGG Vorbis";
            case Codec::FLAC:    return "FLAC";
            case Codec::AacLC:   return "AAC-LC";
            case Codec::HEAacV1: return "HE-AAC v1";
            case Codec::HEAacV2: return "HE-AAC v2";
        }
        return "Unknown";
    }

    /// MIME content-type for the SOURCE handshake.
    QString contentType() const {
        switch (codec) {
            case Codec::MP3:     return "audio/mpeg";
            case Codec::Opus:    return "application/ogg";
            case Codec::Vorbis:  return "audio/ogg";
            case Codec::FLAC:    return "audio/flac";
            case Codec::AacLC:
            case Codec::HEAacV1:
            case Codec::HEAacV2: return "audio/aac";
        }
        return "audio/mpeg";
    }

    /// Human-readable server type for UI display (DNAS first).
    QString serverTypeName() const {
        switch (serverType) {
            case ServerType::DNAS:        return "Mcaster1DNAS";
            case ServerType::Icecast2:    return "Icecast2";
            case ServerType::ShoutcastV1: return "Shoutcast v1";
            case ServerType::ShoutcastV2: return "Shoutcast v2";
        }
        return "Unknown";
    }
};

Q_DECLARE_METATYPE(EncoderConfig)
