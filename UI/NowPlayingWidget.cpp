#include "NowPlayingWidget.h"
#include "ThemeManager.h"
#include "ThemePalette.h"
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
    const auto pal = ThemePalette::forCurrentTheme();
    const QString idle = pal.textDisabled.name();
    const QString mono = "font-family:'Consolas','Courier New',monospace;";

    if (!m_isPlaying) {
        m_dotLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:12px; }").arg(idle));
        m_deckLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:12px; font-weight:900; %2 }").arg(idle, mono));
        m_trackLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:12px; %2 }").arg(idle, mono));
    } else {
        // Re-apply playing state styles with correct theme
        const QString accent = pal.accent.name();
        const QString badgeBg = pal.cardBg.name();
        const QString trackTx = pal.text.name();
        m_deckLabel->setStyleSheet(QString(
            "QLabel { color:%1; font-size:12px; font-weight:900; %3"
            "  background:%2; border:1px solid %1; border-radius:3px; padding:0 3px; }")
            .arg(accent, badgeBg, mono));
        m_trackLabel->setStyleSheet(
            QString("QLabel { color:%1; font-size:12px; %2 }").arg(trackTx, mono));
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

    const auto pal = ThemePalette::forCurrentTheme();
    m_deckLabel->setText(deckId);
    m_deckLabel->setStyleSheet(QString(
        "QLabel { color:%1; font-size:12px; font-weight:900;"
        "  font-family:'Consolas','Courier New',monospace;"
        "  background:%2; border:1px solid %1; border-radius:3px; padding:0 3px; }")
        .arg(pal.accent.name(), pal.cardBg.name()));

    m_trackLabel->setText(track);
    m_trackLabel->setStyleSheet(QString(
        "QLabel { color:%1; font-size:12px;"
        "  font-family:'Consolas','Courier New',monospace; }")
        .arg(pal.text.name()));

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
    const auto pal = ThemePalette::forCurrentTheme();
    m_dotLabel->setStyleSheet(QString("QLabel { color:%1; font-size:12px; }")
        .arg(m_blinkState
             ? pal.success.name()
             : pal.success.darker(200).name()));
    m_dotLabel->setText(QString::fromUtf8("\xe2\x97\x8f"));
    update(); // repaint border
}

void NowPlayingWidget::paintEvent(QPaintEvent* e) {
    QPainter p(this);
    const auto pal = ThemePalette::forCurrentTheme();
    QLinearGradient bg(0, 0, 0, height());
    if (m_isPlaying) {
        bg.setColorAt(0.0, pal.success.darker(400));
        bg.setColorAt(1.0, pal.success.darker(500));
    } else {
        bg.setColorAt(0.0, pal.panelBg);
        bg.setColorAt(1.0, pal.bg);
    }
    p.fillRect(rect(), bg);

    if (m_isPlaying) {
        p.setPen(QPen(pal.success.darker(200), 1));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    QWidget::paintEvent(e);
}
