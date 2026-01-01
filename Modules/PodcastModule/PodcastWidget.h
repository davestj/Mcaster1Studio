#pragma once
#include <QWidget>
#include <QTimer>
#include <QList>
#include "PodcastModule.h"

class QPushButton;
class QProgressBar;
class QLabel;
class QLineEdit;
class QListWidget;

namespace M1 { class PodcastModule; }

/// TrackStrip — single track lane in the podcast recorder UI.
/// Shows: track name label + level meter (QProgressBar).
class TrackStrip : public QWidget {
    Q_OBJECT
public:
    explicit TrackStrip(int trackIndex, M1::PodcastModule* module, QWidget* parent = nullptr);
    void     pollLevel();

private:
    int                  m_trackIndex;
    M1::PodcastModule*   m_module;
    QLabel*              m_nameLabel   = nullptr;
    QProgressBar*        m_levelMeter  = nullptr;
};

/// PodcastWidget — primary UI for PodcastModule.
///
/// Sections (top to bottom):
///   1. Time display — HH:MM:SS.mmm elapsed (QTimer 100ms)
///   2. Transport bar — ARM / RECORD / PAUSE / STOP buttons
///   3. Track lanes — 4x TrackStrip (name + level meter)
///   4. Chapter panel — QListWidget + Add Chapter + Remove Chapter buttons
///   5. Export bar — format selector + file path QLineEdit + Browse + Export
///
/// Dark broadcast hardware styling.
class PodcastWidget : public QWidget {
    Q_OBJECT

public:
    explicit PodcastWidget(M1::PodcastModule* module, QWidget* parent = nullptr);
    ~PodcastWidget() override = default;

private slots:
    void onStateChanged(M1::PodcastModule::State newState);
    void onAddChapter();
    void onRemoveChapter();
    void onBrowseExport();
    void onExportWav();
    void onExportFinished(bool success, const QString& path);
    void pollMeters();
    void updateTimeDisplay();

private:
    void buildUi();
    void applyTheme();
    void updateTransportButtons(M1::PodcastModule::State s);
    void refreshChapterList();

    M1::PodcastModule* m_module = nullptr;

    // ── Time display ──────────────────────────────────────────────────────
    QLabel*       m_timeLabel    = nullptr;

    // ── Transport ─────────────────────────────────────────────────────────
    QPushButton*  m_armBtn    = nullptr;
    QPushButton*  m_recBtn    = nullptr;
    QPushButton*  m_pauseBtn  = nullptr;
    QPushButton*  m_stopBtn   = nullptr;

    // ── Track strips ──────────────────────────────────────────────────────
    QList<TrackStrip*> m_tracks;

    // ── Chapter panel ─────────────────────────────────────────────────────
    QListWidget*  m_chapterList = nullptr;
    QPushButton*  m_addChapBtn  = nullptr;
    QPushButton*  m_delChapBtn  = nullptr;

    // ── Export bar ────────────────────────────────────────────────────────
    QLineEdit*    m_exportPath  = nullptr;
    QPushButton*  m_browseBtn   = nullptr;
    QPushButton*  m_exportBtn   = nullptr;
    QLabel*       m_exportStatus = nullptr;

    // ── Timers ────────────────────────────────────────────────────────────
    QTimer m_meterTimer;  ///< 50ms — polls track level meters
    QTimer m_timeTimer;   ///< 100ms — updates elapsed time display
};
