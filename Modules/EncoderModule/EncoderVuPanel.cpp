#include "EncoderVuPanel.h"
#include "EncoderSlot.h"
#include <QPainter>
#include <QFont>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
EncoderVuPanel::EncoderVuPanel(QList<EncoderSlot*>* slotList, QWidget* parent)
    : QWidget(parent)
    , m_slots(slotList)
{
    setObjectName("EncoderVuPanel");
    setMinimumWidth(50);

    m_timer = new QTimer(this);
    m_timer->setInterval(50);  // 20 fps — broadcast-standard meter refresh
    connect(m_timer, &QTimer::timeout, this, &EncoderVuPanel::onTick);
    m_timer->start();

    rebuild();
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderVuPanel::rebuild()
{
    const int n = m_slots ? m_slots->size() : 0;
    m_meters.resize(n);
    for (auto& m : m_meters) {
        m.smoothL = m.smoothR = 0.0f;
        m.peakL   = m.peakR   = 0.0f;
        m.holdL   = m.holdR   = 0;
    }
    updateGeometry();
    update();
}

// ─────────────────────────────────────────────────────────────────────────────
QSize EncoderVuPanel::sizeHint() const
{
    const int n = m_slots ? m_slots->size() : 0;
    const int pairW = kBarW * 2 + kLRGap;
    const int w = kMargin * 2 + n * pairW + (n > 1 ? (n - 1) * kSlotGap : 0);
    return QSize(std::max(w, 50), 120);
}

QSize EncoderVuPanel::minimumSizeHint() const
{
    return QSize(50, 60);
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderVuPanel::onTick()
{
    if (!m_slots) return;

    const int n = std::min(m_slots->size(), static_cast<qsizetype>(m_meters.size()));

    for (int i = 0; i < n; ++i) {
        auto* slot = (*m_slots)[i];
        auto& ms   = m_meters[i];

        // Read REAL peak levels from the RT thread atomics
        const float rawL = slot->peakL();
        const float rawR = slot->peakR();

        // Ballistic smoothing: fast attack, slow release
        auto smooth = [](float cur, float target) -> float {
            const float coeff = (target > cur) ? kAttack : kRelease;
            return cur + coeff * (target - cur);
        };

        ms.smoothL = smooth(ms.smoothL, rawL);
        ms.smoothR = smooth(ms.smoothR, rawR);

        // Peak hold with decay
        auto updatePeak = [](float& peak, int& hold, float level) {
            if (level >= peak) {
                peak = level;
                hold = kPeakHoldFrames;
            } else if (hold > 0) {
                --hold;
            } else {
                peak *= kPeakDecay;
            }
        };

        updatePeak(ms.peakL, ms.holdL, ms.smoothL);
        updatePeak(ms.peakR, ms.holdR, ms.smoothR);
    }

    update();
}

// ─────────────────────────────────────────────────────────────────────────────
void EncoderVuPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);  // Sharp pixel-aligned bars

    // Dark background
    p.fillRect(rect(), QColor(0x0e, 0x0e, 0x10));

    if (!m_slots || m_slots->isEmpty()) return;

    const int n = std::min(m_slots->size(), static_cast<qsizetype>(m_meters.size()));
    const int pairW  = kBarW * 2 + kLRGap;
    const int totalW = n * pairW + (n > 1 ? (n - 1) * kSlotGap : 0);
    const int x0     = (width() - totalW) / 2;  // Center meters horizontally

    // ── Header: "VU" label + signal status ──────────────────────────────────
    static constexpr int kHeaderH = 16;
    {
        QFont hf(QStringLiteral("Segoe UI"), 7, QFont::Bold);
        p.setFont(hf);

        // Check if ANY slot is receiving audio blocks
        bool receiving = false;
        bool hasSignal = false;
        for (int i = 0; i < n; ++i) {
            auto* slot = (*m_slots)[i];
            if (slot->audioBlockCount() > 0)
                receiving = true;
            if (m_meters[i].smoothL > 1e-6f || m_meters[i].smoothR > 1e-6f)
                hasSignal = true;
        }

        if (hasSignal)
            p.setPen(QColor(0x00, 0xcc, 0x55));  // Green = signal present
        else if (receiving)
            p.setPen(QColor(0x55, 0x55, 0x55));  // Gray = callback firing but no signal
        else
            p.setPen(QColor(0xcc, 0x33, 0x33));  // Red = not receiving audio blocks

        const QString label = hasSignal ? "VU" : (receiving ? "VU --" : "VU OFF");
        p.drawText(QRect(0, 0, width(), kHeaderH), Qt::AlignCenter, label);
    }

    const int barTop    = kMargin + kHeaderH;
    const int barBottom = height() - kMargin - kLabelH;
    const int barH      = barBottom - barTop;
    if (barH < 10) return;

    const float dbRange = kDbClip - kDbMin;  // 60 dB

    // Precompute zone boundaries (normalised 0..1)
    const float greenMax = (-12.0f - kDbMin) / dbRange;  // 0.80
    const float amberMax = ( -3.0f - kDbMin) / dbRange;  // 0.95

    for (int i = 0; i < n; ++i) {
        auto*       slot = (*m_slots)[i];
        const auto& ms   = m_meters[i];
        const int   cx   = x0 + i * (pairW + kSlotGap);

        // Only show activity for LiveDeckMix source slots
        const bool isLiveMix = (slot->config().source == EncoderConfig::Source::LiveDeckMix);

        // ── Draw one vertical bar ───────────────────────────────────────
        auto drawBar = [&](int bx, float level, float peak) {
            // Convert linear to dB  (fabsf explicit to avoid MSVC int overload)
            float db     = (level > 1e-7f) ? 20.0f * std::log10f(level) : kDbMin;
            float peakDb = (peak  > 1e-7f) ? 20.0f * std::log10f(peak)  : kDbMin;
            db     = std::clamp(db,     kDbMin, kDbClip);
            peakDb = std::clamp(peakDb, kDbMin, kDbClip);

            const float fillNorm = (db     - kDbMin) / dbRange;  // 0..1
            const float peakNorm = (peakDb - kDbMin) / dbRange;

            // Background (unlit bar)
            p.fillRect(bx, barTop, kBarW, barH, QColor(0x1a, 0x1a, 0x1e));

            if (!isLiveMix) return;  // Non-LiveDeckMix: show empty bar

            const int fillH = static_cast<int>(fillNorm * barH);

            if (fillH > 0) {
                // Green zone: bottom → greenMax
                const int greenH = static_cast<int>(std::min(fillNorm, greenMax) * barH);
                if (greenH > 0)
                    p.fillRect(bx, barBottom - greenH, kBarW, greenH,
                               QColor(0x00, 0xcc, 0x55));

                // Amber zone: greenMax → amberMax
                if (fillNorm > greenMax) {
                    const int amberBottom = barBottom - static_cast<int>(greenMax * barH);
                    const int amberTop    = barBottom - static_cast<int>(std::min(fillNorm, amberMax) * barH);
                    if (amberBottom > amberTop)
                        p.fillRect(bx, amberTop, kBarW, amberBottom - amberTop,
                                   QColor(0xff, 0xaa, 0x00));
                }

                // Red zone: amberMax → fillNorm
                if (fillNorm > amberMax) {
                    const int redBottom = barBottom - static_cast<int>(amberMax * barH);
                    const int redTop    = barBottom - fillH;
                    if (redBottom > redTop)
                        p.fillRect(bx, redTop, kBarW, redBottom - redTop,
                                   QColor(0xff, 0x22, 0x22));
                }
            }

            // Peak hold tick (1.5px line at peak level)
            if (peakDb > kDbMin + 1.0f) {
                const int peakY = barBottom - static_cast<int>(peakNorm * barH);
                QColor peakCol;
                if (peakDb >= -3.0f)       peakCol = QColor(0xff, 0x22, 0x22);
                else if (peakDb >= -12.0f) peakCol = QColor(0xff, 0xaa, 0x00);
                else                       peakCol = QColor(0x00, 0xcc, 0x55);
                p.setPen(QPen(peakCol, 1.5));
                p.drawLine(bx, peakY, bx + kBarW - 1, peakY);
            }
        };

        drawBar(cx,                    ms.smoothL, ms.peakL);
        drawBar(cx + kBarW + kLRGap,  ms.smoothR, ms.peakR);

        // Slot number label
        const bool active = isLiveMix && (ms.smoothL > 1e-6f || ms.smoothR > 1e-6f);
        p.setPen(active ? QColor(0xcc, 0xcc, 0xcc) : QColor(0x55, 0x55, 0x55));
        QFont f(QStringLiteral("Segoe UI"), 7);
        f.setBold(active);
        p.setFont(f);
        p.drawText(QRect(cx, barBottom + 1, pairW, kLabelH),
                   Qt::AlignHCenter | Qt::AlignTop,
                   QString::number(i + 1));
    }
}
