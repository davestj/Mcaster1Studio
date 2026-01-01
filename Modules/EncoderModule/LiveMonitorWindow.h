#pragma once
#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QTextEdit>

class QNetworkAccessManager;
class QNetworkReply;
class EncoderSlot;
class LiveMonitorChart;

/// LiveMonitorWindow — standalone QMainWindow for per-encoder live monitoring.
///
/// Opened from EncoderListWidget context menu "Live Monitor".
/// One per encoder slot (tracked by slot pointer in EncoderListWidget::m_liveMonitors).
///
/// Shows:
///   - Server health (listeners, peak, bandwidth, uptime, current song)
///     via GET /admin/mcaster1stats XML endpoint
///   - Local encoder stats (state, bytes sent, connected time, peak levels, codec)
///   - Three interactive time-series charts (listeners, bandwidth, bitrate)
///   - Event log tail (last 30 entries)
///
/// Resizable, maximizable, fullscreen (F11), multi-monitor friendly.
class LiveMonitorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit LiveMonitorWindow(EncoderSlot* slot, QWidget* parent = nullptr);
    ~LiveMonitorWindow() override;

    /// Returns the slot this window monitors (for reuse detection).
    EncoderSlot* slot() const { return m_slot; }

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onServerPoll();
    void onLocalRefresh();
    void onStatsReply(QNetworkReply* reply);

private:
    void buildUi();
    void applyStyles();
    void parseStatsXml(const QByteArray& xml);
    void updateLocalStats();
    void appendLogEntry(const QString& formatted);

    static QString formatBytes(qint64 bytes);
    static QString formatBandwidth(double bitsPerSec);
    static QString formatUptime(qint64 secs);

    EncoderSlot* m_slot = nullptr;

    // Polling
    QNetworkAccessManager* m_nam              = nullptr;
    QTimer*                m_serverPollTimer  = nullptr;  // 5s
    QTimer*                m_localRefreshTimer = nullptr;  // 1s

    // Server stats display
    QLabel* m_serverLed        = nullptr;
    QLabel* m_mountListeners   = nullptr;
    QLabel* m_listenerPeak     = nullptr;
    QLabel* m_bandwidth        = nullptr;
    QLabel* m_serverUptime     = nullptr;
    QLabel* m_currentSong      = nullptr;

    // Local encoder stats display
    QLabel* m_encoderState     = nullptr;
    QLabel* m_bytesSent        = nullptr;
    QLabel* m_connectedTime    = nullptr;
    QLabel* m_peakLevels       = nullptr;
    QLabel* m_codecInfo        = nullptr;
    QLabel* m_bufferFill       = nullptr;

    // Charts
    LiveMonitorChart* m_listenersChart = nullptr;
    LiveMonitorChart* m_bandwidthChart = nullptr;
    LiveMonitorChart* m_bitrateChart   = nullptr;

    // Event log tail
    QTextEdit* m_logTail = nullptr;
    int        m_logCount = 0;

    // Last known server stats
    int     m_lastListeners    = 0;
    int     m_lastListenerPeak = 0;
    double  m_lastBandwidthBps = 0.0;
    qint64  m_lastServerUptime = 0;
    QString m_lastCurrentSong;
};
