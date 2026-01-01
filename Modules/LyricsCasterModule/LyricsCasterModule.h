#pragma once
/// @file   LyricsCasterModule.h
/// @path   Modules/LyricsCasterModule/LyricsCasterModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-LyricsCaster — Worship Lyrics Display Module
/// @purpose Manages worship song library, section arrangement, and live lyrics
///          projection for the congregation. We handle song lookup, arrangement
///          building, manual/auto advance, blank screen, and CCLI compliance.
///          Rendering is delegated to GraphicsEngineModule.
/// @reason  Most-used module in the Church Surface — worship lyrics display is
///          the primary visual output during every service.
/// @changelog
///   2026-03-09  Initial implementation — song library, sections, arrangement, live control

#include "IModule.h"
#include "IPlugin.h"
#include <QList>
#include <QMap>

class QTimer;

namespace M1 {

class GraphicsEngineModule;

// ─── Song section types ─────────────────────────────────────────────────────
enum class SectionType {
    Verse, Chorus, Bridge, PreChorus, Tag, Intro, Outro, Interlude, Ending
};

inline QString sectionTypeName(SectionType t) {
    switch (t) {
        case SectionType::Verse:      return "Verse";
        case SectionType::Chorus:     return "Chorus";
        case SectionType::Bridge:     return "Bridge";
        case SectionType::PreChorus:  return "Pre-Chorus";
        case SectionType::Tag:        return "Tag";
        case SectionType::Intro:      return "Intro";
        case SectionType::Outro:      return "Outro";
        case SectionType::Interlude:  return "Interlude";
        case SectionType::Ending:     return "Ending";
        default:                      return "Unknown";
    }
}

// ─── Song section — one block of lyrics ─────────────────────────────────────
struct SongSection {
    SectionType type  = SectionType::Verse;
    int         number = 1;          ///< e.g., Verse 1, Verse 2
    QString     text;                ///< Multi-line lyrics text
    int         autoAdvanceSec = 0;  ///< 0 = manual advance only

    QString label() const {
        if (type == SectionType::Chorus || type == SectionType::Bridge ||
            type == SectionType::Tag)
            return sectionTypeName(type);
        return QString("%1 %2").arg(sectionTypeName(type)).arg(number);
    }
};

// ─── Song — complete worship song ───────────────────────────────────────────
struct WorshipSong {
    int         id = 0;              ///< Database ID
    QString     title;
    QString     author;
    QString     copyright;
    QString     ccliNumber;          ///< CCLI license number
    QString     key;                 ///< Musical key (C, D, Em, etc.)
    int         bpm = 0;             ///< Tempo reference for worship team
    QList<SongSection> sections;     ///< All available sections
    QList<int>  arrangement;         ///< Ordered indices into sections[]
};

// ─── LyricsCasterModule ─────────────────────────────────────────────────────
class LyricsCasterModule : public IModule {
    Q_OBJECT

public:
    explicit LyricsCasterModule(QObject* parent = nullptr);
    ~LyricsCasterModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.church.lyrics"; }
    QString displayName() const override { return "Lyrics Caster"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {600, 480}; }
    QSize minimumModuleSize() const override { return {400, 300}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Graphics engine binding ─────────────────────────────────────────────
    void setGraphicsEngine(GraphicsEngineModule* engine) { m_graphics = engine; }

    // ── Song library API ────────────────────────────────────────────────────
    int addSong(const WorshipSong& song);
    void removeSong(int id);
    void updateSong(const WorshipSong& song);
    const WorshipSong* song(int id) const;
    QList<WorshipSong> allSongs() const { return m_songs.values(); }
    QList<WorshipSong> searchSongs(const QString& query) const;

    // ── Live presentation API ───────────────────────────────────────────────
    void loadSong(int songId);
    void nextSection();
    void prevSection();
    void goToSection(int arrangementIndex);
    void setBlank(bool blank);
    bool isBlank() const { return m_blank; }

    // ── Arrangement editing ─────────────────────────────────────────────────
    void setArrangement(int songId, const QList<int>& sectionIndices);
    void insertArrangementItem(int songId, int position, int sectionIndex);
    void removeArrangementItem(int songId, int position);

    // ── Live state ──────────────────────────────────────────────────────────
    int currentSongId() const { return m_currentSongId; }
    int currentArrangementIndex() const { return m_currentIndex; }
    const SongSection* currentSection() const;
    const SongSection* nextSectionPreview() const;
    QImage currentFrame() const { return m_currentFrame; }

signals:
    void songLoaded(int songId);
    void sectionChanged(int arrangementIndex);
    void blankChanged(bool blank);
    void songLibraryChanged();
    void frameUpdated(const QImage& frame);

private slots:
    void onAutoAdvance();

private:
    void renderCurrentSection();

    GraphicsEngineModule*     m_graphics = nullptr;
    QMap<int, WorshipSong>    m_songs;
    int                       m_nextSongId = 1;

    // Live presentation state
    int     m_currentSongId = -1;
    int     m_currentIndex  = -1;
    bool    m_blank         = false;
    QImage  m_currentFrame;
    QTimer* m_autoAdvanceTimer = nullptr;
};

} // namespace M1
