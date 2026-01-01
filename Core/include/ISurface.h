#pragma once
#include <QString>
#include <QList>

namespace M1 {

/// Surface types — each defines a broadcast intent and default module set.
enum class SurfaceType {
    Alpha,          ///< Primary radio broadcast
    Beta,           ///< Secondary/backup broadcast
    Company,        ///< Corporate/business streaming
    DJ,             ///< Live DJ performance with dual decks
    Entertainment,  ///< TV/Video automation
    Social,         ///< Socialcasting + RTMP streaming
    Podcast,        ///< Professional podcast production
    Church,         ///< Live church services: sermon, worship, AV, podcast, stream
    Custom          ///< User-defined surface
};

/// Convert SurfaceType to display string
inline QString surfaceTypeName(SurfaceType t) {
    switch (t) {
        case SurfaceType::Alpha:         return "Surface Alpha";
        case SurfaceType::Beta:          return "Surface Beta";
        case SurfaceType::Company:       return "Surface Company";
        case SurfaceType::DJ:            return "Surface DJ";
        case SurfaceType::Entertainment: return "Surface Entertainment";
        case SurfaceType::Social:        return "Surface Social";
        case SurfaceType::Podcast:       return "Surface Podcast";
        case SurfaceType::Church:        return "Surface Church";
        case SurfaceType::Custom:        return "Custom Surface";
        default:                         return "Unknown Surface";
    }
}

/// Convert SurfaceType to internal hint string (used in plugin surface_hints)
inline QString surfaceTypeHint(SurfaceType t) {
    switch (t) {
        case SurfaceType::Alpha:         return "alpha";
        case SurfaceType::Beta:          return "beta";
        case SurfaceType::Company:       return "company";
        case SurfaceType::DJ:            return "dj";
        case SurfaceType::Entertainment: return "entertainment";
        case SurfaceType::Social:        return "social";
        case SurfaceType::Podcast:       return "podcast";
        case SurfaceType::Church:        return "church";
        case SurfaceType::Custom:        return "custom";
        default:                         return "custom";
    }
}

/// Convert SurfaceType to short type string used in config files / YAML
inline QString surfaceTypeString(SurfaceType t) { return surfaceTypeHint(t); }

/// Parse a config string back to SurfaceType
inline SurfaceType surfaceTypeFromString(const QString& s) {
    if (s == "alpha")         return SurfaceType::Alpha;
    if (s == "beta")          return SurfaceType::Beta;
    if (s == "company")       return SurfaceType::Company;
    if (s == "dj")            return SurfaceType::DJ;
    if (s == "entertainment") return SurfaceType::Entertainment;
    if (s == "social")        return SurfaceType::Social;
    if (s == "podcast")       return SurfaceType::Podcast;
    if (s == "church")        return SurfaceType::Church;
    return SurfaceType::Custom;
}

/// Default module IDs for each surface type
inline QList<QString> surfaceDefaultModules(SurfaceType t) {
    switch (t) {
        case SurfaceType::Alpha:
            return {"com.mcaster1.vumeter", "com.mcaster1.deck",
                    "com.mcaster1.playlist", "com.mcaster1.encoder",
                    "com.mcaster1.metadata"};
        case SurfaceType::Beta:
            return {"com.mcaster1.vumeter", "com.mcaster1.deck",
                    "com.mcaster1.encoder"};
        case SurfaceType::Company:
            return {"com.mcaster1.playlist", "com.mcaster1.encoder",
                    "com.mcaster1.metadata", "com.mcaster1.monitor"};
        case SurfaceType::DJ:
            return {"com.mcaster1.vumeter", "com.mcaster1.deck",
                    "com.mcaster1.effects", "com.mcaster1.library"};
        case SurfaceType::Entertainment:
            return {"com.mcaster1.video", "com.mcaster1.encoder",
                    "com.mcaster1.playlist"};
        case SurfaceType::Social:
            return {"com.mcaster1.encoder", "com.mcaster1.metadata",
                    "com.mcaster1.monitor"};
        case SurfaceType::Podcast:
            return {"com.mcaster1.ptt", "com.mcaster1.podcast",
                    "com.mcaster1.effects", "com.mcaster1.library",
                    "com.mcaster1.podcast.mixer",
                    "com.mcaster1.podcast.ptt",
                    "com.mcaster1.podcast.recorder",
                    "com.mcaster1.podcast.soundboard",
                    "com.mcaster1.podcast.fx",
                    "com.mcaster1.podcast.editor",
                    "com.mcaster1.podcast.encode",
                    "com.mcaster1.podcast.transcribe",
                    "com.mcaster1.podcast.shownotes",
                    "com.mcaster1.podcast.rss",
                    "com.mcaster1.podcast.publisher",
                    "com.mcaster1.podcast.analytics",
                    "com.mcaster1.podcast.remote"};
        case SurfaceType::Church:
            return {"com.mcaster1.ptt", "com.mcaster1.encoder",
                    "com.mcaster1.vumeter", "com.mcaster1.video",
                    "com.mcaster1.library",
                    "com.mcaster1.church.timerclock",
                    "com.mcaster1.church.graphics",
                    "com.mcaster1.church.lyrics",
                    "com.mcaster1.church.scripture",
                    "com.mcaster1.church.announce",
                    "com.mcaster1.church.teleprompt",
                    "com.mcaster1.church.mediacaster",
                    "com.mcaster1.church.stagemon",
                    "com.mcaster1.church.audiomix",
                    "com.mcaster1.church.transcriberec",
                    "com.mcaster1.church.switchcaster",
                    "com.mcaster1.church.servicerunner"};
        case SurfaceType::Custom:
        default:
            return {};
    }
}

} // namespace M1
