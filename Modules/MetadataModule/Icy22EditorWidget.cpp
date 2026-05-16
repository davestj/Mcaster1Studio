#include "Icy22EditorWidget.h"
#include "MetadataModule.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>
#include <QSizePolicy>
#include <QFileDialog>
#include <QFrame>
#include <QFont>
#include <QTime>

// ─────────────────────────────────────────────────────────────────────────────
// Static factory helpers
// ─────────────────────────────────────────────────────────────────────────────
QLineEdit* Icy22EditorWidget::makeLineEdit(QWidget* parent)
{
    auto* w = new QLineEdit(parent);
    w->setMinimumHeight(26);
    return w;
}

QTextEdit* Icy22EditorWidget::makeTextEdit(QWidget* parent)
{
    auto* w = new QTextEdit(parent);
    w->setMinimumHeight(70);
    w->setMaximumHeight(100);
    return w;
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor
// ─────────────────────────────────────────────────────────────────────────────
Icy22EditorWidget::Icy22EditorWidget(M1::MetadataModule* module, QWidget* parent)
    : QWidget(parent)
    , m_module(module)
{
    buildUi();
    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); });
}

// ─────────────────────────────────────────────────────────────────────────────
// buildUi
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(6);

    // ── Tab widget ────────────────────────────────────────────────────────
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildStationTab(),   "Station");
    m_tabs->addTab(buildShowTab(),      "Show");
    m_tabs->addTab(buildTrackTab(),     "Track");
    m_tabs->addTab(buildDjTab(),        "DJ");
    m_tabs->addTab(buildSocialTab(),    "Social");
    m_tabs->addTab(buildPodcastTab(),   "Podcast");
    m_tabs->addTab(buildBroadcastTab(), "Broadcast");
    m_tabs->addTab(buildContentTab(),   "Content");
    rootLayout->addWidget(m_tabs, 1);

    // ── Separator ─────────────────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    rootLayout->addWidget(sep);

    // ── ICY 1.x StreamTitle display ───────────────────────────────────────
    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel("StreamTitle:", this);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setMinimumWidth(80);
        m_icy1Display = makeLineEdit(this);
        m_icy1Display->setReadOnly(true);
        m_icy1Display->setPlaceholderText("Artist - Title (auto-generated)");
        row->addWidget(lbl);
        row->addWidget(m_icy1Display, 1);
        rootLayout->addLayout(row);
    }

    // ── DNAS target config row ─────────────────────────────────────────────
    {
        auto* dnasBox = new QGroupBox("DNAS Target", this);
        auto* dnasGrid = new QHBoxLayout(dnasBox);
        dnasGrid->setContentsMargins(6, 4, 6, 4);
        dnasGrid->setSpacing(6);

        // Host
        auto* hostLbl = new QLabel("Host:", this);
        hostLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_dnasHost = makeLineEdit(this);
        m_dnasHost->setPlaceholderText("dnas.example.com");
        m_dnasHost->setMinimumWidth(140);

        // Port
        auto* portLbl = new QLabel("Port:", this);
        portLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_dnasPort = new QSpinBox(this);
        m_dnasPort->setRange(1, 65535);
        m_dnasPort->setValue(8000);
        m_dnasPort->setMinimumHeight(26);
        m_dnasPort->setMinimumWidth(60);

        // Mount
        auto* mountLbl = new QLabel("Mount:", this);
        mountLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_dnasMount = makeLineEdit(this);
        m_dnasMount->setPlaceholderText("/live");
        m_dnasMount->setMinimumWidth(70);

        // User
        auto* userLbl = new QLabel("User:", this);
        userLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_dnasUser = makeLineEdit(this);
        m_dnasUser->setPlaceholderText("source");
        m_dnasUser->setMinimumWidth(70);

        // Pass
        auto* passLbl = new QLabel("Pass:", this);
        passLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_dnasPass = makeLineEdit(this);
        m_dnasPass->setEchoMode(QLineEdit::Password);
        m_dnasPass->setMinimumWidth(70);

        dnasGrid->addWidget(hostLbl);
        dnasGrid->addWidget(m_dnasHost);
        dnasGrid->addWidget(portLbl);
        dnasGrid->addWidget(m_dnasPort);
        dnasGrid->addWidget(mountLbl);
        dnasGrid->addWidget(m_dnasMount);
        dnasGrid->addWidget(userLbl);
        dnasGrid->addWidget(m_dnasUser);
        dnasGrid->addWidget(passLbl);
        dnasGrid->addWidget(m_dnasPass);
        dnasGrid->addStretch();

        rootLayout->addWidget(dnasBox);
    }

    // ── Push Now button ────────────────────────────────────────────────────
    {
        auto* btnRow = new QHBoxLayout();
        btnRow->addStretch();
        m_pushNowBtn = new QPushButton("Push Now", this);
        m_pushNowBtn->setObjectName("pushNowBtn");
        m_pushNowBtn->setMinimumHeight(32);
        m_pushNowBtn->setMinimumWidth(120);
        btnRow->addWidget(m_pushNowBtn);
        rootLayout->addLayout(btnRow);
    }

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_pushNowBtn, &QPushButton::clicked, this, &Icy22EditorWidget::onPushNow);
    connect(m_artworkBrowse, &QPushButton::clicked, this, &Icy22EditorWidget::onArtworkBrowse);

    // Auto-update ICY 1.x StreamTitle display when artist or title changes
    connect(m_trackArtist, &QLineEdit::textChanged, this, &Icy22EditorWidget::onTrackFieldChanged);
    connect(m_trackTitle,  &QLineEdit::textChanged, this, &Icy22EditorWidget::onTrackFieldChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab builders
// ─────────────────────────────────────────────────────────────────────────────

QWidget* Icy22EditorWidget::buildStationTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_stationId       = makeLineEdit(container);
    m_stationName     = makeLineEdit(container);
    m_stationLogo     = makeLineEdit(container);
    m_stationGenre    = makeLineEdit(container);
    m_stationUrl      = makeLineEdit(container);
    m_stationNotice   = makeLineEdit(container);
    m_stationLanguage = makeLineEdit(container);

    m_stationId->setPlaceholderText("e.g. station-001");
    m_stationName->setPlaceholderText("Station display name");
    m_stationLogo->setPlaceholderText("https://example.com/logo.png");
    m_stationGenre->setPlaceholderText("e.g. Electronic, Hip-Hop");
    m_stationUrl->setPlaceholderText("https://mystation.com");
    m_stationNotice->setPlaceholderText("Broadcast notice or station message");
    m_stationLanguage->setPlaceholderText("BCP-47 e.g. en-US");

    form->addRow("Station ID:",    m_stationId);
    form->addRow("Name:",          m_stationName);
    form->addRow("Logo URL:",      m_stationLogo);
    form->addRow("Genre:",         m_stationGenre);
    form->addRow("Website URL:",   m_stationUrl);
    form->addRow("Notice:",        m_stationNotice);
    form->addRow("Language:",      m_stationLanguage);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildShowTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_showTitle       = makeLineEdit(container);
    m_showHost        = makeLineEdit(container);
    m_showStart       = new QTimeEdit(container);
    m_showEnd         = new QTimeEdit(container);
    m_showNext        = makeLineEdit(container);
    m_showDescription = makeTextEdit(container);
    m_showPoster      = makeLineEdit(container);

    m_showTitle->setPlaceholderText("The Evening Mix");
    m_showHost->setPlaceholderText("DJ Handle or Name");
    m_showStart->setDisplayFormat("HH:mm");
    m_showEnd->setDisplayFormat("HH:mm");
    m_showNext->setPlaceholderText("Next show title");
    m_showPoster->setPlaceholderText("https://example.com/show-poster.jpg");

    m_showStart->setMinimumHeight(26);
    m_showEnd->setMinimumHeight(26);

    form->addRow("Show Title:",    m_showTitle);
    form->addRow("Host:",          m_showHost);
    form->addRow("Start (HH:MM):", m_showStart);
    form->addRow("End (HH:MM):",   m_showEnd);
    form->addRow("Next Show:",     m_showNext);
    form->addRow("Description:",   m_showDescription);
    form->addRow("Poster URL:",    m_showPoster);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildTrackTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_trackTitle    = makeLineEdit(container);
    m_trackArtist   = makeLineEdit(container);
    m_trackAlbum    = makeLineEdit(container);
    m_trackYear     = makeLineEdit(container);
    m_trackGenre    = makeLineEdit(container);
    m_trackArtwork  = makeLineEdit(container);
    m_trackBpm      = makeLineEdit(container);
    m_trackKey      = makeLineEdit(container);
    m_trackIsrc     = makeLineEdit(container);
    m_trackMbid     = makeLineEdit(container);
    m_trackLabel    = makeLineEdit(container);
    m_trackComposer = makeLineEdit(container);
    m_trackLyricist = makeLineEdit(container);
    m_trackLanguage = makeLineEdit(container);

    // Artwork row: URL field + browse button side by side
    m_artworkBrowse = new QPushButton("Browse...", container);
    m_artworkBrowse->setMinimumHeight(26);
    m_artworkBrowse->setMinimumWidth(70);
    auto* artworkRow = new QHBoxLayout();
    artworkRow->addWidget(m_trackArtwork);
    artworkRow->addWidget(m_artworkBrowse);

    m_trackTitle->setPlaceholderText("Track title");
    m_trackArtist->setPlaceholderText("Artist name");
    m_trackAlbum->setPlaceholderText("Album name");
    m_trackYear->setPlaceholderText("e.g. 2024");
    m_trackGenre->setPlaceholderText("e.g. Electronic");
    m_trackArtwork->setPlaceholderText("https://example.com/artwork.jpg or local path");
    m_trackBpm->setPlaceholderText("e.g. 128");
    m_trackKey->setPlaceholderText("e.g. Am, Gmaj");
    m_trackIsrc->setPlaceholderText("ISRC code");
    m_trackMbid->setPlaceholderText("MusicBrainz recording ID");
    m_trackLabel->setPlaceholderText("Record label");
    m_trackComposer->setPlaceholderText("Composer name");
    m_trackLyricist->setPlaceholderText("Lyricist name");
    m_trackLanguage->setPlaceholderText("BCP-47 e.g. en-US");

    form->addRow("Title:",     m_trackTitle);
    form->addRow("Artist:",    m_trackArtist);
    form->addRow("Album:",     m_trackAlbum);
    form->addRow("Year:",      m_trackYear);
    form->addRow("Genre:",     m_trackGenre);
    form->addRow("Artwork:",   artworkRow);
    form->addRow("BPM:",       m_trackBpm);
    form->addRow("Key:",       m_trackKey);
    form->addRow("ISRC:",      m_trackIsrc);
    form->addRow("MBID:",      m_trackMbid);
    form->addRow("Label:",     m_trackLabel);
    form->addRow("Composer:",  m_trackComposer);
    form->addRow("Lyricist:",  m_trackLyricist);
    form->addRow("Language:",  m_trackLanguage);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildDjTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_djHandle  = makeLineEdit(container);
    m_djName    = makeLineEdit(container);
    m_djBio     = makeTextEdit(container);
    m_djAvatar  = makeLineEdit(container);
    m_djWebsite = makeLineEdit(container);
    m_djEmail   = makeLineEdit(container);

    m_djHandle->setPlaceholderText("On-air handle / stage name");
    m_djName->setPlaceholderText("Real name (optional)");
    m_djAvatar->setPlaceholderText("https://example.com/avatar.jpg");
    m_djWebsite->setPlaceholderText("https://djwebsite.com");
    m_djEmail->setPlaceholderText("dj@example.com");

    form->addRow("Handle:",  m_djHandle);
    form->addRow("Name:",    m_djName);
    form->addRow("Bio:",     m_djBio);
    form->addRow("Avatar:",  m_djAvatar);
    form->addRow("Website:", m_djWebsite);
    form->addRow("Email:",   m_djEmail);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildSocialTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_socialTwitter   = makeLineEdit(container);
    m_socialInstagram = makeLineEdit(container);
    m_socialTiktok    = makeLineEdit(container);
    m_socialYoutube   = makeLineEdit(container);
    m_socialFacebook  = makeLineEdit(container);
    m_socialTwitch    = makeLineEdit(container);
    m_socialLinkedin  = makeLineEdit(container);
    m_socialLinktree  = makeLineEdit(container);
    m_socialHashtags  = makeLineEdit(container);
    m_socialDiscord   = makeLineEdit(container);

    m_socialTwitter->setPlaceholderText("handle (without @)");
    m_socialInstagram->setPlaceholderText("handle");
    m_socialTiktok->setPlaceholderText("@handle or profile URL");
    m_socialYoutube->setPlaceholderText("channel URL or @handle");
    m_socialFacebook->setPlaceholderText("page URL or handle");
    m_socialTwitch->setPlaceholderText("channel name");
    m_socialLinkedin->setPlaceholderText("profile URL or handle");
    m_socialLinktree->setPlaceholderText("linktr.ee/handle");
    m_socialHashtags->setPlaceholderText("#tag1 #tag2 or tag1,tag2");
    m_socialDiscord->setPlaceholderText("invite link or server ID");

    form->addRow("Twitter/X:",  m_socialTwitter);
    form->addRow("Instagram:",  m_socialInstagram);
    form->addRow("TikTok:",     m_socialTiktok);
    form->addRow("YouTube:",    m_socialYoutube);
    form->addRow("Facebook:",   m_socialFacebook);
    form->addRow("Twitch:",     m_socialTwitch);
    form->addRow("LinkedIn:",   m_socialLinkedin);
    form->addRow("Linktree:",   m_socialLinktree);
    form->addRow("Hashtags:",   m_socialHashtags);
    form->addRow("Discord:",    m_socialDiscord);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildPodcastTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_podcastTitle    = makeLineEdit(container);
    m_podcastEpisode  = makeLineEdit(container);
    m_podcastSeason   = makeLineEdit(container);
    m_podcastFeed     = makeLineEdit(container);
    m_podcastGuid     = makeLineEdit(container);
    m_podcastDuration = makeLineEdit(container);
    m_podcastChapter  = makeLineEdit(container);

    m_podcastTitle->setPlaceholderText("Podcast series title");
    m_podcastEpisode->setPlaceholderText("e.g. 42");
    m_podcastSeason->setPlaceholderText("e.g. 3");
    m_podcastFeed->setPlaceholderText("RSS feed URL");
    m_podcastGuid->setPlaceholderText("Episode GUID (unique identifier)");
    m_podcastDuration->setPlaceholderText("HH:MM:SS");
    m_podcastChapter->setPlaceholderText("Current chapter title");

    form->addRow("Title:",    m_podcastTitle);
    form->addRow("Episode:",  m_podcastEpisode);
    form->addRow("Season:",   m_podcastSeason);
    form->addRow("Feed URL:", m_podcastFeed);
    form->addRow("GUID:",     m_podcastGuid);
    form->addRow("Duration:", m_podcastDuration);
    form->addRow("Chapter:",  m_podcastChapter);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildBroadcastTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_broadcastMode       = new QComboBox(container);
    m_broadcastRelay      = makeLineEdit(container);
    m_broadcastCdn        = makeLineEdit(container);
    m_broadcastCrosspost  = makeLineEdit(container);
    m_broadcastLufs       = makeLineEdit(container);
    m_broadcastCodec      = makeLineEdit(container);
    m_broadcastSamplerate = makeLineEdit(container);
    m_broadcastChannels   = makeLineEdit(container);

    m_broadcastMode->addItems({"live", "rebroadcast", "relay"});
    m_broadcastMode->setMinimumHeight(26);

    m_broadcastRelay->setPlaceholderText("Source stream URL (relay mode)");
    m_broadcastCdn->setPlaceholderText("CDN region hint e.g. us-east");
    m_broadcastCrosspost->setPlaceholderText("Comma-separated crosspost targets");
    m_broadcastLufs->setPlaceholderText("e.g. -14");
    m_broadcastCodec->setPlaceholderText("mp3, opus, aac, flac, vorbis");
    m_broadcastSamplerate->setPlaceholderText("e.g. 44100");
    m_broadcastChannels->setPlaceholderText("1 or 2");

    form->addRow("Mode:",        m_broadcastMode);
    form->addRow("Relay URL:",   m_broadcastRelay);
    form->addRow("CDN Region:",  m_broadcastCdn);
    form->addRow("Crosspost:",   m_broadcastCrosspost);
    form->addRow("LUFS:",        m_broadcastLufs);
    form->addRow("Codec:",       m_broadcastCodec);
    form->addRow("Sample Rate:", m_broadcastSamplerate);
    form->addRow("Channels:",    m_broadcastChannels);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* Icy22EditorWidget::buildContentTab()
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* outer = new QVBoxLayout(container);
    outer->setContentsMargins(10, 10, 10, 10);
    outer->setSpacing(8);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setSpacing(6);
    form->setContentsMargins(0, 0, 0, 0);

    m_contentExplicit = new QCheckBox(container);
    m_contentLive     = new QCheckBox(container);
    m_contentType     = new QComboBox(container);
    m_contentRating   = new QComboBox(container);

    m_contentExplicit->setMinimumHeight(26);
    m_contentLive->setMinimumHeight(26);

    m_contentType->addItems({"radio", "tv", "podcast", "socialcast"});
    m_contentType->setMinimumHeight(26);

    m_contentRating->addItems({"", "G", "PG", "MA", "X"});
    m_contentRating->setMinimumHeight(26);

    form->addRow("Explicit:",      m_contentExplicit);
    form->addRow("Live Broadcast:", m_contentLive);
    form->addRow("Content Type:",  m_contentType);
    form->addRow("Rating:",        m_contentRating);

    outer->addLayout(form);
    outer->addStretch();
    scroll->setWidget(container);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
// applyTheme
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::applyTheme()
{
    const auto tp = ThemePalette::forCurrentTheme();

    // Derive accent press/hover from ThemePalette
    const QString accentPress = tp.borderAccent.name();
    const QString accentHover = tp.accentHover.name();

    const QString qss = QString(R"(
/* ─── Root widget ───────────────────────────────────────────────────────── */
Icy22EditorWidget {
    background-color: %1;
    color: %2;
}

/* ─── QTabWidget / QTabBar ─────────────────────────────────────────────── */
QTabWidget::pane {
    background-color: %3;
    border: 1px solid %4;
    border-top: none;
}
QTabBar::tab {
    background-color: %3;
    color: %5;
    border: 1px solid %4;
    border-bottom: none;
    padding: 5px 14px;
    margin-right: 2px;
    font-size: 12px;
}
QTabBar::tab:selected {
    background-color: %1;
    color: %2;
    border-bottom: 2px solid %6;
}
QTabBar::tab:hover:!selected {
    background-color: %4;
    color: %2;
}

/* ─── QScrollArea ───────────────────────────────────────────────────────── */
QScrollArea {
    background-color: %1;
    border: none;
}
QScrollArea > QWidget > QWidget {
    background-color: %1;
}

/* ─── QLineEdit ────────────────────────────────────────────────────────── */
QLineEdit {
    background-color: %7;
    color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 4px 6px;
    min-height: 26px;
    font-size: 12px;
    selection-background-color: %6;
    selection-color: #ffffff;
}
QLineEdit:focus {
    border-color: %6;
}
QLineEdit:read-only {
    background-color: %3;
    color: %5;
}

/* ─── QTextEdit ────────────────────────────────────────────────────────── */
QTextEdit {
    background-color: %7;
    color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 4px 6px;
    font-size: 12px;
    selection-background-color: %6;
    selection-color: #ffffff;
}
QTextEdit:focus {
    border-color: %6;
}

/* ─── QComboBox ────────────────────────────────────────────────────────── */
QComboBox {
    background-color: %7;
    color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 4px 6px;
    min-height: 26px;
    font-size: 12px;
}
QComboBox:focus {
    border-color: %6;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background-color: %3;
    color: %2;
    border: 1px solid %4;
    selection-background-color: %6;
    selection-color: #ffffff;
}

/* ─── QSpinBox ─────────────────────────────────────────────────────────── */
QSpinBox {
    background-color: %7;
    color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 4px 6px;
    min-height: 26px;
    font-size: 12px;
}
QSpinBox:focus {
    border-color: %6;
}
QSpinBox::up-button, QSpinBox::down-button {
    background-color: %4;
    border: none;
    width: 16px;
}
QSpinBox::up-button:hover, QSpinBox::down-button:hover {
    background-color: %6;
}

/* ─── QTimeEdit ────────────────────────────────────────────────────────── */
QTimeEdit {
    background-color: %7;
    color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 4px 6px;
    min-height: 26px;
    font-size: 12px;
}
QTimeEdit:focus {
    border-color: %6;
}
QTimeEdit::up-button, QTimeEdit::down-button {
    background-color: %4;
    border: none;
    width: 16px;
}

/* ─── QCheckBox ────────────────────────────────────────────────────────── */
QCheckBox {
    color: %2;
    font-size: 12px;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid %4;
    border-radius: 2px;
    background-color: %7;
}
QCheckBox::indicator:checked {
    background-color: %6;
    border-color: %6;
}

/* ─── QPushButton ──────────────────────────────────────────────────────── */
QPushButton {
    background-color: %4;
    color: %2;
    border: 1px solid %4;
    border-radius: 3px;
    padding: 4px 12px;
    font-size: 12px;
}
QPushButton:hover {
    background-color: %6;
    border-color: %6;
    color: #ffffff;
}
QPushButton:pressed {
    background-color: %8;
}
QPushButton#pushNowBtn {
    background-color: %6;
    color: #ffffff;
    font-weight: bold;
    font-size: 12px;
    border: none;
    border-radius: 4px;
    padding: 6px 24px;
}
QPushButton#pushNowBtn:hover {
    background-color: %9;
}
QPushButton#pushNowBtn:pressed {
    background-color: %8;
}

/* ─── QGroupBox ────────────────────────────────────────────────────────── */
QGroupBox {
    background-color: %3;
    color: %5;
    border: 1px solid %4;
    border-radius: 4px;
    margin-top: 8px;
    font-size: 12px;
    font-weight: bold;
    padding-top: 4px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 4px;
    left: 8px;
    color: %6;
}

/* ─── QLabel ────────────────────────────────────────────────────────────── */
QLabel {
    color: %5;
    font-size: 12px;
}

/* ─── QFrame (separator) ────────────────────────────────────────────────── */
QFrame[frameShape="4"],
QFrame[frameShape="5"] {
    border: none;
    background-color: %4;
    max-height: 1px;
    margin: 2px 0px;
}

/* ─── QScrollBar ────────────────────────────────────────────────────────── */
QScrollBar:vertical {
    background: %3;
    width: 8px;
    border-radius: 4px;
}
QScrollBar::handle:vertical {
    background: %4;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background: %6;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0px;
}
QScrollBar:horizontal {
    background: %3;
    height: 8px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal {
    background: %4;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover {
    background: %6;
}
QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
    width: 0px;
}
)")
        .arg(tp.bg.name())           // %1
        .arg(tp.text.name())         // %2
        .arg(tp.panelBg.name())      // %3
        .arg(tp.border.name())       // %4
        .arg(tp.textMuted.name())    // %5
        .arg(tp.accent.name())       // %6
        .arg(tp.inputBg.name())      // %7
        .arg(accentPress)            // %8
        .arg(accentHover);           // %9

    setStyleSheet(qss);
}

// ─────────────────────────────────────────────────────────────────────────────
// populateFrom — fill widget fields from IcyMetadata struct
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::populateFrom(const M1::IcyMetadata& meta)
{
    // ── ICY 1.x ───────────────────────────────────────────────────────────
    m_icy1Display->setText(meta.toIcy1StreamTitle());

    // ── Tab 1: Station ────────────────────────────────────────────────────
    m_stationId->setText(meta.stationId);
    m_stationName->setText(meta.stationName);
    m_stationLogo->setText(meta.stationLogo);
    m_stationGenre->setText(meta.stationGenre);
    m_stationUrl->setText(meta.stationUrl);
    m_stationNotice->setText(meta.stationNotice);
    m_stationLanguage->setText(meta.stationLanguage);

    // ── Tab 2: Show ───────────────────────────────────────────────────────
    m_showTitle->setText(meta.showTitle);
    m_showHost->setText(meta.showHost);

    // showStart / showEnd are stored as ISO 8601 datetime strings.
    // Extract HH:MM from the time portion if present.
    auto parseTime = [](const QString& iso) -> QTime {
        // Accept "HH:MM", "HH:MM:SS", or full ISO datetime.
        const int tIdx = iso.indexOf('T');
        const QString timePart = (tIdx >= 0) ? iso.mid(tIdx + 1) : iso;
        QTime t = QTime::fromString(timePart, "HH:mm:ss");
        if (!t.isValid())
            t = QTime::fromString(timePart, "HH:mm");
        if (!t.isValid())
            t = QTime(0, 0);
        return t;
    };
    m_showStart->setTime(parseTime(meta.showStart));
    m_showEnd->setTime(parseTime(meta.showEnd));

    m_showNext->setText(meta.showNext);
    m_showDescription->setPlainText(meta.showDescription);
    m_showPoster->setText(meta.showPoster);

    // ── Tab 3: Track ──────────────────────────────────────────────────────
    m_trackTitle->setText(meta.trackTitle);
    m_trackArtist->setText(meta.trackArtist);
    m_trackAlbum->setText(meta.trackAlbum);
    m_trackYear->setText(meta.trackYear);
    m_trackGenre->setText(meta.trackGenre);
    m_trackArtwork->setText(meta.trackArtwork);
    m_trackBpm->setText(meta.trackBpm);
    m_trackKey->setText(meta.trackKey);
    m_trackIsrc->setText(meta.trackIsrc);
    m_trackMbid->setText(meta.trackMbid);
    m_trackLabel->setText(meta.trackLabel);
    m_trackComposer->setText(meta.trackComposer);
    m_trackLyricist->setText(meta.trackLyricist);
    m_trackLanguage->setText(meta.trackLanguage);

    // ── Tab 4: DJ ─────────────────────────────────────────────────────────
    m_djHandle->setText(meta.djHandle);
    m_djName->setText(meta.djName);
    m_djBio->setPlainText(meta.djBio);
    m_djAvatar->setText(meta.djAvatar);
    m_djWebsite->setText(meta.djWebsite);
    m_djEmail->setText(meta.djEmail);

    // ── Tab 5: Social ─────────────────────────────────────────────────────
    m_socialTwitter->setText(meta.socialTwitter);
    m_socialInstagram->setText(meta.socialInstagram);
    m_socialTiktok->setText(meta.socialTiktok);
    m_socialYoutube->setText(meta.socialYoutube);
    m_socialFacebook->setText(meta.socialFacebook);
    m_socialTwitch->setText(meta.socialTwitch);
    m_socialLinkedin->setText(meta.socialLinkedin);
    m_socialLinktree->setText(meta.socialLinktree);
    m_socialHashtags->setText(meta.socialHashtags);
    m_socialDiscord->setText(meta.socialDiscord);

    // ── Tab 6: Podcast ────────────────────────────────────────────────────
    m_podcastTitle->setText(meta.podcastTitle);
    m_podcastEpisode->setText(meta.podcastEpisode);
    m_podcastSeason->setText(meta.podcastSeason);
    m_podcastFeed->setText(meta.podcastFeed);
    m_podcastGuid->setText(meta.podcastGuid);
    m_podcastDuration->setText(meta.podcastDuration);
    m_podcastChapter->setText(meta.podcastChapter);

    // ── Tab 7: Broadcast ──────────────────────────────────────────────────
    {
        int modeIdx = m_broadcastMode->findText(meta.broadcastMode);
        m_broadcastMode->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    }
    m_broadcastRelay->setText(meta.broadcastRelay);
    m_broadcastCdn->setText(meta.broadcastCdn);
    m_broadcastCrosspost->setText(meta.broadcastCrosspost);
    m_broadcastLufs->setText(meta.broadcastLufs);
    m_broadcastCodec->setText(meta.broadcastCodec);
    m_broadcastSamplerate->setText(meta.broadcastSamplerate);
    m_broadcastChannels->setText(meta.broadcastChannels);

    // ── Tab 8: Content Flags ──────────────────────────────────────────────
    m_contentExplicit->setChecked(meta.contentExplicit.toLower() == "true");
    m_contentLive->setChecked(meta.contentLive.toLower() == "true");
    {
        int typeIdx = m_contentType->findText(meta.contentType);
        m_contentType->setCurrentIndex(typeIdx >= 0 ? typeIdx : 0);
    }
    {
        int ratingIdx = m_contentRating->findText(meta.contentRating);
        m_contentRating->setCurrentIndex(ratingIdx >= 0 ? ratingIdx : 0);
    }

    // ── DNAS config fields (sourced from module) ───────────────────────────
    if (m_module) {
        m_dnasHost->setText(m_module->dnasHost());
        m_dnasPort->setValue(m_module->dnasPort());
        m_dnasMount->setText(m_module->dnasMount());
        m_dnasUser->setText(m_module->dnasUser());
        m_dnasPass->setText(m_module->dnasPass());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// collectTo — read widget fields into IcyMetadata struct
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::collectTo(M1::IcyMetadata& meta) const
{
    // ── Tab 1: Station ────────────────────────────────────────────────────
    meta.stationId       = m_stationId->text().trimmed();
    meta.stationName     = m_stationName->text().trimmed();
    meta.stationLogo     = m_stationLogo->text().trimmed();
    meta.stationGenre    = m_stationGenre->text().trimmed();
    meta.stationUrl      = m_stationUrl->text().trimmed();
    meta.stationNotice   = m_stationNotice->text().trimmed();
    meta.stationLanguage = m_stationLanguage->text().trimmed();

    // ── Tab 2: Show ───────────────────────────────────────────────────────
    meta.showTitle       = m_showTitle->text().trimmed();
    meta.showHost        = m_showHost->text().trimmed();
    meta.showStart       = m_showStart->time().toString("HH:mm");
    meta.showEnd         = m_showEnd->time().toString("HH:mm");
    meta.showNext        = m_showNext->text().trimmed();
    meta.showDescription = m_showDescription->toPlainText().trimmed();
    meta.showPoster      = m_showPoster->text().trimmed();

    // ── Tab 3: Track ──────────────────────────────────────────────────────
    meta.trackTitle    = m_trackTitle->text().trimmed();
    meta.trackArtist   = m_trackArtist->text().trimmed();
    meta.trackAlbum    = m_trackAlbum->text().trimmed();
    meta.trackYear     = m_trackYear->text().trimmed();
    meta.trackGenre    = m_trackGenre->text().trimmed();
    meta.trackArtwork  = m_trackArtwork->text().trimmed();
    meta.trackBpm      = m_trackBpm->text().trimmed();
    meta.trackKey      = m_trackKey->text().trimmed();
    meta.trackIsrc     = m_trackIsrc->text().trimmed();
    meta.trackMbid     = m_trackMbid->text().trimmed();
    meta.trackLabel    = m_trackLabel->text().trimmed();
    meta.trackComposer = m_trackComposer->text().trimmed();
    meta.trackLyricist = m_trackLyricist->text().trimmed();
    meta.trackLanguage = m_trackLanguage->text().trimmed();

    // ── Tab 4: DJ ─────────────────────────────────────────────────────────
    meta.djHandle  = m_djHandle->text().trimmed();
    meta.djName    = m_djName->text().trimmed();
    meta.djBio     = m_djBio->toPlainText().trimmed();
    meta.djAvatar  = m_djAvatar->text().trimmed();
    meta.djWebsite = m_djWebsite->text().trimmed();
    meta.djEmail   = m_djEmail->text().trimmed();

    // ── Tab 5: Social ─────────────────────────────────────────────────────
    meta.socialTwitter   = m_socialTwitter->text().trimmed();
    meta.socialInstagram = m_socialInstagram->text().trimmed();
    meta.socialTiktok    = m_socialTiktok->text().trimmed();
    meta.socialYoutube   = m_socialYoutube->text().trimmed();
    meta.socialFacebook  = m_socialFacebook->text().trimmed();
    meta.socialTwitch    = m_socialTwitch->text().trimmed();
    meta.socialLinkedin  = m_socialLinkedin->text().trimmed();
    meta.socialLinktree  = m_socialLinktree->text().trimmed();
    meta.socialHashtags  = m_socialHashtags->text().trimmed();
    meta.socialDiscord   = m_socialDiscord->text().trimmed();

    // ── Tab 6: Podcast ────────────────────────────────────────────────────
    meta.podcastTitle    = m_podcastTitle->text().trimmed();
    meta.podcastEpisode  = m_podcastEpisode->text().trimmed();
    meta.podcastSeason   = m_podcastSeason->text().trimmed();
    meta.podcastFeed     = m_podcastFeed->text().trimmed();
    meta.podcastGuid     = m_podcastGuid->text().trimmed();
    meta.podcastDuration = m_podcastDuration->text().trimmed();
    meta.podcastChapter  = m_podcastChapter->text().trimmed();

    // ── Tab 7: Broadcast ──────────────────────────────────────────────────
    meta.broadcastMode       = m_broadcastMode->currentText();
    meta.broadcastRelay      = m_broadcastRelay->text().trimmed();
    meta.broadcastCdn        = m_broadcastCdn->text().trimmed();
    meta.broadcastCrosspost  = m_broadcastCrosspost->text().trimmed();
    meta.broadcastLufs       = m_broadcastLufs->text().trimmed();
    meta.broadcastCodec      = m_broadcastCodec->text().trimmed();
    meta.broadcastSamplerate = m_broadcastSamplerate->text().trimmed();
    meta.broadcastChannels   = m_broadcastChannels->text().trimmed();

    // ── Tab 8: Content Flags ──────────────────────────────────────────────
    meta.contentExplicit = m_contentExplicit->isChecked() ? "true" : "false";
    meta.contentLive     = m_contentLive->isChecked()     ? "true" : "false";
    meta.contentType     = m_contentType->currentText();
    meta.contentRating   = m_contentRating->currentText();

    // Sync ICY 1.x StreamTitle from artist + title
    meta.syncStreamTitle();
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot: onTrackFieldChanged
// Update the ICY 1.x StreamTitle display whenever artist or title changes.
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::onTrackFieldChanged()
{
    const QString artist = m_trackArtist->text().trimmed();
    const QString title  = m_trackTitle->text().trimmed();

    QString display;
    if (!artist.isEmpty() && !title.isEmpty())
        display = artist + " - " + title;
    else if (!artist.isEmpty())
        display = artist;
    else if (!title.isEmpty())
        display = title;

    m_icy1Display->setText(display);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot: onPushNow
// Collect current widget state, update the module, and trigger a DNAS push.
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::onPushNow()
{
    if (!m_module)
        return;

    // Collect all tab data into the module's current metadata.
    M1::IcyMetadata collected;
    collectTo(collected);

    // Update the module's stored metadata.
    m_module->onMetadataUpdate(collected);

    // Read DNAS config fields and update module.
    const QString host  = m_dnasHost->text().trimmed();
    const int     port  = m_dnasPort->value();
    const QString mount = m_dnasMount->text().trimmed();
    const QString user  = m_dnasUser->text().trimmed();
    const QString pass  = m_dnasPass->text();

    m_module->setDnasHost(host);
    m_module->setDnasPort(port);
    m_module->setDnasMount(mount);
    m_module->setDnasUser(user);
    m_module->setDnasPass(pass);

    // Fire-and-forget push: ICY 1.x then ICY 2.2.
    m_module->pushToDnas(host, port, mount, user, pass);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot: onArtworkBrowse
// Open a file dialog for local artwork path, populate artwork URL field.
// ─────────────────────────────────────────────────────────────────────────────
void Icy22EditorWidget::onArtworkBrowse()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Select Artwork",
        QString(),
        "Images (*.png *.jpg *.jpeg *.gif *.webp);;All Files (*)"
    );
    if (!path.isEmpty()) {
        m_trackArtwork->setText(path);
    }
}
