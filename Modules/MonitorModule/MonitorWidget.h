#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QTimer>

namespace M1 { class MonitorModule; struct MonitorSnapshot; }

/// MonitorWidget — primary UI for MonitorModule.
///
/// Layout:
///
///   ┌───────────────────────────────────────────────────────────┐
///   │ [LED]  Stats grid: listeners (32px cyan), peak, up, br    │
///   │ Now Playing: scrolling title                               │
///   │ Listener history chart (QPainter polyline + gradient fill) │
///   ├───────────────────────────────────────────────────────────┤
///   │ ▼ Connection Config (collapsible group box)               │
///   │   Host: [____]  Port: [____]  Mount: [____]               │
///   │   Password: [____]  Type: [v]  Interval: [__] s           │
///   │   [Connect]  [Disconnect]                                  │
///   └───────────────────────────────────────────────────────────┘
class MonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit MonitorWidget(M1::MonitorModule* module, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onStatsUpdated(int listeners, int peak, const QString& title);
    void onConnectionStateChanged(bool connected);
    void onPollError(const QString& err);
    void onConnectClicked();
    void onDisconnectClicked();
    void onToggleConfig();

private:
    void buildUi();
    void applyStyles();
    void drawHistoryChart(QPainter& p, const QRect& rect);
    void updateLed(bool connected, bool error = false);
    static QString formatUptime(qint64 secs);

    M1::MonitorModule* m_module = nullptr;

    // ── Stats display ────────────────────────────────────────────────────
    QLabel* m_ledLabel         = nullptr;   ///< Colored circle: gray/green/red
    QLabel* m_listenersLabel   = nullptr;   ///< 32px bold cyan
    QLabel* m_peakLabel        = nullptr;
    QLabel* m_uptimeLabel      = nullptr;
    QLabel* m_bitrateLabel     = nullptr;
    QLabel* m_nowPlayingLabel  = nullptr;   ///< Scrolling marquee via QTimer

    // ── History chart ────────────────────────────────────────────────────
    QWidget* m_chartWidget     = nullptr;   ///< Custom-painted chart area

    // ── Connection config panel (collapsible) ────────────────────────────
    QGroupBox*  m_configGroup    = nullptr;
    QLineEdit*  m_hostEdit       = nullptr;
    QSpinBox*   m_portSpin       = nullptr;
    QLineEdit*  m_mountEdit      = nullptr;
    QLineEdit*  m_passwordEdit   = nullptr;
    QComboBox*  m_serverTypeCombo = nullptr;
    QSpinBox*   m_intervalSpin   = nullptr;
    QPushButton* m_connectBtn    = nullptr;
    QPushButton* m_disconnectBtn = nullptr;
    QPushButton* m_toggleCfgBtn  = nullptr;

    // Marquee scroll support
    QTimer*     m_marqueeTimer   = nullptr;
    QString     m_nowPlayingText;
    int         m_marqueeOffset  = 0;
    bool        m_hasError       = false;
};

/// ChartWidget — inner widget that paints the listener history graph.
class ChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartWidget(M1::MonitorModule* module, QWidget* parent = nullptr);
    QSize sizeHint() const override { return {580, 100}; }
    QSize minimumSizeHint() const override { return {200, 60}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    M1::MonitorModule* m_module = nullptr;
};
