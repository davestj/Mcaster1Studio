#pragma once
#include <portaudio.h>
#include "IModule.h"
#include <QMap>
#include <QTimer>
#include <QNetworkAccessManager>
#include <atomic>
#include <array>
#include <vector>

class QWidget;
class QVBoxLayout;
class QGridLayout;
class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QMediaPlayer;
class QAudioOutput;
class QLineEdit;
class QNetworkReply;
class QTabWidget;
class QListWidget;
class QSpinBox;
class QCheckBox;
class DeckInlineMeter;
class WaveformView;

namespace M1 {

class DeckPlayer;

/// AuxDeckModule -- full-featured auxiliary deck with per-deck audio device routing,
/// recording capability, track history, and CUE bus output.
///
/// Each instance is a self-contained player with its own configurable
/// CUE OUT, AIR OUT, and optional Mcaster1AudioPipe virtual device.
/// Users create AUX Decks, give them custom names (e.g. "Jingle Deck",
/// "Interview Playback"), and assign independent audio outputs.
///
/// Features:
///   - Seek slider, 4 hot cues, loop control, cue point
///   - Pitch/speed control, BPM display with nudge +/- buttons
///   - Album art display, metadata stats grid, state badge
///   - WaveformView, DeckInlineMeter VU
///   - AIR OUT / CUE OUT device combos (IMMEDIATE apply on selection)
///   - Recording to WAV with configurable output path
///   - Track history list (last 50 played)
///   - Recordings browser with play/explore/delete
///   - CUE bus output for headphone monitoring
///
/// Audio routing:
///   AIR OUT  -- main program output device (goes on-air)
///   CUE OUT  -- headphone/preview output device (pre-listen)
///   Both devices are selectable per-deck, independent of global device.
///   Mcaster1AudioPipe virtual devices appear in device lists automatically.
class AuxDeckModule : public IModule {
    Q_OBJECT

public:
    explicit AuxDeckModule(QObject* parent = nullptr);
    ~AuxDeckModule() override;

    // IModule identity
    QString moduleId()    const override { return "com.mcaster1.auxdeck"; }
    QString displayName() const override;
    QString version()     const override { return "3.0.0"; }
    QSize   preferredSize() const override { return {520, 440}; }
    QSize   minimumModuleSize() const override { return {400, 340}; }

    // Lifecycle
    void initialize() override;
    void shutdown()   override;

    // UI
    QWidget* createWidget(QWidget* parent) override;

    // Audio (RT thread)
    void onAudioBlock(AudioBuffer& in, AudioBuffer& out) override;

    // State persistence
    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // Custom name for this AUX deck instance
    QString deckName() const { return m_deckName; }
    void setDeckName(const QString& name);

    // Per-deck audio device configuration
    int airOutDeviceIndex() const { return m_airOutDevice; }
    int cueOutDeviceIndex() const { return m_cueOutDevice; }
    void setAirOutDevice(int deviceIndex);
    void setCueOutDevice(int deviceIndex);

    // Playback controls
    void loadFile(const QString& filePath);
    void play();
    void pause();
    void stop();
    void setCueActive(bool active);

    // Volume
    float volume() const { return m_volume.load(); }
    void setVolume(float v);

    // Levels (for VU display)
    float peakL() const { return m_peakL.load(); }
    float peakR() const { return m_peakR.load(); }

    // CUE bus output (mirrors DeckAModule pattern for MainWindow integration)
    const float* cueMix() const { return m_cueBuf.data(); }
    int cueMixFrames() const { return m_cueMixFrames; }

public slots:
    /// Load a file and immediately play it on the CUE device.
    /// Used by "Play in AUX Deck on CUE device" context menu.
    void loadAndPlayOnCue(const QString& filePath);

signals:
    void deckNameChanged(const QString& name);
    void deviceConfigChanged();
    void playbackStateChanged(int state); // 0=stopped, 1=playing, 2=paused

private slots:
    void onPollTimer();
    void onStateChanged();
    void onTagsLoaded();
    void onBpmDetected(float bpm);
    void onHotCuesChanged();
    void onSeekMoved(int value);
    void onVolumeChanged(int value);
    void onPitchChanged(int value);
    void onSliderMode(int mode);
    void onArtworkReply(QNetworkReply* reply);

private:
    void refreshDeviceList();
    void buildWidget(QWidget* container);
    void updateTimeDisplay();
    void updateSeekSlider();
    void updateBpmDisplay();
    void updateTransportButtons();
    void fetchAlbumArt(const QString& artist, const QString& title);
    QString formatTime(double seconds);

    // Recording helpers
    void startRecording();
    void stopRecording();
    void saveRecordingWav(const QString& filePath);
    void refreshRecordingsList();
    void addTrackHistoryEntry(const QString& artist, const QString& title, double durationSec);
    QString recordingFilePath() const;

    QString m_deckName = "AUX Deck";
    int m_airOutDevice = -1; // -1 = use global default
    int m_cueOutDevice = -1; // -1 = use global default

    std::atomic<float> m_volume{1.0f};
    std::atomic<float> m_peakL{0.0f};
    std::atomic<float> m_peakR{0.0f};
    std::atomic<bool>  m_cueActive{false};

    DeckPlayer* m_player = nullptr;
    QTimer*     m_pollTimer = nullptr;

    int   m_sliderMode = 0;   // 0=Volume, 1=Pitch
    float m_baseBpm    = 0.0f; // detected BPM at native speed

    // Album art network fetch
    QNetworkAccessManager* m_nam = nullptr;

    // CUE bus (mirrors DeckAModule pattern)
    std::vector<float> m_cueBuf;
    std::vector<float> m_tmpBuf;
    int                m_cueMixFrames = 0;

    // Per-bus PortAudio output streams — AIR and CUE are fully independent
    struct AudioBus {
        PaStream* stream = nullptr;
        std::vector<float> ring;
        std::atomic<int> ringW{0};
        std::atomic<int> ringR{0};
        void open(int deviceIndex, void* userData);
        void close();
        void write(const float* data, int frames, int channels);
    };
    AudioBus m_airBus;
    AudioBus m_cueBus;
    static constexpr int kBusRingFrames = 1 << 16;
    static constexpr int kBusRingMask = kBusRingFrames - 1;
    static int paBusCallback(const void* in, void* out, unsigned long frames,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags flags, void* userData);

    // Recording state
    std::atomic<bool>  m_recording{false};
    std::vector<float> m_recBuffer;       // accumulated samples during recording
    int                m_recSampleRate = 48000;
    int                m_recChannels   = 2;

    // Track history (max 50 entries)
    QStringList m_trackHistory;
    static constexpr int kMaxHistory = 50;

    // Recording config
    QString m_recOutputPath;   // recording output directory
    int     m_recFormatIndex = 0; // 0=WAV, 1=FLAC, 2=MP3, 3=Opus, 4=OGG Vorbis
    bool    m_recAutoNaming  = true;
    QString m_recPattern     = "AuxDeck_{name}_{date}_{time}";
    int     m_defaultVolume  = 100;

    // UI widgets (owned by createWidget parent)
    QWidget*    m_widget = nullptr;

    // Header row
    QLineEdit*  m_nameEdit = nullptr;
    QLabel*     m_stateLabel = nullptr;

    // Device combos
    QComboBox*  m_airOutCombo = nullptr;
    QComboBox*  m_cueOutCombo = nullptr;

    // Album art
    QLabel*     m_artLabel = nullptr;

    // Metadata labels
    QLabel*     m_artistLabel = nullptr;
    QLabel*     m_titleLabel = nullptr;
    QLabel*     m_albumLabel = nullptr;

    // Stats grid
    QLabel*     m_curLabel = nullptr;
    QLabel*     m_totLabel = nullptr;
    QLabel*     m_remLabel = nullptr;
    QLabel*     m_bitrateLabel = nullptr;
    QLabel*     m_kHzLabel = nullptr;
    QLabel*     m_stereoLabel = nullptr;
    QLabel*     m_bpmLabel = nullptr;

    // BPM nudge
    QPushButton* m_bpmMinusBtn = nullptr;
    QPushButton* m_bpmPlusBtn  = nullptr;

    // Seek slider
    QSlider*    m_seekSlider = nullptr;

    // Transport
    QPushButton* m_playBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_ejectBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_recBtn = nullptr;   // Record button

    // Cue / Loop
    QPushButton* m_cpBtn = nullptr;
    QPushButton* m_cueJmpBtn = nullptr;
    QPushButton* m_loopBtn = nullptr;

    // Hot cue pads
    std::array<QPushButton*, 4> m_hotBtns{};

    // Volume/pitch fader column
    QSlider*     m_fader = nullptr;
    QPushButton* m_volModeBtn = nullptr;
    QPushButton* m_pitchModeBtn = nullptr;
    QLabel*      m_faderLabel = nullptr;
    QPushButton* m_airToggle = nullptr;  ///< Toggle AIR output on/off
    QPushButton* m_muteBtn = nullptr;
    QPushButton* m_cueToggle = nullptr;  ///< Toggle CUE output on/off

    // VU meter
    DeckInlineMeter* m_vuMeter = nullptr;

    // Tabbed panel (replaces waveform + EQ panel)
    QTabWidget*  m_tabWidget = nullptr;
    QListWidget* m_historyList = nullptr;
    QListWidget* m_recordingsList = nullptr;

    // Config tab widgets
    QComboBox*   m_recFormatCombo = nullptr;
    QLineEdit*   m_recPathEdit = nullptr;
    QCheckBox*   m_autoNamingCheck = nullptr;
    QLineEdit*   m_autoNamingPattern = nullptr;
    QSpinBox*    m_defaultVolSpin = nullptr;
};

} // namespace M1
