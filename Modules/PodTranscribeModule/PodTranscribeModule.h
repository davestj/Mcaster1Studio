#pragma once
/// @file   PodTranscribeModule.h
/// @path   Modules/PodTranscribeModule/PodTranscribeModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodTranscribe — Podcast Transcription Module
/// @purpose Speech-to-text transcription with speaker diarization, editable
///          timestamped transcript entries, and export to text/SRT/VTT.
///          STT engine is stubbed for v1 — integration with Whisper planned
///          for v2. Manual transcript editing and chapter generation work now.
/// @reason  Podcast accessibility requires text transcripts for hearing-
///          impaired audiences, SEO, and show notes generation.
/// @changelog
///   2026-03-09  Initial implementation — transcript model, speaker labels,
///               export (text/SRT/VTT), chapter generation, editable segments

#include "IModule.h"
#include <QList>
#include <QPair>
#include <QStringList>

namespace M1 {

// ─── TranscriptSegment — a timestamped speech segment ───────────────────────
struct TranscriptSegment {
    qint64  startMs    = 0;       ///< Segment start time in milliseconds
    qint64  endMs      = 0;       ///< Segment end time in milliseconds
    QString speaker;              ///< Speaker label (e.g. "Host", "Guest 1")
    QString text;                 ///< Transcribed text content
    double  confidence = 0.0;     ///< STT confidence score 0.0–1.0 (0 = manual entry)
};

// ─── PodTranscribeModule ────────────────────────────────────────────────────
class PodTranscribeModule : public IModule {
    Q_OBJECT

public:
    explicit PodTranscribeModule(QObject* parent = nullptr);
    ~PodTranscribeModule() override;

    // ── IModule identity ────────────────────────────────────────────────────
    QString moduleId()    const override { return "com.mcaster1.podcast.transcribe"; }
    QString displayName() const override { return "Podcast Transcription"; }
    QString version()     const override { return "1.0.0"; }

    QSize preferredSize()     const override { return {550, 400}; }
    QSize minimumModuleSize() const override { return {400, 300}; }

    // ── IModule lifecycle ───────────────────────────────────────────────────
    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Audio file for future STT ───────────────────────────────────────────
    void    loadAudioFile(const QString& path);
    QString audioFilePath() const { return m_audioPath; }

    // ── Segment management ──────────────────────────────────────────────────
    void addSegment(qint64 startMs, qint64 endMs, const QString& speaker,
                    const QString& text);
    void editSegment(int index, const QString& text);
    void removeSegment(int index);
    QList<TranscriptSegment> segments() const { return m_segments; }
    int segmentCount() const { return m_segments.size(); }
    void clearTranscript();

    // ── Speaker management ──────────────────────────────────────────────────
    void setSpeakerLabel(int index, const QString& label);
    QStringList availableSpeakers() const { return m_speakers; }
    void addSpeaker(const QString& name);
    void removeSpeaker(const QString& name);

    // ── Export ───────────────────────────────────────────────────────────────
    bool exportText(const QString& path);
    bool exportSrt(const QString& path);
    bool exportVtt(const QString& path);

    // ── Chapter generation ──────────────────────────────────────────────────
    QList<QPair<qint64, QString>> generateChapters() const;
    void addChapterMarker(qint64 timestampMs, const QString& title);
    void removeChapterMarker(int index);
    QList<QPair<qint64, QString>> chapterMarkers() const { return m_chapters; }

signals:
    void audioFileChanged(const QString& path);
    void segmentsChanged();
    void speakersChanged();
    void chaptersChanged();
    void positionRequested(qint64 positionMs);  ///< Emitted on timestamp click
    void exportFinished(bool success, const QString& path);

private:
    // Audio source
    QString m_audioPath;

    // Transcript segments
    QList<TranscriptSegment> m_segments;

    // Speaker labels
    QStringList m_speakers;

    // Chapter markers
    QList<QPair<qint64, QString>> m_chapters;
};

} // namespace M1
