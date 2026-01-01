#include "NowPlayingWidget.h"
#include "ThemeManager.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QLinearGradient>
#include <QFontMetrics>

NowPlayingWidget::NowPlayingWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("NowPlayingWidget");
    setMinimumWidth(180);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(6, 0, 6, 0);
    row->setSpacing(5);

    // Blinking dot indicator
    m_dotLabel = new QLabel("●", this);
    m_dotLabel->setFixedWidth(12);

    // Deck badge (e.g., "DK-A")
    m_deckLabel = new QLabel("─", this);
    m_deckLabel->setFixedWidth(36);
    m_deckLabel->setAlignment(Qt::AlignCenter);

    // Track info
    m_trackLabel = new QLabel("NO DECK", this);
    m_trackLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_trackLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    row->addWidget(m_dotLabel);
    row->addWidget(m_deckLabel);
    row->addWidget(m_trackLabel, 1);

    // Blink timer (1 Hz)
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(500);
    connect(m_blinkTimer, &QTimer::timeout, this, &NowPlayingWidget::onBlink);

    applyTheme();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { applyTheme(); update(); });
}

void NowPlayingWidget::applyTheme() {
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    const QString idle = isLight ? "#c0b8ae" : "#334155";
    const QString mono = "font-family:'Consolas','Courier New',monospace;";

    if (!m_isPlaying) {
        m_dotLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:10px; }").arg(idle));
        m_deckLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:9px; font-weight:900; %2 }").arg(idle, mono));
        m_trackLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:9px; %2 }").arg(idle, mono));
    } else {
        // Re-apply playing state styles with correct theme
        const QString accent = isLight ? "#1c5caa" : "#0ea5e9";
        const QString badgeBg = isLight ? "#dbeafe" : "#052030";
        const QString trackTx = isLight ? "#1a1814" : "#e2e8f0";
        m_deckLabel->setStyleSheet(QString(
            "QLabel { color:%1; font-size:9px; font-weight:900; %3"
            "  background:%2; border:1px solid %1; border-radius:3px; padding:0 3px; }")
            .arg(accent, badgeBg, mono));
        m_trackLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:9px; %2 }").arg(trackTx, mono));
        // dot color will be updated by next blink tick
    }
}

void NowPlayingWidget::setPlaying(const QString& artist, const QString& title,
                                   const QString& deckId)
{
    m_artist    = artist;
    m_title     = title;
    m_deckId    = deckId;
    m_isPlaying = true;

    const QString track = (!artist.isEmpty() && !title.isEmpty())
                          ? artist + " — " + title
                          : (!title.isEmpty() ? title : artist);

    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    m_deckLabel->setText(deckId);
    m_deckLabel->setStyleSheet(QString(
        "QLabel { color:%1; font-size:9px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace;"
        "  background:%2; border:1px solid %1; border-radius:3px; padding:0 3px; }")
        .arg(isLight ? "#1c5caa" : "#0ea5e9",
             isLight ? "#dbeafe" : "#052030"));

    m_trackLabel->setText(track);
    m_trackLabel->setStyleSheet(QString(
        "QLabel { color:%1; font-size:9px;"
        "  font-family:'Consolas','Courier New',monospace; }")
        .arg(isLight ? "#1a1814" : "#e2e8f0"));

    if (!m_blinkTimer->isActive()) m_blinkTimer->start();
}

void NowPlayingWidget::clearPlaying() {
    m_isPlaying  = false;
    m_blinkTimer->stop();
    applyTheme();
    update();
}

void NowPlayingWidget::onBlink() {
    m_blinkState = !m_blinkState;
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    m_dotLabel->setStyleSheet(QString("QLabel { color:%1; font-size:10px; }")
        .arg(m_blinkState
             ? "#22c55e"
             : (isLight ? "#b8e8c8" : "#064010")));
    m_dotLabel->setText("●");
    update(); // repaint border
}

void NowPlayingWidget::paintEvent(QPaintEvent* e) {
    QPainter p(this);
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    QLinearGradient bg(0, 0, 0, height());
    if (isLight) {
        bg.setColorAt(0.0, m_isPlaying ? QColor(0xf0, 0xfc, 0xe0) : QColor(0xf5, 0xf3, 0xf0));
        bg.setColorAt(1.0, m_isPlaying ? QColor(0xe8, 0xf5, 0xee) : QColor(0xed, 0xe8, 0xe0));
    } else {
        bg.setColorAt(0.0, m_isPlaying ? QColor(8, 20, 14) : QColor(8, 12, 20));
        bg.setColorAt(1.0, m_isPlaying ? QColor(4, 12, 8)  : QColor(5, 8, 15));
    }
    p.fillRect(rect(), bg);

    if (m_isPlaying) {
        p.setPen(QPen(isLight ? QColor(0x16, 0xa3, 0x4a) : QColor(20, 60, 30), 1));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    QWidget::paintEvent(e);
}
