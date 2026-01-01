#pragma once
#include <QString>
#include <QList>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>

/// EncoderEventLog — per-encoder event logging with level-tagged entries.
///
/// Thread-safe circular buffer of log entries. Each EncoderSlot owns one.
/// The log viewer (EncoderLogDialog) reads from this buffer.
///
/// Log levels match the DSP encoder reference:
///   DEBUG, INFO, WARN, ERROR, CONNECT, AUTH, ICY_META
class EncoderEventLog : public QObject {
    Q_OBJECT

public:
    enum class Level {
        DEBUG   = 0,
        INFO    = 1,
        WARN    = 2,
        ERR     = 3,   // Avoid Windows ERROR macro conflict
        CONNECT = 4,
        AUTH    = 5,
        ICY_META = 6
    };

    struct Entry {
        QDateTime   timestamp;
        Level       level;
        QString     tag;
        QString     message;
    };

    explicit EncoderEventLog(QObject* parent = nullptr)
        : QObject(parent) {}

    /// Add a log entry (thread-safe, callable from encoder thread).
    void log(Level level, const QString& tag, const QString& message)
    {
        Entry e;
        e.timestamp = QDateTime::currentDateTime();
        e.level     = level;
        e.tag       = tag;
        e.message   = message;

        {
            QMutexLocker lk(&m_mutex);
            m_entries.append(e);
            if (m_entries.size() > m_maxEntries)
                m_entries.removeFirst();
        }
        emit entryAdded(e);
    }

    /// Convenience methods matching DSP encoder reference.
    void logDebug  (const QString& tag, const QString& msg) { log(Level::DEBUG,    tag, msg); }
    void logInfo   (const QString& tag, const QString& msg) { log(Level::INFO,     tag, msg); }
    void logWarn   (const QString& tag, const QString& msg) { log(Level::WARN,     tag, msg); }
    void logError  (const QString& tag, const QString& msg) { log(Level::ERR,      tag, msg); }
    void logConnect(const QString& tag, const QString& msg) { log(Level::CONNECT,  tag, msg); }
    void logAuth   (const QString& tag, const QString& msg) { log(Level::AUTH,     tag, msg); }
    void logIcy    (const QString& tag, const QString& msg) { log(Level::ICY_META, tag, msg); }

    /// Thread-safe snapshot of all entries.
    QList<Entry> entries() const
    {
        QMutexLocker lk(&m_mutex);
        return m_entries;
    }

    /// Clear all entries.
    void clear()
    {
        QMutexLocker lk(&m_mutex);
        m_entries.clear();
    }

    int maxEntries() const { return m_maxEntries; }
    void setMaxEntries(int n) { m_maxEntries = n; }

    /// Format a log entry as a display string.
    static QString formatEntry(const Entry& e)
    {
        return QString("[%1] [%2] %3  %4")
            .arg(e.timestamp.toString("hh:mm:ss"))
            .arg(levelStr(e.level))
            .arg(e.tag, e.message);
    }

    static QString levelStr(Level l)
    {
        switch (l) {
            case Level::DEBUG:    return "DEBUG";
            case Level::INFO:     return "INFO ";
            case Level::WARN:     return "WARN ";
            case Level::ERR:      return "ERROR";
            case Level::CONNECT:  return "CONN ";
            case Level::AUTH:     return "AUTH ";
            case Level::ICY_META: return "ICY  ";
        }
        return "?    ";
    }

signals:
    void entryAdded(const EncoderEventLog::Entry& entry);

private:
    mutable QMutex  m_mutex;
    QList<Entry>    m_entries;
    int             m_maxEntries = 500;
};

Q_DECLARE_METATYPE(EncoderEventLog::Entry)
