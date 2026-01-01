#include "DnasPoller.h"
#include <QTcpSocket>
#include <QMutexLocker>
#include <QDateTime>
#include <QRegularExpression>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────────────
DnasPoller::DnasPoller(QObject* parent)
    : QThread(parent)
{}

DnasPoller::~DnasPoller()
{
    stop();
    wait(5000);
}

// ─────────────────────────────────────────────────────────────────────────────
QString DnasPoller::mountKey(const QString& host, int port, const QString& mount)
{
    return host + ":" + QString::number(port) + mount;
}

// ─────────────────────────────────────────────────────────────────────────────
void DnasPoller::registerMount(const QString& host, int port,
                                const QString& mount,
                                const QString& adminUser,
                                const QString& adminPass,
                                bool isDnas)
{
    QMutexLocker lk(&m_mutex);
    // Avoid duplicates
    for (const auto& r : m_regs)
        if (r.host == host && r.port == port && r.mount == mount) return;

    m_regs.append({host, mount, adminUser, adminPass, port, isDnas});
}

void DnasPoller::unregisterMount(const QString& host, int port, const QString& mount)
{
    QMutexLocker lk(&m_mutex);
    m_regs.removeIf([&](const Registration& r){
        return r.host == host && r.port == port && r.mount == mount;
    });
    m_stats.remove(mountKey(host, port, mount));
}

// ─────────────────────────────────────────────────────────────────────────────
DnasPoller::MountStats DnasPoller::stats(const QString& host, int port,
                                          const QString& mount) const
{
    QMutexLocker lk(&m_mutex);
    return m_stats.value(mountKey(host, port, mount));
}

void DnasPoller::setInterval(int secs)
{
    m_intervalSec.store(std::max(5, secs));
}

void DnasPoller::stop()
{
    m_stop.store(true);
    quit();
}

// ─────────────────────────────────────────────────────────────────────────────
void DnasPoller::run()
{
    qInfo() << "[DnasPoller] started";
    while (!m_stop.load()) {
        pollOnce();
        // Sleep in 250ms chunks so stop() is responsive
        const int totalMs = m_intervalSec.load() * 1000;
        int slept = 0;
        while (!m_stop.load() && slept < totalMs) {
            QThread::msleep(250);
            slept += 250;
        }
    }
    qInfo() << "[DnasPoller] stopped";
}

// ─────────────────────────────────────────────────────────────────────────────
void DnasPoller::pollOnce()
{
    // Build a deduplicated list of servers to fetch
    struct ServerEntry {
        QString host, adminUser, adminPass;
        int     port;
        bool    isDnas;
        QStringList mounts;
    };
    QList<ServerEntry> servers;

    {
        QMutexLocker lk(&m_mutex);
        for (const auto& r : m_regs) {
            bool found = false;
            for (auto& s : servers) {
                if (s.host == r.host && s.port == r.port) {
                    s.mounts << r.mount;
                    found = true;
                    break;
                }
            }
            if (!found)
                servers.append({r.host, r.adminUser, r.adminPass, r.port, r.isDnas, {r.mount}});
        }
    }

    for (const auto& srv : servers) {
        if (m_stop.load()) return;
        fetchServer(srv.host, srv.port, srv.adminUser, srv.adminPass, srv.isDnas);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void DnasPoller::fetchServer(const QString& host, int port,
                              const QString& adminUser, const QString& adminPass,
                              bool isDnas)
{
    // Both DNAS and Icecast2 use /admin/stats for raw XML
    Q_UNUSED(isDnas);
    const QString endpoint = "/admin/stats";

    const QString credentials = QString(adminUser + ":" + adminPass)
                                .toUtf8().toBase64();
    QByteArray req;
    req += "GET " + endpoint.toUtf8() + " HTTP/1.0\r\n";
    req += "Host: " + host.toUtf8() + ":" + QByteArray::number(port) + "\r\n";
    req += "Authorization: Basic " + credentials.toUtf8() + "\r\n";
    req += "User-Agent: Mcaster1Studio/1.0\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";

    QTcpSocket sock;
    sock.connectToHost(host, static_cast<quint16>(port));
    if (!sock.waitForConnected(3000)) {
        qWarning() << "[DnasPoller] connect failed:" << host << port;
        return;
    }
    sock.write(req);
    sock.waitForBytesWritten(2000);

    QByteArray response;
    while (sock.waitForReadyRead(3000))
        response += sock.readAll();

    sock.close();

    // Strip HTTP headers (find double CRLF)
    const int bodyStart = response.indexOf("\r\n\r\n");
    const QByteArray xml = (bodyStart >= 0)
                            ? response.mid(bodyStart + 4)
                            : response;

    if (xml.isEmpty()) {
        qWarning() << "[DnasPoller] empty response from" << host << port;
        return;
    }

    // Parse and distribute stats to all registered mounts on this server
    QMutexLocker lk(&m_mutex);
    for (const auto& r : m_regs) {
        if (r.host != host || r.port != port) continue;
        MountStats s = parseXml(xml, r.mount);
        s.updatedAtMs = QDateTime::currentMSecsSinceEpoch();
        const QString key = mountKey(host, port, r.mount);
        m_stats[key] = s;
        emit statsUpdated(host, port, r.mount, s);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
DnasPoller::MountStats DnasPoller::parseXml(const QByteArray& xml, const QString& mount)
{
    MountStats out;
    const QString body = QString::fromUtf8(xml);

    // Find <source mount="/xxx"> block
    // Match case-insensitively; mounts may or may not include leading slash
    const QString escapedMount = mount.startsWith('/') ? mount : "/" + mount;
    const QRegularExpression srcRe(
        R"(<source[^>]+mount\s*=\s*["'])" + QRegularExpression::escape(escapedMount) + R"(["'][^>]*>(.+?)</source>)",
        QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);

    auto srcMatch = srcRe.match(body);
    if (!srcMatch.hasMatch()) {
        // Try without the leading slash or case difference
        qDebug() << "[DnasPoller] no <source> block found for mount" << mount;
        return out;
    }
    const QString block = srcMatch.captured(1);

    // Extract fields
    auto extract = [&](const QString& tag) -> QString {
        const QRegularExpression re("<" + tag + R"(\b[^>]*>([^<]*)<\/)" + tag + ">",
                                    QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(block);
        return m.hasMatch() ? m.captured(1).trimmed() : QString();
    };

    const QString lisStr  = extract("listeners");
    const QString peakStr = extract("listener_peak");
    const QString title   = extract("title");

    if (!lisStr.isEmpty())  out.listeners    = lisStr.toInt();
    if (!peakStr.isEmpty()) out.listenerPeak = peakStr.toInt();
    if (!title.isEmpty())   out.currentTitle = title;

    return out;
}
