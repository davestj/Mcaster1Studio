#pragma once
/// @file   PodSoundboardModule.h
/// @path   Modules/PodSoundboardModule/PodSoundboardModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodSoundboard — Instant Audio Triggering Grid
/// @purpose Configurable pad grid for triggering sound effects, stingers,
///          music beds, and intros. Supports banks, keyboard shortcuts,
///          loop/one-shot/toggle modes, and sidechain ducking.
/// @reason  Podcasters need instant access to sound effects and music beds
///          during recording without switching to external apps.
/// @changelog
///   2026-03-09  Initial implementation — pad grid, banks, trigger modes

#include "IModule.h"
#include <QList>
#include <QColor>
#include <QUrl>
#include <atomic>

class QMediaPlayer;
class QAudioOutput;

namespace M1 {

// ─── PadTriggerMode ─────────────────────────────────────────────────────────
enum class PadTriggerMode { OneShot, Loop, Toggle, Hold };

// ─── SoundPad — one pad in the grid ─────────────────────────────────────────
struct SoundPad {
    int     id       = 0;
    QString label;
    QUrl    filePath;
    QColor  color    = QColor(0x33, 0x33, 0x33);
    float   volume   = 1.0f;
    float   fadeInMs  = 0.0f;
    float   fadeOutMs = 0.0f;
    PadTriggerMode mode = PadTriggerMode::OneShot;
    QString shortcutKey;

    QJsonObject toJson() const;
    static SoundPad fromJson(const QJsonObject& obj);
};

// ─── SoundBank — a named set of pads ────────────────────────────────────────
struct SoundBank {
    QString name;
    QList<SoundPad> pads;

    QJsonObject toJson() const;
    static SoundBank fromJson(const QJsonObject& obj);
};

// ─── PodSoundboardModule ────────────────────────────────────────────────────
class PodSoundboardModule : public IModule {
    Q_OBJECT

public:
    explicit PodSoundboardModule(QObject* parent = nullptr);
    ~PodSoundboardModule() override;

    QString moduleId()    const override { return "com.mcaster1.podcast.soundboard"; }
    QString displayName() const override { return "Podcast Soundboard"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {500, 400}; }
    QSize minimumModuleSize() const override { return {300, 250}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Grid size
    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    void setGridSize(int rows, int cols);

    // Bank management
    int addBank(const QString& name);
    void removeBank(int index);
    void switchBank(int index);
    int currentBankIndex() const { return m_currentBank; }
    int bankCount() const { return m_banks.size(); }
    SoundBank& bank(int i) { return m_banks[i]; }
    const SoundBank& currentBank() const { return m_banks[m_currentBank]; }

    // Pad operations
    void setPad(int padIndex, const SoundPad& pad);
    const SoundPad* pad(int padIndex) const;
    void triggerPad(int padIndex);
    void stopPad(int padIndex);
    void stopAll();
    bool isPadPlaying(int padIndex) const;

    // Sidechain ducking
    void setDuckingEnabled(bool on) { m_duckEnabled = on; }
    bool duckingEnabled() const { return m_duckEnabled; }
    void setDuckLevel(float level) { m_duckLevel = level; } // 0.0–1.0

    // Import/export
    bool saveBank(int bankIndex, const QString& filePath);
    bool loadBank(const QString& filePath);

signals:
    void padTriggered(int padIndex);
    void padStopped(int padIndex);
    void bankChanged(int bankIndex);
    void gridSizeChanged(int rows, int cols);

private:
    int m_rows = 4;
    int m_cols = 4;
    int m_currentBank = 0;
    QList<SoundBank> m_banks;
    bool  m_duckEnabled = false;
    float m_duckLevel   = 0.3f;

    // Playback (one player per pad in current bank)
    struct PadPlayer {
        QMediaPlayer* player = nullptr;
        QAudioOutput* audio  = nullptr;
        bool playing = false;
    };
    QList<PadPlayer> m_players;
    void ensurePlayers(int count);
    int m_nextPadId = 1;
};

} // namespace M1
