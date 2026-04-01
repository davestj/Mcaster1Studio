#pragma once
#include "IModule.h"
#include <QMap>
#include <QTimer>
#include <atomic>

class QWidget;
class QVBoxLayout;
class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QMediaPlayer;
class QAudioOutput;
class QLineEdit;

namespace M1 {

class DeckPlayer;

/// AuxDeckModule — auxiliary deck with per-deck audio device routing.
///
/// Each instance is a self-contained player with its own configurable
/// CUE OUT, AIR OUT, and optional Mcaster1AudioPipe virtual device.
/// Users create AUX Decks, give them custom names (e.g. "Jingle Deck",
/// "Interview Playback"), and assign independent audio outputs.
///
/// Audio routing:
///   AIR OUT  — main program output device (goes on-air)
///   CUE OUT  — headphone/preview output device (pre-listen)
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
    QString version()     const override { return "1.0.0"; }
    QSize   preferredSize() const override { return {400, 300}; }
    QSize   minimumModuleSize() const override { return {300, 200}; }

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

signals:
    void deckNameChanged(const QString& name);
    void deviceConfigChanged();
    void playbackStateChanged(int state); // 0=stopped, 1=playing, 2=paused

private slots:
    void onPollLevels();

private:
    void refreshDeviceList();
    void buildWidget(QWidget* container);

    QString m_deckName = "AUX Deck";
    int m_airOutDevice = -1; // -1 = use global default
    int m_cueOutDevice = -1; // -1 = use global default

    std::atomic<float> m_volume{1.0f};
    std::atomic<float> m_peakL{0.0f};
    std::atomic<float> m_peakR{0.0f};
    std::atomic<bool>  m_cueActive{false};

    DeckPlayer* m_player = nullptr;
    QTimer*     m_levelTimer = nullptr;

    // UI widgets (owned by createWidget parent)
    QWidget*    m_widget = nullptr;
    QLineEdit*  m_nameEdit = nullptr;
    QComboBox*  m_airOutCombo = nullptr;
    QComboBox*  m_cueOutCombo = nullptr;
    QLabel*     m_trackLabel = nullptr;
    QLabel*     m_timeLabel = nullptr;
    QSlider*    m_volumeSlider = nullptr;
    QLabel*     m_vuL = nullptr;
    QLabel*     m_vuR = nullptr;
};

} // namespace M1
