#pragma once
#include "IModule.h"
#include "IPlugin.h"
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QList>
#include <QString>

class MonitorWidget;

namespace M1 {

/// Snapshot of server statistics captured at a single poll instant.
struct MonitorSnapshot {
    qint64  timestampMs   = 0;  ///< QDateTime::currentMSecsSinceEpoch()
    int     listeners     = 0;
    int     peakListeners = 0;
    int     bitrate       = 0;
    qint64  uptimeSeconds = 0;
    QString streamTitle;
};

/// Server type enumeration — determines which HTTP endpoint + parser to use.
enum class ServerType {
    Icecast2,       ///< GET /status-json.xsl
    Shoutcast,      ///< GET /7.html
    Mcaster1DNAS    ///< GET /stats (JSON)
};

/// MonitorModule — Phase 10 real-time server statistics monitor.
///
/// Polls an Icecast2, Shoutcast, or Mcaster1DNAS server at a configurable
/// interval (default: 10 s) via HTTP and parses listener counts, peak,
/// stream title, uptime, and bitrate.
///
/// History ring buffer: last 60 snapshots (= 10 min at 10 s interval).
/// History drives the listener-count chart in MonitorWidget.
///
/// onAudioBlock() is a no-op — this module does not process audio.
class MonitorModule : public IModule {
    Q_OBJECT

public:
    /// Connection configuration for a single monitored server.
    struct Config {
        QString    host           = "localhost";
        int        port           = 8000;
        QString    mount          = "/stream";
        QString    password;
        ServerType serverType     = ServerType::Icecast2;
        int        pollIntervalSec = 10;
    };

    static constexpr int kHistorySize = 60; ///< Ring buffer capacity

    explicit MonitorModule(QObject* parent = nullptr);
    ~MonitorModule() override;

    // ── IModule ──────────────────────────────────────────────────────────
    QString  moduleId()      const override { return "com.mcaster1.monitor"; }
    QString  displayName()   const override { return "Monitor"; }
    QSize    preferredSize() const override { return {600, 350}; }

    QWidget* createWidget(QWidget* parent) override;

    void initialize() override;
    void shutdown()   override;

    // No audio processing — server stats module
    void onAudioBlock(AudioBuffer& /*in*/, AudioBuffer& /*out*/) override {}

    void saveState(QSettings& s) override;
    void loadState(QSettings& s) override;

    // ── Connection control ────────────────────────────────────────────────
    void setConfig(const Config& cfg);
    const Config& config() const { return m_config; }

    void connectToServer();
    void disconnectFromServer();
    bool isConnected() const { return m_connected; }

    // ── Latest stats (thread-safe reads on Qt thread) ─────────────────────
    int            listeners()     const { return m_lastSnapshot.listeners;     }
    int            peakListeners() const { return m_lastSnapshot.peakListeners; }
    int            bitrate()       const { return m_lastSnapshot.bitrate;       }
    qint64         uptimeSeconds() const { return m_lastSnapshot.uptimeSeconds; }
    QString        streamTitle()   const { return m_lastSnapshot.streamTitle;   }

    /// Full snapshot history (oldest → newest). Max kHistorySize entries.
    const QList<MonitorSnapshot>& history() const { return m_history; }

signals:
    void statsUpdated(int listeners, int peak, const QString& title);
    void connectionStateChanged(bool connected);
    void pollError(const QString& errorMsg);

private slots:
    void onPollTimer();
    void onNetworkReply(QNetworkReply* reply);

private:
    void            poll();
    void            parseIcecast2(const QByteArray& data);
    void            parseShoutcast(const QByteArray& data);
    void            parseMcaster1DNAS(const QByteArray& data);
    void            pushSnapshot(const MonitorSnapshot& snap);
    QNetworkRequest buildRequest() const;

    Config                  m_config;
    QNetworkAccessManager*  m_nam        = nullptr;
    QTimer*                 m_pollTimer  = nullptr;
    bool                    m_connected  = false;
    MonitorSnapshot         m_lastSnapshot;
    QList<MonitorSnapshot>  m_history;    ///< Ring buffer — oldest first
};

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────
extern "C" {
    MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_monitor_plugin_info();
    MCASTER1_PLUGIN_API M1::IModule*        mcaster1_monitor_create(IModuleHost*);
    MCASTER1_PLUGIN_API void                mcaster1_monitor_destroy(M1::IModule*);
}
