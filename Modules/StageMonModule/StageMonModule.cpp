/// @file   StageMonModule.cpp
/// @path   Modules/StageMonModule/StageMonModule.cpp
/// @author Mcaster1 / dstjohn
/// @date   2026-03-09
/// @title  M1S-StageMon — Stage Confidence Monitor Implementation
/// @purpose Implements the stage confidence display that composites current +
///          next items from LyricsCaster, ScriptureCaster, and TimerClock.
///          We provide Worship, Sermon, and Combined display modes.
/// @reason  The stage team needs more context than the congregation —
///          seeing the next section, elapsed time, and arrangement position.
/// @changelog
///   2026-03-09  Initial implementation

#include "StageMonModule.h"
#include "GraphicsEngineModule.h"
#include "LyricsCasterModule.h"
#include "ScriptureCasterModule.h"
#include "TimerClockModule.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTextEdit>
#include <QPainter>
#include <QSettings>
#include <QDateTime>
#include <QFrame>
#include <QScrollArea>
#include <QSplitter>
#include <QApplication>

namespace {

// ─── InfoCard — a styled info block with title and body ─────────────────────
/// We reuse this pattern for showing "Current", "Next", "Timer", etc. blocks
/// in the stage monitor display.
class InfoCard : public QFrame {
public:
    explicit InfoCard(const QString& title, QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("StageMonInfoCard");
        setFrameShape(QFrame::StyledPanel);

        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(8, 6, 8, 6);
        lay->setSpacing(4);

        m_titleLabel = new QLabel(title);
        m_titleLabel->setObjectName("StageMonCardTitle");
        lay->addWidget(m_titleLabel);

        m_bodyLabel = new QLabel("—");
        m_bodyLabel->setObjectName("StageMonCardBody");
        m_bodyLabel->setWordWrap(true);
        lay->addWidget(m_bodyLabel, 1);
    }

    void setTitle(const QString& title) { m_titleLabel->setText(title); }
    void setBody(const QString& body)   { m_bodyLabel->setText(body); }
    void setBodyFont(const QFont& f)    { m_bodyLabel->setFont(f); }

private:
    QLabel* m_titleLabel = nullptr;
    QLabel* m_bodyLabel  = nullptr;
};

// ─── ClockBar — top bar showing current time and active timers ──────────────
class ClockBar : public QWidget {
public:
    explicit ClockBar(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName("StageMonClockBar");
        auto* lay = new QHBoxLayout(this);
        lay->setContentsMargins(8, 4, 8, 4);

        m_clockLabel = new QLabel;
        m_clockLabel->setObjectName("StageMonClock");
        QFont clockFont;
        clockFont.setPointSize(18);
        clockFont.setBold(true);
        m_clockLabel->setFont(clockFont);
        lay->addWidget(m_clockLabel);

        lay->addStretch(1);

        m_timerLabel = new QLabel;
        m_timerLabel->setObjectName("StageMonTimer");
        QFont timerFont;
        timerFont.setPointSize(18);
        timerFont.setBold(true);
        m_timerLabel->setFont(timerFont);
        lay->addWidget(m_timerLabel);
    }

    void setClock(const QString& text) { m_clockLabel->setText(text); }
    void setTimer(const QString& text) { m_timerLabel->setText(text); }

private:
    QLabel* m_clockLabel = nullptr;
    QLabel* m_timerLabel = nullptr;
};

// ─── WorshipView — lyrics-focused stage display ─────────────────────────────
/// Shows current section text (large), next section preview, arrangement
/// position indicator, and key/tempo reference for the worship team.
class WorshipView : public QWidget {
public:
    explicit WorshipView(M1::LyricsCasterModule* lyrics, QWidget* parent = nullptr)
        : QWidget(parent), m_lyrics(lyrics)
    {
        setObjectName("StageMonWorshipView");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        // Song title + arrangement position
        m_songTitle = new QLabel("No Song Loaded");
        m_songTitle->setObjectName("StageMonSongTitle");
        QFont titleFont;
        titleFont.setPointSize(14);
        titleFont.setBold(true);
        m_songTitle->setFont(titleFont);
        lay->addWidget(m_songTitle);

        // Current section (large text)
        m_currentCard = new InfoCard("CURRENT");
        QFont currentFont;
        currentFont.setPointSize(20);
        m_currentCard->setBodyFont(currentFont);
        lay->addWidget(m_currentCard, 3);

        // Next section preview
        m_nextCard = new InfoCard("NEXT");
        QFont nextFont;
        nextFont.setPointSize(14);
        m_nextCard->setBodyFont(nextFont);
        lay->addWidget(m_nextCard, 1);

        // Arrangement position + key/tempo
        auto* footerRow = new QHBoxLayout;
        m_positionLabel = new QLabel("Section — / —");
        m_positionLabel->setObjectName("StageMonPosition");
        footerRow->addWidget(m_positionLabel);
        footerRow->addStretch(1);
        m_keyTempoLabel = new QLabel("");
        m_keyTempoLabel->setObjectName("StageMonKeyTempo");
        footerRow->addWidget(m_keyTempoLabel);
        lay->addLayout(footerRow);
    }

    void refresh() {
        if (!m_lyrics) {
            m_songTitle->setText("No Lyrics Module Connected");
            m_currentCard->setBody("—");
            m_nextCard->setBody("—");
            m_positionLabel->setText("");
            m_keyTempoLabel->setText("");
            return;
        }

        // Song title — we look up the current song by ID
        const M1::WorshipSong* song = m_lyrics->song(m_lyrics->currentSongId());
        if (!song || song->title.isEmpty()) {
            m_songTitle->setText("No Song Loaded");
            m_currentCard->setTitle("CURRENT");
            m_currentCard->setBody("—");
            m_nextCard->setBody("—");
            m_positionLabel->setText("");
            m_keyTempoLabel->setText("");
            return;
        }

        m_songTitle->setText(song->title +
            (song->author.isEmpty() ? "" : " — " + song->author));

        // Current section — use the direct API which returns a pointer
        const M1::SongSection* curSec = m_lyrics->currentSection();
        if (curSec) {
            m_currentCard->setTitle(
                QString("CURRENT — %1").arg(M1::sectionTypeName(curSec->type)));
            m_currentCard->setBody(curSec->text);
        } else if (m_lyrics->isBlank()) {
            m_currentCard->setTitle("CURRENT — BLANK");
            m_currentCard->setBody("[Screen is blank]");
        } else {
            m_currentCard->setTitle("CURRENT");
            m_currentCard->setBody("—");
        }

        // Next section preview — use the direct API
        const M1::SongSection* nextSec = m_lyrics->nextSectionPreview();
        if (nextSec) {
            m_nextCard->setBody(nextSec->text);
        } else {
            m_nextCard->setBody("[End of Song]");
        }

        // Arrangement position
        int arrIdx = m_lyrics->currentArrangementIndex();
        int totalSections = song->arrangement.isEmpty()
            ? song->sections.size() : song->arrangement.size();
        if (totalSections > 0) {
            m_positionLabel->setText(
                QString("Section %1 / %2").arg(arrIdx + 1).arg(totalSections));
        } else {
            m_positionLabel->setText("");
        }

        // Key/tempo reference
        QString keyTempo;
        if (!song->key.isEmpty()) keyTempo += "Key: " + song->key;
        if (song->bpm > 0) {
            if (!keyTempo.isEmpty()) keyTempo += "  |  ";
            keyTempo += QString("BPM: %1").arg(song->bpm);
        }
        m_keyTempoLabel->setText(keyTempo);
    }

private:
    M1::LyricsCasterModule* m_lyrics = nullptr;
    QLabel*    m_songTitle    = nullptr;
    InfoCard*  m_currentCard  = nullptr;
    InfoCard*  m_nextCard     = nullptr;
    QLabel*    m_positionLabel = nullptr;
    QLabel*    m_keyTempoLabel = nullptr;
};

// ─── SermonView — scripture/sermon-focused stage display ────────────────────
/// Shows current scripture or sermon point, next queue item, and sermon timer.
class SermonView : public QWidget {
public:
    explicit SermonView(M1::ScriptureCasterModule* scripture, QWidget* parent = nullptr)
        : QWidget(parent), m_scripture(scripture)
    {
        setObjectName("StageMonSermonView");
        auto* lay = new QVBoxLayout(this);
        lay->setContentsMargins(4, 4, 4, 4);
        lay->setSpacing(4);

        // Current scripture/sermon point (large)
        m_currentCard = new InfoCard("CURRENT SLIDE");
        QFont currentFont;
        currentFont.setPointSize(18);
        m_currentCard->setBodyFont(currentFont);
        lay->addWidget(m_currentCard, 3);

        // Next queue item
        m_nextCard = new InfoCard("NEXT IN QUEUE");
        QFont nextFont;
        nextFont.setPointSize(14);
        m_nextCard->setBodyFont(nextFont);
        lay->addWidget(m_nextCard, 1);

        // Queue position
        m_queuePos = new QLabel("Queue: — / —");
        m_queuePos->setObjectName("StageMonQueuePos");
        lay->addWidget(m_queuePos);
    }

    void refresh() {
        if (!m_scripture) {
            m_currentCard->setBody("No Scripture Module Connected");
            m_nextCard->setBody("—");
            m_queuePos->setText("");
            return;
        }

        int queueIdx  = m_scripture->currentIndex();
        int queueSize = m_scripture->queueSize();

        // Current item — use the direct API which returns a pointer
        const M1::SermonQueueItem* item = m_scripture->currentItem();
        if (item) {
            switch (item->type) {
                case M1::SermonQueueItem::Type::Scripture:
                    m_currentCard->setTitle("SCRIPTURE");
                    m_currentCard->setBody(item->reference + "\n\n" + item->text);
                    break;
                case M1::SermonQueueItem::Type::SermonPoint:
                    m_currentCard->setTitle("SERMON POINT");
                    m_currentCard->setBody(item->text);
                    break;
                case M1::SermonQueueItem::Type::Blank:
                    m_currentCard->setTitle("BLANK");
                    m_currentCard->setBody("[Screen is blank]");
                    break;
            }
        } else {
            m_currentCard->setTitle("CURRENT SLIDE");
            m_currentCard->setBody("—");
        }

        // Next item
        int nextIdx = queueIdx + 1;
        const M1::SermonQueueItem* next = m_scripture->queueItem(nextIdx);
        if (next) {
            switch (next->type) {
                case M1::SermonQueueItem::Type::Scripture:
                    m_nextCard->setTitle("NEXT — Scripture");
                    m_nextCard->setBody(next->reference);
                    break;
                case M1::SermonQueueItem::Type::SermonPoint:
                    m_nextCard->setTitle("NEXT — Sermon Point");
                    m_nextCard->setBody(next->text);
                    break;
                case M1::SermonQueueItem::Type::Blank:
                    m_nextCard->setTitle("NEXT — Blank");
                    m_nextCard->setBody("[Blank screen]");
                    break;
            }
        } else {
            m_nextCard->setTitle("NEXT IN QUEUE");
            m_nextCard->setBody("[End of Queue]");
        }

        // Queue position
        m_queuePos->setText(QString("Queue: %1 / %2")
            .arg(queueIdx + 1).arg(queueSize));
    }

private:
    M1::ScriptureCasterModule* m_scripture = nullptr;
    InfoCard*  m_currentCard = nullptr;
    InfoCard*  m_nextCard    = nullptr;
    QLabel*    m_queuePos    = nullptr;
};

// ─── StageMonWidget — main composite widget ─────────────────────────────────
class StageMonWidget : public QWidget {
    Q_OBJECT
public:
    explicit StageMonWidget(M1::StageMonModule* mod,
                            M1::LyricsCasterModule* lyrics,
                            M1::ScriptureCasterModule* scripture,
                            M1::TimerClockModule* timerClock,
                            QWidget* parent = nullptr)
        : QWidget(parent), m_mod(mod)
    {
        setObjectName("StageMonWidget");
        auto* mainLay = new QVBoxLayout(this);
        mainLay->setContentsMargins(4, 4, 4, 4);
        mainLay->setSpacing(4);

        // ── Clock bar at top ────────────────────────────────────────────────
        m_clockBar = new ClockBar;
        mainLay->addWidget(m_clockBar);

        // ── Mode selector ───────────────────────────────────────────────────
        auto* modeRow = new QHBoxLayout;
        modeRow->addWidget(new QLabel("Mode:"));
        m_modeCombo = new QComboBox;
        m_modeCombo->addItem("Worship (Lyrics)", static_cast<int>(M1::StageMonModule::MonitorMode::Worship));
        m_modeCombo->addItem("Sermon (Scripture)", static_cast<int>(M1::StageMonModule::MonitorMode::Sermon));
        m_modeCombo->addItem("Combined", static_cast<int>(M1::StageMonModule::MonitorMode::Combined));
        modeRow->addWidget(m_modeCombo, 1);

        m_clockCheck = new QCheckBox("Clock");
        m_clockCheck->setChecked(mod->showClock());
        m_timerCheck = new QCheckBox("Timers");
        m_timerCheck->setChecked(mod->showTimer());
        m_nextCheck  = new QCheckBox("Next Preview");
        m_nextCheck->setChecked(mod->showNextPreview());
        modeRow->addWidget(m_clockCheck);
        modeRow->addWidget(m_timerCheck);
        modeRow->addWidget(m_nextCheck);
        mainLay->addLayout(modeRow);

        // ── Content area ────────────────────────────────────────────────────
        m_worshipView = new WorshipView(lyrics);
        m_sermonView  = new SermonView(scripture);

        // Combined mode: we use a splitter
        m_splitter = new QSplitter(Qt::Horizontal);
        m_splitter->addWidget(m_worshipView);
        m_splitter->addWidget(m_sermonView);
        mainLay->addWidget(m_splitter, 1);

        // ── Store timer reference ───────────────────────────────────────────
        m_timerClock = timerClock;

        // ── Apply initial mode ──────────────────────────────────────────────
        applyMode(mod->monitorMode());

        // ── Connections ─────────────────────────────────────────────────────
        connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                auto mode = static_cast<M1::StageMonModule::MonitorMode>(
                    m_modeCombo->itemData(idx).toInt());
                m_mod->setMonitorMode(mode);
                applyMode(mode);
            });
        connect(m_clockCheck, &QCheckBox::toggled, mod, &M1::StageMonModule::setShowClock);
        connect(m_timerCheck, &QCheckBox::toggled, mod, &M1::StageMonModule::setShowTimer);
        connect(m_nextCheck,  &QCheckBox::toggled, mod, &M1::StageMonModule::setShowNextPreview);
    }

    void refresh() {
        // Update clock
        if (m_mod->showClock()) {
            m_clockBar->setClock(QDateTime::currentDateTime().toString("h:mm:ss AP"));
            m_clockBar->show();
        } else {
            m_clockBar->hide();
        }

        // Update timer display
        if (m_mod->showTimer() && m_timerClock) {
            auto timers = m_timerClock->allTimers();
            QStringList activeTimers;
            for (const auto& t : timers) {
                if (t.state == M1::TimerInstance::State::Running ||
                    t.state == M1::TimerInstance::State::Paused) {
                    activeTimers.append(t.name + ": " + t.formatted());
                }
            }
            if (!activeTimers.isEmpty())
                m_clockBar->setTimer(activeTimers.join("  |  "));
            else
                m_clockBar->setTimer("");
        } else {
            m_clockBar->setTimer("");
        }

        // Refresh views
        m_worshipView->refresh();
        m_sermonView->refresh();
    }

private:
    void applyMode(M1::StageMonModule::MonitorMode mode) {
        switch (mode) {
        case M1::StageMonModule::MonitorMode::Worship:
            m_worshipView->show();
            m_sermonView->hide();
            break;
        case M1::StageMonModule::MonitorMode::Sermon:
            m_worshipView->hide();
            m_sermonView->show();
            break;
        case M1::StageMonModule::MonitorMode::Combined:
            m_worshipView->show();
            m_sermonView->show();
            break;
        }
    }

    M1::StageMonModule*   m_mod        = nullptr;
    M1::TimerClockModule* m_timerClock = nullptr;
    ClockBar*    m_clockBar    = nullptr;
    QComboBox*   m_modeCombo   = nullptr;
    QCheckBox*   m_clockCheck  = nullptr;
    QCheckBox*   m_timerCheck  = nullptr;
    QCheckBox*   m_nextCheck   = nullptr;
    WorshipView* m_worshipView = nullptr;
    SermonView*  m_sermonView  = nullptr;
    QSplitter*   m_splitter    = nullptr;
};

} // anonymous namespace

// ─── MOC include for anonymous namespace QObjects ───────────────────────────
#include "StageMonModule.moc"

namespace M1 {

// ─── Constructor / Destructor ───────────────────────────────────────────────
StageMonModule::StageMonModule(QObject* parent)
    : IModule(parent)
{
}

StageMonModule::~StageMonModule() = default;

// ─── IModule lifecycle ──────────────────────────────────────────────────────
void StageMonModule::initialize() {
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);  // 2Hz refresh for confidence display
    connect(m_refreshTimer, &QTimer::timeout, this, &StageMonModule::onRefreshTick);
}

void StageMonModule::shutdown() {
    if (m_refreshTimer) m_refreshTimer->stop();
}

QWidget* StageMonModule::createWidget(QWidget* parent) {
    initialize();

    auto* widget = new StageMonWidget(this, m_lyrics, m_scripture, m_timerClock, parent);

    // Wire refresh timer to widget
    connect(m_refreshTimer, &QTimer::timeout, widget, &StageMonWidget::refresh);

    m_refreshTimer->start();
    return widget;
}

void StageMonModule::saveState(QSettings& s) {
    s.beginGroup("StageMon");
    s.setValue("mode", static_cast<int>(m_mode));
    s.setValue("fontSize", m_fontSize);
    s.setValue("showClock", m_showClock);
    s.setValue("showTimer", m_showTimer);
    s.setValue("showNextPreview", m_showNextPreview);
    s.endGroup();
}

void StageMonModule::loadState(QSettings& s) {
    s.beginGroup("StageMon");
    m_mode = static_cast<MonitorMode>(s.value("mode", 0).toInt());
    m_fontSize = s.value("fontSize", 28).toInt();
    m_showClock = s.value("showClock", true).toBool();
    m_showTimer = s.value("showTimer", true).toBool();
    m_showNextPreview = s.value("showNextPreview", true).toBool();
    s.endGroup();
}

// ─── Monitor mode ───────────────────────────────────────────────────────────
void StageMonModule::setMonitorMode(MonitorMode mode) {
    if (m_mode == mode) return;
    m_mode = mode;
    emit monitorModeChanged(static_cast<int>(mode));
}

void StageMonModule::setFontSize(int sizePt) {
    m_fontSize = qBound(12, sizePt, 72);
}

void StageMonModule::setShowClock(bool show) {
    m_showClock = show;
}

void StageMonModule::setShowTimer(bool show) {
    m_showTimer = show;
}

void StageMonModule::setShowNextPreview(bool show) {
    m_showNextPreview = show;
}

void StageMonModule::onRefreshTick() {
    emit displayUpdated();
}

} // namespace M1
