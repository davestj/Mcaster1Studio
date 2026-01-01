#pragma once
#include "IcyMetadata.h"
#include <QWidget>
#include <QTabWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QTimeEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

namespace M1 { class MetadataModule; }

/// Icy22EditorWidget — 8-tab ICY 2.2 metadata editor.
///
/// Tab layout mirrors the ICY 2.2 group structure:
///   1. Station   — station-level identity
///   2. Show      — current show info
///   3. Track     — now-playing track details
///   4. DJ        — on-air DJ profile
///   5. Social    — social media handles
///   6. Podcast   — podcast episode metadata
///   7. Broadcast — technical broadcast parameters
///   8. Content   — content flags and ratings
///
/// Bottom bar:
///   - ICY 1.x StreamTitle display (read-only)
///   - DNAS target connection fields
///   - "Push Now" button
///
/// populateFrom() and collectTo() are the primary data exchange methods.
/// All widget interactions stay on the Qt main thread.
class Icy22EditorWidget : public QWidget {
    Q_OBJECT

public:
    explicit Icy22EditorWidget(M1::MetadataModule* module, QWidget* parent = nullptr);
    ~Icy22EditorWidget() override = default;

    /// Fill all tab fields from a metadata struct.
    void populateFrom(const M1::IcyMetadata& meta);

    /// Read all tab fields into a metadata struct.
    void collectTo(M1::IcyMetadata& meta) const;

private slots:
    void onPushNow();
    void onArtworkBrowse();
    void onTrackFieldChanged();

private:
    // ── Build helpers ─────────────────────────────────────────────────────
    void buildUi();
    void applyTheme();

    QWidget* buildStationTab();
    QWidget* buildShowTab();
    QWidget* buildTrackTab();
    QWidget* buildDjTab();
    QWidget* buildSocialTab();
    QWidget* buildPodcastTab();
    QWidget* buildBroadcastTab();
    QWidget* buildContentTab();

    static QLineEdit* makeLineEdit(QWidget* parent = nullptr);
    static QTextEdit* makeTextEdit(QWidget* parent = nullptr);

    // ── Back-reference ────────────────────────────────────────────────────
    M1::MetadataModule* m_module = nullptr;

    // ── Top-level layout widgets ──────────────────────────────────────────
    QTabWidget*  m_tabs              = nullptr;

    // ── Tab 1: Station ────────────────────────────────────────────────────
    QLineEdit* m_stationId       = nullptr;
    QLineEdit* m_stationName     = nullptr;
    QLineEdit* m_stationLogo     = nullptr;
    QLineEdit* m_stationGenre    = nullptr;
    QLineEdit* m_stationUrl      = nullptr;
    QLineEdit* m_stationNotice   = nullptr;
    QLineEdit* m_stationLanguage = nullptr;

    // ── Tab 2: Show ───────────────────────────────────────────────────────
    QLineEdit* m_showTitle       = nullptr;
    QLineEdit* m_showHost        = nullptr;
    QTimeEdit* m_showStart       = nullptr;
    QTimeEdit* m_showEnd         = nullptr;
    QLineEdit* m_showNext        = nullptr;
    QTextEdit* m_showDescription = nullptr;
    QLineEdit* m_showPoster      = nullptr;

    // ── Tab 3: Track ──────────────────────────────────────────────────────
    QLineEdit*   m_trackTitle    = nullptr;
    QLineEdit*   m_trackArtist   = nullptr;
    QLineEdit*   m_trackAlbum    = nullptr;
    QLineEdit*   m_trackYear     = nullptr;
    QLineEdit*   m_trackGenre    = nullptr;
    QLineEdit*   m_trackArtwork  = nullptr;
    QPushButton* m_artworkBrowse = nullptr;
    QLineEdit*   m_trackBpm      = nullptr;
    QLineEdit*   m_trackKey      = nullptr;
    QLineEdit*   m_trackIsrc     = nullptr;
    QLineEdit*   m_trackMbid     = nullptr;
    QLineEdit*   m_trackLabel    = nullptr;
    QLineEdit*   m_trackComposer = nullptr;
    QLineEdit*   m_trackLyricist = nullptr;
    QLineEdit*   m_trackLanguage = nullptr;

    // ── Tab 4: DJ ─────────────────────────────────────────────────────────
    QLineEdit* m_djHandle  = nullptr;
    QLineEdit* m_djName    = nullptr;
    QTextEdit* m_djBio     = nullptr;
    QLineEdit* m_djAvatar  = nullptr;
    QLineEdit* m_djWebsite = nullptr;
    QLineEdit* m_djEmail   = nullptr;

    // ── Tab 5: Social ─────────────────────────────────────────────────────
    QLineEdit* m_socialTwitter   = nullptr;
    QLineEdit* m_socialInstagram = nullptr;
    QLineEdit* m_socialTiktok    = nullptr;
    QLineEdit* m_socialYoutube   = nullptr;
    QLineEdit* m_socialFacebook  = nullptr;
    QLineEdit* m_socialTwitch    = nullptr;
    QLineEdit* m_socialLinkedin  = nullptr;
    QLineEdit* m_socialLinktree  = nullptr;
    QLineEdit* m_socialHashtags  = nullptr;
    QLineEdit* m_socialDiscord   = nullptr;

    // ── Tab 6: Podcast ────────────────────────────────────────────────────
    QLineEdit* m_podcastTitle    = nullptr;
    QLineEdit* m_podcastEpisode  = nullptr;
    QLineEdit* m_podcastSeason   = nullptr;
    QLineEdit* m_podcastFeed     = nullptr;
    QLineEdit* m_podcastGuid     = nullptr;
    QLineEdit* m_podcastDuration = nullptr;
    QLineEdit* m_podcastChapter  = nullptr;

    // ── Tab 7: Broadcast ──────────────────────────────────────────────────
    QComboBox* m_broadcastMode       = nullptr;
    QLineEdit* m_broadcastRelay      = nullptr;
    QLineEdit* m_broadcastCdn        = nullptr;
    QLineEdit* m_broadcastCrosspost  = nullptr;
    QLineEdit* m_broadcastLufs       = nullptr;
    QLineEdit* m_broadcastCodec      = nullptr;
    QLineEdit* m_broadcastSamplerate = nullptr;
    QLineEdit* m_broadcastChannels   = nullptr;

    // ── Tab 8: Content Flags ──────────────────────────────────────────────
    QCheckBox* m_contentExplicit = nullptr;
    QCheckBox* m_contentLive     = nullptr;
    QComboBox* m_contentType     = nullptr;
    QComboBox* m_contentRating   = nullptr;

    // ── Bottom bar ────────────────────────────────────────────────────────
    QLineEdit*   m_icy1Display   = nullptr;  // read-only "Artist - Title"
    QPushButton* m_pushNowBtn    = nullptr;

    // DNAS target config
    QLineEdit* m_dnasHost  = nullptr;
    QSpinBox*  m_dnasPort  = nullptr;
    QLineEdit* m_dnasMount = nullptr;
    QLineEdit* m_dnasUser  = nullptr;
    QLineEdit* m_dnasPass  = nullptr;
};
