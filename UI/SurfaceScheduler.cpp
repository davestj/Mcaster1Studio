#include "SurfaceScheduler.h"
#include <QTimer>
#include <QTime>
#include <QDebug>

SurfaceScheduler::SurfaceScheduler(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(30000); // poll every 30 s
    connect(m_timer, &QTimer::timeout, this, &SurfaceScheduler::onPoll);
}

void SurfaceScheduler::start() {
    m_lastPoll = QTime::currentTime();
    m_timer->start();
}

void SurfaceScheduler::stop() {
    m_timer->stop();
}

void SurfaceScheduler::addEvent(const ScheduledEvent& ev) {
    ScheduledEvent e = ev;
    e.id = m_nextId++;
    m_events.append(e);
}

void SurfaceScheduler::removeEvent(int id) {
    for (int i = 0; i < m_events.size(); ++i) {
        if (m_events[i].id == id) {
            m_events.removeAt(i);
            return;
        }
    }
}

void SurfaceScheduler::updateEvent(const ScheduledEvent& ev) {
    for (auto& e : m_events) {
        if (e.id == ev.id) {
            e = ev;
            return;
        }
    }
}

void SurfaceScheduler::onPoll() {
    const QTime now = QTime::currentTime();

    for (auto& ev : m_events) {
        if (!ev.enabled) continue;

        // Fire if trigger time falls within the window [lastPoll, now]
        // (handles crossing midnight)
        const int nowSecs  = now.msecsSinceStartOfDay()  / 1000;
        const int lastSecs = m_lastPoll.msecsSinceStartOfDay() / 1000;
        const int trigSecs = ev.triggerTime.msecsSinceStartOfDay() / 1000;

        bool shouldFire = false;
        if (lastSecs <= nowSecs) {
            shouldFire = (trigSecs > lastSecs && trigSecs <= nowSecs);
        } else {
            // Midnight crossing
            shouldFire = (trigSecs > lastSecs || trigSecs <= nowSecs);
        }

        if (!shouldFire) continue;

        qInfo() << "[Scheduler] Firing event:" << ev.label << "at" << ev.triggerTime.toString("HH:mm");

        switch (ev.type) {
        case ScheduledEventType::LoadPlaylist:
            emit loadPlaylist(ev.data);
            break;
        case ScheduledEventType::InsertJingle:
            emit insertJingle(ev.data, ev.data2.toInt());
            break;
        case ScheduledEventType::InsertVideo:
            emit insertVideo(ev.data);
            break;
        case ScheduledEventType::LoadMedia:
            emit loadMedia(ev.data, ev.data2.toInt());
            break;
        case ScheduledEventType::RunCommand:
            // Future: emit runCommand(ev.data);
            break;
        }

        emit eventFired(ev);

        if (!ev.repeat)
            ev.enabled = false;
    }

    m_lastPoll = now;
}
