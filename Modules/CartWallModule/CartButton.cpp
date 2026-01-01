#include "CartButton.h"
#include "CartPlayer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QUrl>
#include <cmath>

namespace M1 {

CartButton::CartButton(int index, CartPlayer* player, QWidget* parent)
    : QWidget(parent)
    , m_index(index)
    , m_player(player)
{
    setMinimumSize(100, 60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAcceptDrops(true);
    setToolTip(QString("Cart %1 — click to play, right-click to configure").arg(index + 1));
    setCursor(Qt::PointingHandCursor);

    // Layout: index top-left, title center, duration bottom-right
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 4, 6, 4);
    lay->setSpacing(0);

    m_indexLabel = new QLabel(QString::number(index + 1), this);
    m_indexLabel->setObjectName("CartIndexLabel");
    m_indexLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    QFont idxFont = m_indexLabel->font();
    idxFont.setPixelSize(12);
    idxFont.setBold(true);
    m_indexLabel->setFont(idxFont);

    m_titleLabel = new QLabel("Empty", this);
    m_titleLabel->setObjectName("CartTitleLabel");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(13);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_durationLabel = new QLabel("", this);
    m_durationLabel->setObjectName("CartDurationLabel");
    m_durationLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    QFont durFont = m_durationLabel->font();
    durFont.setPixelSize(12);
    m_durationLabel->setFont(durFont);

    lay->addWidget(m_indexLabel, 0);
    lay->addWidget(m_titleLabel, 1);
    lay->addWidget(m_durationLabel, 0);

    // Connect player state changes
    connect(m_player, &CartPlayer::stateChanged, this, &CartButton::onPlayerStateChanged);
    connect(m_player, &CartPlayer::loadingFinished, this, &CartButton::refreshDisplay);

    // Refresh timer for playing animation
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() { update(); });
}

void CartButton::setTitle(const QString& title) {
    m_title = title;
    m_titleLabel->setText(title.isEmpty() ? "Empty" : title);
}

void CartButton::setColor(const QColor& color) {
    m_color = color;
    update();
}

void CartButton::setFilePath(const QString& path) {
    m_filePath = path;
    if (path.isEmpty()) {
        m_player->unload();
        setTitle("");
        m_durationLabel->setText("");
    } else {
        m_player->loadFile(path);
        QFileInfo fi(path);
        setTitle(fi.completeBaseName());
    }
}

// ── Painting ─────────────────────────────────────────────────────────────────

void CartButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect().adjusted(1, 1, -1, -1);
    const bool playing = m_player->isPlaying();
    const bool empty   = m_player->state() == CartPlayer::State::Empty;

    // Background
    QColor bg = m_color;
    if (empty) bg = bg.darker(200);
    if (playing) bg = bg.lighter(130);
    p.setBrush(bg);

    // Border
    QColor border = playing ? QColor(0, 200, 80) : QColor(80, 80, 80);
    if (playing) {
        // Pulse effect
        float pk = m_player->peakLevel();
        int brightness = 200 + static_cast<int>(55.0f * pk);
        border = QColor(0, std::min(brightness, 255), 80);
    }
    p.setPen(QPen(border, playing ? 2.5 : 1.0));
    p.drawRoundedRect(r, 6, 6);

    // Playing indicator: small green dot
    if (playing) {
        p.setBrush(QColor(0, 220, 60));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(r.right() - 8, r.top() + 8), 4, 4);
    }
}

// ── Mouse handling ───────────────────────────────────────────────────────────

void CartButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit triggered(m_index);
    }
    QWidget::mousePressEvent(event);
}

void CartButton::contextMenuEvent(QContextMenuEvent* event) {
    emit configRequested(m_index);
    event->accept();
}

// ── Drag-and-drop ────────────────────────────────────────────────────────────

void CartButton::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile()) {
            event->acceptProposedAction();
            return;
        }
    }
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
        return;
    }
    event->ignore();
}

void CartButton::dropEvent(QDropEvent* event) {
    QString path;
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().isLocalFile())
            path = urls.first().toLocalFile();
    }
    if (path.isEmpty() && event->mimeData()->hasText()) {
        path = event->mimeData()->text();
    }
    if (!path.isEmpty()) {
        setFilePath(path);
        event->acceptProposedAction();
    }
}

// ── Slots ────────────────────────────────────────────────────────────────────

void CartButton::onPlayerStateChanged() {
    const bool playing = m_player->isPlaying();
    if (playing)
        m_refreshTimer->start();
    else
        m_refreshTimer->stop();
    update();
}

void CartButton::refreshDisplay() {
    // Update duration label after loading finishes
    double dur = m_player->durationSeconds();
    if (dur > 0.0) {
        int secs = static_cast<int>(dur);
        int mins = secs / 60;
        secs %= 60;
        m_durationLabel->setText(QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0')));
    }
    update();
}

} // namespace M1
