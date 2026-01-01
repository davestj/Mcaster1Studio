#pragma once
#include <QThread>
#include <QMutex>
#include <QList>
#include <QMap>
#include <QString>
#include <atomic>

/// DnasPoller — background thread that polls listener stats from streaming servers.
///
/// Supports Mcaster1DNAS (priority) and Icecast2.
/// Polls `/admin/mcaster1stats` for DNAS targets, `/admin/stats` for Icecast2.
///
/// One TCP fetch is performed per unique host:port per poll cycle to avoid
/// duplicate requests when multiple encoder slots target the same server.
/// Stats are distributed to all mounts on that server automatically.
///
/// Usage:
///   1. registerMount() for each active encoder slot.
///   2. start() to begin polling.
///   3. Call stats() or connect to statsUpdated() for live data.
///   4. stop() + wait() before destruction.
class DnasPoller : public QThread {
    Q_OBJECT

public:
    struct MountStats {
        int     listeners    = -1;   ///< -1 = not yet polled / unavailable
        int     listenerPeak = 0;
        QString currentTitle;
        qint64  updatedAtMs  = 0;    ///< QDateTime::currentMSecsSinceEpoch() at last update
    };

    explicit DnasPoller(QObject* parent = nullptr);
    ~DnasPoller() override;

    /// Register a mount for polling.
    /// May be called before or after start().
    void registerMount(const QString& host, int port,
                       const QString& mount,
                       const QString& adminUser,
                       const QString& adminPass,
                       bool isDnas);

    /// Remove a mount from polling.
    void unregisterMount(const QString& host, int port, const QString& mount);

    /// Snapshot of the latest stats for a specific mount (thread-safe).
    MountStats stats(const QString& host, int port, const QString& mount) const;

    /// Set poll interval (default 15 s). Takes effect from the next cycle.
    void setInterval(int secs);

    /// Stop the background thread. Call wait() after this.
    void stop();

signals:
    /// Emitted (via Qt::QueuedConnection) when stats for a mount are refreshed.
    void statsUpdated(const QString& host, int port,
                      const QString& mount, MountStats stats);

protected:
    void run() override;

private:
    struct Registration {
        QString host, mount, adminUser, adminPass;
        int     port  = 8000;
        bool    isDnas = false;
    };

    static QString mountKey(const QString& host, int port, const QString& mount);

    void pollOnce();
    void fetchServer(const QString& host, int port,
                     const QString& adminUser, const QString& adminPass,
                     bool isDnas);
    MountStats parseXml(const QByteArray& xml, const QString& mount);

    mutable QMutex          m_mutex;
    QList<Registration>     m_regs;
    QMap<QString,MountStats> m_stats;   // key = mountKey(host,port,mount)

    std::atomic<int>  m_intervalSec{15};
    std::atomic<bool> m_stop{false};
};
