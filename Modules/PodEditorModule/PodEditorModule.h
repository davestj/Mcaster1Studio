#pragma once
/// @file   PodEditorModule.h
/// @path   Modules/PodEditorModule/PodEditorModule.h
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-PodEditor — Non-Destructive Podcast Editor
/// @purpose DAW-style waveform editor for post-production: timeline display,
///          region selection, cut/trim/silence detection stubs, marker
///          navigation, playback with position indicator, and zoom controls.
/// @reason  Podcasters need basic editing capabilities to trim silence,
///          cut errors, and navigate to chapter markers without leaving
///          the Mcaster1Studio environment.
/// @changelog
///   2026-03-09  Initial implementation — waveform timeline, regions, markers, stubs

#include "IModule.h"
#include <QList>
#include <QColor>
#include <QElapsedTimer>
#include <atomic>

class QTimer;
class QMediaPlayer;
class QAudioOutput;

namespace M1 {

// ─── EditorRegion — a selected or marked region ─────────────────────────────
struct EditorRegion {
    qint64  startMs = 0;
    qint64  endMs   = 0;
    QString label;
};

// ─── WaveformData — peak data for display ───────────────────────────────────
struct WaveformData {
    QList<float> peaks;       ///< Normalized peak values [0.0–1.0]
    int          sampleRate  = 48000;
    qint64       durationMs  = 0;
};

// ─── EditorMarker — a named point in time ───────────────────────────────────
struct EditorMarker {
    qint64  positionMs = 0;
    QString label;
    QColor  color = QColor(0x0e, 0xa5, 0xe9);
};

// ─── PodEditorModule ─────────────────────────────────────────────────────────
class PodEditorModule : public IModule {
    Q_OBJECT

public:
    explicit PodEditorModule(QObject* parent = nullptr);
    ~PodEditorModule() override;

    QString moduleId()    const override { return "com.mcaster1.podcast.editor"; }
    QString displayName() const override { return "Podcast Editor"; }
    QString version()     const override { return "1.0.0"; }
    QSize preferredSize()     const override { return {700, 350}; }
    QSize minimumModuleSize() const override { return {500, 250}; }

    void initialize() override;
    void shutdown()   override;
    QWidget* createWidget(QWidget* parent) override;
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // File I/O
    void loadFile(const QString& path);
    QString currentFilePath() const { return m_filePath; }
    bool hasFile() const { return !m_filePath.isEmpty(); }

    // Playback
    void play();
    void pause();
    void stop();
    void seekTo(qint64 ms);
    qint64 position() const;
    qint64 duration() const { return m_waveform.durationMs; }
    bool isPlaying() const { return m_playing.load(std::memory_order_relaxed); }

    // Selection
    void setSelection(qint64 startMs, qint64 endMs);
    void clearSelection();
    EditorRegion selection() const { return m_selection; }
    bool hasSelection() const { return m_selection.endMs > m_selection.startMs; }

    // Edit operations (v1 stubs — emit signals for future implementation)
    void cut();
    void copy();
    void paste();

    // Markers
    void addMarker(qint64 ms, const QString& label);
    void removeMarker(int index);
    const QList<EditorMarker>& markers() const { return m_markers; }

    // Zoom
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    float zoomLevel() const { return m_zoomLevel; }
    float scrollPosition() const { return m_scrollPos; }
    void setScrollPosition(float pos);

    // Waveform data (for widget painting)
    const WaveformData& waveformData() const { return m_waveform; }

signals:
    void fileLoaded(const QString& path);
    void playbackStateChanged(bool playing);
    void positionChanged(qint64 ms);
    void selectionChanged(qint64 startMs, qint64 endMs);
    void markerAdded(int index);
    void markerRemoved(int index);
    void zoomChanged(float level);
    void editCut();
    void editCopy();
    void editPaste();
    void editorError(const QString& msg);

private slots:
    void onPositionTick();
    void onMediaStatusChanged();

private:
    QString        m_filePath;
    WaveformData   m_waveform;
    EditorRegion   m_selection;
    QList<EditorMarker> m_markers;

    // Playback
    QMediaPlayer*  m_player      = nullptr;
    QAudioOutput*  m_audioOutput = nullptr;
    QTimer*        m_posTicker   = nullptr;
    std::atomic<bool> m_playing{false};

    // Zoom / scroll state
    float m_zoomLevel  = 1.0f;   ///< 1.0 = fit-to-width
    float m_scrollPos  = 0.0f;   ///< 0.0–1.0 horizontal scroll

    // Clipboard region (for paste)
    EditorRegion m_clipboard;

    // Helpers
    void generatePlaceholderWaveform(qint64 durationMs);
};

} // namespace M1
