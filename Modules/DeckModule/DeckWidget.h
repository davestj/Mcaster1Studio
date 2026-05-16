#pragma once
#include "DeckPlayer.h"
#include "MediaItem.h"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QComboBox>
#include <QListWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <atomic>
#include <array>
#include <cmath>

class QVBoxLayout;
class CrossfaderWidget;
class PlaylistWidget;
namespace M1 { class PTTModule; class PlaylistModule; }

// ─── DeckInlineMeter ─────────────────────────────────────────────────────────
/// Compact hi-res vertical L/R VU meter with smooth continuous-fill bars.
/// Broadcast-standard dark background, 3-zone coloring (green/amber/red),
/// peak-hold ticks (3s hold + decay), and dB scale.
class DeckInlineMeter : public QWidget {
    Q_OBJECT
public:
    explicit DeckInlineMeter(QWidget* parent = nullptr);
    void setLevels(float l, float r);

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void onTimer();

private:
    static float linToDb(float v) {
        return (v > 1e-6f) ? 20.0f * std::log10f(v) : -60.0f;
    }

    std::atomic<float> m_rawL{0.0f}, m_rawR{0.0f};
    float m_dispL = 0.0f, m_dispR = 0.0f;
    float m_peakL = 0.0f, m_peakR = 0.0f;
    int   m_holdL = 0,    m_holdR = 0;
    QTimer* m_timer = nullptr;

    static constexpr float kDbMin  = -60.0f;
    static constexpr float kDbClip =   0.0f;
};

// ─── DeckPanel ───────────────────────────────────────────────────────────────
/// Single broadcast deck panel — light sandy/tan theme matching SAM4 reference.
///
/// Layout (top → bottom):
///   [Header: blue bar with deck letter + artist/title + state badge]
///   [Art 56×56] [Stats grid: Cur/Tot/Rem/BPM | kbps/kHz/Stereo] [Vol fader] [L|R VU]
///   [Seek slider — full width]
///   [Transport: ▶ ■ ⏭ | CP CUE LOOP | H1 H2 H3 H4 | V P M AIR CUE]
///   [Tab widget — History | Library | Playlist | Queue (expanding)]
class DeckPanel : public QWidget {
    Q_OBJECT

public:
    explicit DeckPanel(M1::DeckPlayer* player, int deckIndex,
                       QWidget* parent = nullptr);

    void dropFile(const QString& path);

    // ── Data setters (called by DeckModule/MainWindow to populate tabs) ───
    void setLibraryItems(const QList<M1::MediaItem>& items);
    void setPlaylistItems(const QList<M1::MediaItem>& items);
    void setQueueItems(const QList<M1::MediaItem>& items);

signals:
    void fileDropped(const QString& path, int deckIndex);
    void eqRequested(int deckIndex);
    void loadNextFromQueueRequested(int deckIndex);
    void loadFromLibraryRequested(int deckIndex);
    void addPlaylistToQueueRequested(const QString& playlistPath);

    // ── Browser context menu actions ─────────────────────────────────────
    void loadToDeckRequested(const QString& filePath, int targetDeck);
    void addToQueueRequested(const M1::MediaItem& item);
    void editTagsRequested(const M1::MediaItem& item);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* e) override;

private slots:
    void onStateChanged(M1::DeckPlayer::State state);
    void onPositionChanged(qint64 frame);
    void onBpmDetected(float bpm);
    void onHotCuesChanged();
    void onTagsLoaded();
    void onPollTimer();
    void onVolumeChanged(int value);
    void onPitchChanged(int value);
    void onSliderMode(int mode);
    void onSeekMoved(int value);
    void onArtworkSearchReply(QNetworkReply* reply);
    void onArtworkImageReply(QNetworkReply* reply);
    void onPlayBtnEmptyMenu();

private:
    void setupUi();
    void setupBrowserTabs();
    void updateTimeDisplay();
    void updateSeekSlider();
    void updateTransportButtons(M1::DeckPlayer::State state);
    void updateBpmDisplay();
    void fetchAlbumArt(const QString& artist, const QString& title);
    QString formatTime(double seconds);
    static QIcon svgIcon(const QString& path, int sz = 18);

    void showBrowserContextMenu(const QPoint& pos, QListWidget* list);
    void showLibraryContextMenu(const QPoint& pos);
    M1::MediaItem itemFromListRow(QListWidget* list, int row) const;
    void populateListFromItems(QListWidget* list, const QList<M1::MediaItem>& items);

    M1::DeckPlayer* m_player;
    int             m_deckIndex;
    int             m_sliderMode = 0;   // 0=Volume, 1=Pitch
    float           m_baseBpm    = 0.0f;  // detected BPM at native speed

    // Header
    QLabel* m_artistLabel  = nullptr;
    QLabel* m_titleLabel   = nullptr;
    QLabel* m_stateLabel   = nullptr;

    // Album art
    QLabel* m_artLabel     = nullptr;

    // Stats
    QLabel* m_curLabel     = nullptr;
    QLabel* m_totLabel     = nullptr;
    QLabel* m_remLabel     = nullptr;
    QLabel* m_bitrateLabel = nullptr;
    QLabel* m_kHzLabel     = nullptr;
    QLabel* m_stereoLabel  = nullptr;
    QLabel* m_bpmLabel     = nullptr;

    // BPM nudge
    QPushButton* m_bpmMinusBtn = nullptr;
    QPushButton* m_bpmPlusBtn  = nullptr;

    // Seek slider
    QSlider* m_seekSlider  = nullptr;

    // Transport
    QPushButton* m_playBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_nextBtn = nullptr;
    QPushButton* m_cueBtn  = nullptr;
    QPushButton* m_loopBtn = nullptr;
    QPushButton* m_eqBtn   = nullptr;

    // Hot cue pads
    std::array<QPushButton*, 4> m_hotBtns{};

    // Volume/pitch fader column
    QSlider*     m_fader       = nullptr;
    QPushButton* m_volModeBtn  = nullptr;
    QPushButton* m_pitchModeBtn= nullptr;
    QLabel*      m_faderLabel  = nullptr;
    QPushButton* m_muteBtn     = nullptr;
    QPushButton* m_airBtn      = nullptr;
    QPushButton* m_cueOutBtn   = nullptr;

    // VU meter
    DeckInlineMeter* m_vuMeter   = nullptr;

    // ── Browser tabs (replacing standalone historyList) ──────────────────
    QTabWidget*      m_browserTabs  = nullptr;
    QListWidget*     m_historyList  = nullptr;   // Tab 0: History
    QTableWidget*    m_libraryTable = nullptr;   // Tab 1: Library
    QListWidget*     m_playlistList = nullptr;   // Tab 2: Playlist
    QListWidget*     m_queueList    = nullptr;   // Tab 3: Queue

    // EQ panel (collapsible strip, toggled by m_eqBtn)
    QWidget*     m_eqPanel    = nullptr;
    QSlider*     m_eqSlider[3]{};   // range -24..24 → ±12 dB (×0.5)
    QLabel*      m_eqValue [3]{};   // dB readout per band

    // Album art network fetch
    QNetworkAccessManager* m_nam = nullptr;

    QTimer* m_pollTimer = nullptr;
};

// ─── DeckWidget (DeckPlayer Module) ──────────────────────────────────────────
/// Layout:
///   ┌──────────┬──────────────┬──────────┐
///   │          │  CROSSFADER  │          │
///   │          │     PTT      │          │
///   │  Deck A  │──────────────│  Deck B  │
///   │          │  PLAYLIST    │          │
///   │          │  AUTO DJ     │          │
///   └──────────┴──────────────┴──────────┘
///
/// The Playlist/AutoDJ module is EMBEDDED in the center column between
/// the decks, underneath the crossfader and PTT panel.
/// It is auto-created by MainWindow whenever a deck exists.
///
/// DO NOT REMOVE THE AUTO DJ MODULE FROM THIS WIDGET.
/// DO NOT TOUCH THE PLAYLIST/AUTODJ EMBEDDING CODE.
class DeckWidget : public QWidget {
    Q_OBJECT

public:
    explicit DeckWidget(M1::DeckPlayer* deckA,
                        M1::DeckPlayer* deckB,
                        QWidget*        parent = nullptr);

    CrossfaderWidget* crossfader()      const { return m_crossfader; }
    float             crossfaderValue() const;

    /// Attach a PTT module — reveals the compact strip below the decks.
    void setPTTModule(M1::PTTModule* ptt);

    /// Embed Playlist/AutoDJ in the center column below crossfader+PTT.
    void setPlaylistModule(M1::PlaylistModule* playlist);

    /// Populate browser tabs on both panels (called from MainWindow)
    void setLibraryItems(const QList<M1::MediaItem>& items);
    void setPlaylistItems(const QList<M1::MediaItem>& items);
    void setQueueItems(const QList<M1::MediaItem>& items);

    DeckPanel* panelA() const { return m_panelA; }
    DeckPanel* panelB() const { return m_panelB; }

signals:
    void loadNextFromQueueRequested(int deckIndex);
    void loadFromLibraryRequested(int deckIndex);
    void addPlaylistToQueueRequested(const QString& path);
    void loadToDeckRequested(const QString& filePath, int targetDeck);
    void addToQueueRequested(const M1::MediaItem& item);
    void editTagsRequested(const M1::MediaItem& item);

private:
    DeckPanel*        m_panelA     = nullptr;
    DeckPanel*        m_panelB     = nullptr;
    CrossfaderWidget* m_crossfader = nullptr;

    // ── PTT panel ──
    M1::PTTModule*    m_pttMod    = nullptr;
    QWidget*          m_pttPanel  = nullptr;
    QPushButton*      m_pttBtn    = nullptr;
    QLabel*           m_pttLed    = nullptr;
    QComboBox*        m_micCombo  = nullptr;
    QProgressBar*     m_pttMeter  = nullptr;
    QTimer*           m_pttPoll   = nullptr;

    // ── Embedded Playlist/AutoDJ ──
    QVBoxLayout*      m_centerCol = nullptr;
    PlaylistWidget*   m_playlistWidget = nullptr;
};
