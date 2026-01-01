#pragma once
#include <QDialog>
#include <QList>
#include "IAudioEngine.h"

class QComboBox;
class QTabWidget;
class QTableWidget;
class QPushButton;
class SurfaceTabBar;

/// DeviceSettingsDialog — audio device assignment.
///
/// Tab 1 — Global Devices:
///   Mirrors the device section of PreferencesDialog; changes apply to the
///   audio engine immediately on Accept.
///
/// Tab 2 — Surface Assignments:
///   Per-surface override for input and output devices (saved in QSettings).
///   Supports virtual audio cables and Mcaster1AudioPipe loopback devices.
class DeviceSettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit DeviceSettingsDialog(M1::IAudioEngine* engine,
                                  SurfaceTabBar*    surfaceBar,
                                  QWidget*          parent = nullptr);

    /// Apply accepted global device settings to the audio engine.
    /// Call after exec() == QDialog::Accepted.
    void applyGlobalSettings();

private slots:
    void onRefreshDevices();

private:
    void buildGlobalTab();
    void buildSurfaceTab();
    void populateDevices();
    void saveAndClose();

    M1::IAudioEngine* m_engine     = nullptr;
    SurfaceTabBar*    m_surfaceBar = nullptr;

    // ── Global tab controls ──────────────────────────────────────────────
    QComboBox* m_inputCombo      = nullptr;
    QComboBox* m_outputCombo     = nullptr;
    QComboBox* m_sampleRateCombo = nullptr;
    QComboBox* m_bufferSizeCombo = nullptr;
    QList<int> m_inputIndices;
    QList<int> m_outputIndices;

    // ── Surface tab ──────────────────────────────────────────────────────
    QTableWidget* m_surfaceTable = nullptr;
};
