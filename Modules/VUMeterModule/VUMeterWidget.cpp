#include "VUMeterWidget.h"
#include "ThemeManager.h"
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <cmath>
#include <algorithm>

namespace M1 {

VUMeterWidget::VUMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);

    m_timer.setInterval(50); // 20 fps
    connect(&m_timer, &QTimer::timeout, this, &VUMeterWidget::onTimer);
    m_timer.start();
    connect(ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { update(); });
}

VUMeterWidget::~VUMeterWidget() {
    m_timer.stop();
}

void VUMeterWidget::setLevels(float leftLinear, float rightLinear) {
    m_leftLinear.store(std::max(0.0f, std::min(1.0f, leftLinear)),  std::memory_order_relaxed);
    m_rightLinear.store(std::max(0.0f, std::min(1.0f, rightLinear)), std::memory_order_relaxed);
}

void VUMeterWidget::onTimer() {
    const float rawL = m_leftLinear.load(std::memory_order_relaxed);
    const float rawR = m_rightLinear.load(std::memory_order_relaxed);

    // Ballistic: fast attack, slow decay (broadcast VU standard)
    const float attack  = 0.7f;   // ~350ms at 20fps
    const float release = 0.04f;  // ~2.5s at 20fps

    auto applyBallistic = [&](float raw, float& disp, float& peak, int& hold) {
        if (raw > disp) disp = disp + (raw - disp) * attack;
        else            disp = disp - (disp - raw) * release;
        disp = std::max(0.0f, std::min(1.0f, disp));

        const float db = linearToDb(disp);
        if (db > peak) {
            peak = db;
            hold = 60; // 3 seconds @ 20fps
        } else if (hold > 0) {
            --hold;
        } else {
            peak -= 0.5f; // Decay peak hold
            if (peak < DB_MIN) peak = DB_MIN;
        }
    };

    applyBallistic(rawL, m_dispL, m_peakL, m_peakHoldL);
    applyBallistic(rawR, m_dispR, m_peakR, m_peakHoldR);

    QWidget::update(); // Trigger repaint
}

void VUMeterWidget::setCompact(bool compact) {
    m_compact = compact;
    if (compact) {
        setMinimumSize(80, 36);
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        setMinimumSize(minimumSizeHint());
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    updateGeometry();
    update();
}

void VUMeterWidget::setDeviceName(const QString& name) {
    m_deviceName = name;
    update();
}

void VUMeterWidget::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
}

void VUMeterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Background — dark for Dark/Classic themes; sandy for Light
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    p.fillRect(rect(), isLight ? QColor(0xf0, 0xeb, 0xe3) : QColor(0x12, 0x12, 0x14));

    if (m_compact) {
        paintCompact(p);
    } else {
        paintFull(p);
    }
}

void VUMeterWidget::paintCompact(QPainter& p) {
    // Two horizontal bars stacked: L (top) and R (bottom)
    const int margin = 4;
    const int vuLabelW = 16;
    const int barH = (height() - margin * 2 - 4) / 2;  // 4px gap between bars
    const int barX = margin + vuLabelW;
    const int barW = width() - barX - margin;
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);

    // "VU" label top-left
    p.setPen(isLight ? QColor(0x1c, 0x5c, 0xaa) : QColor(0x55, 0x77, 0x99));
    p.setFont(QFont("Consolas", 7, QFont::Bold));
    p.drawText(QRect(margin, margin, vuLabelW, barH), Qt::AlignCenter, "L");
    p.drawText(QRect(margin, margin + barH + 4, vuLabelW, barH), Qt::AlignCenter, "R");

    // Draw horizontal bars
    auto drawHBar = [&](int x, int y, int w, int h, float db, float peakDb) {
        const float norm = std::max(0.0f, std::min(1.0f, (db - DB_MIN) / (DB_CLIP - DB_MIN)));
        const int fillW = static_cast<int>(norm * w);

        // Segmented fill
        const int segments = 20;
        const float segW = static_cast<float>(w) / segments;
        for (int i = 0; i < segments; ++i) {
            const float segDb = DB_MIN + (static_cast<float>(i) / segments) * (DB_CLIP - DB_MIN);
            const bool lit    = (i * segW) < fillW;
            QColor col = levelColor(segDb);
            if (!lit) col = col.darker(400);
            p.fillRect(QRectF(x + i * segW, y, segW - 0.5f, h), col);
        }

        // Peak tick
        if (peakDb > DB_MIN) {
            const float peakNorm = std::max(0.0f, std::min(1.0f, (peakDb - DB_MIN) / (DB_CLIP - DB_MIN)));
            const int peakX = x + static_cast<int>(peakNorm * w);
            p.setPen(QPen(levelColor(peakDb), 1.5));
            p.drawLine(peakX, y, peakX, y + h - 1);
        }
    };

    drawHBar(barX, margin,            barW, barH, linearToDb(m_dispL), m_peakL);
    drawHBar(barX, margin + barH + 4, barW, barH, linearToDb(m_dispR), m_peakR);

    // Device name truncated at right (rendered below in compact mode it's too small, skip if width < 100)
    if (!m_deviceName.isEmpty() && width() >= 100) {
        p.setPen(isLight ? QColor(0x6b, 0x65, 0x60) : QColor(0x44, 0x66, 0x88));
        p.setFont(QFont("Segoe UI", 7));
        const QRect devRect(barX, margin, barW, barH);
        const QString elidedName = p.fontMetrics().elidedText(m_deviceName, Qt::ElideRight, barW - 4);
        // Draw device name semi-transparent over the bar area (top bar right side)
        // Using a small label below bar area if height allows
        if (height() >= 48) {
            p.drawText(QRect(barX, height() - 12, barW, 12),
                       Qt::AlignRight | Qt::AlignVCenter, elidedName);
        }
    }
}

void VUMeterWidget::paintFull(QPainter& p) {
    const int margin   = 8;
    const int labelH   = 20;
    const int devLabelH = m_deviceName.isEmpty() ? 0 : 16;
    const int gap      = 6;
    const int barW     = (width() - margin * 2 - gap) / 2;
    const int barH     = height() - margin * 2 - labelH - devLabelH;

    const QRectF leftRect  (margin,              margin, barW, barH);
    const QRectF rightRect (margin + barW + gap, margin, barW, barH);

    drawBar(p, leftRect,  linearToDb(m_dispL), m_peakL, "L");
    drawBar(p, rightRect, linearToDb(m_dispR), m_peakR, "R");

    // Scale markings on left edge
    const bool isLight = (ThemeManager::instance()->currentTheme() == ThemeManager::Theme::Light);
    const QColor scaleColor = isLight ? QColor(0x6b, 0x65, 0x60) : QColor(0x88, 0x88, 0x88);
    p.setPen(scaleColor);
    p.setFont(QFont("Consolas", 7));
    const float dbs[] = {0, -3, -6, -12, -18, -24, -36, -48, -60};
    for (float db : dbs) {
        const float norm = (db - DB_MIN) / (DB_CLIP - DB_MIN);
        const float y    = leftRect.bottom() - norm * barH;
        p.drawLine(QPointF(leftRect.left() - 3, y), QPointF(leftRect.left(), y));
        p.drawText(QRectF(0, y - 8, margin - 2, 16),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(static_cast<int>(db)));
    }

    // L / R labels
    p.setPen(isLight ? QColor(0x1a, 0x18, 0x14) : QColor(0xaa, 0xaa, 0xaa));
    p.setFont(QFont("Segoe UI", 8, QFont::Bold));
    p.drawText(QRectF(leftRect.left(),  height() - labelH - devLabelH, barW, labelH),
               Qt::AlignCenter, "L");
    p.drawText(QRectF(rightRect.left(), height() - labelH - devLabelH, barW, labelH),
               Qt::AlignCenter, "R");

    // Device name label at the very bottom
    if (!m_deviceName.isEmpty()) {
        p.setPen(isLight ? QColor(0x1c, 0x5c, 0xaa) : QColor(0x55, 0x77, 0x99));
        p.setFont(QFont("Segoe UI", 7));
        const QString elided = p.fontMetrics().elidedText(m_deviceName, Qt::ElideRight, width() - 8);
        p.drawText(QRectF(4, height() - devLabelH, width() - 8, devLabelH),
                   Qt::AlignCenter, elided);
    }
}

void VUMeterWidget::drawBar(QPainter& p, const QRectF& rect, float db, float peakDb,
                             const QString& /*label*/)
{
    const float norm = std::max(0.0f, std::min(1.0f, (db - DB_MIN) / (DB_CLIP - DB_MIN)));
    const float fillH = norm * static_cast<float>(rect.height());

    // Draw segmented gradient bar
    const int segments = 40;
    const float segH = static_cast<float>(rect.height()) / segments;

    for (int i = 0; i < segments; ++i) {
        const float segDb = DB_MIN + (static_cast<float>(i) / segments) * (DB_CLIP - DB_MIN);
        const float segY  = static_cast<float>(rect.bottom()) - (i + 1) * segH;
        const bool  lit   = (i * segH) < fillH;

        QRectF segRect(rect.left(), segY, rect.width(), segH - 1.0f);
        QColor col = levelColor(segDb);
        if (!lit) col = col.darker(350);
        p.fillRect(segRect, col);
    }

    // Peak hold tick
    if (peakDb > DB_MIN) {
        const float peakNorm = std::max(0.0f, std::min(1.0f, (peakDb - DB_MIN) / (DB_CLIP - DB_MIN)));
        const float peakY    = static_cast<float>(rect.bottom()) - peakNorm * static_cast<float>(rect.height());
        p.setPen(QPen(levelColor(peakDb), 2.0));
        p.drawLine(QPointF(rect.left(), peakY), QPointF(rect.right(), peakY));
    }

    // Clip LED at top
    if (db >= DB_CLIP) {
        p.fillRect(QRectF(rect.left(), rect.top() - 6, rect.width(), 5),
                   QColor(0xff, 0x22, 0x22));
    }
}

float VUMeterWidget::linearToDb(float linear) const {
    if (linear <= 0.0f) return DB_MIN;
    const float db = 20.0f * std::log10(linear);
    return std::max(DB_MIN, std::min(DB_CLIP, db));
}

QColor VUMeterWidget::levelColor(float db) const {
    if (db >= DB_RED)   return QColor(0xff, 0x22, 0x22); // Red: 0 to -3 dBFS
    if (db >= DB_AMBER) return QColor(0xff, 0xaa, 0x00); // Amber: -3 to -12 dBFS
    return QColor(0x00, 0xcc, 0x55);                     // Green: < -12 dBFS
}

} // namespace M1
