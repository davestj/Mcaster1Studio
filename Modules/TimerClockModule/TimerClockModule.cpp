/// @file   TimerClockModule.cpp
/// @path   Modules/TimerClockModule/TimerClockModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-TimerClock — Master Timer and Clock Module Implementation
/// @purpose Implements the master clock display, multiple simultaneous timer
///          instances with countdown/countup, warning/alarm thresholds, and
///          segment timing for the Church Surface.
/// @reason  Foundation layer — no dependencies. We reuse the ClockModule's
///          proven 3D LCD-panel rendering pattern for the widget, while adding
///          the timer management engine that ServiceRunner and StageMon need.
/// @changelog
///   2026-03-09  Initial implementation

#include "TimerClockModule.h"
#include <QTimer>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QListWidget>
#include <QPainter>
#include <QLinearGradient>
#include <QSettings>
#include <QDebug>
#include <QInputDialog>

namespace M1 {

// ─── Anonymous namespace: widget implementations ─────────────────────────────
namespace {

// ─── MasterClockPanel — 3D LCD-style clock display ──────────────────────────
/// We reuse the proven ClockWidget rendering pattern from ClockModule:
/// a recessed dark LCD panel with amber glowing digits, but we add a
/// second row showing the active timer (if any) in a contrasting color.
class MasterClockPanel : public QWidget {
    Q_OBJECT
public:
    explicit MasterClockPanel(TimerClockModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_module(mod)
    {
        setObjectName("TimerClockPanel");
        setMinimumSize(280, 100);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        setToolTip("Master clock — synced to system time");

        connect(mod, &TimerClockModule::masterClockTick, this,
                [this](const QString& time, const QString& date) {
            m_timeStr = time;
            m_dateStr = date;
            update();
        });

        // We also listen for timer ticks to show the primary active timer
        connect(mod, &TimerClockModule::timerTick, this,
                [this](int id, qint64 /*elapsed*/, qint64 /*remaining*/) {
            auto* t = m_module->timer(id);
            if (t && t->state == TimerInstance::State::Running) {
                m_activeTimerName = t->name;
                m_activeTimerStr  = t->formatted();
                m_timerWarning    = t->isWarning();
                m_timerExpired    = t->isExpired();
                update();
            }
        });

        connect(mod, &TimerClockModule::timerStateChanged, this,
                [this](int /*id*/, TimerInstance::State state) {
            if (state == TimerInstance::State::Stopped) {
                m_activeTimerStr.clear();
                m_activeTimerName.clear();
                m_timerWarning = false;
                m_timerExpired = false;
                update();
            }
        });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        const int w = width(), h = height();

        // ── 3D raised outer border ──────────────────────────────────────
        p.setPen(QColor(68, 72, 82));
        p.drawLine(0, 0, w - 2, 0);
        p.drawLine(0, 0, 0, h - 2);
        p.setPen(QColor(0, 0, 0));
        p.drawLine(0, h - 1, w - 1, h - 1);
        p.drawLine(w - 1, 0, w - 1, h - 1);

        // ── Dark LCD background ─────────────────────────────────────────
        {
            QLinearGradient bg(0, 1, 0, h - 1);
            bg.setColorAt(0.0, QColor(14, 18, 24));
            bg.setColorAt(1.0, QColor(7, 10, 15));
            p.fillRect(1, 1, w - 2, h - 2, bg);
        }

        // ── Inner recessed ring ─────────────────────────────────────────
        p.setPen(QColor(0, 0, 0));
        p.drawLine(1, 1, w - 3, 1);
        p.drawLine(1, 1, 1, h - 3);
        p.setPen(QColor(28, 34, 44));
        p.drawLine(1, h - 2, w - 2, h - 2);
        p.drawLine(w - 2, 1, w - 2, h - 2);

        // ── Colors ──────────────────────────────────────────────────────
        const QColor kAmber    (242, 168, 12);
        const QColor kAmberGlow(255, 200, 60);
        const QColor kAmberDim (145, 88, 4);
        const QColor kAmberShdw(30, 18, 0);
        const QColor kGreen    (34, 197, 94);
        const QColor kRed      (239, 68, 68);
        const QColor kYellow   (250, 204, 21);

        const int pad = 7;
        bool hasTimer = !m_activeTimerStr.isEmpty();
        int timeH = hasTimer ? (h * 48 / 100) : (h * 55 / 100);
        int dateH = hasTimer ? (h * 18 / 100) : (h * 25 / 100);

        // ── Time row ────────────────────────────────────────────────────
        QFont tf("Consolas", 22, QFont::Bold);
        tf.setStyleHint(QFont::Monospace);
        p.setFont(tf);

        const QRect tRect(pad, 2, w - 2 * pad, timeH);
        p.setPen(kAmberShdw);
        p.drawText(tRect.translated(1, 1), Qt::AlignHCenter | Qt::AlignVCenter, m_timeStr);
        p.setPen(kAmberGlow);
        p.drawText(tRect, Qt::AlignHCenter | Qt::AlignVCenter, m_timeStr);

        // ── Date row ────────────────────────────────────────────────────
        QFont df("Consolas", 9);
        df.setStyleHint(QFont::Monospace);
        p.setFont(df);

        const QRect dRect(pad, timeH + 2, w - 2 * pad, dateH);
        p.setPen(kAmberShdw);
        p.drawText(dRect.translated(1, 1), Qt::AlignHCenter | Qt::AlignVCenter, m_dateStr);
        p.setPen(kAmber);
        p.drawText(dRect, Qt::AlignHCenter | Qt::AlignVCenter, m_dateStr);

        // ── Active timer row (if running) ───────────────────────────────
        if (hasTimer) {
            int timerY = timeH + dateH + 4;
            int timerH = h - timerY - 4;

            // Timer label (name)
            QFont lf("Consolas", 8);
            lf.setStyleHint(QFont::Monospace);
            p.setFont(lf);
            p.setPen(kAmberDim);
            p.drawText(QRect(pad, timerY, w / 2 - pad, timerH),
                       Qt::AlignLeft | Qt::AlignVCenter, m_activeTimerName);

            // Timer value — color coded
            QFont vf("Consolas", 16, QFont::Bold);
            vf.setStyleHint(QFont::Monospace);
            p.setFont(vf);

            QColor timerColor = kGreen;
            if (m_timerExpired)  timerColor = kRed;
            else if (m_timerWarning) timerColor = kYellow;

            const QRect vRect(w / 2, timerY, w / 2 - pad, timerH);
            p.setPen(kAmberShdw);
            p.drawText(vRect.translated(1, 1), Qt::AlignRight | Qt::AlignVCenter, m_activeTimerStr);
            p.setPen(timerColor);
            p.drawText(vRect, Qt::AlignRight | Qt::AlignVCenter, m_activeTimerStr);
        }
    }

private:
    TimerClockModule* m_module;
    QString m_timeStr  = "12:00:00 AM";
    QString m_dateStr  = "Sun  Mar 9  2026";
    QString m_activeTimerName;
    QString m_activeTimerStr;
    bool    m_timerWarning = false;
    bool    m_timerExpired = false;
};

// ─── TimerListItem — custom row for the timer list ──────────────────────────
class TimerListItem : public QWidget {
    Q_OBJECT
public:
    explicit TimerListItem(int timerId, TimerClockModule* mod, QWidget* parent = nullptr)
        : QWidget(parent), m_id(timerId), m_module(mod)
    {
        setObjectName("TimerListItem");
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(4, 2, 4, 2);
        lay->setSpacing(4);

        m_nameLabel = new QLabel(this);
        m_nameLabel->setMinimumWidth(100);
        lay->addWidget(m_nameLabel);

        m_timeLabel = new QLabel("00:00", this);
        m_timeLabel->setObjectName("TimerListTime");
        m_timeLabel->setMinimumWidth(80);
        m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        QFont f("Consolas", 14, QFont::Bold);
        f.setStyleHint(QFont::Monospace);
        m_timeLabel->setFont(f);
        lay->addWidget(m_timeLabel);

        m_startBtn = new QPushButton("Start", this);
        m_startBtn->setFixedWidth(52);
        m_startBtn->setToolTip("Start / Resume this timer");
        lay->addWidget(m_startBtn);

        m_pauseBtn = new QPushButton("Pause", this);
        m_pauseBtn->setFixedWidth(52);
        m_pauseBtn->setToolTip("Pause this timer");
        m_pauseBtn->setEnabled(false);
        lay->addWidget(m_pauseBtn);

        m_stopBtn = new QPushButton("Stop", this);
        m_stopBtn->setFixedWidth(52);
        m_stopBtn->setToolTip("Stop and reset this timer");
        lay->addWidget(m_stopBtn);

        connect(m_startBtn, &QPushButton::clicked, this, [this]() {
            auto* t = m_module->timer(m_id);
            if (t && t->state == TimerInstance::State::Paused)
                m_module->resumeTimer(m_id);
            else
                m_module->startTimer(m_id);
        });
        connect(m_pauseBtn, &QPushButton::clicked, this, [this]() {
            m_module->pauseTimer(m_id);
        });
        connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
            m_module->stopTimer(m_id);
        });

        // Listen for timer ticks to update display
        connect(mod, &TimerClockModule::timerTick, this,
                [this](int id, qint64 /*elapsed*/, qint64 /*remaining*/) {
            if (id != m_id) return;
            auto* t = m_module->timer(m_id);
            if (!t) return;
            m_timeLabel->setText(t->formatted());
        });

        // State changes → update button enable states
        connect(mod, &TimerClockModule::timerStateChanged, this,
                [this](int id, TimerInstance::State state) {
            if (id != m_id) return;
            m_startBtn->setEnabled(state != TimerInstance::State::Running);
            m_pauseBtn->setEnabled(state == TimerInstance::State::Running);
            m_startBtn->setText(
                state == TimerInstance::State::Paused ? "Resume" : "Start");
        });

        // Initialize labels
        auto* t = mod->timer(m_id);
        if (t) {
            m_nameLabel->setText(t->name);
            m_timeLabel->setText(t->formatted());
        }
    }

    int timerId() const { return m_id; }

private:
    int m_id;
    TimerClockModule* m_module;
    QLabel*      m_nameLabel = nullptr;
    QLabel*      m_timeLabel = nullptr;
    QPushButton* m_startBtn  = nullptr;
    QPushButton* m_pauseBtn  = nullptr;
    QPushButton* m_stopBtn   = nullptr;
};

} // anonymous namespace

// ─── TimerClockModule implementation ─────────────────────────────────────────

TimerClockModule::TimerClockModule(QObject* parent)
    : IModule(parent)
    , m_masterTimer(new QTimer(this))
    , m_timerTick(new QTimer(this))
{
    // Master clock: 1Hz for seconds display
    m_masterTimer->setInterval(500);
    connect(m_masterTimer, &QTimer::timeout, this, &TimerClockModule::onMasterTick);

    // Timer instances: 100ms for smooth countdown display
    m_timerTick->setInterval(100);
    connect(m_timerTick, &QTimer::timeout, this, &TimerClockModule::onTimerTick);
}

TimerClockModule::~TimerClockModule() {
    shutdown();
}

void TimerClockModule::initialize() {
    m_masterTimer->start();
    m_timerTick->start();
    onMasterTick();
    qInfo() << "[TimerClockModule] Initialized — master clock + timer engine running";
}

void TimerClockModule::shutdown() {
    m_masterTimer->stop();
    m_timerTick->stop();
}

QWidget* TimerClockModule::createWidget(QWidget* parent) {
    auto* container = new QWidget(parent);
    container->setObjectName("TimerClockWidget");
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);

    // ── Master clock panel (3D LCD) ─────────────────────────────────────
    auto* clockPanel = new MasterClockPanel(this, container);
    clockPanel->setFixedHeight(110);
    vlay->addWidget(clockPanel);

    // ── Timer list area ─────────────────────────────────────────────────
    auto* timerGroup = new QGroupBox("Timers", container);
    timerGroup->setObjectName("TimerClockTimerGroup");
    auto* timerLay = new QVBoxLayout(timerGroup);
    timerLay->setContentsMargins(4, 8, 4, 4);
    timerLay->setSpacing(2);

    auto* timerList = new QWidget(timerGroup);
    timerList->setObjectName("TimerClockList");
    auto* listLay = new QVBoxLayout(timerList);
    listLay->setContentsMargins(0, 0, 0, 0);
    listLay->setSpacing(2);
    timerLay->addWidget(timerList, 1);

    // ── Add timer controls ──────────────────────────────────────────────
    auto* addRow = new QHBoxLayout();
    auto* addBtn = new QPushButton("+ New Timer", timerGroup);
    addBtn->setToolTip("Create a new countdown or countup timer");
    addRow->addWidget(addBtn);
    addRow->addStretch();
    timerLay->addLayout(addRow);

    vlay->addWidget(timerGroup, 1);

    // Wire add button
    connect(addBtn, &QPushButton::clicked, container, [this, listLay, timerList]() {
        bool ok;
        QString name = QInputDialog::getText(timerList, "New Timer",
            "Timer name:", QLineEdit::Normal, "Timer", &ok);
        if (!ok || name.isEmpty()) return;

        // We ask for mode and duration via a quick dialog
        QStringList modes = {"Count Up", "Count Down"};
        QString modeStr = QInputDialog::getItem(timerList, "Timer Mode",
            "Select timer mode:", modes, 0, false, &ok);
        if (!ok) return;

        auto mode = (modeStr == "Count Down")
            ? TimerInstance::Mode::CountDown
            : TimerInstance::Mode::CountUp;

        qint64 durationMs = 0;
        if (mode == TimerInstance::Mode::CountDown) {
            int minutes = QInputDialog::getInt(timerList, "Duration",
                "Countdown duration (minutes):", 30, 1, 600, 1, &ok);
            if (!ok) return;
            durationMs = minutes * 60000LL;
        }

        int id = createTimer(name, mode, durationMs);
        auto* item = new TimerListItem(id, this, timerList);
        listLay->addWidget(item);
    });

    // We populate any existing timers (from loadState)
    for (auto it = m_timers.begin(); it != m_timers.end(); ++it) {
        auto* item = new TimerListItem(it.key(), this, timerList);
        listLay->addWidget(item);
    }

    return container;
}

void TimerClockModule::saveState(QSettings& s) {
    s.beginGroup("TimerClockModule");
    s.setValue("timerCount", m_timers.size());
    int idx = 0;
    for (auto it = m_timers.constBegin(); it != m_timers.constEnd(); ++it, ++idx) {
        const QString prefix = QString("timer_%1/").arg(idx);
        s.setValue(prefix + "id",         it.key());
        s.setValue(prefix + "name",       it->name);
        s.setValue(prefix + "mode",       static_cast<int>(it->mode));
        s.setValue(prefix + "durationMs", it->durationMs);
        s.setValue(prefix + "warningMs",  it->warningMs);
        s.setValue(prefix + "alarmMs",    it->alarmMs);
    }
    s.endGroup();
}

void TimerClockModule::loadState(QSettings& s) {
    s.beginGroup("TimerClockModule");
    int count = s.value("timerCount", 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString prefix = QString("timer_%1/").arg(i);
        int id            = s.value(prefix + "id", 0).toInt();
        QString name      = s.value(prefix + "name", "Timer").toString();
        auto mode         = static_cast<TimerInstance::Mode>(s.value(prefix + "mode", 0).toInt());
        qint64 durationMs = s.value(prefix + "durationMs", 0).toLongLong();
        qint64 warningMs  = s.value(prefix + "warningMs", 300000).toLongLong();
        qint64 alarmMs    = s.value(prefix + "alarmMs", 0).toLongLong();

        if (id >= m_nextTimerId) m_nextTimerId = id + 1;

        TimerInstance ti;
        ti.name       = name;
        ti.mode       = mode;
        ti.durationMs = durationMs;
        ti.warningMs  = warningMs;
        ti.alarmMs    = alarmMs;
        m_timers.insert(id, ti);
    }
    s.endGroup();
}

// ─── Timer management ────────────────────────────────────────────────────────

int TimerClockModule::createTimer(const QString& name,
                                   TimerInstance::Mode mode,
                                   qint64 durationMs)
{
    int id = m_nextTimerId++;
    TimerInstance ti;
    ti.name       = name;
    ti.mode       = mode;
    ti.durationMs = durationMs;
    m_timers.insert(id, ti);
    qInfo() << "[TimerClockModule] Created timer" << id << ":" << name
            << (mode == TimerInstance::Mode::CountDown ? "countdown" : "countup")
            << durationMs << "ms";
    return id;
}

void TimerClockModule::startTimer(int id) {
    auto it = m_timers.find(id);
    if (it == m_timers.end()) return;

    it->elapsedMs     = 0;
    it->pauseAccumMs  = 0;
    it->wallClock.start();
    it->state = TimerInstance::State::Running;

    emit timerStateChanged(id, TimerInstance::State::Running);
    qInfo() << "[TimerClockModule] Timer" << id << "started:" << it->name;
}

void TimerClockModule::pauseTimer(int id) {
    auto it = m_timers.find(id);
    if (it == m_timers.end() || it->state != TimerInstance::State::Running) return;

    it->pauseAccumMs = it->wallClock.elapsed();
    it->elapsedMs    = it->pauseAccumMs;
    it->state        = TimerInstance::State::Paused;

    emit timerStateChanged(id, TimerInstance::State::Paused);
    qInfo() << "[TimerClockModule] Timer" << id << "paused at" << it->formatted();
}

void TimerClockModule::resumeTimer(int id) {
    auto it = m_timers.find(id);
    if (it == m_timers.end() || it->state != TimerInstance::State::Paused) return;

    // We restart the wall clock but remember how much time we had accumulated
    it->wallClock.start();
    it->state = TimerInstance::State::Running;

    emit timerStateChanged(id, TimerInstance::State::Running);
    qInfo() << "[TimerClockModule] Timer" << id << "resumed from" << it->formatted();
}

void TimerClockModule::stopTimer(int id) {
    auto it = m_timers.find(id);
    if (it == m_timers.end()) return;

    it->elapsedMs    = 0;
    it->pauseAccumMs = 0;
    it->state        = TimerInstance::State::Stopped;

    emit timerStateChanged(id, TimerInstance::State::Stopped);
    qInfo() << "[TimerClockModule] Timer" << id << "stopped:" << it->name;
}

void TimerClockModule::removeTimer(int id) {
    m_timers.remove(id);
    qInfo() << "[TimerClockModule] Timer" << id << "removed";
}

void TimerClockModule::setTimerDuration(int id, qint64 durationMs) {
    auto it = m_timers.find(id);
    if (it != m_timers.end()) it->durationMs = durationMs;
}

void TimerClockModule::setTimerWarning(int id, qint64 warningMs) {
    auto it = m_timers.find(id);
    if (it != m_timers.end()) it->warningMs = warningMs;
}

void TimerClockModule::setTimerAlarm(int id, qint64 alarmMs) {
    auto it = m_timers.find(id);
    if (it != m_timers.end()) it->alarmMs = alarmMs;
}

const TimerInstance* TimerClockModule::timer(int id) const {
    auto it = m_timers.constFind(id);
    return (it != m_timers.constEnd()) ? &it.value() : nullptr;
}

// ─── Tick handlers ───────────────────────────────────────────────────────────

void TimerClockModule::onMasterTick() {
    const QDateTime now = QDateTime::currentDateTime();
    m_masterTimeStr = now.toString("h:mm:ss AP");
    m_masterDateStr = now.toString("ddd  MMM d  yyyy");
    emit masterClockTick(m_masterTimeStr, m_masterDateStr);
    emit statusChanged(m_masterTimeStr);
}

void TimerClockModule::onTimerTick() {
    for (auto it = m_timers.begin(); it != m_timers.end(); ++it) {
        if (it->state != TimerInstance::State::Running) continue;

        // We compute elapsed from wall clock + accumulated pause time
        it->elapsedMs = it->pauseAccumMs + it->wallClock.elapsed();

        const int id = it.key();
        const qint64 remaining = it->remainingMs();
        emit timerTick(id, it->elapsedMs, remaining);

        // Check warning threshold
        if (it->isWarning()) {
            emit timerWarning(id);
        }

        // Check expiry (countdown only)
        if (it->isExpired() && it->state != TimerInstance::State::Expired) {
            it->state = TimerInstance::State::Expired;
            emit timerExpired(id);
            emit timerStateChanged(id, TimerInstance::State::Expired);
            qInfo() << "[TimerClockModule] Timer" << id << "EXPIRED:" << it->name;
        }
    }
}

} // namespace M1

// ─── Plugin C ABI exports ────────────────────────────────────────────────────
static Mcaster1PluginInfo s_timerClockInfo = {
    MCASTER1_PLUGIN_API_VERSION,
    "com.mcaster1.church.timerclock",
    "Timer / Clock",
    "1.0.0",
    "church",
    "module",
    "Mcaster1",
    "Master clock and timer system — countdown/countup timers, alarm thresholds, segment timing"
};

extern "C" {
MCASTER1_PLUGIN_API Mcaster1PluginInfo* mcaster1_timerclock_plugin_info() { return &s_timerClockInfo; }
MCASTER1_PLUGIN_API IModule* mcaster1_timerclock_create_module(IModuleHost*) {
    return new M1::TimerClockModule();
}
MCASTER1_PLUGIN_API void mcaster1_timerclock_destroy_module(IModule* m) { delete m; }
}

#include "TimerClockModule.moc"
