#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QList>

namespace M1 {
class HealthModule;
struct HealthSnapshot;
}
class LiveMonitorChart;

/// HealthWidget — dockable UI for the System Health module.
///
/// Shows CPU/memory gauges, encoder slot indicators, deck status,
/// and interactive time-series charts for CPU% and memory usage.
class HealthWidget : public QWidget {
    Q_OBJECT

public:
    explicit HealthWidget(M1::HealthModule* module, QWidget* parent = nullptr);

private slots:
    void onSnapshot(const M1::HealthSnapshot& snap);
    void onExportCsv();
    void onExportJson();

private:
    void buildUi();
    void applyStyles();
    void updateEncoderIndicators(const M1::HealthSnapshot& snap);

    static QString formatMemory(qint64 bytes);

    M1::HealthModule* m_module = nullptr;

    // CPU / Memory labels
    QLabel* m_cpuLabel    = nullptr;
    QLabel* m_memLabel    = nullptr;
    QLabel* m_peakMemLabel = nullptr;

    // CPU / Memory bar widgets (painted)
    QWidget* m_cpuBar  = nullptr;
    QWidget* m_memBar  = nullptr;
    float    m_cpuFill = 0.0f;
    float    m_memFill = 0.0f;

    // Encoder indicators
    QLabel* m_encoderSummary = nullptr;
    QList<QLabel*> m_encoderDots;

    // Deck status
    QLabel* m_deckAStatus = nullptr;
    QLabel* m_deckBStatus = nullptr;

    // Charts
    LiveMonitorChart* m_cpuChart = nullptr;
    LiveMonitorChart* m_memChart = nullptr;

    // Export
    QPushButton* m_exportBtn = nullptr;
    QMenu*       m_exportMenu = nullptr;
};
