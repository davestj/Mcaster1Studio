#include "MonitorModule.h"
#include "MonitorWidget.h"
#include "AudioBuffer.h"
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QUrl>
#include <QRegularExpression>
#include <QDebug>

namespace M1 {

// ─── Constructor / Destructor ─────────────────────────────────────────────────

MonitorModule::MonitorModule(QObject* parent)
    : IModule(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &MonitorModule::onPollTimer);
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &MonitorModule::onNetworkReply);
}

MonitorModule::~MonitorModule() {
    shutdown();
}

// ─── IModule ──────────────────────────────────────────────────────────────────

void MonitorModule::initialize() {
    qInfo() << "[MonitorModule] initialized.";
}

void MonitorModule::shutdown() {
    disconnectFromServer();
}

QWidget* MonitorModule::createWidget(QWidget* parent) {
    return new MonitorWidget(this, parent);
}

// ─── Connection control ───────────────────────────────────────────────────────

void MonitorModule::setConfig(const Config& cfg) {
    const bool wasConnected = m_connected;
    if (wasConnected)
        disconnectFromServer();

    m_config = cfg;

    if (wasConnected)
        connectToServer();
}

void MonitorModule::connectToServer() {
    if (m_connected) return;

    m_connected = true;
    emit connectionStateChanged(true);
    emit statusChanged("Connected — polling");

    // Fire immediately, then on interval
    poll();
    m_pollTimer->start(m_config.pollIntervalSec * 1000);
}

void MonitorModule::disconnectFromServer() {
    if (!m_connected) return;

    m_pollTimer->stop();
    m_connected = false;
    emit connectionStateChanged(false);
    emit statusChanged("Disconnected");
}

// ─── Polling ──────────────────────────────────────────────────────────────────

void MonitorModule::onPollTimer() {
    poll();
}

void MonitorModule::poll() {
    if (!m_connected) return;
    m_nam->get(buildRequest());
}

QNetworkRequest MonitorModule::buildRequest() const {
    QString path;
    switch (m_config.serverType) {
    case ServerType::Icecast2:
        path = "/status-json.xsl";
        break;
    case ServerType::Shoutcast:
        path = "/7.html";
        break;
    case ServerType::Mcaster1DNAS:
        path = "/stats";
        break;
    }

    QUrl url;
    url.setScheme("http");
    url.setHost(m_config.host);
    url.setPort(m_config.port);
    url.setPath(path);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mcaster1Studio/1.0 MonitorModule");
    req.setRawHeader("Accept", "application/json, text/html");

    // Basic auth for Icecast2 admin endpoint (if password supplied)
    if (!m_config.password.isEmpty()) {
        const QString credentials =
            QString("admin:%1").arg(m_config.password);
        req.setRawHeader("Authorization",
            "Basic " + credentials.toUtf8().toBase64());
    }

    return req;
}

void MonitorModule::onNetworkReply(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        qWarning() << "[MonitorModule] Poll error:" << err;
        emit pollError(err);
        emit statusChanged("Error: " + err);
        return;
    }

    const QByteArray data = reply->readAll();

    switch (m_config.serverType) {
    case ServerType::Icecast2:
        parseIcecast2(data);
        break;
    case ServerType::Shoutcast:
        parseShoutcast(data);
        break;
    case ServerType::Mcaster1DNAS:
        parseMcaster1DNAS(data);
        break;
    }
}

// ─── Parsers ──────────────────────────────────────────────────────────────────

void MonitorModule::parseIcecast2(const QByteArray& data) {
    // Icecast2 /status-json.xsl response structure:
    // { "icestats": {
    //     "source": { ... } or [ { ... }, ... ],
    //     "listeners": N,
    //     "server_start_iso8601": "...",
    //   }
    // }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[MonitorModule] Icecast2 JSON parse error:" << err.errorString();
        emit pollError("JSON parse error: " + err.errorString());
        return;
    }

    const QJsonObject icestats = doc.object().value("icestats").toObject();

    MonitorSnapshot snap;
    snap.timestampMs = QDateTime::currentMSecsSinceEpoch();

    // Find the source matching our mount, or take the first one
    const QJsonValue sourceVal = icestats.value("source");
    QJsonObject sourceObj;
    if (sourceVal.isArray()) {
        const QJsonArray arr = sourceVal.toArray();
        for (const auto& v : arr) {
            const QJsonObject s = v.toObject();
            if (s.value("listenurl").toString().contains(m_config.mount)) {
                sourceObj = s;
                break;
            }
        }
        if (sourceObj.isEmpty() && !arr.isEmpty())
            sourceObj = arr.first().toObject();
    } else {
        sourceObj = sourceVal.toObject();
    }

    snap.listeners     = sourceObj.value("listeners").toInt(0);
    snap.peakListeners = sourceObj.value("listener_peak").toInt(snap.listeners);
    snap.bitrate       = sourceObj.value("bitrate").toInt(0);
    snap.streamTitle   = sourceObj.value("title").toString();

    // Server uptime — Icecast2 gives an ISO 8601 start time; compute delta
    const QString startIso = icestats.value("server_start_iso8601").toString();
    if (!startIso.isEmpty()) {
        const QDateTime startDt = QDateTime::fromString(startIso, Qt::ISODate);
        if (startDt.isValid())
            snap.uptimeSeconds = startDt.secsTo(QDateTime::currentDateTimeUtc());
    }

    pushSnapshot(snap);
}

void MonitorModule::parseShoutcast(const QByteArray& data) {
    // Shoutcast /7.html returns a comma-separated line:
    // CurrentListeners,StreamStatus,PeakListeners,MaxListeners,UniqueListeners,
    // Bitrate,SampleRate,StreamTitle
    const QString text = QString::fromUtf8(data).trimmed();
    // Strip HTML tags if present
    QString plain = text;
    plain.remove(QRegularExpression("<[^>]+>"));
    plain = plain.trimmed();

    const QStringList parts = plain.split(',');

    MonitorSnapshot snap;
    snap.timestampMs = QDateTime::currentMSecsSinceEpoch();

    if (parts.size() >= 1) snap.listeners     = parts[0].trimmed().toInt();
    if (parts.size() >= 3) snap.peakListeners = parts[2].trimmed().toInt();
    if (parts.size() >= 6) snap.bitrate       = parts[5].trimmed().toInt();
    if (parts.size() >= 8) snap.streamTitle   = parts[7].trimmed();

    pushSnapshot(snap);
}

void MonitorModule::parseMcaster1DNAS(const QByteArray& data) {
    // Mcaster1DNAS /stats returns JSON:
    // { "listeners": N, "peak_listeners": N, "bitrate": N,
    //   "uptime_seconds": N, "stream_title": "...", "mount": "..." }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "[MonitorModule] DNAS JSON parse error:" << err.errorString();
        emit pollError("JSON parse error: " + err.errorString());
        return;
    }

    const QJsonObject obj = doc.object();

    MonitorSnapshot snap;
    snap.timestampMs   = QDateTime::currentMSecsSinceEpoch();
    snap.listeners     = obj.value("listeners").toInt(0);
    snap.peakListeners = obj.value("peak_listeners").toInt(snap.listeners);
    snap.bitrate       = obj.value("bitrate").toInt(0);
    snap.uptimeSeconds = static_cast<qint64>(obj.value("uptime_seconds").toDouble(0.0));
    snap.streamTitle   = obj.value("stream_title").toString();

    pushSnapshot(snap);
}

// ─── History ring buffer ──────────────────────────────────────────────────────

void MonitorModule::pushSnapshot(const MonitorSnapshot& snap) {
    m_lastSnapshot = snap;

    m_history.append(snap);
    while (m_history.size() > kHistorySize)
        m_history.removeFirst();

    emit statsUpdated(snap.listeners, snap.peakListeners, snap.streamTitle);
    emit statusChanged(QString("Listeners: %1 | Peak: %2")
        .arg(snap.listeners)
        .arg(snap.peakListeners));
}

// ─── State persistence ────────────────────────────────────────────────────────

void MonitorModule::saveState(QSettings& s) {
    s.setValue("monitor/host",            m_config.host);
    s.setValue("monitor/port",            m_config.port);
    s.setValue("monitor/mount",           m_config.mount);
    s.setValue("monitor/password",        m_config.password);
    s.setValue("monitor/serverType",      static_cast<int>(m_config.serverType));
    s.setValue("monitor/pollIntervalSec", m_config.pollIntervalSec);
}

void MonitorModule::loadState(QSettings& s) {
    m_config.host            = s.value("monitor/host",            m_config.host).toString();
    m_config.port            = s.value("monitor/port",            m_config.port).toInt();
    m_config.mount           = s.value("monitor/mount",           m_config.mount).toString();
    m_config.password        = s.value("monitor/password",        m_config.password).toString();
    m_config.serverType      = static_cast<ServerType>(
                                   s.value("monitor/serverType", 0).toInt());
    m_config.pollIntervalSec = s.value("monitor/pollIntervalSec", m_config.pollIntervalSec).toInt();
}

} // namespace M1

// ─── C ABI plugin exports ─────────────────────────────────────────────────────

static Mcaster1PluginInfo s_monitorInfo{
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.monitor",
    "Monitor",
    "1.0.0",
    "*",
    "module",
    "Mcaster1",
    "Real-time server statistics monitor for Icecast2, Shoutcast, and Mcaster1DNAS"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_monitor_plugin_info() {
    return &s_monitorInfo;
}
MCASTER1_PLUGIN_API M1::IModule* mcaster1_monitor_create(IModuleHost*) {
    return new M1::MonitorModule();
}
MCASTER1_PLUGIN_API void mcaster1_monitor_destroy(M1::IModule* m) {
    delete m;
}
} // extern "C"
