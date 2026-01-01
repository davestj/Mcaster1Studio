#pragma once
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QStackedWidget>
#include "IAudioEngine.h"
#include "ThemeManager.h"

class QVBoxLayout;
class QTabWidget;
class QGroupBox;
class QListWidget;
class QTextEdit;
class QTableWidget;
class QNetworkAccessManager;
class QNetworkReply;
namespace M1 { class AudioEngine; }

/// Preferences dialog — audio, appearance, and database settings.
class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(M1::IAudioEngine* engine, QWidget* parent = nullptr);

    int selectedInputDevice()  const;
    int selectedOutputDevice() const;
    int selectedCueDevice()    const;
    int selectedSampleRate()   const;
    int selectedBufferSize()   const;

private slots:
    void onAccepted();
    void onRefreshDevices();
    void onThemeChanged(int index);
    void onDbBackendChanged(int index);
    void onSqliteBrowse();
    void onAiProviderChanged(int index);
    void onOllamaFetchModels();
    void onOllamaInstallModel();
    void onOllamaTestConnection();
    void onOllamaDiagnostic();

private:
    void populateDevices();
    QWidget* buildAudioPage();
    QWidget* buildAppearancePage();
    QWidget* buildDatabasePage();
    QWidget* buildDbServersPage();
    QWidget* buildDisplaysPage();
    QWidget* buildMetricsPage();
    QWidget* buildAIPage();

    void refreshDbServerTable();
    void refreshMonitorTable();
    void refreshCaptureTable();

    M1::IAudioEngine* m_engine = nullptr;
    QTabWidget* m_tabs         = nullptr;

    // Audio page
    QComboBox* m_inputCombo      = nullptr;
    QComboBox* m_outputCombo     = nullptr;
    QComboBox* m_cueCombo        = nullptr;
    QComboBox* m_sampleRateCombo = nullptr;
    QComboBox* m_bufferSizeCombo = nullptr;
    QList<int> m_inputIndices;
    QList<int> m_outputIndices;
    QList<int> m_cueIndices;

    // Appearance page
    QComboBox* m_themeCombo = nullptr;

    // Database page
    QComboBox*  m_dbBackendCombo = nullptr;
    QGroupBox*  m_sqliteGroup    = nullptr;
    QGroupBox*  m_mysqlGroup     = nullptr;
    QLineEdit*  m_sqlitePath     = nullptr;
    QLineEdit*  m_mysqlHost      = nullptr;
    QSpinBox*   m_mysqlPort      = nullptr;
    QLineEdit*  m_mysqlUser      = nullptr;
    QLineEdit*  m_mysqlPass      = nullptr;
    QLineEdit*  m_mysqlDb        = nullptr;
    QLabel*     m_dbStatusLabel  = nullptr;

    // DB Servers page
    QTableWidget* m_dbServerTable  = nullptr;

    // Displays page
    QTableWidget* m_monitorTable   = nullptr;
    QTableWidget* m_captureTable   = nullptr;
    QLabel*       m_gpuInfoLabel   = nullptr;

    // Metrics page
    QCheckBox*  m_metricsEnabled  = nullptr;
    QSpinBox*   m_metricsPort     = nullptr;
    QCheckBox*  m_grafanaEnabled  = nullptr;
    QLineEdit*  m_grafanaEndpoint = nullptr;
    QSpinBox*   m_grafanaInterval = nullptr;
    QLineEdit*  m_grafanaApiKey   = nullptr;

    // AI Integration page
    QComboBox*  m_aiProviderCombo  = nullptr;
    QStackedWidget* m_aiStack      = nullptr;
    QNetworkAccessManager* m_aiNam = nullptr;

    // Ollama widgets
    QLineEdit*  m_ollamaUrl        = nullptr;
    QListWidget* m_ollamaModelList = nullptr;
    QComboBox*  m_ollamaActiveModel = nullptr;
    QTextEdit*  m_ollamaLog        = nullptr;

    // Claude widgets
    QLineEdit*  m_claudeApiKey     = nullptr;
    QComboBox*  m_claudeModel      = nullptr;

    // Grok widgets
    QLineEdit*  m_grokApiKey       = nullptr;
    QComboBox*  m_grokModel        = nullptr;

    // Gemini widgets
    QLineEdit*  m_geminiApiKey     = nullptr;
    QComboBox*  m_geminiModel      = nullptr;

    // ChatGPT widgets
    QLineEdit*  m_chatgptApiKey    = nullptr;
    QComboBox*  m_chatgptModel     = nullptr;

    // Venice widgets
    QLineEdit*  m_veniceApiKey     = nullptr;
    QComboBox*  m_veniceModel      = nullptr;
};
