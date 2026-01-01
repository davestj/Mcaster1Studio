#include "MetadataModule.h"
#include "Icy22EditorWidget.h"
#include "MetadataPusher.h"

namespace M1 {

// ─────────────────────────────────────────────────────────────────────────────
// MetadataModule
// ─────────────────────────────────────────────────────────────────────────────

MetadataModule::MetadataModule(QObject* parent)
    : IModule(parent)
{
}

MetadataModule::~MetadataModule() = default;

void MetadataModule::initialize()
{
    // No async resources to allocate at this phase.
}

void MetadataModule::shutdown()
{
    // Nothing to tear down.
}

QWidget* MetadataModule::createWidget(QWidget* parent)
{
    m_widget = new Icy22EditorWidget(this, parent);
    connect(m_widget, &QObject::destroyed, this, [this]() { m_widget = nullptr; });
    m_widget->populateFrom(m_current);
    return m_widget;
}

// ─── IModule::onMetadataUpdate ────────────────────────────────────────────────
//
// Called from Qt thread (via QueuedConnection from AudioEngine event bus) when
// a new track is loaded/played. Update m_current and reflect in the widget.
// ─────────────────────────────────────────────────────────────────────────────
void MetadataModule::onMetadataUpdate(const IcyMetadata& meta)
{
    m_current = meta;
    m_current.syncStreamTitle();

    if (m_widget) {
        m_widget->populateFrom(m_current);
    }

    emit metadataChanged(m_current);
    emit statusChanged(m_current.toIcy1StreamTitle());
}

// ─── pushToDnas ───────────────────────────────────────────────────────────────
//
// Fire-and-forget: first push ICY 1.x StreamTitle= (GET /admin/metadata),
// then push ICY 2.2 headers (PUT /admin/metadata).
// Both calls are async; errors are logged via qWarning inside MetadataPusher.
// ─────────────────────────────────────────────────────────────────────────────
void MetadataModule::pushToDnas(const QString& host,
                                 int            port,
                                 const QString& mount,
                                 const QString& user,
                                 const QString& pass)
{
    // Always push ICY 1.x first (backward compat / all server types).
    MetadataPusher::pushIcy1(host, port, mount, user, pass,
                             m_current.trackArtist,
                             m_current.trackTitle,
                             this);

    // Push ICY 2.2 extended fields (DNAS only).
    MetadataPusher::pushIcy2(host, port, mount, user, pass,
                             m_current,
                             this);
}

// ─── State persistence ────────────────────────────────────────────────────────

void MetadataModule::saveState(QSettings& s)
{
    s.beginGroup("MetadataModule");

    // DNAS connection params
    s.setValue("dnasHost",  m_dnasHost);
    s.setValue("dnasPort",  m_dnasPort);
    s.setValue("dnasMount", m_dnasMount);
    s.setValue("dnasUser",  m_dnasUser);
    s.setValue("dnasPass",  m_dnasPass);

    // Last metadata — ICY 1.x
    s.setValue("meta/streamTitle",      m_current.streamTitle);

    // Group 1 — Station
    s.setValue("meta/stationId",        m_current.stationId);
    s.setValue("meta/stationName",      m_current.stationName);
    s.setValue("meta/stationLogo",      m_current.stationLogo);
    s.setValue("meta/stationGenre",     m_current.stationGenre);
    s.setValue("meta/stationUrl",       m_current.stationUrl);
    s.setValue("meta/stationNotice",    m_current.stationNotice);
    s.setValue("meta/stationLanguage",  m_current.stationLanguage);

    // Group 2 — Show
    s.setValue("meta/showTitle",        m_current.showTitle);
    s.setValue("meta/showHost",         m_current.showHost);
    s.setValue("meta/showStart",        m_current.showStart);
    s.setValue("meta/showEnd",          m_current.showEnd);
    s.setValue("meta/showNext",         m_current.showNext);
    s.setValue("meta/showDescription",  m_current.showDescription);
    s.setValue("meta/showPoster",       m_current.showPoster);

    // Group 3 — Track
    s.setValue("meta/trackTitle",       m_current.trackTitle);
    s.setValue("meta/trackArtist",      m_current.trackArtist);
    s.setValue("meta/trackAlbum",       m_current.trackAlbum);
    s.setValue("meta/trackYear",        m_current.trackYear);
    s.setValue("meta/trackGenre",       m_current.trackGenre);
    s.setValue("meta/trackArtwork",     m_current.trackArtwork);
    s.setValue("meta/trackBpm",         m_current.trackBpm);
    s.setValue("meta/trackKey",         m_current.trackKey);
    s.setValue("meta/trackIsrc",        m_current.trackIsrc);
    s.setValue("meta/trackMbid",        m_current.trackMbid);
    s.setValue("meta/trackLabel",       m_current.trackLabel);
    s.setValue("meta/trackComposer",    m_current.trackComposer);
    s.setValue("meta/trackLyricist",    m_current.trackLyricist);
    s.setValue("meta/trackLanguage",    m_current.trackLanguage);

    // Group 4 — DJ
    s.setValue("meta/djHandle",         m_current.djHandle);
    s.setValue("meta/djName",           m_current.djName);
    s.setValue("meta/djBio",            m_current.djBio);
    s.setValue("meta/djAvatar",         m_current.djAvatar);
    s.setValue("meta/djWebsite",        m_current.djWebsite);
    s.setValue("meta/djEmail",          m_current.djEmail);

    // Group 5 — Social
    s.setValue("meta/socialTwitter",    m_current.socialTwitter);
    s.setValue("meta/socialInstagram",  m_current.socialInstagram);
    s.setValue("meta/socialTiktok",     m_current.socialTiktok);
    s.setValue("meta/socialYoutube",    m_current.socialYoutube);
    s.setValue("meta/socialFacebook",   m_current.socialFacebook);
    s.setValue("meta/socialTwitch",     m_current.socialTwitch);
    s.setValue("meta/socialLinkedin",   m_current.socialLinkedin);
    s.setValue("meta/socialLinktree",   m_current.socialLinktree);
    s.setValue("meta/socialHashtags",   m_current.socialHashtags);
    s.setValue("meta/socialDiscord",    m_current.socialDiscord);

    // Group 6 — Podcast
    s.setValue("meta/podcastTitle",     m_current.podcastTitle);
    s.setValue("meta/podcastEpisode",   m_current.podcastEpisode);
    s.setValue("meta/podcastSeason",    m_current.podcastSeason);
    s.setValue("meta/podcastFeed",      m_current.podcastFeed);
    s.setValue("meta/podcastGuid",      m_current.podcastGuid);
    s.setValue("meta/podcastDuration",  m_current.podcastDuration);
    s.setValue("meta/podcastChapter",   m_current.podcastChapter);

    // Group 7 — Broadcast
    s.setValue("meta/broadcastMode",       m_current.broadcastMode);
    s.setValue("meta/broadcastRelay",      m_current.broadcastRelay);
    s.setValue("meta/broadcastCdn",        m_current.broadcastCdn);
    s.setValue("meta/broadcastCrosspost",  m_current.broadcastCrosspost);
    s.setValue("meta/broadcastLufs",       m_current.broadcastLufs);
    s.setValue("meta/broadcastCodec",      m_current.broadcastCodec);
    s.setValue("meta/broadcastSamplerate", m_current.broadcastSamplerate);
    s.setValue("meta/broadcastChannels",   m_current.broadcastChannels);

    // Group 8 — Content Flags
    s.setValue("meta/contentExplicit",  m_current.contentExplicit);
    s.setValue("meta/contentLive",      m_current.contentLive);
    s.setValue("meta/contentType",      m_current.contentType);
    s.setValue("meta/contentRating",    m_current.contentRating);

    s.endGroup();
}

void MetadataModule::loadState(QSettings& s)
{
    s.beginGroup("MetadataModule");

    // DNAS connection params
    m_dnasHost  = s.value("dnasHost",  "").toString();
    m_dnasPort  = s.value("dnasPort",  8000).toInt();
    m_dnasMount = s.value("dnasMount", "/live").toString();
    m_dnasUser  = s.value("dnasUser",  "source").toString();
    m_dnasPass  = s.value("dnasPass",  "").toString();

    // Last metadata — ICY 1.x
    m_current.streamTitle     = s.value("meta/streamTitle", "").toString();

    // Group 1 — Station
    m_current.stationId       = s.value("meta/stationId",       "").toString();
    m_current.stationName     = s.value("meta/stationName",     "").toString();
    m_current.stationLogo     = s.value("meta/stationLogo",     "").toString();
    m_current.stationGenre    = s.value("meta/stationGenre",    "").toString();
    m_current.stationUrl      = s.value("meta/stationUrl",      "").toString();
    m_current.stationNotice   = s.value("meta/stationNotice",   "").toString();
    m_current.stationLanguage = s.value("meta/stationLanguage", "").toString();

    // Group 2 — Show
    m_current.showTitle       = s.value("meta/showTitle",       "").toString();
    m_current.showHost        = s.value("meta/showHost",        "").toString();
    m_current.showStart       = s.value("meta/showStart",       "").toString();
    m_current.showEnd         = s.value("meta/showEnd",         "").toString();
    m_current.showNext        = s.value("meta/showNext",        "").toString();
    m_current.showDescription = s.value("meta/showDescription", "").toString();
    m_current.showPoster      = s.value("meta/showPoster",      "").toString();

    // Group 3 — Track
    m_current.trackTitle      = s.value("meta/trackTitle",    "").toString();
    m_current.trackArtist     = s.value("meta/trackArtist",   "").toString();
    m_current.trackAlbum      = s.value("meta/trackAlbum",    "").toString();
    m_current.trackYear       = s.value("meta/trackYear",     "").toString();
    m_current.trackGenre      = s.value("meta/trackGenre",    "").toString();
    m_current.trackArtwork    = s.value("meta/trackArtwork",  "").toString();
    m_current.trackBpm        = s.value("meta/trackBpm",      "").toString();
    m_current.trackKey        = s.value("meta/trackKey",      "").toString();
    m_current.trackIsrc       = s.value("meta/trackIsrc",     "").toString();
    m_current.trackMbid       = s.value("meta/trackMbid",     "").toString();
    m_current.trackLabel      = s.value("meta/trackLabel",    "").toString();
    m_current.trackComposer   = s.value("meta/trackComposer", "").toString();
    m_current.trackLyricist   = s.value("meta/trackLyricist", "").toString();
    m_current.trackLanguage   = s.value("meta/trackLanguage", "").toString();

    // Group 4 — DJ
    m_current.djHandle        = s.value("meta/djHandle",   "").toString();
    m_current.djName          = s.value("meta/djName",     "").toString();
    m_current.djBio           = s.value("meta/djBio",      "").toString();
    m_current.djAvatar        = s.value("meta/djAvatar",   "").toString();
    m_current.djWebsite       = s.value("meta/djWebsite",  "").toString();
    m_current.djEmail         = s.value("meta/djEmail",    "").toString();

    // Group 5 — Social
    m_current.socialTwitter   = s.value("meta/socialTwitter",   "").toString();
    m_current.socialInstagram = s.value("meta/socialInstagram", "").toString();
    m_current.socialTiktok    = s.value("meta/socialTiktok",    "").toString();
    m_current.socialYoutube   = s.value("meta/socialYoutube",   "").toString();
    m_current.socialFacebook  = s.value("meta/socialFacebook",  "").toString();
    m_current.socialTwitch    = s.value("meta/socialTwitch",    "").toString();
    m_current.socialLinkedin  = s.value("meta/socialLinkedin",  "").toString();
    m_current.socialLinktree  = s.value("meta/socialLinktree",  "").toString();
    m_current.socialHashtags  = s.value("meta/socialHashtags",  "").toString();
    m_current.socialDiscord   = s.value("meta/socialDiscord",   "").toString();

    // Group 6 — Podcast
    m_current.podcastTitle    = s.value("meta/podcastTitle",    "").toString();
    m_current.podcastEpisode  = s.value("meta/podcastEpisode",  "").toString();
    m_current.podcastSeason   = s.value("meta/podcastSeason",   "").toString();
    m_current.podcastFeed     = s.value("meta/podcastFeed",     "").toString();
    m_current.podcastGuid     = s.value("meta/podcastGuid",     "").toString();
    m_current.podcastDuration = s.value("meta/podcastDuration", "").toString();
    m_current.podcastChapter  = s.value("meta/podcastChapter",  "").toString();

    // Group 7 — Broadcast
    m_current.broadcastMode       = s.value("meta/broadcastMode",       "live").toString();
    m_current.broadcastRelay      = s.value("meta/broadcastRelay",      "").toString();
    m_current.broadcastCdn        = s.value("meta/broadcastCdn",        "").toString();
    m_current.broadcastCrosspost  = s.value("meta/broadcastCrosspost",  "").toString();
    m_current.broadcastLufs       = s.value("meta/broadcastLufs",       "").toString();
    m_current.broadcastCodec      = s.value("meta/broadcastCodec",      "").toString();
    m_current.broadcastSamplerate = s.value("meta/broadcastSamplerate", "").toString();
    m_current.broadcastChannels   = s.value("meta/broadcastChannels",   "").toString();

    // Group 8 — Content Flags
    m_current.contentExplicit = s.value("meta/contentExplicit", "false").toString();
    m_current.contentLive     = s.value("meta/contentLive",     "false").toString();
    m_current.contentType     = s.value("meta/contentType",     "radio").toString();
    m_current.contentRating   = s.value("meta/contentRating",   "").toString();

    s.endGroup();

    // Sync widget if already created (surface reload scenario).
    if (m_widget) {
        m_widget->populateFrom(m_current);
    }
}

} // namespace M1

// ─────────────────────────────────────────────────────────────────────────────
// C ABI plugin exports
// ─────────────────────────────────────────────────────────────────────────────

static Mcaster1PluginInfo s_metadataInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.metadata",
    "Metadata",
    "1.0.0",
    "*",
    "module",
    "Mcaster1",
    "ICY 1.x + ICY 2.2 metadata editor and DNAS push module"
};

extern "C" {

MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_metadata_plugin_info()
{
    return &s_metadataInfo;
}

MCASTER1_PLUGIN_API M1::IModule* mcaster1_metadata_create(IModuleHost* /*host*/)
{
    return new M1::MetadataModule();
}

MCASTER1_PLUGIN_API void mcaster1_metadata_destroy(M1::IModule* mod)
{
    delete mod;
}

} // extern "C"
