#include "WaveformView.h"
#include "DeckPlayer.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLinearGradient>
#include <cmath>

// Brand colours matching dark.qss
static const QColor kBgColor   {0x07, 0x11, 0x1e};  // hero dark base
static const QColor kGridColor {0x1e, 0x3a, 0x5f, 80};
static const QColor kGreen     {0x22, 0xc5, 0x5e};
static const QColor kAmber     {0xf5, 0x9e, 0x0b};
static const QColor kRed       {0xef, 0x44, 0x44};
static const QColor kCursorCol {0x38, 0xbd, 0xf8};  // Mcaster Cyan
static const QColor kCueColor  {0xf5, 0x9e, 0x0b};  // amber = cue
static const QColor kLoopColor {0x0e, 0xa5, 0xe9, 50};

// Hot cue colours
static const QColor kHotColors[4] = {
    {0xef, 0x44, 0x44},  // H1 red
    {0x22, 0xc5, 0x5e},  // H2 green
    {0x0e, 0xa5, 0xe9},  // H3 blue
    {0xf5, 0x9e, 0x0b},  // H4 amber
};

WaveformView::WaveformView(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("WaveformView");
    setProperty("role", "waveform");
    setMinimumHeight(60);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void WaveformView::setDeckPlayer(M1::DeckPlayer* player) {
    if (m_player) {
        disconnect(m_player, nullptr, this, nullptr);
    }
    m_player = player;
    m_imageDirty = true;

    if (m_player) {
        connect(m_player, &M1::DeckPlayer::positionChanged,
                this, &WaveformView::onPositionChanged);
        connect(m_player, &M1::DeckPlayer::loadingFinished,
                this, &WaveformView::refresh);
        connect(m_player, &M1::DeckPlayer::hotCuesChanged,
                this, [this]() { update(); });
        m_posFrame = m_player->positionSamples();
    }
}

void WaveformView::refresh() {
    m_imageDirty = true;
    buildImage();
    update();
}

void WaveformView::onPositionChanged(qint64 frame) {
    m_posFrame = frame;
    update(); // just repaint — cursor moved
}

void WaveformView::resizeEvent(QResizeEvent*) {
    m_imageDirty = true;
    buildImage();
}

static const QColor kEmptyBg {235, 228, 218};  // sandy tan — shown when no track loaded

void WaveformView::buildImage() {
    const int W = width();
    const int H = height();
    if (W <= 0 || H <= 0) return;

    m_waveImage = QImage(W * 2, H * 2, QImage::Format_ARGB32_Premultiplied);
    m_waveImage.setDevicePixelRatio(2.0);

    if (!m_player || m_player->waveformPeaks().empty()) {
        // Empty / no-track state: use sandy background
        m_waveImage.fill(kEmptyBg.rgb());
        m_imageDirty = false;
        return;
    }

    m_waveImage.fill(kBgColor.rgb());

    QPainter p(&m_waveImage);
    p.setRenderHint(QPainter::Antialiasing, false);

    const auto& peaks = m_player->waveformPeaks();
    const int   nCols = (int)peaks.size();
    const int   cY    = (H * 2) / 2;

    // Draw horizontal centre line
    p.setPen(QPen(kGridColor, 1));
    p.drawLine(0, cY, W * 2, cY);

    // Draw waveform columns
    for (int px = 0; px < W * 2; ++px) {
        int colIdx = (int)((float)px / (W * 2) * nCols);
        colIdx = std::clamp(colIdx, 0, nCols - 1);
        float peak = peaks[colIdx];

        // Color by amplitude
        QColor col = (peak > 0.9f) ? kRed : (peak > 0.65f) ? kAmber : kGreen;
        col.setAlpha(200);

        int barH = (int)(peak * cY * 0.92f);
        p.fillRect(px, cY - barH, 1, barH,     col.darker(110)); // top (L)
        p.fillRect(px, cY,        1, barH + 1, col);              // bottom (R)
    }

    m_imageDirty = false;
}

void WaveformView::paintEvent(QPaintEvent*) {
    if (m_imageDirty) buildImage();

    QPainter p(this);
    const int W = width();
    const int H = height();

    // Draw waveform (scrolling: centre on playhead)
    if (!m_waveImage.isNull()) {
        p.drawImage(0, 0, m_waveImage.scaled(W, H, Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation));
    } else {
        p.fillRect(rect(), kEmptyBg);
    }

    if (!m_player || m_player->totalSamples() <= 0) {
        // Show "drop file" hint on sandy background
        p.setPen(QColor(150, 138, 120, 200));
        p.setFont(QFont("Consolas", 9, QFont::Bold));
        p.drawText(rect(), Qt::AlignCenter, "DROP AUDIO FILE");
        return;
    }

    const qint64 total = m_player->totalSamples();
    auto frameToX = [&](qint64 frame) -> int {
        return (int)((double)frame / total * W);
    };

    // ─── Loop region ──────────────────────────────────────────────
    if (m_player->loopEnabled() && m_player->loopOut() > 0) {
        int x1 = frameToX(m_player->loopIn());
        int x2 = frameToX(m_player->loopOut());
        p.fillRect(x1, 0, x2 - x1, H, kLoopColor);
        p.setPen(QPen(kCursorCol, 1, Qt::DashLine));
        p.drawLine(x1, 0, x1, H);
        p.drawLine(x2, 0, x2, H);
    }

    // ─── Hot cue markers ─────────────────────────────────────────
    for (int i = 0; i < M1::DeckPlayer::kHotCueCount; ++i) {
        qint64 hc = m_player->hotCue(i);
        if (hc < 0) continue;
        int x = frameToX(hc);
        p.setPen(QPen(kHotColors[i], 2));
        p.drawLine(x, 0, x, H - 12);
        // Triangle marker at top
        QPolygon tri;
        tri << QPoint(x - 5, 0) << QPoint(x + 5, 0) << QPoint(x, 8);
        p.setBrush(kHotColors[i]);
        p.drawPolygon(tri);
    }

    // ─── Cue marker ──────────────────────────────────────────────
    {
        int cx = frameToX(m_player->cuePoint());
        p.setPen(QPen(kCueColor, 2));
        p.drawLine(cx, 4, cx, H);
        // Small triangle at bottom
        QPolygon tri;
        tri << QPoint(cx - 5, H) << QPoint(cx + 5, H) << QPoint(cx, H - 8);
        p.setBrush(kCueColor);
        p.drawPolygon(tri);
    }

    // ─── Playhead cursor ─────────────────────────────────────────
    {
        int x = frameToX(m_posFrame);
        p.setPen(QPen(kCursorCol, 2));
        p.drawLine(x, 0, x, H);
        // Bright dot at top
        p.setBrush(kCursorCol);
        p.setPen(Qt::NoPen);
        p.drawEllipse(x - 3, 0, 6, 6);
    }
}

// Seek on click
void WaveformView::mousePressEvent(QMouseEvent* event) {
    if (!m_player || m_player->totalSamples() <= 0) return;
    if (event->button() != Qt::LeftButton) return;

    double ratio = (double)event->pos().x() / width();
    qint64 target = (qint64)(ratio * m_player->totalSamples());
    m_player->seek(target);
}
