#include "LiveMonitorWindow.h"
#include "LiveMonitorChart.h"
#include "EncoderSlot.h"
#include "EncoderEventLog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QSplitter>
#include <QKeyEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

// ═════════════════════════════════════════════════════════════════════════════
// Construction
// ═════════════════════════════════════════════════════════════════════════════
LiveMonitorWindow::LiveMonitorWindow(EncoderSlot* slot, QWidget* parent)
    : QMainWindow(parent)
    , m_slot(slot)
{
    setWindowTitle(slot->config().name + " \xe2\x80\x94 Live Monitor");
    setObjectName("LiveMonitorWindow");
    setMinimumSize(640, 480);
    resize(800, 700);

    buildUi();
    applyStyles();

    // ── Network ─────────────────────────────────────────────────────────────
    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this, &LiveMonitorWindow::onStatsReply);

    // ── Timers ──────────────────────────────────────────────────────────────
    m_serverPollTimer = new QTimer(this);
    m_serverPollTimer->setInterval(5000);
    connect(m_serverPollTimer, &QTimer::timeout, this, &LiveMonitorWindow::onServerPoll);
    m_serverPollTimer->start();

    m_localRefreshTimer = new QTimer(this);
    m_localRefreshTimer->setInterval(1000);
    connect(m_localRefreshTimer, &QTimer::timeout, this, &LiveMonitorWindow::onLocalRefresh);
    m_localRefreshTimer->start();

    // ── Safety: close if slot is destroyed ───────────────────────────────────
    connect(m_slot, &QObject::destroyed, this, &QMainWindow::close);

    // ── Event log wiring ─────────────────────────────────────────────────────
    connect(m_slot->eventLog(), &EncoderEventLog::entryAdded, this,
            [this](const EncoderEventLog::Entry& entry) {
                appendLogEntry(EncoderEventLog::formatEntry(entry));
            });

    // ── Theme changes ────────────────────────────────────────────────────────
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyStyles(); });

    // Initial data
    onServerPoll();
    onLocalRefresh();

    // Seed log with last entries
    const auto entries = m_slot->eventLog()->entries();
    const int start = qMax(0, entries.size() - 30);
    for (int i = start; i < entries.size(); ++i)
        appendLogEntry(EncoderEventLog::formatEntry(entries[i]));
}

LiveMonitorWindow::~LiveMonitorWindow()
{
    m_serverPollTimer->stop();
    m_localRefreshTimer->stop();
}

// ═════════════════════════════════════════════════════════════════════════════
// UI Construction
// ═════════════════════════════════════════════════════════════════════════════
void LiveMonitorWindow::buildUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Top: Server Health + Local Encoder side by side ──────────────────────
    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    // Server Health group
    auto* serverGroup = new QGroupBox("Server Health", central);
    serverGroup->setObjectName("LiveMonitorGroup");
    auto* serverForm = new QVBoxLayout(serverGroup);
    serverForm->setSpacing(3);

    m_serverLed      = new QLabel("\xe2\x97\x8f Polling...", serverGroup);
    m_serverLed->setObjectName("LiveMonitorLed");
    m_mountListeners = new QLabel("Listeners: \xe2\x80\x93", serverGroup);
    m_mountListeners->setObjectName("LiveMonitorBigStat");
    m_listenerPeak   = new QLabel("Peak: \xe2\x80\x93", serverGroup);
    m_bandwidth      = new QLabel("Bandwidth: \xe2\x80\x93", serverGroup);
    m_serverUptime   = new QLabel("Server Uptime: \xe2\x80\x93", serverGroup);
    m_currentSong    = new QLabel("Song: \xe2\x80\x93", serverGroup);
    m_currentSong->setWordWrap(true);

    for (auto* lbl : {m_serverLed, m_mountListeners, m_listenerPeak,
                       m_bandwidth, m_serverUptime, m_currentSong})
        serverForm->addWidget(lbl);

    // Local Encoder group
    auto* localGroup = new QGroupBox("Local Encoder", central);
    localGroup->setObjectName("LiveMonitorGroup");
    auto* localForm = new QVBoxLayout(localGroup);
    localForm->setSpacing(3);

    m_encoderState  = new QLabel("State: \xe2\x97\x8c IDLE", localGroup);
    m_encoderState->setObjectName("LiveMonitorLed");
    m_bytesSent     = new QLabel("Bytes Sent: 0", localGroup);
    m_connectedTime = new QLabel("Connected: \xe2\x80\x93", localGroup);
    m_peakLevels    = new QLabel("Peak L/R: \xe2\x80\x93", localGroup);
    m_codecInfo     = new QLabel("Codec: \xe2\x80\x93", localGroup);
    m_bufferFill    = new QLabel("Ring Buffer: \xe2\x80\x93", localGroup);

    for (auto* lbl : {m_encoderState, m_bytesSent, m_connectedTime,
                       m_peakLevels, m_codecInfo, m_bufferFill})
        localForm->addWidget(lbl);

    topRow->addWidget(serverGroup, 1);
    topRow->addWidget(localGroup, 1);
    root->addLayout(topRow);

    // ── Charts ──────────────────────────────────────────────────────────────
    m_listenersChart = new LiveMonitorChart(central);
    m_listenersChart->configure("Listeners", "",
                                 QColor("#0ea5e9"), QColor("#d4891e"), QColor("#1c5caa"), 120);

    m_bandwidthChart = new LiveMonitorChart(central);
    m_bandwidthChart->configure("Bandwidth", "kbps",
                                 QColor("#22c55e"), QColor("#6b8e23"), QColor("#16a34a"), 120);

    m_bitrateChart = new LiveMonitorChart(central);
    m_bitrateChart->configure("Bitrate Stability", "kbps",
                               QColor("#f59e0b"), QColor("#c87533"), QColor("#d97706"), 120);

    for (auto* chart : {m_listenersChart, m_bandwidthChart, m_bitrateChart}) {
        chart->setMinimumHeight(80);
        chart->setMaximumHeight(140);
        root->addWidget(chart);
    }

    // ── Event Log ───────────────────────────────────────────────────────────
    auto* logGroup = new QGroupBox("Event Log", central);
    logGroup->setObjectName("LiveMonitorGroup");
    auto* logLayout = new QVBoxLayout(logGroup);

    m_logTail = new QTextEdit(logGroup);
    m_logTail->setObjectName("LiveMonitorLog");
    m_logTail->setReadOnly(true);
    m_logTail->setMaximumHeight(150);
    QFont monoFont("Consolas", 9);
    monoFont.setStyleHint(QFont::Monospace);
    m_logTail->setFont(monoFont);
    logLayout->addWidget(m_logTail);

    root->addWidget(logGroup);
}

// ═════════════════════════════════════════════════════════════════════════════
// Theme
// ═════════════════════════════════════════════════════════════════════════════
void LiveMonitorWindow::applyStyles()
{
    const bool isLight   = ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light;
    const bool isClassic = ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Classic;

    QString bg, text, accent, border, groupBg, logBg;
    if (isLight) {
        bg = "#f5f3f0"; text = "#1a1814"; accent = "#1c5caa";
        border = "#d8d4ce"; groupBg = "#ffffff"; logBg = "#f5f3f0";
    } else if (isClassic) {
        bg = "#2a1e14"; text = "#f0e6d8"; accent = "#d4891e";
        border = "#5a4030"; groupBg = "#3d2c1e"; logBg = "#2a1e14";
    } else {
        bg = "#0f172a"; text = "#e2e8f0"; accent = "#0ea5e9";
        border = "#1e3a5f"; groupBg = "#0c1a2e"; logBg = "#0a1628";
    }

    setStyleSheet(QString(R"(
        #LiveMonitorWindow { background: %1; }
        QGroupBox#LiveMonitorGroup {
            color: %2; font-size: 13px; font-weight: bold;
            border: 1px solid %4; border-radius: 4px;
            background: %5; margin-top: 12px; padding: 8px 6px 6px 6px;
        }
        QGroupBox#LiveMonitorGroup::title {
            subcontrol-origin: margin; left: 10px; padding: 0 4px;
        }
        QLabel { color: %2; font-size: 12px; }
        QLabel#LiveMonitorBigStat {
            color: %3; font-size: 24px; font-weight: bold;
        }
        QLabel#LiveMonitorLed { font-size: 13px; font-weight: bold; }
        QTextEdit#LiveMonitorLog {
            background: %6; color: %2; border: 1px solid %4;
            border-radius: 3px; font-size: 11px;
        }
    )").arg(bg, text, accent, border, groupBg, logBg));
}

// ═════════════════════════════════════════════════════════════════════════════
// Server Stats Polling
// ═════════════════════════════════════════════════════════════════════════════
void LiveMonitorWindow::onServerPoll()
{
    if (!m_slot) return;
    const auto& cfg = m_slot->config();

    // Build URL: DNAS uses /admin/mcaster1stats, Icecast2 uses /admin/stats
    const QString path = cfg.isDnas() ? "/admin/mcaster1stats" : "/admin/stats";
    const QString urlStr = QString("http://%1:%2%3").arg(cfg.host).arg(cfg.port).arg(path);

    QNetworkRequest req{QUrl(urlStr)};
    // Basic auth with admin user/pass
    const QByteArray auth = (cfg.adminUser + ":" + cfg.adminPass).toUtf8().toBase64();
    req.setRawHeader("Authorization", "Basic " + auth);
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    m_nam->get(req);
}

void LiveMonitorWindow::onStatsReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_serverLed->setText("\xe2\x9c\x97 " + reply->errorString());
        m_serverLed->setStyleSheet("color: #ef4444;");
        return;
    }

    const QByteArray data = reply->readAll();
    parseStatsXml(data);

    // Update server stats labels
    m_serverLed->setText("\xe2\x97\x8f Connected");
    m_serverLed->setStyleSheet("color: #22c55e;");
    m_mountListeners->setText("Listeners: " + QString::number(m_lastListeners));
    m_listenerPeak->setText("Peak: " + QString::number(m_lastListenerPeak));
    m_bandwidth->setText("Bandwidth: " + formatBandwidth(m_lastBandwidthBps));
    m_serverUptime->setText("Server Uptime: " + formatUptime(m_lastServerUptime));
    m_currentSong->setText("Song: " + (m_lastCurrentSong.isEmpty()
                           ? QString::fromUtf8("\xe2\x80\x93") : m_lastCurrentSong));

    // Push chart samples
    m_listenersChart->pushSample(static_cast<float>(m_lastListeners));
    m_bandwidthChart->pushSample(static_cast<float>(m_lastBandwidthBps / 1000.0));  // kbps
}

void LiveMonitorWindow::parseStatsXml(const QByteArray& xml)
{
    const QString text = QString::fromUtf8(xml);

    // Find the mount block for this slot's mount point
    const QString mount = m_slot->config().mount;
    const QRegularExpression mountRe(
        QString("<source\\s+mount=\"%1\">(.*?)</source>")
            .arg(QRegularExpression::escape(mount)),
        QRegularExpression::DotMatchesEverythingOption
        | QRegularExpression::CaseInsensitiveOption);

    auto match = mountRe.match(text);
    if (match.hasMatch()) {
        const QString block = match.captured(1);

        auto extractInt = [&](const QString& tag) -> int {
            QRegularExpression re(QString("<%1>(\\d+)</%1>").arg(tag));
            auto m = re.match(block);
            return m.hasMatch() ? m.captured(1).toInt() : -1;
        };
        auto extractStr = [&](const QString& tag) -> QString {
            QRegularExpression re(QString("<%1>(.*?)</%1>").arg(tag));
            auto m = re.match(block);
            return m.hasMatch() ? m.captured(1).trimmed() : QString();
        };

        int lis = extractInt("listeners");
        if (lis >= 0) m_lastListeners = lis;
        int peak = extractInt("listener_peak");
        if (peak >= 0) m_lastListenerPeak = peak;
        int bw = extractInt("bandwidth");
        if (bw >= 0) m_lastBandwidthBps = bw;
        QString title = extractStr("title");
        if (!title.isEmpty()) m_lastCurrentSong = title;
    }

    // Global server uptime
    {
        QRegularExpression re("<(?:server_uptime|uptime_seconds)>(\\d+)</");
        auto m = re.match(text);
        if (m.hasMatch()) m_lastServerUptime = m.captured(1).toLongLong();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Local Stats Refresh
// ═════════════════════════════════════════════════════════════════════════════
void LiveMonitorWindow::onLocalRefresh()
{
    updateLocalStats();
}

void LiveMonitorWindow::updateLocalStats()
{
    if (!m_slot) return;

    using S = EncoderSlot::State;
    const S st = m_slot->state();

    // State with color
    static const auto stText = [](S s) -> QString {
        switch (s) {
            case S::Idle:         return "\xe2\x97\x8c IDLE";
            case S::Starting:     return "\xe2\x80\xa6 STARTING";
            case S::Connecting:   return "\xe2\x86\x92 CONNECTING";
            case S::Streaming:    return "\xe2\x97\x8f STREAMING";
            case S::Reconnecting: return "\xe2\x86\xbb RECONNECTING";
            case S::Sleep:        return "\xe2\x8a\x98 SLEEP";
            case S::Error:        return "\xe2\x9c\x97 ERROR";
        }
        return "?";
    };
    static const auto stColor = [](S s) -> QString {
        switch (s) {
            case S::Streaming:    return "color: #22c55e;";
            case S::Connecting:
            case S::Reconnecting: return "color: #f59e0b;";
            case S::Error:        return "color: #ef4444;";
            default:              return "color: #94a3b8;";
        }
    };

    m_encoderState->setText("State: " + stText(st));
    m_encoderState->setStyleSheet(stColor(st));

    m_bytesSent->setText("Bytes Sent: " + formatBytes(m_slot->bytesSent()));
    m_connectedTime->setText("Connected: " + formatUptime(m_slot->connectedSecs()));

    // Peak levels in dBFS
    const float peakL = m_slot->peakL();
    const float peakR = m_slot->peakR();
    auto toDb = [](float linear) -> QString {
        if (linear <= 0.0001f) return "-\xe2\x88\x9e";
        return QString::number(20.0f * std::log10(linear), 'f', 1);
    };
    m_peakLevels->setText(QString("Peak L/R: %1 / %2 dBFS").arg(toDb(peakL), toDb(peakR)));

    // Codec info
    const auto& cfg = m_slot->config();
    m_codecInfo->setText(QString("Codec: %1 %2k %3 %4Hz")
                         .arg(cfg.codecName())
                         .arg(cfg.bitrate)
                         .arg(cfg.channelMode == EncoderConfig::ChannelMode::Mono ? "M" :
                              cfg.channelMode == EncoderConfig::ChannelMode::JointStereo ? "JS" : "S")
                         .arg(cfg.sampleRate));

    // Ring buffer fill (approximate)
    const auto& rb = m_slot->ringBuffer();
    const int cap  = rb.capacity();
    const int used = rb.used();
    const int fillPct = cap > 0 ? (used * 100 / cap) : 0;
    m_bufferFill->setText(QString("Ring Buffer: %1% full (%2/%3 frames)")
                          .arg(fillPct).arg(used).arg(cap));

    // Push bitrate chart sample (from config — should be stable flat line)
    m_bitrateChart->pushSample(static_cast<float>(cfg.bitrate));
}

// ═════════════════════════════════════════════════════════════════════════════
// Event Log
// ═════════════════════════════════════════════════════════════════════════════
void LiveMonitorWindow::appendLogEntry(const QString& formatted)
{
    if (m_logCount >= 200) {
        // Trim old entries
        QTextCursor cursor = m_logTail->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 50);
        cursor.removeSelectedText();
        m_logCount -= 50;
    }

    // Color-code by content
    QString color;
    if (formatted.contains("[ERR"))       color = "#ef4444";
    else if (formatted.contains("[WARN")) color = "#f59e0b";
    else if (formatted.contains("[ICY"))  color = "#22c55e";
    else if (formatted.contains("[AUTH")) color = "#0ea5e9";
    else if (formatted.contains("[CONN")) color = "#38bdf8";
    else                                  color = "#94a3b8";

    m_logTail->append(QString("<span style='color:%1'>%2</span>")
                      .arg(color, formatted.toHtmlEscaped()));
    ++m_logCount;
}

// ═════════════════════════════════════════════════════════════════════════════
// Formatting Helpers
// ═════════════════════════════════════════════════════════════════════════════
QString LiveMonitorWindow::formatBytes(qint64 bytes)
{
    if (bytes < 1024)                return QString::number(bytes) + " B";
    if (bytes < 1024LL * 1024)       return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

QString LiveMonitorWindow::formatBandwidth(double bitsPerSec)
{
    if (bitsPerSec < 1000.0)       return QString::number(bitsPerSec, 'f', 0) + " bps";
    if (bitsPerSec < 1000000.0)    return QString::number(bitsPerSec / 1000.0, 'f', 1) + " kbps";
    if (bitsPerSec < 1000000000.0) return QString::number(bitsPerSec / 1e6, 'f', 2) + " Mbps";
    return QString::number(bitsPerSec / 1e9, 'f', 2) + " Gbps";
}

QString LiveMonitorWindow::formatUptime(qint64 secs)
{
    if (secs < 0) return "\xe2\x80\x93";
    const int d = static_cast<int>(secs / 86400);
    const int h = static_cast<int>((secs % 86400) / 3600);
    const int m = static_cast<int>((secs % 3600) / 60);
    const int s = static_cast<int>(secs % 60);

    if (d > 0) return QString("%1d %2h %3m").arg(d).arg(h).arg(m);
    if (h > 0) return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    return QString("%1m %2s").arg(m).arg(s);
}

// ═════════════════════════════════════════════════════════════════════════════
// Keyboard
// ═════════════════════════════════════════════════════════════════════════════
void LiveMonitorWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
        return;
    }
    QMainWindow::keyPressEvent(event);
}
