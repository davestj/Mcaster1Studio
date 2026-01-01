#pragma once
#include "EncoderConfig.h"
#include <QDialog>

class EncoderSlot;
class QTabWidget;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;
class QGroupBox;
class QListWidget;
class QTimer;

/// EncoderConfigDialog — 5-tab modal configuration dialog for one encoder slot.
///
/// Tabs:
///   1. Basic     — codec, source, server, station info, reconnect
///   2. DSP       — EQ preset, AGC, PTT duck (per-slot processing)
///   3. ICY 2.2   — all 70+ extended metadata fields (DNAS only)
///   4. Archive   — WAV + MP3 recording
///   5. Stats     — live read-only stats (bytes, uptime, listeners, log)
class EncoderConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit EncoderConfigDialog(EncoderSlot* slot, QWidget* parent = nullptr);

private slots:
    void onCodecChanged(int index);
    void onServerTypeChanged(int index);
    void onSourceChanged(int index);
    void onDspToggled(bool enabled);
    void onAgcToggled(bool enabled);
    void onArchiveToggled(bool enabled);
    void onBrowseArchive();
    void onAccepted();
    void refreshStats();

private:
    void buildBasicTab(QTabWidget* tabs);
    void buildDspTab(QTabWidget* tabs);
    void buildIcy2Tab(QTabWidget* tabs);
    void buildArchiveTab(QTabWidget* tabs);
    void buildStatsTab(QTabWidget* tabs);

    void populatePortAudioDevices();
    void loadFromConfig(const EncoderConfig& cfg);
    EncoderConfig readConfig() const;
    void updateIcy2TabState();
    void updateBitrateVsVbr();

    EncoderSlot* m_slot = nullptr;

    // ── Tab 1: Basic ─────────────────────────────────────────────────────────
    QLineEdit*    m_name        = nullptr;
    QComboBox*    m_mode        = nullptr;
    QComboBox*    m_source      = nullptr;
    QComboBox*    m_paDevice    = nullptr;   // PortAudio device list
    QLabel*       m_paDevLbl    = nullptr;
    QComboBox*    m_codec       = nullptr;
    QSpinBox*     m_bitrate     = nullptr;
    QDoubleSpinBox* m_vbrQual   = nullptr;
    QLabel*       m_bitrateLbl  = nullptr;
    QLabel*       m_vbrLbl      = nullptr;
    QSpinBox*     m_sampleRate  = nullptr;
    QSpinBox*     m_channels    = nullptr;
    QComboBox*    m_chanMode    = nullptr;
    QComboBox*    m_serverType  = nullptr;
    QLineEdit*    m_host        = nullptr;
    QSpinBox*     m_port        = nullptr;
    QLineEdit*    m_mount       = nullptr;
    QLineEdit*    m_password    = nullptr;
    QLineEdit*    m_adminUser   = nullptr;
    QLineEdit*    m_adminPass   = nullptr;
    QLineEdit*    m_stationName = nullptr;
    QLineEdit*    m_description = nullptr;
    QLineEdit*    m_genre       = nullptr;
    QLineEdit*    m_url         = nullptr;
    QCheckBox*    m_isPublic    = nullptr;
    QCheckBox*    m_useSsl        = nullptr;
    QCheckBox*    m_autoReconnect = nullptr;
    QSpinBox*     m_retryInterval = nullptr;
    QSpinBox*     m_maxRetries    = nullptr;
    QCheckBox*    m_autoStart     = nullptr;

    // ── Tab 2: DSP ───────────────────────────────────────────────────────────
    QCheckBox*     m_dspEnabled   = nullptr;
    QComboBox*     m_eqPreset     = nullptr;
    QCheckBox*     m_agcEnabled   = nullptr;
    QDoubleSpinBox* m_agcInputGain = nullptr;
    QDoubleSpinBox* m_agcThreshold = nullptr;
    QDoubleSpinBox* m_agcRatio     = nullptr;
    QDoubleSpinBox* m_agcAttack    = nullptr;
    QDoubleSpinBox* m_agcRelease   = nullptr;
    QDoubleSpinBox* m_agcMakeup    = nullptr;
    QDoubleSpinBox* m_agcLimiter   = nullptr;
    QGroupBox*      m_agcGroup     = nullptr;
    QCheckBox*      m_pttDuck      = nullptr;
    QDoubleSpinBox* m_pttAttenDb   = nullptr;

    // ── Tab 3: ICY 2.2 ───────────────────────────────────────────────────────
    QLabel* m_icy2Warning = nullptr;
    // Fields stored as map: key → QLineEdit*
    QMap<QString, QLineEdit*>  m_icy2Edits;
    QMap<QString, QCheckBox*>  m_icy2Checks;  // for boolean fields
    QTabWidget* m_tabs = nullptr;  // kept for ICY2 tab index check

    // ── Tab 4: Archive ───────────────────────────────────────────────────────
    QCheckBox*  m_archiveEnabled = nullptr;
    QLineEdit*  m_archiveDir     = nullptr;
    QCheckBox*  m_archiveWav     = nullptr;
    QCheckBox*  m_archiveMp3     = nullptr;

    // ── Tab 5: Stats ─────────────────────────────────────────────────────────
    QLabel*      m_statBytes    = nullptr;
    QLabel*      m_statUptime   = nullptr;
    QLabel*      m_statState    = nullptr;
    QLabel*      m_statListeners = nullptr;
    QListWidget* m_statLog      = nullptr;
    QTimer*      m_statsTimer   = nullptr;
};
